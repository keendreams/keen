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

; ID_RF_A.ASM

IDEAL
MODEL	MEDIUM,C

INCLUDE	"ID_ASM.EQU"

CACHETILES	= 1		;enable master screen tile caching

;============================================================================

TILESWIDE	=	21
TILESHIGH	=	14

UPDATESIZE	=	(TILESWIDE+1)*TILESHIGH+1

DATASEG

EXTRN	screenseg:WORD
EXTRN	updateptr:WORD
EXTRN	updatestart:WORD
EXTRN	masterofs:WORD		;start of master tile port
EXTRN	bufferofs:WORD		;start of current buffer port
EXTRN	screenstart:WORD	;starts of three screens (0/1/master) in EGA mem
EXTRN	grsegs:WORD
EXTRN	mapsegs:WORD
EXTRN	originmap:WORD
EXTRN	updatemapofs:WORD
EXTRN	tilecache:WORD
EXTRN	tinf:WORD			;seg pointer to map header and tile info
EXTRN	blockstarts:WORD	;offsets from bufferofs for each update block

planemask	db	?
planenum	db	?

CODESEG

screenstartcs	dw	?		;in code segment for accesability




IFE GRMODE-CGAGR
;============================================================================
;
; CGA refresh routines
;
;============================================================================

TILEWIDTH	=	4

;=================
;
; RFL_NewTile
;
; Draws a composit two plane tile to the master screen and sets the update
; spot to 1 in both update pages, forcing the tile to be copied to the
; view pages the next two refreshes
;
; Called to draw newlly scrolled on strips and animating tiles
;
;=================

PROC	RFL_NewTile	updateoffset:WORD
PUBLIC	RFL_NewTile
USES	SI,DI

;
; mark both update lists at this spot
;
	mov	di,[updateoffset]

	mov	bx,[updateptr]			;start of update matrix
	mov	[BYTE bx+di],1

	mov	dx,SCREENWIDTH-TILEWIDTH		;add to get to start of next line

;
; set di to the location in screenseg to draw the tile
;
	shl	di,1
	mov	si,[updatemapofs+di]	;offset in map from origin
	add	si,[originmap]
	mov	di,[blockstarts+di]		;screen location for tile
	add	di,[masterofs]

;
; set BX to the foreground tile number and SI to the background number
; If either BX or SI = 0xFFFF, the tile does not need to be masked together
; as one of the planes totally eclipses the other
;
	mov	es,[mapsegs+2]			;foreground plane
	mov	bx,[es:si]
	mov	es,[mapsegs]			;background plane
	mov	si,[es:si]

	mov	es,[screenseg]

	or	bx,bx
	jz	@@singletile
	jmp	@@maskeddraw			;draw both together

;=============
;
; Draw single background tile from main memory
;
;=============

@@singletile:
	shl	si,1
	mov	ds,[grsegs+STARTTILE16*2+si]

	xor	si,si					;block is segment aligned

REPT	15
	movsw
	movsw
	add	di,dx
ENDM
	movsw
	movsw

	mov	ax,ss
	mov	ds,ax					;restore turbo's data segment
	ret


;=========
;
; Draw a masked tile combo
; Interupts are disabled and the stack segment is reassigned
;
;=========
@@maskeddraw:
	cli							; don't allow ints when SS is set
	shl	bx,1
	mov	ss,[grsegs+STARTTILE16M*2+bx]
	shl	si,1
	mov	ds,[grsegs+STARTTILE16*2+si]

	xor	si,si					;first word of tile data

REPT	16
	mov	ax,[si]					;background tile
	and	ax,[ss:si]				;mask
	or	ax,[ss:si+64]			;masked data
	stosw
	mov	ax,[si+2]				;background tile
	and	ax,[ss:si+2]			;mask
	or	ax,[ss:si+66]			;masked data
	stosw
	add	si,4
	add	di,dx
ENDM

	mov	ax,@DATA
	mov	ss,ax
	sti
	mov	ds,ax
	ret
ENDP

ENDIF



