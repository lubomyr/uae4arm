#ifndef UAE_CPU_PREFETCH_H
#define UAE_CPU_PREFETCH_H

#include "uae/types.h"

STATIC_INLINE uae_u32 get_word_000_prefetch (int o)
{
	uae_u32 v = regs.irc;
	regs.irc = regs.read_buffer = regs.db = get_wordi (m68k_getpci () + o);
	return v;
}
STATIC_INLINE uae_u32 get_byte_000(uaecptr addr)
{
	uae_u32 v = get_byte (addr);
	regs.db = (v << 8) | v;
	regs.read_buffer = v;
	return v;
}
STATIC_INLINE uae_u32 get_word_000(uaecptr addr)
{
	uae_u32 v = get_word (addr);
	regs.db = v;
	regs.read_buffer = v;
	return v;
}
STATIC_INLINE void put_byte_000(uaecptr addr, uae_u32 v)
{
	regs.db = (v << 8) | v;
	regs.write_buffer = v;
	put_byte (addr, v);
}
STATIC_INLINE void put_word_000(uaecptr addr, uae_u32 v)
{
	regs.db = v;
	regs.write_buffer = v;
	put_word (addr, v);
}

#ifdef CPUEMU_13

STATIC_INLINE void do_cycles_ce000_internal(int clocks)
{
	if (currprefs.m68k_speed < 0)
		return;
	x_do_cycles (clocks * CYCLE_UNIT / 2);
}
STATIC_INLINE void do_cycles_ce000 (int clocks)
{
	x_do_cycles (clocks * CYCLE_UNIT / 2);
}

STATIC_INLINE void ipl_fetch (void)
{
	regs.ipl = regs.ipl_pin;
}

uae_u32 mem_access_delay_word_read (uaecptr addr);
uae_u32 mem_access_delay_wordi_read (uaecptr addr);
uae_u32 mem_access_delay_byte_read (uaecptr addr);
void mem_access_delay_byte_write (uaecptr addr, uae_u32 v);
void mem_access_delay_word_write (uaecptr addr, uae_u32 v);

STATIC_INLINE uae_u32 get_long_ce000 (uaecptr addr)
{
	uae_u32 v = mem_access_delay_word_read (addr) << 16;
	v |= mem_access_delay_word_read (addr + 2);
	return v;
}
STATIC_INLINE uae_u32 get_word_ce000 (uaecptr addr)
{
	return mem_access_delay_word_read (addr);
}
STATIC_INLINE uae_u32 get_wordi_ce000 (int offset)
{
	return mem_access_delay_wordi_read (m68k_getpci () + offset);
}
STATIC_INLINE uae_u32 get_byte_ce000 (uaecptr addr)
{
	return mem_access_delay_byte_read (addr);
}
STATIC_INLINE uae_u32 get_word_ce000_prefetch (int o)
{
	uae_u32 v = regs.irc;
	regs.irc = regs.read_buffer = regs.db = x_get_iword (o);
	return v;
}

STATIC_INLINE void put_long_ce000 (uaecptr addr, uae_u32 v)
{
	mem_access_delay_word_write (addr, v >> 16);
	mem_access_delay_word_write (addr + 2, v);
}
STATIC_INLINE void put_word_ce000 (uaecptr addr, uae_u32 v)
{
	mem_access_delay_word_write (addr, v);
}
STATIC_INLINE void put_byte_ce000 (uaecptr addr, uae_u32 v)
{
	mem_access_delay_byte_write (addr, v);
}

#endif

STATIC_INLINE uae_u32 get_disp_ea_000 (uae_u32 base, uae_u32 dp)
{
  int reg = (dp >> 12) & 15;
  uae_s32 regd = regs.regs[reg];
  if ((dp & 0x800) == 0)
  	regd = (uae_s32)(uae_s16)regd;
  return base + (uae_s8)dp + regd;
}

#endif /* UAE_CPU_PREFETCH_H */
