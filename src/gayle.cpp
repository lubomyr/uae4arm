/*
* UAE - The Un*x Amiga Emulator
*
* Gayle (and motherboard resources) memory bank
*
* (c) 2006 - 2015 Toni Wilen
*/

#include "sysdeps.h"

#include "options.h"
#include "memory-uae.h"
#include "custom.h"
#include "newcpu.h"
#include "filesys.h"
#include "gayle.h"
#include "savestate.h"
#include "uae.h"
#include "threaddep/thread.h"
#include "ide.h"
#include "autoconf.h"
#include "devices.h"

/*
600000 to 9FFFFF	4 MB	Credit Card memory if CC present
A00000 to A1FFFF	128 KB	Credit Card Attributes
A20000 to A3FFFF	128 KB	Credit Card I/O
A40000 to A5FFFF	128 KB	Credit Card Bits
A60000 to A7FFFF	128 KB	PC I/O

D80000 to D8FFFF	64 KB SPARE chip select
D90000 to D9FFFF	64 KB ARCNET chip select
DA0000 to DA3FFF	16 KB IDE drive
DA4000 to DA4FFF	16 KB IDE reserved
DA8000 to DAFFFF	32 KB Credit Card and IDE configregisters
DB0000 to DBFFFF	64 KB Not used (reserved for external IDE)
* DC0000 to DCFFFF	64 KB Real Time Clock (RTC)
DD0000 to DDFFFF	64 KB A3000 DMA controller
DD0000 to DD1FFF        A4000 DMAC
DD2000 to DDFFFF        A4000 IDE
DE0000 to DEFFFF	64 KB Motherboard resources
*/

/* Gayle definitions from Linux drivers and preliminary Gayle datasheet */

/* PCMCIA stuff */

#define GAYLE_RAM               0x600000
#define GAYLE_RAMSIZE           0x400000
#define GAYLE_ATTRIBUTE         0xa00000
#define GAYLE_ATTRIBUTESIZE     0x020000
#define GAYLE_IO                0xa20000     /* 16bit and even 8bit registers */
#define GAYLE_IOSIZE            0x010000
#define GAYLE_IO_8BITODD        0xa30000     /* odd 8bit registers */

#define GAYLE_ADDRESS   0xda8000      /* gayle main registers base address */
#define GAYLE_RESET     0xa40000      /* write 0x00 to start reset, read 1 byte to stop reset */

/*  Bases of the IDE interfaces */
#define GAYLE_BASE_4000 0xdd2020    /* A4000 */
#define GAYLE_BASE_1200 0xda0000    /* A1200/A600 and E-Matrix 530 */

/*
*  These are at different offsets from the base
*/
#define GAYLE_IRQ_4000  0x3020    /* WORD register MSB = 1, Harddisk is source of interrupt */
#define GAYLE_CS_1200	0x8000
#define GAYLE_IRQ_1200  0x9000
#define GAYLE_INT_1200  0xA000
#define GAYLE_CFG_1200	0xB000

/* DA8000 */
#define GAYLE_CS_IDE	0x80	/* IDE int status */
#define GAYLE_CS_CCDET	0x40    /* credit card detect */
#define GAYLE_CS_BVD1	0x20    /* battery voltage detect 1 */
#define GAYLE_CS_SC	0x20    /* credit card status change */
#define GAYLE_CS_BVD2	0x10    /* battery voltage detect 2 */
#define GAYLE_CS_DA	0x10    /* digital audio */
#define GAYLE_CS_WR	0x08    /* write enable (1 == enabled) */
#define GAYLE_CS_BSY	0x04    /* credit card busy */
#define GAYLE_CS_IRQ	0x04    /* interrupt request */
#define GAYLE_CS_DAEN   0x02    /* enable digital audio */ 
#define GAYLE_CS_DIS    0x01    /* disable PCMCIA slot */ 

