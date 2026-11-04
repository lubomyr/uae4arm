| HostPlay - plays a video in a window of the Amiga, decoded by the host
| HostView - shows a picture the same way: built from this source with
|            HOSTVIEW set (see build.sh), without the slider, the play
|            button and the sound
|
| Usage: HostPlay [<file>]
| Without a file - from the Shell, or started from its icon - it asks for one
| with the file requester of asl.library. Set as the default tool of a
| video's icon, a double click on the video plays it. A file of a folder
| drive of uae4arm the host plays where it is; one it cannot reach - inside
| a hardfile, on a floppy, in RAM: - goes over to it first, into a copy it
| deletes after: from the Shell HostPlay tells how far it is, and Ctrl-C
| stops it.
|
| In the bottom border of the window: a slider for where the video is, to
| drag or click on to go to another place; a button that pauses and plays
| on, or plays again from the start once the video ended; and a button that
| asks for another file. Space pauses too, Esc or the close gadget ends it.
| The zoom gadget makes the window fill the screen, and gives it its size
| back. The title tells the time played and the length of the video.
|
| The window opens on the Workbench screen, which has to be an RTG one
| (uaegfx) with 16 or 32 bits; the emulator makes Picasso96's 15 and 24 bit
| formats too, but uaegfx offers none. The tool hands the emulator a buffer
| for the inside of the window through uaelib function 202; the emulator
| puts each frame into it, scaled to the window and in the screen's own
| pixel format, and signals the tool, which blits it with
| p96WritePixelArray(). The 68k only copies, and the layers clip the frame
| where other windows lie over it.
|
| Built with GNU as for m68k into a plain binary, made an Amiga executable by
| hunk.py - see build.sh. All code is PC relative, so the hunk needs no
| relocation: the pointers in the tag lists are filled in at run time.
|
| Registers kept: a3 the window, a4 uaelib, a5 DOSBase, d5 the emulator's
| handle of the player, d6 the WBStartup message (0 from the Shell), d7 the
| return code.

	.set	HOSTVIDEO, 202		| uaelib function
	.ifdef	HOSTVIEW
	.set	HOSTVIDEO_VERSION, 4	| pictures
	.else
	.set	HOSTVIDEO_VERSION, 3	| the seek, the time, the copy
	.endif
	.set	HOSTSHOW, 200		| uaelib function that makes the copy

	.set	P96BMA_BYTESPERPIXEL, 5
	.set	P96BMA_RGBFORMAT, 7
	.set	P96BMA_ISP96, 8

	.set	IDCMP_NEWSIZE, 0x2
	.set	IDCMP_REFRESHWINDOW, 0x4
	.set	IDCMP_GADGETDOWN, 0x20
	.set	IDCMP_GADGETUP, 0x40
	.set	IDCMP_CLOSEWINDOW, 0x200
	.set	IDCMP_VANILLAKEY, 0x200000

	.set	GA_Left, 0x80030001
	.set	GA_RelRight, 0x80030002
	.set	GA_RelBottom, 0x80030004
	.set	GA_RelWidth, 0x80030006
	.set	GA_Height, 0x80030007
	.set	GA_Image, 0x8003000A
	.set	GA_ID, 0x80030010
	.set	GA_Immediate, 0x80030015
	.set	GA_RelVerify, 0x80030016
	.set	GA_BottomBorder, 0x8003001B
	.set	GA_Previous, 0x8003001F
	.set	PGA_Freedom, 0x80031001
	.set	PGA_Borderless, 0x80031002
	.set	PGA_Total, 0x80031007
	.set	PGA_Visible, 0x80031008
	.set	PGA_Top, 0x80031009
	.set	PGA_NewLook, 0x8003100A
	.set	IA_Width, 0x80020003
	.set	IA_Height, 0x80020004
	.set	SYSIA_Size, 0x8002000B
	.set	SYSIA_Which, 0x8002000D
	.set	SYSIA_DrawInfo, 0x80020018

	.set	GAD_SLIDER, 1
	.set	GAD_PLAY, 2
	.set	GAD_OPEN, 3

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
	.set	SCREEN, 52
	.set	DRAWINFO, 56
	.set	IMGSIZE, 60		| the images - of the size gadget, for its size -
	.set	IMGPLAY, 64
	.set	IMGOPEN, 68
	.set	GADSLIDER, 72		| and the gadgets, linked in this order
	.set	GADPLAY, 76
	.set	GADOPEN, 80
	.set	DURATION, 84		| in seconds
	.set	SHOWN, 88		| the second the slider shows
	.set	ATEND, 92
	.set	DRAGGING, 96
	.set	STORE, 100		| for GetAttr()
	.set	SIZEW, 104
	.set	SIZEH, 108
	.set	PLAYW, 112
	.set	OPENW, 116
	.set	CFILE, 120		| the copy: the file, the buffer, its size,
	.set	CBUF, 124		| the length of the file, how much went over,
	.set	CBUFSIZE, 128		| the percent shown
	.set	CTOTAL, 132
	.set	CDONE, 136
	.set	CPERCENT, 140
	.set	TITLETURN, 144		| which of the two titles with the time
	.set	CLICKED, 148		| the second clicked on in the slider, or -1
	.set	VARSIZE, 152

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
	move.l	#-1,CLICKED(%a0)

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
	moveq	#39,%d0
	jsr	-552(%a6)		| OpenLibrary
	lea	vars(%pc),%a0
	move.l	%d0,INTUITIONBASE(%a0)
	bne.s	openp96
	lea	msg_nointuition(%pc),%a0
	bsr.w	printmsg
	bra.w	closeall
openp96:
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
	cmp.l	#HOSTVIDEO_VERSION,%d0
	bcc.s	newenough
	lea	msg_oldemu(%pc),%a0
	bra.w	failed
newenough:
	tst.l	%d6
	beq.s	shellargs

| From Workbench: the second argument is the file, if there is one
	movea.l	%d6,%a0
	cmp.l	#2,28(%a0)		| sm_NumArgs
	blt.w	askfirst
	movea.l	36(%a0),%a0		| sm_ArgList
	move.l	8(%a0),%d1		| wa_Lock of the file's drawer
	move.l	12(%a0),-(%sp)		| wa_Name
	movea.l	%a5,%a6
	jsr	-126(%a6)		| CurrentDir
	lea	vars(%pc),%a0
	move.l	%d0,OLDDIR(%a0)
	move.l	#1,DIRCHANGED(%a0)
	move.l	(%sp)+,NAME(%a0)
	bra.w	havename

| From the Shell: the first argument, in quotes if it has spaces
shellargs:
	tst.l	%d3
	beq.w	askfirst
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
	beq.w	askfirst
	moveq	#' ',%d1		| what ends the name
	cmp.b	#'"',%d0
	bne.s	nameset
	addq.l	#1,%a2
	subq.l	#1,%d3
	moveq	#'"',%d1
