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

// KD_KEEN.C

#include "KD_DEF.H"
#pragma hdrstop

/*

player->temp1 = pausetime / pointer to zees when sleeping
player->temp2 = pausecount / stagecount
player->temp3 =
player->temp4 =


*/


/*
=============================================================================

						 LOCAL CONSTANTS

=============================================================================
*/

#define KEENPRIORITY	1

#define PLACESPRITE RF_PlaceSprite (&ob->sprite,ob->x,ob->y,ob->shapenum, \
	spritedraw,KEENPRIORITY);

#define	MINPOLEJUMPTICS	19	// wait tics before allowing a pole regram

#define SPDRUNJUMP		16
#define SPDPOLESIDEJUMP	8
#define WALKAIRSPEED	8
#define DIVESPEED		32
#define	JUMPTIME		16
#define	POLEJUMPTIME	10
#define	SPDJUMP			40
#define	SPDDIVEUP		16
#define	SPDPOLEJUMP		20

#define SPDPOWERUP		-64
#define SPDPOWER		64
#define SPDPOWERY		-20
#define POWERCOUNT		50

#define MAXXSPEED   24

/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/

int	singlegravity;
unsigned	bounceangle[8][8] =
{
{0,0,0,0,0,0,0,0},
{7,6,5,4,3,2,1,0},
{5,4,3,2,1,0,15,14},
{5,4,3,2,1,0,15,14},
{3,2,1,0,15,14,13,12},
{9,8,7,6,5,4,3,2},
{9,8,7,6,5,4,3,2},
{11,10,9,8,7,6,5,4}
};


/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/

int	jumptime;
long	leavepoletime;		// TimeCount when jumped off pole

/*
=============================================================================

						 SCORE BOX ROUTINES

=============================================================================
*/

void	SpawnScore (void);
void ScoreThink (objtype *ob);
void ScoreReact (objtype *ob);

void MemDrawChar (int char8,byte far *dest,unsigned width,unsigned planesize);

statetype s_score	= {NULL,NULL,think,false,
	false,0, 0,0, ScoreThink , NULL, ScoreReact, NULL};


/*
======================
=
= SpawnScore
=
======================
*/

void	SpawnScore (void)
{
	scoreobj->obclass = inertobj;
	scoreobj->active = allways;
	scoreobj->needtoclip = false;
	*((long *)&(scoreobj->temp1)) = -1;		// force score to be updated
	scoreobj->temp3 = -1;			// and flower power
	scoreobj->temp4 = -1;			// and lives
	NewState (scoreobj,&s_score);
}


void	FixScoreBox (void)
{
	unsigned	width, planesize;
	unsigned smallplane,bigplane;
	spritetype	_seg	*block;
	byte	far	*dest;

// draw boobus bomb if on level 15, else flower power
	block = (spritetype _seg *)grsegs[SCOREBOXSPR];
	width = block->width[0];
	planesize = block->planesize[0];
	dest = (byte far *)grsegs[SCOREBOXSPR]+block->sourceoffset[0]
		+ planesize + width*16 + 4*CHARWIDTH;
	if (mapon == 15)
	{
		MemDrawChar (20,dest,width,planesize);
		MemDrawChar (21,dest+CHARWIDTH,width,planesize);
		MemDrawChar (22,dest+width*8,width,planesize);
		MemDrawChar (23,dest+width*8+CHARWIDTH,width,planesize);
	}
	else
	{
		MemDrawChar (28,dest,width,planesize);
		MemDrawChar (29,dest+CHARWIDTH,width,planesize);
		MemDrawChar (30,dest+width*8,width,planesize);
		MemDrawChar (31,dest+width*8+CHARWIDTH,width,planesize);
	}

}


/*
======================
=
= MemDrawChar
=
======================
*/

#if GRMODE == EGAGR

void MemDrawChar (int char8,byte far *dest,unsigned width,unsigned planesize)
{
asm	mov	si,[char8]
asm	shl	si,1
asm	shl	si,1
asm	shl	si,1
asm	shl	si,1
asm	shl	si,1		// index into char 8 segment

asm	mov	ds,[WORD PTR grsegs+STARTTILE8*2]
asm	mov	es,[WORD PTR dest+2]

asm	mov	cx,4		// draw four planes
asm	mov	bx,[width]
asm	dec	bx

planeloop:

asm	mov	di,[WORD PTR dest]

asm	movsb
asm	add	di,bx
asm	movsb
asm	add	di,bx
asm	movsb
asm	add	di,bx
asm	movsb
asm	add	di,bx
asm	movsb
asm	add	di,bx
asm	movsb
asm	add	di,bx
asm	movsb
asm	add	di,bx
asm	movsb

asm	mov	ax,[planesize]
asm	add	[WORD PTR dest],ax

asm	loop	planeloop

asm	mov	ax,ss
asm	mov	ds,ax

}
#endif

#if GRMODE == CGAGR
void MemDrawChar (int char8,byte far *dest,unsigned width,unsigned planesize)
{
asm	mov	si,[char8]
asm	shl	si,1
asm	shl	si,1
asm	shl	si,1
asm	shl	si,1		// index into char 8 segment

asm	mov	ds,[WORD PTR grsegs+STARTTILE8*2]
asm	mov	es,[WORD PTR dest+2]

asm	mov	bx,[width]
asm	sub	bx,2

asm	mov	di,[WORD PTR dest]

asm	movsw
asm	add	di,bx
asm	movsw
asm	add	di,bx
asm	movsw
asm	add	di,bx
asm	movsw
asm	add	di,bx
asm	movsw
asm	add	di,bx
asm	movsw
asm	add	di,bx
asm	movsw
asm	add	di,bx
asm	movsw

asm	mov	ax,ss
asm	mov	ds,ax

	planesize++;		// shut the compiler up
}
#endif


/*
====================
=
= ShiftScore
=
====================
*/
#if GRMODE == EGAGR
void ShiftScore (void)
{
	spritetabletype far *spr;
	spritetype _seg *dest;

	spr = &spritetable[SCOREBOXSPR-STARTSPRITES];
	dest = (spritetype _seg *)grsegs[SCOREBOXSPR];

	CAL_ShiftSprite (FP_SEG(dest),dest->sourceoffset[0],
		dest->sourceoffset[1],spr->width,spr->height,2);

	CAL_ShiftSprite (FP_SEG(dest),dest->sourceoffset[0],
		dest->sourceoffset[2],spr->width,spr->height,4);

	CAL_ShiftSprite (FP_SEG(dest),dest->sourceoffset[0],
		dest->sourceoffset[3],spr->width,spr->height,6);
}
#endif

/*
===============
=
= ScoreThink
=
===============
*/

void ScoreThink (objtype *ob)
{
	char		str[10],*ch;
	spritetype	_seg	*block;
	byte		far *dest;
	unsigned	i, length, width, planesize, number;

//
// score changed
//
	if ((gamestate.score>>16) != ob->temp1
		|| (unsigned)gamestate.score != ob->temp2 )
	{
		block = (spritetype _seg *)grsegs[SCOREBOXSPR];
		width = block->width[0];
		planesize = block->planesize[0];
		dest = (byte far *)grsegs[SCOREBOXSPR]+block->sourceoffset[0]
			+ planesize + width*4 + 1*CHARWIDTH;

		ltoa (gamestate.score,str,10);

		// erase leading spaces
		length = strlen(str);
		for (i=6;i>length;i--)
			MemDrawChar (0,dest+=CHARWIDTH,width,planesize);

		// draw digits
		ch = str;
		while (*ch)
			MemDrawChar (*ch++ - '0'+1,dest+=CHARWIDTH,width,planesize);

#if GRMODE == EGAGR
		ShiftScore ();
#endif
		ob->needtoreact = true;
		ob->temp1 = gamestate.score>>16;
		ob->temp2 = gamestate.score;
	}

//
// flower power changed
//
	if (mapon == 15)
		number = gamestate.boobusbombs;
	else
		number = gamestate.flowerpowers;
	if (number != ob->temp3)
	{
		block = (spritetype _seg *)grsegs[SCOREBOXSPR];
		width = block->width[0];
		planesize = block->planesize[0];
		dest = (byte far *)grsegs[SCOREBOXSPR]+block->sourceoffset[0]
			+ planesize + width*20 + 5*CHARWIDTH;

		if (number > 99)
			strcpy (str,"99");
		else
			ltoa (number,str,10);

		// erase leading spaces
		length = strlen(str);
		for (i=2;i>length;i--)
			MemDrawChar (0,dest+=CHARWIDTH,width,planesize);

		// draw digits
		ch = str;
		while (*ch)
			MemDrawChar (*ch++ - '0'+1,dest+=CHARWIDTH,width,planesize);

#if GRMODE == EGAGR
		ShiftScore ();
#endif
		ob->needtoreact = true;
		ob->temp3 = gamestate.flowerpowers;
	}

//
// lives changed
//
	if (gamestate.lives != ob->temp4)
	{
		block = (spritetype _seg *)grsegs[SCOREBOXSPR];
		width = block->width[0];
		planesize = block->planesize[0];
		dest = (byte far *)grsegs[SCOREBOXSPR]+block->sourceoffset[0]
			+ planesize + width*20 + 2*CHARWIDTH;

		if (gamestate.lives>9)
			MemDrawChar ('9' - '0'+1,dest,width,planesize);
		else
			MemDrawChar (gamestate.lives +1,dest,width,planesize);

#if GRMODE == EGAGR
		ShiftScore ();
#endif
		ob->needtoreact = true;
		ob->temp4 = gamestate.lives;
	}

	if (originxglobal != ob->x || originyglobal != ob->y)
		ob->needtoreact = true;
}


