/* Keen Dreams Source Code
 * Copyright (C) 2014 Javier M. Chavez
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

// ID_VW.C

#include "ID_HEADS.H"

/*
=============================================================================

						 LOCAL CONSTANTS

=============================================================================
*/

#define VIEWWIDTH		40

#define PIXTOBLOCK		4		// 16 pixels to an update block

#if GRMODE == EGAGR
#define SCREENXMASK		(~7)
#define SCREENXPLUS		(7)
#define SCREENXDIV		(8)
#endif

#if GRMODE == CGAGR
#define SCREENXMASK		(~3)
#define SCREENXDIV		(4)
#endif
/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/

cardtype	videocard;		// set by VW_Startup
grtype		grmode;			// CGAgr, EGAgr, VGAgr

unsigned	bufferofs;		// hidden area to draw to before displaying
unsigned	displayofs;		// origin of the visable screen
unsigned	panx,pany;		// panning adjustments inside port in pixels
unsigned	pansx,pansy;	// panning adjustments inside port in screen
							// block limited pixel values (ie 0/8 for ega x)
unsigned	panadjust;		// panx/pany adjusted by screen resolution

unsigned	screenseg;		// normally 0xa000 / 0xb800
unsigned	linewidth;
unsigned	ylookup[VIRTUALHEIGHT];

boolean		screenfaded;

pictabletype	_seg *pictable;
pictabletype	_seg *picmtable;
spritetabletype _seg *spritetable;

/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/

void	VWL_MeasureString (char far *string, word *width, word *height,
		fontstruct _seg *font);
void 	VWL_DrawCursor (void);
void 	VWL_EraseCursor (void);
void 	VWL_DBSetup (void);
void	VWL_UpdateScreenBlocks (void);


int			bordercolor;
int			cursorvisible;
int			cursornumber,cursorwidth,cursorheight,cursorx,cursory;
memptr		cursorsave;
unsigned	cursorspot;

extern	unsigned	bufferwidth,bufferheight;	// used by font drawing stuff

//===========================================================================


/*
=======================
=
= VW_Startup
=
=======================
*/

static	char *ParmStrings[] = {"HIDDENCARD",""};

void	VW_Startup (void)
{
	int i;

	asm	cld;

	videocard = 0;

	for (i = 1;i < _argc;i++)
		if (US_CheckParm(_argv[i],ParmStrings) == 0)
		{
			videocard = EGAcard;
			break;
		}

	if (!videocard)
		videocard = VW_VideoID ();

#if GRMODE == EGAGR
	grmode = EGAGR;
	if (videocard != EGAcard && videocard != VGAcard)
Quit ("Improper video card!  If you really have an EGA/VGA card that I am not \n"
	  "detecting, use the -HIDDENCARD command line parameter!");
	EGAWRITEMODE(0);
#endif

#if GRMODE == CGAGR
	grmode = CGAGR;
	if (videocard < CGAcard || videocard > VGAcard)
Quit ("Improper video card!  If you really have a CGA card that I am not \n"
	  "detecting, use the -HIDDENCARD command line parameter!");
	MM_GetPtr (&(memptr)screenseg,0x10000l);	// grab 64k for floating screen
#endif

	cursorvisible = 0;
}

//===========================================================================

/*
=======================
=
= VW_Shutdown
=
=======================
*/

void	VW_Shutdown (void)
{
	VW_SetScreenMode (TEXTGR);
#if GRMODE == EGAGR
	VW_SetLineWidth (80);
#endif
}

//===========================================================================

/*
========================
=
= VW_SetScreenMode
= Call BIOS to set TEXT / CGAgr / EGAgr / VGAgr
=
========================
*/

