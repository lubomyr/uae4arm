| HostBrowser - shows a web page in a window of the Amiga, drawn by the host
|
| Usage: HostBrowser [<address>]
| Without an address - from the Shell, or started from its icon - it opens
| the home page. "aminet.net" means https://aminet.net.
|
| The page is the host's: on Android a WebView, on a display of its own that
| nobody sees, whose pictures come to the emulator. As in HostPlay, the tool
| hands the emulator a buffer for the inside of the window through uaelib
| function 202; the emulator puts each new picture of the page into it, in
| the screen's own pixel format, and signals the tool, which blits it with
| p96WritePixelArray(). When the window gets another size, so does the page.
|
| The left mouse button goes over as a touch: a click follows a link, a drag
| moves the page. The wheel scrolls it under the pointer. The keys go to the
| page - into a text field, or the cursor keys move it - but for these: the
| right button or Alt-cursor left go back to the page before, Alt-cursor
| right forward again, F5 loads the page again. The close gadget or Right
| Amiga-Q end it. The title of the window is the page's; while a file of the
| page downloads, it tells how far it is.
|
| The window opens on the Workbench screen, which has to be an RTG one
| (uaegfx) with 16 or 32 bits.
|
| Built with GNU as for m68k into a plain binary, made an Amiga executable by
| hunk.py - see build.sh. All code is PC relative, so the hunk needs no
| relocation: the pointers in the tag lists are filled in at run time.
|
| Registers kept: a3 the window, a4 uaelib, a5 DOSBase, d5 the emulator's
| handle of the page, d6 the WBStartup message (0 from the Shell), d7 the
| return code.

	.set	HOSTVIDEO, 202		| uaelib function
	.set	HOSTVIDEO_VERSION, 7	| web pages, with the keys

	.set	HV_OPENURL, 14
	.set	HV_POINTER, 15
	.set	HV_SCROLL, 16
	.set	HV_NAVIGATE, 17
	.set	HV_KEY, 18

	.set	P96BMA_BYTESPERPIXEL, 5
	.set	P96BMA_RGBFORMAT, 7
	.set	P96BMA_ISP96, 8

	.set	IDCMP_NEWSIZE, 0x2
	.set	IDCMP_REFRESHWINDOW, 0x4
	.set	IDCMP_MOUSEBUTTONS, 0x8
	.set	IDCMP_MOUSEMOVE, 0x10
	.set	IDCMP_CLOSEWINDOW, 0x200
	.set	IDCMP_RAWKEY, 0x400
	.set	IDCMP_VANILLAKEY, 0x200000

| The size the window opens at, if the screen has room
	.set	WINW, 800
	.set	WINH, 600

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
	.set	URL, 36
	.set	SCREEN, 40
	.set	PRESSED, 44		| the left button went down on the page
	.set	TITLETURN, 48		| which of the two title buffers is free
	.set	QUAL, 52		| im_Qualifier of the message
	.set	VARSIZE, 56

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
	lea	homepage(%pc),%a1
	move.l	%a1,URL(%a0)

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
	tst.l	%d6			| from Workbench: the home page
	bne.s	haveurl

| From the Shell: the first argument, in quotes if it has spaces
shellargs:
	tst.l	%d3
	beq.s	haveurl
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
	beq.s	haveurl
	moveq	#' ',%d1		| what ends the address
	cmp.b	#'"',%d0
	bne.s	nameset
	addq.l	#1,%a2
	subq.l	#1,%d3
	moveq	#'"',%d1
nameset:
	movea.l	%a2,%a1			| the address starts here
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
	beq.s	haveurl
	cmp.b	#'?',(%a1)		| "HostBrowser ?"
	bne.s	1f
	tst.b	1(%a1)
	bne.s	1f
	lea	msg_usage(%pc),%a0
	bsr.w	reportraw
	moveq	#0,%d7
	bra.w	closeall
1:	lea	vars(%pc),%a0
	move.l	%a1,URL(%a0)

haveurl:
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

| The window, as large as the screen has room for, up to WINW x WINH
havesignal:
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
	moveq	#0,%d1
	move.w	12(%a0),%d1		| sc_Width
	sub.l	#32,%d1			| room for the borders
	cmp.l	#WINW,%d1
	bls.s	1f
	move.l	#WINW,%d1
1:	moveq	#0,%d2
	move.w	14(%a0),%d2		| sc_Height
	sub.l	#64,%d2			| and the screen's title bar
	cmp.l	#WINH,%d2
	bls.s	2f
	move.l	#WINH,%d2
