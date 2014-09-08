; Keen Dreams Source Code
; Copyright (C) 2014 Javier M. Chavez
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License along
; with this program; if not, write to the Free Software Foundation, Inc.,
; 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

;=================================
;
; EGA view manager routines
;
;=================================

;============================================================================
;
; All EGA drawing routines that write out words need to have alternate forms
; for starting on even and odd addresses, because writing a word at segment
; offset 0xffff causes an exception!  To work around this, write a single
; byte out to make the address even, so it wraps cleanly at the end.
;
; All of these routines assume read/write mode 0, and will allways return
; in that state.
; The direction flag should be clear
; readmap/writemask is left in an undefined state
;
;============================================================================


;============================================================================
;
; VW_Plot (int x,y,color)
;
;============================================================================

DATASEG

plotpixels	db	128,64,32,16,8,4,2,1

CODESEG

PROC	VW_Plot x:WORD, y:WORD, color:WORD
PUBLIC	VW_Plot
USES	SI,DI

	mov	es,[screenseg]

	mov	dx,SC_INDEX
	mov	ax,SC_MAPMASK+15*256
	WORDOUT

	mov	dx,GC_INDEX
	mov	ax,GC_MODE+2*256	;write mode 2
	WORDOUT

	mov	di,[bufferofs]
	mov	bx,[y]
	shl	bx,1
	add	di,[ylookup+bx]
	mov	bx,[x]
	mov	ax,bx
	shr	ax,1
	shr	ax,1
	shr	ax,1
	add	di,ax				; di = byte on screen

	and	bx,7
	mov	ah,[plotpixels+bx]
	mov	al,GC_BITMASK		;mask off other pixels
	WORDOUT

	mov		bl,[BYTE color]
	xchg	bl,[es:di]		; load latches and write pixel

	mov	dx,GC_INDEX
	mov	ah,0ffh				;no bit mask
	WORDOUT
	mov	ax,GC_MODE+0*256	;write mode 0
	WORDOUT

	ret

ENDP


;============================================================================
;
; VW_Vlin (int yl,yh,x,color)
;
;============================================================================

PROC	VW_Vlin yl:WORD, yh:WORD, x:WORD, color:WORD
PUBLIC	VW_Vlin
USES	SI,DI

	mov	es,[screenseg]

	mov	dx,SC_INDEX
	mov	ax,SC_MAPMASK+15*256
	WORDOUT

	mov	dx,GC_INDEX
	mov	ax,GC_MODE+2*256	;write mode 2
	WORDOUT

	mov	di,[bufferofs]
	mov	bx,[yl]
	shl	bx,1
	add	di,[ylookup+bx]
	mov	bx,[x]
	mov	ax,bx
	shr	ax,1
	shr	ax,1
	shr	ax,1
	add	di,ax				; di = byte on screen

	and	bx,7
	mov	ah,[plotpixels+bx]
	mov	al,GC_BITMASK		;mask off other pixels
	WORDOUT

	mov	cx,[yh]
	sub	cx,[yl]
	inc	cx					;number of pixels to plot

	mov	bh,[BYTE color]
	mov	dx,[linewidth]

@@plot:
	mov		bl,bh
	xchg	bl,[es:di]		; load latches and write pixel
	add		di,dx

	loop	@@plot

	mov	dx,GC_INDEX
	mov	ah,0ffh				;no bit mask
	WORDOUT
	mov	ax,GC_MODE+0*256	;write mode 0
	WORDOUT

	ret

ENDP


;============================================================================


;===================
;
; VW_DrawTile8
;
; xcoord in bytes (8 pixels), ycoord in pixels
; All Tile8s are in one grseg, so an offset is calculated inside it
;
;===================

PROC	VW_DrawTile8	xcoord:WORD, ycoord:WORD, tile:WORD
PUBLIC	VW_DrawTile8
USES	SI,DI

	mov	es,[screenseg]

	mov	di,[bufferofs]
	add	di,[xcoord]
	mov	bx,[ycoord]
	shl	bx,1
	add	di,[ylookup+bx]
	mov	[ss:screendest],di		;screen destination

	mov	bx,[linewidth]
	dec	bx

	mov	si,[tile]
	shl	si,1
	shl	si,1
	shl	si,1
	shl	si,1
	shl	si,1

	mov	ds,[grsegs+STARTTILE8*2] ; segment for all tile8s

	mov	cx,4					;planes to draw
	mov	ah,0001b				;map mask

	mov	dx,SC_INDEX
	mov	al,SC_MAPMASK

;
; start drawing
;

