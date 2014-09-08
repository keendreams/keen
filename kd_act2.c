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

// KD_ACT1.C
#include "KD_DEF.H"
#pragma hdrstop


/*
=============================================================================

						 LOCAL CONSTANTS

=============================================================================
*/

#define PLACESPRITE RF_PlaceSprite (&ob->sprite,ob->x,ob->y,ob->shapenum, \
	spritedraw,0);


void	ProjectileReact (objtype *ob);

/*
=============================================================================

						   TATER TROOPER

temp1 = chasing player
temp2 = nothink time

=============================================================================
*/

void TaterThink (objtype *ob);
void BackupReact (objtype *ob);

extern	statetype s_taterwalk1;
extern	statetype s_taterwalk2;
extern	statetype s_taterwalk3;
extern	statetype s_taterwalk4;


extern	statetype s_taterattack1;
extern	statetype s_taterattack2;
extern	statetype s_taterattack3;

#pragma warn -sus

statetype s_taterwalk1	= {TATERTROOPWALKL1SPR,TATERTROOPWALKR1SPR,step,false,
	true,10, 128,0, TaterThink, NULL, WalkReact, &s_taterwalk2};
statetype s_taterwalk2	= {TATERTROOPWALKL2SPR,TATERTROOPWALKR2SPR,step,false,
	true,10, 128,0, TaterThink, NULL, WalkReact, &s_taterwalk3};
statetype s_taterwalk3	= {TATERTROOPWALKL3SPR,TATERTROOPWALKR3SPR,step,false,
	true,10, 128,0, TaterThink, NULL, WalkReact, &s_taterwalk4};
statetype s_taterwalk4	= {TATERTROOPWALKL4SPR,TATERTROOPWALKR4SPR,step,false,
	true,10, 128,0, TaterThink, NULL, WalkReact, &s_taterwalk1};

statetype s_taterattack1= {TATERTROOPLUNGEL1SPR,TATERTROOPLUNGER1SPR,step,false,
	false,12, 0,0, NULL, NULL, BackupReact, &s_taterattack2};
statetype s_taterattack2= {TATERTROOPLUNGEL2SPR,TATERTROOPLUNGER2SPR,step,false,
	false,20, 0,0, NULL, NULL, DrawReact, &s_taterattack3};
statetype s_taterattack3= {TATERTROOPLUNGEL1SPR,TATERTROOPLUNGER1SPR,step,false,
	false,8, 0,0, NULL, NULL, DrawReact, &s_taterwalk1};

#pragma warn +sus

/*
====================
=
= SpawnTater
=
====================
*/

void SpawnTater (int tilex, int tiley)
{
	GetNewObj (false);

	new->obclass = taterobj;
	new->x = tilex<<G_T_SHIFT;
	new->y = (tiley<<G_T_SHIFT)-2*BLOCKSIZE + 15;
	new->xdir = 1;
	new->ydir = 1;
	NewState (new,&s_taterwalk1);
	new->hitnorth = 1;
}

/*
====================
=
= TaterThink
=
====================
*/

void TaterThink (objtype *ob)
{
	int delta;

	if (ob->top > player->bottom || ob->bottom < player->top)
		return;

	if ( ob->xdir == -1 )
	{
		delta = ob->left - player->right;
		if (delta > TILEGLOBAL)
			return;
		if (delta < -8*PIXGLOBAL)
		{
			ob->xdir = 1;
			return;
		}
		SD_PlaySound (TATERSWINGSND);
		ob->state = &s_taterattack1;
		return;
	}
	else
	{
		delta = player->left - ob->right;
		if (delta > TILEGLOBAL)
			return;
		if (delta < -8*PIXGLOBAL)
		{
			ob->xdir = -1;
			return;
		}
		SD_PlaySound (TATERSWINGSND);
		ob->state = &s_taterattack1;
		return;
	}
}


/*
====================
=
= BackupReact
=
====================
*/

void BackupReact (objtype *ob)
{
	if (!ob->hitnorth)
	{
		ob->x-=ob->xmove;
		ob->y-=ob->ymove;
	}

	PLACESPRITE;
}

/*
=============================================================================

						   CANTELOUPE CART

=============================================================================
*/

extern	statetype s_cartroll1;
extern	statetype s_cartroll2;

void CartReact (objtype *ob);

#pragma warn -sus

statetype s_cartroll1	= {CANTCARTL1SPR,CANTCARTL1SPR,slide,true,
	false,5, 32,0, NULL, NULL, CartReact, &s_cartroll2};
statetype s_cartroll2	= {CANTCARTL2SPR,CANTCARTL2SPR,slide,true,
	false,5, 32,0, NULL, NULL, CartReact, &s_cartroll1};

#pragma warn +sus

/*
====================
=
= SpawnCart
=
====================
*/

