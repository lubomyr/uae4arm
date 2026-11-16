| fullimage.s - the image of the button for a screen of its own, in the title
| bar of HostView and HostBrowser; included by HostPlay.s and HostBrowser.s
|
| A class of its own over sysiclass: an image of it is made as a ZOOMIMAGE,
| so sysiclass draws it in the look of the system, at the zoom gadget's
| size and in its colours, active or not. Then what is inside the zoom
| gadget's large box - its small box - is taken away, so that the box is
| empty: a window as large as it gets, and not one more zoom gadget.
| Pressed, it is the zoom gadget pressed.
| The large box is what of the inside differs from its background, the
| colour most of it has; Picasso96 reads and writes the pixels. GfxBase is
| kept in the class's cl_UserData.

| The class: %a6 IntuitionBase, %d0 GfxBase. %d0 the class, or 0 with Z set.
makefullclass:
	movem.l	%d2/%a2,-(%sp)
	move.l	%d0,%d2
	suba.l	%a0,%a0			| no name: private
	lea	fullsuper(%pc),%a1
	suba.l	%a2,%a2
	moveq	#0,%d0
	moveq	#0,%d1
	jsr	-678(%a6)		| MakeClass
	tst.l	%d0
	beq.s	1f
	movea.l	%d0,%a0
	lea	fulldispatch(%pc),%a1
	move.l	%a1,8(%a0)		| cl_Dispatcher.h_Entry
	move.l	%d2,36(%a0)		| cl_UserData
1:	movem.l	(%sp)+,%d2/%a2
	tst.l	%d0
	rts

| An image of the class %a0, with the tags at %a1 - those of a sysiclass
| ZOOMIMAGE; %a6 IntuitionBase. %d0 the image, or 0 with Z set.
newfullimage:
	move.l	%a2,-(%sp)
	movea.l	%a1,%a2
	suba.l	%a1,%a1
	jsr	-636(%a6)		| NewObjectA
	movea.l	(%sp)+,%a2
	tst.l	%d0
	rts

| The dispatcher: A0 the class, A2 the image, A1 the message. All but IM_DRAW
| of the looks that are not pressed as sysiclass does it.
fulldispatch:
	cmp.l	#0x202,(%a1)		| IM_DRAW
	bne.s	1f
	cmp.l	#1,12(%a1)		| imp_State: IDS_SELECTED as it is
	bne.s	2f
1:	movea.l	24(%a0),%a0		| cl_Super
	move.l	8(%a0),-(%sp)		| its h_Entry
	rts
2:	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	movea.l	%a0,%a5			| the class
	movea.l	%a1,%a4			| the message
	movea.l	%a2,%a3			| the image
| The zoom gadget
	movea.l	%a4,%a1
	bsr.w	fullsuperdraw
| Its inside in the window: a sixth of the width in from the left, an
| eighth from the right, an eighth of the height from the top and the
| bottom - the frame of the gadget outside it
	moveq	#0,%d6
	move.w	4(%a3),%d6		| ig_Width
	moveq	#0,%d7
	move.w	6(%a3),%d7		| ig_Height
	lea	fullix0(%pc),%a0
	move.w	(%a3),%d0
	add.w	8(%a4),%d0		| imp_Offset.X
	ext.l	%d0
	move.l	%d6,%d1
	divu	#6,%d1
	and.l	#0xFFFF,%d1
	add.l	%d0,%d1
	move.l	%d1,(%a0)		| fullix0
	add.l	%d6,%d0
	move.l	%d6,%d1
	lsr.l	#3,%d1
	sub.l	%d1,%d0
	subq.l	#1,%d0
	move.l	%d0,8(%a0)		| fullix1
	move.w	2(%a3),%d0
	add.w	10(%a4),%d0		| imp_Offset.Y
	ext.l	%d0
	move.l	%d7,%d1
	lsr.l	#3,%d1
	move.l	%d0,%d2
	add.l	%d1,%d2
	move.l	%d2,4(%a0)		| fulliy0
	add.l	%d7,%d0
	sub.l	%d1,%d0
	subq.l	#1,%d0
	move.l	%d0,12(%a0)		| fulliy1
| With Picasso96: its background, and the box
	movea.l	4,%a6
	lea	fullp96name(%pc),%a1
	moveq	#0,%d0
	jsr	-552(%a6)		| OpenLibrary
	move.l	%d0,%d5
	beq.s	9f
	movea.l	%d0,%a6
	bsr.s	fullmost
	move.l	%d0,%d4			| the background
	bsr.w	fullbox
	tst.l	%d0
	beq.s	8f			| nothing differs
