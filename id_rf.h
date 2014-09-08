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

// ID_RF.H

#define __ID_RF__

#ifndef __ID_MM__
#include "ID_MM.H"
#endif

/*
=============================================================================

							CONSTANTS

=============================================================================
*/

#define	MINTICS				2
#define	MAXTICS				6

#define	MAPBORDER			2		// map border must be at least 1

#define	MAXSPRITES			100		// max tracked sprites
#define	MAXANIMTILES		90		// max animating tiles on screen
#define MAXANIMTYPES		50		// max different unique anim tiles on map

#define	MAXMAPHEIGHT		128

#define	PRIORITIES			4
#define	MASKEDTILEPRIORITY	3		// planes go: 0,1,2,MTILES,3

#define TILEGLOBAL			256
#define PIXGLOBAL			16

#define	G_T_SHIFT			8		// global >> ?? = tile
#define	G_P_SHIFT			4		// global >> ?? = pixels
#define P_T_SHIFT			4		// pixels >> ?? = tile

#define	PORTTILESWIDE		21      // all drawing takes place inside a
#define	PORTTILESHIGH		14		// non displayed port of this size

//#define	PORTGLOBALWIDE		(21*TILEGLOBAL)
//#define	PORTGLOBALHIGH		(14*TILEGLOBAL)

#define UPDATEWIDE			(PORTTILESWIDE+1)
#define UPDATEHIGH			PORTTILESHIGH


//===========================================================================

typedef enum {spritedraw,maskdraw} drawtype;

/*
=============================================================================

						 PUBLIC VARIABLES

=============================================================================
*/


extern	boolean		compatability;			// crippled refresh for wierdo SVGAs

extern	unsigned	tics;
extern	long		lasttimecount;

extern	unsigned	originxglobal,originyglobal;
extern	unsigned	originxtile,originytile;
extern	unsigned	originxscreen,originyscreen;

extern	unsigned	mapwidth,mapheight,mapbyteswide,mapwordswide
					,mapbytesextra,mapwordsextra;
extern	unsigned	mapbwidthtable[MAXMAPHEIGHT];

extern	unsigned	originxmin,originxmax,originymin,originymax;

extern	unsigned	masterofs;

//
// the floating update window is also used by the view manager for
// double buffer tracking
//

extern	byte		*updateptr;				// current start of update window

#if GRMODE == CGAGR
extern	byte		*baseupdateptr;
#endif

extern unsigned	blockstarts[UPDATEWIDE*UPDATEHIGH];
extern unsigned	updatemapofs[UPDATEWIDE*UPDATEHIGH];
extern unsigned	uwidthtable[UPDATEHIGH];		// lookup instead of multiple

#define	UPDATETERMINATE	0x0301

/*
=============================================================================

						 PUBLIC FUNCTIONS

=============================================================================
*/

void RF_Startup (void);
void RF_Shutdown (void);

void RF_NewMap (void);
void RF_MarkTileGraphics (void);
void RF_NewPosition (unsigned x, unsigned y);
void RF_Scroll (int x, int y);

void RF_PlaceSprite (void **user,unsigned globalx,unsigned globaly,
	unsigned spritenumber, drawtype draw, int priority);
void RF_RemoveSprite (void **user);

void RF_Refresh (void);
void RF_ForceRefresh (void);
void RF_SetRefreshHook (void (*func) (void) );


