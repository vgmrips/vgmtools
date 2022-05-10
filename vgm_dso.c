// vgm_dso.c - VGM DAC Stream Optimizer
//

// TODO:
//	- optimize DAC databases (move at beginning of file, split into blocks-per-sample)
//	?- optimize Stream Wriites (omit re-initialization, move initialization to beginning)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "stdtype.h"
#include "stdbool.h"
#include "VGMFile.h"
#include "common.h"


/*typedef struct _vgm_pcm_bank_data
{
	UINT32 DataSize;
	UINT8* Data;
	UINT32 DataStart;
} VGM_PCM_DATA;
typedef struct _vgm_pcm_bank
{
	UINT32 BankCount;
	VGM_PCM_DATA* Bank;
	UINT32 DataSize;
	UINT8* Data;
	UINT32 DataPos;
} VGM_PCM_BANK;*/
typedef struct _sample_info
{
	UINT32 startOfs;
	UINT32 dataSize;
	UINT16 smplID;
} SAMPLE_INF;
typedef struct _sample_list
{
	UINT32 smplCnt;
	SAMPLE_INF* samples;
} SAMPLE_LIST;
typedef struct _stream_info
{
	UINT32 posCmd[3];
	UINT8 bankID;
} STREAM_INF;


static bool OpenVGMFile(const char* FileName);
static void WriteVGMFile(const char* FileName);
static void FindUsedROMData(void);
static void OptimizeVGMSampleROM(void);
static void WriteDataBlock(UINT32* DstPos, UINT8 blkType, UINT32 dataSize, const void* data);
static void AddPCMData(UINT8 Type, UINT32 DataSize, const UINT8* Data);
static void AddSampleToList(UINT8 Type, UINT32 startOfs, UINT32 length);
static int SampleCompare(const void* a, const void* b);
static void SortSampleLists(void);
INLINE UINT32 ReadLE32(const UINT8* Data);
INLINE void WriteLE16(const UINT8* Data, UINT16 value);
INLINE void WriteLE32(const UINT8* Data, UINT32 value);


VGM_HEADER VGMHead;
UINT32 VGMDataLen;
UINT8* VGMData;
UINT32 VGMPos;
INT32 VGMSmplPos;
UINT8* DstData;
UINT32 DstDataLen;
char FileBase[MAX_PATH];
bool CancelFlag;

#define PCM_BANK_COUNT	0x40
VGM_PCM_BANK PCMBank[PCM_BANK_COUNT];
SAMPLE_LIST Bank93[PCM_BANK_COUNT];
STREAM_INF StreamInfo[0x100];

