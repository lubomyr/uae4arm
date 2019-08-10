#include "sysdeps.h"
#include "options.h"
#include "include/memory-uae.h"
#include "savestate.h"
#include "blitter.h"

void blitdofast_0 (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (dstp)
			chipmem_wput_indirect (dstp, dstd);
		dstd = (0);
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (ptd) ptd += b->bltdmod;
}
		if (dstp)
			chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_0 (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 dstd = 0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (dstp)
			chipmem_wput_indirect (dstp, dstd);
		dstd = (0);
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (ptd) ptd -= b->bltdmod;
}
		if (dstp)
			chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_a (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc += 2; }
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta += 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)b->bltaold << 16) | bltadat) >> b->blitashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((~srca & srcc));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_a (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc -= 2; }
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta -= 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)bltadat << 16) | b->bltaold) >> b->blitdownashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((~srca & srcc));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_2a (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc += 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb += 2;
			srcb = (((uae_u32)b->bltbold << 16) | bltbdat) >> b->blitbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta += 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)b->bltaold << 16) | bltadat) >> b->blitashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcc & ~(srca & srcb)));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_2a (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc -= 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | b->bltbold) >> b->blitdownbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta -= 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)bltadat << 16) | b->bltaold) >> b->blitdownashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcc & ~(srca & srcb)));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_30 (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcb = b->bltbhold;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb += 2;
			srcb = (((uae_u32)b->bltbold << 16) | bltbdat) >> b->blitbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta += 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)b->bltaold << 16) | bltadat) >> b->blitashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srca & ~srcb));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_30 (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcb = b->bltbhold;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | b->bltbold) >> b->blitdownbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta -= 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)bltadat << 16) | b->bltaold) >> b->blitdownashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srca & ~srcb));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_3a (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc += 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb += 2;
			srcb = (((uae_u32)b->bltbold << 16) | bltbdat) >> b->blitbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta += 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)b->bltaold << 16) | bltadat) >> b->blitashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcb ^ (srca | (srcb ^ srcc))));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_3a (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc -= 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | b->bltbold) >> b->blitdownbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta -= 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)bltadat << 16) | b->bltaold) >> b->blitdownashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcb ^ (srca | (srcb ^ srcc))));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_3c (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcb = b->bltbhold;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb += 2;
			srcb = (((uae_u32)b->bltbold << 16) | bltbdat) >> b->blitbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta += 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)b->bltaold << 16) | bltadat) >> b->blitashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srca ^ srcb));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_3c (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcb = b->bltbhold;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | b->bltbold) >> b->blitdownbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta -= 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)bltadat << 16) | b->bltaold) >> b->blitdownashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srca ^ srcb));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_4a (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc += 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb += 2;
			srcb = (((uae_u32)b->bltbold << 16) | bltbdat) >> b->blitbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta += 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)b->bltaold << 16) | bltadat) >> b->blitashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcc ^ (srca & (srcb | srcc))));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_4a (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc -= 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | b->bltbold) >> b->blitdownbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta -= 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)bltadat << 16) | b->bltaold) >> b->blitdownashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcc ^ (srca & (srcb | srcc))));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_6a (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc += 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb += 2;
			srcb = (((uae_u32)b->bltbold << 16) | bltbdat) >> b->blitbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta += 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)b->bltaold << 16) | bltadat) >> b->blitashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcc ^ (srca & srcb)));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_6a (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc -= 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | b->bltbold) >> b->blitdownbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta -= 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)bltadat << 16) | b->bltaold) >> b->blitdownashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcc ^ (srca & srcb)));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_8a (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc += 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb += 2;
			srcb = (((uae_u32)b->bltbold << 16) | bltbdat) >> b->blitbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta += 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)b->bltaold << 16) | bltadat) >> b->blitashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcc & (~srca | srcb)));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_8a (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc -= 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | b->bltbold) >> b->blitdownbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta -= 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)bltadat << 16) | b->bltaold) >> b->blitdownashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcc & (~srca | srcb)));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_8c (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc += 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb += 2;
			srcb = (((uae_u32)b->bltbold << 16) | bltbdat) >> b->blitbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta += 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)b->bltaold << 16) | bltadat) >> b->blitashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcb & (~srca | srcc)));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_8c (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc -= 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | b->bltbold) >> b->blitdownbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta -= 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)bltadat << 16) | b->bltaold) >> b->blitdownashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcb & (~srca | srcc)));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_9a (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc += 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb += 2;
			srcb = (((uae_u32)b->bltbold << 16) | bltbdat) >> b->blitbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta += 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)b->bltaold << 16) | bltadat) >> b->blitashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcc ^ (srca & ~srcb)));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_9a (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc -= 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | b->bltbold) >> b->blitdownbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta -= 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)bltadat << 16) | b->bltaold) >> b->blitdownashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcc ^ (srca & ~srcb)));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_a8 (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc += 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb += 2;
			srcb = (((uae_u32)b->bltbold << 16) | bltbdat) >> b->blitbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta += 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)b->bltaold << 16) | bltadat) >> b->blitashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcc & (srca | srcb)));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_a8 (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc -= 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | b->bltbold) >> b->blitdownbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta -= 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)bltadat << 16) | b->bltaold) >> b->blitdownashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcc & (srca | srcb)));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_aa (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc += 2; }
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = (srcc);
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_aa (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc -= 2; }
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = (srcc);
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_b1 (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc += 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb += 2;
			srcb = (((uae_u32)b->bltbold << 16) | bltbdat) >> b->blitbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta += 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)b->bltaold << 16) | bltadat) >> b->blitashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = (~(srca ^ (srcc | (srca ^ srcb))));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_b1 (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc -= 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | b->bltbold) >> b->blitdownbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta -= 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)bltadat << 16) | b->bltaold) >> b->blitdownashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = (~(srca ^ (srcc | (srca ^ srcb))));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_ca (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc += 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb += 2;
			srcb = (((uae_u32)b->bltbold << 16) | bltbdat) >> b->blitbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta += 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)b->bltaold << 16) | bltadat) >> b->blitashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcc ^ (srca & (srcb ^ srcc))));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_ca (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc -= 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | b->bltbold) >> b->blitdownbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta -= 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)bltadat << 16) | b->bltaold) >> b->blitdownashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcc ^ (srca & (srcb ^ srcc))));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_cc (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcb = b->bltbhold;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb += 2;
			srcb = (((uae_u32)b->bltbold << 16) | bltbdat) >> b->blitbshift;
			b->bltbold = bltbdat;
		}
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = (srcb);
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (ptb) ptb += b->bltbmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_cc (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcb = b->bltbhold;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | b->bltbold) >> b->blitdownbshift;
			b->bltbold = bltbdat;
		}
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = (srcb);
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (ptb) ptb -= b->bltbmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_d8 (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc += 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb += 2;
			srcb = (((uae_u32)b->bltbold << 16) | bltbdat) >> b->blitbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta += 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)b->bltaold << 16) | bltadat) >> b->blitashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srca ^ (srcc & (srca ^ srcb))));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_d8 (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc -= 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | b->bltbold) >> b->blitdownbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta -= 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)bltadat << 16) | b->bltaold) >> b->blitdownashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srca ^ (srcc & (srca ^ srcb))));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_e2 (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc += 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb += 2;
			srcb = (((uae_u32)b->bltbold << 16) | bltbdat) >> b->blitbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta += 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)b->bltaold << 16) | bltadat) >> b->blitashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcc ^ (srcb & (srca ^ srcc))));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_e2 (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc -= 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | b->bltbold) >> b->blitdownbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta -= 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)bltadat << 16) | b->bltaold) >> b->blitdownashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcc ^ (srcb & (srca ^ srcc))));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_ea (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc += 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb += 2;
			srcb = (((uae_u32)b->bltbold << 16) | bltbdat) >> b->blitbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta += 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)b->bltaold << 16) | bltadat) >> b->blitashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcc | (srca & srcb)));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_ea (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcb = b->bltbhold;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc -= 2; }
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | b->bltbold) >> b->blitdownbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta -= 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)bltadat << 16) | b->bltaold) >> b->blitdownashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srcc | (srca & srcb)));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_f0 (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta += 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)b->bltaold << 16) | bltadat) >> b->blitashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = (srca);
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptd) ptd += b->bltdmod;
}
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_f0 (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta -= 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)bltadat << 16) | b->bltaold) >> b->blitdownashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = (srca);
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptd) ptd -= b->bltdmod;
}
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_fa (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc += 2; }
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta += 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)b->bltaold << 16) | bltadat) >> b->blitashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srca | srcc));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptc) ptc += b->bltcmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_fa (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcc = b->bltcdat;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptc) { srcc = do_get_mem_word ((uae_u16 *)ptc); ptc -= 2; }
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta -= 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)bltadat << 16) | b->bltaold) >> b->blitdownashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srca | srcc));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptc) ptc -= b->bltcmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltcdat = srcc;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_fc (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
int i,j;
uae_u32 totald = 0;
uae_u32 srcb = b->bltbhold;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;

		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb += 2;
			srcb = (((uae_u32)b->bltbold << 16) | bltbdat) >> b->blitbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta += 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)b->bltaold << 16) | bltadat) >> b->blitashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srca | srcb));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd += 2; }
	}
	if (pta) pta += b->bltamod;
	if (ptb) ptb += b->bltbmod;
	if (ptd) ptd += b->bltdmod;
}
b->bltbhold = srcb;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
void blitdofast_desc_fc (uae_u8 *pta, uae_u8 *ptb, uae_u8 *ptc, uaecptr ptd, struct bltinfo *b)
{
uae_u32 totald = 0;
int i,j;
uae_u32 srcb = b->bltbhold;
uae_u32 dstd=0;
uaecptr dstp = 0;
for (j = 0; j < b->vblitsize; j++) {
	for (i = 0; i < b->hblitsize; i++) {
		uae_u32 bltadat, srca;
		if (ptb) {
			uae_u32 bltbdat = b->bltbdat = do_get_mem_word ((uae_u16 *)ptb); ptb -= 2;
			srcb = ((bltbdat << 16) | b->bltbold) >> b->blitdownbshift;
			b->bltbold = bltbdat;
		}
		if (pta) { bltadat = b->bltadat = do_get_mem_word ((uae_u16 *)pta); pta -= 2; } else { bltadat = b->bltadat; }
		bltadat &= blit_masktable[i];
		srca = (((uae_u32)bltadat << 16) | b->bltaold) >> b->blitdownashift;
		b->bltaold = bltadat;
		if (dstp)
		  chipmem_wput_indirect (dstp, dstd);
		dstd = ((srca | srcb));
		totald |= dstd;
		if (ptd) { dstp = ptd; ptd -= 2; }
	}
	if (pta) pta -= b->bltamod;
	if (ptb) ptb -= b->bltbmod;
	if (ptd) ptd -= b->bltdmod;
}
b->bltbhold = srcb;
if (dstp)
  chipmem_wput_indirect (dstp, dstd);
if ((totald<<16) != 0) b->blitzero = 0;
}