void SpawnCart (int tilex, int tiley)
{
	GetNewObj (false);

	new->obclass = cartobj;
	new->x = tilex<<G_T_SHIFT;
	new->y = (tiley<<G_T_SHIFT)-3*PIXGLOBAL;
	new->xdir = 1;
	new->ydir = 1;
	new->active = allways;
	NewState (new,&s_cartroll1);
}



/*
====================
=
= CartReact
=
====================
*/

void CartReact (objtype *ob)
{
	unsigned far *map;

	if (ob->xdir == 1 && ob->hitwest)
	{
		ob->xdir = -1;
	}
	else if (ob->xdir == -1 && ob->hiteast)
	{
		ob->xdir = 1;
	}

	map = mapsegs[1] + mapbwidthtable[ob->tilebottom+1]/2;
	if (ob->xdir == 1)
		map += ob->tileright;
	else
		map += ob->tileleft;

	if ( !tinf[NORTHWALL + *map] )
		ob->xdir = -ob->xdir;

	PLACESPRITE;
}


/*
=============================================================================

							FRENCHY

=============================================================================
*/

#define FRYXSPEED	40
#define FRYYSPEED	-20


void FrenchyThink (objtype *ob);
void FrenchyRunThink (objtype *ob);
void FrenchyThrow (objtype *ob);

extern	statetype s_frenchywalk1;
extern	statetype s_frenchywalk2;
extern	statetype s_frenchywalk3;
extern	statetype s_frenchywalk4;

extern	statetype s_frenchyrun1;
extern	statetype s_frenchyrun2;
extern	statetype s_frenchyrun3;
extern	statetype s_frenchyrun4;

extern	statetype s_frenchythrow1;
extern	statetype s_frenchythrow2;
extern	statetype s_frenchythrow3;

extern	statetype s_fry1;
extern	statetype s_fry2;

#pragma warn -sus

statetype s_frenchywalk1	= {FRENCHYRUNL1SPR,FRENCHYRUNR1SPR,step,false,
	true,10, 128,0, FrenchyThink, NULL, WalkReact, &s_frenchywalk2};
statetype s_frenchywalk2	= {FRENCHYRUNL2SPR,FRENCHYRUNR2SPR,step,false,
	true,10, 128,0, FrenchyThink, NULL, WalkReact, &s_frenchywalk3};
statetype s_frenchywalk3	= {FRENCHYRUNL3SPR,FRENCHYRUNR3SPR,step,false,
	true,10, 128,0, FrenchyThink, NULL, WalkReact, &s_frenchywalk4};
statetype s_frenchywalk4	= {FRENCHYRUNL4SPR,FRENCHYRUNR4SPR,step,false,
	true,10, 128,0, FrenchyThink, NULL, WalkReact, &s_frenchywalk1};

statetype s_frenchyrun1	= {FRENCHYRUNL1SPR,FRENCHYRUNR1SPR,step,true,
	true,5, 128,0, FrenchyRunThink, NULL, WalkReact, &s_frenchyrun2};
statetype s_frenchyrun2	= {FRENCHYRUNL2SPR,FRENCHYRUNR2SPR,step,true,
	true,5, 128,0, FrenchyRunThink, NULL, WalkReact, &s_frenchyrun3};
statetype s_frenchyrun3	= {FRENCHYRUNL3SPR,FRENCHYRUNR3SPR,step,true,
	true,5, 128,0, FrenchyRunThink, NULL, WalkReact, &s_frenchyrun4};
statetype s_frenchyrun4	= {FRENCHYRUNL4SPR,FRENCHYRUNR4SPR,step,true,
	true,5, 128,0, FrenchyRunThink, NULL, WalkReact, &s_frenchyrun1};

statetype s_frenchythrow1	= {FRENCHYTHROWL1SPR,FRENCHYTHROWR1SPR,step,false,
	false,10, 0,0, NULL, NULL, DrawReact, &s_frenchythrow2};
statetype s_frenchythrow2	= {FRENCHYTHROWL2SPR,FRENCHYTHROWR2SPR,step,false,
	false,1, 0,0, FrenchyThrow, NULL, DrawReact, &s_frenchythrow3};
statetype s_frenchythrow3	= {FRENCHYTHROWL2SPR,FRENCHYTHROWR2SPR,step,false,
	false,10, -128,0, NULL, NULL, DrawReact, &s_frenchywalk1};

statetype s_fry1		= {FRENCHFRY1SPR,FRENCHFRY1SPR,stepthink,false,
	false,4, 0,0, ProjectileThink, NULL, ProjectileReact, &s_fry2};
statetype s_fry2		= {FRENCHFRY2SPR,FRENCHFRY2SPR,stepthink,false,
	false,4, 0,0, ProjectileThink, NULL, ProjectileReact, &s_fry1};


#pragma warn +sus


/*
====================
=
= SpawnFrenchy
=
====================
*/

void SpawnFrenchy (int tilex, int tiley)
{
	GetNewObj (false);

	new->obclass = frenchyobj;
	new->x = tilex<<G_T_SHIFT;
	new->y = (tiley<<G_T_SHIFT)-2*BLOCKSIZE;
	new->xdir = 1;
	new->ydir = 1;
	NewState (new,&s_frenchywalk1);
}


