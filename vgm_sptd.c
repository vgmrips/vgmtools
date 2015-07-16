// vgm_sptd.c - VGM Splitter (Delay Edition)
//

#include <stdio.h>
#include <stdlib.h>
#include "stdbool.h"
#include <string.h>

#ifdef WIN32
#include <conio.h>
#include <windows.h>	// for GetTickCount
#endif

#include "zlib.h"

#include "stdtype.h"
#include "VGMFile.h"



static bool OpenVGMFile(const char* FileName);
static void WriteVGMFile(const char* FileName);
static void SplitVGMData(const UINT32 SplitDelay);
#ifdef WIN32
static void PrintMinSec(const UINT32 SamplePos, char* TempStr);
#endif

// Function Prototype from vgm_trml.c
void SetTrimOptions(UINT8 TrimMode, UINT8 WarnMask);
void TrimVGMData(const INT32 StartSmpl, const INT32 LoopSmpl, const INT32 EndSmpl,
				 const bool HasLoop, const bool KeepESmpl);


UINT8 DIGIT_COUNT;
bool HEXION_SPLIT;


VGM_HEADER VGMHead;
UINT32 VGMDataLen;
UINT8* VGMData;
UINT32 VGMPos;
INT32 VGMSmplPos;
UINT8* DstData;
UINT32 DstDataLen;
char FileBase[0x100];