@@planeloop:
	WORDOUT
	shl	ah,1					;shift plane mask over for next plane

	mov	di,[ss:screendest]		;start at same place in all planes

REPT	7
	movsb
	add	di,bx
ENDM
	movsb

	loop	@@planeloop

	mov	ax,ss
	mov	ds,ax					;restore turbo's data segment

	ret

ENDP


;============================================================================
;
; VW_MaskBlock
;
; Draws a masked block shape to the screen.  bufferofs is NOT accounted for.
; The mask comes first, then four planes of data.
;
;============================================================================

DATASEG

UNWOUNDMASKS	=	10


maskroutines	dw	mask0,mask0,mask1E,mask1E,mask2E,mask2O,mask3E,mask3O
				dw	mask4E,mask4O,mask5E,mask5O,mask6E,mask6O
				dw	mask7E,mask7O,mask8E,mask8O,mask9E,mask9O
				dw	mask10E,mask10O


routinetouse	dw	?

CODESEG

PROC	VW_MaskBlock	segm:WORD, ofs:WORD, dest:WORD, wide:WORD, height:WORD, planesize:WORD
PUBLIC	VW_MaskBlock
USES	SI,DI

	mov	es,[screenseg]

	mov	[BYTE planemask],1
	mov	[BYTE planenum],0

	mov	di,[wide]
	mov	dx,[linewidth]
	sub	dx,[wide]
	mov	[linedelta],dx			;amount to add after drawing each line

	mov	bx,[planesize]			; si+bx = data location

	cmp	di,UNWOUNDMASKS
	jbe	@@unwoundroutine
	mov	[routinetouse],OFFSET generalmask
	jmp	NEAR @@startloop

;=================
;
; use the unwound routines
;
;=================

@@unwoundroutine:
	mov	cx,[dest]
	shr	cx,1
	rcl	di,1					;shift a 1 in if destination is odd
	shl	di,1					;to index into a word width table
	mov	ax,[maskroutines+di]	;call the right routine
	mov	[routinetouse],ax

@@startloop:
	mov	ds,[segm]

@@drawplane:
	mov	dx,SC_INDEX
	mov	al,SC_MAPMASK
	mov	ah,[ss:planemask]
	WORDOUT
	mov	dx,GC_INDEX
	mov	al,GC_READMAP
	mov	ah,[ss:planenum]
	WORDOUT

	mov	si,[ofs]				;start back at the top of the mask
	mov	di,[dest]				;start at same place in all planes
	mov	cx,[height]				;scan lines to draw
	mov dx,[ss:linedelta]

	jmp [ss:routinetouse]		;draw one plane
planereturn:					;routine jmps back here

	add	bx,[ss:planesize]		;start of mask = start of next plane

	inc	[ss:planenum]
	shl	[ss:planemask],1		;shift plane mask over for next plane
	cmp	[ss:planemask],10000b	;done all four planes?
	jne	@@drawplane

mask0:
	mov	ax,ss
	mov	ds,ax
	ret							;width of 0 = no drawing

;==============
;
; General purpose masked block drawing.  This could be optimised into
; four routines to use words, but few play loop sprites should be this big!
;
;==============

generalmask:
	mov	dx,cx

@@lineloopgen:
	mov	cx,[wide]
@@byteloop:
	mov	al,[es:di]
	and	al,[si]
	or	al,[bx+si]
	inc	si
	stosb
	loop	@@byteloop

	add	di,[ss:linedelta]
	dec	dx
	jnz	@@lineloopgen
	jmp	planereturn

;=================
;
; Horizontally unwound routines to draw certain masked blocks faster
;
;=================

MACRO	MASKBYTE
	mov	al,[es:di]
	and	al,[si]
	or	al,[bx+si]
	inc	si
	stosb
ENDM

MACRO	MASKWORD
	mov	ax,[es:di]
	and	ax,[si]
	or	ax,[bx+si]
	inc	si
	inc	si
	stosw
ENDM

MACRO	SPRITELOOP	addr
	add	di,dx
	loop	addr
	jmp	planereturn
ENDM


EVEN
mask1E:
	MASKBYTE
	SPRITELOOP	mask1E

EVEN
mask2E:
	MASKWORD
	SPRITELOOP	mask2E

EVEN
mask2O:
	MASKBYTE
	MASKBYTE
	SPRITELOOP	mask2O

EVEN
mask3E:
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask3E

EVEN
mask3O:
	MASKBYTE
	MASKWORD
	SPRITELOOP	mask3O

EVEN
mask4E:
	MASKWORD
	MASKWORD
	SPRITELOOP	mask4E