/*
====================
=
= FrenchyRunThink
=
====================
*/

void FrenchyRunThink (objtype *ob)
{
	ob->state = &s_frenchywalk1;
}


/*
====================
=
= FrenchyThrow
=
====================
*/

void FrenchyThrow (objtype *ob)
{
	GetNewObj (true);
	new->obclass = shotobj;
	if (ob->xdir == 1)
	{
		new->x = ob->x+24*16;
		new->y = ob->y+8*16;
	}
	else
	{
		new->x = ob->x;
		new->y = ob->y+8*16;
	}
	new->xdir = ob->xdir;
	new->ydir = 1;
	new->xspeed = ob->xdir * FRYXSPEED-(US_RndT()>>4);
	new->yspeed = FRYYSPEED;
	new->active = removable;
	NewState (new,&s_fry1);

	ob->nothink = 2;
}


/*
====================
=
= FrenchyThink
=
====================
*/

void FrenchyThink (objtype *ob)
{
	int delta;

	if ( abs(ob->y - player->y) > 3*TILEGLOBAL )
	{
		if (US_RndT()<8)
			ob->xdir = -ob->xdir;		// turn randomly
		return;
	}

	delta = player->x - ob->x;

	if (delta < -8*TILEGLOBAL)
	{
	// walk closer
		ob->xdir = -1;
	}
	if (delta < -4*TILEGLOBAL)
	{
	// throw
		ob->xdir = -1;
		ob->state = &s_frenchythrow1;
	}
	else if (delta < 0)
	{
	// run away
		ob->xdir = 1;
		ob->state = &s_frenchyrun1;
		ob->nothink = 8;
	}
	else if (delta < 4*TILEGLOBAL)
	{
	// run away
		ob->xdir = -1;
		ob->state = &s_frenchyrun1;
		ob->nothink = 8;
	}
	else if (delta < 8*TILEGLOBAL)
	{
	// throw and walk closer
		ob->xdir = 1;
		ob->state = &s_frenchythrow1;
	}
	else
	{
	// walk closer
		ob->xdir = 1;
	}
}


/*
=============================================================================

						  MELON LIPS

ob->temp1 = direction : 0 = left, 1 = right, 2 = down

=============================================================================
*/

#define SPITXSPEED	48
#define SPITYSPEED	-20

void MelonSpitThink (objtype *ob);
void	ProjectileReact (objtype *ob);

extern	statetype s_melonside;
extern	statetype s_melonsidespit;
extern	statetype s_melonsidespit2;

extern	statetype s_melondown;
extern	statetype s_melondownspit;
extern	statetype s_melondownspit2;

extern	statetype s_melonseed1;
extern	statetype s_melonseed2;

extern	statetype s_melonseedd1;
extern	statetype s_melonseedd2;

#pragma warn -sus

statetype s_melonside	= {MELONLIPSL1SPR,MELONLIPSR1SPR,step,false,
	false,200, 0,0, NULL, NULL, DrawReact, &s_melonsidespit};
statetype s_melonsidespit= {MELONLIPSL2SPR,MELONLIPSR2SPR,step,false,
	false,6, 0,0, MelonSpitThink, NULL, DrawReact, &s_melonsidespit2};
statetype s_melonsidespit2= {MELONLIPSL2SPR,MELONLIPSR2SPR,step,false,
	false,6, 0,0, NULL, NULL, DrawReact, &s_melonside};

statetype s_melondown	= {MELONLIPSD1SPR,MELONLIPSD1SPR,step,false,
	false,200, 0,0, NULL, NULL, DrawReact, &s_melondownspit};
statetype s_melondownspit	= {MELONLIPSD2SPR,MELONLIPSD2SPR,step,false,
	false,6, 0,0, MelonSpitThink, NULL, DrawReact, &s_melondownspit2};
statetype s_melondownspit2	= {MELONLIPSD2SPR,MELONLIPSD2SPR,step,false,
	false,6, 0,0, NULL, NULL, DrawReact, &s_melondown};

statetype s_melonseed1	= {MELONSEEDL1SPR,MELONSEEDR1SPR,think,false,
	false,4, 0,0, ProjectileThink, NULL, ProjectileReact, &s_melonseed2};
statetype s_melonseed2	= {MELONSEEDL2SPR,MELONSEEDR2SPR,think,false,
	false,4, 0,0, ProjectileThink, NULL, ProjectileReact, &s_melonseed1};

statetype s_melonseedd1	= {MELONSEEDD1SPR,MELONSEEDD1SPR,stepthink,false,
	false,4, 0,0, ProjectileThink, NULL, ProjectileReact, &s_melonseedd2};
statetype s_melonseedd2	= {MELONSEEDD2SPR,MELONSEEDD2SPR,stepthink,false,
	false,4, 0,0, ProjectileThink, NULL, ProjectileReact, &s_melonseedd1};

#pragma warn +sus

