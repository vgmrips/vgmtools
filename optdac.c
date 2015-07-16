// optdac.c - VGM DAC Optimizer
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
static void OptimizeVGMData(void);
static UINT32 DACCommandsToKill(void);
#ifdef WIN32
static void PrintMinSec(const UINT32 SamplePos, char* TempStr);
#endif


VGM_HEADER VGMHead;
UINT32 VGMDataLen;
UINT8* VGMData;
UINT32 VGMPos;
INT32 VGMSmplPos;
UINT8* DstData;
UINT32 DstDataLen;
char FileBase[0x100];
UINT32 DataSizeA;
UINT32 DataSizeB;

UINT32 NxtCmdPos;
UINT8 NxtCmdCommand;
UINT16 NxtCmdReg;
UINT8 NxtCmdVal;

bool JustTimerCmds;

int main(int argc, char* argv[])
{
	int argbase;
	int ErrVal;
	char FileName[0x100];
	
	printf("VGM DAC Optimizer\n-----------------\n\n");
	
	ErrVal = 0;
	JustTimerCmds = false;
	argbase = 0x01;
	
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
	
	OptimizeVGMData();
	
	if (DataSizeB < DataSizeA)
	{
		if (argc > argbase + 0x01)
			strcpy(FileName, argv[argbase + 0x01]);
		else
			strcpy(FileName, "");
		if (! FileName[0x00])
		{
			strcpy(FileName, FileBase);
			strcat(FileName, "_optimized.vgm");
		}
		WriteVGMFile(FileName);
	}
	
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
	if (VGMHead.lngVersion < 0x00000151)
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
	fwrite(DstData, 0x01, DstDataLen, hFile);
	fclose(hFile);
	
	printf("File written.\n");
	
	return;
}

