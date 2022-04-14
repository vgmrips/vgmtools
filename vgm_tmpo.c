// vgm_tmpo.c - VGM Tempo Scaler
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <zlib.h>

#include "stdtype.h"
#include "stdbool.h"
#include "VGMFile.h"
#include "common.h"


int main(int argc, char* argv[]);
static UINT32 GetGZFileLength(const char* FileName);
static bool OpenVGMFile(const char* FileName, bool* Compressed);
static bool WriteVGMFile(const char* FileName, bool Compress);
static UINT8 PreparseCommands(int ArgCount, char* ArgList[]);
static UINT8 ParseScaleTempoCommand(const char* StripCmd);
static UINT8 PatchVGM(int ArgCount, char* ArgList[]);
static void ScaleVGMData(void);


VGM_HEADER VGMHead;
UINT32 RealHdrSize;
UINT32 RealCpySize;
UINT32 VGMDataLen;
UINT8* VGMData;
double TempoScale;
#ifdef WIN32
FILETIME VGMDate;
#endif
bool KeepDate;

int main(int argc, char* argv[])
{
	int CmdCnt;
	int CurArg;
	// if SourceFile is NOT compressed, FileCompr = false -> DestFile will be uncompressed
	// if SourceFile IS compressed, FileCompr = true -> DestFile will also be compressed
	bool FileCompr;
	int ErrVal;
	UINT8 RetVal;

	printf("VGM Tempo Scaler\n----------------\n");

	ErrVal = 0;
	if (argc <= 1)
	{
		printf("Usage: vgm_tmpo [-command1] [-command2] file1.vgm file2.vgz ...\n");
		printf("Use argument -help for command list.\n");
		goto EndProgram;
	}
	else if (! stricmp(argv[1], "-Help"))
	{
		printf("Help\n----\n");
		printf("Usage: vgm_tmpo [-command1] [-command2] file1.vgm file2.vgz\n");
		printf("\n");
		printf("General Commands:\n");
		printf("    -Help         Show this help\n");
		printf("    -ScaleTempo   Scale the playback speed by adjusting delays\n");
		printf("                   e.g.: -ScaleTempo:1.02    (2%% faster)\n");
		printf("                         -ScaleTempo:0.98    (2%% slower)\n");
		printf("\n");

		printf("Command names are case insensitive.\n");
		goto EndProgram;
	}

	for (CmdCnt = 1; CmdCnt < argc; CmdCnt ++)
	{
		if (*argv[CmdCnt] != '-')
			break;	// skip all commands
	}
	if (CmdCnt < 2)
	{
		printf("Error: No commands specified!\n");
		goto EndProgram;
	}
	if (CmdCnt >= argc)
	{
		printf("Error: No files specified!\n");
		goto EndProgram;
	}

	PreparseCommands(CmdCnt - 1, argv + 1);
	for (CurArg = CmdCnt; CurArg < argc; CurArg ++)
	{
		printf("File: %s ...\n", argv[CurArg]);
		if (! OpenVGMFile(argv[CurArg], &FileCompr))
		{
			printf("Error opening file %s!\n", argv[CurArg]);
			printf("\n");
			ErrVal |= 1;	// There was at least 1 opening-error.
			continue;
		}

		RetVal = PatchVGM(CmdCnt - 1, argv + 1);
		if (RetVal & 0x80)
		{
			if (RetVal == 0x80)
			{
				ErrVal |= 8;
				goto EndProgram;	// Argument Error
			}
			ErrVal |= 4;	// At least 1 file wasn't patched.
		}
		else if (RetVal & 0x7F)
		{
			if (! WriteVGMFile(argv[CurArg], FileCompr))
			{
				printf("Error opening file %s!\n", argv[CurArg]);
				ErrVal |= 2;	// There was at least 1 writing-error.
			}
		}
		printf("\n");
	}

EndProgram:
	DblClickWait(argv[0]);

	return ErrVal;
}

