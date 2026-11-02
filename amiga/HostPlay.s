| HostPlay - plays a video in a window of the Amiga, decoded by the host
|
| Usage: HostPlay [<file>]
| Without a file - from the Shell, or started from its icon - it asks for one
| with the file requester of asl.library. Set as the default tool of a
| video's icon, a double click on the video plays it. Space pauses, Esc or
| the close gadget ends it; at the end of the video the window closes.
| The file has to be on a folder drive of uae4arm, where the host reaches it.
|
| The window opens on the Workbench screen, which has to be an RTG one
| (uaegfx) with 15, 16, 24 or 32 bits. The tool hands the emulator a buffer
| for the inside of the window through uaelib function 202; the emulator
| puts each frame into it, scaled to the window and in the screen's own
| pixel format, and signals the tool, which blits it with
| p96WritePixelArray(). The 68k only copies, and the layers clip the frame
| where other windows lie over it.
|
| Built with GNU as for m68k into a plain binary, made an Amiga executable by
| hunk.py - see build.sh. All code is PC relative, so the hunk needs no
| relocation: the pointers in the tag list are filled in at run time.
|
| Registers kept: a3 the window (the file name before it opens), a4 uaelib, a5 DOSBase, d5 the emulator's
| handle of the player, d6 the WBStartup message (0 from the Shell), d7 the
| return code.

	.set	HOSTVIDEO, 202		| uaelib function

	.set	P96BMA_BYTESPERPIXEL, 5
	.set	P96BMA_RGBFORMAT, 7
	.set	P96BMA_ISP96, 8

	.set	IDCMP_NEWSIZE, 0x2
	.set	IDCMP_REFRESHWINDOW, 0x4
	.set	IDCMP_CLOSEWINDOW, 0x200
	.set	IDCMP_VANILLAKEY, 0x200000

| vars
	.set	INTUITIONBASE, 0
	.set	P96BASE, 4
	.set	SIGBIT, 8
	.set	BUFFER, 12
	.set	WIDTH, 16		| of the inside of the window
	.set	HEIGHT, 20
	.set	RGBFORMAT, 24
	.set	BPP, 28
	.set	TASK, 32
	.set	ASLBASE, 36
	.set	OLDDIR, 40
	.set	DIRCHANGED, 44
	.set	NAME, 48
	.set	VARSIZE, 52

	.text
	.globl	_start

_start:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	movea.l	%a0,%a2			| arguments
	move.l	%d0,%d3			| and their length
	moveq	#0,%d5
	moveq	#0,%d6
	moveq	#20,%d7			| return code
	suba.l	%a3,%a3
	suba.l	%a5,%a5
	lea	vars(%pc),%a0
	moveq	#VARSIZE/4-1,%d0
1:	clr.l	(%a0)+
	dbra	%d0,1b
	lea	vars(%pc),%a0
	move.l	#-1,SIGBIT(%a0)

| From Workbench there is no CLI, and a WBStartup message to take and give back
	movea.l	4,%a6
	suba.l	%a1,%a1
	jsr	-294(%a6)		| FindTask(NULL)
	lea	vars(%pc),%a0
	move.l	%d0,TASK(%a0)
	movea.l	%d0,%a1
	tst.l	172(%a1)		| pr_CLI
	bne.s	opendos
	lea	92(%a1),%a0		| pr_MsgPort
	jsr	-384(%a6)		| WaitPort
	lea	vars(%pc),%a0
	movea.l	TASK(%a0),%a0
	lea	92(%a0),%a0
	jsr	-372(%a6)		| GetMsg
	move.l	%d0,%d6

opendos:
	lea	dosname(%pc),%a1
	moveq	#0,%d0
	jsr	-552(%a6)		| OpenLibrary
	tst.l	%d0
	beq.w	exit
	movea.l	%d0,%a5			| DOSBase
	lea	intuiname(%pc),%a1
	moveq	#36,%d0
	jsr	-552(%a6)		| OpenLibrary
	lea	vars(%pc),%a0
	move.l	%d0,INTUITIONBASE(%a0)
	beq.w	closeall
	lea	p96name(%pc),%a1
	moveq	#2,%d0
	jsr	-552(%a6)		| OpenLibrary
	lea	vars(%pc),%a0
	move.l	%d0,P96BASE(%a0)
	bne.s	findlib
	lea	msg_nop96(%pc),%a0
	bra.w	failed

