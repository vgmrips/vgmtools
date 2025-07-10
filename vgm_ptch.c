// vgm_ptch.c - VGM Patcher
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>	// for toupper()
#include <math.h>
#include <zlib.h>

#include "stdtype.h"
#include "stdbool.h"
#include "VGMFile.h"
#include "chip_strp.h"
#include "common.h"

// GD3 Tag Entry Count
#define GD3T_ENT_V100	11


int main(int argc, char* argv[]);
static UINT32 GetGZFileLength(const char* FileName);
static bool OpenVGMFile(const char* FileName, bool* Compressed);
static void ReadChipExtraData32(UINT32 StartOffset, VGMX_CHP_EXTRA32* ChpExtra);
static void ReadChipExtraData16(UINT32 StartOffset, VGMX_CHP_EXTRA16* ChpExtra);
static bool WriteVGMFile(const char* FileName, bool Compress);
static UINT8 PreparseCommands(int ArgCount, char* ArgList[]);
static UINT8 ParseStripCommand(const char* StripCmd);
static UINT8 PatchVGM(int ArgCount, char* ArgList[]);
static UINT8 GetLoopModVal(char* CmdData);
static UINT8 GetVolModVal(char* CmdData);
static bool ResizeVGMHeader(UINT32 NewSize);
static UINT32 CheckForMinVersion(void);
static UINT8 CheckVGMFile(UINT8 Mode);
static bool ChipCommandIsUnknown(UINT8 Command);
static bool ChipCommandIsValid(UINT8 Command);
static UINT32 RelocateVGMLoop(void);
static UINT32 CalcGD3Length(UINT32 DataPos, UINT16 TagEntries);
static void StripVGMData(void);
static bool StripClock(bool* StripAllPtr, UINT32* ClockPtr);


VGM_HEADER VGMHead;
UINT32 RealHdrSize;
UINT32 RealCpySize;
VGM_HDR_EXTRA VGMHeadX;
VGM_EXTRA VGMH_Extra;
UINT32 VGMDataLen;
UINT8* VGMData;
STRIP_DATA StripVGM[2];
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

	printf("VGM Patcher\n-----------\n");

	ErrVal = 0;
	if (argc <= 1)
	{
		printf("Usage: vgm_ptch [-command1] [-command2] file1.vgm file2.vgz ...\n");
		printf("Use argument -help for command list.\n");
		goto EndProgram;
	}
	else if (! stricmp(argv[1], "-Help"))
	{
		printf("Help\n----\n");
		printf("Usage: vgm_ptch [-command1] [-command2] file1.vgm file2.vgz\n");
		printf("\n");
		printf("General Commands:\n");
		//printf("(no command)  like -ShowTag\n");
		printf("    -Help         Show this help\n");
		printf("    -ChipCmdList  List all supported chips with SetHzxx command\n");
		printf("    -StripList    List all supported chips and channels for the Stip command\n");
		//printf("    -xxxxxxxxx  xxxxxxxxxxxxxxxxxx (following commands are ignored)\n");
		printf("\n");
		printf("Patching Commands:\n");
		printf("Command format: -command:value\n");
		printf("    -SetVer       Set Header-Version (e.g. 1.51, don't use with v1.00-1.10)\n");
		printf("    -UpdateVer    Update Header-Version (additional changes if neccessary)\n");
		printf("    -MinHeader    Minimize Header Size (v1.50+, useful after stripping chips)\n");
		printf("    -MinVer       Minimize VGM Version (v1.50+, useful after stripping chips)\n");
		printf("    -SetRate      Sets the Playback rate (v1.01+)\n");
		printf("    -SetHzxxx     Sets the xxx's chip clock (see -ChipCmdList for details)\n");
		printf("    -SetLoopMod   Set the Loop Modifier (Format: *2, /2.0, 0x20)\n");
		printf("    -SetLoopBase  Set the Loop Base (-128 to 127)\n");
		printf("    -SetVolMod    Set the Volume Modifier (Format: 1.0, 0x00)\n");
		printf("\n");
		_getch();

		printf("Commands to check the lengths (total, loop) and offsets (EOF, loop, GD3):\n");
		printf("    -Check        asks for correction\n");
		printf("    -CheckR       read only mode\n");
		printf("    -CheckL       autofix mode, recalculates lengths\n");
		printf("    -CheckO       like above, but tries to relocate loop offset\n");
		printf("\n");
		printf("    -Strip        Strip the data of a chip and/or channel\n");
		printf("                   Format: -Strip:Chip[-Num][:Ch,Ch,...];Chip\n");
		printf("                   e.g.: -Strip:PSG:0,1,2,Noise,Stereo;YM2151\n");
		printf("                         -Strip:YM2612-0;OKIM6295-1\n");
		printf("                   Note: Stripping does not (yet) work with all chips.\n");
		printf("\n");
		printf("                   Tip: Stripping without commands optimizes the delays.\n");
		printf("\n");

		printf("Command names are case insensitive.\n");
		goto EndProgram;
	}
	else if (! stricmp(argv[1], "-ChipCmdList"))
	{
		printf("Chip Commands\n-------------\n");
		printf("    -SetHzPSG      Sets the SN76496 (PSG) chip clock\n");
		printf("                    Set to (Hz or 0xC0000000) for T6W28 (NGP) mode (v1.51+)\n");
		printf("    -SetHzYM2413   Sets the YM2413 (OPLL) chip clock\n");
		printf("VGM v1.10+:\n");
		printf("    -SetPSG_FdB    Sets the SN76496 Feedback Mask\n");
		printf("    -SetPSG_SRW    Sets the SN76496 Shift Register Width\n");
		printf("    -SetHzYM2612   Sets the YM2612 (OPN2) chip clock\n");
		printf("    -SetHzYM2151   Sets the YM2151 (OPM) chip clock\n");
		printf("VGM v1.51+:\n");
		printf("    -SetPSG_Flags  Sets the SN76496 Flags\n");
		printf("    -SetHzSegaPCM  Sets the SegaPCM chip clock\n");
		printf("    -SetSPCMIntf   Sets the SegaPCM interface register\n");
		printf("    -SetHzRF5C68   Sets the RF5C68 chip clock\n");
		printf("    -SetHzYM2203   Sets the YM2203 (OPN) chip clock\n");
		printf("    -SetHzYM2608   Sets the YM2608 (OPNA) chip clock\n");
		printf("    -SetHzYM2610   Sets the YM2610 (OPNB) chip clock\n");
		printf("                    Set to (Hz or 0x80000000) for YM2610B-mode\n");
		printf("    -SetHzYM3812   Sets the YM3812 (OPL2) chip clock\n");
		printf("    -SetHzYM3526   Sets the YM3526 (OPL) chip clock\n");
		printf("    -SetHzY8950    Sets the Y8950 (MSX AUDIO) chip clock\n");
		printf("    -SetHzYMF262   Sets the YMF262 (OP3) chip clock\n");
		printf("    -SetHzYMF278B  Sets the YMF278B (OPL4) chip clock\n");
		printf("    -SetHzYMF271   Sets the YMF271 (OPLX) chip clock\n");
		printf("    -SetHzYMZ280B  Sets the YMZ280B chip clock\n");
		_getch();

		printf("    -SetHzRF5C164  Sets the RF5C164 (Sega MegaCD PCM) chip clock\n");
		printf("    -SetHzPWM      Sets the PWM chip clock\n");
		printf("    -SetHzAY8910   Sets the AY8910 chip clock\n");
		printf("    -SetAY8910Type     Sets the AY8910 chip type\n");
		printf("    -SetAY8910Flags    Sets the AY8910 Flags\n");
		printf("VGM v1.61+:\n");
		printf("    -SetHzGBDMG    Sets the GameBoy DMG chip clock\n");
		printf("    -SetHzNESAPU   Sets the NES APU chip clock\n");
		printf("    -SetHzMultiPCM Sets the MultiPCM chip clock\n");
		printf("    -SetHzUPD7759  Sets the UPD7759 chip clock\n");
		printf("    -SetHzOKIM6258 Sets the OKIM6258 chip clock\n");
		printf("    -SetOKIM6258Flags  Sets the OKIM6258 Flags\n");
		printf("    -SetHzOKIM6295 Sets the OKIM6295 chip clock\n");
		printf("    -SetHzSCC1     Sets the SCC1 (K051649) chip clock\n");
		printf("    -SetHzK054539  Sets the K054539 chip clock\n");
		printf("    -SetK054539Flags   Sets the K054539 Flags\n");
		printf("    -SetHzHuC6280  Sets the HuC6280 chip clock\n");
		printf("    -SetHzC140     Sets the C140 chip clock\n");
		printf("    -SetC140Type       Sets the C140 chip type\n");
		printf("    -SetHzK053260  Sets the K053260 chip clock\n");
		printf("    -SetHzPokey    Sets the Pokey chip clock\n");
		printf("    -SetHzQSound   Sets the QSound chip clock\n");
		printf("    -SetHzK007232  Sets the K007232 chip clock\n");
		printf("\n");
		printf("Setting a clock rate to 0 disables the chip.\n");
		goto EndProgram;
	}
	else if (! stricmp(argv[1], "-StripList"))
	{
		printf("    Chips          Channels\n");
		printf("    --------       --------\n");
		printf("    PSG/SN76496    0..3, Stereo\n");
		printf("    YM2413        *0..8, BD, SD, TT, TC, HH\n");
		printf("    YM2612         0..5, DAC\n");
		printf("    YM2151        *0..7\n");
		printf("    RF5C68         0..7, Mem\n");
		printf("    YM2203        *0..2, S0..S2\n");
		printf("    YM2608        *0..5, P0..P5, DT, S0..S2\n");
		printf("    YM2610        *0..5, P0..P5, DT, S0..S2\n");
		printf("    YM3812        *0..8, BD, SD, TT, TC, HH\n");
		printf("    YM3526        *0..8, BD, SD, TT, TC, HH\n");
		printf("    Y8950         *0..8, BD, SD, TT, TC, HH, DT\n");
		printf("    YMF262        *0..17, BD, SD, TT, TC, HH\n");
		printf("    YMF278B        -\n");
		printf("    YMF278B_FM    *0..17, BD, SD, TT, TC, HH\n");
		printf("    YMF278B_WT    *0..23\n");
		printf("    YMF271        *0..11\n");
		printf("    YMZ280B       *0..7\n");
		printf("    RF5C164        0..7, Mem\n");
		printf("    PWM            -\n");
		printf("    AY8910        *0..5\n");
		_getch();

		printf("    GBDMG         *0..3\n");
		printf("    NESAPU        *0..4\n");
		printf("    MultiPCM      *0..27\n");
		printf("    UPD7759        -\n");
		printf("    OKIM6258       -\n");
		printf("    OKIM6295      *0..3\n");
		printf("    SCC1/K051649  *0..4\n");
		printf("    K054539       *\n");
		printf("    HuC6280       *\n");
		printf("    C140           0..23, Other\n");
		printf("    K053260       *\n");
		printf("    Pokey         *\n");
		printf("    QSound        *0..15\n");
		printf("    K007232       *0..2\n");
		printf("    DacCtrl        0..255\n");
		printf("* strip whole chip only, no channel stripping\n");
		printf("\n");
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
		/*else if (! RetVal)
		{
			// Showed Tag - no write neccessary
		}*/
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

	memset(&VGMHeadX, 0x00, sizeof(VGM_HDR_EXTRA));
	memset(&VGMH_Extra, 0x00, sizeof(VGM_EXTRA));

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

	// Read Extra Header Data
	if (VGMHead.lngExtraOffset)
	{
		CurPos = 0xBC + VGMHead.lngExtraOffset;
		memcpy(&TempLng,	&VGMData[CurPos], 0x04);
		memcpy(&VGMHeadX,	&VGMData[CurPos], TempLng);
		CurPos += 0x04;

		if (VGMHeadX.Chp2ClkOffset)
			ReadChipExtraData32(CurPos + VGMHeadX.Chp2ClkOffset, &VGMH_Extra.Clocks);
		CurPos += 0x04;

		if (VGMHeadX.ChpVolOffset)
			ReadChipExtraData16(CurPos + VGMHeadX.ChpVolOffset, &VGMH_Extra.Volumes);
		CurPos += 0x04;
	}

	return true;

OpenErr:

	gzclose(hFile);
	return false;
}

static void ReadChipExtraData32(UINT32 StartOffset, VGMX_CHP_EXTRA32* ChpExtra)
{
	UINT32 CurPos;
	UINT8 CurChp;
	VGMX_CHIP_DATA32* TempCD;

	if (! StartOffset)
	{
		ChpExtra->ChipCnt = 0x00;
		ChpExtra->CCData = NULL;
		return;
	}

	CurPos = StartOffset;
	ChpExtra->ChipCnt = VGMData[CurPos];
	if (ChpExtra->ChipCnt)
		ChpExtra->CCData = (VGMX_CHIP_DATA32*)malloc(sizeof(VGMX_CHIP_DATA32) *
													ChpExtra->ChipCnt);
	else
		ChpExtra->CCData = NULL;
	CurPos ++;

	for (CurChp = 0x00; CurChp < ChpExtra->ChipCnt; CurChp ++)
	{
		TempCD = &ChpExtra->CCData[CurChp];
		TempCD->Type = VGMData[CurPos + 0x00];
		memcpy(&TempCD->Data, &VGMData[CurPos + 0x01], 0x04);
		CurPos += 0x05;
	}

	return;
}

