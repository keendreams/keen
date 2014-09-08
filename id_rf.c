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

// ID_RF.C

/*
=============================================================================

notes
-----

scrolling more than one tile / refresh forces a total redraw

two overlapping sprites of equal priority can change drawing order when
updated

=============================================================================
*/

#include "ID_HEADS.H"
#pragma hdrstop

/*
=============================================================================

						 LOCAL CONSTANTS

=============================================================================
*/

#define	SCREENTILESWIDE	20
#define	SCREENTILESHIGH	13

#define	SCREENSPACE		(SCREENWIDTH*240)
#define FREEEGAMEM		(0x10000l-3l*SCREENSPACE)

//
// the update array must have enough space for two screens that can float
// up two two tiles each way
//
// (PORTTILESWIDE+1)*PORTTILESHIGH must be even so the arrays can be cleared
// by word width instructions

#define	UPDATESCREENSIZE	(UPDATEWIDE*PORTTILESHIGH+2)
#define	UPDATESPARESIZE		(UPDATEWIDE*2+4)
#define UPDATESIZE			(UPDATESCREENSIZE+2*UPDATESPARESIZE)

#define G_EGASX_SHIFT	7	// global >> ?? = screen x
#define G_CGASX_SHIFT	6	// global >> ?? = screen x
#define G_SY_SHIFT		4	// global >> ?? = screen y

unsigned	SX_T_SHIFT;		// screen x >> ?? = tile EGA = 1, CGA = 2;
#define	SY_T_SHIFT		4	// screen y >> ?? = tile


#define	EGAPORTSCREENWIDE	42
#define	CGAPORTSCREENWIDE	84
#define	PORTSCREENHIGH  224

#define	UPDATESCREENSIZE	(UPDATEWIDE*PORTTILESHIGH+2)
#define	UPDATESPARESIZE		(UPDATEWIDE*2+4)
#define UPDATESIZE			(UPDATESCREENSIZE+2*UPDATESPARESIZE)

/*
=============================================================================

						   LOCAL TYPES

=============================================================================
*/

typedef	struct spriteliststruct
{
	int			screenx,screeny;
	int			width,height;

	unsigned	grseg,sourceofs,planesize;
	drawtype	draw;
	unsigned	tilex,tiley,tilewide,tilehigh;
	int			priority,updatecount;
	struct spriteliststruct **prevptr,*nextsprite;
} spritelisttype;


typedef struct
{
	int			screenx,screeny;
	int			width,height;
} eraseblocktype;


typedef struct
{
	unsigned	current;		// foreground tiles have high bit set
	int			count;
} tiletype;


typedef struct animtilestruct
{
	unsigned	x,y,tile;
	tiletype	*chain;
	unsigned	far *mapplane;
	struct animtilestruct **prevptr,*nexttile;
} animtiletype;

/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/

unsigned	tics;
long		lasttimecount;

boolean		compatability;			// crippled refresh for wierdo SVGAs

unsigned	mapwidth,mapheight,mapbyteswide,mapwordswide
			,mapbytesextra,mapwordsextra;
unsigned	mapbwidthtable[MAXMAPHEIGHT];

//
// Global : Actor coordinates are in this, at 1/16 th of a pixel, to allow
// for fractional movement and acceleration.
//
// Tiles  : Tile offsets from the upper left corner of the current map.
//
// Screen : Graphics level offsets from map origin, x in bytes, y in pixels.
// originxscreen is the same spot as originxtile, just with extra precision
// so graphics don't need to be done in tile boundaries.
//

unsigned	originxglobal,originyglobal;
unsigned	originxtile,originytile;
unsigned	originxscreen,originyscreen;
unsigned	originmap;
unsigned	originxmin,originxmax,originymin,originymax;
unsigned	originxtile,originytile;

unsigned	masterofs;

//
// Table of the offsets from bufferofs of each tile spot in the
// view port.  The extra wide tile should never be drawn, but the space
// is needed to account for the extra 0 in the update arrays.  Built by
// RF_Startup
//

unsigned	blockstarts[UPDATEWIDE*UPDATEHIGH];
unsigned	updatemapofs[UPDATEWIDE*UPDATEHIGH];

unsigned	uwidthtable[PORTTILESHIGH];		// lookup instead of multiply

byte		update[2][UPDATESIZE];
byte		*updateptr,*baseupdateptr,						// current start of update window
			*updatestart[2],
			*baseupdatestart[2];

/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/

static		char	scratch[20],str[20];

tiletype	allanims[MAXANIMTYPES];
unsigned	numanimchains;

void 		(*refreshvector) (void);

unsigned	screenstart[3] =
	{0,SCREENSPACE,SCREENSPACE*2};

unsigned	xpanmask;			// prevent panning to odd pixels

unsigned	screenpage;			// screen currently being displayed
unsigned	otherpage;

#if GRMODE == EGAGR
unsigned	tilecache[NUMTILE16];
#endif

spritelisttype	spritearray[MAXSPRITES],*prioritystart[PRIORITIES],
				*spritefreeptr;

animtiletype	animarray[MAXANIMTILES],*animhead,*animfreeptr;

int				animfreespot;

eraseblocktype	eraselist[2][MAXSPRITES],*eraselistptr[2];

/*
=============================================================================

						 LOCAL PROTOTYPES

=============================================================================
*/

void RFL_NewTile (unsigned updateoffset);
void RFL_MaskForegroundTiles (void);
void RFL_UpdateTiles (void);

void RFL_CalcOriginStuff (long x, long y);
void RFL_InitSpriteList (void);
void RFL_InitAnimList (void);
void RFL_CheckForAnimTile (unsigned x, unsigned y);
void RFL_AnimateTiles (void);
void RFL_RemoveAnimsOnX (unsigned x);
void RFL_RemoveAnimsOnY (unsigned y);
void RFL_EraseBlocks (void);
void RFL_UpdateSprites (void);


/*
=============================================================================

					 GRMODE INDEPENDANT ROUTINES

=============================================================================
*/


/*
=====================
=
= RF_Startup
=
=====================
*/

static	char *ParmStrings[] = {"comp",""};

void RF_Startup (void)
{
	int i,x,y;
	unsigned	*blockstart;

	if (grmode == EGAGR)
		for (i = 1;i < _argc;i++)
			if (US_CheckParm(_argv[i],ParmStrings) == 0)
			{
				compatability = true;
				break;
			}

	for (i=0;i<PORTTILESHIGH;i++)
		uwidthtable[i] = UPDATEWIDE*i;

	originxmin = originymin = MAPBORDER*TILEGLOBAL;

	eraselistptr[0] = &eraselist[0][0];
	eraselistptr[1] = &eraselist[1][0];



	if (grmode == EGAGR)
	{
		SX_T_SHIFT = 1;

		baseupdatestart[0] = &update[0][UPDATESPARESIZE];
		baseupdatestart[1] = &update[1][UPDATESPARESIZE];

		screenpage = 0;
		otherpage = 1;
		displayofs = screenstart[screenpage];
		bufferofs = screenstart[otherpage];
		masterofs = screenstart[2];

		updateptr = baseupdatestart[otherpage];

		blockstart = &blockstarts[0];
		for (y=0;y<UPDATEHIGH;y++)
			for (x=0;x<UPDATEWIDE;x++)
				*blockstart++ = SCREENWIDTH*16*y+x*TILEWIDTH;

		xpanmask = 6;	// dont pan to odd pixels
	}

	else if (grmode == CGAGR)
	{
		SX_T_SHIFT = 2;

		updateptr = baseupdateptr = &update[0][UPDATESPARESIZE];

		bufferofs = 0;
		masterofs = 0x8000;

		blockstart = &blockstarts[0];
		for (y=0;y<UPDATEHIGH;y++)
			for (x=0;x<UPDATEWIDE;x++)
				*blockstart++ = SCREENWIDTH*16*y+x*TILEWIDTH;
	}
}