/*
====================
=
= SpawnMelon
=
====================
*/

void SpawnMelon (int tilex, int tiley,int dir)
{
	GetNewObj (false);

	new->obclass = melonobj;
	new->x = tilex<<G_T_SHIFT;
	new->y = tiley<<G_T_SHIFT;
	if (dir)
		new->xdir = 1;
	else
		new->xdir = -1;
	if (dir <2)
		NewState (new,&s_melonside);
	else
		NewState (new,&s_melondown);

	new->ticcount = US_RndT()>>1;
	new->temp1 = dir;
}


/*
====================
=
= MelonSpitThink
=
====================
*/

void MelonSpitThink (objtype *ob)
{
	GetNewObj (false);
	new->obclass = shotobj;
	switch (ob->temp1)
	{
	case 0:
		new->x = ob->x+24*16;
		new->y = ob->y+8*16;
		new->xdir = ob->xdir;
		new->ydir = 1;
		new->xspeed = -SPITXSPEED-(US_RndT()>>4);
		new->yspeed = SPITYSPEED;
		NewState (new,&s_melonseed1);
		break;
	case 1:
		new->x = ob->x;
		new->y = ob->y+8*16;
		new->xdir = ob->xdir;
		new->ydir = 1;
		new->xspeed = SPITXSPEED+(US_RndT()>>4);
		new->yspeed = SPITYSPEED;
		NewState (new,&s_melonseed1);
		break;
	case 2:
		new->x = ob->x+8*16;
		new->y = ob->y+24*16;
		new->ydir = 1;
		new->yspeed = -SPITYSPEED;
		NewState (new,&s_melonseedd1);
		break;
	}

	new->active = removable;
}

/*
============================
=
= ProjectileReact
=
============================
*/

void	ProjectileReact (objtype *ob)
{
	unsigned wall,absx,absy,angle,newangle;
	unsigned long speed;

	PLACESPRITE;
	if (ob->hiteast || ob->hitwest)
		ob->xspeed= -ob->xspeed/2;

	if (ob->hitsouth)
		ob->yspeed= -ob->yspeed/2;

	wall = ob->hitnorth;
	if (wall)
	{
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
			RemoveObj (ob);
	}
}

/*
=============================================================================

							SQUASHER

=============================================================================
*/

#define SPDSQUASHLEAPY  50


void SquasherThink (objtype *ob);
void SquasherJumpReact (objtype *ob);

extern	statetype s_squasherwalk1;
extern	statetype s_squasherwalk2;

extern	statetype s_squasherjump1;
extern	statetype s_squasherjump2;

extern	statetype s_squasherwait;

#pragma warn -sus

statetype s_squasherwalk1	= {SQUASHERWALKL1SPR,SQUASHERWALKR1SPR,step,false,
	true,10, 128,0, SquasherThink, NULL, WalkReact, &s_squasherwalk2};
statetype s_squasherwalk2	= {SQUASHERWALKL2SPR,SQUASHERWALKR2SPR,step,false,
	true,10, 128,0, SquasherThink, NULL, WalkReact, &s_squasherwalk1};

statetype s_squasherjump1	= {SQUASHERJUMPL1SPR,SQUASHERJUMPR1SPR,stepthink,false,
	false,20, 0,0, ProjectileThink, NULL, SquasherJumpReact, &s_squasherjump2};
statetype s_squasherjump2	= {SQUASHERJUMPL2SPR,SQUASHERJUMPR2SPR,think,false,
	false,0, 0,0, ProjectileThink, NULL, SquasherJumpReact, NULL};

statetype s_squasherwait	= {SQUASHERJUMPL2SPR,SQUASHERJUMPR2SPR,step,false,
	false,10, 0,0, ProjectileThink, NULL, DrawReact, &s_squasherwalk1};

#pragma warn +sus

/*
====================
=
= SpawnSquasher
=
====================
*/

void SpawnSquasher (int tilex, int tiley)
{
	GetNewObj (false);

	new->obclass = squashobj;
	new->x = tilex<<G_T_SHIFT;
	new->y = (tiley<<G_T_SHIFT)-2*BLOCKSIZE;
	new->xdir = 1;
	new->ydir = 1;
	NewState (new,&s_squasherwalk1);
}

/*
====================
=
= SquasherThink
=
====================
*/

void SquasherThink (objtype *ob)
{
	int delta;

	if ( abs(ob->y - player->y) > 3*TILEGLOBAL )
	{
		if (US_RndT()<8)
			ob->xdir = -ob->xdir;		// turn randomly
		return;
	}


	delta = player->x - ob->x;

	if ( ob->xdir == -1 )
	{
		if (delta < -6*TILEGLOBAL)
			return;
		if (delta > 8*PIXGLOBAL)
		{
			ob->xdir = 1;
			return;
		}
	}
	else
	{
		if (delta > 6*TILEGLOBAL)
			return;
		if (delta < -8*PIXGLOBAL)
		{
			ob->xdir = 1;
			return;
		}
	}

	ob->yspeed = -SPDSQUASHLEAPY;
	ob->xspeed = delta/60;
	ob->state = &s_squasherjump1;
}

