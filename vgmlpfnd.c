// vgmlpfnd.c - VGM Loop Finder
//
//TODO: Change "Start Pos" to "Start Sample"
// more TODO: 0:59.999 becomes 0:60.00

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "stdtype.h"
#include "stdbool.h"
#include "VGMFile.h"
#include "common.h"


//#define TECHNICAL_OUTPUT


typedef struct _vgm_command
{
	UINT32 Pos;
	UINT32 Sample;
	UINT16 Len;
	UINT8 Command;
	UINT32 Value;
} VGM_CMD;


static bool OpenVGMFile(const char* FileName);
static void ReadVGMData(void);
static void FindEqualitiesVGM(void);
static bool EqualityCheck(UINT32 CmpCmd, UINT32 SrcCmd, UINT32 CmdCount);
INLINE bool CompareVGMCommand(VGM_CMD* CmdA, VGM_CMD* CmdB);
//INLINE bool IgnoredCmd(UINT8 Command, UINT8 RegData);
INLINE bool IgnoredCmd(const UINT8* VGMPnt);


// semi-constants
UINT32 STEP_SIZE = 0x01;
UINT32 MIN_EQU_SIZE = 0x0400;
UINT32 START_POS = 0x00;


bool SilentMode;
bool LabelMode;	// Output Audacity labels
VGM_HEADER VGMHead;
UINT32 VGMDataLen;
UINT8* VGMData;
UINT32 VGMPos;
INT32 VGMSmplPos;
char FileBase[0x100];
UINT32 VGMCmdCount;
VGM_CMD* VGMCommand;
UINT32 EndPosCount;
UINT32* EndPosArr;
UINT32 LabelID;