/*
=====================
=
= RF_Shutdown
=
=====================
*/

void RF_Shutdown (void)
{

}

//===========================================================================

/*
=====================
=
= RF_NewMap
=
= Makes some convienient calculations based on maphead->
=
=====================
*/

void RF_NewMap (void)
{
	int i,x,y;
	unsigned spot,*table;

	mapwidth = mapheaderseg[mapon]->width;
	mapbyteswide = 2*mapwidth;
	mapheight = mapheaderseg[mapon]->height;
	mapwordsextra = mapwidth-PORTTILESWIDE;
	mapbytesextra = 2*mapwordsextra;

//
// make a lookup table for the maps left edge
//
	spot = 0;
	for (i=0;i<mapheight;i++)
	{
	  mapbwidthtable[i] = spot;
	  spot += mapbyteswide;
	}

//
// fill in updatemapofs with the new width info
//
	table = &updatemapofs[0];
	for (y=0;y<PORTTILESHIGH;y++)
		for (x=0;x<UPDATEWIDE;x++)
			*table++ = mapbwidthtable[y]+x*2;


//
// the y max value clips off the bottom half of a tile so a map that is
// 13 + MAPBORDER*2 tile high will not scroll at all vertically
//
	originxmax = (mapwidth-MAPBORDER-SCREENTILESWIDE)*TILEGLOBAL;
	originymax = (mapheight-MAPBORDER-SCREENTILESHIGH)*TILEGLOBAL;
	if (originxmax<originxmin)		// for very small maps
		originxmax=originxmin;
	if (originymax<originymin)
		originymax=originymin;

//
// clear out the lists
//
	RFL_InitSpriteList ();
	RFL_InitAnimList ();


	lasttimecount = TimeCount;		// setup for adaptive timing
	tics = 1;
}

//===========================================================================

/*
==========================
=
= RF_MarkTileGraphics
=
= Goes through mapplane[0/1] and marks all background/foreground tiles
= needed, then follows all animation sequences to make sure animated
= tiles get all the stages.  Every unique animating tile is given an
= entry in allanims[], so every instance of that tile will animate at the
= same rate.  The info plane for each animating tile will hold a pointer
= into allanims[], therefore you can't have both an animating foreground
= and background tile in the same spot!
=
==========================
*/

void RF_MarkTileGraphics (void)
{
	unsigned	size;
	int			tile,next,anims;
	unsigned	far	*start,far *end,far *info;
	unsigned	i,tilehigh;

	memset (allanims,0,sizeof(allanims));
	numanimchains = 0;

	size = mapwidth*mapheight;

//
// background plane
//
	start = mapsegs[0];
	info = mapsegs[2];
	end = start+size;
	do
	{
		tile = *start++;
		if (tile>=0)			// <0 is a tile that is never drawn
		{
			CA_MarkGrChunk(STARTTILE16+tile);
			if (tinf[ANIM+tile])
			{
				// this tile will animated

				for (i=0;i<numanimchains;i++)
					if (allanims[i].current == tile)
					{
						*info = (unsigned)&allanims[i];
						goto nextback;
					}

				// new chain of animating tiles

				if (i>=MAXANIMTYPES)
					Quit ("RF_MarkTileGraphics: Too many unique animated tiles!");
				allanims[i].current = tile;
				allanims[i].count = tinf[SPEED+tile];

				*info = (unsigned)&allanims[i];
				numanimchains++;

				anims = 0;
				next = tile+(signed char)(tinf[ANIM+tile]);
				while (next != tile)
				{
					CA_MarkGrChunk(STARTTILE16+next);
					next += (signed char)(tinf[ANIM+next]);
					if (++anims > 20)
						Quit ("MarkTileGraphics: Unending animation!");
				}

			}
		}
nextback:
		info++;
	} while (start<end);

//
// foreground plane
//
	start = mapsegs[1];
	info = mapsegs[2];
	end = start+size;
	do
	{
		tile = *start++;
		if (tile>=0)			// <0 is a tile that is never drawn
		{
			CA_MarkGrChunk(STARTTILE16M+tile);
			if (tinf[MANIM+tile])
			{
				// this tile will animated

				tilehigh = tile | 0x8000;	// foreground tiles have high bit
				for (i=0;i<numanimchains;i++)
					if (allanims[i].current == tilehigh)
					{
						*info = (unsigned)&allanims[i];
						goto nextfront;
					}

				// new chain of animating tiles

				if (i>=MAXANIMTYPES)
					Quit ("RF_MarkTileGraphics: Too many unique animated tiles!");
				allanims[i].current = tilehigh;
				allanims[i].count = tinf[MSPEED+tile];

				*info = (unsigned)&allanims[i];
				numanimchains++;

				anims = 0;
				next = tile+(signed char)(tinf[MANIM+tile]);
				while (next != tile)
				{
					CA_MarkGrChunk(STARTTILE16M+next);
					next += (signed char)(tinf[MANIM+next]);
					if (++anims > 20)
						Quit ("MarkTileGraphics: Unending animation!");
				}

			}
		}
nextfront:
		info++;
	} while (start<end);
}


//===========================================================================


/*
=========================
=
= RFL_InitAnimList
=
= Call to clear out the entire animating tile list and return all of them to
= the free list.
=
=========================
*/

void RFL_InitAnimList (void)
{
	int	i;

	animfreeptr = &animarray[0];

	for (i=0;i<MAXANIMTILES-1;i++)
		animarray[i].nexttile = &animarray[i+1];

	animarray[i].nexttile = NULL;

	animhead = NULL;			// nothing in list
}


/*
====================
=
= RFL_CheckForAnimTile
=
====================
*/

void RFL_CheckForAnimTile (unsigned x, unsigned y)
{
	unsigned 	tile,offset,speed,lasttime,thistime,timemissed;
	unsigned	far *map;
	animtiletype	*anim,*next;

// the info plane of each animating tile has a near pointer into allanims[]
// which gives the current state of all concurrently animating tiles

	offset = mapbwidthtable[y]/2+x;

//
// background
//
	map = mapsegs[0]+offset;
	tile = *map;
	if (tinf[ANIM+tile])
	{
		if (!animfreeptr)
			Quit ("RF_CheckForAnimTile: No free spots in tilearray!");
		anim = animfreeptr;
		animfreeptr = animfreeptr->nexttile;
		next = animhead;				// stick it at the start of the list
		animhead = anim;
		if (next)
			next->prevptr = &anim->nexttile;
		anim->nexttile = next;
		anim->prevptr = &animhead;

		anim->x = x;
		anim->y = y;
		anim->tile = tile;
		anim->mapplane = map;
		anim->chain = (tiletype *)*(mapsegs[2]+offset);
	}

//
// foreground
//
	map = mapsegs[1]+offset;
	tile = *map;
	if (tinf[MANIM+tile])
	{
		if (!animfreeptr)
			Quit ("RF_CheckForAnimTile: No free spots in tilearray!");
		anim = animfreeptr;
		animfreeptr = animfreeptr->nexttile;
		next = animhead;				// stick it at the start of the list
		animhead = anim;
		if (next)
			next->prevptr = &anim->nexttile;
		anim->nexttile = next;
		anim->prevptr = &animhead;

		anim->x = x;
		anim->y = y;
		anim->tile = tile;
		anim->mapplane = map;
		anim->chain = (tiletype *)*(mapsegs[2]+offset);
	}

}