2:	lea	wintags(%pc),%a1
	move.l	%d1,4(%a1)		| WA_InnerWidth
	move.l	%d2,12(%a1)		| WA_InnerHeight
	lea	title(%pc),%a0
	move.l	%a0,20(%a1)		| WA_Title, till the page has one
	move.l	%d0,36(%a1)		| WA_PubScreen
| The zoom gadget's other size: the screen below its title bar
	lea	zoombox(%pc),%a0
	move.l	%a0,44(%a1)		| WA_Zoom
	movea.l	%d0,%a1
	clr.w	(%a0)			| left
	moveq	#0,%d0
	move.b	30(%a1),%d0		| sc_BarHeight
	addq.w	#1,%d0
	move.w	%d0,2(%a0)		| top
	move.w	12(%a1),4(%a0)		| width: sc_Width
	move.w	14(%a1),%d1		| sc_Height
	sub.w	%d0,%d1
	move.w	%d1,6(%a0)		| height
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
	bsr.w	screenformat
	tst.l	%d0
	bne.s	isrtg
	lea	msg_notrtg(%pc),%a0
	bra.w	failed
isrtg:
	bsr.w	innersize
	moveq	#HV_OPENURL,%d0		| the page, for our task and signal, at
	lea	vars(%pc),%a2		| the size of the inside of the window
	move.l	TASK(%a2),%d1
	moveq	#0,%d2
	move.l	SIGBIT(%a2),%d3
	bset	%d3,%d2
	move.l	URL(%a2),%d3
	move.l	WIDTH(%a2),%d4
	swap	%d4
	move.w	HEIGHT+2(%a2),%d4
	bsr.w	callhost
	tst.l	%d0
	bgt.s	haveopened
	lea	msg_busy(%pc),%a0
	beq.w	failed
	lea	msg_cannot(%pc),%a0
	bra.w	failed
haveopened:
	move.l	%d0,%d5
	bsr.w	target
	tst.l	%d0
	bmi.w	badformat
	beq.w	nomem
	bsr.w	draw

| Waits for a picture, the window, or Ctrl-C
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
	moveq	#5,%d0			| the new picture, if one came
	move.l	%d5,%d1
	bsr.w	callhost
	cmp.l	#1,%d0
	bne.s	msgloop
	bsr.w	draw
	bsr.w	newtitle

msgloop:
	movea.l	4,%a6
	movea.l	86(%a3),%a0		| wd_UserPort
	jsr	-372(%a6)		| GetMsg
	tst.l	%d0
	beq.w	mainloop
	movea.l	%d0,%a1
	move.l	20(%a1),%d2		| im_Class
	move.w	24(%a1),%d3		| im_Code
	move.l	32(%a1),%d4		| im_MouseX, im_MouseY
	lea	vars(%pc),%a0
	moveq	#0,%d0
	move.w	26(%a1),%d0
	move.l	%d0,QUAL(%a0)
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
	cmp.l	#IDCMP_MOUSEBUTTONS,%d2
	bne.s	notbutton
	bsr.w	innerpos
	cmp.w	#0x68,%d3		| SELECTDOWN
	bne.s	1f
	tst.l	%d1			| not on the page
	bne.w	msgloop
	lea	vars(%pc),%a0
	move.l	#1,PRESSED(%a0)
	moveq	#0,%d2			| pressed
	bra.s	sendpointer
1:	cmp.w	#0xE8,%d3		| SELECTUP
	bne.s	2f
	lea	vars(%pc),%a0
	tst.l	PRESSED(%a0)
	beq.w	msgloop
	clr.l	PRESSED(%a0)
	moveq	#1,%d2			| let go
	bra.s	sendpointer
2:	cmp.w	#0x69,%d3		| MENUDOWN: back
	bne.w	msgloop
	moveq	#0,%d2
	bra.w	navigate
notbutton:
	cmp.l	#IDCMP_MOUSEMOVE,%d2
	bne.s	notmove
	lea	vars(%pc),%a0
	tst.l	PRESSED(%a0)
	beq.w	msgloop
	bsr.w	innerpos
	moveq	#2,%d2			| moved while pressed
sendpointer:
	move.l	%d0,%d3
	moveq	#HV_POINTER,%d0
	move.l	%d5,%d1
	moveq	#0,%d4
	bsr.w	callhost
	bra.w	msgloop
