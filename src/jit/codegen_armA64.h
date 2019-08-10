/*
 * compiler/codegen_armA64.h - AARCH64 code generator
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
 */

#ifndef ARM_ASMA64_H
#define ARM_ASMA64_H


/* CPSR flags */

#define ARM_N_FLAG        0x80000000
#define ARM_Z_FLAG        0x40000000
#define ARM_C_FLAG        0x20000000
#define ARM_V_FLAG        0x10000000
#define ARM_CV_FLAGS      (ARM_C_FLAG|ARM_V_FLAG)


#define _W(c) emit_long(c)


#define MIN_EL0           0b011

/* read/write flags */
//#define MSR_NZCV_i(i) not available
#define MSR_NZCV_x(Xt)		_W((0b1101010100011 << 19) | (MIN_EL0 << 16) | (0b0100 << 12) | (0b0010 << 8) | (0b000 << 5) | (Xt))
#define MRS_NZCV_x(Xt)    _W((0b1101010100111 << 19) | (MIN_EL0 << 16) | (0b0100 << 12) | (0b0010 << 8) | (0b000 << 5) | (Xt))


/* Branch instructions */
#define B_i(i)            _W((0b000101 << 26) | ((i) & 0x03ffffff))
#define BL_i(i)           _W((0b100101 << 26) | ((i) & 0x03ffffff))
#define BLR_x(Xn)         _W((0b11010110001 << 21) | (0b11111 << 16) | (0 << 10) | ((Xn) << 5) | 0)
#define BR_x(Xn)          _W((0b11010110000 << 21) | (0b11111 << 16) | (0 << 10) | ((Xn) << 5) | 0)

#define CC_B_i(cc,i)      _W((0b01010100 << 24) | (((i) & 0x0007ffff) << 5) | (cc))
#define BEQ_i(i)          CC_B_i(NATIVE_CC_EQ,i)
#define BNE_i(i)          CC_B_i(NATIVE_CC_NE,i)
#define BCS_i(i)          CC_B_i(NATIVE_CC_CS,i)
#define BCC_i(i)          CC_B_i(NATIVE_CC_CC,i)
#define BMI_i(i)          CC_B_i(NATIVE_CC_MI,i)
#define BPL_i(i)          CC_B_i(NATIVE_CC_PL,i)
#define BVS_i(i)          CC_B_i(NATIVE_CC_VS,i)
#define BVC_i(i)          CC_B_i(NATIVE_CC_VC,i)
#define BHI_i(i)          CC_B_i(NATIVE_CC_HI,i)
#define BLS_i(i)          CC_B_i(NATIVE_CC_LS,i)
#define BGE_i(i)          CC_B_i(NATIVE_CC_GE,i)
#define BLT_i(i)          CC_B_i(NATIVE_CC_LT,i)
#define BGT_i(i)          CC_B_i(NATIVE_CC_GT,i)
#define BLE_i(i)          CC_B_i(NATIVE_CC_LE,i)

#define RET               _W((0b11010110010 << 21) | (0b11111 << 16) | (0 << 10) | ((30) << 5) | (0))


/* move immediate values */
#define MOV_xi16sh(Xd,i,sh)   _W((0b110100101 << 23) | ((sh/16) << 21) | (((i) & 0xffff) << 5) | (Xd))
#define MOVK_xi16sh(Xd,i,sh)  _W((0b111100101 << 23) | ((sh/16) << 21) | (((i) & 0xffff) << 5) | (Xd))
#define MVN_xi16sh(Xd,i,sh)   _W((0b100100101 << 23) | ((sh/16) << 21) | (((i) & 0xffff) << 5) | (Xd))
#define MOV_xx(Xd,Xm)         _W((0b10101010000 << 21) | ((Xm) << 16) | (0 << 10) | (0b11111 << 5) | (Xd))


/* signed/unsigned extent */
#define UBFM_xxii(Xd,Xn,immr,imms)   _W((0b1101001101 << 22) | ((immr) << 16) | ((imms) << 10) | ((Xn) << 5) | (Xd))
#define SBFM_xxii(Xd,Xn,immr,imms)   _W((0b1001001101 << 22) | ((immr) << 16) | ((imms) << 10) | ((Xn) << 5) | (Xd))

