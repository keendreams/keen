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

//
//	ID Engine
//	ID_SD.c - Sound Manager
//	v1.0d1
//	By Jason Blochowiak
//

//
//	This module handles dealing with generating sound on the appropriate
//		hardware
//
//	Depends on: User Mgr (for parm checking)
//
//	Globals:
//		For User Mgr:
//			SoundSourcePresent - Sound Source thingie present?
//			SoundBlasterPresent - SoundBlaster card present?
//			AdLibPresent - AdLib card present?
//			SoundMode - What device is used for sound effects
//				(Use SM_SetSoundMode() to set)
//			MusicMode - What device is used for music
//				(Use SM_SetMusicMode() to set)
//		For Cache Mgr:
//			NeedsDigitized - load digitized sounds?
//			NeedsMusic - load music?
//

#pragma hdrstop		// Wierdo thing with MUSE

#include <dos.h>

#ifdef	_MUSE_      // Will be defined in ID_Types.h
#include "ID_SD.h"
#else
#include "ID_HEADS.H"
#endif
#pragma	hdrstop
#pragma	warn	-pia

#define	SDL_SoundFinished()	{SoundNumber = SoundPriority = 0;}

// Macros for SoundBlaster stuff
#define	sbOut(n,b)	outportb((n) + sbLocation,b)
#define	sbIn(n)		inportb((n) + sbLocation)
#define	sbWriteDelay()	while (sbIn(sbWriteStat) & 0x80);
#define	sbReadDelay()	while (sbIn(sbDataAvail) & 0x80);

// Macros for AdLib stuff
#define	selreg(n)	outportb(0x388,n)
#define	writereg(n)	outportb(0x389,n)
#define	readstat()	inportb(0x388)


//
//	Stuff I need
//
// This table maps channel numbers to carrier and modulator op cells
static	byte			carriers[9] =  { 3, 4, 5,11,12,13,19,20,21},
						modifiers[9] = { 0, 1, 2, 8, 9,10,16,17,18};
static	ActiveTrack		*tracks[sqMaxTracks];
static	word			sqMode,sqFadeStep;

//	Global variables
	boolean		LeaveDriveOn,
				SoundSourcePresent,SoundBlasterPresent,AdLibPresent,
				NeedsDigitized,NeedsMusic;
	SDMode		SoundMode;
	SMMode		MusicMode;
	longword	TimeCount;
	word		*SoundTable;	// Really * _seg *SoundTable, but that don't work
	boolean		ssIsTandy;
	word		ssPort = 2;

//	Internal variables
static	boolean			SD_Started;
static	boolean			TimerDone;
static	word			TimerVal,TimerDelay10,TimerDelay25,TimerDelay100;
static	char			*ParmStrings[] =
						{
							"noal",
							"nosb",
							"nodr",
							"noss",
							"sst",
							"ss1",
							"ss2",
							"ss3",
							nil
						};
static	void			(*SoundUserHook)(void);
static	word			SoundNumber,SoundPriority;
static	void interrupt	(*t0OldService)(void);
static	word			t0CountTable[] = {2,2,2,2,10,10};
static	long			LocalTime;

//	PC Sound variables
static	byte			pcLastSample,far *pcSound;
static	longword		pcLengthLeft;
static	word			pcSoundLookup[255];

//	SoundBlaster variables
static	boolean			sbNoCheck,
						sbSamplePlaying,
						sbIsCompressed,sbCompFirst;
static	byte			sbOldIntMask = -1;
static	byte			huge *sbNextSegPtr;
static	int				sbLocation = -1,sbInterrupt = 7,sbIntVec = 0xf,
						sbIntVectors[] = {-1,-1,0xa,0xb,-1,0xd,-1,0xf};
static	longword		sbNextSegLen;
static	SampledSound	huge *sbSamples;
static	void interrupt	(*sbOldIntHand)(void);

//	SoundSource variables
static	boolean			ssNoCheck,
						ssIsCompressed,ssCompFirst,
						ssIsSlow;
static	word			ssControl,ssStatus,ssData,
						ssHoldOver;
static	byte			ssOn,ssOff,
						far *ssSample;
static	longword		ssLengthLeft;

//	AdLib variables
static	boolean			alNoCheck;
static	byte			far *alSound;
static	word			alBlock;
static	longword		alLengthLeft;

//	Sequencer variables
static	boolean			sqActive;
static	word  			*sqTracks[sqMaxTracks];
static	word			alFXReg;

//	Internal routines