static void ReadChipExtraData16(UINT32 StartOffset, VGMX_CHP_EXTRA16* ChpExtra)
{
	UINT32 CurPos;
	UINT8 CurChp;
	VGMX_CHIP_DATA16* TempCD;

	if (! StartOffset)
	{
		ChpExtra->ChipCnt = 0x00;
		ChpExtra->CCData = NULL;
		return;
	}

	CurPos = StartOffset;
	ChpExtra->ChipCnt = VGMData[CurPos];
	if (ChpExtra->ChipCnt)
		ChpExtra->CCData = (VGMX_CHIP_DATA16*)malloc(sizeof(VGMX_CHIP_DATA16) *
													ChpExtra->ChipCnt);
	else
		ChpExtra->CCData = NULL;
	CurPos ++;

	for (CurChp = 0x00; CurChp < ChpExtra->ChipCnt; CurChp ++)
	{
		TempCD = &ChpExtra->CCData[CurChp];
		TempCD->Type = VGMData[CurPos + 0x00];
		TempCD->Flags = VGMData[CurPos + 0x01];
		memcpy(&TempCD->Data, &VGMData[CurPos + 0x02], 0x02);
		CurPos += 0x04;
	}

	return;
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
	//UINT8 TempByt;
	//UINT16 TempSht;
	//UINT32 TempLng;

	memset(&StripVGM, 0x00, sizeof(STRIP_DATA) * 2);
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

		/*if (CmdData != NULL)
			printf("%s: %s\n", CmdStr, CmdData);
		else
			printf("%s\n", CmdStr);*/
		if (! stricmp(CmdStr, "Strip"))
		{
			RetVal = ParseStripCommand(CmdData);
			if (RetVal)
				return RetVal;
			// TODO: Parse "Strip"-Command
			//StripVGM[0].SN76496.All = true;
			//StripVGM[0].YM2612.All = true;
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

static UINT8 ParseStripCommand(const char* StripCmd)
{
	char* StripTmp;
	char* ChipPos;
	char* CpNumPos;
	char* ChnPos;
	char* NxtChip;
	char* StopPos;
	//char* TempStr;
	UINT8 StripMode;
	UINT8 CurChip;
	STRIP_GENERIC* TempChip;
	bool* TChipAll;
	//UINT8 TempByt;
	//UINT16 TempSht;
	//UINT32 TempLng;
	UINT8 ChnNum;
	UINT8 ChipMask;
	UINT8 CurCSet;

	if (StripCmd == NULL)
		return 0x00;	// Passing no argument is valid

	// Format: "ChipA:Chn1,Chn2,Chn3;ChipB-#;ChipC"
	StripTmp = strdup(StripCmd);
	ChipPos = StripTmp;
	while(ChipPos != NULL && *ChipPos != '\0')
	{
		ChnPos = strchr(ChipPos, ':');
		NxtChip = strchr(ChipPos, ';');
		StripMode = 0x00;
		if (NxtChip != NULL && ChnPos != NULL)
		{
			if (ChnPos < NxtChip)
				StripMode = 0x01;	// Strip Channel x,x,x
			else
				StripMode = 0x00;	// Strip All
		}
		else if (ChnPos == NULL)	// && (NxtChip != NULL || NxtChip == NULL)
		{
			StripMode = 0x00;	// Strip All
		}
		else if (ChnPos != NULL)	// && NxtChip == NULL
		{
			StripMode = 0x01;	// Last Entry - Strip Channels
		}

		switch(StripMode)
		{
		case 0x00:	// 00 - Strip All
			// nothing to do
			break;
		case 0x01:	// 01 - Strip Channels
			*ChnPos = 0x00;	// ChnPos can't be NULL
			ChnPos ++;
			break;
		}
		if (NxtChip != NULL)
		{
			*NxtChip = '\0';
			NxtChip ++;
		}

		// Detect the chip IDs to strip
		ChipMask = 0x03;

		CpNumPos = strchr(ChipPos, '-');
		if (CpNumPos != NULL)
		{
			*CpNumPos = '\0';
			CpNumPos ++;
			CurChip = (UINT8)strtol(CpNumPos, NULL, 10);
			if (CurChip < 2)
				ChipMask = 1 << CurChip;
		}

		// I can compare ChipPos, because I zeroed the split-char
		if (! stricmp(ChipPos, "PSG") || ! stricmp(ChipPos, "SN76496"))
		{
			CurChip = 0x00;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].SN76496;
		}
		else if (! stricmp(ChipPos, "YM2413"))
		{
			CurChip = 0x01;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].YM2413;
		}
		else if (! stricmp(ChipPos, "YM2612"))
		{
			CurChip = 0x02;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].YM2612;
		}
		else if (! stricmp(ChipPos, "YM2151"))
		{
			CurChip = 0x03;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].YM2151;
		}
		else if (! stricmp(ChipPos, "SegaPCM"))
		{
			CurChip = 0x04;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].SegaPCM;
		}
		else if (! stricmp(ChipPos, "RF5C68"))
		{
			CurChip = 0x05;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].RF5C68;
		}
		else if (! stricmp(ChipPos, "YM2203"))
		{
			CurChip = 0x06;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].YM2203;
		}
		else if (! stricmp(ChipPos, "YM2608"))
		{
			CurChip = 0x07;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].YM2608;
		}
		else if (! stricmp(ChipPos, "YM2610"))
		{
			CurChip = 0x08;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].YM2610;
		}
		else if (! stricmp(ChipPos, "YM3812"))
		{
			CurChip = 0x09;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].YM3812;
		}
		else if (! stricmp(ChipPos, "YM3526"))
		{
			CurChip = 0x0A;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].YM3526;
		}
		else if (! stricmp(ChipPos, "Y8950"))
		{
			CurChip = 0x0B;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].Y8950;
		}
		else if (! stricmp(ChipPos, "YMF262"))
		{
			CurChip = 0x0C;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].YMF262;
		}
		else if (! stricmp(ChipPos, "YMF278B"))
		{
			CurChip = 0x0D;
			if (StripMode == 0x00)
			{
				TempChip = NULL;
				TChipAll = &StripVGM[0].YMF278B_All;
			}
			else
			{
				TempChip = (STRIP_GENERIC*)&StripVGM[0].YMF278B_WT;
			}
		}
		else if (! stricmp(ChipPos, "YMF278B_WT"))
		{
			CurChip = 0x0D;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].YMF278B_WT;
		}
		else if (! stricmp(ChipPos, "YMF278B_FM"))
		{
			CurChip = 0x8D;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].YMF278B_FM;
		}
		else if (! stricmp(ChipPos, "YMF271"))
		{
			CurChip = 0x0E;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].YMF271;
		}
		else if (! stricmp(ChipPos, "YMZ280B"))
		{
			CurChip = 0x0F;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].YMZ280B;
		}
		else if (! stricmp(ChipPos, "RF5C164"))
		{
			CurChip = 0x10;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].RF5C164;
		}
		else if (! stricmp(ChipPos, "PWM"))
		{
			CurChip = 0x11;
			TempChip = NULL;
			TChipAll = &StripVGM[0].PWM.All;
		}
		else if (! stricmp(ChipPos, "AY8910"))
		{
			CurChip = 0x12;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].AY8910;
		}
		else if (! stricmp(ChipPos, "GBDMG"))
		{
			CurChip = 0x13;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].GBDMG;
		}
		else if (! stricmp(ChipPos, "NESAPU"))
		{
			CurChip = 0x14;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].NESAPU;
		}
		else if (! stricmp(ChipPos, "MultiPCM"))
		{
			CurChip = 0x15;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].MultiPCM;
		}
		else if (! stricmp(ChipPos, "UPD7759"))
		{
			CurChip = 0x16;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].UPD7759;
		}
		else if (! stricmp(ChipPos, "OKIM6258"))
		{
			CurChip = 0x17;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].OKIM6258;
		}
		else if (! stricmp(ChipPos, "OKIM6295"))
		{
			CurChip = 0x18;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].OKIM6295;
		}
		else if (! stricmp(ChipPos, "SCC1") || ! stricmp(ChipPos, "K051649"))
		{
			CurChip = 0x19;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].K051649;
		}
		else if (! stricmp(ChipPos, "K054539"))
		{
			CurChip = 0x1A;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].K054539;
		}
		else if (! stricmp(ChipPos, "HuC6280"))
		{
			CurChip = 0x1B;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].HuC6280;
		}
		else if (! stricmp(ChipPos, "C140"))
		{
			CurChip = 0x1C;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].C140;
		}
		else if (! stricmp(ChipPos, "K053260"))
		{
			CurChip = 0x1D;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].K053260;
		}
		else if (! stricmp(ChipPos, "Pokey"))
		{
			CurChip = 0x1E;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].Pokey;
		}
		else if (! stricmp(ChipPos, "QSound"))
		{
			CurChip = 0x1F;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].QSound;
		}
		else if (! stricmp(ChipPos, "K007232"))
		{
			CurChip = 0x2A;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].K007232;
		}
		else if (! stricmp(ChipPos, "DacCtrl"))
		{
			CurChip = 0x7F;
			TempChip = (STRIP_GENERIC*)&StripVGM[0].DacCtrl;
		}
		else
		{
			printf("Strip Error - Unknown Chip: %s\n", ChipPos);
			return 0x80;
		}

		for (CurCSet = 0; CurCSet < 2; CurCSet ++)
		{
			if (! (ChipMask & (1 << CurCSet)))
			{
				TempChip = (STRIP_GENERIC*)( (STRIP_DATA*)TempChip + 1 );
				continue;	// bit not set - skip
			}

			switch(StripMode)
			{
			case 0x00:	// 00 - Strip All
				if (TempChip == NULL)
					*TChipAll = true;
				else
					TempChip->All = true;
				break;
			case 0x01:	// 01 - Strip Channels
				// ChnPos contains the Channel-List
				ChipPos = ChnPos;
				while(ChipPos != NULL)
				{
					ChnPos = strchr(ChipPos, ',');
					if (ChnPos != NULL)
					{
						*ChnPos = '\0';
						ChnPos ++;
					}

					ChnNum = (UINT8)strtoul(ChipPos, &StopPos, 0);
					if (StopPos != ChipPos)
					{
						if (CurChip == 0x7F)
							StripVGM[CurCSet].DacCtrl.	// set the n-th bit in the array
								StrMsk[ChnNum >> 3] |= (1 << (ChnNum & 0x07));
						else
							TempChip->ChnMask |= (1 << ChnNum);
					}
					else
					{
						if (CurChip == 0x00 && ! stricmp(ChipPos, "Stereo"))
						{
							StripVGM[CurCSet].SN76496.Other |= 0x01;
						}
						else if (CurChip == 0x02 && ! stricmp(ChipPos, "DAC"))
						{
							TempChip->ChnMask |= (1 << 6);
						}
						else if (CurChip == 0x05 && ! stricmp(ChipPos, "Mem"))
						{
							StripVGM[CurCSet].RF5C68.Other |= 0x01;
						}
						else if (CurChip == 0x10 && ! stricmp(ChipPos, "Mem"))
						{
							StripVGM[CurCSet].RF5C164.Other |= 0x01;
						}
						else if (CurChip == 0x1C && ! stricmp(ChipPos, "Other"))
						{
							StripVGM[CurCSet].C140.Other |= 0x01;
						}
						else
						{
							printf("Unknown channel: %s\n", ChipPos);
						}
					}

					ChipPos = ChnPos;
				}
				break;
			}

			// advance by one STRIP_DATA
			TempChip = (STRIP_GENERIC*)( (STRIP_DATA*)TempChip + 1 );
		}

		ChipPos = NxtChip;
	}
	free(StripTmp);

	return 0x00;
}

