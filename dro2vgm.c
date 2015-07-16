// dro2vgm.c - DosBox RAW OPL -> VGM Converter
//

// TODO:	- option to disable the DROv1 Delay Hack
//			- detect Reg 0x105 = 1 with DualOPL2 and set to OPL3
//			  (not really neccessary, as my DosBOX patch alredy does that)

#include <stdio.h>
#include <stdlib.h>
#include "stdbool.h"
#include <string.h>

#ifdef WIN32
#include <conio.h>
#endif

#include "stdtype.h"
#include "VGMFile.h"


static bool OpenDROFile(const char* FileName);
static void WriteVGMFile(const char* FileName);
static void ConvertDRO2VGM(void);


typedef struct dro_file_header
{
	char cSignature[0x08];
	UINT16 iVersionMajor;
	UINT16 iVersionMinor;
} DRO_HEADER;
typedef struct dro_version_header_1
{
	UINT32 iLengthMS;
	UINT32 iLengthBytes;
	UINT32 iHardwareType;
} DRO_VER_HEADER_1;
typedef struct dro_version_header_2
{
	UINT32 iLengthPairs;
	UINT32 iLengthMS;
	UINT8 iHardwareType;
	UINT8 iFormat;
	UINT8 iCompression;
	UINT8 iShortDelayCode;
	UINT8 iLongDelayCode;
	UINT8 iCodemapLength;
} DRO_VER_HEADER_2;

#define FCC_DRO1	0x41524244	// 'DBRA'
#define FCC_DRO2	0x4C504F57	// 'ROPL'
								// -> "DBRAWOPL"

DRO_HEADER DROHead;
DRO_VER_HEADER_2 DROInf;
UINT8* DROCodemap;

VGM_HEADER VGMHead;
UINT32 DRODataLen;
UINT8* DROData;
UINT32 DROPos;
UINT32 DRODataStart;
UINT32 VGMDataLen;
UINT8* VGMData;
UINT32 VGMPos;
char FileBase[0x100];

int main(int argc, char* argv[])
{
	char FileName[0x100];
	
	printf("DRO to VGM Converter\n--------------------\n\n");
	
	printf("File Name:\t");
	if (argc <= 0x01)
	{
		gets(FileName);
	}
	else
	{
		strcpy(FileName, argv[0x01]);
		printf("%s\n", FileName);
	}
	if (! strlen(FileName))
		return 0;
	
	if (! OpenDROFile(FileName))
	{
		printf("Error opening the file!\n");
#ifdef WIN32
		_getch();
#endif
		return 1;
	}
	printf("\n");
	
	ConvertDRO2VGM();
	
	strcpy(FileName, FileBase);
	strcat(FileName, ".vgm");
	WriteVGMFile(FileName);
	
	free(DROData);
	free(VGMData);
	
#ifdef WIN32
	if (argv[0][1] == ':')
	{
		// Executed by Double-Clicking (or Drap and Drop)
		if (_kbhit())
			_getch();
		_getch();
	}
#endif
	
	return 0;
}

