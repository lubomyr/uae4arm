/*
 * sigsegv_linux_arm.cpp - x86_64 Linux SIGSEGV handler
 *
 * Copyright (c) 2014 Jens Heitmann ARAnyM dev team (see AUTHORS)
 * 
 * Inspired by Bernie Meyer's UAE-JIT and Gwenole Beauchesne's Basilisk II-JIT
 *
 * This file is part of the ARAnyM project which builds a new and powerful
 * TOS/FreeMiNT compatible virtual machine running on almost any hardware.
 *
 * ARAnyM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ARAnyM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ARAnyM; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "include/memory-uae.h"
#include "newcpu.h"
#include "custom.h"
#ifdef JIT
#include "jit/comptbl.h"
#include "jit/compemu.h"
#endif
#include "uae.h"

#include <asm/sigcontext.h>
#include <signal.h>
#include <dlfcn.h>
#ifndef ANDROID
#include <execinfo.h>
#else
int backtrace(void**,int){ return 0; }
char** backtrace_symbols(void* const*,int){return NULL; }
void backtrace_symbols_fd(void* const*,int,int){} 
#endif
#include <SDL.h>
#include <fcntl.h>
#include <link.h>
#ifdef ANDROID
#include <android/log.h>
#endif

#ifdef JIT
extern uae_u8* current_compile_p;
extern uae_u8* compiled_code;
extern uae_u8 *popallspace;
extern blockinfo* active;
extern blockinfo* dormant;
extern void invalidate_block(blockinfo* bi);
extern void raise_in_cl_list(blockinfo* bi);
#endif
  

#define SHOW_DETAILS 2

#ifdef WITH_LOGGING
#define output_log  write_log
#else
#define output_log
#endif


enum transfer_type_t {
	TYPE_UNKNOWN,
	TYPE_LOAD,
	TYPE_STORE
};

enum type_size_t {
	SIZE_UNKNOWN,
	SIZE_BYTE,
	SIZE_WORD,
	SIZE_INT
};

enum style_type_t { 
  STYLE_SIGNED, 
  STYLE_UNSIGNED 
};

#define HANDLE_EXCEPTION_NONE 0
#define HANDLE_EXCEPTION_OK 1
#define HANDLE_EXCEPTION_A4000RAM 2

static int in_handler = 0;
static int max_signals = 200;  

/* Where to leave a crash report. Built once at startup, because working it out
   inside a signal handler is neither safe nor possible once memory is suspect. */
static char crash_log_path[MAX_DPATH] = { 0 };

/* The build id of our own shared library, as hex. Offsets in a report only mean
   something against the exact binary they came from, and a report may arrive
   several releases later - this is what pins it to one. */
static char crash_build_id[64] = "unknown";

/* Reads the GNU build id out of our own PT_NOTE segment. Done once at startup:
   the parsing has no business running inside a signal handler. */
static int find_build_id(struct dl_phdr_info *info, size_t size, void *data)
{
  /* The main executable comes first with an empty name; we want the library
     holding this very function, which is the one dladdr resolves. */
  Dl_info self;
  if(!dladdr((void *)find_build_id, &self) || !self.dli_fname)
    return 1;
  if(!info->dlpi_name || strcmp(info->dlpi_name, self.dli_fname) != 0)
    return 0;

  for(int i = 0; i < info->dlpi_phnum; ++i) {
    const ElfW(Phdr) *ph = &info->dlpi_phdr[i];
    if(ph->p_type != PT_NOTE)
      continue;

    const char *p = (const char *)(info->dlpi_addr + ph->p_vaddr);
    const char *end = p + ph->p_memsz;
    while(p + sizeof(ElfW(Nhdr)) <= end) {
      const ElfW(Nhdr) *nh = (const ElfW(Nhdr) *)p;
      const char *name = p + sizeof(ElfW(Nhdr));
      const char *desc = name + ((nh->n_namesz + 3) & ~3);

      if(nh->n_type == NT_GNU_BUILD_ID && nh->n_namesz == 4 &&
         memcmp(name, "GNU", 4) == 0) {
        unsigned int n = nh->n_descsz;
        if(n > (sizeof(crash_build_id) - 1) / 2)
          n = (sizeof(crash_build_id) - 1) / 2;
        for(unsigned int b = 0; b < n; ++b)
          snprintf(crash_build_id + b * 2, 3, "%02x", (unsigned char)desc[b]);
        return 1;
      }
      p = desc + ((nh->n_descsz + 3) & ~3);
    }
  }
  return 1;
}

/* An unhandled fault ends in exit(1) below, which Android sees as an ordinary
   exit: no tombstone, no "app has stopped" dialog, nothing in the bug report.
   Every trace this handler collects is also compiled out unless WITH_LOGGING is
   set, so release builds die in complete silence and the crash cannot be
   diagnosed from a user's report at all.

   Leave the bare minimum behind unconditionally: the signal, the faulting
   address, and the offsets of PC and LR within their module - absolute
   addresses are useless under ASLR, while offsets can be fed straight to
   llvm-symbolizer. It goes to logcat and to a file next to the configurations,
   so a reporter who cannot run adb still has something to send. */
