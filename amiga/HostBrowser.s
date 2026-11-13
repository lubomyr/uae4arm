| HostBrowser - shows a web page in a window of the Amiga, drawn by the host
|
| Usage: HostBrowser [HIDEADDRESS] [<address>]
| Without an address - from the Shell, or started from its icon - it opens
| the home page. "aminet.net" means https://aminet.net.
|
| Above the page an address field shows where the page is, with buttons
| back and forward before it; another address
| typed into it and Return go there, and words that are no address search
| for them. Ctrl-L or F6 put the cursor in it. HIDEADDRESS leaves it out.
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
| Amiga-Q end it. The title of the window is the page's. Files of the page
| download as in AndroidBrowser, with Android's toasts telling it.
|
| Scroll bars in the right and bottom borders show where the page is: drag
| one, click beside its knob for a page, or hold an arrow. Outside a text
| field the cursor keys scroll as well, with Shift by a page.
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
	.set	HOSTVIDEO_VERSION, 9	| web pages, with keys, scroll bars, address

	.set	HV_OPENURL, 14
	.set	HV_POINTER, 15
	.set	HV_SCROLL, 16
	.set	HV_NAVIGATE, 17
	.set	HV_KEY, 18
	.set	HV_SCROLLINFO, 19
	.set	HV_SCROLLTO, 20
	.set	HV_SCROLLACROSS, 21
	.set	HV_ADDRESS, 22
	.set	HV_LOAD, 23

	.set	P96BMA_BYTESPERPIXEL, 5
	.set	P96BMA_RGBFORMAT, 7
	.set	P96BMA_ISP96, 8

	.set	IDCMP_NEWSIZE, 0x2
	.set	IDCMP_REFRESHWINDOW, 0x4
	.set	IDCMP_MOUSEBUTTONS, 0x8
	.set	IDCMP_MOUSEMOVE, 0x10
	.set	IDCMP_GADGETDOWN, 0x20
	.set	IDCMP_GADGETUP, 0x40
	.set	IDCMP_CLOSEWINDOW, 0x200
	.set	IDCMP_RAWKEY, 0x400
	.set	IDCMP_VANILLAKEY, 0x200000
	.set	IDCMP_INTUITICKS, 0x400000

	.set	GA_Left, 0x80030001
	.set	GA_RelRight, 0x80030002
	.set	GA_Top, 0x80030003
	.set	GA_RelBottom, 0x80030004
	.set	GA_Width, 0x80030005
	.set	GA_RelWidth, 0x80030006
	.set	GA_Height, 0x80030007
	.set	GA_RelHeight, 0x80030008
	.set	GA_Image, 0x8003000A
	.set	GA_ID, 0x80030010
	.set	GA_Immediate, 0x80030015
	.set	GA_RelVerify, 0x80030016
	.set	GA_FollowMouse, 0x80030017
	.set	GA_RightBorder, 0x80030018
	.set	GA_BottomBorder, 0x8003001B
	.set	GA_Previous, 0x8003001F
	.set	PGA_Freedom, 0x80031001
	.set	PGA_Borderless, 0x80031002
	.set	PGA_Total, 0x80031007
	.set	PGA_Visible, 0x80031008
	.set	PGA_Top, 0x80031009
	.set	PGA_NewLook, 0x8003100A
	.set	IA_Left, 0x80020001
	.set	IA_Top, 0x80020002
	.set	IA_Width, 0x80020003
	.set	IA_Height, 0x80020004
	.set	SYSIA_Size, 0x8002000B
	.set	SYSIA_Which, 0x8002000D
	.set	SYSIA_DrawInfo, 0x80020018
	.set	IA_EdgesOnly, 0x80020015
	.set	IA_FrameType, 0x8002001B
	.set	STRINGA_MaxChars, 0x80032001
	.set	STRINGA_TextVal, 0x80032012
	.set	ADDRSIZE, 1024		| the longest address