notmove:
	cmp.l	#IDCMP_RAWKEY,%d2
	bne.s	notraw
	moveq	#1,%d2			| the wheel up
	cmp.w	#0x7A,%d3
	beq.s	scrollit
	moveq	#-1,%d2			| down
	cmp.w	#0x7B,%d3
	beq.s	scrollit
	moveq	#2,%d2			| F5: reload
	cmp.w	#0x54,%d3
	beq.s	navigate
	cmp.w	#0x60,%d3		| the qualifier keys, and keys let go,
	bcc.w	msgloop			| are left out
	lea	vars(%pc),%a0
	move.l	QUAL(%a0),%d0
	and.w	#0x30,%d0		| Alt with cursor left and right
	beq.s	1f
	moveq	#0,%d2			| back
	cmp.w	#0x4F,%d3
	beq.s	navigate
	moveq	#1,%d2			| forward
	cmp.w	#0x4E,%d3
	beq.s	navigate
1:	move.l	%d3,%d2			| to the page, as a key with no character
	and.l	#0xFF,%d2
	add.w	#0x100,%d2
	bra.s	sendkey
scrollit:
	bsr.w	innerpos
	move.l	%d0,%d3
	moveq	#HV_SCROLL,%d0
	move.l	%d5,%d1
	moveq	#0,%d4
	bsr.w	callhost
	bra.w	msgloop
navigate:
	moveq	#HV_NAVIGATE,%d0
	move.l	%d5,%d1
	moveq	#0,%d3
	moveq	#0,%d4
	bsr.w	callhost
	bra.w	msgloop
notraw:
	cmp.l	#IDCMP_VANILLAKEY,%d2
	bne.w	msgloop
	moveq	#0,%d2
	move.b	%d3,%d2			| the character
	lea	vars(%pc),%a0
	btst	#7,QUAL+3(%a0)		| Right Amiga
	beq.s	sendkey
	move.b	%d2,%d0
	and.b	#0xDF,%d0
	cmp.b	#'Q',%d0
	beq.s	quit
	bra.w	msgloop			| Amiga's own: not for the page
sendkey:
	moveq	#HV_KEY,%d0
	move.l	%d5,%d1
	lea	vars(%pc),%a0
	move.l	QUAL(%a0),%d3
	moveq	#0,%d4
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
	bra.s	closeall
quit:
	moveq	#0,%d7

closeall:
	tst.l	%d5			| first the page, so that nothing more
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
2:	move.l	SCREEN(%a2),%d0
	beq.s	3f
	movea.l	%d0,%a1
	suba.l	%a0,%a0
	jsr	-516(%a6)		| UnlockPubScreen
3:	bsr.w	freebuffer
	movea.l	4,%a6
	move.l	SIGBIT(%a2),%d0
	bmi.s	4f
	jsr	-336(%a6)		| FreeSignal
4:	move.l	P96BASE(%a2),%d0
	beq.s	5f
	movea.l	%d0,%a1
	jsr	-414(%a6)		| CloseLibrary
5:	move.l	INTUITIONBASE(%a2),%d0
	beq.s	6f
	movea.l	%d0,%a1
	jsr	-414(%a6)		| CloseLibrary
6:	movea.l	%a5,%a1
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

| %d4 the mouse in the window, x << 16 | y. %d0 the same in the inside of
| the window, kept within it; %d1 0 when it was inside, 1 when not.
innerpos:
	movem.l	%d2-%d3/%a2,-(%sp)
	lea	vars(%pc),%a2
	moveq	#0,%d1
	move.l	%d4,%d2
	swap	%d2
	ext.l	%d2			| x
	moveq	#0,%d0
	move.b	54(%a3),%d0		| wd_BorderLeft
	sub.l	%d0,%d2
	move.w	%d4,%d3
	ext.l	%d3			| y
	move.b	55(%a3),%d0		| wd_BorderTop
	sub.l	%d0,%d3
	tst.l	%d2
	bpl.s	1f
	moveq	#0,%d2
	moveq	#1,%d1
1:	cmp.l	WIDTH(%a2),%d2
	blt.s	2f
	move.l	WIDTH(%a2),%d2
	subq.l	#1,%d2
	moveq	#1,%d1
2:	tst.l	%d3
	bpl.s	3f
	moveq	#0,%d3
	moveq	#1,%d1
3:	cmp.l	HEIGHT(%a2),%d3
	blt.s	4f
	move.l	HEIGHT(%a2),%d3
	subq.l	#1,%d3
	moveq	#1,%d1
4:	swap	%d2
	clr.w	%d2
	move.w	%d3,%d2
	move.l	%d2,%d0
	movem.l	(%sp)+,%d2-%d3/%a2
	rts

| The page's title into the free one of two buffers; into the window when it
| changed, and the other buffer is the free one then
newtitle:
	movem.l	%d2-%d4/%a2/%a6,-(%sp)
	lea	vars(%pc),%a2
	lea	titlebuf1(%pc),%a0
	tst.l	TITLETURN(%a2)
	beq.s	1f
	lea	titlebuf2(%pc),%a0