int main(int argc, char* argv[])
{
	int argbase;
	int ErrVal;
	char FileName[0x100];
	char SplitTxt[0x100];
	UINT32 SplitDelay;
	
	printf("VGM Splitter (Delay Edition)\n------------\n\n");
	
	ErrVal = 0;
	DIGIT_COUNT = 2;
	HEXION_SPLIT = 0x00;
	argbase = 0x01;
	if (argc >= argbase + 0x01)
	{
		if (! strncmp(argv[argbase + 0x00], "-digits:", 8))
		{
			DIGIT_COUNT = (UINT16)strtoul(argv[argbase + 0x00] + 8, NULL, 0);
			argbase ++;
		}
	}
	if (! DIGIT_COUNT)
		DIGIT_COUNT = 1;
	if (argc >= argbase + 0x01)
	{
		if (! strcmp(argv[argbase + 0x00], "-hexsplt"))
		{
			HEXION_SPLIT = 0x01;
			argbase ++;
		}
	}
	
	printf("File Name:\t");
	if (argc < argbase + 0x01)
	{
		gets(FileName);
	}
	else
	{
		strcpy(FileName, argv[argbase + 0x00]);
		printf("%s\n", FileName);
	}
	if (! strlen(FileName))
		return 0;
	
	printf("Split Delay (in Samples):\t");
	if (argc <= argbase + 0x02)
	{
		gets(SplitTxt);
	}
	else
	{
		strcpy(SplitTxt, argv[argbase + 0x01]);
		printf("%s\n", SplitTxt);
	}
	SplitDelay = strtoul(SplitTxt, NULL, 0);
	if (! SplitDelay)
		SplitDelay = 0x8000;
	
	if (! OpenVGMFile(FileName))
	{
		printf("Error opening the file!\n");
		ErrVal = 1;
		goto EndProgram;
	}
	printf("\n");
	
	SetTrimOptions(0x00, 0x00);
	SplitVGMData(SplitDelay);
	
	free(VGMData);
	free(DstData);
	
EndProgram:
#ifdef WIN32
	if (argv[0][1] == ':')
	{
		// Executed by Double-Clicking (or Drap and Drop)
		if (_kbhit())
			_getch();
		_getch();
	}
#endif
	
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
	
	strcpy(FileBase, FileName);
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
	
	//printf("                                                                \r");
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

static void SplitVGMData(const UINT32 SplitDelay)
{
	INT32 VGMSmplStart;
	INT32 VGMSmplEnd;
	UINT8 ChipID;
	UINT8 Command;
	UINT32 CmdDelay;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	char TempStr[0x100];
#ifdef WIN32
	UINT32 CmdTimer;
	char MinSecStr[0x80];
#endif
	UINT32 CmdLen;
	bool StopVGM;
	bool IsDelay;
	bool EmptyFile;
	UINT16 SplitFile;
	bool IgnoreCmd;
	UINT32 LastCmdDly;
	UINT32 AddMask;
	
	// +0x100 - Make sure to have enough room for additional delays
	DstData = (UINT8*)malloc(VGMDataLen + 0x100);
	CmdDelay = 0;
	VGMPos = VGMHead.lngDataOffset;
	
#ifdef WIN32
	CmdTimer = 0;
#endif
	VGMSmplStart = 0x00;
	VGMSmplPos = 0x00;
	SplitFile = 0x00;
	LastCmdDly = 0x00;
	StopVGM = false;
	EmptyFile = true;
	AddMask = 0x00;
	while(VGMPos < VGMHead.lngEOFOffset)
	{
		CmdLen = 0x00;
		Command = VGMData[VGMPos + 0x00];
		IsDelay = false;
		IgnoreCmd = false;
		
		if (Command >= 0x70 && Command <= 0x8F)
		{
			switch(Command & 0xF0)
			{
			case 0x70:
				TempSht = (Command & 0x0F) + 0x01;
				VGMSmplPos += TempSht;
				CmdDelay += TempSht;
				IsDelay = true;
				break;
			case 0x80:
				TempSht = Command & 0x0F;
				VGMSmplPos += TempSht;
				CmdDelay += TempSht;
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
				CmdDelay += TempSht;
				CmdLen = 0x01;
				IsDelay = true;
				break;
			case 0x63:	// 1/50s delay
				TempSht = 882;
				VGMSmplPos += TempSht;
				CmdDelay += TempSht;
				CmdLen = 0x01;
				IsDelay = true;
				break;
			case 0x61:	// xx Sample Delay
				memcpy(&TempSht, &VGMData[VGMPos + 0x01], 0x02);
				VGMSmplPos += TempSht;
				CmdDelay += TempSht;
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
				TempSht = ((VGMData[VGMPos + 0x00] & 0x01) << 8) | VGMData[VGMPos + 0x01];
				if (TempSht >= 0x024 && TempSht <= 0x027)	// Timer Registers
					IgnoreCmd = true;
				/*if (TempSht >= 0x0B4 && TempSht <= 0x0B6 || TempSht >= 0x1B4 && TempSht <= 0x1B5)	// Timer Registers
					IgnoreCmd = true;*/
				CmdLen = 0x03;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMData[VGMPos + 0x02];
				memcpy(&TempLng, &VGMData[VGMPos + 0x03], 0x04);
				TempLng &= 0x7FFFFFFF;
				CmdLen = 0x07 + TempLng;
				break;
			case 0xE0:	// Seek to PCM Data Bank Pos
				memcpy(&TempLng, &VGMData[VGMPos + 0x01], 0x04);
				CmdLen = 0x05;
				break;
			case 0x4F:	// GG Stereo
				CmdLen = 0x02;
				break;
			case 0x54:	// YM2151 write
				TempByt = VGMData[VGMPos + 0x01];
				if (TempByt >= 0x10 && TempByt <= 0x14)	// Timer Registers
					IgnoreCmd = true;
				else if (TempByt == 0x08)
					IgnoreCmd = true;
				CmdLen = 0x03;
				break;
			case 0xC0:	// Sega PCM memory write
				memcpy(&TempSht, &VGMData[VGMPos + 0x01], 0x02);
				CmdLen = 0x04;
				break;
			case 0xB0:	// RF5C68 register write
				CmdLen = 0x03;
				break;
			case 0xC1:	// RF5C68 memory write
				memcpy(&TempSht, &VGMData[VGMPos + 0x01], 0x02);
				CmdLen = 0x04;
				break;
			case 0x55:	// YM2203
				TempSht = 0x000 | VGMData[VGMPos + 0x01];
				if (TempSht >= 0x024 && TempSht <= 0x027)	// Timer Registers
					IgnoreCmd = true;
				CmdLen = 0x03;
				break;
			case 0x56:	// YM2608 write port 0
			case 0x57:	// YM2608 write port 1
				TempSht = ((VGMData[VGMPos + 0x00] & 0x01) << 8) | VGMData[VGMPos + 0x01];
				if (TempSht == 0x007)	// Unknown
					IgnoreCmd = true;
				if (TempSht >= 0x024 && TempSht <= 0x027)	// Timer Registers
					IgnoreCmd = true;
				CmdLen = 0x03;
				break;
			case 0x58:	// YM2610 write port 0
			case 0x59:	// YM2610 write port 1
				TempSht = ((VGMData[VGMPos + 0x00] & 0x01) << 8) | VGMData[VGMPos + 0x01];
				if (TempSht >= 0x024 && TempSht <= 0x027)	// Timer Registers
					IgnoreCmd = true;
				CmdLen = 0x03;
				break;
			case 0x5A:	// YM3812 write
				TempByt = VGMData[VGMPos + 0x01];
				if (TempByt >= 0x02 && TempByt <= 0x04)	// Timer Registers
					IgnoreCmd = true;
				CmdLen = 0x03;
				break;
			case 0x5B:	// YM3526 write
				TempByt = VGMData[VGMPos + 0x01];
				if (TempByt >= 0x02 && TempByt <= 0x04)	// Timer Registers
					IgnoreCmd = true;
				CmdLen = 0x03;
				break;
			case 0x5C:	// Y8950 write
				TempByt = VGMData[VGMPos + 0x01];
				if (TempByt >= 0x02 && TempByt <= 0x04)	// Timer Registers
					IgnoreCmd = true;
				CmdLen = 0x03;
				break;
			case 0x5E:	// YMF262 write port 0
			case 0x5F:	// YMF262 write port 1
				TempSht = ((VGMData[VGMPos + 0x00] & 0x01) << 8) | VGMData[VGMPos + 0x01];
				if (TempSht >= 0x002 && TempSht <= 0x004)	// Timer Registers
					IgnoreCmd = true;
				CmdLen = 0x03;
				break;
			case 0x5D:	// YMZ280B write
				CmdLen = 0x03;
				break;
			case 0xD0:	// YMF278B write
				TempSht = ((VGMData[VGMPos + 0x01] & 0x03) << 8) | VGMData[VGMPos + 0x02];
				TempSht &= 0x7FFF;
				if (TempSht >= 0x002 && TempSht <= 0x004)	// Timer Registers
					IgnoreCmd = true;
				CmdLen = 0x04;
				break;
			case 0xD1:	// YMF271 write
				TempSht = ((VGMData[VGMPos + 0x01] & 0x0F) << 8) | VGMData[VGMPos + 0x02];
				TempSht &= 0x7FFF;
				if (TempSht >= 0x610 && TempSht <= 0x613)	// Timer Registers
					IgnoreCmd = true;
				CmdLen = 0x04;
				break;
			case 0xD2:	// K051649 write
				if (HEXION_SPLIT)
				{
					TempByt = VGMData[VGMPos + 0x01] & 0x7F;
					if (TempByt == 0x02 || TempByt == 0x03)
					{
						if (TempByt == 0x02)
							TempByt = VGMData[VGMPos + 0x02];
						else
							TempByt = 0x05;
						// ignore KeyOff or Volume = 0
						if (! VGMData[VGMPos + 0x03])
						{
							if (AddMask & (1 << TempByt))
								AddMask &= ~(1 << TempByt);
							else
								IgnoreCmd = true;
						}
						else
						{
							//AddMask |= (1 << TempByt);
							AddMask |= 0x3F << 1;
						}
					}
				}
				CmdLen = 0x04;
				break;
			case 0xB8:	// OKIM6295 write
				if (HEXION_SPLIT)
				{
					TempByt = VGMData[VGMPos + 0x01] & 0x7F;
					if (! TempByt)
					{
						// ignore Channel Stop
						if (! (AddMask & 0x80))
						{
							// latch-command
							if (VGMData[VGMPos + 0x02] & 0x80)
							{
								AddMask |= 0x80;	// request data-command
								AddMask |= 0x40;	// channel active
							}
							else
							{
								if (AddMask & 0x40)	// channel active?
									AddMask &= ~0x40;	// set channel inactive
								else
									IgnoreCmd = true;	// redundant channel off command
							}
						}
						else
						{
							// data-command
							AddMask &= ~0x80;
						}
					}
				}
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
		VGMPos += CmdLen;
		
		if (CmdDelay >= 200)
			LastCmdDly = VGMSmplPos;
		if (! IsDelay)
		{
			if (! IgnoreCmd)
			{
				CmdDelay = 0x00;
				if (! StopVGM)
				{
					EmptyFile = false;
					VGMSmplEnd = VGMSmplPos;
				}
			}
			else if (EmptyFile)
			{
				VGMSmplStart = VGMSmplPos;
				//CmdDelay = 0x00;
			}
		}
		//if (IsDelay || StopVGM)
		if (CmdDelay || StopVGM)
		{
			if (EmptyFile)
			{
				if (VGMSmplPos - LastCmdDly < 735)
					VGMSmplStart = LastCmdDly;
				else
					VGMSmplStart = VGMSmplPos;
				CmdDelay = 0x00;
			}
			else if (CmdDelay >= SplitDelay || StopVGM)
			{
				TempLng = VGMSmplPos - CmdDelay;
				if (TempLng > VGMSmplStart)	// prevent 0-sample files
				{
					TrimVGMData(VGMSmplStart, 0x00, TempLng, false, true);
					
					SplitFile ++;
					sprintf(TempStr, "%s_%02u.vgm", FileBase, SplitFile);
					WriteVGMFile(TempStr);
				}
#ifdef WIN32
				CmdTimer = 0;
#endif
				
				VGMSmplStart = VGMSmplPos;
				CmdDelay = 0x00;
				EmptyFile = true;
				AddMask = 0;
			}
		}
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
	printf("\t\t\t\t\t\t\t\t\r");
	printf("Done.\n");
	
	return;
}

#ifdef WIN32
static void PrintMinSec(const UINT32 SamplePos, char* TempStr)
{
	float TimeSec;
	UINT16 TimeMin;
	
	TimeSec = (float)SamplePos / (float)44100.0;
	TimeMin = (UINT16)TimeSec / 60;
	TimeSec -= TimeMin * 60;
	sprintf(TempStr, "%02u:%05.2f", TimeMin, TimeSec);
	
	return;
}
#endif
