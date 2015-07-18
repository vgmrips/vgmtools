// optvgm32.c - VGM PWM Optimizer
//

// TODO: Better gcd

#define EXTRA_SYNC

#include "compat.h"
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
static void EnumeratePWMWrite(void);
static UINT32 gcd(UINT32 x, UINT32 y);
static void DataReducer(void);
static void MakeDataStream(void);
static void RewriteVGMData(void);
#ifdef WIN32
static void PrintMinSec(const UINT32 SamplePos, char* TempStr);
#endif


typedef struct vgm_data_write
{
	UINT8 Port;
	UINT16 Value;
	UINT32 SmplPos;
} DATA_WRITE;

typedef struct vgm_data_stream
{
	UINT8 Type;
	UINT32 Length;
	void* Data;
} DATA_STREAM;

typedef struct stream_control_command
{
	UINT8 Command;
	UINT8 StreamID;
	UINT8 DataB1;
	UINT8 DataB2;
	UINT8 DataB3;
	UINT16 DataS1;
	UINT32 DataL1;
	UINT32 DataL2;
} STRM_CTRL_CMD;

typedef struct pwm_channel
{
	UINT8 Command;
	UINT32 SmplStart;
	UINT32 SmplEnd;
	UINT32 SmplLoop;
	UINT32 CmdCount;
	UINT32 CmdEnd;
	UINT32 CmdLoop;
	UINT32 Frequency;
} PWM_CHN;


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

UINT32 WriteAlloc;
UINT32 WriteCount;
DATA_WRITE* VGMWrite;
DATA_STREAM VGMStream;
UINT32 CtrlCmdCount;
STRM_CTRL_CMD VGMCtrlCmd[0x10];

bool JustTimerCmds;