/*
===============
=
= ScoreReact
=
===============
*/

void ScoreReact (objtype *ob)
{
	ob->x = originxglobal;
	ob->y = originyglobal;

#if GRMODE == EGAGR
	RF_PlaceSprite (&ob->sprite
		,ob->x+4*PIXGLOBAL
		,ob->y+4*PIXGLOBAL
		,SCOREBOXSPR
		,spritedraw
		,PRIORITIES-1);
#endif
#if GRMODE == CGAGR
	RF_PlaceSprite (&ob->sprite
		,ob->x+8*PIXGLOBAL
		,ob->y+8*PIXGLOBAL
		,SCOREBOXSPR
		,spritedraw
		,PRIORITIES-1);
#endif
}


/*
=============================================================================

						FLOWER POWER ROUTINES

temp1 = 8, same as power bonus
temp2 = initial direction

=============================================================================
*/

void	PowerCount			(objtype *ob);
void	PowerContact 		(objtype *ob, objtype *hit);
void	PowerReact			(objtype *ob);

extern	statetype s_flowerpower1;
extern	statetype s_flowerpower2;

extern	statetype s_boobusbomb1;
extern	statetype s_boobusbomb2;

extern	statetype s_bombexplode;
extern	statetype s_bombexplode2;
extern	statetype s_bombexplode3;
extern	statetype s_bombexplode4;
extern	statetype s_bombexplode5;
extern	statetype s_bombexplode6;

extern	statetype s_powerblink1;
extern	statetype s_powerblink2;

statetype s_flowerpower1	= {FLOWERPOWER1SPR,FLOWERPOWER1SPR,stepthink,false,
	false,10, 0,0, ProjectileThink, PowerContact, PowerReact, &s_flowerpower2};
statetype s_flowerpower2	= {FLOWERPOWER2SPR,FLOWERPOWER2SPR,stepthink,false,
	false,10, 0,0, ProjectileThink, PowerContact, PowerReact, &s_flowerpower1};

statetype s_boobusbomb1	= {BOOBUSBOMB1SPR,BOOBUSBOMB1SPR,stepthink,false,
	false,5, 0,0, ProjectileThink, PowerContact, PowerReact, &s_boobusbomb2};
statetype s_boobusbomb2	= {BOOBUSBOMB3SPR,BOOBUSBOMB3SPR,stepthink,false,
	false,5, 0,0, ProjectileThink, PowerContact, PowerReact, &s_boobusbomb1};

statetype s_bombexplode	= {BOOBUSBOOM1SPR,BOOBUSBOOM1SPR,step,false,
	false,5, 0,0, NULL, NULL, DrawReact, &s_bombexplode2};
statetype s_bombexplode2= {BOOBUSBOOM2SPR,BOOBUSBOOM2SPR,step,false,
	false,5, 0,0, NULL, NULL, DrawReact, &s_bombexplode3};
statetype s_bombexplode3= {BOOBUSBOOM1SPR,BOOBUSBOOM1SPR,step,false,
	false,5, 0,0, NULL, NULL, DrawReact, &s_bombexplode4};
statetype s_bombexplode4= {BOOBUSBOOM2SPR,BOOBUSBOOM2SPR,step,false,
	false,5, 0,0, NULL, NULL, DrawReact, &s_bombexplode5};
statetype s_bombexplode5= {BOOBUSBOOM1SPR,BOOBUSBOOM1SPR,step,false,
	false,5, 0,0, NULL, NULL, DrawReact, &s_bombexplode6};
statetype s_bombexplode6= {BOOBUSBOOM2SPR,BOOBUSBOOM2SPR,step,false,
	false,5, 0,0, NULL, NULL, DrawReact, NULL};

statetype s_powerblink1	= {FLOWERPOWER1SPR,FLOWERPOWER1SPR,step,false,
	false,5, 0,0, PowerCount, NULL, DrawReact, &s_powerblink2};
statetype s_powerblink2	= {-1,-1,step,false,
	false,5, 0,0, PowerCount, NULL, DrawReact, &s_powerblink1};


/*
===============
=
= ThrowPower
=
===============
*/

void ThrowPower (unsigned x, unsigned y, int dir)
{
	statetype *startstate;

	if (mapon == 15)
	{
		if (!gamestate.boobusbombs)
		{
			SD_PlaySound (NOWAYSND);
			return;						// no bombs to throw
		}
		SD_PlaySound (THROWBOMBSND);
		gamestate.bombsthislevel--;
		gamestate.boobusbombs--;
	}
	else
	{
		if (!gamestate.flowerpowers)
		{
			SD_PlaySound (NOWAYSND);
			return;						// no flower power to throw
		}
		SD_PlaySound (THROWSND);
		gamestate.flowerpowers--;
	}



	GetNewObj (true);
	new->obclass = powerobj;
	new->temp2 = dir;
	new->x = x;
	new->y = y;
	new->tileleft = new->tileright = x>>G_T_SHIFT;
	new->tiletop = new->tilebottom = y>>G_T_SHIFT;
	new->ydir = -1;

	switch (dir)
	{
	case dir_North:
		new->xspeed = 0;
		new->yspeed = SPDPOWERUP;
		break;
	case dir_East:
		new->xspeed = SPDPOWER;
		new->yspeed = SPDPOWERY;
		break;
	case dir_South:
		new->xspeed = 0;
		new->yspeed = SPDPOWER;
		break;
	case dir_West:
		new->xspeed = -SPDPOWER;
		new->yspeed = SPDPOWERY;
		break;
	default:
		Quit ("ThrowPower: Bad dir!");
	}

	if (mapon != 15)
	{
		new->temp1 = 8;  				// flower power bonus
		startstate = &s_flowerpower1;
	}
	else
	{
		new->temp1 = 10;  				// boobus bomb bonus
		startstate = &s_boobusbomb1;
	}
	new->active = removable;

#if 0
	new->x -= 5*new->xspeed;		// make sure they hit nearby walls
	new->y -= 5*new->yspeed;
#endif
	NewState (new,startstate);
#if 0
	new->xmove = 5*new->xspeed;
	new->ymove = 5*new->yspeed;
	ClipToWalls (new);
#endif
}

/*
============================
=
= PowerCount
=
============================
*/

void	PowerCount (objtype *ob)
{
	ob->temp2+=tics;

	ob->shapenum = 0;

	if (ob->temp2 > POWERCOUNT)
		RemoveObj(ob);
}


/*
============================
=
= CalcSingleGravity
=
============================
*/

void	CalcSingleGravity (void)
{
	unsigned	speed;
	long	i;
//
// only accelerate on odd tics, because of limited precision
//
	speed = 0;
	singlegravity = 0;
	for (i=lasttimecount-tics;i<lasttimecount;i++)
	{
		if (i&1)
		{
			speed+=ACCGRAVITY;
			if (speed>SPDMAXY)
			  speed=SPDMAXY;
		}
		singlegravity+=speed;
	}

	singlegravity/=2;
}


/*
============================
=
= PowerContact
=
============================
*/

void	PowerContact (objtype *ob, objtype *hit)
{
	unsigned	x,y,yspot,xspot;

	switch (hit->obclass)
	{
	case	broccoobj:
	case	tomatobj:
	case	carrotobj:
	case	celeryobj:
	case	asparobj:
	case	taterobj:
	case	frenchyobj:
	case	squashobj:
	case	apelobj:
	case	peapodobj:
	case	peabrainobj:
		ChangeToFlower (hit);
		RemoveObj (ob);
		break;
	case	boobusobj:
		if (!--hit->temp4)
		{
		// killed boobus
			GivePoints (50000);
			GivePoints (50000);
			hit->obclass = inertobj;
			ChangeState (hit,&s_boobusdie);
			hit->temp1 = 0;		// death count
		}
		SD_PlaySound (BOMBBOOMSND);
		ChangeState (ob,&s_bombexplode);
		break;
	}
}


/*
============================
=
= PowerReact
=
============================
*/

