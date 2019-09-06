// vgm_ndlz.c - VGM Undualizer
// (Thanks to nineko for the name)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "stdtype.h"
#include "stdbool.h"
#include "VGMFile.h"
#include "common.h"
#include "vgm_lib.h"


typedef struct vgm_info
{
	VGM_HEADER Head;
	UINT32 DataLen;
	UINT8* Data;
	UINT32 Pos;
	INT32 SmplPos;
	bool WroteCmd80;
	bool HadWrt;
} VGM_INF;


static bool OpenVGMFile(const char* FileName);
static void WriteVGMFile(const char* FileName, VGM_INF* VGMInf);
static void CopyHeader(VGM_HEADER* SrcHead, VGM_HEADER* DstHead, bool IsChip2);
static void SplitVGMData(void);


#define MAX_VGMS	0x02

VGM_HEADER SrcHead;
UINT8* SrcData;
UINT32 SrcDataLen;
UINT32 SrcPos;
INT32 SrcSmplPos;
VGM_INF DstVGM[MAX_VGMS];

char FileBase[0x100];

int main(int argc, char* argv[])
{
	int argbase;
	int ErrVal;
	char FileName[0x100];
	UINT8 FileID;

	printf("VGM Undualizer\n--------------\n\n");

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

	SplitVGMData();
	free(SrcData);

	for (FileID = 0x00; FileID < MAX_VGMS; FileID ++)
	{
		sprintf(FileName, "%s_%u.vgm", FileBase, FileID);
		WriteVGMFile(FileName, &DstVGM[FileID]);
		free(DstVGM[FileID].Data);
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
	gzread(hFile, &SrcHead, sizeof(VGM_HEADER));
	ZLIB_SEEKBUG_CHECK(SrcHead);

	// Header preperations
	if (SrcHead.lngVersion < 0x00000150)
	{
		SrcHead.lngDataOffset = 0x00000000;
	}
	if (SrcHead.lngVersion < 0x00000151)
	{
		SrcHead.lngHzSPCM = 0x0000;
		SrcHead.lngSPCMIntf = 0x00000000;
		// all others are zeroed by memset
	}
	// relative -> absolute addresses
	SrcHead.lngEOFOffset += 0x00000004;
	if (SrcHead.lngGD3Offset)
		SrcHead.lngGD3Offset += 0x00000014;
	if (SrcHead.lngLoopOffset)
		SrcHead.lngLoopOffset += 0x0000001C;
	if (! SrcHead.lngDataOffset)
		SrcHead.lngDataOffset = 0x0000000C;
	SrcHead.lngDataOffset += 0x00000034;

	CurPos = SrcHead.lngDataOffset;
	if (SrcHead.lngVersion < 0x00000150)
		CurPos = 0x40;
	TempLng = sizeof(VGM_HEADER);
	if (TempLng > CurPos)
		TempLng -= CurPos;
	else
		TempLng = 0x00;
	memset((UINT8*)&SrcHead + CurPos, 0x00, TempLng);

	// Read Data
	SrcDataLen = SrcHead.lngEOFOffset;
	SrcData = (UINT8*)malloc(SrcDataLen);
	if (SrcData == NULL)
		goto OpenErr;
	gzseek(hFile, 0x00, SEEK_SET);
	gzread(hFile, SrcData, SrcDataLen);

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

static void WriteVGMFile(const char* FileName, VGM_INF* VGMInf)
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

	if (! VGMInf->HadWrt)
	{
		printf("%s skipped (empty).\n", FileTitle);
		return;
	}

	hFile = fopen(FileName, "wb");
	if (hFile == NULL)
	{
		printf("Error writing %s!\n", FileTitle);
		return;
	}

	fwrite(VGMInf->Data, 0x01, VGMInf->DataLen, hFile);
	fclose(hFile);
	printf("%s written.\n", FileTitle);

	return;
}

static void CopyHeader(VGM_HEADER* SrcHead, VGM_HEADER* DstHead, bool IsChip2)
{
	UINT8 CurChip;
	UINT32* SrcClock;
	UINT32* DstClock;

	*DstHead = *SrcHead;
	for (CurChip = 0x00; CurChip < 0x20; CurChip ++)
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
		case 0x18:	// OKIM6295
			SrcClock = &SrcHead->lngHzOKIM6295;
			DstClock = &DstHead->lngHzOKIM6295;
			break;
		}

		if (! IsChip2 || (*SrcClock & 0x40000000))
		{
			*DstClock = *SrcClock & 0xBFFFFFFF;
		}
		else
		{
			*DstClock = 0x00;
			switch(CurChip)
			{
			case 0x00:	// SN76496
				DstHead->shtPSG_Feedback = 0x0000;
				DstHead->bytPSG_SRWidth = 0x00;
				DstHead->bytPSG_Flags = 0x00;
				break;
			case 0x06:	// YM2203
				DstHead->bytAYFlagYM2203 = 0x00;
				break;
			case 0x07:	// YM2608
				DstHead->bytAYFlagYM2608 = 0x00;
				break;
			case 0x12:	// AY8910
				DstHead->bytAYType = 0x00;
				DstHead->bytAYFlag = 0x00;
				break;
			case 0x17:
				DstHead->bytOKI6258Flags = 0x00;
				break;
			case 0x1A:
				DstHead->bytK054539Flags = 0x00;
				break;
			case 0x1C:
				DstHead->bytC140Type = 0x00;
				break;
			}
		}

		// move to next chip clock
		SrcClock ++;
		DstClock ++;
	}

	return;
}