/* DA9000 */
#define GAYLE_IRQ_IDE	    0x80
#define GAYLE_IRQ_CCDET	    0x40    /* credit card detect */
#define GAYLE_IRQ_BVD1	    0x20    /* battery voltage detect 1 */
#define GAYLE_IRQ_SC	    0x20    /* credit card status change */
#define GAYLE_IRQ_BVD2	    0x10    /* battery voltage detect 2 */
#define GAYLE_IRQ_DA	    0x10    /* digital audio */
#define GAYLE_IRQ_WR	    0x08    /* write enable (1 == enabled) */
#define GAYLE_IRQ_BSY	    0x04    /* credit card busy */
#define GAYLE_IRQ_IRQ	    0x04    /* interrupt request */
#define GAYLE_IRQ_RESET	    0x02    /* reset machine after CCDET change */ 
#define GAYLE_IRQ_BERR      0x01    /* generate bus error after CCDET change */ 

/* DAA000 */
#define GAYLE_INT_IDE	    0x80    /* IDE interrupt enable */
#define GAYLE_INT_CCDET	    0x40    /* credit card detect change enable */
#define GAYLE_INT_BVD1	    0x20    /* battery voltage detect 1 change enable */
#define GAYLE_INT_SC	    0x20    /* credit card status change enable */
#define GAYLE_INT_BVD2	    0x10    /* battery voltage detect 2 change enable */
#define GAYLE_INT_DA	    0x10    /* digital audio change enable */
#define GAYLE_INT_WR	    0x08    /* write enable change enabled */
#define GAYLE_INT_BSY	    0x04    /* credit card busy */
#define GAYLE_INT_IRQ	    0x04    /* credit card interrupt request */
#define GAYLE_INT_BVD_LEV   0x02    /* BVD int level, 0=lev2,1=lev6 */ 
#define GAYLE_INT_BSY_LEV   0x01    /* BSY int level, 0=lev2,1=lev6 */ 

/* 0xDAB000 GAYLE_CONFIG */
#define GAYLE_CFG_0V            0x00
#define GAYLE_CFG_5V            0x01
#define GAYLE_CFG_12V           0x02
#define GAYLE_CFG_100NS         0x08
#define GAYLE_CFG_150NS         0x04
#define GAYLE_CFG_250NS         0x00
#define GAYLE_CFG_720NS         0x0c

#define TOTAL_IDE 3
#define GAYLE_IDE_ID 0
#define PCMCIA_IDE_ID 2

static struct ide_hdf *idedrive[TOTAL_IDE * 2];

static int gayle_id_cnt;
static uae_u8 gayle_irq, gayle_int, gayle_cs, gayle_cs_mask, gayle_cfg;
static int ide_splitter;

static struct ide_thread_state gayle_its;

static void gayle_reset(int hardreset);
static void gayle_map_pcmcia(void);

static uae_u8 checkgayleideirq (void)
{
	int i;
	bool irq = false;

	for (i = 0; i < 2; i++) {
		if (idedrive[i]) {
			if (!(idedrive[i]->regs.ide_devcon & 2) && (idedrive[i]->irq || (idedrive[i + 2] && idedrive[i + 2]->irq)))
				irq = true;
			/* IDE killer feature. Do not eat interrupt to make booting faster. */
			if (idedrive[i]->irq && !ide_isdrive (idedrive[i]))
				idedrive[i]->irq = 0;
			if (idedrive[i + 2] && idedrive[i + 2]->irq && !ide_isdrive (idedrive[i + 2]))
				idedrive[i + 2]->irq = 0;
		}
	}
	return irq ? GAYLE_IRQ_IDE : 0;
}