nameset:
	movea.l	%a2,%a1			| the name starts here
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
	cmpa.l	%a1,%a2
	beq.s	askfirst
	lea	vars(%pc),%a0
	move.l	%a1,NAME(%a0)
	cmp.b	#'?',(%a1)		| "HostPlay ?"
	bne.s	havename
	tst.b	1(%a1)
	bne.s	havename
	lea	msg_usage(%pc),%a0
	bsr.w	reportraw
	moveq	#0,%d7
	bra.w	closeall

askfirst:
	bsr.w	askfile
	tst.l	%d0
	bmi.s	noasl
	bne.s	havename
	moveq	#0,%d7			| cancelled
	bra.w	closeall
noasl:
	lea	msg_noasl(%pc),%a0
	bra.w	failed

havename:
	bsr.w	setdrawer
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
	bsr.w	openvideo
	tst.l	%d0
	bmi.w	failed			| %a0 tells why
	move.l	%d0,%d5

| The screen, its DrawInfo for the images, and the gadgets of the border
	lea	vars(%pc),%a2
	movea.l	INTUITIONBASE(%a2),%a6
	suba.l	%a0,%a0
	jsr	-510(%a6)		| LockPubScreen(NULL)
	move.l	%d0,SCREEN(%a2)
	bne.s	havescreen
	lea	msg_nowindow(%pc),%a0
	bra.w	failed
havescreen:
	movea.l	%d0,%a0
	jsr	-690(%a6)		| GetScreenDrawInfo
	move.l	%d0,DRAWINFO(%a2)
	bsr.w	makegadgets
	tst.l	%d0
	bne.s	havegadgets
	lea	msg_nowindow(%pc),%a0
	bra.w	failed

| The window, at the size of the video
havegadgets:
	bsr.w	fitsize
	lea	wintags(%pc),%a1
	move.l	%d1,4(%a1)		| WA_InnerWidth
	move.l	%d2,12(%a1)		| WA_InnerHeight
	bsr.w	maketitle
	lea	wintags(%pc),%a1
	lea	titlebuf(%pc),%a0
	move.l	%a0,20(%a1)		| WA_Title
	move.l	SCREEN(%a2),36(%a1)	| WA_PubScreen
	.ifdef	HOSTVIEW
	move.l	GADOPEN(%a2),44(%a1)	| WA_Gadgets
	.else
	move.l	GADSLIDER(%a2),44(%a1)	| WA_Gadgets
	.endif
| The zoom gadget's other size: the screen below its title bar
	lea	zoombox(%pc),%a0
	move.l	%a0,52(%a1)		| WA_Zoom
	movea.l	SCREEN(%a2),%a1
	clr.w	(%a0)			| left
	moveq	#0,%d0
	move.b	30(%a1),%d0		| sc_BarHeight
	addq.w	#1,%d0
	move.w	%d0,2(%a0)		| top
	move.w	12(%a1),4(%a0)		| width: sc_Width
	move.w	14(%a1),%d1		| sc_Height
	sub.w	%d0,%d1
	move.w	%d1,6(%a0)		| height
	movea.l	INTUITIONBASE(%a2),%a6
	suba.l	%a0,%a0
	lea	wintags(%pc),%a1
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
	.ifndef	HOSTVIEW
	bsr.w	newslider
	.endif

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
	.ifdef	HOSTVIEW
	cmp.l	#1,%d0			| the picture
	bne.s	msgloop
	bsr.w	draw
	.else
	cmp.l	#2,%d0			| the end of the video
	bne.s	notend
	lea	vars(%pc),%a2
	move.l	#1,ATEND(%a2)
	bsr.w	moveslider
	bra.s	msgloop
notend:
	tst.l	%d0
	beq.s	msgloop
	bsr.w	draw
	bsr.w	moveslider
	.endif

msgloop:
	movea.l	4,%a6
	movea.l	86(%a3),%a0		| wd_UserPort
	jsr	-372(%a6)		| GetMsg
	tst.l	%d0
	beq.w	mainloop
	movea.l	%d0,%a1
	move.l	20(%a1),%d2		| im_Class
	move.w	24(%a1),%d3		| im_Code
	movea.l	28(%a1),%a2		| im_IAddress
	move.w	32(%a1),%d4		| im_MouseX
	ext.l	%d4
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
	bra.s	msgloop
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
	bra.s	msgloop
notrefresh:
	.ifndef	HOSTVIEW
	cmp.l	#IDCMP_GADGETDOWN,%d2
	bne.s	notdown
	cmp.w	#GAD_SLIDER,38(%a2)	| gg_GadgetID
	bne.w	msgloop
	lea	vars(%pc),%a0
	move.l	#1,DRAGGING(%a0)
	bsr.w	sliderclick
	bra.w	msgloop
notdown:
	.endif
	cmp.l	#IDCMP_GADGETUP,%d2
	bne.s	notup
	move.w	38(%a2),%d0		| gg_GadgetID
	.ifndef	HOSTVIEW
	cmp.w	#GAD_SLIDER,%d0
	bne.s	notslider
	bsr.w	seekslider
	bra.w	msgloop
notslider:
	cmp.w	#GAD_PLAY,%d0
	bne.s	notplay
	bsr.w	playpause
	bra.w	msgloop
notplay:
	.endif
	cmp.w	#GAD_OPEN,%d0
	bne.w	msgloop
	bsr.w	newfile
	tst.l	%d0
	bmi.w	badformat
	bra.w	msgloop
notup:
	cmp.l	#IDCMP_VANILLAKEY,%d2
	bne.w	msgloop
	cmp.b	#27,%d3			| Esc
	beq.s	quit
	.ifndef	HOSTVIEW
	cmp.b	#' ',%d3
	bne.w	msgloop
	bsr.w	playpause
	.endif
	bra.w	msgloop

badformat:
	lea	msg_format(%pc),%a0
	bra.s	failed
nomem:
	lea	msg_nomem(%pc),%a0
failed:
	bsr.w	report
	moveq	#10,%d7
	bra.s	closeall
quit:
	moveq	#0,%d7

closeall:
	tst.l	%d5			| first the player, so that nothing more
	beq.s	1f			| goes into the buffer
	moveq	#2,%d0
	move.l	%d5,%d1
	bsr.w	callhost
1:	lea	vars(%pc),%a2
	movea.l	INTUITIONBASE(%a2),%a6
	move.l	%a3,%d0
	beq.s	2f
	movea.l	%a3,%a0
	jsr	-72(%a6)		| CloseWindow
| The gadgets, then the images, now that no window has them
2:	lea	GADOPEN+4(%a2),%a3
	moveq	#5,%d2
