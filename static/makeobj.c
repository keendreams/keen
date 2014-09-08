/*
** makeobj.c
**
**---------------------------------------------------------------------------
** Copyright 2014 Braden Obrzut
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
** This is a throwaway program to create OMF object files for DOS. It also
** extracts the object files.  It should be compatible with MakeOBJ by John
** Romero except where we calculate the checksum correctly.
**
*/

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#pragma pack(1)
typedef struct
{
	unsigned char type;
	unsigned short len;
} SegHeader;

typedef struct
{
	unsigned short len;
	unsigned char name;
	unsigned char classname;
	unsigned char overlayname;
} SegDef;
#pragma pack()

const char* ReadFile(const char* fn, int *size)
{
	char* out;

	FILE* f = fopen(fn, "rb");
	fseek(f, 0, SEEK_END);
	*size = ftell(f);
	out = (char*)malloc(*size);
	fseek(f, 0, SEEK_SET);

	fread(out, *size, 1, f);

	fclose(f);

	return out;
}

void WriteFile(const char* fn, const char *data, int size)
{
	FILE* f = fopen(fn, "wb");
	fwrite(data, size, 1, f);
	fclose(f);
}

void Extract(const char* infn)
{
	const char* in;
	const char* start;
	const char* p;
	char outfn[16];
	char str[256];
	char *outdata;
	int outsize;
	int insize;
	SegHeader head;

	outdata = NULL;

	start = in = ReadFile(infn, &insize);

	while(in < start + insize)
	{
		head = *(SegHeader*)in;

		switch(head.type)
		{
			case 0x80: /* THEADR */
				memcpy(outfn, in+4, in[3]);
				outfn[in[3]] = 0;
				printf("Output: %s\n", outfn);
				{
					int i;
					for(i = 0;i < 16;++i)
					{
						if(outfn[i] == ' ')
							outfn[i] = 0;
					}
				}
				break;
			case 0x88: /* COMENT */
				switch(in[3])
				{
					case 0:
						memcpy(str, in+5, head.len-2);
						str[head.len-3] = 0;
						printf("Comment: %s\n", str);
						break;
					default:
						printf("Unknown comment type %X @ %x ignored.\n", (unsigned char)in[3], (unsigned int)(in - start));
						break;
				}
				break;
			case 0x96: /* LNAMES */
				p = in+3;
				while(p < in+head.len+2)
				{
					memcpy(str, p+1, (unsigned char)*p);
					str[(unsigned char)*p] = 0;
					printf("Name: %s\n", str);

					p += (unsigned char)*p+1;
				}
				break;
			case 0x98: /* SEGDEF */
			{
				SegDef *sd;

				sd = *(in+3) ? (SegDef*)(in+4) : (SegDef*)(in+7);
				printf("Segment Length: %d\n", sd->len);

				outdata = (char*)malloc(sd->len);
				outsize = sd->len;
				break;
			}
			case 0x90: /* PUBDEF */
				p = in+5;
				if(in[5] == 0)
					p += 2;
				while(p < in+head.len+2)
				{
					memcpy(str, p+1, (unsigned char)*p);
					str[(unsigned char)*p] = 0;
					printf("Public Name: %s\n", str);

					p += (unsigned char)*p+4;
				}
				break;
			case 0xA0: /* LEDATA */
				printf("Writing data at %d (%d)\n", *(unsigned short*)(in+4), head.len-4);
				memcpy(outdata+*(unsigned short*)(in+4), in+6, head.len-4);
				break;
			case 0x8A: /* MODEND */
				/* Ignore */
				break;
			default:
				printf("Unknown header type %X @ %x ignored.\n", head.type, (unsigned int)(in - start));
				break;
		}

		in += 3 + head.len;
	}

	WriteFile(outfn, outdata, outsize);

	free((char*)start);
	free(outdata);
}

void CheckSum(char *s, unsigned short len)
{
	int sum;

	len += 3;

	sum = 0;
	while(len > 1)
	{
		sum += *(unsigned char*)s;
		++s;
		--len;
	}
	*s = (unsigned char)(0x100-(sum&0xFF));
}