| uaelib sits at $F0FF60, or $EFFF60 when the boot ROM moved for CDTV: a trap
| instruction followed by RTS
findlib:
	movea.l	#0xF0FF60,%a4
	bsr.w	isuaelib
	beq.s	haveuaelib
	movea.l	#0xEFFF60,%a4
	bsr.w	isuaelib
	beq.s	haveuaelib
	lea	msg_notuae(%pc),%a0
	bra.w	failed

haveuaelib:
	moveq	#0,%d0			| the version of the host's side, 0 for none
	bsr.w	callhost
	tst.l	%d0
	bne.s	newenough
	lea	msg_oldemu(%pc),%a0
	bra.w	failed
newenough:
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
	move.l	%d0,OLDDIR(%a0)
	move.l	#1,DIRCHANGED(%a0)
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
	cmp.b	#'?',(%a3)		| "HostPlay ?"
	bne.w	havename
	tst.b	1(%a3)
	bne.w	havename
	lea	msg_usage(%pc),%a0
	bsr.w	report
	moveq	#0,%d7
	suba.l	%a3,%a3
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
	move.l	%d0,ASLBASE(%a0)
	movea.l	%d0,%a6
	lea	asltags(%pc),%a0
	lea	title(%pc),%a1
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
	bra.w	failed

| The file name goes to the vars, a3 is the window's from here
havename:
	lea	vars(%pc),%a0
	move.l	%a3,NAME(%a0)
	suba.l	%a3,%a3
	movea.l	4,%a6
	moveq	#-1,%d0
	jsr	-330(%a6)		| AllocSignal
	ext.w	%d0
	ext.l	%d0
	lea	vars(%pc),%a0
	move.l	%d0,SIGBIT(%a0)
	bpl.s	havesignal
	lea	msg_nosignal(%pc),%a0
	bra.w	failed
havesignal:
	move.l	NAME(%a0),%d1
	moveq	#-2,%d2			| ACCESS_READ
	movea.l	%a5,%a6
	jsr	-84(%a6)		| Lock
	move.l	%d0,%d5
	bne.s	locked
	lea	msg_notfound(%pc),%a0
	bra.w	failed
locked:
	lea	vars(%pc),%a0
	moveq	#1,%d0			| a player, for our task and signal
	move.l	TASK(%a0),%d1
	moveq	#0,%d2
	move.l	SIGBIT(%a0),%d3
	bset	%d3,%d2
	move.l	%d5,%d3			| the Lock of the file
	moveq	#0,%d4
	bsr.w	callhost
	move.l	%d0,-(%sp)
	move.l	%d5,%d1
	movea.l	%a5,%a6
	jsr	-90(%a6)		| UnLock
	move.l	(%sp)+,%d5
	bgt.s	haveplayer
	lea	msg_busy(%pc),%a0
	tst.l	%d5
	beq.s	openfailed
	lea	msg_nopath(%pc),%a0
	cmp.l	#-2,%d5
	beq.s	openfailed
	lea	msg_cannot(%pc),%a0
openfailed:
	moveq	#0,%d5
	bra.w	failed

| The window, at the size of the video
haveplayer:
	moveq	#3,%d0			| the size of the video
	move.l	%d5,%d1
	bsr.w	callhost
	moveq	#0,%d2
	move.w	%d0,%d2			| height
	swap	%d0
	moveq	#0,%d1
	move.w	%d0,%d1			| width
