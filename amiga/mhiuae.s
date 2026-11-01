| mhiuae.library - an MHI driver: MPEG audio decoded by the host
|
| MHI is the interface of the MPEG audio decoder cards (MAS Player and the
| like). Players that support it - AmigaAMP, HippoPlayer and others - load a
| driver from LIBS:MHI/ and hand it the mp3 stream in buffers; the driver
| plays it and hands the buffers back. This one passes the calls on to
| uae4arm through uaelib function 201: the emulator decodes the stream with
| mpg123 and plays it along with Paula, so the 68k only reads the file.
|
| The decoder signals the player's task itself when buffers come back, so
| the library needs no task or interrupt of its own. It does not open where
| uaelib is missing or the emulator knows nothing of MHI.
|
| Built with GNU as for m68k and linked at 0, made an Amiga executable by
| hunk.py with the relocations the linker kept - see build.sh. The ROMTag
| and the init table hold absolute pointers; the code is PC relative.

	.set	VERSION, 1
	.set	REVISION, 0

	.set	SEGLIST, 34		| after struct Library
	.set	UAELIB, 38
	.set	LIBSIZE, 42

	.set	MHI_CMD, 201		| uaelib function

	.text
	.globl	_start

| Run from the Shell, it does nothing
_start:
	moveq	#0,%d0
	rts

romtag:
	.word	0x4AFC			| RTC_MATCHWORD
	.long	romtag
	.long	endskip
	.byte	0x80			| RTF_AUTOINIT
	.byte	VERSION
	.byte	9			| NT_LIBRARY
	.byte	0			| priority
	.long	libname
	.long	idstring
	.long	inittable

inittable:
	.long	LIBSIZE
	.long	functable
	.long	0			| no data table: init fills the base in
	.long	initlib

| Offsets from the table, as MakeFunctions() takes them after a -1
functable:
	.word	-1
	.word	libopen-functable
	.word	libclose-functable
	.word	libexpunge-functable
	.word	libnull-functable
	.word	MHIAllocDecoder-functable
	.word	MHIFreeDecoder-functable
	.word	MHIQueueBuffer-functable
	.word	MHIGetEmpty-functable
	.word	MHIGetStatus-functable
	.word	MHIPlay-functable
	.word	MHIStop-functable
	.word	MHIPause-functable
	.word	MHIQuery-functable
	.word	MHISetParam-functable
	.word	-1

| d0 the library, a0 the segment list, a6 SysBase
initlib:
	movem.l	%a4/%a5,-(%sp)
	movea.l	%d0,%a5
	move.l	%a0,SEGLIST(%a5)
	move.b	#9,8(%a5)		| ln_Type: NT_LIBRARY
	lea	libname(%pc),%a0
	move.l	%a0,10(%a5)		| ln_Name
	move.b	#6,14(%a5)		| lib_Flags: LIBF_SUMUSED, LIBF_CHANGED
	move.w	#VERSION,20(%a5)
	move.w	#REVISION,22(%a5)
	lea	idstring(%pc),%a0
	move.l	%a0,24(%a5)		| lib_IdString

| uaelib sits at $F0FF60, or $EFFF60 when the boot ROM moved for CDTV: a trap
| instruction followed by RTS
	movea.l	#0xF0FF60,%a4
	bsr.s	isuaelib
	beq.s	haveuaelib
	movea.l	#0xEFFF60,%a4
	bsr.s	isuaelib
	bne.s	initfail
haveuaelib:
	move.l	%a4,UAELIB(%a5)
	clr.l	-(%sp)
	clr.l	-(%sp)
	clr.l	-(%sp)
	clr.l	-(%sp)			| the version of the host's MHI, 0 for none
	move.l	#MHI_CMD,-(%sp)
	jsr	(%a4)
	lea	20(%sp),%sp
	tst.l	%d0
	beq.s	initfail
	move.l	%a5,%d0
	movem.l	(%sp)+,%a4/%a5
	rts

initfail:
	bsr.s	freebase
	moveq	#0,%d0
	movem.l	(%sp)+,%a4/%a5
	rts

isuaelib:
	move.w	(%a4),%d0
	and.w	#0xF000,%d0
	cmp.w	#0xA000,%d0
	bne.s	isuaelib_end
	cmp.w	#0x4E75,2(%a4)
isuaelib_end:
	rts

| Frees the library a5, with SysBase in a6
freebase:
	movea.l	%a5,%a1
	moveq	#0,%d0
	move.w	16(%a5),%d0		| lib_NegSize
	suba.l	%d0,%a1
	add.w	18(%a5),%d0		| lib_PosSize
	jsr	-210(%a6)		| FreeMem
	rts

libopen:
	addq.w	#1,32(%a6)		| lib_OpenCnt
	bclr	#3,14(%a6)		| LIBF_DELEXP
	move.l	%a6,%d0
	rts