static void rethink_gayle (void)
{
	int lev2 = 0;
	int lev6 = 0;
	uae_u8 mask;

	if (currprefs.cs_ide == IDE_A4000) {
		gayle_irq |= checkgayleideirq ();
		if (gayle_irq & GAYLE_IRQ_IDE)
			safe_interrupt_set(false);
		return;
	}

	if (currprefs.cs_ide != IDE_A600A1200 && !currprefs.cs_pcmcia)
		return;
	gayle_irq |= checkgayleideirq();
	mask = gayle_int & gayle_irq;
	if (mask & (GAYLE_IRQ_IDE | GAYLE_IRQ_WR))
		lev2 = 1;
	if (mask & GAYLE_IRQ_CCDET)
		lev6 = 1;
	if (mask & (GAYLE_IRQ_BVD1 | GAYLE_IRQ_BVD2)) {
		if (gayle_int & GAYLE_INT_BVD_LEV)
			lev6 = 1;
		else
			lev2 = 1;
	}
	if (mask & GAYLE_IRQ_BSY) {
		if (gayle_int & GAYLE_INT_BSY_LEV)
			lev6 = 1;
		else
			lev2 = 1;
	}
	if (lev2)
		safe_interrupt_set(false);
	if (lev6)
		safe_interrupt_set(true);
}

static void gayle_cs_change (uae_u8 mask, int onoff)
{
	int changed = 0;
	if ((gayle_cs & mask) && !onoff) {
		gayle_cs &= ~mask;
		changed = 1;
	} else if (!(gayle_cs & mask) && onoff) {
		gayle_cs |= mask;
		changed = 1;
	}
	if (changed) {
		gayle_irq |= mask;
		rethink_gayle ();
		if ((mask & GAYLE_CS_CCDET) && (gayle_irq & (GAYLE_IRQ_RESET | GAYLE_IRQ_BERR)) != (GAYLE_IRQ_RESET | GAYLE_IRQ_BERR)) {
			if (gayle_irq & GAYLE_IRQ_RESET)
				uae_reset (0, 0);
			if (gayle_irq & GAYLE_IRQ_BERR)
				Exception (2);
		}
	}
}

static void card_trigger (int insert)
{
	if (insert) {
	} else {
		gayle_cfg = 0;
		gayle_cs_change (GAYLE_CS_CCDET, 0);
		gayle_cs_change (GAYLE_CS_BVD2, 0);
		gayle_cs_change (GAYLE_CS_BVD1, 0);
		gayle_cs_change (GAYLE_CS_WR, 0);
		gayle_cs_change (GAYLE_CS_BSY, 0);
	}
	rethink_gayle ();
}

static void write_gayle_cfg (uae_u8 val)
{
	gayle_cfg = val;
}
static uae_u8 read_gayle_cfg (void)
{
	return gayle_cfg & 0x0f;
}
static void write_gayle_irq (uae_u8 val)
{
	gayle_irq = (gayle_irq & val) | (val & (GAYLE_IRQ_RESET | GAYLE_IRQ_BERR));
}
static uae_u8 read_gayle_irq (void)
{
	return gayle_irq;
}
static void write_gayle_int (uae_u8 val)
{
	gayle_int = val;
}
static uae_u8 read_gayle_int (void)
{
	return gayle_int;
}
static void write_gayle_cs (uae_u8 val)
{
	int ov = gayle_cs;

	gayle_cs_mask = val & ~3;
	gayle_cs &= ~3;
	gayle_cs |= val & 3;
	if ((ov & 1) != (gayle_cs & 1)) {
		gayle_map_pcmcia ();
		/* PCMCIA disable -> enable */
		card_trigger (!(gayle_cs & GAYLE_CS_DIS) ? 1 : 0);
	}
}
static uae_u8 read_gayle_cs (void)
{
	uae_u8 v;

	v = gayle_cs_mask | gayle_cs;
	v |= checkgayleideirq ();
	return v;
}

static int get_gayle_ide_reg (uaecptr addr, struct ide_hdf **ide)
{
	int ide2;
	addr &= 0xffff;
	*ide = NULL;
	if (addr >= GAYLE_IRQ_4000 && addr <= GAYLE_IRQ_4000 + 1 && currprefs.cs_ide == IDE_A4000)
		return -1;
	addr &= ~0x2020;
	addr >>= 2;
	ide2 = 0;
	if (addr & IDE_SECONDARY) {
		if (ide_splitter) {
			ide2 = 2;
			addr &= ~IDE_SECONDARY;
		}
	}
	*ide = idedrive[ide2 + idedrive[ide2]->ide_drv];
	return addr;
}