| A video larger than the screen gets a smaller window of the same shape
	movem.l	%d1-%d2,-(%sp)
	lea	vars(%pc),%a0
	movea.l	INTUITIONBASE(%a0),%a6
	suba.l	%a0,%a0
	jsr	-510(%a6)		| LockPubScreen(NULL)
	movem.l	(%sp)+,%d1-%d2
	tst.l	%d0
	beq.s	sized
	movea.l	%d0,%a1
	moveq	#0,%d3
	move.w	12(%a1),%d3		| sc_Width
	moveq	#0,%d4
	move.w	14(%a1),%d4		| sc_Height
	movem.l	%d1-%d2,-(%sp)
	suba.l	%a0,%a0
	jsr	-516(%a6)		| UnlockPubScreen
	movem.l	(%sp)+,%d1-%d2
	sub.l	#32,%d3			| room for the borders
	sub.l	#64,%d4			| and the screen's title bar
	cmp.l	%d3,%d1
	bls.s	1f
	mulu	%d3,%d2
	divu	%d1,%d2
	and.l	#0xFFFF,%d2
	move.l	%d3,%d1
1:	cmp.l	%d4,%d2
	bls.s	sized
	mulu	%d4,%d1
	divu	%d2,%d1
	and.l	#0xFFFF,%d1
	move.l	%d4,%d2
sized:
	lea	wintags(%pc),%a1
	move.l	%d1,4(%a1)		| WA_InnerWidth
	move.l	%d2,12(%a1)		| WA_InnerHeight
	lea	vars(%pc),%a0
	move.l	NAME(%a0),%d1
	movea.l	%a5,%a6
	jsr	-870(%a6)		| FilePart: the title
	lea	wintags(%pc),%a1
	move.l	%d0,20(%a1)		| WA_Title
	lea	vars(%pc),%a0
	movea.l	INTUITIONBASE(%a0),%a6
	suba.l	%a0,%a0
	jsr	-606(%a6)		| OpenWindowTagList
	movea.l	%d0,%a3
	tst.l	%d0
	bne.s	havewindow
	lea	msg_nowindow(%pc),%a0
	bra.w	failed

| The screen's bitmap: an RTG one, in a format the host makes
havewindow:
	lea	vars(%pc),%a2
	movea.l	P96BASE(%a2),%a6
	movea.l	50(%a3),%a0		| wd_RPort
	movea.l	4(%a0),%a0		| rp_BitMap
	moveq	#P96BMA_ISP96,%d0
	jsr	-42(%a6)		| p96GetBitMapAttr
	tst.l	%d0
	bne.s	isrtg
	lea	msg_notrtg(%pc),%a0
	bra.w	failed
isrtg:
	movea.l	50(%a3),%a0
	movea.l	4(%a0),%a0
	moveq	#P96BMA_RGBFORMAT,%d0
	jsr	-42(%a6)		| p96GetBitMapAttr
	move.l	%d0,RGBFORMAT(%a2)
	movea.l	50(%a3),%a0
	movea.l	4(%a0),%a0
	moveq	#P96BMA_BYTESPERPIXEL,%d0
	jsr	-42(%a6)		| p96GetBitMapAttr
	move.l	%d0,BPP(%a2)
	bsr.w	target
	tst.l	%d0
	bmi.w	badformat
	beq.w	nomem
	bsr.w	draw

| Waits for a frame, the window, or Ctrl-C
mainloop:
	movea.l	4,%a6
	lea	vars(%pc),%a2
	moveq	#0,%d0
	move.l	SIGBIT(%a2),%d1
	bset	%d1,%d0
	movea.l	86(%a3),%a0		| wd_UserPort
	move.b	15(%a0),%d1		| mp_SigBit
	bset	%d1,%d0
	bset	#12,%d0			| SIGBREAKF_CTRL_C
	jsr	-318(%a6)		| Wait
	move.l	%d0,%d2
	btst	#12,%d2
	bne.w	quit
	move.l	SIGBIT(%a2),%d1
	btst	%d1,%d2
	beq.s	msgloop
	moveq	#5,%d0			| the next frame, if one fell due
	move.l	%d5,%d1
	bsr.w	callhost
	cmp.l	#2,%d0			| the end of the video
	beq.w	quit
	tst.l	%d0
	beq.s	msgloop
	bsr.w	draw