static UINT8 PatchVGM(int ArgCount, char* ArgList[])
{
	int CurArg;
	char* CmdStr;
	char* CmdData;
	//UINT32 DataLen;
	char ChrBak;
	char* TempPnt;
	char TempStr[0x03];
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	INT32 TempSLng;
	UINT8 RetVal;
	UINT8 ResVal;
	UINT32 OldVal;
	UINT32 NewVal;
	UINT32* ChipHzPnt;
	bool LightChange;

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
		LightChange = false;
		if (CmdData != NULL)
			printf("%s: %s\n", CmdStr, CmdData);
		else
			printf("%s\n", CmdStr);
		if (! stricmp(CmdStr, "SetVer") || ! stricmp(CmdStr, "UpdateVer"))
		{
			if (CmdData == NULL)
				goto IncompleteArg;

			OldVal = VGMHead.lngVersion;

			TempPnt  = strchr(CmdData, '.');
			TempLng = strtoul(CmdData, NULL, 0x10);	// Version Major
			if (TempPnt)
			{
				strncpy(TempStr, TempPnt + 0x01, 0x02);
				TempStr[0x02] = 0x00;
				if (! TempStr[0x01])
					TempStr[0x01] = '0';
				TempByt = (UINT8)strtoul(TempStr, NULL, 0x10);	// Version Minor
			}
			else
			{
				TempByt = 0x00;
			}
			NewVal = (TempLng << 8) | (TempByt << 0);

			if (OldVal != NewVal)
			{
				if (! stricmp(CmdStr, "UpdateVer"))
				{
					// Up- and downdate to new version
					if (OldVal < NewVal)
					{
						// Update version (write missing header data)
						if (OldVal < 0x101 && NewVal >= 0x101)	// v1.00 -> v1.01+
						{
							VGMHead.lngRate = 0;
						}
						if (OldVal < 0x110 && NewVal >= 0x110)	// v1.01 -> v1.10+
						{
							VGMHead.shtPSG_Feedback = 0x0009;
							VGMHead.bytPSG_SRWidth = 0x10;
							VGMHead.lngHzYM2612 = VGMHead.lngHzYM2413;
							VGMHead.lngHzYM2151 = VGMHead.lngHzYM2413;
							// TODO: Scan vgm for used chips
						}
						if (OldVal < 0x150 && NewVal >= 0x150)	// v1.10 -> v1.50+
						{
							VGMHead.lngDataOffset = 0x0C;
						}
						if (OldVal < 0x151 && NewVal >= 0x151)	// v1.50 -> v1.51+
						{
							VGMHead.bytPSG_Flags = (0x00 << 0) | (0x00 << 1) | (0x00 << 2);
							VGMHead.lngHzSPCM = 0x0000;
							VGMHead.lngSPCMIntf = 0x00000000;
							// all others are zeroed by memset during OpenVGMFile
						}
					}
					else if (OldVal > NewVal)
					{
						// Downdate version (strip unneeded header data)
						if (OldVal >= 0x151 && NewVal < 0x151)	// v1.51+ -> v1.50
						{
							VGMHead.bytPSG_Flags = 0x00;
							TempLng = sizeof(VGM_HEADER) - 0x38;
							memset(&VGMHead.lngHzSPCM, 0x00, TempLng);
						}
						if (OldVal >= 0x150 && NewVal < 0x150)	// v1.50+ -> v1.10
						{
							if (VGMHead.lngDataOffset != 0x0C)
							{
								// I'm just too lazy to move all data
								// and no one would even THINK of downdating vgms
								// Update: Except for me ...
								//         (I wanted v1.51 -> 1.10 -> 1.50 for a smaller header)
								// Update2: Now this can be done by -MinHeader, but I'll keep this option
								ResizeVGMHeader(0x40);
							}
							VGMHead.lngDataOffset = 0x00;
						}
						if (OldVal >= 0x110 && NewVal < 0x110)	// v1.10+ -> v1.01
						{
							VGMHead.shtPSG_Feedback = 0x0000;
							VGMHead.bytPSG_SRWidth = 0x00;
							if (! VGMHead.lngHzYM2413)
							{
								if (VGMHead.lngHzYM2151)
									VGMHead.lngHzYM2413 = VGMHead.lngHzYM2151;
								else if (VGMHead.lngHzYM2612)
									VGMHead.lngHzYM2413 = VGMHead.lngHzYM2612;
							}
						}
						if (OldVal >= 0x101 && NewVal < 0x101)	// v1.01+ -> v1.00
						{
							VGMHead.lngRate = 0;
						}
					}

					VGMHead.lngVersion = NewVal;
					RetVal |= 0x10;
				}
				else //if (! stricmp(CmdStr, "SetVer"))
				{
					RetVal |= 0x10;
					if (NewVal >= 0x150 && ! VGMHead.lngDataOffset)
					{
						printf("The VGM has no data offset, but you're changing the Version to >= 1.50.\n");
						printf("This will create bad VGMs. Please use -UpdateVer instead!\n");
					}
					else if (NewVal < 0x150 && VGMHead.lngDataOffset)
					{
						printf("The VGM has a data offset set, but you're changing the Version to < 1.50.\n");
						printf("This will create bad VGMs. Please use -UpdateVer instead!\n");
					}

					if (RetVal & 0x10)
						VGMHead.lngVersion = NewVal;
				}
			}
		}
		else if (! stricmp(CmdStr, "MinHeader"))
		{
			if (VGMHead.lngVersion >= 0x150)
			{
#if 0
				NewVal = sizeof(VGM_HEADER);
				ChipHzPnt = (UINT32*)((UINT8*)&VGMHead + NewVal);
				while(ChipHzPnt > &VGMHead.lngDataOffset)
				{
					ChipHzPnt --;
					if (*ChipHzPnt)
						break;
					NewVal -= 0x04;
				}
#else
				NewVal = RealHdrSize;
				ChipHzPnt = (UINT32*)(&VGMData[NewVal]);
				while(ChipHzPnt > (UINT32*)&VGMData[0x34])
				{
					ChipHzPnt --;
					if (*ChipHzPnt)
						break;
					NewVal -= 0x04;
				}
#endif
				NewVal = (NewVal + 0x3F) >> 6 << 6;	// round up to 0x40 bytes;

				//OldVal = 0x34 + VGMHead.lngDataOffset;
				OldVal = RealHdrSize;
				if (NewVal < OldVal)
				{
					ResizeVGMHeader(NewVal);

					printf("Header Size Old: 0x%02X bytes, New: 0x%02X bytes\n", OldVal, NewVal);
					RetVal |= 0x10;
				}
			}
			else
			{
				printf("Warning! Command ignored - VGM version too low!\n");
			}
		}
		else if (! stricmp(CmdStr, "MinVer"))
		{
			if (VGMHead.lngVersion >= 0x150)
			{
				OldVal = VGMHead.lngVersion;
				NewVal = CheckForMinVersion();
				if (NewVal < OldVal)
				{
					VGMHead.lngVersion = NewVal;

					printf("VGM Version Old: %u.%02X, New: %u.%02X\n",
							OldVal >> 8, OldVal & 0xFF, NewVal >> 8, NewVal & 0xFF);
					RetVal |= 0x10;
				}
			}
			else
			{
				printf("Warning! Command ignored - VGM version too low!\n");
			}
		}
		else if (! stricmp(CmdStr, "ResizeHead"))
		{
			if (VGMHead.lngVersion >= 0x150)
			{
				OldVal = 0x38;
				NewVal = strtoul(CmdData, NULL, 0);
				if (NewVal >= OldVal)
				{
					//OldVal = 0x34 + VGMHead.lngDataOffset;
					OldVal = RealHdrSize;
					if (OldVal != NewVal)
					{
						ResizeVGMHeader(NewVal);
						printf("VGM Header Size Old: 0x%02X, New: 0x%02X\n", OldVal, NewVal);
						RetVal |= 0x10;
					}
				}
				else
				{
					printf("Invalid header size: 0x%02X! 0x%02X is minimum!\n", NewVal, OldVal);
				}
			}
			else
			{
				printf("Warning! Command ignored - VGM version too low!\n");
			}
		}
		else if (! stricmp(CmdStr, "SetRate"))
		{
			if (CmdData == NULL)
				goto IncompleteArg;

			NewVal = strtoul(CmdData, NULL, 0);
			if (NewVal != VGMHead.lngRate)
			{
				VGMHead.lngRate = NewVal;
				RetVal |= 0x10;
			}
		}
		else if (! strnicmp(CmdStr, "SetHz", 5))
		{
			if (CmdData == NULL)
				goto IncompleteArg;

			CmdStr += 5;
			ChipHzPnt = NULL;
			if (! stricmp(CmdStr, "PSG"))
			{
				ChipHzPnt = &VGMHead.lngHzPSG;
				OldVal = 0x100;
			}
			else if (! stricmp(CmdStr, "YM2413"))
			{
				ChipHzPnt = &VGMHead.lngHzYM2413;
				OldVal = 0x100;
			}
			else if (! stricmp(CmdStr, "YM2612"))
			{
				ChipHzPnt = &VGMHead.lngHzYM2612;
				OldVal = 0x110;
			}
			else if (! stricmp(CmdStr, "YM2151"))
			{
				ChipHzPnt = &VGMHead.lngHzYM2151;
				OldVal = 0x110;
			}
			else if (! stricmp(CmdStr, "SegaPCM"))
			{
				ChipHzPnt = &VGMHead.lngHzSPCM;
				OldVal = 0x151;
			}
			else if (! stricmp(CmdStr, "RF5C68"))
			{
				ChipHzPnt = &VGMHead.lngHzRF5C68;
				OldVal = 0x151;
			}
			else if (! stricmp(CmdStr, "YM2203"))
			{
				ChipHzPnt = &VGMHead.lngHzYM2203;
				OldVal = 0x151;
			}
			else if (! stricmp(CmdStr, "YM2608"))
			{
				ChipHzPnt = &VGMHead.lngHzYM2608;
				OldVal = 0x151;
			}
			else if (! stricmp(CmdStr, "YM2610"))
			{
				ChipHzPnt = &VGMHead.lngHzYM2610;
				OldVal = 0x151;
			}
			else if (! stricmp(CmdStr, "YM3812"))
			{
				ChipHzPnt = &VGMHead.lngHzYM3812;
				OldVal = 0x151;
			}
			else if (! stricmp(CmdStr, "YM3526"))
			{
				ChipHzPnt = &VGMHead.lngHzYM3526;
				OldVal = 0x151;
			}
			else if (! stricmp(CmdStr, "Y8950"))
			{
				ChipHzPnt = &VGMHead.lngHzY8950;
				OldVal = 0x151;
			}
			else if (! stricmp(CmdStr, "YMF262"))
			{
				ChipHzPnt = &VGMHead.lngHzYMF262;
				OldVal = 0x151;
			}
			else if (! stricmp(CmdStr, "YMF278B"))
			{
				ChipHzPnt = &VGMHead.lngHzYMF278B;
				OldVal = 0x151;
			}
			else if (! stricmp(CmdStr, "YMF271"))
			{
				ChipHzPnt = &VGMHead.lngHzYMF271;
				OldVal = 0x151;
			}
			else if (! stricmp(CmdStr, "YMZ280B"))
			{
				ChipHzPnt = &VGMHead.lngHzYMZ280B;
				OldVal = 0x151;
			}
			else if (! stricmp(CmdStr, "RF5C164"))
			{
				ChipHzPnt = &VGMHead.lngHzRF5C164;
				OldVal = 0x151;
			}
			else if (! stricmp(CmdStr, "PWM"))
			{
				ChipHzPnt = &VGMHead.lngHzPWM;
				OldVal = 0x151;
			}
			else if (! stricmp(CmdStr, "AY8910"))
			{
				ChipHzPnt = &VGMHead.lngHzAY8910;
				OldVal = 0x151;
			}
			else if (! stricmp(CmdStr, "GBDMG"))
			{
				ChipHzPnt = &VGMHead.lngHzGBDMG;
				OldVal = 0x161;
			}
			else if (! stricmp(CmdStr, "NESAPU"))
			{
				ChipHzPnt = &VGMHead.lngHzNESAPU;
				OldVal = 0x161;
			}
			else if (! stricmp(CmdStr, "MultiPCM"))
			{
				ChipHzPnt = &VGMHead.lngHzMultiPCM;
				OldVal = 0x161;
			}
			else if (! stricmp(CmdStr, "UPD7759"))
			{
				ChipHzPnt = &VGMHead.lngHzUPD7759;
				OldVal = 0x161;
			}
			else if (! stricmp(CmdStr, "OKIM6258"))
			{
				ChipHzPnt = &VGMHead.lngHzOKIM6258;
				OldVal = 0x161;
			}
			else if (! stricmp(CmdStr, "OKIM6295"))
			{
				ChipHzPnt = &VGMHead.lngHzOKIM6295;
				OldVal = 0x161;
			}
			else if (! stricmp(CmdStr, "SCC1") || ! stricmp(CmdStr, "K051649"))
			{
				ChipHzPnt = &VGMHead.lngHzK051649;
				OldVal = 0x161;
			}
			else if (! stricmp(CmdStr, "K054539"))
			{
				ChipHzPnt = &VGMHead.lngHzK054539;
				OldVal = 0x161;
			}
			else if (! stricmp(CmdStr, "HuC6280"))
			{
				ChipHzPnt = &VGMHead.lngHzHuC6280;
				OldVal = 0x161;
			}
			else if (! stricmp(CmdStr, "C140"))
			{
				ChipHzPnt = &VGMHead.lngHzC140;
				OldVal = 0x161;
			}
			else if (! stricmp(CmdStr, "K053260"))
			{
				ChipHzPnt = &VGMHead.lngHzK053260;
				OldVal = 0x161;
			}
			else if (! stricmp(CmdStr, "Pokey"))
			{
				ChipHzPnt = &VGMHead.lngHzPokey;
				OldVal = 0x161;
			}
			else if (! stricmp(CmdStr, "QSound"))
			{
				ChipHzPnt = &VGMHead.lngHzQSound;
				OldVal = 0x161;
			}
			else if (! stricmp(CmdStr, "K007232"))
			{
				ChipHzPnt = &VGMHead.lngHzK007232;
				OldVal = 0x172;
			}
			else
			{
				printf("Error - Unknown Chip: %s\n", CmdStr);
				return 0x80;
			}

			if (VGMHead.lngVersion >= OldVal)
			{
				NewVal = strtoul(CmdData, NULL, 0);

				if(!(NewVal & 0x3fffffff))
					NewVal |= *ChipHzPnt & 0x3fffffff;

				if (ChipHzPnt != NULL && NewVal != *ChipHzPnt)
				{
					*ChipHzPnt = NewVal;
					RetVal |= 0x10;
				}
			}
			else
			{
				printf("Warning! Command ignored - VGM version too low!\n");
			}
		}
		else if (! strnicmp(CmdStr, "SetPSG_", 7))
		{
			if (CmdData == NULL)
				goto IncompleteArg;

			CmdStr += 7;
			if (! stricmp(CmdStr, "FdB"))
			{
				if (VGMHead.lngVersion >= 0x110)
				{
					TempSht = (UINT16)strtoul(CmdData, NULL, 0);
					if (TempSht != VGMHead.shtPSG_Feedback)
					{
						VGMHead.shtPSG_Feedback = TempSht;
						RetVal |= 0x10;
					}
				}
				else
				{
					printf("Warning! Command ignored - VGM version too low!\n");
				}
			}
			else if (! stricmp(CmdStr, "SRW"))
			{
				if (VGMHead.lngVersion >= 0x110)
				{
					TempByt = (UINT8)strtoul(CmdData, NULL, 0);
					if (TempByt != VGMHead.bytPSG_SRWidth)
					{
						VGMHead.bytPSG_SRWidth = TempByt;
						RetVal |= 0x10;
					}
				}
				else
				{
					printf("Warning! Command ignored - VGM version too low!\n");
				}
			}
			else if (! stricmp(CmdStr, "Flags"))
			{
				if (VGMHead.lngVersion >= 0x150)
				{
					TempByt = (UINT8)strtoul(CmdData, NULL, 0);
					if (TempByt != VGMHead.bytPSG_Flags)
					{
						if (VGMHead.lngVersion < 0x151)
							printf("Warning! Command executed, but VGM version too low!\n");
						VGMHead.bytPSG_Flags = TempByt;
						RetVal |= 0x10;
					}
				}
				else
				{
					printf("Warning! Command ignored - VGM version too low!\n");
				}
			}
			else
			{
				printf("Error - Unknown Command: -%s\n", CmdStr - 7);
				return 0x80;
			}
		}
		else if (! stricmp(CmdStr, "SetSPCMIntf"))
		{
			if (CmdData == NULL)
				goto IncompleteArg;

			if (VGMHead.lngVersion >= 0x151)
			{
				NewVal = strtoul(CmdData, NULL, 0);
				if (NewVal != VGMHead.lngSPCMIntf)
				{
					VGMHead.lngSPCMIntf = NewVal;
					RetVal |= 0x10;
				}
			}
			else
			{
				printf("Warning! Command ignored - VGM version too low!\n");
			}
		}
		else if (! stricmp(CmdStr, "SetAY8910Type"))
		{
			if (CmdData == NULL)
				goto IncompleteArg;

			if (VGMHead.lngVersion >= 0x151)
			{
				TempByt = (UINT8)strtoul(CmdData, NULL, 0);
				if (TempByt != VGMHead.bytAYType)
				{
					VGMHead.bytAYType = TempByt;
					RetVal |= 0x10;
				}
			}
			else
			{
				printf("Warning! Command ignored - VGM version too low!\n");
			}
		}
		else if (! stricmp(CmdStr, "SetAY8910Flags"))
		{
			if (CmdData == NULL)
				goto IncompleteArg;

			if (VGMHead.lngVersion >= 0x151)
			{
				TempByt = (UINT8)strtoul(CmdData, NULL, 0);
				if (TempByt != VGMHead.bytAYFlag)
				{
					VGMHead.bytAYFlag = TempByt;
					RetVal |= 0x10;
				}
			}
			else
			{
				printf("Warning! Command ignored - VGM version too low!\n");
			}
		}
		else if (! stricmp(CmdStr, "SetLoopMod"))
		{
			if (CmdData == NULL)
				goto IncompleteArg;

			if (VGMHead.lngVersion >= 0x150)
			{
				TempByt = GetLoopModVal(CmdData);
				if (TempByt != VGMHead.bytLoopModifier)
				{
					if (VGMHead.lngVersion < 0x151)
						printf("Warning! Command executed, but VGM version too low!\n");
					//OldVal = 0x34 + VGMHead.lngDataOffset;
					OldVal = RealHdrSize;
					if (OldVal < 0x7F)
					{
						NewVal = 0x80;
						ResizeVGMHeader(0x80);
						printf("Info: Header resized - Old: 0x%02X bytes, New: 0x%02X bytes\n",
								OldVal, NewVal);
					}
					VGMHead.bytLoopModifier = (UINT8)TempByt;
					RetVal |= 0x10;
				}
			}
			else
			{
				printf("Warning! Command ignored - VGM version too low!\n");
			}
		}
		else if (! stricmp(CmdStr, "SetLoopBase"))
		{
			if (CmdData == NULL)
				goto IncompleteArg;

			if (VGMHead.lngVersion >= 0x150)
			{
				TempSLng = strtol(CmdData, NULL, 0);
				if (TempSLng != VGMHead.bytLoopBase)
				{
					if (VGMHead.lngVersion < 0x151)
						printf("Warning! Command executed, but VGM version too low!\n");
					//OldVal = 0x34 + VGMHead.lngDataOffset;
					OldVal = RealHdrSize;
					if (OldVal < 0x7E)
					{
						NewVal = 0x80;
						ResizeVGMHeader(0x80);
						printf("Info: Header resized - Old: 0x%02X bytes, New: 0x%02X bytes\n",
								OldVal, NewVal);
					}
					VGMHead.bytLoopBase = (INT8)TempSLng;
					RetVal |= 0x10;
				}
			}
			else
			{
				printf("Warning! Command ignored - VGM version too low!\n");
			}
		}
		else if (! stricmp(CmdStr, "SetVolMod"))
		{
			if (CmdData == NULL)
				goto IncompleteArg;

			if (VGMHead.lngVersion >= 0x150)
			{
				TempByt = GetVolModVal(CmdData);
				if (TempByt != VGMHead.bytVolumeModifier)
				{
					if (VGMHead.lngVersion < 0x151)
						printf("Warning! Command executed, but VGM version too low!\n");
					//OldVal = 0x34 + VGMHead.lngDataOffset;
					OldVal = RealHdrSize;
					if (OldVal < 0x7C)
					{
						NewVal = 0x80;
						ResizeVGMHeader(0x80);
						printf("Info: Header resized - Old: 0x%02X bytes, New: 0x%02X bytes\n",
								OldVal, NewVal);
					}
					VGMHead.bytVolumeModifier = TempByt;
					RetVal |= 0x10;
				}
			}
			else
			{
				printf("Warning! Command ignored - VGM version too low!\n");
			}
		}
		else if (! stricmp(CmdStr, "SetOKIM6258Flags"))
		{
			if (CmdData == NULL)
				goto IncompleteArg;

			if (VGMHead.lngVersion >= 0x161)
			{
				TempByt = (UINT8)strtoul(CmdData, NULL, 0);
				if (TempByt != VGMHead.bytOKI6258Flags)
				{
					VGMHead.bytOKI6258Flags = TempByt;
					RetVal |= 0x10;
				}
			}
			else
			{
				printf("Warning! Command ignored - VGM version too low!\n");
			}
		}
		else if (! stricmp(CmdStr, "SetK054539Flags"))
		{
			if (CmdData == NULL)
				goto IncompleteArg;

			if (VGMHead.lngVersion >= 0x161)
			{
				TempByt = (UINT8)strtoul(CmdData, NULL, 0);
				if (TempByt != VGMHead.bytK054539Flags)
				{
					VGMHead.bytK054539Flags = TempByt;
					RetVal |= 0x10;
				}
			}
			else
			{
				printf("Warning! Command ignored - VGM version too low!\n");
			}
		}
		else if (! stricmp(CmdStr, "SetC140Type"))
		{
			if (CmdData == NULL)
				goto IncompleteArg;

			if (VGMHead.lngVersion >= 0x161)
			{
				TempByt = (UINT8)strtoul(CmdData, NULL, 0);
				if (TempByt != VGMHead.bytC140Type)
				{
					VGMHead.bytC140Type = TempByt;
					RetVal |= 0x10;
				}
			}
			else
			{
				printf("Warning! Command ignored - VGM version too low!\n");
			}
		}
		else if (! strnicmp(CmdStr, "Check", 5))
		{
			CmdStr += 5;
			if (! stricmp(CmdStr, ""))
			{
				// ask for way to correct
				TempByt = 0x03;
			}
			else if (! stricmp(CmdStr, "R"))
			{
				// read only
				TempByt = 0x00;
			}
			else if (! stricmp(CmdStr, "L"))
			{
				// fix by recalculating length
				TempByt = 0x01;
			}
			else if (! stricmp(CmdStr, "O"))
			{
				// fix by relocating loop offset
				TempByt = 0x02;
			}
			else if (! stricmp(CmdStr, "T"))
			{
				// ask + auto-correct tags
				TempByt = 0x07;
			}
			else
			{
				printf("Error - Unknown Command: -%s\n", CmdStr - 5);
				return 0x80;
			}

			TempByt = CheckVGMFile(TempByt);
			if (TempByt == 0xFF)
			{
				printf("Error processing vgm file!\n");
				return 0xFF;
			}
			else if (TempByt)
			{
				RetVal |= 0x10;
			}
			LightChange = true;
		}
		else if (! stricmp(CmdStr, "Strip"))
		{
			printf("Stripping data ...");
			StripVGMData();
			RetVal |= 0x10;
		}
		else if (! stricmp(CmdStr, "KeepDate"))
		{
			KeepDate = true;
		}
		else
		{
			printf("Error - Unknown Command: -%s\n", CmdStr);
			return 0x80;
		}
		if (RetVal & 0x10)
		{
			if (! LightChange)
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
		/*if (VGMHead.lngVersion < 0x150)
			TempLng = 0x40;
		else
			TempLng = 0x34 + VGMHead.lngDataOffset;
		memcpy(&VGMData[0x00], &VGMHead, TempLng);*/
		memcpy(&VGMData[0x00], &VGMHead, RealCpySize);
	}

	return ResVal;

IncompleteArg:

	printf("Error! Argument incomplete!\n");

	return 0x80;
}