static uae_u32 gayle_read2 (uaecptr addr)
{
	struct ide_hdf *ide = NULL;
	int ide_reg;

	addr &= 0xffff;
	if (currprefs.cs_ide <= 0) {
		if (addr == 0x201c) // AR1200 IDE detection hack
			return 0x7f;
		return 0xff;
	}
	if (addr >= GAYLE_IRQ_4000 && addr <= GAYLE_IRQ_4000 + 1 && currprefs.cs_ide == IDE_A4000) {
		uae_u8 v = gayle_irq;
		gayle_irq = 0;
		return v;
	}
	if (addr >= 0x4000) {
		if (addr == GAYLE_IRQ_1200) {
			if (currprefs.cs_ide == IDE_A600A1200)
				return read_gayle_irq ();
			return 0;
		} else if (addr == GAYLE_INT_1200) {
			if (currprefs.cs_ide == IDE_A600A1200)
				return read_gayle_int ();
			return 0;
		}
		return 0;
	}
	ide_reg = get_gayle_ide_reg (addr, &ide);
	/* Emulated "ide killer". Prevents long KS boot delay if no drives installed */
	if (!ide_isdrive (idedrive[0]) && !ide_isdrive (idedrive[1]) && !ide_isdrive (idedrive[2]) && !ide_isdrive (idedrive[3])) {
		if (ide_reg == IDE_STATUS)
			return 0x7f;
		return 0xff;
	}
	return ide_read_reg (ide, ide_reg);
}

static void gayle_write2 (uaecptr addr, uae_u32 val)
{
	struct ide_hdf *ide = NULL;
	int ide_reg;

	if (currprefs.cs_ide <= 0)
		return;
	if (currprefs.cs_ide == IDE_A600A1200) {
		if (addr == GAYLE_IRQ_1200) {
			write_gayle_irq (val);
			return;
		}
		if (addr == GAYLE_INT_1200) {
			write_gayle_int (val);
			return;
		}
	}
	if (addr >= 0x4000)
		return;
	ide_reg = get_gayle_ide_reg (addr, &ide);
	ide_write_reg (ide, ide_reg, val);
}

static int gayle_read (uaecptr addr)
{
	uaecptr oaddr = addr;
	uae_u32 v = 0;
	int got = 0;
	if (currprefs.cs_ide == IDE_A600A1200) {
		if ((addr & 0xA0000) != 0xA0000)
			return 0;
	}
	addr &= 0xffff;
	if (currprefs.cs_pcmcia) {
		if (currprefs.cs_ide != IDE_A600A1200) {
			if (addr == GAYLE_IRQ_1200) {
				v = read_gayle_irq ();
				got = 1;
			} else if (addr == GAYLE_INT_1200) {
				v = read_gayle_int ();
				got = 1;
			}
		}
		if (addr == GAYLE_CS_1200) {
			v = read_gayle_cs ();
			got = 1;
		} else if (addr == GAYLE_CFG_1200) {
			v = read_gayle_cfg ();
			got = 1;
		}
	}
	if (!got)
		v = gayle_read2 (addr);
	return v;
}

static void gayle_write (uaecptr addr, int val)
{
	uaecptr oaddr = addr;
	int got = 0;
	if (currprefs.cs_ide == IDE_A600A1200) {
		if ((addr & 0xA0000) != 0xA0000)
			return;
	}
	addr &= 0xffff;
	if (currprefs.cs_pcmcia) {
		if (currprefs.cs_ide != IDE_A600A1200) {
			if (addr == GAYLE_IRQ_1200) {
				write_gayle_irq (val);
				got = 1;
			} else if (addr == GAYLE_INT_1200) {
				write_gayle_int (val);
				got = 1;
			}
		}
		if (addr == GAYLE_CS_1200) {
			write_gayle_cs (val);
			got = 1;
		} else if (addr == GAYLE_CFG_1200) {
			write_gayle_cfg (val);
			got = 1;
		}
	}

	if (!got)
		gayle_write2 (addr, val);
}