void MakeDataObj(const char* infn, const char* outfn, const char* segname, const char* symname, int altmode)
{
#define Flush() fwrite(d.buf, d.head.len+3, 1, f)
	union
	{
		char buf[4096];
		SegHeader head;
	} d;
	int i;
	FILE *f;
	int insize;
	const char *in;
	const char *infn_stripped = strrchr(infn, '/');
	if(strrchr(infn, '\\') > infn_stripped)
		infn_stripped = strrchr(infn, '\\');
	if(infn_stripped == NULL)
		infn_stripped = infn;
	else
		++infn_stripped;

	f = fopen(outfn, "wb");

	in = ReadFile(infn, &insize);

	d.head.type = 0x80;
	d.head.len = 14;
	d.buf[3] = 12;
	if(d.buf[3] > 12)
		d.buf[3] = 12;
	sprintf(&d.buf[4], "%-12s", infn_stripped);
	for(i = 0;i < strlen(infn_stripped) && i < 12;++i)
		d.buf[4+i] = toupper(d.buf[4+i]);
	/* CheckSum(d.buf, d.head.len); */
	d.buf[17] = 0; /* For some reason this one isn't checksummed by MakeOBJ */
	Flush();

	d.head.type = 0x88;
	d.head.len = 15;
	d.buf[3] = 0;
	d.buf[4] = 0;
	/* We're not really MakeOBJ v1.1, but to allow us to verify with md5sums */
	memcpy(&d.buf[5], "MakeOBJ v1.1", 12);
	CheckSum(d.buf, d.head.len);
	Flush();

	d.head.type = 0x96;
	d.head.len = strlen(infn_stripped)+40;
	d.buf[3] = 6;
	memcpy(&d.buf[4], "DGROUP", 6);
	d.buf[10] = 5;
	memcpy(&d.buf[11], "_DATA", 5);
	d.buf[16] = 4;
	memcpy(&d.buf[17], "DATA", 4);
	d.buf[21] = 0;
	d.buf[22] = 5;
	memcpy(&d.buf[23], "_TEXT", 5);
	d.buf[28] = 4;
	memcpy(&d.buf[29], "CODE", 4);
	d.buf[33] = 8;
	memcpy(&d.buf[34], "FAR_DATA", 8);
	if(!segname)
	{
		if(!altmode)
		{
			d.buf[42] = strlen(infn_stripped)-1;
			for(i = 0;i < strlen(infn_stripped)-4;++i)
			{
				if(i == 0)
					d.buf[43] = toupper(infn_stripped[0]);
				else
					d.buf[43+i] = tolower(infn_stripped[i]);
			}
			memcpy(&d.buf[43+i], "Seg", 3);
		}
		else
		{
			d.head.len = 40;
		}
	}
	else
	{
		d.head.len = strlen(segname)+41;
		d.buf[42] = strlen(segname);
		strcpy(&d.buf[43], segname);
	}
	CheckSum(d.buf, d.head.len);
	Flush();

	d.head.type = 0x98;
	d.head.len = 7;
	*(unsigned short*)(d.buf+4) = insize;
	if(altmode == 0)
	{
		d.buf[3] = (char)((unsigned char)0x60);
		d.buf[6] = 8;
		d.buf[7] = 7;
		d.buf[8] = 4;
	}
	else
	{
		d.buf[3] = (char)((unsigned char)0x48);
		d.buf[6] = 2;
		d.buf[7] = 3;
		d.buf[8] = 4;
	}
	CheckSum(d.buf, d.head.len);
	Flush();

	if(altmode)
	{
		d.head.type = 0x9A;
		d.head.len = 4;
		d.buf[3] = 1;
		d.buf[4] = (char)((unsigned char)0xFF);
		d.buf[5] = 1;
		CheckSum(d.buf, d.head.len);
		Flush();
	}

	d.head.type = 0x90;
	d.head.len = strlen(infn_stripped)+4;
	d.buf[3] = 1;
	d.buf[4] = 1;
	if(!symname)
	{
		d.buf[5] = strlen(infn_stripped)-3;
		d.buf[6] = '_';
		for(i = 0;i < strlen(infn_stripped)-4;++i)
			d.buf[7+i] = tolower(infn_stripped[i]);
	}
	else
	{
		d.head.len = strlen(symname)+7;
		d.buf[5] = strlen(symname);
		strcpy(&d.buf[6], symname);
		i = strlen(symname)-1;
	}
	d.buf[7+i] = 0;
	d.buf[8+i] = 0;
	d.buf[9+i] = 0;
	/* This checksum is calculated wrong in MakeOBJ, although I don't know in what way. */
	CheckSum(d.buf, d.head.len);
	Flush();

#define LEDATA_LEN 1024
	for(i = 0;i < insize;i += LEDATA_LEN)
	{
		d.head.type = 0xA0;
		d.head.len = insize - i > LEDATA_LEN ? LEDATA_LEN+4 : insize - i + 4;
		d.buf[3] = 1;
		*(unsigned short*)(d.buf+4) = i;
		memcpy(&d.buf[6], &in[i], d.head.len-4);
		CheckSum(d.buf, d.head.len);
		Flush();
	}

	d.head.type = 0x8A;
	d.head.len = 2;
	d.buf[3] = 0;
	d.buf[4] = 0;
	CheckSum(d.buf, d.head.len);
	Flush();

	fclose(f);
	free((char*)in);
}