3:	move.l	-(%a3),%d0
	beq.s	4f
	movea.l	%d0,%a0
	jsr	-642(%a6)		| DisposeObject
4:	dbra	%d2,3b
	move.l	DRAWINFO(%a2),%d0
	beq.s	5f
	movea.l	%d0,%a1
	movea.l	SCREEN(%a2),%a0
	jsr	-696(%a6)		| FreeScreenDrawInfo
5:	move.l	SCREEN(%a2),%d0
	beq.s	6f
	movea.l	%d0,%a1
	suba.l	%a0,%a0
	jsr	-516(%a6)		| UnlockPubScreen
6:	bsr.w	freebuffer
	movea.l	4,%a6
	move.l	SIGBIT(%a2),%d0
	bmi.s	7f
	jsr	-336(%a6)		| FreeSignal
7:	tst.l	DIRCHANGED(%a2)
	beq.s	8f
	move.l	OLDDIR(%a2),%d1
	movea.l	%a5,%a6
	jsr	-126(%a6)		| CurrentDir
	movea.l	4,%a6
8:	move.l	ASLBASE(%a2),%d0
	beq.s	9f
	movea.l	%d0,%a1
	jsr	-414(%a6)		| CloseLibrary
9:	move.l	P96BASE(%a2),%d0
	beq.s	10f
	movea.l	%d0,%a1
	jsr	-414(%a6)		| CloseLibrary
10:	move.l	INTUITIONBASE(%a2),%d0
	beq.s	11f
	movea.l	%d0,%a1
	jsr	-414(%a6)		| CloseLibrary
11:	movea.l	%a5,%a1
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

| Hands the file in NAME to the emulator. %d0: a handle, or -1 with the
| reason in %a0.
openvideo:
	movem.l	%d2-%d4/%a6,-(%sp)
	lea	vars(%pc),%a0
	move.l	NAME(%a0),%d1
	moveq	#-2,%d2			| ACCESS_READ
	movea.l	%a5,%a6
	jsr	-84(%a6)		| Lock
	move.l	%d0,%d4
	bne.s	1f
	lea	msg_notfound(%pc),%a0
	moveq	#-1,%d0
	bra.s	openvideo_end
1:	lea	vars(%pc),%a0
	moveq	#1,%d0			| a player, for our task and signal
	move.l	TASK(%a0),%d1
	moveq	#0,%d2
	move.l	SIGBIT(%a0),%d3
	bset	%d3,%d2
	move.l	%d4,%d3			| the Lock of the file
	move.l	%d4,-(%sp)
	moveq	#0,%d4
	bsr.w	callhost
	move.l	(%sp)+,%d1
	move.l	%d0,-(%sp)
	movea.l	%a5,%a6
	jsr	-90(%a6)		| UnLock
	move.l	(%sp)+,%d0
	cmp.l	#-2,%d0			| not on a folder drive: a copy
	bne.s	openresult
	bsr.w	copyvideo
	tst.l	%d0
	bgt.s	openvideo_end
	cmpa.w	#0,%a0			| the copy told why
	bne.s	2f
openresult:
	tst.l	%d0
	bgt.s	openvideo_end
	lea	msg_busy(%pc),%a0
	beq.s	2f
	lea	msg_cannot(%pc),%a0
2:	moveq	#-1,%d0
openvideo_end:
	movem.l	(%sp)+,%d2-%d4/%a6
	rts

| A file the host cannot reach goes over to it in pieces, into a copy that
| the emulator plays. %d0 as from HostPlay's open: a handle, 0 busy, -1 not
| played - or -1 with the reason in %a0 already, with %a0 then not 0.
copyvideo:
	movem.l	%d2-%d4/%a2/%a6,-(%sp)
	lea	vars(%pc),%a2
	clr.l	CFILE(%a2)
	clr.l	CBUF(%a2)
	move.l	#-1,CPERCENT(%a2)
	moveq	#4,%d0			| start a copy, named for the file
	move.l	NAME(%a2),%d1
	bsr.w	callshow
	lea	msg_nocopy(%pc),%a0
	cmp.l	#1,%d0
	bne.w	copy_fail
	move.l	NAME(%a2),%d1
	move.l	#1005,%d2		| MODE_OLDFILE
	movea.l	%a5,%a6
	jsr	-30(%a6)		| Open
	move.l	%d0,CFILE(%a2)
	lea	msg_noread(%pc),%a0
	beq.w	copy_drop
| Its size, for the progress: the end, seen going back to the start
	move.l	%d0,%d1
	moveq	#0,%d2
	moveq	#1,%d3			| OFFSET_END
	jsr	-66(%a6)		| Seek
	move.l	CFILE(%a2),%d1
	moveq	#0,%d2
	moveq	#-1,%d3			| OFFSET_BEGINNING
	jsr	-66(%a6)		| Seek
	move.l	%d0,CTOTAL(%a2)
	bpl.s	1f
	clr.l	CTOTAL(%a2)
1:	clr.l	CDONE(%a2)
| A large buffer, or a small one with little memory
	movea.l	4,%a6
	move.l	#BIGBUF,CBUFSIZE(%a2)
	move.l	#BIGBUF,%d0
	moveq	#1,%d1			| MEMF_PUBLIC
	jsr	-198(%a6)		| AllocMem
	tst.l	%d0
	bne.s	2f
	move.l	#SMALLBUF,CBUFSIZE(%a2)
	move.l	#SMALLBUF,%d0
	moveq	#1,%d1
	jsr	-198(%a6)		| AllocMem
	lea	msg_nomem(%pc),%a0
	tst.l	%d0
	beq.w	copy_drop
2:	move.l	%d0,CBUF(%a2)
copy_loop:
	bsr.w	progress
	movea.l	4,%a6
	moveq	#0,%d0
	move.l	#0x1000,%d1		| SIGBREAKF_CTRL_C
	jsr	-306(%a6)		| SetSignal: takes Ctrl-C
	lea	msg_stopped(%pc),%a0
	btst	#12,%d0
	bne.s	copy_drop
	move.l	CFILE(%a2),%d1
	move.l	CBUF(%a2),%d2
	move.l	CBUFSIZE(%a2),%d3
	movea.l	%a5,%a6
	jsr	-42(%a6)		| Read
	lea	msg_noread(%pc),%a0
	tst.l	%d0
	bmi.s	copy_drop
	beq.s	copy_play
	add.l	%d0,CDONE(%a2)
	move.l	%d0,%d2			| bytes
	move.l	CBUF(%a2),%d1		| at
	moveq	#5,%d0			| add them to the copy
	bsr.w	callshow
	cmp.l	#1,%d0
	beq.s	copy_loop
	lea	msg_nocopy(%pc),%a0	| the emulator dropped it
	bra.s	copy_fail