DECLARE_MEMORY_FUNCTIONS(gayle);
addrbank gayle_bank = {
	gayle_lget, gayle_wget, gayle_bget,
	gayle_lput, gayle_wput, gayle_bput,
	default_xlate, default_check, NULL, NULL, _T("Gayle (low)"),
	dummy_lgeti, dummy_wgeti,
	ABFLAG_IO, S_READ, S_WRITE
};

static uae_u32 REGPARAM2 gayle_lget (uaecptr addr)
{
	struct ide_hdf *ide = NULL;
	int ide_reg;
	uae_u32 v;
	ide_reg = get_gayle_ide_reg (addr, &ide);
	if (ide_reg == IDE_DATA) {
		v = ide_get_data (ide) << 16;
		v |= ide_get_data (ide);
		return v;
	}
	v = gayle_wget (addr) << 16;
	v |= gayle_wget (addr + 2);
	return v;
}
static uae_u32 REGPARAM2 gayle_wget (uaecptr addr)
{
	struct ide_hdf *ide = NULL;
	int ide_reg;
	uae_u32 v;
	ide_reg = get_gayle_ide_reg (addr, &ide);
	if (ide_reg == IDE_DATA) {
		v = ide_get_data (ide);
		return v;
	}
	v = gayle_bget (addr) << 8;
	v |= gayle_bget (addr + 1);
	return v;
}
static uae_u32 REGPARAM2 gayle_bget (uaecptr addr)
{
	uae_u32 v;
	v = gayle_read (addr);
	return v;
}

static void REGPARAM2 gayle_lput (uaecptr addr, uae_u32 value)
{
	struct ide_hdf *ide = NULL;
	int ide_reg;
	ide_reg = get_gayle_ide_reg (addr, &ide);
	if (ide_reg == IDE_DATA) {
		ide_put_data (ide, value >> 16);
		ide_put_data (ide, value & 0xffff);
		return;
	}
	gayle_wput (addr, value >> 16);
	gayle_wput (addr + 2, value & 0xffff);
}
static void REGPARAM2 gayle_wput (uaecptr addr, uae_u32 value)
{
	struct ide_hdf *ide = NULL;
	int ide_reg;
	ide_reg = get_gayle_ide_reg (addr, &ide);
	if (ide_reg == IDE_DATA) {
		ide_put_data (ide, value);
		return;
	}
	gayle_bput (addr, value >> 8);
	gayle_bput (addr + 1, value & 0xff);
}

static void REGPARAM2 gayle_bput (uaecptr addr, uae_u32 value)
{
	gayle_write (addr, value);
}

static void gayle2_write (uaecptr addr, uae_u32 v)
{
	gayle_id_cnt = 0;
}

static uae_u32 gayle2_read (uaecptr addr)
{
	uae_u8 v = 0;
	addr &= 0xffff;
	if (addr == 0x1000) {
		/* Gayle ID. Gayle = 0xd0. AA Gayle = 0xd1 */
		if (gayle_id_cnt == 0 || gayle_id_cnt == 1 || gayle_id_cnt == 3 || ((currprefs.chipset_mask & CSMASK_AGA) && gayle_id_cnt == 7) ||
			(currprefs.cs_cd32cd && !currprefs.cs_ide && !currprefs.cs_pcmcia && gayle_id_cnt == 2))
			v = 0x80;
		else
			v = 0x00;
		gayle_id_cnt++;
	}
	return v;
}

DECLARE_MEMORY_FUNCTIONS(gayle2);
addrbank gayle2_bank = {
	gayle2_lget, gayle2_wget, gayle2_bget,
	gayle2_lput, gayle2_wput, gayle2_bput,
	default_xlate, default_check, NULL, NULL, _T("Gayle (high)"),
	dummy_lgeti, dummy_wgeti,
	ABFLAG_IO, S_READ, S_WRITE
};