int main(int argc, char* argv[])
{
	int argbase;
	int ErrVal;
	char FileName[0x100];
	char InputTxt[0x100];
	UINT32 TempLng;

	SilentMode = false;
	LabelMode = false;
	argbase = 1;
	while(argbase < argc && argv[argbase][0] == '-')
	{
		if (! stricmp(argv[argbase], "-silent"))
		{
			SilentMode = true;
			argbase ++;
		}
		else if (! stricmp(argv[argbase], "-labels"))
		{
			LabelMode = true;
			argbase ++;
		}
		else
		{
			break;
		}
	}

	if (! SilentMode)
		fprintf(stderr, "VGM Loop Finder\n---------------\n\n");

	ErrVal = 0;
	fprintf(stderr, "File Name:\t");
	if (argc <= argbase + 0)
	{
		ReadFilename(FileName, sizeof(FileName));
	}
	else
	{
		strcpy(FileName, argv[argbase + 0]);
		fprintf(stderr, "%s\n", FileName);
	}
	if (! strlen(FileName))
		return 0;

	if (! SilentMode)
		fprintf(stderr, "Step Size (default: %u):\t", STEP_SIZE);
	if (argc <= argbase + 1)
	{
		fgets(InputTxt, sizeof(InputTxt), stdin);
	}
	else
	{
		strcpy(InputTxt, argv[argbase + 1]);
		if (! SilentMode)
			fprintf(stderr, "%s\n", InputTxt);
	}
	TempLng = strtoul(InputTxt, NULL, 0);
	if (TempLng)
		STEP_SIZE = TempLng;

	if (! SilentMode)
		fprintf(stderr, "Minimum Number of matching Commands (default: %u):\t", MIN_EQU_SIZE);
	if (argc <= argbase + 2)
	{
		fgets(InputTxt, sizeof(InputTxt), stdin);
	}
	else
	{
		strcpy(InputTxt, argv[argbase + 2]);
		if (! SilentMode)
			fprintf(stderr, "%s\n", InputTxt);
	}
	TempLng = strtoul(InputTxt, NULL, 0);
	if (TempLng)
		MIN_EQU_SIZE = TempLng;

	if (! SilentMode)
		fprintf(stderr, "Start Pos (default: %u - auto):\t", START_POS);
	if (argc <= argbase + 3)
	{
		fgets(InputTxt, sizeof(InputTxt), stdin);
	}
	else
	{
		strcpy(InputTxt, argv[argbase + 3]);
		if (! SilentMode)
			fprintf(stderr, "%s\n", InputTxt);
	}
	TempLng = strtoul(InputTxt, NULL, 0);
	if (TempLng)
		START_POS = TempLng;

	if (! OpenVGMFile(FileName))
	{
		fprintf(stderr, "Error opening the file!\n");
		ErrVal = 1;
		goto EndProgram;
	}
	fprintf(stderr, "\n");

	ReadVGMData();
	free(VGMData);

	FindEqualitiesVGM();

	free(VGMCommand);

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
	//if (VGMHead.lngGD3Offset)
	//	VGMHead.lngGD3Offset += 0x00000014;
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

static void ReadVGMData(void)
{
	UINT8 ChipID;
	UINT8 Command;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	UINT32 CmdLen;
	bool StopVGM;
	VGM_CMD* TempCmd;
	UINT32 CurCmd;	// this variable is just for debugging

	fprintf(stderr, "Counting Commands ...");
	VGMPos = VGMHead.lngDataOffset;

	VGMCmdCount = 0x00;
	StopVGM = false;
	while(VGMPos < VGMHead.lngEOFOffset)
	{
		CmdLen = 0x00;
		Command = VGMData[VGMPos + 0x00];

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
			case 0x50:	// SN76496 write
				CmdLen = 0x02;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMData[VGMPos + 0x02];
				memcpy(&TempLng, &VGMData[VGMPos + 0x03], 0x04);
				TempLng &= 0x7FFFFFFF;

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
			case 0x41: // K007232 write
				CmdLen = 0x03;
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
		//TempByt = (CmdLen > 0x01) ? VGMData[VGMPos + 0x01] : 0x00;
		//if (! IgnoredCmd(Command, TempByt))
		if (! IgnoredCmd(VGMData + VGMPos))
			VGMCmdCount ++;

		VGMPos += CmdLen;
		if (StopVGM)
			break;
	}
	fprintf(stderr, "  %u\n", VGMCmdCount);

	// this includes the EOF command (the print-function needs it)
	VGMCommand = (VGM_CMD*)malloc((VGMCmdCount + 0x01) * sizeof(VGM_CMD));

	if (! SilentMode)
		fprintf(stderr, "Reading Commands ...");
	VGMPos = VGMHead.lngDataOffset;
	VGMSmplPos = 0;

	CurCmd = 0x00;
	TempCmd = VGMCommand;
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
			case 0x3F:
				if (VGMHead.lngHzPSG & 0x40000000)
				{
					Command += 0x10;
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
			case 0xAE:
			case 0xAF:
				if (VGMHead.lngHzYMF262 & 0x40000000)
				{
					Command -= 0x50;
					ChipID = 0x01;
				}
				break;
			case 0xAD:
				if (VGMHead.lngHzYMZ280B & 0x40000000)
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
				memcpy(&TempSht, &VGMData[VGMPos + 0x01], 0x02);
				VGMSmplPos += TempSht;
				CmdLen = 0x03;
				break;
			case 0x50:	// SN76496 write
				CmdLen = 0x02;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMData[VGMPos + 0x02];
				memcpy(&TempLng, &VGMData[VGMPos + 0x03], 0x04);
				TempLng &= 0x7FFFFFFF;

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
        case 0x41: // K007232 write
            CmdLen = 0x03;
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
					fprintf(stderr, "Unknown Command: %X\n", Command);
					Command = 0x6F;
					CmdLen = 0x01;
					//StopVGM = true;
					break;
				}
				break;
			}
		}
		TempByt = (CmdLen > 0x01) ? VGMData[VGMPos + 0x01] : 0x00;
		//if (StopVGM || ! IgnoredCmd(Command, TempByt))
		if (StopVGM || ! IgnoredCmd(VGMData + VGMPos))
		{
			TempCmd->Pos = VGMPos;
			TempCmd->Sample = VGMSmplPos;
			TempCmd->Len = (UINT16)CmdLen;
			TempCmd->Command = VGMData[VGMPos + 0x00];
			TempCmd->Value = 0x00;
			for (TempByt = 0x01; TempByt < CmdLen; TempByt ++)
				TempCmd->Value |= VGMData[VGMPos + TempByt] << ((CmdLen - TempByt - 0x01) * 8);
			CurCmd ++;
			TempCmd ++;
		}

		VGMPos += CmdLen;
		if (StopVGM)
			break;
	}
	if (! SilentMode)
		fprintf(stderr, "  Done.\n");

	return;
}