void	PowerReact (objtype *ob)
{
	unsigned wall,absx,absy,angle,newangle;
	unsigned long speed;

	PLACESPRITE;
	if (ob->hiteast || ob->hitwest)
	{
		ob->xspeed= -ob->xspeed/2;
		ob->obclass = bonusobj;
	}

	if (ob->hitsouth)
	{
		if ( ob->hitsouth == 17)	// go through pole holes
		{
			if (ob->temp2 != dir_North)
				ob->obclass = bonusobj;
			ob->top-=32;
			ob->y-=32;
			ob->xspeed = 0;
			ob->x = ob->tilemidx*TILEGLOBAL + 4*PIXGLOBAL;
		}
		else
		{
			ob->yspeed= -ob->yspeed/2;
		}
		return;
	}

	wall = ob->hitnorth;
	if ( wall == 17)	// go through pole holes
	{
		if (ob->temp2 != dir_South)
			ob->obclass = bonusobj;
		ob->bottom+=32;
		ob->y+=32;
		ob->xspeed = 0;
		ob->x = ob->tilemidx*TILEGLOBAL + 4*PIXGLOBAL;
	}
	else if (wall)
	{
		ob->obclass = bonusobj;
		if (ob->yspeed < 0)
			ob->yspeed = 0;

		absx = abs(ob->xspeed);
		absy = ob->yspeed;
		if (absx>absy)
		{
			if (absx>absy*2)	// 22 degrees
			{
				angle = 0;
				speed = absx*286;	// x*sqrt(5)/2
			}
			else				// 45 degrees
			{
				angle = 1;
				speed = absx*362;	// x*sqrt(2)
			}
		}
		else
		{
			if (absy>absx*2)	// 90 degrees
			{
				angle = 3;
				speed = absy*256;
			}
			else
			{
				angle = 2;		// 67 degrees
				speed = absy*286;	// y*sqrt(5)/2
			}
		}
		if (ob->xspeed > 0)
			angle = 7-angle;

		speed >>= 1;
		newangle = bounceangle[ob->hitnorth][angle];
		switch (newangle)
		{
		case 0:
			ob->xspeed = speed / 286;
			ob->yspeed = -ob->xspeed / 2;
			break;
		case 1:
			ob->xspeed = speed / 362;
			ob->yspeed = -ob->xspeed;
			break;
		case 2:
			ob->yspeed = -(speed / 286);
			ob->xspeed = -ob->yspeed / 2;
			break;
		case 3:

		case 4:
			ob->xspeed = 0;
			ob->yspeed = -(speed / 256);
			break;
		case 5:
			ob->yspeed = -(speed / 286);
			ob->xspeed = ob->yspeed / 2;
			break;
		case 6:
			ob->xspeed = ob->yspeed = -(speed / 362);
			break;
		case 7:
			ob->xspeed = -(speed / 286);
			ob->yspeed = ob->xspeed / 2;
			break;

		case 8:
			ob->xspeed = -(speed / 286);
			ob->yspeed = -ob->xspeed / 2;
			break;
		case 9:
			ob->xspeed = -(speed / 362);
			ob->yspeed = -ob->xspeed;
			break;
		case 10:
			ob->yspeed = speed / 286;
			ob->xspeed = -ob->yspeed / 2;
			break;
		case 11:

		case 12:
			ob->xspeed = 0;
			ob->yspeed = -(speed / 256);
			break;
		case 13:
			ob->yspeed = speed / 286;
			ob->xspeed = ob->yspeed / 2;
			break;
		case 14:
			ob->xspeed = speed / 362;
			ob->yspeed = speed / 362;
			break;
		case 15:
			ob->xspeed = speed / 286;
			ob->yspeed = ob->xspeed / 2;
			break;
		}

		if (speed < 256*16)
		{
			if (mapon == 15)
			{
				ob->ydir = -1;			// bonus stuff flies up when touched
				ob->temp2 = ob->shapenum = BOOBUSBOMB1SPR;
				ob->temp3 = ob->temp2 + 2;
				ob->needtoclip = 0;
				ChangeState (ob,&s_bonus);
			}
			else
			{
				ChangeState (ob,&s_powerblink1);
				ob->needtoclip = 0;
				ob->temp2 = 0;		// blink time
			}
		}
	}
}


/*
=============================================================================

							   MINI KEEN

player->temp1 = dir
player->temp2 = animation stage

#define WORLDKEENSLEEP1SPR		238
#define WORLDKEENSLEEP2SPR		239
#define WORLDKEENWAVE1SPR		240
#define WORLDKEENWAVE2SPR		241
=============================================================================
*/

void	SpawnWorldKeen (int tilex, int tiley);
void	KeenWorldThink		(objtype *ob);
void	KeenWorldWalk		(objtype *ob);

extern	statetype s_worldkeen;

extern	statetype s_worldkeenwave1;
extern	statetype s_worldkeenwave2;
extern	statetype s_worldkeenwave3;
extern	statetype s_worldkeenwave4;
extern	statetype s_worldkeenwave5;

extern	statetype s_worldkeenwait;

extern	statetype s_worldkeensleep1;
extern	statetype s_worldkeensleep2;

extern	statetype s_worldwalk;

#pragma warn -sus

statetype s_worldkeen	= {NULL,NULL,stepthink,false,
	false,360, 0,0, KeenWorldThink, NULL, DrawReact, &s_worldkeenwave1};

statetype s_worldkeenwave1= {WORLDKEENWAVE1SPR,WORLDKEENWAVE1SPR,stepthink,false,
	false,20, 0,0, KeenWorldThink, NULL, DrawReact, &s_worldkeenwave2};
statetype s_worldkeenwave2= {WORLDKEENWAVE2SPR,WORLDKEENWAVE2SPR,stepthink,false,
	false,20, 0,0, KeenWorldThink, NULL, DrawReact, &s_worldkeenwave3};
statetype s_worldkeenwave3= {WORLDKEENWAVE1SPR,WORLDKEENWAVE1SPR,stepthink,false,
	false,20, 0,0, KeenWorldThink, NULL, DrawReact, &s_worldkeenwave4};
statetype s_worldkeenwave4= {WORLDKEENWAVE2SPR,WORLDKEENWAVE2SPR,stepthink,false,
	false,20, 0,0, KeenWorldThink, NULL, DrawReact, &s_worldkeenwave5};
statetype s_worldkeenwave5= {WORLDKEENWAVE1SPR,WORLDKEENWAVE1SPR,stepthink,false,
	false,20, 0,0, KeenWorldThink, NULL, DrawReact, &s_worldkeenwait};

statetype s_worldkeenwait	= {WORLDKEEND3SPR,WORLDKEEND3SPR,stepthink,false,
	false,400, 0,0, KeenWorldThink, NULL, DrawReact, &s_worldkeensleep1};

statetype s_worldkeensleep1	= {WORLDKEENSLEEP1SPR,WORLDKEENSLEEP1SPR,stepthink,false,
	false,30, 0,0, KeenWorldThink, NULL, DrawReact, &s_worldkeensleep2};
statetype s_worldkeensleep2	= {WORLDKEENSLEEP2SPR,WORLDKEENSLEEP2SPR,stepthink,false,
	false,90, 0,0, KeenWorldThink, NULL, DrawReact, &s_worldkeensleep2};

statetype s_worldwalk	= {NULL,NULL,slide,false,
	false,4, 16,16, KeenWorldWalk, NULL, DrawReact, &s_worldwalk};

#pragma warn +sus

/*
======================
=
= SpawnWorldKeen
=
======================
*/

void	SpawnWorldKeen (int tilex, int tiley)
{
	player->obclass = keenobj;
	if (!gamestate.worldx)
	{
		player->x = tilex<<G_T_SHIFT;
		player->y = tiley<<G_T_SHIFT;
	}
	else
	{
		player->x = gamestate.worldx;
		player->y = gamestate.worldy;
	}

	player->active = yes;
	player->needtoclip = true;
	player->xdir = 0;
	player->ydir = 0;
	player->temp1 = dir_West;
	player->temp2 = 3;
	player->shapenum = 3+WORLDKEENL1SPR-1;				// feet together
	NewState (player,&s_worldkeen);
}


/*
======================
=
= CheckEnterLevel
=
======================
*/

void	CheckEnterLevel (objtype *ob)
{
	int	x,y,tile;

	for (y=ob->tiletop;y<=ob->tilebottom;y++)
		for (x=ob->tileleft;x<=ob->tileright;x++)
		{
			tile = *((unsigned _seg *)mapsegs[2]+mapbwidthtable[y]/2+x);
			if (tile >= 3 && tile <=18)
			{
			//
			// enter level!
			//
				if (tile == 17)
				{
					if (gamestate.boobusbombs < 12)
					{
						VW_FixRefreshBuffer ();
						US_CenterWindow (26,6);
						PrintY+= 8;
						US_CPrint ("You can't possibly\n"
								   "defeat King Boobus Tuber\n"
								   "with less than 12 bombs!");
						VW_UpdateScreen ();
						IN_Ack ();
						RF_ForceRefresh ();
						return;
					}
				}
				gamestate.worldx = ob->x;
				gamestate.worldy = ob->y;
				gamestate.mapon = tile-2;
				playstate = levelcomplete;
				SD_PlaySound (ENTERLEVELSND);
			}
		}
}


/*
======================
=
= KeenWorldThink
=
======================
*/

void	KeenWorldThink (objtype *ob)
{
	if (c.dir!=dir_None)
	{
		ob->state = &s_worldwalk;
		ob->temp2 = 0;
		KeenWorldWalk (ob);
	}

	if (c.button0 || c.button1)
		CheckEnterLevel(ob);
}


/*
======================
=
= KeenWorldWalk
=
======================
*/

int worldshapes[8] = {WORLDKEENU1SPR-1,WORLDKEENUR1SPR-1,WORLDKEENR1SPR-1,
	WORLDKEENDR1SPR-1,WORLDKEEND1SPR-1,WORLDKEENDL1SPR-1,WORLDKEENL1SPR-1,
	WORLDKEENUL1SPR-1};

int worldanims[4] ={2,3,1,3};

