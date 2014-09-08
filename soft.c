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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <alloc.h>
#include <fcntl.h>
#include <dos.h>
#include <io.h>

#include "kd_def.h"
//#include "gelib.h"
#include "soft.h"
#include "lzw.h"
#include "lzhuff.h"
#include "jam_io.h"


BufferedIO lzwBIO;




//===========================================================================
//
//										SWITCHES
//
//===========================================================================

#define LZH_SUPPORT		1
#define LZW_SUPPORT		0




//=========================================================================
//
//
//								GENERAL LOAD ROUTINES
//
//
//=========================================================================



//--------------------------------------------------------------------------
// BLoad()			-- THIS HAS NOT BEEN FULLY TESTED!
//
// NOTICE : This version of BLOAD is compatable with JAMPak V3.0 and the
//				new fileformat...
//--------------------------------------------------------------------------
unsigned long BLoad(char *SourceFile, memptr *DstPtr)
{
	int handle;

	memptr SrcPtr;
	unsigned long i, j, k, r, c;
	word flags;
	byte Buffer[8];
	unsigned long SrcLen,DstLen;
	struct CMP1Header CompHeader;
	boolean Compressed = false;


	memset((void *)&CompHeader,0,sizeof(struct CMP1Header));

	//
	// Open file to load....
	//

	if ((handle = open(SourceFile, O_RDONLY|O_BINARY)) == -1)
		return(0);

	//
	// Look for JAMPAK headers
	//

	read(handle,Buffer,4);

	if (!strncmp(Buffer,COMP,4))
	{
		//
		// Compressed under OLD file format
		//

		Compressed = true;
		SrcLen = Verify(SourceFile);

		read(handle,(void *)&CompHeader.OrginalLen,4);
		CompHeader.CompType = ct_LZW;
		MM_GetPtr(DstPtr,CompHeader.OrginalLen);
		if (!*DstPtr)
			return(0);
	}
	else
	if (!strncmp(Buffer,CMP1,4))
	{
		//
		// Compressed under new file format...
		//

		Compressed = true;
		SrcLen = Verify(SourceFile);

		read(handle,(void *)&CompHeader,sizeof(struct CMP1Header));
		MM_GetPtr(DstPtr,CompHeader.OrginalLen);
		if (!*DstPtr)
			return(0);
	}
	else
		DstLen = Verify(SourceFile);


	//
	// Load the file in memory...
	//

	if (Compressed)
	{
		DstLen = CompHeader.OrginalLen;

		if ((MM_TotalFree() < SrcLen) && (CompHeader.CompType))
		{
			if (!InitBufferedIO(handle,&lzwBIO))
				Quit("No memory for buffered I/O.");

			switch (CompHeader.CompType)
			{
				#if LZW_SUPPORT
				case ct_LZW:
					lzwDecompress(&lzwBIO,MK_FP(*DstPtr,0),CompHeader.OrginalLen,(SRC_BFILE|DEST_MEM));
				break;
				#endif

				#if LZH_SUPPORT
				case ct_LZH:
					lzhDecompress(&lzwBIO,MK_FP(*DstPtr,0),CompHeader.OrginalLen,CompHeader.CompressLen,(SRC_BFILE|DEST_MEM));
				break;
				#endif

				default:
					Quit("BLoad() - Unrecognized/Supported compression");
				break;
			}

			FreeBufferedIO(&lzwBIO);
		}
		else
		{
			CA_LoadFile(SourceFile,&SrcPtr);
			switch (CompHeader.CompType)
			{
				#if LZW_SUPPORT
				case ct_LZW:
					lzwDecompress(MK_FP(SrcPtr,8),MK_FP(*DstPtr,0),CompHeader.OrginalLen,(SRC_MEM|DEST_MEM));
				break;
				#endif

				#if LZH_SUPPORT
				case ct_LZH:
					lzhDecompress(MK_FP(SrcPtr,8),MK_FP(*DstPtr,0),CompHeader.OrginalLen,CompHeader.CompressLen,(SRC_MEM|DEST_MEM));
				break;
				#endif

				default:
					Quit("BLoad() - Unrecognized/Supported compression");
				break;
			}
			MM_FreePtr(&SrcPtr);
		}
	}
	else
		CA_LoadFile(SourceFile,DstPtr);

	close(handle);
	return(DstLen);
}




