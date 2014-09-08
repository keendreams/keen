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

// NEWMM.H

#ifndef __ID_MM__

#define __ID_MM__

#ifndef __TYPES__
#include "ID_TYPES.H"
#endif

void Quit (char *error);


//==========================================================================

#define SAVENEARHEAP	0x400		// space to leave in data segment
#define SAVEFARHEAP		0			// space to leave in far heap

#define	BUFFERSIZE		0x1000		// miscelanious, allways available buffer

#define MAXBLOCKS		1300

//==========================================================================

typedef void _seg * memptr;

typedef struct
{
	long	nearheap,farheap,EMSmem,XMSmem,mainmem;
} mminfotype;

//==========================================================================

extern	mminfotype	mminfo;
extern	memptr		bufferseg;
extern	boolean		bombonerror;

extern	void		(* beforesort) (void);
extern	void		(* aftersort) (void);

//==========================================================================

void MM_Startup (void);
void MM_Shutdown (void);
void MM_MapEMS (void);

void MM_GetPtr (memptr *baseptr,unsigned long size);
void MM_FreePtr (memptr *baseptr);

void MM_SetPurge (memptr *baseptr, int purge);
void MM_SetLock (memptr *baseptr, boolean locked);
void MM_SortMem (void);

void MM_ShowMemory (void);

long MM_UnusedMemory (void);
long MM_TotalFree (void);


#endif