void	KeenWorldWalk (objtype *ob)
{
	ob->xdir = c.xaxis;
	ob->ydir = c.yaxis;

	if (++ob->temp2==4)
		ob->temp2 = 0;

	if (c.button0 || c.button1)
		CheckEnterLevel(ob);

	if (c.dir == dir_None)
	{
		ob->state = &s_worldkeen;
		ob->shapenum = worldshapes[ob->temp1]+3;
		return;
	}

	ob->temp1 = c.dir;
	ob->shapenum = worldshapes[ob->temp1]+worldanims[ob->temp2];
}


/*
=============================================================================

								KEEN

player->temp1 = pausetime / pointer to zees when sleeping
player->temp2 = pausecount / stagecount
player->temp3 =
player->temp4 = pole x location

=============================================================================
*/

void	KeenStandThink		(objtype *ob);
void	KeenPauseThink		(objtype *ob);
void	KeenGoSleepThink	(objtype *ob);
void	KeenSleepThink		(objtype *ob);
void	KeenDieThink		(objtype *ob);
void	KeenDuckThink		(objtype *ob);
void	KeenDropDownThink	(objtype *ob);
void	KeenWalkThink		(objtype *ob);
void	KeenAirThink		(objtype *ob);
void	KeenPoleThink		(objtype *ob);
void	KeenClimbThink		(objtype *ob);
void	KeenDropThink		(objtype *ob);
void	KeenDive			(objtype *ob);
void	KeenThrow			(objtype *ob);

void	KeenContact 		(objtype *ob, objtype *hit);
void	KeenSimpleReact		(objtype *ob);
void	KeenStandReact		(objtype *ob);
void	KeenWalkReact		(objtype *ob);
void	KeenPoleReact		(objtype *ob);
void	KeenAirReact		(objtype *ob);
void	KeenDiveReact		(objtype *ob);
void	KeenSlideReact		(objtype *ob);

//-------------------

extern	statetype s_keenzee1;
extern	statetype s_keenzee2;
extern	statetype s_keenzee3;
extern	statetype s_keenzee4;
extern	statetype s_keenzee5;

//-------------------

extern	statetype	s_keenstand;
extern	statetype s_keenpauselook;
extern	statetype s_keenyawn1;
extern	statetype s_keenyawn2;
extern	statetype s_keenyawn3;
extern	statetype s_keenyawn4;

extern	statetype s_keenwait1;
extern	statetype s_keenwait2;
extern	statetype s_keenwait3;
extern	statetype s_keenwait4;
extern	statetype s_keenwait5;
extern	statetype s_keenwait6;

extern	statetype s_keenmoon1;
extern	statetype s_keenmoon2;
extern	statetype s_keenmoon3;

extern	statetype s_keengosleep1;
extern	statetype s_keengosleep2;
extern	statetype s_keensleep1;
extern	statetype s_keensleep2;

extern	statetype s_keendie1;
extern	statetype s_keendie2;
extern	statetype s_keendie3;
extern	statetype s_keendie4;

extern	statetype	s_keenlookup;
extern	statetype	s_keenduck;
extern	statetype	s_keendrop;

//-------------------

extern	statetype s_keenreach;

extern	statetype s_keenpole;

extern	statetype s_keenclimb1;
extern	statetype s_keenclimb2;
extern	statetype s_keenclimb3;

extern	statetype s_keenslide1;
extern	statetype s_keenslide2;
extern	statetype s_keenslide3;
extern	statetype s_keenslide4;

extern	statetype s_keenpolethrow1;
extern	statetype s_keenpolethrow2;
extern	statetype s_keenpolethrow3;

extern	statetype s_keenpolethrowup1;
extern	statetype s_keenpolethrowup2;
extern	statetype s_keenpolethrowup3;

extern	statetype s_keenpolethrowdown1;
extern	statetype s_keenpolethrowdown2;
extern	statetype s_keenpolethrowdown3;

//-------------------

extern	statetype	s_keenwalk1;
extern	statetype	s_keenwalk2;
extern	statetype	s_keenwalk3;
extern	statetype	s_keenwalk4;

extern	statetype	s_keenthrow1;
extern	statetype	s_keenthrow2;
extern	statetype	s_keenthrow3;
extern	statetype	s_keenthrow4;

extern	statetype	s_keenthrowup1;
extern	statetype	s_keenthrowup2;
extern	statetype	s_keenthrowup3;

extern	statetype	s_keendive1;
extern	statetype	s_keendive2;
extern	statetype	s_keendive3;

//-------------------

extern	statetype s_keenjumpup1;
extern	statetype s_keenjumpup2;
extern	statetype s_keenjumpup3;

extern	statetype s_keenjump1;
extern	statetype s_keenjump2;
extern	statetype s_keenjump3;

extern	statetype s_keenairthrow1;
extern	statetype s_keenairthrow2;
extern	statetype s_keenairthrow3;

extern	statetype s_keenairthrowup1;
extern	statetype s_keenairthrowup2;
extern	statetype s_keenairthrowup3;

extern	statetype s_keenairthrowdown1;
extern	statetype s_keenairthrowdown2;
extern	statetype s_keenairthrowdown3;

#pragma warn -sus

//-------------------

statetype s_keenzee1	= {KEENZEES1SPR,KEENZEES1SPR,step,false,
	false,30, 0,0, NULL, NULL, DrawReact, &s_keenzee2};
statetype s_keenzee2	= {KEENZEES2SPR,KEENZEES2SPR,step,false,
	false,30, 0,0, NULL, NULL, DrawReact, &s_keenzee3};
statetype s_keenzee3	= {KEENZEES3SPR,KEENZEES3SPR,step,false,
	false,30, 0,0, NULL, NULL, DrawReact, &s_keenzee1};

//-------------------

statetype s_keenstand	= {KEENSTANDLSPR,KEENSTANDRSPR,stepthink,false,
	true,4, 0,16, KeenPauseThink, KeenContact, KeenStandReact, &s_keenstand};
statetype s_keenpauselook= {KEENLOOKULSPR,KEENLOOKURSPR,stepthink,false,
	true,60, 0,0, KeenPauseThink, KeenContact, KeenStandReact, &s_keenstand};
statetype s_keenyawn1	= {KEENYAWN1SPR,KEENYAWN1SPR,stepthink,false,
	true,40, 0,0, KeenPauseThink, KeenContact, KeenStandReact, &s_keenyawn2};
statetype s_keenyawn2	= {KEENYAWN2SPR,KEENYAWN2SPR,stepthink,false,
	true,40, 0,0, KeenPauseThink, KeenContact, KeenStandReact, &s_keenyawn3};
statetype s_keenyawn3	= {KEENYAWN3SPR,KEENYAWN3SPR,stepthink,false,
	true,40, 0,0, KeenPauseThink, KeenContact, KeenStandReact, &s_keenyawn4};
statetype s_keenyawn4	= {KEENYAWN4SPR,KEENYAWN4SPR,stepthink,false,
	true,40, 0,0, KeenPauseThink, KeenContact, KeenStandReact, &s_keenstand};

statetype s_keenwait1	= {KEENWAITL2SPR,KEENWAITR2SPR,stepthink,false,
	true,90, 0,0, KeenPauseThink, KeenContact, KeenStandReact, &s_keenwait2};
statetype s_keenwait2	= {KEENWAITL1SPR,KEENWAITR1SPR,stepthink,false,
	true,10, 0,0, KeenPauseThink, KeenContact, KeenStandReact, &s_keenwait3};
statetype s_keenwait3	= {KEENWAITL2SPR,KEENWAITR2SPR,stepthink,false,
	true,90, 0,0, KeenPauseThink, KeenContact, KeenStandReact, &s_keenwait4};
statetype s_keenwait4	= {KEENWAITL1SPR,KEENWAITR1SPR,stepthink,false,
	true,10, 0,0, KeenPauseThink, KeenContact, KeenStandReact, &s_keenwait5};
statetype s_keenwait5	= {KEENWAITL2SPR,KEENWAITR2SPR,stepthink,false,
	true,90, 0,0, KeenPauseThink, KeenContact, KeenStandReact, &s_keenwait6};
statetype s_keenwait6	= {KEENWAITL3SPR,KEENWAITR3SPR,stepthink,false,
	true,70, 0,0, KeenPauseThink, KeenContact, KeenStandReact, &s_keenstand};

statetype s_keengosleep1= {KEENSLEEP1SPR,KEENSLEEP1SPR,stepthink,false,
	true,20, 0,0, KeenPauseThink, KeenContact, KeenStandReact, &s_keengosleep2};
statetype s_keengosleep2= {KEENSLEEP2SPR,KEENSLEEP2SPR,step,false,
	true,20, 0,0, KeenGoSleepThink, KeenContact, KeenStandReact, &s_keensleep1};
statetype s_keensleep1= {KEENSLEEP3SPR,KEENSLEEP3SPR,stepthink,false,
	true,120, 0,0, KeenSleepThink, KeenContact, KeenStandReact, &s_keensleep2};
statetype s_keensleep2= {KEENSLEEP4SPR,KEENSLEEP4SPR,stepthink,false,
	true,120, 0,0, KeenSleepThink, KeenContact, KeenStandReact, &s_keensleep1};

