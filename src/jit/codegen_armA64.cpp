/*
 * compiler/codegen_arm.cpp - AARCH64 code generator
 *
 * Copyright (c) 2019 TomB
 * 
 * This file is part of the UAE4ARM project.
 *
 * JIT compiler m68k -> ARMv8.1
 *
 * Original 68040 JIT compiler for UAE, copyright 2000-2002 Bernd Meyer
 * This file is derived from CCG, copyright 1999-2003 Ian Piumarta
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Note: JIT for aarch64 only works if memory for Amiga part (regs.natmen_offset
 *       to additional_mem + ADDITIONAL_MEMSIZE + BARRIER) is allocated in lower
 *       32 bit address space. See alloc_AmigaMem() in generic_mem.cpp.
 */

#include "flags_arm.h"

/*************************************************************************
 * Some basic information about the the target CPU                       *
 *************************************************************************/

#define R0_INDEX 0
#define R1_INDEX 1
#define R2_INDEX 2
#define R3_INDEX 3
#define R4_INDEX 4
#define R5_INDEX 5
#define R6_INDEX 6
#define R7_INDEX 7
#define R8_INDEX  8
#define R9_INDEX  9
#define R10_INDEX 10
#define R11_INDEX 11
#define R12_INDEX 12
#define R13_INDEX 13
#define R14_INDEX 14
#define R15_INDEX 15
#define R16_INDEX 16
#define R17_INDEX 17
#define R18_INDEX 18
#define R27_INDEX 27
#define R28_INDEX 28

#define RSP_INDEX 31
#define RLR_INDEX 30
#define RFP_INDEX 29

/* The register in which subroutines return an integer return value */
#define REG_RESULT R0_INDEX

/* The registers subroutines take their first and second argument in */
#define REG_PAR1 R0_INDEX
#define REG_PAR2 R1_INDEX

#define REG_WORK1 R2_INDEX
#define REG_WORK2 R3_INDEX
#define REG_WORK3 R4_INDEX
#define REG_WORK4 R5_INDEX

#define REG_PC_TMP R1_INDEX /* Another register that is not the above */

#define R_MEMSTART 27
#define R_REGSTRUCT 28
uae_s8 always_used[] = {2,3,4,5,18,R_MEMSTART,R_REGSTRUCT,-1}; // r2-r5 are work register in emitted code, r18 special use reg

uae_u8 call_saved[] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1, 1,1,1,1, 1,1,1,1, 1,0,0,0};

/* This *should* be the same as call_saved. But:
   - We might not really know which registers are saved, and which aren't,
     so we need to preserve some, but don't want to rely on everyone else
     also saving those registers
   - Special registers (such like the stack pointer) should not be "preserved"
     by pushing, even though they are "saved" across function calls
   - r19 - r26 not in use, so no need to preserve
   - if you change need_to_preserve, modify raw_push_regs_to_preserve() and raw_pop_preserved_regs()
*/
static const uae_u8 need_to_preserve[] = {0,0,0,0, 0,0,1,1, 1,1,1,1, 1,1,1,1, 1,1,0,0, 0,0,0,0, 0,0,0,1, 1,0,0,0};
/* Do we really need to preserve r6-r15? They are scratch register in AArch64. If not, 
static const uae_u8 need_to_preserve[] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 1,1,0,0, 0,0,0,0, 0,0,0,1, 1,0,0,0};
*/

#include "codegen_armA64.h"


STATIC_INLINE void UNSIGNED8_IMM_2_REG(W4 r, IM8 v) {
	MOV_xi16sh(r, (uae_u16) v, 0);
}

STATIC_INLINE void SIGNED8_IMM_2_REG(W4 r, IM8 v) {
	uae_s16 v16 = (uae_s16)(uae_s8)v;
	if (v16 & 0x8000) {
		MVN_xi16sh(r, (uae_u16) ~v16, 0);
	} else {
		MOV_xi16sh(r, (uae_u16) v16, 0);
	}
}

STATIC_INLINE void UNSIGNED16_IMM_2_REG(W4 r, IM16 v) {
  MOV_xi16sh(r, v, 0);
}

STATIC_INLINE void SIGNED16_IMM_2_REG(W4 r, IM16 v) {
	if (v & 0x8000) {
		MVN_xi16sh(r, (uae_u16) ~v, 0);
	} else {
		MOV_xi16sh(r, (uae_u16) v, 0);
	}
}

