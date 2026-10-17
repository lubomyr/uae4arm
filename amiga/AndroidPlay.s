| AndroidPlay - plays a video file in a window of uae4arm over the Amiga screen
|
| Usage: AndroidPlay <file>
| The file has to be on a folder drive of uae4arm, not inside a hardfile: the
| emulator hands its path to the host, and the host's decoder plays it. The
| tool calls uaelib function 200 with a Lock of the file.
|
| Built with GNU as for m68k into a plain binary, made an Amiga executable by
| hunk.py - see build.sh. All code is PC relative, so the hunk needs no
| relocation.

	.text
	.globl	_start

_start:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	movea.l	%a0,%a2			| arguments
	move.l	%d0,%d3			| and their length
	moveq	#20,%d7			| return code
	movea.l	4,%a6			| ExecBase
	lea	dosname(%pc),%a1
	moveq	#0,%d0
	jsr	-552(%a6)		| OpenLibrary
	tst.l	%d0
	beq.w	exit
	movea.l	%d0,%a5			| DOSBase

| The file name: the first argument, in quotes if it has spaces
skipspace:
	tst.l	%d3
	beq.w	usage
	move.b	(%a2),%d0
	cmp.b	#' ',%d0
	beq.s	nextspace
	cmp.b	#9,%d0
	bne.s	namestart
nextspace:
	addq.l	#1,%a2
	subq.l	#1,%d3
	bra.s	skipspace
namestart:
	cmp.b	#10,%d0
	beq.w	usage
	moveq	#' ',%d1		| what ends the name
	cmp.b	#'"',%d0
	bne.s	nameset
	addq.l	#1,%a2
	subq.l	#1,%d3
	moveq	#'"',%d1
nameset:
	movea.l	%a2,%a3			| the name starts here
nameloop:
	tst.l	%d3
	beq.s	nameend
	move.b	(%a2),%d0
	cmp.b	%d1,%d0
	beq.s	nameend
	cmp.b	#10,%d0
	beq.s	nameend
	addq.l	#1,%a2
	subq.l	#1,%d3
	bra.s	nameloop
nameend:
	clr.b	(%a2)
	cmpa.l	%a3,%a2
	beq.w	usage

| uaelib sits at $F0FF60, or $EFFF60 when the boot ROM moved for CDTV: a trap
| instruction followed by RTS
	movea.l	#0xF0FF60,%a4
	bsr.w	isuaelib
	beq.s	haveuaelib
	movea.l	#0xEFFF60,%a4
	bsr.w	isuaelib
	beq.s	haveuaelib
	lea	msg_notuae(%pc),%a0
	bsr.w	print
	bra.w	closedos

haveuaelib:
	move.l	%a3,%d1
	moveq	#-2,%d2			| ACCESS_READ
	movea.l	%a5,%a6
	jsr	-84(%a6)		| Lock
	move.l	%d0,%d4
	bne.s	locked
	lea	msg_notfound(%pc),%a0
	bsr.w	print
	moveq	#10,%d7
	bra.w	closedos

locked:
	move.l	%d4,-(%sp)		| the Lock
	move.l	#1,-(%sp)		| play a video
	move.l	#200,-(%sp)		| uaelib function
	jsr	(%a4)
	lea	12(%sp),%sp
	move.l	%d0,%d5

	move.l	%d4,%d1
	movea.l	%a5,%a6
	jsr	-90(%a6)		| UnLock

	moveq	#0,%d7
	cmp.l	#1,%d5
	beq.s	closedos
	moveq	#10,%d7
	lea	msg_oldemu(%pc),%a0
	tst.l	%d5
	beq.s	failed
	lea	msg_notfolder(%pc),%a0
	cmp.l	#2,%d5
	beq.s	failed
	lea	msg_cannot(%pc),%a0
failed:
	bsr.w	print
	bra.s	closedos

usage:
	lea	msg_usage(%pc),%a0
	bsr.w	print
	moveq	#10,%d7

closedos:
	movea.l	%a5,%a1
	movea.l	4,%a6
	jsr	-414(%a6)		| CloseLibrary
exit:
	move.l	%d7,%d0
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

| Z set if %a4 points at a trap followed by RTS
isuaelib:
	move.w	(%a4),%d0
	and.w	#0xF000,%d0
	cmp.w	#0xA000,%d0
	bne.s	isuaelib_end
	cmp.w	#0x4E75,2(%a4)
isuaelib_end:
	rts

| Writes the string at %a0 to the output
print:
	movem.l	%d2-%d3/%a6,-(%sp)
	move.l	%a0,%d2
	moveq	#-1,%d3
print_len:
	addq.l	#1,%d3
	tst.b	(%a0)+
	bne.s	print_len
	movea.l	%a5,%a6
	jsr	-60(%a6)		| Output
	move.l	%d0,%d1
	beq.s	print_end
	jsr	-48(%a6)		| Write
print_end:
	movem.l	(%sp)+,%d2-%d3/%a6
	rts

dosname:	.asciz	"dos.library"
msg_usage:	.ascii	"Usage: AndroidPlay <file>\n"
		.ascii	"Plays a video file in a window of uae4arm over the Amiga screen.\n"
		.asciz	"The file has to be on a folder drive, not inside a hardfile.\n"
msg_notuae:	.asciz	"AndroidPlay: this runs in uae4arm only\n"
msg_notfound:	.asciz	"AndroidPlay: no such file\n"
msg_oldemu:	.asciz	"AndroidPlay: this needs a newer uae4arm\n"
msg_notfolder:	.asciz	"AndroidPlay: the file has to be on a folder drive, not inside a hardfile\n"
msg_cannot:	.asciz	"AndroidPlay: uae4arm cannot play it here\n"
		.even