static uae_u32 REGPARAM2 gayle2_lget (uaecptr addr)
{
	uae_u32 v;
	v = gayle2_wget (addr) << 16;
	v |= gayle2_wget (addr + 2);
	return v;
}
static uae_u32 REGPARAM2 gayle2_wget (uaecptr addr)
{
	uae_u16 v;
	v = gayle2_bget (addr) << 8;
	v |= gayle2_bget (addr + 1);
	return v;
}
static uae_u32 REGPARAM2 gayle2_bget (uaecptr addr)
{
	return gayle2_read (addr);
}

static void REGPARAM2 gayle2_lput (uaecptr addr, uae_u32 value)
{
	gayle2_wput (addr, value >> 16);
	gayle2_wput (addr + 2, value & 0xffff);
}

static void REGPARAM2 gayle2_wput (uaecptr addr, uae_u32 value)
{
	gayle2_bput (addr, value >> 8);
	gayle2_bput (addr + 1, value & 0xff);
}

static void REGPARAM2 gayle2_bput (uaecptr addr, uae_u32 value)
{
	gayle2_write (addr, value);
}

static uae_u8 ramsey_config;
static int gary_coldboot;
static int gary_timeout;
int gary_toenb;

static void mbres_write (uaecptr addr, uae_u32 val, int size)
{
	addr &= 0xffff;
	if (addr < 0x8000) {
		uae_u32 addr2 = addr & 3;
		uae_u32 addr64 = (addr >> 6) & 3;
		if (addr64 == 0 && addr2 == 0x03)
			ramsey_config = val;
		if (addr2 == 0x02)
			gary_coldboot = (val & 0x80) ? 1 : 0;
		if (addr2 == 0x01)
			gary_toenb = (val & 0x80) ? 1 : 0;
		if (addr2 == 0x00)
			gary_timeout = (val & 0x80) ? 1 : 0;
	}
}

static uae_u32 mbres_read (uaecptr addr, int size)
{
	uae_u32 v = 0;

	addr &= 0xffff;

	uae_u32 addr2 = addr & 3;
	uae_u32 addr64 = (addr >> 6) & 3;
	for (;;) {
		if (addr64 == 1 && addr2 == 0x03) { /* RAMSEY revision */
			if (currprefs.cs_ramseyrev >= 0)
				v = currprefs.cs_ramseyrev;
			break;
		}
		if (addr64 == 0 && addr2 == 0x03) { /* RAMSEY config */
			if (currprefs.cs_ramseyrev >= 0)
				v = ramsey_config;
			break;
		}
		if (addr2 == 0x03) {
			v = 0xff;
			break;
		}
		if (addr2 == 0x02) { /* coldreboot flag */
			if (currprefs.cs_fatgaryrev >= 0)
				v = gary_coldboot ? 0x80 : 0x00;
		}
		if (addr2 == 0x01) { /* toenb flag */
			if (currprefs.cs_fatgaryrev >= 0)
				v = gary_toenb ? 0x80 : 0x00;
		}
		if (addr2 == 0x00) { /* timeout flag */
			if (currprefs.cs_fatgaryrev >= 0)
				v = gary_timeout ? 0x80 : 0x00;
		}
		v |= 0x7f;
		break;
	}
	return v;
}

static uae_u32 REGPARAM3 mbres_lget (uaecptr) REGPARAM;
static uae_u32 REGPARAM3 mbres_wget (uaecptr) REGPARAM;
static uae_u32 REGPARAM3 mbres_bget (uaecptr) REGPARAM;
static void REGPARAM3 mbres_lput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 mbres_wput (uaecptr, uae_u32) REGPARAM;
static void REGPARAM3 mbres_bput (uaecptr, uae_u32) REGPARAM;

static uae_u32 REGPARAM2 mbres_lget (uaecptr addr)
{
	uae_u32 v;
	v = mbres_wget (addr) << 16;
	v |= mbres_wget (addr + 2);
	return v;
}
static uae_u32 REGPARAM2 mbres_wget (uaecptr addr)
{
	return mbres_read (addr, 2);
}
static uae_u32 REGPARAM2 mbres_bget (uaecptr addr)
{
	return mbres_read (addr, 1);
}