STATIC_INLINE void UNSIGNED8_REG_2_REG(W4 d, RR4 s) {
	UXTB_xx(d, s);
}

STATIC_INLINE void SIGNED8_REG_2_REG(W4 d, RR4 s) {
	SXTB_xx(d, s);
}

STATIC_INLINE void UNSIGNED16_REG_2_REG(W4 d, RR4 s) {
	UXTH_xx(d, s);
}

STATIC_INLINE void SIGNED16_REG_2_REG(W4 d, RR4 s) {
	SXTH_xx(d, s);
}

#define ZERO_EXTEND_8_REG_2_REG(d,s) UNSIGNED8_REG_2_REG(d,s)
#define ZERO_EXTEND_16_REG_2_REG(d,s) UNSIGNED16_REG_2_REG(d,s)
#define SIGN_EXTEND_8_REG_2_REG(d,s) SIGNED8_REG_2_REG(d,s)
#define SIGN_EXTEND_16_REG_2_REG(d,s) SIGNED16_REG_2_REG(d,s)

STATIC_INLINE void LOAD_U32(int r, uae_u32 val)
{
  MOV_xi16sh(r, val, 0);
  if(val >> 16)
    MOVK_xi16sh(r, val >> 16, 16);
}

STATIC_INLINE void LOAD_U64(int r, uae_u64 val)
{
  MOV_xi16sh(r, val, 0);
  MOVK_xi16sh(r, val >> 16, 16);
  if(val >> 32) {
    MOVK_xi16sh(r, val >> 32, 32);
    MOVK_xi16sh(r, val >> 48, 48);
  }
}


#define NUM_PUSH_CMDS 8
#define NUM_POP_CMDS 8
/* NUMBER_REGS_TO_PRESERVE must be even to keep SP aligned to 16 */
#define NUMBER_REGS_TO_PRESERVE 14
STATIC_INLINE void raw_push_regs_to_preserve(void) {
  SUB_xxi(RSP_INDEX, RSP_INDEX, NUMBER_REGS_TO_PRESERVE*8);
	STP_xxxi(6, 7, RSP_INDEX, 0 * 8);
	STP_xxxi(8, 9, RSP_INDEX, 2 * 8);
	STP_xxxi(10, 11, RSP_INDEX, 4 * 8);
	STP_xxxi(12, 13, RSP_INDEX, 6 * 8);
	STP_xxxi(14, 15, RSP_INDEX, 8 * 8);
	STP_xxxi(16, 17, RSP_INDEX, 10 * 8);
	STP_xxxi(27, 28, RSP_INDEX, 12 * 8);
}

STATIC_INLINE void raw_pop_preserved_regs(void) {
	LDP_xxxi(6, 7, RSP_INDEX, 0 * 8);
	LDP_xxxi(8, 9, RSP_INDEX, 2 * 8);
	LDP_xxxi(10, 11, RSP_INDEX, 4 * 8);
	LDP_xxxi(12, 13, RSP_INDEX, 6 * 8);
	LDP_xxxi(14, 15, RSP_INDEX, 8 * 8);
	LDP_xxxi(16, 17, RSP_INDEX, 10 * 8);
	LDP_xxxi(27, 28, RSP_INDEX, 12 * 8);
  ADD_xxi(RSP_INDEX, RSP_INDEX, NUMBER_REGS_TO_PRESERVE*8);
}
/* If it's not required to preserve r6-r15:
#define NUM_PUSH_CMDS 3
#define NUM_POP_CMDS 3
#define NUMBER_REGS_TO_PRESERVE 4 
STATIC_INLINE void raw_push_regs_to_preserve(void) {
  SUB_xxi(RSP_INDEX, RSP_INDEX, NUMBER_REGS_TO_PRESERVE*8);
	STP_xxxi(16, 17, RSP_INDEX, 0 * 8);
	STP_xxxi(27, 28, RSP_INDEX, 2 * 8);
}

STATIC_INLINE void raw_pop_preserved_regs(void) {
	LDP_xxxi(16, 17, RSP_INDEX, 0 * 8);
	LDP_xxxi(27, 28, RSP_INDEX, 2 * 8);
  ADD_xxi(RSP_INDEX, RSP_INDEX, NUMBER_REGS_TO_PRESERVE*8);
}
*/

STATIC_INLINE void raw_flags_evicted(int r)
{
  live.state[FLAGTMP].status = INMEM;
  live.state[FLAGTMP].realreg = -1;
  /* We just "evicted" FLAGTMP. */
  if (live.nat[r].nholds != 1) {
    /* Huh? */
    abort();
  }
  live.nat[r].nholds = 0;
}

