// this file generates text lines from chip writes

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "stdtype.h"
#include "stdbool.h"


typedef struct chip_count
{
	UINT32 SN76496;
	UINT32 YM2413;
	UINT32 YM2612;
	UINT32 YM2151;
	UINT32 SegaPCM;
	UINT32 RF5C68;
	UINT32 YM2203;
	UINT32 YM2608;
	UINT32 YM2610;
	UINT32 YM3812;
	UINT32 YM3526;
	UINT32 Y8950;
	UINT32 YMF262;
	UINT32 YMF278B;
	UINT32 YMF271;
	UINT32 YMZ280B;
	UINT32 RF5C164;
	UINT32 PWM;
	UINT32 AY8910;
	UINT32 GBDMG;
	UINT32 NESAPU;
	UINT32 MultiPCM;
	UINT32 UPD7759;
	UINT32 OKIM6258;
	UINT32 OKIM6295;
	UINT32 K051649;
	UINT32 K054539;
	UINT32 HuC6280;
	UINT32 C140;
	UINT32 K053260;
	UINT32 Pokey;
	UINT32 QSound;
	UINT32 SCSP;
	UINT32 WSwan;
	UINT32 VSU;
	UINT32 SAA1099;
	UINT32 ES5503;
	UINT32 ES5506;
	UINT32 X1_010;
	UINT32 C352;
	UINT32 GA20;
	UINT32 MIKEY;
	UINT32 K007232;
} CHIP_CNT;


static const char* ONOFF_STR[0x02] = {"On", "Off"};
static const char* ENABLE_STRS[0x02] = {"Enable", "Disable"};
static const char* NOTE_STRS[0x0C] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

static const char* T6W28_PORT[0x02] = {"L/T", "R/N"};
static const char* SN76496_NOISE_TYPE[0x04] = {"Periodic", "White"};
static const char* SN76496_NOISE_FREQ[0x04] = {"High (6927Hz)", "Med (3463Hz)", "Low (1731Hz)", "Ch 2"};
/*static const char* YM2413_INS_NAMES[0x10] = {"User instrument", "Violin", "Guitar", "Piano", "Flute",
	"Clarinet", "Oboe", "Trumpet", "Organ", "Horn", "Synthesizer", "Harpsichord",
	"Vibraphone", "Synthesizer Bass", "Acoustic Bass", "Electric Guitar"};
static const char* YM2413_RHYTHM_NAMES[0x05] = {"High Hat", "Cymbal", "Tom-Tom", "Snare Drum",
	"Bass Drum"};*/
static const char* YM2151_WAVE_FORM[0x04] = {"Sawtooth", "Square", "Triangle", "Random Noise"};
static const UINT32 dt2_tab[0x04] = {0, 384, 500, 608};
static const char* YMZ280B_MODES[0x04] = {"Unknown", "ADPCM", "PCM8", "PCM16"};

static const char* OPN_LFOFreqs[0x08]= {"3.98", "5.56", "6.02", "6.37", "6.88", "9.63", "48.1", "72.2"};
//static const double PI = 3.1415926535897932;
//static const double PI_HLF = PI / 2;

static const char* PWM_PORTS[0x06] = {"Control Reg", "Cycle Reg", "Left Ch", "Right Ch", "Both Ch", "Invalid"};

static const char* YDT_RAMTYPE[0x04] = {"RAM (1-bit)", "ROM", "RAM (8-bit)", "ROM (invalid)"};

static const char* ADDR_2S_STR[0x02] = {"Low", "High"};
static const char* ADDR_3S_STR[0x03] = {"Low", "Mid", "High"};


#define OPN_TYPE_SSG	0x01	// SSG support
#define OPN_TYPE_LFOPAN	0x02	// OPN type LFO and PAN
#define OPN_TYPE_6CH	0x04	// FM 6CH / 3CH
#define OPN_TYPE_DAC	0x08	// YM2612's DAC device
#define OPN_TYPE_ADPCM	0x10	// two ADPCM units
#define OPN_TYPE_2610	0x20	// bogus flag to differentiate 2608 from 2610

#define OPN_TYPE_YM2203 (OPN_TYPE_SSG)
#define OPN_TYPE_YM2608 (OPN_TYPE_SSG | OPN_TYPE_LFOPAN | OPN_TYPE_6CH | OPN_TYPE_ADPCM)
#define OPN_TYPE_YM2610 (OPN_TYPE_SSG | OPN_TYPE_LFOPAN | OPN_TYPE_6CH | OPN_TYPE_ADPCM | OPN_TYPE_2610)
#define OPN_TYPE_YM2612 (OPN_TYPE_DAC | OPN_TYPE_LFOPAN | OPN_TYPE_6CH)

#define OPN_YM2203 0x00
#define OPN_YM2608 0x01
#define OPN_YM2610 0x02
#define OPN_YM2612 0x03
static const char FMOPN_TYPES[0x04] = {OPN_TYPE_YM2203, OPN_TYPE_YM2608, OPN_TYPE_YM2610,
										OPN_TYPE_YM2612};


#define OPL_TYPE_WAVESEL	0x01	// waveform select
#define OPL_TYPE_ADPCM		0x02	// DELTA-T ADPCM unit
#define OPL_TYPE_KEYBOARD	0x04	// keyboard interface
#define OPL_TYPE_IO			0x08	// I/O port
#define OPL_TYPE_OPL3		0x10	// OPL3 Mode
#define OPL_TYPE_OPL4		0x20	// OPL4 Mode

#define OPL_TYPE_YM3526	(0)
#define OPL_TYPE_YM3812	(OPL_TYPE_WAVESEL)
#define OPL_TYPE_Y8950	(OPL_TYPE_ADPCM | OPL_TYPE_KEYBOARD | OPL_TYPE_IO)
#define OPL_TYPE_YMF262	(OPL_TYPE_YM3812 | OPL_TYPE_OPL3)
#define OPL_TYPE_YMF278	(OPL_TYPE_YMF262 | OPL_TYPE_OPL4)

#define OPL_YM3526	0x00
#define OPL_YM3812	0x01
#define OPL_Y8950	0x02
#define OPL_YMF262	0x03
#define OPL_YMF278	0x04
static const char FMOPL_TYPES[0x05] = {OPL_TYPE_YM3526, OPL_TYPE_YM3812, OPL_TYPE_Y8950,
										OPL_TYPE_YMF262, OPL_TYPE_YMF278};

#define NR10	0x00
#define NR11	0x01
#define NR12	0x02
#define NR13	0x03
#define NR14	0x04
#define NR21	0x06
#define NR22	0x07
#define NR23	0x08
#define NR24	0x09
#define NR30	0x0A
#define NR31	0x0B
#define NR32	0x0C
#define NR33	0x0D
#define NR34	0x0E
#define NR41	0x10
#define NR42	0x11
#define NR43	0x12
#define NR44	0x13
#define NR50	0x14
#define NR51	0x15
#define NR52	0x16
static const float GB_WAVE_DUTY[4] = {12.5f, 25.0f, 50.0f, 75.0f};	// in %
static const char* GB_NOISE_MODE[0x02] = {"Counter", "Consecutive"};

#define APU_WRA0	0x00
#define APU_WRA1	0x01
#define APU_WRA2	0x02
#define APU_WRA3	0x03
#define APU_WRB0	0x04
#define APU_WRB1	0x05
#define APU_WRB2	0x06
#define APU_WRB3	0x07
#define APU_WRC0	0x08
#define APU_WRC2	0x0A
#define APU_WRC3	0x0B
#define APU_WRD0	0x0C
#define APU_WRD2	0x0E
#define APU_WRD3	0x0F
#define APU_WRE0	0x10
#define APU_WRE1	0x11
#define APU_WRE2	0x12
#define APU_WRE3	0x13
#define APU_SMASK	0x15
#define APU_IRQCTRL	0x17
static const int dpcm_clocks[16] = {428, 380, 340, 320, 286, 254, 226, 214,
	190, 160, 142, 128, 106, 85, 72, 54};

#define AUDF1_C		0x00
#define AUDC1_C		0x01
#define AUDF2_C		0x02
#define AUDC2_C		0x03
#define AUDF3_C		0x04
#define AUDC3_C		0x05
#define AUDF4_C		0x06
#define AUDC4_C		0x07
#define AUDCTL_C	0x08
#define STIMER_C	0x09
#define SKREST_C	0x0A
#define POTGO_C		0x0B
#define SEROUT_C	0x0D
#define IRQEN_C		0x0E
#define SKCTL_C		0x0F

static const int okim6258_dividers[4] = {1024, 768, 512, 512};

static const UINT8 okim6295_voltbl[0x10] =
{	0x20, 0x16, 0x10, 0x0B, 0x08, 0x06, 0x04, 0x03,
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const int multipcm_val2chan[] =
{
	 0, 1, 2, 3, 4, 5, 6, -1,
	 7, 8, 9,10,11,12,13, -1,
	14,15,16,17,18,19,20, -1,
	21,22,23,24,25,26,27, -1,
};

static const char* OPX_SYNC_TYPES[0x04] = {"4op FM", "2x 2op FM", "3op FM + PCM", "PCM"};
static const float OPX_PCM_DBVol[0x10] =
{	  0.0f,  2.5f,  6.0f,  8.5f, 12.0f, 14.5f, 18.1f, 20.6f,
	 24.1f, 26.6f, 30.1f, 32.6f, 36.1f, 96.1f, 96.1f, 96.1f};

static const char* ES5503_MODES[0x04] = {"Free-Run", "One-Shot", "Sync", "Swap"};

static const char* K054539_SAMPLE_MODES[0x04] = {"8-bit PCM", "16-bit PCM", "4-bit DPCM", "unknown"};

typedef struct ymf278b_chip
{
	UINT8 smplH[24];	// high bit of the sample ID
} YMF278B_DATA;
typedef struct ymf271_chip
{
	UINT8 group_sync[12];
} YMF271_DATA;
typedef struct okim6295_chip
{
	UINT8 Command;
} OKIM6295_DATA;
typedef struct multipcm_chip
{
	INT8 Slot;
	UINT8 Address;
} MULTIPCM_DATA;
typedef struct upd7759_chip
{
	bool HasROM;
} UPD7759_DATA;
typedef struct es5506_chip
{
	UINT8 Mode;	// 00 = ES5505, 01 - ES5506
	union
	{
		UINT8 d8[4];
		UINT32 d32;
	} latch;
} ES5506_DATA;
typedef struct wswan_chip
{
	bool pcmEnable;
} WSWAN_DATA;


static void opn_write(char* TempStr, UINT8 Mode, UINT8 Port, UINT8 Register,
					  UINT8 Data);
static void rf5cxx_reg_write(char* TempStr, UINT8 Register, UINT8 Data);
static void opl_write(char* TempStr, UINT8 Mode, UINT8 Port, UINT8 Register,
					  UINT8 Data);
static void ay8910_part_write(char* TempStr, UINT8 Register, UINT8 Data);
static void ymf271_write_fm_reg(char* TempStr, UINT8 Register, UINT8 Data);
static void ymf271_write_fm(char* TempStr, UINT8 Port, UINT8 Register, UINT8 Data);
static void FM_ADPCMAWrite(char* TempStr, UINT8 Register, UINT8 Data);
static void YM_DELTAT_ADPCM_Write(char* TempStr, UINT8 Register, UINT8 Data);
static void multipcm_WriteSlot(char* TempStr, INT8 Slot, UINT8 Register, UINT8 Data);


static char WriteStr[0x100];
static char RedirectStr[0x100];
static char* ChipStr;
static CHIP_CNT ChpCnt;
static UINT8 ChpCur;

static YMF278B_DATA CacheYMF278B[0x02];
static YMF271_DATA CacheYMF271[0x02];
static OKIM6295_DATA CacheOKI6295[0x02];
static MULTIPCM_DATA CacheMultiPCM[0x02];
static UPD7759_DATA CacheUPD7759[0x02];
static ES5506_DATA CacheES5506[0x02];
static WSWAN_DATA CacheWSwan[0x02];

void InitChips(UINT32* ChipCounts)
{
	memset(&ChpCnt, 0x00, sizeof(CHIP_CNT));
	memcpy(&ChpCnt, ChipCounts, sizeof(UINT32) * 0x20);
	memset(CacheYMF278B, 0x00, sizeof(YMF278B_DATA) * 0x02);
	memset(CacheYMF271, 0x00, sizeof(YMF271_DATA) * 0x02);
	memset(CacheOKI6295, 0x00, sizeof(OKIM6295_DATA) * 0x02);
	memset(CacheMultiPCM, 0x00, sizeof(MULTIPCM_DATA) * 0x02);
	memset(CacheUPD7759, 0x00, sizeof(UPD7759_DATA) * 0x02);
	memset(CacheES5506, 0x00, sizeof(ES5506_DATA) * 0x02);
	memset(CacheWSwan, 0x00, sizeof(WSWAN_DATA) * 0x02);
	CacheOKI6295[0].Command = 0xFF;
	CacheOKI6295[1].Command = 0xFF;
	ChpCur = 0x00;
	ChipStr = RedirectStr + 0xF0;

	return;
}

void SetChip(UINT8 ChipID)
{
	ChpCur = ChipID;

	return;
}

INLINE const char* OnOff(UINT32 Value)
{
	return ONOFF_STR[Value ? 0x00 : 0x01];
}

INLINE const char* Enable(UINT32 Value)
{
	return ENABLE_STRS[Value ? 0x00 : 0x01];
}

INLINE UINT8 YM2151_Note(UINT8 FNum, UINT8 Block)
{
	UINT8 NoteVal;

	if (FNum > 0xF)
		return 0xFF;	// Invalid FNum
	if (Block > 0x07)
		Block = 0x07;

	if (! ((FNum & 0x03) == 0x03))
	{
		NoteVal = FNum >> 2;
		NoteVal = NoteVal * 3 + (FNum & 0x03);
		NoteVal = 61 + NoteVal;
	}
	else
	{
		NoteVal = 0xFF;
	}

	if (NoteVal == 0xFF)
		return 0xFF;

	return NoteVal + (Block - 0x04) * 12;
}

INLINE UINT32 GetChipName(UINT8 ChipType, const char** RetName)
{
	const char* ChipName;
	UINT32 ChipCnt;

	switch(ChipType)
	{
	case 0x00:	// SN76496 / T6W28
		if (ChpCnt.SN76496 & 0x80000000)
			ChipName = "T6W28";
		else
			ChipName = "SN76496";
		ChipCnt = ChpCnt.SN76496;
		break;
	case 0x01:
		ChipName = "YM2413";
		ChipCnt = ChpCnt.YM2413;
		break;
	case 0x02:
		ChipName = "YM2612";
		ChipCnt = ChpCnt.YM2612;
		break;
	case 0x03:
		ChipName = "YM2151";
		ChipCnt = ChpCnt.YM2151;
		break;
	case 0x04:
		ChipName = "SegaPCM";
		ChipCnt = ChpCnt.SegaPCM;
		break;
	case 0x05:
		ChipName = "RF5C68";
		ChipCnt = ChpCnt.RF5C68;
		break;
	case 0x06:
		ChipName = "YM2203";
		ChipCnt = ChpCnt.YM2203;
		break;
	case 0x07:
		ChipName = "YM2608";
		ChipCnt = ChpCnt.YM2608;
		break;
	case 0x08:	// YM2610
		ChipName = (ChpCnt.YM2610 & 0x80000000) ? "YM2610B" : "YM2610";
		ChipCnt = ChpCnt.YM2610;
		break;
	case 0x09:
		ChipName = "YM3812";
		ChipCnt = ChpCnt.YM3812;
		break;
	case 0x0A:
		ChipName = "YM3526";
		ChipCnt = ChpCnt.YM3526;
		break;
	case 0x0B:
		ChipName = "Y8950";
		ChipCnt = ChpCnt.Y8950;
		break;
	case 0x0C:
		ChipName = "YMF262";
		ChipCnt = ChpCnt.YMF262;
		break;
	case 0x0D:
		ChipName = "YMF278B";
		ChipCnt = ChpCnt.YMF278B;
		break;
	case 0x0E:
		ChipName = "YMF271";
		ChipCnt = ChpCnt.YMF271;
		break;
	case 0x0F:
		ChipName = "YMZ280B";
		ChipCnt = ChpCnt.YMZ280B;
		break;
	case 0x10:
		ChipName = "RF5C164";
		ChipCnt = ChpCnt.RF5C164;
		break;
	case 0x11:
		ChipName = "PWM";
		ChipCnt = ChpCnt.PWM;
		break;
	case 0x12:
		ChipName = "AY8910";
		ChipCnt = ChpCnt.AY8910;
		break;
	case 0x13:
		ChipName = "GB DMG";
		ChipCnt = ChpCnt.GBDMG;
		break;
	case 0x14:
		ChipName = "NES APU";
		ChipCnt = ChpCnt.NESAPU;
		break;
	case 0x15:
		ChipName = "MultiPCM";
		ChipCnt = ChpCnt.MultiPCM;
		break;
	case 0x16:
		ChipName = "UPD7759";
		ChipCnt = ChpCnt.UPD7759;
		break;
	case 0x17:
		ChipName = "OKIM6258";
		ChipCnt = ChpCnt.OKIM6258;
		break;
	case 0x18:
		ChipName = "OKIM6295";
		ChipCnt = ChpCnt.OKIM6295;
		break;
	case 0x19:
		ChipName = "K051649";
		ChipCnt = ChpCnt.K051649;
		break;
	case 0x1A:
		ChipName = "K054539";
		ChipCnt = ChpCnt.K054539;
		break;
	case 0x1B:
		ChipName = "HuC6280";
		ChipCnt = ChpCnt.HuC6280;
		break;
	case 0x1C:
		ChipName = "C140";
		ChipCnt = ChpCnt.C140;
		break;
	case 0x1D:
		ChipName = "K053260";
		ChipCnt = ChpCnt.K053260;
		break;
	case 0x1E:
		ChipName = "Pokey";
		ChipCnt = ChpCnt.Pokey;
		break;
	case 0x1F:
		ChipName = "QSound";
		ChipCnt = ChpCnt.QSound;
		break;
	case 0x20:
		ChipName = "SCSP";
		ChipCnt = ChpCnt.SCSP;
		break;
	case 0x21:
		ChipName = "WSwan";
		ChipCnt = ChpCnt.WSwan;
		break;
	case 0x22:
		ChipName = "VSU";
		ChipCnt = ChpCnt.VSU;
		break;
	case 0x23:
		ChipName = "SAA1099";
		ChipCnt = ChpCnt.SAA1099;
		break;
	case 0x24:
		ChipName = "ES5503";
		ChipCnt = ChpCnt.ES5503;
		break;
	case 0x25:
		ChipName = "ES5506";
		ChipCnt = ChpCnt.ES5506;
		break;
	case 0x26:
		ChipName = "X1-010";
		ChipCnt = ChpCnt.X1_010;
		break;
	case 0x27:
		ChipName = "C352";
		ChipCnt = ChpCnt.C352;
		break;
	case 0x28:
		ChipName = "GA20";
		ChipCnt = ChpCnt.GA20;
		break;
	case 0x2A:
		ChipName = "K007232";
		ChipCnt = ChpCnt.K007232;
		break;
	default:
		ChipName = "Unknown";
		ChipCnt = 0x00;
		break;
	}

	*RetName = ChipName;
	return ChipCnt;
}

INLINE void WriteChipID(UINT8 ChipType)
{
	const char* ChipName;
	UINT32 ChipCnt;

	ChipCnt = GetChipName(ChipType, &ChipName);
	switch(ChipType)
	{
	case 0x00:	// SN76496/T6W28
		if (ChpCnt.SN76496 & 0x80000000)
		{
			sprintf(ChipStr, "%s %s:\t", ChipName, T6W28_PORT[ChpCur]);
			return;
		}
		break;
	}

	ChipCnt &= ~0x80000000;
	if (ChipCnt <= 0x01)
		sprintf(ChipStr, "%s:", ChipName);
	else
		sprintf(ChipStr, "%s #%u:", ChipName, ChpCur);
	if (strlen(ChipStr) < 0x08)
		strcat(ChipStr, "\t");
	strcat(ChipStr, "\t");

	return;
}

void GetFullChipName(char* TempStr, UINT8 ChipType)
{
	const char* ChipName;
	UINT32 ChipCnt;
	UINT8 CurChip;

	CurChip = ChpCur | (ChipType >> 7);
	ChipType &= 0x7F;

	ChipCnt = GetChipName(ChipType, &ChipName);
	switch(ChipType)
	{
	case 0x00:	// SN76496/T6W28
		if (ChpCnt.SN76496 & 0x80000000)
		{
			ChipCnt = 0x01;
		}
		break;
	}

	ChipCnt &= ~0x80000000;
	if (ChipCnt <= 0x01)
		sprintf(TempStr, "%s:", ChipName);
	else
		sprintf(TempStr, "%s #%u:", ChipName, CurChip);

	return;
}

static UINT8 GetLogVolPercent(UINT8 VolLevel, UINT8 Steps_6db, UINT8 Silent)
{
	float TempVol;

	if (VolLevel >= Silent)
		return 0;

	TempVol = (float)pow(2.0, -1.0 * VolLevel / Steps_6db);

	return (UINT8)(100 * TempVol + 0.5f);
}

static UINT8 GetDBTblPercent(UINT8 VolLevel, const float* DBTable, UINT8 Silent)
{
	float TempVol;

	if (VolLevel >= Silent)
		return 0;

	TempVol = (float)pow(2.0, -DBTable[VolLevel] / 6.0f);

	return (UINT8)(100 * TempVol + 0.5f);
}

void GGStereo(char* TempStr, UINT8 Data)
{
	const char CH_CHARS[4] = {'0', '1', '2', 'N'};
	UINT8 CurChn;
	UINT32 StrPos;
	UINT8 ChnEn;

	// Format:
	//	Bit	76543210
	//	L/R	LLLLRRRR
	//	Ch	32103210
	WriteChipID(0x00);
	sprintf(TempStr, "%sGG Stereo: ", ChipStr);
	StrPos = strlen(TempStr);
	for (CurChn = 0x00; CurChn < 0x08; CurChn ++)
	{
		ChnEn = Data & (0x01 << (CurChn ^ 0x04));
		TempStr[StrPos] = ChnEn ? CH_CHARS[CurChn & 0x03] : '-';
		StrPos ++;
	}
	TempStr[StrPos] = 0x00;

	return;
}

void sn76496_write(char* TempStr, UINT8 Command)
{
	UINT8 CurChn;
	UINT8 CurData;

	WriteChipID(0x00);
	if (! (Command & 0x80))
	{
		CurData = Command & 0x7F;
		sprintf(TempStr, "%sData: %02X", ChipStr, CurData);
	}
	else
	{
		CurChn = (Command & 0x60) >> 5;
		CurData = Command & 0x0F;
		switch(Command & 0xF0)
		{
		case 0x80:
		case 0xA0:
		case 0xC0:
			sprintf(TempStr, "%sLatch/Data: Tone Ch %u -> 0x%03X", ChipStr, CurChn, CurData);
			break;
		case 0xE0:
			sprintf(WriteStr, "%s, %s", SN76496_NOISE_TYPE[(Command & 0x04) >> 2],
					SN76496_NOISE_FREQ[Command & 0x03]);

			sprintf(TempStr, "%sNoise Type: %u - %s", ChipStr, CurData, WriteStr);
			break;
		case 0x90:
		case 0xB0:
		case 0xD0:
		case 0xF0:
			sprintf(TempStr, "%sLatch/Data: Volume Ch %u -> 0x%01X = %u%%",
					ChipStr, CurChn, CurData, GetLogVolPercent(CurData & 0x0F, 0x03, 0x0F));
			break;
		}
	}

	return;
}

void ym2413_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	WriteChipID(0x01);
	sprintf(TempStr, "%sReg 0x%02X Data 0x%02X", ChipStr, Register, Data);

	return;
}