| The gadgets of the borders: the scroll bars, then their arrows
	.set	GAD_V, 1
	.set	GAD_H, 2
	.set	GAD_UP, 3
	.set	GAD_DOWN, 4
	.set	GAD_LEFT, 5
	.set	GAD_RIGHT, 6
	.set	GAD_ADDR, 7		| the address field
	.set	GAD_BACK, 8		| and the buttons before it
	.set	GAD_FWD, 9

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
	.set	LASTV, 56		| the tops of the scroll bars, by GAD_V, GAD_H
	.set	LASTH, 60
	.set	DRAWINFO, 64
	.set	IMGBACK, 68		| the images - the buttons back and forward,
	.set	IMGFWD, 72		| the address field's frame, the size
	.set	IMGFRAME, 76		| gadget, for its size - then the gadgets,
	.set	IMGSIZE, 80		| by their GAD_
	.set	IMGUP, 84
	.set	IMGDOWN, 88
	.set	IMGLEFT, 92
	.set	IMGRIGHT, 96
	.set	GADV, 100
	.set	GADH, 104
	.set	GADUP, 108
	.set	GADDOWN, 112
	.set	GADLEFT, 116
	.set	GADRIGHT, 120
	.set	GADADDR, 124
	.set	GADBACK, 128
	.set	GADFWD, 132
	.set	SIZEW, 136
	.set	SIZEH, 140
	.set	ARROWW, 144		| of the arrows up and down
	.set	ARROWH, 148
	.set	LEFTW, 152		| of those left and right
	.set	DRAGGING, 156		| the scroll bar being dragged, or 0
	.set	HELD, 160		| the arrow held, or 0, and for how long
	.set	HELDTICKS, 164
	.set	STORE, 168		| for GetAttr()
	.set	IADDR, 172		| im_IAddress of the message
	.set	BARTOP, 176		| the height of the window's title bar
	.set	NOADDR, 180		| HIDEADDRESS
	.set	PAGETOP, 184		| the page below the address field
	.set	FONTH, 188		| the height of the screen's font
	.set	ADDRDIRTY, 192		| the address changed while it was typed into
	.set	BACKW, 196		| the width of a button back or forward
	.set	FRAMEX, 200		| the frame of the field, from the left border
	.set	BACKH, 204		| the height of a button
	.set	GAP, 208		| around the buttons, as above and below them
	.set	GRAYCLASS, 212		| the class of their images
	.set	VARSIZE, 216

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
	bne.w	haveurl

| From the Shell: HIDEADDRESS, and the address, in quotes if it has spaces
shellargs:
	tst.l	%d3
	beq.w	haveurl
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
	movea.l	%a1,%a0
	move.l	%a1,-(%sp)
	lea	kw_hideaddress(%pc),%a1
	bsr.w	strieq
	movea.l	(%sp)+,%a1
	bne.s	2f
	lea	vars(%pc),%a0
	move.l	#1,NOADDR(%a0)
	bra.s	nextarg
2:	cmp.b	#'?',(%a1)		| "HostBrowser ?"
	bne.s	1f
	tst.b	1(%a1)
	bne.s	1f
	lea	msg_usage(%pc),%a0
	bsr.w	reportraw
	moveq	#0,%d7
	bra.w	closeall
1:	lea	vars(%pc),%a0
	move.l	%a1,URL(%a0)
nextarg:
	tst.l	%d3
	beq.s	haveurl
	addq.l	#1,%a2			| past the end of the argument
	subq.l	#1,%d3
	bra.w	shellargs

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
	jsr	-690(%a6)		| GetScreenDrawInfo
	move.l	%d0,DRAWINFO(%a2)
	bsr.w	makegadgets
	tst.l	%d0
	bne.s	1f
	lea	msg_nowindow(%pc),%a0
	bra.w	failed
1:	move.l	SCREEN(%a2),%d0
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
	move.l	GADV(%a2),52(%a1)	| WA_Gadgets
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
	bsr.w	drawaddr
	bsr.w	updatebars
	bsr.w	newaddress

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
	bsr.w	updatebars
	bsr.w	newaddress

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
	move.l	28(%a1),IADDR(%a0)
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
	bsr.w	drawaddr
	bsr.w	updatebars
	bra.w	msgloop
notsize:
	cmp.l	#IDCMP_REFRESHWINDOW,%d2
	bne.s	notrefresh
	lea	vars(%pc),%a2
	movea.l	INTUITIONBASE(%a2),%a6
	movea.l	%a3,%a0
	jsr	-354(%a6)		| BeginRefresh
	bsr.w	draw
	bsr.w	drawaddr
	lea	vars(%pc),%a2
	movea.l	INTUITIONBASE(%a2),%a6
	movea.l	%a3,%a0
	moveq	#1,%d0
	jsr	-366(%a6)		| EndRefresh
	bra.w	msgloop
notrefresh:
| A scroll bar pressed is dragged till let go; an arrow scrolls, and again
| while held
	cmp.l	#IDCMP_GADGETDOWN,%d2
	bne.s	notdown
	lea	vars(%pc),%a2
	movea.l	IADDR(%a2),%a0
	moveq	#0,%d0
	move.w	38(%a0),%d0		| gg_GadgetID
	cmp.w	#GAD_ADDR,%d0		| the field and the buttons by it: on
	bcc.w	msgloop			| GADGETUP
	cmp.w	#GAD_H,%d0
	bls.s	1f
	move.l	%d0,HELD(%a2)
	clr.l	HELDTICKS(%a2)
	bsr.w	arrowstep
	bra.w	msgloop
1:	move.l	%d0,DRAGGING(%a2)
	bra.w	msgloop
notdown:
	cmp.l	#IDCMP_GADGETUP,%d2
	bne.s	notup
	lea	vars(%pc),%a2
	movea.l	IADDR(%a2),%a0
	moveq	#0,%d0
	move.w	38(%a0),%d0		| gg_GadgetID
	cmp.w	#GAD_ADDR,%d0
	bne.s	2f
	bsr.w	addressgo		| Return in the address field
	bra.w	msgloop
2:	moveq	#0,%d2			| back
	cmp.w	#GAD_BACK,%d0
	beq.w	navigate
	moveq	#1,%d2			| forward
	cmp.w	#GAD_FWD,%d0
	beq.w	navigate
	cmp.w	#GAD_H,%d0
	bls.s	1f
	clr.l	HELD(%a2)
	bra.w	msgloop