EVEN
mask4O:
	MASKBYTE
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask4O

EVEN
mask5E:
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask5E

EVEN
mask5O:
	MASKBYTE
	MASKWORD
	MASKWORD
	SPRITELOOP	mask5O

EVEN
mask6E:
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask6E

EVEN
mask6O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask6O

EVEN
mask7E:
	MASKWORD
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask7E

EVEN
mask7O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask7O

EVEN
mask8E:
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask8E

EVEN
mask8O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask8O

EVEN
mask9E:
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask9E

EVEN
mask9O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask9O

EVEN
mask10E:
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	SPRITELOOP	mask10E

EVEN
mask10O:
	MASKBYTE
	MASKWORD
	MASKWORD
	MASKWORD
	MASKWORD
	MASKBYTE
	SPRITELOOP	mask10O


ENDP


;============================================================================
;
; VW_ScreenToScreen
;
; Basic block copy routine.  Copies one block of screen memory to another,
; using write mode 1 (sets it and returns with write mode 0).  bufferofs is
; NOT accounted for.
;
;============================================================================

PROC	VW_ScreenToScreen	source:WORD, dest:WORD, wide:WORD, height:WORD
PUBLIC	VW_ScreenToScreen
USES	SI,DI

	pushf
	cli

	mov	dx,SC_INDEX
	mov	ax,SC_MAPMASK+15*256
	WORDOUT
	mov	dx,GC_INDEX
	mov	ax,GC_MODE+1*256
	WORDOUT

	popf

	mov	bx,[linewidth]
	sub	bx,[wide]

	mov	ax,[screenseg]
	mov	es,ax
	mov	ds,ax

	mov	si,[source]
	mov	di,[dest]				;start at same place in all planes
	mov	dx,[height]				;scan lines to draw
	mov	ax,[wide]

@@lineloop:
	mov	cx,ax
	rep	movsb
	add	si,bx
	add	di,bx

	dec	dx
	jnz	@@lineloop

	mov	dx,GC_INDEX
	mov	ax,GC_MODE+0*256
	WORDOUT

	mov	ax,ss
	mov	ds,ax					;restore turbo's data segment

	ret

ENDP


;============================================================================
;
; VW_MemToScreen
;
; Basic block drawing routine. Takes a block shape at segment pointer source
; with four planes of width by height data, and draws it to dest in the
; virtual screen, based on linewidth.  bufferofs is NOT accounted for.
; There are four drawing routines to provide the best optimized code while
; accounting for odd segment wrappings due to the floating screens.
;
;============================================================================

DATASEG

memtoscreentable	dw	eventoeven,eventoodd,oddtoeven,oddtoodd

CODESEG


PROC	VW_MemToScreen	source:WORD, dest:WORD, wide:WORD, height:WORD
PUBLIC	VW_MemToScreen
USES	SI,DI

	mov	es,[screenseg]

	mov	bx,[linewidth]
	sub	bx,[wide]

	mov	ds,[source]


	xor	si,si					;block is segment aligned

	xor	di,di
	shr	[wide],1				;change wide to words, and see if carry is set
	rcl	di,1					;1 if wide is odd
	mov	ax,[dest]
	shr	ax,1
	rcl	di,1					;shift a 1 in if destination is odd
	shl	di,1					;to index into a word width table
	mov	ax,SC_MAPMASK+0001b*256	;map mask for plane 0
	jmp	[ss:memtoscreentable+di]	;call the right routine

;==============
;
; Copy an even width block to an even video address
;
;==============

eventoeven:
	mov	dx,SC_INDEX
	WORDOUT

	mov	di,[dest]				;start at same place in all planes
	mov	dx,[height]				;scan lines to draw

@@lineloopEE:
	mov	cx,[wide]
	rep	movsw

	add	di,bx

	dec	dx
	jnz	@@lineloopEE

	shl	ah,1					;shift plane mask over for next plane
	cmp	ah,10000b				;done all four planes?
	jne	eventoeven

	mov	ax,ss
	mov	ds,ax					;restore turbo's data segment

	ret

;==============
;
; Copy an odd width block to an even video address
;
;==============

oddtoeven:
	mov	dx,SC_INDEX
	WORDOUT

	mov	di,[dest]				;start at same place in all planes
	mov	dx,[height]				;scan lines to draw

@@lineloopOE:
	mov	cx,[wide]
	rep	movsw
	movsb						;copy the last byte

	add	di,bx

	dec	dx
	jnz	@@lineloopOE

	shl	ah,1					;shift plane mask over for next plane
	cmp	ah,10000b				;done all four planes?
	jne	oddtoeven

	mov	ax,ss
	mov	ds,ax					;restore turbo's data segment

	ret