static void FindEqualitiesVGM(void)
{
	UINT32 CmpStart;
	UINT32 SrcStart;
	UINT32 SrcCmd;
	UINT32 CmpCmd;
	UINT32 CurCmd;
	//UINT8 CmpMode;
	bool CmpResult;
#ifdef WIN32
	DWORD PrintTime;
#endif

	CurCmd = 0x00;
	// Seek to start-pos
	while(VGMCommand[CurCmd].Pos < START_POS && CurCmd < VGMCmdCount)
		CurCmd ++;

	if (! LabelMode)
	{
#ifdef TECHNICAL_OUTPUT
		printf("     Source Block\t      Block Copy\t   Copy Information\n");
		printf("Start\tEnd\tSmpl\tStart\tEnd\tSmpl\tLength\tCmds\tSamples\n");
#else
		printf("  Source Block\t\t  Block Copy\t\tCopy Information\n");
		printf("Start\t  Time\t\tStart\t  Time\t\tCmds\tTime\n");
#endif
	}

	EndPosCount = 0;
	EndPosArr = (UINT32*)malloc(0x4000 * sizeof(UINT32));
	LabelID = 0;
#ifdef WIN32
	PrintTime = 0;
#endif

	while(CurCmd < VGMCmdCount)
	{
		CmpStart = CurCmd;
#if 0
		// Old routine
		// Works (and is faster), but doesn't find all loops (or finds them always at the first possible spot)
		CmpMode = 0x00;
		for (SrcCmd = CurCmd + 0x01; SrcCmd < VGMCmdCount; SrcCmd ++)
		{
			switch(CmpMode)
			{
			case 0x00:
				CmpResult = CompareVGMCommand(VGMCommand + SrcCmd, VGMCommand + CmpStart);
				if (CmpResult)
				{
					CmpMode = 0x01;
					SrcStart = SrcCmd;
					CmpCmd = CmpStart + 0x01;
				}
				break;
			case 0x01:
				CmpResult = CompareVGMCommand(VGMCommand + CmpCmd, VGMCommand + SrcCmd);
				if (! CmpResult)
				{
					EqualityCheck(CmpStart, SrcStart, CmpCmd - CmpStart);
					//CmpCmd = 0x00;
					CmpMode = 0x00;
				}
				else
				{
					CmpCmd ++;
				}
				break;
			}
		}

		if (CmpMode == 0x01)
		{
			EqualityCheck(CmpStart, SrcStart, CmpCmd - CmpStart);
			//CmpCmd = 0x00;
			//CmpMode = 0x00;
		}
#endif

		// New routine.
		// now with complexity O(n^3)
		for (SrcStart = CurCmd + 0x01; SrcStart < VGMCmdCount; SrcStart ++)
		{
			CmpResult = CompareVGMCommand(VGMCommand + SrcStart, VGMCommand + CmpStart);
			if (CmpResult)
			{
				SrcCmd = SrcStart + 0x01;
				CmpCmd = CmpStart + 0x01;
				for (; SrcCmd < VGMCmdCount; SrcCmd ++, CmpCmd ++)
				{
					CmpResult = CompareVGMCommand(VGMCommand + CmpCmd, VGMCommand + SrcCmd);
					if (! CmpResult)
					{
						EqualityCheck(CmpStart, SrcStart, CmpCmd - CmpStart);
						break;
					}
				}
				if (CmpResult)
					EqualityCheck(CmpStart, SrcStart, CmpCmd - CmpStart);
			}
		}

#ifdef WIN32
		if (PrintTime < GetTickCount())
		{
			if (! SilentMode)
				fprintf(stderr, "%.3f %% - %u / %u\r",
						100.0 * CurCmd / VGMCmdCount, CurCmd, VGMCmdCount);
			PrintTime = GetTickCount() + 500;
		}
#endif

		CurCmd += STEP_SIZE;
	}
	if (! SilentMode)
		fprintf(stderr, "\t\t\t\t\r");
	fprintf(stderr, "Done.\n");
	if (! SilentMode)
		fprintf(stderr, "\n");

	return;
}