////////////////////////////////////////////////////////////////////////////
//
// LoadLIBShape()
//
int LoadLIBShape(char *SLIB_Filename, char *Filename,struct Shape *SHP)
{
	#define CHUNK(Name)	(*ptr == *Name) &&			\
								(*(ptr+1) == *(Name+1)) &&	\
								(*(ptr+2) == *(Name+2)) &&	\
								(*(ptr+3) == *(Name+3))


	int RT_CODE;
	FILE *fp;
	char CHUNK[5];
	char far *ptr;
	memptr IFFfile = NULL;
	unsigned long FileLen, size, ChunkLen;
	int loop;


	RT_CODE = 1;

	// Decompress to ram and return ptr to data and return len of data in
	//	passed variable...

	if (!LoadLIBFile(SLIB_Filename,Filename,&IFFfile))
		Quit("Error Loading Compressed lib shape!");

	// Evaluate the file
	//
	ptr = MK_FP(IFFfile,0);
	if (!CHUNK("FORM"))
		goto EXIT_FUNC;
	ptr += 4;

	FileLen = *(long far *)ptr;
	SwapLong((long far *)&FileLen);
	ptr += 4;

	if (!CHUNK("ILBM"))
		goto EXIT_FUNC;
	ptr += 4;

	FileLen += 4;
	while (FileLen)
	{
		ChunkLen = *(long far *)(ptr+4);
		SwapLong((long far *)&ChunkLen);
		ChunkLen = (ChunkLen+1) & 0xFFFFFFFE;

		if (CHUNK("BMHD"))
		{
			ptr += 8;
			SHP->bmHdr.w = ((struct BitMapHeader far *)ptr)->w;
			SHP->bmHdr.h = ((struct BitMapHeader far *)ptr)->h;
			SHP->bmHdr.x = ((struct BitMapHeader far *)ptr)->x;
			SHP->bmHdr.y = ((struct BitMapHeader far *)ptr)->y;
			SHP->bmHdr.d = ((struct BitMapHeader far *)ptr)->d;
			SHP->bmHdr.trans = ((struct BitMapHeader far *)ptr)->trans;
			SHP->bmHdr.comp = ((struct BitMapHeader far *)ptr)->comp;
			SHP->bmHdr.pad = ((struct BitMapHeader far *)ptr)->pad;
			SwapWord(&SHP->bmHdr.w);
			SwapWord(&SHP->bmHdr.h);
			SwapWord(&SHP->bmHdr.x);
			SwapWord(&SHP->bmHdr.y);
			ptr += ChunkLen;
		}
		else
		if (CHUNK("BODY"))
		{
			ptr += 4;
			size = *((long far *)ptr);
			ptr += 4;
			SwapLong((long far *)&size);
			SHP->BPR = (SHP->bmHdr.w+7) >> 3;
			MM_GetPtr(&SHP->Data,size);
			if (!SHP->Data)
				goto EXIT_FUNC;
			movedata(FP_SEG(ptr),FP_OFF(ptr),FP_SEG(SHP->Data),0,size);
			ptr += ChunkLen;

			break;
		}
		else
			ptr += ChunkLen+8;

		FileLen -= ChunkLen+8;
	}

	RT_CODE = 0;

EXIT_FUNC:;
	if (IFFfile)
	{
//		segptr = (memptr)FP_SEG(IFFfile);
		MM_FreePtr(&IFFfile);
	}

	return (RT_CODE);
}