static UINT8 GetLoopModVal(char* CmdData)
{
	double LoopDbl;
	INT32 LoopLng;

	switch(CmdData[0x00])
	{
	case '*':
	case '/':
		// *2, /2.0
		LoopDbl = strtod(CmdData + 0x01,  NULL);
		switch(CmdData[0x00])
		{
		case '*':
			LoopLng = (UINT32)floor(LoopDbl * 16.0 + 0.5);
			break;
		case '/':
			LoopLng = (UINT32)floor(16.0 / LoopDbl + 0.5);
			break;
		}
		if (LoopLng == 0x00)
			LoopLng = 0x01;
		break;
	default:
		// 0x00 is accepted here, but only here
		LoopLng = strtol(CmdData, NULL, 0);
		break;
	}

	// Note 0x00 is valid, but equal to 0x10, so minimum is 0x01
	if (LoopLng < 0x00)
		LoopLng = 0x01;
	else if (LoopLng > 0xFF)
		LoopLng = 0xFF;

	return (UINT8)LoopLng;
}

static UINT8 GetVolModVal(char* CmdData)
{
	char* EndStr;
	double VolDbl;
	INT32 VolLng;

	VolLng = strtol(CmdData, &EndStr, 0);
	if (*EndStr == '.')
	{
		// format 1.0
		VolDbl = strtod(CmdData, NULL);
		if (VolDbl < 0.25)
			VolDbl = 0.25;
		else if (VolDbl > 64.0)
			VolDbl = 64.0;
		VolLng = (INT32)floor(log(VolDbl) / log(2.0) * 0x20 + 0.5);
	}

	if (VolLng < 0x00)
	{
		if (VolLng < -0x3F)
			VolLng = -0x3F;
		VolLng = 0x100 + VolLng;
	}
	else if (VolLng > 0xFF)
	{
		VolLng = 0x00;
	}

	return (UINT8)VolLng;
}

static bool ResizeVGMHeader(UINT32 NewSize)
{
	UINT32 OldSize;
	INT32 PosMove;

	// for now I want the complete header (incl. extentions)
	/*if (! VGMHead.lngDataOffset)
		OldSize = 0x40;
	else
		OldSize = 0x34 + VGMHead.lngDataOffset;*/
	OldSize = RealHdrSize;

	if (OldSize == NewSize)
		return false;

	if (NewSize < 0xC0 && OldSize >= 0xC0)
		OldSize = 0x34 + VGMHead.lngDataOffset;

	PosMove = NewSize - OldSize;
	VGMHead.lngEOFOffset += PosMove;
	if (VGMHead.lngGD3Offset)
		VGMHead.lngGD3Offset += PosMove;
	if (VGMHead.lngLoopOffset)
		VGMHead.lngLoopOffset += PosMove;
	if (VGMHead.lngExtraOffset)
		VGMHead.lngExtraOffset += PosMove;
	VGMHead.lngDataOffset += PosMove;	// can't use (NewSize - 0x34), if I have extra headers
	RealHdrSize = NewSize;

	if (PosMove > 0)
	{
		VGMData = (UINT8*)realloc(VGMData, VGMDataLen + PosMove);
		memmove(&VGMData[NewSize], &VGMData[OldSize], VGMDataLen - OldSize);
		memset(&VGMData[OldSize], 0x00, PosMove);
	}
	else
	{
		memmove(&VGMData[NewSize], &VGMData[OldSize], VGMDataLen - OldSize);
	}
	VGMDataLen += PosMove;

	OldSize = sizeof(VGM_HEADER);
	RealCpySize = (RealHdrSize <= OldSize) ? RealHdrSize : OldSize;

	return true;
}

static UINT32 CheckForMinVersion(void)
{
	UINT32 CurPos;
	UINT8 Command;
	UINT8 TempByt;
	//UINT16 TempSht;
	UINT32 TempLng;
	UINT32 CmdLen;
	bool StopVGM;
	UINT32 MinVer;
	UINT32* ChipHzPnt;

	TempLng = sizeof(VGM_HEADER);
	ChipHzPnt = (UINT32*)((UINT8*)&VGMHead + TempLng);
	while(ChipHzPnt > &VGMHead.lngDataOffset)
	{
		ChipHzPnt --;
		TempLng -= 0x04;
		if (*ChipHzPnt)
			break;
	}
	if (TempLng <= 0x34)
		MinVer = 0x150;
	else if (TempLng <= 0x7C)
		MinVer = 0x151;
	else if (TempLng <= 0xB4)
		MinVer = 0x161;
	else if (TempLng == 0xBC)
		MinVer = 0x170;
	else //if (TempLng < 0xF0)
		MinVer = 0x171;
	if (VGMHead.lngVersion > 0x171)
		return VGMHead.lngVersion;

	if (MinVer == 0x150)
	{
		// check for dual chip usage of <1.51 chips
		if ((VGMHead.lngHzPSG & 0xC0000000) ||
			(VGMHead.lngHzYM2413 & 0xC0000000) ||
			(VGMHead.lngHzYM2612 & 0xC0000000) ||
			(VGMHead.lngHzYM2151 & 0xC0000000))
			MinVer = 0x151;
	}

	StopVGM = false;
	if (VGMHead.lngVersion < 0x150)
		CurPos = 0x40;
	else
		CurPos = 0x34 + VGMHead.lngDataOffset;
	while(CurPos < VGMDataLen)
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
			case 0x63:	// 1/50s delay
				CmdLen = 0x01;
				break;
			case 0x61:	// xx Sample Delay
				CmdLen = 0x03;
				break;
			case 0x50:	// SN76496 write
				CmdLen = 0x02;
				break;
			case 0x67:	// Data Block (PCM Data Stream)
				TempByt = VGMData[CurPos + 0x02];
				switch(TempByt & 0xC0)
				{
				case 0x00:
					break;
				case 0x40:
					if (MinVer < 0x160)
						MinVer = 0x160;
					break;
				case 0x80:
				case 0xC0:
					if (MinVer < 0x151)
						MinVer = 0x151;
					break;
				}
				memcpy(&TempLng, &VGMData[CurPos + 0x03], 0x04);
				TempLng &= 0x7FFFFFFF;
				CmdLen = 0x07 + TempLng;
				break;
			case 0x68:	// PCM RAM write
				if (MinVer < 0x160)
					MinVer = 0x160;
				CmdLen = 0x0C;
				break;
			case 0x90:	// DAC Ctrl: Setup Chip
				if (MinVer < 0x160)
					MinVer = 0x160;
				CmdLen = 0x05;
				break;
			case 0x91:	// DAC Ctrl: Set Data
				if (MinVer < 0x160)
					MinVer = 0x160;
				CmdLen = 0x05;
				break;
			case 0x92:	// DAC Ctrl: Set Freq
				if (MinVer < 0x160)
					MinVer = 0x160;
				CmdLen = 0x06;
				break;
			case 0x93:	// DAC Ctrl: Play from Start Pos
				if (MinVer < 0x160)
					MinVer = 0x160;
				CmdLen = 0x0B;
				break;
			case 0x94:	// DAC Ctrl: Stop immediately
				if (MinVer < 0x160)
					MinVer = 0x160;
				CmdLen = 0x02;
				break;
			case 0x95:	// DAC Ctrl: Play Block (small)
				if (MinVer < 0x160)
					MinVer = 0x160;
				CmdLen = 0x05;
				break;
			default:	// Handle all other commands
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
					//StopVGM = true;
					break;
				}
				break;
			}
		}
		CurPos += CmdLen;

		if (StopVGM)
			break;
	}

	return MinVer;
}

