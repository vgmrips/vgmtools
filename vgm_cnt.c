// vgm_cnt.c - VGM Command Counter
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


typedef struct chip_count_state
{
	bool Used;
	UINT8 ChnReg;
	UINT32 CmdCount;
	INT32 KeyOnCnt;
	UINT32 KeyState;
	UINT32 VolState;
} CHIP_STATE;


static bool OpenVGMFile(const char* FileName);
static void CountVGMData(void);
static void print_wordnum(const char* Word, INT32 Number);
static void DoChipCommand(UINT8 ChipSet, UINT8 ChipID, UINT16 Reg, UINT16 Data);
static void DoKeyOnOff(CHIP_STATE* CState, UINT8 Chn, UINT8 OnOff, UINT8 VolFlag);


#define CHIP_COUNT	0x2A

VGM_HEADER VGMHead;
UINT32 VGMDataLen;
UINT8* VGMData;
UINT32 VGMPos;
UINT32 VGMSmplPos;
CHIP_STATE ChipState[0x02][2][CHIP_COUNT];

int main(int argc, char* argv[])
{
	int argbase;
	int ErrVal;
	char FileName[0x100];

	printf("VGM Command Counter\n-------------------\n\n");

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

	CountVGMData();

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

	return true;

OpenErr:

	gzclose(hFile);
	return false;
}

