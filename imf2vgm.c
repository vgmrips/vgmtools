// imf2vgm.c - IMF -> VGM Converter
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stdtype.h"
#include "VGMFile.h"
#include "common.h"


static UINT8 OpenIMFFile(const char* FileName);
static void WriteVGMFile(const char* FileName);
static void ConvertIMF2VGM(void);


UINT8 IMFType;
UINT16 IMFRate;
VGM_HEADER VGMHead;
UINT32 IMFDataLen;
UINT8* IMFData;
UINT32 IMFPos;
UINT32 IMFDataStart;
UINT32 IMFDataEnd;
UINT32 VGMDataLen;
UINT8* VGMData;
UINT32 VGMPos;
char FileBase[0x100];
UINT8 LoopOn;

int main(int argc, char* argv[])
{
	int argbase;
	int ErrVal;
	char FileName[0x100];
	UINT16 ForceHz;
	UINT8 ForceType;
	
	printf("IMF to VGM Converter\n--------------------\n\n");
	
	ErrVal = 0;
	argbase = 1;
	ForceHz = 0;
	ForceType = 0xFF;
	LoopOn = 0x00;
	
	while(argbase < argc && argv[argbase][0] == '-')
	{
		if (! stricmp(argv[argbase], "-help"))
		{
			printf("Usage: imf2vgm [-Loop] [-Hz###] [-Type#] Input.imf [Output.vgm]\n");
			printf("\n");
			printf("Loop: Makes the song loop from beginning to end.\n");
			printf("Hz:   There are 3 known speeds: 280, 560 and 700\n");
			printf("      560 is default for .imf, 700 for .wlf files\n");
			printf("Type: Can be 0 (no header) or 1 (header with 2-byte file size)\n");
			return 0;
		}
		else if (! stricmp(argv[argbase], "-Loop"))
		{
			LoopOn = 0x01;
			argbase ++;
		}
		else if (! strnicmp(argv[argbase], "-Hz", 3))
		{
			ForceHz = (UINT16)strtoul(argv[argbase] + 3, NULL, 0);
			argbase ++;
		}
		else if (! strnicmp(argv[argbase], "-Type", 5))
		{
			ForceType = (UINT8)strtoul(argv[argbase] + 5, NULL, 0);
			if (ForceType >= 0x02)
			{
				printf("Error: Type must be either 0 or 1!\n");
				return 2;
			}
			argbase ++;
		}
		else
		{
			break;
		}
	}
	
	printf("File Name:\t");
	if (argc <= argbase + 0)
	{
		ReadFilename(FileName, sizeof(FileName));
	}
	else
	{
		strcpy(FileName, argv[argbase + 0]);
		printf("%s\n", FileName);
	}
	if (! strlen(FileName))
		return 0;
	
	if (OpenIMFFile(FileName))
	{
		printf("Error opening the file!\n");
		ErrVal = 1;
		goto EndProgram;
	}
	printf("\n");
	
	if (ForceHz)
		IMFRate = ForceHz;
	if (ForceType < 0xFF)
		IMFType = ForceType;
	ConvertIMF2VGM();
	
	if (argc > argbase + 1)
		strcpy(FileName, argv[argbase + 1]);
	else
		strcpy(FileName, "");
	if (FileName[0] == '\0')
	{
		strcpy(FileName, FileBase);
		strcat(FileName, ".vgm");
	}
	WriteVGMFile(FileName);
	
	free(IMFData);
	free(VGMData);
	
EndProgram:
	DblClickWait(argv[0]);
	
	return ErrVal;
}

static UINT8 OpenIMFFile(const char* FileName)
{
	FILE* hFile;
	UINT16 TempSht;
	char* TempPnt;
	
	hFile = fopen(FileName, "rb");
	if (hFile == NULL)
		return 0xFF;
	
	fseek(hFile, 0x00, SEEK_END);
	IMFDataLen = ftell(hFile);
	
	// Read Data
	IMFData = (UINT8*)malloc(IMFDataLen);
	if (IMFData == NULL)
		goto OpenErr;
	fseek(hFile, 0x00, SEEK_SET);
	IMFDataLen = fread(IMFData, 0x01, IMFDataLen, hFile);
	
	fclose(hFile);
	
	memcpy(&TempSht, &IMFData[0x00], 0x02);
	if (! TempSht)
		IMFType = 0x00;
	else
		IMFType = 0x01;
	
	strcpy(FileBase, FileName);
	TempPnt = strrchr(FileBase, '.');
	if (TempPnt != NULL)
	{
		*TempPnt = 0x00;
		TempPnt ++;
	}
	else
	{
		TempPnt = FileBase + strlen(FileBase);
	}
	if (! stricmp(TempPnt, "wlf"))
		IMFRate = 700;
	else
		IMFRate = 560;
	
	return 0x00;

OpenErr:

	fclose(hFile);
	return 0x80;
}