statetype s_keengetup	= {KEENGETUPLSPR,KEENGETUPRSPR,step,false,
	true,15, 0,0, NULL, KeenContact, KeenSimpleReact, &s_keenstand};

statetype s_keendie1		= {KEENDREAM1SPR,KEENDREAM1SPR,step,false,
	false,20, 0,0, NULL, NULL, KeenSimpleReact, &s_keendie2};
statetype s_keendie2		= {KEENDREAM2SPR,KEENDREAM2SPR,step,false,
	false,20, 0,0, NULL, NULL, KeenSimpleReact, &s_keendie3};
statetype s_keendie3		= {KEENDREAM3SPR,KEENDREAM3SPR,step,false,
	false,120, 0,0, KeenDieThink, NULL, KeenSimpleReact, &s_keendie3};

statetype s_keenlookup	= {KEENLOOKULSPR,KEENLOOKURSPR,think,false,
	true,0, 0,0, KeenStandThink, KeenContact, KeenStandReact, &s_keenlookup};

statetype s_keenduck	= {KEENDUCKLSPR,KEENDUCKRSPR,think,false,
	true,0, 0,0, KeenDuckThink, KeenContact, KeenStandReact, &s_keenduck};

statetype s_keendrop	= {KEENDUCKLSPR,KEENDUCKRSPR,step,false,
	false,0, 0,0, KeenDropDownThink, KeenContact, KeenSimpleReact, &s_keenjumpup3};

//-------------------

statetype s_keenpole	= {KEENSHINNYL1SPR,KEENSHINNYR1SPR,think,false,
	false,0, 0,0, KeenPoleThink, KeenContact, KeenSimpleReact, &s_keenpole};

statetype s_keenclimb1	= {KEENSHINNYL1SPR,KEENSHINNYR1SPR,slidethink,false,
	false,8, 0,8, KeenClimbThink, KeenContact, KeenSimpleReact, &s_keenclimb2};
statetype s_keenclimb2	= {KEENSHINNYL2SPR,KEENSHINNYR2SPR,slidethink,false,
	false,8, 0,8, KeenClimbThink, KeenContact, KeenSimpleReact, &s_keenclimb3};
statetype s_keenclimb3	= {KEENSHINNYL3SPR,KEENSHINNYR3SPR,slidethink,false,
	false,8, 0,8, KeenClimbThink, KeenContact, KeenSimpleReact, &s_keenclimb1};

statetype s_keenslide1	= {KEENSLIDED1SPR,KEENSLIDED1SPR,slide,false,
	false,8, 0,24, KeenDropThink, KeenContact, KeenSimpleReact, &s_keenslide2};
statetype s_keenslide2	= {KEENSLIDED2SPR,KEENSLIDED2SPR,slide,false,
	false,8, 0,24, KeenDropThink, KeenContact, KeenSimpleReact, &s_keenslide3};
statetype s_keenslide3	= {KEENSLIDED3SPR,KEENSLIDED3SPR,slide,false,
	false,8, 0,24, KeenDropThink, KeenContact, KeenSimpleReact, &s_keenslide4};
statetype s_keenslide4	= {KEENSLIDED4SPR,KEENSLIDED4SPR,slide,false,
	false,8, 0,24, KeenDropThink, KeenContact, KeenSimpleReact, &s_keenslide1};

statetype s_keenpolethrow1	= {KEENPTHROWL1SPR,KEENPTHROWR1SPR,step,false,
	false,8, 0,0, NULL, KeenContact, KeenSimpleReact, &s_keenpolethrow2};
statetype s_keenpolethrow2	= {KEENPTHROWL2SPR,KEENPTHROWR2SPR,step,true,
	false,1, 0,0, KeenThrow, KeenContact, KeenSimpleReact, &s_keenpolethrow3};
statetype s_keenpolethrow3	= {KEENPTHROWL2SPR,KEENPTHROWR2SPR,step,false,
	false,10, 0,0, NULL, KeenContact, KeenSimpleReact, &s_keenpole};

statetype s_keenpolethrowup1 = {KEENPLTHROWU1SPR,KEENPRTHROWU1SPR,step,false,
	false,8, 0,0, NULL, KeenContact, KeenSimpleReact, &s_keenpolethrowup2};
statetype s_keenpolethrowup2 = {KEENPLTHROWU2SPR,KEENPRTHROWU2SPR,step,true,
	false,1, 0,0, KeenThrow, KeenContact, KeenSimpleReact, &s_keenpolethrowup3};
statetype s_keenpolethrowup3 = {KEENPLTHROWU2SPR,KEENPRTHROWU2SPR,step,false,
	false,10, 0,0, NULL, KeenContact, KeenSimpleReact, &s_keenpole};

statetype s_keenpolethrowdown1 = {KEENPLTHROWD1SPR,KEENPRTHROWD1SPR,step,false,
	false,8, 0,0, NULL, KeenContact, KeenSimpleReact, &s_keenpolethrowdown2};
statetype s_keenpolethrowdown2 = {KEENPLTHROWD2SPR,KEENPRTHROWD2SPR,step,true,
	false,1, 0,0, KeenThrow, KeenContact, KeenSimpleReact, &s_keenpolethrowdown3};
statetype s_keenpolethrowdown3 = {KEENPLTHROWD2SPR,KEENPRTHROWD2SPR,step,false,
	false,10, 0,0, NULL, KeenContact, KeenSimpleReact, &s_keenpole};


//-------------------

statetype s_keenwalk1	= {KEENRUNL1SPR,KEENRUNR1SPR,slidethink,true,
	true,6, 24,0, KeenWalkThink, KeenContact, KeenWalkReact, &s_keenwalk2};
statetype s_keenwalk2	= {KEENRUNL2SPR,KEENRUNR2SPR,slidethink,true,
	true,6, 24,0, KeenWalkThink, KeenContact, KeenWalkReact, &s_keenwalk3};
statetype s_keenwalk3	= {KEENRUNL3SPR,KEENRUNR3SPR,slidethink,true,
	true,6, 24,0, KeenWalkThink, KeenContact, KeenWalkReact, &s_keenwalk4};
statetype s_keenwalk4	= {KEENRUNL4SPR,KEENRUNR4SPR,slidethink,true,
	true,6, 24,0, KeenWalkThink, KeenContact, KeenWalkReact, &s_keenwalk1};

statetype s_keenthrow1	= {KEENTHROWL1SPR,KEENTHROWR1SPR,step,true,
	true,4, 0,0, NULL, KeenContact, KeenStandReact, &s_keenthrow2};
statetype s_keenthrow2	= {KEENTHROWL2SPR,KEENTHROWR2SPR,step,false,
	true,4, 0,0, NULL, KeenContact, KeenStandReact, &s_keenthrow3};
statetype s_keenthrow3	= {KEENTHROWL3SPR,KEENTHROWR3SPR,step,true,
	true,1, 0,0, KeenThrow, KeenContact, KeenStandReact, &s_keenthrow4};
statetype s_keenthrow4	= {KEENTHROWL3SPR,KEENTHROWR3SPR,step,false,
	true,10, 0,0, NULL, KeenContact, KeenStandReact, &s_keenstand};

statetype s_keenthrowup1	= {KEENTHROWU1SPR,KEENTHROWU1SPR,step,false,
	true,8, 0,0, NULL, KeenContact, KeenStandReact, &s_keenthrowup2};
statetype s_keenthrowup2	= {KEENTHROWU2SPR,KEENTHROWU2SPR,step,true,
	true,1, 0,0, KeenThrow, KeenContact, KeenStandReact, &s_keenthrowup3};
statetype s_keenthrowup3	= {KEENTHROWU2SPR,KEENTHROWU2SPR,step,false,
	true,10, 0,0, NULL, KeenContact, KeenStandReact, &s_keenstand};

//-------------------

statetype s_keenjumpup1	= {KEENJUMPUL1SPR,KEENJUMPUR1SPR,think,false,
	false,0, 0,0, KeenAirThink, KeenContact, KeenAirReact, &s_keenjumpup2};
statetype s_keenjumpup2	= {KEENJUMPUL2SPR,KEENJUMPUR2SPR,think,false,
	false,0, 0,0, KeenAirThink, KeenContact, KeenAirReact, &s_keenjumpup3};
statetype s_keenjumpup3	= {KEENJUMPUL1SPR,KEENJUMPUR1SPR,think,false,
	false,0, 0,0, KeenAirThink, KeenContact, KeenAirReact, &s_keenstand};

statetype s_keenjump1	= {KEENJUMPL1SPR,KEENJUMPR1SPR,think,false,
	false,0, 0,0, KeenAirThink, KeenContact, KeenAirReact, &s_keenjump2};
statetype s_keenjump2	= {KEENJUMPL2SPR,KEENJUMPR2SPR,think,false,
	false,0, 0,0, KeenAirThink, KeenContact, KeenAirReact, &s_keenjump3};
statetype s_keenjump3	= {KEENJUMPL3SPR,KEENJUMPR3SPR,think,false,
	false,0, 0,0, KeenAirThink, KeenContact, KeenAirReact, &s_keenstand};

statetype s_keenairthrow1= {KEENJLTHROWL1SPR,KEENJRTHROWR1SPR,stepthink,false,
	false,8, 0,0, ProjectileThink, KeenContact, KeenAirReact, &s_keenairthrow2};