static void SplitVGMData(void)
{
	UINT8 FileID;
	UINT8 Command;
	UINT32 CmdDelay;
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
	bool StopVGM;
	bool WriteEvent;
	UINT32 FixOfs;
	UINT8 FixByt;

	for (FileID = 0x00; FileID < MAX_VGMS; FileID ++)
	{
		TempVGM = &DstVGM[FileID];

		CopyHeader(&SrcHead, &TempVGM->Head, (FileID > 0x00));
		TempVGM->Pos = TempVGM->Head.lngDataOffset;
		TempVGM->DataLen = SrcDataLen;
		TempVGM->Data = (UINT8*)malloc(TempVGM->DataLen);
		TempVGM->SmplPos = 0;
		TempVGM->WroteCmd80 = false;
		TempVGM->HadWrt = false;
	}

#ifdef WIN32
	CmdTimer = 0;
#endif
	StopVGM = false;
	SrcPos = SrcHead.lngDataOffset;
	while(SrcPos < SrcDataLen)
	{
		CmdDelay = 0;
		Command = SrcData[SrcPos];
		WriteEvent = true;
		if (SrcPos == SrcHead.lngLoopOffset)
		{
			for (FileID = 0x00; FileID < MAX_VGMS; FileID ++)
			{
				TempVGM = &DstVGM[FileID];

				TempLng = SrcSmplPos - TempVGM->SmplPos;
				VGMLib_WriteDelay(TempVGM->Data, &TempVGM->Pos, TempLng, &TempVGM->WroteCmd80);
				TempVGM->Head.lngLoopOffset = TempVGM->Pos;
				TempVGM->SmplPos = SrcSmplPos;
			}
		}

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
				break;
			}
			CmdLen = 0x01;
		}
		else
		{
			FileID = 0x00;
			switch(Command)
			{
			case 0x66:	// End Of File
				StopVGM = true;

				WriteEvent = false;
				CmdLen = 0x01;
				break;
			case 0x62:	// 1/60s delay
				TempSht = 735;
				CmdDelay = TempSht;

				WriteEvent = false;
				CmdLen = 0x01;
				break;
			case 0x63:	// 1/50s delay
				TempSht = 882;
				CmdDelay = TempSht;

				WriteEvent = false;
				CmdLen = 0x01;
				break;
			case 0x61:	// xx Sample Delay
				memcpy(&TempSht, &SrcData[SrcPos + 0x01], 0x02);
				CmdDelay = TempSht;

				WriteEvent = false;
				CmdLen = 0x03;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = SrcData[SrcPos + 0x02];
				memcpy(&TempLng, &SrcData[SrcPos + 0x03], 0x04);
				FileID = (TempLng & 0x80000000) >> 31;
				TempLng &= 0x7FFFFFFF;

				FixOfs = 0x06;
				FixByt = SrcData[SrcPos + 0x06] & 0x7F;

				CmdLen = 0x07 + TempLng;
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
			case 0x50:
			case 0x4F:
				CmdLen = 0x02;
				break;
			case 0x30:
				FileID = 0x01;
				FixOfs = 0x00;
				FixByt = 0x50;

				CmdLen = 0x02;
				break;
			case 0x3F:
				FileID = 0x01;
				FixOfs = 0x00;
				FixByt = 0x4F;

				CmdLen = 0x02;
				break;
			default:
				switch(Command & 0xF0)
				{
				case 0x30:
					CmdLen = 0x02;
					break;
				case 0x40:
				case 0x50:
					CmdLen = 0x03;
					break;
				case 0xA0:
					if (Command == 0xA0)
					{
						FileID = (SrcData[SrcPos + 0x01] & 0x80) >> 7;
						FixOfs = 0x01;
						FixByt = SrcData[SrcPos + 0x01] & 0x7F;
					}
					else
					{
						FileID = 0x01;
						FixOfs = 0x00;
						FixByt = Command - 0x50;
					}

					CmdLen = 0x03;
					break;
				case 0xB0:
					FileID = (SrcData[SrcPos + 0x01] & 0x80) >> 7;
					FixOfs = 0x01;
					FixByt = SrcData[SrcPos + 0x01] & 0x7F;

					CmdLen = 0x03;
					break;
				case 0xC0:
				case 0xD0:
					FileID = (SrcData[SrcPos + 0x01] & 0x80) >> 7;
					FixOfs = 0x01;
					FixByt = SrcData[SrcPos + 0x01] & 0x7F;

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
				}	// end switch(Command & 0xF0)
			}	// end switch(Command)
		}

		SrcSmplPos += CmdDelay;
		if (WriteEvent)
		{
			TempVGM = &DstVGM[FileID];

			TempLng = SrcSmplPos - TempVGM->SmplPos;
			VGMLib_WriteDelay(TempVGM->Data, &TempVGM->Pos, TempLng, &TempVGM->WroteCmd80);
			TempVGM->SmplPos = SrcSmplPos;

			// Write Event
			TempVGM->HadWrt = true;
			TempVGM->WroteCmd80 = ((Command & 0xF0) == 0x80);
			if (TempVGM->WroteCmd80)
			{
				CmdDelay = Command & 0x0F;
				Command &= 0x80;
			}

			if (CmdLen > 0x01)
				memcpy(&TempVGM->Data[TempVGM->Pos], &SrcData[SrcPos], CmdLen);
			else
				TempVGM->Data[TempVGM->Pos] = Command;	// write the 0x80-command correctly

			// now fix the 2nd-chip commands
			if (FileID)
				TempVGM->Data[TempVGM->Pos + FixOfs] = FixByt;
			TempVGM->Pos += CmdLen;
		}
		SrcPos += CmdLen;
		if (StopVGM)
			break;

