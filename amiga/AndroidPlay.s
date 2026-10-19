| AndroidPlay - plays a video file in a window of uae4arm over the Amiga screen
|
| Usage: AndroidPlay [<file>]
| Without a file - from the Shell, or started from its icon - it asks for one
| with the file requester of asl.library. Set as the default tool of a video's
| icon, a double click on the video plays it. "AndroidPlay ?" tells the usage.
|
| A file of a folder drive of uae4arm the emulator hands to the host by its
| path, and the host's decoder plays it: the tool calls uaelib function 200
| with a Lock of the file. One the host cannot reach - inside a hardfile, on a
| floppy, in RAM: - the tool reads and hands over in pieces, into a copy the
| host plays and deletes after; from the Shell it tells how far it is, and
| Ctrl-C stops it.
|
| Built with GNU as for m68k into a plain binary, made an Amiga executable by
| hunk.py - see build.sh. All code is PC relative, so the hunk needs no
| relocation: the pointers in the tag list and in the EasyStruct are filled in
| at run time.
|
| Registers kept: a2 arguments, d3 their length, a3 the file name, a4 uaelib,
| a5 DOSBase, d4 the Lock, d5 what uaelib gave back, d6 the WBStartup message
| (0 from the Shell), d7 the return code.

	.text
	.globl	_start

_start:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	movea.l	%a0,%a2			| arguments
	move.l	%d0,%d3			| and their length
	moveq	#0,%d6
	moveq	#20,%d7			| return code
	lea	vars(%pc),%a0
	clr.l	(%a0)+			| intuitionbase
	clr.l	(%a0)+			| aslbase
	clr.l	(%a0)+			| olddir
	clr.l	(%a0)			| dirchanged

| From Workbench there is no CLI, and a WBStartup message to take and give back
	movea.l	4,%a6
	suba.l	%a1,%a1
	jsr	-294(%a6)		| FindTask(NULL)
	movea.l	%d0,%a3
	tst.l	172(%a3)		| pr_CLI
	bne.s	opendos
	lea	92(%a3),%a0		| pr_MsgPort
	jsr	-384(%a6)		| WaitPort
	lea	92(%a3),%a0
	jsr	-372(%a6)		| GetMsg
	move.l	%d0,%d6
	lea	intuiname(%pc),%a1
	moveq	#36,%d0
	jsr	-552(%a6)		| OpenLibrary: for the messages
	lea	vars(%pc),%a0
	move.l	%d0,(%a0)

opendos:
	lea	dosname(%pc),%a1
	moveq	#0,%d0
	jsr	-552(%a6)		| OpenLibrary
	tst.l	%d0
	beq.w	exit
	movea.l	%d0,%a5			| DOSBase

	tst.l	%d6
	beq.s	shellargs

| From Workbench: the second argument is the file, if there is one
	movea.l	%d6,%a0
	cmp.l	#2,28(%a0)		| sm_NumArgs
	blt.w	askfile
	movea.l	36(%a0),%a0		| sm_ArgList
	move.l	8(%a0),%d1		| wa_Lock of the file's drawer
	movea.l	12(%a0),%a3		| wa_Name
	movea.l	%a5,%a6
	jsr	-126(%a6)		| CurrentDir
	lea	vars(%pc),%a0
	move.l	%d0,8(%a0)		| olddir
	move.l	#1,12(%a0)		| dirchanged
	bra.w	havename

| From the Shell: the first argument, in quotes if it has spaces
shellargs:
	tst.l	%d3
	beq.w	askfile
	move.b	(%a2),%d0
	cmp.b	#' ',%d0
	beq.s	nextspace
	cmp.b	#9,%d0
	bne.s	namestart
nextspace:
	addq.l	#1,%a2
	subq.l	#1,%d3
	bra.s	shellargs
namestart:
	cmp.b	#10,%d0
	beq.w	askfile
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
	beq.w	askfile
	cmp.b	#'?',(%a3)		| "AndroidPlay ?"
	bne.w	havename
	tst.b	1(%a3)
	bne.w	havename
	lea	msg_usage(%pc),%a0
	bsr.w	report
	moveq	#0,%d7
	bra.w	closeall

| No file given: the file requester of asl.library
askfile:
	movea.l	4,%a6
	lea	aslname(%pc),%a1
	moveq	#36,%d0
	jsr	-552(%a6)		| OpenLibrary
	tst.l	%d0
	beq.w	noasl
	lea	vars(%pc),%a0
	move.l	%d0,4(%a0)		| aslbase
	movea.l	%d0,%a6
	lea	asltags(%pc),%a0
	lea	reqtitle(%pc),%a1
	move.l	%a1,4(%a0)		| ASLFR_TitleText
	lea	pattern(%pc),%a1
	move.l	%a1,20(%a0)		| ASLFR_InitialPattern
	moveq	#0,%d0			| ASL_FileRequest
	jsr	-48(%a6)		| AllocAslRequest
	tst.l	%d0
	beq.w	noasl
	move.l	%d0,%d4
	movea.l	%d0,%a0
	suba.l	%a1,%a1
	jsr	-60(%a6)		| AslRequest
	tst.l	%d0
	beq.s	cancelled
	movea.l	%d4,%a0
	movea.l	4(%a0),%a1		| rf_File
	tst.b	(%a1)
	beq.s	cancelled