static UINT32 GetGZFileLength(const char* FileName)
{
	FILE* hFile;
	UINT32 FileSize;
	UINT16 gzHead;

	hFile = fopen(FileName, "rb");
	if (hFile == NULL)
		return 0xFFFFFFFF;

	fread(&gzHead, 0x02, 0x01, hFile);

	if (gzHead != 0x8B1F)
	{
		// normal file
		fseek(hFile, 0x00, SEEK_END);
		FileSize = ftell(hFile);
	}
	else
	{
		// .gz File
		fseek(hFile, -4, SEEK_END);
		fread(&FileSize, 0x04, 0x01, hFile);
	}

	fclose(hFile);

	return FileSize;
}

static bool OpenVGMFile(const char* FileName, bool* Compressed)
{
	gzFile hFile;
#ifdef WIN32
	HANDLE hFileWin;
#endif
	UINT32 FileSize;
	UINT32 CurPos;
	UINT32 TempLng;

#ifdef WIN32
	hFileWin = CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
							OPEN_EXISTING, 0, NULL);
	if (hFileWin != INVALID_HANDLE_VALUE)
	{
		GetFileTime(hFileWin, NULL, NULL, &VGMDate);
		CloseHandle(hFileWin);
	}
#endif
	KeepDate = true;
	FileSize = GetGZFileLength(FileName);

	hFile = gzopen(FileName, "rb");
	if (hFile == NULL)
		return false;

	gzseek(hFile, 0x00, SEEK_SET);
	gzread(hFile, &TempLng, 0x04);
	if (TempLng != FCC_VGM)
		goto OpenErr;

	*Compressed = ! gzdirect(hFile);

	gzseek(hFile, 0x00, SEEK_SET);
	gzread(hFile, &VGMHead, sizeof(VGM_HEADER));
	ZLIB_SEEKBUG_CHECK(VGMHead);

	// I skip the Header preperations. I'll deal with that later

	if (VGMHead.lngVersion < 0x150)
		RealHdrSize = 0x40;
	else
		RealHdrSize = 0x34 + VGMHead.lngDataOffset;
	TempLng = sizeof(VGM_HEADER);
	if (TempLng > RealHdrSize)
		memset((UINT8*)&VGMHead + RealHdrSize, 0x00, TempLng - RealHdrSize);


	if (VGMHead.lngExtraOffset)
	{
		CurPos = 0xBC + VGMHead.lngExtraOffset;
		if (CurPos < RealHdrSize)
		{
			if (CurPos < TempLng)
				memset((UINT8*)&VGMHead + CurPos, 0x00, TempLng - CurPos);
			RealHdrSize = CurPos;
		}
	}
	// never copy more bytes than the structure has
	RealCpySize = (RealHdrSize <= TempLng) ? RealHdrSize : TempLng;

	// Read Data
	if (*Compressed)
		VGMDataLen = 0x04 + VGMHead.lngEOFOffset;	// size from EOF offset
	else
		VGMDataLen = FileSize;	// size of the actual file
	VGMData = (UINT8*)malloc(VGMDataLen);
	if (VGMData == NULL)
		goto OpenErr;
	gzseek(hFile, 0x00, SEEK_SET);
	gzread(hFile, VGMData, VGMDataLen);

	gzclose(hFile);
	return true;

OpenErr:

	gzclose(hFile);
	return false;
}

static bool WriteVGMFile(const char* FileName, bool Compress)
{
	union {
		gzFile gz;
		FILE *f;
	} hFile;
#ifdef WIN32
	HANDLE hFileWin;
#endif

	if (! Compress)
		hFile.f = fopen(FileName, "wb");
	else
		hFile.gz = gzopen(FileName, "wb9");
	if (hFile.f == NULL)
		return false;

	// Write VGM Data (including GD3 Tag)
	if (! Compress)
	{
		fseek(hFile.f, 0x00, SEEK_SET);
		fwrite(VGMData, 0x01, VGMDataLen, hFile.f);
	}
	else
	{
		gzseek(hFile.gz, 0x00, SEEK_SET);
		gzwrite(hFile.gz, VGMData, VGMDataLen);
	}

	if (! Compress)
		fclose(hFile.f);
	else
		gzclose(hFile.gz);

	if (KeepDate)
	{
#ifdef WIN32
		hFileWin = CreateFile(FileName, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
								OPEN_EXISTING, 0, NULL);
		if (hFileWin != INVALID_HANDLE_VALUE)
		{
			SetFileTime(hFileWin, NULL, NULL, &VGMDate);
			CloseHandle(hFileWin);
		}
#endif
	}

	printf("File written.\n");

	return true;
}

