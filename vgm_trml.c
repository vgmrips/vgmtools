// vgm_trml.c - VGM Trimming Library
//
//TODO: display warning/question if notes are playing at the end of a jingle

#include <stdio.h>
#include <stdlib.h>
#include "stdbool.h"
#include <memory.h>

//#define SHOW_PROGRESS

#ifdef SHOW_PROGRESS

#ifdef WIN32
#include <windows.h>	// for GetTickCount
#else
#undef SHOW_PROGRESS
#endif

#endif

#include "zlib.h"

#include "stdtype.h"
#include "VGMFile.h"
#include "vgm_lib.h"


#define CHIP_SETS	0x02

// Rewritten Registers:
//	- PSG: NoiseMode (Reg 0xE?) / GG Stereo
//	- YM2612: LFO Frequency (Reg 0x22) / Ch 3 Mode (Reg 0x27) / DAC Enable (Reg 0x02B)
//	- YM2151: LFO Frequency (0x18) / LFO Amplitude/Phase Modul (Reg 0x19) / LFO Wave Select (Reg 0x1B)
//	- RF5C68: Memory Bank Register (0x07) [handled in RF5Cxx Rewrite Routine]
//FarTodo: RF5Cxx Channel Address Registers
//	- YM2203: Ch 3 Mode (Reg 0x27)
//	- YM2608: LFO Frequency (Reg 0x22) / Ch 3 Mode (Reg 0x27) / ADPCM Volume (Reg 0x011) / Delta-T Volume (Reg 0x10B)
//	- YM2610: LFO Frequency (Reg 0x22) / Ch 3 Mode (Reg 0x27) / ADPCM Volume (Reg 0x101) / Delta-T Volume (Reg 0x01B)
//	- YM3812/YM3526/Y8950: Wave Select (Reg 0x01) / CSM/KeySplit (Reg 0x08) / Rhythm/AM/FM (Reg 0xBD)
//	- YMF262/YMF278B: Wave Select (Reg 0x001) / CSM/KeySplit (Reg 0x008) / Rhythm/AM/FM (Reg 0x0BD) / OPL3/4 Mode Enable (Reg 0x105) / 4-Ch-Mode (Reg 0x104)
//	- Y8950: Delta-T Volume (Reg 0x12)
//	- YMZ280B: Key On Enable (Reg 0xFF)
//	- YMF271: Group Registers (Reg 0x600 - 0x60F)
//	- RF5C164: Memory Bank Register (0x07) [handled in RF5Cxx Rewrite Routine]
//	- GB DMG: Control Regs (0x14 - 0x16), WaveRAM (0x20 - 0x2F)

typedef struct chip_state_data
{
	UINT8 Mode;	// Register Size: 00 - 8 bit, 01 - 16 bit
	UINT16 RegCount;
	union
	{
		UINT8* R08;
		UINT16* R16;
	} RegData;
	UINT8* RegMask;
} CHIP_STATE;
typedef struct chip_state_memory
{
	UINT32 MemSize;
	UINT8* MemData;
	bool HadWrt;
} CHIP_MEMORY;
typedef struct chip_state_channels
{
	UINT32 ChnCount;
	UINT32 ChnMask;
	UINT32 ChnMask2;
} CHIP_CHNS;
typedef struct chip_data
{
	CHIP_STATE Regs;
	CHIP_MEMORY Mem;
	CHIP_CHNS Chns;
} CHIP_DATA;

#define RF_RAM_SIZE	0x10000	// 64 KB

typedef struct rewrite_chipset_new
{
	CHIP_DATA SN76496;
	CHIP_DATA YM2413;
	CHIP_DATA YM2612;
	CHIP_DATA YM2151;
	CHIP_DATA SegaPCM;
	CHIP_DATA RF5C68;
	CHIP_DATA YM2203;
	CHIP_DATA YM2608;
	CHIP_DATA YM2610;
	CHIP_DATA YM3812;
	CHIP_DATA YM3526;
	CHIP_DATA Y8950;
	CHIP_DATA YMF262;
	CHIP_DATA YMF278B;
	CHIP_DATA YMZ280B;
	CHIP_DATA YMF271;
	CHIP_DATA RF5C164;
	CHIP_DATA PWM;
	CHIP_DATA AY8910;
	CHIP_DATA GBDMG;
	CHIP_DATA NESAPU;
	CHIP_DATA MultiPCM;
	CHIP_DATA UPD7759;
	CHIP_DATA OKIM6258;
	CHIP_DATA OKIM6295;
	CHIP_DATA K051649;
	CHIP_DATA K054539;
	CHIP_DATA HuC6280;
	CHIP_DATA C140;
	CHIP_DATA K053260;
	CHIP_DATA Pokey;
	CHIP_DATA QSound;
} REWRT_CHIPSET_NEW;

#define CHIP_COUNT	0x20


void SetTrimOptions(bool StateSave);
static void PrepareChipMemory(void);
static void SetImportantCommands(void);
static void InitializeVGM(UINT8* DstData, UINT32* DstPos);
static UINT32 ReadCommand(UINT8 Mask);
static void CommandCheck(UINT8 Mode, UINT8 Command, CHIP_DATA* ChpData, UINT16 CmdReg);
void TrimVGMData(const INT32 StartSmpl, const INT32 LoopSmpl, const INT32 EndSmpl,
				 const bool HasLoop, const bool KeepESmpl);
static void WriteVGMHeader(UINT8* DstData, const UINT8* SrcData, const UINT32 EOFPos,
						   const UINT32 SampleCount, const UINT32 LoopPos,
						   const UINT32 LoopSmpls);
#ifdef SHOW_PROGRESS
static void PrintMinSec(const UINT32 SamplePos, char* TempStr);
#endif
static void VGMReadAhead(const UINT32 StartPos, const UINT32 Samples);
static void DisplayPlayingNoteWarning(void);


// Options:
//	- Complete Rewrite: writes some sort of chip save state at the beginning of the VGM
//	- VGMTool Trim: writes all commands and simply strips the delays (deprecated, can sort of break things with OPN chips)
//	- Intelligent: writes all necessary commands to restore the chip state (e.g. unused channels will be left out)
static bool COMPLETE_REWRITE = false;
static bool VGMTOOL_TRIM = false;
static bool INTELLIGENT = false;

extern VGM_HEADER VGMHead;
extern UINT32 VGMDataLen;
extern UINT8* VGMData;
static UINT32 VGMPos;
static INT32 VGMSmplPos;
extern UINT8* DstData;
extern UINT32 DstDataLen;

REWRT_CHIPSET_NEW RC[CHIP_SETS];

void SetTrimOptions(bool StateSave)
{
#ifdef _DEBUG
	if (CHIP_COUNT * sizeof(CHIP_DATA) != sizeof(REWRT_CHIPSET_NEW))
	{
		printf("Fatal Error! ChipSet structure invalid!\n");
		getchar();
		exit(-1);
	}
#endif
	COMPLETE_REWRITE = StateSave;
	
	return;
}