1:	bsr.w	propmove		| a click beside the knob ends here too
	lea	vars(%pc),%a2
	clr.l	DRAGGING(%a2)
	bra.w	msgloop
notup:
	cmp.l	#IDCMP_INTUITICKS,%d2
	bne.s	notticks
	lea	vars(%pc),%a2
	move.l	HELD(%a2),%d0
	beq.w	msgloop
	move.l	%d0,%d1
	lsl.l	#2,%d1
	movea.l	GADV-4(%a2,%d1.l),%a0
	btst	#7,13(%a0)		| gg_Flags & GFLG_SELECTED: still held
	bne.s	1f
	clr.l	HELD(%a2)
	bra.w	msgloop
1:	addq.l	#1,HELDTICKS(%a2)
	cmp.l	#2,HELDTICKS(%a2)	| a moment before it goes on
	bcs.w	msgloop
	bsr.w	arrowstep
	bra.w	msgloop
notticks:
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
	move.l	DRAGGING(%a0),%d0
	beq.s	1f
	bsr.w	propmove
	bra.w	msgloop
1:	tst.l	PRESSED(%a0)
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
	cmp.w	#0x55,%d3		| F6: into the address field
	bne.s	2f
	bsr.w	toaddress
	bra.w	msgloop
2:
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
	cmp.b	#12,%d2			| Ctrl-L: into the address field
	bne.s	1f
	tst.l	GADADDR(%a0)
	beq.s	1f
	bsr.w	toaddress
	bra.w	msgloop
1:	btst	#7,QUAL+3(%a0)		| Right Amiga
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
| The gadgets, then the images, now that no window has them
2:	lea	GADFWD+4(%a2),%a3
	moveq	#16,%d2
7:	move.l	-(%a3),%d0
	beq.s	8f
	movea.l	%d0,%a0
	jsr	-642(%a6)		| DisposeObject
8:	dbra	%d2,7b
	move.l	GRAYCLASS(%a2),%d0	| no image of it is left
	beq.s	10f
	movea.l	%d0,%a0
	jsr	-714(%a6)		| FreeClass
10:	move.l	DRAWINFO(%a2),%d0
	beq.s	9f
	movea.l	%d0,%a1
	movea.l	SCREEN(%a2),%a0
	jsr	-696(%a6)		| FreeScreenDrawInfo
9:	move.l	SCREEN(%a2),%d0
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
	sub.l	PAGETOP(%a2),%d3
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

| The images and gadgets of the borders, linked in the order of their GAD_,
| on the screen in SCREEN with its DrawInfo. %d0: 0 when one did not come.
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
	moveq	#0x0B,%d0		| UPIMAGE
	bsr.w	newimage
	move.l	%d0,IMGUP(%a2)
	beq.w	makegadgets_end
	move.l	#IA_Width,%d0
	bsr.w	getattr
	move.l	%d0,ARROWW(%a2)
	move.l	#IA_Height,%d0
	movea.l	IMGUP(%a2),%a0
	bsr.w	getattr
	move.l	%d0,ARROWH(%a2)
	moveq	#0x0D,%d0		| DOWNIMAGE
	bsr.w	newimage
	move.l	%d0,IMGDOWN(%a2)
	beq.w	makegadgets_end
	moveq	#0x0A,%d0		| LEFTIMAGE
	bsr.w	newimage
	move.l	%d0,IMGLEFT(%a2)
	beq.w	makegadgets_end
	move.l	#IA_Width,%d0
	bsr.w	getattr
	move.l	%d0,LEFTW(%a2)
	moveq	#0x0C,%d0		| RIGHTIMAGE
	bsr.w	newimage
	move.l	%d0,IMGRIGHT(%a2)
	beq.w	makegadgets_end

| The height of the title bar, below which the bar up and down starts
	movea.l	SCREEN(%a2),%a0
	moveq	#0,%d0
	move.b	35(%a0),%d0		| sc_WBorTop
	movea.l	40(%a0),%a1		| sc_Font
	moveq	#0,%d1
	move.w	4(%a1),%d1		| ta_YSize
	move.l	%d1,FONTH(%a2)
	add.l	%d1,%d0
	addq.l	#1,%d0
	move.l	%d0,BARTOP(%a2)
| The address field's row above the page: the field in a frame
	tst.l	NOADDR(%a2)
	bne.s	1f
	move.l	FONTH(%a2),%d0
	addq.l	#6,%d0
	move.l	%d0,PAGETOP(%a2)
1:

| Up and down: in the right border, above the arrows
	lea	vproptags(%pc),%a0
	moveq	#5,%d0
	sub.l	SIZEW(%a2),%d0
	move.l	%d0,4(%a0)		| GA_RelRight
	move.l	BARTOP(%a2),%d0
	addq.l	#1,%d0
	move.l	%d0,12(%a0)		| GA_Top
	move.l	SIZEW(%a2),%d0
	subq.l	#8,%d0
	move.l	%d0,20(%a0)		| GA_Width
	move.l	BARTOP(%a2),%d0
	add.l	SIZEH(%a2),%d0
	add.l	ARROWH(%a2),%d0
	add.l	ARROWH(%a2),%d0
	addq.l	#2,%d0
	neg.l	%d0
	move.l	%d0,28(%a0)		| GA_RelHeight
	movea.l	%a0,%a1
	lea	propclass(%pc),%a0
	bsr.w	newobject
	move.l	%d0,GADV(%a2)
	beq.w	makegadgets_end