static UINT8 PreparseCommands(int ArgCount, char* ArgList[])
{
	int CurArg;
	char* CmdStr;
	char* CmdData;
	UINT8 RetVal;
	char ChrBak;

	for (CurArg = 0; CurArg < ArgCount; CurArg ++)
	{
		CmdStr = ArgList[CurArg] + 1;	// Skip the '-' at the beginning

		CmdData = strchr(CmdStr, ':');
		if (CmdData != NULL)
		{
			// actually this is quite dirty ...
			ChrBak = *CmdData;
			*CmdData = 0x00;
			CmdData ++;
		}

		if (! stricmp(CmdStr, "ScaleTempo"))
		{
			RetVal = ParseScaleTempoCommand(CmdData);
			if (RetVal)
				return RetVal;
		}
		if (CmdData != NULL)
		{
			// ... and this even more
			CmdData --;
			*CmdData = ChrBak;
		}
	}

	return 0x00;
}

static UINT8 ParseScaleTempoCommand(const char* ScaleTempoCmd)
{
	char* endptr = (char*)ScaleTempoCmd;
	TempoScale = strtod(ScaleTempoCmd, &endptr);
	return 0;
}

static UINT8 PatchVGM(int ArgCount, char* ArgList[])
{
	int CurArg;
	char* CmdStr;
	char* CmdData;
	char ChrBak;
	UINT8 RetVal;
	UINT8 ResVal;

	// Execute Commands
	ResVal = 0x00;	// nothing done - skip writing
	//if (! ArgCount)
	//	ShowHeader();
	for (CurArg = 0; CurArg < ArgCount; CurArg ++)
	{
		CmdStr = ArgList[CurArg] + 1;	// Skip the '-' at the beginning

		CmdData = strchr(CmdStr, ':');
		if (CmdData != NULL)
		{
			// and the same dirt here again
			ChrBak = *CmdData;
			*CmdData = 0x00;
			CmdData ++;
		}

		RetVal = 0x00;
		if (CmdData != NULL)
			printf("%s: %s\n", CmdStr, CmdData);
		else
			printf("%s\n", CmdStr);

		if (! stricmp(CmdStr, "ScaleTempo"))
		{
			printf("Scaling tempo ...");
			ScaleVGMData();
			RetVal |= 0x10;
		}
		else
		{
			printf("Error - Unknown Command: -%s\n", CmdStr);
			return 0x80;
		}
		if (RetVal & 0x10)
		{
			KeepDate = false;
		}
		ResVal |= RetVal;

		if (CmdData != NULL)
		{
			CmdData --;
			*CmdData = ChrBak;
		}
	}

	if (ResVal & 0x10)
	{
		// Write VGM Header
		memcpy(&VGMData[0x00], &VGMHead, RealCpySize);
	}

	return ResVal;
}

