// vgmmerge.c - VGM Merger
//
// TODO: remove redundand blocks when merging 2xDAC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "stdtype.h"
#include "stdbool.h"
#include "VGMFile.h"
#include "common.h"


typedef struct chip_mappings CHIP_MAPS;

static bool OpenVGMFile(const char* FileName, const UINT8 VgmNo);
static void WriteVGMFile(const char* FileName);
static void MergeVGMHeader(VGM_HEADER* DstHead, VGM_HEADER* SrcHead, CHIP_MAPS* SrcChpMap);
static void MergeVGMData(void);
INLINE UINT16 GetCmdLen(UINT8 Command);


// semi-constant
UINT16 MAX_VGMS = 0x02;
bool NO_DUAL_MIXING = false;


typedef struct chip_mapping_data
{
	bool MapToDual;
} CHIP_MAP;
struct chip_mappings
{
	CHIP_MAP SN76496;
	CHIP_MAP YM2413;
	CHIP_MAP YM2612;
	CHIP_MAP YM2151;
	CHIP_MAP SegaPCM;
	CHIP_MAP RF5C68;
	CHIP_MAP YM2203;
	CHIP_MAP YM2608;
	CHIP_MAP YM2610;
	CHIP_MAP YM3812;
	CHIP_MAP YM3526;
	CHIP_MAP Y8950;
	CHIP_MAP YMF262;
	CHIP_MAP YMF278B;
	CHIP_MAP YMF271;
	CHIP_MAP YMZ280B;
	CHIP_MAP RF5C164;
	CHIP_MAP PWM;
	CHIP_MAP AY8910;
	CHIP_MAP GBDMG;
	CHIP_MAP NESAPU;
	CHIP_MAP K007232;
	CHIP_MAP Default;
};	// CHIP_MAPS
typedef struct data_block_mapping
{
	UINT16 BlockCnt;
	UINT16 BlockAlloc;
	UINT16* BlkMap;
} DATABLK_MAP;
typedef struct vgm_info
{
	VGM_HEADER Head;
	CHIP_MAPS ChipMap;
	UINT8 StrmMap[0x100];
	UINT8 StrmDBlk[0x100];
	DATABLK_MAP DataBlk[0x40];
	UINT32 DataLen;
	UINT8* Data;
	UINT32 Pos;
	INT32 SmplPos;
} VGM_INF;


VGM_INF* SrcVGM;
UINT8* DstData;
UINT32 DstDataLen;
INT32 DstSmplPos;
char FileBase[0x100];

UINT8 ErrMsgs;

int main(int argc, char* argv[])
{
	int ErrVal;
	int argbase;
	char FileName[0x100];
	UINT8 FileNo;
	UINT32 TempLng;

	printf("VGM Merger\n----------\n\n");

	ErrVal = 0;
	argbase = 0;

	argbase = 1;
	while(argbase < argc && argv[argbase][0] == '-')
	{
		if (! strncmp(argv[argbase], "-f:", 3))
		{
			TempLng = strtoul(argv[argbase] + 3, NULL, 0);
			if (TempLng == 1)
			{
				printf("This doesn't make any sense.\n");
				goto EndProgram;
			}
			else if (TempLng > 0x100)
			{
				printf("Are you kidding me?? No way!\n");
				goto EndProgram;
			}
			if (TempLng)
				MAX_VGMS = (UINT16)TempLng;
			argbase ++;
		}
		else if (! strcmp(argv[argbase], "-nodual"))
		{
			NO_DUAL_MIXING = true;
			argbase ++;
		}
		else
		{
			break;
		}
	}
	SrcVGM = (VGM_INF*)malloc(MAX_VGMS * sizeof(VGM_INF));

	for (FileNo = 0x00; FileNo < MAX_VGMS; FileNo ++)
	{
		printf("File #%u:\t", FileNo + 1);
		if (argc <= argbase + FileNo)
		{
			ReadFilename(FileName, sizeof(FileName));
		}
		else
		{
			strcpy(FileName, argv[argbase + FileNo]);
			printf("%s\n", FileName);
		}
		if (! strlen(FileName))
			return 0;

		if (! OpenVGMFile(FileName, FileNo))
		{
			printf("Error opening the file!\n");
			ErrVal = 1;
			while(FileNo > 0x00)
			{
				free(SrcVGM[FileNo - 1].Data);
				FileNo --;
			}
			free(SrcVGM);
			goto EndProgram;
		}
	}
	printf("\n");

	MergeVGMData();

	if (argc > argbase + MAX_VGMS)
		strcpy(FileName, argv[argbase + MAX_VGMS]);
	else
		strcpy(FileName, "");
	if (FileName[0] == '\0')
	{
		strcpy(FileName, FileBase);
		strcat(FileName, "_merged.vgm");
	}
	WriteVGMFile(FileName);

	for (FileNo = 0x00; FileNo < MAX_VGMS; FileNo ++)
		free(SrcVGM[FileNo].Data);
	free(DstData);

EndProgram:
	DblClickWait(argv[0]);

	return ErrVal;
}