int main(int argc, char* argv[])
{
	int argbase;
	int ErrVal;
	char FileName[0x100];
	
	printf("VGM PWM Optimizer\n-----------------\n\n");
	
	ErrVal = 0;
	JustTimerCmds = false;
	argbase = 0x01;
	
	printf("File Name:\t");
	if (argc <= argbase + 0x00)
	{
		gets_s(FileName, sizeof(FileName));
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
	
	EnumeratePWMWrite();
	DataReducer();
	MakeDataStream();
	
	free(VGMWrite);	VGMWrite = NULL;
	WriteAlloc = 0x00;	WriteCount = 0x00;
	
	RewriteVGMData();
	
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
	waitkey(argv[0]);
	
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

static void EnumeratePWMWrite(void)
{
	UINT8 Command;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
#ifdef WIN32
	UINT32 CmdTimer;
	char TempStr[0x80];
	char MinSecStr[0x80];
#endif
	UINT32 CmdLen;
	bool StopVGM;
	bool FirstAfterLoop[0x03];
	DATA_WRITE* TempWrt;
	
	VGMPos = VGMHead.lngDataOffset;
	VGMSmplPos = 0;
	
	WriteAlloc = VGMHead.lngDataOffset / 8;
	VGMWrite = (DATA_WRITE*)malloc(WriteAlloc * sizeof(DATA_WRITE));
	WriteCount = 0x00;
	
#ifdef WIN32
	CmdTimer = 0;
#endif
	StopVGM = false;
	for (TempByt = 0x00; TempByt < 0x03; TempByt ++)
		FirstAfterLoop[TempByt] = false;
	while(VGMPos < VGMHead.lngEOFOffset)
	{
		CmdLen = 0x00;
		Command = VGMData[VGMPos + 0x00];
		if (VGMPos == VGMHead.lngLoopOffset)
		{
			for (TempByt = 0x00; TempByt < 0x03; TempByt ++)
				FirstAfterLoop[TempByt] = true;
		}
		
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
				memcpy(&TempSht, &VGMData[VGMPos + 0x01], 0x02);
				VGMSmplPos += TempSht;
				CmdLen = 0x03;
				break;
			case 0x50:	// SN76496 write
				CmdLen = 0x02;
				break;
			case 0xB2:	// PWM write
				TempByt = VGMData[VGMPos + 0x01] >> 4;
				// Channel Write: 0x02 - Left, 0x03 - Right, 0x04 - Both
				if (TempByt >= 0x02 && TempByt <= 0x04)
				{
					if (WriteAlloc < WriteCount + 0x01)
					{
						WriteAlloc += 0x1000;
						VGMWrite = (DATA_WRITE*)realloc(VGMWrite,
														WriteAlloc * sizeof(DATA_WRITE));
					}
					TempWrt = &VGMWrite[WriteCount];
					WriteCount ++;
					
					TempByt -= 0x02;
					TempWrt->Port = TempByt;
					if (FirstAfterLoop[TempByt])
					{
						TempWrt->Port |= 0x80;
						FirstAfterLoop[TempByt] = false;
					}
					TempWrt->Value = ((VGMData[VGMPos + 0x01] & 0x0F) << 8) |
									(VGMData[VGMPos + 0x02] << 0);
					TempWrt->SmplPos = VGMSmplPos;
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
	printf("\t\t\t\t\t\t\t\t\r");
	
	printf("%u PWM writes found.\n", WriteCount);
	
	return;
}

static UINT32 gcd(UINT32 x, UINT32 y)
{
	UINT32 shift;
	UINT32 diff;
	
	// Thanks to Wikipedia for this algorithm
	// http://en.wikipedia.org/wiki/Binary_GCD_algorithm
	if (! x || ! y)
		return x | y;
	
	for (shift = 0; ((x | y) & 1) == 0; shift ++)
	{
		x >>= 1;
		y >>= 1;
	}
	
	while((x & 1) == 0)
		x >>= 1;
	
	do
	{
		while((y & 1) == 0)
			y >>= 1;
		
		if (x < y)
		{
			y -= x;
		}
		else
		{
			diff = x - y;
			x = y;
			y = diff;
		}
		y >>= 1;
	} while(y);
	
	return x << shift;
}

static void DataReducer(void)
{
	UINT32 MinSmplUsg;
	UINT32 SmplGCD;
	UINT16 LstSmpl;
	UINT32 SmplRep;
	UINT32 CurWrt;
	UINT32 SrcWrt;
	bool LoopWaiting;
	
	if (! WriteCount)
		return;
	//if (VGMWrite[0].Port != VGMWrite[1].Port)
	//	return;
	
	MinSmplUsg = WriteCount;
	LstSmpl = 0xFFFF;
	for (CurWrt = 0x00; CurWrt < WriteCount; CurWrt ++)
	{
		if (LstSmpl != VGMWrite[CurWrt].Value)
		{
			LstSmpl = VGMWrite[CurWrt].Value;
			if (SmplRep == 0x01)
				return;
			if (SmplRep < MinSmplUsg)
				MinSmplUsg = SmplRep;
			SmplRep = 0x00;
		}
		SmplRep ++;
	}
	if (SmplRep < MinSmplUsg)
		MinSmplUsg = SmplRep;
	
	LstSmpl = 0xFFFF;
	SmplGCD = MinSmplUsg;
	for (CurWrt = 0x00; CurWrt < WriteCount; CurWrt ++)
	{
		if (LstSmpl != VGMWrite[CurWrt].Value)
		{
			LstSmpl = VGMWrite[CurWrt].Value;
			SmplGCD = gcd(SmplRep, SmplGCD);
			if (SmplGCD == 0x01)
				return;
			SmplRep = 0x00;
		}
		SmplRep ++;
	}
	
	printf("Data Reduction x%u ...", SmplGCD);
	
	// Eliminate multiple samples
	LoopWaiting = false;
	if (VGMWrite[0].Port != VGMWrite[1].Port)
		VGMWrite[0].Port |= VGMWrite[1].Port << 4;
	for (CurWrt = 0x00, SrcWrt = 0x00; SrcWrt < WriteCount; CurWrt ++)
	{
		VGMWrite[CurWrt] = VGMWrite[SrcWrt];
		if (LoopWaiting)
		{
			VGMWrite[CurWrt].Port |= 0x80;
			LoopWaiting = false;
		}
		for (SmplRep = 0x00; SmplRep < SmplGCD; SmplRep ++, SrcWrt ++)
		{
			if (VGMWrite[SrcWrt].Port & 0x80)
				LoopWaiting = true;
		}
	}
	WriteCount = CurWrt;
	printf("  Done.\n");
	
	return;
}

static void MakeDataStream(void)
{
	const char* CHN_STR[0x03] = {"Left", "Right", "Both"};
	UINT32 CurWrt;
	UINT32 WrtCount;
	DATA_WRITE* TempWrt;
	STRM_CTRL_CMD* TempCmd;
	UINT16* DataStr;
	UINT64 CmdDiff;
	UINT32 SmplDiff;
	UINT8 ChnCnt;
	UINT8 CurChn;
	PWM_CHN PWMChn[0x02];
	PWM_CHN* TempChn;
	UINT8 TempByt;
	
	VGMStream.Type = 0x03;	// PWM Data Block
	VGMStream.Length = WriteCount * 0x02;	// 2 Bytes per Write
	VGMStream.Data = malloc(VGMStream.Length);
	DataStr = (UINT16*)VGMStream.Data;
	
	for (CurChn = 0x00; CurChn < 0x02; CurChn ++)
	{
		TempChn = &PWMChn[CurChn];
		TempChn->Command = 0xFF;
		TempChn->SmplStart = 0x00000000;
		TempChn->SmplEnd = 0x00000000;
		TempChn->SmplLoop = 0xFFFFFFFF;
		TempChn->CmdCount = 0x00;
		TempChn->CmdEnd = 0xFFFFFFFF;
		TempChn->CmdLoop = 0xFFFFFFFF;
		TempChn->Frequency = 0x00000000;
	}
	WrtCount = 0x00;
	
	for (CurWrt = 0x00; CurWrt < WriteCount; CurWrt ++)
	{
		TempWrt = &VGMWrite[CurWrt];
		TempByt = TempWrt->Port & 0x03;
		for (CurChn = 0x00; CurChn < 0x02; CurChn ++)
		{
			TempChn = &PWMChn[CurChn];
			if (TempChn->Command == TempByt)
			{
				break;
			}
			else if (TempChn->Command == 0xFF)
			{
				TempChn->Command = TempByt;
				TempChn->SmplStart = TempWrt->SmplPos;
				break;
			}
		}
		if (CurChn >= 0x02)
			continue;
		
		if (TempWrt->Port & 0x80)
		{
			TempChn->SmplLoop = TempWrt->SmplPos;
			TempChn->CmdLoop = WrtCount;
		}
		DataStr[WrtCount] = TempWrt->Value;
		WrtCount ++;
		TempChn->CmdCount ++;
		if (TempWrt->SmplPos > TempChn->SmplEnd)
		{
			TempChn->SmplEnd = TempWrt->SmplPos;
			TempChn->CmdEnd = TempChn->CmdCount;
		}
	}
	if (VGMWrite[0x00].Port & 0x30)
	{
		CurChn = (VGMWrite[0x00].Port & 0x30) >> 4;
		TempByt = (VGMWrite[0x00].Port & 0x03) >> 0;
		PWMChn[CurChn] = PWMChn[TempByt];
		PWMChn[CurChn].Command = CurChn;
		if (PWMChn[CurChn].CmdLoop != 0xFFFFFFFF)
			PWMChn[CurChn].CmdLoop ++;
	}
	
	ChnCnt = 0x00;
	for (CurChn = 0x00; CurChn < 0x02; CurChn ++)
	{
		if (PWMChn[CurChn].Command == 0xFF)
			break;
		ChnCnt ++;
	}
	
	for (CurChn = 0x00; CurChn < ChnCnt; CurChn ++)
	{
		//Frequency = CommandCount / (SampleCount / 44100.0);
		TempChn = &PWMChn[CurChn];
		
		SmplDiff = TempChn->SmplEnd - TempChn->SmplStart;
		CmdDiff = TempChn->CmdCount;
		CmdDiff = 44100 * CmdDiff + SmplDiff / 2;
		TempChn->Frequency = (UINT32)(CmdDiff / SmplDiff);
		printf("PWM Frequency %s Chn: %u\n", CHN_STR[TempChn->Command], TempChn->Frequency);
		
		if (TempChn->SmplLoop < TempChn->SmplStart)
		{
			// this case IS possible (e.g. loop at 0.5s, stream start at 1.0s)
			TempChn->SmplLoop = 0xFFFFFFFF;
			TempChn->CmdLoop = 0xFFFFFFFF;
		}
	}
	
	CtrlCmdCount = 0x00;
	
	for (CurChn = 0x00; CurChn < ChnCnt; CurChn ++)
	{
		TempCmd = &VGMCtrlCmd[CtrlCmdCount];
		CtrlCmdCount ++;
		TempCmd->Command = 0x90;	// Setup Stream 00
		TempCmd->StreamID = CurChn;
		TempCmd->DataB1 = 0x11;	// 0x11 - PWM chip
		TempCmd->DataB2 = 0x00;
		TempCmd->DataB3 = 0x02 + PWMChn[CurChn].Command;
		
		TempCmd = &VGMCtrlCmd[CtrlCmdCount];
		CtrlCmdCount ++;
		TempCmd->Command = 0x91;	// Set Stream 00 Data
		TempCmd->StreamID = CurChn;
		TempCmd->DataB1 = VGMStream.Type;
		if (VGMWrite[0x00].Port & 0x30)
		{
			TempCmd->DataB2 = 0x01;	// Step Size
			TempCmd->DataB3 = 0x00;	// Step Base
		}
		else
		{
			TempCmd->DataB2 = ChnCnt;	// Step Size
			TempCmd->DataB3 = CurChn;	// Step Base
		}
	}
	
	for (CurChn = 0x00; CurChn < ChnCnt; CurChn ++)
	{
		TempCmd = &VGMCtrlCmd[CtrlCmdCount];
		CtrlCmdCount ++;
		TempCmd->Command = 0x92;	// Setup Stream Frequency (and Fast-Play Value)
		TempCmd->StreamID = CurChn;
		TempCmd->DataL1 = PWMChn[CurChn].Frequency;
		TempCmd->DataS1 = 0x0000;	// Block ID
		TempCmd->DataB1 = 0x00;		// No looping
	}
	
	for (CurChn = 0x00; CurChn < ChnCnt; CurChn ++)
	{
		if (PWMChn[CurChn].CmdLoop == 0xFFFFFFFF)
			continue;
		
		TempCmd = &VGMCtrlCmd[CtrlCmdCount];
		CtrlCmdCount ++;
		TempCmd->Command = 0x93;	// Play Stream (long call for loop reposition)
		TempCmd->StreamID = CurChn;
		TempCmd->DataL1 = (PWMChn[CurChn].CmdLoop - CurChn) * 0x02;
		TempCmd->DataB1 = 0x03;		// play until end
		TempCmd->DataL2 = 0x00;
	}
	
	return;
}

static void RewriteVGMData(void)
{
	UINT32 DstPos;
	UINT8 Command;
	UINT32 CmdDelay;
	UINT32 AllDelay;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	STRM_CTRL_CMD* TempCmd;
#ifdef WIN32
	UINT32 CmdTimer;
	char TempStr[0x80];
	char MinSecStr[0x80];
#endif
	UINT32 CmdLen;
	bool StopVGM;
	bool WriteEvent;
	bool WriteExtra;
	UINT32 NewLoopS;
	UINT32 StrmCmd;
	bool ChnWritten[0x03];
	bool FirstAfterLoop[0x03];
#ifdef EXTRA_SYNC
	UINT32 SyncCnt[0x03];
	UINT32 DataPos[0x03];
	UINT32 PWMFreq;
	UINT32 PWMStep;
#endif
	
	DstData = (UINT8*)malloc(VGMDataLen + 0x100);
	AllDelay = 0;
	VGMPos = VGMHead.lngDataOffset;
	DstPos = VGMHead.lngDataOffset;
	VGMSmplPos = 0;
	NewLoopS = 0x00;
	memcpy(DstData, VGMData, VGMPos);	// Copy Header
	for (TempByt = 0x00; TempByt < 0x03; TempByt ++)
	{
		ChnWritten[TempByt] = false;
		FirstAfterLoop[TempByt] = false;
#ifdef EXTRA_SYNC
		SyncCnt[TempByt] = 0x00;
		DataPos[TempByt] = 0x00;
#endif
	}
	
#ifdef WIN32
	CmdTimer = 0;
#endif
	StopVGM = false;
	
	DstData[DstPos + 0x00] = 0x67;
	DstData[DstPos + 0x01] = 0x66;
	DstData[DstPos + 0x02] = VGMStream.Type;
	memcpy(&DstData[DstPos + 0x03], &VGMStream.Length, 0x04);
	memcpy(&DstData[DstPos + 0x07], VGMStream.Data, VGMStream.Length);
	DstPos += 0x07 + VGMStream.Length;
	
	for (StrmCmd = 0x00; StrmCmd < CtrlCmdCount; StrmCmd ++)
	{
		TempCmd = &VGMCtrlCmd[StrmCmd];
		if (TempCmd->Command >= 0x92)
			break;
		
		DstData[DstPos + 0x00] = TempCmd->Command;
		DstData[DstPos + 0x01] = TempCmd->StreamID;
		switch(TempCmd->Command)
		{
		case 0x90:
			DstData[DstPos + 0x02] = TempCmd->DataB1;
			DstData[DstPos + 0x03] = TempCmd->DataB2;
			DstData[DstPos + 0x04] = TempCmd->DataB3;
			DstPos += 0x05;
			break;
		case 0x91:
			DstData[DstPos + 0x02] = TempCmd->DataB1;
			DstData[DstPos + 0x03] = TempCmd->DataB2;
#ifdef EXTRA_SYNC
			PWMStep = TempCmd->DataB2 * 2;
#endif
			DstData[DstPos + 0x04] = TempCmd->DataB3;
			DstPos += 0x05;
			break;
		}
	}
	
#ifdef EXTRA_SYNC
	PWMFreq = 0xFFFFFFFF;
#endif
	while(VGMPos < VGMHead.lngEOFOffset)
	{
		CmdDelay = 0;
		CmdLen = 0x00;
		Command = VGMData[VGMPos + 0x00];
		WriteEvent = true;
		WriteExtra = false;
		if (VGMPos == VGMHead.lngLoopOffset)
		{
			for (TempByt = 0x00; TempByt < 0x03; TempByt ++)
				FirstAfterLoop[TempByt] = true;
			WriteExtra = true;
		}
		
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
			case 0xB2:	// PWM write
				TempByt = VGMData[VGMPos + 0x01] >> 4;
				// Channel Write: 0x02 - Left, 0x03 - Right, 0x04 - Both
				if (TempByt >= 0x02 && TempByt <= 0x04)
				{
					TempByt -= 0x02;
					
					if (! ChnWritten[TempByt] || FirstAfterLoop[TempByt])
						WriteExtra = true;
#ifdef EXTRA_SYNC
					if (! (SyncCnt[TempByt] % PWMFreq))
						WriteExtra = true;
#endif
					WriteEvent = false;
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
		
		if (WriteEvent || WriteExtra)
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
		
		if (Command == 0xB2)
		{
			TempByt = VGMData[VGMPos + 0x01] >> 4;
			// Channel Write: 0x02 - Left, 0x03 - Right, 0x04 - Both
			if (TempByt >= 0x02 && TempByt <= 0x04)
			{
				TempByt -= 0x02;
				
				if (! ChnWritten[TempByt])
				{
					TempCmd = &VGMCtrlCmd[StrmCmd];
					DstData[DstPos + 0x00] = TempCmd->Command;	// is always 0x92
					DstData[DstPos + 0x01] = TempCmd->StreamID;
					memcpy(&DstData[DstPos + 0x02], &TempCmd->DataL1, 0x04);
#ifdef EXTRA_SYNC
					PWMFreq = TempCmd->DataL1;
#endif
					DstPos += 0x06;
					
					DstData[DstPos + 0x00] = 0x95;
					DstData[DstPos + 0x01] = TempCmd->StreamID;
					memcpy(&DstData[DstPos + 0x02], &TempCmd->DataS1, 0x02);
					DstData[DstPos + 0x04] = TempCmd->DataB1;
					DstPos += 0x05;
					StrmCmd ++;
					
					ChnWritten[TempByt] = true;
					if (FirstAfterLoop[TempByt])
					{
						StrmCmd ++;
						FirstAfterLoop[TempByt] = false;	// MAY be possible
					}
				}
				else if (FirstAfterLoop[TempByt])
				{
					TempCmd = &VGMCtrlCmd[StrmCmd];
					DstData[DstPos + 0x00] = TempCmd->Command;	// is always 0x93
					DstData[DstPos + 0x01] = TempCmd->StreamID;
					memcpy(&DstData[DstPos + 0x02], &TempCmd->DataL1, 0x04);
					DstData[DstPos + 0x06] = TempCmd->DataB1;
					memcpy(&DstData[DstPos + 0x07], &TempCmd->DataL2, 0x04);
					DstPos += 0x0B;
					StrmCmd ++;
					
					FirstAfterLoop[TempByt] = false;
				}
#ifdef EXTRA_SYNC
				else if (! (SyncCnt[TempByt] % PWMFreq))
				{
					DstData[DstPos + 0x00] = 0x93;
					DstData[DstPos + 0x01] = TempByt % 2;
					memcpy(&DstData[DstPos + 0x02], &DataPos[TempByt], 0x04);
					DstData[DstPos + 0x06] = 0x00;
					TempLng = 0x00;
					memcpy(&DstData[DstPos + 0x07], &TempLng, 0x04);
					DstPos += 0x0B;
				}
				DataPos[TempByt] += PWMStep;
				SyncCnt[TempByt] ++;
#endif
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
	
	if (VGMHead.lngVersion < 0x00000160)
	{
		VGMHead.lngVersion = 0x00000160;
		memcpy(&DstData[0x08], &VGMHead.lngVersion, 0x04);
	}
	
	TempLng = DstDataLen - 0x04;
	memcpy(&DstData[0x04], &TempLng, 0x04);
	
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