static void opn_write(char* TempStr, UINT8 Mode, UINT8 Port, UINT8 Register,
					  UINT8 Data)
{
	UINT16 RegVal;
	UINT8 Channel;
	UINT8 Slot;
	float TempSng;
	UINT8 TempByt;

	RegVal = (Port << 8) | Register;
	if ((RegVal & 0x1F0) == 0x000 && (FMOPN_TYPES[Mode] & OPN_TYPE_SSG))
	{
		ay8910_part_write(WriteStr, Register, Data);
		sprintf(TempStr, "SSG: %s", WriteStr);
	}
	else if ((RegVal & 0x1F0) == 0x020)
	{
		// write a OPN mode register 0x20-0x2F
		switch(Register)
		{
		case 0x21:	// Test
			sprintf(TempStr, "Test Register");
			break;
		case 0x22:	// LFO FREQ (YM2608/YM2610/YM2610B/YM2612)
			if (! (FMOPN_TYPES[Mode] & OPN_TYPE_LFOPAN))
				goto WriteRegData;

			if (Data & 0x08) // LFO enabled ?
				sprintf(TempStr, "Low Frequency Oscillator: %s Hz",
						OPN_LFOFreqs[Data & 0x07]);
			else
				sprintf(TempStr, "Low Frequency Oscillator: Disable");
			break;
		case 0x24:	// timer A High 8
			sprintf(TempStr, "Timer A MSB: %02X", Data);
			break;
		case 0x25:	// timer A Low 2
			sprintf(TempStr, "Timer A LSB: %02X", Data & 0x03);
			break;
		case 0x26:	// timer B
			sprintf(TempStr, "Timer B: %02X", Data);
			break;
		case 0x27:	// mode, timer control
			sprintf(TempStr, "CSM Mode: %s", Enable(Data & 0x80));
			sprintf(TempStr, "%s, 3 Slot Mode: %s", TempStr, Enable(Data & 0x40));

			sprintf(TempStr, "%s, Enable Timer: %c%c, Timer IRQ Enable: %c%c, Reset Timer Status: %c%c",
					TempStr,
					(Data & 0x01) ? 'A' : '-', (Data & 0x02) ? 'B' : '-',
					(Data & 0x04) ? 'A' : '-', (Data & 0x08) ? 'B' : '-',
					(Data & 0x10) ? 'A' : '-', (Data & 0x20) ? 'B' : '-');
			break;
		case 0x28:	// key on / off
			Channel = Data & 0x03;
			if (Channel == 0x03)
			{
				sprintf(TempStr, "Key On/Off: Invalid Channel");
				break;
			}
			if ((Data & 0x04) && (FMOPN_TYPES[Mode] & OPN_TYPE_6CH))
				Channel += 3;

			sprintf(TempStr, "Channel %u Key On/Off: ", Channel);

			sprintf(TempStr, "%sSlot1 %s, Slot2 %s, Slot3 %s, Slot4 %s", TempStr,
					OnOff(Data & 0x10), OnOff(Data & 0x20), OnOff(Data & 0x40),
					OnOff(Data & 0x80));
			break;
		case 0x29:	// SCH,xx,xxx,EN_ZERO,EN_BRDY,EN_EOS,EN_TB,EN_TA
			if (Mode != OPN_YM2608)
				goto WriteRegData;

			sprintf(TempStr, "%s Mode (%u FM channels), IRQ Mask: 0x%02X",
					(Data & 0x80) ? "OPNA" : "OPN", (Data & 0x80) ? 6 : 3, Data & 0x1F);
			break;
		case 0x2A:	// DAC data (YM2612)
			if (! (FMOPN_TYPES[Mode] & OPN_TYPE_DAC))
				goto WriteRegData;

			sprintf(TempStr, "DAC = %02X", Data);
			break;
		case 0x2B:	// DAC Sel (YM2612)
			if (! (FMOPN_TYPES[Mode] & OPN_TYPE_DAC))
				goto WriteRegData;

			// b7 = dac enable
			sprintf(TempStr, "DAC %s", Enable(Data & 0x80));
			break;
		case 0x2C:	// DAC Test  Register (YM2612)
			if (! (FMOPN_TYPES[Mode] & OPN_TYPE_DAC))
				goto WriteRegData;

			sprintf(TempStr, "DAC Test Register: Special DAC Mode %s", Enable(Data & 0x20));
			break;
		default:
			goto WriteRegData;
		}
	}
	else if (Register >= 0x30)
	{
		Channel = (Register & 0x03);
		if (Channel == 0x03)	// 0xX3,0xX7,0xXB,0xXF
		{
			sprintf(TempStr, "Invalid Channel");
			return;
		}
		if (Port)
			Channel += 3;
		Slot = (Register & 0x0C) >> 2;

		switch(Register & 0xF0)
		{
		case 0x30:	// DET , MUL
			sprintf(WriteStr, "Detune: %u", (Data & 0x70) >> 4);
			if (Data & 0x0F)
				TempSng = (float)(Data & 0x0F);
			else
				TempSng = 0.5f;
			sprintf(WriteStr, "%s, Multiple: Freq * %.1f", WriteStr, TempSng);
			break;
		case 0x40:	// TL
			sprintf(WriteStr, "Total Level: 0x%02X = %u%%",
					Data & 0x7F, GetLogVolPercent(Data & 0x7F, 0x08, 0xFF));
			break;
		case 0x50:	// KS, AR
			sprintf(WriteStr, "Attack Rate: %02X, Key Scale: 1 / %01X",
					(Data & 0x1F) >> 0, (~Data & 0xC0) >> 6);
			break;
		case 0x60:	// bit7 = AM ENABLE, DR
			if (FMOPN_TYPES[Mode] & OPN_TYPE_LFOPAN)
				sprintf(WriteStr, "Amplitude Modulation: %s, ", Enable(Data & 0x80));
			else
				strcpy(WriteStr, "");
			sprintf(WriteStr, "%sDecay Rate: %02X", WriteStr,
					Data & 0x1F);
			break;
		case 0x70:	//     SR
			sprintf(WriteStr, "Sustain Rate: %02X",
					Data & 0x1F);
			break;
		case 0x80:	// SL, RR
			sprintf(WriteStr, "Sustain Level: %01X, Release Rate: %01X",
					(Data & 0xF0) >> 4, (Data & 0x0F) >> 0);
			break;
		case 0x90:	// SSG-EG
			//SLOT->ssg  =  v&0x0f;
			//SLOT->ssgn = (v&0x04)>>1; // bit 1 in ssgn = attack
			sprintf(WriteStr, "SSG-EG Flags: Envelope %s, Attack %s, Alternate %s, Hold %s",
					OnOff(Data & 0x08), OnOff(Data & 0x04), OnOff(Data & 0x02),
					OnOff(Data & 0x01));
			break;
		case 0xA0:
			switch(Register & 0x0C)
			{
			case 0x00:	// 0xa0-0xa2 : FNUM1
				/*{
					UINT32 fn = (((UINT32)( (OPN->ST.fn_h)&7))<<8) + v;
					UINT8 blk = OPN->ST.fn_h>>3;
					// keyscale code
					CH->kcode = (blk<<2) | opn_fktable[fn >> 7];
					// phase increment counter
					CH->fc = OPN->fn_table[fn*2]>>(7-blk);

					// store fnum in clear form for LFO PM calculations
					CH->block_fnum = (blk<<11) | fn;
				}*/
				sprintf(WriteStr, "F-Num (set) LSB = %02X", Data);
				break;
			case 0x04:	// 0xa4-0xa6 : FNUM2,BLK
				//OPN->ST.fn_h = v&0x3f;
				sprintf(WriteStr, "F-Num (prepare) MSB = %01X, Octave %u", Data & 0x07,
						(Data & 0x38) >> 3);
				break;
			case 0x08:	// 0xa8-0xaa : 3CH FNUM1
				if (Port)
					goto WriteRegData;

				/*UINT32 fn = (((UINT32)(OPN->SL3.fn_h&7))<<8) + v;
				UINT8 blk = OPN->SL3.fn_h>>3;
				// keyscale code
				OPN->SL3.kcode[c]= (blk<<2) | opn_fktable[fn >> 7];
				// phase increment counter
				OPN->SL3.fc[c] = OPN->fn_table[fn*2]>>(7-blk);
				OPN->SL3.block_fnum[c] = (blk<<11) | fn;
				(OPN->P_CH)[2].SLOT[SLOT1].Incr=-1;*/
				sprintf(WriteStr, "F-Num Op %u (set) LSB = %02X", Channel, Data);
				Channel = 2;
				break;
			case 0x0C:	// 0xac-0xae : 3CH FNUM2,BLK
				if (Port)
					goto WriteRegData;

				//OPN->SL3.fn_h = v&0x3f;
				sprintf(WriteStr, "F-Num Op %u (prepare) MSB = %01X, Octave %u", Channel,
						Data & 0x07, (Data & 0x38) >> 3);
				Channel = 2;
				break;
			}
			break;
		case 0xB0:
			switch(Register & 0x0C)
			{
			case 0x00:	// 0xb0-0xb2 : FB,ALGO
				TempByt = (Data & 0x38) >> 3;
				if (TempByt == 0)
					strcpy(TempStr, "none");
				else if (TempByt < 5)	// 1 = PI/16, 4 = PI/2
					sprintf(TempStr, "PI / %u", 32 >> TempByt);
				else
					sprintf(TempStr, "%u x PI", 1 << (TempByt - 5));
				sprintf(WriteStr, "Feedback: %s, Algorithm: %u", TempStr, Data & 0x07);
				break;
			case 0x04:	// 0xb4-0xb6 : L , R , AMS , PMS (YM2612/YM2610B/YM2610/YM2608)
				if (! (FMOPN_TYPES[Mode] & OPN_TYPE_LFOPAN))
					goto WriteRegData;

				// b0-2 PMS
				//CH->pms = (v & 7) * 32; // CH->pms = PM depth * 32 (index in lfo_pm_table)

				// b4-5 AMS
				//CH->ams = lfo_ams_depth_shift[(v>>4) & 0x03];

				// PAN :  b7 = L, b6 = R
				/*OPN->pan[ c*2   ] = (v & 0x80) ? ~0 : 0;
				OPN->pan[ c*2+1 ] = (v & 0x40) ? ~0 : 0;*/
				sprintf(WriteStr, "PMS: 0x%01X, AMS: 0x%01X, Stereo: %c%c", Data & 0x07,
						(Data & 0x30) >> 4, (Data & 0x80) ? 'L' : '-',
						(Data & 0x40) ? 'R' : '-');
				break;
			default:
				goto WriteRegData;
			}
			break;
		default:
			goto WriteRegData;
		}

		if (Register < 0xA0)
			sprintf(TempStr, "Ch %u Slot %u %s", Channel, Slot, WriteStr);
		else
			sprintf(TempStr, "Ch %u %s", Channel, WriteStr);
	}
	else if (FMOPN_TYPES[Mode] & OPN_TYPE_ADPCM)
	{
		if (FMOPN_TYPES[Mode] & OPN_TYPE_2610)
		{
			// YM2610 Mode
			if ((RegVal & 0x1F0) == 0x010)
			{
				if (RegVal >= 0x1C)
					goto WriteRegData;
				YM_DELTAT_ADPCM_Write(TempStr, Register & 0x0F, Data);
			}
			else if ((RegVal & 0x1F0) >= 0x100 && (RegVal & 0x1F0) < 0x130)
			{
				FM_ADPCMAWrite(TempStr, Register, Data);
			}
			else
			{
				goto WriteRegData;
			}
		}
		else
		{
			// YM2608 Mode
			if ((RegVal & 0x1F0) == 0x010)
			{
				FM_ADPCMAWrite(TempStr, Register & 0x0F, Data);
			}
			else if ((RegVal & 0x1F0) == 0x100)
			{
				YM_DELTAT_ADPCM_Write(TempStr, Register & 0x0F, Data);
			}
			else
			{
				goto WriteRegData;
			}
		}
	}
	else
	{
		goto WriteRegData;
	}

	return;

WriteRegData:

	if (! (FMOPN_TYPES[Mode] & OPN_TYPE_6CH) || ! Port)
		sprintf(TempStr, "Reg 0x%02X Data 0x%02X", Register, Data);
	else
		sprintf(TempStr, "Reg 0x%01X%02X Data 0x%02X", Port, Register, Data);

	return;
}

void ym2612_write(char* TempStr, UINT8 Port, UINT8 Register, UINT8 Data)
{
	WriteChipID(0x02);
	opn_write(RedirectStr, OPN_YM2612, Port, Register, Data);

	sprintf(TempStr, "%s%s", ChipStr, RedirectStr);

	return;
}