int main(int argc, char* argv[])
{
	int argbase;
	int ErrVal;
	char FileName[MAX_PATH];
	UINT8 curBank;
	UINT8 smplOverlap;

	printf("VGM DAC Stream Optimizer\n------------------------\n\n");

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
		return 1;

	if (! OpenVGMFile(FileName))
	{
		printf("Error opening the file!\n");
		ErrVal = 1;
		goto EndProgram;
	}
	printf("\n");

	DstData = NULL;
	memset(PCMBank, 0x00, sizeof(VGM_PCM_BANK) * PCM_BANK_COUNT);
	memset(Bank93, 0x00, sizeof(SAMPLE_LIST) * PCM_BANK_COUNT);

	CancelFlag = false;
	FindUsedROMData();
	if (CancelFlag)
	{
		ErrVal = 9;
		goto BreakProgress;
	}
	SortSampleLists();
	{
		UINT32 curSmpl;
		SAMPLE_LIST* tempBnk;
		SAMPLE_INF* tempSmpl;

		printf("Data Block Usage\n---------------\n");
		for (curBank = 0x00; curBank < PCM_BANK_COUNT; curBank ++)
		{
			tempBnk = &Bank93[curBank];
			if (tempBnk->smplCnt == 0)
				continue;

			printf("Start   End     Size  -- Datablock Type 0x%02X\n", curBank);
			for (curSmpl = 0; curSmpl < tempBnk->smplCnt; curSmpl ++)
			{
				tempSmpl = &tempBnk->samples[curSmpl];
				printf("%06X  %06X  %04X\n", tempSmpl->startOfs, tempSmpl->startOfs + tempSmpl->dataSize - 1, tempSmpl->dataSize);
			}
		}
	}
	smplOverlap = 0;
	{
		UINT32 curSmpl;
		SAMPLE_LIST* tempBnk;
		SAMPLE_INF* tempSmpl;
		UINT32 smplPos;

		for (curBank = 0x00; curBank < PCM_BANK_COUNT; curBank ++)
		{
			tempBnk = &Bank93[curBank];

			smplPos = 0;
			for (curSmpl = 0; curSmpl < tempBnk->smplCnt; curSmpl ++)
			{
				tempSmpl = &tempBnk->samples[curSmpl];
				if (tempSmpl->startOfs < smplPos)
				{
					smplOverlap = 1;
					break;
				}
				smplPos = tempSmpl->startOfs + tempSmpl->dataSize;
			}
		}
	}
	if (smplOverlap)
	{
		printf("Overlapping samples! Unable to optimize.\n");
		ErrVal = 0;
		goto BreakProgress;
	}

	OptimizeVGMSampleROM();

	if (argc > argbase + 1)
		strncpy(FileName, argv[argbase + 1], MAX_PATH-1);
	else
		strcpy(FileName, "");
	if (FileName[0] == '\0')
	{
		snprintf(FileName, MAX_PATH, "%s_optimized.vgm", FileBase);
	}
	WriteVGMFile(FileName);

BreakProgress:
	free(VGMData);
	free(DstData);
	for (curBank = 0x00; curBank < PCM_BANK_COUNT; curBank ++)
	{
		free(PCMBank[curBank].Bank);
		free(PCMBank[curBank].Data);
		free(Bank93[curBank].samples);
	}

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

	hFile = fopen(FileName, "wb");
	fwrite(DstData, 0x01, DstDataLen, hFile);
	fclose(hFile);

	printf("File written.\n");

	return;
}

static void FindUsedROMData(void)
{
	UINT8 ChipID;
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
	const UINT8* VGMPnt;
	STREAM_INF* tempSInf;

	printf("Creating ROM-Usage Mask ...\n");
	VGMPos = VGMHead.lngDataOffset;

#ifdef WIN32
	CmdTimer = 0;
#endif
	memset(StreamInfo, 0x00, sizeof(STREAM_INF) * 0x100);

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
			case 0x50:	// SN76496 write
				CmdLen = 0x02;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMPnt[0x02];
				memcpy(&TempLng, &VGMPnt[0x03], 0x04);

				ChipID = (TempLng & 0x80000000) >> 31;
				TempLng &= 0x7FFFFFFF;

				switch(TempByt & 0xC0)
				{
				case 0x00:	// Database Block
					AddPCMData(TempByt, TempLng, &VGMPnt[0x07]);
					break;
				case 0x40:
					printf("Unable to optimize DAC streams that use compreseed data blocks!\n");
					StopVGM = true;
					CancelFlag = true;
					break;
				case 0x80:	// ROM/RAM Dump
				case 0xC0:	// RAM Write
					break;
				}
				CmdLen = 0x07 + TempLng;
				break;
			case 0x90:	// DAC Ctrl: Setup Chip
				tempSInf = &StreamInfo[VGMPnt[0x01]];
				if (tempSInf->posCmd[0] == 0)
					tempSInf->posCmd[0] = VGMPos;
				else if (tempSInf->posCmd[0] != (UINT32)-1)
				{
					if (memcmp(&VGMData[tempSInf->posCmd[0]], VGMPnt, 0x05))
						tempSInf->posCmd[0] = (UINT32)-1;
				}
				CmdLen = 0x05;
				break;
			case 0x91:	// DAC Ctrl: Set Data
				tempSInf = &StreamInfo[VGMPnt[0x01]];
				if (tempSInf->posCmd[1] == 0)
					tempSInf->posCmd[1] = VGMPos;
				else if (tempSInf->posCmd[1] != (UINT32)-1)
				{
					if (memcmp(&VGMData[tempSInf->posCmd[1]], VGMPnt, 0x05))
						tempSInf->posCmd[1] = (UINT32)-1;
				}
				tempSInf->bankID = VGMPnt[0x02];
				CmdLen = 0x05;
				break;
			case 0x92:	// DAC Ctrl: Set Freq
				tempSInf = &StreamInfo[VGMPnt[0x01]];
				if (tempSInf->posCmd[2] == 0)
					tempSInf->posCmd[2] = VGMPos;
				else if (tempSInf->posCmd[2] != (UINT32)-1)
				{
					if (memcmp(&VGMData[tempSInf->posCmd[2]], VGMPnt, 0x06))
						tempSInf->posCmd[2] = (UINT32)-1;
				}
				CmdLen = 0x06;
				break;
			case 0x93:	// DAC Ctrl: Play from Start Pos
				tempSInf = &StreamInfo[VGMPnt[0x01]];
				{
					UINT32 dStart;
					UINT32 dLen;

					dStart = ReadLE32(&VGMPnt[0x02]);
					dLen = ReadLE32(&VGMPnt[0x07]);
					AddSampleToList(tempSInf->bankID, dStart, dLen);
				}
				CmdLen = 0x0B;
				break;
			case 0x94:	// DAC Ctrl: Stop immediately
				CmdLen = 0x02;
				break;
			case 0x95:	// DAC Ctrl: Play Block (small)
				printf("Stream Command optimization already used.\n");
				StopVGM = true;
				CancelFlag = true;
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
			CmdLen = VGMHead.lngEOFOffset - VGMHead.lngDataOffset;
			printf("%04.3f %% - %s / %s (%08X / %08X) ...\r", (float)TempLng / CmdLen * 100,
					MinSecStr, TempStr, VGMPos, VGMHead.lngEOFOffset);
			CmdTimer = GetTickCount() + 200;
		}
