| AndroidBrowser - shows a web page in a window of uae4arm over the Amiga screen
|
| Usage: AndroidBrowser [HIDEADDRESS] [NEWWINDOW] [<address> | <file>]
| HIDEADDRESS leaves out the address field of the window, when there is an
| address to show. NEWWINDOW shows it in a window of its own; without it the
| window used last shows it. An address with "://" goes as it is; a name that is a file
| shows that file,
| which has to be on a folder drive of uae4arm; anything else is an address
| without its scheme, "aminet.net" for https://aminet.net. Without either the
| window asks for an address. Started from its icon it does the same; as the
| default tool of a page's icon it shows that page. "AndroidBrowser ?" tells
| the usage.
|
| It calls uaelib function 200: 2 with the address, 3 with a Lock of the file,
| plus $100 for HIDEADDRESS and $200 for NEWWINDOW.
| Built like AndroidPlay - see build.sh; all code is PC relative.
|
| Registers kept: a2 arguments, d3 their length, a3 the name, a4 uaelib,
| a5 DOSBase, d4 the Lock, d5 what uaelib gave back, d6 the WBStartup message
| (0 from the Shell), d7 the return code.

	.text
	.globl	_start

_start:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	movea.l	%a0,%a2			| arguments
	move.l	%d0,%d3			| and their length
	moveq	#0,%d6
	moveq	#0,%d4
	moveq	#20,%d7			| return code
	lea	vars(%pc),%a0
	clr.l	(%a0)+			| intuitionbase
	clr.l	(%a0)+			| olddir
	clr.l	(%a0)+			| dirchanged
	clr.l	(%a0)			| flags for uaelib

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

| From Workbench: the second argument is the page, if there is one
	movea.l	%d6,%a0
	cmp.l	#2,28(%a0)		| sm_NumArgs
	blt.w	noname
	movea.l	36(%a0),%a0		| sm_ArgList
	move.l	8(%a0),%d1		| wa_Lock of the page's drawer
	movea.l	12(%a0),%a3		| wa_Name
	movea.l	%a5,%a6
	jsr	-126(%a6)		| CurrentDir
	lea	vars(%pc),%a0
	move.l	%d0,4(%a0)		| olddir
	move.l	#1,8(%a0)		| dirchanged
	bra.w	asfile

| From the Shell: the first argument, in quotes if it has spaces
shellargs:
	tst.l	%d3
	beq.w	noname
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
	beq.w	noname
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
	beq.w	noname
| HIDEADDRESS and NEWWINDOW, before the address
	movea.l	%a3,%a0
	lea	kw_hideaddress(%pc),%a1
	bsr.w	strieq
	bne.s	notkw_hide
	move.l	#0x100,%d0
	bra.s	keyword
notkw_hide:
	movea.l	%a3,%a0
	lea	kw_newwindow(%pc),%a1
	bsr.w	strieq
	bne.s	notkeyword
	move.l	#0x200,%d0
keyword:
	lea	vars(%pc),%a0
	or.l	%d0,12(%a0)		| flags
	tst.l	%d3
	beq.w	noname
	addq.l	#1,%a2			| past the end of the keyword
	subq.l	#1,%d3
	bra.w	shellargs
notkeyword:
	cmp.b	#'?',(%a3)		| "AndroidBrowser ?"
	bne.s	notusage
	tst.b	1(%a3)
	bne.s	notusage
	lea	msg_usage(%pc),%a0
	bsr.w	report
	moveq	#0,%d7
	bra.w	closeall
notusage:
| "://" in it: an address
	movea.l	%a3,%a0
findscheme:
	move.b	(%a0)+,%d0
	beq.s	asfile
	cmp.b	#':',%d0
	bne.s	findscheme
	cmp.b	#'/',(%a0)
	bne.s	findscheme
	cmp.b	#'/',1(%a0)
	bne.s	findscheme
	bra.w	asaddress

| A file, if there is one by that name; else an address after all
asfile:
	bsr.w	finduaelib
	bne.w	closeall
	move.l	%a3,%d1
	moveq	#-2,%d2			| ACCESS_READ
	movea.l	%a5,%a6
	jsr	-84(%a6)		| Lock
	move.l	%d0,%d4
	beq.s	notfile
	move.l	%d4,-(%sp)		| the Lock
	moveq	#3,%d0			| a file as a web page
	bra.s	call
notfile:
	tst.l	%d6			| from Workbench it was a file
	beq.s	asaddress
	lea	msg_notfound(%pc),%a0
	bsr.w	report
	moveq	#10,%d7
	bra.w	closeall