| Across: in the bottom border, left of the arrows
	lea	hproptags(%pc),%a0
	moveq	#0,%d0
	movea.l	SCREEN(%a2),%a1
	move.b	36(%a1),%d0		| sc_WBorLeft
	move.l	%d0,4(%a0)		| GA_Left
	moveq	#3,%d1
	sub.l	SIZEH(%a2),%d1
	move.l	%d1,12(%a0)		| GA_RelBottom
	add.l	LEFTW(%a2),%d0
	add.l	LEFTW(%a2),%d0
	add.l	SIZEW(%a2),%d0
	addq.l	#1,%d0
	neg.l	%d0
	move.l	%d0,20(%a0)		| GA_RelWidth
	move.l	SIZEH(%a2),%d0
	subq.l	#4,%d0
	move.l	%d0,28(%a0)		| GA_Height
	move.l	GADV(%a2),124(%a0)	| GA_Previous
	movea.l	%a0,%a1
	lea	propclass(%pc),%a0
	bsr.w	newobject
	move.l	%d0,GADH(%a2)
	beq.w	makegadgets_end

| The arrows, up and down above the size gadget, left and right beside it
	move.l	#GA_RightBorder,%d3
	moveq	#1,%d0
	sub.l	ARROWW(%a2),%d0
	moveq	#1,%d1
	sub.l	SIZEH(%a2),%d1
	sub.l	ARROWH(%a2),%d1
	sub.l	ARROWH(%a2),%d1
	moveq	#GAD_UP,%d2
	move.l	GADH(%a2),%d4
	movea.l	IMGUP(%a2),%a0
	bsr.w	newarrow
	move.l	%d0,GADUP(%a2)
	beq.w	makegadgets_end
	moveq	#1,%d0
	sub.l	ARROWW(%a2),%d0
	moveq	#1,%d1
	sub.l	SIZEH(%a2),%d1
	sub.l	ARROWH(%a2),%d1
	moveq	#GAD_DOWN,%d2
	move.l	GADUP(%a2),%d4
	movea.l	IMGDOWN(%a2),%a0
	bsr.w	newarrow
	move.l	%d0,GADDOWN(%a2)
	beq.w	makegadgets_end
	move.l	#GA_BottomBorder,%d3
	moveq	#1,%d0
	sub.l	SIZEW(%a2),%d0
	sub.l	LEFTW(%a2),%d0
	sub.l	LEFTW(%a2),%d0
	moveq	#1,%d1
	sub.l	SIZEH(%a2),%d1
	moveq	#GAD_LEFT,%d2
	move.l	GADDOWN(%a2),%d4
	movea.l	IMGLEFT(%a2),%a0
	bsr.w	newarrow
	move.l	%d0,GADLEFT(%a2)
	beq.w	makegadgets_end
	moveq	#1,%d0
	sub.l	SIZEW(%a2),%d0
	sub.l	LEFTW(%a2),%d0
	moveq	#1,%d1
	sub.l	SIZEH(%a2),%d1
	moveq	#GAD_RIGHT,%d2
	move.l	GADLEFT(%a2),%d4
	movea.l	IMGRIGHT(%a2),%a0
	bsr.w	newarrow
	move.l	%d0,GADRIGHT(%a2)
	beq.w	makegadgets_end
	tst.l	NOADDR(%a2)
	bne.w	makegadgets_end

| Back and forward: arrows as those of the scroll bars, at the left of the
| row, in the middle of the field's height; as far apart, and from the
| border and the field, as they are from the frame's top and bottom
| They are grey, as in a window that is not the active one: inside the
| window they would be in the colours of the active window's border
	move.l	%a2,-(%sp)
	suba.l	%a0,%a0
	lea	sysiclass(%pc),%a1
	suba.l	%a2,%a2
	moveq	#0,%d0
	moveq	#0,%d1
	jsr	-678(%a6)		| MakeClass
	movea.l	(%sp)+,%a2
	move.l	%d0,GRAYCLASS(%a2)
	beq.w	makegadgets_end
	movea.l	%d0,%a0
	lea	graydispatch(%pc),%a1
	move.l	%a1,8(%a0)		| cl_Dispatcher.h_Entry
	lea	imagetags(%pc),%a1
	move.l	#0x0A,20(%a1)		| LEFTIMAGE
	movea.l	GRAYCLASS(%a2),%a0
	bsr.w	newprivate
	move.l	%d0,IMGBACK(%a2)
	beq.w	makegadgets_end
	move.l	#IA_Width,%d0		| sysiclass keeps the size of its own
	bsr.w	getattr
	move.l	%d0,BACKW(%a2)
	move.l	#IA_Height,%d0
	movea.l	IMGBACK(%a2),%a0
	bsr.w	getattr
	move.l	%d0,BACKH(%a2)
	move.l	FONTH(%a2),%d1
	addq.l	#4,%d1			| the frame's height
	sub.l	%d0,%d1
	asr.l	#1,%d1
	bgt.s	1f
	moveq	#1,%d1