static void PrepareChipMemory(void)
{
	REWRT_CHIPSET_NEW* TempRC;
	UINT8 CurCSet;
	
	for (CurCSet = 0x00; CurCSet < CHIP_SETS; CurCSet ++)
	{
		TempRC = &RC[CurCSet];
		memset(TempRC, 0x00, sizeof(REWRT_CHIPSET_NEW));
		
		if (VGMHead.lngHzPSG)
		{
			// SN76496 Register Map:
			// 00-07 Data-only commands, 08-0F - ChnSel+Data, 10 - GG Stereo, 11 - LastCmd
			if (! CurCSet || (VGMHead.lngHzPSG & 0x40000000))
			{
				// 4 Chn * Freq+Vol * 2-Byte Data + GG Stereo + LastCmd
				TempRC->SN76496.Regs.RegCount = 0x12;
				TempRC->SN76496.Chns.ChnCount = 0x04;
			}
		}
		if (VGMHead.lngHzYM2413)
		{
			if (! CurCSet || (VGMHead.lngHzYM2413 & 0x40000000))
			{
				TempRC->YM2413.Regs.RegCount = 0x40;
				TempRC->YM2413.Chns.ChnCount = 0x09;
			}
		}
		if (VGMHead.lngHzYM2612)
		{
			if (! CurCSet || (VGMHead.lngHzYM2612 & 0x40000000))
			{
				TempRC->YM2612.Regs.RegCount = 0x200;
				TempRC->YM2612.Chns.ChnCount = 0x06;
			}
		}
		if (VGMHead.lngHzYM2151)
		{
			// YM2151 Register Map Notes:
			//	0x19 - AMD (Reg 0x19, bit 7 = 0)
			//	0x1A - PMD (Reg 0x19, bit 7 = 1)
			if (! CurCSet || (VGMHead.lngHzYM2151 & 0x40000000))
			{
				TempRC->YM2151.Regs.RegCount = 0x100;
				TempRC->YM2151.Chns.ChnCount = 0x08;
			}
		}
		if (VGMHead.lngHzSPCM)
		{
			// "Register" Memory
			if (! CurCSet || (VGMHead.lngHzSPCM & 0x40000000))
			{
				TempRC->SegaPCM.Regs.RegCount = 0x100;
				TempRC->SegaPCM.Chns.ChnCount = 0x08;
			}
		}
		if (VGMHead.lngHzRF5C68)
		{
			// RF5C68 Register Map:
			// 00-06, 07-0D, .. - Channel Regs
			// 38 - Memory Bank
			// 39 - Channel Bank
			// 3A - Channel Mask
			if (! CurCSet || (VGMHead.lngHzRF5C68 & 0x40000000))
			{
				// 8 Chn * 0x07 Regs + MBank + CBank + ChnMask
				TempRC->RF5C68.Regs.RegCount = 0x3B;
				TempRC->RF5C68.Mem.MemSize = RF_RAM_SIZE;
				TempRC->RF5C68.Chns.ChnCount = 0x08;
			}
		}
		if (VGMHead.lngHzYM2203)
		{
			if (! CurCSet || (VGMHead.lngHzYM2203 & 0x40000000))
			{
				TempRC->YM2203.Regs.RegCount = 0x100;
				TempRC->YM2203.Chns.ChnCount = 0x03;
			}
		}
		if (VGMHead.lngHzYM2608)
		{
			if (! CurCSet || (VGMHead.lngHzYM2608 & 0x40000000))
			{
				TempRC->YM2608.Regs.RegCount = 0x200;
				TempRC->YM2608.Chns.ChnCount = 0x06 + 0x06 + 0x01;
			}
		}
		if (VGMHead.lngHzYM2610)
		{
			if (! CurCSet || (VGMHead.lngHzYM2610 & 0x40000000))
			{
				TempRC->YM2610.Regs.RegCount = 0x200;
				TempRC->YM2610.Chns.ChnCount = 0x06 + 0x06 + 0x01;
			}
		}
		if (VGMHead.lngHzYM3812)
		{
			if (! CurCSet || (VGMHead.lngHzYM3812 & 0x40000000))
			{
				TempRC->YM3812.Regs.RegCount = 0x100;
				TempRC->YM3812.Chns.ChnCount = 0x09 + 0x05;
			}
		}
		if (VGMHead.lngHzYM3526)
		{
			if (! CurCSet || (VGMHead.lngHzYM3526 & 0x40000000))
			{
				TempRC->YM3526.Regs.RegCount = 0x100;
				TempRC->YM3526.Chns.ChnCount = 0x09 + 0x05;
			}
		}
		if (VGMHead.lngHzY8950)
		{
			if (! CurCSet || (VGMHead.lngHzY8950 & 0x40000000))
			{
				TempRC->Y8950.Regs.RegCount = 0x100;
				TempRC->Y8950.Chns.ChnCount = 0x09 + 0x05 + 0x01;
			}
		}
		if (VGMHead.lngHzYMF262)
		{
			if (! CurCSet || (VGMHead.lngHzYMF262 & 0x40000000))
			{
				TempRC->YMF262.Regs.RegCount = 0x200;
				TempRC->YMF262.Chns.ChnCount = 0x12 + 0x05;
			}
		}
		if (VGMHead.lngHzYMF278B)
		{
			if (! CurCSet || (VGMHead.lngHzYMF278B & 0x40000000))
			{
				TempRC->YMF278B.Regs.RegCount = 0x300;
				TempRC->YMF278B.Chns.ChnCount = /*0x12 + 0x05 + */0x18;
			}
		}
		if (VGMHead.lngHzYMZ280B)
		{
			if (! CurCSet || (VGMHead.lngHzYMZ280B & 0x40000000))
			{
				TempRC->YMZ280B.Regs.RegCount = 0x100;
				TempRC->YMZ280B.Chns.ChnCount = 0x08;
			}
		}
		if (VGMHead.lngHzYMF271)
		{
			if (! CurCSet || (VGMHead.lngHzYMF271 & 0x40000000))
			{
				TempRC->YMF271.Regs.RegCount = 0x800;
				TempRC->YMF271.Chns.ChnCount = 0x0C;
			}
		}
		if (VGMHead.lngHzRF5C164)
		{
			// RF5C164 Register Map see RF5C68
			if (! CurCSet || (VGMHead.lngHzRF5C164 & 0x40000000))
			{
				// CBank + MBank + ChnMask + 8 Chn * 0x07 Regs
				TempRC->RF5C164.Regs.RegCount = 0x3B;
				TempRC->RF5C164.Mem.MemSize = RF_RAM_SIZE;
				TempRC->RF5C164.Chns.ChnCount = 0x08;
			}
		}
		if (VGMHead.lngHzPWM)
		{
			if (! CurCSet || (VGMHead.lngHzPWM & 0x40000000))
			{
				TempRC->PWM.Regs.Mode = 0x01;		// actually 12 bit, not 16
				TempRC->PWM.Regs.RegCount = 0x05;
				TempRC->PWM.Chns.ChnCount = 0x00;	// it's just a stream of PCM data
			}
		}
		if (VGMHead.lngHzAY8910)
		{
			if (! CurCSet || (VGMHead.lngHzAY8910 & 0x40000000))
			{
				TempRC->AY8910.Regs.RegCount = 0x10;
				TempRC->AY8910.Chns.ChnCount = 0x03;
			}
		}
		if (VGMHead.lngHzGBDMG)
		{
			if (! CurCSet || (VGMHead.lngHzGBDMG & 0x40000000))
			{
				TempRC->GBDMG.Regs.RegCount = 0x30;
				TempRC->GBDMG.Chns.ChnCount = 0x04;
			}
		}
		if (VGMHead.lngHzNESAPU)
		{
			if (! CurCSet || (VGMHead.lngHzNESAPU & 0x40000000))
			{
				TempRC->NESAPU.Regs.RegCount = 0x18;
				TempRC->NESAPU.Chns.ChnCount = 0x05;
			}
		}
		if (VGMHead.lngHzMultiPCM)
		{
			if (! CurCSet || (VGMHead.lngHzMultiPCM & 0x40000000))
			{
				// 28 Chn * 8 Reg + CurSlot + CurAddr
				TempRC->MultiPCM.Regs.RegCount = 0xE2;
				TempRC->MultiPCM.Chns.ChnCount = 0x1C;
			}
		}
		if (VGMHead.lngHzUPD7759)
		{
			if (! CurCSet || (VGMHead.lngHzUPD7759 & 0x40000000))
			{
				TempRC->UPD7759.Regs.RegCount = 0x04;
				TempRC->UPD7759.Chns.ChnCount = 0x01;
			}
		}
		if (VGMHead.lngHzOKIM6258)
		{
			if (! CurCSet || (VGMHead.lngHzOKIM6258 & 0x40000000))
			{
				// One reg for every port (Ctrl, Data, Pan, Clock-stuff)
				TempRC->OKIM6258.Regs.RegCount = 0x10;
				TempRC->OKIM6258.Chns.ChnCount = 0x01;
			}
		}
		if (VGMHead.lngHzOKIM6295)
		{
			if (! CurCSet || (VGMHead.lngHzOKIM6295 & 0x40000000))
			{
				TempRC->OKIM6295.Regs.RegCount = 0x05;
				TempRC->OKIM6295.Chns.ChnCount = 0x04;
			}
		}
		if (VGMHead.lngHzK051649)
		{
			if (! CurCSet || (VGMHead.lngHzK051649 & 0x40000000))
			{
				// 5 Chn * (2 Regs + 0x20 Waveform) + CurOfs + ChnMask
				TempRC->K051649.Regs.RegCount = 0xA6;
				TempRC->K051649.Chns.ChnCount = 0x05;
			}
		}
		if (VGMHead.lngHzK054539)
		{
			if (! CurCSet || (VGMHead.lngHzK054539 & 0x40000000))
			{
				TempRC->K054539.Regs.RegCount = 0x230;
				TempRC->K054539.Chns.ChnCount = 0x08;
			}
		}
		if (VGMHead.lngHzHuC6280)
		{
			if (! CurCSet || (VGMHead.lngHzHuC6280 & 0x40000000))
			{
				// 4 global Regs + 6 Chn * (6 Regs + 0x20 Waveform) = 0xE8
				TempRC->HuC6280.Regs.RegCount = 0xE8;
				TempRC->HuC6280.Chns.ChnCount = 0x06;
			}
		}
		if (VGMHead.lngHzC140)
		{
			if (! CurCSet || (VGMHead.lngHzC140 & 0x40000000))
			{
				TempRC->C140.Regs.RegCount = 0x200;
				TempRC->C140.Chns.ChnCount = 0x18;
			}
		}
		if (VGMHead.lngHzK053260)
		{
			if (! CurCSet || (VGMHead.lngHzK053260 & 0x40000000))
			{
				TempRC->K053260.Regs.RegCount = 0x30;
				TempRC->K053260.Chns.ChnCount = 0x04;
			}
		}
		if (VGMHead.lngHzPokey)
		{
			if (! CurCSet || (VGMHead.lngHzPokey & 0x40000000))
			{
				TempRC->Pokey.Regs.RegCount = 0x10;
				TempRC->Pokey.Chns.ChnCount = 0x04;
			}
		}
		if (VGMHead.lngHzQSound)
		{
			if (! CurCSet || (VGMHead.lngHzQSound & 0x40000000))
			{
				TempRC->QSound.Regs.Mode = 0x01;
				TempRC->QSound.Regs.RegCount = 0x100;
				TempRC->QSound.Chns.ChnCount = 0x10;
			}
		}
	}
	
	return;
}