/*
====================
=
= RFL_RemoveAnimsOnX
=
====================
*/

void RFL_RemoveAnimsOnX (unsigned x)
{
	animtiletype *current,*next;

	current = animhead;
	while (current)
	{
		if (current->x == x)
		{
			*(void **)current->prevptr = current->nexttile;
			if (current->nexttile)
				current->nexttile->prevptr = current->prevptr;
			next = current->nexttile;
			current->nexttile = animfreeptr;
			animfreeptr = current;
			current = next;
		}
		else
			current = current->nexttile;
	}
}


/*
====================
=
= RFL_RemoveAnimsOnY
=
====================
*/

void RFL_RemoveAnimsOnY (unsigned y)
{
	animtiletype *current,*next;

	current = animhead;
	while (current)
	{
		if (current->y == y)
		{
			*(void **)current->prevptr = current->nexttile;
			if (current->nexttile)
				current->nexttile->prevptr = current->prevptr;
			next = current->nexttile;
			current->nexttile = animfreeptr;
			animfreeptr = current;
			current = next;
		}
		else
			current = current->nexttile;
	}
}


/*
====================
=
= RFL_AnimateTiles
=
====================
*/

void RFL_AnimateTiles (void)
{
	animtiletype *current;
	unsigned	updateofs,tile,x,y;
	tiletype	*anim;

//
// animate the lists of tiles
//
	anim = &allanims[0];
	while (anim->current)
	{
		anim->count-=tics;
		while ( anim->count < 1)
		{
			if (anim->current & 0x8000)
			{
				tile = anim->current & 0x7fff;
				tile += (signed char)tinf[MANIM+tile];
				anim->count += tinf[MSPEED+tile];
				tile |= 0x8000;
			}
			else
			{
				tile = anim->current;
				tile += (signed char)tinf[ANIM+tile];
				anim->count += tinf[SPEED+tile];
			}
			anim->current = tile;
		}
		anim++;
	}


//
// traverse the list of animating tiles
//
	current = animhead;
	while (current)
	{
		tile =current->chain->current;
		if ( tile != current->tile)
		{
		// tile has animated
		//
		// remove tile from master screen cache,
		// change a tile to its next state, set the structure up for
		// next animation, and post an update region to both update pages
		//
			current->tile = tile;

			*(current->mapplane) = tile & 0x7fff; 		// change in map

#if GRMODE == EGAGR
			if (tile<0x8000)		// background
				tilecache[tile] = 0;
#endif

			x = current->x-originxtile;
			y = current->y-originytile;

			if (x>=PORTTILESWIDE || y>=PORTTILESHIGH)
				Quit ("RFL_AnimateTiles: Out of bounds!");

			updateofs = uwidthtable[y] + x;
			RFL_NewTile(updateofs);				// puts "1"s in both pages
		}
		current = current->nexttile;
	}
}


//===========================================================================

/*
=========================
=
= RFL_InitSpriteList
=
= Call to clear out the entire sprite list and return all of them to
= the free list.
=
=========================
*/

void RFL_InitSpriteList (void)
{
	int	i;

	spritefreeptr = &spritearray[0];
	for (i=0;i<MAXSPRITES-1;i++)
		spritearray[i].nextsprite = &spritearray[i+1];

	spritearray[i].nextsprite = NULL;

// NULL in all priority levels

	memset (prioritystart,0,sizeof(prioritystart));
}

//===========================================================================

/*
=================
=
= RFL_CalcOriginStuff
=
= Calculate all the global variables for a new position
= Long parms so position can be clipped to a maximum near 64k
=
=================
*/

void RFL_CalcOriginStuff (long x, long y)
{
	if (x<originxmin)
	  x=originxmin;
	else if (x>originxmax)
	  x=originxmax;

	if (y<originymin)
	  y=originymin;
	else if (y>originymax)
	  y=originymax;

	originxglobal = x;
	originyglobal = y;
	originxtile = originxglobal>>G_T_SHIFT;
	originytile = originyglobal>>G_T_SHIFT;
	originxscreen = originxtile<<SX_T_SHIFT;
	originyscreen = originytile<<SY_T_SHIFT;
	originmap = mapbwidthtable[originytile] + originxtile*2;

#if GRMODE == EGAGR
	panx = (originxglobal>>G_P_SHIFT) & 15;
	pansx = panx & 8;
	pany = pansy = (originyglobal>>G_P_SHIFT) & 15;
	panadjust = panx/8 + ylookup[pany];
#endif

#if GRMODE == CGAGR
	panx = (originxglobal>>G_P_SHIFT) & 15;
	pansx = panx & 12;
	pany = pansy = (originyglobal>>G_P_SHIFT) & 15;
	panadjust = pansx/4 + ylookup[pansy];
#endif

}

//===========================================================================

/*
=====================
=
= RF_SetRefreshHook
=
=====================
*/

void RF_SetRefreshHook (void (*func) (void) )
{
	refreshvector = func;
}



/*
=================
=
= RFL_NewRow
=
= Bring a new row of tiles onto the port, spawning animating tiles
=
=================
*/

void	RFL_NewRow (int dir)
{
	unsigned count,updatespot,updatestep;
	int		x,y,xstep,ystep;

	switch (dir)
	{
	case 0:		// top row
		updatespot = 0;
		updatestep = 1;
		x = originxtile;
		y = originytile;
		xstep = 1;
		ystep = 0;
		count = PORTTILESWIDE;
		break;

	case 1:		// right row
		updatespot = PORTTILESWIDE-1;
		updatestep = UPDATEWIDE;
		x = originxtile + PORTTILESWIDE-1;
		y = originytile;
		xstep = 0;
		ystep = 1;
		count = PORTTILESHIGH;
		break;

	case 2:		// bottom row
		updatespot = UPDATEWIDE*(PORTTILESHIGH-1);
		updatestep = 1;
		x = originxtile;
		y = originytile + PORTTILESHIGH-1;
		xstep = 1;
		ystep = 0;
		count = PORTTILESWIDE;
		break;

	case 3:		// left row
		updatespot = 0;
		updatestep = UPDATEWIDE;
		x = originxtile;
		y = originytile;
		xstep = 0;
		ystep = 1;
		count = PORTTILESHIGH;
		break;
	default:
		Quit ("RFL_NewRow: Bad dir!");
	}

	while (count--)
	{
		RFL_NewTile(updatespot);
		RFL_CheckForAnimTile (x,y);
		updatespot+=updatestep;
		x+=xstep;
		y+=ystep;
	}
}

//===========================================================================

/*
=====================
=
= RF_ForceRefresh
=
=====================
*/