1:	move.l	%d1,GAP(%a2)
	move.l	BACKW(%a2),%d0
	add.l	%d1,%d0
	add.l	%d0,%d0
	add.l	%d1,%d0
	move.l	%d0,FRAMEX(%a2)		| gap, button, gap, button, gap
	lea	imagetags(%pc),%a1
	move.l	#0x0C,20(%a1)		| RIGHTIMAGE
	movea.l	GRAYCLASS(%a2),%a0
	bsr.w	newprivate
	move.l	%d0,IMGFWD(%a2)
	beq.w	makegadgets_end

| The address field: inside the window, across it in a ridge
	lea	frametags(%pc),%a1
	move.l	FONTH(%a2),%d0
	addq.l	#4,%d0
	move.l	%d0,12(%a1)		| IA_Height
	lea	frameiclass(%pc),%a0
	bsr.w	newobject
	move.l	%d0,IMGFRAME(%a2)
	beq.w	makegadgets_end
	lea	addrtags(%pc),%a1
	moveq	#0,%d0
	movea.l	SCREEN(%a2),%a0
	move.b	36(%a0),%d0		| sc_WBorLeft
	add.l	FRAMEX(%a2),%d0
	addq.l	#4,%d0
	move.l	%d0,4(%a1)		| GA_Left
	move.l	BARTOP(%a2),%d1
	addq.l	#3,%d1
	move.l	%d1,12(%a1)		| GA_Top
	addq.l	#6,%d0
	add.l	SIZEW(%a2),%d0
	neg.l	%d0
	move.l	%d0,20(%a1)		| GA_RelWidth
	move.l	FONTH(%a2),28(%a1)	| GA_Height
	move.l	GADRIGHT(%a2),36(%a1)	| GA_Previous
	lea	strgclass(%pc),%a0
	bsr.w	newobject
	move.l	%d0,GADADDR(%a2)
	beq.s	makegadgets_end
| the buttons, after the field in the list
	lea	btntags(%pc),%a1
	moveq	#0,%d0
	movea.l	SCREEN(%a2),%a0
	move.b	36(%a0),%d0		| sc_WBorLeft
	add.l	GAP(%a2),%d0
	move.l	%d0,4(%a1)		| GA_Left
	move.l	FONTH(%a2),%d1		| in the middle of the frame
	addq.l	#4,%d1
	sub.l	BACKH(%a2),%d1
	asr.l	#1,%d1
	add.l	BARTOP(%a2),%d1
	addq.l	#1,%d1
	move.l	%d1,12(%a1)		| GA_Top
	move.l	IMGBACK(%a2),20(%a1)	| GA_Image
	move.l	#GAD_BACK,28(%a1)	| GA_ID
	move.l	GADADDR(%a2),36(%a1)	| GA_Previous
	lea	buttonclass(%pc),%a0
	bsr.w	newobject
	move.l	%d0,GADBACK(%a2)
	beq.s	makegadgets_end
	lea	btntags(%pc),%a1
	move.l	BACKW(%a2),%d0
	add.l	GAP(%a2),%d0
	add.l	%d0,4(%a1)		| GA_Left: beside it
	move.l	IMGFWD(%a2),20(%a1)
	move.l	#GAD_FWD,28(%a1)
	move.l	GADBACK(%a2),36(%a1)
	lea	buttonclass(%pc),%a0
	bsr.w	newobject
	move.l	%d0,GADFWD(%a2)
makegadgets_end:
	movem.l	(%sp)+,%d2-%d4/%a2/%a6
	rts

| An arrow: %d0 GA_RelRight, %d1 GA_RelBottom, %d2 its GAD_, %d3 the tag of
| its border, %d4 the gadget before, %a0 its image; in %d0
newarrow:
	lea	arrowtags(%pc),%a1
	move.l	%d0,4(%a1)		| GA_RelRight
	move.l	%d1,12(%a1)		| GA_RelBottom
	move.l	%a0,20(%a1)		| GA_Image
	move.l	%d2,28(%a1)		| GA_ID
	move.l	%d4,36(%a1)		| GA_Previous
	move.l	%d3,40(%a1)		| GA_RightBorder or GA_BottomBorder
	lea	buttonclass(%pc),%a0
	bra.s	newobject

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

| An object of the private class at %a0, with the tags at %a1
newprivate:
	move.l	%a2,-(%sp)
	movea.l	%a1,%a2
	suba.l	%a1,%a1
	jsr	-636(%a6)		| NewObjectA
	movea.l	%d0,%a0
	movea.l	(%sp)+,%a2
	tst.l	%d0
	rts