noname:
	lea	empty(%pc),%a3
asaddress:
	bsr.w	finduaelib
	bne.w	closeall
	move.l	%a3,-(%sp)		| the address
	moveq	#2,%d0			| a web page
call:
	lea	vars(%pc),%a0
	or.l	12(%a0),%d0		| flags
	move.l	%d0,-(%sp)
	move.l	#200,-(%sp)		| uaelib function
	jsr	(%a4)
	lea	12(%sp),%sp
	move.l	%d0,%d5

	tst.l	%d4
	beq.s	result
	move.l	%d4,%d1
	movea.l	%a5,%a6
	jsr	-90(%a6)		| UnLock

result:
	moveq	#0,%d7
	cmp.l	#1,%d5
	beq.s	closeall
	moveq	#10,%d7
	lea	msg_oldemu(%pc),%a0
	tst.l	%d5
	beq.s	failed
	lea	msg_notfolder(%pc),%a0
	cmp.l	#2,%d5
	beq.s	failed
	lea	msg_cannot(%pc),%a0
failed:
	bsr.w	report

closeall:
	lea	vars(%pc),%a2
	tst.l	8(%a2)			| dirchanged
	beq.s	closedos
	move.l	4(%a2),%d1		| olddir
	movea.l	%a5,%a6
	jsr	-126(%a6)		| CurrentDir
closedos:
	movea.l	%a5,%a1
	movea.l	4,%a6
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

| uaelib into %a4: at $F0FF60, or $EFFF60 when the boot ROM moved for CDTV, a
| trap instruction followed by RTS. Z clear, and told, if there is none.
finduaelib:
	movea.l	#0xF0FF60,%a4
	bsr.s	isuaelib
	beq.s	finduaelib_end
	movea.l	#0xEFFF60,%a4
	bsr.s	isuaelib
	beq.s	finduaelib_end
	lea	msg_notuae(%pc),%a0
	bsr.s	report
	moveq	#1,%d0			| Z clear
finduaelib_end:
	rts

| Z set if the string at %a0 is the one at %a1 - capitals - in any case
strieq:
	move.b	(%a0)+,%d0
	cmp.b	#'a',%d0
	blt.s	strieq_upper
	cmp.b	#'z',%d0
	bgt.s	strieq_upper
	sub.b	#32,%d0
strieq_upper:
	move.b	(%a1)+,%d1
	cmp.b	%d1,%d0
	bne.s	strieq_end
	tst.b	%d1
	bne.s	strieq
strieq_end:
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

| Tells the message at %a0: in the Shell, or in a requester from Workbench
report:
	tst.l	%d6
	beq.s	print
	movem.l	%d0/%d2/%a2-%a3/%a6,-(%sp)
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
	movem.l	(%sp)+,%d0/%d2/%a2-%a3/%a6
	rts

| Writes the string at %a0 to the output
print:
	movem.l	%d0/%d2-%d3/%a6,-(%sp)
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
	movem.l	(%sp)+,%d0/%d2-%d3/%a6
	rts

	.even
vars:		.long	0, 0, 0, 0	| intuitionbase, olddir, dirchanged, flags
easy:		.long	0, 0, 0, 0, 0	| struct EasyStruct

dosname:	.asciz	"dos.library"
intuiname:	.asciz	"intuition.library"
reqtitle:	.asciz	"AndroidBrowser"
okgadget:	.asciz	"Ok"
empty:		.byte	0
kw_hideaddress:	.asciz	"HIDEADDRESS"
kw_newwindow:	.asciz	"NEWWINDOW"
msg_usage:	.ascii	"Usage: AndroidBrowser [HIDEADDRESS] [NEWWINDOW] [<address> | <file>]\n"
		.ascii	"Shows a web page in a window of uae4arm over the Amiga screen:\n"
		.ascii	"an address such as https://aminet.net or aminet.net, or a file\n"
		.ascii	"on a folder drive. Without either the window asks for an address.\n"
		.ascii	"HIDEADDRESS leaves out the address field of the window;\n"
		.ascii	"NEWWINDOW shows the page in a window of its own.\n"
		.byte	0
msg_notuae:	.asciz	"AndroidBrowser: this runs in uae4arm only\n"
msg_notfound:	.asciz	"AndroidBrowser: no such file\n"
msg_oldemu:	.asciz	"AndroidBrowser: this needs a newer uae4arm\n"
msg_notfolder:	.asciz	"AndroidBrowser: the file has to be on a folder drive, not inside a hardfile\n"
msg_cannot:	.asciz	"AndroidBrowser: uae4arm cannot show it here\n"
		.even
