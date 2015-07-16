// opl_23.c - VGM Compressor
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
#include "vgm_lib.h"


static bool OpenVGMFile(const char* FileName);
static void WriteVGMFile(const char* FileName);
static void CompressVGMData(void);
#ifdef WIN32
static void PrintMinSec(const UINT32 SamplePos, char* TempStr);
#endif

VGM_HEADER VGMHead;
UINT32 VGMDataLen;
UINT8* VGMData;
UINT32 VGMPos;
INT32 VGMSmplPos;
//UINT8* DstData;
//UINT32 DstDataLen;
char FileBase[0x100];

bool OPL3to2;
#define OPL3_LEFT	0x10
#define OPL3_RIGHT	0x20
UINT8 OPL2ChipMap[0x02] = {0x01, 0x00};

int main(int argc, char* argv[])
{
	int argbase;
	int ErrVal;
	char FileName[0x100];
	bool NeedProcessing;
	UINT32 NewClk_OPL2;
	UINT32 NewClk_OPL3;
	
	printf("OPL 2<->3 Converter\n-------------------\n");
	
	ErrVal = 0;
	argbase = 0x01;
	OPL3to2 = true;
	if (argc >= argbase + 0x01)
	{
		if (! strcmp(argv[argbase + 0x00], "-toopl3"))
		{
			OPL3to2 = false;
			argbase ++;
		}
	}
	
	printf("File Name:\t");
	if (argc <= argbase + 0x00)
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
	
	if (! OpenVGMFile(FileName))
	{
		printf("Error opening the file!\n");
		ErrVal = 1;
		goto EndProgram;
	}
	printf("\n");
	
	NeedProcessing = false;
	if (OPL3to2)
	{
		if (VGMHead.lngHzYMF262)
		{
			if (VGMHead.lngHzYMF262 & 0x40000000)
			{
				printf("Sorry, but I can't work with Dual OPL3!\n");
			}
			else
			{
				NeedProcessing = true;
				NewClk_OPL2 = 0xC0000000 | (VGMHead.lngHzYMF262 / 4);
				NewClk_OPL3 = 0x00;
			}
		}
	}
	else
	{
		if (VGMHead.lngHzYM3812)
		{
			if (~VGMHead.lngHzYM3812 & 0xC0000000)
			{
				printf("Sorry, but currently I need panned DualOPL2!\n");
			}
			else
			{
				NeedProcessing = true;
				NewClk_OPL2 = 0x00;
				NewClk_OPL3 = VGMHead.lngHzYM3812 * 4;
			}
		}
	}
	if (NeedProcessing)
	{
		memcpy(&VGMData[0x50], &NewClk_OPL2, 0x04);
		memcpy(&VGMData[0x5C], &NewClk_OPL3, 0x04);
		CompressVGMData();
		
		if (argc > argbase + 0x01)
			strcpy(FileName, argv[argbase + 0x01]);
		else
			strcpy(FileName, "");
		if (! FileName[0x00])
			sprintf(FileName, "%s_opl%u.vgm", FileBase, (OPL3to2 ? 2 : 3));
		WriteVGMFile(FileName);
	}
	
	free(VGMData);
	//free(DstData);
	
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
	if (VGMHead.lngVersion < 0x00000101)
	{
		VGMHead.lngRate = 0;
	}
	if (VGMHead.lngVersion < 0x00000110)
	{
		VGMHead.shtPSG_Feedback = 0x0000;
		VGMHead.bytPSG_SRWidth = 0x00;
		VGMHead.lngHzYM2612 = VGMHead.lngHzYM2413;
		VGMHead.lngHzYM2151 = VGMHead.lngHzYM2413;
	}
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
	
	hFile = fopen(FileName, "wb");
	//fwrite(DstData, 0x01, DstDataLen, hFile);
	fwrite(VGMData, 0x01, VGMDataLen, hFile);
	fclose(hFile);
	
	printf("File written.\n");
	
	return;
}