STATIC_INLINE void raw_flags_to_reg(int r)
{
  uintptr idx = (uintptr) &(regs.ccrflags.nzcv) - (uintptr) &regs;
	MRS_NZCV_x(r);
	STR_wXi(r, R_REGSTRUCT, idx);
	raw_flags_evicted(r);
}

STATIC_INLINE void raw_reg_to_flags(int r)
{
	MSR_NZCV_x(r);
}


//
// compuemu_support used raw calls
//
LOWFUNC(WRITE,RMW,2,compemu_raw_inc_opcount,(IM16 op))
{
  uintptr idx = (uintptr) &(regs.raw_cputbl_count) - (uintptr) &regs;
  LDR_xXi(REG_WORK2, R_REGSTRUCT, idx);
  MOV_xi16sh(REG_WORK1, op, 0);
  ADD_xxxLSLi(REG_WORK2, REG_WORK2, REG_WORK1, 2);
  LDR_wXi(REG_WORK1, REG_WORK2, 0);

  ADD_wwi(REG_WORK1, REG_WORK1, 1);

  STR_wXi(REG_WORK1, REG_WORK2, 0);
}
LENDFUNC(WRITE,RMW,1,compemu_raw_inc_opcount,(IM16 op))

LOWFUNC(WRITE,READ,1,compemu_raw_cmp_pc,(IMPTR s))
{
  /* s is always >= NATMEM_OFFSETX and < NATMEM_OFFSETX + max. Amiga mem */
  clobber_flags();
  
  uintptr idx = (uintptr) &(regs.pc_p) - (uintptr) &regs;
  LDR_xXi(REG_WORK1, R_REGSTRUCT, idx); // regs.pc_p is 64 bit
  
  LOAD_U64(REG_WORK2, s);
	CMP_xx(REG_WORK1, REG_WORK2);
}
LENDFUNC(WRITE,READ,1,compemu_raw_cmp_pc,(IMPTR s))

LOWFUNC(NONE,NONE,3,compemu_raw_lea_l_brr,(W4 d, RR4 s, IM32 offset))
{
  LOAD_U32(REG_WORK1, offset);
	ADD_xxx(d, s, REG_WORK1);
}
LENDFUNC(NONE,NONE,3,compemu_raw_lea_l_brr,(W4 d, RR4 s, IM32 offset))

LOWFUNC(NONE,WRITE,1,compemu_raw_set_pc_i,(IMPTR s))
{
  /* s is always >= NATMEM_OFFSETX and < NATMEM_OFFSETX + max. Amiga mem */
  LOAD_U64(REG_WORK2, s);
  uintptr idx = (uintptr) &(regs.pc_p) - (uintptr) &regs;
  STR_xXi(REG_WORK2, R_REGSTRUCT, idx);
}
LENDFUNC(NONE,WRITE,1,compemu_raw_set_pc_i,(IMPTR s))

LOWFUNC(NONE,WRITE,2,compemu_raw_mov_l_mi,(MEMW d, IM32 s))
{
  /* d points always to memory in regs struct */
  LOAD_U32(REG_WORK2, s);
  uintptr idx = d - (uintptr) & regs;
  if(d == (uintptr) &(regs.pc_p))
    STR_xXi(REG_WORK2, R_REGSTRUCT, idx);
  else
    STR_wXi(REG_WORK2, R_REGSTRUCT, idx);
}
LENDFUNC(NONE,WRITE,2,compemu_raw_mov_l_mi,(MEMW d, IM32 s))

LOWFUNC(NONE,WRITE,1,compemu_raw_set_pc_r,(RR4 s))
{
  uintptr idx = (uintptr) &(regs.pc_p) - (uintptr) &regs;
  STR_xXi(s, R_REGSTRUCT, idx);
}
LENDFUNC(NONE,WRITE,1,compemu_raw_set_pc_r,(RR4 s))

LOWFUNC(NONE,WRITE,2,compemu_raw_mov_l_mr,(MEMW d, RR4 s))
{
  /* d points always to memory in regs struct */
  uintptr idx = d - (uintptr) & regs;
  if(d == (uintptr) &(regs.pc_p))
    STR_xXi(s, R_REGSTRUCT, idx);
  else
    STR_wXi(s, R_REGSTRUCT, idx);
}
LENDFUNC(NONE,WRITE,2,compemu_raw_mov_l_mr,(MEMW d, RR4 s))