static void SetImportantCommands(void)
{
	CHIP_DATA* TempCD;
	CHIP_STATE* TempReg;
	CHIP_MEMORY* TempMem;
	UINT8 CurCSet;
	UINT8 CurChip;
	UINT8 CurReg;
	
	for (CurCSet = 0x00; CurCSet < CHIP_SETS; CurCSet ++)
	{
		TempCD = (CHIP_DATA*)&RC[CurCSet];
		
		for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++, TempCD ++)
		{
			TempReg = &TempCD->Regs;
			TempMem = &TempCD->Mem;
			
			if (TempReg->RegCount)
			{
				if (! TempReg->Mode)
				{
					TempReg->RegData.R08 = (UINT8*)malloc(TempReg->RegCount * 0x01);
					memset(TempReg->RegData.R08, 0x00, TempReg->RegCount * 0x01);
				}
				else
				{
					TempReg->RegData.R16 = (UINT16*)malloc(TempReg->RegCount * 0x02);
					memset(TempReg->RegData.R16, 0x00, TempReg->RegCount * 0x02);
				}
				TempReg->RegMask = (UINT8*)malloc(TempReg->RegCount);
				memset(TempReg->RegMask, 0x00, TempReg->RegCount);
			}
			else
			{
				TempReg->RegData.R08 = NULL;
				TempReg->RegMask = NULL;
			}
			if (TempMem->MemSize)
			{
				TempMem->MemData = (UINT8*)malloc(TempMem->MemSize);
				memset(TempMem->MemData, 0x00, TempMem->MemSize);
			}
			else
			{
				TempMem->MemData = NULL;
			}
			TempMem->HadWrt = false;
			
			if (! TempReg->RegCount)
				continue;
			
			switch(CurChip)
			{
			case 0x00:	// SN76496
				TempReg->RegMask[0x0E] |= 0x80;		// Noise Mode
				TempReg->RegMask[0x10] |= 0x80;		// GG Stereo
				break;
			case 0x01:	// YM2413
				TempReg->RegMask[0x0E] |= 0x80;		// Rhythm Register
				break;
			case 0x02:	// YM2612
				TempReg->RegMask[0x022] |= 0x80;	// LFO Frequency
				TempReg->RegMask[0x027] |= 0x80;	// Ch 3 Special Mode
				TempReg->RegMask[0x02B] |= 0x80;	// DAC Enable
				break;
			case 0x03:	// YM2151
				TempReg->RegMask[0x01] |= 0x80;		// LFO On/Off
				TempReg->RegMask[0x0F] |= 0x80;		// Noise Enable/Frequency
				TempReg->RegMask[0x14] |= 0x80;		// CSM Select
				TempReg->RegMask[0x18] |= 0x80;		// LFO Frequency
				TempReg->RegMask[0x19] |= 0x80;		// LFO Amplitude Modul.
				TempReg->RegMask[0x1A] |= 0x80;		// LFO Phase Modul.
				TempReg->RegMask[0x1B] |= 0x80;		// LFO Wave Select
				break;
			case 0x04:	// Sega PCM
				break;
			case 0x05:	// RF5C68
				memset(TempMem->MemData, 0xFF, TempMem->MemSize);
				break;
			case 0x06:	// YM2203
				TempReg->RegMask[0x27] |= 0x80;		// Ch 3 Special Mode
				TempReg->RegMask[0x2F] |= 0x80;		// Prescaler
				break;
			case 0x07:	// YM2608
				TempReg->RegMask[0x022] |= 0x80;	// LFO Frequency
				TempReg->RegMask[0x027] |= 0x80;	// Ch 3 Special Mode
				TempReg->RegMask[0x029] |= 0x80;	// 3/6-Ch-Mode
				TempReg->RegMask[0x02F] |= 0x80;	// Prescaler
				TempReg->RegMask[0x011] |= 0x80;	// ADPCM Volume
				TempReg->RegMask[0x10B] |= 0x80;	// Delta-T Volume
				break;
			case 0x08:	// YM2610
				TempReg->RegMask[0x022] |= 0x80;	// LFO Frequency
				TempReg->RegMask[0x027] |= 0x80;	// Ch 3 Special Mode
				TempReg->RegMask[0x01B] |= 0x80;	// Delta-T Volume
				TempReg->RegMask[0x101] |= 0x80;	// ADPCM Volume
				break;
			case 0x09:	// YM3812
				TempReg->RegMask[0x01] |= 0x80;		// Wave Select
				TempReg->RegMask[0x08] |= 0x80;		// CSM / KeySplit
				TempReg->RegMask[0xBD] |= 0x80;		// Rhythm / AM / FM
				break;
			case 0x0A:	// YM3526
				TempReg->RegMask[0x01] |= 0x80;		// Wave Select
				TempReg->RegMask[0x08] |= 0x80;		// CSM / KeySplit
				TempReg->RegMask[0xBD] |= 0x80;		// Rhythm / AM / FM
				break;
			case 0x0B:	// Y8950
				TempReg->RegMask[0x01] |= 0x80;		// Wave Select
				TempReg->RegMask[0x08] |= 0x80;		// CSM / KeySplit
				TempReg->RegMask[0x12] |= 0x80;		// Delta-T Volume
				TempReg->RegMask[0xBD] |= 0x80;		// Rhythm / AM / FM
				break;
			case 0x0C:	// YMF262
				TempReg->RegMask[0x001] |= 0x80;	// Wave Select
				TempReg->RegMask[0x008] |= 0x80;	// CSM / KeySplit
				TempReg->RegMask[0x0BD] |= 0x80;	// Rhythm / AM / FM
				TempReg->RegMask[0x104] |= 0x80;	// 4-Ch-Mode
				TempReg->RegMask[0x105] |= 0x80;	// OPL3 Mode Enable
				break;
			case 0x0D:	// YMF278B
				TempReg->RegMask[0x001] |= 0x80;	// Wave Select
				TempReg->RegMask[0x008] |= 0x80;	// CSM / KeySplit
				TempReg->RegMask[0x0BD] |= 0x80;	// Rhythm / AM / FM
				TempReg->RegMask[0x104] |= 0x80;	// 4-Ch-Mode
				TempReg->RegMask[0x105] |= 0x80;	// OPL3/4 Mode Enable
				break;
			case 0x0E:	// YMZ280B
				TempReg->RegMask[0x80] |= 0x80;		// DSP L/R Enable
				TempReg->RegMask[0x81] |= 0x80;		// DSP Enable 0x82
				TempReg->RegMask[0x82] |= 0x80;		// DSP Data
				TempReg->RegMask[0xFF] |= 0x80;		// Key On Enable
				break;
			case 0x0F:	// YMF271
				// Group Registers
				for (CurReg = 0x00; CurReg < 0x10; CurReg ++)
					TempReg->RegMask[0x600 | CurReg] |= 0x80;
				break;
			case 0x10:	// RF5C164
				memset(TempMem->MemData, 0xFF, TempMem->MemSize);
				break;
			case 0x11:	// PWM
				TempReg->RegMask[0x00] |= 0x80;	// Control Register
				TempReg->RegMask[0x01] |= 0x80;	// Cycle Register
				break;
			case 0x12:	// AY8910
				break;
			case 0x13:	// GB DMG
				// Control Regs
				for (CurReg = 0x00; CurReg < 0x03; CurReg ++)
					TempReg->RegMask[0x14 + CurReg] |= 0x80;
				
				// WaveRAM
				for (CurReg = 0x00; CurReg < 0x10; CurReg ++)
					TempReg->RegMask[0x20 | CurReg] |= 0x80;
				break;
			case 0x14:	// NES APU
				break;
			case 0x15:	// MultiPCM
				break;
			case 0x16:	// UPD7759
				break;
			case 0x17:	// OKIM6258
				// not needed
				TempReg->RegData.R08[0x08] = (VGMHead.lngHzOKIM6258 >>  0) & 0xFF;
				TempReg->RegData.R08[0x09] = (VGMHead.lngHzOKIM6258 >>  8) & 0xFF;
				TempReg->RegData.R08[0x0A] = (VGMHead.lngHzOKIM6258 >> 16) & 0xFF;
				TempReg->RegData.R08[0x0B] = (VGMHead.lngHzOKIM6258 >> 24) & 0xFF;
				TempReg->RegData.R08[0x0C] = (VGMHead.bytOKI6258Flags >> 0) & 0x03;
				break;
			case 0x18:	// OKIM6295
				break;
			case 0x19:	// K051649
				break;
			case 0x1A:	// K054539
				TempReg->RegMask[0x22E] |= 0x80;
				TempReg->RegMask[0x22F] |= 0x80;	// Thanks to 2ch-H for reporting this.
				break;
			case 0x1B:	// HuC6280
				// Control Regs (Balance, LFO)
				for (CurReg = 0xE4; CurReg < 0xE8; CurReg ++)
					TempReg->RegMask[CurReg] |= 0x80;
				
				// Wave RAM
				for (CurReg = 0x00; CurReg < 0xC0; CurReg ++)
					TempReg->RegMask[CurReg] |= 0x80;
				break;
			case 0x1C:	// C140
				break;
			case 0x1D:	// K053260
				break;
			case 0x1E:	// Pokey
				break;
			}
		}
	}
	
	return;
}

