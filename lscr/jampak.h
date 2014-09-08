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
//
//
//
//


#define LZW_N		4096
#define LZW_F		18


// LZW_THRESHOLD :encode string into position and length if match_length is
// greater than this

#define LZW_THRESHOLD				2

// index for root of binary search trees
//

#define LZW_NIL       			LZW_N


//
// FILE CHUNK IDs
//
// NOTE: The only reason for changing from COMP to CMP2 and having multi
//			comp header structs is for downward compatablity.
//

#define COMP					("COMP")		// Comp type is ct_LZW ALWAYS!
#define CMP2					("CMP2")		// Comp type is determined in header.


//
// 	COMPRESSION TYPES
//

#if 0
//
//  	FILE CHUNK HEADER FORMATS
//

struct COMPStruct
{
	unsigned long DecompLen;

};


struct CMP2Header
{
	unsigned CompType;
	unsigned long DecompLen;

};
#endif

//memptr segptr;
//extern BufferedIO lzwBIO;


//
//	  PARAMETER PASSING TYPES (POINTER TYPES)
//

#define SRC_FILE	 			(0x0001)				// C's non-buffered file i/o
#define SRC_FFILE				(0x0002)				// C's buffered ffile i/o
#define SRC_MEM				(0x0004)				// FAR memory Ptrs
#define SRC_BFILE				(0x0008)				// Buffered File I/O

#define SRC_TYPES 			(SRC_FILE | SRC_FFILE | SRC_MEM | SRC_BFILE)

#define DEST_FILE				(0x0100)				// C's non-buffered file i/o
#define DEST_FFILE			(0x0200)				// C's buffered ffile i/o
#define DEST_MEM				(0x0400)				// FAR memory Ptrs

#define DEST_TYPES 			(DEST_FILE | DEST_FFILE | DEST_MEM)


//---------------------------------------------------------------------------
//								FUNCTION PROTOTYPEING
//---------------------------------------------------------------------------

#if 0

//void DecompressFILEtoFILE(FILE *infile, FILE *outfile);
//unsigned long CompressFILEtoFILE(FILE *infile, FILE *outfile,unsigned long DataLength);


unsigned long lzwCompress(void far *infile, void far *outfile,unsigned long DataLength,unsigned PtrTypes);
void lzwDecompress(void far *infile, void far *outfile,unsigned long DataLength,unsigned PtrTypes);

int WritePtr(long outfile, unsigned char data, unsigned PtrType);
int ReadPtr(long infile, unsigned PtrType);

memptr InitBufferedIO(int handle, BufferedIO *bio);
void FreeBufferedIO(BufferedIO *bio);
byte bio_readch(BufferedIO *bio);

//unsigned long BLoad(char *SourceFile, memptr *DstPtr);
memptr LoadLIBFile(char *LibName,char *FileName,memptr *MemPtr);
int LoadLIBShape(char *SLIB_Filename, char *Filename,struct Shape *SHP);
long FileSize(char *filename)

#endif