#ifdef WIN32
		if (CmdTimer < GetTickCount())
		{
			PrintMinSec(SrcSmplPos, MinSecStr);
			PrintMinSec(SrcHead.lngTotalSamples, TempStr);
			printf("%04.3f %% - %s / %s (%08X / %08X) ...\r", (float)SrcPos / SrcDataLen *
					100, MinSecStr, TempStr, SrcPos, SrcDataLen);
			CmdTimer = GetTickCount() + 200;
		}
#endif
	}
	for (FileID = 0x00; FileID < MAX_VGMS; FileID ++)
	{
		TempVGM = &DstVGM[FileID];
		if (! TempVGM->HadWrt)
			continue;

		// write EOF
		TempLng = SrcSmplPos - TempVGM->SmplPos;
		VGMLib_WriteDelay(TempVGM->Data, &TempVGM->Pos, TempLng, &TempVGM->WroteCmd80);
		TempVGM->Data[TempVGM->Pos] = 0x66;
		TempVGM->Pos ++;
	}
	printf("\t\t\t\t\t\t\t\t\r");

	if (SrcHead.lngGD3Offset && SrcHead.lngGD3Offset + 0x0B < SrcHead.lngEOFOffset)
	{
		SrcPos = SrcHead.lngGD3Offset;
		memcpy(&TempLng, &SrcData[SrcPos + 0x00], 0x04);
		if (TempLng == FCC_GD3)
		{
			memcpy(&CmdLen, &SrcData[SrcPos + 0x08], 0x04);
			CmdLen += 0x0C;

			for (FileID = 0x00; FileID < MAX_VGMS; FileID ++)
			{
				TempVGM = &DstVGM[FileID];
				if (! TempVGM->HadWrt)
					continue;

				TempVGM->Head.lngGD3Offset = TempVGM->Pos;
				memcpy(&TempVGM->Data[TempVGM->Pos], &SrcData[SrcPos], CmdLen);
				TempVGM->Pos += CmdLen;
			}
		}
	}

	for (FileID = 0x00; FileID < MAX_VGMS; FileID ++)
	{
		TempVGM = &DstVGM[FileID];
		if (! TempVGM->HadWrt)
			continue;

		TempVGM->DataLen = TempVGM->Pos;
		TempVGM->Head.lngEOFOffset = TempVGM->DataLen - 0x04;
		if (TempVGM->Head.lngGD3Offset)
			TempVGM->Head.lngGD3Offset -= 0x14;
		if (TempVGM->Head.lngLoopOffset)
			TempVGM->Head.lngLoopOffset -= 0x1C;
		TempLng = TempVGM->Head.lngDataOffset;
		TempVGM->Head.lngDataOffset -= 0x34;

		// Copy Header
		if (sizeof(VGM_HEADER) < TempLng)
		{
			FixOfs = sizeof(VGM_HEADER);
			memcpy(&TempVGM->Data[0x00], &TempVGM->Head, FixOfs);
			memcpy(&TempVGM->Data[FixOfs], &SrcData[FixOfs], TempLng - FixOfs);
		}
		else
		{
			memcpy(&TempVGM->Data[0x00], &TempVGM->Head, TempLng);
		}
	}

	return;
}