/*
====================
=
= SquasherJumpReact
=
====================
*/

void SquasherJumpReact (objtype *ob)
{
	if (ob->hitsouth)
		ob->yspeed = 0;

	if (ob->hitnorth)
		ChangeState (ob,&s_squasherwait);

	PLACESPRITE;
}


/*
=============================================================================

								APEL

temp4 = pole x coordinate

=============================================================================
*/

void ApelThink (objtype *ob);
void ApelClimbThink (objtype *ob);
void ApelSlideThink (objtype *ob);
void ApelFallThink (objtype *ob);

void ApelClimbReact (objtype *ob);
void ApelSlideReact (objtype *ob);
void ApelFallReact (objtype *ob);

extern	statetype s_apelwalk1;
extern	statetype s_apelwalk2;
extern	statetype s_apelwalk3;

extern	statetype s_apelclimb1;
extern	statetype s_apelclimb2;

extern	statetype s_apelslide1;
extern	statetype s_apelslide2;
extern	statetype s_apelslide3;
extern	statetype s_apelslide4;

extern	statetype s_apelfall;

#pragma warn -sus

statetype s_apelwalk1	= {APELWALKL1SPR,APELWALKR1SPR,step,false,
	true,10, 128,0, ApelThink, NULL, WalkReact, &s_apelwalk2};
statetype s_apelwalk2	= {APELWALKL2SPR,APELWALKR2SPR,step,false,
	true,10, 128,0, ApelThink, NULL, WalkReact, &s_apelwalk3};
statetype s_apelwalk3	= {APELWALKL3SPR,APELWALKR3SPR,step,false,
	true,10, 128,0, ApelThink, NULL, WalkReact, &s_apelwalk1};

statetype s_apelclimb1	= {APELSHINNY1SPR,APELSHINNY1SPR,slide,false,
	false,6, 0,-16, ApelClimbThink, NULL, ApelClimbReact, &s_apelclimb2};
statetype s_apelclimb2	= {APELSHINNY2SPR,APELSHINNY2SPR,slide,false,
	false,6, 0,-16, ApelClimbThink, NULL, ApelClimbReact, &s_apelclimb1};

statetype s_apelslide1	= {APELSLIDE1SPR,APELSLIDE1SPR,slide,false,
	false,6, 0,16, ApelSlideThink, NULL, ApelFallReact, &s_apelslide2};
statetype s_apelslide2	= {APELSLIDE2SPR,APELSLIDE2SPR,slide,false,
	false,6, 0,16, ApelSlideThink, NULL, ApelFallReact, &s_apelslide3};
statetype s_apelslide3	= {APELSLIDE3SPR,APELSLIDE3SPR,slide,false,
	false,6, 0,16, ApelSlideThink, NULL, ApelFallReact, &s_apelslide4};
statetype s_apelslide4	= {APELSLIDE4SPR,APELSLIDE4SPR,slide,false,
	false,6, 0,16, ApelSlideThink, NULL, ApelFallReact, &s_apelslide1};

statetype s_apelfall = {APELWALKL1SPR,APELWALKR1SPR,think,false,
	false,0, 0,0, ProjectileThink, NULL, ApelFallReact, NULL};

#pragma warn +sus


/*
====================
=
= SpawnApel
=
====================
*/

void SpawnApel (int tilex, int tiley)
{
	GetNewObj (false);

	new->obclass = apelobj;
	new->x = tilex<<G_T_SHIFT;
	new->y = (tiley<<G_T_SHIFT)-2*BLOCKSIZE;
	new->xdir = 1;
	new->ydir = 1;
	NewState (new,&s_apelwalk1);
}


/*
====================
=
= ApelThink
=
====================
*/

void ApelThink (objtype *ob)
{
	int	x,y;
	unsigned far *map;

	if (ob->top > player->bottom || ob->bottom < player->top)
	{
	//
	// try to climb a pole to reach player
	//
		if (ob->y < player->y)
			y = ob->tilebottom;
		else
			y = ob->tiletop;

		map = (unsigned _seg *)mapsegs[1]+
			mapbwidthtable[y]/2 + ob->tilemidx;

		if ((tinf[INTILE+*map]&0x7f) == 1)
		{
			ob->xmove = (ob->tilemidx<<G_T_SHIFT) - ob->x;
			ob->ymove = 0;
			ob->temp4 = ob->tilemidx;	// for future reference
			ob->needtoclip = false;		// can climb through pole holes
			if (ob->y < player->y)
				ob->state = &s_apelslide1;
			else
				ob->state = &s_apelclimb1;
			return;
		}
	}

	if (US_RndT()>32)		// don't turn around all the time
		return;

	if (ob->x < player->x)
		ob->xdir = 1;
	else
		ob->xdir = -1;

}


/*
====================
=
= ApelClimbThink
=
====================
*/