static void OptimizeVGMData(void)
{
	UINT32 DstPos;
	UINT8 ChipID;
	UINT8 Command;
	UINT32 CmdDelay;
	UINT32 AllDelay;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	//UINT32 ROMSize;
	//UINT32 DataStart;
	//UINT32 DataLen;
#ifdef WIN32
	UINT32 CmdTimer;
	char TempStr[0x80];
	char MinSecStr[0x80];
#endif
	UINT32 CmdLen;
	bool StopVGM;
	bool WriteEvent;
	UINT32 NewLoopS;
	UINT32 RemDACKill[0x02];
	
	DstData = (UINT8*)malloc(VGMDataLen + 0x100);
	AllDelay = 0;
	VGMPos = VGMHead.lngDataOffset;
	DstPos = VGMHead.lngDataOffset;
	VGMSmplPos = 0;
	NewLoopS = 0x00;
	memcpy(DstData, VGMData, VGMPos);	// Copy Header
	
	for (ChipID = 0x00; ChipID < 0x02; ChipID ++)
		RemDACKill[ChipID] = 0x00;
	
#ifdef WIN32
	CmdTimer = 0;
#endif
	StopVGM = false;
	while(VGMPos < VGMHead.lngEOFOffset)
	{
		CmdDelay = 0;
		CmdLen = 0x00;
		Command = VGMData[VGMPos + 0x00];
		WriteEvent = true;
		
		if (Command >= 0x70 && Command <= 0x8F)
		{
			switch(Command & 0xF0)
			{
			case 0x70:
				TempSht = (Command & 0x0F) + 0x01;
				VGMSmplPos += TempSht;
				CmdDelay = TempSht;
				WriteEvent = false;
				break;
			case 0x80:
				TempSht = Command & 0x0F;
				VGMSmplPos += TempSht;
				//CmdDelay = TempSht;
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
			case 0xA2:
			case 0xA3:
				if (VGMHead.lngHzYM2612 & 0x40000000)
				{
					Command -= 0x50;
					ChipID = 0x01;
				}
				break;
			}
			
			NxtCmdPos = VGMPos;
			NxtCmdCommand = Command;
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
				WriteEvent = false;
				break;
			case 0x63:	// 1/50s delay
				TempSht = 882;
				VGMSmplPos += TempSht;
				CmdDelay = TempSht;
				CmdLen = 0x01;
				WriteEvent = false;
				break;
			case 0x61:	// xx Sample Delay
				memcpy(&TempSht, &VGMData[VGMPos + 0x01], 0x02);
				VGMSmplPos += TempSht;
				CmdDelay = TempSht;
				CmdLen = 0x03;
				WriteEvent = false;
				break;
			case 0x50:	// SN76496 write
				CmdLen = 0x02;
				break;
			case 0x52:	// YM2612 write port 0
			case 0x53:	// YM2612 write port 1
				TempSht = ((Command & 0x01) << 8) | (VGMData[VGMPos + 0x01] << 0);
				if (TempSht == 0x02A)
				{
					if (! RemDACKill[ChipID])
						RemDACKill[ChipID] = DACCommandsToKill();
					else
						WriteEvent = false;
					
					if (RemDACKill[ChipID])
						RemDACKill[ChipID] --;
				}
				CmdLen = 0x03;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMData[VGMPos + 0x02];
				memcpy(&TempLng, &VGMData[VGMPos + 0x03], 0x04);
				TempLng &= 0x7FFFFFFF;
				
				CmdLen = 0x07 + TempLng;
				break;
			case 0x90:	// DAC Ctrl: Setup Chip
				VGMPos += 0x05;
				break;
			case 0x91:	// DAC Ctrl: Set Data
				VGMPos += 0x05;
				break;
			case 0x92:	// DAC Ctrl: Set Freq
				VGMPos += 0x06;
				break;
			case 0x93:	// DAC Ctrl: Play from Start Pos
				VGMPos += 0x0B;
				break;
			case 0x94:	// DAC Ctrl: Stop immediately
				VGMPos += 0x02;
				break;
			case 0x95:	// DAC Ctrl: Play Block (small)
				VGMPos += 0x05;
				break;
			default:	// Handle all other known and unknown commands
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
		
		if (WriteEvent || VGMPos == VGMHead.lngLoopOffset)
		{
			if (VGMPos != VGMHead.lngLoopOffset)
			{
				AllDelay += CmdDelay;
				CmdDelay = 0x00;
			}
			while(AllDelay)
			{
				if (AllDelay <= 0xFFFF)
					TempSht = (UINT16)AllDelay;
				else
					TempSht = 0xFFFF;
				
				if (! TempSht)
				{
					// don't do anything - I just want to be safe
				}
				if (TempSht <= 0x10)
				{
					DstData[DstPos] = 0x70 | (TempSht - 0x01);
					DstPos ++;
				}
				else if (TempSht <= 0x20)
				{
					DstData[DstPos] = 0x7F;
					DstPos ++;
					DstData[DstPos] = 0x70 | (TempSht - 0x11);
					DstPos ++;
				}
				else if ((TempSht >=  735 && TempSht <=  751) || TempSht == 1470)
				{
					TempLng = TempSht;
					while(TempLng >= 735)
					{
						DstData[DstPos] = 0x62;
						DstPos ++;
						TempLng -= 735;
					}
					TempSht -= (UINT16)TempLng;
				}
				else if ((TempSht >=  882 && TempSht <=  898) || TempSht == 1764)
				{
					TempLng = TempSht;
					while(TempLng >= 882)
					{
						DstData[DstPos] = 0x63;
						DstPos ++;
						TempLng -= 882;
					}
					TempSht -= (UINT16)TempLng;
				}
				else if (TempSht == 1617)
				{
					DstData[DstPos] = 0x63;
					DstPos ++;
					DstData[DstPos] = 0x62;
					DstPos ++;
				}
				else
				{
					DstData[DstPos + 0x00] = 0x61;
					memcpy(&DstData[DstPos + 0x01], &TempSht, 0x02);
					DstPos += 0x03;
				}
				AllDelay -= TempSht;
			}
			AllDelay = CmdDelay;
			CmdDelay = 0x00;
			
			if (VGMPos == VGMHead.lngLoopOffset)
				NewLoopS = DstPos;
			
			if (WriteEvent)
			{
				memcpy(&DstData[DstPos], &VGMData[VGMPos], CmdLen);
				DstPos += CmdLen;
			}
		}
		else
		{
			AllDelay += CmdDelay;
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
	DataSizeA = VGMPos - VGMHead.lngDataOffset;
	DataSizeB = DstPos - VGMHead.lngDataOffset;
	if (VGMHead.lngLoopOffset)
	{
		VGMHead.lngLoopOffset = NewLoopS;
		if (! NewLoopS)
			printf("Error! Failed to relocate Loop Point!\n");
		else
			NewLoopS -= 0x1C;
		memcpy(&DstData[0x1C], &NewLoopS, 0x04);
	}
	printf("\t\t\t\t\t\t\t\t\r");
	
	if (VGMHead.lngGD3Offset)
	{
		VGMPos = VGMHead.lngGD3Offset;
		memcpy(&TempLng, &VGMData[VGMPos + 0x00], 0x04);
		if (TempLng == FCC_GD3)
		{
			memcpy(&CmdLen, &VGMData[VGMPos + 0x08], 0x04);
			CmdLen += 0x0C;
			
			VGMHead.lngGD3Offset = DstPos;
			TempLng = DstPos - 0x14;
			memcpy(&DstData[0x14], &TempLng, 0x04);
			memcpy(&DstData[DstPos], &VGMData[VGMPos], CmdLen);
			DstPos += CmdLen;
		}
	}
	DstDataLen = DstPos;
	TempLng = DstDataLen - 0x04;
	memcpy(&DstData[0x04], &TempLng, 0x04);
	
	return;
}

static UINT32 DACCommandsToKill(void)
{
	UINT32 CurPos;
	UINT8 Command;
	//UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	UINT32 CmdLen;
	UINT32 CmdCount;
	UINT8 ChipCmd;
	UINT8 DACVal;
	bool StopVGM;
	
	CurPos = NxtCmdPos;
	CmdCount = 0x00;
	StopVGM = false;
	while(CurPos < VGMHead.lngEOFOffset)
	{
		CmdLen = 0x00;
		Command = VGMData[CurPos + 0x00];
		
		if (Command >= 0x70 && Command <= 0x8F)
		{
			CmdLen = 0x01;
		}
		else
		{
			switch(Command)
			{
			case 0x66:	// End Of File
				CmdLen = 0x01;
				StopVGM = true;
				break;
			case 0x62:	// 1/60s delay
				CmdLen = 0x01;
				break;
			case 0x63:	// 1/50s delay
				CmdLen = 0x01;
				break;
			case 0x61:	// xx Sample Delay
				CmdLen = 0x03;
				break;
			case 0x67:	// PCM Data Stream
				memcpy(&TempLng, &VGMData[CurPos + 0x03], 0x04);
				CmdLen = 0x07 + TempLng;
				break;
			case 0x68:	// PCM RAM write
				CmdLen = 0x0C;
				break;
			case 0x50:	// SN76496 write
				CmdLen = 0x02;
				break;
			case 0x52:	// YM2612 write port 0
			case 0x53:	// YM2612 write port 1
			case 0xA2:
			case 0xA3:
				TempSht = ((Command & 0x01) << 8) | (VGMData[CurPos + 0x01] << 0);
				if (TempSht == 0x02A)
				{
					if (! CmdCount)
					{
						ChipCmd = Command;
						DACVal = VGMData[CurPos + 0x02];
						CmdCount = 0x01;
					}
					else if (Command == ChipCmd)
					{
						if (VGMData[CurPos + 0x02] == DACVal)
							CmdCount ++;
						else
							StopVGM = true;
					}
				}
				CmdLen = 0x03;
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
					CmdLen = 0x01;
					break;
				}
				break;
			}
		}
		
		CurPos += CmdLen;
		if (StopVGM)
			break;
	}
	
	if (CmdCount < 0x80)	// with a sample rate of 8 KHz, this equals 16 ms
		CmdCount = 0x00;
	
	return CmdCount;
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