LOWFUNC(NONE,NONE,2,compemu_raw_mov_l_ri,(W4 d, IM32 s))
{
  LOAD_U32(d, s);
}
LENDFUNC(NONE,NONE,2,compemu_raw_mov_l_ri,(W4 d, IM32 s))

LOWFUNC(NONE,READ,2,compemu_raw_mov_l_rm,(W4 d, MEMR s))
{
  if(s >= (uintptr) &regs && s < ((uintptr) &regs) + sizeof(struct regstruct)) {
    uintptr idx = s - (uintptr) & regs;
    if(s == (uintptr) &(regs.pc_p))
      LDR_xXi(d, R_REGSTRUCT, idx);
    else
      LDR_wXi(d, R_REGSTRUCT, idx);
  } else {
    LOAD_U64(REG_WORK1, s);
	  LDR_xXi(d, REG_WORK1, 0);
  }
}
LENDFUNC(NONE,READ,2,compemu_raw_mov_l_rm,(W4 d, MEMR s))

LOWFUNC(NONE,NONE,2,compemu_raw_mov_l_rr,(W4 d, RR4 s))
{
	MOV_xx(d, s);
}
LENDFUNC(NONE,NONE,2,compemu_raw_mov_l_rr,(W4 d, RR4 s))

LOWFUNC(WRITE,RMW,1,compemu_raw_dec_m,(MEMRW d))
{
  clobber_flags();

  LOAD_U64(REG_WORK1, d);
  LDR_wXi(REG_WORK2, REG_WORK1, 0);
  SUBS_wwi(REG_WORK2, REG_WORK2, 1);
  STR_wXi(REG_WORK2, REG_WORK1, 0);
}
LENDFUNC(WRITE,RMW,1,compemu_raw_dec_m,(MEMRW ds))

LOWFUNC(WRITE,RMW,2,compemu_raw_sub_l_mi,(MEMRW d, IM32 s))
{
  /* d points always to memory in regs struct */
  clobber_flags();

  uintptr idx = d - (uintptr) & regs;
  LDR_wXi(REG_WORK2, R_REGSTRUCT, idx);

  if((s & ~0xfff) == 0) {
    SUBS_wwi(REG_WORK2, REG_WORK2, s);
  } else {
    LOAD_U32(REG_WORK1, s);
	  SUBS_www(REG_WORK2, REG_WORK2, REG_WORK1);
  }
  STR_wXi(REG_WORK2, R_REGSTRUCT, idx);
}
LENDFUNC(WRITE,RMW,2,compemu_raw_sub_l_mi,(MEMRW d, IM32 s))

LOWFUNC(WRITE,NONE,2,compemu_raw_test_l_rr,(RR4 d, RR4 s))
{
  clobber_flags();
	TST_ww(d, s);
}
LENDFUNC(WRITE,NONE,2,compemu_raw_test_l_rr,(RR4 d, RR4 s))

STATIC_INLINE void compemu_raw_call(uintptr t)
{
  LOAD_U64(REG_WORK1, t);

  SUB_xxi(RSP_INDEX, RSP_INDEX, 16);
	STR_xXi(RLR_INDEX, RSP_INDEX, 0);
	BLR_x(REG_WORK1);
	LDR_xXi(RLR_INDEX, RSP_INDEX, 0);
  ADD_xxi(RSP_INDEX, RSP_INDEX, 16);
}

STATIC_INLINE void compemu_raw_call_r(RR4 r)
{
  SUB_xxi(RSP_INDEX, RSP_INDEX, 16);
	STR_xXi(RLR_INDEX, RSP_INDEX, 0);
	BLR_x(r);
	LDR_xXi(RLR_INDEX, RSP_INDEX, 0);
  ADD_xxi(RSP_INDEX, RSP_INDEX, 16);
}