| What is inside the box's lines, the background
	addq.l	#1,%d0
	addq.l	#1,%d1
	subq.l	#1,%d2
	subq.l	#1,%d3
	cmp.l	%d0,%d2
	blt.s	8f
	cmp.l	%d1,%d3
	blt.s	8f
	movea.l	4(%a4),%a1		| imp_RPort
	jsr	-126(%a6)		| p96RectFill
8:	movea.l	%a6,%a1
	movea.l	4,%a6
	jsr	-414(%a6)		| CloseLibrary
9:	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	moveq	#0,%d0
	rts

| The colour most of the inside has, in the window of the message at %a4,
| with Picasso96 in %a6: %d0. Eight colours are counted, the rest left out.
fullmost:
	movem.l	%d2-%d7/%a2-%a3,-(%sp)
	lea	-64(%sp),%sp		| colour, count - eight times
	movea.l	%sp,%a3
	moveq	#0,%d7			| colours so far
	move.l	fulliy0(%pc),%d3
1:	cmp.l	fulliy1(%pc),%d3
	bgt.s	6f
	move.l	fullix0(%pc),%d2
2:	cmp.l	fullix1(%pc),%d2
	bgt.s	5f
	bsr.w	fullread
	movea.l	%a3,%a0
	move.l	%d7,%d1
	bra.s	4f
3:	cmp.l	(%a0),%d0
	beq.s	7f
	addq.l	#8,%a0
4:	dbra	%d1,3b
	cmp.l	#8,%d7			| a new colour, if there is room
	bcc.s	8f
	move.l	%d0,(%a0)
	clr.l	4(%a0)
	addq.l	#1,%d7
7:	addq.l	#1,4(%a0)
8:	addq.l	#1,%d2
	bra.s	2b
5:	addq.l	#1,%d3
	bra.s	1b
| the one counted most
6:	moveq	#0,%d0
	moveq	#-1,%d2			| its count
	movea.l	%a3,%a0
	bra.s	3f
2:	cmp.l	4(%a0),%d2
	bge.s	1f
	move.l	4(%a0),%d2
	move.l	(%a0),%d0
1:	addq.l	#8,%a0
3:	dbra	%d7,2b
	lea	64(%sp),%sp
	movem.l	(%sp)+,%d2-%d7/%a2-%a3
	rts

| The box: what of the inside is not of colour %d4. %d0,%d1 to %d2,%d3 its
| ends in the window; %d0 0 when all is of that colour.
fullbox:
	movem.l	%d4-%d7/%a2-%a3,-(%sp)
	move.l	#0x7FFF,%a2		| left
	move.l	#0x7FFF,%a3		| top
	moveq	#-1,%d6			| right
	moveq	#-1,%d7			| bottom
	move.l	fulliy0(%pc),%d3
1:	cmp.l	fulliy1(%pc),%d3
	bgt.s	5f
	move.l	fullix0(%pc),%d2
2:	cmp.l	fullix1(%pc),%d2
	bgt.s	4f
	bsr.s	fullread
	cmp.l	%d4,%d0
	beq.s	3f
	cmpa.l	%d2,%a2
	ble.s	6f
	movea.l	%d2,%a2
6:	cmpa.l	%d3,%a3
	ble.s	7f
	movea.l	%d3,%a3
7:	cmp.l	%d2,%d6
	bge.s	8f
	move.l	%d2,%d6
8:	cmp.l	%d3,%d7
	bge.s	3f
	move.l	%d3,%d7
3:	addq.l	#1,%d2
	bra.s	2b
4:	addq.l	#1,%d3
	bra.s	1b
5:	moveq	#0,%d0
	tst.l	%d6
	bmi.s	9f
	move.l	%a2,%d0
	move.l	%a3,%d1
	move.l	%d6,%d2
	move.l	%d7,%d3
9:	movem.l	(%sp)+,%d4-%d7/%a2-%a3
	rts

| The colour of the pixel %d2,%d3 of the window, without the alpha: %d0
fullread:
	movea.l	4(%a4),%a1
	move.l	%d2,%d0
	move.l	%d3,%d1
	jsr	-120(%a6)		| p96ReadPixel
	and.l	#0x00FFFFFF,%d0
	rts

| sysiclass draws the message at %a1 of the image %a3; %a5 the class
fullsuperdraw:
	movem.l	%a2/%a6,-(%sp)
	movea.l	%a3,%a2
	movea.l	24(%a5),%a0		| cl_Super
	movea.l	8(%a0),%a6		| its h_Entry
	jsr	(%a6)
	movem.l	(%sp)+,%a2/%a6
	rts

fullsuper:	.asciz	"sysiclass"
fullp96name:	.asciz	"Picasso96API.library"
	.even
fullix0:	.long	0		| the inside of the image in the window,
fulliy0:	.long	0		| both ends in it
fullix1:	.long	0
fulliy1:	.long	0