void ym2151_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	UINT8 Channel;
	UINT8 Operator;

	WriteChipID(0x03);
	//sprintf(TempStr, "%sReg 0x%02X Data 0x%02X", ChipStr, Register, Data);
	if (Register < 0x20)
	{
		switch(Register)
		{
		case 0x01:	// LFO reset(bit 1), Test Register (other bits)
			sprintf(WriteStr, "Test Register, LFO %s", (Data & 0x02) ? "stopped/reset" : "running");
			break;
		case 0x08:
			sprintf(WriteStr, "Ch %u Key On/Off: M1 %s, M2 %s, C1 %s, C2 %s", Data & 0x07,
					OnOff(Data & 0x08), OnOff(Data & 0x20),
					OnOff(Data & 0x10), OnOff(Data & 0x40));
			break;
		case 0x0F:	// noise mode enable, noise period
			sprintf(WriteStr, "Noise Mode %s, Noise Period 0x%02X",
					Enable(Data & 0x80), Data & 0x1F);
			break;
		case 0x10:	// timer A hi
			sprintf(WriteStr, "Timer A MSB: %02X", Data);
			//chip->timer_A_index = (chip->timer_A_index & 0x003) | (v<<2);
			break;
		case 0x11:	// timer A low
			sprintf(WriteStr, "Timer A LSB: %02X", Data & 0x03);
			//chip->timer_A_index = (chip->timer_A_index & 0x3fc) | (v & 3);
			break;
		case 0x12:	// timer B
			sprintf(WriteStr, "Timer B: %02X", Data);
			//chip->timer_B_index = v;
			break;
		case 0x14:	// CSM, irq flag reset, irq enable, timer start/stop
			sprintf(WriteStr, "CSM Mode: %s", Enable(Data & 0x80));
			//chip->irq_enable = v;	// bit 3-timer B, bit 2-timer A, bit 7 - CSM

			sprintf(WriteStr, "%s, Enable Timer: %c%c, Timer IRQ Enable: %c%c, Reset Timer Status: %c%c",
					WriteStr,
					(Data & 0x01) ? 'A' : '-', (Data & 0x02) ? 'B' : '-',
					(Data & 0x04) ? 'A' : '-', (Data & 0x08) ? 'B' : '-',
					(Data & 0x10) ? 'A' : '-', (Data & 0x20) ? 'B' : '-');
			break;
		case 0x18:	// LFO frequency
			sprintf(WriteStr, "LFO Frequency 0x%02X", Data);
			//chip->lfo_overflow    = ( 1 << ((15-(v>>4))+3) ) * (1<<LFO_SH);
			//chip->lfo_counter_add = 0x10 + (v & 0x0f);
			break;
		case 0x19:	// PMD (bit 7==1) or AMD (bit 7==0)
			if (Data & 0x80)
			{
				sprintf(WriteStr, "LFO Phase Modul. Depth: 0x%02X", Data & 0x7F);
				//chip->pmd = v & 0x7f;
			}
			else
			{
				sprintf(WriteStr, "LFO Amplitude Modul. Depth: 0x%02X", Data & 0x7F);
				//chip->amd = v & 0x7f;
			}
			break;
		case 0x1B:	// CT2, CT1, LFO waveform
			sprintf(WriteStr, "LFO Wave Select: %s, CT1 %s, CT2 %s",
					YM2151_WAVE_FORM[Data & 0x03], OnOff(Data & 0x40), OnOff(Data & 0x80));
			//chip->ct = v >> 6;
			//chip->lfo_wsel = v & 3;
			break;
		default:
			sprintf(WriteStr, "Undocumented register #%02X, Value 0x%02X", Register, Data);
			break;
		}

		sprintf(TempStr, "%s%s", ChipStr, WriteStr);
	}
	else
	{
		Channel = Register & 0x07;
		Operator = (Register & 0x18) >> 3;
		switch(Register & 0xE0)
		{
		case 0x20:
			switch(Register & 0x18)
			{
			case 0x00:	// RL enable, Feedback, Connection
				sprintf(WriteStr, "Stereo: %c%c, Feedback: %u, Algorithm: %u",
						(Data & 0x40) ? 'L' : '-', (Data & 0x80) ? 'R' : '-',
						(Data >> 3) & 0x07, Data & 0x07);
				break;
			case 0x08:	// Key Code
				Data &= 0x7F;
				Operator = YM2151_Note((UINT8)(Data & 0x0F), (UINT8)(Data >> 4));
				if (! Data)
					sprintf(WriteStr, "Key Code: 0x%02X = --", Data);
				else if (Operator != 0xFF)
					sprintf(WriteStr, "Key Code: 0x%02X = %s%d", Data,
							NOTE_STRS[Operator % 12], (Operator / 12) - 2);
				else
					sprintf(WriteStr, "Key Code: 0x%02X = ??", Data);
				/*if (v != op->kc)
				{
					UINT32 kc, kc_channel;

					kc_channel = (v - (v>>2))*64;
					kc_channel += 768;
					kc_channel |= (op->kc_i & 63);

					(op+0)->kc = v;
					(op+0)->kc_i = kc_channel;
					(op+1)->kc = v;
					(op+1)->kc_i = kc_channel;
					(op+2)->kc = v;
					(op+2)->kc_i = kc_channel;
					(op+3)->kc = v;
					(op+3)->kc_i = kc_channel;

					kc = v>>2;

					(op+0)->dt1 = chip->dt1_freq[ (op+0)->dt1_i + kc ];
					(op+0)->freq = ( (chip->freq[ kc_channel + (op+0)->dt2 ] + (op+0)->dt1) * (op+0)->mul ) >> 1;

					(op+1)->dt1 = chip->dt1_freq[ (op+1)->dt1_i + kc ];
					(op+1)->freq = ( (chip->freq[ kc_channel + (op+1)->dt2 ] + (op+1)->dt1) * (op+1)->mul ) >> 1;

					(op+2)->dt1 = chip->dt1_freq[ (op+2)->dt1_i + kc ];
					(op+2)->freq = ( (chip->freq[ kc_channel + (op+2)->dt2 ] + (op+2)->dt1) * (op+2)->mul ) >> 1;

					(op+3)->dt1 = chip->dt1_freq[ (op+3)->dt1_i + kc ];
					(op+3)->freq = ( (chip->freq[ kc_channel + (op+3)->dt2 ] + (op+3)->dt1) * (op+3)->mul ) >> 1;

					refresh_EG( op );
				}*/
				break;

			case 0x10:	// Key Fraction
				Data >>= 2;
				sprintf(WriteStr, "Key Fraction: 0x%02X", Data);
				/*if (v !=  (op->kc_i & 63))
				{
					UINT32 kc_channel;

					kc_channel = v;
					kc_channel |= (op->kc_i & ~63);

					(op+0)->kc_i = kc_channel;
					(op+1)->kc_i = kc_channel;
					(op+2)->kc_i = kc_channel;
					(op+3)->kc_i = kc_channel;

					(op+0)->freq = ( (chip->freq[ kc_channel + (op+0)->dt2 ] + (op+0)->dt1) * (op+0)->mul ) >> 1;
					(op+1)->freq = ( (chip->freq[ kc_channel + (op+1)->dt2 ] + (op+1)->dt1) * (op+1)->mul ) >> 1;
					(op+2)->freq = ( (chip->freq[ kc_channel + (op+2)->dt2 ] + (op+2)->dt1) * (op+2)->mul ) >> 1;
					(op+3)->freq = ( (chip->freq[ kc_channel + (op+3)->dt2 ] + (op+3)->dt1) * (op+3)->mul ) >> 1;
				}*/
				break;

			case 0x18:	// PMS, AMS
				sprintf(WriteStr, "PMS: 0x%02X, AMS: 0x%02X", (Data >> 4) & 0x07, Data & 0x03);
				//op->pms = (v>>4) & 7;
				//op->ams = (v & 3);
				break;
			}
			break;
		case 0x40:		// DT1, MUL
			sprintf(WriteStr, "Detune 1: 0x%02X, Freq Multipler: 0x%02X",
					(Data & 0x70) << 1, Data & 0x0F);
			/*{
				UINT32 olddt1_i = op->dt1_i;
				UINT32 oldmul = op->mul;

				op->dt1_i = (v&0x70)<<1;
				op->mul   = (v&0x0f) ? (v&0x0f)<<1: 1;

				if (olddt1_i != op->dt1_i)
					op->dt1 = chip->dt1_freq[ op->dt1_i + (op->kc>>2) ];

				if ( (olddt1_i != op->dt1_i) || (oldmul != op->mul) )
					op->freq = ( (chip->freq[ op->kc_i + op->dt2 ] + op->dt1) * op->mul ) >> 1;
			}*/
			break;
		case 0x60:		// TL
			sprintf(WriteStr, "Total Level: 0x%02X = %u%%",
					Data & 0x7F, GetLogVolPercent(Data & 0x7F, 0x08, 0xFF));
			//op->tl = (v&0x7f)<<(10-7); // 7bit TL
			break;
		case 0x80:		// KS, AR
			sprintf(WriteStr, "Key Scale: 0x%02X, Attack Rate: 0x%02X",
					0x05 - (Data >> 6), Data & 0x1F);
			/*{
				UINT32 oldks = op->ks;
				UINT32 oldar = op->ar;

				op->ks = 5-(v>>6);
				op->ar = (v&0x1f) ? 32 + ((v&0x1f)<<1) : 0;

				if ( (op->ar != oldar) || (op->ks != oldks) )
				{
					if ((op->ar + (op->kc>>op->ks)) < 32+62)
					{
						op->eg_sh_ar  = eg_rate_shift [op->ar  + (op->kc>>op->ks) ];
						op->eg_sel_ar = eg_rate_select[op->ar  + (op->kc>>op->ks) ];
					}
					else
					{
						op->eg_sh_ar  = 0;
						op->eg_sel_ar = 17*RATE_STEPS;
					}
				}

				if (op->ks != oldks)
				{
					op->eg_sh_d1r = eg_rate_shift [op->d1r + (op->kc>>op->ks) ];
					op->eg_sel_d1r= eg_rate_select[op->d1r + (op->kc>>op->ks) ];
					op->eg_sh_d2r = eg_rate_shift [op->d2r + (op->kc>>op->ks) ];
					op->eg_sel_d2r= eg_rate_select[op->d2r + (op->kc>>op->ks) ];
					op->eg_sh_rr  = eg_rate_shift [op->rr  + (op->kc>>op->ks) ];
					op->eg_sel_rr = eg_rate_select[op->rr  + (op->kc>>op->ks) ];
				}
			}*/
			break;
		case 0xA0:		// LFO AM enable, D1R
			sprintf(WriteStr, "LFO Amplitude Modul. %s, Decay Rate 1: 0x%02X",
					OnOff(Data & 0x80), Data & 0x1F);
			/*op->AMmask = (v&0x80) ? ~0 : 0;
			op->d1r    = (v&0x1f) ? 32 + ((v&0x1f)<<1) : 0;
			op->eg_sh_d1r = eg_rate_shift [op->d1r + (op->kc>>op->ks) ];
			op->eg_sel_d1r= eg_rate_select[op->d1r + (op->kc>>op->ks) ];*/
			break;
		case 0xC0:		// DT2, D2R
			sprintf(WriteStr, "Detune 2: %u, Decay Rate 2: 0x%02X",
					dt2_tab[Data >> 6], Data & 0x1F);
			/*{
				UINT32 olddt2 = op->dt2;
				op->dt2 = dt2_tab[ v>>6 ];
				if (op->dt2 != olddt2)
					op->freq = ( (chip->freq[ op->kc_i + op->dt2 ] + op->dt1) * op->mul ) >> 1;
			}
			op->d2r = (v&0x1f) ? 32 + ((v&0x1f)<<1) : 0;
			op->eg_sh_d2r = eg_rate_shift [op->d2r + (op->kc>>op->ks) ];
			op->eg_sel_d2r= eg_rate_select[op->d2r + (op->kc>>op->ks) ];*/
			break;
		case 0xE0:		// D1L, RR
			sprintf(WriteStr, "D1L: 0x%02X, Release Rate: 0x%02X",
					Data >> 4, 34 + ((Data & 0x0F) << 2));
			/*op->d1l = d1l_tab[ v>>4 ];
			op->rr  = 34 + ((v&0x0f)<<2);
			op->eg_sh_rr  = eg_rate_shift [op->rr  + (op->kc>>op->ks) ];
			op->eg_sel_rr = eg_rate_select[op->rr  + (op->kc>>op->ks) ];*/
			break;
		}

		if (Register < 0x40)
			sprintf(TempStr, "%sCh %u %s", ChipStr, Channel, WriteStr);
		else
			sprintf(TempStr, "%sCh %u Op %u %s", ChipStr, Channel, Operator, WriteStr);
	}

	return;
}

void segapcm_mem_write(char* TempStr, UINT16 Offset, UINT8 Data)
{
	UINT8 Channel;
	UINT16 RelOffset;

	WriteChipID(0x04);

	Offset &= 0x07FF;
	Channel = (Offset >> 3) & 0xF;
	RelOffset = Offset & ~0x0078;

	//sprintf(TempStr, "SegaPCM:\tOffset 0x%04X Data 0x%02X", Offset, Data);
	switch(RelOffset)
	{
	case 0x86:
		sprintf(WriteStr, "%s", Enable(~Data & 0x01));
		break;
	case 0x04:
	case 0x05:
		sprintf(WriteStr, "Loop Address %s 0x%02X", ADDR_2S_STR[RelOffset & 0x01], Data);
		break;
	case 0x84:
	case 0x85:
		sprintf(WriteStr, "Current Address %s 0x%02X", ADDR_2S_STR[RelOffset & 0x01], Data);
		break;
	case 0x06:
		sprintf(WriteStr, "End Address 0x%02X", Data);
		break;
	case 0x07:
		sprintf(WriteStr, "Sample Delta Time 0x%02X", Data);
		break;
	case 0x02:
		Data &= 0x7F;
		sprintf(WriteStr, "Volume L 0x%02X = %u%%", Data, 100 * Data / 0x7F);
		break;
	case 0x03:
		Data &= 0x7F;
		sprintf(WriteStr, "Volume R 0x%02X = %u%%", Data, 100 * Data / 0x7F);
		break;
	default:
		sprintf(WriteStr, "Write Offset 0x%03X, Data 0x%02X", Offset, Data);
		break;
	}

	sprintf(TempStr, "%sCh %u %s", ChipStr, Channel, WriteStr);

	return;
}

static void rf5cxx_reg_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	UINT8 CurChn;
	UINT32 StrPos;
	bool ChnEn;

	switch(Register)
	{
	case 0x00:	// Evelope
		sprintf(WriteStr, "Envelope %.2f %%", 100 * Data / 255.0f);
		break;
	case 0x01:	// Pan
		sprintf(WriteStr, "Pan: VolL %u %%, VolR %u %%",
				100 * (Data & 0x0F) / 0x0F, 100 * (Data >> 4) / 0x0F);
		break;
	case 0x02:	// Frequency Step (LB)
		sprintf(WriteStr, "Freq. Step Low: 0x%02X", Data);
		break;
	case 0x03:	// Frequency Step (HB)
		sprintf(WriteStr, "Freq. Step High: 0x%02X", Data);
		break;
	case 0x04:	// Loop Address Low
		sprintf(WriteStr, "Loop Address Low 0x%02X", Data);
		break;
	case 0x05:	// Loop Address High
		sprintf(WriteStr, "Loop Address High 0x%02X", Data);
		break;
	case 0x06:	// Start Address
		sprintf(WriteStr, "Start Address 0x%04X", Data << 8);
		break;
	case 0x07:	// Control Register
		sprintf(WriteStr, "Chip %s", Enable((Data & 0x80) >> 7));
		if (Data & 0x40)
			sprintf(WriteStr, "%s, Select Channel %u", WriteStr, Data & 0x07);
		else
			sprintf(WriteStr, "%s, Select Bank 0x%X (Memory Base 0x%04X)",
					WriteStr, Data & 0x0F, (Data & 0x0F) << 12);
		break;
	case 0x08:	// Sound On/Off
		sprintf(WriteStr, "Channel Enable: ");
		StrPos = strlen(WriteStr);
		for (CurChn = 0x00; CurChn < 0x08; CurChn ++)
		{
			ChnEn = ! (Data & (0x01 << CurChn));
			WriteStr[StrPos] = ChnEn ? ('0' + CurChn) : '-';
			StrPos ++;
		}
		WriteStr[StrPos] = 0x00;
		break;
	default:
		sprintf(WriteStr, "Reg 0x%02X, Data 0x%02X", Register, Data);
		break;
	}

	sprintf(TempStr, "%s%s", ChipStr, WriteStr);
	//sprintf(TempStr, "%sReg 0x%02X Data 0x%02X", ChipStr, Register, Data);

	return;
}

void rf5c68_reg_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	WriteChipID(0x05);
	rf5cxx_reg_write(TempStr, Register, Data);

	return;
}

void rf5c68_mem_write(char* TempStr, UINT16 Offset, UINT8 Data)
{
	WriteChipID(0x05);
	sprintf(TempStr, "%sMem 0x%04X Data 0x%02X", ChipStr, Offset, Data);

	return;
}

void ym2203_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	WriteChipID(0x06);
	opn_write(RedirectStr, OPN_YM2203, 0x00, Register, Data);

	sprintf(TempStr, "%s%s", ChipStr, RedirectStr);

	return;
}

void ym2608_write(char* TempStr, UINT8 Port, UINT8 Register, UINT8 Data)
{
	WriteChipID(0x07);
	opn_write(RedirectStr, OPN_YM2608, Port, Register, Data);

	sprintf(TempStr, "%s%s", ChipStr, RedirectStr);

	return;
}

void ym2610_write(char* TempStr, UINT8 Port, UINT8 Register, UINT8 Data)
{
	WriteChipID(0x08);
	opn_write(RedirectStr, OPN_YM2610, Port, Register, Data);

	sprintf(TempStr, "%s%s", ChipStr, RedirectStr);

	return;
}

static void opl_write(char* TempStr, UINT8 Mode, UINT8 Port, UINT8 Register,
					  UINT8 Data)
{
	UINT8 Channel;
	UINT8 Operator;

	if (Register < 0x20)
	{
		// 00-1f:control
		switch(Register & 0x1F)
		{
		case 0x01:	// waveform select enable
			if (! (FMOPL_TYPES[Mode] & OPL_TYPE_WAVESEL) || Port)
				goto WriteRegData;

			sprintf(TempStr, "Waveform Select: %s", Enable(Data & 0x20));
			break;
		case 0x02:	// Timer 1
			if (Port)
				goto WriteRegData;

			sprintf(TempStr, "Timer 1 = %u", (0x100 - Data) * 0x04);
			break;
		case 0x03:	// Timer 2
			if (Port)
				goto WriteRegData;

			sprintf(TempStr, "Timer 2 = %u", (0x100 - Data) * 0x10);
			break;
		case 0x04:	// IRQ clear / mask and Timer enable
			switch(Port)
			{
			case 0x00:
				if (Data & 0x80)
				{	// IRQ flag clear
					sprintf(TempStr, "IRQ Flag Clear");
				}
				else
				{	// set IRQ mask ,timer enable
					sprintf(TempStr, "Set IRQ Mask: 0x%02X", Data >> 3);
					sprintf(TempStr, "%s, Timer 1: %s", TempStr, Enable(Data & 0x01));
					sprintf(TempStr, "%s, Timer 2: %s", TempStr, Enable(Data & 0x02));

					// IRQRST,T1MSK,t2MSK,EOSMSK,BRMSK,x,ST2,ST1
				}
				break;
			case 0x01:
				if (! (FMOPL_TYPES[Mode] & OPL_TYPE_OPL3))
					goto WriteRegData;

				sprintf(TempStr, "4-Op Mode: ");
				sprintf(TempStr, "%sCh 0-3: %s, ", TempStr, Enable(Data & 0x01));
				sprintf(TempStr, "%sCh 1-4: %s, ", TempStr, Enable(Data & 0x02));
				sprintf(TempStr, "%sCh 2-5: %s, ", TempStr, Enable(Data & 0x04));
				sprintf(TempStr, "%sCh 9-12: %s, ", TempStr, Enable(Data & 0x08));
				sprintf(TempStr, "%sCh 10-13: %s, ", TempStr, Enable(Data & 0x10));
				sprintf(TempStr, "%sCh 11-14: %s", TempStr, Enable(Data & 0x20));
				break;
			}
			break;
		case 0x05:	// OPL3/OPL4 Mode Enable
			if (! (FMOPL_TYPES[Mode] & OPL_TYPE_OPL3) || ! Port)
				goto WriteRegData;

			sprintf(TempStr, "OPL3 Mode: %s", Enable(Data & 0x01));
			if (FMOPL_TYPES[Mode] & OPL_TYPE_OPL4)
				sprintf(TempStr, "%s, OPL4 Mode: %s", TempStr, Enable(Data & 0x02));
			break;
		case 0x06:		// Key Board OUT
			if (! (FMOPL_TYPES[Mode] & OPL_TYPE_KEYBOARD) || Port)
				goto WriteRegData;

			sprintf(TempStr, "Key Board Write: 0x%02X", Data);
			break;
		case 0x07:	// DELTA-T control 1 : START,REC,MEMDATA,REPT,SPOFF,x,x,RST
			if (! (FMOPL_TYPES[Mode] & OPL_TYPE_ADPCM) || Port)
				goto WriteRegData;

			//sprintf(TempStr, "DELTA-T Control 1: 0x%02X", Data);
			YM_DELTAT_ADPCM_Write(TempStr, Register - 0x07, Data);
			break;
		case 0x08:	// MODE,DELTA-T control 2 : CSM,NOTESEL,x,x,smpl,da/ad,64k,rom
			if (Port)
				goto WriteRegData;

			sprintf(TempStr, "CSM %s, Note Select: %s", OnOff(Data & 0x80), OnOff(Data & 0x40));

			if (! (FMOPL_TYPES[Mode] & OPL_TYPE_ADPCM))
				break;

			strcat(TempStr, ", ");
			YM_DELTAT_ADPCM_Write(TempStr + strlen(TempStr), Register - 0x07, (Data & 0x0F) | 0xC0);
			break;
		case 0x09:		// START ADD
		case 0x0a:
		case 0x0b:		// STOP ADD
		case 0x0c:
		case 0x0d:		// PRESCALE
		case 0x0e:
		case 0x0f:		// ADPCM data write
		case 0x10: 		// DELTA-N
		case 0x11: 		// DELTA-N
		case 0x12: 		// ADPCM volume
			if (! (FMOPL_TYPES[Mode] & OPL_TYPE_ADPCM) || Port)
				goto WriteRegData;

			YM_DELTAT_ADPCM_Write(TempStr, Register - 0x07, Data);
			break;
		case 0x15:		// DAC data high 8 bits (F7,F6...F2)
			if (! (FMOPL_TYPES[Mode] & OPL_TYPE_ADPCM) || Port)
				goto WriteRegData;

			sprintf(TempStr, "DAC Write High: 0x%02X", Data);
			break;
		case 0x16:		// DAC data low 2 bits (F1, F0 in bits 7,6)
			if (! (FMOPL_TYPES[Mode] & OPL_TYPE_ADPCM) || Port)
				goto WriteRegData;

			sprintf(TempStr, "DAC Write Low: 0x%02X", Data);
			break;
		case 0x17:		// DAC data shift (S2,S1,S0 in bits 2,1,0)
			if (! (FMOPL_TYPES[Mode] & OPL_TYPE_ADPCM) || Port)
				goto WriteRegData;

			sprintf(TempStr, "DAC Write Data Shift: 0x%02X", Data);
			break;
		case 0x18:		// I/O CTRL (Direction)
			if (! (FMOPL_TYPES[Mode] & OPL_TYPE_IO) || Port)
				goto WriteRegData;

			sprintf(TempStr, "I/O Direction: 0x%02X", Data & 0x0F);
			break;
		case 0x19:		// I/O DATA
			if (! (FMOPL_TYPES[Mode] & OPL_TYPE_IO) || Port)
				goto WriteRegData;

			sprintf(TempStr, "I/O Data/Latch: 0x%02X", Data);
			break;
		default:
			goto WriteRegData;
		}
	}
	else
	{
		Channel = ((Register & 0x18) >> 3) * 3 + ((Register & 0x07) % 3);
		Operator = (Register & 0x07) / 3;
		switch(Register & 0xE0)
		{
		case 0x20:	// am ON, vib ON, ksr, eg_type, mul
			sprintf(WriteStr, "AM: %s, Vibrato: %s, KSR: %s, EG Type: %u, Freq Multipler: %u",
					OnOff(Data & 0x80), OnOff(Data & 0x40), OnOff(Data & 0x20),
					(Data & 0x10) >> 4, Data & 0x0F);
			break;
		case 0x40:
			sprintf(WriteStr, "Key Scaling: %01X, Total Level: 0x%02X = %u%%",
					(Data & 0xC0) >> 6, Data & 0x3F, GetLogVolPercent(Data & 0x3F, 0x08, 0xFF));
			break;
		case 0x60:
			sprintf(WriteStr, "Attack Rate: %01X, Decay Rate: %01X",
					(Data & 0xF0) >> 4, (Data & 0x0F) >> 0);
			break;
		case 0x80:
			sprintf(WriteStr, "Sustain Level: %01X, Release Rate: %01X",
					(Data & 0xF0) >> 4, (Data & 0x0F) >> 0);
			break;
		case 0xA0:
			if (Register == 0xBD)			// am depth, vibrato depth, r,bd,sd,tom,tc,hh
			{
				sprintf(WriteStr, "AM Depth: %.1f, Vibrato Depth: %u cent, Rhythm Mode %s",
						(Data & 0x80) ? 4.8 : 1.0, (Data & 0x40) ? 14 : 7, Enable(Data & 0x20));

				if (Data & 0x20)
				{
					sprintf(WriteStr, "%s, BD %s, SD %s, TOM %s, CYM %s, HH %s", WriteStr,
							OnOff(Data & 0x10), OnOff(Data & 0x08), OnOff(Data & 0x04),
							OnOff(Data & 0x02), OnOff(Data & 0x01));
				}
				break;
			}

			// keyon,block,fnum
			if ((Register & 0x0F) > 8)
				goto WriteRegData;

			if (! (Register & 0x10))
			{	// a0-a8
				sprintf(WriteStr, "F-Num LSB = %02X", Data);
			}
			else
			{	// b0-b8
				sprintf(WriteStr, "F-Num MSB = %01X, Octave %u, Key %s", Data & 0x03,
						(Data & 0x1C) >> 2, OnOff(Data & 0x20));
			}
			break;
		case 0xC0:
			// FB,C
			if ((Register & 0x1F) > 8)
				goto WriteRegData;

			sprintf(WriteStr, "Algorithm: %s, Feedback %u", (Data & 0x01) ? "AM" : "FM",
					(Data & 0x0E) >> 1);
			break;
		case 0xE0: // waveform select
			// TODO: - ignore write if Wave Select is not enabled in test register (OPL2 only)
			if (! (FMOPL_TYPES[Mode] & OPL_TYPE_WAVESEL))
				goto WriteRegData;

			if (! (FMOPL_TYPES[Mode] & OPL_TYPE_OPL3))
				Data &= 0x03;	// only 2 wave forms on OPL2
			sprintf(WriteStr, "Waveform Select: %u", Data & 0x07);

			break;
		default:
			goto WriteRegData;
		}

		if (Register < 0xA0 || Register >= 0xE0)
			sprintf(TempStr, "Ch %u Op %u %s", Channel, Operator, WriteStr);
		else if (Register != 0xBD)
			sprintf(TempStr, "Ch %u %s", Register & 0x0F, WriteStr);
		else
			sprintf(TempStr, "%s", WriteStr);
	}

	return;

WriteRegData:

	if (! (FMOPL_TYPES[Mode] & OPL_TYPE_OPL3) || ! Port)
		sprintf(TempStr, "Reg 0x%02X Data 0x%02X", Register, Data);
	else
		sprintf(TempStr, "Reg 0x%01X%02X Data 0x%02X", Port, Register, Data);

	return;
}