void RF_ForceRefresh (void)
{
	RF_NewPosition (originxglobal,originyglobal);
	RF_Refresh ();
	RF_Refresh ();
}



/*
=============================================================================

					EGA specific routines

=============================================================================
*/

#if GRMODE == EGAGR


/*
=====================
=
= RF_NewPosition EGA
=
=====================
*/

void RF_NewPosition (unsigned x, unsigned y)
{
	int mx,my;
	byte	*page0ptr,*page1ptr;
	unsigned 	updatenum;

//
// calculate new origin related globals
//
	RFL_CalcOriginStuff (x,y);

//
// clear out all animating tiles
//
	RFL_InitAnimList ();

//
// set up the new update arrays at base position
//
	memset (tilecache,0,sizeof(tilecache));		// old cache is invalid

	updatestart[0] = baseupdatestart[0];
	updatestart[1] = baseupdatestart[1];

	page0ptr = updatestart[0]+PORTTILESWIDE;	// used to stick "0"s after rows
	page1ptr = updatestart[1]+PORTTILESWIDE;

	updatenum = 0;				// start at first visable tile

	for (my=0;my<PORTTILESHIGH;my++)
	{
		for (mx=0;mx<PORTTILESWIDE;mx++)
		{
			RFL_NewTile(updatenum);			// puts "1"s in both pages
			RFL_CheckForAnimTile(mx+originxtile,my+originytile);
			updatenum++;
		}
		updatenum++;
		*page0ptr = *page1ptr = 0; // set a 0 at end of a line of tiles
		page0ptr+=(PORTTILESWIDE+1);
		page1ptr+=(PORTTILESWIDE+1);
	}
	*(word *)(page0ptr-PORTTILESWIDE)
		= *(word *)(page1ptr-PORTTILESWIDE) = UPDATETERMINATE;
}

//===========================================================================

/*
=================
=
= RFL_OldRow EGA
=
= Uncache the trailing row of tiles
=
=================
*/

void	RFL_OldRow (unsigned updatespot,unsigned count,unsigned step)
{

asm	mov	si,[updatespot]			// pointer inside each map plane
asm	mov	cx,[count]				// number of tiles to clear
asm	mov	dx,[step]				// move to next tile
asm	mov	es,[WORD PTR mapsegs]			// background plane
asm	mov	ds,[WORD PTR mapsegs+2]			// foreground plane

clearcache:
asm	mov	bx,[si]
asm	or	bx,bx
asm	jnz	blockok					// if a foreground tile, block wasn't cached
asm	mov	bx,[es:si]
asm	shl	bx,1
asm	mov	[WORD PTR ss:tilecache+bx],0  //tile is no longer in master screen cache
blockok:
asm	add	si,dx
asm	loop	clearcache

asm	mov	ax,ss
asm	mov	ds,ax

}


/*
=====================
=
= RF_Scroll  EGA
=
= Move the origin x/y global coordinates, readjust the screen panning, and
= scroll if needed.  If the scroll distance is greater than one tile, the
= entire screen will be redrawn (this could be generalized, but scrolling
= more than one tile per refresh is a bad idea!).
=
=====================
*/

void RF_Scroll (int x, int y)
{
	long		neworgx,neworgy;
	int			i,deltax,deltay,absdx,absdy;
	int			oldxt,oldyt,move,yy;
	unsigned	updatespot;
	byte		*update0,*update1;
	unsigned	oldpanx,oldpanadjust,oldoriginmap,oldscreen,newscreen,screencopy;
	int			screenmove;

	oldxt = originxtile;
	oldyt = originytile;
	oldoriginmap = originmap;
	oldpanadjust = panadjust;
	oldpanx = panx;

	RFL_CalcOriginStuff ((long)originxglobal + x,(long)originyglobal + y);

	deltax = originxtile - oldxt;
	absdx = abs(deltax);
	deltay = originytile - oldyt;
	absdy = abs(deltay);

	if (absdx>1 || absdy>1)
	{
	//
	// scrolled more than one tile, so start from scratch
	//
		RF_NewPosition(originxglobal,originyglobal);
		return;
	}

	if (!absdx && !absdy)
		return;					// the screen has not scrolled an entire tile


//
// adjust screens and handle SVGA crippled compatability mode
//
	screenmove = deltay*16*SCREENWIDTH + deltax*TILEWIDTH;
	for (i=0;i<3;i++)
	{
		screenstart[i]+= screenmove;
		if (compatability && screenstart[i] > (0x10000l-SCREENSPACE) )
		{
			//
			// move the screen to the opposite end of the buffer
			//
			screencopy = screenmove>0 ? FREEEGAMEM : -FREEEGAMEM;
			oldscreen = screenstart[i] - screenmove;
			newscreen = oldscreen + screencopy;
			screenstart[i] = newscreen + screenmove;
			VW_ScreenToScreen (oldscreen,newscreen,
				PORTTILESWIDE*2,PORTTILESHIGH*16);

			if (i==screenpage)
				VW_SetScreen(newscreen+oldpanadjust,oldpanx & xpanmask);
		}
	}
	bufferofs = screenstart[otherpage];
	displayofs = screenstart[screenpage];
	masterofs = screenstart[2];


//
// float the update regions
//
	move = deltax;
	if (deltay==1)
	  move += UPDATEWIDE;
	else if (deltay==-1)
	  move -= UPDATEWIDE;

	updatestart[0]+=move;
	updatestart[1]+=move;

//
// draw the new tiles just scrolled on to the master screen, and
// mark them as needing to be copied to each screen next refreshes
// Make sure a zero is at the end of each row in update
//

	if (deltax)
	{
		if (deltax==1)
		{
			RFL_NewRow (1);			// new right row
			RFL_OldRow (oldoriginmap,PORTTILESHIGH,mapbyteswide);
			RFL_RemoveAnimsOnX (originxtile-1);
		}
		else
		{
			RFL_NewRow (3);			// new left row
			RFL_OldRow (oldoriginmap+(PORTTILESWIDE-1)*2,PORTTILESHIGH
			,mapbyteswide);
			RFL_RemoveAnimsOnX (originxtile+PORTTILESWIDE);
		}

		update0 = updatestart[0]+PORTTILESWIDE;
		update1 = updatestart[1]+PORTTILESWIDE;
		for	(yy=0;yy<PORTTILESHIGH;yy++)
		{
			*update0 = *update1 = 0;	// drop a 0 at end of each row
			update0+=UPDATEWIDE;
			update1+=UPDATEWIDE;
		}
	}

//----------------

	if (deltay)
	{
		if (deltay==1)
		{
			RFL_NewRow (2);			// new bottom row
			RFL_OldRow (oldoriginmap,PORTTILESWIDE,2);
			updatespot = UPDATEWIDE*(PORTTILESHIGH-1);
			RFL_RemoveAnimsOnY (originytile-1);
		}
		else
		{
			RFL_NewRow (0);			// new top row
			RFL_OldRow (oldoriginmap+mapbwidthtable[PORTTILESHIGH-1]
			,PORTTILESWIDE,2);
			updatespot = 0;
			RFL_RemoveAnimsOnY (originytile+PORTTILESHIGH);
		}

		*(updatestart[0]+updatespot+PORTTILESWIDE) =
			*(updatestart[1]+updatespot+PORTTILESWIDE) = 0;
	}

//----------------

	//
	// place a new terminator
	//
	update0 = updatestart[0]+UPDATEWIDE*PORTTILESHIGH-1;
	update1 = updatestart[1]+UPDATEWIDE*PORTTILESHIGH-1;
	*update0++ = *update1++ = 0;
	*(unsigned *)update0 = *(unsigned *)update1 = UPDATETERMINATE;
}