void ApelClimbThink (objtype *ob)
{
	unsigned far *map;

	map = (unsigned _seg *)mapsegs[1]+
		mapbwidthtable[ob->tiletop]/2 + ob->temp4;

	if ((tinf[INTILE+*map]&0x7f) != 1)
	{
		ob->needtoclip = true;
		ob->state = &s_apelfall;
	}
}


/*
====================
=
= ApelSlideThink
=
====================
*/

void ApelSlideThink (objtype *ob)
{
	unsigned far *map;

	map = (unsigned _seg *)mapsegs[1]+
		mapbwidthtable[ob->tilebottom]/2 + ob->temp4;

	if ((tinf[INTILE+*map]&0x7f) != 1)
	{
		ob->needtoclip = true;
		ob->state = &s_apelfall;
	}
}


/*
====================
=
= ApelClimbReact
=
====================
*/

void ApelClimbReact (objtype *ob)
{
	if (ob->hitsouth)
		ChangeState (ob,&s_apelfall);
	PLACESPRITE;
}


/*
====================
=
= ApelFallReact
=
====================
*/

void ApelFallReact (objtype *ob)
{
	if (ob->hitnorth)
		ChangeState (ob,&s_apelwalk1);
	PLACESPRITE;
}


/*
=============================================================================

								PEA BRAIN

=============================================================================
*/

void PeaBrainThink (objtype *ob);
void PeaFlyReact (objtype *ob);

extern statetype s_peabrainfly;

extern	statetype s_peabrainwalk1;
extern	statetype s_peabrainwalk2;
extern	statetype s_peabrainwalk3;
extern	statetype s_peabrainwalk4;

#pragma warn -sus

statetype s_peabrainfly	= {PEABRAINWALKL1SPR,PEABRAINWALKR1SPR,think,false,
	false,0, 0,0, ProjectileThink, NULL, PeaFlyReact, NULL};

statetype s_peabrainwalk1	= {PEABRAINWALKL1SPR,PEABRAINWALKR1SPR,step,false,
	true,10, 128,0, PeaBrainThink, NULL, WalkReact, &s_peabrainwalk2};
statetype s_peabrainwalk2	= {PEABRAINWALKL2SPR,PEABRAINWALKR2SPR,step,false,
	true,10, 128,0, PeaBrainThink, NULL, WalkReact, &s_peabrainwalk3};
statetype s_peabrainwalk3	= {PEABRAINWALKL3SPR,PEABRAINWALKR3SPR,step,false,
	true,10, 128,0, PeaBrainThink, NULL, WalkReact, &s_peabrainwalk4};
statetype s_peabrainwalk4	= {PEABRAINWALKL4SPR,PEABRAINWALKR4SPR,step,false,
	true,10, 128,0, PeaBrainThink, NULL, WalkReact, &s_peabrainwalk1};

#pragma warn +sus

/*
====================
=
= SpawnPeaBrain
=
====================
*/

void SpawnPeaBrain (int tilex, int tiley)
{
	GetNewObj (false);

	new->obclass = peabrainobj;
	new->x = tilex<<G_T_SHIFT;
	new->y = tiley<<G_T_SHIFT;
	NewState (new,&s_peabrainwalk1);
}

/*
====================
=
= PeaBrainThink
=
====================
*/

void PeaBrainThink (objtype *ob)
{
	ob++;
}


/*
====================
=
= PeaFlyReact
=
====================
*/

void PeaFlyReact (objtype *ob)
{
	if (ob->hitnorth)
		ChangeState (ob,&s_peabrainwalk1);

	PLACESPRITE;
}


/*
=============================================================================

								PEA POD

temp1 = number of peas spit

=============================================================================
*/

#define MAXPEASPIT	4

#define PEAXSPEED	48
#define PEAYSPEED	-20


void PeaPodThink (objtype *ob);
void SpitPeaBrain (objtype *ob);

extern	statetype s_peapodwalk1;
extern	statetype s_peapodwalk2;
extern	statetype s_peapodwalk3;
extern	statetype s_peapodwalk4;

extern	statetype s_peapodspit1;
extern	statetype s_peapodspit2;

#pragma warn -sus

statetype s_peapodwalk1	= {PEAPODRUNL1SPR,PEAPODRUNR1SPR,step,false,
	true,10, 128,0, PeaPodThink, NULL, WalkReact, &s_peapodwalk2};
statetype s_peapodwalk2	= {PEAPODRUNL2SPR,PEAPODRUNR2SPR,step,false,
	true,10, 128,0, PeaPodThink, NULL, WalkReact, &s_peapodwalk3};
statetype s_peapodwalk3	= {PEAPODRUNL3SPR,PEAPODRUNR3SPR,step,false,
	true,10, 128,0, PeaPodThink, NULL, WalkReact, &s_peapodwalk4};
statetype s_peapodwalk4	= {PEAPODRUNL4SPR,PEAPODRUNR4SPR,step,false,
	true,10, 128,0, PeaPodThink, NULL, WalkReact, &s_peapodwalk1};