libclose:
	moveq	#0,%d0
	subq.w	#1,32(%a6)
	bne.s	libclose_end
	btst	#3,14(%a6)
	bne.s	libexpunge
libclose_end:
	rts

libexpunge:
	tst.w	32(%a6)
	beq.s	expunge_now
	bset	#3,14(%a6)		| when the last one closes
	moveq	#0,%d0
	rts
expunge_now:
	movem.l	%d2/%a5/%a6,-(%sp)
	movea.l	%a6,%a5
	move.l	SEGLIST(%a5),%d2
	movea.l	4,%a6
	movea.l	%a5,%a1
	jsr	-252(%a6)		| Remove
	bsr.s	freebase
	move.l	%d2,%d0
	movem.l	(%sp)+,%d2/%a5/%a6
	rts

libnull:
	moveq	#0,%d0
	rts

| The MHI calls push the three arguments of the host's call and jump to
| host with the command in d0; host takes them off again.

| a0 task, d0 signal mask
MHIAllocDecoder:
	clr.l	-(%sp)
	move.l	%d0,-(%sp)
	move.l	%a0,-(%sp)
	moveq	#1,%d0
	bra.s	host

| a3 handle
MHIFreeDecoder:
	moveq	#2,%d0
	bra.s	handleonly

| a3 handle, a0 buffer, d0 size
MHIQueueBuffer:
	move.l	%d0,-(%sp)
	move.l	%a0,-(%sp)
	move.l	%a3,-(%sp)
	moveq	#3,%d0
	bra.s	host

MHIGetEmpty:
	moveq	#4,%d0
	bra.s	handleonly

MHIGetStatus:
	moveq	#5,%d0
	bra.s	handleonly

MHIPlay:
	moveq	#6,%d0
	bra.s	handleonly

MHIStop:
	moveq	#7,%d0
	bra.s	handleonly

MHIPause:
	moveq	#8,%d0
	bra.s	handleonly

| a3 handle, d0 parameter (a UWORD), d1 value
MHISetParam:
	move.l	%d1,-(%sp)
	andi.l	#0xFFFF,%d0
	move.l	%d0,-(%sp)
	move.l	%a3,-(%sp)
	moveq	#9,%d0
	bra.s	host

handleonly:
	clr.l	-(%sp)
	clr.l	-(%sp)
	move.l	%a3,-(%sp)
host:
	move.l	%d0,-(%sp)
	move.l	#MHI_CMD,-(%sp)
	movea.l	UAELIB(%a6),%a0
	jsr	(%a0)
	lea	20(%sp),%sp
	rts

| d1 the query: the strings, then yes or no from the table
MHIQuery:
	lea	capabilities(%pc),%a0
	tst.l	%d1			| MHIQ_CAPABILITIES
	beq.s	query_string
	lea	decodername(%pc),%a0
	cmp.l	#1000,%d1		| MHIQ_DECODER_NAME
	beq.s	query_string
	lea	idstring(%pc),%a0
	cmp.l	#1001,%d1		| MHIQ_DECODER_VERSION
	beq.s	query_string
	lea	author(%pc),%a0
	cmp.l	#1002,%d1		| MHIQ_AUTHOR
	beq.s	query_string
	moveq	#1,%d0
	cmp.l	#1010,%d1		| MHIQ_IS_HARDWARE: not the 68k's work
	beq.s	query_end
	moveq	#0,%d0
	cmp.l	#QUERIES,%d1
	bcc.s	query_end
	lea	queries(%pc),%a0
	move.b	(%a0,%d1.w),%d0
query_end:
	rts
query_string:
	move.l	%a0,%d0
	rts

| MHIF_SUPPORTED for the queries below 43 that mpg123 and the mixing answer
queries:
	.byte	0, 1, 1, 1, 0, 0, 0, 0, 0, 0	| 0-9: MPEG 1, 2, 2.5
	.byte	1, 1, 1, 0, 0, 0, 0, 0, 0, 0	| 10-19: layers 1, 2, 3
	.byte	1, 1, 0, 0, 0, 0, 0, 0, 0, 0	| 20-29: VBR, joint stereo
	.byte	0, 0, 0, 0, 0, 0, 0, 0, 0, 0	| 30-39: no tone controls
	.byte	1, 1, 1				| 40-42: volume, panning, crossmixing
	.set	QUERIES, . - queries

libname:
	.asciz	"mhiuae.library"
idstring:
	.asciz	"mhiuae 1.0 (01.11.2026)\r\n"
	.ascii	"$VER: mhiuae.library 1.0 (01.11.2026)"
	.byte	0
decodername:
	.asciz	"uae4arm host decoder (mpg123)"
author:
	.asciz	"uae4arm"
capabilities:
	.asciz	"audio/mpeg{audio/mp1,audio/mp2,audio/mp3}"
	.even
endskip:
