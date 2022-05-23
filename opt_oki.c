// opt_oki.c - VGM OKIM6258 Optimizer
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "stdtype.h"
#include "stdbool.h"
#include "VGMFile.h"
#include "common.h"


// Structures
typedef struct vgm_dma_action
{
	UINT32 posStart;
	UINT32 posEnd;
	UINT32 dmaStartOfs;
	UINT32 dmaLength;
	UINT32 length;	// actual length
	UINT32 dalloc;	// data allocation size
	UINT8* data;
	UINT8 chipID;
	UINT16 drumID;
} DMA_ACTION;
typedef struct vgm_drum_info
{
	UINT32 id;
	UINT32 length;
	UINT8* data;
	UINT32 sortKey;
	UINT32 playCount;
} DRUM_INF;
typedef struct vgm_drum_table
{
	UINT32 DrmCount;
	UINT32 DrmAlloc;
	DRUM_INF* Drums;
} DRUM_TABLE;

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


// Function Prototypes
static bool OpenVGMFile(const char* FileName);
static void WriteVGMFile(const char* FileName);
static void EnumerateOkiWrites(void);
static void GenerateDrumTable(void);
static void WriteDACStreamCmd(const STRM_CTRL_CMD* StrmCmd, UINT8* DstData, UINT32* DestPos);
static UINT32 GetOkiStreamRate(UINT32 clock, UINT8 div);
static void RewriteVGMData(void);
static UINT16 ReadBE16(const UINT8* data);
static UINT32 ReadBE32(const UINT8* data);
static UINT32 ReadLE32(const UINT8* data);
static void WriteLE32(UINT8* buffer, UINT32 value);


// Variables
VGM_HEADER VGMHead;
UINT32 VGMLoopSmplOfs;
UINT32 VGMDataLen;
UINT8* VGMData;
UINT32 VGMPos;
INT32 VGMSmplPos;
UINT8* DstData;
UINT32 DstDataLen;
char FileBase[0x100];
UINT32 DataSizeA;
UINT32 DataSizeB;

UINT8 ChipCnt;
UINT32 DmaActAlloc;
UINT32 DmaActCount;
DMA_ACTION* DmaActions;
DRUM_TABLE DrumTbl;
UINT8 OkiActionMask[2];	// [0] = 1st chip, [1] = 2nd chip
UINT32 StreamEventCount[2];	// total event count, for memory preallocation, [0] = frequency changes, [1] = play commands

bool DumpDrums;

int main(int argc, char* argv[])
{
	int argbase;
	int ErrVal;
	char FileName[0x100];

	printf("VGM OKIM6258 Optimizer\n----------------------\n\n");

	DumpDrums = false;
	ErrVal = 0;
	argbase = 1;

	printf("File Name:\t");
	if (argc <= argbase + 0)
	{
		ReadFilename(FileName, 0x100);
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

	EnumerateOkiWrites();
	if (DmaActCount == 0)
	{
		printf("No OKIM6258 DMA streams found.\n");
		free(DmaActions);
		free(VGMData);
		goto EndProgram;
	}
	if (((OkiActionMask[0] | OkiActionMask[1]) & 0x02))
		printf("DMA start offset will be used to help sorting drums.\n");
	else
		printf("Drums will be sorted in order of occourrence.\n");
	GenerateDrumTable();
	RewriteVGMData();

	printf("Data Compression: %u -> %u (%.1f %%)\n",
			DataSizeA, DataSizeB, 100.0 * DataSizeB / (float)DataSizeA);

	if (DataSizeB < DataSizeA)
	{
		if (argc > argbase + 1)
			strcpy(FileName, argv[argbase + 1]);
		else
			strcpy(FileName, "");
		if (FileName[0] == '\0')
		{
			strcpy(FileName, FileBase);
			strcat(FileName, "_optimized.vgm");
		}
		WriteVGMFile(FileName);
	}

	free(DmaActions);
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
	if (hFile == NULL)
	{
		printf("Error writing %s!\n", FileName);
		return;
	}
	fwrite(DstData, 0x01, DstDataLen, hFile);
	fclose(hFile);

	printf("File written.\n");

	return;
}

static void EnumerateOkiWrites(void)
{
	UINT8 Command;
	UINT8 curChip;
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
	UINT8 curReg;
	UINT8 curData;
	UINT32 activeDmaID[2];
	UINT8 dmaState[2][8];
	UINT8 clkState[2][5];

	VGMLoopSmplOfs = VGMHead.lngTotalSamples - VGMHead.lngLoopSamples;
	VGMPos = VGMHead.lngDataOffset;
	VGMSmplPos = 0;

	ChipCnt = (VGMHead.lngHzOKIM6258 & 0x40000000) ? 2 : 1;
	DmaActions = NULL;
	DmaActAlloc = 0;
	DmaActCount = 0;
	for (curChip = 0; curChip < 2; curChip ++)
		activeDmaID[curChip] = (UINT32)-1;
	memset(dmaState, 0x00, sizeof(dmaState));

	memset(OkiActionMask, 0x00, sizeof(OkiActionMask));
	memset(StreamEventCount, 0x00, sizeof(StreamEventCount));
	for (curChip = 0; curChip < 2; curChip ++)
	{
		WriteLE32(&clkState[curChip][0], VGMHead.lngHzOKIM6258 & 0x3FFFFFFF);
		clkState[curChip][4] = VGMHead.bytOKI6258Flags & 0x03;
	}

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
			case 0xB7:	// OKIM6258 write
				curChip = (VGMData[VGMPos + 0x01] & 0x80) >> 7;
				curReg = VGMData[VGMPos + 0x01] & 0x7F;
				curData = VGMData[VGMPos + 0x02];
				CmdLen = 0x03;
				if (curChip >= ChipCnt)
					break;

				if (curReg == 0x01)
				{
					if (activeDmaID[curChip] != (UINT32)-1)
					{
						DMA_ACTION* dmaAct = &DmaActions[activeDmaID[curChip]];

						if (dmaAct->length >= dmaAct->dalloc)
						{
							dmaAct->dalloc += 0x10000;
							dmaAct->data = (UINT8*)realloc(dmaAct->data, dmaAct->dalloc);
						}
						dmaAct->data[dmaAct->length] = curData;
						dmaAct->length ++;
					}
				}
				else if (curReg >= 0x08 && curReg <= 0x0C)
				{
					if (curReg == 0x0B)
						curData &= 0x3F;
					if (curReg == 0x0B || curReg == 0x0C)
						OkiActionMask[curChip] |= 0x10;	// mark clock change
					if (curData != clkState[curChip][curReg & 0x07])
						printf("Warning! Clock change at 0x%06X\n", VGMPos);
					clkState[curChip][curReg & 0x07] = curData;
					StreamEventCount[0] ++;
				}
				else if (curReg >= 0x10 && curReg <= 0x17)
				{
					dmaState[curChip][curReg & 0x07] = curData;
				}

				if (curReg == 0x17)	// no "else" if is intended
				{
					UINT8 dmaMask = (1 << curChip);

					// Bit 7 (80): start (trigger)
					// Bit 6 (40): continue (trigger)
					// Bit 5 (20): halt (state)
					// Bit 4 (10): abort (trigger)
					// Bit 3 (08): interrupt (state)
					if ((curData & 0x10) && activeDmaID[curChip] != (UINT32)-1)	// DMA Stop
					{
						DMA_ACTION* dmaAct = &DmaActions[activeDmaID[curChip]];
						dmaAct->posEnd = VGMPos;
						if (dmaAct->dmaLength && dmaAct->length > dmaAct->dmaLength)
							printf("Warning: more OKI writes (%u) than set by DMA length (%u)\n",
									dmaAct->length > dmaAct->dmaLength);
						activeDmaID[curChip] = (UINT32)-1;
					}
					if (curData & 0x80)	// DMA Start
					{
						DMA_ACTION* dmaAct;
						UINT32 dmaID = DmaActCount;

						if (activeDmaID[curChip] != (UINT32)-1)
							DmaActions[activeDmaID[curChip]].posEnd = VGMPos;

						if (DmaActCount >= DmaActAlloc)
						{
							DmaActAlloc += 0x100;
							DmaActions = (DMA_ACTION*)realloc(DmaActions,
													DmaActAlloc * sizeof(DMA_ACTION));
						}

						dmaAct = &DmaActions[dmaID];
						dmaAct->chipID = curChip;
						dmaAct->posStart = VGMPos;
						dmaAct->posEnd = (UINT32)-1;
						{
							UINT8* dsData = dmaState[curChip];
							dmaAct->dmaStartOfs = ReadBE32(&dsData[0]);
							dmaAct->dmaLength = ReadBE16(&dsData[4]);
							if (dmaAct->dmaStartOfs)
								OkiActionMask[curChip] |= 0x02;
						}
						dmaAct->data = NULL;
						dmaAct->dalloc = dmaAct->length = 0x00;
						DmaActCount ++;
						activeDmaID[curChip] = dmaID;
						OkiActionMask[curChip] |= 0x01;	// uses ADPCM stream
						if (!(OkiActionMask[curChip] & 0x10))	// no clock change yet?
							OkiActionMask[curChip] |= 0x20;	// require explicit frequency setting at init
						StreamEventCount[1] ++;
					}
					else if (curData & 0x40)	// DMA continue
					{
						if (activeDmaID[curChip] != (UINT32)-1)
						{
							DMA_ACTION* dmaAct = &DmaActions[activeDmaID[curChip]];
							UINT8* dsData = dmaState[curChip];
							dmaAct->dmaLength += ReadBE16(&dsData[4]);
						}
					}
				}

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

	//printf("%u OKI writes found.\n", DmaActCount);

	return;
}

static int drum_sort_compare(const void* p1, const void* p2)
{
	DRUM_INF* Drm1 = (DRUM_INF*)p1;
	DRUM_INF* Drm2 = (DRUM_INF*)p2;

	if (Drm1->sortKey < Drm2->sortKey)
		return -1;
	else if (Drm1->sortKey > Drm2->sortKey)
		return +1;
	// sort key is equal - compare ID to ensure a stable sort with equal sort keys
	else if (Drm1->id < Drm2->id)
		return -1;
	else if (Drm1->id > Drm2->id)
		return +1;
	else
		return 0;
}

static void GenerateDrumTable(void)
{
	UINT32 curDA;
	UINT32 curDrm;
	UINT32* drmRevIDs;

	DrumTbl.DrmCount = 0x00;
	DrumTbl.DrmAlloc = 0x100;
	DrumTbl.Drums = (DRUM_INF*)malloc(DrumTbl.DrmAlloc * sizeof(DRUM_INF));

	for (curDA = 0; curDA < DmaActCount; curDA ++)
	{
		DMA_ACTION* dmaAct = &DmaActions[curDA];
		UINT32 drmIdFound = (UINT32)-1;
		UINT32 sortKey = dmaAct->dmaStartOfs;

		for (curDrm = 0; curDrm < DrumTbl.DrmCount; curDrm ++)
		{
			DRUM_INF* dInf = &DrumTbl.Drums[curDrm];
			UINT32 cmpLen = (dmaAct->length < dInf->length) ? dmaAct->length : dInf->length;
			if (! memcmp(dmaAct->data, dInf->data, cmpLen))
			{
				drmIdFound = curDrm;
				break;
			}
		}
		if (drmIdFound != (UINT32)-1)
		{
			DRUM_INF* dInf = &DrumTbl.Drums[drmIdFound];
			if (dmaAct->length > dInf->length)
			{
				dInf->length = dmaAct->length;
				dInf->data = dmaAct->data;
				dInf->sortKey = sortKey;
			}
		}
		else
		{
			DRUM_INF* dInf;
			drmIdFound = DrumTbl.DrmCount;
			if (DrumTbl.DrmCount >= DrumTbl.DrmAlloc)
			{
				DrumTbl.DrmAlloc += 0x100;
				DrumTbl.Drums = (DRUM_INF*)realloc(DrumTbl.Drums,
													DrumTbl.DrmAlloc * sizeof(DRUM_INF));
			}

			dInf = &DrumTbl.Drums[DrumTbl.DrmCount];
			DrumTbl.DrmCount ++;
			dInf->length = dmaAct->length;
			dInf->data = dmaAct->data;
			dInf->sortKey = sortKey;
			dInf->playCount = 0;
		}
		dmaAct->drumID = drmIdFound;
		DrumTbl.Drums[drmIdFound].playCount ++;
	}

	if (DumpDrums)
	{
		for (curDrm = 0; curDrm < DrumTbl.DrmCount; curDrm ++)
		{
			char DrumDump_Name[MAX_PATH + 0x10];
			FILE* hFile;
			DRUM_INF* dInf = &DrumTbl.Drums[curDrm];

			if (!dInf->sortKey)
				sprintf(DrumDump_Name, "%s_%03X.raw", FileBase, curDrm);
			else
				sprintf(DrumDump_Name, "%s_%03X_ofs-%06X.raw", FileBase, curDrm, dInf->sortKey);
			hFile = fopen(DrumDump_Name, "wb");
			if (hFile != NULL)
			{
				fwrite(dInf->data, 0x01, dInf->length, hFile);
				fclose(hFile);
			}
		}
	}

	// sort by sortKey
	for (curDrm = 0; curDrm < DrumTbl.DrmCount; curDrm ++)
		DrumTbl.Drums[curDrm].id = curDrm;
	qsort(DrumTbl.Drums, DrumTbl.DrmCount, sizeof(DRUM_INF), &drum_sort_compare);

	// and fix all the drum IDs in DMA actions
	drmRevIDs = (UINT32*)malloc(DrumTbl.DrmCount * sizeof(UINT32));
	for (curDrm = 0; curDrm < DrumTbl.DrmCount; curDrm ++)
		drmRevIDs[DrumTbl.Drums[curDrm].id] = curDrm;
	for (curDA = 0; curDA < DmaActCount; curDA ++)
	{
		DMA_ACTION* dmaAct = &DmaActions[curDA];
		if (dmaAct->drumID < DrumTbl.DrmCount)
			dmaAct->drumID = drmRevIDs[dmaAct->drumID];
	}
	free(drmRevIDs);

	return;
}

static void WriteDACStreamCmd(const STRM_CTRL_CMD* StrmCmd, UINT8* DstData, UINT32* DestPos)
{
	UINT32 DstPos;

	DstPos = *DestPos;
	DstData[DstPos + 0x00] = StrmCmd->Command;
	DstData[DstPos + 0x01] = StrmCmd->StreamID;
	switch(StrmCmd->Command)
	{
	case 0x90:
		DstData[DstPos + 0x02] = StrmCmd->DataB1;
		DstData[DstPos + 0x03] = StrmCmd->DataB2;
		DstData[DstPos + 0x04] = StrmCmd->DataB3;
		DstPos += 0x05;
		break;
	case 0x91:
		DstData[DstPos + 0x02] = StrmCmd->DataB1;
		DstData[DstPos + 0x03] = StrmCmd->DataB2;
		DstData[DstPos + 0x04] = StrmCmd->DataB3;
		DstPos += 0x05;
		break;
	case 0x92:
		memcpy(&DstData[DstPos + 0x02], &StrmCmd->DataL1, 0x04);
		DstPos += 0x06;
		break;
	case 0x94:
		DstPos += 0x02;
		break;
	case 0x95:
		memcpy(&DstData[DstPos + 0x02], &StrmCmd->DataS1, 0x02);
		DstData[DstPos + 0x04] = StrmCmd->DataB1;
		DstPos += 0x05;
		break;
	}
	*DestPos = DstPos;

	return;
}

static UINT32 GetOkiStreamRate(UINT32 clock, UINT8 div)
{
	static const UINT32 OKI_CLK_DIVS[4] = {1024, 768, 512, 512};
	UINT32 realDiv = OKI_CLK_DIVS[div & 0x03] * 2;	// *2 because ADPCM data rate = half the sample rate
	return (clock + realDiv / 2) / realDiv;
}

static void RewriteVGMData(void)
{
	UINT32 DstPos;
	UINT8 Command;
	UINT8 curChip;
	UINT32 CmdDelay;
	UINT32 AllDelay;
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
	bool WriteExtra;
	UINT32 NewLoopPos;
	UINT8 curReg;
	UINT8 curData;
	UINT32 nextDmaID;
	UINT32 activeDmaID[2];
	UINT8 clkState[2][5];
	UINT32 okiClock[2];
	UINT8 okiDivider[2];
	UINT32 okiFreq[2];
	UINT8 okiStrmState;

	// data to be appended after current VGM command
	UINT32 appendLen;
	UINT8 appendBuf[0x100];

	DstDataLen = VGMDataLen + 0x100;
	DstDataLen += StreamEventCount[0] * 0x06 + StreamEventCount[1] * 0x05;
	DstData = (UINT8*)malloc(DstDataLen);
	AllDelay = 0;
	VGMPos = VGMHead.lngDataOffset;
	DstPos = VGMHead.lngDataOffset;
	VGMSmplPos = 0;
	NewLoopPos = 0x00;
	memcpy(DstData, VGMData, VGMPos);	// Copy Header

#ifdef WIN32
	CmdTimer = 0;
#endif
	StopVGM = false;

	// write data blocks with drum samples (one block for each sample)
	for (TempLng = 0x00; TempLng < DrumTbl.DrmCount; TempLng ++)
	{
		DRUM_INF* TempDrm = &DrumTbl.Drums[TempLng];
		DstData[DstPos + 0x00] = 0x67;
		DstData[DstPos + 0x01] = 0x66;
		DstData[DstPos + 0x02] = 0x04;	// OKIM6258 Data Block
		memcpy(&DstData[DstPos + 0x03], &TempDrm->length, 0x04);
		memcpy(&DstData[DstPos + 0x07], TempDrm->data, TempDrm->length);
		DstPos += 0x07 + TempDrm->length;
	}

	for (curChip = 0; curChip < 2; curChip ++)
	{
		okiClock[curChip] = VGMHead.lngHzOKIM6258 & 0x3FFFFFFF;
		okiDivider[curChip] = VGMHead.bytOKI6258Flags & 0x03;
		WriteLE32(&clkState[curChip][0], okiClock[curChip]);
		clkState[curChip][4] = okiDivider[curChip];

		activeDmaID[curChip] = (UINT32)-1;
		okiFreq[curChip] = 0;
	}
	for (curChip = 0; curChip < ChipCnt; curChip ++)
	{
		STRM_CTRL_CMD scCmd;

		if (OkiActionMask[curChip] & 0x01)
		{
			scCmd.Command = 0x90;	// Setup Stream 00
			scCmd.StreamID = curChip;
			scCmd.DataB1 = 0x17 | (curChip << 7);	// 0x17 - OKIM6258 chip
			scCmd.DataB2 = 0x00;
			scCmd.DataB3 = 0x01;	// command 01: data write
			WriteDACStreamCmd(&scCmd, DstData, &DstPos);

			scCmd.Command = 0x91;	// Set Stream 00 Data
			scCmd.StreamID = curChip;
			scCmd.DataB1 = 0x04;	// Block Type 04: OKIM6258 Data Block
			scCmd.DataB2 = 0x01;	// Step Size
			scCmd.DataB3 = 0x00;	// Step Base
			WriteDACStreamCmd(&scCmd, DstData, &DstPos);
		}

		if (OkiActionMask[curChip] & 0x20)	// when frequency init is required
		{
			scCmd.Command = 0x92;	// Set Stream Frequency
			scCmd.StreamID = curChip;
			scCmd.DataL1 = GetOkiStreamRate(okiClock[curChip], okiDivider[curChip]);
			WriteDACStreamCmd(&scCmd, DstData, &DstPos);
		}
	}

	nextDmaID = 0;
	appendLen = 0;
	okiStrmState = 0x00;
	while(VGMPos < VGMHead.lngEOFOffset)
	{
		CmdDelay = 0;
		CmdLen = 0x00;
		Command = VGMData[VGMPos + 0x00];
		WriteEvent = true;
		WriteExtra = false;
		if (VGMPos == VGMHead.lngLoopOffset)
		{
			WriteExtra = true;
			for (curChip = 0; curChip < 2; curChip ++)
				okiFreq[curChip] = 0;	// enforce refresh
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
			case 0xB7:	// OKIM6258 write
				curChip = (VGMData[VGMPos + 0x01] & 0x80) >> 7;
				curReg = VGMData[VGMPos + 0x01] & 0x7F;
				curData = VGMData[VGMPos + 0x02];
				CmdLen = 0x03;
				if (curChip >= ChipCnt)
					break;

				if (curReg == 0x01)
				{
					// do NOT write when within a DmaAction for this chip
					if (activeDmaID[curChip] != (UINT32)-1)
					{
						WriteEvent = false;
						if (okiStrmState & (0x10 << curChip))
						{
							DMA_ACTION* dmaAct = &DmaActions[activeDmaID[curChip]];
							STRM_CTRL_CMD scCmd;

							okiStrmState &= ~(0x10 << curChip);	// remove 'pending start' bit
							okiStrmState |= (0x01 << curChip);	// set 'stream running' bit

							scCmd.Command = 0x95;	// Quick-Play block
							scCmd.StreamID = curChip;
							scCmd.DataS1 = dmaAct->drumID;	// Block ID
							scCmd.DataB1 = 0x00;			// No looping
							WriteDACStreamCmd(&scCmd, appendBuf, &appendLen);
							WriteExtra = true;
						}
					}
				}
				else if (curReg >= 0x08 && curReg <= 0x0C)
				{
					if (curReg == 0x0B)
						curData &= 0x3F;
					clkState[curChip][curReg & 0x07] = curData;

					if (curReg == 0x0B || curReg == 0x0C)
					{
						UINT32 nextPos = VGMPos + CmdLen;
						STRM_CTRL_CMD scCmd;
						UINT32 newFreq;

						if (curReg == 0x0B)
						{
							// Master Clock Change
							okiClock[curChip] = ReadLE32(&clkState[curChip][0]);
							if (nextPos + 0x03 <= VGMHead.lngEOFOffset)
							{
								if (VGMData[nextPos + 0x00] == VGMData[VGMPos + 0x00] &&
									VGMData[nextPos + 0x01] == ((curChip << 7) | 0x0C))
									break;	// exit now - frequency will be set during next Clock Divider change
							}
						}
						else if (curReg == 0x0C)
						{
							// Clock Divider Change
							okiDivider[curChip] = curData;
							if (nextPos + 0x03 <= VGMHead.lngEOFOffset)
							{
								if (VGMData[nextPos + 0x00] == VGMData[VGMPos + 0x00] &&
									VGMData[nextPos + 0x01] == ((curChip << 7) | 0x08))
									break;	// exit now - frequency will be set during next Master Clock change
							}
						}

						newFreq = GetOkiStreamRate(okiClock[curChip], okiDivider[curChip]);
						if (newFreq == okiFreq[curChip])
							break;
						okiFreq[curChip] = newFreq;

						scCmd.Command = 0x92;	// Set Stream Frequency
						scCmd.StreamID = curChip;
						scCmd.DataL1 = okiFreq[curChip];
						WriteDACStreamCmd(&scCmd, appendBuf, &appendLen);
						WriteExtra = true;
					}
				}
				else if (curReg == 0x17)	// no "else" if is intended
				{
					UINT32 dmaID = activeDmaID[curChip];
					if (dmaID != (UINT32)-1 && VGMPos >= DmaActions[dmaID].posEnd)
					{
						// omit "Stop Stream" block when just restarting DMA
						if ((curData & 0x10) || !(curData & 0x80))	// if (dmaStop || !dmaStart)
						{
							if (okiStrmState & (0x01 << curChip))	// only stop when it was already running
							{
								STRM_CTRL_CMD scCmd;
								scCmd.Command = 0x94;	// Stop Stream block
								scCmd.StreamID = curChip;
								WriteDACStreamCmd(&scCmd, appendBuf, &appendLen);
							}
							okiStrmState &= ~(0x11 << curChip);
						}
						activeDmaID[curChip] = (UINT32)-1;
					}

					if (nextDmaID < DmaActCount && VGMPos >= DmaActions[nextDmaID].posStart)
					{
						activeDmaID[curChip] = nextDmaID;
						nextDmaID ++;

						// enqueue "stream start" command
						// The next 6258 data command will then start the stream.
						okiStrmState |= (0x10 << curChip);
					}

					if (curData & 0x40)
					{
						if (!(curData & 0x90))
							WriteEvent = false;	// remove all the "DMA continue" commands that are used by streaming drivers
					}
				}
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
				else if (TempSht <= 0x10)
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
				NewLoopPos = DstPos;

			if (WriteEvent)
			{
				memcpy(&DstData[DstPos], &VGMData[VGMPos], CmdLen);
				DstPos += CmdLen;
			}
			if (appendLen > 0)
			{
				memcpy(&DstData[DstPos], appendBuf, appendLen);
				DstPos += appendLen;
				appendLen = 0;
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
		VGMHead.lngLoopOffset = NewLoopPos;
		if (! NewLoopPos)
			printf("Error! Failed to relocate Loop Point!\n");
		else
			NewLoopPos -= 0x1C;
		memcpy(&DstData[0x1C], &NewLoopPos, 0x04);
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

static UINT16 ReadBE16(const UINT8* data)
{
	return	(data[0x00] <<  8) | (data[0x01] <<  0);
}

static UINT32 ReadBE32(const UINT8* data)
{
	return	(data[0x00] << 24) | (data[0x01] << 16) |
			(data[0x02] <<  8) | (data[0x03] <<  0);
}

static UINT32 ReadLE32(const UINT8* data)
{
	return	(data[0x00] <<  0) | (data[0x01] <<  8) |
			(data[0x02] << 16) | (data[0x03] << 24);
}

static void WriteLE32(UINT8* buffer, UINT32 value)
{
	buffer[0x00] = (value >>  0) & 0xFF;
	buffer[0x01] = (value >>  8) & 0xFF;
	buffer[0x02] = (value >> 16) & 0xFF;
	buffer[0x03] = (value >> 24) & 0xFF;

	return;
}