void DumpData(const char* infn, const char* outfn, int skip)
{
	FILE *f;
	int i;
	int insize;
	char symname[9];
	const char *in;
	const char *infn_stripped = strrchr(infn, '/');
	if(strrchr(infn, '\\') > infn_stripped)
		infn_stripped = strrchr(infn, '\\');
	if(infn_stripped == NULL)
		infn_stripped = infn;
	else
		++infn_stripped;

	f = fopen(outfn, "wb");

	memset(symname, 0, 9);
	memcpy(symname, infn_stripped, strlen(infn_stripped)-4);
	fprintf(f, "char far %s[] ={\r\n", symname);

	in = ReadFile(infn, &insize);

	for(i = skip;i < insize;++i)
	{
		fprintf(f, "%d", (unsigned char)in[i]);
		if(i != insize-1)
			fprintf(f, ",\r\n");
	}
	fprintf(f, " };\r\n");

	fclose(f);
	free((char*)in);
}

int main(int argc, char* argv[])
{
	if(argc < 3)
	{
		printf("Converts file to OMF.\nUseage:\n  ./makeobj [fx] <input> ...\n");
		return 0;
	}

	switch(argv[1][0])
	{
		case 'c':
			if(argc < 4)
			{
				printf("Need an output location. (Extra parms: <output> [<symbol>])\n");
				return 0;
			}
			else
			{
				const char *symname = NULL;
				if(argc >= 5)
					symname = argv[4];
				MakeDataObj(argv[2], argv[3], NULL, symname, 1);
			}
			break;
		default:
		case 'f':
			if(argc < 4)
			{
				printf("Need an output location. (Extra parms: <output> [<segname> <symbol>])\n");
				return 0;
			}
			else
			{
				const char *segname = NULL, *symname = NULL;
				if(argc >= 6)
				{
					segname = argv[4];
					symname = argv[5];
				}
				MakeDataObj(argv[2], argv[3], segname, symname, 0);
			}
			break;
		case 'x':
			Extract(argv[2]);
			break;
		case 's':
			if(argc < 4)
			{
				printf("Need an output location. (Extra parms: <output> [<skip>])\n");
				return 0;
			}
			else
			{
				int skip = 0;
				if(argc >= 5)
				{
					skip = atoi(argv[4]);
				}
				DumpData(argv[2], argv[3], skip);
			}
			break;
			break;
	}
	return 0;
}