| The dispatcher of the buttons' images: sysiclass, but IM_DRAW of the normal
| and the selected look draws those of an inactive window. A0 the class, A2
| the object, A1 the message.
graydispatch:
	movea.l	24(%a0),%a0		| cl_Super
	cmp.l	#0x202,(%a1)		| IM_DRAW
	bne.s	2f
	move.l	12(%a1),%d0		| imp_State
	cmp.l	#1,%d0			| IDS_NORMAL or IDS_SELECTED
	bhi.s	2f
	lea	-20(%sp),%sp		| a copy of the message
	move.l	(%a1),(%sp)
	move.l	4(%a1),4(%sp)		| imp_RPort
	move.l	8(%a1),8(%sp)		| imp_Offset
	addq.l	#5,%d0			| IDS_INACTIVENORMAL, IDS_INACTIVESELECTED
	move.l	%d0,12(%sp)
	move.l	16(%a1),16(%sp)		| imp_DrInfo
	movea.l	%sp,%a1
	pea	1f(%pc)
	move.l	8(%a0),-(%sp)		| the superclass's h_Entry
	rts
1:	lea	20(%sp),%sp
	rts
2:	move.l	8(%a0),-(%sp)		| the rest as sysiclass does it
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

| The frame of the address field, and the field in it, after the page was
| drawn or the window got another size
drawaddr:
	movem.l	%d2-%d4/%a2/%a6,-(%sp)
	lea	vars(%pc),%a2
	tst.l	GADADDR(%a2)
	beq.s	1f
	movea.l	INTUITIONBASE(%a2),%a6
	lea	framesize(%pc),%a1
	move.l	WIDTH(%a2),%d0
	sub.l	FRAMEX(%a2),%d0
	subq.l	#2,%d0
	move.l	%d0,4(%a1)		| IA_Width
	movea.l	IMGFRAME(%a2),%a0
	jsr	-648(%a6)		| SetAttrsA
	movea.l	50(%a3),%a0		| wd_RPort
	movea.l	IMGFRAME(%a2),%a1
	moveq	#0,%d0
	move.b	54(%a3),%d0		| wd_BorderLeft
	add.l	FRAMEX(%a2),%d0
	moveq	#0,%d1
	move.b	55(%a3),%d1		| wd_BorderTop
	addq.l	#1,%d1
	moveq	#0,%d2			| IDS_NORMAL
	movea.l	DRAWINFO(%a2),%a2
	jsr	-618(%a6)		| DrawImageState
	lea	vars(%pc),%a2
	movea.l	GADADDR(%a2),%a0
	movea.l	%a3,%a1
	suba.l	%a2,%a2
	moveq	#3,%d0			| with the buttons after it
	jsr	-432(%a6)		| RefreshGList
1:	movem.l	(%sp)+,%d2-%d4/%a2/%a6
	rts

| The page's address into the field, when it changed - but not while it is
| typed into: then once it is left
newaddress:
	movem.l	%d2-%d4/%a2-%a3/%a6,-(%sp)
	lea	vars(%pc),%a2
	tst.l	GADADDR(%a2)
	beq.s	2f
	moveq	#HV_ADDRESS,%d0
	move.l	%d5,%d1
	lea	addrbuf(%pc),%a0
	move.l	%a0,%d2
	move.l	#ADDRSIZE,%d3
	moveq	#0,%d4
	bsr.w	callhost
	cmp.l	#2,%d0
	beq.s	1f
	tst.l	ADDRDIRTY(%a2)
	beq.s	2f
1:	move.l	#1,ADDRDIRTY(%a2)
	movea.l	GADADDR(%a2),%a0
	btst	#7,13(%a0)		| gg_Flags & GFLG_SELECTED: typed into
	bne.s	2f
	clr.l	ADDRDIRTY(%a2)
	movea.l	INTUITIONBASE(%a2),%a6
	movea.l	%a3,%a1			| the window
	lea	addrset(%pc),%a3
	lea	addrbuf(%pc),%a2
	move.l	%a2,4(%a3)		| STRINGA_TextVal
	suba.l	%a2,%a2
	jsr	-660(%a6)		| SetGadgetAttrsA
2:	movem.l	(%sp)+,%d2-%d4/%a2-%a3/%a6
	rts

| The cursor into the address field
toaddress:
	movem.l	%a2/%a6,-(%sp)
	lea	vars(%pc),%a0
	move.l	GADADDR(%a0),%d0
	beq.s	1f
	movea.l	INTUITIONBASE(%a0),%a6
	movea.l	%d0,%a0
	movea.l	%a3,%a1
	suba.l	%a2,%a2
	jsr	-462(%a6)		| ActivateGadget
1:	movem.l	(%sp)+,%a2/%a6
	rts

| Return in the address field: the page goes to what is in it
addressgo:
	movem.l	%d2-%d4/%a2/%a6,-(%sp)
	lea	vars(%pc),%a2
	movea.l	INTUITIONBASE(%a2),%a6
	movea.l	GADADDR(%a2),%a0
	move.l	#STRINGA_TextVal,%d0
	bsr.w	getattr
	tst.l	%d0
	beq.s	1f
	move.l	%d0,%d2
	moveq	#HV_LOAD,%d0
	move.l	%d5,%d1
	moveq	#0,%d3
	moveq	#0,%d4
	bsr.w	callhost