statetype s_peapodspit1	= {PEAPODSPITLSPR,PEAPODSPITRSPR,step,false,
	true,30, 0,0, SpitPeaBrain, NULL, DrawReact, &s_peapodspit2};
statetype s_peapodspit2	= {PEAPODSPITLSPR,PEAPODSPITRSPR,step,false,
	true,30, 0,0, NULL, NULL, DrawReact, &s_peapodwalk1};

#pragma warn +sus

/*
====================
=
= SpawnPeaPod
=
====================
*/

void SpawnPeaPod (int tilex, int tiley)
{
	GetNewObj (false);

	new->obclass = peapodobj;
	new->x = tilex<<G_T_SHIFT;
	new->y = (tiley<<G_T_SHIFT)-2*BLOCKSIZE;
	new->xdir = 1;
	new->ydir = 1;
	NewState (new,&s_peapodwalk1);
}

/*
====================
=
= SpitPeaBrain
=
====================
*/

void SpitPeaBrain (objtype *ob)
{
	GetNewObj (true);
	new->obclass = peabrainobj;
	if (ob->xdir == 1)
	{
		new->x = ob->x+8*16;
		new->y = ob->y+8*16;
	}
	else
	{
		new->x = ob->x;
		new->y = ob->y+8*16;
	}
	new->xdir = ob->xdir;
	new->ydir = 1;
	new->xspeed = ob->xdir * PEAXSPEED-(US_RndT()>>4);
	new->yspeed = PEAYSPEED;
	NewState (new,&s_peabrainfly);
}



/*
====================
=
= PeaPodThink
=
====================
*/

void PeaPodThink (objtype *ob)
{
	int delta;

	if ( abs(ob->y - player->y) > 3*TILEGLOBAL )
		return;

	if (player->x < ob->x && ob->xdir == 1)
		return;

	if (player->x > ob->x && ob->xdir == -1)
		return;

	if (US_RndT()<8 && ob->temp1 < MAXPEASPIT)
	{
		ob->temp1 ++;
		ob->state = &s_peapodspit1;
		ob->xmove = 0;
	}
}


/*
=============================================================================

							BOOBUS TUBOR

temp4 = hit points

=============================================================================
*/

#define PREFRAGTHINK	60
#define POSTFRAGTHINK	60

#define SPDBOOBUSJUMP	60
#define SPDBOOBUSRUNJUMP 24

void BoobusThink (objtype *ob);
void FinishThink (objtype *ob);
void FragThink (objtype *ob);
void BoobusGroundReact (objtype *ob);
void BoobusAirReact (objtype *ob);

extern	statetype s_boobuswalk1;
extern	statetype s_boobuswalk2;
extern	statetype s_boobuswalk3;
extern	statetype s_boobuswalk4;

extern	statetype s_boobusjump;

extern	statetype s_deathwait1;
extern	statetype s_deathwait2;
extern	statetype s_deathwait3;
extern	statetype s_deathboom1;
extern	statetype s_deathboom2;
extern	statetype s_deathboom3;
extern	statetype s_deathboom4;
extern	statetype s_deathboom5;
extern	statetype s_deathboom6;

#pragma warn -sus

statetype s_boobuswalk1	= {BOOBUSWALKL1SPR,BOOBUSWALKR1SPR,step,false,
	true,10, 128,0, BoobusThink, NULL, BoobusGroundReact, &s_boobuswalk2};
statetype s_boobuswalk2	= {BOOBUSWALKL2SPR,BOOBUSWALKR2SPR,step,false,
	true,10, 128,0, BoobusThink, NULL, BoobusGroundReact, &s_boobuswalk3};
statetype s_boobuswalk3	= {BOOBUSWALKL3SPR,BOOBUSWALKR3SPR,step,false,
	true,10, 128,0, BoobusThink, NULL, BoobusGroundReact, &s_boobuswalk4};
statetype s_boobuswalk4	= {BOOBUSWALKL4SPR,BOOBUSWALKR4SPR,step,false,
	true,10, 128,0, BoobusThink, NULL, BoobusGroundReact, &s_boobuswalk1};

statetype s_boobusjump	= {BOOBUSJUMPSPR,BOOBUSJUMPSPR,think,false,
	false,0, 0,0, ProjectileThink, NULL, BoobusAirReact, NULL};

statetype s_boobusdie	= {BOOBUSJUMPSPR,BOOBUSJUMPSPR,step,false,
	false,4, 0,0, FragThink, NULL, DrawReact, &s_boobusdie};
statetype s_boobusdie2	= {NULL,NULL,step,false,
	false,4, 0,0, FragThink, NULL, NULL, &s_boobusdie2};
statetype s_boobusdie3	= {NULL,NULL,step,false,
	false,250, 0,0, FinishThink, NULL, NULL, NULL};

statetype s_deathboom1	= {BOOBUSBOOM1SPR,BOOBUSBOOM1SPR,step,false,
	false,20, 0,0, ProjectileThink, NULL, DrawReact3, &s_deathboom2};