static void InitializeVGM(UINT8* DstData, UINT32* DstPosRef)
{
	UINT32 DstPos = *DstPosRef;
	UINT8 CurCSet;
	UINT8 CurChip;
	UINT16 CurReg;
	UINT16 WrtReg;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	CHIP_DATA* TempCD;
	CHIP_STATE* TempReg;
	CHIP_MEMORY* TempMem;
	UINT8 ChipCmd;
	UINT8 CmdType;
	
	if (VGMTOOL_TRIM)
		return;
	
	for (CurCSet = 0x00; CurCSet < CHIP_SETS; CurCSet ++)
	{
		TempCD = (CHIP_DATA*)&RC[CurCSet];
		
		for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++, TempCD ++)
		{
			TempMem = &TempCD->Mem;
			
			if (! TempMem->MemSize || ! TempMem->HadWrt)
				continue;
			
			switch(CurChip)
			{
			case 0x05:	// RF5C68
			case 0x10:	// RF5C164
				if (CurChip == 0x05)
					TempByt = 0x00;
				else if (CurChip == 0x10)
					TempByt = 0x01;
				
				DstData[DstPos + 0x00] = 0x67;
				DstData[DstPos + 0x01] = 0x66;
				DstData[DstPos + 0x02] = 0xC0 + TempByt;
				
				TempLng = TempMem->MemSize + 0x02;
				memcpy(&DstData[DstPos + 0x03], &TempLng, 0x04);
				
				TempSht = 0x0000;
				memcpy(&DstData[DstPos + 0x07], &TempSht, 0x02);
				memcpy(&DstData[DstPos + 0x09], TempMem->MemData, TempMem->MemSize);
				DstPos += 0x07 + TempLng;
				
				DstData[DstPos + 0x00] = 0xB0 + TempByt;
				DstData[DstPos + 0x01] = 0x07;
				DstData[DstPos + 0x02] = 0x00 | TempCD->Regs.RegData.R08[0x38];
				DstPos += 0x03;
				break;
			}
		}
	}
	
	for (CurCSet = 0x00; CurCSet < CHIP_SETS; CurCSet ++)
	{
		TempCD = (CHIP_DATA*)&RC[CurCSet];
		
		for (CurChip = 0x00; CurChip < CHIP_COUNT; CurChip ++, TempCD ++)
		{
			TempReg = &TempCD->Regs;
			
			if (! TempReg->RegCount)
				continue;
			
			for (CurReg = 0x00; CurReg < TempReg->RegCount; CurReg ++)
			{
				if ((TempReg->RegMask[CurReg] & 0x80) || COMPLETE_REWRITE)
					TempReg->RegMask[CurReg] &= 0x7F;
				else
					TempReg->RegMask[CurReg] = 0x00;
			}
			
			// CmdType
			//		10 - 1 Port
			//		11 - 2 Ports, Port is ORed with Command Byte
			//		12 - 1 Port but few Registers, Register Byte is ORed with 2nd Chip Flag
			//		20 - 2+ Ports, Command and Port are seperate bytes
			//		21 - 16-bit Memory Offset
			//		8x - like 1x, but with special workarounds
			switch(CurChip)
			{
			case 0x00:	// SN76496
				CurReg = 0x10;	// GG Stereo
				if ((TempReg->RegMask[CurReg] & 0x7F) == 0x01)
				{
					DstData[DstPos + 0x00] = 0x4F - CurCSet * 0x10;
					DstData[DstPos + 0x01] = TempReg->RegData.R08[CurReg];
					DstPos += 0x02;
				}
				
				ChipCmd = 0x50 - CurCSet * 0x20;
				for (CurReg = 0x08; CurReg < 0x10; CurReg ++)
				{
					if ((TempReg->RegMask[CurReg] & 0x7F) == 0x01)
					{
						DstData[DstPos + 0x00] = ChipCmd;
						DstData[DstPos + 0x01] = TempReg->RegData.R08[CurReg];
						DstPos += 0x02;
						
						TempByt = CurReg & 0x07;
						if (CurReg < 0x0E && ! (CurReg & 0x01) &&
							TempReg->RegMask[TempByt])
						{
							// Write Data Byte
							DstData[DstPos + 0x00] = ChipCmd;
							DstData[DstPos + 0x01] = TempReg->RegData.R08[TempByt];
							DstPos += 0x02;
						}
					}
				}
				
				CmdType = 0x00;
				break;
			case 0x01:	// YM2413
				ChipCmd = 0x51 + CurCSet * 0x50;
				CmdType = 0x10;
				break;
			case 0x02:	// YM2612
				ChipCmd = 0x52 + CurCSet * 0x50;
				CmdType = 0x81;
				break;
			case 0x03:	// YM2151
				ChipCmd = 0x54 + CurCSet * 0x50;
				CmdType = 0x80;
				break;
			case 0x04:	// Sega PCM
				ChipCmd = 0xC0;
				CmdType = 0x21;
				break;
			case 0x05:	// RF5C68
			case 0x10:	// RF5C164
				if (CurChip == 0x05)
					ChipCmd = 0xB0;
				else if (CurChip == 0x10)
					ChipCmd = 0xB1;
				
				TempByt = 0xFF;
				for (CurReg = 0x00; CurReg < TempReg->RegCount; CurReg ++)
				{
					if (CurReg / 0x07 >= 8)
						break;
					
					if ((TempReg->RegMask[CurReg] & 0x7F) == 0x01)
					{
						if (TempByt != (CurReg / 0x07))
						{
							TempByt = CurReg / 0x07;
							DstData[DstPos + 0x00] = ChipCmd;
							DstData[DstPos + 0x01] = (CurCSet << 7) | 0x07;
							DstData[DstPos + 0x02] = 0xC0 | TempByt;
							DstPos += 0x03;
						}
						
						DstData[DstPos + 0x00] = ChipCmd;
						DstData[DstPos + 0x01] = (CurCSet << 7) | (CurReg % 0x07);
						DstData[DstPos + 0x02] = TempReg->RegData.R08[CurReg];
						DstPos += 0x03;
					}
				}
				if (TempByt == (TempReg->RegData.R08[0x39] & 0x07))
					TempReg->RegMask[CurReg] = 0x00;
				for (; CurReg < TempReg->RegCount; CurReg ++)
				{
					if ((TempReg->RegMask[CurReg] & 0x7F) == 0x01)
					{
						if (CurReg < 0x3A)
							TempByt = 0x07;
						else
							TempByt = 0x08;
						DstData[DstPos + 0x00] = ChipCmd;
						DstData[DstPos + 0x01] = (CurCSet << 7) | CurReg;
						DstData[DstPos + 0x02] = TempReg->RegData.R08[CurReg];
						DstPos += 0x03;
					}
				}
				
				CmdType = 0x00;
				break;
			case 0x06:	// YM2203
				ChipCmd = 0x55 + CurCSet * 0x50;
				CmdType = 0x80;
				break;
			case 0x07:	// YM2608
				ChipCmd = 0x56 + CurCSet * 0x50;
				CmdType = 0x81;
				break;
			case 0x08:	// YM2610
				ChipCmd = 0x58 + CurCSet * 0x50;
				CmdType = 0x81;
				break;
			case 0x09:	// YM3812
				ChipCmd = 0x5A + CurCSet * 0x50;
				CmdType = 0x10;
				break;
			case 0x0A:	// YM3526
				ChipCmd = 0x5B + CurCSet * 0x50;
				CmdType = 0x10;
				break;
			case 0x0B:	// Y8950
				ChipCmd = 0x5C + CurCSet * 0x50;
				CmdType = 0x10;
				break;
			case 0x0C:	// YMF262
				ChipCmd = 0x5E + CurCSet * 0x50;
				CmdType = 0x11;
				
				CurReg = 0x105;	// OPL3 Enabe
				if ((TempReg->RegMask[CurReg] & 0x7F) == 0x01)
				{
					DstData[DstPos + 0x00] = ChipCmd | (CurReg >> 8);
					DstData[DstPos + 0x01] = CurReg & 0xFF;
					DstData[DstPos + 0x02] = TempReg->RegData.R08[CurReg];
					DstPos += 0x03;
					TempReg->RegMask[CurReg] = 0x00;
				}
				
				CurReg = 0x104;	// 4-Op Mode
				if ((TempReg->RegMask[CurReg] & 0x7F) == 0x01)
				{
					DstData[DstPos + 0x00] = ChipCmd | (CurReg >> 8);
					DstData[DstPos + 0x01] = CurReg & 0xFF;
					DstData[DstPos + 0x02] = TempReg->RegData.R08[CurReg];
					DstPos += 0x03;
					TempReg->RegMask[CurReg] = 0x00;
				}
				break;
			case 0x0D:	// YMF278B
				ChipCmd = 0xD0;
				CmdType = 0x20;
				
				CurReg = 0x105;	// OPL3 Enabe
				if ((TempReg->RegMask[CurReg] & 0x7F) == 0x01)
				{
					DstData[DstPos + 0x00] = ChipCmd;
					DstData[DstPos + 0x01] = (CurCSet << 7) | (CurReg >> 8);
					DstData[DstPos + 0x02] = CurReg & 0xFF;
					DstData[DstPos + 0x03] = TempReg->RegData.R08[CurReg];
					DstPos += 0x04;
					TempReg->RegMask[CurReg] = 0x00;
				}
				
				CurReg = 0x104;	// 4-Op Mode
				if ((TempReg->RegMask[CurReg] & 0x7F) == 0x01)
				{
					DstData[DstPos + 0x00] = ChipCmd;
					DstData[DstPos + 0x01] = (CurCSet << 7) | (CurReg >> 8);
					DstData[DstPos + 0x02] = CurReg & 0xFF;
					DstData[DstPos + 0x03] = TempReg->RegData.R08[CurReg];
					DstPos += 0x04;
					TempReg->RegMask[CurReg] = 0x00;
				}
				break;
			case 0x0E:	// YMZ280B
				ChipCmd = 0x5D + CurCSet * 0x50;
				CmdType = 0x10;
				break;
			case 0x0F:	// YMF271
				ChipCmd = 0xD1;
				CmdType = 0x20;
				break;
			case 0x11:	// PWM
				ChipCmd = 0xB2;
				for (CurReg = 0x00; CurReg < TempReg->RegCount; CurReg ++)
				{
					if ((TempReg->RegMask[CurReg] & 0x7F) == 0x01)
					{
						DstData[DstPos + 0x00] = ChipCmd;
						DstData[DstPos + 0x01] = (CurReg << 4) | (TempReg->RegData.R16[CurReg] >> 8);
						DstData[DstPos + 0x02] = TempReg->RegData.R16[CurReg] & 0xFF;
						DstPos += 0x03;
					}
				}
				
				CmdType = 0x00;
				break;
			case 0x12:	// AY8910
				ChipCmd = 0xA0;
				CmdType = 0x12;
				break;
			case 0x13:	// GB DMG
				ChipCmd = 0xB3;
				
				CurReg = 0x16;	// Master Enable must be always written first
				if ((TempReg->RegMask[CurReg] & 0x01) &&
					(TempReg->RegData.R08[CurReg] & 0x80))
				{
					DstData[DstPos + 0x00] = ChipCmd;
					DstData[DstPos + 0x01] = (CurCSet << 7) | CurReg;
					DstData[DstPos + 0x02] = TempReg->RegData.R08[CurReg];
					DstPos += 0x03;
					TempReg->RegMask[CurReg] = 0x00;
				}
				
				// was completely written / complete write ahead?
				TempByt = 0x01;
				for (CurReg = 0x20; CurReg < 0x30; CurReg ++)
				{
					TempByt &= (TempReg->RegMask[CurReg] & 0x01) | ~0x01;
					TempByt |= (TempReg->RegMask[CurReg] & 0x10);
				}
				// write WaveRAM
				for (CurReg = 0x20; CurReg < 0x30; CurReg ++)
				{
					if (TempByt == 0x01 || ((TempByt & 0x10) && (TempReg->RegMask[CurReg] & 0x01)))
					{
						DstData[DstPos + 0x00] = ChipCmd;
						DstData[DstPos + 0x01] = (CurReg) | (CurCSet << 7);
						DstData[DstPos + 0x02] = TempReg->RegData.R08[CurReg];
						DstPos += 0x03;
						TempReg->RegMask[CurReg] = 0x00;
					}
				}
				
				// now write the other control regs
				for (CurReg = 0x14; CurReg <= 0x15; CurReg ++)
				{
					if ((TempReg->RegMask[CurReg] & 0x7F) == 0x01)
					{
						DstData[DstPos + 0x00] = ChipCmd;
						DstData[DstPos + 0x01] = (CurCSet << 7) | CurReg;
						DstData[DstPos + 0x02] = TempReg->RegData.R08[CurReg];
						DstPos += 0x03;
						TempReg->RegMask[CurReg] = 0x00;
					}
				}
				
				CmdType = 0x12;
				break;
			case 0x14:	// NES APU
				ChipCmd = 0xB4;
				CmdType = 0x12;
				break;
			case 0x15:	// MultiPCM
				ChipCmd = 0xB5;
				CmdType = 0xFF;
				break;
			case 0x16:	// UPD7759
				ChipCmd = 0xB6;
				CmdType = 0xFF;
				break;
			case 0x17:	// OKIM6258
				ChipCmd = 0xB7;
				CmdType = 0xFF;
				if ((TempReg->RegMask[0x0B] & 0x7F) == 0x01)
				{
					VGMHead.lngHzOKIM6258 =	(TempReg->RegData.R08[0x08] <<  0) |
											(TempReg->RegData.R08[0x09] <<  8) |
											(TempReg->RegData.R08[0x0A] << 16) |
											(TempReg->RegData.R08[0x0B] << 24);
				}
				if ((TempReg->RegMask[0x0C] & 0x7F) == 0x01)
				{
					VGMHead.bytOKI6258Flags &= ~0x03;
					VGMHead.bytOKI6258Flags |= TempReg->RegData.R08[0x0C] & 0x03;
				}
				
				for (CurReg = 0x00; CurReg < 0x03; CurReg ++)
				{
					WrtReg = (0x02 + CurReg) % 0x03;	// write in order 02, 00, 01
					if ((TempReg->RegMask[WrtReg] & 0x7F) == 0x01)
					{
						DstData[DstPos + 0x00] = ChipCmd;
						DstData[DstPos + 0x01] = (CurCSet << 7) | CurReg;
						DstData[DstPos + 0x02] = TempReg->RegData.R08[WrtReg];
						DstPos += 0x03;
					}
				}
				break;
			case 0x18:	// OKIM6295
				ChipCmd = 0xB8;
				CmdType = 0xFF;
				break;
			case 0x19:	// K051649
				ChipCmd = 0xD2;
				CmdType = 0xFF;
				break;
			case 0x1A:	// K054539
				ChipCmd = 0xD3;
				CmdType = 0x20;
				break;
			case 0x1B:	// HuC6280
				ChipCmd = 0xB9;
				
				for (CurReg = 0xE5; CurReg < 0xE8; CurReg ++)
				{
					if ((TempReg->RegMask[CurReg] & 0x7F) == 0x01)
					{
						DstData[DstPos + 0x00] = ChipCmd;
						if (CurReg < 0xE6)
							WrtReg = 0x00 + (CurReg - 0xE4);
						else
							WrtReg = 0x08 + (CurReg - 0xE6);
						DstData[DstPos + 0x01] = (CurCSet << 7) | WrtReg;
						DstData[DstPos + 0x02] = TempReg->RegData.R08[CurReg];
						DstPos += 0x03;
					}
				}
				
				for (TempByt = 0x00; TempByt < 0x06; TempByt ++)
				{
					CmdType = 0x00;
					WrtReg = 0xC0 + TempByt * 0x06;
					if ((TempReg->RegMask[TempByt * 0x20] & 0x7F) == 0x01)
					{
						// write Channel Select
						DstData[DstPos + 0x00] = ChipCmd;
						DstData[DstPos + 0x01] = (CurCSet << 7) | 0x00;
						DstData[DstPos + 0x02] = TempByt;
						DstPos += 0x03;
						CmdType = 0x01;
						
						// make sure the waveform index is reset
						DstData[DstPos + 0x00] = ChipCmd;
						DstData[DstPos + 0x01] = (CurCSet << 7) | 0x04;
						DstData[DstPos + 0x02] = 0x40;	// DDA Mode On
						DstPos += 0x03;
						DstData[DstPos + 0x00] = ChipCmd;
						DstData[DstPos + 0x01] = (CurCSet << 7) | 0x04;
						DstData[DstPos + 0x02] = 0x00;	// DDA Mode Off, reset wave index
						DstPos += 0x03;
						if (TempReg->RegData.R08[WrtReg + 0x02] == 0x00)
							TempReg->RegMask[WrtReg + 0x02] = 0x00;
						
						// write actual waveform data
						for (CurReg = 0x00; CurReg < 0x20; CurReg ++)
						{
							if (! (TempReg->RegMask[TempByt * 0x20 + CurReg] & 0x7F))
								break;
							
							DstData[DstPos + 0x00] = ChipCmd;
							DstData[DstPos + 0x01] = (CurCSet << 7) | 0x06;
							DstData[DstPos + 0x02] = TempReg->RegData.R08[TempByt * 0x20 + CurReg];
							DstPos += 0x03;
						}
					}
					
					for (TempSht = 0x00; TempSht < 0x05; TempSht ++)
					{
						if (TempSht == 0x00)
							CurReg = 0x05;	// Noise Mode
						else if (TempSht <= 0x02)
							CurReg = TempSht - 0x01;	// Frequency LSB/MSB
						else if (TempSht == 0x03)
							CurReg = 0x03;	// Volume
						else if (TempSht == 0x04)
							CurReg = 0x02;	// Control
						
						if ((TempReg->RegMask[WrtReg + CurReg] & 0x7F) == 0x01)
						{
							if (! CmdType)
							{
								// write Channel Select
								DstData[DstPos + 0x00] = ChipCmd;
								DstData[DstPos + 0x01] = (CurCSet << 7) | 0x00;
								DstData[DstPos + 0x02] = TempByt;
								DstPos += 0x03;
								CmdType = 0x01;
							}
							
							DstData[DstPos + 0x00] = ChipCmd;
							DstData[DstPos + 0x01] = (CurCSet << 7) | (0x02 + CurReg);
							DstData[DstPos + 0x02] = TempReg->RegData.R08[WrtReg + CurReg];
							DstPos += 0x03;
						}
					}
				}
				
				CurReg = 0xE4;
				if ((TempReg->RegMask[CurReg] & 0x7F) == 0x01)
				{
					DstData[DstPos + 0x00] = ChipCmd;
					DstData[DstPos + 0x01] = (CurCSet << 7) | 0x00;
					DstData[DstPos + 0x02] = TempReg->RegData.R08[CurReg];
					DstPos += 0x03;
				}
				
				CmdType = 0x00;
				break;
			case 0x1C:	// C140
				ChipCmd = 0xD4;
				CmdType = 0x20;
				break;
			case 0x1D:	// K053260
				ChipCmd = 0xBA;
				CmdType = 0xFF;
				break;
			case 0x1E:	// Pokey
				ChipCmd = 0xBB;
				CmdType = 0xFF;
				break;
			case 0x1F:	// QSound
				ChipCmd = 0xC4;
				
				for (CurReg = 0x00; CurReg < TempReg->RegCount; CurReg ++)
				{
					if ((TempReg->RegMask[CurReg] & 0x7F) == 0x01)
					{
						TempSht = TempReg->RegData.R16[CurReg];
						DstData[DstPos + 0x00] = ChipCmd;
						DstData[DstPos + 0x01] = (TempSht & 0xFF00) >> 8;
						DstData[DstPos + 0x02] = (TempSht & 0x00FF) >> 0;
						DstData[DstPos + 0x03] = (UINT8)CurReg;
						DstPos += 0x04;
					}
				}
				
				CmdType = 0x00;
				break;
			default:
				CmdType = 0xFF;
				break;
			}
			
			switch(CmdType & 0xF0)
			{
			case 0x10:
				for (CurReg = 0x00; CurReg < TempReg->RegCount; CurReg ++)
				{
					if ((TempReg->RegMask[CurReg] & 0x7F) == 0x01)
					{
						if ((CmdType & 0x0F) == 0x00)
						{
							DstData[DstPos + 0x00] = ChipCmd;
							DstData[DstPos + 0x01] = CurReg & 0xFF;
						}
						else if ((CmdType & 0x0F) == 0x01)
						{
							DstData[DstPos + 0x00] = ChipCmd | (CurReg >> 8);
							DstData[DstPos + 0x01] = CurReg & 0xFF;
						}
						else if ((CmdType & 0x0F) == 0x02)
						{
							DstData[DstPos + 0x00] = ChipCmd;
							DstData[DstPos + 0x01] = (CurCSet << 7) | CurReg;
						}
						DstData[DstPos + 0x02] = TempReg->RegData.R08[CurReg];
						DstPos += 0x03;
					}
				}
				break;
			case 0x20:
				for (CurReg = 0x00; CurReg < TempReg->RegCount; CurReg ++)
				{
					if ((TempReg->RegMask[CurReg] & 0x7F) == 0x01)
					{
						DstData[DstPos + 0x00] = ChipCmd;
						if ((CmdType & 0x0F) == 0x00)
						{
							DstData[DstPos + 0x01] = (CurCSet << 7) | (CurReg >> 8);
							DstData[DstPos + 0x02] = CurReg & 0xFF;
						}
						else //if ((CmdType & 0x0F) == 0x01)
						{
							DstData[DstPos + 0x01] = CurReg & 0xFF;
							DstData[DstPos + 0x02] = (CurCSet << 7) | (CurReg >> 8);
						}
						DstData[DstPos + 0x03] = TempReg->RegData.R08[CurReg];
						DstPos += 0x04;
					}
				}
				break;
			case 0x80:
				for (CurReg = 0x00; CurReg < TempReg->RegCount; CurReg ++)
				{
					if (CurChip == 0x03)
					{
						// OPM Workaround for Reg 0x19
						if (CurReg == 0x1A)
							WrtReg = 0x19;
						else
							WrtReg = CurReg;
						TempSht = CurReg;
					}
					else //if (CurChip == 0x02, 0x06, 0x07, 0x08)
					{
						// Workaround for OPN frequency register (0xA0)
						if ((CurReg & 0xE0) == 0xA0)
						{
							// B0 has priority over A0
							if ((CurReg & 0xF0) == 0xA0)
								TempSht = 0xB0 | (CurReg & 0x10F);
							else	// order for A0..A7 is A4 A0 A5 A1 A6 A2 A7 A3
								TempSht = 0xA0 | (CurReg & 0x108) | 
											((CurReg & 0x07) >> 1) |
											((~CurReg & 0x01) << 2);
						}
						else
						{
							TempSht = CurReg;
							if (CurChip == 0x06)
							{
								if (CurReg == 0x28)
									TempSht = 0xFF;
								//else if (CurReg == 0xFF)
								//	TempSht = 0x28;
							}
							else
							{
								if (CurReg == 0x28)
									TempSht = 0x1FF;
								//else if (CurReg == 0x1FF)
								//	TempSht = 0x28;
							}
						}
						WrtReg = TempSht;
					}
					if ((TempReg->RegMask[TempSht] & 0x7F) == 0x01)
					{
						DstData[DstPos + 0x00] = ChipCmd | (WrtReg >> 8);
						DstData[DstPos + 0x01] = WrtReg & 0xFF;
						DstData[DstPos + 0x02] = TempReg->RegData.R08[TempSht];
						DstPos += 0x03;
					}
				}
				break;
			}
		}
	}
	
	*DstPosRef = DstPos;
	
	return;
}

