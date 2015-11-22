// vgm_spts.c - VGM Splitter (Sample Edition)
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
static void SplitVGMData(int argc, char* argv[]);

// Function Prototype from vgm_trml.c
void SetTrimOptions(UINT8 TrimMode, UINT8 WarnMask);
void TrimVGMData(const INT32 StartSmpl, const INT32 LoopSmpl, const INT32 EndSmpl,
				 const bool HasLoop, const bool KeepESmpl);


VGM_HEADER VGMHead;
UINT32 VGMDataLen;
UINT8* VGMData;
UINT32 VGMPos;
INT32 VGMSmplStart;
INT32 VGMSmplPos;
UINT8* DstData;
UINT32 DstDataLen;
char FileBase[0x100];

int main(int argc, char* argv[])
{
	int argbase;
	int ErrVal;
	char FileName[0x100];
	
	printf("VGM Splitter (Sample Edition)\n------------\n\n");
	
	ErrVal = 0;
	argbase = 1;
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
	
	if (! OpenVGMFile(FileName))
	{
		printf("Error opening the file!\n");
		ErrVal = 1;
		goto EndProgram;
	}
	printf("\n");
	
	SetTrimOptions(0x00, 0x00);
	SplitVGMData(argc, argv);
	
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

static void SplitVGMData(int argc, char* argv[])
{
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
	int CurArg;
	char SplitTxt[0x100];
	INT32 SplitSmpl;
	bool IgnoreCmd;
	
	// +0x100 - Make sure to have enough room for additional delays
	DstData = (UINT8*)malloc(VGMDataLen + 0x100);
	CmdDelay = 0;
	VGMPos = VGMHead.lngDataOffset;
	
	VGMSmplPos = 0x00;
	SplitFile = 0x00;
	StopVGM = false;
	
	CurArg = 2;
	while(! StopVGM)
	{
		printf("Current Sample: %u - Split Sample:\t", VGMSmplPos);
		if (CurArg < argc)
		{
			strcpy(SplitTxt, argv[CurArg]);
			printf("%s\n", SplitTxt);
			CurArg ++;
		}
		else
		{
			fgets(SplitTxt, sizeof(SplitTxt), stdin);
		}
		SplitSmpl = strtol(SplitTxt, NULL, 0);
		if (! SplitSmpl)
			break;
		else if (SplitSmpl == -1)	// -1 = write rest of the file
			SplitSmpl = VGMHead.lngTotalSamples + 1;	// include data of final sample
		else if (SplitSmpl < VGMSmplPos)
		{
			printf("Invalid Sample Point!\n");
			continue;
		}
		
		TempLng = 0x00;
		memcpy(&VGMData[0x1C], &TempLng, 0x04);	// Disable Loops
		memcpy(&VGMData[0x20], &TempLng, 0x04);
		
#ifdef WIN32
		CmdTimer = 0;
#endif
		CmdDelay = 0x00;
		EmptyFile = true;
		VGMSmplStart = VGMSmplPos;
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
					CmdLen = 0x03;
					break;
				case 0xC0:	// Sega PCM memory write
					//memcpy(&TempSht, &VGMData[VGMPos + 0x01], 0x02);
					CmdLen = 0x04;
					break;
				case 0xB0:	// RF5C68 register write
					CmdLen = 0x03;
					break;
				case 0xC1:	// RF5C68 memory write
					//memcpy(&TempSht, &VGMData[VGMPos + 0x01], 0x02);
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
					TempSht = ((VGMData[VGMPos + 0x01] & 0x01) << 8) | VGMData[VGMPos + 0x02];
					if (TempSht >= 0x002 && TempSht <= 0x004)	// Timer Registers
						IgnoreCmd = true;
					CmdLen = 0x04;
					break;
				case 0xD1:	// YMF271 write
					CmdLen = 0x04;
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
			
			if (! IsDelay)
			{
				if (! IgnoreCmd)
				{
					CmdDelay = 0x00;
					if (! StopVGM)
						EmptyFile = false;
				}
				else if (EmptyFile)
				{
					VGMSmplStart = VGMSmplPos;
					CmdDelay = 0x00;
				}
			}
			if (IsDelay || StopVGM)
			{
				if (EmptyFile)
				{
					VGMSmplStart = VGMSmplPos;
					CmdDelay = 0x00;
				} 
				else if (VGMSmplPos >= SplitSmpl || StopVGM)
				{
					TempLng = VGMSmplPos - CmdDelay;
					TrimVGMData(VGMSmplStart, 0x00, TempLng, false, true);
					
					SplitFile ++;
					sprintf(TempStr, "%s_%02u.vgm", FileBase, SplitFile);
					WriteVGMFile(TempStr);
					
					VGMSmplStart = VGMSmplPos;
					break;
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
				printf("%04.3f %% - %s / %s (%08X / %08X) ...\r", (float)TempLng / CmdLen *
						100, MinSecStr, TempStr, VGMPos, VGMHead.lngEOFOffset);
				CmdTimer = GetTickCount() + 200;
			}
#endif
		}
	}
	//printf("\t\t\t\t\t\t\t\t\r");
	//printf("Done.\n");
	
	return;
}