void ym3812_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	WriteChipID(0x09);
	opl_write(RedirectStr, OPL_YM3812, 0x00, Register, Data);

	sprintf(TempStr, "%s%s", ChipStr, RedirectStr);

	return;
}

void ym3526_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	WriteChipID(0x0A);
	opl_write(RedirectStr, OPL_YM3526, 0x00, Register, Data);

	sprintf(TempStr, "%s%s", ChipStr, RedirectStr);

	return;
}

void y8950_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	WriteChipID(0x0B);
	opl_write(RedirectStr, OPL_Y8950, 0x00, Register, Data);

	sprintf(TempStr, "%s%s", ChipStr, RedirectStr);

	return;
}

void ymf262_write(char* TempStr, UINT8 Port, UINT8 Register, UINT8 Data)
{
	WriteChipID(0x0C);
	opl_write(RedirectStr, OPL_YMF262, Port, Register, Data);

	sprintf(TempStr, "%sPort %X %s", ChipStr, Port, RedirectStr);
	//sprintf(TempStr, "YMF262:\tReg 0x%02X Data 0x%02X", Register | (Port << 8), Data);

	return;
}

void ymz280b_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	UINT8 Voice;
	UINT8 Addr;

	WriteChipID(0x0F);
	if (Register < 0x80)
	{
		Voice = (Register >> 2) & 0x07;
		Addr = 0x3 - (Register >> 5);

		switch(Register & 0xE3)
		{
		case 0x00:		// pitch low 8 bits
			sprintf(WriteStr, "Pitch Low 0x%02X", Data);
			break;
		case 0x01:		// pitch upper 1 bit, loop, key on, mode
			sprintf(WriteStr, "Pitch High 0x%01X, Looping %s, Mode %s, Key %s",
					Data & 0x01, OnOff(Data & 0x10), YMZ280B_MODES[(Data & 0x60) >> 5],
					OnOff(Data & 0x80));
			break;
		case 0x02:		// total level
			sprintf(WriteStr, "Total Level %.0f %%", 100 * Data / 255.0f);
			break;
		case 0x03:		// pan
			sprintf(WriteStr, "Pan 0x%02X = %d%%", Data, 200 * (Data - 0x08) / 0x0F);
			break;
		case 0x20:		// start address high
		case 0x40:		// start address middle
		case 0x60:		// start address low
			sprintf(WriteStr, "Start Address %s 0x%02X", ADDR_3S_STR[Addr], Data);
			break;
		case 0x21:		// loop start address high
		case 0x41:		// loop start address middle
		case 0x61:		// loop start address low
			sprintf(WriteStr, "Loop Start Address %s 0x%02X", ADDR_3S_STR[Addr], Data);
			break;
		case 0x22:		// loop end address high
		case 0x42:		// loop end address middle
		case 0x62:		// loop end address low
			sprintf(WriteStr, "Loop End Address %s 0x%02X", ADDR_3S_STR[Addr], Data);
			break;
		case 0x23:		// stop address high
		case 0x43:		// stop address middle
		case 0x63:		// stop address low
			sprintf(WriteStr, "Stop Address %s 0x%02X", ADDR_3S_STR[Addr], Data);
			break;
		default:
			sprintf(WriteStr, "Unknown Register Write %02X = %02X", Register, Data);
			break;
		}
		sprintf(TempStr, "%sCh %u %s", ChipStr, Voice, WriteStr);
	}
	else	// upper registers are special
	{
		Addr = 0x02 - (Register & 0x03);
		switch(Register)
		{
		case 0x84:		// ROM readback / RAM write (high)
		case 0x85:		// ROM readback / RAM write (med)
		case 0x86:		// ROM readback / RAM write (low)
			sprintf(WriteStr, "ROM Readback / RAM Write Address %s 0x%02X",
					ADDR_3S_STR[Addr], Data);
			break;
		case 0x87:		// RAM write
			sprintf(WriteStr, "RAM Write: 0x%02X", Data);
			break;
		case 0xFE:		// IRQ mask
			sprintf(WriteStr, "IRQ Mask 0x%02X", Data);
			break;
		case 0xFF:		// IRQ enable, test, etc
			sprintf(WriteStr, "IRQ %s", Enable(Data & 0x10));
			sprintf(WriteStr, "%s, KeyOn %s", WriteStr, Enable(Data & 0x80));
			break;
		default:
			sprintf(WriteStr, "Unknown Register Write %02X = %02X", Register, Data);
			break;
		}
		sprintf(TempStr, "%s%s", ChipStr, WriteStr);
	}

	return;
}

void ymf278b_write(char* TempStr, UINT8 Port, UINT8 Register, UINT8 Data)
{
	YMF278B_DATA* TempYMF = &CacheYMF278B[ChpCur];
	UINT8 Channel;
	UINT8 ChnReg;
	UINT16 TempSht;
	INT8 TempSByt;

	WriteChipID(0x0D);

	if (Port <= 0x01)
	{
		opl_write(RedirectStr, OPL_YMF278, Port, Register, Data);

		sprintf(TempStr, "%sFM Port %X %s", ChipStr, Port, RedirectStr);
		return;
	}
	else if (Port == 0x02)
	{
		if (Register >= 0x08 && Register <= 0xF7)
		{
			Channel = (Register - 0x08) % 24;
			ChnReg = (Register - 0x08) / 24;

			switch(ChnReg)
			{
			case 0x00:	// sample ID (bits 0-7) / load sample
				TempSht = (TempYMF->smplH[Channel] << 8) | (Data << 0);
				sprintf(WriteStr, "Load Sample 0x%02X", TempSht);
				break;
			case 0x01:	// f-num (bits 0-6), sample ID (bit 8)
				TempYMF->smplH[Channel] = Data & 0x01;
				sprintf(WriteStr, "F-Num LSB = 0x%02X, Sample MSB = 0x%02X",
						(Data & 0xFE) >> 1, Data & 0x01);
				break;
			case 0x02:	// octave, pseudo-reverb, f-num (bits 7-9)
				TempSByt = (Data & 0xF0) >> 4;
				if (TempSByt & 0x08)
					TempSByt |= 0xF0;
				sprintf(WriteStr, "Octave %d, Pseudo-Reverb: %s, F-Num MSB = %01X",
						TempSByt, OnOff(Data & 0x08), Data & 0x07);
				break;
			case 0x03:	// total level
				sprintf(WriteStr, "Total Level: 0x%02X = %u%%, TL %s",
						(Data & 0xFE) >> 1, GetLogVolPercent((Data & 0xFE) >> 1, 0x10, 0x7F),
						(Data & 0x01) ? "Set" : "Interpolate");
				break;
			case 0x04:		// key on, damp, LFO reset, output pin, pan
				TempSByt = (Data & 0x0F) >> 0;
				if (TempSByt & 0x08)
					TempSByt |= 0xF0;
				sprintf(WriteStr, "Key %s, Damping %s, LFO Reset %s, Output Pin: %s, Pan 0x%02X = %d%%",
						OnOff(Data & 0x80), OnOff(Data & 0x40), OnOff(Data & 0x20),
						(Data & 0x10) ? "D02" : "D01", (Data & 0x0F) >> 0, 100 * TempSByt / 0x07);
				break;
			case 0x05:	// LFO speed, vibrato depth
				sprintf(WriteStr, "LFO Speed: 0x%01X, Vibrato Depth: 0x%01X",
						(Data & 0x38) >> 3, (Data & 0x07) >> 0);
				break;
			case 0x06:	// AR, D1R
				sprintf(WriteStr, "Attack Rate: 0x%01X, Decay Rate: 0x%01X",
						(Data & 0xF0) >> 4, (Data & 0x0F) >> 0);
				break;
			case 0x07:	// D1L, D2R
				sprintf(WriteStr, "Sustain Level: 0x%01X, Sustain Rate: 0x%01X",
						(Data & 0xF0) >> 4, (Data & 0x0F) >> 0);
				break;
			case 0x08:	// RC, RR
				if ((Data & 0xF0) == 0xF0)
					sprintf(WriteStr, "Rate Correction: %s, Release Rate: 0x%01X",
							"off", (Data & 0x0F) >> 0);
				else
					sprintf(WriteStr, "Rate Correction: %u, Release Rate: 0x%01X",
							(Data & 0xF0) >> 4, (Data & 0x0F) >> 0);
				break;
			case 0x09:	// AM
				sprintf(WriteStr, "Amplitude Modulation: 0x%01X", (Data & 0x07) >> 0);
				break;
			default:
				sprintf(WriteStr, "Unknown Register Write %02X = %02X", Register, Data);
				break;
			}
			sprintf(TempStr, "%sCh %u %s", ChipStr, Channel, WriteStr);
		}
		else	// upper registers are special
		{
			switch(Register)
			{
			case 0x00:	// test
			case 0x01:	// test
				sprintf(WriteStr, "Test Register 0x%02X = 0x%02X", Register, Data);
				break;
			case 0x02:	// memory access mode
				sprintf(WriteStr, "Wave Table Offset: 0x%06X, Memory Type %s, Access Mode: %s",
						((Data & 0x1C) >> 2) * 0x080000,
						(Data & 0x02) ? "SRAM+ROM" : "ROM",
						(Data & 0x01) ? "CPU read/write" : "normal");
				break;
			case 0x03:	// memory address (high)
			case 0x04:	// memory address (med)
			case 0x05:	// memory address (low)
				TempSht = 0x05 - Register;
				sprintf(WriteStr, "Set Memory Address %s = 0x%02X", ADDR_3S_STR[TempSht], Data);
				break;
			case 0x06:	// memory read/write
				sprintf(WriteStr, "Memory Write: Data 0x%02X", Data);
				break;
			case 0xF8:	// FM Mix Control
			case 0xF9:	// PCM Mix Control
				sprintf(WriteStr, "%s Mix L: 0x%01X = %u%%, 0x%01X = %u%%",
						(Register == 0xF9) ? "PCM" : "FM",
						(Data & 0x38) >> 3, GetLogVolPercent((Data & 0x38) >> 3, 0x02, 0xFF),
						(Data & 0x07) >> 0, GetLogVolPercent((Data & 0x07) >> 0, 0x02, 0xFF));
				break;
			default:
				sprintf(WriteStr, "Unknown Register Write %02X = %02X", Register, Data);
				break;
			}
			sprintf(TempStr, "%s%s", ChipStr, WriteStr);
		}
	}
	else
	{
		sprintf(TempStr, "%sPort %x Reg 0x%02X Data 0x%02X", ChipStr, Port, Register, Data);
	}

	return;
}

/*void ymf271_write(char* TempStr, UINT8 Port, UINT8 Register, UINT8 Data)
{
	WriteChipID(0x0E);

	sprintf(TempStr, "%sPort %x Reg 0x%02X Data 0x%02X", ChipStr, Port, Register, Data);

	return;
}*/

void rf5c164_reg_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	WriteChipID(0x10);
	rf5cxx_reg_write(TempStr, Register, Data);

	return;
}

void rf5c164_mem_write(char* TempStr, UINT16 Offset, UINT8 Data)
{
	WriteChipID(0x10);
	sprintf(TempStr, "%sMem 0x%04X Data 0x%02X", ChipStr, Offset, Data);

	return;
}

static void ay8910_part_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	UINT8 CurChn;
	UINT32 StrPos;
	UINT8 ChnEn;

	switch(Register)
	{
	case 0x00:	// AY_AFINE
	case 0x02:	// AY_BFINE
	case 0x04:	// AY_CFINE
		sprintf(TempStr, "Chn %c Freq. Fine: 0x%02X", 'A' + (Register >> 1), Data);
		break;
	case 0x01:	// AY_ACOARSE
	case 0x03:	// AY_BCOARSE
	case 0x05:	// AY_CCOARSE
		sprintf(TempStr, "Chn %c Freq. Coarse: 0x%01X", 'A' + (Register >> 1), Data & 0x0F);
		break;
	case 0x06:	// AY_NOISEPER
		sprintf(TempStr, "Noise Period: 0x%02X", Data & 0x1F);
		break;
	case 0x07:	// AY_ENABLE
		sprintf(TempStr, "Enable: Channel ");
		StrPos = strlen(TempStr);
		for (CurChn = 0; CurChn < 3; CurChn ++)
		{
			ChnEn = ~Data & (0x01 << (CurChn + 0));
			TempStr[StrPos] = ChnEn ? ('A' + CurChn) : '-';
			StrPos ++;
		}
		TempStr[StrPos] = 0x00;

		sprintf(TempStr, "%s, Noise ", TempStr);
		StrPos = strlen(TempStr);
		for (CurChn = 0; CurChn < 3; CurChn ++)
		{
			ChnEn = ~Data & (0x01 << (CurChn + 3));
			TempStr[StrPos] = ChnEn ? ('A' + CurChn) : '-';
			StrPos ++;
		}
		TempStr[StrPos] = 0x00;

		sprintf(TempStr, "%s, Port ", TempStr);
		StrPos = strlen(TempStr);
		// Ports are just INPUT (off) or OUTPUT (on) mode
		for (CurChn = 0; CurChn < 2; CurChn ++)
		{
			ChnEn = Data & (0x01 << (CurChn + 6));
			TempStr[StrPos] = ChnEn ? 'O' : 'I';
			StrPos ++;
		}
		TempStr[StrPos] = 0x00;
		break;
	case 0x08:	// AY_AVOL
	case 0x09:	// AY_BVOL
	case 0x0A:	// AY_CVOL
		sprintf(TempStr, "Chn %c Volume %u%%, Envelope Mode: %u", 'A' + (Register & 0x03),
				100 * (Data & 0x0F) / 0x0F, (Data & 0x10) >> 1);
		break;
	case 0x0B:	// AY_EFINE
		sprintf(TempStr, "Envelope Freq. Fine: 0x%02X", Data);
		break;
	case 0x0C:	// AY_ECOARSE
		sprintf(TempStr, "Envelope Freq. Coarse: 0x%02X", Data);
		break;
	case 0x0D:	// AY_ESHAPE
		sprintf(TempStr, "Envelope Shape: 0x%02X", Data);
		break;
	case 0x0E:	// AY_PORTA
		sprintf(TempStr, "Write Port A: 0x%02X", Data);
		break;
	case 0x0F:	// AY_PORTB
		sprintf(TempStr, "Write Port B: 0x%02X", Data);
		break;
	default:
		sprintf(TempStr, "Reg 0x%02X, Data 0x%02X", Register, Data);
		break;
	}

	return;
}

void ay8910_reg_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	WriteChipID(0x12);

	ay8910_part_write(RedirectStr, Register, Data);

	sprintf(TempStr, "%s%s", ChipStr, RedirectStr);

	return;
}

static const char *ay8910_stereo_names [4] = { "off", "left", "right", "center" };
void ay8910_stereo_mask_write(char* TempStr, UINT8 Data)
{
	WriteChipID((Data & 0x40)? 0x06: 0x12);
	sprintf(TempStr, "%sSet %sStereo Mask: Ch A %s, Ch B %s, Ch C %s",
		ChipStr,
		(Data & 0x40)? "SSG ": "",
		ay8910_stereo_names [(Data >>0) &3],
		ay8910_stereo_names [(Data >>2) &3],
		ay8910_stereo_names [(Data >>4) &3]
	);
	return;
}

void pwm_write(char* TempStr, UINT16 Port, UINT16 Data)
{
	UINT8 PortVal;

	WriteChipID(0x11);

	PortVal = (Port > 0x05) ? 0x05 : Port;
	sprintf(WriteStr, "%s, Data 0x%03X", PWM_PORTS[PortVal], Data);

	sprintf(TempStr, "%s%s", ChipStr, WriteStr);

	return;
}

static void ymf271_write_fm_reg(char* TempStr, UINT8 Register, UINT8 Data)
{
	switch(Register)
	{
	case 0:
		sprintf(TempStr, "Key %s, ExtOut: %X", OnOff(Data & 0x01), (Data >> 3) & 0x0F);
		break;
	case 1:
		sprintf(TempStr, "LFO Freq: %02X", Data);
		break;
	case 2:
		sprintf(TempStr, "LFO Wave: %u, PMS: %u, AMS: %u",
				Data & 0x03, (Data >> 3) & 0x07, (Data >> 6) & 0x03);
		break;
	case 3:
		sprintf(TempStr, "Multiple: %X, Detune: %u", Data & 0x0F, (Data >> 4) & 0x07);
		break;
	case 4:
		sprintf(TempStr, "Total Level: %02X = %u%%",
				Data & 0x7F, GetLogVolPercent(Data & 0x7F, 0x08, 0xFF));
		break;
	case 5:
		sprintf(TempStr, "Attack Rate: %02X, Key Scale: %u", Data & 0x1F, (Data >> 5) & 0x07);
		break;
	case 6:
		sprintf(TempStr, "Decay 1 Rate: %02X", Data & 0x1F);
		break;
	case 7:
		sprintf(TempStr, "Decay 2 Rate: %02X", Data & 0x1F);
		break;
	case 8:
		sprintf(TempStr, "Release Rate: %X, Decay 1 Level: %X",
				Data & 0x0F, (Data >> 4) & 0x0F);
		break;
	case 9:
		sprintf(TempStr, "FNum LSB: %02X", Data);
		break;
	case 10:
		sprintf(TempStr, "FNum MSB: %02X, Block: %X",
				Data & 0x0F, (Data >> 4) & 0x0F);
		break;
	case 11:
		sprintf(TempStr, "Waveform: %u, Feedback: %u, AccOn: %s",
				Data & 0x07, (Data >> 4) & 0x07, Enable(Data & 0x80));
		break;
	case 12:
		sprintf(TempStr, "Algorithm: %X", Data & 0x0F);
		break;
	case 13:
		sprintf(TempStr, "Channel 0 Volume: %X = %u%%, Channel 1 Volume: %X = %u%%",
				(Data >> 4) & 0x0F, GetDBTblPercent((Data >> 4) & 0x0F, OPX_PCM_DBVol, 0x0D),
				(Data >> 0) & 0x0F, GetDBTblPercent((Data >> 0) & 0x0F, OPX_PCM_DBVol, 0x0D));
		break;
	case 14:
		sprintf(TempStr, "Channel 2 Volume: %X = %u%%, Channel 3 Volume: %X = %u%%",
				(Data >> 4) & 0x0F, GetDBTblPercent((Data >> 4) & 0x0F, OPX_PCM_DBVol, 0x0D),
				(Data >> 0) & 0x0F, GetDBTblPercent((Data >> 0) & 0x0F, OPX_PCM_DBVol, 0x0D));
		break;
	default:
		sprintf(TempStr, "Invalid Register");
		break;
	}

	return;
}

