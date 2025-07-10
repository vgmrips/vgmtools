// vgm2txt.c - VGM Text Writer
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>	// for pow()
#include <zlib.h>

#include "stdtype.h"
#include "stdbool.h"
#include "VGMFile.h"
#include "common.h"
#include "chiptext.h"


static bool OpenVGMFile(const char* FileName);
static void WriteVGM2Txt(const char* FileName);
static void WriteClockText(char* Buffer, UINT32 Clock, char* ChipName);
static void WriteVGMData2Txt(FILE* hFile);



VGM_HEADER VGMHead;
UINT32 VGMDataLen;
UINT8* VGMData;
UINT32 VGMPos;
INT32 VGMSmplPos;
INT32 VGMWriteFrom;
INT32 VGMWriteTo;
char FileBase[0x100];

int main(int argc, char* argv[])
{
	int argbase;
	int ErrVal;
	UINT32 TimeMin;
	UINT32 TimeSec;
	UINT32 TimeMS;
	char FileName[0x100];

	fprintf(stderr, "VGM Text Writer\n---------------\n\n");

	ErrVal = 0;
	argbase = 1;
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

	if (! OpenVGMFile(FileName))
	{
		fprintf(stderr, "Error opening the file!\n");
		ErrVal = 1;
		goto EndProgram;
	}
	fprintf(stderr, "\n");

	TimeMin = TimeSec = TimeMS = 0;
	fprintf(stderr, "Start Time:\t");
	if (argc <= argbase + 1)
	{
		fgets(FileName, sizeof(FileName), stdin);
	}
	else
	{
		strcpy(FileName, argv[argbase + 1]);
		fprintf(stderr, "%s\n", FileName);
	}
	sscanf(FileName, "%u:%u.%u", &TimeMin, &TimeSec, &TimeMS);
	VGMWriteFrom = (TimeMin * 6000 + TimeSec * 100 + TimeMS) * 441;

	TimeMin = TimeSec = TimeMS = 0;
	fprintf(stderr, "End Time:\t");
	if (argc <= argbase + 2)
	{
		fgets(FileName, sizeof(FileName), stdin);
	}
	else
	{
		strcpy(FileName, argv[argbase + 2]);
		fprintf(stderr, "%s\n", FileName);
	}
	sscanf(FileName, "%u:%u.%u", &TimeMin, &TimeSec, &TimeMS);
	VGMWriteTo = (TimeMin * 6000 + TimeSec * 100 + TimeMS) * 441;
	//if (! VGMWriteTo)
	//	VGMWriteTo = VGMHead.lngTotalSamples;

	if (argc > argbase + 3)
		strcpy(FileName, argv[argbase + 3]);
	else
		strcpy(FileName, "");
	if (FileName[0] == '\0')
	{
		strcpy(FileName, FileBase);
		strcat(FileName, ".txt");
	}
	WriteVGM2Txt(FileName);

	free(VGMData);

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
		VGMHead.lngHzYM2612 = 0x00000000;
		VGMHead.lngHzYM2151 = 0x00000000;
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

static void WriteVGM2Txt(const char* FileName)
{
	const char* AY_NAMES[0x14] = {
		"AY-3-8910A", "AY-3-8912A", "AY-3-8913A", "AY8930", "AY-3-8914", NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		"YM2149", "YM3439", "YMZ284", "YMZ294"};
	FILE* hFile;
	UINT32 TempLng;
	char TempStr[0x80];
	INT16 TempSSht;
	double TempDbl;

	memset(TempStr, 0x00, 0x80);

	if (! strcmp(FileName, "-"))
	{
		hFile = stdout;
	}
	else
	{
		hFile = fopen(FileName, "wt");
		if (hFile == NULL)
			return;
	}

	fprintf(hFile, "VGM Header:\n");
	memcpy(TempStr, &VGMData[0x00], 0x04);
	fprintf(hFile, "VGM Signature:\t\t\"%s\"\n", TempStr);
	fprintf(hFile, "File Version:\t\t0x%08X (%u.%02X)\n", VGMHead.lngVersion,
					VGMHead.lngVersion >> 8, VGMHead.lngVersion & 0xFF);
	fprintf(hFile, "EOF Offset:\t\t0x%08X (absolute)\n", VGMHead.lngEOFOffset);
	fprintf(hFile, "GD3 Tag Offset:\t\t0x%08X (absolute)\n", VGMHead.lngGD3Offset);
	fprintf(hFile, "Data Offset:\t\t0x%08X (absolute)\n", VGMHead.lngDataOffset);

	PrintMinSec(VGMHead.lngTotalSamples, TempStr);
	fprintf(hFile, "Total Length:\t\t%u samples (%s s)\n", VGMHead.lngTotalSamples, TempStr);
	fprintf(hFile, "Loop Point Offset:\t0x%08X (absolute)\n", VGMHead.lngLoopOffset);
	if (VGMHead.lngLoopSamples)
		TempLng = VGMHead.lngTotalSamples - VGMHead.lngLoopSamples;
	else
		TempLng = 0;
	PrintMinSec(TempLng, TempStr);
	fprintf(hFile, "Loop Start:\t\t%u samples (%s s)\n", TempLng, TempStr);
	PrintMinSec(VGMHead.lngLoopSamples, TempStr);
	fprintf(hFile, "Loop Length:\t\t%u samples (%s s)\n", VGMHead.lngLoopSamples, TempStr);
	fprintf(hFile, "Recording Rate:\t\t%u Hz\n", VGMHead.lngRate);

	sprintf(TempStr, "%u Hz", VGMHead.lngHzPSG & 0x3FFFFFF);
	if (VGMHead.lngHzPSG & 0x40000000)
	{
		if (VGMHead.lngHzPSG & 0x80000000)
			sprintf(TempStr, "%s - T6W28", TempStr);
		else
			sprintf(TempStr, "%s - Dual %s", TempStr, "SN76496");
	}
	else if (! VGMHead.lngHzPSG)
	{
		sprintf(TempStr, "%s - unused", TempStr);
	}
	fprintf(hFile, "SN76496 Clock:\t\t%s\n", TempStr);
	fprintf(hFile, "SN76496 Feedback:\t0x%02X\n", VGMHead.shtPSG_Feedback);
	fprintf(hFile, "SN76496 ShiftRegWidth:\t%u bits\n", VGMHead.bytPSG_SRWidth);
	fprintf(hFile, "SN76496 Flags:\t\t0x%02X\n", VGMHead.bytPSG_Flags);

	WriteClockText(TempStr, VGMHead.lngHzYM2413, "YM2413");
	fprintf(hFile, "YM2413 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzYM2612, "YM2612");
	fprintf(hFile, "YM2612 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzYM2151, "YM2151");
	fprintf(hFile, "YM2151 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzSPCM, NULL);
	fprintf(hFile, "SegaPCM Clock:\t\t%s\n", TempStr);
	fprintf(hFile, "SegaPCM Interface:\t0x%08X\n", VGMHead.lngSPCMIntf);

	WriteClockText(TempStr, VGMHead.lngHzRF5C68 & ~0x40000000, NULL);
	fprintf(hFile, "RF5C68 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzYM2203, "YM2203");
	fprintf(hFile, "YM2203 Clock:\t\t%s\n", TempStr);
	fprintf(hFile, "YM2203 AY8910 Flags:\t0x%02X\n", VGMHead.bytAYFlagYM2203);

	WriteClockText(TempStr, VGMHead.lngHzYM2608, "YM2608");
	fprintf(hFile, "YM2608 Clock:\t\t%s\n", TempStr);
	fprintf(hFile, "YM2608 AY8910 Flags:\t0x%02X\n", VGMHead.bytAYFlagYM2608);

	sprintf(TempStr, "%u Hz", VGMHead.lngHzYM2610 & ~0xC0000000);
	if (VGMHead.lngHzYM2610 & 0x80000000)
		sprintf(TempStr, "%s (YM2610B Mode)", TempStr);
	if (VGMHead.lngHzYM2610 & 0x40000000)
		sprintf(TempStr, "%s - Dual %s", TempStr, "YM2610");
	else if (! VGMHead.lngHzYM2610)
		sprintf(TempStr, "%s - unused", TempStr);
	fprintf(hFile, "YM2610 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzYM3812, "YM3812");
	fprintf(hFile, "YM3812 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzYM3526, "YM3526");
	fprintf(hFile, "YM3526 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzY8950, "Y8950");
	fprintf(hFile, "Y8950 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzYMF262, "YMF262");
	fprintf(hFile, "YMF262 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzYMF278B, "YMF278B");
	fprintf(hFile, "YMF278B Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzYMF271, "YMF271");
	fprintf(hFile, "YMF271 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzYMZ280B, "YMZ280B");
	fprintf(hFile, "YMZ280B Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzRF5C164 & ~0x40000000, "RF5C164");
	fprintf(hFile, "RF5C164 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzPWM & ~0x40000000, "PWM");
	fprintf(hFile, "PWM Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzAY8910, "AY8910");
	fprintf(hFile, "AY8910 Clock:\t\t%s\n", TempStr);
	if (VGMHead.bytAYType < 0x14 && AY_NAMES[VGMHead.bytAYType] != NULL)
		fprintf(hFile, "AY8910 Type:\t\t0x%02X - %s\n", VGMHead.bytAYType,
				AY_NAMES[VGMHead.bytAYType]);
	else
		fprintf(hFile, "AY8910 Type:\t\t0x%02X - %s\n", VGMHead.bytAYType, "???");
	fprintf(hFile, "AY8910 Flags:\t\t0x%02X\n", VGMHead.bytAYFlag);

	if (VGMHead.bytVolumeModifier <= VOLUME_MODIF_WRAP)
		TempSSht = VGMHead.bytVolumeModifier;
	else if (VGMHead.bytVolumeModifier == (VOLUME_MODIF_WRAP + 0x01))
		TempSSht = VOLUME_MODIF_WRAP - 0x100;
	else
		TempSSht = VGMHead.bytVolumeModifier - 0x100;
	TempDbl = pow(2.0, TempSSht / (double)0x20);
	fprintf(hFile, "Volume Modifier:\t0x%02X (%d) = %.3f\n", VGMHead.bytVolumeModifier,
			TempSSht, TempDbl);

	fprintf(hFile, "Reserved (0x7D):\t0x%02X\n", VGMHead.bytReserved2);

	if (VGMHead.bytLoopBase > 0)
		strcpy(TempStr, "+");
	else if (VGMHead.bytLoopBase < 0)
		strcpy(TempStr, "-");
	else
#ifdef WIN32
		strcpy(TempStr, "\xB1");		// plus-minus character (CP 1252)
#else
		strcpy(TempStr, "\xC2\xB1");	// plus-minus character (UTF-8)
#endif
	fprintf(hFile, "Loop Base:\t\t0x%02X = %s%d\n", VGMHead.bytLoopBase, TempStr,
			VGMHead.bytLoopBase);

	TempDbl = TempSSht / 16.0;
	fprintf(hFile, "Loop Modifier:\t\t0x%02X (MaxLoops * %.2f)\n", VGMHead.bytLoopModifier,
			TempDbl);

	WriteClockText(TempStr, VGMHead.lngHzGBDMG, "GB DMG");
	fprintf(hFile, "GB DMG Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzNESAPU, "NES APU");
	if (VGMHead.lngHzNESAPU & 0x80000000)
		sprintf(TempStr, "%s (with FDS channel)", TempStr);
	fprintf(hFile, "NES APU Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzMultiPCM, "MultiPCM");
	fprintf(hFile, "MultiPCM Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzUPD7759, "UPD7759");
	fprintf(hFile, "UPD7759 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzOKIM6258, "OKIM6258");
	fprintf(hFile, "OKIM6258 Clock:\t\t%s\n", TempStr);
	fprintf(hFile, "OKIM6258 Flags:\t\t0x%02X\n", VGMHead.bytOKI6258Flags);
	fprintf(hFile, "K054539 Flags:\t\t0x%02X\n", VGMHead.bytK054539Flags);
	fprintf(hFile, "C140 Type:\t\t0x%02X\n", VGMHead.bytC140Type);

	WriteClockText(TempStr, VGMHead.lngHzOKIM6295, "OKIM6295");
	fprintf(hFile, "OKIM6295 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzK051649, "K051649");
	if (VGMHead.lngHzK051649 & 0x80000000)
		sprintf(TempStr, "%s (SCC+ mode)", TempStr);
	fprintf(hFile, "K051649 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzK054539, "K054539");
	fprintf(hFile, "K054539 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzHuC6280, "HuC6280");
	fprintf(hFile, "HuC6280 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzC140, "C140");
	fprintf(hFile, "C140 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzK053260, "K053260");
	fprintf(hFile, "K053260 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzPokey, "Pokey");
	fprintf(hFile, "Pokey Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzQSound & ~0x40000000, "QSound");
	fprintf(hFile, "QSound Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzSCSP, "SCSP");
	fprintf(hFile, "SCSP Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzWSwan, "WSwan");
	fprintf(hFile, "WonderSwan Clock:\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzVSU, "VSU");
	fprintf(hFile, "VSU Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzSAA1099, "SAA1099");
	fprintf(hFile, "SAA1099 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzES5503, "ES5503");
	fprintf(hFile, "ES5503 Clock:\t\t%s\n", TempStr);

	if (VGMHead.lngHzNESAPU & 0x80000000)
		WriteClockText(TempStr, VGMHead.lngHzES5506, "ES5506");
	else
		WriteClockText(TempStr, VGMHead.lngHzES5506, "ES5505");
	fprintf(hFile, "ES5506 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzX1_010, "X1-010");
	fprintf(hFile, "X1-010 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzC352, "C352");
	fprintf(hFile, "C352 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzGA20, "GA20");
	fprintf(hFile, "GA20 Clock:\t\t%s\n", TempStr);

	WriteClockText(TempStr, VGMHead.lngHzMikey, "MIKEY");
	fprintf(hFile, "MIKEY Clock:\t\t%s\n", TempStr);
	
	WriteClockText(TempStr, VGMHead.lngHzK007232, "K007232");
	fprintf(hFile, "K007232 Clock:\t\t%s\n", TempStr);
	
	fprintf(hFile, "\n");
	fprintf(hFile, "VGMData:\n");
	VGMPos = VGMHead.lngDataOffset;
	VGMSmplPos = 0;

#if ! defined(_DEBUG) && defined(_MSC_VER)
	// Catch possible crashes (Windows non-debug only)
	__try
	{
		WriteVGMData2Txt(hFile);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		fprintf(stderr, "Program Error!\n");
	}
#else
	WriteVGMData2Txt(hFile);
#endif

	if (hFile != stdout)
		fclose(hFile);

	fprintf(stderr, "File written.\n");

	return;
}

static void WriteClockText(char* Buffer, UINT32 Clock, char* ChipName)
{
	sprintf(Buffer, "%u Hz", Clock & 0x3FFFFFF);
	if (Clock & 0x40000000)
		sprintf(Buffer, "%s - Dual %s", Buffer, ChipName);
	else if (! Clock)
		sprintf(Buffer, "%s - unused", Buffer);

	return;
}

static void WriteVGMData2Txt(FILE* hFile)
{
	UINT8 Command;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	UINT32 ROMSize;
	UINT32 DataStart;
	UINT32 DataLen;
#ifdef WIN32
	UINT32 CmdTimer;
#endif
	char TempStr[0x100];
	char MinSecStr[0x80];
	UINT32 CmdLen;
	bool StopVGM;
	bool WriteEvents;
	UINT32 ChipCounters[0x20];
	UINT8 CurChip;
	const UINT8* VGMPnt;

	for (TempByt = 0x00; TempByt < 0x20; TempByt ++)
	{
		switch(TempByt)
		{
		case 0x00:
			TempLng = VGMHead.lngHzPSG;
			break;
		case 0x01:
			TempLng = VGMHead.lngHzYM2413;
			break;
		case 0x02:
			TempLng = VGMHead.lngHzYM2612;
			break;
		case 0x03:
			TempLng = VGMHead.lngHzYM2151;
			break;
		case 0x04:
			TempLng = VGMHead.lngHzSPCM;
			break;
		case 0x05:
			TempLng = VGMHead.lngHzRF5C68;
			break;
		case 0x06:
			TempLng = VGMHead.lngHzYM2203;
			break;
		case 0x07:
			TempLng = VGMHead.lngHzYM2608;
			break;
		case 0x08:
			TempLng = VGMHead.lngHzYM2610;
			break;
		case 0x09:
			TempLng = VGMHead.lngHzYM3812;
			break;
		case 0x0A:
			TempLng = VGMHead.lngHzYM3526;
			break;
		case 0x0B:
			TempLng = VGMHead.lngHzY8950;
			break;
		case 0x0C:
			TempLng = VGMHead.lngHzYMF262;
			break;
		case 0x0D:
			TempLng = VGMHead.lngHzYMF278B;
			break;
		case 0x0E:
			TempLng = VGMHead.lngHzYMF271;
			break;
		case 0x0F:
			TempLng = VGMHead.lngHzYMZ280B;
			break;
		case 0x10:
			TempLng = VGMHead.lngHzRF5C164;
			break;
		case 0x11:
			TempLng = VGMHead.lngHzPWM;
			break;
		case 0x12:
			TempLng = VGMHead.lngHzAY8910;
			break;
		case 0x13:
			TempLng = VGMHead.lngHzGBDMG;
			break;
		case 0x14:
			TempLng = VGMHead.lngHzNESAPU;
			break;
		case 0x15:
			TempLng = VGMHead.lngHzMultiPCM;
			break;
		case 0x16:
			TempLng = VGMHead.lngHzUPD7759;
			break;
		case 0x17:
			TempLng = VGMHead.lngHzOKIM6258;
			break;
		case 0x18:
			TempLng = VGMHead.lngHzOKIM6295;
			break;
		case 0x19:
			TempLng = VGMHead.lngHzK051649;
			break;
		case 0x1A:
			TempLng = VGMHead.lngHzK054539;
			break;
		case 0x1B:
			TempLng = VGMHead.lngHzHuC6280;
			break;
		case 0x1C:
			TempLng = VGMHead.lngHzC140;
			break;
		case 0x1D:
			TempLng = VGMHead.lngHzK053260;
			break;
		case 0x1E:
			TempLng = VGMHead.lngHzPokey;
			break;
		case 0x1F:
			TempLng = VGMHead.lngHzQSound;
			break;
		default:
			TempLng = 0x00;
			break;
		}
		if (TempLng)
		{
			TempSht = (TempLng & 0x40000000) ? 0x02 : 0x01;
			TempLng = (TempLng & 0x80000000) | TempSht;
		}
		ChipCounters[TempByt] = TempLng;
	}
	InitChips(ChipCounters);
	if (VGMHead.lngHzES5506)
	{
		SetChip(0);
		TempByt = (VGMHead.lngHzES5506 >> 31) & 0x01;
		es5506_w(NULL, 0xFF, TempByt);
		if (VGMHead.lngHzES5506 & 0x40000000)
		{
			SetChip(1);
			TempByt = (VGMHead.lngHzES5506 >> 31) & 0x01;
			es5506_w(NULL, 0xFF, TempByt);
		}
	}

#ifdef WIN32
	CmdTimer = 0;
#endif
	StopVGM = false;
	WriteEvents = (VGMSmplPos >= VGMWriteFrom);
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
				WriteEvents = (VGMSmplPos >= VGMWriteFrom);
				if (WriteEvents)
				{
					PrintMinSec(VGMSmplPos, MinSecStr);
					sprintf(TempStr, "Wait:\t%2u sample(s) (   %03.2f ms)\t(total\t%u (%s))",
							TempSht, TempSht / 44.1, VGMSmplPos, MinSecStr);
				}
				break;
			case 0x80:
				TempSht = Command & 0x0F;
				VGMSmplPos += TempSht;
				WriteEvents = (VGMSmplPos >= VGMWriteFrom);
				if (WriteEvents)
				{
					PrintMinSec(VGMSmplPos, MinSecStr);
					sprintf(TempStr, "Wait:\t%2u sample(s) (   %03.2f ms)\t(total\t%u (%s))",
							TempSht, TempSht / 44.1, VGMSmplPos, MinSecStr);
				}
				break;
			}
			CmdLen = 0x01;
		}
		else
		{
			VGMPnt = &VGMData[VGMPos];

			// Cheat Mode (to use 2 instances of 1 chip)
			CurChip = 0x00;
			switch(Command)
			{
			case 0x30:
				if (VGMHead.lngHzPSG & 0x40000000)
				{
					Command += 0x20;
					CurChip = 0x01;
				}
				break;
			case 0x3F:
				if (VGMHead.lngHzPSG & 0x40000000)
				{
					Command += 0x10;
					CurChip = 0x01;
				}
				break;
			case 0xA1:
				if (VGMHead.lngHzYM2413 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xA2:
			case 0xA3:
				if (VGMHead.lngHzYM2612 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xA4:
				if (VGMHead.lngHzYM2151 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xA5:
				if (VGMHead.lngHzYM2203 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xA6:
			case 0xA7:
				if (VGMHead.lngHzYM2608 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xA8:
			case 0xA9:
				if (VGMHead.lngHzYM2610 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xAA:
				if (VGMHead.lngHzYM3812 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xAB:
				if (VGMHead.lngHzYM3526 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xAC:
				if (VGMHead.lngHzY8950 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xAE:
			case 0xAF:
				if (VGMHead.lngHzYMF262 & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			case 0xAD:
				if (VGMHead.lngHzYMZ280B & 0x40000000)
				{
					Command -= 0x50;
					CurChip = 0x01;
				}
				break;
			}
			SetChip(CurChip);

			strcpy(TempStr, "unsupported chip");
			switch(Command)
			{
			case 0x66:	// End Of File
				sprintf(TempStr, "End of Data");
				CmdLen = 0x01;
				StopVGM = true;
				break;
			case 0x62:	// 1/60s delay
				TempSht = 735;
				VGMSmplPos += TempSht;
				WriteEvents = (VGMSmplPos >= VGMWriteFrom);
				if (WriteEvents)
				{
					PrintMinSec(VGMSmplPos, MinSecStr);
					sprintf(TempStr, "Wait:\t%u samples (1/60 s)\t(total\t%u (%s))",
							TempSht, VGMSmplPos, MinSecStr);
				}
				CmdLen = 0x01;
				break;
			case 0x63:	// 1/50s delay
				TempSht = 882;
				VGMSmplPos += TempSht;
				WriteEvents = (VGMSmplPos >= VGMWriteFrom);
				if (WriteEvents)
				{
					PrintMinSec(VGMSmplPos, MinSecStr);
					sprintf(TempStr, "Wait:\t%u samples (1/50 s)\t(total\t%u (%s))",
							TempSht, VGMSmplPos, MinSecStr);
				}
				CmdLen = 0x01;
				break;
			case 0x61:	// xx Sample Delay
				memcpy(&TempSht, &VGMPnt[0x01], 0x02);
				VGMSmplPos += TempSht;
				WriteEvents = (VGMSmplPos >= VGMWriteFrom);
				if (WriteEvents)
				{
					PrintMinSec(VGMSmplPos, MinSecStr);
					sprintf(TempStr, "Wait:\t%u samples (   %03.2f ms)\t(total\t%u (%s))",
							TempSht, TempSht / 44.1, VGMSmplPos, MinSecStr);
				}
				CmdLen = 0x03;
				break;
			case 0x50:	// SN76496 write
				if (WriteEvents)
				{
					sn76496_write(TempStr, VGMPnt[0x01]);
				}
				CmdLen = 0x02;
				break;
			case 0x51:	// YM2413 write
				if (WriteEvents)
				{
					ym2413_write(TempStr, VGMPnt[0x01], VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0x52:	// YM2612 write port 0
			case 0x53:	// YM2612 write port 1
				if (WriteEvents)
				{
					ym2612_write(TempStr, Command & 0x01, VGMPnt[0x01], VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0x67:	// Data Block (PCM Data Stream)
				TempByt = VGMPnt[0x02];
				memcpy(&TempLng, &VGMPnt[0x03], 0x04);

				CurChip = (TempLng & 0x80000000) >> 31;
				TempLng &= 0x7FFFFFFF;

				if (WriteEvents)
				{
					switch(TempByt & 0xC0)
					{
					case 0x00:	// Database Block
					case 0x40:
						DataLen = TempLng;
						switch(TempByt & 0x3F)
						{
						case 0x00:	// YM2612 PCM Database
							strcpy(MinSecStr, "YM2612 PCM Database");
							break;
						case 0x01:	// RF5C68 PCM Database
							strcpy(MinSecStr, "RF5C68 PCM Database");
							break;
						case 0x02:	// RF5C164 PCM Database
							strcpy(MinSecStr, "RF5C164 PCM Database");
							break;
						case 0x08:	// SN76489 PCM Database
							strcpy(MinSecStr, "SN76489 PCM Database");
							break;
						default:
							strcpy(MinSecStr, "Unknown Database Type");
							break;
						}

						if ((TempByt & 0xC0) == 0x40)
							strcat(MinSecStr, " (compressed)");
						break;
					case 0x80:	// ROM/RAM Dump
						memcpy(&ROMSize, &VGMPnt[0x07], 0x04);
						memcpy(&DataStart, &VGMPnt[0x0B], 0x04);
						DataLen = TempLng - 0x08;
						switch(TempByt)
						{
						case 0x80:	// SegaPCM ROM
							strcpy(MinSecStr, "SegaPCM ROM");
							break;
						case 0x81:	// YM2608 DELTA-T ROM Image
							strcpy(MinSecStr, "YM2608 DELTA-T ROM");
							break;
						case 0x82:	// YM2610 ADPCM ROM Image
							strcpy(MinSecStr, "YM2610 ADPCM ROM");
							break;
						case 0x83:	// YM2610 DELTA-T ROM Image
							strcpy(MinSecStr, "YM2610 DELTA-T ROM");
							break;
						case 0x84:	// YMF278B ROM Image
							strcpy(MinSecStr, "YMF278B ROM");
							break;
						case 0x85:	// YMF271 ROM Image
							strcpy(MinSecStr, "YMF271 ROM");
							break;
						case 0x86:	// YMZ280B ROM Image
							strcpy(MinSecStr, "YMZ280B ROM");
							break;
						case 0x87:	// YMF278B RAM Image
							strcpy(MinSecStr, "YMF278B ROM");
							break;
						case 0x88:	// Y8950 DELTA-T ROM Image
							strcpy(MinSecStr, "Y8950 DELTA-T ROM");
							break;
						case 0x89:	// MultiPCM ROM Image
							strcpy(MinSecStr, "MultiPCM ROM");
							break;
						case 0x8A:	// UPD7759 ROM Image
							strcpy(MinSecStr, "UPD7759 ROM");

							SetChip(CurChip);
							upd7759_write(NULL, 0xFF, 0x01);
							break;
						case 0x8B:	// OKIM6295 ROM Image
							strcpy(MinSecStr, "OKIM6295 ROM");
							break;
						case 0x8C:	// K054539 ROM Image
							strcpy(MinSecStr, "K054539 ROM");
							break;
						case 0x8D:	// C140 ROM Image
							strcpy(MinSecStr, "C140 ROM");
							break;
						case 0x8E:	// K053260 ROM Image
							strcpy(MinSecStr, "K053260 ROM");
							break;
						case 0x8F:	// QSound ROM Image
							strcpy(MinSecStr, "QSound ROM");
							break;
						case 0x90:	// ES5506 ROM Image
							strcpy(MinSecStr, "ES5506 ROM");
							break;
						case 0x91:	// X1-010 ROM Image
							strcpy(MinSecStr, "X1-010 ROM");
							break;
						case 0x92:	// C352 ROM Image
							strcpy(MinSecStr, "C352 ROM");
							break;
						case 0x93:	// GA20 ROM Image
							strcpy(MinSecStr, "GA20 ROM");
							break;
						case 0x94:	// K007232 ROM Image
							strcpy(MinSecStr, "K007232 ROM");
							break;
						default:
							strcpy(MinSecStr, "Unknown ROM Type");
							break;
						}
						break;
					case 0xC0:	// RAM Write
						if (TempByt & 0x20)
						{
							memcpy(&DataStart, &VGMPnt[0x07], 0x04);
							DataLen = TempLng - 0x04;
						}
						else
						{
							DataStart = 0x00;
							memcpy(&DataStart, &VGMPnt[0x07], 0x02);
							DataLen = TempLng - 0x02;
						}
						switch(TempByt)
						{
						case 0xC0:	// RF5C68 PCM Data
							strcpy(MinSecStr, "RF5C68 RAM Data");
							break;
						case 0xC1:	// RF5C164 PCM Data
							strcpy(MinSecStr, "RF5C164 RAM Data");
							break;
						case 0xC2:	// NES APU DPCM Data
							strcpy(MinSecStr, "NES APU RAM Data");
							break;
						case 0xE0:	// SCSP PCM Data
							strcpy(MinSecStr, "SCSP RAM Data");
							break;
						default:
							strcpy(MinSecStr, "Unknown RAM Data");
							break;
						}
						break;
					}

					sprintf(TempStr, "Data Block - Type %02X: %s%s\n", TempByt, MinSecStr,
									(CurChip ? " (2nd chip)" : "") );
					sprintf(TempStr, "%s\t\t\tData Length: 0x%08X", TempStr, DataLen);
					switch(TempByt & 0xC0)
					{
					case 0x00:	// Database Block
						break;
					case 0x40:	// compressed Database Block
						// TODO: print information about compression type etc.
						break;
					case 0x80:	// ROM/RAM Dump
						sprintf(TempStr, "%s\n\t\t\tROM Size: 0x%08X\tData Start: 0x%08X",
								TempStr, ROMSize, DataStart);
						break;
					case 0xC0:	// RAM Write
						sprintf(TempStr, "%s\t\tWrite to Address: 0x%0*X",
								TempStr, (TempByt & 0x20) ? 6 : 4, DataStart);
						break;
					}
				}
				CmdLen = 0x07 + TempLng;
				break;
			case 0xE0:	// Seek to PCM Data Bank Pos
				memcpy(&TempLng, &VGMPnt[0x01], 0x04);
				if (WriteEvents)
				{
					sprintf(TempStr, "Seek to PCM Data Bank Pos: %08X", TempLng);
				}
				CmdLen = 0x05;
				break;
			case 0x4F:	// GG Stereo
				if (WriteEvents)
				{
					GGStereo(TempStr, VGMPnt[0x01]);
				}
				CmdLen = 0x02;
				break;
			case 0x54:	// YM2151 write
				if (WriteEvents)
				{
					ym2151_write(TempStr, VGMPnt[0x01], VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0xC0:	// Sega PCM memory write
				memcpy(&TempSht, &VGMPnt[0x01], 0x02);
				if (WriteEvents)
				{
					segapcm_mem_write(TempStr, TempSht, VGMPnt[0x03]);
				}
				CmdLen = 0x04;
				break;
			case 0xB0:	// RF5C68 register write
				if (WriteEvents)
				{
					rf5c68_reg_write(TempStr, VGMPnt[0x01], VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0xC1:	// RF5C68 memory write
				memcpy(&TempSht, &VGMPnt[0x01], 0x02);
				if (WriteEvents)
				{
					rf5c68_mem_write(TempStr, TempSht, VGMPnt[0x03]);
				}
				CmdLen = 0x04;
				break;
			case 0x55:	// YM2203
				if (WriteEvents)
				{
					ym2203_write(TempStr, VGMPnt[0x01], VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0x56:	// YM2608 write port 0
			case 0x57:	// YM2608 write port 1
				if (WriteEvents)
				{
					ym2608_write(TempStr, Command & 0x01, VGMPnt[0x01], VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0x58:	// YM2610 write port 0
			case 0x59:	// YM2610 write port 1
				if (WriteEvents)
				{
					ym2610_write(TempStr, Command & 0x01, VGMPnt[0x01], VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0x5A:	// YM3812 write
				if (WriteEvents)
				{
					ym3812_write(TempStr, VGMPnt[0x01], VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0x5B:	// YM3526 write
				if (WriteEvents)
				{
					ym3526_write(TempStr, VGMPnt[0x01], VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0x5C:	// Y8950 write
				if (WriteEvents)
				{
					y8950_write(TempStr, VGMPnt[0x01], VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0x5E:	// YMF262 write port 0
			case 0x5F:	// YMF262 write port 1
				if (WriteEvents)
				{
					ymf262_write(TempStr, Command & 0x01, VGMPnt[0x01], VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0x5D:	// YMZ280B write
				if (WriteEvents)
				{
					ymz280b_write(TempStr, VGMPnt[0x01], VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0xD0:	// YMF278B write
				if (WriteEvents)
				{
					ymf278b_write(TempStr, VGMPnt[0x01], VGMPnt[0x02], VGMPnt[0x03]);
				}
				CmdLen = 0x04;
				break;
			case 0xD1:	// YMF271 write
				if (WriteEvents)
				{
					ymf271_write(TempStr, VGMPnt[0x01], VGMPnt[0x02], VGMPnt[0x03]);
				}
				CmdLen = 0x04;
				break;
			case 0xB1:	// RF5C164 register write
				if (WriteEvents)
				{
					rf5c164_reg_write(TempStr, VGMPnt[0x01], VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0xC2:	// RF5C164 memory write
				memcpy(&TempSht, &VGMPnt[0x01], 0x02);
				if (WriteEvents)
				{
					rf5c164_mem_write(TempStr, TempSht, VGMPnt[0x03]);
				}
				CmdLen = 0x04;
				break;
			case 0x68:	// PCM RAM write
				if (WriteEvents)
				{
					TempByt = VGMPnt[0x02];
					DataStart = TempLng = DataLen = 0x00;
					memcpy(&DataStart, &VGMPnt[0x03], 0x03);
					memcpy(&TempLng, &VGMPnt[0x06], 0x03);
					memcpy(&DataLen, &VGMPnt[0x09], 0x03);
					if (! DataLen)
						DataLen += 0x01000000;
					switch(TempByt)
					{
					case 0x01:	// RF5C68 PCM Database
						strcpy(MinSecStr, "RF5C68");
						break;
					case 0x02:	// RF5C164 PCM Database
						strcpy(MinSecStr, "RF5C164");
						break;
					default:
						strcpy(MinSecStr, "Unknown chip");
						break;
					}
					sprintf(TempStr, "PCM RAM write:\t%s\n"
									"\t\t\tCopy 0x%06X Bytes from 0x%06X (Database) "
									"to 0x%06X (RAM)",
									MinSecStr, DataLen, DataStart, TempLng);
				}
				CmdLen = 0x0C;
				break;
			case 0xB2:	// PWM register write
				if (WriteEvents)
				{
					pwm_write(TempStr, (VGMPnt[0x01] & 0xF0) >> 4,
										(VGMPnt[0x01] & 0x0F) << 8 |
										(VGMPnt[0x02] << 0));
				}
				CmdLen = 0x03;
				break;
			case 0xA0:	// AY8910 register write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					ay8910_reg_write(TempStr, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0x31:	// AY8910 stereo mask write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					ay8910_stereo_mask_write(TempStr, VGMPnt[0x01] & 0x7F);
				}
				CmdLen = 0x02;
				break;
			case 0xB3:	// GameBoy DMG write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					gb_sound_write(TempStr, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0xB4:	// NES APU write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					nes_psg_write(TempStr, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0xB5:	// MultiPCM write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					multipcm_write(TempStr, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0xC3:	// MultiPCM memory write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					memcpy(&TempSht, &VGMPnt[0x02], 0x02);
					multipcm_bank_write(TempStr, VGMPnt[0x01] & 0x7F, TempSht);
				}
				CmdLen = 0x04;
				break;
			case 0xB6:	// UPD7759 write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					upd7759_write(TempStr, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0xB7:	// OKIM6258 write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					okim6258_write(TempStr, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0xB8:	// OKIM6295 write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					okim6295_write(TempStr, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0xD2:	// SCC1 write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					k051649_write(TempStr, VGMPnt[0x01] & 0x7F, VGMPnt[0x02], VGMPnt[0x03]);
				}
				CmdLen = 0x04;
				break;
			case 0xD3:	// K054539 write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					TempSht = ((VGMPnt[0x01] & 0x7F) << 8) | (VGMPnt[0x02] << 0);
					k054539_write(TempStr, TempSht, VGMPnt[0x03]);
				}
				CmdLen = 0x04;
				break;
			case 0xB9:	// HuC6280 write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					c6280_write(TempStr, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0xD4:	// C140 write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					c140_write(TempStr, VGMPnt[0x01] & 0x7F, VGMPnt[0x02], VGMPnt[0x03]);
				}
				CmdLen = 0x04;
				break;
			case 0xBA:	// K053260 write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					k053260_write(TempStr, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0xBB:	// Pokey write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					pokey_write(TempStr, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0xC4:	// Q-Sound write
				if (WriteEvents)
				{
					//SetChip(0x00);
					qsound_write(TempStr, VGMPnt[0x03], (VGMPnt[0x01] << 8) | (VGMPnt[0x02] << 0));
				}
				CmdLen = 0x04;
				break;
			case 0xBC:	// WonderSwan write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					wswan_write(TempStr, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0xC6:	// WonderSwan Memory write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					TempSht = ((VGMPnt[0x01] & 0x7F) << 8) | (VGMPnt[0x02] << 0);
					ws_mem_write(TempStr, TempSht, VGMPnt[0x03]);
				}
				CmdLen = 0x04;
				break;
			case 0xC7:	// VSU write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					TempSht = ((VGMPnt[0x01] & 0x7F) << 8) | (VGMPnt[0x02] << 0);
					vsu_write(TempStr, TempSht, VGMPnt[0x03]);
				}
				CmdLen = 0x04;
				break;
			case 0xBD:	// SAA1099 write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					saa1099_write(TempStr, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0xE1:	// C352 write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					c352_write(TempStr, ((VGMPnt[0x01]&0x7F) << 8) | (VGMPnt[0x02] << 0),
								(VGMPnt[0x03] << 8) | (VGMPnt[0x04] << 0));
				}
				CmdLen = 0x05;
				break;
			case 0xD5:	// ES5503 write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					es5503_write(TempStr, VGMPnt[0x02], VGMPnt[0x03]);
				}
				CmdLen = 0x04;
				break;
			case 0xBE:	// ES5506 write (8-bit data)
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					es5506_w(TempStr, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0xD6:	// ES5506 write (16-bit data)
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					es5506_w16(TempStr, VGMPnt[0x01] & 0x7F,
								(VGMPnt[0x02] << 8) | (VGMPnt[0x03] << 0));
				}
				CmdLen = 0x04;
				break;
			case 0xC8:	// X1-010 write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					x1_010_write(TempStr, ((VGMPnt[0x01] & 0x7F)<<8) | VGMPnt[0x02], VGMPnt[0x03]);
				}
				CmdLen = 0x04;
				break;
			case 0x41:	// K007232 write
				if (WriteEvents)
				{
					SetChip((VGMPnt[0x01] & 0x80) >> 7);
					k007232_write(TempStr, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				}
				CmdLen = 0x03;
				break;
			case 0x90:	// DAC Ctrl: Setup Chip
				if (WriteEvents)
				{
					GetFullChipName(MinSecStr, VGMPnt[0x02]);	// Chip Type

					sprintf(TempStr, "DAC Ctrl:\tStream #%u - Setup Chip: %s Reg %02X %02X",
							VGMPnt[0x01], MinSecStr, VGMPnt[0x03], VGMPnt[0x04]);
				}
				CmdLen = 0x05;
				break;
			case 0x91:	// DAC Ctrl: Set Data
				if (WriteEvents)
				{
					sprintf(TempStr, "DAC Ctrl:\tStream #%u - Set Data: Bank %02X, "
							"Step Size %02X, Step Base %02X",
							VGMPnt[0x01], VGMPnt[0x02], VGMPnt[0x03], VGMPnt[0x04]);
				}
				CmdLen = 0x05;
				break;
			case 0x92:	// DAC Ctrl: Set Freq
				if (WriteEvents)
				{
					memcpy(&TempLng, &VGMPnt[0x02], 0x04);
					sprintf(TempStr, "DAC Ctrl:\tStream #%u - Set Frequency: %u Hz",
							VGMPnt[0x01], TempLng);
				}
				CmdLen = 0x06;
				break;
			case 0x93:	// DAC Ctrl: Play from Start Pos
				if (WriteEvents)
				{
					// Loop Flag ("Play" / "Play/Loop")
					strcpy(MinSecStr, "Play");
					if (VGMPnt[0x06] & 0x80)
						strcat(MinSecStr, "/Loop");

					memcpy(&TempLng, &VGMPnt[0x02], 0x04);
					sprintf(TempStr, "DAC Ctrl:\tStream #%u - %s from 0x%02X",
							VGMPnt[0x01], MinSecStr, TempLng);
					switch(VGMPnt[0x06] & 0x7F)	// Length Mode
					{
					case 0x00:	// ignore
						sprintf(TempStr, "%s (length ignored)", TempStr);
						break;
					case 0x01:	// length = number of commands
						memcpy(&TempLng, &VGMPnt[0x07], 0x04);
						sprintf(TempStr, "%s for 0x%02X Commands", TempStr, TempLng);
						break;
					case 0x02:	// length in msec
						memcpy(&TempLng, &VGMPnt[0x07], 0x04);
						sprintf(TempStr, "%s for %u ms", TempStr, TempLng);
						break;
					case 0x03:	// play until end of data
						sprintf(TempStr, "%s until End of Data", TempStr);
						break;
					}
				}
				CmdLen = 0x0B;
				break;
			case 0x94:	// DAC Ctrl: Stop immediately
				if (WriteEvents)
				{
					memcpy(&TempLng, &VGMPnt[0x02], 0x04);
					if (VGMPnt[0x01] < 0xFF)
						sprintf(TempStr, "DAC Ctrl:\tStream #%u - Stop Stream", VGMPnt[0x01]);
					else
						sprintf(TempStr, "DAC Ctrl:\tStop All Streams");
				}
				CmdLen = 0x02;
				break;
			case 0x95:	// DAC Ctrl: Play Block (small)
				if (WriteEvents)
				{
					// Loop Flag ("Play" / "Play/Loop")
					strcpy(MinSecStr, "Play");
					if (VGMPnt[0x04] & 0x01)
						strcat(MinSecStr, "/Loop");

					memcpy(&TempSht, &VGMPnt[0x02], 0x02);
					sprintf(TempStr, "DAC Ctrl:\tStream #%u - %s: Block 0x%02X",
							VGMPnt[0x01], MinSecStr, TempSht);
				}
				CmdLen = 0x05;
				break;
			default:
				if (WriteEvents)
				{
					sprintf(TempStr, "Unknown command: %02X", Command);
				}

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
					CmdLen = 0x01;
					//StopVGM = true;
					break;
				}
				break;
			}
		}

		if (WriteEvents)
		{
			if (VGMPos == VGMHead.lngLoopOffset)
				fprintf(hFile, "--- Loop Point ---\n");
			for (TempLng = 0x00; TempLng < CmdLen; TempLng ++)
			{
				if ((Command == 0x67 || Command == 0x68) && TempLng >= 0x03)
					break;
				if (TempLng >= 0x04)
					break;
				sprintf(MinSecStr + TempLng * 0x03, "%02X ", VGMData[VGMPos + TempLng]);
			}
			if (TempLng < 0x04)
				memset(MinSecStr + TempLng * 0x03, ' ', (0x04 - TempLng) * 0x03);
			MinSecStr[0x04 * 0x03 - 0x01] = 0x00;

			fprintf(hFile, "0x%08X: %s %s\n", VGMPos, MinSecStr, TempStr);
		}
		if (VGMWriteTo && VGMSmplPos > VGMWriteTo)
			StopVGM = true;

		VGMPos += CmdLen;
		if (StopVGM)
			break;

#ifdef WIN32
		if (CmdTimer < GetTickCount())
		{
			PrintMinSec(VGMSmplPos, MinSecStr);
			PrintMinSec(VGMHead.lngTotalSamples, TempStr);
			if (VGMSmplPos >= VGMWriteFrom)
				TempLng = VGMSmplPos - VGMWriteFrom;
			else
				TempLng = 0;
			if (VGMWriteTo)
				ROMSize = VGMWriteTo - VGMWriteFrom;
			else
				ROMSize = VGMHead.lngTotalSamples - VGMWriteFrom;
			fprintf(stderr, "%04.3f %% - %s / %s (%08X / %08X) ...\r", (float)TempLng / ROMSize * 100,
					MinSecStr, TempStr, VGMPos, VGMHead.lngEOFOffset);
			CmdTimer = GetTickCount() + 200;
		}
#endif
	}
	fprintf(stderr, "%*s\r", 64, "");

	return;
}