///////////////////////////////////////////////////////////////////////////
//
//	SDL_SetTimer0() - Sets system timer 0 to the specified speed
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_SetTimer0(word speed)
{
#ifndef TPROF	// If using Borland's profiling, don't screw with the timer
	outportb(0x43,0x36);				// Change timer 0
	outportb(0x40,speed);
	outportb(0x40,speed >> 8);
#else
	speed++;	// Shut the compiler up
#endif
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_SetIntsPerSec() - Uses SDL_SetTimer0() to set the number of
//		interrupts generated by system timer 0 per second
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_SetIntsPerSec(word ints)
{
	SDL_SetTimer0(1192755 / ints);
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_TimingService() - Used by SDL_InitDelay() to determine a timing
//		value for the current system that we're running on
//
///////////////////////////////////////////////////////////////////////////
static void interrupt
SDL_TimingService(void)
{
	TimerVal = _CX;
	TimerDone++;

	outportb(0x20,0x20);				// Ack interrupt
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_InitDelay() - Sets up TimerDelay's for SDL_Delay()
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_InitDelay(void)
{
	int		i;
	word	timer,speed;

	setvect(8,SDL_TimingService);		// Set to my timer 0 ISR

	SDL_SetIntsPerSec(1000);			// Time 1ms

	for (i = 0,timer = 0;i < 10;i++)	// Do timing test 10 times
	{
	asm	xor		dx,dx					// Zero DX
	asm	mov		cx,0xffff				// Put starting value in CX
	asm	mov		[TimerDone],cx			// TimerDone = false - 1
startloop:
	asm	or		[TimerDone],0
	asm	jnz		startloop				// Make sure we're at the start
loop:
	asm	test	[TimerDone],1			// See if TimerDone flag got hit
	asm	jnz		done					// Yep - drop out of the loop
	asm	loop	loop
done:

		if (0xffff - TimerVal > timer)
			timer = 0xffff - TimerVal;
	}
	timer += timer / 2;					// Use some slop
	TimerDelay10 =  timer / (1000 / 10);
	TimerDelay25 =  timer / (1000 / 25);
	TimerDelay100 = timer / (1000 / 100);

	SDL_SetTimer0(0);					// Reset timer 0

	setvect(8,t0OldService);			// Set back to old ISR
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_Delay() - Delays the specified amount of time
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_Delay(word delay)
{
	if (!delay)
		return;

asm	mov		cx,[delay]
loop:
asm	test	[TimerDone],0	// Useless code - just for timing equivilency
asm	jnz		done
asm	loop	loop
done:;
}

//
//	PC Sound code
//

///////////////////////////////////////////////////////////////////////////
//
//	SDL_PCPlaySound() - Plays the specified sound on the PC speaker
//
///////////////////////////////////////////////////////////////////////////
#ifdef	_MUSE_
void
#else
static void
#endif
SDL_PCPlaySound(PCSound far *sound)
{
asm	pushf
asm	cli

	pcLastSample = -1;
	pcLengthLeft = sound->common.length;
	pcSound = sound->data;

asm	popf
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_PCStopSound() - Stops the current sound playing on the PC Speaker
//
///////////////////////////////////////////////////////////////////////////
#ifdef	_MUSE_
void
#else
static void
#endif
SDL_PCStopSound(void)
{
asm	pushf
asm	cli

	(long)pcSound = 0;

asm	in	al,0x61		  	// Turn the speaker off
asm	and	al,0xfd			// ~2
asm	out	0x61,al

asm	popf
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_PCService() - Handles playing the next sample in a PC sound
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_PCService(void)
{
	byte	s;
	word	t;

	if (pcSound)
	{
		s = *pcSound++;
		if (s != pcLastSample)
		{
		asm	pushf
		asm	cli

			pcLastSample = s;
			if (s)					// We have a frequency!
			{
				t = pcSoundLookup[s];
			asm	mov	bx,[t]

			asm	mov	al,0xb6			// Write to channel 2 (speaker) timer
			asm	out	43h,al
			asm	mov	al,bl
			asm	out	42h,al			// Low byte
			asm	mov	al,bh
			asm	out	42h,al			// High byte

			asm	in	al,0x61			// Turn the speaker & gate on
			asm	or	al,3
			asm	out	0x61,al
			}
			else					// Time for some silence
			{
			asm	in	al,0x61		  	// Turn the speaker & gate off
			asm	and	al,0xfc			// ~3
			asm	out	0x61,al
			}

		asm	popf
		}

		if (!(--pcLengthLeft))
		{
			SDL_PCStopSound();
			SDL_SoundFinished();
		}
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_ShutPC() - Turns off the pc speaker
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_ShutPC(void)
{
asm	pushf
asm	cli

asm	in	al,0x61		  	// Turn the speaker & gate off
asm	and	al,0xfc			// ~3
asm	out	0x61,al

asm	popf
}

//
//	SoundBlaster code
//

///////////////////////////////////////////////////////////////////////////
//
//	SDL_SBStopSample() - Stops any active sampled sound and causes DMA
//		requests from the SoundBlaster to cease
//
///////////////////////////////////////////////////////////////////////////
#ifdef	_MUSE_
void
#else
static void
#endif
SDL_SBStopSample(void)
{
	byte	is;

	if (sbSamplePlaying)
	{
		sbSamplePlaying = false;

		sbWriteDelay();
		sbOut(sbWriteCmd,0xd0);	// Turn off DSP DMA

		is = inportb(0x21);	// Restore interrupt mask bit
		if (sbOldIntMask & (1 << sbInterrupt))
			is |= (1 << sbInterrupt);
		else
			is &= ~(1 << sbInterrupt);
		outportb(0x21,is);
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_SBPlaySeg() - Plays a chunk of sampled sound on the SoundBlaster
//	Insures that the chunk doesn't cross a bank boundary, programs the DMA
//	 controller, and tells the SB to start doing DMA requests for DAC
//
///////////////////////////////////////////////////////////////////////////
static longword
SDL_SBPlaySeg(byte huge *data,longword length)
{
	unsigned		datapage;
	longword		dataofs,uselen;

	uselen = length;
	datapage = FP_SEG(data) >> 12;
	dataofs = ( (FP_SEG(data)&0xfff)<<4 ) + FP_OFF(data);
	if (dataofs>=0x10000)
	{
	  datapage++;
	  dataofs-=0x10000;
	}

	if (dataofs + uselen > 0x10000)
		uselen = 0x10000 - dataofs;

	uselen--;

	// Program the DMA controller
	outportb(0x0a,5);							// Mask off channel 1 DMA
	outportb(0x0c,0);							// Clear byte ptr F/F to lower byte
	outportb(0x0b,0x49);						// Set transfer mode for D/A conv
	outportb(0x02,(byte)dataofs);				// Give LSB of address
	outportb(0x02,(byte)(dataofs >> 8));		// Give MSB of address
	outportb(0x83,(byte)datapage);				// Give page of address
	outportb(0x03,(byte)uselen);				// Give LSB of length
	outportb(0x03,(byte)(uselen >> 8));			// Give MSB of length
	outportb(0x0a,1);							// Turn on channel 1 DMA

	// Start playing the thing
	sbWriteDelay();
	sbOut(sbWriteCmd,0x14);
	sbWriteDelay();
	sbOut(sbWriteData,(byte)uselen);
	sbWriteDelay();
	sbOut(sbWriteData,(byte)(uselen >> 8));

	return(uselen + 1);
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_SBService() - Services the SoundBlaster DMA interrupt
//
///////////////////////////////////////////////////////////////////////////
static void interrupt
SDL_SBService(void)
{
	longword	used;

	sbIn(sbDataAvail);	// Ack interrupt to SB

	if (sbNextSegPtr)
	{
		used = SDL_SBPlaySeg(sbNextSegPtr,sbNextSegLen);
		if (sbNextSegLen <= used)
			sbNextSegPtr = nil;
		else
		{
			sbNextSegPtr += used;
			sbNextSegLen -= used;
		}
	}
	else
	{
		SDL_SBStopSample();
		SDL_SoundFinished();
	}

	outportb(0x20,0x20);	// Ack interrupt
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_SBPlaySample() - Plays a sampled sound on the SoundBlaster. Sets up
//		DMA to play the sound
//
///////////////////////////////////////////////////////////////////////////
#ifdef	_MUSE_
void
#else
static void
#endif
SDL_SBPlaySample(SampledSound far *sample)
{
	byte			huge *data,
					timevalue;
	longword		used;

	SDL_SBStopSample();

	data = (byte huge *)(sample->data);
	timevalue = 256 - (1000000 / sample->hertz);

	// DEBUG - deal with compressed sounds

	// Set the SoundBlaster DAC time constant
	sbWriteDelay();
	sbOut(sbWriteCmd,0x40);
	sbWriteDelay();
	sbOut(sbWriteData,timevalue);

	used = SDL_SBPlaySeg(data,sample->common.length);
	if (sample->common.length <= used)
		sbNextSegPtr = nil;
	else
	{
		sbNextSegPtr = data + used;
		sbNextSegLen = sample->common.length - used;
	}

	// Save old interrupt status and unmask ours
	sbOldIntMask = inportb(0x21);
	outportb(0x21,sbOldIntMask & ~(1 << sbInterrupt));

	sbWriteDelay();
	sbOut(sbWriteCmd,0xd4);						// Make sure DSP DMA is enabled

	sbSamplePlaying = true;
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_CheckSB() - Checks to see if a SoundBlaster resides at a
//		particular I/O location
//
///////////////////////////////////////////////////////////////////////////
static boolean
SDL_CheckSB(int port)
{
	int	i;

	sbLocation = port << 4;		// Initialize stuff for later use

	sbOut(sbReset,true);		// Reset the SoundBlaster DSP
	SDL_Delay(TimerDelay10);	// Wait 4usec
	sbOut(sbReset,false);		// Turn off sb DSP reset
	SDL_Delay(TimerDelay100);	// Wait 100usec
	for (i = 0;i < 100;i++)
	{
		if (sbIn(sbDataAvail) & 0x80)		// If data is available...
		{
			if (sbIn(sbReadData) == 0xaa)	// If it matches correct value
				return(true);
			else
			{
				sbLocation = -1;			// Otherwise not a SoundBlaster
				return(false);
			}
		}
	}
	sbLocation = -1;						// Retry count exceeded - fail
	return(false);
}

///////////////////////////////////////////////////////////////////////////
//
//	Checks to see if a SoundBlaster is in the system. If the port passed is
//		-1, then it scans through all possible I/O locations. If the port
//		passed is 0, then it uses the default (2). If the port is >0, then
//		it just passes it directly to SDL_CheckSB()
//
///////////////////////////////////////////////////////////////////////////
static boolean
SDL_DetectSoundBlaster(int port)
{
	int	i;

	if (port == 0)					// If user specifies default, use 2
		port = 2;
	if (port == -1)
	{
		if (SDL_CheckSB(2))			// Check default before scanning
			return(true);

		for (i = 1;i <= 6;i++)		// Scan through possible SB locations
			if (SDL_CheckSB(i))		// If found at this address,
				return(true);		//	return success
		return(false);				// All addresses failed, return failure
	}
	else
		return(SDL_CheckSB(port));	// User specified address or default
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_StartSB() - Turns on the SoundBlaster
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_StartSB(void)
{
	sbOldIntHand = getvect(sbIntVec);	// Get old interrupt handler
	setvect(sbIntVec,SDL_SBService);	// Set mine

	sbWriteDelay();
	sbOut(sbWriteCmd,0xd1);				// Turn on DSP speaker
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_ShutSB() - Turns off the SoundBlaster
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_ShutSB(void)
{
	SDL_SBStopSample();

	setvect(sbIntVec,sbOldIntHand);		// Set vector back
}

//	Sound Source Code

///////////////////////////////////////////////////////////////////////////
//
//	SDL_SSStopSample() - Stops a sample playing on the Sound Source
//
///////////////////////////////////////////////////////////////////////////
#ifdef	_MUSE_
void
#else
static void
#endif
SDL_SSStopSample(void)
{
	(long)ssSample = 0;
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_SSService() - Handles playing the next sample on the Sound Source
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_SSService(void)
{
	boolean	gotit;
	byte	v;

	while (ssSample)
	{
	asm	mov		dx,[ssStatus]	// Check to see if FIFO is currently empty
	asm	in		al,dx
	asm	test	al,0x40
	asm	jnz		done			// Nope - don't push any more data out

		gotit = false;
		if (ssIsSlow && (ssHoldOver != (word)-1))
		{
			gotit = true;
			v = ssHoldOver;
			ssHoldOver = -1;
		}
		else
		{
			if (ssIsCompressed)
			{
				if (ssCompFirst)
				{
					// DEBUG - not written
				}
				// DEBUG - not written
			}
			else
			{
				v = *ssSample++;
				gotit = true;
				if (ssIsSlow)
					ssHoldOver = v;
				if (!(--ssLengthLeft))
				{
					(long)ssSample = 0;
					SDL_SoundFinished();
				}
			}
		}

		if (gotit)
		{
		asm	mov		dx,[ssData]		// Pump the value out
		asm	mov		al,[v]
		asm	out		dx,al

		asm	mov		dx,[ssControl]	// Pulse printer select
		asm	mov		al,[ssOff]
		asm	out		dx,al
		asm	push	ax
		asm	pop		ax
		asm	mov		al,[ssOn]
		asm	out		dx,al

		asm	push	ax				// Delay a short while
		asm	pop		ax
		asm	push	ax
		asm	pop		ax
		}
	}
done:
	;	// Garbage for compiler
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_SSPlaySample() - Plays the specified sample on the Sound Source
//
///////////////////////////////////////////////////////////////////////////
#ifdef	_MUSE_
void
#else
static void
#endif
SDL_SSPlaySample(SampledSound far *sample)
{
asm	pushf
asm	cli

	ssLengthLeft = sample->common.length;
	ssSample = sample->data;
	ssIsSlow = sample->hertz < 7000;
	ssHoldOver = -1;
	if (sample->bits < 8)
		ssIsCompressed = ssCompFirst = true;

asm	popf
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_StartSS() - Sets up for and turns on the Sound Source
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_StartSS(void)
{
	if (ssPort == 3)
		ssControl = 0x27a;	// If using LPT3
	else if (ssPort == 2)
		ssControl = 0x37a;	// If using LPT2
	else
		ssControl = 0x3be;	// If using LPT1
	ssStatus = ssControl - 1;
	ssData = ssStatus - 1;

	ssOn = 0x04;
	if (ssIsTandy)
		ssOff = 0x0e;				// Tandy wierdness
	else
		ssOff = 0x0c;				// For normal machines

	outportb(ssControl,ssOn);		// Enable SS
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_ShutSS() - Turns off the Sound Source
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_ShutSS(void)
{
	outportb(ssControl,ssOff);
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_CheckSS() - Checks to see if a Sound Source is present at the
//		location specified by the sound source variables
//
///////////////////////////////////////////////////////////////////////////
static boolean
SDL_CheckSS(void)
{
	boolean		present = false;
	longword	lasttime;

	// Turn the Sound Source on and wait awhile (4 ticks)
	SDL_StartSS();

	lasttime = TimeCount;
	while (TimeCount < lasttime + 4)
		;

asm	mov		dx,[ssStatus]	// Check to see if FIFO is currently empty
asm	in		al,dx
asm	test	al,0x40
asm	jnz		checkdone		// Nope - Sound Source not here

asm	mov		cx,32			// Force FIFO overflow (FIFO is 16 bytes)
outloop:
asm	mov		dx,[ssData]		// Pump a neutral value out
asm	mov		al,0x80
asm	out		dx,al

asm	mov		dx,[ssControl]	// Pulse printer select
asm	mov		al,[ssOff]
asm	out		dx,al
asm	push	ax
asm	pop		ax
asm	mov		al,[ssOn]
asm	out		dx,al

asm	push	ax				// Delay a short while before we do this again
asm	pop		ax
asm	push	ax
asm	pop		ax

asm	loop	outloop

asm	mov		dx,[ssStatus]	// Is FIFO overflowed now?
asm	in		al,dx
asm	test	al,0x40
asm	jz		checkdone		// Nope, still not - Sound Source not here

	present = true;			// Yes - it's here!

checkdone:
	SDL_ShutSS();
	return(present);
}

static boolean
SDL_DetectSoundSource(void)
{
	for (ssPort = 1;ssPort <= 3;ssPort++)
		if (SDL_CheckSS())
			return(true);
	return(false);
}

// 	AdLib Code

///////////////////////////////////////////////////////////////////////////
//
//	alOut(n,b) - Puts b in AdLib card register n
//
///////////////////////////////////////////////////////////////////////////
void
alOut(byte n,byte b)
{
	asm	pushf
	asm	cli

	asm	mov		dx,0x388
	asm	mov		al,[n]
	asm	out		dx,al
	SDL_Delay(TimerDelay10);

	asm	mov		dx,0x389
	asm	mov		al,[b]
	asm	out		dx,al

	asm	popf

	SDL_Delay(TimerDelay25);
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_SetInstrument() - Puts an instrument into a generator
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_SetInstrument(int which,Instrument *inst)
{
	byte		c,m;

	// DEBUG - what about percussive instruments?

	m = modifiers[which];
	c = carriers[which];
	tracks[which]->inst = *inst;

	alOut(m + alChar,inst->mChar);
	alOut(m + alScale,inst->mScale);
	alOut(m + alAttack,inst->mAttack);
	alOut(m + alSus,inst->mSus);
	alOut(m + alWave,inst->mWave);

	alOut(c + alChar,inst->cChar);
	alOut(c + alScale,inst->cScale);
	alOut(c + alAttack,inst->cAttack);
	alOut(c + alSus,inst->cSus);
	alOut(c + alWave,inst->cWave);
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_ALStopSound() - Turns off any sound effects playing through the
//		AdLib card
//
///////////////////////////////////////////////////////////////////////////
#ifdef	_MUSE_
void
#else
static void
#endif
SDL_ALStopSound(void)
{
asm	pushf
asm	cli

	(long)alSound = 0;
	alOut(alFreqH + 0,0);

asm	popf
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_ALPlaySound() - Plays the specified sound on the AdLib card
//
///////////////////////////////////////////////////////////////////////////
#ifdef	_MUSE_
void
#else
static void
#endif
SDL_ALPlaySound(AdLibSound far *sound)
{
	byte		c,m;
	Instrument	far *inst;

asm	pushf
asm	cli

	SDL_ALStopSound();

	alLengthLeft = sound->common.length;
	alSound = sound->data;
	alBlock = ((sound->block & 7) << 2) | 0x20;
	inst = &sound->inst;

	if (!(inst->mSus | inst->cSus))
		Quit("SDL_ALPlaySound() - Seriously suspicious instrument");

	m = modifiers[0];
	c = carriers[0];
	alOut(m + alChar,inst->mChar);
	alOut(m + alScale,inst->mScale);
	alOut(m + alAttack,inst->mAttack);
	alOut(m + alSus,inst->mSus);
	alOut(m + alWave,inst->mWave);
	alOut(c + alChar,inst->cChar);
	alOut(c + alScale,inst->cScale);
	alOut(c + alAttack,inst->cAttack);
	alOut(c + alSus,inst->cSus);
	alOut(c + alWave,inst->cWave);

asm	popf
}

///////////////////////////////////////////////////////////////////////////
//
// 	SDL_ALSoundService() - Plays the next sample out through the AdLib card
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_ALSoundService(void)
{
	byte	s;

	if (alSound)
	{
		s = *alSound++;
		if (!s)
			alOut(alFreqH + 0,0);
		else
		{
			alOut(alFreqL + 0,s);
			alOut(alFreqH + 0,alBlock);
		}

		if (!(--alLengthLeft))
		{
			(long)alSound = 0;
			alOut(alFreqH + 0,0);
			SDL_SoundFinished();
		}
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_SelectMeasure() - sets up sequencing variables for a given track
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_SelectMeasure(ActiveTrack *track)
{
	track->seq = track->moods[track->mood];
	track->nextevent = 0;
}

///////////////////////////////////////////////////////////////////////////
//
//	My AdLib interrupt service routine
//	Called 140 times/second by SDL_t0Service()
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_ALService(void)
{
	boolean		update;
	word		*seq;
	longword	next;
	int			i;
	Instrument	*ins;
	ActiveTrack	*track;

	if (!sqActive)
		return;

	for (i = 1;i <= sqMaxTracks;i++)
	{
		if (!((track = tracks[i - 1]) && (seq = track->seq)))
			continue;

		if ((sqMode != sqmode_Normal) && (sqFadeStep < sqMaxFade))
		{
			// DEBUG - not done
			sqFadeStep++;
		}

		next = track->nextevent;
		while (next <= TimeCount)
		{
			update = true;
			switch (*seq++)
			{
			case sev_NoteOff:
				alOut(alFreqH + i,0);	/* Turn the note off */
				next = TimeCount + *seq;
				break;
			case sev_NoteOn:
			case sev_NotePitch:
				next = TimeCount + *seq++;
				alOut(alFreqL + i,*seq);		/* Program low 8 bits of freq */
				alOut(alFreqH + i,(*seq >> 8) | 0x20);	/* & hi 5 bits and turn note on */
				break;
			case sev_NewInst:
				ins = (Instrument *)seq;
				SDL_SetInstrument(i,ins++);
				seq = (word *)(ins);
				next = TimeCount + *seq;
				break;
			case sev_NewPerc:
				// DEBUG - write this
				break;
			case sev_PercOn:
				alFXReg |= (1 << *seq);
				alOut(alEffects,alFXReg);
				break;
			case sev_PercOff:
				alFXReg &= ~(1 << *seq);
				alOut(alEffects,alFXReg);
				break;
			case sev_SeqEnd:
				SDL_SelectMeasure(track);
				next = track->nextevent;
				if (!(seq = track->seq))
					continue;
				update = false;
				break;
			case sev_Null:
				next = TimeCount + *seq;
				break;
			default:
				break;
			}
			if (update)
			{
				track->seq = ++seq;
				track->nextevent = next;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_ShutAL() - Shuts down the AdLib card for sound effects
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_ShutAL(void)
{
	word	i;

	alOut(alEffects,0);
	alOut(alFreqH + 0,0);
	// DEBUG - reprogram with null instrument
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_ShutAL() - Starts up the AdLib card for sound effects
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_StartAL(void)
{
	alFXReg = 0;
	alOut(alEffects,alFXReg);
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_DetectAdLib() - Determines if there's an AdLib (or SoundBlaster
//		emulating an AdLib) present
//
///////////////////////////////////////////////////////////////////////////
static boolean
SDL_DetectAdLib(void)
{
	byte	status1,status2;
	int		i;

	alOut(4,0x60);	// Reset T1 & T2
	alOut(4,0x80);	// Reset IRQ
	status1 = readstat();
	alOut(2,0xff);	// Set timer 1
	alOut(4,0x21);	// Start timer 1
	SDL_Delay(TimerDelay100);

	status2 = readstat();
	alOut(4,0x60);
	alOut(4,0x80);

	if (((status1 & 0xe0) == 0x00) && ((status2 & 0xe0) == 0xc0))
		return(true);
	else
		return(false);
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_t0Service() - My timer 0 ISR which handles the different timings and
//		dispatches to whatever other routines are appropriate
//
///////////////////////////////////////////////////////////////////////////
static void interrupt
SDL_t0Service(void)
{
	byte		sdcount;
static	word	count = 1,
				alcount = 1,
				drivecount = 1;

	switch (SoundMode)
	{
	case sdm_PC:
		SDL_PCService();
		break;
	case sdm_AdLib:
		SDL_ALSoundService();
		break;
	case sdm_SoundSource:
		SDL_SSService();
		break;
	}

	if ((MusicMode == smm_AdLib) && !(--alcount))
	{
		alcount = t0CountTable[SoundMode] / 2;
		SDL_ALService();
	}

	if (!(--count))
	{
		count = t0CountTable[SoundMode];
		LocalTime++;
		TimeCount++;
		if (SoundUserHook)
			SoundUserHook();

		// If one of the drives is on, and we're not told to leave it on...
		if ((peekb(0x40,0x3f) & 3) && !LeaveDriveOn)
		{
			if (!(--drivecount))
			{
				drivecount = 5;

				sdcount = peekb(0x40,0x40);	// Get system drive count
				if (sdcount < 2)			// Time to turn it off
				{
					// Wait until it's off
					while ((peekb(0x40,0x3f) & 3))
					{
					asm	pushf
						t0OldService();
					}
				}
				else	// Not time yet, just decrement counter
					pokeb(0x40,0x40,--sdcount);
			}
		}
	}

	outportb(0x20,0x20);	// Ack the interrupt
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_ShutDevice() - turns off whatever device was being used for sound fx
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_ShutDevice(void)
{
	switch (SoundMode)
	{
	case sdm_PC:
		SDL_ShutPC();
		break;
	case sdm_AdLib:
		SDL_ShutAL();
		break;
	case sdm_SoundBlaster:
		SDL_ShutSB();
		break;
	case sdm_SoundSource:
		SDL_ShutSS();
		break;
	}
	SoundMode = sdm_Off;
}

///////////////////////////////////////////////////////////////////////////
//
//	SDL_StartDevice() - turns on whatever device is to be used for sound fx
//
///////////////////////////////////////////////////////////////////////////
static void
SDL_StartDevice(void)
{
	switch (SoundMode)
	{
	case sdm_AdLib:
		SDL_StartAL();
		break;
	case sdm_SoundBlaster:
		SDL_StartSB();
		break;
	case sdm_SoundSource:
		SDL_StartSS();
		break;
	}
	SoundNumber = SoundPriority = 0;
}

//	Public routines

///////////////////////////////////////////////////////////////////////////
//
//	SD_SetSoundMode() - Sets which sound hardware to use for sound effects
//
///////////////////////////////////////////////////////////////////////////
boolean
SD_SetSoundMode(SDMode mode)
{
	boolean	result;
	word	rate,speed,
			tableoffset;

	SD_StopSound();

	switch (mode)
	{
	case sdm_Off:
		NeedsDigitized = false;
		result = true;
		break;
	case sdm_PC:
		tableoffset = STARTPCSOUNDS;
		NeedsDigitized = false;
		result = true;
		break;
	case sdm_AdLib:
		if (AdLibPresent)
		{
			tableoffset = STARTADLIBSOUNDS;
			NeedsDigitized = false;
			result = true;
		}
		break;
	case sdm_SoundBlaster:
		if (SoundBlasterPresent)
		{
			tableoffset = STARTDIGISOUNDS;
			NeedsDigitized = true;
			result = true;
		}
		break;
	case sdm_SoundSource:
		tableoffset = STARTDIGISOUNDS;
		NeedsDigitized = true;
		result = true;
		break;
	default:
		result = false;
		break;
	}

	if (result && (mode != SoundMode))
	{
		SDL_ShutDevice();
		SoundMode = mode;
#ifndef	_MUSE_
		SoundTable = (word *)(&audiosegs[tableoffset]);
#endif
		SDL_StartDevice();
	}

	rate = TickBase * t0CountTable[SoundMode];
	SDL_SetIntsPerSec(rate);

	return(result);
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_SetMusicMode() - sets the device to use for background music
//
///////////////////////////////////////////////////////////////////////////
boolean
SD_SetMusicMode(SMMode mode)
{
	boolean	result;

	SD_FadeOutMusic();
	while (SD_MusicPlaying())
		;

	switch (mode)
	{
	case smm_Off:
		NeedsMusic = false;
		result = true;
		break;
	case smm_AdLib:
		if (AdLibPresent)
		{
			NeedsMusic = true;
			result = true;
		}
		break;
	default:
		result = false;
		break;
	}

	if (result)
		MusicMode = mode;

	return(result);
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_Startup() - starts up the Sound Mgr
//		Detects all additional sound hardware and installs my ISR
//
///////////////////////////////////////////////////////////////////////////
void
SD_Startup(void)
{
	int	i;

	if (SD_Started)
		return;

	ssNoCheck = false;
	ssIsTandy = false;
	alNoCheck = false;
	sbNoCheck = false;
	LeaveDriveOn = false;
#ifndef	_MUSE_
	for (i = 1;i < _argc;i++)
	{
		switch (US_CheckParm(_argv[i],ParmStrings))
		{
		case 0:						// No AdLib detection
			alNoCheck = true;
			break;
		case 1:						// No SoundBlaster detection
			sbNoCheck = true;
			break;
		case 2:
			LeaveDriveOn = true;	// No drive turnoff
			break;
		case 3:
			ssNoCheck = true;		// No Sound Source detection
			break;
		case 4:						// Tandy Sound Source handling
			ssIsTandy = true;
			break;
		case 5:						// Sound Source present at LPT1
			ssPort = 1;
			ssNoCheck = SoundSourcePresent = true;
			break;
		case 6:                     // Sound Source present at LPT2
			ssPort = 2;
			ssNoCheck = SoundSourcePresent = true;
			break;
		case 7:                     // Sound Source present at LPT3
			ssPort = 3;
			ssNoCheck = SoundSourcePresent = true;
			break;
		}
	}
#endif

	SoundUserHook = 0;

	t0OldService = getvect(8);	// Get old timer 0 ISR

	SDL_InitDelay();			// SDL_InitDelay() uses t0OldService

	setvect(8,SDL_t0Service);	// Set to my timer 0 ISR
	LocalTime = TimeCount = 0;

	SD_SetSoundMode(sdm_Off);
	SD_SetMusicMode(smm_Off);

	if (!ssNoCheck)
		SoundSourcePresent = SDL_DetectSoundSource();

	if (!alNoCheck)
	{
		AdLibPresent = SDL_DetectAdLib();
		if (AdLibPresent && !sbNoCheck)
			SoundBlasterPresent = SDL_DetectSoundBlaster(-1);
	}

	for (i = 0;i < 255;i++)
		pcSoundLookup[i] = i * 60;

	SD_Started = true;
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_Default() - Sets up the default behaviour for the Sound Mgr whether
//		the config file was present or not.
//
///////////////////////////////////////////////////////////////////////////
void
SD_Default(boolean gotit,SDMode sd,SMMode sm)
{
	boolean	gotsd,gotsm;

	gotsd = gotsm = gotit;

	if (gotsd)	// Make sure requested sound hardware is available
	{
		switch (sd)
		{
		case sdm_AdLib:
			gotsd = AdLibPresent;
			break;
		case sdm_SoundBlaster:
			gotsd = SoundBlasterPresent;
			break;
		case sdm_SoundSource:
			gotsd = SoundSourcePresent;
			break;
		}
	}
	if (!gotsd)
	{
#if 0	// DEBUG - hack for Keen Dreams because of no space...
		// Use the best sound hardware available
		if (SoundBlasterPresent)
			sd = sdm_SoundBlaster;
		else if (SoundSourcePresent)
			sd = sdm_SoundSource;
		else if (AdLibPresent)
			sd = sdm_AdLib;
		else
			sd = sdm_PC;
#else
		if (AdLibPresent)
			sd = sdm_AdLib;
		else
			sd = sdm_PC;
#endif
	}
	if (sd != SoundMode)
		SD_SetSoundMode(sd);


	if (gotsm)	// Make sure requested music hardware is available
	{
		switch (sm)
		{
		case sdm_AdLib:
			gotsm = AdLibPresent;
			break;
		}
	}
	if (!gotsm)
	{
#if 0	// DEBUG - hack for Keen Dreams because of no space...
		if (AdLibPresent)
			sm = smm_AdLib;
#endif
	}
	if (sm != MusicMode)
		SD_SetMusicMode(sm);
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_Shutdown() - shuts down the Sound Mgr
//		Removes sound ISR and turns off whatever sound hardware was active
//
///////////////////////////////////////////////////////////////////////////
void
SD_Shutdown(void)
{
	if (!SD_Started)
		return;

	SDL_ShutDevice();

	asm	pushf
	asm	cli

	SDL_SetTimer0(0);

	setvect(8,t0OldService);

	asm	popf
	// DEBUG - set the system clock

	SD_Started = false;
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_SetUserHook() - sets the routine that the Sound Mgr calls every 1/70th
//		of a second from its timer 0 ISR
//
///////////////////////////////////////////////////////////////////////////
void
SD_SetUserHook(void (* hook)(void))
{
	SoundUserHook = hook;
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_PlaySound() - plays the specified sound on the appropriate hardware
//
///////////////////////////////////////////////////////////////////////////
void
SD_PlaySound(word sound)
{
	SoundCommon	far *s;

	if (SoundMode == sdm_Off)
		return;

	s = MK_FP(SoundTable[sound],0);
	if (!s)
		Quit("SD_PlaySound() - Attempted to play an uncached sound");
	if (s->priority < SoundPriority)
		return;

	switch (SoundMode)
	{
	case sdm_PC:
		SDL_PCPlaySound((void far *)s);
		break;
	case sdm_AdLib:
		SDL_ALPlaySound((void far *)s);
		break;
	case sdm_SoundBlaster:
		SDL_SBPlaySample((void far *)s);
		break;
	case sdm_SoundSource:
		SDL_SSPlaySample((void far *)s);
		break;
	}

	SoundNumber = sound;
	SoundPriority = s->priority;
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_SoundPlaying() - returns the sound number that's playing, or 0 if
//		no sound is playing
///////////////////////////////////////////////////////////////////////////
word
SD_SoundPlaying(void)
{
	boolean	result = false;

	switch (SoundMode)
	{
	case sdm_PC:
		result = pcSound? true : false;
		break;
	case sdm_AdLib:
		result = alSound? true : false;
		break;
	case sdm_SoundBlaster:
		result = sbSamplePlaying;
		break;
	case sdm_SoundSource:
		result = ssSample? true : false;
		break;
	}

	if (result)
		return(SoundNumber);
	else
		return(false);
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_StopSound() - if a sound is playing, stops it
//
///////////////////////////////////////////////////////////////////////////
void
SD_StopSound(void)
{
	switch (SoundMode)
	{
	case sdm_PC:
		SDL_PCStopSound();
		break;
	case sdm_AdLib:
		SDL_ALStopSound();
		break;
	case sdm_SoundBlaster:
		SDL_SBStopSample();
		break;
	case sdm_SoundSource:
		SDL_SSStopSample();
		break;
	}

	SDL_SoundFinished();
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_WaitSoundDone() - waits until the current sound is done playing
//
///////////////////////////////////////////////////////////////////////////
void
SD_WaitSoundDone(void)
{
	while (SD_SoundPlaying())
		;
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_StartMusic() - starts playing the music pointed to
//
///////////////////////////////////////////////////////////////////////////
void
SD_StartMusic(Ptr music)	// DEBUG - this shouldn't be a Ptr...
{
	switch (MusicMode)
	{
	case smm_AdLib:
		music = music;
		// DEBUG - not written
		break;
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_FadeOutMusic() - starts fading out the music. Call SD_MusicPlaying()
//		to see if the fadeout is complete
//
///////////////////////////////////////////////////////////////////////////
void
SD_FadeOutMusic(void)
{
	switch (MusicMode)
	{
	case smm_AdLib:
		// DEBUG - not written
		break;
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	SD_MusicPlaying() - returns true if music is currently playing, false if
//		not
//
///////////////////////////////////////////////////////////////////////////
boolean
SD_MusicPlaying(void)
{
	boolean	result;

	switch (MusicMode)
	{
	case smm_AdLib:
		result = false;
		// DEBUG - not written
		break;
	default:
		result = false;
	}

	return(result);
}