STATIC_INLINE void compemu_raw_jcc_l_oponly(int cc)
{
	switch (cc) {
		case NATIVE_CC_HI: // HI
			BEQ_i(2);										// beq no jump
			BCC_i(0);										// bcc jump
			break;

		case NATIVE_CC_LS: // LS
			BEQ_i(2);										// beq jump
			BCC_i(2);										// bcc no jump
			// jump
			B_i(0); 
			// no jump
			break;

		case NATIVE_CC_F_OGT: // Jump if valid and greater than
			BVS_i(2);		// do not jump if NaN
			BGT_i(0);		// jump if greater than
			break;

		case NATIVE_CC_F_OGE: // Jump if valid and greater or equal
			BVS_i(2);		// do not jump if NaN
			BCS_i(0);		// jump if carry set
			break;
			
		case NATIVE_CC_F_OLT: // Jump if vaild and less than
			BVS_i(2);		// do not jump if NaN
			BCC_i(0);		// jump if carry cleared
			break;
			
		case NATIVE_CC_F_OLE: // Jump if valid and less or equal
			BVS_i(2);		// do not jump if NaN
			BLE_i(0);		// jump if less or equal
			break;
			
		case NATIVE_CC_F_OGL: // Jump if valid and greator or less
			BVS_i(2);		// do not jump if NaN
			BNE_i(0);		// jump if not equal
			break;

		case NATIVE_CC_F_OR: // Jump if valid
			BVC_i(0);
			break;
			
		case NATIVE_CC_F_UN: // Jump if NAN
			BVS_i(0); 
			break;

		case NATIVE_CC_F_UEQ: // Jump if NAN or equal
			BVS_i(2); 	// jump if NaN
			BNE_i(2);		// do not jump if greater or less
			// jump
			B_i(0); 
			break;

		case NATIVE_CC_F_UGT: // Jump if NAN or greater than
			BVS_i(2); 	// jump if NaN
			BLS_i(2);		// do not jump if lower or same
			// jump
			B_i(0); 
			break;

		case NATIVE_CC_F_UGE: // Jump if NAN or greater or equal
			BVS_i(2); 	// jump if NaN
			BMI_i(2);		// do not jump if lower
			// jump
			B_i(0); 
			break;

		case NATIVE_CC_F_ULT: // Jump if NAN or less than
			BVS_i(2); 	// jump if NaN
			BGE_i(2);		// do not jump if greater or equal
			// jump
			B_i(0); 
			break;

		case NATIVE_CC_F_ULE: // Jump if NAN or less or equal
			BVS_i(2); 	// jump if NaN
			BGT_i(2);		// do not jump if greater
			// jump
			B_i(0); 
			break;
	
		default:
	    CC_B_i(cc, 0);
			break;
	}
  // emit of target into last branch will be done by caller
}

STATIC_INLINE void compemu_raw_handle_except(IM32 cycles)
{
	uae_u32* branchadd;	
	uintptr offs;

  clobber_flags();

  uintptr idx = (uintptr)(&regs.jit_exception) - (uintptr)(&regs);
  LDR_wXi(REG_WORK1, R_REGSTRUCT, idx);
	TST_ww(REG_WORK1, REG_WORK1);

	branchadd = (uae_u32*)get_target();
	BEQ_i(0);		// no exception, jump to next instruction
	
  // countdown -= scaled_cycles(totcycles);
  offs = (uintptr)&countdown - (uintptr)&regs;
	LDR_wXi(REG_WORK1, R_REGSTRUCT, offs);
  if((cycles & ~0xfff) == 0) {
	  SUBS_wwi(REG_WORK1, REG_WORK1, cycles);
	} else {
    LOAD_U32(REG_WORK2, cycles);
  	SUBS_www(REG_WORK1, REG_WORK1, REG_WORK2);
  }
	STR_wXi(REG_WORK1, R_REGSTRUCT, offs);

  raw_pop_preserved_regs();
  LDR_xPCi(REG_WORK1, 12); // <execute_exception>
  BR_x(REG_WORK1);
	emit_longlong((uintptr)execute_exception);
	
	// Write target of next instruction
	write_jmp_target(branchadd, (uintptr)get_target());
}

STATIC_INLINE void compemu_raw_maybe_recompile(uintptr t)
{
  BGE_i(NUM_POP_CMDS + 5);
  raw_pop_preserved_regs();
  LDR_xPCi(REG_WORK1, 12); 
  BR_x(REG_WORK1);
  emit_longlong(t);
}

STATIC_INLINE void compemu_raw_jmp(uintptr t)
{
  LDR_xPCi(REG_WORK1, 12);
  BR_x(REG_WORK1);
  emit_longlong(t);
}

STATIC_INLINE void compemu_raw_jmp_m_indexed(uintptr base, uae_u32 r, uae_u32 m)
{
	int shft=0;
	if(m)
	  shft=1;

  LDR_xPCi(REG_WORK1, 16);
	LDR_xXwLSLi(REG_WORK1, REG_WORK1, r, shft);
	BR_x(REG_WORK1);
	emit_longlong(base);
}