static void ymf271_write_fm(char* TempStr, UINT8 Port, UINT8 Register, UINT8 Data)
{
	UINT8 SlotReg;
	UINT8 SlotNum;
	UINT8 SyncMode;
	UINT8 SyncReg;

	if ((Register & 0x03) == 0x03)
	{
		sprintf(RedirectStr, "Invalid Slot");
		return;
	}

	SlotNum = ((Register & 0x0F) / 0x04 * 0x03) + (Register & 0x03);
	SlotReg = (Register >> 4) & 0x0F;
	if (SlotNum >= 12 || 12 * Port > 48)
	{
		sprintf(RedirectStr, "Progrmm Error");
		return;
	}

	// check if the register is a synchronized register
	SyncReg = 0;
	switch(SlotReg)
	{
	case  0:
	case  9:
	case 10:
	case 12:
	case 13:
	case 14:
		SyncReg = 1;
		break;
	default:
		break;
	}

	// check if the slot is key on slot for synchronizing
	SyncMode = 0;
	switch(CacheYMF271[ChpCur].group_sync[SlotNum])
	{
	case 0:		// 4 slot mode
		if (Port == 0)
			SyncMode = 1;
		break;
	case 1:		// 2x 2 slot mode
		if (Port == 0 || Port == 1)
			SyncMode = 1;
		break;
	case 2:		// 3 slot + 1 slot mode
		if (Port == 0)
			SyncMode = 1;
		break;
	default:
		break;
	}

	ymf271_write_fm_reg(WriteStr, SlotReg, Data);

	if (SyncMode && SyncReg)		// key-on slot & synced register
	{
		switch(CacheYMF271[ChpCur].group_sync[SlotNum])
		{
		case 0:		// 4 slot mode
			sprintf(TempStr, "4-Slot (%u/%u/%u/%u): %s", (12 * 0) + SlotNum,
					(12 * 1) + SlotNum, (12 * 2) + SlotNum, (12 * 3) + SlotNum, WriteStr);
			break;
		case 1:		// 2x 2 slot mode
			if (Port == 0)		// Slot 1 - Slot 3
			{
				sprintf(TempStr, "2x 2-Slot (%u/%u): %s",
						(12 * 0) + SlotNum, (12 * 2) + SlotNum, WriteStr);
			}
			else				// Slot 2 - Slot 4
			{
				sprintf(TempStr, "2x 2-Slot (%u/%u): %s",
						(12 * 1) + SlotNum, (12 * 3) + SlotNum, WriteStr);
			}
			break;
		case 2:		// 3 slot + 1 slot mode
			// 1 slot is handled normally
			sprintf(TempStr, "3+1-Slot (%u/%u/%u): %s",
					(12 * 0) + SlotNum, (12 * 1) + SlotNum, (12 * 2) + SlotNum, WriteStr);
			break;
		default:
			sprintf(TempStr, "Invalid Sync: %s", WriteStr);
			break;
		}
	}
	else		// write register normally
	{
		sprintf(TempStr, "Slot %u: %s", (12 * Port) + SlotNum, WriteStr);
	}

	return;
}

void ymf271_write(char* TempStr, UINT8 Port, UINT8 Register, UINT8 Data)
{
	UINT8 SlotNum;
	UINT8 Addr;

	WriteChipID(0x0E);

	//sprintf(TempStr, "%sPort %x Reg 0x%02X Data 0x%02X", ChipStr, Port, Register, Data);

	switch(Port)
	{
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
		ymf271_write_fm(RedirectStr, Port, Register, Data);
		break;
	case 0x04:
		if ((Register & 0x03) == 0x03)
		{
			sprintf(WriteStr, "Invalid Slot");
		}
		else
		{
			SlotNum = ((Register & 0x0F) / 0x04 * 0x03) + (Register & 0x03);
			Addr = (Register >> 4) % 3;

			switch((Register >> 4) & 0x0F)
			{
			case 0:
			case 1:
			case 2:
				sprintf(RedirectStr, "Start Address %s: %02X", ADDR_3S_STR[Addr], Data);
				break;
			case 3:
			case 4:
			case 5:
				sprintf(RedirectStr, "End Address %s: %02X", ADDR_3S_STR[Addr], Data);
				break;
			case 6:
			case 7:
			case 8:
				sprintf(RedirectStr, "Loop Address %s: %02X", ADDR_3S_STR[Addr], Data);
				break;
			case 9:
				sprintf(RedirectStr, "FS: %u, Bits: %u, SrcNote: %u, SrcB: %u", Data & 0x03,
						(Data & 0x04) ? 12 : 8, (Data >> 3) & 0x03, (Data >> 5) & 0x07);
				break;
			default:
				sprintf(RedirectStr, "Invalid Register");
				break;
			}
			sprintf(WriteStr, "Slot %u %s", SlotNum, RedirectStr);
		}
		sprintf(RedirectStr, "PCM Write: %s", WriteStr);
		break;
	case 0x06:
		if (! (Register & 0xF0))
		{
			if ((Register & 0x03) == 0x03)
			{
				sprintf(RedirectStr, "Group Register: Invalid Channel");
				break;
			}

			SlotNum = ((Register & 0x0F) / 0x04 * 0x03) + (Register & 0x03);
			CacheYMF271[ChpCur].group_sync[SlotNum] = Data & 0x03;
			sprintf(RedirectStr, "Group Register: Slot %u, Sync Mode: %u (%s)",
					SlotNum, Data & 0x03, OPX_SYNC_TYPES[Data & 0x03]);
		}
		else
		{
			switch(Register)
			{
			case 0x10:	// Timer A MSB
				sprintf(RedirectStr, "Timer A MSB: %02X", Data);
				break;
			case 0x11:	// Timer A LSB
				sprintf(RedirectStr, "Timer A LSB: %02X", Data & 0x03);
				break;
			case 0x12:	// Timer B
				sprintf(RedirectStr, "Timer B: %02X", Data);
				break;
			case 0x13:	// Timer A/B Load, Timer A/B IRQ Enable, Timer A/B Reset
				sprintf(RedirectStr, "Enable Timer: %c%c, Timer IRQ Enable: %c%c, Reset Timer Status: %c%c",
						(Data & 0x01) ? 'A' : '-', (Data & 0x02) ? 'B' : '-',
						(Data & 0x04) ? 'A' : '-', (Data & 0x08) ? 'B' : '-',
						(Data & 0x10) ? 'A' : '-', (Data & 0x20) ? 'B' : '-');
				break;
			case 0x14:
			case 0x15:
				sprintf(RedirectStr, "Set External Address %s: %02X",
						ADDR_3S_STR[Register & 0x03], Data);
				break;
			case 0x16:
				sprintf(RedirectStr, "Set External Address High: %02X, External Read: %s",
						Data & 0x7F, OnOff(~Data & 0x80));
				break;
			case 0x17:
				sprintf(RedirectStr, "External Write: Data %02X", Data);
				break;
			}
		}
		break;
	default:
		sprintf(RedirectStr, "Invalid Port");
		break;
	}

	sprintf(TempStr, "%s%s", ChipStr, RedirectStr);

	return;
}

void gb_sound_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	UINT8 TempByt;
	INT8 TempSByt;
	UINT32 StrPos;

	WriteChipID(0x13);

	switch(Register)
	{
	//MODE 1
	case NR10: // Sweep (R/W)
		TempSByt = (Data & 0x08) >> 3;
		TempSByt |= TempSByt - 1;	// 1 -> Dir 1, 0 -> Dir -1
		sprintf(WriteStr, "Ch 1 Sweep Shift: %u, Sweep Dir: %d, Sweep Time: %u",
				Data & 0x07, TempSByt, (Data & 0x70) >> 4);
		break;
	case NR11: // Sound length/Wave pattern duty (R/W)
		sprintf(WriteStr, "Ch 1 Wave Pattern Duty: %.1f%%, Sound Length: %u",
				GB_WAVE_DUTY[(Data & 0xC0) >> 6], Data & 0x3F);
		break;
	case NR12: // Envelope (R/W)
		TempSByt = (Data & 0x08) >> 3;
		TempSByt |= TempSByt - 1;	// 1 -> Dir 1, 0 -> Dir -1
		sprintf(WriteStr, "Ch 1 Envelope Value: %u, Env. Dir: %d, Env. Length: %u",
				(Data & 0xF0) >> 4, TempSByt, Data & 0x07);
		break;
	case NR13: // Frequency lo (R/W)
		sprintf(WriteStr, "Ch 1 Frequency LSB: %02X", Data);
		break;
	case NR14: // Frequency hi / Initialize (R/W)
		sprintf(WriteStr, "Ch 1 Mode: %u, Frequency MSB: %01X",
				(Data & 0x40) >> 6, Data & 0x07);
		if (Data & 0x80)
			sprintf(WriteStr, "%s, Key On", WriteStr);
		break;
	//MODE 2
	case NR21: // Sound length/Wave pattern duty (R/W)
		sprintf(WriteStr, "Ch 2 Wave Pattern Duty: %.1f%%, Sound Length: %u",
				GB_WAVE_DUTY[(Data & 0xC0) >> 6], Data & 0x3F);
		break;
	case NR22: // Envelope (R/W)
		TempSByt = (Data & 0x08) >> 3;
		TempSByt |= TempSByt - 1;	// 1 -> Dir 1, 0 -> Dir -1
		sprintf(WriteStr, "Ch 2 Envelope Value: %u, Env. Dir: %d, Env. Length: %u",
				(Data & 0xF0) >> 4, TempSByt, Data & 0x07);
		break;
	case NR23: // Frequency lo (R/W)
		sprintf(WriteStr, "Ch 2 Frequency LSB: %02X", Data);
		break;
	case NR24: // Frequency hi / Initialize (R/W)
		sprintf(WriteStr, "Ch 2 Mode: %u, Frequency MSB: %01X",
				(Data & 0x40) >> 6, Data & 0x07);
		if (Data & 0x80)
			sprintf(WriteStr, "%s, Key On", WriteStr);
		break;
	//MODE 3
	case NR30: // Sound On/Off (R/W)
		sprintf(WriteStr, "Ch 3 Sound %s", OnOff(Data & 0x80));
		break;
	case NR31: // Sound Length (R/W)
		sprintf(WriteStr, "Ch 3 Sound Length: %u", Data);
		break;
	case NR32: // Select Output Level
		TempByt = (Data & 0x60) >> 5;
		sprintf(WriteStr, "Ch 3 Output Level: %X = %u%%", TempByt,
				100 * (0x03 - TempByt) / 0x03);
		break;
	case NR33: // Frequency lo (W)
		sprintf(WriteStr, "Ch 3 Frequency LSB: %02X", Data);
		break;
	case NR34: // Frequency hi / Initialize (W)
		sprintf(WriteStr, "Ch 3 Mode: %u, Frequency MSB: %01X",
				(Data & 0x40) >> 6, Data & 0x07);
		if (Data & 0x80)
			sprintf(WriteStr, "%s, Key On", WriteStr);
		break;
	//MODE 4
	case NR41: // Sound Length (R/W)
		sprintf(WriteStr, "Ch N Sound Length: %u", Data & 0x3F);
		break;
	case NR42: // Envelope (R/W)
		TempSByt = (Data & 0x08) >> 3;
		TempSByt |= TempSByt - 1;	// 1 -> Dir 1, 0 -> Dir -1
		sprintf(WriteStr, "Ch N Envelope Value: %u, Env. Dir: %d, Env. Length: %u",
				(Data & 0xF0) >> 4, TempSByt, Data & 0x07);
		break;
	case NR43: // Polynomial Counter/Frequency
		sprintf(WriteStr, "Ch N Freq. Divider: %u, Shift Freq.: %u, Counter Size: %u bit",
				Data & 0x07, (Data & 0xF0) >> 4, 0x08 + (Data & 0x08));
		break;
	case NR44: // Counter/Consecutive / Initialize (R/W)
		sprintf(WriteStr, "Ch N Mode: %s",
				GB_NOISE_MODE[(Data & 0x40) >> 6]);
		if (Data & 0x80)
			sprintf(WriteStr, "%s, Key On", WriteStr);
		break;
	// CONTROL
	case NR50: // Channel Control / On/Off / Volume (R/W)
		sprintf(WriteStr, "Master Volume L: %u = %u%%, Volume R: %u = %u%%",
				Data & 0x07, 100 * (Data & 0x07) / 0x07,
				(Data & 0x70) >> 4, 100 * (Data & 0x70) / 0x70);
		break;
	case NR51: // Selection of Sound Output Terminal
		sprintf(WriteStr, "Sound Output Left: ");
		StrPos = strlen(WriteStr);
		for (TempByt = 0x00; TempByt < 0x04; TempByt ++)
		{
			if ((TempByt & 0x03) != 0x03)
				TempSByt = '0' + (TempByt & 0x03);
			else
				TempSByt = 'N';
			WriteStr[StrPos] = (Data & (0x10 << TempByt)) ? TempSByt : '-';
			StrPos ++;
		}
		WriteStr[StrPos] = 0x00;

		sprintf(WriteStr, "%s, Right: ", WriteStr);
		StrPos = strlen(WriteStr);
		for (TempByt = 0x00; TempByt < 0x04; TempByt ++)
		{
			if ((TempByt & 0x03) != 0x03)
				TempSByt = '0' + (TempByt & 0x03);
			else
				TempSByt = 'N';
			WriteStr[StrPos] = (Data & (0x01 << TempByt)) ? TempSByt : '-';
			StrPos ++;
		}
		WriteStr[StrPos] = 0x00;
		break;
	case NR52: // Sound On/Off (R/W)
		/* Only bit 7 is writable, writing to bits 0-3 does NOT enable or
		   disable sound.  They are read-only */
		sprintf(WriteStr, "Sound %s", OnOff(Data & 0x80));
		if (Data & 0x80)
			sprintf(WriteStr, "%s (Reset Chip if chip was disabled)", WriteStr);
		break;
	default:
		sprintf(WriteStr, "Reg 0x%02X, Data 0x%02X", Register, Data);
		break;
	}

	sprintf(TempStr, "%s%s", ChipStr, WriteStr);

	return;
}

void nes_psg_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	UINT8 CurChn;

	WriteChipID(0x14);

	switch(Register & 0xE0)
	{
	case 0x00:	// NES APU
		switch(Register)
		{
		// Squares
		case APU_WRA0:
		case APU_WRB0:
			CurChn = (Register & 0x04) >> 2;
			sprintf(WriteStr, "Square %u: Duty Cycle: %.1f%%, Hold: %s, Envelope: %s, Volume %X",
					CurChn, GB_WAVE_DUTY[(Data & 0xC0) >> 6], OnOff(Data & 0x20),
					OnOff(Data & 0x10), Data & 0x0F);
			break;
		case APU_WRA1:
		case APU_WRB1:
			CurChn = (Register & 0x04) >> 2;
			sprintf(WriteStr, "Square %u: Sweep: %s, Length: %u, %s, Shift: %u",
					CurChn, OnOff(Data & 0x80), (Data & 0x70) >> 4,
					((Data & 0x08) ? "Increment" : "Decrement"), Data & 0x07);
			break;
		case APU_WRA2:
		case APU_WRB2:
			CurChn = (Register & 0x04) >> 2;
			sprintf(WriteStr, "Square %u: Frequency LSB: %02X", CurChn, Data);
			break;
		case APU_WRA3:
		case APU_WRB3:
			CurChn = (Register & 0x04) >> 2;
			sprintf(WriteStr, "Square %u: Frequency MSB: %01X, Length: %u VBlank(s)",
					CurChn, Data & 0x07, (Data & 0xF8) >> 3);
			break;
		// Triangle
		case APU_WRC0:
			sprintf(WriteStr, "Triangle: Hold: %s, Linear Length: %u",
					OnOff(Data & 0x80), Data & 0x7F);
			break;
		case APU_WRC2:
			sprintf(WriteStr, "Triangle: Frequency LSB: %02X", Data);
			break;
		case APU_WRC3:
			sprintf(WriteStr, "Triangle: Frequency MSB: %01X, Length: %u VBlank(s)",
					Data & 0x07, (Data & 0xF8) >> 3);
			break;
		// Noise
		case APU_WRD0:
			sprintf(WriteStr, "Noise: Hold: %s, Envelope: %s, Volume %X",
					OnOff(Data & 0x20), OnOff(Data & 0x10), Data & 0x0F);
			break;
		case APU_WRD2:
			sprintf(WriteStr, "Noise: Frequency LSB: %02X, Short Sample: %s",
					Data & 0x0F, OnOff(Data & 0x80));
			break;
		case APU_WRD3:
			sprintf(WriteStr, "Noise: Length: %u VBlank(s)", (Data & 0xF8) >> 3);
			break;
		// DMC
		case APU_WRE0:
			sprintf(WriteStr, "DPCM: IRQ: %s, Looping: %s, Cycles per Sample: %u",
					OnOff(Data & 0x80), OnOff(Data & 0x40), dpcm_clocks[Data & 0x0F]);
			break;
		case APU_WRE1: // 7-bit DAC
			sprintf(WriteStr, "DPCM: Sample Data: %X", Data & 0x7F);
			break;
		case APU_WRE2:
			sprintf(WriteStr, "DPCM: Set Sample Address: 0x%X", 0xC000 | (Data << 6));
			break;
		case APU_WRE3:
			sprintf(WriteStr, "DPCM: Set Sample Length: 0x%03X", (Data << 4) + 1);
			break;
		case APU_IRQCTRL:
			sprintf(WriteStr, "IRQ Ctrl: 0x%02X", Data);
			break;
		case APU_SMASK:
			sprintf(WriteStr, "Channel Enable: Square 0 %s, Square 1 %s, Triangle %s, "
					"Noise %s, DPCM %s", OnOff(Data & 0x01), OnOff(Data & 0x02),
					OnOff(Data & 0x04), OnOff(Data & 0x08), OnOff(Data & 0x10));
			break;
		default:
			sprintf(WriteStr, "Reg 0x%02X, Data 0x%02X", Register, Data);
			break;
		}
		break;
	case 0x20:	// FDS sound
		switch(Register + 0x60)
		{
		case 0x80:	// $4080 volume envelope
			if (Data & 0x80)
				sprintf(RedirectStr, "Volume Envelope: %s, Mode: %+d, Level: %02X",
						Enable(~Data & 0x80), (Data & 0x40) ? +1 : -1, Data & 0x3F);
			else
				sprintf(RedirectStr, "Volume Envelope: %s, Mode: %+d, Speed: %02X",
						Enable(~Data & 0x80), (Data & 0x40) ? +1 : -1, Data & 0x3F);
			break;
	//	case 0x81:	// $4081 ---
	//		break;
		case 0x82:	// $4082 wave frequency low
			sprintf(RedirectStr, "Frequency LSB: %02X", Data);
			break;
		case 0x83:	// $4083 wave frequency high / enables
			sprintf(RedirectStr, "Frequency MSB: %01X, Wave Halt: %s, Envelope Halt: %s",
					Data & 0x0F, Enable(Data & 0x80), Enable(Data & 0x40));
			break;
		case 0x84:	// $4084 mod envelope
			if (Data & 0x80)
				sprintf(RedirectStr, "Modulation Envelope: %s, Mode: %+d, Level: %02X",
						Enable(~Data & 0x80), (Data & 0x40) ? +1 : -1, Data & 0x3F);
			else
				sprintf(RedirectStr, "Modulation Envelope: %s, Mode: %+d, Speed: %02X",
						Enable(~Data & 0x80), (Data & 0x40) ? +1 : -1, Data & 0x3F);
			break;
		case 0x85:	// $4085 mod position
			sprintf(RedirectStr, "Modulation Position: %02X", Data & 0x7F);
			break;
		case 0x86:	// $4086 mod frequency low
			sprintf(RedirectStr, "Mod. Frequency LSB: %02X", Data);
			break;
		case 0x87:	// $4087 mod frequency high / enable
			sprintf(RedirectStr, "Mod. Frequency MSB: %01X, Wave Halt: %s",
					Data & 0x0F, Enable(Data & 0x80));
			break;
		case 0x88:	// $4088 mod table write
			sprintf(RedirectStr, "Mod. Table Write: %02X", Data & 0x7F);
			break;
		case 0x89:	// $4089 wave write enable, master volume
			sprintf(RedirectStr, "Wave Write: %s, Master Volume %X",
					OnOff(Data & 0x80), Data & 0x03);
			break;
		case 0x8A:	// $408A envelope speed
			sprintf(RedirectStr, "Envelope Speed: %02X", Data);
			break;
		case 0x9F:	// $4023 master I/O enable/disable
			sprintf(RedirectStr, "Master I/O: %s", Enable(Data & 0x80));
			break;
		default:
			sprintf(RedirectStr, "Reg 0x%02X, Data 0x%02X", Register, Data);
			break;
		}
		sprintf(WriteStr, "FDS: %s", RedirectStr);
		break;
	case 0x40:	// FDS Wave RAM
	case 0x60:
		sprintf(WriteStr, "FDS Wave RAM 0x%02X = 0x%02X", Register & 0x3F, Data);
		break;
	default:
		sprintf(WriteStr, "Reg 0x%02X, Data 0x%02X", Register, Data);
		break;
	}

	sprintf(TempStr, "%s%s", ChipStr, WriteStr);

	return;
}