1:	move.l	%a0,%d2
	moveq	#10,%d0			| HV_CMD_DESCRIBE
	move.l	%d5,%d1
	move.l	#TITLESIZE,%d3
	moveq	#0,%d4
	bsr.w	callhost
	cmp.l	#2,%d0
	bne.s	2f
	eori.l	#1,TITLETURN(%a2)
	movea.l	INTUITIONBASE(%a2),%a6
	movea.l	%a3,%a0
	movea.l	%d2,%a1
	movea.w	#-1,%a2			| the screen's title stays
	jsr	-276(%a6)		| SetWindowTitles
2:	movem.l	(%sp)+,%d2-%d4/%a2/%a6
	rts

| What the window's screen is, for the buffer: %d0 0 when it is no RTG one
screenformat:
	movem.l	%a2/%a6,-(%sp)
	lea	vars(%pc),%a2
	movea.l	P96BASE(%a2),%a6
	movea.l	50(%a3),%a0		| wd_RPort
	movea.l	4(%a0),%a0		| rp_BitMap
	moveq	#P96BMA_ISP96,%d0
	jsr	-42(%a6)		| p96GetBitMapAttr
	tst.l	%d0
	beq.s	1f
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
	moveq	#1,%d0
1:	movem.l	(%sp)+,%a2/%a6
	rts

| The size of the inside of the window into WIDTH and HEIGHT
innersize:
	movem.l	%d2-%d3/%a2,-(%sp)
	lea	vars(%pc),%a2
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
	movem.l	(%sp)+,%d2-%d3/%a2
	rts

| Makes the buffer for the inside of the window anew and hands it to the
| emulator, which puts the picture of now into it. %d0: 1 done, 0 no memory,
| -1 the screen's format is not one the emulator makes.
target:
	movem.l	%d2-%d4/%a2/%a6,-(%sp)
	lea	vars(%pc),%a2
	bsr.w	freebuffer
	bsr.s	innersize
	move.l	WIDTH(%a2),%d2
	move.l	HEIGHT(%a2),%d3
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
wintags:	.long	0x80000076, 0	| WA_InnerWidth
		.long	0x80000077, 0	| WA_InnerHeight
		.long	0x8000006E, 0	| WA_Title
		.long	0x8000006A, IDCMP_NEWSIZE+IDCMP_REFRESHWINDOW+IDCMP_MOUSEBUTTONS+IDCMP_MOUSEMOVE+IDCMP_CLOSEWINDOW+IDCMP_VANILLAKEY+IDCMP_RAWKEY
		.long	0x80000079, 0	| WA_PubScreen
		.long	0x8000007D, 0	| WA_Zoom
		.long	0x80000072, 160	| WA_MinWidth
		.long	0x80000073, 64	| WA_MinHeight
		.long	0x80000074, -1	| WA_MaxWidth: the screen's
		.long	0x80000075, -1	| WA_MaxHeight
		.long	0x80000081, 1	| WA_SizeGadget
		.long	0x80000082, 1	| WA_DragBar
		.long	0x80000083, 1	| WA_DepthGadget
		.long	0x80000084, 1	| WA_CloseGadget
		.long	0x80000086, 1	| WA_ReportMouse: for a drag
		.long	0x80000089, 1	| WA_Activate
		.long	0x8000008A, 1	| WA_RMBTrap: the right button goes back
		.long	0x8000008C, 1	| WA_SimpleRefresh
		.long	0		| TAG_DONE

	.set	TITLESIZE, 256
titlebuf1:	.space	TITLESIZE
titlebuf2:	.space	TITLESIZE

dosname:	.asciz	"dos.library"
intuiname:	.asciz	"intuition.library"
p96name:	.asciz	"Picasso96API.library"
title:		.asciz	"HostBrowser"
colon:		.asciz	": "
okgadget:	.asciz	"Ok"
homepage:	.asciz	"https://www.google.com/"
msg_usage:	.ascii	"Usage: HostBrowser [<address>]\n"
		.ascii	"Shows a web page in a window, drawn by uae4arm; without an\n"
		.ascii	"address the home page. A click follows a link, a drag moves\n"
		.ascii	"the page, the wheel scrolls it; the keys go to the page. The\n"
		.ascii	"right button or Alt-cursor left go back, Alt-cursor right\n"
		.ascii	"forward, F5 loads the page again, Right Amiga-Q ends it.\n"
		.asciz	"Files of the page go to the folder of Browser downloads.\n"
msg_cannot:	.asciz	"uae4arm cannot show web pages here - it needs Android 8 or later\n"
msg_nointuition: .asciz	"this needs intuition.library 39\n"
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