1:	movem.l	(%sp)+,%d2-%d4/%a2/%a6
	rts

| Z set if the string at %a0 is the one at %a1 - capitals - in any case
strieq:
	move.b	(%a0)+,%d0
	cmp.b	#'a',%d0
	blt.s	1f
	cmp.b	#'z',%d0
	bgt.s	1f
	sub.b	#32,%d0
1:	move.b	(%a1)+,%d1
	cmp.b	%d1,%d0
	bne.s	2f
	tst.b	%d1
	bne.s	strieq
2:	rts

| The scroll bars to where the page is, if that changed
updatebars:
	movem.l	%d2-%d4,-(%sp)
	moveq	#HV_SCROLLINFO,%d0
	move.l	%d5,%d1
	lea	barinfo(%pc),%a0
	move.l	%a0,%d2
	moveq	#0,%d3
	moveq	#0,%d4
	bsr.w	callhost
	tst.l	%d0
	beq.s	1f
	moveq	#GAD_V,%d0
	lea	barinfo(%pc),%a0
	bsr.s	setbar
	moveq	#GAD_H,%d0
	lea	barinfo+12(%pc),%a0
	bsr.s	setbar
1:	movem.l	(%sp)+,%d2-%d4
	rts

| The scroll bar %d0 (GAD_V or GAD_H) to the total, visible and top at %a0,
| unless it is being dragged
setbar:
	movem.l	%a2-%a3/%a6,-(%sp)
	lea	vars(%pc),%a2
	cmp.l	DRAGGING(%a2),%d0
	beq.s	1f
	move.l	%d0,%d1
	lsl.l	#2,%d1
	move.l	8(%a0),LASTV-4(%a2,%d1.l)
	lea	bartags(%pc),%a1
	move.l	(%a0),4(%a1)		| PGA_Total
	move.l	4(%a0),12(%a1)		| PGA_Visible
	move.l	8(%a0),20(%a1)		| PGA_Top
	movea.l	INTUITIONBASE(%a2),%a6
	movea.l	GADV-4(%a2,%d1.l),%a0
	movea.l	%a3,%a1			| the window
	lea	bartags(%pc),%a3
	suba.l	%a2,%a2
	jsr	-660(%a6)		| SetGadgetAttrsA
1:	movem.l	(%sp)+,%a2-%a3/%a6
	rts

| The scroll bar %d0 (GAD_V or GAD_H) moved: the page goes there
propmove:
	movem.l	%d2-%d4/%a2/%a6,-(%sp)
	lea	vars(%pc),%a2
	move.l	%d0,%d2
	lsl.l	#2,%d0
	movea.l	GADV-4(%a2,%d0.l),%a0
	movea.l	INTUITIONBASE(%a2),%a6
	move.l	#PGA_Top,%d0
	bsr.w	getattr
	move.l	%d2,%d1
	lsl.l	#2,%d1
	cmp.l	LASTV-4(%a2,%d1.l),%d0
	beq.s	1f
	move.l	%d0,LASTV-4(%a2,%d1.l)
	move.l	%d0,%d3			| the top
	subq.l	#1,%d2			| 0 up and down, 1 across
	moveq	#HV_SCROLLTO,%d0
	move.l	%d5,%d1
	moveq	#0,%d4
	bsr.w	callhost
1:	movem.l	(%sp)+,%d2-%d4/%a2/%a6
	rts

| The arrow %d0 pressed: the page scrolls a step, at the middle of the window
arrowstep:
	movem.l	%d2-%d4/%a2,-(%sp)
	lea	vars(%pc),%a2
	move.l	WIDTH(%a2),%d3
	lsr.l	#1,%d3
	swap	%d3
	move.l	HEIGHT(%a2),%d1
	lsr.l	#1,%d1
	move.w	%d1,%d3
	move.l	%d0,%d1
	moveq	#HV_SCROLL,%d0
	moveq	#1,%d2			| up
	cmp.w	#GAD_UP,%d1
	beq.s	1f
	moveq	#-1,%d2			| down
	cmp.w	#GAD_DOWN,%d1
	beq.s	1f
	moveq	#HV_SCROLLACROSS,%d0	| left
	cmp.w	#GAD_LEFT,%d1
	beq.s	1f
	moveq	#1,%d2			| right
1:	move.l	%d5,%d1
	moveq	#0,%d4
	bsr.w	callhost
	movem.l	(%sp)+,%d2-%d4/%a2
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
	sub.l	PAGETOP(%a2),%d3	| the address field above the page
	bgt.s	1f
	moveq	#1,%d3
1:	move.l	%d2,WIDTH(%a2)
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
	add.l	PAGETOP(%a2),%d3
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
		.long	0x8000006A, IDCMP_NEWSIZE+IDCMP_REFRESHWINDOW+IDCMP_MOUSEBUTTONS+IDCMP_MOUSEMOVE+IDCMP_CLOSEWINDOW+IDCMP_VANILLAKEY+IDCMP_RAWKEY+IDCMP_GADGETDOWN+IDCMP_GADGETUP+IDCMP_INTUITICKS
		.long	0x80000079, 0	| WA_PubScreen
		.long	0x8000007D, 0	| WA_Zoom
		.long	0x8000006C, 0	| WA_Gadgets
		.long	0x80000072, 160	| WA_MinWidth
		.long	0x80000073, 120	| WA_MinHeight
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
		.long	0x8000008E, 1	| WA_SizeBRight
		.long	0x8000008F, 1	| WA_SizeBBottom
		.long	0		| TAG_DONE