static UINT8 CheckVGMFile(UINT8 Mode)
{
	UINT32 CurPos;
	UINT8 Command;
	UINT8 TempByt;
	//UINT16 TempSht;
	UINT32 TempLng;
	UINT32 CmdLen;
	bool StopVGM;
	bool HasLoop;
	bool InLoop;
	UINT32 CountLen;
	UINT32 CountLoop;
	UINT32 VGMLoop;
	UINT32 LoopPos;
	UINT16 CmdDelay;
	UINT32 VGMEoDPos;	// EoD - End of Data
	UINT32 VGMGD3Pos;
	UINT32 VGMVer;
	UINT32 GD3Ver;
	UINT8 VGMErr;
	UINT8 GD3Flags;
	UINT8 RetVal;
	bool BadCmdFound;
	UINT32 NewDataPos;
	bool GD3Fix;
	bool CmdWarning[0x100];

	HasLoop = VGMHead.lngLoopOffset ? true : false;
	if (! HasLoop && VGMHead.lngLoopSamples)
	{
		printf("LoopOffset missing, but LoopSamples > 0!\n");
		HasLoop = true;
		printf("Assume song to have a loop!\n");
	}
	if (HasLoop)
		VGMLoop = VGMHead.lngTotalSamples - VGMHead.lngLoopSamples;
	if (VGMHead.lngLoopOffset)
		LoopPos = 0x1C + VGMHead.lngLoopOffset;
	else
		LoopPos = 0x00;
	CountLen = 0x00;
	CountLoop = 0x00;

	VGMErr = 0x00;
	InLoop = false;
	if (VGMHead.lngVersion < 0x150)
	{
		CurPos = 0x40;
	}
	else
	{
		CurPos = 0x34 + VGMHead.lngDataOffset;
		Command = VGMData[CurPos + 0x00];
		BadCmdFound = false;
		if (VGMHead.lngVersion > 0x161 && ChipCommandIsUnknown(Command))
			BadCmdFound = true;
		if (! VGMHead.lngDataOffset || (! BadCmdFound && ! ChipCommandIsValid(Command)))
		{
			VGMErr |= 0x04;
			printf("Bad Data Offset 0x%X!", CurPos);

			StopVGM = false;
			while(CurPos < VGMDataLen)
			{
				Command = VGMData[CurPos + 0x00];
				if (Command >= 0x70 && Command <= 0x8F)
				{
					StopVGM = true;
					CmdLen = 0x01;
				}
				else
				{
					switch(Command)
					{
					case 0x66:	// End Of File
						StopVGM = true;
						break;
					case 0x62:	// 1/60s delay
						StopVGM = true;
						break;
					case 0x63:	// 1/50s delay
						StopVGM = true;
						break;
					case 0x61:	// xx Sample Delay
						StopVGM = true;
						break;
					case 0x50:	// SN76496 write
						CmdLen = 0x02;
						StopVGM = ChipCommandIsValid(Command);
						break;
					case 0x67:	// Data Block (PCM Data Stream)
						TempByt = VGMData[CurPos + 0x01];
						memcpy(&TempLng, &VGMData[CurPos + 0x03], 0x04);
						TempLng &= 0x7FFFFFFF;
						CmdLen = 0x07 + TempLng;

						if (TempByt == 0x66 && CurPos + CmdLen <= VGMDataLen)
							StopVGM = true;
						else
							CmdLen = 0x01;	// invalid block
						break;
					case 0x68:	// PCM RAM write
						TempByt = VGMData[CurPos + 0x01];
						CmdLen = 0x0C;
						if (TempByt == 0x66)
							StopVGM = true;
						else
							CmdLen = 0x01;	// invalid block
						break;
					case 0x90:	// DAC Ctrl: Setup Chip
					case 0x91:	// DAC Ctrl: Set Data
					case 0x92:	// DAC Ctrl: Set Freq
					case 0x93:	// DAC Ctrl: Play from Start Pos
					case 0x94:	// DAC Ctrl: Stop immediately
					case 0x95:	// DAC Ctrl: Play Block (small)
						StopVGM = true;
						break;
					default:	// Handle all other commands
						switch(Command & 0xF0)
						{
						case 0x30:
							CmdLen = 0x02;
							StopVGM = ChipCommandIsValid(Command);
							break;
						case 0x40:
						case 0x50:
						case 0xA0:
						case 0xB0:
							CmdLen = 0x03;
							StopVGM = ChipCommandIsValid(Command);
							break;
						case 0xC0:
						case 0xD0:
							CmdLen = 0x04;
							StopVGM = ChipCommandIsValid(Command);
							break;
						case 0xE0:
						case 0xF0:
							CmdLen = 0x05;
							StopVGM = ChipCommandIsValid(Command);
							break;
						default:
							CmdLen = 0x01;
							break;
						}
						break;
					}
				}
				if (StopVGM)
					break;

				CurPos += CmdLen;
			}
			NewDataPos = CurPos;
			printf("  New Offset is: 0x%X\n", NewDataPos);
		}

		if (VGMHead.lngDataOffset)
			CurPos = 0x34 + VGMHead.lngDataOffset;
	}

	memset(CmdWarning, 0x00, 0x100);
	StopVGM = false;
	BadCmdFound = false;
	while(CurPos < VGMDataLen)	// I have my reasons for NOT using VGMHead.lngEOFOffset
	{
		if (HasLoop)
		{
			if (CurPos == LoopPos)
				InLoop = true;
		}

		CmdLen = 0x00;
		CmdDelay = 0x00;
		Command = VGMData[CurPos + 0x00];
		if (Command >= 0x70 && Command <= 0x8F)
		{
			switch(Command & 0xF0)
			{
			case 0x70:
				CmdDelay = (Command & 0x0F) + 0x01;
				break;
			case 0x80:
				CmdDelay = Command & 0x0F;
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
				CmdDelay = 735;
				CmdLen = 0x01;
				break;
			case 0x63:	// 1/50s delay
				CmdDelay = 882;
				CmdLen = 0x01;
				break;
			case 0x61:	// xx Sample Delay
				memcpy(&CmdDelay, &VGMData[CurPos + 0x01], 0x02);
				CmdLen = 0x03;
				break;
			case 0x50:	// SN76496 write
				if (CmdWarning[Command] && ! ChipCommandIsValid(Command))
				{
					CmdWarning[Command] = true;
					printf("Warning! Command %02X found, but associated chip not used!\n",
							Command);
				}
				CmdLen = 0x02;
				break;
			case 0x67:	// Data Block (PCM Data Stream)
				TempByt = VGMData[CurPos + 0x02];
				memcpy(&TempLng, &VGMData[CurPos + 0x03], 0x04);
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
			default:	// Handle all other commands
				switch(Command & 0xF0)
				{
				case 0x30:
				case 0x40:
					CmdLen = 0x02;
					if (CmdWarning[Command] && ! ChipCommandIsValid(Command))
					{
						CmdWarning[Command] = true;
						if (ChipCommandIsUnknown(Command))
							printf("0x%X: Warning! Unknown Command %02X found!\n", CurPos, Command);
						else
							printf("0x%X: Warning! Command %02X found, but associated chip not "
									"used!\n", CurPos, Command);
					}
					break;
				case 0x50:
				case 0xA0:
				case 0xB0:
					CmdLen = 0x03;
					if (CmdWarning[Command] && ! ChipCommandIsValid(Command))
					{
						CmdWarning[Command] = true;
						if (ChipCommandIsUnknown(Command))
							printf("0x%X: Warning! Unknown Command %02X found!\n", CurPos, Command);
						else
							printf("0x%X: Warning! Command %02X found, but associated chip not "
									"used!\n", CurPos, Command);
					}
					break;
				case 0xC0:
				case 0xD0:
					CmdLen = 0x04;
					if (CmdWarning[Command] && ! ChipCommandIsValid(Command))
					{
						CmdWarning[Command] = true;
						if (ChipCommandIsUnknown(Command))
							printf("0x%X: Warning! Unknown Command %02X found!\n", CurPos, Command);
						else
							printf("0x%X: Warning! Command %02X found, but associated chip not "
									"used!\n", CurPos, Command);
					}
					break;
				case 0xE0:
				case 0xF0:
					CmdLen = 0x05;
					if (CmdWarning[Command] && ! ChipCommandIsValid(Command))
					{
						CmdWarning[Command] = true;
						if (ChipCommandIsUnknown(Command))
							printf("0x%X: Warning! Unknown Command %02X found!\n", CurPos, Command);
						else
							printf("0x%X: Warning! Command %02X found, but associated chip not "
									"used!\n", CurPos, Command);
					}
					break;
				default:
					CmdLen = 0x01;
					//StopVGM = true;
					if (! BadCmdFound)
					{
						CmdWarning[Command] = true;
						BadCmdFound = true;
						printf("0x%X: Warning! Command with unknown argument length found!\n", CurPos);
					}
					break;
				}
				break;
			}
		}
		CurPos += CmdLen;
		CountLen += CmdDelay;
		if (InLoop)
			CountLoop += CmdDelay;

		if (StopVGM)
			break;
	}
	VGMEoDPos = CurPos;

	// VGMErr Bits:
	//	Bit	Val	Error Description
	//	 0	 01	Total Samples
	//	 1	 02	Loop Samples / Loop Offset
	//	 2	 04	Bad Data Offset
	//	 4	 10	GD3 Tag Offset
	//	 5	 20	GD3 Tag Length / EOF Offset
	//	 7	 80	Version too low
	// GD3Flags Bits:
	//	Bit	Val	Error Description
	//	0-3	 0F	Checked
	//	4-7	 F0	In Header
	//	 0	 11	GD3 found
	//	 1	 02	GD3 Offset wrong
	//	 3	 04
	//	 4	 08
	//VGMErr = 0x00;
	GD3Flags = 0x00;
	GD3Fix = false;
	if (CountLen != VGMHead.lngTotalSamples)
		VGMErr |= 0x01;
	if (CountLoop != VGMHead.lngLoopSamples)
		VGMErr |= 0x02;

	if (CurPos > 0x04 + VGMHead.lngEOFOffset)
		printf("Warning! EOF Offset before EOF command!\n");
	if (! StopVGM)
		printf("Warning! File end before EOF command!\n");
	if (HasLoop && ! InLoop)
		printf("Warning! Loop offset between commands!\n");

	if (VGMHead.lngGD3Offset)
	{
		// Check for GD3 Tag at GD3 Offset
		CurPos = 0x14 + VGMHead.lngGD3Offset;
		if (CurPos <= VGMDataLen - 0x04)	// avoid crashes
			memcpy(&TempLng, &VGMData[CurPos], 0x04);
		else
			TempLng = 0x00;
		if (TempLng == FCC_GD3)
		{
			GD3Flags |= 0x01;	// GD3 Check - found
			VGMGD3Pos = CurPos;
		}
	}
	if (! (GD3Flags & 0x01))
	{
		if (VGMEoDPos <= VGMDataLen - 0x04)
		{
			// Check for GD3 Tag at EoD
			CurPos = VGMEoDPos;
			memcpy(&TempLng, &VGMData[CurPos], 0x04);
		}
		else
		{
			TempLng = 0x00;
		}
		if (TempLng != FCC_GD3)
		{
			GD3Flags |= 0x00;	// GD3 Check - missing
			if (VGMHead.lngGD3Offset)
			{
				printf("GD3 Offset set, but GD3 Tag not found!\n");
				VGMErr |= 0x10;
				GD3Flags |= 0x10;	// GD3 Header - set
			}
			//else - all ok
		}
		else
		{
			GD3Flags |= 0x01;	// GD3 Check - found
			VGMGD3Pos = CurPos;
		}
	}
	if (GD3Flags & 0x01)
	{
		CurPos = VGMGD3Pos;
		if (! VGMHead.lngGD3Offset)
		{
			printf("GD3 Tag found, but GD3 Offset missing!\n");
			VGMErr |= 0x10;
			GD3Flags |= 0x00;	// GD3 Header - not set
		}
		else if (CurPos != 0x14 + VGMHead.lngGD3Offset)	// CurPos == GD3 Offset ?
		{
			printf("Wrong GD3 Tag Offset! (Header: 0x%06X  File: 0x%06X)\n",
					0x14 + VGMHead.lngGD3Offset, CurPos);
			VGMErr |= 0x10;
			GD3Flags |= 0x02;	// GD3 Offset - wrong
		}

		memcpy(&GD3Ver, &VGMData[CurPos + 0x04], 0x04);
		if (GD3Ver > 0x100)
			printf("GD3 Tag version newer than supported - correction may be skipped!\n");

		memcpy(&CmdLen, &VGMData[CurPos + 0x08], 0x04);
		TempLng = CalcGD3Length(CurPos + 0x0C, GD3T_ENT_V100);
		if (TempLng != CmdLen)
		{
			printf("Wrong GD3 Length! (Header: 0x%06X  File: 0x%06X)\n", CmdLen, TempLng);
			if (GD3Ver == 0x100)
			{
				VGMErr |= 0x20;
				// Catch the special case where VGMTool r2 adds an additional \0 character.
				// In that case, -CheckT does an automatic fix.
				if (TempLng == CmdLen - 0x02 && ! *(UINT16*)(VGMData + CurPos + 0x0C + TempLng))
					GD3Flags |= 0x80;
			}
			else
			{
				// unknown GD3 version - don't fix length
			}
		}

		//TempLng += 0x0C;	// calculate EOF based on calculated GD3 length
		TempLng = 0x0C + CmdLen;	// calculate EOF based on current GD3 length
		if (CurPos + TempLng != 0x04 + VGMHead.lngEOFOffset)	// Check EOF Offset
		{
			printf("Wrong EOF Offset! (Header: 0x%06X  File: 0x%06X)\n",
					0x04 + VGMHead.lngEOFOffset, CurPos + TempLng);
			VGMErr |= 0x20;
		}
	}
	else
	{
		if (VGMEoDPos != 0x04 + VGMHead.lngEOFOffset)	// Check EOF Offset
		{
			printf("Wrong EOF Offset! (Header: 0x%06X  File: 0x%06X)\n",
					0x04 + VGMHead.lngEOFOffset, VGMEoDPos);
			VGMErr |= 0x20;
		}
	}

	if (VGMHead.lngVersion >= 0x0150)
	{
		VGMVer = CheckForMinVersion();
		if (VGMVer > VGMHead.lngVersion)
		{
			printf("VGM Version too low! (is %03X, should be %03X)\n",
					VGMHead.lngVersion, VGMVer);
			VGMErr |= 0x80;
		}
	}

	printf("\t%9s  %9s\n", "Header", "Counted");
	printf("Length\t%9u  %9u", VGMHead.lngTotalSamples, CountLen);
	if (VGMHead.lngTotalSamples != CountLen)
		printf(" !");
	printf("\n");
	printf("Loop\t%9u  %9u", VGMHead.lngLoopSamples, CountLoop);
	if (VGMHead.lngLoopSamples != CountLoop)
		printf(" !");
	printf("\n");

	if (! VGMErr && ! BadCmdFound)
		return 0x00;	// No errors - It's all okay

	if (BadCmdFound)
	{
		Mode |= 0x10;
		if (VGMHead.lngVersion > 0x161)
		{
			printf("Unknown Commands found and VGM version newer than supported!\n"
					"Set to Read-Only Mode!\n");
			Mode &= ~0x03;	// Set Read-Only-Mode
		}
		if (! VGMErr)
			Mode &= ~0x03;	// If there are no fixable errors, we don't need to ask.
	}

	printf("There are some errors. ");
	if ((Mode & 0x13) == 0x00)
	{
		printf("Please try -Check to fix them.\n");
	}
	else if ((Mode & 0x03) == 0x03 && ! ((Mode & 0x04) && GD3Flags == 0x81))
	{
		printf("Press ESC / F (fix)");
		if (VGMErr & 0x02)
			printf(" / R (fix by relocating)");
		printf(": ");
#ifdef WIN32
		TempByt = 0x00;
		while(! (TempByt == 0x1B || TempByt == 'F' || TempByt == 'R'))
		{
			TempByt = (UINT8)toupper(_getch());
		}
		if (TempByt == 0x1B)
			printf("ESC\n");
		else
			printf("%c\n", TempByt);
#else
		TempByt = (UINT8)toupper(getchar());
#endif
		Mode &= ~0x03;
		if (TempByt == 'F')
			Mode |= 0x01;
		else if (TempByt == 'R')
			Mode |= 0x02;
		else
			Mode |= 0x00;
	}
	else
	{
		printf("\n");
	}

	if (! (Mode & 0x03))
		return 0x00;

	RetVal = 0x00;
	if (VGMErr & 0x01)
	{
		VGMHead.lngTotalSamples = CountLen;
		RetVal |= 0x01;
	}
	//if (HasLoop && CountLoop != VGMHead.lngLoopSamples)
	if (HasLoop && (VGMErr & 0x02))
	{
		if ((Mode & 0x03) == 0x01)
		{
			if (InLoop)
			{
				VGMHead.lngLoopSamples = CountLoop;
				RetVal |= 0x02;
			}
			else
			{
				printf("Can't fix loop samples due to incorrect loop offset!\n");
				printf("Try to relocate the loop offset!\n");
			}
		}
		else if ((Mode & 0x03) == 0x02)
		{
			LoopPos = RelocateVGMLoop();
			if (LoopPos)
			{
				VGMHead.lngLoopOffset = LoopPos - 0x1C;
				RetVal |= 0x02;
			}
			else
			{
				printf("Failed to relocate loop offset!\n");
			}
		}
	}
	if (VGMErr & 0x04)
	{
		VGMHead.lngDataOffset = NewDataPos - 0x34;
		RetVal |= 0x01;
	}

	//	 4	 10	GD3 Tag Offset
	//	 5	 20	GD3 Tag Length / EOF Offset
	if (VGMErr & 0x10)
	{
		switch(GD3Flags & 0x11)
		{
		case 0x10:	// GD3 offset set, but no GD3 found
			VGMHead.lngGD3Offset = 0x00;
			break;
		case 0x01:	// GD3 offset missing, but GD3 found
			GD3Flags |= 0x02;	// GD3 offset correction
			break;
		case 0x00:	// GD3 completely missing
		case 0x11:	// GD3 found, offset is good
			break;
		}

		if (GD3Flags & 0x02)	// GD3 Offset wrong
		{
			VGMHead.lngGD3Offset = VGMGD3Pos - 0x14;
		}
		RetVal |= 0x10;
	}
	if (VGMErr & 0x20)
	{
		if (GD3Flags & 0x01)
		{
			CurPos = 0x14 + VGMHead.lngGD3Offset;
			memcpy(&CmdLen, &VGMData[CurPos + 0x08], 0x04);
			TempLng = CalcGD3Length(CurPos + 0x0C, GD3T_ENT_V100);
			if (TempLng != CmdLen)
			{
				if (TempLng < CmdLen && GD3Ver > 0x100)
				{
					printf("GD3 Tag longer than counted - skip correction!\n");
					TempLng = CmdLen;
				}
				memcpy(&VGMData[CurPos + 0x08], &TempLng, 0x04);	// rewrite GD3 length
				GD3Fix = true;
			}

			TempLng += 0x0C;
			VGMEoDPos = CurPos + TempLng;
		}
		VGMDataLen = VGMEoDPos;
		VGMHead.lngEOFOffset = VGMDataLen - 0x04;
		RetVal |= 0x20;
	}
	if (VGMErr & 0x80)
	{
		VGMHead.lngVersion = VGMVer;
		RetVal |= 0x80;
	}
	if ((VGMErr & ~0x20) || ! GD3Fix)
		KeepDate = false;

	return RetVal;
}