statetype s_keenairthrow2= {KEENJLTHROWL2SPR,KEENJRTHROWR2SPR,step,true,
	false,1, 0,0, KeenThrow, KeenContact, KeenAirReact, &s_keenairthrow3};
statetype s_keenairthrow3= {KEENJLTHROWL2SPR,KEENJRTHROWR2SPR,stepthink,false,
	false,10, 0,0, ProjectileThink, KeenContact, KeenAirReact, &s_keenjumpup3};

statetype s_keenairthrowup1= {KEENJLTHROWU1SPR,KEENJRTHROWU1SPR,stepthink,false,
	false,8, 0,0, ProjectileThink, KeenContact, KeenAirReact, &s_keenairthrowup2};
statetype s_keenairthrowup2= {KEENJLTHROWU2SPR,KEENJRTHROWU2SPR,step,true,
	false,1, 0,0, KeenThrow, KeenContact, KeenAirReact, &s_keenairthrowup3};
statetype s_keenairthrowup3= {KEENJLTHROWU2SPR,KEENJRTHROWU2SPR,stepthink,false,
	false,10, 0,0, ProjectileThink, KeenContact, KeenAirReact, &s_keenjumpup3};

statetype s_keenairthrowdown1= {KEENJLTHROWD1SPR,KEENJRTHROWD1SPR,stepthink,false,
	false,8, 0,0, ProjectileThink, KeenContact, KeenAirReact, &s_keenairthrowdown2};
statetype s_keenairthrowdown2= {KEENJLTHROWD2SPR,KEENJRTHROWD2SPR,step,true,
	false,1, 0,0, KeenThrow, KeenContact, KeenAirReact, &s_keenairthrowdown3};
statetype s_keenairthrowdown3= {KEENJLTHROWD2SPR,KEENJRTHROWD2SPR,stepthink,false,
	false,10, 0,0, ProjectileThink, KeenContact, KeenAirReact, &s_keenjumpup3};


//===========================================================================

#pragma warn +sus


/*
===============
=
= SpawnKeen
=
===============
*/

void SpawnKeen (int tilex, int tiley, int dir)
{
	player->obclass = keenobj;
	player->active = yes;
	player->needtoclip = true;
	player->x = tilex<<G_T_SHIFT;
	player->y = (tiley<<G_T_SHIFT) - BLOCKSIZE*2;

	player->xdir = dir;
	player->ydir = 1;
	NewState (player,&s_keenstand);
}

//==========================================================================

/*
======================
=
= CheckGrabPole
=
======================
*/

boolean	CheckGrabPole (objtype *ob)
{
	int	x;
	unsigned far *map;

//
// kludgy bit to not let you grab a pole the instant you jump off it
//
	if (TimeCount < leavepoletime)
		leavepoletime = 0;
	else if (TimeCount - leavepoletime < MINPOLEJUMPTICS)
		return false;

	if (c.yaxis == -1)
		map = (unsigned _seg *)mapsegs[1]+
			mapbwidthtable[(ob->top+6*PIXGLOBAL)/TILEGLOBAL]/2;
	else
		map = (unsigned _seg *)mapsegs[1]+
			mapbwidthtable[ob->tilebottom]/2;

	x = (ob->left + (ob->right - ob->left)/2) >>G_T_SHIFT;

	map += x;

	if ((tinf[INTILE+*map]&0x7f) == 1)
	{
		ob->xmove = ((x<<G_T_SHIFT)-8*PIXGLOBAL) - ob->x;
		ob->ymove = c.yaxis*32;
		ob->temp4 = x;				// for future reference
		ob->needtoclip = false;		// can climb through pole holes
		ob->state = &s_keenpole;
		return true;
	}

	return false;
}

//==========================================================================


/*
============================
=
= KeenStandThink
=
============================
*/

void KeenStandThink (objtype *ob)
{
	if (c.xaxis)
	{
	// started walking
		ob->xdir = c.xaxis;
		ob->state = &s_keenwalk1;
		ob->xmove = ob->xdir*s_keenwalk1.xmove*tics;
		KeenWalkThink(ob);
		return;
	}

	if (c.button0 && !button0held)
	{
	// jump straight up
		SD_PlaySound (JUMPSND);
		ob->xspeed = 0;
		ob->yspeed = -SPDJUMP;
		ob->xmove = 0;
		ob->ymove = 0;		// ob->yspeed*tics;			// DEBUG
		jumptime = JUMPTIME;	// -tics;
		ob->state = &s_keenjumpup1;
		button0held = true;
		return;
	}

	if (c.button1 && !button1held)
	{
	// throw current xdir
		if (c.yaxis == -1)
			ob->state = &s_keenthrowup1;
		else
			ob->state = &s_keenthrow1;
		button1held = true;
		return;
	}

	switch (c.yaxis)
	{
	case -1:
		if (!CheckGrabPole(ob))			// try to go up/down pole first
			ob->state = &s_keenlookup;
		return;
	case 0:
		ob->state = &s_keenstand;
		return;
	case 1:
		if (!CheckGrabPole(ob))			// try to go up/down pole first
			ob->state = &s_keenduck;
		return;
	}
}

//===========================================================================

/*
=======================
=
= KeenPauseThink
=
= Do special animations in time
=
=======================
*/

void KeenPauseThink (objtype *ob)
{
	if (c.dir != dir_None || c.button0 || c.button1)
	{
		ob->temp1 = ob->temp2 = 0;			// not paused any more
		ob->state = &s_keenstand;
		KeenStandThink(ob);
		return;
	}

	if ((ob->hitnorth & ~7) != 24)	// if it is 0 now, keen is standing on a sprite
		ob->temp1 += tics;

	switch (ob->temp2)
	{
	case 0:
		if (ob->temp1 > 200)
		{
			ob->temp2++;
			ob->state = &s_keenpauselook;
			ob->temp1 = 0;
		}
		break;
	case 1:
		if (ob->temp1 > 300)
		{
			ob->temp2++;
			ob->state = &s_keenwait1;
			ob->temp1 = 0;
		}
		break;
	case 2:
		if (ob->temp1 > 700)
		{
			ob->temp2++;
			ob->state = &s_keenyawn1;
			ob->temp1 = 0;
		}
		break;
	case 3:
		if (ob->temp1 > 400)
		{
			ob->temp2++;
			ob->state = &s_keenpauselook;
			ob->temp1 = 0;
		}
		break;
	case 4:
		if (ob->temp1 > 700)
		{
			ob->temp2++;
			ob->state = &s_keenyawn1;
			ob->temp1 = 0;
		}
		break;
	case 5:
		if (ob->temp1 > 400)
		{
			ob->temp2++;
			ob->state = &s_keengosleep1;
		}
		break;
	}
}


//===========================================================================

/*
=======================
=
= KeenGoSleepThink
=
=======================
*/

void KeenGoSleepThink (objtype *ob)
{
//
// spawn the zees above keens head
//
	GetNewObj (true);

	new->obclass = inertobj;
	new->x = player->x;
	new->y = player->y-4*PIXGLOBAL;
	new->xdir = 1;
	new->ydir = -1;
	NewState (new,&s_keenzee1);

	ob->temp1 = (int)new;				// so they can be removed later
}


//===========================================================================

/*
=======================
=
= KeenSleepThink
=
=======================
*/

void KeenSleepThink (objtype *ob)
{
	if (c.dir != dir_None || c.button0 || c.button1)
	{
		if (ob->temp1 != (unsigned)&dummyobj)
			RemoveObj ((objtype *)ob->temp1);	// remove the zees
		ob->temp1 = ob->temp2 = 0;			// not paused any more
		ob->state = &s_keengetup;
	}
}


//===========================================================================

/*
=======================
=
= KeenDieThink
=
=======================
*/

void KeenDieThink (objtype *ob)
{
	ob++;			// shut up compiler
	playstate = died;
}


//===========================================================================

/*
=======================
=
= KeenDuckThink
=
=======================
*/

void KeenDuckThink (objtype *ob)
{
	unsigned far *map, tile;
	int midtile,bottomtile,move;

	if (c.yaxis != 1)
	{
		ob->state = &s_keenstand;
		KeenStandThink(ob);
		return;
	}

	if (c.xaxis)
		ob->xdir = c.xaxis;

	if (c.button0 && !button0held)
	{
	//
	// drop down a level
	//
		button0held = true;

		midtile = (ob->tileleft + ob->tileright) >> 1;
		bottomtile = ob->tilebottom;
		map = (unsigned far *)mapsegs[1] + mapbwidthtable[bottomtile]/2
			+ midtile;
		tile = *map;
		if (tinf[WESTWALL+tile] || tinf[EASTWALL+tile]
			|| tinf[SOUTHWALL+tile])
			return;				// wall prevents drop down

		map += mapwidth;
		tile = *map;
		if (tinf[WESTWALL+tile] || tinf[EASTWALL+tile]
			|| tinf[SOUTHWALL+tile])
			return;				// wall prevents drop down

		move = PIXGLOBAL*(tics<4 ? 4: tics);
		ob->bottom += move;
		ob->y += move;
		ob->xmove = ob->ymove = 0;
		ob->state = &s_keenjumpup3;
		ob->temp2 = 1;		// allready in last stage
		ob->xspeed = ob->yspeed = 0;
		SD_PlaySound (PLUMMETSND);
	}
}