| The path: the drawer, then the file added to it
	movea.l	8(%a0),%a1		| rf_Dir
	lea	pathbuf(%pc),%a0
	move.w	#PATHSIZE-2,%d0
copydir:
	move.b	(%a1)+,(%a0)+
	dbeq	%d0,copydir
	clr.b	-1(%a0)
	lea	pathbuf(%pc),%a0
	move.l	%a0,%d1
	movea.l	%d4,%a0
	move.l	4(%a0),%d2		| rf_File
	move.l	#PATHSIZE,%d3
	movea.l	%a5,%a6
	jsr	-882(%a6)		| AddPart
	bsr.w	freereq
	lea	pathbuf(%pc),%a3
	bra.s	havename
cancelled:
	bsr.w	freereq
	moveq	#0,%d7
	bra.w	closeall
noasl:
	lea	msg_noasl(%pc),%a0
	bsr.w	report
	moveq	#10,%d7
	bra.w	closeall

| uaelib sits at $F0FF60, or $EFFF60 when the boot ROM moved for CDTV: a trap
| instruction followed by RTS
havename:
	movea.l	#0xF0FF60,%a4
	bsr.w	isuaelib
	beq.s	haveuaelib
	movea.l	#0xEFFF60,%a4
	bsr.w	isuaelib
	beq.s	haveuaelib
	lea	msg_notuae(%pc),%a0
	bsr.w	report
	bra.w	closeall

haveuaelib:
	move.l	%a3,%d1
	moveq	#-2,%d2			| ACCESS_READ
	movea.l	%a5,%a6
	jsr	-84(%a6)		| Lock
	move.l	%d0,%d4
	bne.s	locked
	lea	msg_notfound(%pc),%a0
	bsr.w	report
	moveq	#10,%d7
	bra.w	closeall

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

	cmp.l	#2,%d5		| not on a folder drive: a copy
	bne.s	result
	bsr.w	copyvideo
result:
	moveq	#0,%d7
	cmp.l	#1,%d5
	beq.w	closeall
	moveq	#5,%d7
	lea	msg_stopped(%pc),%a0
	cmp.l	#5,%d5
	beq.s	failed
	moveq	#10,%d7
	lea	msg_oldemu(%pc),%a0
	tst.l	%d5
	beq.s	failed
	lea	msg_noread(%pc),%a0
	cmp.l	#6,%d5
	beq.s	failed
	lea	msg_nomem(%pc),%a0
	cmp.l	#7,%d5
	beq.s	failed
	lea	msg_nocopy(%pc),%a0
	cmp.l	#8,%d5
	beq.s	failed
	lea	msg_cannot(%pc),%a0
failed:
	bsr.w	report

closeall:
	lea	vars(%pc),%a2
	tst.l	12(%a2)			| dirchanged
	beq.s	closeasl
	move.l	8(%a2),%d1		| olddir
	movea.l	%a5,%a6
	jsr	-126(%a6)		| CurrentDir
closeasl:
	movea.l	4,%a6
	move.l	4(%a2),%d0		| aslbase
	beq.s	closedos
	movea.l	%d0,%a1
	jsr	-414(%a6)		| CloseLibrary
closedos:
	movea.l	%a5,%a1
	jsr	-414(%a6)		| CloseLibrary
exit:
	movea.l	4,%a6
	lea	vars(%pc),%a2
	move.l	(%a2),%d0		| intuitionbase
	beq.s	replywb
	movea.l	%d0,%a1
	jsr	-414(%a6)		| CloseLibrary
replywb:
	tst.l	%d6
	beq.s	done
	jsr	-132(%a6)		| Forbid: Workbench unloads us once replied
	movea.l	%d6,%a1
	jsr	-378(%a6)		| ReplyMsg
done:
	move.l	%d7,%d0
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