static void REGPARAM2 mbres_lput (uaecptr addr, uae_u32 value)
{
	mbres_wput (addr, value >> 16);
	mbres_wput (addr + 2, value & 0xffff);
}

static void REGPARAM2 mbres_wput (uaecptr addr, uae_u32 value)
{
	mbres_write (addr, value, 2);
}

static void REGPARAM2 mbres_bput (uaecptr addr, uae_u32 value)
{
	mbres_write (addr, value, 1);
}

static addrbank mbres_sub_bank = {
	mbres_lget, mbres_wget, mbres_bget,
	mbres_lput, mbres_wput, mbres_bput,
	default_xlate, default_check, NULL, NULL, _T("Motherboard Resources"),
	dummy_lgeti, dummy_wgeti,
	ABFLAG_IO, S_READ, S_WRITE
};

static struct addrbank_sub mbres_sub_banks[] = {
	{ &mbres_sub_bank, 0x0000 },
	{ &dummy_bank,     0x8000 },
	{ NULL }
};

addrbank mbres_bank = {
	sub_bank_lget, sub_bank_wget, sub_bank_bget,
	sub_bank_lput, sub_bank_wput, sub_bank_bput,
	sub_bank_xlate, sub_bank_check, NULL, NULL, _T("Motherboard Resources"),
	sub_bank_lgeti, sub_bank_wgeti,
	ABFLAG_IO, S_READ, S_WRITE, mbres_sub_banks
};

static int freepcmcia (int reset)
{
	remove_ide_unit(idedrive, PCMCIA_IDE_ID * 2);

	gayle_cfg = 0;
	gayle_cs = 0;
	return 1;
}

static void gayle_map_pcmcia (void)
{
	if (currprefs.cs_pcmcia == 0)
		return;
	int idx = 0;
	bool pcmcia_override = false;
	while (!pcmcia_override) {
		int cnt = 0;
		for (int i = 0; i < 8; i++) {
			struct autoconfig_info *aci = expansion_get_autoconfig_by_address(&currprefs, 6 * 1024 * 1024 + i * 512 * 1024, idx);
			if (aci) {
				if (aci->zorro > 0) {
					pcmcia_override = true;
				}
			} else {
				cnt++;
			}
		}
		if (cnt >= 8)
			break;
		idx++;
	}
	map_banks_cond (&dummy_bank, 0xa0, 8, 0);
	if (currprefs.chipmem.size <= 4 * 1024 * 1024 && !pcmcia_override)
		map_banks_cond (&dummy_bank, PCMCIA_COMMON_START >> 16, PCMCIA_COMMON_SIZE >> 16, 0);
}

void gayle_free_units (void)
{
	for (int i = 0; i < TOTAL_IDE * 2; i++) {
		remove_ide_unit(idedrive, i);
	}
	freepcmcia (1);
}

void gayle_add_ide_unit (int ch, struct uaedev_config_info *ci, struct romconfig *rc)
{
	struct ide_hdf *ide;

	if (ch >= 2 * 2)
		return;
	ide = add_ide_unit (idedrive, TOTAL_IDE * 2, ch, ci, NULL);
}

static void gayle_init(void);

bool gayle_ide_init(struct autoconfig_info *aci)
{
	aci->zorro = 0;
	if (aci->prefs->cs_ide == 1) {
		aci->start = GAYLE_BASE_1200;
		aci->size = 0x10000;
	} else {
		aci->start = GAYLE_BASE_4000;
		aci->size = 0x1000;
	}
	device_add_reset(gayle_reset);
	if (aci->doinit)
		gayle_init();
	return true;
}

bool gayle_init_pcmcia(struct autoconfig_info *aci)
{
	aci->start = PCMCIA_COMMON_START;
	aci->size = 0xa80000 - aci->start;
	aci->zorro = 0;
	device_add_reset(gayle_reset);
	if (aci->doinit)
		gayle_init();
	return true;
}

static void gayle_hsync(void)
{
	if (ide_interrupt_hsync(idedrive[0]) || ide_interrupt_hsync(idedrive[2]) || ide_interrupt_hsync(idedrive[4]))
		rethink_gayle();
}