static void WriteVGMFile(const char* FileName)
{
	FILE* hFile;
	
	hFile = fopen(FileName, "wb");
	fwrite(VGMData, 0x01, VGMDataLen, hFile);
	fclose(hFile);
	
	printf("File written.\n");
	
	return;
}

static void ConvertIMF2VGM(void)
{
	UINT16 CurDelay;
	UINT32 CurTick;
	UINT16 HalfRate;
	UINT64 TempTick;
	UINT32 VGMSmplL;
	UINT32 VGMSmplC;
	UINT32 SmplVal;
	
	VGMDataLen = sizeof(VGM_HEADER) + IMFDataLen * 0x02;
	VGMData = (UINT8*)malloc(VGMDataLen);
	
	printf("IMF Type: %u, IMF Playback Rate: %u Hz\n", IMFType, IMFRate);
	
	memcpy(&CurDelay, &IMFData[0x00], 0x02);
	if (IMFType == 0x00)
	{
		IMFDataStart = 0x0000;
		IMFDataEnd = IMFDataLen;
	}
	else //if (IMFType == 0x01)
	{
		IMFDataStart = 0x0002;
		IMFDataEnd = IMFDataStart + CurDelay;
	}
	
	// Generate VGM Header
	memset(&VGMHead, 0x00, sizeof(VGM_HEADER));
	VGMHead.fccVGM = FCC_VGM;
	VGMHead.lngVersion = 0x00000151;
	VGMHead.lngRate = IMFRate;
	VGMHead.lngDataOffset = 0x80;
	VGMHead.lngHzYM3812 = 3579545;
	
	// Convert data
	IMFPos = IMFDataStart;
	VGMPos = VGMHead.lngDataOffset;
	CurTick = 0;
	VGMSmplL = 0;
	HalfRate = IMFRate / 2;	// for correct rounding
	while(IMFPos < IMFDataEnd)
	{
		if (VGMPos >= VGMDataLen - 0x08)
		{
			VGMDataLen += 0x8000;
			VGMData = (UINT8*)realloc(VGMData, VGMDataLen);
		}
		VGMData[VGMPos + 0x00] = 0x5A;
		VGMData[VGMPos + 0x01] = IMFData[IMFPos + 0x00];	// register
		VGMData[VGMPos + 0x02] = IMFData[IMFPos + 0x01];	// data
		VGMPos += 0x03;
		
		memcpy(&CurDelay, &IMFData[IMFPos + 0x02], 0x02);
		CurTick += CurDelay;
		TempTick = (UINT64)CurTick * 44100 + HalfRate;
		VGMSmplC = (UINT32)(TempTick / IMFRate);
		IMFPos += 0x04;
		
		if (VGMSmplL < VGMSmplC)
		{
			SmplVal = VGMSmplC - VGMSmplL;
			while(SmplVal)
			{
				if (SmplVal <= 0xFFFF)
					CurDelay = (UINT16)SmplVal;
				else
					CurDelay = 0xFFFF;
				
				if (VGMPos >= VGMDataLen - 0x08)
				{
					VGMDataLen += 0x8000;
					VGMData = (UINT8*)realloc(VGMData, VGMDataLen);
				}
				VGMData[VGMPos + 0x00] = 0x61;
				memcpy(&VGMData[VGMPos + 0x01], &CurDelay, 0x02);
				VGMPos += 0x03;
				SmplVal -= CurDelay;
			}
			VGMSmplL = VGMSmplC;
		}
	}
	VGMData[VGMPos] = 0x66;
	VGMPos += 0x01;
	
	VGMDataLen = VGMPos;
	VGMHead.lngEOFOffset = VGMDataLen;
	VGMHead.lngTotalSamples = VGMSmplL;
	if (LoopOn)
	{
		VGMHead.lngLoopOffset = VGMHead.lngDataOffset;
		VGMHead.lngLoopSamples = VGMHead.lngTotalSamples;
	}
	
	SmplVal = VGMHead.lngDataOffset;
	if (SmplVal > sizeof(VGM_HEADER))
		SmplVal = sizeof(VGM_HEADER);
	VGMHead.lngEOFOffset -= 0x04;
	if (VGMHead.lngLoopOffset)
		VGMHead.lngLoopOffset -= 0x1C;
	VGMHead.lngDataOffset -= 0x34;
	memcpy(&VGMData[0x00], &VGMHead, SmplVal);
	
	return;
}