| A video the host cannot reach goes over to it in pieces, into a copy the host
| plays. %d5 gets the result: 1 played, 3 the host cannot play it, 5 stopped
| by Ctrl-C, 6 not read, 7 no memory, 8 not copied - by an older emulator, or
| without room on the host. %a2 points at vars.
copyvideo:
	lea	vars(%pc),%a2
	clr.l	16(%a2)			| file
	clr.l	20(%a2)			| buffer
	move.l	#-1,36(%a2)		| no progress shown
	moveq	#8,%d5
	move.l	%a3,-(%sp)		| the name, for its extension
	move.l	#4,-(%sp)		| start a copy
	move.l	#200,-(%sp)
	jsr	(%a4)
	lea	12(%sp),%sp
	cmp.l	#1,%d0
	bne.w	copy_end
	moveq	#6,%d5
	move.l	%a3,%d1
	move.l	#1005,%d2		| MODE_OLDFILE
	movea.l	%a5,%a6
	jsr	-30(%a6)		| Open
	move.l	%d0,16(%a2)
	beq.w	copy_drop
| Its size, for the progress: the end, seen going back to the start
	move.l	%d0,%d1
	moveq	#0,%d2
	moveq	#1,%d3			| OFFSET_END
	jsr	-66(%a6)		| Seek
	move.l	16(%a2),%d1
	moveq	#0,%d2
	moveq	#-1,%d3			| OFFSET_BEGINNING
	jsr	-66(%a6)		| Seek
	move.l	%d0,28(%a2)		| total
	bpl.s	1f
	clr.l	28(%a2)
1:	clr.l	32(%a2)			| done
| A large buffer, or a small one with little memory
	moveq	#7,%d5
	movea.l	4,%a6
	move.l	#BIGBUF,24(%a2)
	move.l	#BIGBUF,%d0
	moveq	#1,%d1			| MEMF_PUBLIC
	jsr	-198(%a6)		| AllocMem
	tst.l	%d0
	bne.s	2f
	move.l	#SMALLBUF,24(%a2)
	move.l	#SMALLBUF,%d0
	moveq	#1,%d1
	jsr	-198(%a6)		| AllocMem
	tst.l	%d0
	beq.w	copy_drop
2:	move.l	%d0,20(%a2)
copy_loop:
	bsr.w	progress
	movea.l	4,%a6
	moveq	#0,%d0
	move.l	#0x1000,%d1		| SIGBREAKF_CTRL_C
	jsr	-306(%a6)		| SetSignal: takes Ctrl-C
	moveq	#5,%d5
	btst	#12,%d0
	bne.s	copy_drop
	moveq	#6,%d5
	move.l	16(%a2),%d1
	move.l	20(%a2),%d2
	move.l	24(%a2),%d3
	movea.l	%a5,%a6
	jsr	-42(%a6)		| Read
	tst.l	%d0
	bmi.s	copy_drop
	beq.s	copy_play
	add.l	%d0,32(%a2)		| done
	moveq	#8,%d5
	move.l	%d0,-(%sp)		| bytes
	move.l	20(%a2),-(%sp)		| at
	move.l	#5,-(%sp)		| add them to the copy
	move.l	#200,-(%sp)
	jsr	(%a4)
	lea	16(%sp),%sp
	cmp.l	#1,%d0
	beq.s	copy_loop
	bra.s	copy_close		| the emulator dropped it
copy_play:
	bsr.w	progress_end
	move.l	#6,-(%sp)		| play the copy
	move.l	#200,-(%sp)
	jsr	(%a4)
	addq.l	#8,%sp
	move.l	%d0,%d5
	bra.s	copy_close
copy_drop:
	move.l	#7,-(%sp)		| drop the copy
	move.l	#200,-(%sp)
	jsr	(%a4)
	addq.l	#8,%sp
copy_close:
	bsr.w	progress_end
	movea.l	4,%a6
	move.l	20(%a2),%d0
	beq.s	3f
	movea.l	%d0,%a1
	move.l	24(%a2),%d0
	jsr	-210(%a6)		| FreeMem
3:	move.l	16(%a2),%d1
	beq.s	copy_end
	movea.l	%a5,%a6
	jsr	-36(%a6)		| Close
copy_end:
	rts

| From the Shell: how much of the file went over, when it changed
progress:
	tst.l	%d6
	bne.s	progress_ret
	move.l	32(%a2),%d0		| done
	move.l	28(%a2),%d1		| of all
	beq.s	progress_ret
4:	cmp.l	#0xFFFF,%d1		| small enough for mulu and divu
	bls.s	5f
	lsr.l	#1,%d0
	lsr.l	#1,%d1
	bra.s	4b
5:	mulu	#100,%d0
	divu	%d1,%d0
	and.l	#0xFFFF,%d0
	cmp.l	36(%a2),%d0
	beq.s	progress_ret
	move.l	%d0,36(%a2)
	lea	linebuf(%pc),%a0
	lea	msg_copying(%pc),%a1
6:	move.b	(%a1)+,(%a0)+
	bne.s	6b
	subq.l	#1,%a0
	moveq	#0,%d1			| digits