static UINT16 GetChipCommand(UINT8 Command)
{
	// Note: returns ChipID only for PSG and YM-chips
	UINT8 ChipID;
	
	// Cheat Mode (to use 2 instances of 1 chip)
	ChipID = 0x00;
	switch(Command)
	{
	case 0x66:	// End Of File
	case 0x62:	// 1/60s delay
	case 0x63:	// 1/50s delay
	case 0x61:	// xx Sample Delay
	case 0x67:	// PCM Data Stream
	case 0xE0:	// Seek to PCM Data Bank Pos
	case 0x68:	// PCM RAM write
		return 0x0000;
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
	}
	
	return (ChipID << 8) | (Command << 0);
}

static UINT32 ReadCommand(UINT8 Mask)
{
	UINT8 ChipID;
	UINT8 Command;
	//UINT8 TempByt;
	//UINT16 TempSht;
	//UINT32 TempLng;
	UINT16 CmdReg;
	UINT16 ChnReg;
	CHIP_DATA* TempChp;
	CHIP_STATE* TempReg;
	UINT32 CmdLen;
	
	CmdReg = GetChipCommand(VGMData[VGMPos + 0x00]);
	if (! CmdReg)
		return 0x00;
	
	ChipID = (CmdReg & 0x0FF00) >> 8;
	Command = (CmdReg & 0x00FF) >> 0;
	
	CmdLen = 0x00;
	TempChp = NULL;
	switch(Command)
	{
	case 0x50:	// SN76496 write
		TempChp = &RC[ChipID].SN76496;
		TempReg = &TempChp->Regs;
		if (TempReg->RegCount)
		{
			CmdReg = (VGMData[VGMPos + 0x01] & 0xF0) >> 4;
			if (CmdReg & 0x08)
				TempReg->RegData.R08[0x11] = (UINT8)CmdReg;
			else
				CmdReg = TempReg->RegData.R08[0x11] & 0x07;
			
			if (CmdReg < TempReg->RegCount)
			{
				if (Mask == 0x01)
					TempReg->RegData.R08[CmdReg] = VGMData[VGMPos + 0x01];
				TempReg->RegMask[CmdReg] |= Mask;
			}
		}
		
		CmdLen = 0x02;
		break;
	case 0x4F:	// GG Stereo
		TempChp = &RC[ChipID].SN76496;
		TempReg = &TempChp->Regs;
		if (TempReg->RegCount)
		{
			CmdReg = 0x10;
			if (Mask == 0x01)
				TempReg->RegData.R08[CmdReg] = VGMData[VGMPos + 0x01];
			TempReg->RegMask[CmdReg] |= Mask;
		}
		
		CmdLen = 0x02;
		break;
	case 0x51:	// YM2413 write
	case 0x55:	// YM2203 write
	case 0x5A:	// YM3812 write
	case 0x5B:	// YM3526 write
	case 0x5C:	// Y8950 write
	case 0x5D:	// YMZ280B write
		if (Command == 0x51)
			TempChp = &RC[ChipID].YM2413;
		else if (Command == 0x55)
			TempChp = &RC[ChipID].YM2203;
		else if (Command == 0x5A)
			TempChp = &RC[ChipID].YM3812;
		else if (Command == 0x5B)
			TempChp = &RC[ChipID].YM3526;
		else if (Command == 0x5C)
			TempChp = &RC[ChipID].Y8950;
		else if (Command == 0x5D)
			TempChp = &RC[ChipID].YMZ280B;
		TempReg = &TempChp->Regs;
		
		if (TempReg->RegCount)
		{
			CmdReg = VGMData[VGMPos + 0x01];
			if (CmdReg < TempReg->RegCount)
			{
				if (Mask == 0x01)
					TempReg->RegData.R08[CmdReg] = VGMData[VGMPos + 0x02];
				TempReg->RegMask[CmdReg] |= Mask;
			}
		}
		
		CmdLen = 0x03;
		break;
	case 0x52:	// YM2612 write port 0
	case 0x53:	// YM2612 write port 1
	case 0x56:	// YM2608 write port 0
	case 0x57:	// YM2608 write port 1
	case 0x58:	// YM2610 write port 0
	case 0x59:	// YM2610 write port 1
	case 0x5E:	// YMF262 write port 0
	case 0x5F:	// YMF262 write port 1
		if ((Command & 0xFE) == 0x52)
			TempChp = &RC[ChipID].YM2612;
		else if ((Command & 0xFE) == 0x56)
			TempChp = &RC[ChipID].YM2608;
		else if ((Command & 0xFE) == 0x58)
			TempChp = &RC[ChipID].YM2610;
		else if ((Command & 0xFE) == 0x5E)
			TempChp = &RC[ChipID].YMF262;
		TempReg = &TempChp->Regs;
		
		if (TempReg->RegCount)
		{
			CmdReg = ((Command & 0x01) << 8) | VGMData[VGMPos + 0x01];
			if (CmdReg < TempReg->RegCount)
			{
				if (Mask == 0x01)
					TempReg->RegData.R08[CmdReg] = VGMData[VGMPos + 0x02];
				TempReg->RegMask[CmdReg] |= Mask;
			}
		}
		
		CmdLen = 0x03;
		break;
	case 0x54:	// YM2151 write
		TempChp = &RC[ChipID].YM2151;
		TempReg = &TempChp->Regs;
		if (TempReg->RegCount)
		{
			CmdReg = VGMData[VGMPos + 0x01];
			if (CmdReg == 0x1A)
				CmdReg = 0xFFFF;
			else if (CmdReg == 0x19 && (VGMData[VGMPos + 0x02] & 0x80))
				CmdReg = 0x1A;
			
			if (CmdReg < TempReg->RegCount)
			{
				if (Mask == 0x01)
					TempReg->RegData.R08[CmdReg] = VGMData[VGMPos + 0x02];
				TempReg->RegMask[CmdReg] |= Mask;
			}
		}
		
		CmdLen = 0x03;
		break;
	case 0xC0:	// Sega PCM memory write
		TempChp = &RC[ChipID].SegaPCM;
		TempReg = &TempChp->Regs;
		if (TempReg->RegCount)
		{
			memcpy(&CmdReg, &VGMData[VGMPos + 0x01], 0x02);
			if (CmdReg < TempReg->RegCount)
			{
				if (Mask == 0x01)
					TempReg->RegData.R08[CmdReg] = VGMData[VGMPos + 0x03];
				TempReg->RegMask[CmdReg] |= Mask;
			}
		}
		
		CmdLen = 0x04;
		break;
	case 0xB0:	// RF5C68 register write
	case 0xB1:	// RF5C164 register write
		if (Command == 0xB0)
			TempChp = &RC[ChipID].RF5C68;
		else //if (Command == 0xB1)
			TempChp = &RC[ChipID].RF5C164;
		TempReg = &TempChp->Regs;
		if (TempReg->RegCount)
		{
			// 00-06, 07-0D, .. - Channel Regs
			// 38 - Memory Bank
			// 39 - Channel Bank
			// 3A - Channel Mask
			CmdReg = VGMData[VGMPos + 0x01];
			if (CmdReg < 0x07)
			{
				ChnReg = TempReg->RegData.R08[0x39] & 0x07;
				CmdReg += ChnReg * 0x07;
				if (Mask == 0x01)
					TempReg->RegData.R08[CmdReg] = VGMData[VGMPos + 0x02];
				TempReg->RegMask[CmdReg] |= Mask;
			}
			else if (CmdReg == 0x07)
			{
				if (! (VGMData[VGMPos + 0x02] & 0x40))
					CmdReg = 0x38;
				else
					CmdReg = 0x39;
				if (Mask == 0x01)
					TempReg->RegData.R08[CmdReg] = VGMData[VGMPos + 0x02];
				TempReg->RegMask[CmdReg] |= Mask;
				
				CmdReg ^= 0x01;	// force Chip Enable for both regs
				TempReg->RegData.R08[CmdReg] |= VGMData[VGMPos + 0x02] & 0x80;
			}
			else if (CmdReg = 0x08)
			{
				CmdReg = 0x3A;
				if (Mask == 0x01)
					TempReg->RegData.R08[CmdReg] = VGMData[VGMPos + 0x02];
				TempReg->RegMask[CmdReg] |= Mask;
			}
		}
		
		CmdLen = 0x03;
		break;
	case 0xD0:	// YMF278B write
	case 0xD1:	// YMF271 write
	case 0xD2:	// SCC1 write
	case 0xD3:	// K054539 write
	case 0xD4:	// C140 write
		ChipID = VGMData[VGMPos + 0x01] >> 7;
		if (Command == 0xD0)
			TempChp = &RC[ChipID].YMF278B;
		else if (Command == 0xD1)
			TempChp = &RC[ChipID].YMF271;
		else if (Command == 0xD2)
			TempChp = &RC[ChipID].K051649;
		else if (Command == 0xD3)
			TempChp = &RC[ChipID].K054539;
		else if (Command == 0xD4)
			TempChp = &RC[ChipID].C140;
		TempReg = &TempChp->Regs;
		
		if (TempReg->RegCount)
		{
			CmdReg = ((VGMData[VGMPos + 0x01] & 0x7F) << 8) | VGMData[VGMPos + 0x02];
			if (CmdReg < TempReg->RegCount)
			{
				if (Mask == 0x01)
					TempReg->RegData.R08[CmdReg] = VGMData[VGMPos + 0x03];
				TempReg->RegMask[CmdReg] |= Mask;
			}
		}
		
		CmdLen = 0x04;
		break;
	case 0xB2:	// PWM register write
		TempChp = &RC[ChipID].PWM;
		TempReg = &TempChp->Regs;
		if (TempReg->RegCount)
		{
			CmdReg = VGMData[VGMPos + 0x01] >> 4;
			if (CmdReg < TempReg->RegCount)
			{
				if (Mask == 0x01)
					TempReg->RegData.R16[CmdReg] =	(VGMData[VGMPos + 0x01] << 8) |
													(VGMData[VGMPos + 0x02] << 0);
				TempReg->RegMask[CmdReg] |= Mask;
			}
		}
		
		CmdLen = 0x03;
		break;
	case 0xA0:	// AY8910 write
	case 0xB3:	// GameBoy DMG write
	case 0xB4:	// NES APU write
	case 0xB5:	// MultiPCM write
	case 0xB6:	// UPD7759 write
	case 0xB7:	// OKIM6258 write
	case 0xB8:	// OKIM6295 write
	case 0xBA:	// K053260 write
	case 0xBB:	// Pokey write
		ChipID = VGMData[VGMPos + 0x01] >> 7;
		if (Command == 0xA0)
			TempChp = &RC[ChipID].AY8910;
		else if (Command == 0xB3)
			TempChp = &RC[ChipID].GBDMG;
		else if (Command == 0xB4)
			TempChp = &RC[ChipID].NESAPU;
		else if (Command == 0xB5)
			TempChp = &RC[ChipID].MultiPCM;
		else if (Command == 0xB6)
			TempChp = &RC[ChipID].UPD7759;
		else if (Command == 0xB7)
			TempChp = &RC[ChipID].OKIM6258;
		else if (Command == 0xB8)
			TempChp = &RC[ChipID].OKIM6295;
		else if (Command == 0xBA)
			TempChp = &RC[ChipID].K053260;
		else if (Command == 0xBB)
			TempChp = &RC[ChipID].Pokey;
		TempReg = &TempChp->Regs;
		
		if (TempReg->RegCount)
		{
			CmdReg = VGMData[VGMPos + 0x01] & 0x7F;
			if (CmdReg < TempReg->RegCount)
			{
				if (Mask == 0x01)
					TempReg->RegData.R08[CmdReg] = VGMData[VGMPos + 0x02];
				TempReg->RegMask[CmdReg] |= Mask;
			}
		}
		
		CmdLen = 0x03;
		break;
	case 0xB9:	// HuC6280 write
		ChipID = VGMData[VGMPos + 0x01] >> 7;
		TempChp = &RC[ChipID].HuC6280;
		TempReg = &TempChp->Regs;
		
		if (TempReg->RegCount)
		{
			// 00-BF - Waveform RAM
			// C0-E3 - Channels Regs (Freq LSB, MSB; Ctrl/Vol, Pan, WaveIdx, Noise)
			// E4-E7 - Global Regs (Chn Select, Pan, LFO Freq, LFO Ctrl)
			CmdReg = VGMData[VGMPos + 0x01] & 0x0F;
			switch(CmdReg)
			{
			case 0x00: // Channel select
			case 0x01: // Global balance
				CmdReg = 0xE4 + CmdReg;
				break;
			case 0x02: // Channel frequency (LSB)
			case 0x03: // Channel frequency (MSB)
			case 0x04: // Channel control (key-on, DDA mode, volume)
			case 0x05: // Channel balance
			case 0x06: // Channel waveform data
			case 0x07: // Noise control (enable, frequency)
				if (TempReg->RegData.R08[0xE4] >= 0x06)
				{
					CmdReg = 0xFFFF;
					break;
				}
				
				ChnReg = 0xC0 + TempReg->RegData.R08[0xE4] * 0x06;
				if (CmdReg == 0x04)
				{
					// 1-to-0 transition of DDA bit resets waveform index
					if ((TempReg->RegData.R08[ChnReg + 0x02] & 0x40) &&
						! (VGMData[VGMPos + 0x02] & 0x40))
						TempReg->RegData.R08[ChnReg + 0x04] = 0x00;
				}
				else if (CmdReg == 0x06)
				{
					if (! (TempReg->RegData.R08[ChnReg + 0x02] & 0x40))
					{
						CmdReg = TempReg->RegData.R08[0xE4] * 0x20 +
								TempReg->RegData.R08[ChnReg + 0x04];
						TempReg->RegData.R08[ChnReg + 0x04] ++;
						TempReg->RegData.R08[ChnReg + 0x04] &= 0x1F;
						break;
					}
					else
					{
						CmdReg = 0xFFFF;
						break;
					}
				}
				CmdReg = ChnReg + (CmdReg - 0x02);
				break;
			case 0x08: // LFO frequency
			case 0x09: // LFO control (enable, mode)
				CmdReg = 0xE6 + (CmdReg - 0x08);
				break;
			default:
				CmdReg = 0xFFFF;
				break;
			}
			
			if (CmdReg < TempReg->RegCount)
			{
				if (Mask == 0x01)
					TempReg->RegData.R08[CmdReg] = VGMData[VGMPos + 0x02];
				TempReg->RegMask[CmdReg] |= Mask;
			}
		}
		
		CmdLen = 0x03;
		break;
	case 0xC4:	// Q-Sound write
		TempChp = &RC[ChipID].QSound;
		TempReg = &TempChp->Regs;
		
		if (TempReg->RegCount)
		{
			CmdReg = VGMData[VGMPos + 0x03];
			if (CmdReg < TempReg->RegCount)
			{
				if (Mask == 0x01)
					TempReg->RegData.R16[CmdReg] = (VGMData[VGMPos + 0x01] << 8) |
													(VGMData[VGMPos + 0x02] << 0);
				TempReg->RegMask[CmdReg] |= Mask;
			}
		}
		
		CmdLen = 0x04;
		break;
	}
	CommandCheck(0x00, Command, TempChp, CmdReg);
	
	return CmdLen;
}