static bool ChipCommandIsUnknown(UINT8 Command)
{
	//return ((Command <= 0x4E && Command != 0x30) || /*(Command >= 0xA1 && Command <= 0xAF) ||*/
	return ((Command != 0x3F && Command >= 0x32 && Command <= 0x4E) || /*(Command >= 0xA1 && Command <= 0xAF) ||*/
			(Command >= 0xBC && Command <= 0xBF) || (Command >= 0xC5 && Command <= 0xCF) ||
			(Command >= 0xD5 && Command <= 0xDF) || Command >= 0xE1);
}

static bool ChipCommandIsValid(UINT8 Command)
{
	if (ChipCommandIsUnknown(Command))
		return false;

	if (Command >= 0x61 && Command <= 0x63)
		return true;
	if ((Command & 0xF0) == 0x70)
		return true;
	if (Command == 0x67 || Command == 0x68)
		return true;
	if (Command >= 0x90 && Command <= 0x95)
		return true;
	if (Command == 0x31 && (VGMHead.lngHzAY8910 || VGMHead.lngHzYM2203))
		return true;
	if (((Command == 0x50 || Command == 0x4F) && VGMHead.lngHzPSG) ||
		((Command == 0x30 || Command == 0x3F) && (VGMHead.lngHzPSG & 0x40000000)))
		return true;
	if ((Command == 0x51 && VGMHead.lngHzYM2413) ||
		(Command == 0xA1 && (VGMHead.lngHzYM2413 & 0x40000000)))
		return true;
	if (((Command == 0x52 || Command == 0x53) && VGMHead.lngHzYM2612) ||
		((Command == 0xA2 || Command == 0xA3) && (VGMHead.lngHzYM2612 & 0x40000000)))
		return true;
	if ((Command == 0x54 && VGMHead.lngHzYM2151) ||
		(Command == 0xA4 && (VGMHead.lngHzYM2151 & 0x40000000)))
		return true;
	if (Command == 0xC0 && VGMHead.lngHzSPCM)
		return true;
	if ((Command == 0xB0 || Command == 0xC1) && VGMHead.lngHzRF5C68)
		return true;
	if ((Command == 0x55 && VGMHead.lngHzYM2203) ||
		(Command == 0xA5 && (VGMHead.lngHzYM2203 & 0x40000000)))
		return true;
	if (((Command == 0x56 || Command == 0x57) && VGMHead.lngHzYM2608) ||
		((Command == 0xA6 || Command == 0xA7) && (VGMHead.lngHzYM2608 & 0x40000000)))
		return true;
	if (((Command == 0x58 || Command == 0x59) && VGMHead.lngHzYM2610) ||
		((Command == 0xA8 || Command == 0xA9) && (VGMHead.lngHzYM2610 & 0x40000000)))
		return true;
	if ((Command == 0x5A && VGMHead.lngHzYM3812) ||
		(Command == 0xAA && (VGMHead.lngHzYM3812 & 0x40000000)))
		return true;
	if ((Command == 0x5B && VGMHead.lngHzYM3526) ||
		(Command == 0xAB && (VGMHead.lngHzYM3526 & 0x40000000)))
		return true;
	if ((Command == 0x5C && VGMHead.lngHzY8950) ||
		(Command == 0xAC && (VGMHead.lngHzY8950 & 0x40000000)))
		return true;
	if (((Command == 0x5E || Command == 0x5F) && VGMHead.lngHzYMF262) ||
		((Command == 0xAE || Command == 0xAF) && (VGMHead.lngHzYMF262 & 0x40000000)))
		return true;
	if (Command == 0xD0 && VGMHead.lngHzYMF278B)
		return true;
	if (Command == 0xD1 && VGMHead.lngHzYMF271)
		return true;
	if ((Command == 0x5D && VGMHead.lngHzYMZ280B) ||
		(Command == 0xAD && (VGMHead.lngHzYMZ280B & 0x40000000)))
		return true;
	if ((Command == 0xB1 || Command == 0xC2) && VGMHead.lngHzRF5C164)
		return true;
	if (Command == 0xB2 && VGMHead.lngHzPWM)
		return true;
	if (Command == 0xA0 && VGMHead.lngHzAY8910)
		return true;
	if (Command == 0x68 && (VGMHead.lngHzRF5C68 || VGMHead.lngHzRF5C164))
		return true;
	if (Command == 0xB3 && VGMHead.lngHzGBDMG)
		return true;
	if (Command == 0xB4 && VGMHead.lngHzNESAPU)
		return true;
	if ((Command == 0xB5 || Command == 0xC3) && VGMHead.lngHzMultiPCM)
		return true;
	if (Command == 0xB6 && VGMHead.lngHzUPD7759)
		return true;
	if (Command == 0xB7 && VGMHead.lngHzOKIM6258)
		return true;
	if (Command == 0xB8 && VGMHead.lngHzOKIM6295)
		return true;
	if (Command == 0xD2 && VGMHead.lngHzK051649)
		return true;
	if (Command == 0xD3 && VGMHead.lngHzK054539)
		return true;
	if (Command == 0xB9 && VGMHead.lngHzHuC6280)
		return true;
	if (Command == 0xD4 && VGMHead.lngHzC140)
		return true;
	if (Command == 0xBA && VGMHead.lngHzK053260)
		return true;
	if (Command == 0xBB && VGMHead.lngHzPokey)
		return true;
	if (Command == 0xC4 && VGMHead.lngHzQSound)
		return true;
	if (Command == 0x41 && VGMHead.lngHzK007232)
		return true;

	return false;
}

static UINT32 RelocateVGMLoop(void)
{
	UINT32 CurPos;
	UINT8 Command;
	UINT8 TempByt;
	//UINT16 TempSht;
	UINT32 TempLng;
	UINT32 CmdLen;
	bool StopVGM;
	UINT32 VGMSmplPos;
	UINT32 VGMLoop;
	UINT16 CmdDelay;

	VGMLoop = VGMHead.lngTotalSamples - VGMHead.lngLoopSamples;
	VGMSmplPos = 0x00;

	StopVGM = false;
	if (VGMHead.lngVersion < 0x150)
		CurPos = 0x40;
	else
		CurPos = 0x34 + VGMHead.lngDataOffset;
	while(CurPos < VGMDataLen)	// I have my reasons for NOT using VGMHead.lngEOFOffset
	{
		CmdLen = 0x00;
		CmdDelay = 0x00;
		Command = VGMData[CurPos + 0x00];
		if (Command >= 0x70 && Command <= 0x8F)
		{
			switch(Command & 0xF0)
			{
			case 0x70:
				CmdDelay = (Command & 0x0F) + 0x01;
				break;
			case 0x80:
				CmdDelay = Command & 0x0F;
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
				CmdDelay = 735;
				CmdLen = 0x01;
				break;
			case 0x63:	// 1/50s delay
				CmdDelay = 882;
				CmdLen = 0x01;
				break;
			case 0x61:	// xx Sample Delay
				memcpy(&CmdDelay, &VGMData[CurPos + 0x01], 0x02);
				CmdLen = 0x03;
				break;
			case 0x50:	// SN76496 write
				CmdLen = 0x02;
				break;
			case 0x67:	// Data Block (PCM Data Stream)
				TempByt = VGMData[CurPos + 0x02];
				memcpy(&TempLng, &VGMData[CurPos + 0x03], 0x04);
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
			default:	// Handle all other commands
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
					//StopVGM = true;
					break;
				}
				break;
			}
		}
		CurPos += CmdLen;
		VGMSmplPos += CmdDelay;
		if (VGMSmplPos == VGMLoop)
			return CurPos;

		if (StopVGM)
			break;
	}

	return 0x00;
}

static UINT32 CalcGD3Length(UINT32 DataPos, UINT16 TagEntries)
{
	UINT32 CurPos;
	UINT16* CurChr;
	UINT16 CurEnt;

	CurPos = DataPos;
	for (CurEnt = 0x00; CurEnt < TagEntries; CurEnt ++)
	{
		do
		{
			CurChr = (UINT16*)&VGMData[CurPos];
			CurPos += 0x02;
		} while(*CurChr);
	}

	return CurPos - DataPos;
}

INLINE bool GetFromMask(const UINT8* Mask, const UINT8 Data)
{
	return (Mask[Data >> 3] >> (Data & 0x07)) & 0x01;
}