msgloop:
	movea.l	4,%a6
	movea.l	86(%a3),%a0		| wd_UserPort
	jsr	-372(%a6)		| GetMsg
	tst.l	%d0
	beq.s	mainloop
	movea.l	%d0,%a1
	move.l	20(%a1),%d2		| im_Class
	move.w	24(%a1),%d3		| im_Code
	jsr	-378(%a6)		| ReplyMsg
	cmp.l	#IDCMP_CLOSEWINDOW,%d2
	beq.w	quit
	cmp.l	#IDCMP_NEWSIZE,%d2
	bne.s	notsize
	bsr.w	target
	tst.l	%d0
	bmi.w	badformat
	beq.w	nomem
	bsr.w	draw
	bra.w	msgloop
notsize:
	cmp.l	#IDCMP_REFRESHWINDOW,%d2
	bne.s	notrefresh
	lea	vars(%pc),%a2
	movea.l	INTUITIONBASE(%a2),%a6
	movea.l	%a3,%a0
	jsr	-354(%a6)		| BeginRefresh
	bsr.w	draw
	lea	vars(%pc),%a2
	movea.l	INTUITIONBASE(%a2),%a6
	movea.l	%a3,%a0
	moveq	#1,%d0
	jsr	-366(%a6)		| EndRefresh
	bra.w	msgloop
notrefresh:
	cmp.l	#IDCMP_VANILLAKEY,%d2
	bne.w	msgloop
	cmp.b	#27,%d3			| Esc
	beq.s	quit
	cmp.b	#' ',%d3
	bne.w	msgloop
	moveq	#6,%d0			| pause, or play on
	move.l	%d5,%d1
	bsr.w	callhost
	bra.w	msgloop

badformat:
	lea	msg_format(%pc),%a0
	bra.s	failed
nomem:
	lea	msg_nomem(%pc),%a0
failed:
	bsr.w	report
	moveq	#10,%d7
	bra.w	closeall
quit:
	moveq	#0,%d7

closeall:
	tst.l	%d5			| first the player, so that nothing more
	beq.s	1f			| goes into the buffer
	moveq	#2,%d0
	move.l	%d5,%d1
	bsr.w	callhost
1:	lea	vars(%pc),%a2
	move.l	%a3,%d0
	beq.s	2f
	movea.l	INTUITIONBASE(%a2),%a6
	movea.l	%a3,%a0
	jsr	-72(%a6)		| CloseWindow
2:	bsr.w	freebuffer
	movea.l	4,%a6
	move.l	SIGBIT(%a2),%d0
	bmi.s	3f
	jsr	-336(%a6)		| FreeSignal
3:	tst.l	DIRCHANGED(%a2)
	beq.s	6f
	move.l	OLDDIR(%a2),%d1
	movea.l	%a5,%a6
	jsr	-126(%a6)		| CurrentDir
	movea.l	4,%a6
6:	move.l	ASLBASE(%a2),%d0
	beq.s	7f
	movea.l	%d0,%a1
	jsr	-414(%a6)		| CloseLibrary
7:	move.l	P96BASE(%a2),%d0
	beq.s	4f
	movea.l	%d0,%a1
	jsr	-414(%a6)		| CloseLibrary
4:	move.l	INTUITIONBASE(%a2),%d0
	beq.s	5f
	movea.l	%d0,%a1
	jsr	-414(%a6)		| CloseLibrary
5:	movea.l	%a5,%a1
	jsr	-414(%a6)		| CloseLibrary
exit:
	tst.l	%d6
	beq.s	done
	movea.l	4,%a6
	jsr	-132(%a6)		| Forbid: Workbench unloads us once replied
	movea.l	%d6,%a1
	jsr	-378(%a6)		| ReplyMsg
done:
	move.l	%d7,%d0
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

