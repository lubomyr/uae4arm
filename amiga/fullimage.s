| fullimage.s - the image of the button for a screen of its own, in the title
| bar of HostView and HostBrowser; included by HostPlay.s and HostBrowser.s
|
| A class of its own over sysiclass: an image of it is made as a ZOOMIMAGE,
| so sysiclass draws it in the look of the system, at the zoom gadget's
| size. Not pressed, it is the zoom gadget with the inside of the zoom
| gadget pressed - a square, as of a window as large as it gets - so that
| it is not one more zoom gadget; pressed, it is the zoom gadget pressed.
| GfxBase is kept in the class's cl_UserData.

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
| of the look that is not pressed as sysiclass does it.
fulldispatch:
	cmp.l	#0x202,(%a1)		| IM_DRAW
	bne.s	1f
	move.l	12(%a1),%d0		| imp_State
	beq.s	2f			| IDS_NORMAL
	cmp.l	#5,%d0			| IDS_INACTIVENORMAL
	beq.s	2f
1:	movea.l	24(%a0),%a0		| cl_Super
	move.l	8(%a0),-(%sp)		| its h_Entry
	rts
| The zoom gadget pressed into a bitmap of its own, the same kind as the
| window's; then the zoom gadget into the window, and the inside of the
| pressed one over its inside
2:	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	movea.l	%a0,%a5			| the class
	movea.l	%a1,%a4			| the message
	movea.l	%a2,%a3			| the image
	movea.l	36(%a5),%a6		| GfxBase
	moveq	#0,%d6
	move.w	4(%a3),%d6		| ig_Width
	moveq	#0,%d7
	move.w	6(%a3),%d7		| ig_Height
	movea.l	4(%a4),%a0		| imp_RPort
	movea.l	4(%a0),%a0		| rp_BitMap
	moveq	#4,%d1			| BMA_DEPTH
	jsr	-960(%a6)		| GetBitMapAttr
	move.l	%d0,%d2			| depth
	move.l	%d6,%d0
	move.l	%d7,%d1
	moveq	#0,%d3			| flags
	movea.l	4(%a4),%a0
	movea.l	4(%a0),%a0		| the friend
	jsr	-918(%a6)		| AllocBitMap
	movea.l	%d0,%a2			| the bitmap
	tst.l	%d0
	beq.w	8f
	lea	fullrport(%pc),%a1
	jsr	-198(%a6)		| InitRastPort
	lea	fullrport(%pc),%a1
	move.l	%a2,4(%a1)		| rp_BitMap
| The pressed one, at 0,0 of the bitmap: a copy of the message
	lea	-20(%sp),%sp
	move.l	(%a4),(%sp)		| IM_DRAW
	move.l	%a1,4(%sp)		| imp_RPort
	move.w	(%a3),%d0
	neg.w	%d0
	move.w	%d0,8(%sp)		| imp_Offset.X
	move.w	2(%a3),%d0
	neg.w	%d0
	move.w	%d0,10(%sp)		| imp_Offset.Y
	move.l	12(%a4),%d0
	addq.l	#1,%d0			| IDS_SELECTED, IDS_INACTIVESELECTED
	move.l	%d0,12(%sp)
	move.l	16(%a4),16(%sp)		| imp_DrInfo
	movea.l	%sp,%a1
	bsr.s	fullsuperdraw
	lea	20(%sp),%sp
| The zoom gadget as it is
	movea.l	%a4,%a1
	bsr.s	fullsuperdraw
| The inside of the pressed one over it
	lea	fullrport(%pc),%a0
	moveq	#2,%d0			| from
	moveq	#2,%d1
	movea.l	4(%a4),%a1		| to
	move.w	(%a3),%d2
	add.w	8(%a4),%d2
	ext.l	%d2
	addq.l	#2,%d2
	move.w	2(%a3),%d3
	add.w	10(%a4),%d3
	ext.l	%d3
	addq.l	#2,%d3
	move.l	%d6,%d4			| width
	subq.l	#4,%d4
	move.l	%d7,%d5			| height
	subq.l	#4,%d5
	move.l	#0xC0,%d6		| a copy
	tst.l	%d4
	ble.s	3f
	tst.l	%d5
	ble.s	3f
	jsr	-552(%a6)		| ClipBlit
3:	jsr	-228(%a6)		| WaitBlit
	movea.l	%a2,%a0
	jsr	-924(%a6)		| FreeBitMap
	bra.s	9f
| No bitmap: the zoom gadget as it is
8:	movea.l	%a4,%a1
	bsr.s	fullsuperdraw
9:	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	moveq	#0,%d0
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
	.even
fullrport:	.space	100		| struct RastPort