static void CommandCheck(UINT8 Mode, UINT8 Command, CHIP_DATA* ChpData, UINT16 CmdReg)
{
	// Checks the command for Key On and Key Off
	// and sets the according channel bits
	CHIP_CHNS* TempChn;
	CHIP_STATE* TempReg;
	UINT8 CurChn;
	UINT8 KeyOnOff;
	UINT16 TempSht;
	
	if (ChpData == NULL)
		return;
	TempReg = &ChpData->Regs;
	TempChn = &ChpData->Chns;
	if (! TempChn->ChnCount)
		return;
	
	switch(Command)
	{
	case 0x50:	// SN76496 write
		CmdReg &= ~0x09;
		CurChn = (CmdReg & 0x06) >> 1;
		KeyOnOff = 0x00;
		
		// test Frequency
		TempSht = (TempReg->RegData.R08[CmdReg | 0x00] << 0) |
					(TempReg->RegData.R08[CmdReg | 0x08] << 4);
		if (TempSht < 0x06)
			KeyOnOff |= 0x01;
		// test volume
		if (TempReg->RegData.R08[CmdReg | 0x09] == 0xFF)
			KeyOnOff |= 0x01;
		
		TempChn->ChnMask &= ~(1 << CurChn);
		TempChn->ChnMask |= (KeyOnOff << CurChn);
		break;
	case 0x4F:	// GG Stereo
		break;
	case 0x51:	// YM2413 write
		if ((CmdReg & 0x30) == 0x20)
		{
			CurChn = CmdReg & 0x0F;
			KeyOnOff = (TempReg->RegData.R08[CmdReg] & 0x10) >> 4;
			
			TempChn->ChnMask &= ~(1 << CurChn);
			TempChn->ChnMask |= (KeyOnOff << CurChn);
		}
		break;
	case 0x5A:	// YM3812 write
	case 0x5B:	// YM3526 write
	case 0x5C:	// Y8950 write
	case 0x5E:	// YMF262 write port 0
	case 0x5F:	// YMF262 write port 1
		if ((CmdReg & 0xF0) == 0xB0)
		{
			CurChn = ((CmdReg & 0x100) >> 8) * 9 + (CmdReg & 0x0F);
			KeyOnOff = (TempReg->RegData.R08[CmdReg] & 0x20) >> 5;
			
			TempChn->ChnMask &= ~(1 << CurChn);
			TempChn->ChnMask |= (KeyOnOff << CurChn);
		}
		break;
	case 0x52:	// YM2612 write port 0
	case 0x53:	// YM2612 write port 1
	case 0x55:	// YM2203 write
	case 0x56:	// YM2608 write port 0
	case 0x57:	// YM2608 write port 1
	case 0x58:	// YM2610 write port 0
	case 0x59:	// YM2610 write port 1
		if (CmdReg == 0x028)
		{
			CurChn = (TempReg->RegData.R08[0x028] / 0x04 * 3) +
					(TempReg->RegData.R08[0x028] & 0x03);
			KeyOnOff = (TempReg->RegData.R08[0x028] & 0xF0) ? 0x01 : 0x00;
			
			TempChn->ChnMask &= ~(1 << CurChn);
			TempChn->ChnMask |= (KeyOnOff << CurChn);
		}
		break;
	case 0x54:	// YM2151 write
		if (CmdReg == 0x08)
		{
			CurChn = TempReg->RegData.R08[0x08] & 0x07;
			KeyOnOff = (TempReg->RegData.R08[0x08] & 0x78) ? 0x01 : 0x00;
			
			TempChn->ChnMask &= ~(1 << CurChn);
			TempChn->ChnMask |= (KeyOnOff << CurChn);
		}
		break;
	case 0xC0:	// Sega PCM memory write
		if ((CmdReg & ~0x78) == 0x86)
		{
			CurChn = (CmdReg >> 3) & 0x0F;
			KeyOnOff = TempReg->RegData.R08[CmdReg] & 0x01;
			
			TempChn->ChnMask &= ~(1 << CurChn);
			TempChn->ChnMask |= (KeyOnOff << CurChn);
		}
		break;
	case 0xB0:	// RF5C68 register write
	case 0xB1:	// RF5C164 register write
		if (CmdReg = 0x3A)
		{
			// This is actually Reg 08, but it already got
			// redirected to 3A.
			TempChn->ChnMask = ~TempReg->RegData.R08[CmdReg];
		}
		break;
	case 0x5D:	// YMZ280B write
		if ((CmdReg & 0xE3) == 0x01)
		{
			CurChn = (CmdReg & 0x1C) >> 2;
			KeyOnOff = (TempReg->RegData.R08[CmdReg] & 0x80) >> 7;
			
			TempChn->ChnMask &= ~(1 << CurChn);
			TempChn->ChnMask |= (KeyOnOff << CurChn);
		}
		break;
	case 0xD0:	// YMF278B write
		if (CmdReg < 0x200)
		{
			if ((CmdReg & 0xF0) == 0xB0)
			{
				CurChn = ((CmdReg & 0x100) >> 8) * 9 + (CmdReg & 0x0F);
				KeyOnOff = (TempReg->RegData.R08[CmdReg] & 0x20) >> 5;
				
				TempChn->ChnMask &= ~(1 << CurChn);
				TempChn->ChnMask |= (KeyOnOff << CurChn);
			}
		}
		else
		{
			if (CmdReg >= 0x68 && CmdReg <= 0x7F)
			{
				CurChn = CmdReg - 0x68;
				KeyOnOff = (TempReg->RegData.R08[CmdReg] & 0x80) >> 7;
				
				TempChn->ChnMask2 &= ~(1 << CurChn);
				TempChn->ChnMask2 |= (KeyOnOff << CurChn);
			}
		}
		break;
	case 0xD1:	// YMF271 write
		// no - I don't want to do this right now.
		break;
	case 0xB2:	// PWM register write
		// nothing to do
		break;
	case 0xA0:	// AY8910 write
		if (CmdReg == 0x07)
		{
			TempChn->ChnMask = ~TempReg->RegData.R08[0x07] & 0x3F;
		}
		break;
	case 0xB3:	// GameBoy DMG write
		if (CmdReg < 0x14)
		{
			if ((CmdReg % 0x05) == 0x04)
			{
				CurChn = CmdReg / 0x05;
				KeyOnOff = (TempReg->RegData.R08[CmdReg] & 0x80) >> 7;
				
				// just ORs the enable-bit, doesn't reset it
				TempChn->ChnMask |= (KeyOnOff << CurChn);
			}
			else if ((CmdReg % 0x05) == 0x00)
			{
				CurChn = CmdReg / 0x05;
				KeyOnOff = (TempReg->RegData.R08[CmdReg] & 0x80) >> 7;
				
				TempChn->ChnMask &= ~(1 << CurChn);
				TempChn->ChnMask |= (KeyOnOff << CurChn);
			}
		}
		break;
	case 0xB4:	// NES APU write
		if (CmdReg == 0x15)
		{
			TempChn->ChnMask = TempReg->RegData.R08[0x15] & 0x1F;
		}
		
		break;
	case 0xB9:	// HuC6280 write
		if (CmdReg >= 0xC0 && CmdReg <= 0xE3)
		{
			TempSht = CmdReg - 0xC0;
			if ((TempSht % 0x06) == 0x02)
			{
				CurChn = TempSht / 0x06;
				KeyOnOff = (TempReg->RegData.R08[CmdReg] & 0x80) >> 7;
				
				TempChn->ChnMask &= ~(1 << CurChn);
				TempChn->ChnMask |= (KeyOnOff << CurChn);
			}
		}
		break;
	case 0xC4:	// Q-Sound write
		break;
	}
	
	return;
}