static bool OpenVGMFile(const char* FileName, const UINT8 VgmNo)
{
	gzFile hFile;
	UINT32 TempLng;
	VGM_HEADER VGMHead;
	UINT32 CurPos;
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
	if (VGMHead.lngVersion < 0x00000150)
		CurPos = 0x40;
	TempLng = sizeof(VGM_HEADER);
	if (TempLng > CurPos)
		TempLng -= CurPos;
	else
		TempLng = 0x00;
	memset((UINT8*)&VGMHead + CurPos, 0x00, TempLng);

	// Read Data
	SrcVGM[VgmNo].DataLen = VGMHead.lngEOFOffset;
	SrcVGM[VgmNo].Data = (UINT8*)malloc(SrcVGM[VgmNo].DataLen);
	if (SrcVGM[VgmNo].Data == NULL)
		goto OpenErr;
	gzseek(hFile, 0x00, SEEK_SET);
	gzread(hFile, SrcVGM[VgmNo].Data, SrcVGM[VgmNo].DataLen);
	SrcVGM[VgmNo].Head = VGMHead;

	gzclose(hFile);

	if (! VgmNo)
	{
		strcpy(FileBase, FileName);
		TempPnt = strrchr(FileBase, '.');
		if (TempPnt != NULL)
			*TempPnt = 0x00;
	}

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

static void MergeVGMHeader(VGM_HEADER* DstHead, VGM_HEADER* SrcHead, CHIP_MAPS* SrcChpMap)
{
	const char* CHIP_NAMES[] =
	{	"SN76496", "YM2413", "YM2612", "YM2151", "SegaPCM", "RF5C68", "YM2203", "YM2608",
		"YM2610", "YM3812", "YM3526", "Y8950", "YMF262", "YMF278B", "YMF271", "YMZ280B",
		"RF5C164", "PWM", "AY8910"};
	UINT8 CurChip;
	UINT32* SrcClock;
	UINT32* DstClock;
	bool DualSupport;
	CHIP_MAP* TempMap;

	if (DstHead->lngVersion != SrcHead->lngVersion)
	{
		printf("Warning! Merging VGM files with different versions!\n");
		if (DstHead->lngVersion < SrcHead->lngVersion)
			DstHead->lngVersion = SrcHead->lngVersion;
	}

	DstHead->lngLoopOffset |= SrcHead->lngLoopOffset;	// I just need to know if there IS one
	if (DstHead->lngLoopSamples != SrcHead->lngLoopSamples)
	{
		printf("Warning! Loops lengths different!\n");
		if (! DstHead->lngLoopSamples)
			DstHead->lngLoopSamples = SrcHead->lngLoopSamples;
	}
	if (DstHead->lngTotalSamples < SrcHead->lngTotalSamples)
		DstHead->lngTotalSamples = SrcHead->lngTotalSamples;
	if (DstHead->lngDataOffset < SrcHead->lngDataOffset)
		DstHead->lngDataOffset = SrcHead->lngDataOffset;
	if (SrcHead->bytLoopModifier)
	{
		if (! DstHead->bytLoopModifier)
			DstHead->bytLoopModifier = SrcHead->bytLoopModifier;
		else if (DstHead->bytLoopModifier != SrcHead->bytLoopModifier)
			printf("Warning! Different Loop Modifier!\n");
	}

	// merge chip clocks
	memset(SrcChpMap, 0x00, sizeof(CHIP_MAPS));
	TempMap = &SrcChpMap->SN76496;
	for (CurChip = 0x00; CurChip < 0x15; CurChip ++)
	{
		// jump to chip value
		switch(CurChip)
		{
		case 0x00:	// SN76496
			SrcClock = &SrcHead->lngHzPSG;
			DstClock = &DstHead->lngHzPSG;
			break;
		case 0x02:	// YM2612
			SrcClock = &SrcHead->lngHzYM2612;
			DstClock = &DstHead->lngHzYM2612;
			break;
		case 0x04:	// SegaPCM
			SrcClock = &SrcHead->lngHzSPCM;
			DstClock = &DstHead->lngHzSPCM;
			break;
		case 0x05:	// RF5C68
			SrcClock = &SrcHead->lngHzRF5C68;
			DstClock = &DstHead->lngHzRF5C68;
			break;
		case 0x13:	// GB DMG
			SrcClock = &SrcHead->lngHzGBDMG;
			DstClock = &DstHead->lngHzGBDMG;
			break;
		}

		switch(CurChip)
		{
		case 0x00:	// SN76496
		case 0x01:	// YM2413
		case 0x02:	// YM2612
		case 0x03:	// YM2151
		case 0x04:	// SegaPCM
		case 0x06:	// YM2203
		case 0x07:	// YM2608
		case 0x08:	// YM2610
		case 0x09:	// YM3812
		case 0x0A:	// YM3526
		case 0x0B:	// Y8950
		case 0x0C:	// YMF262
		case 0x0D:	// YMF278B
		case 0x0E:	// YMF271
		case 0x0F:	// YMZ280B
		case 0x12:	// AY8910
		case 0x13:	// GB DMG
		case 0x14:	// NES APU
			DualSupport = true;
			break;
		case 0x05:	// RF5C68
		case 0x10:	// RF5C164
		case 0x11:	// PWM
			DualSupport = false;
			break;
		default:
			DualSupport = false;
			TempMap = &SrcChpMap->Default;
			break;
		}
		DualSupport &= ! NO_DUAL_MIXING;

		// check chip clock
		if (*SrcClock)
		{
			if (! *DstClock)
			{
				*DstClock = *SrcClock;
			}
			else
			{
				if ((*DstClock & 0x80000000) != (*SrcClock & 0x80000000))
					printf("Warning! Different Clock-Bit 31 for %s!\n", CHIP_NAMES[CurChip]);

				if (! DualSupport || (*DstClock & 0x40000000) || (*SrcClock & 0x40000000))
				{
					printf("Warning! Merging multiple %s chips with the same type into "
							"each other!", CHIP_NAMES[CurChip]);
				}
				else
				{
					*DstClock |= (*SrcClock & 0x80000000) | 0x40000000;
					TempMap->MapToDual = DualSupport;
				}
				if ((*DstClock & 0x3FFFFFFF) != (*SrcClock & 0x3FFFFFFF))
					printf("Warning! Different chip clocks for %s!\n", CHIP_NAMES[CurChip]);
			}
		}

		// do special checks
		switch(CurChip)
		{
		case 0x00:	// SN76496
			if (SrcHead->shtPSG_Feedback)
			{
				if (! DstHead->shtPSG_Feedback)
					DstHead->shtPSG_Feedback = SrcHead->shtPSG_Feedback;
				else if (DstHead->shtPSG_Feedback != SrcHead->shtPSG_Feedback)
					printf("Warning! Different Feedback for %s!\n", CHIP_NAMES[CurChip]);
			}

			if (SrcHead->bytPSG_SRWidth)
			{
				if (! DstHead->bytPSG_SRWidth)
					DstHead->bytPSG_SRWidth = SrcHead->bytPSG_SRWidth;
				else if (DstHead->bytPSG_SRWidth != SrcHead->bytPSG_SRWidth)
					printf("Warning! Different SR Width for %s!\n", CHIP_NAMES[CurChip]);
			}

			if (SrcHead->bytPSG_Flags)
			{
				if (! DstHead->bytPSG_Flags)
					DstHead->bytPSG_Flags = SrcHead->bytPSG_Flags;
				else if (DstHead->bytPSG_Flags != SrcHead->bytPSG_Flags)
					printf("Warning! Different Flags for %s!\n", CHIP_NAMES[CurChip]);
			}
			break;
		case 0x04:	// SegaPCM
			if (SrcHead->lngSPCMIntf)
			{
				if (! DstHead->lngSPCMIntf)
					DstHead->lngSPCMIntf = SrcHead->lngSPCMIntf;
				else if (DstHead->lngSPCMIntf != SrcHead->lngSPCMIntf)
					printf("Warning! Different Interface Reg for %s!\n", CHIP_NAMES[CurChip]);
			}
			break;
		case 0x06:	// YM2203
			if (SrcHead->bytAYFlagYM2203)
			{
				if (! DstHead->bytAYFlagYM2203)
					DstHead->bytAYFlagYM2203 = SrcHead->bytAYFlagYM2203;
				else if (DstHead->bytAYFlagYM2203 != SrcHead->bytAYFlagYM2203)
					printf("Warning! Different Flags for %s!\n", CHIP_NAMES[CurChip]);
			}
			break;
		case 0x07:	// YM2608
			if (SrcHead->bytAYFlagYM2608)
			{
				if (! DstHead->bytAYFlagYM2608)
					DstHead->bytAYFlagYM2608 = SrcHead->bytAYFlagYM2608;
				else if (DstHead->bytAYFlagYM2608 != SrcHead->bytAYFlagYM2608)
					printf("Warning! Different Flags for %s!\n", CHIP_NAMES[CurChip]);
			}
			break;
		case 0x12:	// AY8910
			if (SrcHead->bytAYType)
			{
				if (! DstHead->bytAYType)
					DstHead->bytAYType = SrcHead->bytAYType;
				else if (DstHead->bytAYType != SrcHead->bytAYType)
					printf("Warning! Different Chip Type for %s!\n", CHIP_NAMES[CurChip]);
			}

			if (SrcHead->bytAYFlag)
			{
				if (! DstHead->bytAYFlag)
					DstHead->bytAYFlag = SrcHead->bytAYFlag;
				else if (DstHead->bytAYFlag != SrcHead->bytAYFlag)
					printf("Warning! Different Flags for %s!\n", CHIP_NAMES[CurChip]);
			}
			break;
		}

		// move to next chip clock
		SrcClock ++;
		DstClock ++;
		TempMap ++;
	}

	return;
}

#define BITVAL_BYTE(BitVal)	(BitVal >> 3)
#define BITVAL_BIT(BitVal)	(BitVal & 0x07)
#define GET_BIT(Var, Bit)		(Var[BITVAL_BYTE(Bit)] >> BITVAL_BIT(Bit))
#define SET_BIT(Var, Bit)		Var[BITVAL_BYTE(Bit)] |= 1 << BITVAL_BIT(Bit)

static void MergeVGMData(void)
{
	const UINT8 CMD_CHIP_MAP_5x[0x10] =
	{	0x00, 0x01, 0x02, 0x02, 0x03, 0x06, 0x07, 0x07, 0x08, 0x08, 0x09, 0x0A, 0x0B, 0x0F, 0x0C, 0x0C};
	const UINT8 CMD_CHIP_MAP_Bx[0x30] =
	{	0x05, 0x10, 0x11, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x1B, 0x1D, 0x1E, 0xFF, 0xFF, 0xFF, 0xFF,
	//const UINT8 CMD_CHIP_MAP_Cx[0x10] =
		0x04, 0x05, 0x10, 0x15, 0x1F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	//const UINT8 CMD_CHIP_MAP_Dx[0x10] =
		0x0D, 0x0E, 0x19, 0x1A, 0x1C, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	UINT8 FileID;
	VGM_HEADER DstHead;
	UINT32* SrcPos;
	UINT32 DstPos;
	bool* EndReached;
	INT32 NxtSmplPos;
	UINT32 DstSmplCount;
	UINT8 Command;
	UINT32 CmdDelay;
	UINT32 DstDelay;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	VGM_INF* TempVGM;
#ifdef WIN32
	UINT32 CmdTimer;
	char TempStr[0x80];
	char MinSecStr[0x80];
#endif
	UINT32 CmdLen;
	UINT8 VGMsRunning;
	bool WriteEvent;
	bool IsLoopPnt;
	UINT32 NewLoopPos;
	bool WroteCmd80;
	UINT8 WasDblk;
	CHIP_MAP* TempMap;
	UINT8 UsedStreams[0x20];
	UINT8 PtchCmdFlags;
	UINT8 PtchCmdData[0x08];
	DATABLK_MAP* TempDBlk;
	UINT16 DataBlkUsed[0x40];

	SrcPos = (UINT32*)malloc(MAX_VGMS * sizeof(UINT32));
	EndReached = (bool*)malloc(MAX_VGMS * sizeof(bool));

	DstHead = SrcVGM[0x00].Head;
	memset(&SrcVGM[0x00].ChipMap, 0x00, sizeof(SrcVGM[0x00].ChipMap));
	for (FileID = 0x01; FileID < MAX_VGMS; FileID ++)
		MergeVGMHeader(&DstHead, &SrcVGM[FileID].Head, &SrcVGM[FileID].ChipMap);
	for (FileID = 0x00; FileID < MAX_VGMS; FileID ++)
	{
		memset(SrcVGM[FileID].StrmMap, 0xFF, 0x100);
		for (TempByt = 0x00; TempByt < 0x40; TempByt ++)
		{
			SrcVGM[FileID].DataBlk[TempByt].BlockCnt = 0x00;
			SrcVGM[FileID].DataBlk[TempByt].BlockAlloc = 0x00;
			SrcVGM[FileID].DataBlk[TempByt].BlkMap = NULL;
		}
	}
	memset(UsedStreams, 0x00, 0x20);
	memset(DataBlkUsed, 0x00, sizeof(DataBlkUsed));

	DstPos = DstHead.lngDataOffset;
	DstDataLen = 0x00;
	DstSmplPos = 0;
	DstSmplCount = DstHead.lngTotalSamples;
	TempLng = 0x000;
	VGMsRunning = 0x00;
	for (FileID = 0x00; FileID < MAX_VGMS; FileID ++)
	{
		TempVGM = &SrcVGM[FileID];
		DstDataLen += TempVGM->DataLen;
		TempVGM->Pos = TempVGM->Head.lngDataOffset;
		TempVGM->SmplPos = 0;
		EndReached[FileID] = false;
		VGMsRunning ++;
	}
	for (FileID = 0x00; FileID < MAX_VGMS; FileID ++)
	{
		TempVGM = &SrcVGM[FileID];
		if (TempVGM->Head.lngGD3Offset && TempVGM->Head.lngGD3Offset + 0x0B < TempVGM->Head.lngEOFOffset)
		{
			NxtSmplPos = TempVGM->Head.lngGD3Offset;
			memcpy(&TempLng, &TempVGM->Data[NxtSmplPos + 0x00], 0x04);
			if (TempLng == FCC_GD3)
			{
				memcpy(&CmdLen, &TempVGM->Data[NxtSmplPos + 0x08], 0x04);
				DstDataLen += 0x0C + CmdLen;
				break;
			}
		}
	}
	DstDataLen += 0x100;	// some additional space
	DstData = (UINT8*)malloc(DstDataLen);

	NxtSmplPos = 0x00;
	NewLoopPos = 0x00;

#ifdef WIN32
	CmdTimer = 0;
#endif
	ErrMsgs = 0x00;
	WroteCmd80 = false;
	while(VGMsRunning)
	{
		WriteEvent = true;
		for (FileID = 0x00; FileID < MAX_VGMS; FileID ++)
		{
			if (EndReached[FileID])
				continue;

			// find sample of next command
			if (WriteEvent || SrcVGM[FileID].SmplPos < NxtSmplPos)	// Note: This line breaks the auto-completion somehow.
				NxtSmplPos = SrcVGM[FileID].SmplPos;
			WriteEvent = false;
		}

		WasDblk = 0x00;
		for (FileID = 0x00; FileID < MAX_VGMS; FileID ++)
		{
			TempVGM = &SrcVGM[FileID];
			while(! EndReached[FileID])
			{
				CmdDelay = 0;
				PtchCmdFlags = 0x00;
				Command = TempVGM->Data[TempVGM->Pos];

				// Make the VGMs looks nicer ;)
				// (write data blocks of all VGMs first, other commands later)
				if (! WasDblk)
				{
					if (Command == 0x67)
						WasDblk = 0x01;
					else
						WasDblk = 0x02;
				}
				else
				{
					if (WasDblk == 0x01 && Command != 0x67)
						break;
				}

				CmdLen = GetCmdLen(Command);
				WriteEvent = true;
				if (TempVGM->Pos == TempVGM->Head.lngLoopOffset && ! NewLoopPos)
					IsLoopPnt = true;
				else
					IsLoopPnt = false;

				if (Command >= 0x70 && Command <= 0x8F)
				{
					switch(Command & 0xF0)
					{
					case 0x70:
						TempSht = (Command & 0x0F) + 0x01;
						CmdDelay = TempSht;
						WriteEvent = false;
						break;
					case 0x80:
						// Handling is done at WriteEvent
						//WriteEvent = true;	// I still need to write it
						if (TempVGM->ChipMap.YM2612.MapToDual)
						{
							if (! (ErrMsgs & 0x01))
							{
								ErrMsgs |= 0x01;
								printf("Warning: VGM 1.50 DAC commands used by VGM #%u!\n", 1 + FileID);
								printf("v1.50 DAC commands can only be used by the first YM2612 chip,\n");
								printf("so the result will be wrong.\n");
							}
						}
						break;
					}
				}
				else
				{
					switch(Command)
					{
					case 0x66:	// End Of File
						CmdLen = 0x00;
						EndReached[FileID] = true;
						VGMsRunning --;
						if (VGMsRunning)
						{
							WriteEvent = false;
						}
						else
						{
							// force to write this command
							NxtSmplPos = TempVGM->SmplPos;
							CmdLen = 0x01;
						}
						break;
					case 0x62:	// 1/60s delay
						TempSht = 735;
						CmdDelay = TempSht;
						WriteEvent = false;
						break;
					case 0x63:	// 1/50s delay
						TempSht = 882;
						CmdDelay = TempSht;
						WriteEvent = false;
						break;
					case 0x61:	// xx Sample Delay
						memcpy(&TempSht, &TempVGM->Data[TempVGM->Pos + 0x01], 0x02);
						CmdDelay = TempSht;
						WriteEvent = false;
						break;
					case 0x67:	// PCM Data Stream
						TempByt = TempVGM->Data[TempVGM->Pos + 0x02];
						if ((TempByt & 0xC0) == 0x40 && TempByt != 0x7F)
							TempByt &= 0x3F;

						switch(TempByt)
						{
						case 0x00:
							TempMap = &TempVGM->ChipMap.YM2612;
							break;
						case 0x01:
							TempMap = &TempVGM->ChipMap.RF5C68;
							break;
						case 0x02:
							TempMap = &TempVGM->ChipMap.RF5C164;
							break;
						case 0x03:
							TempMap = &TempVGM->ChipMap.PWM;
							break;
						case 0x80:
							TempMap = &TempVGM->ChipMap.SegaPCM;
							break;
						case 0x81:
							TempMap = &TempVGM->ChipMap.YM2608;
							break;
						case 0x82:
							TempMap = &TempVGM->ChipMap.YM2610;
							break;
						case 0x83:
							TempMap = &TempVGM->ChipMap.YM2610;
							break;
						case 0x84:
							TempMap = &TempVGM->ChipMap.YMF278B;
							break;
						case 0x85:
							TempMap = &TempVGM->ChipMap.YMF271;
							break;
						case 0x86:
							TempMap = &TempVGM->ChipMap.YMZ280B;
							break;
						case 0x87:
							TempMap = &TempVGM->ChipMap.YMF278B;
							break;
						case 0x88:
							TempMap = &TempVGM->ChipMap.Y8950;
							break;
						/*case 0x89:
							TempMap = &TempVGM->ChipMap.MultiPCM;
							break;
						case 0x8A:
							TempMap = &TempVGM->ChipMap.UPD7759;
							break;
						case 0x8B:
							TempMap = &TempVGM->ChipMap.OKIM6295;
							break;
						case 0x8C:
							TempMap = &TempVGM->ChipMap.K054539;
							break;
						case 0x8D:
							TempMap = &TempVGM->ChipMap.C140;
							break;
						case 0x8E:
							TempMap = &TempVGM->ChipMap.K053260;
							break;
						case 0x8F:
							TempMap = &TempVGM->ChipMap.QSound;
							break;*/
						case 0xC0:
							TempMap = &TempVGM->ChipMap.RF5C68;
							break;
						case 0xC1:
							TempMap = &TempVGM->ChipMap.RF5C164;
							break;
						case 0xC2:
							TempMap = &TempVGM->ChipMap.NESAPU;
							break;
						default:
							TempMap = NULL;
							break;
						}
						if (TempByt < 0x7F)	// yes, it's NOT < 80
						{
							TempMap = NULL;
							TempDBlk = &SrcVGM[FileID].DataBlk[TempByt];
							if (TempDBlk->BlockCnt >= TempDBlk->BlockAlloc)
							{
								TempDBlk->BlockAlloc += 0x20;
								TempDBlk->BlkMap = (UINT16*)realloc(TempDBlk->BlkMap,
																	TempDBlk->BlockAlloc * sizeof(UINT16));
							}
							TempDBlk->BlkMap[TempDBlk->BlockCnt] = DataBlkUsed[TempByt];
							TempDBlk->BlockCnt ++;
							DataBlkUsed[TempByt] ++;
						}

						if (TempMap != NULL && TempMap->MapToDual)
						{
							PtchCmdFlags = 1 << 0x06;
							PtchCmdData[0x06] = 0x80 | TempVGM->Data[TempVGM->Pos + 0x06];
						}
						memcpy(&TempLng, &TempVGM->Data[TempVGM->Pos + 0x03], 0x04);
						TempLng &= 0x7FFFFFFF;

						CmdLen = 0x07 + TempLng;
						break;
					case 0x68:	// PCM RAM write
						TempByt = TempVGM->Data[TempVGM->Pos + 0x02];
						switch(TempByt)
						{
						case 0x01:
							TempMap = &TempVGM->ChipMap.RF5C68;
							break;
						case 0x02:
							TempMap = &TempVGM->ChipMap.RF5C164;
							break;
						default:
							TempMap = NULL;
							break;
						}
						/*if (TempMap != NULL && TempMap->MapToDual)
						{
							PtchCmdFlags = 1 << 0x02;
							PtchCmdData[0x02] = 0x80 | TempVGM->Data[TempVGM->Pos + 0x02];
						}*/
						CmdLen = 0x0C;
						break;
					case 0x90:	// DAC Ctrl: Setup Chip
						TempByt = TempVGM->Data[TempVGM->Pos + 0x01];
						if (TempVGM->StrmMap[TempByt] == 0xFF)
						{
							if (! GET_BIT(UsedStreams, TempByt))
							{
								TempVGM->StrmMap[TempByt] = TempByt;
							}
							else
							{
								for (TempSht = 0x00; TempSht < 0xFF; TempSht ++)
								{
									if (! GET_BIT(UsedStreams, TempSht))
									{
										TempVGM->StrmMap[TempByt] = (UINT8)TempSht;
										break;
									}
								}
							}
							SET_BIT(UsedStreams, TempVGM->StrmMap[TempByt]);
						}

						TempMap = &TempVGM->ChipMap.SN76496 + TempVGM->Data[TempVGM->Pos + 0x02];
						if (TempMap->MapToDual)
						{
							PtchCmdFlags = 1 << 0x02;
							PtchCmdData[0x02] = 0x80 | TempVGM->Data[TempVGM->Pos + 0x02];
						}

						CmdLen = 0x05;
						break;
					case 0x91:	// DAC Ctrl: Set Data
						TempByt = TempVGM->Data[TempVGM->Pos + 0x01];
						if (TempVGM->StrmMap[TempByt] != 0xFF)
							TempVGM->StrmDBlk[TempByt] = TempVGM->Data[TempVGM->Pos + 0x02];
						CmdLen = 0x05;
						break;
					case 0x92:	// DAC Ctrl: Set Freq
						CmdLen = 0x06;
						break;
					case 0x93:	// DAC Ctrl: Play from Start Pos
						TempByt = TempVGM->Data[TempVGM->Pos + 0x01];
						if (TempVGM->StrmMap[TempByt] != 0xFF && TempVGM->StrmMap[TempByt] != TempByt)
						{
							if (! (ErrMsgs & 0x02))
							{
								ErrMsgs |= 0x02;
								printf("Warning: Remapping of 0x93 Stream Commands not yet implemented!\n");
							}
						}
						CmdLen = 0x0B;
						break;
					case 0x94:	// DAC Ctrl: Stop immediately
						CmdLen = 0x02;
						break;
					case 0x95:	// DAC Ctrl: Play Block (small)
						TempByt = TempVGM->Data[TempVGM->Pos + 0x01];
						if (TempVGM->StrmMap[TempByt] != 0xFF)
						{
							TempByt = TempVGM->StrmDBlk[TempByt];
							TempDBlk = &SrcVGM[FileID].DataBlk[TempByt];
							memcpy(&TempSht, &TempVGM->Data[TempVGM->Pos + 0x02], 0x02);
							if (TempDBlk->BlkMap[TempSht] != TempSht)
							{
								PtchCmdFlags |= (1 << 0x02) | (1 << 0x03);
								memcpy(&PtchCmdData[0x02], &TempDBlk->BlkMap[TempSht], 0x02);
							}
						}
						CmdLen = 0x05;
						break;
					}
				}
				switch(Command & 0xF0)
				{
				case 0x40:
					if (Command == 0x4F)
					{
						TempMap = &TempVGM->ChipMap.SN76496;
						if (TempMap->MapToDual)
						{
							PtchCmdFlags = 1 << 0x00;
							PtchCmdData[0x00] = 0x3F;
						}
					}
					break;
				case 0x50:
					TempMap = &TempVGM->ChipMap.SN76496 + CMD_CHIP_MAP_5x[Command & 0x0F];
					if (TempMap->MapToDual)
					{
						PtchCmdFlags = 1 << 0x00;
						if (Command == 0x50)
							PtchCmdData[0x00] = 0x30;
						else
							PtchCmdData[0x00] = Command + 0x50;
					}
					break;
				case 0xA0:
					if (Command == 0xA0)
					{
						TempMap = &TempVGM->ChipMap.AY8910;
						if (TempMap->MapToDual)
						{
							PtchCmdFlags = 1 << 0x01;
							PtchCmdData[0x01] = 0x80 | TempVGM->Data[TempVGM->Pos + 0x01];
						}
					}
					break;
				case 0xB0:
				case 0xC0:
				case 0xD0:
					TempByt = CMD_CHIP_MAP_Bx[Command - 0xB0];
					if (TempByt != 0xFF && TempByt < 0x15)	// TODO: Make it v1.61 compatible and remove <0x15
					{
						TempMap = &TempVGM->ChipMap.SN76496 + TempByt;
						if (TempMap->MapToDual)
						{
							PtchCmdFlags = 1 << 0x01;
							PtchCmdData[0x01] = 0x80 | TempVGM->Data[TempVGM->Pos + 0x01];
						}
					}
					break;
				case 0x90:
					TempByt = TempVGM->Data[TempVGM->Pos + 0x01];
					if (TempVGM->StrmMap[TempByt] == 0xFF)
					{
						WriteEvent = false;
					}
					else if (TempVGM->Data[TempVGM->Pos + 0x01] != TempVGM->StrmMap[TempByt])
					{
						PtchCmdFlags |= 1 << 0x01;
						PtchCmdData[0x01] = TempVGM->StrmMap[TempByt];
					}
					break;
				}
				if (WriteEvent || IsLoopPnt)
				{
					// placed here because I need to read all delays
					if (TempVGM->SmplPos > NxtSmplPos)
						break;	// exit while

					// Write Delay
					DstDelay = NxtSmplPos - DstSmplPos;
					while(DstDelay)
					{
						if (DstDelay <= 0xFFFF)
							TempSht = (UINT16)DstDelay;
						else
							TempSht = 0xFFFF;

						if (WroteCmd80)
						{
							// highest delay compression - Example:
							// Delay   39 -> 8F 7F 77
							// Delay 1485 -> 8F 62 62 (instead of 80 61 CD 05)
							// Delay  910 -> 8F 63 7D (instead of 80 61 8E 03)
							if (TempSht >= 0x20 && TempSht <= 0x2F)			// 7x
								TempSht -= 0x10;
							else if (TempSht >=  735 && TempSht <=  766)	// 62
								TempSht -= 735;
							else if (TempSht >= 1470 && TempSht <= 1485)	// 62 62
								TempSht -= 1470;
							else if (TempSht >=  882 && TempSht <=  913)	// 63
								TempSht -= 882;
							else if (TempSht >= 1764 && TempSht <= 1779)	// 63 63
								TempSht -= 1764;
							else if (TempSht >= 1617 && TempSht <= 1632)	// 62 63
								TempSht -= 1617;

							/*if (TempSht >= 0x10 && TempSht <= 0x1F)
								TempSht = 0x0F;
							else if (TempSht >= 0x20)
								TempSht = 0x00;*/
							if (TempSht >= 0x10)
								TempSht = 0x0F;
							DstData[DstPos - 1] |= TempSht;
							WroteCmd80 = false;
						}
						else if (! TempSht)
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
						DstDelay -= TempSht;
						DstSmplPos += TempSht;
					}

					if (IsLoopPnt)
						NewLoopPos = DstPos;

					if (WriteEvent)
					{
						if (DstPos + CmdLen > DstDataLen)
						{
							printf("Error! Allocated space too small!\n");
							printf("Please report this bug.\n");
							//return;

							DstData[DstPos] = 0x66;
							DstPos ++;
							VGMsRunning = 0x00;
							break;
						}

						// Write Event
						WroteCmd80 = ((Command & 0xF0) == 0x80);
						if (WroteCmd80)
						{
							CmdDelay = Command & 0x0F;
							Command &= 0x80;
						}
						if (CmdLen != 0x01)
							memcpy(&DstData[DstPos], &TempVGM->Data[TempVGM->Pos], CmdLen);
						else
							DstData[DstPos] = Command;	// write the 0x80-command correctly

						if (PtchCmdFlags)
						{
							for (TempByt = 0x00; TempByt < 0x04; TempByt ++)
							{
								if (PtchCmdFlags & (1 << TempByt))
									DstData[DstPos + TempByt] = PtchCmdData[TempByt];
							}
						}
						DstPos += CmdLen;
					}
				}	// end if (WriteEvent || IsLoopPnt)
				TempVGM->Pos += CmdLen;
				TempVGM->SmplPos += CmdDelay;
			}	// end while(! EndReached)
		}	// end for (FileID)

#ifdef WIN32
		if (CmdTimer < GetTickCount())
		{
			PrintMinSec(DstSmplPos, MinSecStr);
			PrintMinSec(DstSmplCount, TempStr);
			printf("%04.3f %% - %s / %s (%08X / %08X) ...\r", (float)DstPos / DstDataLen *
					100, MinSecStr, TempStr, DstPos, DstDataLen);
			CmdTimer = GetTickCount() + 200;
		}
#endif
	}
	if (DstHead.lngLoopOffset)
	{
		if (NewLoopPos)
		{
			DstHead.lngLoopOffset = NewLoopPos - 0x1C;
		}
		else
		{
			printf("Error! Failed to relocate Loop Point!\n");
			DstHead.lngLoopOffset = 0x00;
		}
	}
	printf("\t\t\t\t\t\t\t\t\r");

	for (FileID = 0x00; FileID < MAX_VGMS; FileID ++)
	{
		TempVGM = &SrcVGM[FileID];
		if (TempVGM->Head.lngGD3Offset && TempVGM->Head.lngGD3Offset + 0x0B < TempVGM->Head.lngEOFOffset)
		{
			NxtSmplPos = TempVGM->Head.lngGD3Offset;
			memcpy(&TempLng, &TempVGM->Data[NxtSmplPos + 0x00], 0x04);
			if (TempLng == FCC_GD3)
			{
				memcpy(&CmdLen, &TempVGM->Data[NxtSmplPos + 0x08], 0x04);
				CmdLen += 0x0C;

				DstHead.lngGD3Offset = DstPos - 0x14;
				memcpy(&DstData[DstPos], &TempVGM->Data[NxtSmplPos], CmdLen);
				DstPos += CmdLen;
				break;
			}
		}
	}

	DstDataLen = DstPos;
	DstHead.lngEOFOffset = DstDataLen - 0x04;
	DstPos = DstHead.lngDataOffset;
	DstHead.lngDataOffset -= 0x34;

	// Copy Header
	if (sizeof(VGM_HEADER) < DstPos)
	{
		TempLng = sizeof(VGM_HEADER);
		memcpy(&DstData[0x00], &DstHead, TempLng);
		memcpy(&DstData[TempLng], &SrcVGM[0x00].Data[TempLng], DstPos - TempLng);
	}
	else
	{
		memcpy(&DstData[0x00], &DstHead, DstPos);
	}

	free(SrcPos);
	free(EndReached);

	return;
}

INLINE UINT16 GetCmdLen(UINT8 Command)
{
    // Handle special commands up front
    switch(Command)
    {
        case 0x41: // K007232 write
            return 0x03;
        // (add here any other special cases)
    }
	switch(Command & 0xF0)
	{
	case 0x70:
	case 0x80:
		return 0x01;
	case 0x30:
	case 0x40:
		return 0x02;
	case 0x50:
	case 0xA0:
	case 0xB0:
		if (Command == 0x50)
			return 0x02;
		return 0x03;
	case 0xC0:
	case 0xD0:
		return 0x04;
	case 0xE0:
	case 0xF0:
		return 0x05;
	case 0x60:
		switch(Command)
		{
		case 0x61:
			return 0x03;
		case 0x62:
		case 0x63:
			return 0x01;
		case 0x66:
			return 0x01;
		case 0x67:
			return 0x00;	// must be handled seperately
		case 0x68:
			return 0x0C;
		}
		// fall through
	case 0x90:
		switch(Command)
		{
		case 0x90:	// DAC Ctrl: Setup Chip
			return 0x05;
		case 0x91:	// DAC Ctrl: Set Data
			return 0x05;
		case 0x92:	// DAC Ctrl: Set Freq
			return 0x06;
		case 0x93:	// DAC Ctrl: Play from Start Pos
			return 0x0B;
		case 0x94:	// DAC Ctrl: Stop immediately
			return 0x02;
		case 0x95:	// DAC Ctrl: Play Block (small)
			return 0x05;
		}
		// fall through
	default:
		printf("Unknown Command: %X\n", Command);
		return 0x01;
	}
}