static void StripVGMData(void)
{
	UINT32 VGMPos;
	UINT8* DstData;
	UINT32 DstPos;
	UINT8 ChipID;
	UINT8 Command;
	UINT32 CmdDelay;
	UINT32 AllDelay;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	UINT32 LoopOfs;
	//UINT32 DataLen;
	UINT32 CmdLen;
	bool StopVGM;
	bool WriteEvent;
	UINT32 NewLoopS;
	bool WroteCmd80;
	UINT8* VGMPnt;

	DstData = (UINT8*)malloc(VGMDataLen + 0x100);
	AllDelay = 0;
	if (VGMHead.lngDataOffset)
		VGMPos = 0x34 + VGMHead.lngDataOffset;
	else
		VGMPos = 0x40;
	DstPos = VGMPos;
	LoopOfs = VGMHead.lngLoopOffset ? (0x1C + VGMHead.lngLoopOffset) : 0x00;
	NewLoopS = 0x00;
	memcpy(DstData, VGMData, VGMPos);	// Copy Header

	InitAllChips();
	/*if (VGMHead.lngHzK054539)
	{
		SetChipSet(0x00);
		k054539_write(0xFF, 0x00, VGMHead.bytK054539Flags);
		if (VGMHead.lngHzK054539 & 0x40000000)
		{
			SetChipSet(0x01);
			k054539_write(0xFF, 0x00, VGMHead.bytK054539Flags);
		}
	}*/
	if (VGMHead.lngHzC140)
	{
		SetChipSet(0x00);
		c140_write(0xFF, 0x00, VGMHead.bytC140Type);
		if (VGMHead.lngHzC140 & 0x40000000)
		{
			SetChipSet(0x01);
			c140_write(0xFF, 0x00, VGMHead.bytC140Type);
		}
	}

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
			case 0x80:
				// Handling is done at WriteEvent
				if (StripVGM[0].YM2612.All || StripVGM[0].YM2612.ChnMask & (1 << 6))
				{
					CmdDelay = Command & 0x0F;
					WriteEvent = false;
				}
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

			SetChipSet(ChipID);
			switch(Command)
			{
			case 0x66:	// End Of File
				CmdLen = 0x01;
				StopVGM = true;
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
			case 0x61:	// xx Sample Delay
				memcpy(&TempSht, &VGMPnt[0x01], 0x02);
				CmdDelay = TempSht;
				CmdLen = 0x03;
				WriteEvent = false;
				break;
			case 0x50:	// SN76496 write
				WriteEvent = sn76496_write(VGMPnt[0x01]);
				if (WriteEvent && ChipID && StripVGM[0].SN76496.All)
					VGMPnt[0x00] = Command;
				CmdLen = 0x02;
				break;
			case 0x51:	// YM2413 write
				WriteEvent = ym2413_write(VGMPnt[0x01], VGMPnt[0x02]);
				if (WriteEvent && ChipID && StripVGM[0].YM2413.All)
					VGMPnt[0x00] = Command;
				CmdLen = 0x03;
				break;
			case 0x52:	// YM2612 write port 0
			case 0x53:	// YM2612 write port 1
				TempByt = Command & 0x01;
				WriteEvent = ym2612_write(TempByt, VGMPnt[0x01], VGMPnt[0x02]);
				if (WriteEvent && ChipID && StripVGM[0].YM2612.All)
					VGMPnt[0x00] = Command;
				CmdLen = 0x03;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMPnt[0x02];
				memcpy(&TempLng, &VGMPnt[0x03], 0x04);

				ChipID = (TempLng & 0x80000000) >> 31;
				TempLng &= 0x7FFFFFFF;
				//SetChipSet(ChipID);

				switch(TempByt & 0xC0)
				{
				case 0x00:	// Database Block
				case 0x40:
					switch(TempByt & 0x3F)
					{
					case 0x00:	// YM2612 PCM Data
						if (StripVGM[ChipID].YM2612.All ||
							(StripVGM[ChipID].YM2612.ChnMask & (0x01 << 6)))
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].YM2612.All)
							VGMPnt[0x06] &= ~0x80;
						break;
					case 0x01:	// RF5C68 PCM Database
						if (StripVGM[ChipID].RF5C68.All)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].RF5C68.All)
							VGMPnt[0x06] &= ~0x80;
						break;
					case 0x02:	// RF5C164 PCM Database
						if (StripVGM[ChipID].RF5C164.All)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].RF5C164.All)
							VGMPnt[0x06] &= ~0x80;
						break;
					default:
						if (StripVGM[ChipID].Unknown)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].Unknown)
							VGMPnt[0x06] &= ~0x80;
						break;
					}
					break;
				case 0x80:	// ROM/RAM Dump
					switch(TempByt)
					{
					case 0x80:	// SegaPCM ROM
						if (StripVGM[ChipID].SegaPCM.All)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].SegaPCM.All)
							VGMPnt[0x06] &= ~0x80;
						break;
					case 0x81:	// YM2608 DELTA-T ROM Image
						if (StripVGM[ChipID].YM2608.All)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].YM2608.All)
							VGMPnt[0x06] &= ~0x80;
						break;
					case 0x82:	// YM2610 ADPCM ROM Image
					case 0x83:	// YM2610 DELTA-T ROM Image
						if (StripVGM[ChipID].YM2610.All)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].YM2610.All)
							VGMPnt[0x06] &= ~0x80;
						break;
					case 0x84:	// YMF278B ROM Image
					case 0x87:	// YMF278B RAM Image
						if (StripVGM[ChipID].YMF278B_All || StripVGM[ChipID].YMF278B_WT.All)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].YMF278B_All)
							VGMPnt[0x06] &= ~0x80;
						break;
					case 0x85:	// YMF271 ROM Image
						if (StripVGM[ChipID].YMF271.All)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].YMF271.All)
							VGMPnt[0x06] &= ~0x80;
						break;
					case 0x86:	// YMZ280B ROM Image
						if (StripVGM[ChipID].YMZ280B.All)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].YMZ280B.All)
							VGMPnt[0x06] &= ~0x80;
						break;
					case 0x88:	// Y8950 DELTA-T ROM Image
						if (StripVGM[ChipID].Y8950.All)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].Y8950.All)
							VGMPnt[0x06] &= ~0x80;
						break;
					case 0x89:	// MultiPCM ROM Image
						if (StripVGM[ChipID].MultiPCM.All)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].MultiPCM.All)
							VGMPnt[0x06] &= ~0x80;
						break;
					case 0x8A:	// UPD7759 ROM Image
						if (StripVGM[ChipID].UPD7759.All)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].UPD7759.All)
							VGMPnt[0x06] &= ~0x80;
						break;
					case 0x8B:	// OKIM6295 ROM Image
						if (StripVGM[ChipID].OKIM6295.All)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].OKIM6295.All)
							VGMPnt[0x06] &= ~0x80;
						break;
					case 0x8C:	// K054539 ROM Image
						if (StripVGM[ChipID].K054539.All)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].K054539.All)
							VGMPnt[0x06] &= ~0x80;
						break;
					case 0x8D:	// C140 ROM Image
						if (StripVGM[ChipID].C140.All)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].C140.All)
							VGMPnt[0x06] &= ~0x80;
						break;
					case 0x8E:	// K053260 ROM Image
						if (StripVGM[ChipID].K053260.All)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].K053260.All)
							VGMPnt[0x06] &= ~0x80;
						break;
					case 0x8F:	// Q-Sound ROM Image
						if (StripVGM[ChipID].QSound.All)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].QSound.All)
							VGMPnt[0x06] &= ~0x80;
						break;
					case 0x94:	// K007232 ROM Image
						if (StripVGM[ChipID].K007232.All)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].K007232.All)
							VGMPnt[0x06] &= ~0x80;
						break;
					}
					break;
				case 0xC0:	// RAM Write
					switch(TempByt)
					{
					case 0xC0:	// RF5C68 RAM
						if (StripVGM[ChipID].RF5C68.All)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].RF5C68.All)
							VGMPnt[0x06] &= ~0x80;
						break;
					case 0xC1:	// RF5C164 RAM
						if (StripVGM[ChipID].RF5C164.All)
							WriteEvent = false;
						if (WriteEvent && ChipID && StripVGM[0].RF5C164.All)
							VGMPnt[0x06] &= ~0x80;
						break;
					}
					break;
				}
				CmdLen = 0x07 + TempLng;
				break;
			case 0xE0:	// Seek to PCM Data Bank Pos
				memcpy(&TempLng, &VGMPnt[0x01], 0x04);
				if (StripVGM[ChipID].YM2612.All ||
					StripVGM[ChipID].YM2612.ChnMask & (1 << 6))
					WriteEvent = false;
				CmdLen = 0x05;
				break;
			case 0x4F:	// GG Stereo
				WriteEvent = GGStereo(VGMPnt[0x01]);
				if (WriteEvent && ChipID && StripVGM[0].SN76496.All)
					VGMPnt[0x00] = Command;
				CmdLen = 0x02;
				break;
			case 0x54:	// YM2151 write
				WriteEvent = ym2151_write(VGMPnt[0x01], VGMPnt[0x02]);
				if (WriteEvent && ChipID && StripVGM[0].YM2151.All)
					VGMPnt[0x00] = Command;
				CmdLen = 0x03;
				break;
			case 0xC0:	// Sega PCM memory write
				memcpy(&TempSht, &VGMPnt[0x01], 0x02);
				ChipID = (TempSht & 0x8000) >> 15;
				TempSht &= 0x7FFF;
				SetChipSet(ChipID);

				WriteEvent = segapcm_mem_write(TempSht, VGMPnt[0x03]);
				if (WriteEvent && ChipID && StripVGM[0].SegaPCM.All)
					VGMPnt[0x02] &= ~0x80;
				CmdLen = 0x04;
				break;
			case 0xB0:	// RF5C68 register write
				ChipID = (VGMPnt[0x01] & 0x80) >> 7;
				SetChipSet(ChipID);
				WriteEvent = rf5c68_reg_write(VGMPnt[0x01] & 0x7F, &VGMPnt[0x02]);
				if (WriteEvent && ChipID && StripVGM[0].RF5C68.All)
					VGMPnt[0x01] &= ~0x80;
				CmdLen = 0x03;
				break;
			case 0xC1:	// RF5C68 memory write
				memcpy(&TempSht, &VGMPnt[0x01], 0x02);
				//ChipID = 0;
				WriteEvent = rf5c68_mem_write(TempSht, VGMPnt[0x03]);
				//if (WriteEvent && ChipID && StripVGM[0].RF5C68.All)
				//	VGMPnt[0x0#] &= ~0x80;
				CmdLen = 0x04;
				break;
			case 0x55:	// YM2203
				WriteEvent = ym2203_write(VGMPnt[0x01], VGMPnt[0x02]);
				if (WriteEvent && ChipID && StripVGM[0].YM2203.All)
					VGMPnt[0x00] = Command;
				CmdLen = 0x03;
				break;
			case 0x56:	// YM2608 write port 0
			case 0x57:	// YM2608 write port 1
				TempByt = Command & 0x01;
				WriteEvent = ym2608_write(TempByt, VGMPnt[0x01], VGMPnt[0x02]);
				if (WriteEvent && ChipID && StripVGM[0].YM2608.All)
					VGMPnt[0x00] = Command;
				CmdLen = 0x03;
				break;
			case 0x58:	// YM2610 write port 0
			case 0x59:	// YM2610 write port 1
				TempByt = Command & 0x01;
				WriteEvent = ym2610_write(TempByt, VGMPnt[0x01], VGMPnt[0x02]);
				if (WriteEvent && ChipID && StripVGM[0].YM2610.All)
					VGMPnt[0x00] = Command;
				CmdLen = 0x03;
				break;
			case 0x5A:	// YM3812 write
				WriteEvent = ym3812_write(VGMPnt[0x01], VGMPnt[0x02]);
				if (WriteEvent && ChipID && StripVGM[0].YM3812.All)
					VGMPnt[0x00] = Command;
				CmdLen = 0x03;
				break;
			case 0x5B:	// YM3526 write
				WriteEvent = ym3526_write(VGMPnt[0x01], VGMPnt[0x02]);
				if (WriteEvent && ChipID && StripVGM[0].YM3526.All)
					VGMPnt[0x00] = Command;
				CmdLen = 0x03;
				break;
			case 0x5C:	// Y8950 write
				WriteEvent = y8950_write(VGMPnt[0x01], VGMPnt[0x02]);
				if (WriteEvent && ChipID && StripVGM[0].Y8950.All)
					VGMPnt[0x00] = Command;
				CmdLen = 0x03;
				break;
			case 0x5E:	// YMF262 write port 0
			case 0x5F:	// YMF262 write port 1
				TempByt = Command & 0x01;
				WriteEvent = ymf262_write(TempByt, VGMPnt[0x01], VGMPnt[0x02]);
				if (WriteEvent && ChipID && StripVGM[0].YMF262.All)
					VGMPnt[0x00] = Command;
				CmdLen = 0x03;
				break;
			case 0x5D:	// YMZ280B write
				WriteEvent = ymz280b_write(VGMPnt[0x01], VGMPnt[0x02]);
				if (WriteEvent && ChipID && StripVGM[0].YMZ280B.All)
					VGMPnt[0x00] = Command;
				CmdLen = 0x03;
				break;
			case 0xD0:	// YMF278B register write
				ChipID = (VGMPnt[0x01] & 0x80) >> 7;
				WriteEvent = true;
				if (StripVGM[ChipID].YMF278B_All ||
					(StripVGM[ChipID].YMF278B_FM.All && StripVGM[ChipID].YMF278B_WT.All))
					WriteEvent = false;

				if (StripVGM[ChipID].YMF278B_FM.All && VGMPnt[0x01] < 0x02 &&
					! (VGMPnt[0x01] == 0x01 && VGMPnt[0x02] == 0x05))
					WriteEvent = false;	// Don't kill the WaveTable-On Command at Reg 0x105
				if (StripVGM[ChipID].YMF278B_WT.All && VGMPnt[0x01] == 0x02)
					WriteEvent = false;
				if (WriteEvent && ChipID && StripVGM[0].YMF278B_All)
					VGMPnt[0x01] &= ~0x80;
				CmdLen = 0x04;
				break;
			case 0xD1:	// YMF271 register write
				ChipID = (VGMPnt[0x01] & 0x80) >> 7;
				WriteEvent = true;
				if (StripVGM[ChipID].YMF271.All)
					WriteEvent = false;
				if (WriteEvent && ChipID && StripVGM[0].YMF271.All)
					VGMPnt[0x01] &= ~0x80;
				CmdLen = 0x04;
				break;
			case 0xB1:	// RF5C164 register write
				ChipID = (VGMPnt[0x01] & 0x80) >> 7;
				SetChipSet(ChipID);
				WriteEvent = rf5c164_reg_write(VGMPnt[0x01] & 0x7F, &VGMPnt[0x02]);
				if (WriteEvent && ChipID && StripVGM[0].RF5C164.All)
					VGMPnt[0x01] &= ~0x80;
				CmdLen = 0x03;
				break;
			case 0xC2:	// RF5C164 memory write
				memcpy(&TempSht, &VGMPnt[0x01], 0x02);
				//ChipID = 0;
				WriteEvent = rf5c164_mem_write(TempSht, VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0xB2:	// PWM register write
				ChipID = 0x00;
				if (StripVGM[ChipID].PWM.All)
					WriteEvent = false;
				CmdLen = 0x03;
				break;
			case 0x68:	// PCM RAM write
				ChipID = (VGMPnt[0x02] & 0x80) >> 7;
				TempByt = VGMPnt[0x02] & 0x7F;
				switch(TempByt)
				{
				case 0x01:	// RF5C68
					if (StripVGM[ChipID].RF5C68.All)
						WriteEvent = false;
					if (WriteEvent && ChipID && StripVGM[0].RF5C68.All)
						VGMPnt[0x02] &= ~0x80;
					break;
				case 0x02:	// RF5C164
					if (StripVGM[ChipID].RF5C164.All)
						WriteEvent = false;
					if (WriteEvent && ChipID && StripVGM[0].RF5C164.All)
						VGMPnt[0x02] &= ~0x80;
					break;
				//case 0x06:	// SCSP
				//	if (StripVGM[ChipID].SCSP.All)
				//		WriteEvent = false;
				//	if (WriteEvent && ChipID && StripVGM[0].SCSP.All)
				//		VGMPnt[0x02] &= ~0x80;
				//	break;
				case 0x07:	// NES APU
					if (StripVGM[ChipID].NESAPU.All)
						WriteEvent = false;
					if (WriteEvent && ChipID && StripVGM[0].NESAPU.All)
						VGMPnt[0x02] &= ~0x80;
					break;
				}
				CmdLen = 0x0C;
				break;
			case 0xA0:	// AY8910 register write
				ChipID = (VGMPnt[0x01] & 0x80) >> 7;
				if (StripVGM[ChipID].AY8910.All)
					WriteEvent = false;
				if (WriteEvent && ChipID && StripVGM[0].AY8910.All)
					VGMPnt[0x01] &= ~0x80;
				CmdLen = 0x03;
				break;
			case 0xB3:	// GameBoy DMG write
				ChipID = (VGMPnt[0x01] & 0x80) >> 7;
				if (StripVGM[ChipID].GBDMG.All)
					WriteEvent = false;
				if (WriteEvent && ChipID && StripVGM[0].GBDMG.All)
					VGMPnt[0x01] &= ~0x80;
				CmdLen = 0x03;
				break;
			case 0xB4:	// NES APU write
				ChipID = (VGMPnt[0x01] & 0x80) >> 7;
				if (StripVGM[ChipID].NESAPU.All)
					WriteEvent = false;
				if (WriteEvent && ChipID && StripVGM[0].NESAPU.All)
					VGMPnt[0x01] &= ~0x80;
				CmdLen = 0x03;
				break;
			case 0xB5:	// MultiPCM write
				ChipID = (VGMPnt[0x01] & 0x80) >> 7;
				if (StripVGM[ChipID].MultiPCM.All)
					WriteEvent = false;
				if (WriteEvent && ChipID && StripVGM[0].MultiPCM.All)
					VGMPnt[0x01] &= ~0x80;
				CmdLen = 0x03;
				break;
			case 0xC3:	// MultiPCM memory write
				ChipID = (VGMPnt[0x01] & 0x80) >> 7;
				if (StripVGM[ChipID].MultiPCM.All)
					WriteEvent = false;
				if (WriteEvent && ChipID && StripVGM[0].MultiPCM.All)
					VGMPnt[0x01] &= ~0x80;
				CmdLen = 0x04;
				break;
			case 0xB6:	// UPD7759 write
				ChipID = (VGMPnt[0x01] & 0x80) >> 7;
				if (StripVGM[ChipID].UPD7759.All)
					WriteEvent = false;
				if (WriteEvent && ChipID && StripVGM[0].UPD7759.All)
					VGMPnt[0x01] &= ~0x80;
				CmdLen = 0x03;
				break;
			case 0xB7:	// OKIM6258 write
				ChipID = (VGMPnt[0x01] & 0x80) >> 7;
				if (StripVGM[ChipID].OKIM6258.All)
					WriteEvent = false;
				if (WriteEvent && ChipID && StripVGM[0].OKIM6258.All)
					VGMPnt[0x01] &= ~0x80;
				CmdLen = 0x03;
				break;
			case 0xB8:	// OKIM6295 write
				ChipID = (VGMPnt[0x01] & 0x80) >> 7;
				if (StripVGM[ChipID].OKIM6295.All)
					WriteEvent = false;
				if (WriteEvent && ChipID && StripVGM[0].OKIM6295.All)
					VGMPnt[0x01] &= ~0x80;
				CmdLen = 0x03;
				break;
			case 0xD2:	// SCC1 write
				ChipID = (VGMPnt[0x01] & 0x80) >> 7;
				if (StripVGM[ChipID].K051649.All)
					WriteEvent = false;
				if (WriteEvent && ChipID && StripVGM[0].K051649.All)
					VGMPnt[0x01] &= ~0x80;
				CmdLen = 0x04;
				break;
			case 0xD3:	// K054539 write
				ChipID = (VGMPnt[0x01] & 0x80) >> 7;
				if (StripVGM[ChipID].K054539.All)
					WriteEvent = false;
				if (WriteEvent && ChipID && StripVGM[0].K054539.All)
					VGMPnt[0x01] &= ~0x80;
				CmdLen = 0x04;
				break;
			case 0xB9:	// HuC6280 write
				ChipID = (VGMPnt[0x01] & 0x80) >> 7;
				if (StripVGM[ChipID].HuC6280.All)
					WriteEvent = false;
				if (WriteEvent && ChipID && StripVGM[0].HuC6280.All)
					VGMPnt[0x01] &= ~0x80;
				CmdLen = 0x03;
				break;
			case 0xD4:	// C140 write
				ChipID = (VGMPnt[0x01] & 0x80) >> 7;
				SetChipSet(ChipID);
				WriteEvent = c140_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02], VGMPnt[0x03]);
				if (WriteEvent && ChipID && StripVGM[0].C140.All)
					VGMPnt[0x01] &= ~0x80;
				CmdLen = 0x04;
				break;
			case 0xBA:	// K053260 write
				ChipID = (VGMPnt[0x01] & 0x80) >> 7;
				if (StripVGM[ChipID].K053260.All)
					WriteEvent = false;
				if (WriteEvent && ChipID && StripVGM[0].K053260.All)
					VGMPnt[0x01] &= ~0x80;
				CmdLen = 0x03;
				break;
			case 0xBB:	// Pokey write
				ChipID = (VGMPnt[0x01] & 0x80) >> 7;
				if (StripVGM[ChipID].Pokey.All)
					WriteEvent = false;
				if (WriteEvent && ChipID && StripVGM[0].Pokey.All)
					VGMPnt[0x01] &= ~0x80;
				CmdLen = 0x03;
				break;
			case 0xC4:	// Q-Sound write
				ChipID = 0x00;
				if (StripVGM[ChipID].QSound.All)
					WriteEvent = false;
				CmdLen = 0x04;
				break;
			case 0x41:	// K007232 write
				ChipID = (VGMPnt[0x01] & 0x80) >> 7;
				if (StripVGM[ChipID].K007232.All)
					WriteEvent = false;
				if (WriteEvent && ChipID && StripVGM[0].K007232.All)
					VGMPnt[0x01] &= ~0x80;
				CmdLen = 0x03;
				break;
			case 0x90:	// DAC Ctrl: Setup Chip
				if (StripVGM[0].DacCtrl.All ||
					GetFromMask(StripVGM[0].DacCtrl.StrMsk, VGMPnt[0x01]))
					WriteEvent = false;
				CmdLen = 0x05;
				break;
			case 0x91:	// DAC Ctrl: Set Data
				if (StripVGM[0].DacCtrl.All ||
					GetFromMask(StripVGM[0].DacCtrl.StrMsk, VGMPnt[0x01]))
					WriteEvent = false;
				CmdLen = 0x05;
				break;
			case 0x92:	// DAC Ctrl: Set Freq
				if (StripVGM[0].DacCtrl.All ||
					GetFromMask(StripVGM[0].DacCtrl.StrMsk, VGMPnt[0x01]))
					WriteEvent = false;
				CmdLen = 0x06;
				break;
			case 0x93:	// DAC Ctrl: Play from Start Pos
				if (StripVGM[0].DacCtrl.All ||
					GetFromMask(StripVGM[0].DacCtrl.StrMsk, VGMPnt[0x01]))
					WriteEvent = false;
				CmdLen = 0x0B;
				break;
			case 0x94:	// DAC Ctrl: Stop immediately
				if (StripVGM[0].DacCtrl.All ||
					GetFromMask(StripVGM[0].DacCtrl.StrMsk, VGMPnt[0x01]))
					WriteEvent = false;
				CmdLen = 0x02;
				break;
			case 0x95:	// DAC Ctrl: Play Block (small)
				if (StripVGM[0].DacCtrl.All ||
					GetFromMask(StripVGM[0].DacCtrl.StrMsk, VGMPnt[0x01]))
					WriteEvent = false;
				CmdLen = 0x05;
				break;
			default:
				if (StripVGM[0].Unknown)
					WriteEvent = false;
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
			while(AllDelay)
			{
				if (AllDelay <= 0xFFFF)
					TempSht = (UINT16)AllDelay;
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
				AllDelay -= TempSht;
			}
			AllDelay = CmdDelay;
			CmdDelay = 0x00;

			if (VGMPos == LoopOfs)
				NewLoopS = DstPos;

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

	if (StripClock(&StripVGM[0].SN76496.All, &VGMHead.lngHzPSG))
	{
		VGMHead.shtPSG_Feedback = 0x00;
		VGMHead.bytPSG_SRWidth = 0x00;
		VGMHead.bytPSG_Flags = 0x00;
	}
	StripClock(&StripVGM[0].YM2413.All, &VGMHead.lngHzYM2413);
	StripClock(&StripVGM[0].YM2612.All, &VGMHead.lngHzYM2612);
	StripClock(&StripVGM[0].YM2151.All, &VGMHead.lngHzYM2151);
	if (StripClock(&StripVGM[0].SegaPCM.All, &VGMHead.lngHzSPCM))
		VGMHead.lngSPCMIntf = 0x00;
	StripClock(&StripVGM[0].RF5C68.All, &VGMHead.lngHzRF5C68);
	if (StripClock(&StripVGM[0].YM2203.All, &VGMHead.lngHzYM2203))
		VGMHead.bytAYFlagYM2203 = 0x00;
	if (StripClock(&StripVGM[0].YM2608.All, &VGMHead.lngHzYM2608))
		VGMHead.bytAYFlagYM2608 = 0x00;
	StripClock(&StripVGM[0].YM2610.All, &VGMHead.lngHzYM2610);
	StripClock(&StripVGM[0].YM3812.All, &VGMHead.lngHzYM3812);
	StripClock(&StripVGM[0].YM3526.All, &VGMHead.lngHzYM3526);
	StripClock(&StripVGM[0].Y8950.All, &VGMHead.lngHzY8950);
	StripClock(&StripVGM[0].YMF278B_All, &VGMHead.lngHzYMF278B);
	StripClock(&StripVGM[0].YMF271.All, &VGMHead.lngHzYMF271);
	StripClock(&StripVGM[0].YMZ280B.All, &VGMHead.lngHzYMZ280B);
	StripClock(&StripVGM[0].RF5C164.All, &VGMHead.lngHzRF5C164);
	StripClock(&StripVGM[0].PWM.All, &VGMHead.lngHzPWM);
	if (StripClock(&StripVGM[0].AY8910.All, &VGMHead.lngHzAY8910))
	{
		VGMHead.bytAYType = 0x00;
		VGMHead.bytAYFlag = 0x00;
	}
	StripClock(&StripVGM[0].GBDMG.All, &VGMHead.lngHzGBDMG);
	StripClock(&StripVGM[0].NESAPU.All, &VGMHead.lngHzNESAPU);
	StripClock(&StripVGM[0].MultiPCM.All, &VGMHead.lngHzMultiPCM);
	StripClock(&StripVGM[0].UPD7759.All, &VGMHead.lngHzUPD7759);
	if (StripClock(&StripVGM[0].OKIM6258.All, &VGMHead.lngHzOKIM6258))
		VGMHead.bytOKI6258Flags = 0x00;
	StripClock(&StripVGM[0].OKIM6295.All, &VGMHead.lngHzOKIM6295);
	StripClock(&StripVGM[0].K051649.All, &VGMHead.lngHzK051649);
	if (StripClock(&StripVGM[0].K054539.All, &VGMHead.lngHzK054539))
		VGMHead.bytK054539Flags = 0x00;
	StripClock(&StripVGM[0].HuC6280.All, &VGMHead.lngHzHuC6280);
	if (StripClock(&StripVGM[0].C140.All, &VGMHead.lngHzC140))
		VGMHead.bytC140Type = 0x00;
	StripClock(&StripVGM[0].K053260.All, &VGMHead.lngHzK053260);
	StripClock(&StripVGM[0].Pokey.All, &VGMHead.lngHzPokey);
	StripClock(&StripVGM[0].QSound.All, &VGMHead.lngHzQSound);
	StripClock(&StripVGM[0].K007232.All, &VGMHead.lngHzK007232);
	//StripClock(&StripVGM[0].SCSP.All, &VGMHead.lngHzSCSP);

	// PatchVGM will rewrite the header later

	VGMData = (UINT8*)realloc(VGMData, VGMDataLen);
	memcpy(VGMData, DstData, VGMDataLen);
	free(DstData);

	FreeAllChips();

	return;
}

static bool StripClock(bool* StripAllPtr, UINT32* ClockPtr)
{
	bool* StpAll1 = (bool*)((STRIP_DATA*)StripAllPtr + 1);	// StripVGM[1].###.All

	if (*ClockPtr & 0x40000000)
	{
		// Chips 0+1 are be stripped
		if (*StripAllPtr && *StpAll1)
		{
			*ClockPtr = 0;	// strip chip clock
			return true;	// strip all chip parameters
		}
		else if (*StripAllPtr || *StpAll1)
		{
			// either of the chips remains used
			*ClockPtr &= ~0x40000000;	// strip Dual Chip flag
		}
	}
	else
	{
		// if Chip 0 to be stripped and Chip 1 is not used
		if (*StripAllPtr)
		{
			*ClockPtr = 0;	// strip Chip 0 clock
			return true;	// strip all chip parameters
		}
	}
	return false;	// keep chip parameters
}