;==============
;
; Copy an even width block to an odd video address
;
;==============

eventoodd:
	dec	[wide]					;one word has to be handled seperately
EOplaneloop:
	mov	dx,SC_INDEX
	WORDOUT

	mov	di,[dest]				;start at same place in all planes
	mov	dx,[height]				;scan lines to draw

@@lineloopEO:
	movsb
	mov	cx,[wide]
	rep	movsw
	movsb

	add	di,bx

	dec	dx
	jnz	@@lineloopEO

	shl	ah,1					;shift plane mask over for next plane
	cmp	ah,10000b				;done all four planes?
	jne	EOplaneloop

	mov	ax,ss
	mov	ds,ax					;restore turbo's data segment

	ret

;==============
;
; Copy an odd width block to an odd video address
;
;==============

oddtoodd:
	mov	dx,SC_INDEX
	WORDOUT

	mov	di,[dest]				;start at same place in all planes
	mov	dx,[height]				;scan lines to draw

@@lineloopOO:
	movsb
	mov	cx,[wide]
	rep	movsw

	add	di,bx

	dec	dx
	jnz	@@lineloopOO

	shl	ah,1					;shift plane mask over for next plane
	cmp	ah,10000b				;done all four planes?
	jne	oddtoodd

	mov	ax,ss
	mov	ds,ax					;restore turbo's data segment

	ret


ENDP

;===========================================================================
;
; VW_ScreenToMem
;
; Copies a block of video memory to main memory, in order from planes 0-3.
; This could be optimized along the lines of VW_MemToScreen to take advantage
; of word copies, but this is an infrequently called routine.
;
;===========================================================================

PROC	VW_ScreenToMem	source:WORD, dest:WORD, wide:WORD, height:WORD
PUBLIC	VW_ScreenToMem
USES	SI,DI

	mov	es,[dest]

	mov	bx,[linewidth]
	sub	bx,[wide]

	mov	ds,[screenseg]

	mov	ax,GC_READMAP			;read map for plane 0

	xor	di,di

@@planeloop:
	mov	dx,GC_INDEX
	WORDOUT

	mov	si,[source]				;start at same place in all planes
	mov	dx,[height]				;scan lines to draw

@@lineloop:
	mov	cx,[wide]
	rep	movsb

	add	si,bx

	dec	dx
	jnz	@@lineloop

	inc	ah
	cmp	ah,4					;done all four planes?
	jne	@@planeloop

	mov	ax,ss
	mov	ds,ax					;restore turbo's data segment

	ret

ENDP


;============================================================================
;
; VWL_UpdateScreenBlocks
;
; Scans through the update matrix and copies any areas that have changed
; to the visable screen, then zeros the update array
;
;============================================================================



; AX	0/1 for scasb, temp for segment register transfers
; BX    width for block copies
; CX	REP counter
; DX	line width deltas
; SI	source for copies
; DI	scas dest / movsb dest
; BP	pointer to end of bufferblocks

PROC	VWL_UpdateScreenBlocks
PUBLIC	VWL_UpdateScreenBlocks
USES	SI,DI,BP

	jmp	SHORT @@realstart
@@done:
;
; all tiles have been scanned
;
	mov	dx,GC_INDEX				; restore write mode 0
	mov	ax,GC_MODE+0*256
	WORDOUT

	xor	ax,ax					; clear out the update matrix
	mov	cx,UPDATEWIDE*UPDATEHIGH/2

	mov	di,[updateptr]
	rep	stosw

	ret

@@realstart:
	mov	dx,SC_INDEX
	mov	ax,SC_MAPMASK+15*256
	WORDOUT
	mov	dx,GC_INDEX
	mov	ax,GC_MODE+1*256
	WORDOUT

	mov	di,[updateptr]			; start of floating update screen
	mov	bp,di
	add	bp,UPDATEWIDE*UPDATEHIGH+1 ; when di = bp, all tiles have been scanned

	push	di
	mov	cx,-1					; definately scan the entire thing

;
; scan for a 1 in the update list, meaning a tile needs to be copied
; from the master screen to the current screen
;
@@findtile:
	pop	di						; place to continue scaning from
	mov	ax,ss
	mov	es,ax					; search in the data segment
	mov	ds,ax
	mov al,1
	repne	scasb
	cmp	di,bp
	jae	@@done

	cmp	[BYTE di],al
	jne	@@singletile
	jmp	@@tileblock