copy_play:
	bsr.w	progress_end
	moveq	#11,%d0			| a player for the copy
	move.l	TASK(%a2),%d1
	moveq	#0,%d2
	move.l	SIGBIT(%a2),%d3
	bset	%d3,%d2
	moveq	#0,%d3
	moveq	#0,%d4
	bsr.w	callhost
	suba.l	%a0,%a0			| what the emulator said tells it
	bra.s	copy_close
copy_drop:
	move.l	%a0,-(%sp)
	moveq	#7,%d0			| drop the copy
	bsr.w	callshow
	movea.l	(%sp)+,%a0
copy_fail:
	bsr.w	progress_end
	moveq	#-1,%d0
copy_close:
	movem.l	%d0/%a0,-(%sp)
	movea.l	4,%a6
	move.l	CBUF(%a2),%d0
	beq.s	3f
	movea.l	%d0,%a1
	move.l	CBUFSIZE(%a2),%d0
	jsr	-210(%a6)		| FreeMem
3:	move.l	CFILE(%a2),%d1
	beq.s	4f
	movea.l	%a5,%a6
	jsr	-36(%a6)		| Close
4:	movem.l	(%sp)+,%d0/%a0
	movem.l	(%sp)+,%d2-%d4/%a2/%a6
	rts

| From the Shell: how much of the file went over, when it changed
progress:
	tst.l	%d6
	bne.s	progress_ret
	move.l	CDONE(%a2),%d0
	move.l	CTOTAL(%a2),%d1
	beq.s	progress_ret
4:	cmp.l	#0xFFFF,%d1		| small enough for mulu and divu
	bls.s	5f
	lsr.l	#1,%d0
	lsr.l	#1,%d1
	bra.s	4b
5:	mulu	#100,%d0
	divu	%d1,%d0
	and.l	#0xFFFF,%d0
	cmp.l	CPERCENT(%a2),%d0
	beq.s	progress_ret
	move.l	%d0,CPERCENT(%a2)
	lea	linebuf(%pc),%a0
	move.b	#13,(%a0)+		| CR: the line anew
	lea	title(%pc),%a1
5:	move.b	(%a1)+,(%a0)+
	bne.s	5b
	subq.l	#1,%a0
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
	move.l	%a0,-(%sp)
	cmp.l	#-1,CPERCENT(%a2)
	beq.s	9f
	move.l	#-1,CPERCENT(%a2)
	lea	newline(%pc),%a0
	bsr.w	print
9:	movea.l	(%sp)+,%a0
	rts

| The file requester: %d0 the name (in pathbuf, and in NAME), 0 when
| cancelled, -1 without asl.library
askfile:
	movem.l	%d2-%d4/%a2/%a6,-(%sp)
	lea	vars(%pc),%a2
	move.l	ASLBASE(%a2),%d0
	bne.s	1f
	movea.l	4,%a6
	lea	aslname(%pc),%a1
	moveq	#36,%d0
	jsr	-552(%a6)		| OpenLibrary
	move.l	%d0,ASLBASE(%a2)
	beq.w	askfile_noasl
1:	movea.l	%d0,%a6
	lea	asltags(%pc),%a0
	lea	title(%pc),%a1
	move.l	%a1,4(%a0)		| ASLFR_TitleText
	lea	pattern(%pc),%a1
	move.l	%a1,20(%a0)		| ASLFR_InitialPattern
	move.l	%a3,36(%a0)		| ASLFR_Window: ours, once it is open
	lea	drawerbuf(%pc),%a1
	move.l	%a1,44(%a0)		| ASLFR_InitialDrawer: the last file's
	moveq	#0,%d0			| ASL_FileRequest
	jsr	-48(%a6)		| AllocAslRequest
	tst.l	%d0
	beq.s	askfile_noasl
	move.l	%d0,%d4
	movea.l	%d0,%a0
	suba.l	%a1,%a1
	jsr	-60(%a6)		| AslRequest
	tst.l	%d0
	beq.s	askfile_cancel
	movea.l	%d4,%a0
	movea.l	4(%a0),%a1		| rf_File
	tst.b	(%a1)
	beq.s	askfile_cancel
| The path: the drawer, then the file added to it
	movea.l	8(%a0),%a1		| rf_Dir
	lea	pathbuf(%pc),%a0
	move.w	#PATHSIZE-2,%d0
2:	move.b	(%a1)+,(%a0)+
	dbeq	%d0,2b
	clr.b	-1(%a0)
	lea	pathbuf(%pc),%a0
	move.l	%a0,%d1
	movea.l	%d4,%a0
	move.l	4(%a0),%d2		| rf_File
	move.l	#PATHSIZE,%d3
	move.l	%a6,-(%sp)
	movea.l	%a5,%a6
	jsr	-882(%a6)		| AddPart
	movea.l	(%sp)+,%a6
	movea.l	%d4,%a0
	jsr	-54(%a6)		| FreeAslRequest
	lea	pathbuf(%pc),%a0
	move.l	%a0,NAME(%a2)
	bsr.s	setdrawer
	lea	pathbuf(%pc),%a0
	move.l	%a0,%d0
	bra.s	askfile_end
askfile_cancel:
	movea.l	%d4,%a0
	jsr	-54(%a6)		| FreeAslRequest
	moveq	#0,%d0
	bra.s	askfile_end
askfile_noasl:
	moveq	#-1,%d0
askfile_end:
	movem.l	(%sp)+,%d2-%d4/%a2/%a6
	rts

| The drawer of the file in NAME, for the file requester to start in
setdrawer:
	movem.l	%d2/%a6,-(%sp)
	lea	vars(%pc),%a0
	movea.l	NAME(%a0),%a1
	lea	drawerbuf(%pc),%a0
	move.w	#PATHSIZE-2,%d0
1:	move.b	(%a1)+,(%a0)+
	dbeq	%d0,1b
	clr.b	-1(%a0)
	lea	drawerbuf(%pc),%a0
	move.l	%a0,%d1
	movea.l	%a5,%a6
	jsr	-876(%a6)		| PathPart
	movea.l	%d0,%a0
	clr.b	(%a0)
	movem.l	(%sp)+,%d2/%a6
	rts

| Another file in the same window: the old video goes once the new one
| plays. %d0: 0, or -1 when the screen's format is not one the emulator
| makes.
newfile:
	movem.l	%d2-%d4/%a2/%a6,-(%sp)
	bsr.w	askfile
	tst.l	%d0
	bmi.w	newfile_noasl
	beq.w	newfile_end
	bsr.w	openvideo
	tst.l	%d0
	bpl.s	1f
	bsr.w	report
	moveq	#0,%d0
	bra.w	newfile_end