| Makes the buffer for the inside of the window anew and hands it to the
| emulator, which puts the frame of now into it. %d0: 1 done, 0 no memory,
| -1 the screen's format is not one the emulator makes.
target:
	movem.l	%d2-%d4/%a2/%a6,-(%sp)
	lea	vars(%pc),%a2
	bsr.w	freebuffer
	moveq	#0,%d0
	moveq	#0,%d2
	move.w	8(%a3),%d2		| wd_Width
	move.b	54(%a3),%d0		| wd_BorderLeft
	sub.l	%d0,%d2
	move.b	56(%a3),%d0		| wd_BorderRight
	sub.l	%d0,%d2
	moveq	#0,%d3
	move.w	10(%a3),%d3		| wd_Height
	move.b	55(%a3),%d0		| wd_BorderTop
	sub.l	%d0,%d3
	move.b	57(%a3),%d0		| wd_BorderBottom
	sub.l	%d0,%d3
	move.l	%d2,WIDTH(%a2)
	move.l	%d3,HEIGHT(%a2)
	lea	renderinfo(%pc),%a0
	move.l	%d2,%d0
	mulu	BPP+2(%a2),%d0		| bytes per row
	move.w	%d0,4(%a0)		| ri_BytesPerRow
	move.l	RGBFORMAT(%a2),8(%a0)	| ri_RGBFormat
	mulu	%d3,%d0
	moveq	#0,%d1			| MEMF_ANY
	movea.l	4,%a6
	jsr	-684(%a6)		| AllocVec
	move.l	%d0,BUFFER(%a2)
	beq.s	target_end
	lea	renderinfo(%pc),%a0
	move.l	%d0,(%a0)		| ri_Memory
	move.l	%d0,%d2			| the buffer, its size and format
	move.l	WIDTH(%a2),%d3
	swap	%d3
	move.w	HEIGHT+2(%a2),%d3
	move.l	RGBFORMAT(%a2),%d4
	move.l	%d5,%d1
	moveq	#4,%d0
	bsr.w	callhost
	tst.l	%d0
	bne.s	target_end
	bsr.s	freebuffer
	moveq	#-1,%d0
target_end:
	movem.l	(%sp)+,%d2-%d4/%a2/%a6
	rts

| Frees the buffer, if there is one
freebuffer:
	movem.l	%a2/%a6,-(%sp)
	lea	vars(%pc),%a2
	move.l	BUFFER(%a2),%d0
	beq.s	1f
	clr.l	BUFFER(%a2)
	movea.l	%d0,%a1
	movea.l	4,%a6
	jsr	-690(%a6)		| FreeVec
1:	movem.l	(%sp)+,%a2/%a6
	rts

| Blits the buffer into the inside of the window
draw:
	movem.l	%d2-%d5/%a2/%a6,-(%sp)
	lea	vars(%pc),%a2
	tst.l	BUFFER(%a2)
	beq.s	draw_end
	movea.l	P96BASE(%a2),%a6
	move.l	WIDTH(%a2),%d4
	move.l	HEIGHT(%a2),%d5
	lea	renderinfo(%pc),%a0
	moveq	#0,%d0
	moveq	#0,%d1
	movea.l	50(%a3),%a1		| wd_RPort
	moveq	#0,%d2
	move.b	54(%a3),%d2		| wd_BorderLeft
	moveq	#0,%d3
	move.b	55(%a3),%d3		| wd_BorderTop
	jsr	-102(%a6)		| p96WritePixelArray
draw_end:
	movem.l	(%sp)+,%d2-%d5/%a2/%a6
	rts

| Calls the emulator: %d0 the command, %d1-%d4 its arguments; %d0 the result
callhost:
	move.l	%d4,-(%sp)
	move.l	%d3,-(%sp)
	move.l	%d2,-(%sp)
	move.l	%d1,-(%sp)
	move.l	%d0,-(%sp)
	move.l	#HOSTVIDEO,-(%sp)
	jsr	(%a4)
	lea	24(%sp),%sp
	rts