;============
;
; copy a single tile
;
;============
@@singletile:
	inc	di						; we know the next tile is nothing
	push	di					; save off the spot being scanned
	sub	di,[updateptr]
	shl	di,1
	mov	di,[blockstarts-4+di]	; start of tile location on screen
	mov	si,di
	add	si,[bufferofs]
	add	di,[displayofs]

	mov	dx,[linewidth]
	sub	dx,2
	mov	ax,[screenseg]
	mov	ds,ax
	mov	es,ax

REPT	15
	movsb
	movsb
	add	si,dx
	add	di,dx
ENDM
	movsb
	movsb

	jmp	@@findtile

;============
;
; more than one tile in a row needs to be updated, so do it as a group
;
;============
EVEN
@@tileblock:
	mov	dx,di					; hold starting position + 1 in dx
	inc	di						; we know the next tile also gets updated
	repe	scasb				; see how many more in a row
	push	di					; save off the spot being scanned

	mov	bx,di
	sub	bx,dx					; number of tiles in a row
	shl	bx,1					; number of bytes / row

	mov	di,dx					; lookup position of start tile
	sub	di,[updateptr]
	shl	di,1
	mov	di,[blockstarts-2+di]	; start of tile location
	mov	si,di
	add	si,[bufferofs]
	add	di,[displayofs]

	mov	dx,[linewidth]
	sub	dx,bx					; offset to next line on screen

	mov	ax,[screenseg]
	mov	ds,ax
	mov	es,ax

REPT	15
	mov	cx,bx
	rep	movsb
	add	si,dx
	add	di,dx
ENDM
	mov	cx,bx
	rep	movsb

	dec	cx						; was 0 from last rep movsb, now $ffff for scasb
	jmp	@@findtile

ENDP


;===========================================================================
;
;                    MISC EGA ROUTINES
;
;===========================================================================

;==============
;
; VW_SetScreen
;
;==============

PROC	VW_SetScreen  crtc:WORD, pel:WORD
PUBLIC	VW_SetScreen

if waitforvbl

	mov	dx,STATUS_REGISTER_1

;
; wait util the CRTC just starts scaning a diplayed line to set the CRTC start
;
	cli

@@waitnodisplay:
	in	al,dx
	test	al,01b
	jz	@@waitnodisplay

@@waitdisplay:
	in	al,dx
	test	al,01b
	jnz	@@waitdisplay

endif

;
; set CRTC start
;
; for some reason, my XT's EGA card doesn't like word outs to the CRTC
; index...
;
	mov	cx,[crtc]
	mov	dx,CRTC_INDEX
	mov	al,0ch		;start address high register
	out	dx,al
	inc	dx
	mov	al,ch
	out	dx,al
	dec	dx
	mov	al,0dh		;start address low register
	out	dx,al
	mov	al,cl
	inc	dx
	out	dx,al

if waitforvbl

;
; wait for a vertical retrace to set pel panning
;
	mov	dx,STATUS_REGISTER_1
@@waitvbl:
	sti     		;service interrupts
	jmp	$+2
	cli
	in	al,dx
	test	al,00001000b	;look for vertical retrace
	jz	@@waitvbl

endif

;
; set horizontal panning
;
	mov	dx,ATR_INDEX
	mov	al,ATR_PELPAN or 20h
	out	dx,al
	jmp	$+2
	mov	al,[BYTE pel]		;pel pan value
	out	dx,al

	sti

	ret

ENDP


if NUMFONT+NUMFONTM

;===========================================================================
;
; GENERAL FONT DRAWING ROUTINES
;
;===========================================================================

DATASEG

px	dw	?					; proportional character drawing coordinates
py	dw	?
pdrawmode	db	11000b		; 8 = OR, 24 = XOR, put in GC_DATAROTATE
fontcolor	db	15		;0-15 mapmask value

PUBLIC	px,py,pdrawmode,fontcolor

;
; offsets in font structure
;
pcharheight	=	0		;lines high
charloc		=	2		;pointers to every character
charwidth	=	514		;every character's width in pixels


propchar	dw	?			; the character number to shift
stringptr	dw	?,?


BUFFWIDTH	=	50
BUFFHEIGHT	=   32			; must be twice as high as font for masked fonts

databuffer	db	BUFFWIDTH*BUFFHEIGHT dup (?)

bufferwidth	dw	?						; bytes with valid info / line
bufferheight dw	?						; number of lines currently used
PUBLIC	bufferwidth,bufferheight

bufferbyte	dw	?
bufferbit	dw	?

screenspot	dw	?						; where the buffer is going

bufferextra	dw	?						; add at end of a line copy
screenextra	dw	?