//===========================================================================

/*
=======================
=
= KeenDropDownThink
=
=======================
*/

void KeenDropDownThink (objtype *ob)
{
	ob->needtoclip = true;
}


//===========================================================================

/*
=======================
=
= KeenWalkThink
=
=======================
*/

unsigned slopespeed[8] = {0,0,4,4,8,-4,-4,-8};

void KeenWalkThink (objtype *ob)
{
	int move;

	if (!c.xaxis)
	{
	//
	// stopped running
	//
		KeenStandThink (ob);
		return;
	}

	ob->xdir = c.xaxis;

	if (c.yaxis && CheckGrabPole(ob))		// try to go up/down pole
		return;

	if (c.button0 && !button0held)
	{
	//
	// running jump
	//
		SD_PlaySound (JUMPSND);
		ob->xspeed = ob->xdir * SPDRUNJUMP;
		ob->yspeed = -SPDJUMP;
		ob->xmove = 0;
		ob->ymove = 0;					// ob->yspeed*tics;
		button0held = true;
		jumptime = JUMPTIME; //-tics;		// DEBUG
		ob->state = &s_keenjump1;
	}

	if (c.button1 && !button1held)
	{
	//
	// throw
	//
		ob->state = &s_keenthrow1;
		button1held = true;
		return;
	}

	//
	// give speed for slopes
	//
	move = tics*slopespeed[ob->hitnorth&7];

	ob->xmove += move;
}

//===========================================================================

/*
=======================
=
= KeenAirThink
=
=======================
*/

void	KeenAirThink		(objtype *ob)
{
	if (jumptime)
	{
		if (jumptime<tics)
		{
			ob->ymove = ob->yspeed*jumptime;
			jumptime = 0;
		}
		else
		{
			ob->ymove = ob->yspeed*tics;
			if (!jumpcheat)
				jumptime-=tics;
		}
		if (!c.button0)
			jumptime = 0;

		if (!jumptime)
		{
			ob->temp2 = 0;
			ob->state = ob->state->nextstate;	// switch to second jump stage
		}
	}
	else
	{
		DoGravity(ob);

		if (ob->yspeed>0 && !ob->temp2)
		{
			ob->state = ob->state->nextstate;	// switch to third jump stage
			ob->temp2 = 1;
		}
	}

//-------------

	if (c.xaxis)
	{
		ob->xdir = c.xaxis;
		AccelerateX(ob,c.xaxis*2,MAXXSPEED);
	}
	else
		FrictionX(ob);

	if (c.button1 && !button1held)
	{
		button1held = true;
	//
	// throw
	//
		switch (c.yaxis)
		{
		case -1:
			ob->state = &s_keenairthrowup1;
			return;
		case 0:
			ob->state = &s_keenairthrow1;
			return;
		case 1:
			ob->state = &s_keenairthrowdown1;
			return;
		}
	}

	if (ob->hitsouth==17)		// going through a pole hole
	{
		ob->xspeed = ob->xmove = 0;
	}


	if (c.yaxis == -1)
		CheckGrabPole (ob);

}

//===========================================================================

/*
=======================
=
= KeenPoleThink
=
=======================
*/

int	polexspeed[3] = {-SPDPOLESIDEJUMP,0,SPDPOLESIDEJUMP};

void	PoleActions (objtype *ob)
{
	if (c.xaxis)
		ob->xdir = c.xaxis;

	if (c.button0 && !button0held)		// jump off the pole
	{
		SD_PlaySound (JUMPSND);
		ob->xspeed = polexspeed[c.xaxis+1];
		ob->yspeed = -SPDPOLEJUMP;
		ob->needtoclip = true;
		jumptime = POLEJUMPTIME;
		ob->state = &s_keenjump1;
		ob->ydir = 1;
		button0held = true;
		leavepoletime = TimeCount;
		return;
	}

	if (c.button1 && !button1held)
	{
		button1held = true;

		switch (c.yaxis)
		{
		case -1:
			ob->state = &s_keenpolethrowup1;
			break;
		case 0:
			ob->state = &s_keenpolethrow1;
			break;
		case 1:
			ob->state = &s_keenpolethrowdown1;
			break;
		}
	}
}


void	KeenPoleThink		(objtype *ob)
{
	unsigned far *map, tile;

	switch (c.yaxis)
	{
	case -1:
		ob->state = &s_keenclimb1;
		ob->ydir = -1;
		return;

	case 1:
		ob->state = &s_keenslide1;
		ob->ydir = 1;
		KeenDropThink (ob);
		return;
	}

	if (c.xaxis)
	{
	//
	// walk off pole if right next to ground
	//
		map = mapsegs[1] + (mapbwidthtable[ob->tilebottom+1]/2 + ob->tilemidx);
		tile = *map;
		if (tinf[NORTHWALL+tile])
		{
			ob->xspeed = 0;
			ob->yspeed = 0;
			ob->needtoclip = true;
			jumptime = 0;
			ob->state = &s_keenjump3;
			ob->ydir = 1;
			SD_PlaySound (PLUMMETSND);
			return;
		}
	}

	PoleActions (ob);
}


/*
=======================
=
= KeenClimbThink
=
=======================
*/

void	KeenClimbThink		(objtype *ob)
{
	unsigned far *map;

	map = (unsigned _seg *)mapsegs[1]+mapbwidthtable[ob->tiletop]/2+ob->temp4;

	if ((tinf[INTILE+*map]&0x7f) != 1)
	{
		ob->ymove=0;
		ob->state = &s_keenpole;		// ran out of pole
		return;
	}

	switch (c.yaxis)
	{
	case 0:
		ob->state = &s_keenpole;
		ob->ydir = 0;
		break;

	case 1:
		ob->state = &s_keenslide1;
		ob->ydir = 1;
		KeenDropThink (ob);
		break;
	}

	PoleActions (ob);
}


/*
=======================
=
= KeenDropThink
=
=======================
*/

void	KeenDropThink		(objtype *ob)
{
	unsigned far *map;

	map = (unsigned _seg *)mapsegs[1]+mapbwidthtable[ob->tilebottom]/2+ob->temp4;

	if ((tinf[INTILE+*map]&0x7f) != 1)
	{
		SD_PlaySound (PLUMMETSND);
		ob->state = &s_keenjump3;		// ran out of pole
		jumptime = 0;
		ob->temp2 = 1;
		ob->xspeed = polexspeed[c.xaxis+1];
		ob->yspeed = 0;
		ob->needtoclip = true;
		ob->tilebottom--;
		return;
	}

	switch (c.yaxis)
	{
	case -1:
		ob->state = &s_keenclimb1;
		ob->ydir = -1;
		break;

	case 0:
		ob->state = &s_keenpole;
		ob->ydir = 0;
		break;
	}

	PoleActions (ob);
}

//===========================================================================

/*
=======================
=
= KeenThrow
=
=======================
*/

void	KeenThrow	(objtype *ob)
{
// can't use &<var> in a switch statement...

	if (ob->state == &s_keenthrow3)
	{
		if (ob->xdir > 0)
			ThrowPower (ob->midx-4*PIXGLOBAL,ob->y+8*PIXGLOBAL,dir_East);
		else
			ThrowPower (ob->midx-4*PIXGLOBAL,ob->y+8*PIXGLOBAL,dir_West);
		return;
	}

	if (ob->state == &s_keenpolethrow2)
	{
		if (ob->xdir > 0)
			ThrowPower (ob->x+24*PIXGLOBAL,ob->y,dir_East);
		else
			ThrowPower (ob->x-8*PIXGLOBAL,ob->y,dir_West);
		return;
	}

	if (ob->state == &s_keenthrowup2)
	{
		ThrowPower (ob->x+4*PIXGLOBAL,ob->y-8*PIXGLOBAL,dir_North);
		return;
	}

	if (ob->state == &s_keenpolethrowup2)
	{
		ThrowPower (ob->x+8*PIXGLOBAL,ob->y-8*PIXGLOBAL,dir_North);
		return;
	}

	if (ob->state == &s_keenpolethrowdown2)
	{
		ThrowPower (ob->x+8*PIXGLOBAL,ob->y+8*PIXGLOBAL,dir_South);
		return;
	}

	if (ob->state == &s_keenairthrow2)
	{
		if (ob->xdir > 0)
			ThrowPower (ob->midx-4*PIXGLOBAL,ob->y+8*PIXGLOBAL,dir_East);
		else
			ThrowPower (ob->midx-4*PIXGLOBAL,ob->y+8*PIXGLOBAL,dir_West);
#if 0
		if (ob->xdir > 0)
			ThrowPower (ob->x+32*PIXGLOBAL,ob->y+8*PIXGLOBAL,dir_East);
		else
			ThrowPower (ob->x-16*PIXGLOBAL,ob->y+8*PIXGLOBAL,dir_West);
#endif

		new->xspeed += ob->xspeed/2;
		new->yspeed += ob->yspeed/2;
		return;
	}

	if (ob->state == &s_keenairthrowup2)
	{
		if (ob->xdir > 0)
			ThrowPower (ob->x+16*PIXGLOBAL,ob->y,dir_North);
		else
			ThrowPower (ob->x+4*PIXGLOBAL,ob->y,dir_North);
		new->xspeed += ob->xspeed/2;
		new->yspeed += ob->yspeed/2;
		return;
	}

	if (ob->state == &s_keenairthrowdown2)
	{
		if (ob->xdir > 0)
			ThrowPower (ob->x+3*PIXGLOBAL,ob->y+16*PIXGLOBAL,dir_South);
		else
			ThrowPower (ob->x+12*PIXGLOBAL,ob->y+16*PIXGLOBAL,dir_South);
		new->xspeed += ob->xspeed/2;
		new->yspeed += ob->yspeed/2;
		return;
	}

	Quit ("KeenThrow: Bad state!");
}