static void initide (void)
{
	gayle_its.idetable = idedrive;
	start_ide_thread(&gayle_its);
	alloc_ide_mem (idedrive, TOTAL_IDE * 2, &gayle_its);
	ide_initialize(idedrive, GAYLE_IDE_ID);
	ide_initialize(idedrive, GAYLE_IDE_ID + 1);

	if (isrestore ())
		return;
	ide_splitter = 0;
	if (ide_isdrive (idedrive[2]) || ide_isdrive(idedrive[3])) {
		ide_splitter = 1;
		write_log (_T("IDE splitter enabled\n"));
	}
	gayle_irq = gayle_int = 0;
}

static void gayle_free (void)
{
	stop_ide_thread(&gayle_its);
}

static void gayle_reset (int hardreset)
{
	static TCHAR bankname[100];

	initide ();
	if (hardreset) {
		ramsey_config = 0;
		gary_coldboot = 1;
		gary_timeout = 0;
		gary_toenb = 0;
	}
	_tcscpy (bankname, _T("Gayle (low)"));
	if (currprefs.cs_ide == IDE_A4000)
		_tcscpy (bankname, _T("A4000 IDE"));
	gayle_bank.name = bankname;

	gayle_map_pcmcia();
}

uae_u8 *restore_gayle (uae_u8 *src)
{
	changed_prefs.cs_ide = restore_u8 ();
	gayle_int = restore_u8 ();
	gayle_irq = restore_u8 ();
	gayle_cs = restore_u8 ();
	gayle_cs_mask = restore_u8 ();
	gayle_cfg = restore_u8 ();
	return src;
}

static void gayle_init(void)
{
	device_add_rethink(rethink_gayle);
	device_add_hsync(gayle_hsync);
	device_add_exit(gayle_free);
}

uae_u8 *save_gayle (int *len, uae_u8 *dstptr)
{
	uae_u8 *dstbak, *dst;

	if (currprefs.cs_ide <= 0)
		return NULL;
	if (dstptr)
		dstbak = dst = dstptr;
	else
		dstbak = dst = xmalloc (uae_u8, 1000);
	save_u8 (currprefs.cs_ide);
	save_u8 (gayle_int);
	save_u8 (gayle_irq);
	save_u8 (gayle_cs);
	save_u8 (gayle_cs_mask);
	save_u8 (gayle_cfg);
	*len = dst - dstbak;
	return dstbak;
}

uae_u8 *save_gayle_ide (int num, int *len, uae_u8 *dstptr)
{
	uae_u8 *dstbak, *dst;
	struct ide_hdf *ide;

	if (num >= TOTAL_IDE * 2 || idedrive[num] == NULL)
		return NULL;
	if (currprefs.cs_ide <= 0)
		return NULL;
	ide = idedrive[num];
	if (ide->hdhfd.size == 0)
		return NULL;
	if (dstptr)
		dstbak = dst = dstptr;
	else
		dstbak = dst = xmalloc (uae_u8, 1000);
	save_u32 (num);
	dst = ide_save_state(dst, ide);
	*len = dst - dstbak;
	return dstbak;
}

uae_u8 *restore_gayle_ide (uae_u8 *src)
{
	int num, readonly, blocksize;
	uae_u64 size;
	TCHAR *path;
	struct ide_hdf *ide;

	alloc_ide_mem (idedrive, TOTAL_IDE * 2, NULL);
	num = restore_u32 ();
	ide = idedrive[num];
	size = restore_u64 ();
	path = restore_string ();
	_tcscpy (ide->hdhfd.hfd.ci.rootdir, path);
	blocksize = restore_u32 ();
	readonly = restore_u32 ();
	src = ide_restore_state(src, ide);
	if (ide->hdhfd.hfd.virtual_size)
		gayle_add_ide_unit (num, NULL, NULL);
	else
		gayle_add_ide_unit (num, NULL, NULL);
	xfree (path);
	return src;
}