static void report_fatal_signal(int signum, void *fault_addr, void *pc, void *lr)
{
  Dl_info info;
  char pcoff[MAX_DPATH] = "unknown";
  char lroff[MAX_DPATH] = "unknown";
  char msg[MAX_DPATH * 3];
  int fd;

  if(dladdr(pc, &info) && info.dli_fbase)
    snprintf(pcoff, sizeof(pcoff) - 1, "0x%lx in %s",
      (unsigned long)((char *)pc - (char *)info.dli_fbase), info.dli_fname);
  if(dladdr(lr, &info) && info.dli_fbase)
    snprintf(lroff, sizeof(lroff) - 1, "0x%lx in %s",
      (unsigned long)((char *)lr - (char *)info.dli_fbase), info.dli_fname);

  snprintf(msg, sizeof(msg) - 1,
    "uae4arm crashed: signal %d at address %p\n  build = %s\n"
    "  PC offset = %s\n  LR offset = %s\n",
    signum, fault_addr, crash_build_id, pcoff, lroff);

#ifdef ANDROID
  __android_log_print(ANDROID_LOG_FATAL, "uae4arm", "%s", msg);
#endif

  if(crash_log_path[0]) {
    fd = open(crash_log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if(fd >= 0) {
      write(fd, msg, strlen(msg));
      close(fd);
    }
  }
}

/* Called where the handlers are installed, not from init_max_signals(): that one
   runs when emulation starts, and a crash in the GUI happens before it ever does. */
void init_crash_report(void)
{
  fetch_configurationpath(crash_log_path, MAX_DPATH - 32);
  strncat(crash_log_path, "uae4arm_crash.log", MAX_DPATH - 1);
  dl_iterate_phdr(find_build_id, NULL);
#ifdef ANDROID
  /* Also state it up front, so a plain logcat says which build is running -
     and so a report that says "unknown" can be told apart from a stale file. */
  __android_log_print(ANDROID_LOG_INFO, "uae4arm", "build id %s", crash_build_id);
#endif
}

void init_max_signals(void)
{
#ifdef WITH_LOGGING
  max_signals = 20;
#else
  max_signals = 200;  
#endif
}


#if defined(CPU_AARCH64)

#ifdef JIT
static int delete_trigger(blockinfo *bi, void *pc)
{
	while (bi) {
		if (bi->handler && (uae_u8*)bi->direct_handler <= pc &&	(uae_u8*)bi->nexthandler > pc) {
			output_log(_T("JIT: Deleted trigger (0x%016x < 0x%016x < 0x%016x) 0x%016x\n"),
				bi->handler, pc, bi->nexthandler, bi->pc_p);
			invalidate_block(bi);
			raise_in_cl_list(bi);
			countdown = 0;
			set_special(0);
			return 1;
		}
		bi = bi->next;
	}
	return 0;
}
#endif


typedef uae_u64 uintptr;

static int handle_exception(mcontext_t *sigcont, long fault_addr)  
{
  int handled = HANDLE_EXCEPTION_NONE;
	uintptr fault_pc = (uintptr)sigcont->pc;
  
	if (fault_pc == 0) {
    output_log(_T("PC is NULL.\n"));
	  return HANDLE_EXCEPTION_NONE;
  }

	// Check for exception in handler
	if (in_handler > 0) {
    output_log(_T("Segmentation fault in handler.\n"));
    return HANDLE_EXCEPTION_NONE;
  }
  ++in_handler;

#ifdef JIT
  for(;;) {
  	// We analyse only exceptions from JIT
    if(currprefs.cachesize == 0) {
      output_log(_T("JIT not in use.\n"));
      break;
    }

    // Did the error happens in compiled code?
  	if (fault_pc >= (uintptr)compiled_code && fault_pc < (uintptr)current_compile_p)
  	  output_log(_T("Error in compiled code.\n"));
  	else if(fault_pc >= (uintptr)popallspace && fault_pc < (uintptr)(popallspace + POPALLSPACE_SIZE))
  	  output_log(_T("Error in popallspace code.\n"));
  	else {
  	  output_log(_T("Error not in JIT code.\n"));
  		break;
  	}

    // Get Amiga address of illegal memory address
    long amiga_addr = (long) fault_addr - (long) regs.natmem_offset;
  
    // Check for stupid RAM detection of kickstart
    if(a3000lmem_bank.allocated_size > 0 && amiga_addr >= a3000lmem_bank.start - 0x00100000 && amiga_addr < a3000lmem_bank.start - 0x00100000 + 8) {
      output_log(_T("  Stupid kickstart detection for size of ramsey_low at 0x%08x.\n"), amiga_addr);
      sigcont->pc += 4;
      handled = HANDLE_EXCEPTION_A4000RAM;
      break;
    }
  
    // Check for stupid RAM detection of kickstart
    if(a3000hmem_bank.allocated_size > 0 && amiga_addr >= a3000hmem_bank.start + a3000hmem_bank.allocated_size && amiga_addr < a3000hmem_bank.start + a3000hmem_bank.allocated_size + 8) {
      output_log(_T("  Stupid kickstart detection for size of ramsey_high at 0x%08x.\n"), amiga_addr);
      sigcont->pc += 4;
      handled = HANDLE_EXCEPTION_A4000RAM;
      break;
    }
  
    // Get memory bank of address
  	addrbank *ab = &get_mem_bank(amiga_addr);
  	if (ab)
  		output_log(_T("JIT: Address bank: %s, address %08x\n"), ab->name ? ab->name : _T("NONE"), amiga_addr);
    
    // Analyse AARCH64 instruction
	  const unsigned int opcode = ((uae_u32*)fault_pc)[0];
		output_log(_T("JIT: AARCH64 opcode = 0x%08x\n"), opcode);
#ifdef JIT_DEBUG
extern void disam_range(void *start, void *stop);
    disam_range((void*)(fault_pc - 128), (void*)(fault_pc + 32));
#endif
    transfer_type_t transfer_type = TYPE_UNKNOWN;
	  int transfer_size = SIZE_UNKNOWN;

    unsigned int masked_op = opcode & 0xfffffc00;
    switch(masked_op) {
      case 0x383b6800: // STRB_wXx
				transfer_size = SIZE_BYTE;
				transfer_type = TYPE_STORE;
        break;

      case 0x783b6800: // STRH_wXx
				transfer_size = SIZE_WORD;
				transfer_type = TYPE_STORE;
        break;

      case 0xb83b6800: // STR_wXx
				transfer_size = SIZE_INT;
				transfer_type = TYPE_STORE;
        break;

      case 0x387b6800: // LDRB_wXx
				transfer_size = SIZE_BYTE;
				transfer_type = TYPE_LOAD;
        break;

      case 0x787b6800: // LDRH_wXx
				transfer_size = SIZE_WORD;
				transfer_type = TYPE_LOAD;
        break;

      case 0xb87b6800: // LDR_wXx
				transfer_size = SIZE_INT;
				transfer_type = TYPE_LOAD;
        break;

  		default:
  			output_log(_T("AARCH64: Handling of instruction 0x%08x not supported.\n"), opcode);
    }

  	if (transfer_size != SIZE_UNKNOWN) {
      // Get AARCH64 register
      int rd = opcode & 0x1f;

      output_log(_T("%s %s register x%d\n"), 
        transfer_size == SIZE_BYTE ? _T("byte") : transfer_size == SIZE_WORD ? _T("word") :	transfer_size == SIZE_INT ? _T("long") : _T("unknown"),
    		transfer_type == TYPE_LOAD ? _T("load to") : _T("store from"),
    		rd);

    	if (transfer_type == TYPE_LOAD) {
    	  // Perform load via indirect memory call
    	  uae_u32 oldval = sigcont->regs[rd];
    		switch(transfer_size) {
    		  case SIZE_BYTE:
    		    sigcont->regs[rd] = (uae_u8)get_byte(amiga_addr);
    		    break;

    		  case SIZE_WORD:
    		    sigcont->regs[rd] = bswap_16((uae_u16)get_word(amiga_addr));
    		    break;

    		  case SIZE_INT:
    		    sigcont->regs[rd] = bswap_32(get_long(amiga_addr));
    		    break;
    		}
    	  output_log(_T("New value in x%d: 0x%08x (old: 0x%08x)\n"), rd, sigcont->regs[rd], oldval);
    	} else {
    	  // Perform store via indirect memory call
    		switch(transfer_size) {
    		  case SIZE_BYTE: {
    		    put_byte(amiga_addr, sigcont->regs[rd]);
    		    break;
    		  }
    		  case SIZE_WORD: {
    		    put_word(amiga_addr, bswap_16(sigcont->regs[rd]));
    		    break;
    		  }
    		  case SIZE_INT: {
    		    put_long(amiga_addr, bswap_32(sigcont->regs[rd]));
    		    break;
    		  }
    		}
    	  output_log(_T("Stored value from x%d to 0x%08x\n"), rd, amiga_addr);
    	}
     	
     	// Go to next instruction
      sigcont->pc += 4;
    	handled = HANDLE_EXCEPTION_OK;
    	
    	if (!delete_trigger(active, (void*)fault_pc)) {
      	/* Not found in the active list. Might be a rom routine that
      	 * is in the dormant list */
      	delete_trigger(dormant, (void*)fault_pc);
    	}
    }

    break;
  }
#endif

  in_handler--;
	return handled;
} 

void signal_segv(int signum, siginfo_t* info, void*ptr) 
{
  int handled = HANDLE_EXCEPTION_NONE;
  ucontext_t *ucontext = (ucontext_t*)ptr;
  Dl_info dlinfo;

  output_log(_T("--- New exception ---\n"));

#ifdef TRACER
	trace_end();
#endif

	mcontext_t *context = &(ucontext->uc_mcontext);

	unsigned long long *regs = context->regs;
	uintptr addr = (uintptr)info->si_addr;

  handled = handle_exception(context, addr);

#if SHOW_DETAILS
  if(handled != HANDLE_EXCEPTION_A4000RAM) {
    if(signum == 4)
      output_log(_T("Illegal Instruction\n"));
    else
      output_log(_T("Segmentation Fault\n"));
  
    output_log(_T("info.si_signo = %d\n"), signum);
    output_log(_T("info.si_errno = %d\n"), info->si_errno);
    output_log(_T("info.si_code = %d\n"), info->si_code);
    output_log(_T("info.si_addr = %08x\n"), info->si_addr);
    if(signum == 4)
      output_log(_T("       value = 0x%08x\n"), *((uae_u32*)(info->si_addr)));

    for(int i=0; i < 31; ++i)
      output_log(_T("x%02d  = 0x%016lx\n"), i, ucontext->uc_mcontext.regs[i]);
    output_log(_T("SP  = 0x%016lx\n"), ucontext->uc_mcontext.sp);
    output_log(_T("PC  = 0x%016lx\n"), ucontext->uc_mcontext.pc);
    output_log(_T("Fault Address = 0x%016lx\n"), ucontext->uc_mcontext.fault_address);
    output_log(_T("pstate  = 0x%016lx\n"), ucontext->uc_mcontext.pstate);

    // Absolute addresses are useless with ASLR, print offsets within the
    // module so they can be fed to a symbolizer.
    {
      Dl_info off;
      void *pc = (void *)ucontext->uc_mcontext.pc;
      void *lr = (void *)ucontext->uc_mcontext.regs[30];
      if(dladdr(pc, &off) && off.dli_fbase)
        output_log(_T("PC offset = 0x%lx in %s\n"), (unsigned long)((char *)pc - (char *)off.dli_fbase), off.dli_fname);
      if(dladdr(lr, &off) && off.dli_fbase)
        output_log(_T("LR offset = 0x%lx in %s\n"), (unsigned long)((char *)lr - (char *)off.dli_fbase), off.dli_fname);
    }

    void *getaddr = (void *)ucontext->uc_mcontext.regs[30];
    if(dladdr(getaddr, &dlinfo))
  	  output_log(_T("LR - 0x%08X: <%s> (%s)\n"), getaddr, dlinfo.dli_sname, dlinfo.dli_fname);
    else
      output_log(_T("LR - 0x%08X: symbol not found\n"), getaddr);
  }
#endif

#if SHOW_DETAILS > 1
 if(handled != HANDLE_EXCEPTION_A4000RAM) {
  	output_log(_T("Stack trace:\n"));
  
    #define MAX_BACKTRACE 20
    
    void *array[MAX_BACKTRACE];
    int size = backtrace(array, MAX_BACKTRACE);
    for(int i=0; i<size; ++i)
    {
      if (dladdr(array[i], &dlinfo)) {
        const char *symname = dlinfo.dli_sname;
    	  output_log(_T("0x%08x <%s + 0x%08x> (%s)\n"), array[i], symname,
          (unsigned long)array[i] - (unsigned long)dlinfo.dli_saddr, dlinfo.dli_fname);
      }
    }
  
  	output_log(_T("Stack trace (non-dedicated):\n"));
    char **strings;
    void *bt[100];
    int sz = backtrace(bt, 100);
    strings = backtrace_symbols(bt, sz);
    for(int i = 0; i < sz; ++i)
      output_log(_T("%s\n"), strings[i]);
  	output_log(_T("End of stack trace.\n"));
  }
#endif

  output_log(_T("--- end exception ---\n"));

  /* Only count violations we could not emulate. A handled one means the JIT
     hit memory it cannot reach directly, we stepped in and the program carried
     on correctly - charging those to the budget makes the emulator give up on
     programs that simply use such memory a lot. An unhandled one resumes at
     the same instruction and would fault forever, which is what this guard is
     actually for. */
  if (handled == HANDLE_EXCEPTION_NONE) {
    --max_signals;
    if(max_signals <= 0) {
      target_startup_msg(_T("Exception"), _T("Too many access violations. Please turn off JIT."));
      uae_restart(1, NULL);
      return;
    }
  }

	if (handled != HANDLE_EXCEPTION_NONE)
	  return;

  report_fatal_signal(signum, info->si_addr, (void *)ucontext->uc_mcontext.pc,
    (void *)ucontext->uc_mcontext.regs[30]);
  SDL_Quit();
  exit(1);
}


void signal_buserror(int signum, siginfo_t* info, void*ptr) 
{
  ucontext_t *ucontext = (ucontext_t*)ptr;
  Dl_info dlinfo;

  output_log(_T("--- New exception ---\n"));

#ifdef TRACER
	trace_end();
#endif

	mcontext_t *context = &(ucontext->uc_mcontext);

	unsigned long long *regs = context->regs;
	uintptr_t addr = (uintptr_t)info->si_addr;

  output_log(_T("info.si_signo = %d\n"), signum);
  output_log(_T("info.si_errno = %d\n"), info->si_errno);
  output_log(_T("info.si_code = %d\n"), info->si_code);
  output_log(_T("info.si_addr = %08x\n"), info->si_addr);
  if(signum == 4)
    output_log(_T("       value = 0x%08x\n"), *((uae_u32*)(info->si_addr)));

  for(int i=0; i < 31; ++i)
    output_log(_T("x%02d  = 0x%016lx\n"), i, ucontext->uc_mcontext.regs[i]);
  output_log(_T("SP  = 0x%016lx\n"), ucontext->uc_mcontext.sp);
  output_log(_T("PC  = 0x%016lx\n"), ucontext->uc_mcontext.pc);
  output_log(_T("Fault Address = 0x%016lx\n"), ucontext->uc_mcontext.fault_address);
  output_log(_T("pstate  = 0x%016lx\n"), ucontext->uc_mcontext.pstate);

  // Absolute addresses are useless with ASLR, print offsets within the
  // module so they can be fed to a symbolizer.
  {
    Dl_info off;
    void *pc = (void *)ucontext->uc_mcontext.pc;
    void *lr = (void *)ucontext->uc_mcontext.regs[30];
    if(dladdr(pc, &off) && off.dli_fbase)
      output_log(_T("PC offset = 0x%lx in %s\n"), (unsigned long)((char *)pc - (char *)off.dli_fbase), off.dli_fname);
    if(dladdr(lr, &off) && off.dli_fbase)
      output_log(_T("LR offset = 0x%lx in %s\n"), (unsigned long)((char *)lr - (char *)off.dli_fbase), off.dli_fname);
  }

  void *getaddr = (void *)ucontext->uc_mcontext.regs[30];
  if(dladdr(getaddr, &dlinfo))
	  output_log(_T("LR - 0x%08X: <%s> (%s)\n"), getaddr, dlinfo.dli_sname, dlinfo.dli_fname);
  else
    output_log(_T("LR - 0x%08X: symbol not found\n"), getaddr);

	output_log(_T("Stack trace:\n"));

  #define MAX_BACKTRACE 20
  
  void *array[MAX_BACKTRACE];
  int size = backtrace(array, MAX_BACKTRACE);
  for(int i=0; i<size; ++i)
  {
    if (dladdr(array[i], &dlinfo)) {
      const char *symname = dlinfo.dli_sname;
  	  output_log(_T("0x%08x <%s + 0x%08x> (%s)\n"), array[i], symname,
        (unsigned long)array[i] - (unsigned long)dlinfo.dli_saddr, dlinfo.dli_fname);
    }
  }

	output_log(_T("Stack trace (non-dedicated):\n"));
  char **strings;
  void *bt[100];
  int sz = backtrace(bt, 100);
  strings = backtrace_symbols(bt, sz);
  for(int i = 0; i < sz; ++i)
    output_log(_T("%s\n"), strings[i]);
	output_log(_T("End of stack trace.\n"));

  output_log(_T("--- end exception ---\n"));

  report_fatal_signal(signum, info->si_addr, (void *)ucontext->uc_mcontext.pc,
    (void *)ucontext->uc_mcontext.regs[30]);
  SDL_Quit();
  exit(1);
}

#else

enum {
  ARM_REG_PC = 15,
  ARM_REG_CPSR = 16
};

static const char * reg_names[] = {
	"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
	"r8", "r9", "r10/sl", "r11/fp", "r12/ip", "r13/sp", "r14/lr", "r15/pc"
};


#ifdef JIT
static int delete_trigger(blockinfo *bi, void *pc)
{
	while (bi) {
		if (bi->handler && (uae_u8*)bi->direct_handler <= pc &&	(uae_u8*)bi->nexthandler > pc) {
			output_log(_T("JIT: Deleted trigger (0x%08x < 0x%08x < 0x%08x) 0x%08x\n"),
				bi->handler, pc, bi->nexthandler, bi->pc_p);
			invalidate_block(bi);
			raise_in_cl_list(bi);
			countdown = 0;
			set_special(0);
			return 1;
		}
		bi = bi->next;
	}
	return 0;
}
#endif


static int handle_exception(unsigned long *pregs, long fault_addr)  
{
  int handled = HANDLE_EXCEPTION_NONE;
	unsigned int *fault_pc = (unsigned int *)pregs[ARM_REG_PC];
  
	if (fault_pc == 0) {
    output_log(_T("PC is NULL.\n"));
	  return HANDLE_EXCEPTION_NONE;
  }

	// Check for exception in handler
	if (in_handler > 0) {
    output_log(_T("Segmentation fault in handler.\n"));
    return HANDLE_EXCEPTION_NONE;
  }
  ++in_handler;

#ifdef JIT
  for(;;) {
  	// We analyse only exceptions from JIT
    if(currprefs.cachesize == 0) {
      output_log(_T("JIT not in use.\n"));
      break;
    }

    // Did the error happens in compiled code?
  	if ((uae_u8*)fault_pc >= compiled_code && (uae_u8*)fault_pc < current_compile_p)
  	  output_log(_T("Error in compiled code.\n"));
  	else if((uae_u8*)fault_pc >= popallspace && (uae_u8*)fault_pc < popallspace + POPALLSPACE_SIZE)
  	  output_log(_T("Error in popallspace code.\n"));
  	else {
  	  output_log(_T("Error not in JIT code.\n"));
  		break;
  	}

    // Get Amiga address of illegal memory address
    long amiga_addr = (long) fault_addr - (long) regs.natmem_offset;
  
    // Check for stupid RAM detection of kickstart
    if(a3000lmem_bank.allocated_size > 0 && amiga_addr >= a3000lmem_bank.start - 0x00100000 && amiga_addr < a3000lmem_bank.start - 0x00100000 + 8) {
      output_log(_T("  Stupid kickstart detection for size of ramsey_low at 0x%08x.\n"), amiga_addr);
      pregs[ARM_REG_PC] += 4;
      handled = HANDLE_EXCEPTION_A4000RAM;
      break;
    }
  
    // Check for stupid RAM detection of kickstart
    if(a3000hmem_bank.allocated_size > 0 && amiga_addr >= a3000hmem_bank.start + a3000hmem_bank.allocated_size && amiga_addr < a3000hmem_bank.start + a3000hmem_bank.allocated_size + 8) {
      output_log(_T("  Stupid kickstart detection for size of ramsey_high at 0x%08x.\n"), amiga_addr);
      pregs[ARM_REG_PC] += 4;
      handled = HANDLE_EXCEPTION_A4000RAM;
      break;
    }
  
    // Get memory bank of address
  	addrbank *ab = &get_mem_bank(amiga_addr);
  	if (ab)
  		output_log(_T("JIT: Address bank: %s, address %08x\n"), ab->name ? ab->name : _T("NONE"), amiga_addr);
    
    // Analyse ARM instruction
	  const unsigned int opcode = fault_pc[0];
    transfer_type_t transfer_type = TYPE_UNKNOWN;
	  int transfer_size = SIZE_UNKNOWN;
	  int style = STYLE_UNSIGNED;
		output_log(_T("JIT: ARM opcode = 0x%08x\n"), opcode);
	  
  	// Handle load/store instructions only
	  switch ((opcode >> 25) & 7) {
		  case 0: // Halfword and Signed Data Transfer (LDRH, STRH, LDRSB, LDRSH)
			  // Determine transfer size (S/H bits)
  			switch ((opcode >> 5) & 3) {
  				case 0: // SWP instruction
  					output_log(_T("ARM: SWP Instruction, not supported (0x%08x)\n"), opcode);
  					break;
  				case 1: // Unsigned halfwords
	  				transfer_size = SIZE_WORD;
	  				break;
  				case 3: // Signed halfwords
  					style = STYLE_SIGNED;
	  				transfer_size = SIZE_WORD;
	  				break;
  				case 2: // Signed byte
  					style = STYLE_SIGNED;
				  	transfer_size = SIZE_BYTE;
	  				break;
  			}
  			break;

  		case 2:
	  	case 3: // Single Data Transfer (LDR, STR)
  			style = STYLE_UNSIGNED;
  			// Determine transfer size (B bit)
  			if (((opcode >> 22) & 1) == 1)
    			transfer_size = SIZE_BYTE;
  			else
    			transfer_size = SIZE_INT;
  			break;

  		default:
  			output_log(_T("ARM: Handling of instruction 0x%08x not supported.\n"), opcode);
  	}

  	// Determine transfer type (L bit)
  	if (((opcode >> 20) & 1) == 1)
  		transfer_type = TYPE_LOAD;
  	else
  		transfer_type = TYPE_STORE;

    // Get ARM register
    int rd = (opcode >> 12) & 0xf;

    output_log(_T("%s %s register %s\n"), 
      transfer_size == SIZE_BYTE ? _T("byte") : transfer_size == SIZE_WORD ? _T("word") :	transfer_size == SIZE_INT ? _T("long") : _T("unknown"),
  		transfer_type == TYPE_LOAD ? _T("load to") : _T("store from"),
  		reg_names[rd]);
    
  	if (transfer_size != SIZE_UNKNOWN) {
    	if (transfer_type == TYPE_LOAD) {
    	  // Perform load via indirect memory call
    	  uae_u32 oldval = pregs[rd];
    		switch(transfer_size) {
    		  case SIZE_BYTE:
    		    pregs[rd] = style == STYLE_SIGNED ? (uae_s8)get_byte(amiga_addr) : (uae_u8)get_byte(amiga_addr);
    		    break;

    		  case SIZE_WORD:
    		    pregs[rd] = bswap_16(style == STYLE_SIGNED ? (uae_s16)get_word(amiga_addr) : (uae_u16)get_word(amiga_addr));
    		    break;

    		  case SIZE_INT:
    		    pregs[rd] = bswap_32(get_long(amiga_addr));
    		    break;
    		}
    	  output_log(_T("New value in %s: 0x%08x (old: 0x%08x)\n"), reg_names[rd], pregs[rd], oldval);
    	} else {
    	  // Perform store via indirect memory call
    		switch(transfer_size) {
    		  case SIZE_BYTE: {
    		    put_byte(amiga_addr, pregs[rd]);
    		    break;
    		  }
    		  case SIZE_WORD: {
    		    put_word(amiga_addr, bswap_16(pregs[rd]));
    		    break;
    		  }
    		  case SIZE_INT: {
    		    put_long(amiga_addr, bswap_32(pregs[rd]));
    		    break;
    		  }
    		}
    	  output_log(_T("Stored value from %s to 0x%08x\n"), reg_names[rd], amiga_addr);
    	}
     	
     	// Go to next instruction
      pregs[ARM_REG_PC] += 4;
    	handled = HANDLE_EXCEPTION_OK;
    	
    	if (!delete_trigger(active, fault_pc)) {
      	/* Not found in the active list. Might be a rom routine that
      	 * is in the dormant list */
      	delete_trigger(dormant, fault_pc);
    	}
    }
  
    break;
  }
#endif

  in_handler--;
	return handled;
} 

void signal_segv(int signum, siginfo_t* info, void*ptr) 
{
  int handled = HANDLE_EXCEPTION_NONE;
  ucontext_t *ucontext = (ucontext_t*)ptr;
  Dl_info dlinfo;

  output_log(_T("--- New exception ---\n"));

#ifdef TRACER
	trace_end();
#endif

	mcontext_t *context = &(ucontext->uc_mcontext);

	unsigned long *regs = &context->arm_r0;
	uintptr addr = (uintptr)info->si_addr;

  handled = handle_exception(regs, addr);

#if SHOW_DETAILS
  if(handled != HANDLE_EXCEPTION_A4000RAM) {
    if(signum == 4)
      output_log(_T("Illegal Instruction\n"));
    else
      output_log(_T("Segmentation Fault\n"));
  
    output_log(_T("info.si_signo = %d\n"), signum);
    output_log(_T("info.si_errno = %d\n"), info->si_errno);
    output_log(_T("info.si_code = %d\n"), info->si_code);
    output_log(_T("info.si_addr = %08x\n"), info->si_addr);
    if(signum == 4)
      output_log(_T("       value = 0x%08x\n"), *((uae_u32*)(info->si_addr)));
    output_log(_T("r0  = 0x%08x\n"), ucontext->uc_mcontext.arm_r0);
    output_log(_T("r1  = 0x%08x\n"), ucontext->uc_mcontext.arm_r1);
    output_log(_T("r2  = 0x%08x\n"), ucontext->uc_mcontext.arm_r2);
    output_log(_T("r3  = 0x%08x\n"), ucontext->uc_mcontext.arm_r3);
    output_log(_T("r4  = 0x%08x\n"), ucontext->uc_mcontext.arm_r4);
    output_log(_T("r5  = 0x%08x\n"), ucontext->uc_mcontext.arm_r5);
    output_log(_T("r6  = 0x%08x\n"), ucontext->uc_mcontext.arm_r6);
    output_log(_T("r7  = 0x%08x\n"), ucontext->uc_mcontext.arm_r7);
    output_log(_T("r8  = 0x%08x\n"), ucontext->uc_mcontext.arm_r8);
    output_log(_T("r9  = 0x%08x\n"), ucontext->uc_mcontext.arm_r9);
    output_log(_T("r10 = 0x%08x\n"), ucontext->uc_mcontext.arm_r10);
    output_log(_T("FP  = 0x%08x\n"), ucontext->uc_mcontext.arm_fp);
    output_log(_T("IP  = 0x%08x\n"), ucontext->uc_mcontext.arm_ip);
    output_log(_T("SP  = 0x%08x\n"), ucontext->uc_mcontext.arm_sp);
    output_log(_T("LR  = 0x%08x\n"), ucontext->uc_mcontext.arm_lr);
    output_log(_T("PC  = 0x%08x\n"), ucontext->uc_mcontext.arm_pc);
    output_log(_T("CPSR = 0x%08x\n"), ucontext->uc_mcontext.arm_cpsr);
    output_log(_T("Fault Address = 0x%08x\n"), ucontext->uc_mcontext.fault_address);
    output_log(_T("Trap no = 0x%08x\n"), ucontext->uc_mcontext.trap_no);
    output_log(_T("Err Code = 0x%08x\n"), ucontext->uc_mcontext.error_code);
    output_log(_T("Old Mask = 0x%08x\n"), ucontext->uc_mcontext.oldmask);
  
    void *getaddr = (void *)ucontext->uc_mcontext.arm_lr;
    if(dladdr(getaddr, &dlinfo))
  	  output_log(_T("LR - 0x%08X: <%s> (%s)\n"), getaddr, dlinfo.dli_sname, dlinfo.dli_fname);
    else
      output_log(_T("LR - 0x%08X: symbol not found\n"), getaddr);
  }
#endif

#if SHOW_DETAILS > 1
 if(handled != HANDLE_EXCEPTION_A4000RAM) {
  	output_log(_T("Stack trace:\n"));
  
    #define MAX_BACKTRACE 20
    
    void *array[MAX_BACKTRACE];
    int size = backtrace(array, MAX_BACKTRACE);
    for(int i=0; i<size; ++i)
    {
      if (dladdr(array[i], &dlinfo)) {
        const char *symname = dlinfo.dli_sname;
    	  output_log(_T("0x%08x <%s + 0x%08x> (%s)\n"), array[i], symname,
          (unsigned long)array[i] - (unsigned long)dlinfo.dli_saddr, dlinfo.dli_fname);
      }
    }
  
    void *ip = (void*)ucontext->uc_mcontext.arm_r10;
    void **bp = (void**)ucontext->uc_mcontext.arm_r10;
    int f = 0;
    while(bp && ip) {
      if (!dladdr(ip, &dlinfo)) {
        output_log(_T("IP out of range\n"));
        break;
      }
      const char *symname = dlinfo.dli_sname;
  	  output_log(_T("%02d: 0x%08x <%s + 0x%08x> (%s)\n"), ++f, ip, symname,
        (unsigned long)ip - (unsigned long)dlinfo.dli_saddr, dlinfo.dli_fname);
  	  if(dlinfo.dli_sname && !strcmp(dlinfo.dli_sname, "main"))
        break;
      ip = bp[1];
      bp = (void**)bp[0];
    }
  
  	output_log(_T("Stack trace (non-dedicated):\n"));
    char **strings;
    void *bt[100];
    int sz = backtrace(bt, 100);
    strings = backtrace_symbols(bt, sz);
    for(int i = 0; i < sz; ++i)
      output_log(_T("%s\n"), strings[i]);
  	output_log(_T("End of stack trace.\n"));
  }
#endif

  output_log(_T("--- end exception ---\n"));

  /* Only count violations we could not emulate. A handled one means the JIT
     hit memory it cannot reach directly, we stepped in and the program carried
     on correctly - charging those to the budget makes the emulator give up on
     programs that simply use such memory a lot. An unhandled one resumes at
     the same instruction and would fault forever, which is what this guard is
     actually for. */
  if (handled == HANDLE_EXCEPTION_NONE) {
    --max_signals;
    if(max_signals <= 0) {
      target_startup_msg(_T("Exception"), _T("Too many access violations. Please turn off JIT."));
      uae_restart(1, NULL);
      return;
    }
  }

	if (handled != HANDLE_EXCEPTION_NONE)
	  return;

  report_fatal_signal(signum, info->si_addr, (void *)ucontext->uc_mcontext.arm_pc,
    (void *)ucontext->uc_mcontext.arm_lr);
  SDL_Quit();
  exit(1);
}


void signal_buserror(int signum, siginfo_t* info, void*ptr) 
{
  ucontext_t *ucontext = (ucontext_t*)ptr;
  Dl_info dlinfo;

  output_log(_T("--- New exception ---\n"));

#ifdef TRACER
	trace_end();
#endif

	mcontext_t *context = &(ucontext->uc_mcontext);

	unsigned long *regs = &context->arm_r0;
	uintptr_t addr = (uintptr_t)info->si_addr;

  output_log(_T("info.si_signo = %d\n"), signum);
  output_log(_T("info.si_errno = %d\n"), info->si_errno);
  output_log(_T("info.si_code = %d\n"), info->si_code);
  output_log(_T("info.si_addr = %08x\n"), info->si_addr);
  if(signum == 4)
    output_log(_T("       value = 0x%08x\n"), *((uae_u32*)(info->si_addr)));
  output_log(_T("r0  = 0x%08x\n"), ucontext->uc_mcontext.arm_r0);
  output_log(_T("r1  = 0x%08x\n"), ucontext->uc_mcontext.arm_r1);
  output_log(_T("r2  = 0x%08x\n"), ucontext->uc_mcontext.arm_r2);
  output_log(_T("r3  = 0x%08x\n"), ucontext->uc_mcontext.arm_r3);
  output_log(_T("r4  = 0x%08x\n"), ucontext->uc_mcontext.arm_r4);
  output_log(_T("r5  = 0x%08x\n"), ucontext->uc_mcontext.arm_r5);
  output_log(_T("r6  = 0x%08x\n"), ucontext->uc_mcontext.arm_r6);
  output_log(_T("r7  = 0x%08x\n"), ucontext->uc_mcontext.arm_r7);
  output_log(_T("r8  = 0x%08x\n"), ucontext->uc_mcontext.arm_r8);
  output_log(_T("r9  = 0x%08x\n"), ucontext->uc_mcontext.arm_r9);
  output_log(_T("r10 = 0x%08x\n"), ucontext->uc_mcontext.arm_r10);
  output_log(_T("FP  = 0x%08x\n"), ucontext->uc_mcontext.arm_fp);
  output_log(_T("IP  = 0x%08x\n"), ucontext->uc_mcontext.arm_ip);
  output_log(_T("SP  = 0x%08x\n"), ucontext->uc_mcontext.arm_sp);
  output_log(_T("LR  = 0x%08x\n"), ucontext->uc_mcontext.arm_lr);
  output_log(_T("PC  = 0x%08x\n"), ucontext->uc_mcontext.arm_pc);
  output_log(_T("CPSR = 0x%08x\n"), ucontext->uc_mcontext.arm_cpsr);
  output_log(_T("Trap no = 0x%08x\n"), ucontext->uc_mcontext.trap_no);
  output_log(_T("Err Code = 0x%08x\n"), ucontext->uc_mcontext.error_code);
  output_log(_T("Old Mask = 0x%08x\n"), ucontext->uc_mcontext.oldmask);
  output_log(_T("Fault Address = 0x%08x\n"), ucontext->uc_mcontext.fault_address);

  void *getaddr = (void *)ucontext->uc_mcontext.arm_lr;
  if(dladdr(getaddr, &dlinfo))
	  output_log(_T("LR - 0x%08X: <%s> (%s)\n"), getaddr, dlinfo.dli_sname, dlinfo.dli_fname);
  else
    output_log(_T("LR - 0x%08X: symbol not found\n"), getaddr);

	output_log(_T("Stack trace:\n"));

  #define MAX_BACKTRACE 20
  
  void *array[MAX_BACKTRACE];
  int size = backtrace(array, MAX_BACKTRACE);
  for(int i=0; i<size; ++i)
  {
    if (dladdr(array[i], &dlinfo)) {
      const char *symname = dlinfo.dli_sname;
  	  output_log(_T("0x%08x <%s + 0x%08x> (%s)\n"), array[i], symname,
        (unsigned long)array[i] - (unsigned long)dlinfo.dli_saddr, dlinfo.dli_fname);
    }
  }

  void *ip = (void*)ucontext->uc_mcontext.arm_r10;
  void **bp = (void**)ucontext->uc_mcontext.arm_r10;
  int f = 0;
  while(bp && ip) {
    if (!dladdr(ip, &dlinfo)) {
      output_log(_T("IP out of range\n"));
      break;
    }
    const char *symname = dlinfo.dli_sname;
	  output_log(_T("%02d: 0x%08x <%s + 0x%08x> (%s)\n"), ++f, ip, symname,
      (unsigned long)ip - (unsigned long)dlinfo.dli_saddr, dlinfo.dli_fname);
	  if(dlinfo.dli_sname && !strcmp(dlinfo.dli_sname, "main"))
      break;
    ip = bp[1];
    bp = (void**)bp[0];
  }

	output_log(_T("Stack trace (non-dedicated):\n"));
  char **strings;
  void *bt[100];
  int sz = backtrace(bt, 100);
  strings = backtrace_symbols(bt, sz);
  for(int i = 0; i < sz; ++i)
    output_log(_T("%s\n"), strings[i]);
	output_log(_T("End of stack trace.\n"));

  output_log(_T("--- end exception ---\n"));

  report_fatal_signal(signum, info->si_addr, (void *)ucontext->uc_mcontext.arm_pc,
    (void *)ucontext->uc_mcontext.arm_lr);
  SDL_Quit();
  exit(1);
}

#endif


void signal_term(int signum, siginfo_t* info, void*ptr) 
{
  output_log(_T("--- SIGTERM ---\n"));

#ifdef TRACER
	trace_end();
#endif

  SDL_Quit();
  exit(1);
}