void TrimVGMData(const INT32 StartSmpl, const INT32 LoopSmpl, const INT32 EndSmpl,
				 const bool HasLoop, const bool KeepESmpl)
{
	INT32 LoopSmplA;
	INT32 EndSmplA;
	UINT32 DstPos;
	UINT8 ChipID;
	UINT8 Command;
	UINT32 CmdDelay;
	bool DlyToWrt;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
#ifdef SHOW_PROGRESS
	UINT32 CmdTimer;
	char TempStr[0x100];
	char MinSecStr[0x80];
#endif
	UINT32 CmdLen;
	//UINT16 CmdReg;
	//UINT8 CmdData;
	bool StopVGM;
	bool IsDelay;
	UINT8 WriteMode;
	UINT32 LoopPos;
	CHIP_DATA* TempCD;
	CHIP_STATE* TempReg;
	CHIP_MEMORY* TempMem;
	bool ForceCmdWrite;
	
	// +0x100 - Make sure to have enough room for additional delays
	DstDataLen = VGMDataLen + 0x100;
	CmdDelay = 0;
	VGMPos = VGMHead.lngDataOffset;
	DstPos = VGMHead.lngDataOffset;
	
	switch(HasLoop)
	{
	case 0x00:	// No Loop
		LoopSmplA = 0x00;
		break;
	case 0x01:	// Has Loop
		if (LoopSmpl == -1)	// Keep Loop
			LoopSmplA = VGMHead.lngTotalSamples - VGMHead.lngLoopSamples;
		else
			LoopSmplA = LoopSmpl;
		break;
	}
	EndSmplA = EndSmpl + (KeepESmpl ? 1 : 0);
	
	PrepareChipMemory();
	SetImportantCommands();
	
	for (ChipID = 0x00; ChipID < CHIP_SETS; ChipID ++)
	{
		TempCD = (CHIP_DATA*)&RC[ChipID];
		
		for (TempByt = 0x00; TempByt < CHIP_COUNT; TempByt ++, TempCD ++)
		{
			DstDataLen += TempCD->Mem.MemSize;
		}
	}
	
	if (DstData == NULL)	// now allocate memory, if needed
		DstData = (UINT8*)malloc(DstDataLen);
	
#ifdef SHOW_PROGRESS
	CmdTimer = 0;
#endif
	VGMSmplPos = 0x00;
	LoopPos = 0x00;
	StopVGM = false;
	ForceCmdWrite = false;
	DlyToWrt = false;
	WriteMode = 0x00;
	while(VGMPos < VGMHead.lngEOFOffset)
	{
		CmdLen = 0x00;
		Command = VGMData[VGMPos + 0x00];
		IsDelay = false;
		
		CmdDelay = 0x00;
		if (Command >= 0x70 && Command <= 0x8F)
		{
			switch(Command & 0xF0)
			{
			case 0x70:
				TempSht = (Command & 0x0F) + 0x01;
				VGMSmplPos += TempSht;
				CmdDelay = TempSht;
				IsDelay = true;
				break;
			case 0x80:
				TempSht = Command & 0x0F;
				VGMSmplPos += TempSht;
				CmdDelay = TempSht;
				break;
			}
			CmdLen = 0x01;
		}
		else
		{
			CmdLen = ReadCommand(0x01);
			if (! CmdLen)
			{
			
			ChipID = 0x00;
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
				IsDelay = true;
				break;
			case 0x63:	// 1/50s delay
				TempSht = 882;
				VGMSmplPos += TempSht;
				CmdDelay = TempSht;
				CmdLen = 0x01;
				IsDelay = true;
				break;
			case 0x61:	// xx Sample Delay
				memcpy(&TempSht, &VGMData[VGMPos + 0x01], 0x02);
				VGMSmplPos += TempSht;
				CmdDelay = TempSht;
				CmdLen = 0x03;
				IsDelay = true;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMData[VGMPos + 0x02];
				memcpy(&TempLng, &VGMData[VGMPos + 0x03], 0x04);
				
				ChipID = (TempLng & 0x80000000) >> 31;
				TempLng &= 0x7FFFFFFF;
				
				switch(TempByt & 0xC0)
				{
				case 0x00:	// Database Block
				case 0x40:
					ForceCmdWrite = true;
					break;
				case 0x80:	// ROM/RAM Dump
					ForceCmdWrite = true;
					break;
				case 0xC0:	// RAM Write
					memcpy(&TempSht, &VGMData[VGMPos + 0x07], 0x02);
					CmdLen = TempLng - 0x02;
					switch(TempByt)
					{
					case 0xC0:	// RF5C68 RAM Write
					case 0xC1:	// RF5C164 RAM Write
						if (TempByt == 0xC0)
						{
							TempReg = &RC[ChipID].RF5C68.Regs;
							TempMem = &RC[ChipID].RF5C68.Mem;
						}
						else //if (TempByt == 0xC1)
						{
							TempReg = &RC[ChipID].RF5C164.Regs;
							TempMem = &RC[ChipID].RF5C164.Mem;
						}
						if (TempMem->MemSize)
						{
							TempSht |= (TempReg->RegData.R08[0x38] & 0x0F) << 12;
							memcpy(&TempMem->MemData[TempSht], &VGMData[VGMPos + 0x09], CmdLen);
							TempMem->HadWrt = true;
						}
						break;
					}
					break;
				}
				CmdLen = 0x07 + TempLng;
				break;
			case 0xE0:	// Seek to PCM Data Bank Pos
				memcpy(&TempLng, &VGMData[VGMPos + 0x01], 0x04);
				CmdLen = 0x05;
				break;
			case 0xC1:	// RF5C68 memory write
			case 0xC2:	// RF5C164 memory write
				if (Command == 0xC1)
				{
					TempReg = &RC[ChipID].RF5C68.Regs;
					TempMem = &RC[ChipID].RF5C68.Mem;
				}
				else //if (Command == 0xC2)
				{
					TempReg = &RC[ChipID].RF5C164.Regs;
					TempMem = &RC[ChipID].RF5C164.Mem;
				}
				memcpy(&TempSht, &VGMData[VGMPos + 0x01], 0x02);
				if (TempMem->MemSize)
				{
					TempSht |= (TempReg->RegData.R08[0x38] & 0x0F) << 12;
					TempMem->MemData[TempSht] = VGMData[VGMPos + 0x03];
					TempMem->HadWrt = true;
				}
				CmdLen = 0x04;
				break;
			case 0x68:	// PCM RAM write
				CmdLen = 0x0C;
				break;
			case 0x90:	// DAC Ctrl: Setup Chip
				ForceCmdWrite = true;
				CmdLen = 0x05;
				break;
			case 0x91:	// DAC Ctrl: Set Data
				ForceCmdWrite = true;
				CmdLen = 0x05;
				break;
			case 0x92:	// DAC Ctrl: Set Freq
				ForceCmdWrite = true;
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
					printf("Unknown Command: %hX\n", Command);
					CmdLen = 0x01;
					//StopVGM = true;
					break;
				}
				break;
			}
			
			}
		}
		
		TempSht = 0x00;
		switch(WriteMode)
		{
		case 0x00:	// before Start Point
			if (VGMSmplPos >= StartSmpl)
			{
				// Beginn to write to trimmed vgm
				
				// Rewrite neccessary commands
				VGMReadAhead(VGMPos + CmdLen, 2 * 44100);	// Read 2 seconds ahead
				
				InitializeVGM(DstData, &DstPos);
				
				CmdDelay = VGMSmplPos - StartSmpl;	// queue Initial Delay
				if (CmdDelay & 0x80000000)	// if CmdDelay < 0 for unsigned
				{
					printf("Critical Program Error! Trimming will be incorrect!\nPlease report!\n");
					CmdDelay = 0x00;
				}
				DlyToWrt = true;
				
				TempSht = 0x01;
				WriteMode = 0x01;
			}
			if (WriteMode == 0x00)
			{
				if (ForceCmdWrite || (VGMTOOL_TRIM && ! IsDelay))
				{
					memcpy(&DstData[DstPos], &VGMData[VGMPos], CmdLen);
					DstPos += CmdLen;
					ForceCmdWrite = false;
				}
				break;
			}
			// Fall through for Loop Point Check
		case 0x01:
			if (HasLoop && ! LoopPos)
			{
				if (VGMSmplPos >= LoopSmplA)
				{
					// Insert Loop Point
					TempLng = LoopSmplA - (VGMSmplPos - CmdDelay);	// Delay before Loop
					if (TempLng & 0x80000000)	// if TempLng < 0
					{
						printf("Critical Program Error! Trimming will be incorrect!\nPlease report!\n");
						TempLng = 0x00;
					}
					VGMLib_WriteDelay(DstData, &DstPos, TempLng, NULL);
					
					LoopPos = DstPos;
					
					CmdDelay = VGMSmplPos - LoopSmplA;	// queue Delay after Loop
					if (CmdDelay & 0x80000000)	// CmdDelay overflow check
					{
						printf("Critical Program Error! Trimming will be incorrect!\nPlease report!\n");
						CmdDelay = 0x00;
					}
					DlyToWrt = true;
					TempSht = 0x01;
				}
			}
			if (IsDelay || StopVGM)
			{
				if (VGMSmplPos >= EndSmplA || StopVGM)
				{
					// Finish trimming
					TempLng = EndSmpl - (VGMSmplPos - CmdDelay);	// it's EndSmpl, not EndSmplA
					if (TempLng & 0x80000000)	// TempLng overflow check
					{
						printf("Critical Program Error! Trimming will be incorrect!\nPlease report!\n");
						TempLng = 0x00;
					}
					VGMLib_WriteDelay(DstData, &DstPos, TempLng, NULL);
					DstData[DstPos] = 0x66;
					DstPos ++;
					
					if (HasLoop)
					{
						WriteVGMHeader(DstData, VGMData, DstPos, EndSmpl - StartSmpl,
										LoopPos, EndSmpl - LoopSmplA);
					}
					else
					{
						DisplayPlayingNoteWarning();
						WriteVGMHeader(DstData, VGMData, DstPos, EndSmpl - StartSmpl,
										0x00, 0x00);
					}
					
					StopVGM = true;
					IsDelay = true;
					WriteMode = 0x02;
				}
				else if (! TempSht)
				{
					IsDelay = false;
				}
			}
			
			if (DlyToWrt)
			{
				// write remaining delays ("left-overs" from Start and Loop Start)
				VGMLib_WriteDelay(DstData, &DstPos, CmdDelay, NULL);
				DlyToWrt = false;
			}
			if (! IsDelay)
			{
				memcpy(&DstData[DstPos], &VGMData[VGMPos], CmdLen);
				DstPos += CmdLen;
			}
			break;
		case 0x02:	// after End Point
			break;
		}
		VGMPos += CmdLen;
		if (StopVGM)
			break;
		