| Frees the file requester in %d4
freereq:
	lea	vars(%pc),%a0
	movea.l	ASLBASE(%a0),%a6
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
	move.l	INTUITIONBASE(%a1),%d0
	beq.s	report_end
	movea.l	%d0,%a6
	lea	easy(%pc),%a1
	move.l	#20,(%a1)		| es_StructSize
	clr.l	4(%a1)			| es_Flags
	lea	title(%pc),%a0
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
vars:		.space	VARSIZE
renderinfo:	.long	0		| ri_Memory
		.word	0, 0		| ri_BytesPerRow, pad
		.long	0		| ri_RGBFormat
easy:		.long	0, 0, 0, 0, 0	| struct EasyStruct
asltags:	.long	0x80080001, 0	| ASLFR_TitleText
		.long	0x80080032, 1	| ASLFR_DoPatterns
		.long	0x8008000A, 0	| ASLFR_InitialPattern
		.long	0x8008003C, 1	| ASLFR_RejectIcons
		.long	0		| TAG_DONE
wintags:	.long	0x80000076, 0	| WA_InnerWidth
		.long	0x80000077, 0	| WA_InnerHeight
		.long	0x8000006E, 0	| WA_Title
		.long	0x8000006A, IDCMP_NEWSIZE+IDCMP_REFRESHWINDOW+IDCMP_CLOSEWINDOW+IDCMP_VANILLAKEY
		.long	0x80000072, 64	| WA_MinWidth
		.long	0x80000073, 48	| WA_MinHeight
		.long	0x80000074, -1	| WA_MaxWidth: the screen's
		.long	0x80000075, -1	| WA_MaxHeight
		.long	0x80000081, 1	| WA_SizeGadget
		.long	0x80000082, 1	| WA_DragBar
		.long	0x80000083, 1	| WA_DepthGadget
		.long	0x80000084, 1	| WA_CloseGadget
		.long	0x80000089, 1	| WA_Activate
		.long	0x8000008C, 1	| WA_SimpleRefresh
		.long	0x8000008F, 1	| WA_SizeBBottom
		.long	0		| TAG_DONE

	.set	PATHSIZE, 512
pathbuf:	.space	PATHSIZE

dosname:	.asciz	"dos.library"
aslname:	.asciz	"asl.library"
intuiname:	.asciz	"intuition.library"
p96name:	.asciz	"Picasso96API.library"
title:		.asciz	"HostPlay"
okgadget:	.asciz	"Ok"
pattern:	.asciz	"#?.(mp4|m4v|mkv|webm|avi|mov|mpg|mpeg|ts|wmv|flv|3gp|ogv)"
msg_usage:	.ascii	"Usage: HostPlay [<file>]\n"
		.ascii	"Plays a video in a window, decoded by uae4arm; without a file\n"
		.ascii	"it asks for one. Space pauses, Esc ends it. The file has to be\n"
		.asciz	"on a folder drive of uae4arm.\n"
msg_notfound:	.asciz	"HostPlay: no such file\n"
msg_nopath:	.asciz	"HostPlay: the file has to be on a folder drive of uae4arm\n"
msg_cannot:	.asciz	"HostPlay: uae4arm cannot play this file\n"
msg_noasl:	.asciz	"HostPlay: no file requester - asl.library 36 or later is needed\n"
msg_nop96:	.asciz	"HostPlay: this needs Picasso96API.library\n"
msg_notuae:	.asciz	"HostPlay: this runs in uae4arm only\n"
msg_oldemu:	.asciz	"HostPlay: this needs a newer uae4arm\n"
msg_nosignal:	.asciz	"HostPlay: no free signal\n"
msg_busy:	.asciz	"HostPlay: uae4arm plays two videos already\n"
msg_nowindow:	.asciz	"HostPlay: the window does not open\n"
msg_notrtg:	.asciz	"HostPlay: the Workbench screen has to be an RTG one\n"
msg_format:	.asciz	"HostPlay: the screen needs 15, 16, 24 or 32 bits\n"
msg_nomem:	.asciz	"HostPlay: not enough memory for the window\n"
		.even