void c140_write(char* TempStr, UINT8 Port, UINT8 Register, UINT8 Data)
{
	UINT16 RegVal;
	UINT8 Channel;

	WriteChipID(0x1C);

	RegVal = (Port << 8) | Register;
	RegVal &= 0x1FF;

	// mirror the bank registers on the 219, fixes bkrtmaq (and probably xday2 based on notes in the HLE)
	if ((RegVal >= 0x1F8) /*&& (info->banking_type == C140_TYPE_ASIC219)*/)
		RegVal -= 0x008;

	if (RegVal < 0x180)
	{
		Channel = RegVal >> 4;
		switch(RegVal & 0x0F)
		{
		case 0x00:
			sprintf(WriteStr, "Volume R = %02X", Data);
			break;
		case 0x01:
			sprintf(WriteStr, "Volume L = %02X", Data);
			break;
		case 0x02:
			sprintf(WriteStr, "Frequency MSB = %02X", Data);
			break;
		case 0x03:
			sprintf(WriteStr, "Frequency LSB = %02X", Data);
			break;
		case 0x04:
			sprintf(WriteStr, "Set Bank = 0x%02X", Data);
			break;
		case 0x05:
			sprintf(WriteStr, "Mode: Key %s, PCM Sign Flip: %s, Looping %s, "
					"Compr. PCM %s, Sign+Magn. Format: %s",
					OnOff(Data & 0x80), OnOff(Data & 0x40), OnOff(Data & 0x10),
					Enable(Data & 0x08), Enable(Data & 0x01));
			break;
		case 0x06:
			sprintf(WriteStr, "Start Addr MSB: 0x%02X", Data);
			break;
		case 0x07:
			sprintf(WriteStr, "Start Addr LSB: 0x%02X", Data);
			break;
		case 0x08:
			sprintf(WriteStr, "End Addr MSB: 0x%02X", Data);
			break;
		case 0x09:
			sprintf(WriteStr, "End Addr LSB: 0x%02X", Data);
			break;
		case 0x0A:
			sprintf(WriteStr, "Loop Addr MSB: 0x%02X", Data);
			break;
		case 0x0B:
			sprintf(WriteStr, "Loop Addr LSB: 0x%02X", Data);
			break;
		case 0x0C:
		case 0x0D:
		case 0x0E:
		case 0x0F:
			sprintf(WriteStr, "Unknown Reg = 0x%02X", Data);
			break;
		}

		sprintf(TempStr, "%sCh %u %s", ChipStr, Channel, WriteStr);
	}
	else
	{
		if (RegVal >= 0x1F0 && (RegVal & 0x01))
		{
			sprintf(WriteStr, "Bank Register %u, Data 0x%02X", ((Register + 1) & 0x07) >> 1, Data);
		}
		else
		{
			sprintf(WriteStr, "Reg 0x%03X, Data 0x%02X", RegVal, Data);
		}

		sprintf(TempStr, "%s%s", ChipStr, WriteStr);
	}

	return;
}

void c6280_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	WriteChipID(0x1B);

	switch(Register & 0x0F)
	{
	case 0x00: // Channel select
		sprintf(WriteStr, "Select Channel %u", Data & 0x07);
		break;
	case 0x01: // Global balance
		sprintf(WriteStr, "Global Balance: Left %u%%, Right %u%%",
				((Data & 0xF0) * 100 + 0x78) / 0xF0,
				((Data & 0x0F) * 100 + 0x07) / 0x0F);
		break;
	case 0x02: // Channel frequency (LSB)
		sprintf(WriteStr, "Channel Frequency LSB: %02X", Data);
		break;
	case 0x03: // Channel frequency (MSB)
		sprintf(WriteStr, "Channel Frequency MSB: %01X", Data & 0x0F);
		break;
	case 0x04: // Channel control (key-on, DDA mode, volume)
		/*// 1-to-0 transition of DDA bit resets waveform index
		if((q->control & 0x40) && ((data & 0x40) == 0))
		{
			q->index = 0;
		}
		q->control = data;*/
		sprintf(WriteStr, "Channel Control: %s, DDA Mode %s, Volume 0x%02X = %u%%",
				Enable(Data & 0x80), Enable(Data & 0x40), Data & 0x1F,
				((Data & 0x1F) * 100 + 0x0F) / 0x1F);
		break;
	case 0x05: // Channel balance
		sprintf(WriteStr, "Channel Balance: Left %u%%, Right %u%%",
				((Data & 0xF0) * 100 + 0x78) / 0xF0,
				((Data & 0x0F) * 100 + 0x07) / 0x0F);
		break;
	case 0x06: // Channel waveform data
		sprintf(WriteStr, "Channel Waveform Data: %02X", Data);
		break;
	case 0x07: // Noise control (enable, frequency)
		sprintf(WriteStr, "Channel Noise Control: %s, Freqency 0x%02X",
				Enable(Data & 0x80), Data & 0x1F);
		break;
	case 0x08: // LFO frequency
		sprintf(WriteStr, "LFO Frequency: 0x%02X", Data);
		break;
	case 0x09: // LFO control (enable, mode)
		sprintf(WriteStr, "LFO Control: %s, Mode 0x%02X",
				Enable(Data & 0x80), Data & 0x03);
		break;
	default:
		sprintf(WriteStr, "Reg 0x%02X, Data 0x%02X", Register & 0x0F, Data);
		break;
	}

	sprintf(TempStr, "%s%s", ChipStr, WriteStr);

	return;
}

void qsound_write(char* TempStr, UINT8 Offset, UINT16 Value)
{
	UINT8 ch;
	UINT8 reg;
	INT8 TempByt;

	WriteChipID(0x1F);

	if (Offset < 0x80) // PCM registers
	{
		ch = Offset >> 3;
		reg = Offset & 0x07;
		if(reg == 0)
			ch = (ch + 1) & 0x0F;
	}
	else if (Offset < 0x93) // panning
	{
		ch = Offset - 0x80;
		reg = 8;
	}
	else if (Offset == 0x93)
	{
		sprintf(WriteStr, "Set Echo Feedback: 0x%04X (%d %%)", Value, Value*100/32767);
		sprintf(TempStr, "%s%s", ChipStr, WriteStr);
		return;
	}
	else if (Offset >= 0xBA && Offset < 0xCA) // echo
	{
		ch = Offset - 0xBA;
		reg = 9;
	}
	else if (Offset >= 0xCA && Offset < 0xD6) // ADPCM regs
	{
		static const int adpcm_reg_map[4] = {1,5,0,6}; // start, end, bank, vol
		ch = 16+((Offset-0xCA)>>2);
		reg = adpcm_reg_map[(Offset-0xCA)&3];
	}
	else if (Offset >= 0xD6 && Offset < 0xD9) // ADPCM key on
	{
		ch = 16+(Offset-0xD6);
		reg = 10;
	}
	else if (Offset == 0xD9)
	{
		sprintf(WriteStr, "Set Echo Delay: 0x%04X (%d samples, %.2f ms)", Value, Value-0x554, (Value/24038.)*1000.);
		sprintf(TempStr, "%s%s", ChipStr, WriteStr);
		return;
	}
	else if (Offset >= 0xDA && Offset < 0xE2)
	{
		ch = ((Offset - 0xDA)&2)>>1;
		reg = ((Offset - 0xDA)&4)>>2;
		sprintf(WriteStr, "Set %s %s Filter %s: 0x%04X",
			(ch == 0) ? "Left" : "Right",
			((Offset&1) == 0) ? "Wet" : "Dry",
			(reg == 0) ? "Select" : "Delay",
			Value);
		sprintf(TempStr, "%s%s", ChipStr, WriteStr);
		return;
	}
	else if (Offset == 0xE2)
	{
		sprintf(WriteStr, "Set Delay Update Flag: 0x%04X", Value);
		sprintf(TempStr, "%s%s", ChipStr, WriteStr);
		return;
	}
	else if (Offset == 0xE3)
	{
		sprintf(WriteStr, "Set Chip Mode: 0x%04X", Value);
		if(Value == 0x0000)
			strcat(WriteStr, " (Soft Reset)");
		else if(Value == 0x0288)
			strcat(WriteStr, " (Reset chip)");
		else if(Value == 0x0039)
			strcat(WriteStr, " (Update filters)");
		else if(Value == 0x061A)
			strcat(WriteStr, " (Quad filter mode: Reset chip)");
		else if(Value == 0x004F)
			strcat(WriteStr, " (Quad filter mode: Update filters)");
		else if(Value == 0x000C)
			strcat(WriteStr, " (Test Mode 1)");
		else if(Value == 0x000F)
			strcat(WriteStr, " (Test Mode 2)");
		sprintf(TempStr, "%s%s", ChipStr, WriteStr);
		return;
	}
	else if (Offset >= 0xE4 && Offset < 0xE8)
	{
		ch = ((Offset - 0xE4)&2)>>1;
		sprintf(WriteStr, "Set %s %s Master Volume: 0x%04X (%d %%)",
			(ch == 0) ? "Left" : "Right",
			((Offset&1) == 0) ? "Wet" : "Dry",
			Value, (INT16)Value*100/16383);
		sprintf(TempStr, "%s%s", ChipStr, WriteStr);
		return;
	}
	else
	{
		sprintf(WriteStr, "Register %02X: 0x%04X", Offset, Value);
		sprintf(TempStr, "%s%s", ChipStr, WriteStr);
		return;
	}

	switch(reg)
	{
	case 0: // Bank
		sprintf(WriteStr, "Set Bank: %02X (base 0x%06X)", Value & 0x7F,
				(Value & 0x7F) << 16);
		break;
	case 1: // start
		sprintf(WriteStr, "Set Start Address: 0x%04X", Value);
		break;
	case 2: // pitch
		sprintf(WriteStr, "Set Pitch: 0x%04X", Value);
		if (! Value)
			sprintf(WriteStr, "%s (Key Off)", WriteStr);
		else
			sprintf(WriteStr, "%s (%d Hz)", WriteStr, Value*24038/4096);
		break;
	case 3: // phase
		sprintf(WriteStr, "Set Phase: 0x%04X", Value);
		break;
	case 4: // loop
		sprintf(WriteStr, "Set Loop Length: 0x%04X", Value);
		break;
	case 5: // end
		sprintf(WriteStr, "Set End Address: 0x%04X", Value);
		break;
	case 6: // master volume
		sprintf(WriteStr, "Set Volume: 0x%04X (%d %%)", Value, (INT16)Value*100/8191);
		break;
	case 8: // pan
		if(Value >= 0x110 && Value <= 0x130)
		{
			TempByt = (Value - 0x120);
			sprintf(WriteStr, "Set Pan: 0x%04X (%d)", Value, TempByt);
		}
		else if(Value >= 0x140 && Value <= 0x160)
		{
			TempByt = (Value - 0x150);
			sprintf(WriteStr, "Set Pan: 0x%04X (%d, No Spatial Effect)", Value, TempByt);
		}
		else
		{
			sprintf(WriteStr, "Set Pan: 0x%04X (Illegal)", Value);
		}
		break;
	case 9: // echo
		sprintf(WriteStr, "Set Echo: 0x%04X (%d %%)", Value, (INT16)Value*100/32767);
		break;
	case 10: // key on (ADPCM only)
		sprintf(WriteStr, "Key On: 0x%04X", Value);
		break;
	default:
		sprintf(WriteStr, "Register %u: 0x%04X", reg, Value);
		break;
	}
	if(ch < 16)
		sprintf(TempStr, "%sCh %u: %s", ChipStr, ch, WriteStr);
	else if(ch < 19)
		sprintf(TempStr, "%sADPCM Ch %u: %s", ChipStr, ch-16, WriteStr);

	return;
}

void k053260_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	UINT8 CurChn;
	UINT32 StrPos;
	UINT8 ChnEn;

	WriteChipID(0x1D);

	if (Register < 0x08)
	{
		// communication registers
		sprintf(WriteStr, "Communication Register %02X: 0x%02X", Register, Data);
	}
	else if (Register < 0x28)
	{
		// channel setup
		switch(Register & 0x07)
		{
		case 0:	// sample rate low
			sprintf(RedirectStr, "Set Sample Rate LSB: 0x%02X", Data);
			break;
		case 1:	// sample rate high
			sprintf(RedirectStr, "Set Sample Rate MSB: 0x%01X", Data & 0x0F);
			break;
		case 2:	// size low
			sprintf(RedirectStr, "Set Sample Size LSB: 0x%02X", Data);
			break;
		case 3:	// size high
			sprintf(RedirectStr, "Set Sample Size MSB: 0x%02X", Data);
			break;
		case 4:	// start low
			sprintf(RedirectStr, "Set Sample Start LSB: 0x%02X", Data);
			break;
		case 5:	// start high
			sprintf(RedirectStr, "Set Sample Start MSB: 0x%02X", Data);
			break;
		case 6: // bank
			sprintf(RedirectStr, "Set Bank: 0x%02X", Data);
			break;
		case 7: // volume
			sprintf(RedirectStr, "Set Volume: 0x%02X = %u%%",
					Data & 0x7F, ((Data & 0x7F) * 100 + 0x3E) / 0x7F);
			break;
		}

		sprintf(WriteStr, "Ch %u: %s", (Register - 8) / 8, RedirectStr);
	}
	else
	{
		switch(Register)
		{
		case 0x28:
			sprintf(WriteStr, "Channel Enable: ");
			StrPos = strlen(WriteStr);
			for (CurChn = 0x00; CurChn < 0x04; CurChn ++)
			{
				ChnEn = Data & (0x01 << CurChn);
				WriteStr[StrPos] = ChnEn ? ('0' + CurChn) : '-';
				StrPos ++;
			}
			WriteStr[StrPos] = 0x00;
			break;
		case 0x2A: // loop, ppcm
			sprintf(WriteStr, "Loop Enable: Ch ----, Packed PCM Enable: Ch ----");
			StrPos = 0x00;
			for (CurChn = 0x00; CurChn < 0x08; CurChn ++)
			{
				if (! (CurChn & 0x03))
					StrPos = strchr(WriteStr + StrPos, '-') - WriteStr;

				if (Data & (0x01 << CurChn))
					WriteStr[StrPos] = '0' + (CurChn & 0x03);
				StrPos ++;
			}
			WriteStr[StrPos] = 0x00;
			break;
		case 0x2C:	// pan
			sprintf(WriteStr, "Ch 0 Pan: %01X, Ch 1 Pan: %01X",
					Data & 0x07, (Data & 0x38) >> 3);
			break;
		case 0x2D:	// more pan
			sprintf(WriteStr, "Ch 2 Pan: %01X, Ch 3 Pan: %01X",
					Data & 0x07, (Data & 0x38) >> 3);
			break;
		case 0x2F:	// control
			// bit 0 = read ROM
			// bit 1 = enable sound output
			// bit 2 = unknown
			sprintf(WriteStr, "Control Reg: Read ROM: %s, Sound Output: %s, "
					"Unknown Bit: %s", Enable(Data & 0x01), Enable(Data & 0x02),
					OnOff(Data & 0x04));
			break;
		default:
			sprintf(WriteStr, "Register %02X: 0x%02X", Register, Data);
			break;
		}
	}

	sprintf(TempStr, "%s%s", ChipStr, WriteStr);

	return;
}

void pokey_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	WriteChipID(0x1E);

	Register &= 0x0F;
	switch(Register)
	{
	case AUDF1_C:
	case AUDF2_C:
	case AUDF3_C:
	case AUDF4_C:
		sprintf(WriteStr, "Channel %u Frequency: 0x%02X", (Register & 0x06) >> 1, Data);
		break;
	case AUDC1_C:
	case AUDC2_C:
	case AUDC3_C:
	case AUDC4_C:
		sprintf(WriteStr, "Channel %u Poly5: %s, Poly%u, Pure Tone: %s, "
				"VolOutOnly: %s, Volume: 0x%01X = %u%%", (Register & 0x06) >> 1,
				OnOff(~Data & 0x80), (Data & 0x40) ? 4 : 17, OnOff(Data & 0x20),
				OnOff(Data & 0x10), Data & 0x0F, 100 * (Data & 0x0F) / 0x0F);
		break;
	case AUDCTL_C:
		sprintf(WriteStr, "Audio Control: Poly%u, Chn HiClk: %c%c, "
				"Chn 1-2 joined: %s, Chn 3-4 joined: %s, Chn HiFilter: %c%c, Clock: %.2f KHz",
				(Data & 0x80) ? 9 : 17, (Data & 0x40) ? '1' : '-', (Data & 0x20) ? '3' : '-',
				OnOff(Data & 0x10), OnOff(Data & 0x08),
				(Data & 0x04) ? '1' : '-', (Data & 0x02) ? '2' : '-',
				(Data & 0x01) ? 15.69 : 63.92);
		break;
	case STIMER_C:
		sprintf(WriteStr, "STimer: 0x%02X", Data);
		break;
	case SKREST_C:
		sprintf(WriteStr, "Reset SK Status Reg: 0x%02X", Data);
		break;
	case POTGO_C:
		sprintf(WriteStr, "POT Go: 0x%02X", Data);
		break;
	case SEROUT_C:
		sprintf(WriteStr, "Serial Out: 0x%02X", Data);
		break;
	case IRQEN_C:
		sprintf(WriteStr, "IRQ Enable Bits: 0x%02X", Data);
		break;
	case SKCTL_C:
		sprintf(WriteStr, "SK Control: 0x%02X", Data);
		break;
	}

	sprintf(TempStr, "%s%s", ChipStr, WriteStr);

	return;
}