CODESEG

;======================
;
; Macros to table shift a byte of font
;
;======================

MACRO	SHIFTNOXOR
	mov	al,[es:bx]		; source
	xor	ah,ah
	shl	ax,1
	mov	si,ax
	mov	ax,[bp+si]		; table shift into two bytes
	or	[di],al			; or with first byte
	inc	di
	mov	[di],ah			; replace next byte
	inc	bx				; next source byte
ENDM

MACRO	SHIFTWITHXOR
	mov	al,[es:bx]		; source
	xor	ah,ah
	shl	ax,1
	mov	si,ax
	mov	ax,[bp+si]		; table shift into two bytes
	not	ax
	and	[di],al			; and with first byte
	inc	di
	mov	[di],ah			; replace next byte
	inc	bx				; next source byte
ENDM


;=======================
;
; BufferToScreen
;
; Pass buffer start in SI (somewhere in databuffer)
; Draws the buffer to the EGA screen in the current write mode
;
;========================

PROC	BufferToScreen	NEAR

	mov	es,[screenseg]
	mov	di,[screenspot]

	mov	bx,[bufferwidth]		;calculate offsets for end of each line
	or	bx,bx
	jnz	@@isthere
	ret							;nothing to draw

@@isthere:
	mov	ax,[linewidth]
	sub	ax,bx
	mov	[screenextra],ax
	mov	ax,BUFFWIDTH
	sub	ax,bx
	mov	[bufferextra],ax

	mov	bx,[bufferheight]		;lines to copy
@@lineloop:
	mov	cx,[bufferwidth]		;bytes to copy
@@byteloop:
	lodsb						;get a byte from the buffer
	xchg	[es:di],al			;load latches and store back to screen
	inc	di

	loop	@@byteloop

	add	si,[bufferextra]
	add	di,[screenextra]

	dec	bx
	jnz	@@lineloop

	ret
ENDP


;============================================================================
;
; NON MASKED FONT DRAWING ROUTINES
;
;============================================================================

if numfont

DATASEG

shiftdrawtable	dw      0,shift1wide,shift2wide,shift3wide

CODESEG

;==================
;
; ShiftPropChar
;
; Call with BX = character number (0-255)
; Draws one character to the buffer at bufferbyte/bufferbit, and adjusts
; them to the new position
;
;==================

PROC	ShiftPropChar	NEAR

	mov	es,[grsegs+STARTFONT*2]	;segment of font to use

;
; find character location, width, and height
;
	mov	si,[es:charwidth+bx]
	and	si,0ffh					;SI hold width in pixels
	shl	bx,1
	mov	bx,[es:charloc+bx]		;BX holds pointer to character data

;
; look up which shift table to use, based on bufferbit
;
	mov	di,[bufferbit]
	shl	di,1
	mov	bp,[shifttabletable+di]	;BP holds pointer to shift table

	mov	di,OFFSET databuffer
	add	di,[bufferbyte]			;DI holds pointer to buffer

;
; advance position by character width
;
	mov	cx,[bufferbit]
	add	cx,si					;new bit position
	mov	ax,cx
	and	ax,7
	mov	[bufferbit],ax			;new bit position
	mov	ax,cx
	shr	ax,1
	shr	ax,1
	shr	ax,1
	add	[bufferbyte],ax			;new byte position

	add	si,7
	shr	si,1
	shr	si,1
	shr	si,1					;bytes the character is wide
	shl	si,1                    ;*2 to look up in shiftdrawtable

	mov	cx,[es:pcharheight]
	mov	dx,BUFFWIDTH
	jmp	[ss:shiftdrawtable+si]	;procedure to draw this width

;
; one byte character
;
shift1wide:
	dec	dx
EVEN
@@loop1:
	SHIFTNOXOR
	add	di,dx			; next line in buffer

	loop	@@loop1

	ret

;
; two byte character
;
shift2wide:
	dec	dx
	dec	dx
EVEN
@@loop2:
	SHIFTNOXOR
	SHIFTNOXOR
	add	di,dx			; next line in buffer

	loop	@@loop2

	ret

;
; three byte character
;
shift3wide:
	sub	dx,3
EVEN
@@loop3:
	SHIFTNOXOR
	SHIFTNOXOR
	SHIFTNOXOR
	add	di,dx			; next line in buffer

	loop	@@loop3

	ret



ENDP

;============================================================================

;==================
;
; VW_DrawPropString
;
; Draws a C string of characters at px/py and advances px
;
; Assumes write mode 0
;
;==================

CODESEG