//===========================================================================

/*
=====================
=
= RF_PlaceSprite   EGA
=
=====================
*/

void RF_PlaceSprite (void **user,unsigned globalx,unsigned globaly,
	unsigned spritenumber, drawtype draw, int priority)
{
	spritelisttype	register *sprite,*next;
	spritetabletype far *spr;
	spritetype _seg	*block;
	unsigned	shift,pixx;

	if (!spritenumber)
	{
		RF_RemoveSprite (user);
		return;
	}

	sprite = (spritelisttype *)*user;

	if	(sprite)
	{
	// sprite allready exists in the list, so we can use it's block

	//
	// post an erase block to both pages by copying screenx,screeny,width,height
	// both pages may not need to be erased if the sprite just changed last frame
	//
		if (sprite->updatecount<2)
		{
			if (!sprite->updatecount)
				memcpy (eraselistptr[otherpage]++,sprite,sizeof(eraseblocktype));
			memcpy (eraselistptr[screenpage]++,sprite,sizeof(eraseblocktype));
		}

		if (priority != sprite->priority)
		{
		// sprite mvoed to another priority, so unlink the old one and
		// relink it in the new priority

			next = sprite->nextsprite;			// cut old links
			if (next)
				next->prevptr = sprite->prevptr;
			*sprite->prevptr = next;
			goto linknewspot;
		}
	}
	else
	{
	// this is a brand new sprite, so allocate a block from the array

		if (!spritefreeptr)
			Quit ("RF_PlaceSprite: No free spots in spritearray!");

		sprite = spritefreeptr;
		spritefreeptr = spritefreeptr->nextsprite;

linknewspot:
		next = prioritystart[priority];		// stick it in new spot
		if (next)
			next->prevptr = &sprite->nextsprite;
		sprite->nextsprite = next;
		prioritystart[priority] = sprite;
		sprite->prevptr = &prioritystart[priority];
	}

//
// write the new info to the sprite
//
	spr = &spritetable[spritenumber-STARTSPRITES];
	block = (spritetype _seg *)grsegs[spritenumber];

	globaly+=spr->orgy;
	globalx+=spr->orgx;

	pixx = globalx >> G_SY_SHIFT;
	shift = (pixx&7)/2;

	sprite->screenx = pixx >> (G_EGASX_SHIFT-G_SY_SHIFT);
	sprite->screeny = globaly >> G_SY_SHIFT;
	sprite->width = block->width[shift];
	sprite->height = spr->height;
	sprite->grseg = spritenumber;
	sprite->sourceofs = block->sourceoffset[shift];
	sprite->planesize = block->planesize[shift];
	sprite->draw = draw;
	sprite->priority = priority;
	sprite->tilex = sprite->screenx >> SX_T_SHIFT;
	sprite->tiley = sprite->screeny >> SY_T_SHIFT;
	sprite->tilewide = ( (sprite->screenx + sprite->width -1) >> SX_T_SHIFT )
		- sprite->tilex + 1;
	sprite->tilehigh = ( (sprite->screeny + sprite->height -1) >> SY_T_SHIFT )
		- sprite->tiley + 1;

	sprite->updatecount = 2;		// draw on next two refreshes

// save the sprite pointer off in the user's pointer so it can be moved
// again later

	*user = sprite;
}

//===========================================================================

/*
=====================
=
= RF_RemoveSprite  EGA
=
=====================
*/

void RF_RemoveSprite (void **user)
{
	spritelisttype	*sprite,*next;

	sprite = (spritelisttype *)*user;
	if (!sprite)
		return;

//
// post an erase block to both pages by copying screenx,screeny,width,height
// both pages may not need to be erased if the sprite just changed last frame
//
	if (sprite->updatecount<2)
	{
		if (!sprite->updatecount)
			memcpy (eraselistptr[otherpage]++,sprite,sizeof(eraseblocktype));
		memcpy (eraselistptr[screenpage]++,sprite,sizeof(eraseblocktype));
	}

//
// unlink the sprite node
//
	next = sprite->nextsprite;
	if (next)						// if (!next), sprite is last in chain
		next->prevptr = sprite->prevptr;
	*sprite->prevptr = next;

//
// add it back to the free list
//
	sprite->nextsprite = spritefreeptr;
	spritefreeptr = sprite;

//
// null the users pointer, so next time that actor gets placed, it will
// allocate a new block
//

	*user = 0;
}


//===========================================================================


/*
====================
=
= RFL_EraseBlocks  EGA
=
= Write mode 1 should be set
=
====================
*/

void RFL_EraseBlocks (void)
{
	eraseblocktype	*block,*done;
	int			screenxh,screenyh;
	unsigned	pos,xtl,ytl,xth,yth,x,y;
	byte		*updatespot;
	unsigned	updatedelta;
	unsigned	erasecount;

#ifdef PROFILE
	erasecount = 0;
#endif

	block = otherpage ? &eraselist[1][0] : &eraselist[0][0];

	done = eraselistptr[otherpage];

	while (block != done)
	{

	//
	// clip the block to the current screen view
	//
		block->screenx -= originxscreen;
		block->screeny -= originyscreen;

		if (block->screenx < 0)
		{
			block->width += block->screenx;
			if (block->width<1)
				goto next;
			block->screenx = 0;
		}

		if (block->screeny < 0)
		{
			block->height += block->screeny;
			if (block->height<1)
				goto next;
			block->screeny = 0;
		}

		screenxh = block->screenx + block->width;
		screenyh = block->screeny + block->height;

		if (screenxh > EGAPORTSCREENWIDE)
		{
			block->width = EGAPORTSCREENWIDE-block->screenx;
			screenxh = block->screenx + block->width;
		}

		if (screenyh > PORTSCREENHIGH)
		{
			block->height = PORTSCREENHIGH-block->screeny;
			screenyh = block->screeny + block->height;
		}

		if (block->width<1 || block->height<1)
			goto next;

	//
	// erase the block by copying from the master screen
	//
		pos = ylookup[block->screeny]+block->screenx;
		VW_ScreenToScreen (masterofs+pos,bufferofs+pos,
			block->width,block->height);

	//
	// put 2s in update where the block was, to force sprites to update
	//
		xtl = block->screenx >> SX_T_SHIFT;
		xth = (block->screenx+block->width-1) >> SX_T_SHIFT;
		ytl = block->screeny >> SY_T_SHIFT;
		yth = (block->screeny+block->height-1) >> SY_T_SHIFT;

		updatespot = updateptr + uwidthtable[ytl] + xtl;
		updatedelta = UPDATEWIDE - (xth-xtl+1);

		for (y=ytl;y<=yth;y++)
		{
			for (x=xtl;x<=xth;x++)
				*updatespot++ = 2;
			updatespot += updatedelta;		// down to next line
		}
#ifdef PROFILE
		erasecount++;
#endif

next:
		block++;
	}
	eraselistptr[otherpage] = otherpage ? &eraselist[1][0] : &eraselist[0][0];
#ifdef PROFILE
	strcpy (scratch,"\tErase:");
	itoa (erasecount,str,10);
	strcat (scratch,str);
	write (profilehandle,scratch,strlen(scratch));
#endif

}