1:	move.l	%d0,-(%sp)
	moveq	#2,%d0			| the old one stops
	move.l	%d5,%d1
	bsr.w	callhost
	move.l	(%sp)+,%d5
	lea	vars(%pc),%a2
	clr.l	ATEND(%a2)
	bsr.w	maketitle
	movea.l	INTUITIONBASE(%a2),%a6
	movea.l	%a3,%a0
	lea	titlebuf(%pc),%a1
	movea.l	#-1,%a2			| the screen's title stays
	jsr	-276(%a6)		| SetWindowTitles
	.ifndef	HOSTVIEW
	bsr.w	newslider
	.endif
| The window at the new video's size, where it fits on the screen; the
| emulator gets the buffer anew now, and again once the size changed
	bsr.w	fitsize
	moveq	#0,%d0
	move.b	54(%a3),%d0		| wd_BorderLeft
	add.l	%d0,%d1
	move.b	56(%a3),%d0		| wd_BorderRight
	add.l	%d0,%d1
	move.b	55(%a3),%d0		| wd_BorderTop
	add.l	%d0,%d2
	move.b	57(%a3),%d0		| wd_BorderBottom
	add.l	%d0,%d2
	move.l	%d2,%d3			| height
	move.l	%d1,%d2			| width
	lea	vars(%pc),%a2
	movea.l	SCREEN(%a2),%a0
	moveq	#0,%d0
	move.w	4(%a3),%d0		| wd_LeftEdge
	moveq	#0,%d4
	move.w	12(%a0),%d4		| sc_Width
	sub.l	%d2,%d4
	bpl.s	2f
	moveq	#0,%d4
2:	cmp.l	%d4,%d0
	ble.s	3f
	move.l	%d4,%d0
3:	moveq	#0,%d1
	move.w	6(%a3),%d1		| wd_TopEdge
	moveq	#0,%d4
	move.w	14(%a0),%d4		| sc_Height
	sub.l	%d3,%d4
	bpl.s	4f
	moveq	#0,%d4
4:	cmp.l	%d4,%d1
	ble.s	5f
	move.l	%d4,%d1
5:	movea.l	INTUITIONBASE(%a2),%a6
	movea.l	%a3,%a0
	jsr	-486(%a6)		| ChangeWindowBox
	bsr.w	target
	tst.l	%d0
	bmi.s	newfile_end
	bsr.w	draw
	moveq	#0,%d0
	bra.s	newfile_end
newfile_noasl:
	lea	msg_noasl(%pc),%a0
	bsr.w	report
	moveq	#0,%d0
newfile_end:
	movem.l	(%sp)+,%d2-%d4/%a2/%a6
	rts

| The video's size, made to fit the screen in the same shape: %d1 width,
| %d2 height
fitsize:
	movem.l	%d3-%d4/%a2,-(%sp)
	moveq	#3,%d0			| the size of the video
	move.l	%d5,%d1
	bsr.w	callhost
	moveq	#0,%d2
	move.w	%d0,%d2			| height
	swap	%d0
	moveq	#0,%d1
	move.w	%d0,%d1			| width
	lea	vars(%pc),%a2
	movea.l	SCREEN(%a2),%a0
	moveq	#0,%d3
	move.w	12(%a0),%d3		| sc_Width
	moveq	#0,%d4
	move.w	14(%a0),%d4		| sc_Height
	sub.l	#32,%d3			| room for the borders
	sub.l	#64,%d4			| and the screen's title bar
	cmp.l	%d3,%d1
	bls.s	1f
	mulu	%d3,%d2
	divu	%d1,%d2
	and.l	#0xFFFF,%d2
	move.l	%d3,%d1
1:	cmp.l	%d4,%d2
	bls.s	2f
	mulu	%d4,%d1
	divu	%d2,%d1
	and.l	#0xFFFF,%d1
	move.l	%d4,%d2
2:	movem.l	(%sp)+,%d3-%d4/%a2
	rts

| The title: the file's name and what the video is, in titlebuf
maketitle:
	movem.l	%d2-%d4/%a2/%a6,-(%sp)
	lea	vars(%pc),%a2
	move.l	NAME(%a2),%d1
	movea.l	%a5,%a6
	jsr	-870(%a6)		| FilePart
	movea.l	%d0,%a1
	lea	titlebuf(%pc),%a0
	move.w	#TITLESIZE-64-1,%d0
1:	move.b	(%a1)+,(%a0)+
	dbeq	%d0,1b
	move.b	#' ',-1(%a0)
	move.l	%a0,%d2			| the emulator adds the rest
	lea	titlebuf+TITLESIZE(%pc),%a1
	move.l	%a1,%d3
	sub.l	%a0,%d3
	moveq	#10,%d0
	move.l	%d5,%d1
	moveq	#0,%d4
	bsr.w	callhost
	movem.l	(%sp)+,%d2-%d4/%a2/%a6
	rts

| The images and gadgets of the bottom border, linked in the order of the
| vars, on the screen in SCREEN with its DrawInfo. %d0: 0 when one did not
| come.
makegadgets:
	movem.l	%d2-%d4/%a2/%a6,-(%sp)
	lea	vars(%pc),%a2
	movea.l	INTUITIONBASE(%a2),%a6
	moveq	#1,%d2			| SYSISIZE_LOWRES
	movea.l	SCREEN(%a2),%a0
	btst	#1,20(%a0)		| sc_Flags & SCREENHIRES (0x200)
	beq.s	1f
	moveq	#0,%d2			| SYSISIZE_MEDRES
1:	lea	imagetags(%pc),%a0
	move.l	DRAWINFO(%a2),4(%a0)	| SYSIA_DrawInfo
	move.l	%d2,12(%a0)		| SYSIA_Size
	moveq	#2,%d0			| SIZEIMAGE
	bsr.w	newimage
	move.l	%d0,IMGSIZE(%a2)
	beq.w	makegadgets_end
	move.l	#IA_Width,%d0
	bsr.w	getattr
	move.l	%d0,SIZEW(%a2)
	move.l	#IA_Height,%d0
	movea.l	IMGSIZE(%a2),%a0
	bsr.w	getattr
	move.l	%d0,SIZEH(%a2)
	moveq	#0x0C,%d0		| RIGHTIMAGE: play
	bsr.w	newimage
	move.l	%d0,IMGPLAY(%a2)
	beq.w	makegadgets_end
	move.l	#IA_Width,%d0
	bsr.w	getattr
	move.l	%d0,PLAYW(%a2)
	moveq	#0x0B,%d0		| UPIMAGE: another file
	bsr.w	newimage
	move.l	%d0,IMGOPEN(%a2)
	beq.w	makegadgets_end
	move.l	#IA_Width,%d0
	bsr.w	getattr
	move.l	%d0,OPENW(%a2)

	.ifndef	HOSTVIEW