PROC	VW_DrawPropString	string:DWORD
PUBLIC	VW_DrawPropString
USES	SI,DI

;
; proportional spaceing, which clears the buffer ahead of it, so only
; clear the first collumn
;
	mov	al,0
line	=	0
REPT	BUFFHEIGHT
	mov	[BYTE databuffer+BUFFWIDTH*line],al
line	=	line+1
ENDM

;
; shift the characters into the buffer
;
@@shiftchars:
	mov	ax,[px]
	and	ax,7
	mov	[bufferbit],ax
	mov	[bufferbyte],0

	mov	ax,[WORD string]
	mov	[stringptr],ax
	mov	ax,[WORD string+2]
	mov	[stringptr+2],ax

@@shiftone:
	mov	es,[stringptr+2]
	mov	bx,[stringptr]
	inc	[stringptr]
	mov	bx,[es:bx]
	xor	bh,bh
	or	bl,bl
	jz	@@allshifted
	call	ShiftPropChar
	jmp	@@shiftone

@@allshifted:
;
; calculate position to draw buffer on screen
;
	mov	bx,[py]
	shl	bx,1
	mov	di,[ylookup+bx]
	add	di,[bufferofs]
	add	di,[panadjust]

	mov	ax,[px]
	shr	ax,1
	shr	ax,1
	shr	ax,1		;x location in bytes
	add	di,ax
	mov	[screenspot],di

;
; advance px
;
	mov	ax,[bufferbyte]
	shl	ax,1
	shl	ax,1
	shl	ax,1
	or	ax,[bufferbit]
	add	[px],ax

;
; draw it
;

; set xor/or mode
	mov	dx,GC_INDEX
	mov	al,GC_DATAROTATE
	mov	ah,[pdrawmode]
	WORDOUT

; set mapmask to color
	mov	dx,SC_INDEX
	mov	al,SC_MAPMASK
	mov	ah,[fontcolor]
	WORDOUT

	mov	ax,[bufferbyte]
	test	[bufferbit],7
	jz	@@go
	inc	ax				;so the partial byte also gets drawn
@@go:
	mov	[bufferwidth],ax
	mov	es,[grsegs+STARTFONT*2]
	mov	ax,[es:pcharheight]
	mov	[bufferheight],ax

	mov	si,OFFSET databuffer
	call	BufferToScreen

; set copy mode
	mov	dx,GC_INDEX
	mov	ax,GC_DATAROTATE
	WORDOUT

; set mapmask to all
	mov	dx,SC_INDEX
	mov	ax,SC_MAPMASK + 15*256
	WORDOUT


	ret

ENDP

endif	;numfont

;============================================================================
;
; MASKED FONT DRAWING ROUTINES
;
;============================================================================

if	numfontm

DATASEG

mshiftdrawtable	dw      0,mshift1wide,mshift2wide,mshift3wide


CODESEG

;==================
;
; ShiftMPropChar
;
; Call with BX = character number (0-255)
; Draws one character to the buffer at bufferbyte/bufferbit, and adjusts
; them to the new position
;
;==================

PROC	ShiftMPropChar	NEAR

	mov	es,[grsegs+STARTFONTM*2]	;segment of font to use

;
; find character location, width, and height
;
	mov	si,[es:charwidth+bx]
	and	si,0ffh					;SI hold width in pixels
	shl	bx,1
	mov	bx,[es:charloc+bx]		;BX holds pointer to character data

;
; look up which shift table to use, based on bufferbit
;
	mov	di,[bufferbit]
	shl	di,1
	mov	bp,[shifttabletable+di]	;BP holds pointer to shift table

	mov	di,OFFSET databuffer
	add	di,[bufferbyte]			;DI holds pointer to buffer

	mov	cx,[bufferbit]
	add	cx,si					;new bit position
	mov	ax,cx
	and	ax,7
	mov	[bufferbit],ax			;new bit position
	mov	ax,cx
	shr	ax,1
	shr	ax,1
	shr	ax,1
	add	[bufferbyte],ax			;new byte position

	add	si,7
	shr	si,1
	shr	si,1
	shr	si,1					;bytes the character is wide
	shl	si,1                    ;*2 to look up in shiftdrawtable

	mov	cx,[es:pcharheight]
	mov	dx,BUFFWIDTH
	jmp	[ss:mshiftdrawtable+si]	;procedure to draw this width

;
; one byte character
;
mshift1wide:
	dec	dx

EVEN
@@loop1m:
	SHIFTWITHXOR
	add	di,dx			; next line in buffer

	loop	@@loop1m

	mov	cx,[es:pcharheight]