STATIC_INLINE void compemu_raw_maybe_cachemiss(uintptr t)
{
  BEQ_i(NUM_POP_CMDS + 5);
  raw_pop_preserved_regs();
  LDR_xPCi(REG_WORK1, 12);
  BR_x(REG_WORK1);
  emit_longlong(t);
}

STATIC_INLINE void compemu_raw_jz_b_oponly(void)
{
  BEQ_i(0);   // Real distance set by caller
}

// Optimize access to struct regstruct with and memory with fixed registers

LOWFUNC(NONE,NONE,1,compemu_raw_init_r_regstruct,(IMPTR s))
{
  LOAD_U64(R_REGSTRUCT, s);
	uintptr offsmem = (uintptr)&NATMEM_OFFSETX - (uintptr) &regs;
  LDR_xXi(R_MEMSTART, R_REGSTRUCT, offsmem);
}
LENDFUNC(NONE,NONE,1,compemu_raw_init_r_regstruct,(IMPTR s))

// Handle end of compiled block
LOWFUNC(NONE,NONE,2,compemu_raw_endblock_pc_inreg,(RR4 rr_pc, IM32 cycles))
{
  clobber_flags();

  // countdown -= scaled_cycles(totcycles);
  uintptr offs = (uintptr)&countdown - (uintptr)&regs;
	LDR_wXi(REG_WORK1, R_REGSTRUCT, offs);
  if((cycles & ~0xfff) == 0) {
	  SUBS_wwi(REG_WORK1, REG_WORK1, cycles);
	} else {
	  LOAD_U32(REG_WORK2, cycles);
  	SUBS_www(REG_WORK1, REG_WORK1, REG_WORK2);
  }
	STR_wXi(REG_WORK1, R_REGSTRUCT, offs);

	BMI_i(7);
	BFC_xii(rr_pc, 16, 48); // apply TAGMASK
  LDR_xPCi(REG_WORK1, 16); // <cache_tags>
	LDR_xXxLSLi(REG_WORK1, REG_WORK1, rr_pc, 2);
  BR_x(REG_WORK1);
	emit_longlong((uintptr)cache_tags);

  raw_pop_preserved_regs();
  LDR_xPCi(REG_WORK1, 12); // <do_nothing>
  BR_x(REG_WORK1);

	emit_longlong((uintptr)do_nothing);
}
LENDFUNC(NONE,NONE,2,compemu_raw_endblock_pc_inreg,(RR4 rr_pc, IM32 cycles))

STATIC_INLINE uae_u32* compemu_raw_endblock_pc_isconst(IM32 cycles, IMPTR v)
{
  /* v is always >= NATMEM_OFFSETX and < NATMEM_OFFSETX + max. Amiga mem */
	uae_u32* tba;
  clobber_flags();

  // countdown -= scaled_cycles(totcycles);
  uintptr offs = (uintptr)&countdown - (uintptr)&regs;
	LDR_wXi(REG_WORK1, R_REGSTRUCT, offs);
  if((cycles & ~0xfff) == 0) {
	  SUBS_wwi(REG_WORK1, REG_WORK1, cycles);
	} else {
	  LOAD_U32(REG_WORK2, cycles);
  	SUBS_www(REG_WORK1, REG_WORK1, REG_WORK2);
  }
	STR_wXi(REG_WORK1, R_REGSTRUCT, offs);

	tba = (uae_u32*)get_target();
  CC_B_i(NATIVE_CC_MI^1, 0); // <target set by caller>
  
  raw_pop_preserved_regs();
  LDR_xPCi(REG_WORK1, 20); // <v>
  offs = (uintptr)&regs.pc_p - (uintptr)&regs;
  STR_xXi(REG_WORK1, R_REGSTRUCT, offs);
  LDR_xPCi(REG_WORK1, 20); // <do_nothing>
  BR_x(REG_WORK1);

	emit_longlong(v);
	emit_longlong((uintptr)do_nothing);

	return tba;  
}

LOWFUNC(NONE,READ,2,compemu_raw_tag_pc,(W4 d, MEMR s))
{
  uintptr idx = (uintptr)(s) - (uintptr)&regs;
  LDRH_wXi(d, R_REGSTRUCT, idx);
}
LENDFUNC(NONE,READ,2,compemu_raw_tag_pc,(W4 d, MEMR s))
