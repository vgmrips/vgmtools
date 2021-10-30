// vgm_facc.c - Round VGM to Frames (Make VGM Frame Accurate)
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "stdtype.h"
#include "stdbool.h"
#include "VGMFile.h"
#include "common.h"


static bool OpenVGMFile(const char* FileName);
static void WriteVGMFile(const char* FileName);
static void RoundVGMData(void);
static void WriteVGMHeader(UINT8* DstData, const UINT8* SrcData, const UINT32 EOFPos,
						   const UINT32 SampleCount, const UINT32 LoopPos,
						   const UINT32 LoopSmpls);
INLINE INT8 sign(double Value);
INLINE INT32 RoundU(double Value);
static void SetRoundError(UINT32 SrcVal, UINT32 RndVal, INT32* ErrMin, INT32* ErrMax,
						  INT32* PosMin, INT32* PosMax);


VGM_HEADER VGMHead;
UINT32 VGMDataLen;
UINT8* VGMData;
UINT32 VGMPos;
UINT32 VGMSmplPos;
UINT8* DstData;
UINT32 DstDataLen;
char FileBase[MAX_PATH];
float RoundA = 0.5;
UINT16 RoundTo;

int main(int argc, char* argv[])
{
	int ErrVal;
	int argbase;
	char FileName[MAX_PATH];

	printf("Make VGM Frame Accurate\n-----------------------\n\n");

	ErrVal = 0;
	argbase = 1;

	printf("File Name:\t");
	if (argc <= argbase + 0)
	{
		ReadFilename(FileName, sizeof(FileName));
	}
	else
	{
		strncpy(FileName, argv[argbase + 0], MAX_PATH-1);
		printf("%s\n", FileName);
	}
	if (! strlen(FileName))
		return 0;

	if (! OpenVGMFile(FileName))
	{
		printf("Error opening the file!\n");
		ErrVal = 1;
		goto EndProgram;
	}
	printf("\n");

	RoundVGMData();

	if (argc > argbase + 1)
		strncpy(FileName, argv[argbase + 1], MAX_PATH-1);
	else
		strcpy(FileName, "");
	if (FileName[0] == '\0')
	{
		snprintf(FileName, MAX_PATH, "%s_frame.vgm", FileBase);
	}
	WriteVGMFile(FileName);

	free(VGMData);
	free(DstData);

EndProgram:
	DblClickWait(argv[0]);

	return ErrVal;
}

static bool OpenVGMFile(const char* FileName)
{
	gzFile hFile;
	UINT32 CurPos;
	UINT32 TempLng;
	char* TempPnt;

	hFile = gzopen(FileName, "rb");
	if (hFile == NULL)
		return false;

	gzseek(hFile, 0x00, SEEK_SET);
	gzread(hFile, &TempLng, 0x04);
	if (TempLng != FCC_VGM)
		goto OpenErr;

	gzseek(hFile, 0x00, SEEK_SET);
	gzread(hFile, &VGMHead, sizeof(VGM_HEADER));
	ZLIB_SEEKBUG_CHECK(VGMHead);

	// Header preperations
	if (VGMHead.lngVersion < 0x00000150)
	{
		VGMHead.lngDataOffset = 0x00000000;
	}
	if (VGMHead.lngVersion < 0x00000151)
	{
		VGMHead.lngHzSPCM = 0x0000;
		VGMHead.lngSPCMIntf = 0x00000000;
		// all others are zeroed by memset
	}
	// relative -> absolute addresses
	VGMHead.lngEOFOffset += 0x00000004;
	if (VGMHead.lngGD3Offset)
		VGMHead.lngGD3Offset += 0x00000014;
	if (VGMHead.lngLoopOffset)
		VGMHead.lngLoopOffset += 0x0000001C;
	if (! VGMHead.lngDataOffset)
		VGMHead.lngDataOffset = 0x0000000C;
	VGMHead.lngDataOffset += 0x00000034;

	CurPos = VGMHead.lngDataOffset;
	if (VGMHead.lngVersion < 0x00000150)
		CurPos = 0x40;
	TempLng = sizeof(VGM_HEADER);
	if (TempLng > CurPos)
		TempLng -= CurPos;
	else
		TempLng = 0x00;
	memset((UINT8*)&VGMHead + CurPos, 0x00, TempLng);

	// Read Data
	VGMDataLen = VGMHead.lngEOFOffset;
	VGMData = (UINT8*)malloc(VGMDataLen);
	if (VGMData == NULL)
		goto OpenErr;
	gzseek(hFile, 0x00, SEEK_SET);
	gzread(hFile, VGMData, VGMDataLen);

	gzclose(hFile);

	strncpy(FileBase, FileName, MAX_PATH-1);
	TempPnt = strrchr(FileBase, '.');
	if (TempPnt != NULL)
		*TempPnt = 0x00;

	return true;

OpenErr:

	gzclose(hFile);
	return false;
}