EVEN
@@loop1:
	SHIFTNOXOR
	add	di,dx			; next line in buffer
	loop	@@loop1

	ret

;
; two byte character
;
mshift2wide:
	dec	dx
	dec	dx
EVEN
@@loop2m:
	SHIFTWITHXOR
	SHIFTWITHXOR
	add	di,dx			; next line in buffer

	loop	@@loop2m

	mov	cx,[es:pcharheight]

EVEN
@@loop2:
	SHIFTNOXOR
	SHIFTNOXOR
	add	di,dx			; next line in buffer
	loop	@@loop2

	ret

;
; three byte character
;
mshift3wide:
	sub	dx,3
EVEN
@@loop3m:
	SHIFTWITHXOR
	SHIFTWITHXOR
	SHIFTWITHXOR
	add	di,dx			; next line in buffer

	loop	@@loop3m

	mov	cx,[es:pcharheight]

EVEN
@@loop3:
	SHIFTNOXOR
	SHIFTNOXOR
	SHIFTNOXOR
	add	di,dx			; next line in buffer
	loop	@@loop3

	ret


ENDP

;============================================================================

;==================
;
; VW_DrawMPropString
;
; Draws a C string of characters at px/py and advances px
;
; Assumes write mode 0
;
;==================



PROC	VW_DrawMPropString	string:DWORD
PUBLIC	VW_DrawMPropString
USES	SI,DI

;
; clear out the first byte of the buffer, the rest will automatically be
; cleared as characters are drawn into it
;
	mov	es,[grsegs+STARTFONTM*2]
	mov	dx,[es:pcharheight]
	mov	di,OFFSET databuffer
	mov	ax,ds
	mov	es,ax
	mov	bx,BUFFWIDTH-1

	mov	cx,dx
	mov	al,0ffh
@@maskfill:
	stosb				; fill the mask part with $ff
	add	di,bx
	loop	@@maskfill

	mov	cx,dx
	xor	al,al
@@datafill:
	stosb				; fill the data part with $0
	add	di,bx
	loop	@@datafill

;
; shift the characters into the buffer
;
	mov	ax,[px]
	and	ax,7
	mov	[bufferbit],ax
	mov	[bufferbyte],0

	mov	ax,[WORD string]
	mov	[stringptr],ax
	mov	ax,[WORD string+2]
	mov	[stringptr+2],ax

@@shiftone:
	mov	es,[stringptr+2]
	mov	bx,[stringptr]
	inc	[stringptr]
	mov	bx,[es:bx]
	xor	bh,bh
	or	bl,bl
	jz	@@allshifted
	call	ShiftMPropChar
	jmp	@@shiftone

@@allshifted:
;
; calculate position to draw buffer on screen
;
	mov	bx,[py]
	shl	bx,1
	mov	di,[ylookup+bx]
	add	di,[bufferofs]
	add	di,[panadjust]

	mov	ax,[px]
	shr	ax,1
	shr	ax,1
	shr	ax,1		;x location in bytes
	add	di,ax
	mov	[screenspot],di

;
; advance px
;
	mov	ax,[bufferbyte]
	shl	ax,1
	shl	ax,1
	shl	ax,1
	or	ax,[bufferbit]
	add	[px],ax

;
; draw it
;
	mov	ax,[bufferbyte]
	test	[bufferbit],7
	jz	@@go
	inc	ax				;so the partial byte also gets drawn
@@go:
	mov	[bufferwidth],ax
	mov	es,[grsegs+STARTFONTM*2]
	mov	ax,[es:pcharheight]
	mov	[bufferheight],ax

; set AND mode to punch out the mask
	mov	dx,GC_INDEX
	mov	ax,GC_DATAROTATE + 8*256
	WORDOUT

; set mapmask to all
	mov	dx,SC_INDEX
	mov	ax,SC_MAPMASK + 15*256
	WORDOUT

	mov	si,OFFSET databuffer
	call	BufferToScreen

; set OR mode to fill in the color
	mov	dx,GC_INDEX
	mov	ax,GC_DATAROTATE + 16*256
	WORDOUT

; set mapmask to color
	mov	dx,SC_INDEX
	mov	al,SC_MAPMASK
	mov	ah,[fontcolor]
	WORDOUT

	call	BufferToScreen		; SI is still in the right position in buffer

; set copy mode
	mov	dx,GC_INDEX
	mov	ax,GC_DATAROTATE
	WORDOUT

; set mapmask to all
	mov	dx,SC_INDEX
	mov	ax,SC_MAPMASK + 15*256
	WORDOUT


	ret

ENDP

endif		; if numfontm

endif		; if fonts