statetype s_deathboom2	= {BOOBUSBOOM2SPR,BOOBUSBOOM2SPR,step,false,
	false,20, 0,0, ProjectileThink, NULL, DrawReact3, &s_deathboom3};
statetype s_deathboom3	= {POOF1SPR,POOF1SPR,step,false,
	false,40, 0,0, ProjectileThink, NULL, DrawReact3, &s_deathboom4};
statetype s_deathboom4	= {POOF2SPR,POOF2SPR,step,false,
	false,30, 0,0, ProjectileThink, NULL, DrawReact3, &s_deathboom5};
statetype s_deathboom5	= {POOF3SPR,POOF3SPR,step,false,
	false,30, 0,0, ProjectileThink, NULL, DrawReact3, &s_deathboom6};
statetype s_deathboom6	= {POOF4SPR,POOF4SPR,step,false,
	false,30, 0,0, ProjectileThink, NULL, DrawReact3, NULL};


#pragma warn +sus

/*
====================
=
= SpawnBoobus
=
====================
*/

void SpawnBoobus (int tilex, int tiley)
{
	GetNewObj (false);

	new->obclass = boobusobj;
	new->x = tilex<<G_T_SHIFT;
	new->y = (tiley<<G_T_SHIFT)-11*BLOCKSIZE;
	new->xdir = -1;
	NewState (new,&s_boobuswalk1);
	new->temp4 = 12;			// hit points
}


/*
===================
=
= FragThink
=
===================
*/

void FragThink (objtype *ob)
{
	if (++ob->temp1 == PREFRAGTHINK)
		ob->state = &s_boobusdie2;
	if (++ob->temp1 == POSTFRAGTHINK)
	{
		RF_RemoveSprite (&ob->sprite);
		ob->state = &s_boobusdie3;
	}

	SD_PlaySound (BOMBBOOMSND);
	GetNewObj (true);
	new->x = ob->x-BLOCKSIZE + 5*US_RndT();
	new->y = ob->y-BLOCKSIZE + 5*US_RndT();
	new->xspeed = 0; //US_RndT()/4-32;
	new->yspeed = 0; //US_RndT()/4-32;
	US_RndT();					// keep rnd from even wrapping
	ChangeState (new,&s_deathboom1);
}


/*
===================
=
= FinishThink
=
===================
*/

void FinishThink (objtype *ob)
{
	playstate = victorious;
	ob++;
	SD_PlaySound (BOOBUSGONESND);
}


/*
====================
=
= BoobusThink
=
====================
*/

void BoobusThink (objtype *ob)
{
	unsigned	move;
	boolean	inline = false;

	if (ob->left > player->right)
		ob->xdir = -1;
	else if (ob->right < player->left)
		ob->xdir = 1;
	else
		inline = true;

	if (player->top < ob->bottom && player->bottom > ob->top)
	{
	// on same level as player, so charge!

	}
	else
	{
	// above or below player, so get directly in line and jump
		if (inline)
		{
			if (ob->y < player->y)
			{
			// drop down
				move = PIXGLOBAL*8;
				ob->tilebottom++;
				ob->bottom += move;
				ob->y += move;
				ob->state = &s_boobusjump;
				ob->yspeed = ob->xmove = ob->xspeed = ob->ymove = 0;
			}
			else
			{
			// jump up
				ob->state = &s_boobusjump;
				ob->yspeed = -SPDBOOBUSJUMP;
				ob->xspeed = 0;
			}

		}
	}
}

/*
====================
=
= BoobusGroundReact
=
====================
*/

void BoobusGroundReact (objtype *ob)
{
	if (ob->xdir == 1 && ob->hitwest)
	{
		ob->x -= ob->xmove;
		ob->xdir = -1;
		ob->nothink = US_RndT()>>5;
		ChangeState (ob,ob->state);
	}
	else if (ob->xdir == -1 && ob->hiteast)
	{
		ob->x -= ob->xmove;
		ob->xdir = 1;
		ob->nothink = US_RndT()>>5;
		ChangeState (ob,ob->state);
	}
	else if (!ob->hitnorth)
	{
		if (ob->bottom >= player->bottom)
		{
		 // jump over
			ob->x -= ob->xmove;
			ob->y -= ob->ymove;
			ob->yspeed = -SPDBOOBUSJUMP;
			ob->xspeed = ob->xdir * SPDBOOBUSRUNJUMP;
		}
		else
		{
		// drop down
			ob->xspeed = ob->yspeed = 0;
		}
		ChangeState (ob,&s_boobusjump);
	}

	PLACESPRITE;
}



/*
====================
=
= BoobusAirReact
=
====================
*/

void BoobusAirReact (objtype *ob)
{
	if (ob->hitsouth)
		ob->yspeed = 0;

	if (ob->hitnorth)
		ChangeState (ob,&s_boobuswalk1);

	PLACESPRITE;
}