static bool OpenDROFile(const char* FileName)
{
	FILE* hFile;
	UINT32 fccHeader;
	UINT32 CurPos;
	UINT16 TempSht;
	UINT32 TempLng;
	char* TempPnt;
	DRO_VER_HEADER_1 DRO_V1;
	
	hFile = fopen(FileName, "rb");
	if (hFile == NULL)
		return false;
	
	fseek(hFile, 0x00, SEEK_SET);
	fread(&fccHeader, 0x01, 0x04, hFile);
	if (fccHeader != FCC_DRO1)
		goto OpenErr;
	fread(&fccHeader, 0x01, 0x04, hFile);
	if (fccHeader != FCC_DRO2)
		goto OpenErr;
	
	fseek(hFile, 0x00, SEEK_END);
	DRODataLen = ftell(hFile);
	
	// Read Data
	DROData = (UINT8*)malloc(DRODataLen);
	if (DROData == NULL)
		goto OpenErr;
	fseek(hFile, 0x00, SEEK_SET);
	DRODataLen = fread(DROData, 0x01, DRODataLen, hFile);
	
	CurPos = 0x00;
	memcpy(&DROHead, &DROData[0x00], sizeof(DRO_HEADER));
	CurPos += sizeof(DRO_HEADER);
	
	memcpy(&TempLng, &DROData[0x08], 0x04);
	if (TempLng & 0xFF00FF00)
	{
		// DosBox Version 0.61
		// this version didn't contain Version Bytes
		CurPos = 0x08;
		DROHead.iVersionMajor = 0x00;
		DROHead.iVersionMinor = 0x00;
	}
	else if (! (TempLng & 0x0000FFFF))
	{
		// DosBox Version 0.63
		// the order of the Version Bytes is swapped in this version
		memcpy(&TempSht, &DROData[0x0A], 0x02);
		if (TempSht == 0x01)
		{
			memcpy(&DROHead.iVersionMinor, &DROData[0x08], 0x02);
			DROHead.iVersionMajor = TempSht;
		}
	}
	
	switch(DROHead.iVersionMajor)
	{
	case 0:	// Version 0 (DosBox Version 0.61)
	case 1:	// Version 1 (DosBox Version 0.63)
		switch(DROHead.iVersionMajor)
		{
		case 0:	// Version 0
			memcpy(&DRO_V1, &DROData[CurPos + 0x00], 0x08);
			DRO_V1.iHardwareType = DROData[CurPos + 0x08];
			CurPos += 0x09;
			break;
		case 1:	// Version 1
			memcpy(&DRO_V1, &DROData[CurPos], sizeof(DRO_VER_HEADER_1));
			CurPos += sizeof(DRO_VER_HEADER_1);
			break;
		}
		
		DROInf.iLengthPairs = DRO_V1.iLengthBytes >> 1;
		DROInf.iLengthMS = DRO_V1.iLengthMS;
		switch(DRO_V1.iHardwareType)
		{
		case 0x01:	// OPL3
			DROInf.iHardwareType = 0x02;
			break;
		case 0x02:	// Dual OPL2
			DROInf.iHardwareType = 0x01;
			break;
		default:
			DROInf.iHardwareType = (UINT8)DRO_V1.iHardwareType;
			break;
		}
		DROInf.iFormat = 0x00;
		DROInf.iCompression = 0x00;
		DROInf.iShortDelayCode = 0x00;
		DROInf.iLongDelayCode = 0x01;
		DROInf.iCodemapLength = 0x00;
		
		break;
	case 2:	// Version 2 (DosBox Version 0.73)
		// sizeof(DRO_VER_HEADER_2) returns 0x10, but the exact size is 0x0E
		memcpy(&DROInf, &DROData[CurPos], 0x0E);
		CurPos += 0x0E;
		
		break;
	default:
		goto OpenErr;
	}
	
	if (DROInf.iCodemapLength)
	{
		DROCodemap = (UINT8*)malloc(DROInf.iCodemapLength * sizeof(UINT8));
		memcpy(DROCodemap, &DROData[CurPos], DROInf.iCodemapLength);
		CurPos += DROInf.iCodemapLength;
	}
	else
	{
		DROCodemap = NULL;
	}
	DRODataStart = CurPos;
	CurPos += DROInf.iLengthPairs << 1;
	if (CurPos < DRODataLen)
		DRODataLen = CurPos;
	
	// Generate VGM Header
	memset(&VGMHead, 0x00, sizeof(VGM_HEADER));
	VGMHead.fccVGM = FCC_VGM;
	VGMHead.lngVersion = 0x00000151;
	VGMHead.lngTotalSamples = (UINT32)(DROInf.iLengthMS * 44.1f + 0.5f);
	VGMHead.lngRate = 1000;
	VGMHead.lngDataOffset = 0x80;
	switch(DROInf.iHardwareType)
	{
	case 0x00:	// Single OPL2 Chip
		VGMHead.lngHzYM3812 = 3579545;
		break;
	case 0x01:	// Dual OPL2 Chip
		VGMHead.lngHzYM3812 = 3579545 | 0xC0000000;
		break;
	case 0x02:	// Single OPL3 Chip
		VGMHead.lngHzYMF262 = 14318180;
		break;
	default:
		VGMHead.lngHzYM3812 = 3579545 | 0x40000000;
		break;
	}
	
	fclose(hFile);
	
	strcpy(FileBase, FileName);
	TempPnt = strrchr(FileBase, '.');
	if (TempPnt != NULL)
		*TempPnt = 0x00;
	
	return true;

OpenErr:

	fclose(hFile);
	return false;
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

static void ConvertDRO2VGM(void)
{
	UINT16 TempSht;
	UINT8 CurChip;
	UINT8 ChipCmd;
	UINT8 CurCmd;
	UINT8 CurVal;
	UINT32 CurMS;
	UINT32 VGMSmplL;
	UINT32 VGMSmplC;
	UINT32 SmplVal;
	
	VGMDataLen = DRODataLen * 0x04; // this should be 200% safe
	VGMData = (UINT8*)malloc(VGMDataLen);
	
	printf("DRO File Version: %u.%02u\n", DROHead.iVersionMajor, DROHead.iVersionMinor);
	DROPos = DRODataStart;
	VGMPos = VGMHead.lngDataOffset;
	VGMDataLen = DROPos + VGMDataLen;
	CurMS = 0x00;
	VGMSmplL = 0x00;
	VGMSmplC = 0x00;
	
	CurChip = 0x00;
	while(DROPos < DRODataLen)
	{
		CurCmd = DROData[DROPos + 0x00];
		
		if (DROHead.iVersionMajor <= 0x01)
		{
			if (CurCmd <= 0x04 && DROPos < 0x20)
				CurCmd = 0x04;	// Fix Dro-Initialisation Bug
			if (CurCmd == 0x01)
			{
				// fix these quite common bugs of v1-files
				if (! (DROData[DROPos + 0x01] & ~0x20))
				{
					if (DROData[DROPos + 0x02] && /*DROData[DROPos + 0x02] <= 0x04 ||*/
						DROData[DROPos + 0x02] >= 0x20)
						CurCmd = 0x04;
				}
				if (DROData[DROPos + 0x02] == 0xBD)
					CurCmd = 0x04;
			}
		}
		if (CurCmd == DROInf.iShortDelayCode || CurCmd == DROInf.iLongDelayCode)
		{
			switch(DROHead.iVersionMajor)
			{
			case 0x00:
			case 0x01:
				if (CurCmd == DROInf.iShortDelayCode)	// == 0x00
				{
					CurMS += 0x01 + DROData[DROPos + 0x01];
					DROPos += 0x02;
				}
				else if (CurCmd == DROInf.iLongDelayCode)	// == 0x01
				{
					memcpy(&TempSht, &DROData[DROPos + 0x01], 0x02);
					CurMS += 0x01 + TempSht;
					DROPos += 0x03;
				}
				break;
			case 0x02:
				TempSht = 0x01 + DROData[DROPos + 0x01];
				if (CurCmd == DROInf.iShortDelayCode)
					CurMS += TempSht;
				else if (CurCmd == DROInf.iLongDelayCode)
					CurMS += TempSht << 8;
				
				DROPos += 0x02;
				break;
			}
			
			VGMSmplC = (UINT32)(CurMS * 44.1f + 0.5f);
		}
		else if (DROHead.iVersionMajor <= 0x01 && (CurCmd == 0x02 || CurCmd == 0x03))
		{
			CurChip = CurCmd & 0x01;
			DROPos += 0x01;
		}
		else
		{
			if (VGMSmplL < VGMSmplC)
			{
				SmplVal = VGMSmplC - VGMSmplL;
				if (VGMSmplC < VGMSmplL)
					*((char*)NULL) = 0x00;
				while(SmplVal)
				{
					if (SmplVal <= 0xFFFF)
						TempSht = (UINT16)SmplVal;
					else
						TempSht = 0xFFFF;
					
					VGMData[VGMPos + 0x00] = 0x61;
					memcpy(&VGMData[VGMPos + 0x01], &TempSht, 0x02);
					VGMPos += 0x03;
					SmplVal -= TempSht;
				}
				VGMSmplL = VGMSmplC;
			}
			
			switch(DROHead.iVersionMajor)
			{
			case 0x00:
			case 0x01:
				if (CurCmd == 0x04)
				{
					if (CurCmd == DROData[DROPos + 0x00] && DROPos >= 0x20)
						DROPos += 0x01;
					CurCmd = DROData[DROPos + 0x00];
				}
				break;
			case 0x02:
				CurChip = (CurCmd & 0x80) >> 7;
				CurCmd = DROCodemap[CurCmd & 0x7F];
				break;
			}
			CurVal = DROData[DROPos + 0x01];
			DROPos += 0x02;
			
			switch(DROInf.iHardwareType)
			{
			case 0x00:	// OPL2
				ChipCmd = 0x5A;
				break;
			case 0x01:	// Dual OPL2
				ChipCmd = 0x5A + CurChip * 0x50;
				break;
			case 0x02:	// OPL3
				ChipCmd = 0x5E | CurChip;
				break;
			}
			VGMData[VGMPos + 0x00] = ChipCmd;
			VGMData[VGMPos + 0x01] = CurCmd;
			VGMData[VGMPos + 0x02] = CurVal;
			VGMPos += 0x03;
		}
	}
	VGMData[VGMPos + 0x00] = 0x66;
	VGMPos += 0x01;
	
	if (VGMSmplL != VGMHead.lngTotalSamples)
	{
		printf("Warning! There was an error during delay calculations!\n");
		printf("DRO ms Header: %u, counted: %u\n", DROInf.iLengthMS, CurMS);
		printf("Please relog the file with DosBox 0.73 or higher.\n");
	}
	
	VGMDataLen = VGMPos;
	VGMHead.lngEOFOffset = VGMDataLen;
	
	SmplVal = VGMHead.lngDataOffset;
	if (SmplVal > sizeof(VGM_HEADER))
		SmplVal = sizeof(VGM_HEADER);
	VGMHead.lngDataOffset -= 0x34;
	VGMHead.lngEOFOffset -= 0x04;
	memcpy(&VGMData[0x00], &VGMHead, SmplVal);
	
	return;
}