#define UXTB_xx(Xd,Xn)    UBFM_xxii(Xd,Xn,0,7)
#define UXTH_xx(Xd,Xn)    UBFM_xxii(Xd,Xn,0,15)
#define SXTB_xx(Xd,Xn)    SBFM_xxii(Xd,Xn,0,7)
#define SXTH_xx(Xd,Xn)    SBFM_xxii(Xd,Xn,0,15)


/* shift */
#define LSL_xxi(Xd,Xn,i)    _W((0b1101001101 << 22) | (((64-i)&0x3f) << 16) | ((63-i) << 10) | ((Xn) << 5) | (Xd))


/* load/store registers */
#define LDP_xxxi(Xt1,Xt2,Xn,i)       _W((0b1010100101 << 22) | (((i/8) & 0x7f) << 15) | ((Xt2) << 10) | ((Xn) << 5) | (Xt1))
#define STP_xxxi(Xt1,Xt2,Xn,i)       _W((0b1010100100 << 22) | (((i/8) & 0x7f) << 15) | ((Xt2) << 10) | ((Xn) << 5) | (Xt1))

#define LDR_wXi(Wt,Xn,i)  _W((0b1011100101 << 22) | (((i/4) & 0x1ff) << 10) | ((Xn) << 5) | (Wt))
#define LDR_xXi(Xt,Xn,i)  _W((0b1111100101 << 22) | (((i/4) & 0x1ff) << 10) | ((Xn) << 5) | (Xt))
#define LDR_xXxLSLi(Xt,Xn,Xm,i)   _W((0b11111000011 << 21) | ((Xm) << 16) | (0b011 << 13) | ((i) << 12) | (0b10 << 10) | ((Xn) << 5) | (Xt))
#define LDR_xXwLSLi(Xt,Xn,Wm,i)   _W((0b11111000011 << 21) | ((Wm) << 16) | (0b010 << 13) | ((i) << 12) | (0b10 << 10) | ((Xn) << 5) | (Xt))

#define STR_wXi(Wt,Xn,i)  _W((0b1011100100 << 22) | (((i/4) & 0x1ff) << 10) | ((Xn) << 5) | (Wt))
#define STR_xXi(Xt,Xn,i)  _W((0b1111100100 << 22) | (((i/4) & 0x1ff) << 10) | ((Xn) << 5) | (Xt))

#define LDR_xPCi(Xt,i)    _W((0b01011000 << 24) | (((i/4) & 0x7ffff) << 5) | (Xt))
#define LDRH_wXi(Xt,Xn,i) _W((0b0111100101 << 22) | (((i/2) &0xfff) << 10) | ((Xn) << 5) | (Xt))


/* ADD/SUB */
#define ADD_wwi(Wd,Wn,i)          _W((0b0001000100 << 22) | (((i) & 0xfff) << 10) | ((Wn) << 5) | (Wd))
#define ADD_xxi(Xd,Xn,i)          _W((0b1001000100 << 22) | (((i) & 0xfff) << 10) | ((Xn) << 5) | (Xd))
#define SUB_xxi(Xd,Xn,i)          _W((0b1101000100 << 22) | (((i) & 0xfff) << 10) | ((Xn) << 5) | (Xd))
#define SUBS_wwi(Wd,Wn,i)         _W((0b0111000100 << 22) | (((i) & 0xfff) << 10) | ((Wn) << 5) | (Wd))
#define SUBS_www(Wd,Wn,Wm)        _W((0b01101011000 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (Wd))
#define ADD_xxx(Xd,Xn,Xm)         _W((0b10001011000 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (Xd))
#define ADD_xxxLSLi(Xd,Xn,Xm,i)   _W((0b10001011000 << 21) | ((Xm) << 16) | (((i) & 0x3f) << 10) | ((Xn) << 5) | (Xd))


/* CMP/TST */
#define CMP_xx(Xn,Xm)             _W((0b11101011000 << 21) | ((Xm) << 16) | (0 << 10) | ((Xn) << 5) | (0b11111))
#define TST_ww(Wn,Wm)             _W((0b01101010000 << 21) | ((Wm) << 16) | (0 << 10) | ((Wn) << 5) | (0b11111))


/* misc */
#define BFC_xii(Xd,lsb,width)     _W((0b1011001101 << 22) | (((64-lsb)&0x3f) << 16) | ((width-1) << 10) | (0b11111 << 5) | (Xd))


#endif /* ARM_ASMA64_H */