static void WriteVGMFile(const char* FileName)
{
	FILE* hFile;
	const char* FileTitle;

	printf("\t\t\t\t\t\t\t\t\r");
	FileTitle = strrchr(FileName, '\\');
	if (FileTitle == NULL)
		FileTitle = FileName;
	else
		FileTitle ++;

	hFile = fopen(FileName, "wb");
	if (hFile == NULL)
	{
		printf("Error writing %s!\n", FileTitle);
		return;
	}

	fwrite(DstData, 0x01, DstDataLen, hFile);
	fclose(hFile);
	printf("%s written.\n", FileTitle);

	return;
}

static void RoundVGMData(void)
{
	UINT32 DstPos;
	UINT8 ChipID;
	UINT8 Command;
	UINT32 CmdDelay;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
#ifdef WIN32
	char TempStr[0x80];
	UINT32 CmdTimer;
	char MinSecStr[0x80];
#endif
	UINT32 CmdLen;
	bool StopVGM;
	bool IsDelay;
	UINT32 LoopPos;
	UINT32 SampleCount;
	UINT32 LoopSmpl;
	UINT32 VGMSmplRnd;	// Rounded Sample Pos
	UINT32 VGMSmplLast;
	UINT8 DelayCmd;
	UINT16 MaxDelay;
	INT32 RndErrMin;
	INT32 RndErrMax;
	INT32 RndErrMax1;
	INT32 RndPosMin;
	INT32 RndPosMax;
	INT32 RndPosMax1;

	DstData = (UINT8*)malloc(VGMDataLen);
	VGMPos = VGMHead.lngDataOffset;
	DstPos = VGMHead.lngDataOffset;

	switch(VGMHead.lngRate)
	{
	case 0:
		printf("VGM Frame Rate 0 - set to 60.\n");
		VGMHead.lngRate = 60;
		memcpy(&VGMData[0x24], &VGMHead.lngRate, 0x04);
	case 60:
		RoundTo = 735;
		DelayCmd = 0x62;
		break;
	case 50:
		RoundTo = 882;
		DelayCmd = 0x63;
		break;
	default:
		if (44100 % VGMHead.lngRate)
		{
			printf("Error! VGM Frame Rate can't be rounded to whole samples!\n");
			return;
		}
		printf("Warning! Unusual VGM Frame Rate: %u\n", VGMHead.lngRate);
		RoundTo = (UINT16)(44100 / VGMHead.lngRate);
		DelayCmd = 0x00;
		break;
	}
	MaxDelay = (UINT16)(0xFFFF / RoundTo * RoundTo);
	SampleCount = RoundU(VGMHead.lngTotalSamples);
	LoopSmpl = RoundU(VGMHead.lngTotalSamples - VGMHead.lngLoopSamples);

	RndErrMin = 0;
	RndErrMax = 0;
	RndErrMax1 = 0;
	RndPosMin = 0;
	RndPosMax = 0;
	RndPosMax1 = 0;

#ifdef WIN32
	CmdTimer = 0;
#endif
	VGMSmplPos = 0x00;
	VGMSmplRnd = 0x00;
	VGMSmplLast = 0x00;
	LoopPos = 0x00;
	StopVGM = false;
	while(VGMPos < VGMHead.lngEOFOffset)
	{
		CmdLen = 0x00;
		Command = VGMData[VGMPos + 0x00];
		IsDelay = false;

		CmdDelay = 0x00;
		if (Command >= 0x70 && Command <= 0x8F)
		{
			switch(Command & 0xF0)
			{
			case 0x70:
				TempSht = (Command & 0x0F) + 0x01;
				VGMSmplPos += TempSht;
				CmdDelay = TempSht;
				IsDelay = true;
				break;
			case 0x80:
				TempSht = Command & 0x0F;
				VGMSmplPos += TempSht;
				CmdDelay = TempSht;
				break;
			}
			CmdLen = 0x01;
		}
		else
		{
			// Cheat Mode (to use 2 instances of 1 chip)
			ChipID = 0x00;
			switch(Command)
			{
			case 0x30:
				if (VGMHead.lngHzPSG & 0x40000000)
				{
					Command += 0x20;
					ChipID = 0x01;
				}
				break;
			case 0xA1:
				if (VGMHead.lngHzYM2413 & 0x40000000)
				{
					Command -= 0x50;
					ChipID = 0x01;
				}
				break;
			case 0xA2:
			case 0xA3:
				if (VGMHead.lngHzYM2612 & 0x40000000)
				{
					Command -= 0x50;
					ChipID = 0x01;
				}
				break;
			case 0xA4:
				if (VGMHead.lngHzYM2151 & 0x40000000)
				{
					Command -= 0x50;
					ChipID = 0x01;
				}
				break;
			case 0xA5:
				if (VGMHead.lngHzYM2203 & 0x40000000)
				{
					Command -= 0x50;
					ChipID = 0x01;
				}
				break;
			case 0xA6:
			case 0xA7:
				if (VGMHead.lngHzYM2608 & 0x40000000)
				{
					Command -= 0x50;
					ChipID = 0x01;
				}
				break;
			case 0xA8:
			case 0xA9:
				if (VGMHead.lngHzYM2610 & 0x40000000)
				{
					Command -= 0x50;
					ChipID = 0x01;
				}
				break;
			case 0xAA:
				if (VGMHead.lngHzYM3812 & 0x40000000)
				{
					Command -= 0x50;
					ChipID = 0x01;
				}
				break;
			case 0xAB:
				if (VGMHead.lngHzYM3526 & 0x40000000)
				{
					Command -= 0x50;
					ChipID = 0x01;
				}
				break;
			case 0xAC:
				if (VGMHead.lngHzY8950 & 0x40000000)
				{
					Command -= 0x50;
					ChipID = 0x01;
				}
				break;
			}

			switch(Command)
			{
			case 0x66:	// End Of File
				CmdLen = 0x01;
				StopVGM = true;
				break;
			case 0x62:	// 1/60s delay
				TempSht = 735;
				VGMSmplPos += TempSht;
				CmdDelay = TempSht;
				CmdLen = 0x01;
				IsDelay = true;
				break;
			case 0x63:	// 1/50s delay
				TempSht = 882;
				VGMSmplPos += TempSht;
				CmdDelay = TempSht;
				CmdLen = 0x01;
				IsDelay = true;
				break;
			case 0x61:	// xx Sample Delay
				memcpy(&TempSht, &VGMData[VGMPos + 0x01], 0x02);
				VGMSmplPos += TempSht;
				CmdDelay = TempSht;
				CmdLen = 0x03;
				IsDelay = true;
				break;
			case 0x50:	// SN76496 write
				CmdLen = 0x02;
				break;
			case 0x51:	// YM2413 write
				CmdLen = 0x03;
				break;
			case 0x52:	// YM2612 write port 0
			case 0x53:	// YM2612 write port 1
				CmdLen = 0x03;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMData[VGMPos + 0x02];
				memcpy(&TempLng, &VGMData[VGMPos + 0x03], 0x04);
				TempLng &= 0x7FFFFFFF;
				CmdLen = 0x07 + TempLng;
				break;
			case 0xE0:	// Seek to PCM Data Bank Pos
				CmdLen = 0x05;
				break;
			case 0x4F:	// GG Stereo
				CmdLen = 0x02;
				break;
			case 0x54:	// YM2151 write
				CmdLen = 0x03;
				break;
			case 0xC0:	// Sega PCM memory write
				CmdLen = 0x04;
				break;
			case 0xB0:	// RF5C68 register write
				CmdLen = 0x03;
				break;
			case 0xC1:	// RF5C68 memory write
				CmdLen = 0x04;
				break;
			case 0x55:	// YM2203
				CmdLen = 0x03;
				break;
			case 0x56:	// YM2608 write port 0
			case 0x57:	// YM2608 write port 1
				CmdLen = 0x03;
				break;
			case 0x58:	// YM2610 write port 0
			case 0x59:	// YM2610 write port 1
				CmdLen = 0x03;
				break;
			case 0x5A:	// YM3812 write
				CmdLen = 0x03;
				break;
			case 0x5B:	// YM3526 write
				CmdLen = 0x03;
				break;
			case 0x5C:	// Y8950 write
				CmdLen = 0x03;
				break;
			case 0x5E:	// YMF262 write port 0
			case 0x5F:	// YMF262 write port 1
				CmdLen = 0x03;
				break;
			case 0x5D:	// YMZ280B write
				CmdLen = 0x03;
				break;
			case 0x68:	// PCM RAM write
				CmdLen = 0x0C;
				break;
			case 0x90:	// DAC Ctrl: Setup Chip
				CmdLen = 0x05;
				break;
			case 0x91:	// DAC Ctrl: Set Data
				CmdLen = 0x05;
				break;
			case 0x92:	// DAC Ctrl: Set Freq
				CmdLen = 0x06;
				break;
			case 0x93:	// DAC Ctrl: Play from Start Pos
				CmdLen = 0x0B;
				break;
			case 0x94:	// DAC Ctrl: Stop immediately
				CmdLen = 0x02;
				break;
			case 0x95:	// DAC Ctrl: Play Block (small)
				CmdLen = 0x05;
				break;
			default:
				switch(Command & 0xF0)
				{
				case 0x30:
				case 0x40:
					CmdLen = 0x02;
					break;
				case 0x50:
				case 0xA0:
				case 0xB0:
					CmdLen = 0x03;
					break;
				case 0xC0:
				case 0xD0:
					CmdLen = 0x04;
					break;
				case 0xE0:
				case 0xF0:
					CmdLen = 0x05;
					break;
				default:
					printf("Unknown Command: %X\n", Command);
					CmdLen = 0x01;
					//StopVGM = true;
					break;
				}
				break;
			}
		}

		if (VGMHead.lngLoopOffset && VGMPos == VGMHead.lngLoopOffset)
			LoopPos = DstPos;
		if (IsDelay)
		{
			VGMSmplRnd = RoundU(VGMSmplPos);
			if (CmdDelay < 0xFFFF)
			{
				if (VGMSmplRnd < RoundTo)
					SetRoundError(VGMSmplPos, VGMSmplRnd, &RndErrMin, &RndErrMax1,
									&RndPosMin, &RndPosMax1);
				else
					SetRoundError(VGMSmplPos, VGMSmplRnd, &RndErrMin, &RndErrMax,
									&RndPosMin, &RndPosMax);
			}
			while(VGMSmplLast < VGMSmplRnd)
			{
				TempLng = VGMSmplRnd - VGMSmplLast;
				if (DelayCmd && TempLng <= (UINT32)RoundTo * 2)
				{
					TempSht = 0x0000;
					while(TempLng)
					{
						DstData[DstPos + 0x00] = DelayCmd;
						DstPos ++;
						TempLng -= RoundTo;
						TempSht += RoundTo;
					}
				}
				else
				{
					if (TempLng <= MaxDelay)
						TempSht = (UINT16)TempLng;
					else
						TempSht = MaxDelay;

					DstData[DstPos + 0x00] = 0x61;
					memcpy(&DstData[DstPos + 0x01], &TempSht, 0x02);
					DstPos += 0x03;
				}

				VGMSmplLast += TempSht;
			}
		}
		else if (! IsDelay)
		{
			memcpy(&DstData[DstPos], &VGMData[VGMPos], CmdLen);
			DstPos += CmdLen;
		}
		VGMPos += CmdLen;
		if (StopVGM)
			break;

#ifdef WIN32
		if (CmdTimer < GetTickCount())
		{
			PrintMinSec(VGMSmplPos, MinSecStr);
			PrintMinSec(VGMHead.lngTotalSamples, TempStr);
			TempLng = VGMPos - VGMHead.lngDataOffset;
			CmdLen = VGMHead.lngEOFOffset - VGMHead.lngDataOffset;
			printf("%04.3f %% - %s / %s (%08X / %08X) ...\r", (float)TempLng / CmdLen * 100,
					MinSecStr, TempStr, VGMPos, VGMHead.lngEOFOffset);
			CmdTimer = GetTickCount() + 200;
		}
#endif
	}

	if (VGMHead.lngLoopOffset && ! LoopPos)
		printf("Warning! Failed to relocate Loop Point!\n");

	WriteVGMHeader(DstData, VGMData, DstPos, SampleCount,
					LoopPos, SampleCount - LoopSmpl);
	/*sprintf(TempStr, "%s_frame.vgm", FileBase);
	WriteVGMFile(TempStr);*/

	printf("Maximum Rounding Difference:\n");
	printf("\t%d at %06X\n\t%d at %06X\n\t%d at %06X (first frame only)\n",
			RndErrMin, RndPosMin, RndErrMax, RndPosMax, RndErrMax1, RndPosMax1);

	return;
}