void k051649_write(char* TempStr, UINT8 Port, UINT8 Register, UINT8 Data)
{
	UINT8 CurChn;
	UINT32 StrPos;
	UINT8 ChnEn;

	WriteChipID(0x19);

	switch(Port)
	{
	case 0x00:	// k051649_waveform_w
		if (Register >= 0x60)
			sprintf(WriteStr, "Ch 3/4: Write Waveform at %02X = %02X",
					Register & 0x1F, Data);
		else
			sprintf(WriteStr, "Ch %u: Write Waveform at %02X = %02X",
					Register >> 5, Register & 0x1F, Data);
		break;
	case 0x01:	// k051649_frequency_w
		if (Register & 0x01)
			sprintf(WriteStr, "Ch %u: Set Frequency MSB = %01X",
					Register >> 1, Data & 0x0F);
		else
			sprintf(WriteStr, "Ch %u: Set Frequency LSB = %02X",
					Register >> 1, Data);
		break;
	case 0x02:	// k051649_volume_w
		sprintf(WriteStr, "Ch %u: Set Volume: 0x%01X = %u%%",
				Register & 0x07, Data & 0x0F, 100 * (Data & 0x0F) / 0x0F);
		break;
	case 0x03:	// k051649_keyonoff_w
		sprintf(WriteStr, "Channel Enable: ");
		StrPos = strlen(WriteStr);
		for (CurChn = 0x00; CurChn < 0x05; CurChn ++)
		{
			ChnEn = Data & (0x01 << CurChn);
			WriteStr[StrPos] = ChnEn ? ('0' + CurChn) : '-';
			StrPos ++;
		}
		WriteStr[StrPos] = 0x00;
		break;
	case 0x04:	// k052539_waveform_w
		sprintf(WriteStr, "Ch %u: Write Waveform at %02X = %02X",
				Register >> 5, Register & 0x1F, Data);
		break;
	case 0x05:	// k051649_test_w
		sprintf(WriteStr, "Deformation Register = %02X", Data);
		break;
	default:
		sprintf(WriteStr, "Port %x Reg 0x%02X Data 0x%02X", Port, Register, Data);
		break;
	}

	sprintf(TempStr, "%s%s", ChipStr, WriteStr);

	return;
}

void okim6295_write(char* TempStr, UINT8 Port, UINT8 Data)
{
	UINT8 CurChn;
	UINT32 StrPos;
	UINT8 ChnEn;

	WriteChipID(0x18);

	switch(Port)
	{
	case 0x00:	// okim6295_write_command
		if (CacheOKI6295[ChpCur].Command != 0xFF)
		{
			sprintf(WriteStr, "Start Channel: ");
			StrPos = strlen(WriteStr);
			for (CurChn = 0x00; CurChn < 0x04; CurChn ++)
			{
				ChnEn = Data & (0x10 << CurChn);
				WriteStr[StrPos] = ChnEn ? ('0' + CurChn) : '-';
				StrPos ++;
			}
			sprintf(WriteStr + StrPos, ", Volume: 0x%01X = %u%%",
					Data & 0x0F, 100 * okim6295_voltbl[Data & 0x0F] / 0x20);

			CacheOKI6295[ChpCur].Command = 0xFF;
		}
		else if (Data & 0x80)
		{
			sprintf(WriteStr, "Play Sample 0x%02X on Channels", Data & 0x7F);
			CacheOKI6295[ChpCur].Command = Data & 0x7F;
		}
		else
		{
			sprintf(WriteStr, "Stop Channel: ");
			StrPos = strlen(WriteStr);
			for (CurChn = 0x00; CurChn < 0x04; CurChn ++)
			{
				ChnEn = Data & (0x08 << CurChn);
				WriteStr[StrPos] = ChnEn ? ('0' + CurChn) : '-';
				StrPos ++;
			}
			WriteStr[StrPos] = 0x00;
		}
		break;
	case 0x08:
		sprintf(WriteStr, "Set Master Clock: xxxxxx%02X", Data);
		break;
	case 0x09:
		sprintf(WriteStr, "Set Master Clock: xxxx%02Xxx", Data);
		break;
	case 0x0A:
		sprintf(WriteStr, "Set Master Clock: xx%02Xxxxx", Data);
		break;
	case 0x0B:
		sprintf(WriteStr, "Set Master Clock: %02Xxxxxxx", Data);
		break;
	case 0x0C:
		sprintf(WriteStr, "Set Clock Divider to %u", Data ? 132 : 165);
		break;
	case 0x0E:
		sprintf(WriteStr, "NMK112 Bank Mode: %s, banked Sample Table: %s",
				Enable(Data & 0x01), OnOff(Data & 0x80));
		break;
	case 0x0F:
		sprintf(WriteStr, "Set Bank to %06X", Data << 18);
		break;
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
		sprintf(WriteStr, "Set NMK112 Bank %u to %06X",
				Port & 0x03, Data << 16);
		break;
	default:
		sprintf(WriteStr, "Port %02X: 0x%02X", Port, Data);
		break;
	}

	sprintf(TempStr, "%s%s", ChipStr, WriteStr);

	return;
}

void okim6258_write(char* TempStr, UINT8 Port, UINT8 Data)
{
	WriteChipID(0x17);

	switch(Port)
	{
	case 0x00:
		sprintf(WriteStr, "Control Write: ");
		if (Data & 0x01)
		{
			sprintf(WriteStr, "%sStop", WriteStr);
		}
		else
		{
			sprintf(WriteStr, "%sPlay: %s, Record: %s", WriteStr,
					OnOff(Data & 0x02), OnOff(Data & 0x04));
		}
		break;
	case 0x01:
		sprintf(WriteStr, "Data Write: 0x%02X", Data);
		break;
	case 0x02:
		sprintf(WriteStr, "Pan Write: %c%c",
				(Data & 0x02) ? '-' : 'L', (Data & 0x01) ? '-' : 'R');
		break;
	case 0x08:
		sprintf(WriteStr, "Set Master Clock: xxxxxx%02X", Data);
		break;
	case 0x09:
		sprintf(WriteStr, "Set Master Clock: xxxx%02Xxx", Data);
		break;
	case 0x0A:
		sprintf(WriteStr, "Set Master Clock: xx%02Xxxxx", Data);
		break;
	case 0x0B:
		sprintf(WriteStr, "Set Master Clock: %02Xxxxxxx", Data);
		break;
	case 0x0C:
		sprintf(WriteStr, "Set Clock Divider to %u", okim6258_dividers[Data & 0x03]);
		break;
	default:
		sprintf(WriteStr, "Port %02X: 0x%02X", Port, Data);
		break;
	}

	sprintf(TempStr, "%s%s", ChipStr, WriteStr);

	return;
}

static void FM_ADPCMAWrite(char* TempStr, UINT8 Register, UINT8 Data)
{
	UINT8 CurChn;
	UINT32 StrPos;
	UINT8 ChnEn;

	switch(Register)
	{
	case 0x00:	// DM,--,C5,C4,C3,C2,C1,C0
		sprintf(WriteStr, "Key %s: ", OnOff(~Data & 0x80));
		StrPos = strlen(WriteStr);
		for (CurChn = 0; CurChn < 6; CurChn ++)
		{
			ChnEn = Data & (0x01 << CurChn);
			WriteStr[StrPos] = ChnEn ? ('0' + CurChn) : '-';
			StrPos ++;
		}
		WriteStr[StrPos] = 0x00;
		break;
	case 0x01:	// B0-5 = TL
		sprintf(WriteStr, "ADPCM Master Volume: 0x%02X = %u%%",
				Data & 0x3F, GetLogVolPercent(~Data & 0x3F, 0x08, 0x3F));
		break;
	default:
		CurChn = Register & 0x07;
		ChnEn = Register & 0x38;
		if (CurChn >= 6)
			ChnEn = 0x00;	// force generic register text
		switch(ChnEn)
		{
		case 0x08:	// B7=L,B6=R, B4-0=IL
			sprintf(WriteStr, "Ch %u: Stereo: %c%c, Volume: 0x%02X = %u%%", CurChn,
					(Data & 0x80) ? 'L' : '-', (Data & 0x40) ? 'R' : '-',
					Data & 0x1F, GetLogVolPercent(~Data & 0x1F, 0x08, 0x3F));
			break;
		case 0x10:
		case 0x18:
			sprintf(WriteStr, "Ch %u: Start Address %s 0x%02X", CurChn,
					ADDR_2S_STR[(Register & 0x08) >> 3], Data);
			break;
		case 0x20:
		case 0x28:
			sprintf(WriteStr, "Ch %u: End Address %s 0x%02X", CurChn,
					ADDR_2S_STR[(Register & 0x08) >> 3], Data);
			break;
		default:
			sprintf(WriteStr, "Register %02X: 0x%02X", Register, Data);
			break;
		}
	}
	sprintf(TempStr, "ADPCM A: %s", WriteStr);

	return;
}

static void YM_DELTAT_ADPCM_Write(char* TempStr, UINT8 Register, UINT8 Data)
{
	switch(Register)
	{
	case 0x00:	// START,REC,MEMDATA,REPEAT,SPOFF,--,--,RESET
		sprintf(WriteStr, "Control 1: Start: %s, Record: %s, MemMode: %s, Repeat: %s, Speaker: %s, Reset: %s",
				OnOff(Data & 0x80), OnOff(Data & 0x40), (Data & 0x20) ? "Internal" : "External",
				OnOff(Data & 0x10), OnOff(~Data & 0x08), OnOff(Data & 0x01));
		break;
	case 0x01:	// L,R,-,-,SAMPLE,DA/AD,RAMTYPE,ROM
		sprintf(WriteStr, "Control 2: Stereo: %c%c, Sample: %s, DA/AD: %s, RAM/ROM Type: %s",
				(Data & 0x40) ? 'L' : '-', (Data & 0x80) ? 'R' : '-',
				OnOff(Data & 0x08), (Data & 0x04) ? "DA" : "AD", YDT_RAMTYPE[Data & 0x03]);
		break;
	case 0x02:	// Start Address L
	case 0x03:	// Start Address H
		sprintf(WriteStr, "Start Address %s 0x%02X",
				ADDR_2S_STR[Register & 0x01], Data);
		break;
	case 0x04:	// Stop Address L
	case 0x05:	// Stop Address H
		sprintf(WriteStr, "Stop Address %s 0x%02X",
				ADDR_2S_STR[Register & 0x01], Data);
		break;
	case 0x06:	// Prescale L (ADPCM and Record frq)
	case 0x07:	// Prescale H
		sprintf(WriteStr, "Record Prescale %s 0x%02X",
				ADDR_2S_STR[Register & 0x01], Data);
		break;
	case 0x08:	// ADPCM data
		sprintf(WriteStr, "ADPCM Data = %02X", Data);
		break;
	case 0x09:	// DELTA-N L (ADPCM Playback Prescaler)
	case 0x0A:	// DELTA-N H
		sprintf(WriteStr, "Playback Prescale %s 0x%02X",
				ADDR_2S_STR[~Register & 0x01], Data);
		break;
	case 0x0B:	// Output level control (volume, linear)
		sprintf(WriteStr, "Volume: 0x%02X = %u%%", Data, 100 * Data / 0xFF);
		break;
	case 0x0C:	// Limit Address L
	case 0x0D:	// Limit Address H
		sprintf(WriteStr, "Limit Address %s 0x%02X",
				ADDR_2S_STR[Register & 0x01], Data);
		break;
	case 0x0E:
		sprintf(WriteStr, "DAC Data = %02X", Data);
		break;
	default:
		sprintf(WriteStr, "Register %02X: 0x%02X", Register, Data);
		break;
	}
	sprintf(TempStr, "DELTA-T: %s", WriteStr);

	return;
}

void multipcm_write(char* TempStr, UINT8 Port, UINT8 Data)
{
	MULTIPCM_DATA* TempMPCM;

	TempMPCM = &CacheMultiPCM[ChpCur];
	WriteChipID(0x15);

	switch(Port)
	{
	case 0:		// Data write
		multipcm_WriteSlot(RedirectStr, TempMPCM->Slot, TempMPCM->Address, Data);
		break;
	case 1:
		TempMPCM->Slot = multipcm_val2chan[Data & 0x1F];
		sprintf(RedirectStr, "Channel = %d", TempMPCM->Slot);
		break;
	case 2:
		TempMPCM->Address = (Data > 7) ? 7 : Data;
		sprintf(RedirectStr, "Register = %d", TempMPCM->Address);
		break;
	default:
		sprintf(RedirectStr, "Port %02X: 0x%02X", Port, Data);
		break;
	}
	sprintf(TempStr, "%s%s", ChipStr, RedirectStr);

	return;
}

static void multipcm_WriteSlot(char* TempStr, INT8 Slot, UINT8 Register, UINT8 Data)
{
	INT8 TempSByt;

	switch(Register)
	{
	case 0:	// PANPOT
		sprintf(WriteStr, "Pan: 0x%X", Data >> 4);
		break;
	case 1:	// Sample
		sprintf(WriteStr, "Set Sample = 0x%X", Data);
		break;
	case 2:	// Pitch
		sprintf(WriteStr, "Pitch LSB = 0x%02X", Data);
		break;
	case 3:
		TempSByt = Data >> 4;
		if (TempSByt & 0x08)
			TempSByt |= 0xF0;
		sprintf(WriteStr, "Pitch MSB = 0x%01X, Octave %d", Data & 0x0F, TempSByt);
		break;
	case 4:	// KeyOn/Off (and more?)
		sprintf(WriteStr, "Key %s", OnOff(Data & 0x80));
		break;
	case 5:	// TL+Interpolation
		sprintf(WriteStr, "Total Level = %02X, Interpolate: %s",
				Data >> 1, OnOff(Data & 0x01));
		break;
	case 6:	// LFO freq+PLFO
		sprintf(WriteStr, "LFO Frequency: %u, Phase LFO: %u",
				(Data >> 3) & 0x07, Data & 0x07);
		break;
	case 7:	// ALFO
		sprintf(WriteStr, "Amplitude LFO: %u", Data & 0x07);
		break;
	}
	sprintf(TempStr, "Channel %d: %s", Slot, WriteStr);

	return;
}

void multipcm_bank_write(char* TempStr, UINT8 Port, UINT16 Data)
{
	WriteChipID(0x15);

	sprintf(WriteStr, "Set Bank %c%c to 0x%06X",
			(Port & 0x01) ? 'L' : '-', (Port & 0x02) ? 'R' : '-', Data << 16);

	sprintf(TempStr, "%s%s", ChipStr, WriteStr);

	return;
}

void upd7759_write(char* TempStr, UINT8 Port, UINT8 Data)
{
	if (Port == 0xFF)
	{
		CacheUPD7759[ChpCur].HasROM = Data ? true : false;
		return;
	}

	WriteChipID(0x16);

	switch(Port)
	{
	case 0:
		sprintf(WriteStr, "Reset: %s", Enable(! Data));
		break;
	case 1:
		sprintf(WriteStr, "Start: %s", Enable(Data));
		break;
	case 2:
		if (CacheUPD7759[ChpCur].HasROM)
			sprintf(WriteStr, "Sample = 0x%02X", Data);
		else
			sprintf(WriteStr, "FIFO = 0x%02X", Data);
		break;
	case 3:
		sprintf(WriteStr, "Set Bank 0x%06X", Data * 0x20000);
		break;
	default:
		sprintf(WriteStr, "Port %02X: 0x%02X", Port, Data);
		break;
	}
	sprintf(TempStr, "%s%s", ChipStr, WriteStr);

	return;
}

void scsp_write(char* TempStr, UINT16 Register, UINT8 Data)
{
	WriteChipID(0x16);

	if (Register < 0x400)
	{
		int slot=Register/0x20;
		Register&=0x1f;
		//*((unsigned short *) (scsp->Slots[slot].udata.datab+(Register))) = val;
		//SCSP_UpdateSlotReg(scsp,slot,Register&0x1f);
		sprintf(WriteStr, "Channel %u, Register %02X: 0x%02X", slot, Register, Data);
	}
	else if (Register < 0x600)
	{
		if (Register < 0x430)
		{
			//*((unsigned short *) (scsp->udata.datab+((Register&0x3f)))) = val;
			//SCSP_UpdateReg(scsp, Register&0x3f);
			sprintf(WriteStr, "Register %03X: 0x%02X", Register, Data);
		}
		else
		{
			sprintf(WriteStr, "Register %03X: 0x%02X", Register, Data);
		}
	}
	else if (Register < 0x700)
		sprintf(WriteStr, "Ring Buffer %02X = 0x%02X", Register & 0xFF, Data);
	else if (Register < 0x780)
		sprintf(WriteStr, "DSP COEF %02X = 0x%02X", Register & 0x7F, Data);
	else if (Register < 0x800)	// MADRS is mirrored twice: 780-7BF, 7C0-7FF
		sprintf(WriteStr, "DSP MADRS %02X = 0x%02X", Register & 0x3F, Data);
	else if(Register < 0xC00)
		sprintf(WriteStr, "DSP MPRO %03X = 0x%02X", Register & 0x3FF, Data);
	else
		sprintf(WriteStr, "Register %03X: 0x%02X", Register, Data);
	sprintf(TempStr, "%s%s", ChipStr, WriteStr);

	return;
}

void vsu_write(char* TempStr, UINT16 Register, UINT8 Data)
{
	UINT8 CurChn;
	WriteChipID(0x22);

	Register <<= 2;	// all documentation uses offsets multiplied by 4

	if (Register < 0x280)
	{
		sprintf(WriteStr, "Chn %u WaveData 0x%02X = 0x%02X",
				Register >> 7, (Register >> 2) & 0x1F, Data & 0x3F);
	}
	else if (Register < 0x400)
	{
		sprintf(WriteStr, "ModData 0x%02X = 0x%02X",
				(Register >> 2) & 0x1F, Data & 0x3F);
	}
	else if (Register < 0x580)
	{
		CurChn = (Register >> 6) & 0x0F;
		switch((Register >> 2) & 0x0F)
		{
		case 0x00:
			sprintf(RedirectStr, "IntlControl: Key %s, Timeout: %s, Time: 0x%02X",
					OnOff(Data & 0x80), Enable(Data & 0x20), Data & 0x1F);
			break;
		case 0x01:
			sprintf(RedirectStr, "Volume L: %u = %u%%, Volume R: %u = %u%%",
				Data & 0x0F, 100 * (Data & 0x0F) / 0x0F,
				(Data & 0xF0) >> 4, 100 * (Data & 0xF0) / 0xF0);
			break;
		case 0x02:
			sprintf(RedirectStr, "Frequency LSB: 0x%02X", Data);
			break;
		case 0x03:
			sprintf(RedirectStr, "Frequency MSB: 0x%02X", Data);
			break;
		case 0x04:
			sprintf(RedirectStr, "Envelope Control LSB: 0x%02X", Data);
			break;
		case 0x05:
			sprintf(RedirectStr, "Envelope Control MSB: 0x%02X", Data);
			break;
		case 0x06:
			sprintf(RedirectStr, "WaveRAM Address: 0x%02X", Data);
			break;
		case 0x07:
			if (CurChn == 4)
			{
				sprintf(RedirectStr, "Sweep Control: 0x%02X", Data);
				break;
			}
			// fall through
		default:
			sprintf(RedirectStr, "Reg 0x%02X, Data 0x%02X", Register, Data);
			break;
		}
		sprintf(WriteStr, "Ch %u %s", CurChn, RedirectStr);
	}
	else if (Register == 0x580)
	{
		sprintf(WriteStr, "Stop All Channels: %s", Enable(Data & 0x01));
	}
	else
	{
		sprintf(WriteStr, "Reg 0x%02X, Data 0x%02X", Register, Data);
	}

	sprintf(TempStr, "%s%s", ChipStr, WriteStr);

	return;
}

void saa1099_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	UINT8 CurChn;
	UINT32 StrPos;
	UINT8 ChnEn;

	WriteChipID(0x23);

	if (Register < 0x10)
	{
		CurChn = Register & 0x07;
		if (Register & 0x08)
		{
			sprintf(WriteStr, "Chn %u Freq: 0x%02X", CurChn, Data);
		}
		else
		{
			sprintf(WriteStr, "Chn %u VolL 0x%01X = %u%%, VolR 0x%01X = %u%%", CurChn,
					Data & 0x0F, 100 * (Data & 0x0F) / 0x0F,
					(Data & 0xF0) >> 4, 100 * (Data & 0xF0) / 0xF0);
		}
	}
	else
	{
		switch(Register)
		{
		case 0x10:
		case 0x11:
		case 0x12:
			CurChn = (Register & 0x03) * 2;
			sprintf(WriteStr, "Chn %u Octave: %u, Chn %u Octave: %u",
					CurChn, Data & 0x07, CurChn + 1, (Data & 0x70) >> 4);
			break;
		case 0x14:
		case 0x15:
			sprintf(WriteStr, "%s Enable: Channel ", (Register == 0x14) ? "Tone" : "Noise");
			StrPos = strlen(WriteStr);
			for (CurChn = 0; CurChn < 6; CurChn ++)
			{
				ChnEn = Data & (0x01 << (CurChn + 0));
				WriteStr[StrPos] = ChnEn ? ('0' + CurChn) : '-';
				StrPos ++;
			}
			WriteStr[StrPos] = 0x00;
			break;
		case 0x16:
			sprintf(WriteStr, "Noise Gen. 0 Param: %u, Noise Gen. 0 Param: %u",
					Data & 0x03, (Data & 0x30) >> 4);
			break;
		case 0x18:
		case 0x19:
			sprintf(WriteStr, "Envelope Gen. %u: %s, Param: 0x%02X",
					Register & 0x01, Enable(Data & 0x80), Data & 0x3F);
			break;
		case 0x1C:
			sprintf(WriteStr, "All Sound: %s, Reset: %s",
					Enable(Data & 0x01), OnOff(Data & 0x02));
			break;
		default:
			sprintf(WriteStr, "Reg 0x%02X, Data 0x%02X", Register, Data);
			break;
		}
	}

	sprintf(TempStr, "%s%s", ChipStr, WriteStr);

	return;
}