static bool EqualityCheck(UINT32 CmpCmd, UINT32 SrcCmd, UINT32 CmdCount)
{
	UINT32 CurPosItm;
	UINT8 BlkFlags;
	const char ExtraChr[0x04] = {0x00, 'f', 'e', '!'};
	VGM_CMD* CmdSrcS;
	VGM_CMD* CmdSrcE;
	VGM_CMD* CmdCpyS;
	VGM_CMD* CmdCpyE;
#ifndef TECHNICAL_OUTPUT
	char TempStr[0x10];
#endif

	if (CmdCount < MIN_EQU_SIZE)
		return false;

	for (CurPosItm = 0x00; CurPosItm < EndPosCount; CurPosItm ++)
	{
		if (EndPosArr[CurPosItm] == SrcCmd + CmdCount)
			return false;
	}

	EndPosArr[EndPosCount] = SrcCmd + CmdCount;
	EndPosCount ++;

	CmdSrcS = &VGMCommand[CmpCmd];
	CmdSrcE = &VGMCommand[CmpCmd + CmdCount];
	CmdCpyS = &VGMCommand[SrcCmd];
	CmdCpyE = &VGMCommand[SrcCmd + CmdCount];
	BlkFlags = 0x00;
	if (CmdSrcE->Pos >= CmdCpyS->Pos)
		BlkFlags |= 0x01;	// Notify user that this may be a good loop
	if (SrcCmd + CmdCount >= VGMCmdCount)
		BlkFlags |= 0x02;	// Notify user that it matched until the End of File

	if (LabelMode)
	{
		LabelID ++;
		printf("%g\t%g\tLoop %d (%d cmds)\n", CmdSrcS->Sample / 44100.0,
				CmdCpyS->Sample / 44100.0, LabelID, CmdCount);
	}
	else
	{
		//	     Source Block             Block Copy           Copy Information
		//	Start   End     Smpl    Start   End     Smpl    Length  Cmds    Samples

		//	printf("%X\t%X\t%u\t%X\t%X\t%u\t%X\t%u\t%u\n",
		//			CmpStart, CmpPos - 0x01, TimeSmplC, SrcStart, SrcCmd, CmpTimeB,
		//			CmpPos - CmpStart, CmpCnt, CmpTime);
#ifdef TECHNICAL_OUTPUT
		printf("%X\t%X\t", CmdSrcS->Pos, CmdSrcE->Pos - 0x01);
		if (BlkFlags)
			printf("\b%c", ExtraChr[BlkFlags]);
		printf("%u\t%X\t%X\t%u\t%X\t%u\t%u\n",
				CmdSrcS->Sample, CmdCpyS->Pos, CmdCpyE->Pos - 0x01, CmdCpyS->Sample,
				CmdSrcE->Pos - CmdSrcS->Pos, CmdCount, CmdSrcE->Sample - CmdSrcS->Sample);
#else
		PrintMinSec(CmdSrcS->Sample, TempStr);
		//printf("%X\t%s", CmdSrcS->Pos, TempStr);
		printf("%u\t%s", CmdSrcS->Sample, TempStr);
		if (BlkFlags)
			printf("  %c", ExtraChr[BlkFlags]);

		PrintMinSec(CmdCpyS->Sample, TempStr);
		//printf("\t%X\t%s", CmdCpyS->Pos, TempStr);
		printf("\t%u\t%s", CmdCpyS->Sample, TempStr);

		PrintMinSec(CmdSrcE->Sample - CmdSrcS->Sample, TempStr);
		printf("\t%u\t%s\n", CmdCount, TempStr);
#endif
	}

	return true;
}

INLINE bool CompareVGMCommand(VGM_CMD* CmdA, VGM_CMD* CmdB)
{
	if (CmdA->Command != CmdB->Command)
		return false;
	if (CmdA->Value != CmdB->Value)
		return false;
	return true;
}