IFE GRMODE-EGAGR
;===========================================================================
;
; EGA refresh routines
;
;===========================================================================

TILEWIDTH	=	2

;=================
;
; RFL_NewTile
;
; Draws a composit two plane tile to the master screen and sets the update
; spot to 1 in both update pages, forcing the tile to be copied to the
; view pages the next two refreshes
;
; Called to draw newlly scrolled on strips and animating tiles
;
; Assumes write mode 0
;
;=================

PROC	RFL_NewTile	updateoffset:WORD
PUBLIC	RFL_NewTile
USES	SI,DI

;
; mark both update lists at this spot
;
	mov	di,[updateoffset]

	mov	bx,[updatestart]		;page 0 pointer
	mov	[BYTE bx+di],1
	mov	bx,[updatestart+2]		;page 1 pointer
	mov	[BYTE bx+di],1

;
; set screenstartcs to the location in screenseg to draw the tile
;
	shl	di,1
	mov	si,[updatemapofs+di]	;offset in map from origin
	add	si,[originmap]
	mov	di,[blockstarts+di]		;screen location for tile
	add	di,[masterofs]
	mov	[cs:screenstartcs],di

;
; set BX to the foreground tile number and SI to the background number
; If either BX or SI = 0xFFFF, the tile does not need to be masked together
; as one of the planes totally eclipses the other
;
	mov	es,[mapsegs+2]			;foreground plane
	mov	bx,[es:si]
	mov	es,[mapsegs]			;background plane
	mov	si,[es:si]

	mov	es,[screenseg]
	mov	dx,SC_INDEX				;for stepping through map mask planes

	or	bx,bx
	jz	@@singletile
	jmp	@@maskeddraw			;draw both together

;=========
;
; No foreground tile, so draw a single background tile.
; Use the master screen cache if possible
;
;=========
@@singletile:

	mov	bx,SCREENWIDTH-2		;add to get to start of next line
	shl	si,1

IFE CACHETILES
	jmp	@@singlemain
ENDIF

	mov	ax,[tilecache+si]
	or	ax,ax
	jz	@@singlemain
;=============
;
; Draw single tile from cache
;
;=============

	mov	si,ax

	mov	ax,SC_MAPMASK + 15*256	;all planes
	WORDOUT

	mov	dx,GC_INDEX
	mov	ax,GC_MODE + 1*256		;write mode 1
	WORDOUT

	mov	di,[cs:screenstartcs]
	mov	ds,[screenseg]

REPT	15
	movsb
	movsb
	add	si,bx
	add	di,bx
ENDM
	movsb
	movsb

	xor	ah,ah					;write mode 0
	WORDOUT

	mov	ax,ss
	mov	ds,ax					;restore turbo's data segment
	ret

;=============
;
; Draw single tile from main memory
;
;=============

@@singlemain:
	mov	ax,[cs:screenstartcs]
	mov	[tilecache+si],ax		;next time it can be drawn from here with latch
	mov	ds,[grsegs+STARTTILE16*2+si]

	xor	si,si					;block is segment aligned

	mov	ax,SC_MAPMASK+0001b*256	;map mask for plane 0

	mov	cx,4					;draw four planes
@@planeloop:
	mov	dx,SC_INDEX
	WORDOUT

	mov	di,[cs:screenstartcs]	;start at same place in all planes

REPT	15
	movsw
	add	di,bx
ENDM
	movsw

	shl	ah,1					;shift plane mask over for next plane
	loop	@@planeloop

	mov	ax,ss
	mov	ds,ax					;restore turbo's data segment
	ret


;=========
;
; Draw a masked tile combo
; Interupts are disabled and the stack segment is reassigned
;
;=========
@@maskeddraw:
	cli							; don't allow ints when SS is set
	shl	bx,1
	mov	ss,[grsegs+STARTTILE16M*2+bx]
	shl	si,1
	mov	ds,[grsegs+STARTTILE16*2+si]

	xor	si,si					;first word of tile data

	mov	ax,SC_MAPMASK+0001b*256	;map mask for plane 0

	mov	di,[cs:screenstartcs]
@@planeloopm:
	WORDOUT