/*
=============================================================================

						CONTACT ROUTINES

=============================================================================
*/

/*
============================
=
= KillKeen
=
============================
*/

void KillKeen (void)
{
	if (!godmode)
	{
		SD_PlaySound (WAKEUPSND);
		player->needtoclip = false;
		ChangeState (player,&s_keendie1);
	}
}



/*
============================
=
= KeenContact
=
============================
*/

unsigned bonuspoints[6] = {100,200,500,1000,2000,5000};

void	KeenContact (objtype *ob, objtype *hit)
{
	switch (hit->obclass)
	{
	case	bonusobj:
		hit->obclass = inertobj;
		switch (hit->temp1)
		{
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
			SD_PlaySound (GETPOINTSSND);
			hit->shapenum = BONUS100SPR+hit->temp1;
			GivePoints (bonuspoints[hit->temp1]);
			ChangeState (hit,&s_bonusrise);
			break;
		case 6:
			SD_PlaySound (EXTRAKEENSND);
			hit->shapenum = BONUS1UPSPR;
			gamestate.lives++;
			ChangeState (hit,&s_bonusrise);
			break;
		case 7:
			SD_PlaySound (EXTRAKEENSND);
			hit->shapenum = BONUSSUPERSPR;
			gamestate.lives+=3;
			gamestate.flowerpowers+=8;
			GivePoints (10000);
			ChangeState (hit,&s_bonusrise);
			break;
		case 8:
			SD_PlaySound (GETPOWERSND);
			hit->shapenum = BONUSFLOWERSPR;
			gamestate.flowerpowers++;
			ChangeState (hit,&s_bonusrise);
			break;
		case 9:
			SD_PlaySound (GETPOWERSND);
			hit->shapenum = BONUSFLOWERUPSPR;
			gamestate.flowerpowers+=5;
			ChangeState (hit,&s_bonusrise);
			break;
		case 10:
			SD_PlaySound (GETBOMBSND);
			hit->shapenum = BONUSBOMBSPR;
			gamestate.boobusbombs++;
			gamestate.bombsthislevel++;
			ChangeState (hit,&s_bonusrise);
			break;
		case 11:
			SD_PlaySound (GETKEYSND);
			hit->shapenum = BONUSKEYSPR;
			gamestate.keys++;
			ChangeState (hit,&s_bonusrise);
			break;
		}
		break;

	case	doorobj:
		if (gamestate.keys)
		{
			if (hit->state != &s_doorraise)
			{
				SD_PlaySound (OPENDOORSND);
				gamestate.keys--;
				ChangeState (hit,&s_doorraise);
			}
		}
		else
		{
			SD_PlaySound (NOWAYSND);
			ClipToSpriteSide (ob,hit);
		}
		break;
	case	taterobj:
		if (hit->state == &s_taterattack2)
			KillKeen ();
		break;
	case	carrotobj:
		ClipToSpriteSide (ob,hit);
		if (!ob->needtoclip)		// got pushed off a pole
		{
			SD_PlaySound (PLUMMETSND);
			ob->needtoclip = true;
			ob->xspeed = ob->yspeed = 0;
			ChangeState(ob,&s_keenjump3);
			ob->temp2 = 1;
			jumptime = 0;
		}
		break;
	case	cartobj:
		ClipToSprite (ob,hit,true);
		break;
	case	broccoobj:
		if (hit->state == &s_broccosmash3 || hit->state == &s_broccosmash4)
			KillKeen ();
		break;
	case	squashobj:
		if (hit->state == &s_squasherjump2)
			KillKeen ();
		else
		{
			ClipToSpriteSide (ob,hit);
			if (!ob->needtoclip)		// got pushed off a pole
			{
				SD_PlaySound (PLUMMETSND);
				ob->needtoclip = true;
				ob->xspeed = ob->yspeed = 0;
				ChangeState(ob,&s_keenjump3);
				ob->temp2 = 1;
				jumptime = 0;
			}
		}
		break;
	case	grapeobj:
		if (hit->state == &s_grapefall)
			KillKeen ();
		break;
	case	tomatobj:
	case	celeryobj:
	case	asparobj:
	case	turnipobj:
	case	cauliobj:
	case	brusselobj:
	case	mushroomobj:
	case	apelobj:
	case	peabrainobj:
	case	boobusobj:
	case	shotobj:
			KillKeen ();
		break;

	}
}


/*
=============================================================================

						 REACTION ROUTINES

=============================================================================
*/


/*
============================
=
= KeenSimpleReact
=
============================
*/

void	KeenSimpleReact (objtype *ob)
{
	PLACESPRITE;
}


/*
============================
=
= KeenStandReact
=
============================
*/

void	KeenStandReact (objtype *ob)
{
	if (!ob->hitnorth)
	{
	//
	// walked off an edge
	//
		SD_PlaySound (PLUMMETSND);
		ob->xspeed = ob->xdir*WALKAIRSPEED;
		ChangeState (ob,&s_keenjump3);
		ob->temp2 = 1;
		jumptime = 0;
	}
	else if ( (ob->hitnorth & ~7) == 8)	// deadly floor!
	{
		KillKeen ();
	}

	PLACESPRITE;
}

/*
============================
=
= KeenWalkReact
=
============================
*/

void	KeenWalkReact (objtype *ob)
{
	if (!ob->hitnorth)
	{
	//
	// walked off an edge
	//
		ob->xspeed = ob->xdir*WALKAIRSPEED;
		ob->yspeed = 0;
		ChangeState (ob,&s_keenjump3);
		ob->temp2 = 1;
		jumptime = 0;
	}
	else if ( (ob->hitnorth & ~7) == 8)	// deadly floor!
	{
		KillKeen ();
		goto placeit;
	}

	if (ob->hiteast == 2)			// doors
	{

	}
	else if (ob->hitwest == 2)		// doors
	{

	}
	else if (ob->hiteast || ob->hitwest)
	{
	//
	// ran into a wall
	//
		ob->ticcount = 0;
		ob->state = &s_keenstand;
		ob->shapenum = ob->xdir == 1 ? s_keenstand.rightshapenum :
			s_keenstand.leftshapenum;
	}
placeit:

	PLACESPRITE;
}


/*
============================
=
= KeenAirReact
=
============================
*/

void	KeenAirReact (objtype *ob)
{
	int x,y;
	unsigned far *map,mapextra;

	if (ob->hiteast || ob->hitwest)
		ob->xspeed = 0;

	map = mapsegs[1] + (mapbwidthtable[ob->tiletop]/2) + ob->tileleft;
	mapextra = mapwidth - (ob->tileright - ob->tileleft+1);
	for (y=ob->tiletop;y<=ob->tilebottom;y++,map+=mapextra)
		for (x=ob->tileleft;x<=ob->tileright;x++,map++)
			if (tinf[SOUTHWALL+*map] == 17)	// jumping up through a pole hole
			{
				ob->xspeed = 0;
				ob->x = ob->tilemidx*TILEGLOBAL - 2*PIXGLOBAL;
				goto checknorth;
			}

	if (ob->hitsouth)
	{
		if (ob->hitsouth == 17)	// jumping up through a pole hole
		{
			ob->y -= 32;
			ob->top -= 32;
			ob->xspeed = 0;
			ob->x = ob->tilemidx*TILEGLOBAL - 2*PIXGLOBAL;
		}
		else
		{
			SD_PlaySound (HITHEADSND);

			if (ob->hitsouth > 1)
			{
				ob->yspeed += 16;
				if (ob->yspeed<0)	// push away from slopes
					ob->yspeed = 0;
			}
			else
				ob->yspeed = 0;
			jumptime = 0;
		}
	}

checknorth:
	if (ob->hitnorth)
	{
		if (!(ob->hitnorth == 25 && jumptime))	// KLUDGE to allow jumping off
		{										// sprites
			ob->temp1 = ob->temp2 = 0;
			ChangeState (ob,&s_keenstand);
			SD_PlaySound (LANDSND);
		}
	}

	PLACESPRITE;
}

/*
============================
=
= KeenSlideReact
=
============================
*/

void	KeenSlideReact (objtype *ob)
{
	unsigned far *map;

	if (ob->hitnorth)			// friction slow down
	{
		map = mapsegs[2] + (mapbwidthtable[ob->tiletop]/2 + ob->tileleft);
		if (!tinf[SOUTHWALL+*map] && !tinf[SOUTHWALL+*(map+1)])
			FrictionX(ob);
	}


	if (ob->hitwest || ob->hiteast || !ob->xspeed)
		ChangeState (ob,&s_keengetup);

	PLACESPRITE;
}

