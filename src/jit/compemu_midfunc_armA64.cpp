/*
 * compiler/compemu_midfunc_armA64.cpp - Native MIDFUNCS for AARCH64
 *
 * Copyright (c) 2019 TomB
 *
 * Inspired by Christian Bauer's Basilisk II
 *
 *  Original 68040 JIT compiler for UAE, copyright 2000-2002 Bernd Meyer
 *
 *  Adaptation for Basilisk II and improvements, copyright 2000-2002
 *    Gwenole Beauchesne
 *
 *  Basilisk II (C) 1997-2002 Christian Bauer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Note:
 *  	File is included by compemu_support.cpp
 *
 */

/********************************************************************
 * CPU functions exposed to gencomp. Both CREATE and EMIT time      *
 ********************************************************************/

/*
 *  RULES FOR HANDLING REGISTERS:
 *
 *  * In the function headers, order the parameters
 *     - 1st registers written to
 *     - 2nd read/modify/write registers
 *     - 3rd registers read from
 *  * Before calling raw_*, you must call readreg, writereg or rmw for
 *    each register
 *  * The order for this is
 *     - 1nd call readreg for all registers read without offset
 *     - 2rd call rmw for all rmw registers
 *     - 3th call writereg for all written-to registers
 *     - 4th call raw_*
 *     - 5th unlock2 all registers that were locked
 */

#define INIT_REGS_b(d,s) 					\
	int targetIsReg = (d < 16); 		\
	int s_is_d = (s == d);  				\
	if(!s_is_d) 										\
		s = readreg(s); 							\
	d = rmw(d);											\
	if(s_is_d)											\
		s = d;

#define INIT_RREGS_b(d,s) 				\
	int targetIsReg = (d < 16); 		\
	int s_is_d = (s == d);  				\
	if(!s_is_d) 										\
		s = readreg(s); 							\
	d = readreg(d);									\
	if(s_is_d)											\
		s = d;

#define INIT_REG_b(d) 						\
	int targetIsReg = (d < 16); 		\
	d = rmw(d);

#define INIT_RREG_b(d) 						\
	int targetIsReg = (d < 16); 		\
	d = readreg(d);

#define INIT_WREG_b(d) 						\
	int targetIsReg = (d < 16); 		\
	if(targetIsReg)									\
		d = rmw(d); 									\
	else														\
		d = writereg(d);

#define INIT_REGS_w(d,s) 					\
	int targetIsReg = (d < 16); 		\
	int s_is_d = (s == d);  				\
	if(!s_is_d) 										\
		s = readreg(s); 							\
	d = rmw(d);											\
	if(s_is_d)											\
		s = d;

#define INIT_RREGS_w(d,s)					\
	int targetIsReg = (d < 16); 		\
	int s_is_d = (s == d);  				\
	if(!s_is_d) 										\
		s = readreg(s); 							\
	d = readreg(d);									\
	if(s_is_d)											\
		s = d;

#define INIT_REG_w(d) 						\
	int targetIsReg = (d < 16); 		\
	d = rmw(d);

#define INIT_RREG_w(d) 						\
	int targetIsReg = (d < 16); 		\
	d = readreg(d);

#define INIT_WREG_w(d) 						\
	int targetIsReg = (d < 16); 		\
	if(targetIsReg)									\
		d = rmw(d);										\
	else														\
		d = writereg(d);

#define INIT_REGS_l(d,s) 					\
	int targetIsReg = (d < 16); 		\
	int s_is_d = (s == d);  				\
	if(!s_is_d) 										\
		s = readreg(s); 							\
	d = rmw(d);											\
	if(s_is_d)											\
		s = d;

#define INIT_RREGS_l(d,s) 				\
	int targetIsReg = (d < 16); 		\
	int s_is_d = (s == d);  				\
	if(!s_is_d) 										\
		s = readreg(s); 							\
	d = readreg(d);									\
	if(s_is_d)											\
		s = d;