#ifdef SHOW_PROGRESS
		if (CmdTimer < GetTickCount())
		{
			PrintMinSec(VGMSmplPos, MinSecStr);
			PrintMinSec(VGMHead.lngTotalSamples, TempStr);
			TempLng = VGMPos - VGMHead.lngDataOffset;
			CmdLen = VGMHead.lngEOFOffset - VGMHead.lngDataOffset;
			printf("%04.3f %% - %s / %s (%08lX / %08lX) ...\r", (float)TempLng / CmdLen * 100,
					MinSecStr, TempStr, VGMPos, VGMHead.lngEOFOffset);
			CmdTimer = GetTickCount() + 200;
		}
#endif
	}
	
	for (ChipID = 0x00; ChipID < CHIP_SETS; ChipID ++)
	{
		TempCD = (CHIP_DATA*)&RC[ChipID];
		
		for (TempByt = 0x00; TempByt < CHIP_COUNT; TempByt ++, TempCD ++)
		{
			if (TempCD->Regs.RegData.R08 != NULL)
				free(TempCD->Regs.RegData.R08);
			if (TempCD->Regs.RegMask!= NULL)
				free(TempCD->Regs.RegMask);
			if (TempCD->Mem.MemData != NULL)
				free(TempCD->Mem.MemData);
		}
	}
	
	return;
}

static void WriteVGMHeader(UINT8* DstData, const UINT8* SrcData, const UINT32 EOFPos,
						   const UINT32 SampleCount, const UINT32 LoopPos,
						   const UINT32 LoopSmpls)
{
	UINT32 CurPos;
	UINT32 DstPos;
	UINT32 CmdLen;
	UINT32 TempLng;
	
	memcpy(DstData, VGMData, VGMHead.lngDataOffset);	// Copy Header
	memcpy(&DstData[0x18], &SampleCount, 0x04);
	TempLng = LoopPos;
	if (TempLng)
		TempLng -= 0x1C;
	memcpy(&DstData[0x1C], &TempLng, 0x04);
	memcpy(&DstData[0x20], &LoopSmpls, 0x04);
	if (VGMHead.lngDataOffset >= 0x95)
	{
		// rewrite OKIM6258 settings, because the may've changed
		memcpy(&DstData[0x90], &VGMHead.lngHzOKIM6258, 0x04);
		DstData[0x94] = VGMHead.bytOKI6258Flags;
	}
	
	DstPos = EOFPos;
	if (VGMHead.lngGD3Offset)
	{
		CurPos = VGMHead.lngGD3Offset;
		memcpy(&TempLng, &VGMData[CurPos + 0x00], 0x04);
		if (TempLng == FCC_GD3)
		{
			memcpy(&CmdLen, &VGMData[CurPos + 0x08], 0x04);
			CmdLen += 0x0C;
			
			TempLng = DstPos - 0x14;
			memcpy(&DstData[0x14], &TempLng, 0x04);
			memcpy(&DstData[DstPos], &VGMData[CurPos], CmdLen);	// Copy GD3 Tag
			DstPos += CmdLen;
		}
	}
	
	DstDataLen = DstPos;
	TempLng = DstDataLen - 0x04;
	memcpy(&DstData[0x04], &TempLng, 0x04);	// Write EOF Position
	
	return;
}

#ifdef SHOW_PROGRESS
static void PrintMinSec(const UINT32 SamplePos, char* TempStr)
{
	float TimeSec;
	UINT16 TimeMin;
	
	TimeSec = (float)SamplePos / (float)44100.0;
	TimeMin = (UINT16)TimeSec / 60;
	TimeSec -= TimeMin * 60;
	sprintf(TempStr, "%02hu:%05.2f", TimeMin, TimeSec);
	
	return;
}
#endif

static void VGMReadAhead(const UINT32 StartPos, const UINT32 Samples)
{
	// Read ahead to check for neccessary commands
	UINT32 CurPos;
	UINT32 CurSmpl;
	//UINT8 ChipID;
	UINT8 Command;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	UINT32 CmdLen;
	//UINT16 CmdReg;
	//UINT8* TempPnt;
	bool StopVGM;
	
	if (COMPLETE_REWRITE || VGMTOOL_TRIM)
		return;
	
	CurPos = StartPos;
	CurSmpl = 0x00;
	StopVGM = false;
	while(CurPos < VGMHead.lngEOFOffset)
	{
		CmdLen = 0x00;
		Command = VGMData[CurPos + 0x00];
		
		if (Command >= 0x70 && Command <= 0x8F)
		{
			switch(Command & 0xF0)
			{
			case 0x70:
				TempSht = (Command & 0x0F) + 0x01;
				CurSmpl += TempSht;
				break;
			case 0x80:
				TempSht = Command & 0x0F;
				CurSmpl += TempSht;
				break;
			}
			CmdLen = 0x01;
		}
		else
		{
			CmdLen = ReadCommand(0x01);
			if (! CmdLen)
			{
			
			switch(Command)
			{
			case 0x66:	// End Of File
				CmdLen = 0x01;
				StopVGM = true;
				break;
			case 0x62:	// 1/60s delay
				TempSht = 735;
				CurSmpl += TempSht;
				CmdLen = 0x01;
				break;
			case 0x63:	// 1/50s delay
				TempSht = 882;
				CurSmpl += TempSht;
				CmdLen = 0x01;
				break;
			case 0x61:	// xx Sample Delay
				memcpy(&TempSht, &VGMData[CurPos + 0x01], 0x02);
				CurSmpl += TempSht;
				CmdLen = 0x03;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMData[CurPos + 0x02];
				memcpy(&TempLng, &VGMData[CurPos + 0x03], 0x04);
				TempLng &= 0x7FFFFFFF;
				CmdLen = 0x07 + TempLng;
				break;
			case 0xE0:	// Seek to PCM Data Bank Pos
				memcpy(&TempLng, &VGMData[CurPos + 0x01], 0x04);
				CmdLen = 0x05;
				break;
			case 0xC1:	// RF5C68 memory write
			case 0xC2:	// RF5C164 memory write
				memcpy(&TempSht, &VGMData[CurPos + 0x01], 0x02);
				CmdLen = 0x04;
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
					CmdLen = 0x01;
					//StopVGM = true;
					break;
				}
				break;
			}
			
			}
		}
		CurPos += CmdLen;
		if (CurSmpl >= Samples)
			break;
		if (StopVGM)
			break;
	}
	
	return;
}

static void DisplayPlayingNoteWarning(void)
{
	// go through all chips and check for playing notes ...
	return;
}