/*
====================
=
= RFL_UpdateSprites EGA
=
= NOTE: Implement vertical clipping!
=
====================
*/

void RFL_UpdateSprites (void)
{
	spritelisttype	*sprite;
	int	portx,porty,x,y,xtl,xth,ytl,yth;
	int	priority;
	unsigned dest;
	byte		*updatespot,*baseupdatespot;
	unsigned	updatedelta;
	unsigned	updatecount;
	unsigned	height,sourceofs;

#ifdef PROFILE
	updatecount = 0;
#endif

	for (priority=0;priority<PRIORITIES;priority++)
	{
		if (priority==MASKEDTILEPRIORITY)
			RFL_MaskForegroundTiles ();

		for (sprite = prioritystart[priority]; sprite ;
			sprite = (spritelisttype *)sprite->nextsprite)
		{
		//
		// see if the sprite has any visable area in the port
		//

			portx = sprite->screenx - originxscreen;
			porty = sprite->screeny - originyscreen;
			xtl = portx >> SX_T_SHIFT;
			xth = (portx + sprite->width-1) >> SX_T_SHIFT;
			ytl = porty >> SY_T_SHIFT;
			yth = (porty + sprite->height-1) >> SY_T_SHIFT;

			if (xtl<0)
			  xtl = 0;
			if (xth>=PORTTILESWIDE)
			  xth = PORTTILESWIDE-1;
			if (ytl<0)
			  ytl = 0;
			if (yth>=PORTTILESHIGH)
			  yth = PORTTILESHIGH-1;

			if (xtl>xth || ytl>yth)
				continue;

		//
		// see if it's visable area covers any non 0 update tiles
		//
			updatespot = baseupdatespot = updateptr + uwidthtable[ytl] + xtl;
			updatedelta = UPDATEWIDE - (xth-xtl+1);

			if (sprite->updatecount)
			{
				sprite->updatecount--;			// the sprite was just placed,
				goto redraw;					// so draw it for sure
			}

			for (y=ytl;y<=yth;y++)
			{
				for (x=xtl;x<=xth;x++)
					if (*updatespot++)
						goto redraw;
				updatespot += updatedelta;		// down to next line
			}
			continue;							// no need to update

redraw:
		//
		// set the tiles it covers to 3, because those tiles are being
		// updated
		//
			updatespot = baseupdatespot;
			for (y=ytl;y<=yth;y++)
			{
				for (x=xtl;x<=xth;x++)
					*updatespot++ = 3;
				updatespot += updatedelta;		// down to next line
			}
		//
		// draw it!
		//
			height = sprite->height;
			sourceofs = sprite->sourceofs;
			if (porty<0)
			{
				height += porty;					// clip top off
				sourceofs -= porty*sprite->width;
				porty = 0;
			}
			else if (porty+height>PORTSCREENHIGH)
			{
				height = PORTSCREENHIGH - porty;    // clip bottom off
			}

			dest = bufferofs + ylookup[porty] + portx;

			switch (sprite->draw)
			{
			case spritedraw:
				VW_MaskBlock(grsegs[sprite->grseg], sourceofs,
					dest,sprite->width,height,sprite->planesize);
				break;

			case maskdraw:
				break;

			}
#ifdef PROFILE
			updatecount++;
#endif


		}
	}
#ifdef PROFILE
	strcpy (scratch,"\tSprites:");
	itoa (updatecount,str,10);
	strcat (scratch,str);
	write (profilehandle,scratch,strlen(scratch));
#endif

}


/*
=====================
=
= RF_Refresh   EGA
=
= All routines will draw at the port at bufferofs, possibly copying from
= the port at masterofs.  The EGA version then page flips, while the
= CGA version updates the screen from the buffer port.
=
= Screenpage is the currently displayed page, not the one being drawn
= Otherpage is the page to be worked with now
=
=====================
*/

void RF_Refresh (void)
{
	byte	*newupdate;
	long	newtime;

	updateptr = updatestart[otherpage];

	RFL_AnimateTiles ();		// DEBUG

//
// update newly scrolled on tiles and animated tiles from the master screen
//
	EGAWRITEMODE(1);
	EGAMAPMASK(15);
	RFL_UpdateTiles ();
	RFL_EraseBlocks ();

//
// Update is all 0 except where sprites have changed or new area has
// been scrolled on.  Go through all sprites and update the ones that cover
// a non 0 update tile
//
	EGAWRITEMODE(0);
	RFL_UpdateSprites ();

//
// if the main program has a refresh hook set, call their function before
// displaying the new page
//
	if (refreshvector)
		refreshvector();

//
// display the changed screen
//
	VW_SetScreen(bufferofs+panadjust,panx & xpanmask);

//
// prepare for next refresh
//
// Set the update array to the middle position and clear it out to all "0"s
// with an UPDATETERMINATE at the end
//
	updatestart[otherpage] = newupdate = baseupdatestart[otherpage];
asm	mov	ax,ds
asm	mov	es,ax
asm	xor	ax,ax
asm	mov	cx,(UPDATESCREENSIZE-2)/2
asm	mov	di,[newupdate]
asm	rep	stosw
asm	mov	[WORD PTR es:di],UPDATETERMINATE

	screenpage ^= 1;
	otherpage ^= 1;
	bufferofs = screenstart[otherpage];
	displayofs = screenstart[screenpage];

//
// calculate tics since last refresh for adaptive timing
//
	if (lasttimecount > TimeCount)
		lasttimecount = TimeCount;		// if the game was paused a LONG time
	do
	{
		newtime = TimeCount;
		tics = newtime-lasttimecount;
	} while (tics<MINTICS);
	lasttimecount = newtime;

#ifdef PROFILE
	strcpy (scratch,"\tTics:");
	itoa (tics,str,10);
	strcat (scratch,str);
	strcat (scratch,"\n");
	write (profilehandle,scratch,strlen(scratch));
#endif

	if (tics>MAXTICS)
	{
		TimeCount -= (tics-MAXTICS);
		tics = MAXTICS;
	}
}

#endif		// GRMODE == EGAGR

/*
=============================================================================

					CGA specific routines

=============================================================================
*/

#if GRMODE == CGAGR


/*
=====================
=
= RF_NewPosition   CGA
=
=====================
*/

void RF_NewPosition (unsigned x, unsigned y)
{
	int mx,my;
	byte	*spotptr;
	unsigned 	updatenum;

//
// calculate new origin related globals
//
	RFL_CalcOriginStuff (x,y);

//
// clear out all animating tiles
//
	RFL_InitAnimList ();

//
// set up the new update arrays at base position
//
	updateptr = baseupdateptr;

	spotptr = updateptr + PORTTILESWIDE;	// used to stick "0"s after rows

	updatenum = 0;				// start at first visable tile

	for (my=0;my<PORTTILESHIGH;my++)
	{
		for (mx=0;mx<PORTTILESWIDE;mx++)
		{
			RFL_NewTile(updatenum);			// puts "1"s in both pages
			RFL_CheckForAnimTile(mx+originxtile,my+originytile);
			updatenum++;
		}
		updatenum++;
		*spotptr = 0; // set a 0 at end of a line of tiles
		spotptr +=(PORTTILESWIDE+1);
	}
	*(word *)(spotptr-PORTTILESWIDE) = UPDATETERMINATE;
}


