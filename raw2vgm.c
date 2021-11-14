// raw2vgm.c - RAW -> VGM Converter
// Programmed by Valley Bell, written in 30 minutes (based on IMF2VGM)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stdtype.h"
#include "VGMFile.h"
#include "common.h"


static UINT8 OpenRAWFile(const char* FileName);
static void WriteVGMFile(const char* FileName);
static void ConvertRAW2VGM(void);


VGM_HEADER VGMHead;
UINT32 RAWDataLen;
UINT8* RAWData;
UINT32 RAWPos;
UINT32 RAWDataStart;
UINT32 RAWDataEnd;
UINT32 VGMDataLen;
UINT8* VGMData;
UINT32 VGMPos;
char FileBase[MAX_PATH];
UINT8 LoopOn;

int main(int argc, char* argv[])
{
	int argbase;
	int ErrVal;
	char FileName[MAX_PATH];

	printf("RAW to VGM Converter\n--------------------\n\n");

	ErrVal = 0;
	argbase = 1;
	LoopOn = 0x00;

	while(argbase < argc && argv[argbase][0] == '-')
	{
		if (! stricmp(argv[argbase], "-help"))
		{
			printf("Usage: raw2vgm [-Loop] Input.raw [Output.vgm]\n");
			printf("\n");
			printf("Loop: Makes the song loop from beginning to end.\n");
			return 0;
		}
		else if (! stricmp(argv[argbase], "-Loop"))
		{
			LoopOn = 0x01;
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
		snprintf(FileName, sizeof(FileName), "%s", argv[argbase + 0]);
		printf("%s\n", FileName);
	}
	if (! strlen(FileName))
		return 0;

	if (OpenRAWFile(FileName))
	{
		printf("Error opening the file!\n");
		ErrVal = 1;
		goto EndProgram;
	}
	printf("\n");

	ConvertRAW2VGM();

	if (argc > argbase + 1)
		snprintf(FileName, sizeof(FileName), "%s", argv[argbase + 1]);
	else
		*FileName='\0';
	if (FileName[0] == '\0')
	{
		snprintf(FileName, MAX_PATH, "%s.vgm", FileBase);
	}
	WriteVGMFile(FileName);

	free(RAWData);
	free(VGMData);

EndProgram:
	DblClickWait(argv[0]);

	return ErrVal;
}

static UINT8 OpenRAWFile(const char* FileName)
{
	FILE* hFile;
	//UINT16 TempSht;
	char* TempPnt;

	hFile = fopen(FileName, "rb");
	if (hFile == NULL)
		return 0xFF;

	fseek(hFile, 0x00, SEEK_END);
	RAWDataLen = ftell(hFile);

	// Read Data
	RAWData = (UINT8*)malloc(RAWDataLen);
	if (RAWData == NULL)
		goto OpenErr;
	fseek(hFile, 0x00, SEEK_SET);
	RAWDataLen = fread(RAWData, 0x01, RAWDataLen, hFile);

	fclose(hFile);

	snprintf(FileBase, sizeof(FileBase), "%s", FileName);
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

static void ConvertRAW2VGM(void)
{
	UINT16 RAWClock;
	UINT32 CurTick;
	UINT32 HalfRate;
	UINT64 TempTick;
	UINT32 VGMSmplL;
	UINT32 VGMSmplC;
	UINT32 SmplVal;
	UINT16 CurDelay;
	UINT8 CurReg;
	UINT8 CurData;
	UINT8 CurChip;

	VGMDataLen = sizeof(VGM_HEADER) + RAWDataLen * 0x02;
	VGMData = (UINT8*)malloc(VGMDataLen);

	memcpy(&RAWClock, &RAWData[0x08], 0x02);

	// Generate VGM Header
	memset(&VGMHead, 0x00, sizeof(VGM_HEADER));
	VGMHead.fccVGM = FCC_VGM;
	VGMHead.lngVersion = 0x00000151;
	VGMHead.lngRate = 0;
	VGMHead.lngDataOffset = 0x80;
	VGMHead.lngHzYM3812 = 3579545;

	// Convert data
	RAWPos = 0x0A;
	VGMPos = VGMHead.lngDataOffset;
	CurChip = 0x00;
	CurTick = 0;
	VGMSmplL = 0;
	// !! Note: The rates are actually 1193180 and 44100, but I strip
	//          the last 0 of them to get smaller mid-calculation values.
	//          It doesn't cause any precision loss anyway.
	HalfRate = 119318 / 2;	// for correct rounding
	while(RAWPos < RAWDataLen)
	{
		CurReg = RAWData[RAWPos + 0x01];
		CurData = RAWData[RAWPos + 0x00];
		RAWPos += 0x02;

		switch(CurReg)
		{
		case 0x00:	// Delay
			CurTick += CurData * RAWClock;
			break;
		case 0x02:	// Control Data
			switch(CurData)
			{
			case 0x00:	// clock change
				memcpy(&RAWClock, &RAWData[RAWPos], 0x02);
				RAWPos += 0x02;
				break;
			case 0x02:	// switch to low OPL chip (port 1)
				VGMHead.lngHzYM3812 |= 0x40000000;
				// fall through
			case 0x01:	// switch to low OPL chip (port 0)
				CurChip = CurData - 0x01;
				break;
			default:
				printf("Unknown Control Type 0x%02X found!\n", CurData);
				break;
			}
			break;
		default:
			if (VGMPos >= VGMDataLen - 0x08)
			{
				VGMDataLen += 0x8000;
				VGMData = (UINT8*)realloc(VGMData, VGMDataLen);
			}

			TempTick = (UINT64)CurTick * 4410 + HalfRate;
			VGMSmplC = (UINT32)(TempTick / 119318);
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

			VGMData[VGMPos + 0x00] = 0x5A + CurChip * 0x50;
			VGMData[VGMPos + 0x01] = CurReg;
			VGMData[VGMPos + 0x02] = CurData;
			VGMPos += 0x03;

			break;
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