static void WriteVGMHeader(UINT8* DstData, const UINT8* SrcData, const UINT32 EOFPos,
						   const UINT32 SampleCount, const UINT32 LoopPos,
						   const UINT32 LoopSmpls)
{
	UINT32 CurPos;
	UINT32 DstPos;
	UINT32 CmdLen;
	UINT32 TempLng;

	memcpy(DstData, VGMData, VGMHead.lngDataOffset);	// Copy Header
	memcpy(&DstData[0x18], &SampleCount, 0x04);
	TempLng = LoopPos;
	if (TempLng)
		TempLng -= 0x1C;
	memcpy(&DstData[0x1C], &TempLng, 0x04);
	memcpy(&DstData[0x20], &LoopSmpls, 0x04);

	DstPos = EOFPos;
	if (VGMHead.lngGD3Offset && VGMHead.lngGD3Offset + 0x0B < VGMHead.lngEOFOffset)
	{
		CurPos = VGMHead.lngGD3Offset;
		memcpy(&TempLng, &VGMData[CurPos + 0x00], 0x04);
		if (TempLng == FCC_GD3)
		{
			memcpy(&CmdLen, &VGMData[CurPos + 0x08], 0x04);
			CmdLen += 0x0C;

			TempLng = DstPos - 0x14;
			memcpy(&DstData[0x14], &TempLng, 0x04);
			memcpy(&DstData[DstPos], &VGMData[CurPos], CmdLen);	// Copy GD3 Tag
			DstPos += CmdLen;
		}
	}

	DstDataLen = DstPos;
	TempLng = DstDataLen - 0x04;
	memcpy(&DstData[0x04], &TempLng, 0x04);	// Write EOF Position

	return;
}

INLINE INT8 sign(double Value)
{
	if (Value > 0.0)
		return 1;
	else if (Value < 0.0)
		return -1;
	else
		return 0;
}

INLINE INT32 RoundU(double Value)
{
	return (INT32)(Value / RoundTo + RoundA * sign(Value)) * RoundTo;
}

static void SetRoundError(UINT32 SrcVal, UINT32 RndVal, INT32* ErrMin, INT32* ErrMax,
						  INT32* PosMin, INT32* PosMax)
{
	INT32 TempLng;

	TempLng = SrcVal - RndVal;
	if (TempLng < 0)
	{
		if (TempLng < *ErrMin)
		{
			*ErrMin = TempLng;
			*PosMin = VGMPos;
		}
	}
	else
	{
		if (TempLng > *ErrMax)
		{
			*ErrMax = TempLng;
			*PosMax = VGMPos;
		}
	}

	return;
}