tileofs		=	0
lineoffset	=	0
REPT	16
	mov	bx,[si+tileofs]			;background tile
	and	bx,[ss:tileofs]			;mask
	or	bx,[ss:si+tileofs+32]	;masked data
	mov	[es:di+lineoffset],bx
tileofs		=	tileofs + 2
lineoffset	=	lineoffset + SCREENWIDTH
ENDM
	add	si,32
	shl	ah,1					;shift plane mask over for next plane
	cmp	ah,10000b
	je	@@done					;drawn all four planes
	jmp	@@planeloopm

@@done:
	mov	ax,@DATA
	mov	ss,ax
	sti
	mov	ds,ax
	ret
ENDP

ENDIF

IFE GRMODE-VGAGR
;============================================================================
;
; VGA refresh routines
;
;============================================================================


ENDIF


;============================================================================
;
; reasonably common refresh routines
;
;============================================================================


;=================
;
; RFL_UpdateTiles
;
; Scans through the update matrix pointed to by updateptr, looking for 1s.
; A 1 represents a tile that needs to be copied from the master screen to the
; current screen (a new row or an animated tiled).  If more than one adjacent
; tile in a horizontal row needs to be copied, they will be copied as a group.
;
; Assumes write mode 1
;
;=================


; AX	0/1 for scasb, temp for segment register transfers
; BX    width for block copies
; CX	REP counter
; DX	line width deltas
; SI	source for copies
; DI	scas dest / movsb dest
; BP	pointer to UPDATETERMINATE
;
; DS
; ES
; SS

PROC	RFL_UpdateTiles
PUBLIC	RFL_UpdateTiles
USES	SI,DI,BP

	jmp	SHORT @@realstart
@@done:
;
; all tiles have been scanned
;
	ret

@@realstart:
	mov	di,[updateptr]
	mov	bp,(TILESWIDE+1)*TILESHIGH+1
	add	bp,di					; when di = bx, all tiles have been scanned
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
	je	@@done

	cmp	[BYTE di],al
	jne	@@singletile
	jmp	@@tileblock

;============
;
; copy a single tile
;
;============
EVEN
@@singletile:
	inc	di						; we know the next tile is nothing
	push	di					; save off the spot being scanned
	sub	di,[updateptr]
	shl	di,1
	mov	di,[blockstarts-4+di]	; start of tile location on screen
	mov	si,di
	add	di,[bufferofs]			; dest in current screen
	add	si,[masterofs]			; source in master screen

	mov	dx,SCREENWIDTH-TILEWIDTH
	mov	ax,[screenseg]
	mov	ds,ax
	mov	es,ax

;--------------------------

IFE GRMODE-CGAGR

REPT	15
	movsw
	movsw
	add	si,dx
	add	di,dx
ENDM
	movsw
	movsw

ENDIF

;--------------------------

IFE GRMODE-EGAGR

REPT	15
	movsb
	movsb
	add	si,dx
	add	di,dx
ENDM
	movsb
	movsb

ENDIF

;--------------------------

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
	add	di,[bufferofs]			; dest in current screen
	add	si,[masterofs]			; source in master screen

	mov	dx,SCREENWIDTH
	sub	dx,bx					; offset to next line on screen
IFE GRMODE-CGAGR
	sub	dx,bx					; bx is words wide in CGA tiles
ENDIF

	mov	ax,[screenseg]
	mov	ds,ax
	mov	es,ax

REPT	15
	mov	cx,bx
IFE GRMODE-CGAGR
	rep	movsw
ENDIF
IFE GRMODE-EGAGR
	rep	movsb
ENDIF
	add	si,dx
	add	di,dx
ENDM
	mov	cx,bx
IFE GRMODE-CGAGR
	rep	movsw
ENDIF
IFE GRMODE-EGAGR
	rep	movsb
ENDIF

	dec	cx						; was 0 from last rep movsb, now $ffff for scasb
	jmp	@@findtile

ENDP


;============================================================================