static void ScaleVGMData(void)
{
	UINT32 VGMPos;
	UINT8* DstData;
	UINT32 DstPos;
	UINT8 Command;
	UINT32 CmdDelay;
	UINT32 AllDelay;
	UINT16 TempSht;
	UINT32 TempLng;
	UINT32 LoopOfs;
	UINT32 CmdLen;
	bool StopVGM;
	bool WriteEvent;
	UINT32 NewLoopS;
	bool WroteCmd80;
	UINT8* VGMPnt;

	UINT32 CurrentPositionSamples = 0;
	UINT32 ScaledPositionSamples = 0;
	UINT32 ScaledLoopPositionSamples = 0;
	UINT32 ScaledDelay = 0;
	double ScaleFactor = 1.0 / TempoScale;

	DstData = (UINT8*)malloc(VGMDataLen * 2);
	AllDelay = 0;
	if (VGMHead.lngDataOffset)
		VGMPos = 0x34 + VGMHead.lngDataOffset;
	else
		VGMPos = 0x40;
	DstPos = VGMPos;
	LoopOfs = VGMHead.lngLoopOffset ? (0x1C + VGMHead.lngLoopOffset) : 0x00;
	NewLoopS = 0x00;
	memcpy(DstData, VGMData, VGMPos);	// Copy Header

	StopVGM = false;
	WroteCmd80 = false;
	while(VGMPos < VGMDataLen)
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
				CmdDelay = TempSht;
				WriteEvent = false;
				break;
			}
			CmdLen = 0x01;
		}
		else
		{
			VGMPnt = &VGMData[VGMPos];

			switch(Command)
			{
			case 0x50:	// SN76496 write
				CmdLen = 0x02;
				break;
			case 0x61:	// xx Sample Delay
				memcpy(&TempSht, &VGMPnt[0x01], 0x02);
				CmdDelay = TempSht;
				CmdLen = 0x03;
				WriteEvent = false;
				break;
			case 0x62:	// 1/60s delay
				TempSht = 735;
				CmdDelay = TempSht;
				CmdLen = 0x01;
				WriteEvent = false;
				break;
			case 0x63:	// 1/50s delay
				TempSht = 882;
				CmdDelay = TempSht;
				CmdLen = 0x01;
				WriteEvent = false;
				break;
			case 0x66:	// End Of File
				CmdLen = 0x01;
				StopVGM = true;
				break;
			case 0x67:	// PCM Data Stream
				memcpy(&TempLng, &VGMPnt[0x03], 0x04);
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
					printf("Unknown Command: %02X\n", Command);
					CmdLen = 0x01;
					//StopVGM = true;
					break;
				}
				break;
			}
		}

		if (WriteEvent || VGMPos == LoopOfs)
		{
			if (VGMPos != LoopOfs)
			{
				AllDelay += CmdDelay;
				CmdDelay = 0x00;
			}

			ScaledDelay = (UINT32)(((double)(CurrentPositionSamples + AllDelay)) * ScaleFactor) - ScaledPositionSamples;

			while(ScaledDelay)
			{
				if (ScaledDelay <= 0xFFFF)
					TempSht = (UINT16)ScaledDelay;
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

				//	if (TempSht >= 0x10 && TempSht <= 0x1F)
				//		TempSht = 0x0F;
				//	else if (TempSht >= 0x20)
				//		TempSht = 0x00;
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
				ScaledDelay -= TempSht;
				ScaledPositionSamples += TempSht;
			}
			CurrentPositionSamples += AllDelay;
			AllDelay = CmdDelay;
			CmdDelay = 0x00;

			if (VGMPos == LoopOfs) {
				NewLoopS = DstPos;
				ScaledLoopPositionSamples = ScaledPositionSamples;
			}

			if (WriteEvent)
			{
				// Write Event
				WroteCmd80 = ((Command & 0xF0) == 0x80);
				if (WroteCmd80)
				{
					AllDelay += Command & 0x0F;
					Command &= 0x80;
				}
				if (CmdLen != 0x01)
					memcpy(&DstData[DstPos], &VGMData[VGMPos], CmdLen);
				else
					DstData[DstPos] = Command;	// write the 0x80-command correctly
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
	}
	if (LoopOfs)
	{
		if (! NewLoopS)
		{
			printf("Error! Failed to relocate Loop Point!\n");
			NewLoopS = 0x1C;
		}
		VGMHead.lngLoopOffset = NewLoopS - 0x1C;
		VGMHead.lngLoopSamples = ScaledPositionSamples - ScaledLoopPositionSamples;
	}
	printf("\t\t\t\t\t\t\t\t\r");

	if (VGMHead.lngGD3Offset)
	{
		VGMPos = 0x14 + VGMHead.lngGD3Offset;
		memcpy(&TempLng, &VGMData[VGMPos + 0x00], 0x04);
		if (TempLng == FCC_GD3)
		{
			memcpy(&CmdLen, &VGMData[VGMPos + 0x08], 0x04);
			CmdLen += 0x0C;

			VGMHead.lngGD3Offset = DstPos - 0x14;
			memcpy(&DstData[DstPos], &VGMData[VGMPos], CmdLen);
			DstPos += CmdLen;
		}
	}
	VGMDataLen = DstPos;
	VGMHead.lngEOFOffset = VGMDataLen - 0x04;
	VGMHead.lngTotalSamples = ScaledPositionSamples;

	// PatchVGM will rewrite the header later

	VGMData = (UINT8*)realloc(VGMData, VGMDataLen);
	memcpy(VGMData, DstData, VGMDataLen);
	free(DstData);

	return;
}
