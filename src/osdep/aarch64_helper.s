// Some functions and tests to increase performance in drawing.cpp and custom.cpp

//.arm

.global save_host_fp_regs
.global restore_host_fp_regs
.global copy_screen_8bit
.global copy_screen_16bit_swap
.global copy_screen_32bit_to_16bit

.text

.align 8

//----------------------------------------------------------------
// save_host_fp_regs
//----------------------------------------------------------------
save_host_fp_regs:
  st4  {v7.2D-v10.2D}, [x0], #64
  st4  {v11.2D-v14.2D}, [x0], #64
  st1  {v15.2D}, [x0], #16
  ret
  
//----------------------------------------------------------------
// restore_host_fp_regs
//----------------------------------------------------------------
restore_host_fp_regs:
  ld4  {v7.2D-v10.2D}, [x0], #64
  ld4  {v11.2D-v14.2D}, [x0], #64
  ld1  {v15.2D}, [x0], #16
  ret


//----------------------------------------------------------------
// copy_screen_8bit
//
// x0: uae_u8   *dst
// x1: uae_u8   *src
// x2: int      bytes always a multiple of 64: even number of lines, number of pixel per line is multiple of 32 (320, 640, 800, 1024, 1152, 1280)
// x3: uae_u32  *clut
//
// void copy_screen_8bit(uae_u8 *dst, uae_u8 *src, int bytes, uae_u32 *clut);
//
//----------------------------------------------------------------
copy_screen_8bit:
  mov       x7, #64
copy_screen_8bit_loop:
  ldrsw     x4, [x1], #4
  ubfx      x5, x4, #0, #8
  ldrsw     x6, [x3, x5, lsl #2]
  ubfx      x5, x4, #8, #8
  strh      w6, [x0], #2
  ldrsw     x6, [x3, x5, lsl #2]
  ubfx      x5, x4, #16, #8
  strh      w6, [x0], #2
  ldrsw     x6, [x3, x5, lsl #2]
  ubfx      x5, x4, #24, #8
  strh      w6, [x0], #2
  ldrsw     x6, [x3, x5, lsl #2]
  subs      x7, x7, #4
  strh      w6, [x0], #2
  bgt       copy_screen_8bit_loop
  subs      x2, x2, #64
  bgt       copy_screen_8bit
  ret
  

//----------------------------------------------------------------
// copy_screen_16bit_swap
//
// x0: uae_u8   *dst
// x1: uae_u8   *src
// x2: int      bytes always a multiple of 128: even number of lines, 2 bytes per pixel, number of pixel per line is multiple of 32 (320, 640, 800, 1024, 1152, 1280)
//
// void copy_screen_16bit_swap(uae_u8 *dst, uae_u8 *src, int bytes);
//
//----------------------------------------------------------------
copy_screen_16bit_swap:
  ld4       {v0.2D-v3.2D}, [x1], #64
  rev16     v0.16b, v0.16b
  rev16     v1.16b, v1.16b
  rev16     v2.16b, v2.16b
  rev16     v3.16b, v3.16b
  subs      w2, w2, #64
  st4       {v0.2D-v3.2D}, [x0], #64
  bne copy_screen_16bit_swap
  ret
  

//----------------------------------------------------------------
// copy_screen_32bit_to_16bit
//
// x0: uae_u8   *dst - Format (bits): rrrr rggg gggb bbbb
// x1: uae_u8   *src - Format (bytes) in memory rgba
// x2: int      bytes
//
// void copy_screen_32bit_to_16bit(uae_u8 *dst, uae_u8 *src, int bytes);
//
//----------------------------------------------------------------
copy_screen_32bit_to_16bit:
  mov x3, #0xf800
  mov v0.s[0], w3
  mov v0.s[1], w3
  mov v0.s[2], w3
  mov v0.s[3], w3
  mov x3, #0x07e0
  mov v1.s[0], w3
  mov v1.s[1], w3
  mov v1.s[2], w3
  mov v1.s[3], w3 
copy_screen_32bit_to_16bit_loop:
  ld1       {v3.4S}, [x1], #16
  rev64     v3.16B, v3.16B
  ushr      v4.4S, v3.4S, #16
  ushr      v5.4S, v3.4S, #13
  ushr      v3.4S, v3.4S, #11
  bit       v3.16B, v4.16B, v0.16B
  bit       v3.16B, v5.16B, v1.16B
  xtn       v3.4H, v3.4S
  rev32     v3.4H, v3.4H
  subs      w2, w2, #16
  st1       {v3.4H}, [x0], #8
  bne       copy_screen_32bit_to_16bit_loop
  ret

.align 8