void x1_010_write(char* TempStr, UINT16 Offset, UINT8 val)
{
	UINT8 chan, pos;

	WriteChipID(0x26);

	if (Offset < 0x80)
	{
		chan = (Offset >> 3) & 0xfff;
		switch(Offset & 0x7)
		{
		case 0x0:
			sprintf(WriteStr, "Ch %u, Flags %02x", chan, val);
			if(val & 0x80)
				sprintf(WriteStr, "%s, Divide Frequency", WriteStr);
			if(val & 0x04)
				sprintf(WriteStr, "%s, Loop", WriteStr);
			if(val & 0x02)
				sprintf(WriteStr, "%s, Waveform", WriteStr);
			else
				sprintf(WriteStr, "%s, PCM", WriteStr);
			if(val & 0x01)
				sprintf(WriteStr, "%s, Key On", WriteStr);
			else
				sprintf(WriteStr, "%s, Key Off", WriteStr);
			break;
		case 0x1:
			sprintf(WriteStr, "Ch %u, Volume: %02X", chan, val);
			break;
		case 0x2:
			sprintf(WriteStr, "Ch %u, PCM Frequency/Waveform Pitch Lo: %02X", chan, val);
			break;
		case 0x3:
			sprintf(WriteStr, "Ch %u, Waveform Pitch Hi: %02X", chan, val);
			break;
		case 0x4:
			sprintf(WriteStr, "Ch %u, PCM Start/Envelope Time: %02X", chan, val);
			break;
		case 0x5:
			sprintf(WriteStr, "Ch %u, PCM End/Envelope No: %02X", chan, val);
			break;
		default:
			sprintf(WriteStr, "Ch %d, Register %02X: %02X", chan, Offset&0x07, val);
			break;
		}
	}
	else
	{
		chan = (Offset>>7)&0x1f;
		pos = Offset&0x7f;

		if(Offset < 0x1000)
		{
			sprintf(WriteStr, "Envelope %u, position %02x: %02X", chan, pos, val);
		}
		else if(Offset < 0x2000)
		{
			sprintf(WriteStr, "Waveform %u, position %02x: %02X", chan, pos, val);
		}
		else
			sprintf(WriteStr, "Offset 0x%04X, Data 0x%2X", Offset, val);


	}
	sprintf(TempStr, "%s%s", ChipStr, WriteStr);

	return;
}

void c352_write(char* TempStr, UINT16 Offset, UINT16 val)
{
	UINT16 address = Offset* 2;
	UINT8 chan;

	WriteChipID(0x27);

	if (address < 0x200)
	{
		chan = (address >> 4) & 0xfff;
		switch(address & 0xf)
		{
		case 0x0:	// volumes (output 1)
			sprintf(WriteStr, "Ch %u, volume: L=%02X = %u%%, R=%02X = %u%%", chan,
					val&0xff, 100 * (val&0x00ff) / 0x00ff,
					val>>8, 100 * (val&0xff00) / 0xff00);
			break;
		case 0x2:	// volumes (output 2)
			sprintf(WriteStr, "Ch %u, volume: BL=%02X = %u%%, BR=%02X = %u%%", chan,
					val&0xff, 100 * (val&0x00ff) / 0x00ff,
					val>>8, 100 * (val&0xff00) / 0xff00);
			break;
		case 0x4:	// pitch
			sprintf(WriteStr, "Ch %u, pitch: %04X", chan, val);
			break;
		case 0x6:	// flags
			sprintf(WriteStr, "Ch %u, flags: %04X", chan, val);
			break;
		case 0x8:	// bank (bits 16-31 of address);
			sprintf(WriteStr, "Ch %u, bank: %02X", chan, val&0xff);
			break;
		case 0xa:	// start address
			sprintf(WriteStr, "Ch %u, start: address %04X", chan, val&0xffff);
			break;
		case 0xc:	// end address
			sprintf(WriteStr, "Ch %u, end address: %04X", chan, val&0xffff);
			break;
		case 0xe:	// loop address
			sprintf(WriteStr, "Ch %u, loop address: %04X", chan, val&0xffff);
			break;
		default:
			sprintf(WriteStr, "Reg 0x%04X, Data 0x%04X", Offset, val);
			break;
		}
	}
	else
	{
		switch(address)
		{
		case 0x404:	// execute key-ons/offs
			sprintf(WriteStr, "Update key-ons");
			break;
		default:
			sprintf(WriteStr, "Reg 0x%04X, Data 0x%04X", Offset, val);
			break;
		}
	}
	sprintf(TempStr, "%s%s", ChipStr, WriteStr);

	return;
}

void es5503_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	UINT8 CurChn;

	WriteChipID(0x24);

	if (Register < 0xE0)
	{
		CurChn = Register & 0x1F;
		switch(Register & 0xE0)
		{
		case 0x00:
			sprintf(RedirectStr, "Frequency LSB: 0x%02X", Data);
			break;
		case 0x20:
			sprintf(RedirectStr, "Frequency MSB: 0x%02X", Data);
			break;
		case 0x40:
			sprintf(RedirectStr, "Volume: %02X = %u%%", Data, 100 * Data / 0xFF);
			break;
		case 0x60:
			sprintf(RedirectStr, "Data: 0x%02X", Data);
			break;
		case 0x80:
			sprintf(RedirectStr, "WaveRAM Address: 0x%04X", Data << 8);
			break;
		case 0xA0:
			sprintf(RedirectStr, "Osc. Control: Channel %u, IRQ %s, Mode %s, Osc. %s",
					(Data & 0xF0) >> 4, Enable(Data & 0x08),
					ES5503_MODES[(Data & 0x06) >> 1], Enable(~Data & 0x01));
			break;
		case 0xC0:
			sprintf(RedirectStr, "Bank %X, WaveTblSize: 0x%02X bytes, Resolution: %u",
					(Data & 0x40) >> 6, 0x100 << ((Data & 0x38) >> 3), 9 + (Data & 0x07));
			break;
		default:
			sprintf(RedirectStr, "Reg 0x%02X, Data 0x%02X", Register, Data);
			break;
		}
		sprintf(WriteStr, "Ch %u %s", CurChn, RedirectStr);
	}
	else
	{
		switch(Register)
		{
		case 0xE0:
			sprintf(WriteStr, "Interrupt Status (ignored)");
			break;
		case 0xE1:
			sprintf(WriteStr, "Enabled Oscillators: %u", 1 + ((Data >> 1) & 0x1F));
			break;
		default:
			sprintf(WriteStr, "Reg 0x%02X, Data 0x%02X", Register, Data);
			break;
		}
	}

	sprintf(TempStr, "%s%s", ChipStr, WriteStr);

	return;
}

void es5506_w(char* TempStr, UINT8 offset, UINT8 data)
{
	if (offset == 0xFF)
	{
		CacheES5506[ChpCur].Mode = data;
		return;
	}

	WriteChipID(0x25);
	//opn_write(RedirectStr, OPN_YM2203, 0x00, Register, Data);

	if (offset < 0x40)
	{
		/*if (! CacheES5506[ChpCur].Mode)
			es5505_w(chip, offset, data);
		else
			es5506_w(chip, offset, data);*/
	}
	else
	{
		sprintf(RedirectStr, "Set Voice %u Bank: 0x%06X", offset & 0x1F, data << 20);
	}
	sprintf(TempStr, "%s%s", ChipStr, RedirectStr);

	return;
}

void es5506_w16(char* TempStr, UINT8 offset, UINT16 data)
{
	if (offset < 0x40)
	{
		/*if (! CacheES5506[ChpCur].Mode)
		{
			es5505_w(chip, offset | 0, (data & 0xFF00) >> 8);
			es5505_w(chip, offset | 1, (data & 0x00FF) >> 0);
		}
		else
		{
			es5506_w(chip, offset | 0, (data & 0xFF00) >> 8);
			es5506_w(chip, offset | 1, (data & 0x00FF) >> 0);
		}*/
	}
	else
	{
		sprintf(RedirectStr, "Set Voice %u Bank: 0x%06X", offset & 0x1F, data << 20);
	}
	return;
}

void k054539_write(char* TempStr, UINT16 Register, UINT8 Data)
{
	UINT8 CurChn;
	UINT8 CurReg;
	UINT32 StrPos;
	UINT8 ChnEn;

	WriteChipID(0x1A);

	if (Register < 0x100)
	{
		CurChn = (Register & 0xE0) >> 5;
		CurReg = (Register & 0x1F) >> 0;
		switch(CurReg)
		{
		case 0x00:	// pitch low
		case 0x01:	// pitch mid
		case 0x02:	// pitch high
			sprintf(RedirectStr, "Set Freq. Step %s: 0x%02X", ADDR_3S_STR[CurReg - 0x00], Data);
			break;
		case 0x03: // volume
			sprintf(RedirectStr, "Set Volume: 0x%02X = %u%%",
					Data, GetLogVolPercent(Data, 11, 0xFF));	// 0x10 = -9 db
			break;
		case 0x04: // reverb volume
			sprintf(RedirectStr, "Set Reverb Volume: 0x%02X = %u%%",
					Data, GetLogVolPercent(Data, 11, 0xFF));	// 0x10 = -9 db
			break;
		case 0x05:	// pan
			sprintf(RedirectStr, "Set Pan: 0x%02X", Data);
			break;
		case 0x06:	// reverb delay low
		case 0x07:	// reverb delay high
			sprintf(RedirectStr, "Set Reverb Delay %s: 0x%01X", ADDR_2S_STR[CurReg - 0x06], Data);
			break;
		case 0x08:	// loop offset low
		case 0x09:	// loop offset mid
		case 0x0A:	// loop offset high
			sprintf(RedirectStr, "Set Loop Offset %s: 0x%02X", ADDR_3S_STR[CurReg - 0x08], Data);
			break;
		case 0x0C:	// start offset low
		case 0x0D:	// start offset mid
		case 0x0E:	// start offset high
			sprintf(RedirectStr, "Set Start Offset %s: 0x%02X", ADDR_3S_STR[CurReg - 0x0C], Data);
			break;
		default:
			sprintf(WriteStr, "Register %02X: 0x%02X", Register, Data);
			break;
		}

		sprintf(WriteStr, "Ch %u: %s", CurChn, RedirectStr);
	}
	else if (Register >= 0x200 && Register < 0x210)
	{
		CurChn = (Register & 0x0E) >> 1;
		CurReg = (Register & 0x01) >> 0;
		switch(CurReg)
		{
		case 0:
			sprintf(RedirectStr, "Direction: %s, Sample Mode: %s",
				(Data & 0x20) ? "Backwards" : "Forwards",
				K054539_SAMPLE_MODES[(Data & 0x0C) >> 2], Data);
			break;
		case 1:
			sprintf(RedirectStr, "Loop %s", OnOff(Data & 0x01));
			break;
		}

		sprintf(WriteStr, "Ch %u: %s", CurChn, RedirectStr);
	}
	else
	{
		switch(Register)
		{
		case 0x214:	// key on
		case 0x215:	// key off
			sprintf(WriteStr, "Key %s: ", OnOff(~Register & 0x01));
			StrPos = strlen(WriteStr);
			for (CurChn = 0x00; CurChn < 0x08; CurChn ++)
			{
				ChnEn = Data & (1 << CurChn);
				WriteStr[StrPos] = ChnEn ? ('0' + CurChn) : '-';
				StrPos ++;
			}
			WriteStr[StrPos] = 0x00;
			break;
		case 0x227:	// timer
			sprintf(WriteStr, "Timer: %02X", Data);
			break;
		case 0x22C:	// channel enable
			sprintf(WriteStr, "Channel Enable [read only]: %02X", Data);
			break;
		case 0x22D:	// data read/write
			sprintf(WriteStr, "RAM Write: 0x%02X", Data);
			break;
		case 0x22E:	// ROM/RAM select
			sprintf(WriteStr, "Select %s Bank: 0x%06X",
				(Data & 0x80) ? "RAM" : "ROM", (Data & 0x7F) * 0x20000);
			break;
		case 0x22F:	// global control
			sprintf(WriteStr, "Control Reg: Lock Registers: %s, Timer %s, ROM/RAM Readback: %s "
					"Unknown Bit: %s, Sound Output: %s",
					OnOff(Data & 0x80), Enable(Data & 0x20), Enable(Data & 0x10),
					OnOff(Data & 0x02), OnOff(Data & 0x01));
			break;
		default:
			sprintf(WriteStr, "Register %02X: 0x%03X", Register, Data);
			break;
		}
	}

	sprintf(TempStr, "%s%s", ChipStr, WriteStr);

	return;
}

void wswan_write(char* TempStr, UINT8 Register, UINT8 Data)
{
	WSWAN_DATA* TempWS;
	UINT8 Port;
	UINT8 CurChn;
	UINT32 StrPos;
	UINT8 ChnEn;

	TempWS = &CacheWSwan[ChpCur];
	WriteChipID(0x21);

	Port = 0x80 + Register;
	switch(Port)
	{
	case 0x80:	// Ch 0 Freq. LSB
	case 0x81:	// Ch 0 Freq. MSB
	case 0x82:	// Ch 1 Freq. LSB
	case 0x83:	// Ch 1 Freq. MSB
	case 0x84:	// Ch 2 Freq. LSB
	case 0x85:	// Ch 2 Freq. MSB
	case 0x86:	// Ch 3 Freq. LSB
	case 0x87:	// Ch 3 Freq. MSB
		CurChn = (Port >> 1) & 0x03;
		sprintf(WriteStr, "Ch %u: Set Freq. Step %s: 0x%02X",
			CurChn, ADDR_2S_STR[Port & 0x01], Data);
		break;
	case 0x88:	// Ch 0 volume
	case 0x89:	// Ch 1 volume
	case 0x8A:	// Ch 2 volume
	case 0x8B:	// Ch 3 volume
		CurChn = Port & 0x03;
		if (CurChn == 1 && TempWS->pcmEnable)
		{
			sprintf(WriteStr, "Ch %u PCM Data = %02X", CurChn, Data);
		}
		else
		{
			UINT8 volL = (Data >> 4) & 0x0F;
			UINT8 volR = (Data >> 0) & 0x0F;
			sprintf(WriteStr, "Ch %u Volume L: %X = %u%%, Volume R: %X = %u%%",
					CurChn, volL, 100 * volL / 0x0F, volR, 100 * volR / 0x0F);
		}
		break;
	case 0x8C:	// Sweep Step
		sprintf(WriteStr, "Sweep Step: %+d", (INT8)Data);
		break;
	case 0x8D:	// Sweep Time
		sprintf(WriteStr, "Sweep Time: %u", Data + 1);
		break;
	case 0x8E:	// Noise Type
		sprintf(WriteStr, "Noise Type: %u, Reset: %s",
			Data & 0x07, OnOff(Data & 0x08));
		break;
	case 0x8F:	// Waveform Address
		sprintf(WriteStr, "Waveform Base Address: 0x%04X", Data << 6);
		break;
	case 0x90:	// SNDMOD
		sprintf(WriteStr, "Channel Enable: ");
		StrPos = strlen(WriteStr);
		for (CurChn = 0x00; CurChn < 0x04; CurChn ++)
		{
			ChnEn = Data & (0x01 << CurChn);
			WriteStr[StrPos] = ChnEn ? ('0' + CurChn) : '-';
			StrPos ++;
		}
		sprintf(WriteStr + StrPos, ", Ch 1 PCM: %s, Sweep: %s, Ch 3 Noise: %s",
			OnOff(Data & 0x20), Enable(Data & 0x40), OnOff(Data & 0x80));
		TempWS->pcmEnable = !!(Data & 0x20);
		break;
	case 0x91:	// SNDOUT
		sprintf(WriteStr, "SNDOUT = 0x%02X", Data);
		break;
	//case 0x92:	// PCSRL
	//case 0x93:	// PCSRH
	case 0x94:	// PCM Volume
		CurChn = 1;
		{
			UINT8 volL = (Data >> 2) & 0x03;
			UINT8 volR = (Data >> 0) & 0x03;
			sprintf(WriteStr, "Ch %u PCM Volume L: %X = %u%%, Volume R: %X = %u%%",
					CurChn, volL, 100 * volL / 0x03, volR, 100 * volR / 0x03);
		}
		break;
	default:
		sprintf(WriteStr, "Register %02X: 0x%03X", Register, Data);
		break;
	}

	sprintf(TempStr, "%s%s", ChipStr, WriteStr);

	return;
}

void ws_mem_write(char* TempStr, UINT16 Offset, UINT8 Data)
{
	WriteChipID(0x21);
	sprintf(TempStr, "%sMem 0x%04X = 0x%02X", ChipStr, Offset, Data);

	return;
}

void k007232_write(char* TempStr, UINT8 offset, UINT8 data)
{
    char WriteStr[128], RedirectStr[96];
    UINT8 ch = (offset >= 6 && offset < 0x0C) ? 1 : 0; // Channel 0 or 1
    UINT8 reg = offset % 6; // offset offset within the channel

	WriteChipID(0x2A);
	
    if (offset < 0x06 || (offset >= 6 && offset < 0x0C))
    {
        // offsets for pitch, start address, and key on/off
        switch (reg)
        {
        case 0: // Pitch LSB
            sprintf(RedirectStr, "Pitch LSB: 0x%02X", data);
            break;
        case 1: // Pitch MSB + frequency mode
        {
            static const char* freq_modes[4] = { "12-bit", "8-bit", "4-bit", "reserved" };
            int freq_mode = (data >> 4) & 3;
            sprintf(RedirectStr, "Pitch MSB: 0x%01X, Freq Mode: %s",
                data & 0x0F, freq_modes[freq_mode]);
            break;
        }
        case 2: // Start address LSB
            sprintf(RedirectStr, "Start Address LSB: 0x%02X", data);
            break;
        case 3: // Start address MID
            sprintf(RedirectStr, "Start Address MID: 0x%02X", data);
            break;
        case 4: // Start address MSB (bit 0 only)
            sprintf(RedirectStr, "Start Address MSB (bit 0): %u", data & 0x01);
            break;
        case 5: // Key on/off trigger
            sprintf(RedirectStr, "Key On/Off Trigger: %s", (data & 1) ? "Start" : "Stop");
            break;
        }
        sprintf(WriteStr, "Ch%u: %s", ch, RedirectStr);
    }
    else
    {
        // offsets for external control, loop enable, and bankswitching
        switch (offset)
        {
        case 0x0C: // External port write (volume/pan control, typically)
            sprintf(WriteStr, "External Port Write: 0x%02X", data);
            break;
        case 0x0D: // Loop enable for channels
            sprintf(WriteStr, "Loop Enable: Ch0: %s, Ch1: %s",
                (data & 0x01) ? "On" : "Off",
                (data & 0x02) ? "On" : "Off");
            break;
        case 0x10: // Left volume for Channel 0
            sprintf(WriteStr, "Left Volume (Ch0): 0x%02X", data);
            break;
        case 0x11: // Right volume for Channel 0
            sprintf(WriteStr, "Right Volume (Ch0): 0x%02X", data);
            break;
        case 0x12: // Left volume for Channel 1
            sprintf(WriteStr, "Left Volume (Ch1): 0x%02X", data);
            break;
        case 0x13: // Right volume for Channel 1
            sprintf(WriteStr, "Right Volume (Ch1): 0x%02X", data);
            break;
        case 0x14: // Bankswitch for Channel 0
            sprintf(WriteStr, "Bankswitch (Ch0): 0x%02X", data);
            break;
        case 0x15: // Bankswitch for Channel 1
            sprintf(WriteStr, "Bankswitch (Ch1): 0x%02X", data);
            break;
		case 0x1F: // Special command for k007232 read
			sprintf(WriteStr, "Chip read: 0x%2X", data);
			break;
        default:
            sprintf(WriteStr, "Unknown Register 0x%02X: data 0x%02X", offset, data);
            break;
        }
    }

    // Final output string
    sprintf(TempStr, "K007232: %s", WriteStr);
}