#endif
	}
	printf("\t\t\t\t\t\t\t\t\r");

	return;
}

static void OptimizeVGMSampleROM(void)
{
	UINT32 DstPos;
	UINT8 ChipID;
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
	bool WriteEvent;
	const UINT8* VGMPnt;
	UINT32 NewLoopS;
	UINT8 curBank;
	UINT32 curSmpl;
	VGM_PCM_BANK* tempPCM;
	SAMPLE_LIST* tempBnk;
	SAMPLE_INF* tempSmpl;
	STREAM_INF* tempSInf;

	DstData = (UINT8*)malloc(VGMDataLen + 0x100);
	VGMPos = VGMHead.lngDataOffset;
	DstPos = VGMHead.lngDataOffset;
	NewLoopS = 0x00;
	memcpy(DstData, VGMData, VGMPos);	// Copy Header

#ifdef WIN32
	CmdTimer = 0;
#endif
	for (curBank = 0x00; curBank < PCM_BANK_COUNT; curBank ++)
	{
		tempPCM = &PCMBank[curBank];
		tempBnk = &Bank93[curBank];
		if (tempPCM->DataSize == 0 || tempBnk->smplCnt == 0)
			continue;

		TempLng = 0;
		TempSht = 0x0000;
		for (curSmpl = 0; curSmpl < tempBnk->smplCnt; curSmpl ++)
		{
			tempSmpl = &tempBnk->samples[curSmpl];

			if (TempLng < tempSmpl->startOfs)
			{
				WriteDataBlock(&DstPos, curBank, tempSmpl->startOfs - TempLng, &tempPCM->Data[TempLng]);
				TempSht ++;
			}

			tempSmpl->smplID = TempSht;
			WriteDataBlock(&DstPos, curBank, tempSmpl->dataSize, &tempPCM->Data[tempSmpl->startOfs]);
			TempSht ++;
			TempLng = tempSmpl->startOfs + tempSmpl->dataSize;
		}
		if (TempLng < tempPCM->DataSize)
			WriteDataBlock(&DstPos, curBank, tempPCM->DataSize - TempLng, &tempPCM->Data[TempLng]);
	}
	for (curBank = 0x00; curBank < 0xFF; curBank ++)
	{
		tempSInf = &StreamInfo[curBank];
		for (Command = 0x00; Command < 0x03; Command ++)
		{
			if (tempSInf->posCmd[Command] == (UINT32)-1)
				tempSInf->posCmd[Command] = 0x00;
		}
		if (! tempSInf->posCmd[0x00])
		{
			// prevent cmd 91/92 before cmd 90
			tempSInf->posCmd[0x01] = 0x00;
			tempSInf->posCmd[0x02] = 0x00;
		}

		for (Command = 0x00; Command < 0x03; Command ++)
		{
			if (! tempSInf->posCmd[Command])
				continue;

			CmdLen = (Command == 0x02) ? 0x06 : 0x05;
			printf("Copying data from for command %02X 0x%06X -> 0x%06X\n", 0x90 + Command, tempSInf->posCmd[Command], DstPos);
			memcpy(&DstData[DstPos], &VGMData[tempSInf->posCmd[Command]], CmdLen);
			DstPos += CmdLen;
		}
	}

	StopVGM = false;
	while(VGMPos < VGMHead.lngEOFOffset)
	{
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
			case 0x50:	// SN76496 write
				CmdLen = 0x02;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMPnt[0x02];
				memcpy(&TempLng, &VGMPnt[0x03], 0x04);

				ChipID = (TempLng & 0x80000000) >> 31;
				TempLng &= 0x7FFFFFFF;

				switch(TempByt & 0xC0)
				{
				case 0x00:	// Database Block
				case 0x40:
					WriteEvent = false;
					break;
				case 0x80:	// ROM/RAM Dump
				case 0xC0:	// RAM Write
					break;
				}
				CmdLen = 0x07 + TempLng;
				break;
			case 0xE0:	// Seek to PCM Data Bank Pos
				CmdLen = 0x05;
				break;
			case 0x4F:	// GG Stereo
				CmdLen = 0x02;
				break;
			case 0x68:	// PCM RAM write
				CmdLen = 0x0C;
				break;
			case 0x90:	// DAC Ctrl: Setup Chip
				tempSInf = &StreamInfo[VGMPnt[0x01]];
				if (tempSInf->posCmd[0] && ! memcmp(&VGMData[tempSInf->posCmd[0]], VGMPnt, 0x05))
					WriteEvent = false;
				CmdLen = 0x05;
				break;
			case 0x91:	// DAC Ctrl: Set Data
				tempSInf = &StreamInfo[VGMPnt[0x01]];
				if (tempSInf->posCmd[1] && ! memcmp(&VGMData[tempSInf->posCmd[1]], VGMPnt, 0x05))
					WriteEvent = false;
				tempSInf->bankID = VGMPnt[0x02];
				CmdLen = 0x05;
				break;
			case 0x92:	// DAC Ctrl: Set Freq
				tempSInf = &StreamInfo[VGMPnt[0x01]];
				if (tempSInf->posCmd[2] && ! memcmp(&VGMData[tempSInf->posCmd[2]], VGMPnt, 0x06))
					WriteEvent = false;
				CmdLen = 0x06;
				break;
			case 0x93:	// DAC Ctrl: Play from Start Pos
				tempSInf = &StreamInfo[VGMPnt[0x01]];
				{
					UINT32 dStart;
					UINT32 dLen;

					tempBnk = &Bank93[tempSInf->bankID];
					dStart = ReadLE32(&VGMPnt[0x02]);
					dLen = ReadLE32(&VGMPnt[0x07]);
					for (curSmpl = 0; curSmpl < tempBnk->smplCnt; curSmpl ++)
					{
						tempSmpl = &tempBnk->samples[curSmpl];
						if (tempSmpl->startOfs == dStart && tempSmpl->dataSize == dLen)
						{
							// write Play Block command
							DstData[DstPos + 0x00] = 0x95;
							DstData[DstPos + 0x01] = VGMPnt[0x01];
							WriteLE16(&DstData[DstPos + 0x02], tempSmpl->smplID);
							DstData[DstPos + 0x04] = (VGMPnt[0x06] & 0x70) | ((VGMPnt[0x06] & 0x80) >> 7);
							DstPos += 0x05;
							WriteEvent = false;
						}
					}
				}
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
					CmdLen = 0x02;
					break;
				case 0x40:
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

		// Note: In the case that the loop offset points to a Data Block,
		//       it gets moved to the first command after it.
		if (VGMPos == VGMHead.lngLoopOffset)
			NewLoopS = DstPos;
		if (WriteEvent)
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
	if (! NewLoopS && VGMHead.lngLoopOffset)
		printf("Warning! Failed to relocate loop!\n");
	if (VGMHead.lngLoopOffset)
	{
		TempLng = NewLoopS - 0x1C;
		memcpy(&DstData[0x1C], &TempLng, 0x04);
	}
	printf("\t\t\t\t\t\t\t\t\r");

	if (VGMHead.lngGD3Offset && VGMHead.lngGD3Offset + 0x0B < VGMHead.lngEOFOffset)
	{
		VGMPos = VGMHead.lngGD3Offset;
		memcpy(&TempLng, &VGMData[VGMPos + 0x00], 0x04);
		if (TempLng == FCC_GD3)
		{
			memcpy(&CmdLen, &VGMData[VGMPos + 0x08], 0x04);
			CmdLen += 0x0C;

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

static void WriteDataBlock(UINT32* DstPos, UINT8 blkType, UINT32 dataSize, const void* data)
{
	DstData[*DstPos + 0x00] = 0x67;
	DstData[*DstPos + 0x01] = 0x66;
	DstData[*DstPos + 0x02] = blkType;
	WriteLE32(&DstData[*DstPos + 0x03], dataSize);
	memcpy(&DstData[*DstPos + 0x07], data, dataSize);
	(*DstPos) += 0x07 + dataSize;

	return;
}

static void AddPCMData(UINT8 Type, UINT32 DataSize, const UINT8* Data)
{
	UINT32 CurBnk;
	VGM_PCM_BANK* tempPCM;
	VGM_PCM_DATA* tempBnk;
	UINT8 BnkType;

	BnkType = Type & 0x3F;
	if (BnkType >= PCM_BANK_COUNT)
		return;
	if (Type == 0x7F)
		return;

	tempPCM = &PCMBank[BnkType];
	CurBnk = tempPCM->BankCount;
	tempPCM->BankCount ++;
	tempPCM->Bank = (VGM_PCM_DATA*)realloc(tempPCM->Bank, sizeof(VGM_PCM_DATA) * tempPCM->BankCount);

	tempPCM->Data = realloc(tempPCM->Data, tempPCM->DataSize + DataSize);
	tempBnk = &tempPCM->Bank[CurBnk];
	tempBnk->DataStart = tempPCM->DataSize;

	tempBnk->DataSize = DataSize;
	tempBnk->Data = tempPCM->Data + tempBnk->DataStart;
	memcpy(tempBnk->Data, Data, DataSize);
	tempPCM->DataSize += DataSize;

	return;
}

static void AddSampleToList(UINT8 Type, UINT32 startOfs, UINT32 length)
{
	UINT8 BnkType;
	UINT32 curSmpl;
	SAMPLE_LIST* tempBnk;
	SAMPLE_INF* tempSmpl;

	BnkType = Type & 0x3F;
	if (BnkType >= PCM_BANK_COUNT)
		return;

	tempBnk = &Bank93[BnkType];
	for (curSmpl = 0; curSmpl < tempBnk->smplCnt; curSmpl ++)
	{
		tempSmpl = &tempBnk->samples[curSmpl];
		if (tempSmpl->startOfs == startOfs && tempSmpl->dataSize == length)
			return;
	}
	curSmpl = tempBnk->smplCnt;
	tempBnk->smplCnt ++;
	tempBnk->samples = (SAMPLE_INF*)realloc(tempBnk->samples, sizeof(SAMPLE_INF) * tempBnk->smplCnt);

	tempSmpl = &tempBnk->samples[curSmpl];
	tempSmpl->startOfs = startOfs;
	tempSmpl->dataSize = length;

	return;
}

static int SampleCompare(const void* a, const void* b)
{
	return (((SAMPLE_INF*)a)->startOfs - ((SAMPLE_INF*)b)->startOfs);
}

static void SortSampleLists(void)
{
	UINT8 curBank;
	SAMPLE_LIST* tempBnk;

	for (curBank = 0x00; curBank < PCM_BANK_COUNT; curBank ++)
	{
		tempBnk = &Bank93[curBank];
		qsort(tempBnk->samples, tempBnk->smplCnt, sizeof(SAMPLE_INF), &SampleCompare);
	}

	return;
}

INLINE UINT32 ReadLE32(const UINT8* Data)
{
	return *(UINT32*)Data;
}

INLINE void WriteLE16(const UINT8* Data, UINT16 value)
{
	*(UINT16*)Data = value;
	return;
}

INLINE void WriteLE32(const UINT8* Data, UINT32 value)
{
	*(UINT32*)Data = value;
	return;
}