static void CountVGMData()
{
	const char* CHIP_STRS[CHIP_COUNT] =
	{	"SN76496", "YM2413", "YM2612", "YM2151", "SegaPCM", "RF5C68", "YM2203", "YM2608",
		"YM2610", "YM3812", "YM3526", "Y8950", "YMF262", "YMF278B PCM", "YMF271", "YMZ280B",
		"RF5C164", "PWM", "AY8910", "GameBoy", "NES APU", "MultiPCM", "uPD7759", "OKIM6258",
		"OKIM6295", "K051649", "K054539", "HuC6280", "C140", "K053260", "Pokey", "QSound",
		"K007232"};
	const char* SPCCHIP_STRS[CHIP_COUNT] =
	{	"", "", "", "", "", "", "", "",
		"", "", "", "", "", "YMF278B FM", "", "",
		"", "", "", "", "FDS", "", "", "",
		"", "", "", "", "", "", "", ""};

	UINT8 Command;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	UINT32 ROMSize;
	UINT32 DataStart;
	UINT32 DataLen;
	UINT32 CmdLen;
	bool StopVGM;
	UINT32 ChipCounters[CHIP_COUNT];
	UINT8 CurChip;
	const UINT8* VGMPnt;

	memset(ChipState, 0x00, sizeof(ChipState));
	ChipState[0x00][0][0x18].ChnReg = 0xFF;
	ChipState[0x01][0][0x18].ChnReg = 0xFF;
	for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++)
	{
		switch(CurChip)
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
		case 0x2A:
			TempLng = VGMHead.lngHzK007232;
			break;
		default:
			TempLng = 0x00;
			break;
		}
		if (TempLng)
		{
			ChipState[0x00][0][CurChip].Used = true;
			ChipState[0x01][0][CurChip].Used = (TempLng & 0x40000000) >> 30;
		}
		else
		{
			ChipState[0x00][0][CurChip].Used = false;
			ChipState[0x01][0][CurChip].Used = false;
		}
		switch(CurChip)
		{
		case 0x14:	// NES APU - FDS
			if (TempLng & 0x80000000)
			{
				ChipState[0x00][1][CurChip].Used = ChipState[0x00][0][CurChip].Used;
				ChipState[0x01][1][CurChip].Used = ChipState[0x01][1][CurChip].Used;
			}
			else
			{
				ChipState[0x00][1][CurChip].Used = false;
				ChipState[0x01][1][CurChip].Used = false;
			}
			break;
		}
		ChipCounters[CurChip] = TempLng;
	}

	StopVGM = false;
	VGMPos = VGMHead.lngDataOffset;
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
				DoChipCommand(CurChip, 0x00, 0x00, VGMPnt[0x01]);
				CmdLen = 0x02;
				break;
			case 0x51:	// YM2413 write
				DoChipCommand(CurChip, 0x01, VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0x52:	// YM2612 write port 0
			case 0x53:	// YM2612 write port 1
				DoChipCommand(CurChip, 0x02, ((Command & 0x01) << 8) | VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0x67:	// Data Block (PCM Data Stream)
				TempByt = VGMPnt[0x02];
				memcpy(&TempLng, &VGMPnt[0x03], 0x04);

				CurChip = (TempLng & 0x80000000) >> 31;
				TempLng &= 0x7FFFFFFF;

				switch(TempByt & 0xC0)
				{
				case 0x00:	// Database Block
				case 0x40:
					DataLen = TempLng;
					switch(TempByt & 0x3F)
					{
					case 0x00:	// YM2612 PCM Database
						DoChipCommand(CurChip, 0x02, 0xFFFF, TempByt);
						break;
					case 0x01:	// RF5C68 PCM Database
						DoChipCommand(CurChip, 0x05, 0xFFFF, TempByt);
						break;
					case 0x02:	// RF5C164 PCM Database
						DoChipCommand(CurChip, 0x10, 0xFFFF, TempByt);
						break;
					default:
						break;
					}
					break;
				case 0x80:	// ROM/RAM Dump
					memcpy(&ROMSize, &VGMPnt[0x07], 0x04);
					memcpy(&DataStart, &VGMPnt[0x0B], 0x04);
					DataLen = TempLng - 0x08;
					switch(TempByt)
					{
					case 0x80:	// SegaPCM ROM
						DoChipCommand(CurChip, 0x04, 0xFFFF, TempByt);
						break;
					case 0x81:	// YM2608 DELTA-T ROM Image
						DoChipCommand(CurChip, 0x07, 0xFFFF, TempByt);
						break;
					case 0x82:	// YM2610 ADPCM ROM Image
						DoChipCommand(CurChip, 0x08, 0xFFFF, TempByt);
						break;
					case 0x83:	// YM2610 DELTA-T ROM Image
						DoChipCommand(CurChip, 0x08, 0xFFFF, TempByt);
						break;
					case 0x84:	// YMF278B ROM Image
						DoChipCommand(CurChip, 0x0D, 0xFFFF, TempByt);
						break;
					case 0x85:	// YMF271 ROM Image
						DoChipCommand(CurChip, 0x0E, 0xFFFF, TempByt);
						break;
					case 0x86:	// YMZ280B ROM Image
						DoChipCommand(CurChip, 0x0F, 0xFFFF, TempByt);
						break;
					case 0x87:	// YMF278B RAM Image
						DoChipCommand(CurChip, 0x0D, 0xFFFF, TempByt);
						break;
					case 0x88:	// Y8950 DELTA-T ROM Image
						DoChipCommand(CurChip, 0x0B, 0xFFFF, TempByt);
						break;
					case 0x89:	// MultiPCM ROM Image
						DoChipCommand(CurChip, 0x15, 0xFFFF, TempByt);
						break;
					case 0x8A:	// UPD7759 ROM Image
						DoChipCommand(CurChip, 0x16, 0xFFFF, TempByt);
						break;
					case 0x8B:	// OKIM6295 ROM Image
						DoChipCommand(CurChip, 0x18, 0xFFFF, TempByt);
						break;
					case 0x8C:	// K054539 ROM Image
						DoChipCommand(CurChip, 0x1A, 0xFFFF, TempByt);
						break;
					case 0x8D:	// C140 ROM Image
						DoChipCommand(CurChip, 0x1C, 0xFFFF, TempByt);
						break;
					case 0x8E:	// K053260 ROM Image
						DoChipCommand(CurChip, 0x1D, 0xFFFF, TempByt);
						break;
					case 0x8F:	// QSound ROM Image
						DoChipCommand(CurChip, 0x1F, 0xFFFF, TempByt);
						break;
					case 0x94:	// K007232 ROM Image
						DoChipCommand(CurChip, 0x2A, 0xFFFF, TempByt);
						break;
					default:
						break;
					}
					break;
				case 0xC0:	// RAM Write
					memcpy(&TempSht, &VGMPnt[0x07], 0x02);
					DataLen = TempLng - 0x02;
					switch(TempByt)
					{
					case 0xC0:	// RF5C68 PCM Data
						DoChipCommand(CurChip, 0x05, 0xFFFF, TempByt);
						break;
					case 0xC1:	// RF5C164 PCM Data
						DoChipCommand(CurChip, 0x10, 0xFFFF, TempByt);
						break;
					case 0xC2:	// NES APU DPCM Data
						DoChipCommand(CurChip, 0x14, 0xFFFF, TempByt);
						break;
					default:
						break;
					}
					break;
				}
				CmdLen = 0x07 + TempLng;
				break;
			case 0xE0:	// Seek to PCM Data Bank Pos
				memcpy(&TempLng, &VGMPnt[0x01], 0x04);
				CmdLen = 0x05;
				break;
			case 0x4F:	// GG Stereo
				DoChipCommand(CurChip, 0x00, 0x01, VGMPnt[0x01]);
				CmdLen = 0x02;
				break;
			case 0x54:	// YM2151 write
				DoChipCommand(CurChip, 0x03, VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xC0:	// Sega PCM memory write
				memcpy(&TempSht, &VGMPnt[0x01], 0x02);
				DoChipCommand(CurChip, 0x04, TempSht, VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0xB0:	// RF5C68 register write
				DoChipCommand(CurChip, 0x05, VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xC1:	// RF5C68 memory write
				memcpy(&TempSht, &VGMPnt[0x01], 0x02);
				//DoChipCommand(CurChip, 0x05, TempSht, VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0x55:	// YM2203
				DoChipCommand(CurChip, 0x06, VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0x56:	// YM2608 write port 0
			case 0x57:	// YM2608 write port 1
				DoChipCommand(CurChip, 0x07, ((Command & 0x01) << 8) | VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0x58:	// YM2610 write port 0
			case 0x59:	// YM2610 write port 1
				DoChipCommand(CurChip, 0x08, ((Command & 0x01) << 8) | VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0x5A:	// YM3812 write
				DoChipCommand(CurChip, 0x09, VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0x5B:	// YM3526 write
				DoChipCommand(CurChip, 0x0A, VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0x5C:	// Y8950 write
				DoChipCommand(CurChip, 0x0B, VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0x5E:	// YMF262 write port 0
			case 0x5F:	// YMF262 write port 1
				DoChipCommand(CurChip, 0x0C, ((Command & 0x01) << 8) | VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0x5D:	// YMZ280B write
				DoChipCommand(CurChip, 0x0F, VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xD0:	// YMF278B write
				DoChipCommand(CurChip, 0x0D, (VGMPnt[0x01] << 8) | VGMPnt[0x02], VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0xD1:	// YMF271 write
				DoChipCommand(CurChip, 0x0E, (VGMPnt[0x01] << 8) | VGMPnt[0x02], VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0xB1:	// RF5C164 register write
				DoChipCommand(CurChip, 0x10, VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xC2:	// RF5C164 memory write
				memcpy(&TempSht, &VGMPnt[0x01], 0x02);
				//DoChipCommand(CurChip, 0x10, TempSht, VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0x68:	// PCM RAM write
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
					DoChipCommand(CurChip, 0x05, 0xFFFE, TempByt);
					break;
				case 0x02:	// RF5C164 PCM Database
					DoChipCommand(CurChip, 0x01, 0xFFFE, TempByt);
					break;
				default:
					break;
				}
				CmdLen = 0x0C;
				break;
			case 0xB2:	// PWM register write
				DoChipCommand(CurChip, 0x11, (VGMPnt[0x01] & 0xF0) >> 4,
											(VGMPnt[0x01] & 0x0F) << 8 |
											(VGMPnt[0x02] << 0));
				CmdLen = 0x03;
				break;
			case 0xA0:	// AY8910 register write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				DoChipCommand(CurChip, 0x12, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xB3:	// GameBoy DMG write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				DoChipCommand(CurChip, 0x13, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xB4:	// NES APU write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				DoChipCommand(CurChip, 0x14, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xB5:	// MultiPCM write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				DoChipCommand(CurChip, 0x15, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xC3:	// MultiPCM memory write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				memcpy(&TempSht, &VGMPnt[0x02], 0x02);
				DoChipCommand(CurChip, 0x15, VGMPnt[0x01] & 0x7F, TempSht);
				CmdLen = 0x04;
				break;
			case 0xB6:	// UPD7759 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				DoChipCommand(CurChip, 0x16, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xB7:	// OKIM6258 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				DoChipCommand(CurChip, 0x17, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xB8:	// OKIM6295 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				DoChipCommand(CurChip, 0x18, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xD2:	// SCC1 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				DoChipCommand(CurChip, 0x19, ((VGMPnt[0x01] & 0x7F) << 8) | VGMPnt[0x02], VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0xD3:	// K054539 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				DoChipCommand(CurChip, 0x1A, ((VGMPnt[0x01] & 0x7F) << 8) | VGMPnt[0x02], VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0xB9:	// HuC6280 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				DoChipCommand(CurChip, 0x1B, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xD4:	// C140 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				DoChipCommand(CurChip, 0x1C, ((VGMPnt[0x01] & 0x7F) << 8) | VGMPnt[0x02], VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0xBA:	// K053260 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				DoChipCommand(CurChip, 0x1D, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xBB:	// Pokey write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				DoChipCommand(CurChip, 0x1E, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xC4:	// Q-Sound write
				DoChipCommand(0x00, 0x1F, VGMPnt[0x03], (VGMPnt[0x01] << 8) | (VGMPnt[0x02] << 0));
				CmdLen = 0x04;
				break;
			case 0x41:	// K007232 write
				CurChip = (VGMPnt[0x01] & 0x80) >> 7;
				DoChipCommand(CurChip, 0x2A, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
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
		VGMPos += CmdLen;
		if (StopVGM)
			break;
	}

	for (TempByt = 0x00; TempByt < 0x02; TempByt ++)
	{
		for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++)
		{
			UINT8 spcChip;
			for (spcChip = 0; spcChip < 2; spcChip ++)
			{
				CHIP_STATE* TempChp = &ChipState[TempByt][spcChip][CurChip];

				if (TempChp->Used || TempChp->CmdCount)
				{
					const char* ChipName = spcChip ? SPCCHIP_STRS[CurChip] : CHIP_STRS[CurChip];
					printf("%s #%u: ", ChipName, TempByt);
					if (! TempChp->Used)
						printf("!! Clock is Zero !!, ");
					print_wordnum("Command", TempChp->CmdCount);
					printf(", ");
					print_wordnum("Note", TempChp->KeyOnCnt);
					printf("\n");
				}
			}
		}
	}

	return;
}

static void print_wordnum(const char* Word, INT32 Number)
{
	if (Number == -1)
		printf("- %ss", Word);
	else if (Number == 1)
		printf("%d %s", Number, Word);
	else
		printf("%d %ss", Number, Word);

	return;
}

static void DoChipCommand(UINT8 ChipSet, UINT8 ChipID, UINT16 Reg, UINT16 Data)
{
	CHIP_STATE* TempChp = &ChipState[ChipSet][0][ChipID];
	UINT8 CurChn;

	TempChp->CmdCount ++;
	if (Reg & 0x8000)
		return;

	switch(ChipID)
	{
	case 0x00:	// SN76496
		if (Reg == 0x00)
		{
			if ((Data & 0x90) == 0x90)
			{
				CurChn = Data >> 5;
				DoKeyOnOff(TempChp, CurChn, ~Data & 0x0F, 0x00);
			}
		}
		break;
	case 0x01:	// YM2413
		if ((Reg & 0x30) == 0x20)
		{
			CurChn = Reg & 0x0F;
			DoKeyOnOff(TempChp, CurChn, Data & 0x10, 0x00);
		}
		else if ((Reg & 0x3F) == 0x0E)
		{
			if (Data & 0x20)
			{
				for (CurChn = 0; CurChn < 5; CurChn ++)
					DoKeyOnOff(TempChp, 9 + CurChn, Data & (1 << CurChn), 0x00);
			}
			else
			{
				for (CurChn = 0; CurChn < 5; CurChn ++)
					DoKeyOnOff(TempChp, 9 + CurChn, false, 0x00);
			}
		}
		break;
	case 0x02:	// YM2612
	case 0x06:	// YM2203
	case 0x07:	// YM2608
	case 0x08:	// YM2610
		// Channels: 6x FM, 6x ADPCM, 1xDeltaT, 6xSSG
		if (Reg == 0x28)
		{
			CurChn = Data & 0x03;
			if (ChipID != 0x06)
				CurChn += ((Data & 0x04) >> 2) * 3;
			DoKeyOnOff(TempChp, CurChn, Data & 0xF0, 0x00);
		}
		else if (ChipID != 0x02)
		{
			if (Reg == 0x07)
			{
				for (CurChn = 0x00; CurChn < 0x06; CurChn ++)
					DoKeyOnOff(TempChp, 13 + CurChn, ~Data & (1 << CurChn), 0x80);
			}
			else if (Reg >= 0x08 && Reg <= 0x0A)
			{
				CurChn = Reg - 0x08;
				DoKeyOnOff(TempChp, 13 + CurChn, Data & 0x0F, 0x81);
			}
		}
		break;
	case 0x03:	// YM2151
		if (Reg == 0x08)
		{
			CurChn = Data & 0x07;
			DoKeyOnOff(TempChp, CurChn, Data & 0x78, 0x00);
		}
		break;
	case 0x04:	// SegaPCM
		if ((Reg & 0x0087) == 0x86)
		{
			CurChn = (Reg >> 3) & 0x0F;
			DoKeyOnOff(TempChp, CurChn, ~Data & 0x01, 0x00);
		}
		break;
	case 0x05:	// RF5C68
	case 0x10:	// RF5C164
		if (Reg == 0x08)
		{
			for (CurChn = 0x00; CurChn < 0x08; CurChn ++)
				DoKeyOnOff(TempChp, CurChn, ~Data & (1 << CurChn), 0x00);
		}
		break;
	case 0x09:	// YM3812
	case 0x0A:	// YM3526
	case 0x0B:	// Y8950
	case 0x0C:	// YMF262
	case 0x0D:	// YMF278B
		if (Reg < 0x200)
		{
			if ((Reg & 0x0F0) == 0x0B0)
			{
				CurChn = ((Reg & 0x100) >> 8) * 9 + (Reg & 0x00F);
				DoKeyOnOff(TempChp, CurChn, Data & 0x20, 0x00);
			}
			else if (Reg == 0x0BD)
			{
				if (Data & 0x20)
				{
					for (CurChn = 0; CurChn < 5; CurChn ++)
						DoKeyOnOff(TempChp, 9 + CurChn, Data & (1 << CurChn), 0x00);
				}
				else
				{
					for (CurChn = 0; CurChn < 5; CurChn ++)
						DoKeyOnOff(TempChp, 9 + CurChn, false, 0x00);
				}
			}
			else if (Reg == 0x07 && ChipID == 0x0B)
			{
				DoKeyOnOff(TempChp, 14, Data & 0x80, 0x00);
			}
		}
		else
		{
			TempChp->CmdCount --;
			TempChp = &ChipState[ChipSet][1][ChipID];
			TempChp->CmdCount ++;
			Reg &= 0xFF;
			if (Reg >= 0x08 && Reg <= 0xF7)
			{
				UINT8 slotReg = (Reg - 8) / 24;
				UINT8 Chn = (Reg - 8) % 24;
				if (slotReg == 4)
					DoKeyOnOff(TempChp, Chn, Data & 0x80, 0x00);
			}
		}
		break;
	case 0x0E:	// YMF271
		if (Reg < 0x400)
		{
			if (((Reg >> 4) & 0x0F) == 0x00)
			{
				DoKeyOnOff(TempChp, 14, Data & 0x01, 0x00);
			}
		}
		break;
	case 0x0F:	// YM280B
		if (Reg < 0x80)
		{
			CurChn = (Reg >> 2) & 0x07;
			if ((Reg & 0xE3) == 0x01)
			{
				DoKeyOnOff(TempChp, CurChn, Data & 0x80, 0x00);
			}
		}
		break;
	case 0x11:	// PWM
		if (! TempChp->KeyState)
		{
			TempChp->KeyState = 0x01;
			TempChp->KeyOnCnt ++;
		}
		break;
	case 0x12:	// AY8910
		if (Reg == 0x07)
		{
			for (CurChn = 0x00; CurChn < 0x06; CurChn ++)
				DoKeyOnOff(TempChp, CurChn, ~Data & (1 << CurChn), 0x80);
		}
		else if (Reg >= 0x08 && Reg <= 0x0A)
		{
			CurChn = Reg - 0x08;
			DoKeyOnOff(TempChp, 13 + CurChn, Data & 0x0F, 0x81);
		}
		break;
	case 0x13:	// GB DMG
		if (Reg < 0x14)
		{
			CurChn = Reg / 5;
			if ((Reg % 5) == 4)
			{
				DoKeyOnOff(TempChp, CurChn, 0x00, 0x00);
				DoKeyOnOff(TempChp, CurChn, Data & 0x80, 0x00);
			}
		}
		break;
	case 0x14:	// NES APU
		if (Reg == 0x15)
		{
			for (CurChn = 0x00; CurChn < 0x05; CurChn ++)
				DoKeyOnOff(TempChp, CurChn, Data & (1 << CurChn), 0x00);
		}
		else if (Reg == 0x23)
		{
			TempChp->CmdCount --;
			TempChp = &ChipState[ChipSet][1][ChipID];
			TempChp->CmdCount ++;
			DoKeyOnOff(TempChp, 0x00, ! (Data & 0x80), 0x00);
		}
		break;
	case 0x15:	// MultiPCM
		TempChp->KeyOnCnt = -1;
		break;
	case 0x16:	// UPD7759
		TempChp->KeyOnCnt = -1;
		break;
	case 0x17:	// OKIM6258
		TempChp->KeyOnCnt = -1;
		break;
	case 0x18:	// OKIM6295
		if (Reg == 0x00)
		{
			if (TempChp->ChnReg != 0xFF)
			{
				for (CurChn = 0x00; CurChn < 0x04; CurChn ++)
				{
					if (Data & (0x10 << CurChn))
						DoKeyOnOff(TempChp, CurChn, 0x01, 0x00);
				}

				TempChp->ChnReg = 0xFF;
			}
			else if (Data & 0x80)
			{
				TempChp->ChnReg = Data & 0x7F;
			}
			else
			{
				for (CurChn = 0x00; CurChn < 0x04; CurChn ++)
				{
					if (Data & (0x08 << CurChn))
						DoKeyOnOff(TempChp, CurChn, 0x00, 0x00);
				}
			}
		}
		break;
	case 0x19:	// K051649
		TempChp->KeyOnCnt = -1;
		break;
	case 0x1A:	// K054539
		TempChp->KeyOnCnt = -1;
		break;
	case 0x1B:	// HuC6280
		if (Reg == 0x00)
		{
			TempChp->ChnReg = Data & 0x07;
		}
		else if (Reg == 0x05)
		{
			DoKeyOnOff(TempChp, TempChp->ChnReg, Data & 0x80, 0x00);
		}
		break;
	case 0x1C:	// C140
		if (Reg < 0x180)
		{
			CurChn = Reg >> 4;
			if ((Reg & 0x00F) == 0x005)
				DoKeyOnOff(TempChp, CurChn, Data & 0x80, 0x00);
		}
		break;
	case 0x1D:	// K053260
		if (Reg == 0x28)
		{
			for (CurChn = 0x00; CurChn < 0x04; CurChn ++)
				DoKeyOnOff(TempChp, CurChn, Data & (1 << CurChn), 0x00);
		}
		break;
	case 0x1E:	// Pokey
		if (Reg < 0x08 && (Reg & 0x01))
		{
			CurChn = Reg >> 1;
			DoKeyOnOff(TempChp, CurChn, ~Data & 0x80, 0x00);
		}
		break;
	case 0x1F:	// QSound
		if (Reg < 0x80)
		{
			CurChn = Reg >> 3;
			if ((Reg & 0x07) == 0x02)
			{
				if (! Data)
					DoKeyOnOff(TempChp, CurChn, false, 0x00);
			}
			else if ((Reg & 0x07) == 0x06)
			{
				DoKeyOnOff(TempChp, CurChn, Data > 0, 0x00);
			}
		}
		break;
		case 0x2A:	// K007232
			if (Reg == 0x05 || Reg == 0x0B)
			{
				UINT8 ch = (Reg == 0x05) ? 0 : 1;
				DoKeyOnOff(TempChp, ch, 1, 0x00);
			}
			break;
	}

	return;
}

static void DoKeyOnOff(CHIP_STATE* CState, UINT8 Chn, UINT8 OnOff, UINT8 VolFlag)
{
	UINT32 KeyMask;

	KeyMask = 0x01 << Chn;
	if (VolFlag & 0x01)
	{
		if (OnOff)
		{
			if (! (CState->VolState & KeyMask))
			{
				CState->VolState |= KeyMask;
				if (! (VolFlag & 0x80) | (CState->VolState & KeyMask))
					CState->KeyOnCnt ++;
			}
		}
		else
		{
			CState->VolState &= ~KeyMask;
		}
	}
	else
	{
		if (OnOff)
		{
			if (! (CState->VolState & KeyMask))
			{
				CState->VolState |= KeyMask;
				if (! (VolFlag & 0x80) | (CState->KeyState & KeyMask))
					CState->KeyOnCnt ++;
			}
		}
		else
		{
			CState->VolState &= ~KeyMask;
		}
	}

	return;
}