//INLINE bool IgnoredCmd(UINT8 Command, UINT8 RegData)
INLINE bool IgnoredCmd(const UINT8* VGMPnt)
{
#define Command	VGMPnt[0x00]
#define RegData	VGMPnt[0x01]
#define RegVal	VGMPnt[0x02]
	if (Command >= 0x60 && Command <= 0x6F)
		return true;	// Delays, Data Block etc.
	if (Command >= 0x70 && Command <= 0x8F)
		return true;	// 1-16 Sample Delay and YM2612 DAC Write + 0-15 Sample Delay
	if ((Command == 0x52 || Command == 0x53 || Command == 0x55 || Command == 0x56 || Command == 0x58) &&
		(RegData == 0x2A || (RegData >= 0x24 && RegData <= 0x27) || (RegData & 0xBC) == 0xB4 ||
		(RegData >= 0x0E && RegData <= 0x0F)))
		return true;	// YM2612 DAC or OPN Timer or SSG Port Write
	if (Command == 0x58 && (RegData == 0x1C))
		return true;	// YM2610 Flag Control

//	if ((Command == 0x52 || Command == 0x53) &&
//		(RegData & 0xBC) == 0xB4)
//		return true;	// YM2612 Stereo
	if (Command == 0x58 && (RegData >= 0x19 && RegData <= 0x1B))
		return true;	// YM2610 DELTA-T: Delta-N
	//if (Command == 0x59 && (RegData >= 0x00 && RegData <= 0x2F))
	//	return true;	// YM2610 ADPCM
	if (Command == 0x58 && (RegData >= 0x00 && RegData <= 0x05))
		return true;	// YM2610 SSG Freq
	if (Command == 0x58 && (RegData >= 0x08 && RegData <= 0x0A))
		return true;	// YM2610 SSG Vol

	if (Command == 0x54 && (RegData >= 0x10 && RegData <= 0x14))
		return true;	// YM2151 Timer
	if (((Command >= 0x5A && Command <= 0x5C) || Command == 0x5E) &&
		(RegData >= 0x02 && RegData <= 0x04))
		return true;	// OPL Timer Registers
	if (Command == 0x5D && (RegData & 0xE3) == 0x03)
		return true;	// YMZ280B Pan Register
	if (Command == 0xC1 || Command == 0xC2)
		return true;	// RF5C68 Memory Write
	if (Command == 0xA0 && (RegData >= 0x0E && RegData <= 0x0F))
		return true;	// AY8910 Port Write
	if ((Command == 0xB0 || Command == 0xB1) && RegData == 0x07)
		return true;	// RF5C68 Bank Register
	if (Command == 0xB2 && ((RegData & 0xF0) >= 0x20 && (RegData & 0xF0) <= 0x40))
		return true;	// PWM Channel Write
	if (Command == 0xD1 && RegData == 0x06)
		return true;	// YMF271 Timer Registers (and Group-Reg actually)
	if (Command == 0xD4 && ((RegData & 0x7F) == 0x01 && (RegVal & 0xF0) == 0xF0))
		return true;	// C140 Bank Writes and unknown Regs (Timer?)
	if (Command == 0xB7 && RegData == 0x01)
		return true;	// OKIM6258 ADPCM Data
	if (Command == 0xB5 && RegData >= 0x01)
		return true;	// MultiPCM "Set Slot"

	/*if (Command == 0xBA)
	{
		if ((RegData & 0x07) == 0x07 || RegData == 0x2A)
			return true;	// TODO: remove
	}
	else
		return true;*/
	if (Command == 0xC4)
	{
		// Hack for Super Street Fighter 2
		if (VGMPnt[0x03] < 0x80)
		{
			//if ((VGMPnt[0x03] & 0x07) == 0x06 || !((VGMPnt[0x03] & 0x07) == 0x02 && (RegData || RegVal)))
			//if (((VGMPnt[0x03] & 0x07) != 0x02 || (RegData || RegVal)))
			//if ((VGMPnt[0x03] & 0x07) != 0x06)
			//	return true;
		}
		else
		{
			//if (VGMPnt[0x03] == 0x93 || VGMPnt[0x03] == 0xD9)
				return true;
		}
	}

	return false;
}