| The slider: from the left border up to the buttons
	lea	slidertags(%pc),%a0
	moveq	#0,%d0
	movea.l	SCREEN(%a2),%a1
	move.b	36(%a1),%d0		| sc_WBorLeft
	move.l	%d0,4(%a0)		| GA_Left
	moveq	#3,%d1
	sub.l	SIZEH(%a2),%d1
	move.l	%d1,12(%a0)		| GA_RelBottom
	add.l	PLAYW(%a2),%d0
	add.l	OPENW(%a2),%d0
	add.l	SIZEW(%a2),%d0
	addq.l	#1,%d0
	neg.l	%d0
	move.l	%d0,20(%a0)		| GA_RelWidth
	move.l	SIZEH(%a2),%d0
	subq.l	#4,%d0
	move.l	%d0,28(%a0)		| GA_Height
	lea	slidertags(%pc),%a1
	lea	propclass(%pc),%a0
	bsr.w	newobject
	move.l	%d0,GADSLIDER(%a2)
	beq.s	makegadgets_end

| The buttons, left of the size gadget
	lea	buttontags(%pc),%a0
	move.l	SIZEW(%a2),%d0
	add.l	OPENW(%a2),%d0
	add.l	PLAYW(%a2),%d0
	moveq	#1,%d1
	sub.l	%d0,%d1
	move.l	%d1,4(%a0)		| GA_RelRight
	moveq	#1,%d1
	sub.l	SIZEH(%a2),%d1
	move.l	%d1,12(%a0)		| GA_RelBottom
	move.l	IMGPLAY(%a2),20(%a0)	| GA_Image
	move.l	#GAD_PLAY,28(%a0)	| GA_ID
	move.l	GADSLIDER(%a2),36(%a0)	| GA_Previous
	movea.l	%a0,%a1
	lea	buttonclass(%pc),%a0
	bsr.w	newobject
	move.l	%d0,GADPLAY(%a2)
	beq.s	makegadgets_end
	lea	buttontags(%pc),%a0
	move.l	SIZEW(%a2),%d0
	add.l	OPENW(%a2),%d0
	moveq	#1,%d1
	sub.l	%d0,%d1
	move.l	%d1,4(%a0)		| GA_RelRight
	move.l	IMGOPEN(%a2),20(%a0)	| GA_Image
	move.l	#GAD_OPEN,28(%a0)	| GA_ID
	move.l	GADPLAY(%a2),36(%a0)	| GA_Previous
	.else
| The button, left of the size gadget
	lea	buttontags(%pc),%a0
	moveq	#1,%d1
	sub.l	SIZEH(%a2),%d1
	move.l	%d1,12(%a0)		| GA_RelBottom
	move.l	SIZEW(%a2),%d0
	add.l	OPENW(%a2),%d0
	moveq	#1,%d1
	sub.l	%d0,%d1
	move.l	%d1,4(%a0)		| GA_RelRight
	move.l	IMGOPEN(%a2),20(%a0)	| GA_Image
	move.l	#GAD_OPEN,28(%a0)	| GA_ID
	clr.l	36(%a0)			| GA_Previous: the first
	.endif
	movea.l	%a0,%a1
	lea	buttonclass(%pc),%a0
	bsr.w	newobject
	move.l	%d0,GADOPEN(%a2)
makegadgets_end:
	movem.l	(%sp)+,%d2-%d4/%a2/%a6
	rts

| A sysiclass image of the kind in %d0, with imagetags; in %d0 and %a0.
| Needs INTUITIONBASE in %a6.
newimage:
	lea	imagetags(%pc),%a1
	move.l	%d0,20(%a1)		| SYSIA_Which
	lea	sysiclass(%pc),%a0
| An object of the class named at %a0, with the tags at %a1
newobject:
	move.l	%a2,-(%sp)
	movea.l	%a1,%a2
	movea.l	%a0,%a1
	suba.l	%a0,%a0
	jsr	-636(%a6)		| NewObjectA
	movea.l	%d0,%a0
	movea.l	(%sp)+,%a2
	tst.l	%d0
	rts

| The attribute %d0 of the object %a0, with INTUITIONBASE in %a6
getattr:
	move.l	%a2,-(%sp)
	lea	vars(%pc),%a2
	lea	STORE(%a2),%a1
	clr.l	(%a1)
	jsr	-654(%a6)		| GetAttr
	move.l	STORE(%a2),%d0
	movea.l	(%sp)+,%a2
	rts

	.ifndef	HOSTVIEW
| The slider for a new video: its length, at the start
newslider:
	movem.l	%d2-%d4/%a2,-(%sp)
	lea	vars(%pc),%a2
	moveq	#8,%d0			| the length
	move.l	%d5,%d1
	bsr.w	callhost
	divu	#1000,%d0		| in seconds, up to 18 hours
	bvc.s	1f
	move.l	#0xFFFF,%d0
1:	and.l	#0xFFFF,%d0
	move.l	%d0,DURATION(%a2)
	clr.l	SHOWN(%a2)
	lea	proptags(%pc),%a0
	move.l	#PGA_Total,(%a0)
	addq.l	#1,%d0			| room for the knob at the end
	move.l	%d0,4(%a0)
	move.l	#PGA_Visible,8(%a0)
	move.l	#1,12(%a0)
	move.l	#PGA_Top,16(%a0)
	clr.l	20(%a0)
	bsr.s	setslider
	bsr.w	showtime
	movem.l	(%sp)+,%d2-%d4/%a2
	rts

| The slider to where the video is now, when that is another second and
| it is not being dragged
moveslider:
	movem.l	%d2-%d4/%a2,-(%sp)
	lea	vars(%pc),%a2
	tst.l	DRAGGING(%a2)
	bne.s	1f
	moveq	#7,%d0			| the time
	move.l	%d5,%d1
	bsr.w	callhost
	divu	#1000,%d0
	bvs.s	1f
	and.l	#0xFFFF,%d0
	cmp.l	SHOWN(%a2),%d0
	beq.s	1f
	move.l	%d0,SHOWN(%a2)
	lea	proptags(%pc),%a0
	move.l	#PGA_Top,(%a0)
	move.l	%d0,4(%a0)
	clr.l	8(%a0)			| TAG_DONE after the one
	bsr.s	setslider
	bsr.w	showtime
1:	movem.l	(%sp)+,%d2-%d4/%a2
	rts

| SetGadgetAttrsA() of the slider with proptags
setslider:
	movem.l	%a2-%a3/%a6,-(%sp)
	lea	vars(%pc),%a0
	movea.l	INTUITIONBASE(%a0),%a6
	movea.l	%a3,%a1			| the window
	movea.l	GADSLIDER(%a0),%a0
	suba.l	%a2,%a2
	lea	proptags(%pc),%a3
	jsr	-660(%a6)		| SetGadgetAttrsA
	movem.l	(%sp)+,%a2-%a3/%a6
	rts