//----------------------------------------------------------------------------
// LoadLIBFile() -- Copies a file from an existing archive to dos.
//
// PARAMETERS :
//
//			LibName 	- Name of lib file created with SoftLib V1.0
//
//			FileName - Name of file to load from lib file.
//
//			MemPtr   - (IF !NULL) - Pointer to memory to load into ..
//						  (IF NULL)  - Routine allocates necessary memory and
//											returns a MEM(SEG) pointer to memory allocated.
//
// RETURN :
//
//   		(IF !NULL) - A pointer to the loaded data.
//			(IF NULL)  - Error!
//
//----------------------------------------------------------------------------
memptr LoadLIBFile(char *LibName,char *FileName,memptr *MemPtr)
{
	int handle;
	unsigned long header;
	struct ChunkHeader Header;
	unsigned long ChunkLen;
	short x;
	struct FileEntryHdr FileEntry;     			// Storage for file once found
	struct FileEntryHdr FileEntryHeader;		// Header used durring searching
	struct SoftLibHdr LibraryHeader;				// Library header - Version Checking
	boolean FileFound = false;
	unsigned long id_slib = ID_SLIB;
	unsigned long id_chunk = ID_CHUNK;


	//
	// OPEN SOFTLIB FILE
	//

	if ((handle = open(LibName,O_RDONLY|O_BINARY, S_IREAD)) == -1)
		return(NULL);


	//
	//	VERIFY it is a SOFTLIB (SLIB) file
	//

	if (read(handle,&header,4) == -1)
	{
		close(handle);
		return(NULL);
	}

	if (header != id_slib)
	{
		close(handle);
		return(NULL);
	}


	//
	// CHECK LIBRARY HEADER VERSION NUMBER
	//

	if (read(handle, &LibraryHeader,sizeof(struct SoftLibHdr)) == -1)
		Quit("read error in LoadSLIBFile()\n");

	if (LibraryHeader.Version > SOFTLIB_VER)
		Quit("Unsupported file ver ");


	//
	// MANAGE FILE ENTRY HEADERS...
	//

	for (x = 1;x<=LibraryHeader.FileCount;x++)
	{
		if (read(handle, &FileEntryHeader,sizeof(struct FileEntryHdr)) == -1)
		{
			close(handle);
			return(NULL);
		}

		if (!stricmp(FileEntryHeader.FileName,FileName))
		{
			FileEntry = FileEntryHeader;
			FileFound = true;
		}
	}

	//
	// IF FILE HAS BEEN FOUND THEN SEEK TO POSITION AND EXTRACT
	//	ELSE RETURN WITH ERROR CODE...
	//

	if (FileFound)
	{
		if (lseek(handle,FileEntry.Offset,SEEK_CUR) == -1)
		{
			close(handle);
			return(NULL);
		}

		//
		// READ CHUNK HEADER - Verify we are at the beginning of a chunk..
		//

		if (read(handle,(char *)&Header,sizeof(struct ChunkHeader)) == -1)
			Quit("LIB File - Unable to read Header!");

		if (Header.HeaderID != id_chunk)
			Quit("LIB File - BAD HeaderID!");

		//
		// Allocate memory if Necessary...
		//


		if (!*MemPtr)
			MM_GetPtr(MemPtr,FileEntry.OrginalLength);

		//
		//	Calculate the length of the data (without the chunk header).
		//

		ChunkLen = FileEntry.ChunkLen - sizeof(struct ChunkHeader);


		//
		// Extract Data from file
		//

		switch (Header.Compression)
		{

			#if LZW_SUPPORT
			case ct_LZW:
				if (!InitBufferedIO(handle,&lzwBIO))
					Quit("No memory for buffered I/O.");
				lzwDecompress(&lzwBIO,MK_FP(*MemPtr,0),FileEntry.OrginalLength,(SRC_BFILE|DEST_MEM));
				FreeBufferedIO(&lzwBIO);
				break;
			#endif

			#if LZH_SUPPORT
			case ct_LZH:
				if (!InitBufferedIO(handle,&lzwBIO))
					Quit("No memory for buffered I/O.");
				lzhDecompress(&lzwBIO, MK_FP(*MemPtr,0), FileEntry.OrginalLength, ChunkLen, (SRC_BFILE|DEST_MEM));
				FreeBufferedIO(&lzwBIO);
				break;
			#endif

			case ct_NONE:
				if (!CA_FarRead(handle,MK_FP(*MemPtr,0),ChunkLen))
				{
//					close(handle);
					*MemPtr = NULL;
				}
				break;

			default:
				close(handle);
				Quit("Unknown Chunk.Compression Type!");
				break;
		}
	}
	else
		*MemPtr = NULL;

	close(handle);
	return(*MemPtr);
}