#define EXIT_REGS(d,s)						\
	unlock2(d);											\
	if(!s_is_d)											\
		unlock2(s);
	

MIDFUNC(0,live_flags,(void))
{
	live.flags_on_stack = TRASH;
	live.flags_in_flags = VALID;
	live.flags_are_important = 1;
}
MENDFUNC(0,live_flags,(void))

MIDFUNC(0,dont_care_flags,(void))
{
	live.flags_are_important = 0;
}
MENDFUNC(0,dont_care_flags,(void))

MIDFUNC(0,make_flags_live,(void))
{
	make_flags_live_internal();
}
MENDFUNC(0,make_flags_live,(void))

MIDFUNC(2,mov_l_mi,(IMPTR d, IM32 s))
{
  /* d points always to memory in regs struct */
  LOAD_U32(REG_WORK2, s);
  uintptr idx = d - (uintptr) &regs;
  if(d == (uintptr) &(regs.pc_p) || d == (uintptr) &(regs.pc_oldp))
    STR_xXi(REG_WORK2, R_REGSTRUCT, idx);
  else
    STR_wXi(REG_WORK2, R_REGSTRUCT, idx);
}
MENDFUNC(2,mov_l_mi,(IMPTR d, IM32 s))

MIDFUNC(4,disp_ea20_target_add,(RW4 target, RR4 reg, IM8 shift, IM8 extend))
{
	if(isconst(target) && isconst(reg)) {
		if(extend)
			set_const(target, live.state[target].val + (((uae_s32)(uae_s16)live.state[reg].val) << (shift & 0x1f)));
		else
			set_const(target, live.state[target].val + (live.state[reg].val << (shift & 0x1f)));
		return;
	}

	reg = readreg(reg);
	target = rmw(target);
	
	if(extend) {
		SIGNED16_REG_2_REG(REG_WORK1, reg);
		ADD_xxxLSLi(target, target, REG_WORK1, shift & 0x1f);
	} else {
		ADD_xxxLSLi(target, target, reg, shift & 0x1f);
	}
	
	unlock2(target);
	unlock2(reg);
}
MENDFUNC(4,disp_ea20_target_add,(RW4 target, RR4 reg, IM8 shift, IM8 extend))

MIDFUNC(4,disp_ea20_target_mov,(W4 target, RR4 reg, IM8 shift, IM8 extend))
{
	if(isconst(reg)) {
		if(extend)
			set_const(target, ((uae_s32)(uae_s16)live.state[reg].val) << (shift & 0x1f));
		else
			set_const(target, live.state[reg].val << (shift & 0x1f));
		return;
	}

	reg = readreg(reg);
	target = writereg(target);
	
	if(extend) {
		SIGNED16_REG_2_REG(REG_WORK1, reg);
		LSL_xxi(target, REG_WORK1, shift & 0x1f);
	} else {
		LSL_xxi(target, reg, shift & 0x1f);
	}
	
	unlock2(target);
	unlock2(reg);
}
MENDFUNC(4,disp_ea20_target_mov,(W4 target, RR4 reg, IM8 shift, IM8 extend))




STATIC_INLINE void flush_cpu_icache(void *start, void *stop)
{
  __builtin___clear_cache(start, stop);
}


STATIC_INLINE void write_jmp_target(uae_u32* jmpaddr, uintptr a) 
{
	uintptr off = ((uintptr)a - (uintptr)jmpaddr) >> 2;
  if((*(jmpaddr) & 0xfc000000) == 0x14000000)
    /* branch always */
    *(jmpaddr) = (*(jmpaddr) & 0xfc000000) | (off & 0x03ffffff);
  else
    /* conditional branch */
    *(jmpaddr) = (*(jmpaddr) & 0xff00001f) | ((off << 5) & 0x00ffffe0);

  flush_cpu_icache((void *)jmpaddr, (void *)&jmpaddr[1]);
}
