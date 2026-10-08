/*
 * Plain C versions of the screen conversions that aarch64_helper.s and
 * arm_helper.s do in assembly, for the Linux PC build (LinuxBuild.sh).
 * Same arguments and results; bytes counts the source.
 */

#include "sysconfig.h"
#include "sysdeps.h"

extern "C" {

void copy_screen_8bit_to_16bit(uae_u8 *dst, uae_u8 *src, int bytes, uae_u32 *clut)
{
	uae_u16 *d = (uae_u16 *)dst;
	for (int i = 0; i < bytes; i++)
		d[i] = (uae_u16)clut[src[i]];
}

void copy_screen_8bit_to_32bit(uae_u8 *dst, uae_u8 *src, int bytes, uae_u32 *clut)
{
	uae_u32 *d = (uae_u32 *)dst;
	for (int i = 0; i < bytes; i++)
		d[i] = clut[src[i]];
}

void copy_screen_16bit_swap(uae_u8 *dst, uae_u8 *src, int bytes)
{
	uae_u16 *d = (uae_u16 *)dst, *s = (uae_u16 *)src;
	for (int i = 0; i < bytes / 2; i++)
		d[i] = __builtin_bswap16(s[i]);
}

/* big-endian R5G6B5 in, xRGB 8888 out */
void copy_screen_16bit_to_32bit(uae_u8 *dst, uae_u8 *src, int bytes)
{
	uae_u32 *d = (uae_u32 *)dst;
	uae_u16 *s = (uae_u16 *)src;
	for (int i = 0; i < bytes / 2; i++) {
		uae_u32 v = __builtin_bswap16(s[i]);
		d[i] = ((v & 0x1f) << 3) | (((v >> 5) & 0x3f) << 10) | (((v >> 11) & 0x1f) << 19);
	}
}

/* BGRA in memory in, R5G6B5 out */
void copy_screen_32bit_to_16bit(uae_u8 *dst, uae_u8 *src, int bytes)
{
	uae_u16 *d = (uae_u16 *)dst;
	uae_u32 *s = (uae_u32 *)src;
	for (int i = 0; i < bytes / 4; i++) {
		uae_u32 v = __builtin_bswap32(s[i]);
		d[i] = (uae_u16)(((v >> 8) & 0xf800) | ((v >> 5) & 0x07e0) | ((v >> 3) & 0x001f));
	}
}

/* RGBA in memory in, R5G6B5 out */
void copy_screen_32bit_to_16bit_rgba(uae_u8 *dst, uae_u8 *src, int bytes)
{
	uae_u16 *d = (uae_u16 *)dst;
	uae_u32 *s = (uae_u32 *)src;
	for (int i = 0; i < bytes / 4; i++) {
		uae_u32 v = __builtin_bswap32(s[i]);
		d[i] = (uae_u16)(((v >> 16) & 0xf800) | ((v >> 13) & 0x07e0) | ((v >> 11) & 0x001f));
	}
}

/* BGRA in memory in, ARGB in memory out */
void copy_screen_32bit_to_32bit(uae_u8 *dst, uae_u8 *src, int bytes)
{
	uae_u32 *d = (uae_u32 *)dst, *s = (uae_u32 *)src;
	for (int i = 0; i < bytes / 4; i++)
		d[i] = __builtin_bswap32(s[i]);
}

}

/* fpp.cpp sets the x87 control word on x86 for UAE's long double FPU. This
   build uses the same double precision FPU code as ARM, so there is nothing
   to set. */
void init_fpucw_x87(void)
{
}