void VW_SetScreenMode (int grmode)
{
	switch (grmode)
	{
	  case TEXTGR:  _AX = 3;
		  geninterrupt (0x10);
		  screenseg=0xb000;
		  break;
	  case CGAGR: _AX = 4;
		  geninterrupt (0x10);		// screenseg is actually a main mem buffer
		  break;
	  case EGAGR: _AX = 0xd;
		  geninterrupt (0x10);
		  screenseg=0xa000;
		  break;
#ifdef VGAGAME
	  case VGAGR:{
		  char extern VGAPAL;	// deluxepaint vga pallet .OBJ file
		  void far *vgapal = &VGAPAL;
		  SetCool256 ();		// custom 256 color mode
		  screenseg=0xa000;
		  _ES = FP_SEG(vgapal);
		  _DX = FP_OFF(vgapal);
		  _BX = 0;
		  _CX = 0x100;
		  _AX = 0x1012;
		  geninterrupt(0x10);			// set the deluxepaint pallet

		  break;
#endif
	}
	VW_SetLineWidth(SCREENWIDTH);
}

/*
=============================================================================

							SCREEN FADES

=============================================================================
*/

char colors[7][17]=
{{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {0,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,0},
 {0,0,0,0,0,0,0,0,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0},
 {0,1,2,3,4,5,6,7,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0},
 {0,1,2,3,4,5,6,7,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0},
 {0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f}};


void VW_ColorBorder (int color)
{
	_AH=0x10;
	_AL=1;
	_BH=color;
	geninterrupt (0x10);
	bordercolor = color;
}

void VW_SetDefaultColors(void)
{
#if GRMODE == EGAGR
	colors[3][16] = bordercolor;
	_ES=FP_SEG(&colors[3]);
	_DX=FP_OFF(&colors[3]);
	_AX=0x1002;
	geninterrupt(0x10);
	screenfaded = false;
#endif
}


void VW_FadeOut(void)
{
#if GRMODE == EGAGR
	int i;

	for (i=3;i>=0;i--)
	{
	  colors[i][16] = bordercolor;
	  _ES=FP_SEG(&colors[i]);
	  _DX=FP_OFF(&colors[i]);
	  _AX=0x1002;
	  geninterrupt(0x10);
	  VW_WaitVBL(6);
	}
	screenfaded = true;
#endif
}


void VW_FadeIn(void)
{
#if GRMODE == EGAGR
	int i;

	for (i=0;i<4;i++)
	{
	  colors[i][16] = bordercolor;
	  _ES=FP_SEG(&colors[i]);
	  _DX=FP_OFF(&colors[i]);
	  _AX=0x1002;
	  geninterrupt(0x10);
	  VW_WaitVBL(6);
	}
	screenfaded = false;
#endif
}

void VW_FadeUp(void)
{
#if GRMODE == EGAGR
	int i;

	for (i=3;i<6;i++)
	{
	  colors[i][16] = bordercolor;
	  _ES=FP_SEG(&colors[i]);
	  _DX=FP_OFF(&colors[i]);
	  _AX=0x1002;
	  geninterrupt(0x10);
	  VW_WaitVBL(6);
	}
	screenfaded = true;
#endif
}

void VW_FadeDown(void)
{
#if GRMODE == EGAGR
	int i;

	for (i=5;i>2;i--)
	{
	  colors[i][16] = bordercolor;
	  _ES=FP_SEG(&colors[i]);
	  _DX=FP_OFF(&colors[i]);
	  _AX=0x1002;
	  geninterrupt(0x10);
	  VW_WaitVBL(6);
	}
	screenfaded = false;
#endif
}



//===========================================================================

/*
====================
=
= VW_SetLineWidth
=
= Must be an even number of bytes
=
====================
*/

void VW_SetLineWidth (int width)
{
  int i,offset;

#if GRMODE == EGAGR
//
// set wide virtual screen
//
asm	mov	dx,CRTC_INDEX
asm	mov	al,CRTC_OFFSET
asm mov	ah,[BYTE PTR width]
asm	shr	ah,1
asm	out	dx,ax
#endif

//
// set up lookup tables
//
  linewidth = width;

  offset = 0;

  for (i=0;i<VIRTUALHEIGHT;i++)
  {
	ylookup[i]=offset;
	offset += width;
  }
}

//===========================================================================

/*
====================
=
= VW_ClearVideo
=
====================
*/

void	VW_ClearVideo (int color)
{
#if GRMODE == EGAGR
	EGAWRITEMODE(2);
	EGAMAPMASK(15);
#endif

asm	mov	es,[screenseg]
asm	xor	di,di
asm	mov	cx,0xffff
asm	mov	al,[BYTE PTR color]
asm	rep	stosb
asm	stosb

#if GRMODE == EGAGR
	EGAWRITEMODE(0);
#endif
}

//===========================================================================

#if NUMPICS>0

/*
====================
=
= VW_DrawPic
=
= X in bytes, y in pixels, chunknum is the #defined picnum
=
====================
*/

void VW_DrawPic(unsigned x, unsigned y, unsigned chunknum)
{
	int	picnum = chunknum - STARTPICS;
	memptr source;
	unsigned dest,width,height;

	source = grsegs[chunknum];
	dest = ylookup[y]+x+bufferofs;
	width = pictable[picnum].width;
	height = pictable[picnum].height;

	VW_MemToScreen(source,dest,width,height);
}

#endif

#if NUMPICM>0

/*
====================
=
= VW_DrawMPic
=
= X in bytes, y in pixels, chunknum is the #defined picnum
=
====================
*/

void VW_DrawMPic(unsigned x, unsigned y, unsigned chunknum)
{
	int	picnum = chunknum - STARTPICM;
	memptr source;
	unsigned dest,width,height;

	source = grsegs[chunknum];
	dest = ylookup[y]+x+bufferofs;
	width = pictable[picnum].width;
	height = pictable[picnum].height;

	VW_MaskBlock(source,0,dest,width,height,width*height);
}

#endif

//===========================================================================

#if NUMSPRITES>0

/*
====================
=
= VW_DrawSprite
=
= X and Y in pixels, it will match the closest shift possible
=
= To do:
= Add vertical clipping!
= Make the shifts act as center points, rather than break points
=
====================
*/

void VW_DrawSprite(int x, int y, unsigned chunknum)
{
	spritetabletype far *spr;
	spritetype _seg	*block;
	unsigned	dest,shift;

	spr = &spritetable[chunknum-STARTSPRITES];
	block = (spritetype _seg *)grsegs[chunknum];

	y+=spr->orgy>>G_P_SHIFT;
	x+=spr->orgx>>G_P_SHIFT;

#if GRMODE == EGAGR
	shift = (x&7)/2;
#endif
#if GRMODE == CGAGR
	shift = 0;
#endif

	dest = bufferofs + ylookup[y];
	if (x>=0)
		dest += x/SCREENXDIV;
	else
		dest += (x+1)/SCREENXDIV;

	VW_MaskBlock (block,block->sourceoffset[shift],dest,
		block->width[shift],spr->height,block->planesize[shift]);
}

#endif


/*
==================
=
= VW_Hlin
=
==================
*/


#if GRMODE == EGAGR

unsigned char leftmask[8] = {0xff,0x7f,0x3f,0x1f,0xf,7,3,1};
unsigned char rightmask[8] = {0x80,0xc0,0xe0,0xf0,0xf8,0xfc,0xfe,0xff};

void VW_Hlin(unsigned xl, unsigned xh, unsigned y, unsigned color)
{
  unsigned dest,xlb,xhb,maskleft,maskright,mid;

	xlb=xl/8;
	xhb=xh/8;

	EGAWRITEMODE(2);
	EGAMAPMASK(15);

	maskleft = leftmask[xl&7];
	maskright = rightmask[xh&7];

	mid = xhb-xlb-1;
	dest = bufferofs+ylookup[y]+xlb;

  if (xlb==xhb)
  {
  //
  // entire line is in one byte
  //

	maskleft&=maskright;

	asm	mov	es,[screenseg]
	asm	mov	di,[dest]

	asm	mov	dx,GC_INDEX
	asm	mov	al,GC_BITMASK
	asm	mov	ah,[BYTE PTR maskleft]
	asm	out	dx,ax		// mask off pixels

	asm	mov	al,[BYTE PTR color]
	asm	xchg	al,[es:di]	// load latches and write pixels

	goto	done;
  }

asm	mov	es,[screenseg]
asm	mov	di,[dest]
asm	mov	dx,GC_INDEX
asm	mov	bh,[BYTE PTR color]

//
// draw left side
//
asm	mov	al,GC_BITMASK
asm	mov	ah,[BYTE PTR maskleft]
asm	out	dx,ax		// mask off pixels

asm	mov	al,bh
asm	mov	bl,[es:di]	// load latches
asm	stosb

//
// draw middle
//
asm	mov	ax,GC_BITMASK + 255*256
asm	out	dx,ax		// no masking

asm	mov	al,bh
asm	mov	cx,[mid]
asm	rep	stosb

//
// draw right side
//
asm	mov	al,GC_BITMASK
asm	mov	ah,[BYTE PTR maskright]
asm	out	dx,ax		// mask off pixels

asm	xchg	bh,[es:di]	// load latches and write pixels

done:
	EGABITMASK(255);
	EGAWRITEMODE(0);
}
#endif


#if GRMODE == CGAGR

unsigned char pixmask[4] = {0xc0,0x30,0x0c,0x03};
unsigned char leftmask[4] = {0xff,0x3f,0x0f,0x03};
unsigned char rightmask[4] = {0xc0,0xf0,0xfc,0xff};
unsigned char colorbyte[4] = {0,0x55,0xaa,0xff};

//
// could be optimized for rep stosw
//
void VW_Hlin(unsigned xl, unsigned xh, unsigned y, unsigned color)
{
	unsigned dest,xlb,xhb,mid;
	byte maskleft,maskright;

	color = colorbyte[color];	// expand 2 color bits to 8

	xlb=xl/4;
	xhb=xh/4;

	maskleft = leftmask[xl&3];
	maskright = rightmask[xh&3];

	mid = xhb-xlb-1;
	dest = bufferofs+ylookup[y]+xlb;
asm	mov	es,[screenseg]

	if (xlb==xhb)
	{
	//
	// entire line is in one byte
	//
		maskleft&=maskright;

		asm	mov	ah,[maskleft]
		asm	mov	bl,[BYTE PTR color]
		asm	and	bl,[maskleft]
		asm	not	ah

		asm	mov	di,[dest]

		asm	mov	al,[es:di]
		asm	and	al,ah			// mask out pixels
		asm	or	al,bl			// or in color
		asm	mov	[es:di],al
		return;
	}

asm	mov	di,[dest]
asm	mov	bh,[BYTE PTR color]

//
// draw left side
//
asm	mov	ah,[maskleft]
asm	mov	bl,bh
asm	and	bl,[maskleft]
asm	not	ah
asm	mov	al,[es:di]
asm	and	al,ah			// mask out pixels
asm	or	al,bl			// or in color
asm	stosb

//
// draw middle
//
asm	mov	al,bh
asm	mov	cx,[mid]
asm	rep	stosb

//
// draw right side
//
asm	mov	ah,[maskright]
asm	mov	bl,bh
asm	and	bl,[maskright]
asm	not	ah
asm	mov	al,[es:di]
asm	and	al,ah			// mask out pixels
asm	or	al,bl			// or in color
asm	stosb
}
#endif


/*
==================
=
= VW_Bar
=
= Pixel addressable block fill routine
=
==================
*/

#if GRMODE == CGAGR

void VW_Bar (unsigned x, unsigned y, unsigned width, unsigned height,
	unsigned color)
{
	unsigned xh = x+width-1;

	while (height--)
		VW_Hlin (x,xh,y++,color);
}

#endif


#if	GRMODE == EGAGR

void VW_Bar (unsigned x, unsigned y, unsigned width, unsigned height,
	unsigned color)
{
	unsigned dest,xh,xlb,xhb,maskleft,maskright,mid;

	xh = x+width-1;
	xlb=x/8;
	xhb=xh/8;

	EGAWRITEMODE(2);
	EGAMAPMASK(15);

	maskleft = leftmask[x&7];
	maskright = rightmask[xh&7];

	mid = xhb-xlb-1;
	dest = bufferofs+ylookup[y]+xlb;

	if (xlb==xhb)
	{
	//
	// entire line is in one byte
	//

		maskleft&=maskright;

	asm	mov	es,[screenseg]
	asm	mov	di,[dest]

	asm	mov	dx,GC_INDEX
	asm	mov	al,GC_BITMASK
	asm	mov	ah,[BYTE PTR maskleft]
	asm	out	dx,ax		// mask off pixels

	asm	mov	ah,[BYTE PTR color]
	asm	mov	dx,[linewidth]
yloop1:
	asm	mov	al,ah
	asm	xchg	al,[es:di]	// load latches and write pixels
	asm	add	di,dx			// down to next line
	asm	dec	[height]
	asm	jnz	yloop1

		goto	done;
	}

asm	mov	es,[screenseg]
asm	mov	di,[dest]
asm	mov	bh,[BYTE PTR color]
asm	mov	dx,GC_INDEX
asm	mov	si,[linewidth]
asm	sub	si,[mid]			// add to di at end of line to get to next scan
asm	dec	si

//
// draw left side
//
yloop2:
asm	mov	al,GC_BITMASK
asm	mov	ah,[BYTE PTR maskleft]
asm	out	dx,ax		// mask off pixels

asm	mov	al,bh
asm	mov	bl,[es:di]	// load latches
asm	stosb

//
// draw middle
//
asm	mov	ax,GC_BITMASK + 255*256
asm	out	dx,ax		// no masking

asm	mov	al,bh
asm	mov	cx,[mid]
asm	rep	stosb

//
// draw right side
//
asm	mov	al,GC_BITMASK
asm	mov	ah,[BYTE PTR maskright]
asm	out	dx,ax		// mask off pixels

asm	mov	al,bh
asm	xchg	al,[es:di]	// load latches and write pixels

asm	add	di,si		// move to start of next line
asm	dec	[height]
asm	jnz	yloop2

done:
	EGABITMASK(255);
	EGAWRITEMODE(0);
}

#endif

//==========================================================================

/*
==================
=
= VW_MeasureString
=
==================
*/

#if NUMFONT+NUMFONTM>0
void
VWL_MeasureString (char far *string, word *width, word *height, fontstruct _seg *font)
{
	*height = font->height;
	for (*width = 0;*string;string++)
		*width += font->width[*string];		// proportional width
}

void	VW_MeasurePropString (char far *string, word *width, word *height)
{
	VWL_MeasureString(string,width,height,(fontstruct _seg *)grsegs[STARTFONT]);
}

void	VW_MeasureMPropString  (char far *string, word *width, word *height)
{
	VWL_MeasureString(string,width,height,(fontstruct _seg *)grsegs[STARTFONTM]);
}


#endif


/*
=============================================================================

							CGA stuff

=============================================================================
*/

#if GRMODE == CGAGR

#define CGACRTCWIDTH	40

/*
==========================
=
= VW_CGAFullUpdate
=
==========================
*/

void VW_CGAFullUpdate (void)
{
	byte	*update;
	boolean	halftile;
	unsigned	x,y,middlerows,middlecollumns;

	displayofs = bufferofs+panadjust;

asm	mov	ax,0xb800
asm	mov	es,ax

asm	mov	si,[displayofs]
asm	xor	di,di

asm	mov	bx,100				// pairs of scan lines to copy
asm	mov	dx,[linewidth]
asm	sub	dx,80

asm	mov	ds,[screenseg]
asm	test	si,1
asm	jz	evenblock

//
// odd source
//
asm	mov	ax,39				// words accross screen
copytwolineso:
asm	movsb
asm	mov	cx,ax
asm	rep	movsw
asm	movsb
asm	add	si,dx
asm	add	di,0x2000-80		// go to the interlaced bank
asm	movsb
asm	mov	cx,ax
asm	rep	movsw
asm	movsb
asm	add	si,dx
asm	sub	di,0x2000			// go to the non interlaced bank

asm	dec	bx
asm	jnz	copytwolineso
asm	jmp	blitdone

//
// even source
//
evenblock:
asm	mov	ax,40				// words accross screen
copytwolines:
asm	mov	cx,ax
asm	rep	movsw
asm	add	si,dx
asm	add	di,0x2000-80		// go to the interlaced bank
asm	mov	cx,ax
asm	rep	movsw
asm	add	si,dx
asm	sub	di,0x2000			// go to the non interlaced bank

asm	dec	bx
asm	jnz	copytwolines

blitdone:
asm	mov	ax,ss
asm	mov	ds,ax
asm	mov	es,ax

asm	xor	ax,ax				// clear out the update matrix
asm	mov	cx,UPDATEWIDE*UPDATEHIGH/2

asm	mov	di,[baseupdateptr]
asm	rep	stosw

	updateptr = baseupdateptr;
	*(unsigned *)(updateptr + UPDATEWIDE*PORTTILESHIGH) = UPDATETERMINATE;
}


#endif

/*
=============================================================================

					   CURSOR ROUTINES

These only work in the context of the double buffered update routines

=============================================================================
*/

/*
====================
=
= VWL_DrawCursor
=
= Background saves, then draws the cursor at cursorspot
=
====================
*/

void VWL_DrawCursor (void)
{
	cursorspot = bufferofs + ylookup[cursory+pansy]+(cursorx+pansx)/SCREENXDIV;
	VW_ScreenToMem(cursorspot,cursorsave,cursorwidth,cursorheight);
	VWB_DrawSprite(cursorx,cursory,cursornumber);
}


//==========================================================================


/*
====================
=
= VWL_EraseCursor
=
====================
*/

void VWL_EraseCursor (void)
{
	VW_MemToScreen(cursorsave,cursorspot,cursorwidth,cursorheight);
	VW_MarkUpdateBlock ((cursorx+pansx)&SCREENXMASK,cursory+pansy,
		( (cursorx+pansx)&SCREENXMASK)+cursorwidth*SCREENXDIV-1,
		cursory+pansy+cursorheight-1);
}


//==========================================================================


/*
====================
=
= VW_ShowCursor
=
====================
*/

void VW_ShowCursor (void)
{
	cursorvisible++;
}


//==========================================================================

/*
====================
=
= VW_HideCursor
=
====================
*/

void VW_HideCursor (void)
{
	cursorvisible--;
}

//==========================================================================

/*
====================
=
= VW_MoveCursor
=
====================
*/

void VW_MoveCursor (int x, int y)
{
	cursorx = x;
	cursory = y;
}

//==========================================================================

/*
====================
=
= VW_SetCursor
=
= Load in a sprite to be used as a cursor, and allocate background save space
=
====================
*/

void VW_SetCursor (int spritenum)
{
	if (cursornumber)
	{
		MM_SetLock (&grsegs[cursornumber],false);
		MM_FreePtr (&cursorsave);
	}

	cursornumber = spritenum;

	CA_CacheGrChunk (spritenum);
	MM_SetLock (&grsegs[cursornumber],true);

	cursorwidth = spritetable[spritenum-STARTSPRITES].width+1;
	cursorheight = spritetable[spritenum-STARTSPRITES].height;

	MM_GetPtr (&cursorsave,cursorwidth*cursorheight*5);
}


/*
=============================================================================

				Double buffer management routines

=============================================================================
*/

/*
======================
=
= VW_InitDoubleBuffer
=
======================
*/

void VW_InitDoubleBuffer (void)
{
#if GRMODE == EGAGR
	VW_SetScreen (displayofs+panadjust,0);			// no pel pan
#endif
}


/*
======================
=
= VW_FixRefreshBuffer
=
= Copies the view page to the buffer page on page flipped refreshes to
= avoid a one frame shear around pop up windows
=
======================
*/

void VW_FixRefreshBuffer (void)
{
#if GRMODE == EGAGR
	VW_ScreenToScreen (displayofs,bufferofs,PORTTILESWIDE*4*CHARWIDTH,
		PORTTILESHIGH*16);
#endif
}


/*
======================
=
= VW_QuitDoubleBuffer
=
======================
*/

void VW_QuitDoubleBuffer (void)
{
}


/*
=======================
=
= VW_MarkUpdateBlock
=
= Takes a pixel bounded block and marks the tiles in bufferblocks
= Returns 0 if the entire block is off the buffer screen
=
=======================
*/

int VW_MarkUpdateBlock (int x1, int y1, int x2, int y2)
{
	int	x,y,xt1,yt1,xt2,yt2,nextline;
	byte *mark;

	xt1 = x1>>PIXTOBLOCK;
	yt1 = y1>>PIXTOBLOCK;

	xt2 = x2>>PIXTOBLOCK;
	yt2 = y2>>PIXTOBLOCK;

	if (xt1<0)
		xt1=0;
	else if (xt1>=UPDATEWIDE-1)
		return 0;

	if (yt1<0)
		yt1=0;
	else if (yt1>UPDATEHIGH)
		return 0;

	if (xt2<0)
		return 0;
	else if (xt2>=UPDATEWIDE-1)
		xt2 = UPDATEWIDE-2;

	if (yt2<0)
		return 0;
	else if (yt2>=UPDATEHIGH)
		yt2 = UPDATEHIGH-1;

	mark = updateptr + uwidthtable[yt1] + xt1;
	nextline = UPDATEWIDE - (xt2-xt1) - 1;

	for (y=yt1;y<=yt2;y++)
	{
		for (x=xt1;x<=xt2;x++)
			*mark++ = 1;			// this tile will need to be updated

		mark += nextline;
	}

	return 1;
}


/*
===========================
=
= VW_UpdateScreen
=
= Updates any changed areas of the double buffer and displays the cursor
=
===========================
*/

void VW_UpdateScreen (void)
{
	if (cursorvisible>0)
		VWL_DrawCursor();

#if GRMODE == EGAGR
	VWL_UpdateScreenBlocks();

asm	cli
asm	mov	cx,[displayofs]
asm	add	cx,[panadjust]
asm	mov	dx,CRTC_INDEX
asm	mov	al,0ch		// start address high register
asm	out	dx,al
asm	inc	dx
asm	mov	al,ch
asm	out	dx,al
asm	dec	dx
asm	mov	al,0dh		// start address low register
asm	out	dx,al
asm	mov	al,cl
asm	inc	dx
asm	out	dx,al
asm	sti

#endif
#if GRMODE == CGAGR
	VW_CGAFullUpdate();
#endif

	if (cursorvisible>0)
		VWL_EraseCursor();
}



void VWB_DrawTile8 (int x, int y, int tile)
{
	x+=pansx;
	y+=pansy;
	if (VW_MarkUpdateBlock (x&SCREENXMASK,y,(x&SCREENXMASK)+7,y+7))
		VW_DrawTile8 (x/SCREENXDIV,y,tile);
}

void VWB_DrawTile8M (int x, int y, int tile)
{
	int xb;

	x+=pansx;
	y+=pansy;
	xb = x/SCREENXDIV; 			// use intermediate because VW_DT8M is macro
	if (VW_MarkUpdateBlock (x&SCREENXMASK,y,(x&SCREENXMASK)+7,y+7))
		VW_DrawTile8M (xb,y,tile);
}

void VWB_DrawTile16 (int x, int y, int tile)
{
	x+=pansx;
	y+=pansy;
	if (VW_MarkUpdateBlock (x&SCREENXMASK,y,(x&SCREENXMASK)+15,y+15))
		VW_DrawTile16 (x/SCREENXDIV,y,tile);
}

void VWB_DrawTile16M (int x, int y, int tile)
{
	int xb;

	x+=pansx;
	y+=pansy;
	xb = x/SCREENXDIV;		// use intermediate because VW_DT16M is macro
	if (VW_MarkUpdateBlock (x&SCREENXMASK,y,(x&SCREENXMASK)+15,y+15))
		VW_DrawTile16M (xb,y,tile);
}

#if NUMPICS
void VWB_DrawPic (int x, int y, int chunknum)
{
// mostly copied from drawpic
	int	picnum = chunknum - STARTPICS;
	memptr source;
	unsigned dest,width,height;

	x+=pansx;
	y+=pansy;
	x/= SCREENXDIV;

	source = grsegs[chunknum];
	dest = ylookup[y]+x+bufferofs;
	width = pictable[picnum].width;
	height = pictable[picnum].height;

	if (VW_MarkUpdateBlock (x*SCREENXDIV,y,(x+width)*SCREENXDIV-1,y+height-1))
		VW_MemToScreen(source,dest,width,height);
}
#endif

#if NUMPICM>0
void VWB_DrawMPic(int x, int y, int chunknum)
{
// mostly copied from drawmpic
	int	picnum = chunknum - STARTPICM;
	memptr source;
	unsigned dest,width,height;

	x+=pansx;
	y+=pansy;
	x/=SCREENXDIV;

	source = grsegs[chunknum];
	dest = ylookup[y]+x+bufferofs;
	width = picmtable[picnum].width;
	height = picmtable[picnum].height;

	if (VW_MarkUpdateBlock (x*SCREENXDIV,y,(x+width)*SCREENXDIV-1,y+height-1))
		VW_MaskBlock(source,0,dest,width,height,width*height);
}
#endif


void VWB_Bar (int x, int y, int width, int height, int color)
{
	x+=pansx;
	y+=pansy;
	if (VW_MarkUpdateBlock (x,y,x+width,y+height-1) )
		VW_Bar (x,y,width,height,color);
}


#if NUMFONT
void VWB_DrawPropString	 (char far *string)
{
	int x,y;
	x = px+pansx;
	y = py+pansy;
	VW_DrawPropString (string);
//	VW_MarkUpdateBlock(0,0,320,200);
	VW_MarkUpdateBlock(x,y,x+bufferwidth*8-1,y+bufferheight-1);
}
#endif


#if NUMFONTM
void VWB_DrawMPropString (char far *string)
{
	int x,y;
	x = px+pansx;
	y = py+pansy;
	VW_DrawMPropString (string);
	VW_MarkUpdateBlock(x,y,x+bufferwidth*8-1,y+bufferheight-1);
}
#endif

#if NUMSPRITES
void VWB_DrawSprite(int x, int y, int chunknum)
{
	spritetabletype far *spr;
	spritetype _seg	*block;
	unsigned	dest,shift,width,height;

	x+=pansx;
	y+=pansy;

	spr = &spritetable[chunknum-STARTSPRITES];
	block = (spritetype _seg *)grsegs[chunknum];

	y+=spr->orgy>>G_P_SHIFT;
	x+=spr->orgx>>G_P_SHIFT;


#if GRMODE == EGAGR
	shift = (x&7)/2;
#endif
#if GRMODE == CGAGR
	shift = 0;
#endif

	dest = bufferofs + ylookup[y];
	if (x>=0)
		dest += x/SCREENXDIV;
	else
		dest += (x+1)/SCREENXDIV;

	width = block->width[shift];
	height = spr->height;

	if (VW_MarkUpdateBlock (x&SCREENXMASK,y,(x&SCREENXMASK)+width*SCREENXDIV-1
		,y+height-1))
		VW_MaskBlock (block,block->sourceoffset[shift],dest,
			width,height,block->planesize[shift]);
}
#endif

void VWB_Plot (int x, int y, int color)
{
	x+=pansx;
	y+=pansy;
	if (VW_MarkUpdateBlock (x,y,x,y))
		VW_Plot(x,y,color);
}

void VWB_Hlin (int x1, int x2, int y, int color)
{
	x1+=pansx;
	x2+=pansx;
	y+=pansy;
	if (VW_MarkUpdateBlock (x1,y,x2,y))
		VW_Hlin(x1,x2,y,color);
}

void VWB_Vlin (int y1, int y2, int x, int color)
{
	x+=pansx;
	y1+=pansy;
	y2+=pansy;
	if (VW_MarkUpdateBlock (x,y1,x,y2))
		VW_Vlin(y1,y2,x,color);
}


//===========================================================================