| The slider let go: the video goes to where it is
seekslider:
	movem.l	%d2-%d4/%a2/%a6,-(%sp)
	lea	vars(%pc),%a2
	clr.l	DRAGGING(%a2)
	move.l	CLICKED(%a2),%d0
	bmi.s	1f
| a click beside the knob: there, with the knob put there too
	move.l	#-1,CLICKED(%a2)
	move.l	%d0,SHOWN(%a2)
	lea	proptags(%pc),%a0
	move.l	#PGA_Top,(%a0)
	move.l	%d0,4(%a0)
	clr.l	8(%a0)
	bsr.w	setslider
	move.l	SHOWN(%a2),%d0
	bra.s	2f
1:	movea.l	INTUITIONBASE(%a2),%a6
	movea.l	GADSLIDER(%a2),%a0
	move.l	#PGA_Top,%d0
	bsr.w	getattr
	move.l	%d0,SHOWN(%a2)
2:
	mulu	#1000,%d0
	move.l	%d0,%d2
	moveq	#9,%d0			| seek
	move.l	%d5,%d1
	moveq	#0,%d3
	moveq	#0,%d4
	bsr.w	callhost
	clr.l	ATEND(%a2)
	bsr.w	showtime
	movem.l	(%sp)+,%d2-%d4/%a2/%a6
	rts

| The slider pressed, at %d4 across the window, %a2 the gadget: beside the
| knob - which on its own would move a page, a second here - CLICKED gets
| the second of that place, for when the slider is let go.
sliderclick:
	movem.l	%d2/%a1-%a2,-(%sp)
	lea	vars(%pc),%a1
	move.l	#-1,CLICKED(%a1)
	tst.l	DURATION(%a1)
	beq.s	1f
	movea.l	34(%a2),%a0		| gg_SpecialInfo: the PropInfo
	btst	#0,(%a0)		| pi_Flags & KNOBHIT (0x100)
	bne.s	1f
	moveq	#0,%d0
	move.w	10(%a0),%d0		| pi_CWidth
	beq.s	1f
	move.w	4(%a2),%d1		| gg_LeftEdge
	add.w	18(%a0),%d1		| pi_LeftBorder
	ext.l	%d1
	move.l	%d4,%d2
	sub.l	%d1,%d2
	bpl.s	2f
	moveq	#0,%d2
2:	cmp.l	%d0,%d2
	bls.s	3f
	move.l	%d0,%d2
3:	mulu	DURATION+2(%a1),%d2
	divu	%d0,%d2
	and.l	#0xFFFF,%d2
	move.l	%d2,CLICKED(%a1)
1:	movem.l	(%sp)+,%d2/%a1-%a2
	rts

| The title with the time first: "1:23 / 45:00  name (size) (codec)". The
| two buffers take turns, so that Intuition never draws one being written.
showtime:
	movem.l	%d2-%d4/%a2/%a6,-(%sp)
	lea	vars(%pc),%a2
	lea	timetitle1(%pc),%a0
	not.l	TITLETURN(%a2)
	beq.s	1f
	lea	timetitle2(%pc),%a0
1:	move.l	%a0,-(%sp)
	moveq	#0,%d1			| hours, for an hour or more
	cmp.l	#3600,DURATION(%a2)
	bcc.s	2f
	cmp.l	#3600,SHOWN(%a2)
	bcs.s	3f
2:	moveq	#1,%d1
3:	move.l	SHOWN(%a2),%d0
	bsr.s	puttime
	tst.l	DURATION(%a2)
	beq.s	4f
	move.b	#' ',(%a0)+
	move.b	#'/',(%a0)+
	move.b	#' ',(%a0)+
	move.l	DURATION(%a2),%d0
	bsr.s	puttime
4:	move.b	#' ',(%a0)+
	move.b	#' ',(%a0)+
	lea	titlebuf(%pc),%a1
5:	move.b	(%a1)+,(%a0)+
	bne.s	5b
	movea.l	(%sp)+,%a1
	movea.l	INTUITIONBASE(%a2),%a6
	movea.l	%a3,%a0
	movea.l	#-1,%a2			| the screen's title stays
	jsr	-276(%a6)		| SetWindowTitles
	movem.l	(%sp)+,%d2-%d4/%a2/%a6
	rts

| Writes %d0 seconds at %a0 on: m:ss, or h:mm:ss with %d1 set
puttime:
	move.l	%d2,-(%sp)
	and.l	#0xFFFF,%d0
	tst.l	%d1
	beq.s	1f
	divu	#3600,%d0		| hours, and the seconds left
	move.l	%d0,%d2
	swap	%d2
	and.l	#0xFFFF,%d2
	bsr.s	putnum
	move.b	#':',(%a0)+
	move.l	%d2,%d0
	divu	#60,%d0
	move.l	%d0,%d2
	swap	%d2
	bsr.s	put2
	bra.s	2f
1:	divu	#60,%d0			| minutes, and the seconds left
	move.l	%d0,%d2
	swap	%d2
	bsr.s	putnum
2:	move.b	#':',(%a0)+
	move.w	%d2,%d0
	bsr.s	put2
	move.l	(%sp)+,%d2
	rts

| %d0.w below 100: as it is
putnum:
	cmp.w	#10,%d0
	bcc.s	put2
	add.b	#'0',%d0
	move.b	%d0,(%a0)+
	rts
| %d0.w below 100: two digits
put2:
	and.l	#0xFFFF,%d0
	divu	#10,%d0
	add.b	#'0',%d0
	move.b	%d0,(%a0)+
	swap	%d0
	add.b	#'0',%d0
	move.b	%d0,(%a0)+
	rts

| Pauses, plays on, or plays again from the start once the video ended
playpause:
	movem.l	%d2-%d4/%a2,-(%sp)
	lea	vars(%pc),%a2
	moveq	#0,%d2
	moveq	#0,%d3
	moveq	#0,%d4
	move.l	%d5,%d1
	tst.l	ATEND(%a2)
	beq.s	1f
	clr.l	ATEND(%a2)
	moveq	#9,%d0			| seek to the start
	bsr.w	callhost
	bra.s	2f
1:	moveq	#6,%d0			| pause, or play on
	bsr.w	callhost
2:	movem.l	(%sp)+,%d2-%d4/%a2
	rts
	.endif

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

| Calls uaelib function 200: %d0 what, %d1 and %d2 its arguments
callshow:
	move.l	%d2,-(%sp)
	move.l	%d1,-(%sp)
	move.l	%d0,-(%sp)
	move.l	#HOSTSHOW,-(%sp)
	jsr	(%a4)
	lea	16(%sp),%sp
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