/*
=====================
=
= RF_Scroll       CGA
=
= Move the origin x/y global coordinates, readjust the screen panning, and
= scroll if needed.  If the scroll distance is greater than one tile, the
= entire screen will be redrawn (this could be generalized, but scrolling
= more than one tile per refresh is a bad idea!).
=
=====================
*/

void RF_Scroll (int x, int y)
{
	long		neworgx,neworgy;
	int			i,deltax,deltay,absdx,absdy;
	int			oldxt,oldyt,move,yy;
	unsigned	updatespot;
	byte		*spotptr;
	unsigned	oldoriginmap,oldscreen,newscreen,screencopy;
	int			screenmove;

	oldxt = originxtile;
	oldyt = originytile;

	RFL_CalcOriginStuff ((long)originxglobal + x,(long)originyglobal + y);

	deltax = originxtile - oldxt;
	absdx = abs(deltax);
	deltay = originytile - oldyt;
	absdy = abs(deltay);

	if (absdx>1 || absdy>1)
	{
	//
	// scrolled more than one tile, so start from scratch
	//
		RF_NewPosition(originxglobal,originyglobal);
		return;
	}

	if (!absdx && !absdy)
		return;					// the screen has not scrolled an entire tile


//
// float screens
//
	screenmove = deltay*16*SCREENWIDTH + deltax*TILEWIDTH;
	bufferofs += screenmove;
	masterofs += screenmove;


//
// float the update regions
//
	move = deltax;
	if (deltay==1)
	  move += UPDATEWIDE;
	else if (deltay==-1)
	  move -= UPDATEWIDE;

	updateptr+=move;

//
// draw the new tiles just scrolled on to the master screen, and
// mark them as needing to be copied to each screen next refreshes
// Make sure a zero is at the end of each row in update
//

	if (deltax)
	{
		if (deltax==1)
		{
			RFL_NewRow (1);			// new right row
			RFL_RemoveAnimsOnX (originxtile-1);
		}
		else
		{
			RFL_NewRow (3);			// new left row
			RFL_RemoveAnimsOnX (originxtile+PORTTILESWIDE);
		}

		spotptr = updateptr+PORTTILESWIDE;
		for	(yy=0;yy<PORTTILESHIGH;yy++)
		{
			*spotptr = 0;		// drop a 0 at end of each row
			spotptr+=UPDATEWIDE;
		}
	}

//----------------

	if (deltay)
	{
		if (deltay==1)
		{
			RFL_NewRow (2);			// new bottom row
			*(updateptr+UPDATEWIDE*(PORTTILESHIGH-1)+PORTTILESWIDE) = 0;
			RFL_RemoveAnimsOnY (originytile-1);
		}
		else
		{
			RFL_NewRow (0);			// new top row
			*(updateptr+PORTTILESWIDE) = 0;
			RFL_RemoveAnimsOnY (originytile+PORTTILESHIGH);
		}
	}

//----------------

	//
	// place a new terminator
	//
	spotptr = updateptr+UPDATEWIDE*PORTTILESHIGH-1;
	*spotptr++ = 0;
	*(unsigned *)spotptr = UPDATETERMINATE;
}

/*
=====================
=
= RF_PlaceSprite  CGA
=
=====================
*/

void RF_PlaceSprite (void **user,unsigned globalx,unsigned globaly,
	unsigned spritenumber, drawtype draw, int priority)
{
	spritelisttype	register *sprite,*next;
	spritetabletype far *spr;
	spritetype _seg	*block;
	unsigned	shift,pixx;

	if (!spritenumber)
	{
		RF_RemoveSprite (user);
		return;
	}

	sprite = (spritelisttype *)*user;

	if	(sprite)
	{
	// sprite allready exists in the list, so we can use it's block

	//
	// post an erase block to erase the old position by copying
	// screenx,screeny,width,height
	//
		if (!sprite->updatecount)		// may not have been drawn at all yet
			memcpy (eraselistptr[0]++,sprite,sizeof(eraseblocktype));

		if (priority != sprite->priority)
		{
		// sprite moved to another priority, so unlink the old one and
		// relink it in the new priority

			next = sprite->nextsprite;			// cut old links
			if (next)
				next->prevptr = sprite->prevptr;
			*sprite->prevptr = next;
			goto linknewspot;
		}
	}
	else
	{
	// this is a brand new sprite, so allocate a block from the array

		if (!spritefreeptr)
			Quit ("RF_PlaceSprite: No free spots in spritearray!");

		sprite = spritefreeptr;
		spritefreeptr = spritefreeptr->nextsprite;

linknewspot:
		next = prioritystart[priority];		// stick it in new spot
		if (next)
			next->prevptr = &sprite->nextsprite;
		sprite->nextsprite = next;
		prioritystart[priority] = sprite;
		sprite->prevptr = &prioritystart[priority];
	}

//
// write the new info to the sprite
//
	spr = &spritetable[spritenumber-STARTSPRITES];
	block = (spritetype _seg *)grsegs[spritenumber];

	globaly+=spr->orgy;
	globalx+=spr->orgx;

	sprite->screenx = globalx >> G_CGASX_SHIFT;
	sprite->screeny = globaly >> G_SY_SHIFT;
	sprite->width = block->width[0];
	sprite->height = spr->height;
	sprite->grseg = spritenumber;
	sprite->sourceofs = block->sourceoffset[0];
	sprite->planesize = block->planesize[0];
	sprite->draw = draw;
	sprite->priority = priority;
	sprite->tilex = sprite->screenx >> SX_T_SHIFT;
	sprite->tiley = sprite->screeny >> SY_T_SHIFT;
	sprite->tilewide = ( (sprite->screenx + sprite->width -1) >> SX_T_SHIFT )
		- sprite->tilex + 1;
	sprite->tilehigh = ( (sprite->screeny + sprite->height -1) >> SY_T_SHIFT )
		- sprite->tiley + 1;

	sprite->updatecount = 1;		// draw on next refresh

// save the sprite pointer off in the user's pointer so it can be moved
// again later

	*user = sprite;
}

//===========================================================================

/*
=====================
=
= RF_RemoveSprite CGA
=
=====================
*/

void RF_RemoveSprite (void **user)
{
	spritelisttype	*sprite,*next;

	sprite = (spritelisttype *)*user;
	if (!sprite)
		return;

//
// post an erase block to erase the old position by copying
// screenx,screeny,width,height
//
	if (!sprite->updatecount)
	{
		memcpy (eraselistptr[0]++,sprite,sizeof(eraseblocktype));
	}

//
// unlink the sprite node
//
	next = sprite->nextsprite;
	if (next)						// if (!next), sprite is last in chain
		next->prevptr = sprite->prevptr;
	*sprite->prevptr = next;

//
// add it back to the free list
//
	sprite->nextsprite = spritefreeptr;
	spritefreeptr = sprite;

//
// null the users pointer, so next time that actor gets placed, it will
// allocate a new block
//

	*user = 0;
}


/*
====================
=
= RFL_EraseBlocks CGA
=
= Write mode 1 should be set
=
====================
*/