imagetags:	.long	SYSIA_DrawInfo, 0
		.long	SYSIA_Size, 0
		.long	SYSIA_Which, 0
		.long	0
vproptags:	.long	GA_RelRight, 0
		.long	GA_Top, 0
		.long	GA_Width, 0
		.long	GA_RelHeight, 0
		.long	GA_ID, GAD_V
		.long	GA_RightBorder, 1
		.long	GA_RelVerify, 1
		.long	GA_Immediate, 1
		.long	GA_FollowMouse, 1
		.long	PGA_Freedom, 4	| FREEVERT
		.long	PGA_NewLook, 1
		.long	PGA_Borderless, 1
		.long	PGA_Total, 1
		.long	PGA_Visible, 1
		.long	PGA_Top, 0
		.long	GA_Previous, 0
		.long	0
hproptags:	.long	GA_Left, 0
		.long	GA_RelBottom, 0
		.long	GA_RelWidth, 0
		.long	GA_Height, 0
		.long	GA_ID, GAD_H
		.long	GA_BottomBorder, 1
		.long	GA_RelVerify, 1
		.long	GA_Immediate, 1
		.long	GA_FollowMouse, 1
		.long	PGA_Freedom, 2	| FREEHORIZ
		.long	PGA_NewLook, 1
		.long	PGA_Borderless, 1
		.long	PGA_Total, 1
		.long	PGA_Visible, 1
		.long	PGA_Top, 0
		.long	GA_Previous, 0
		.long	0
arrowtags:	.long	GA_RelRight, 0
		.long	GA_RelBottom, 0
		.long	GA_Image, 0
		.long	GA_ID, 0
		.long	GA_Previous, 0
		.long	GA_RightBorder, 1
		.long	GA_RelVerify, 1
		.long	GA_Immediate, 1
		.long	0
frametags:	.long	IA_Width, 100
		.long	IA_Height, 0
		.long	IA_FrameType, 2	| FRAME_RIDGE
		.long	0
btntags:	.long	GA_Left, 0
		.long	GA_Top, 0
		.long	GA_Image, 0
		.long	GA_ID, 0
		.long	GA_Previous, 0
		.long	GA_RelVerify, 1
		.long	0
framesize:	.long	IA_Width, 0
		.long	0
addrtags:	.long	GA_Left, 0
		.long	GA_Top, 0
		.long	GA_RelWidth, 0
		.long	GA_Height, 0
		.long	GA_Previous, 0
		.long	GA_ID, GAD_ADDR
		.long	GA_RelVerify, 1
		.long	STRINGA_MaxChars, ADDRSIZE
		.long	0
addrset:	.long	STRINGA_TextVal, 0
		.long	0
bartags:	.long	PGA_Total, 0
		.long	PGA_Visible, 0
		.long	PGA_Top, 0
		.long	0
barinfo:	.long	0, 0, 0, 0, 0, 0
addrbuf:	.space	ADDRSIZE

	.set	TITLESIZE, 256
titlebuf1:	.space	TITLESIZE
titlebuf2:	.space	TITLESIZE

dosname:	.asciz	"dos.library"
intuiname:	.asciz	"intuition.library"
p96name:	.asciz	"Picasso96API.library"
sysiclass:	.asciz	"sysiclass"
propclass:	.asciz	"propgclass"
buttonclass:	.asciz	"buttongclass"
frameiclass:	.asciz	"frameiclass"
strgclass:	.asciz	"strgclass"
kw_hideaddress:	.asciz	"HIDEADDRESS"
title:		.asciz	"HostBrowser"
colon:		.asciz	": "
okgadget:	.asciz	"Ok"
homepage:	.asciz	"https://www.google.com/"
msg_usage:	.ascii	"Usage: HostBrowser [HIDEADDRESS] [<address>]\n"
		.ascii	"Shows a web page in a window, drawn by uae4arm; without an\n"
		.ascii	"address the home page. A click follows a link, a drag moves\n"
		.ascii	"the page, the wheel scrolls it; the keys go to the page. The\n"
		.ascii	"right button or Alt-cursor left go back, Alt-cursor right\n"
		.ascii	"forward, F5 loads the page again, Right Amiga-Q ends it.\n"
		.ascii	"The scroll bars in the borders scroll it too, and so do the\n"
		.ascii	"cursor keys outside a text field, with Shift by a page.\n"
		.ascii	"The address field above the page goes to what is typed into\n"
		.ascii	"it, or searches for it; Ctrl-L or F6 put the cursor in it.\n"
		.ascii	"The buttons before it go back and forward.\n"
		.ascii	"HIDEADDRESS leaves it out.\n"
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