static void CompressVGMData(void)
{
	UINT8 ChipID;
	UINT8 Command;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	UINT32 ROMSize;
	//UINT32 DataStart;
	//UINT32 DataLen;
#ifdef WIN32
	UINT32 CmdTimer;
	char TempStr[0x80];
	char MinSecStr[0x80];
#endif
	UINT32 CmdLen;
	bool StopVGM;
	UINT8* VGMPnt;
	
	//DstData = (UINT8*)malloc(VGMDataLen + 0x100);
	VGMPos = VGMHead.lngDataOffset;
	//DstPos = VGMHead.lngDataOffset;
	VGMSmplPos = 0;
	//memcpy(DstData, VGMData, VGMPos);	// Copy Header
	
#ifdef WIN32
	CmdTimer = 0;
#endif
	
	StopVGM = false;
	while(VGMPos < VGMHead.lngEOFOffset)
	{
		CmdLen = 0x00;
		Command = VGMData[VGMPos + 0x00];
		
		if (Command >= 0x70 && Command <= 0x8F)
		{
			switch(Command & 0xF0)
			{
			case 0x70:
				TempSht = (Command & 0x0F) + 0x01;
				VGMSmplPos += TempSht;
				break;
			case 0x80:
				TempSht = Command & 0x0F;
				VGMSmplPos += TempSht;
				break;
			}
			CmdLen = 0x01;
		}
		else
		{
			VGMPnt = &VGMData[VGMPos];
			
			// Cheat Mode (to use 2 instances of 1 chip)
			ChipID = 0x00;
			switch(Command)
			{
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
			case 0xAE:
			case 0xAF:
				if (VGMHead.lngHzYMF262 & 0x40000000)
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
				CmdLen = 0x01;
				break;
			case 0x63:	// 1/50s delay
				TempSht = 882;
				VGMSmplPos += TempSht;
				CmdLen = 0x01;
				break;
			case 0x61:	// xx Sample Delay
				memcpy(&TempSht, &VGMPnt[0x01], 0x02);
				VGMSmplPos += TempSht;
				CmdLen = 0x03;
				break;
			case 0x5A:	// YM3812 write
				if (! OPL3to2)
				{
					switch(VGMPnt[0x01] & 0xF0)
					{
					case 0xC0:
						if (ChipID == 0x00)
							VGMPnt[0x02] |= OPL3_LEFT;
						else if (ChipID == 0x01)
							VGMPnt[0x02] |= OPL3_RIGHT;
						break;
					}
					
					if (VGMPnt[0x01] == 0x05)
					{
						VGMPnt[0x00] = 0x5F;
						VGMPnt[0x02] |= 0x01;
					}
					else if (VGMPnt[0x01] < 0x20)
						VGMPnt[0x00] = 0x5E;
					else
						VGMPnt[0x00] = 0x5E + OPL2ChipMap[ChipID];
				}
				CmdLen = 0x03;
				break;
			case 0x5E:	// YMF262 write port 0
			case 0x5F:	// YMF262 write port 1
				TempByt = Command & 0x01;
				if (OPL3to2)
				{
					switch(VGMPnt[0x01] & 0xF0)
					{
					case 0x00:
						if (! TempByt && VGMPnt[0x01] == 0x01)
							VGMPnt[0x02] |= 0x20;	// enforce Wave Forms
						break;
					case 0xC0:
						if (TempByt == OPL2ChipMap[0x00] && (VGMPnt[0x02] & OPL3_RIGHT))
							printf("Warning! Stereo mismatch at 0x%06!\n", VGMPos);
						else if (TempByt == OPL2ChipMap[0x01] && (VGMPnt[0x02] & OPL3_LEFT))
							printf("Warning! Stereo mismatch at 0x%06!\n", VGMPos);
						break;
					case 0xE0:
					case 0xF0:
						if (VGMPnt[0x02] & 0x04)
							printf("Warning! Waveform %02 used at 0x%06!\n", VGMPnt[0x02], VGMPos);
						break;
					}
					if (VGMPnt[0x01] < 0x20)
						VGMPnt[0x00] = TempByt ? 0xAA : 0x5A;
					else
						VGMPnt[0x00] = OPL2ChipMap[TempByt] ? 0xAA : 0x5A;
				}
				CmdLen = 0x03;
				break;
			case 0x50:	// SN76496 write
				CmdLen = 0x02;
				break;
			case 0x67:	// PCM Data Stream
				memcpy(&TempLng, &VGMPnt[0x03], 0x04);
				
				ChipID = (TempLng & 0x80000000) >> 31;
				TempLng &= 0x7FFFFFFF;
				
				CmdLen = 0x07 + TempLng;
				break;
			case 0xE0:	// Seek to PCM Data Bank Pos
				memcpy(&TempLng, &VGMPnt[0x01], 0x04);
				CmdLen = 0x05;
				break;
			case 0x4F:	// GG Stereo
				CmdLen = 0x02;
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
		if (StopVGM)
			break;
		
#ifdef WIN32
		if (CmdTimer < GetTickCount())
		{
			PrintMinSec(VGMSmplPos, MinSecStr);
			PrintMinSec(VGMHead.lngTotalSamples, TempStr);
			TempLng = VGMPos - VGMHead.lngDataOffset;
			ROMSize = VGMHead.lngEOFOffset - VGMHead.lngDataOffset;
			printf("%04.3f %% - %s / %s (%08X / %08X) ...\r", (float)TempLng / ROMSize * 100,
					MinSecStr, TempStr, VGMPos, VGMHead.lngEOFOffset);
			CmdTimer = GetTickCount() + 200;
		}
#endif
	}
	printf("\t\t\t\t\t\t\t\t\r");
	
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