void RFL_EraseBlocks (void)
{
	eraseblocktype	*block,*done;
	int			screenxh,screenyh;
	unsigned	pos,xtl,ytl,xth,yth,x,y;
	byte		*updatespot;
	unsigned	updatedelta;

	block = &eraselist[0][0];

	done = eraselistptr[0];

	while (block != done)
	{

	//
	// clip the block to the current screen view
	//
		block->screenx -= originxscreen;
		block->screeny -= originyscreen;

		if (block->screenx < 0)
		{
			block->width += block->screenx;
			if (block->width<1)
				goto next;
			block->screenx = 0;
		}

		if (block->screeny < 0)
		{
			block->height += block->screeny;
			if (block->height<1)
				goto next;
			block->screeny = 0;
		}

		screenxh = block->screenx + block->width;
		screenyh = block->screeny + block->height;

		if (screenxh > CGAPORTSCREENWIDE)
		{
			block->width = CGAPORTSCREENWIDE-block->screenx;
			screenxh = block->screenx + block->width;
		}

		if (screenyh > PORTSCREENHIGH)
		{
			block->height = PORTSCREENHIGH-block->screeny;
			screenyh = block->screeny + block->height;
		}

		if (block->width<1 || block->height<1)
			goto next;

	//
	// erase the block by copying from the master screen
	//
		pos = ylookup[block->screeny]+block->screenx;
		block->width = (block->width + (pos&1) + 1)& ~1;
		pos &= ~1;				// make sure a word copy gets used
		VW_ScreenToScreen (masterofs+pos,bufferofs+pos,
			block->width,block->height);

	//
	// put 2s in update where the block was, to force sprites to update
	//
		xtl = block->screenx >> SX_T_SHIFT;
		xth = (block->screenx+block->width-1) >> SX_T_SHIFT;
		ytl = block->screeny >> SY_T_SHIFT;
		yth = (block->screeny+block->height-1) >> SY_T_SHIFT;

		updatespot = updateptr + uwidthtable[ytl] + xtl;
		updatedelta = UPDATEWIDE - (xth-xtl+1);

		for (y=ytl;y<=yth;y++)
		{
			for (x=xtl;x<=xth;x++)
				*updatespot++ = 2;
			updatespot += updatedelta;		// down to next line
		}

next:
		block++;
	}
	eraselistptr[0] = &eraselist[0][0];
}


/*
====================
=
= RFL_UpdateSprites      CGA
=
= NOTE: Implement vertical clipping!
=
====================
*/

void RFL_UpdateSprites (void)
{
	spritelisttype	*sprite;
	int	portx,porty,x,y,xtl,xth,ytl,yth;
	int	priority;
	unsigned dest;
	byte		*updatespot,*baseupdatespot;
	unsigned	updatedelta;

	unsigned	updatecount;
	unsigned	height,sourceofs;

#ifdef PROFILE
	updatecount = 0;
#endif


	for (priority=0;priority<PRIORITIES;priority++)
	{
		if (priority==MASKEDTILEPRIORITY)
			RFL_MaskForegroundTiles ();

		for (sprite = prioritystart[priority]; sprite ;
			sprite = (spritelisttype *)sprite->nextsprite)
		{
		//
		// see if the sprite has any visable area in the port
		//

			portx = sprite->screenx - originxscreen;
			porty = sprite->screeny - originyscreen;
			xtl = portx >> SX_T_SHIFT;
			xth = (portx + sprite->width-1) >> SX_T_SHIFT;
			ytl = porty >> SY_T_SHIFT;
			yth = (porty + sprite->height-1) >> SY_T_SHIFT;

			if (xtl<0)
			  xtl = 0;
			if (xth>=PORTTILESWIDE)
			  xth = PORTTILESWIDE-1;
			if (ytl<0)
			  ytl = 0;
			if (yth>=PORTTILESHIGH)
			  yth = PORTTILESHIGH-1;

			if (xtl>xth || ytl>yth)
				continue;

		//
		// see if it's visable area covers any non 0 update tiles
		//
			updatespot = baseupdatespot = updateptr + uwidthtable[ytl] + xtl;
			updatedelta = UPDATEWIDE - (xth-xtl+1);

			if (sprite->updatecount)
			{
				sprite->updatecount--;			// the sprite was just placed,
				goto redraw;					// so draw it for sure
			}

			for (y=ytl;y<=yth;y++)
			{
				for (x=xtl;x<=xth;x++)
					if (*updatespot++)
						goto redraw;
				updatespot += updatedelta;		// down to next line
			}
			continue;							// no need to update

redraw:
		//
		// set the tiles it covers to 3, because those tiles are being
		// updated
		//
			updatespot = baseupdatespot;
			for (y=ytl;y<=yth;y++)
			{
				for (x=xtl;x<=xth;x++)
					*updatespot++ = 3;
				updatespot += updatedelta;		// down to next line
			}
		//
		// draw it!
		//
			height = sprite->height;
			sourceofs = sprite->sourceofs;
			if (porty<0)
			{
				height += porty;					// clip top off
				sourceofs -= porty*sprite->width;
				porty = 0;
			}
			else if (porty+height>PORTSCREENHIGH)
			{
				height = PORTSCREENHIGH - porty;    // clip bottom off
			}

			dest = bufferofs + ylookup[porty] + portx;

			switch (sprite->draw)
			{
			case spritedraw:
				VW_MaskBlock(grsegs[sprite->grseg], sourceofs,
					dest,sprite->width,height,sprite->planesize);
				break;

			case maskdraw:
				break;

			}
#ifdef PROFILE
			updatecount++;
#endif


		}
	}
}


/*
=====================
=
= RF_Refresh        CGA
=
= All routines will draw at the port at bufferofs, possibly copying from
= the port at masterofs.  The EGA version then page flips, while the
= CGA version updates the screen from the buffer port.
=
= Screenpage is the currently displayed page, not the one being drawn
= Otherpage is the page to be worked with now
=
=====================
*/

void RF_Refresh (void)
{
	long newtime;

	RFL_AnimateTiles ();

//
// update newly scrolled on tiles and animated tiles from the master screen
//
	RFL_UpdateTiles ();
	RFL_EraseBlocks ();

//
// Update is all 0 except where sprites have changed or new area has
// been scrolled on.  Go through all sprites and update the ones that cover
// a non 0 update tile
//
	RFL_UpdateSprites ();

//
// if the main program has a refresh hook set, call their function before
// displaying the new page
//
	if (refreshvector)
		refreshvector();

//
// update everything to the screen
//
	VW_CGAFullUpdate ();

//
// calculate tics since last refresh for adaptive timing
//
	if (lasttimecount > TimeCount)
		lasttimecount = TimeCount;		// if the game was paused a LONG time
	do
	{
		newtime = TimeCount;
		tics = newtime-lasttimecount;
	} while (tics<MINTICS);
	lasttimecount = newtime;

#ifdef PROFILE
	itoa (tics,str,10);
	strcat (str,"\t");
	ltoa (TimeCount,str2,10);
	strcat (str,str2);
	strcat (str,"\t");
	ltoa (LocalTime,str2,10);
	strcat (str,str2);
	strcat (str,"\n");
	write (profile,str,strlen(str));
#endif
	if (tics>MAXTICS)
		tics = MAXTICS;

}

#endif		// GRMODE == CGAGR