| Z set if %a4 points at a trap followed by RTS
isuaelib:
	move.w	(%a4),%d0
	and.w	#0xF000,%d0
	cmp.w	#0xA000,%d0
	bne.s	isuaelib_end
	cmp.w	#0x4E75,2(%a4)
isuaelib_end:
	rts

| Tells the message at %a0: in the Shell after the tool's name, or in a
| requester from Workbench
report:
	tst.l	%d6
	bne.s	reportraw
| The message at %a0 to the output, after the tool's name
printmsg:
	move.l	%a0,-(%sp)
	lea	title(%pc),%a0
	bsr.s	print
	lea	colon(%pc),%a0
	bsr.s	print
	movea.l	(%sp)+,%a0
	bra.s	print
| The same without the name: the usage
reportraw:
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
	movea.l	%a3,%a0			| our window, if it is open
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
zoombox:	.word	0, 0, 0, 0	| struct IBox
renderinfo:	.long	0		| ri_Memory
		.word	0, 0		| ri_BytesPerRow, pad
		.long	0		| ri_RGBFormat
easy:		.long	0, 0, 0, 0, 0	| struct EasyStruct
asltags:	.long	0x80080001, 0	| ASLFR_TitleText
		.long	0x80080032, 1	| ASLFR_DoPatterns
		.long	0x8008000A, 0	| ASLFR_InitialPattern
		.long	0x8008003C, 1	| ASLFR_RejectIcons
		.long	0x80080002, 0	| ASLFR_Window
		.long	0x80080009, 0	| ASLFR_InitialDrawer
		.long	0		| TAG_DONE
wintags:	.long	0x80000076, 0	| WA_InnerWidth
		.long	0x80000077, 0	| WA_InnerHeight
		.long	0x8000006E, 0	| WA_Title
		.long	0x8000006A, IDCMP_NEWSIZE+IDCMP_REFRESHWINDOW+IDCMP_GADGETDOWN+IDCMP_GADGETUP+IDCMP_CLOSEWINDOW+IDCMP_VANILLAKEY
		.long	0x80000079, 0	| WA_PubScreen
		.long	0x8000006C, 0	| WA_Gadgets
		.long	0x8000007D, 0	| WA_Zoom
		.long	0x80000072, 160	| WA_MinWidth: room for the slider
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
imagetags:	.long	SYSIA_DrawInfo, 0
		.long	SYSIA_Size, 0
		.long	SYSIA_Which, 0
		.long	0
slidertags:	.long	GA_Left, 0
		.long	GA_RelBottom, 0
		.long	GA_RelWidth, 0
		.long	GA_Height, 0
		.long	GA_ID, GAD_SLIDER
		.long	GA_BottomBorder, 1
		.long	GA_RelVerify, 1
		.long	GA_Immediate, 1
		.long	PGA_Freedom, 2	| FREEHORIZ
		.long	PGA_NewLook, 1
		.long	PGA_Borderless, 1
		.long	PGA_Total, 1
		.long	PGA_Visible, 1
		.long	PGA_Top, 0
		.long	0
buttontags:	.long	GA_RelRight, 0
		.long	GA_RelBottom, 0
		.long	GA_Image, 0
		.long	GA_ID, 0
		.long	GA_Previous, 0
		.long	GA_BottomBorder, 1
		.long	GA_RelVerify, 1
		.long	0
proptags:	.long	PGA_Total, 0
		.long	PGA_Visible, 0
		.long	PGA_Top, 0
		.long	0

	.set	PATHSIZE, 512
pathbuf:	.space	PATHSIZE
drawerbuf:	.space	PATHSIZE
	.set	TITLESIZE, 256
titlebuf:	.space	TITLESIZE
timetitle1:	.space	TITLESIZE+32
timetitle2:	.space	TITLESIZE+32
	.set	BIGBUF, 262144
	.set	SMALLBUF, 32768
linebuf:	.space	48

dosname:	.asciz	"dos.library"
intuiname:	.asciz	"intuition.library"
aslname:	.asciz	"asl.library"
p96name:	.asciz	"Picasso96API.library"
sysiclass:	.asciz	"sysiclass"
propclass:	.asciz	"propgclass"
buttonclass:	.asciz	"buttongclass"
	.ifdef	HOSTVIEW
title:		.asciz	"HostView"
	.else
title:		.asciz	"HostPlay"
	.endif
colon:		.asciz	": "
okgadget:	.asciz	"Ok"
	.ifdef	HOSTVIEW
pattern:	.asciz	"#?.(jpg|jpeg|png|gif|webp|bmp|heic|heif|avif)"
msg_usage:	.ascii	"Usage: HostView [<file>]\n"
		.ascii	"Shows a picture in a window, decoded by uae4arm; without a\n"
		.ascii	"file it asks for one. In the bottom border: another file.\n"
		.ascii	"Esc ends it. A file the host cannot reach, such as one inside\n"
		.asciz	"a hardfile, goes over to it first; Ctrl-C stops that.\n"
msg_cannot:	.asciz	"uae4arm cannot show this file\n"
	.else
pattern:	.asciz	"#?.(mp4|m4v|mkv|webm|avi|mov|mpg|mpeg|ts|wmv|flv|3gp|ogv)"
msg_usage:	.ascii	"Usage: HostPlay [<file>]\n"
		.ascii	"Plays a video in a window, decoded by uae4arm; without a file\n"
		.ascii	"it asks for one. In the bottom border: where the video is,\n"
		.ascii	"play and pause, another file. Space pauses, Esc ends it.\n"
		.ascii	"A file the host cannot reach, such as one inside a hardfile,\n"
		.asciz	"goes over to it first; Ctrl-C stops that.\n"
msg_cannot:	.asciz	"uae4arm cannot play this file\n"
	.endif
msg_nointuition: .asciz	"this needs intuition.library 39\n"
msg_notfound:	.asciz	"no such file\n"
msg_copying:	.asciz	": processing "
newline:	.asciz	"\n"
msg_stopped:	.asciz	"stopped\n"
msg_noread:	.asciz	"cannot read the file\n"
msg_nocopy:	.asciz	"uae4arm could not take the file - it needs more room\n"
msg_noasl:	.asciz	"no file requester - asl.library 36 or later is needed\n"
msg_nop96:	.asciz	"this needs Picasso96API.library\n"
msg_notuae:	.asciz	"this runs in uae4arm only\n"
msg_oldemu:	.asciz	"this needs a newer uae4arm\n"
msg_nosignal:	.asciz	"no free signal\n"
msg_busy:	.asciz	"uae4arm has two open already\n"
msg_nowindow:	.asciz	"the window does not open\n"
msg_notrtg:	.asciz	"the Workbench screen has to be an RTG one\n"
msg_format:	.asciz	"the screen needs 16 or 32 bits\n"
msg_nomem:	.asciz	"not enough memory for the window\n"
		.even