7:	divu	#10,%d0
	swap	%d0
	add.b	#'0',%d0
	move.b	%d0,-(%sp)
	addq.l	#1,%d1
	clr.w	%d0
	swap	%d0
	tst.l	%d0
	bne.s	7b
8:	move.b	(%sp)+,(%a0)+
	subq.l	#1,%d1
	bne.s	8b
	move.b	#'%',(%a0)+
	clr.b	(%a0)
	lea	linebuf(%pc),%a0
	bsr.w	print
progress_ret:
	rts

| Ends the line of the progress, if there is one
progress_end:
	cmp.l	#-1,36(%a2)
	beq.s	9f
	move.l	#-1,36(%a2)
	lea	newline(%pc),%a0
	bsr.w	print
9:	rts

| Frees the file requester in %d4
freereq:
	lea	vars(%pc),%a0
	movea.l	4(%a0),%a6		| aslbase
	movea.l	%d4,%a0
	jmp	-54(%a6)		| FreeAslRequest

| Z set if %a4 points at a trap followed by RTS
isuaelib:
	move.w	(%a4),%d0
	and.w	#0xF000,%d0
	cmp.w	#0xA000,%d0
	bne.s	isuaelib_end
	cmp.w	#0x4E75,2(%a4)
isuaelib_end:
	rts

| Tells the message at %a0: in the Shell, or in a requester from Workbench
report:
	tst.l	%d6
	beq.s	print
	movem.l	%d2/%a2-%a3/%a6,-(%sp)
	move.l	%a0,%d2
	lea	vars(%pc),%a1
	move.l	(%a1),%d0		| intuitionbase
	beq.s	report_end
	movea.l	%d0,%a6
	lea	easy(%pc),%a1
	move.l	#20,(%a1)		| es_StructSize
	clr.l	4(%a1)			| es_Flags
	lea	reqtitle(%pc),%a0
	move.l	%a0,8(%a1)		| es_Title
	move.l	%d2,12(%a1)		| es_TextFormat
	lea	okgadget(%pc),%a0
	move.l	%a0,16(%a1)		| es_GadgetFormat
	suba.l	%a0,%a0			| no window
	suba.l	%a2,%a2
	suba.l	%a3,%a3
	jsr	-588(%a6)		| EasyRequestArgs
report_end:
	movem.l	(%sp)+,%d2/%a2-%a3/%a6
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

	.even
vars:		.long	0, 0, 0, 0	| intuitionbase, aslbase, olddir, dirchanged
		.long	0, 0, 0, 0, 0, 0	| copy: file, buffer, its size, total, done, percent shown
easy:		.long	0, 0, 0, 0, 0	| struct EasyStruct
asltags:	.long	0x80080001, 0	| ASLFR_TitleText
		.long	0x80080032, 1	| ASLFR_DoPatterns
		.long	0x8008000A, 0	| ASLFR_InitialPattern
		.long	0x8008003C, 1	| ASLFR_RejectIcons
		.long	0		| TAG_DONE

	.set	PATHSIZE, 512
pathbuf:	.space	PATHSIZE
	.set	BIGBUF, 262144
	.set	SMALLBUF, 32768
linebuf:	.space	48

dosname:	.asciz	"dos.library"
aslname:	.asciz	"asl.library"
intuiname:	.asciz	"intuition.library"
reqtitle:	.asciz	"AndroidPlay"
okgadget:	.asciz	"Ok"
pattern:	.asciz	"#?.(mp4|m4v|mkv|webm|mov|avi|3gp|mpg|mpeg|ts|wmv|flv)"
msg_usage:	.ascii	"Usage: AndroidPlay [<file>]\n"
		.ascii	"Plays a video file in a window of uae4arm over the Amiga screen;\n"
		.ascii	"without a file it asks for one. A file the host cannot reach,\n"
		.ascii	"such as one inside a hardfile, goes over to it first;\n"
		.asciz	"Ctrl-C stops that.\n"
msg_notuae:	.asciz	"AndroidPlay: this runs in uae4arm only\n"
msg_notfound:	.asciz	"AndroidPlay: no such file\n"
msg_oldemu:	.asciz	"AndroidPlay: this needs a newer uae4arm\n"
msg_copying:	.asciz	"\rAndroidPlay: processing "
newline:	.asciz	"\n"
msg_stopped:	.asciz	"AndroidPlay: stopped\n"
msg_noread:	.asciz	"AndroidPlay: cannot read the file\n"
msg_nomem:	.asciz	"AndroidPlay: not enough memory\n"
msg_nocopy:	.asciz	"AndroidPlay: uae4arm could not take the file - it needs a newer uae4arm, or more room\n"
msg_cannot:	.asciz	"AndroidPlay: uae4arm cannot play it here\n"
msg_noasl:	.asciz	"AndroidPlay: no file requester - asl.library 36 or later is needed\n"
		.even