;=================
;
; RFL_MaskForegroundTiles
;
; Scan through update looking for 3's.  If the foreground tile there is a
; masked foreground tile, draw it to the screen
;
;=================

PROC	RFL_MaskForegroundTiles
PUBLIC	RFL_MaskForegroundTiles
USES	SI,DI,BP
	jmp	SHORT @@realstart
@@done:
;
; all tiles have been scanned
;
	ret

@@realstart:
	mov	di,[updateptr]
	mov	bp,(TILESWIDE+1)*TILESHIGH+2
	add	bp,di					; when di = bx, all tiles have been scanned
	push	di
	mov	cx,-1					; definately scan the entire thing
;
; scan for a 3 in the update list
;
@@findtile:
	mov	ax,ss
	mov	es,ax					; scan in the data segment
	mov	al,3
	pop	di						; place to continue scaning from
	repne	scasb
	cmp	di,bp
	je	@@done

;============
;
; found a tile, see if it needs to be masked on
;
;============

	push	di

	sub	di,[updateptr]
	shl	di,1
	mov	si,[updatemapofs-2+di]	; offset from originmap
	add	si,[originmap]

	mov	es,[mapsegs+2]			; foreground map plane segment
	mov	si,[es:si]				; foreground tile number

	or	si,si
	jz	@@findtile				; 0 = no foreground tile

	mov	bx,si
	add	bx,INTILE				;INTILE tile info table
	mov	es,[tinf]
	test	[BYTE PTR es:bx],80h		;high bit = masked tile
	jz	@@findtile

;-------------------

IFE GRMODE-CGAGR
;=================
;
; mask the tile CGA
;
;=================

	mov	di,[blockstarts-2+di]
	add	di,[bufferofs]
	mov	es,[screenseg]
	shl	si,1
	mov	ds,[grsegs+STARTTILE16M*2+si]

	mov	bx,64					;data starts 64 bytes after mask

	xor	si,si

lineoffset	=	0
REPT	16
	mov	ax,[es:di+lineoffset]	;background
	and	ax,[si]					;mask
	or	ax,[si+bx]				;masked data
	mov	[es:di+lineoffset],ax	;background
	inc	si
	inc	si
	mov	ax,[es:di+lineoffset+2]	;background
	and	ax,[si]					;mask
	or	ax,[si+bx]				;masked data
	mov	[es:di+lineoffset+2],ax	;background
	inc	si
	inc	si
lineoffset	=	lineoffset + SCREENWIDTH
ENDM
ENDIF

;-------------------

IFE GRMODE-EGAGR
;=================
;
; mask the tile
;
;=================

	mov	[BYTE planemask],1
	mov	[BYTE planenum],0

	mov	di,[blockstarts-2+di]
	add	di,[bufferofs]
	mov	[cs:screenstartcs],di
	mov	es,[screenseg]
	shl	si,1
	mov	ds,[grsegs+STARTTILE16M*2+si]

	mov	bx,32					;data starts 32 bytes after mask

@@planeloopm:
	mov	dx,SC_INDEX
	mov	al,SC_MAPMASK
	mov	ah,[ss:planemask]
	WORDOUT
	mov	dx,GC_INDEX
	mov	al,GC_READMAP
	mov	ah,[ss:planenum]
	WORDOUT

	xor	si,si
	mov	di,[cs:screenstartcs]
lineoffset	=	0
REPT	16
	mov	cx,[es:di+lineoffset]	;background
	and	cx,[si]					;mask
	or	cx,[si+bx]				;masked data
	inc	si
	inc	si
	mov	[es:di+lineoffset],cx
lineoffset	=	lineoffset + SCREENWIDTH
ENDM
	add	bx,32					;the mask is now further away
	inc	[ss:planenum]
	shl	[ss:planemask],1		;shift plane mask over for next plane
	cmp	[ss:planemask],10000b	;done all four planes?
	je	@@drawn					;drawn all four planes
	jmp	@@planeloopm

@@drawn:
ENDIF

;-------------------

	mov	ax,ss
	mov	ds,ax
	mov	cx,-1					;definately scan the entire thing

	jmp	@@findtile

ENDP


END

