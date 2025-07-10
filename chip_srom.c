// TODO: Check sample end < sample start

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "stdtype.h"
#include "stdbool.h"


#define SPCM_BANK_256		(11)
#define SPCM_BANK_512		(12)
#define SPCM_BANK_12M		(13)
#define SPCM_BANK_MASK7		(0x70 << 16)
#define SPCM_BANK_MASKF		(0xF0 << 16)
#define SPCM_BANK_MASKF8	(0xF8 << 16)

// ADPCM type A channel struct
typedef struct
{
	UINT8 flag;			// port state
	UINT32 start;	// sample data start address
	UINT32 end;		// sample data end address
} ADPCM_CH;
#define ADPCMA_ADDRESS_SHIFT	8	// adpcm A address shift

// DELTA-T (adpcm type B) struct
typedef struct deltat_adpcm_state	// AT: rearranged and tigntened structure
{
	UINT8 *memory;
	UINT8 *memory_usg;
	UINT32 memory_size;
	UINT32 start;		// start address
	//UINT32 limit;		// limit address
	UINT32 end;			// end address
	UINT8 portstate;		// port status
	UINT8 control2;			// control reg: SAMPLE, DA/AD, RAM TYPE (x8bit / x1bit), ROM/RAM
	UINT8 portshift;		// address bits shift-left:
							//		** 8 for YM2610,
							//		** 5 for Y8950 and YM2608
	UINT8 DRAMportshift;	// address bits shift-right:
							//		** 0 for ROM and x8bit DRAMs,
							//		** 3 for x1 DRAMs
	UINT8 reg[16];			// adpcm registers
	UINT8 emulation_mode;	// which chip we're emulating
} YM_DELTAT;
#define YM_DELTAT_MODE_NORMAL	0
#define YM_DELTAT_MODE_YM2610	1
static const UINT8 dram_rightshift[4] = {3, 0, 0, 0};


typedef struct segapcm_data
{
	UINT32 intf_bank;
	UINT8 bankshift;
	UINT32 bankmask;
	UINT32 rgnmask;
	UINT32 ROMSize;
	UINT8* ROMData;
	UINT8* ROMUsage;
	UINT8 RAMData[0x800];
} SEGAPCM_DATA;

typedef struct ym2608_data
{
	UINT8 RegData[0x200];
	UINT32 DT_ROMSize;
	UINT8* DT_ROMData;
	UINT8* DT_ROMUsage;
	YM_DELTAT DeltaT;	// Delta-T ADPCM unit
} YM2608_DATA;

typedef struct ym2610_data
{
	UINT8 RegData[0x200];
	UINT32 AP_ROMSize;
	UINT8* AP_ROMData;
	UINT8* AP_ROMUsage;
	ADPCM_CH ADPCM[6];					// adpcm channels
	UINT32 ADPCMReg[0x30];	// registers
	UINT32 DT_ROMSize;
	UINT8* DT_ROMData;
	UINT8* DT_ROMUsage;
	YM_DELTAT DeltaT;	// Delta-T ADPCM unit
} YM2610_DATA;

typedef struct y8950_data
{
	UINT8 RegData[0x100];
	UINT32 DT_ROMSize;
	UINT8* DT_ROMData;
	UINT8* DT_ROMUsage;
	YM_DELTAT DeltaT;	// Delta-T ADPCM unit
} Y8950_DATA;

typedef struct YMZ280BVoice
{
	UINT8 keyon;
	UINT32 start;
	UINT32 stop;
	UINT32 loop_start;
	UINT32 loop_end;
} YMZ_VOICE;
typedef struct ymz280b_data
{
	YMZ_VOICE Voice[8];
	UINT32 ROMSize;
	UINT8* ROMData;
	UINT8* ROMUsage;
} YMZ280B_DATA;

typedef struct ymf271_slot
{
	UINT8 waveform;

	UINT32 startaddr;
	UINT32 loopaddr;
	UINT32 endaddr;

	UINT8 active;
	UINT8 bits;
	UINT8 IsPCM;
} YMF271_SLOT;
typedef struct ymf271_group
{
	UINT8 sync;	// 0/1 - FM only, 2 - FM+PCM, 3 - PCM only
} YMF271_GROUP;
typedef struct ymz271_data
{
	YMF271_SLOT slots[48];
	YMF271_GROUP groups[12];

	UINT32 ROMSize;
	UINT8* ROMData;
	UINT8* ROMUsage;
} YMF271_DATA;

typedef struct rf5c_pcm_channel
{
	UINT8 enable;
	UINT16 start;
	UINT16 loopst;
	UINT16 step;
} RF5C_CHANNEL;
typedef struct rf5c_data
{
	RF5C_CHANNEL Channel[8];
	UINT8 SelChn;
	UINT16 SelBank;
	UINT32 RAMSize;
	UINT8* RAMData;
	UINT8* RAMUsage;
} RF5C_DATA;

#define OKIM6295_VOICES		4
typedef struct okim_voice
{
	bool		playing;
	UINT32		start;
	UINT32		stop;
} OKIM_VOICE;
typedef struct okim6295_data
{
	OKIM_VOICE	voice[OKIM6295_VOICES];
	UINT8		command;
	UINT8		nmk_mode;
	UINT32		bank_offs;
	UINT8		nmk_bank[4];

	UINT32 ROMSize;
	UINT8* ROMData;
	UINT8* ROMUsage;
} OKIM6295_DATA;

enum
{
	C140_TYPE_SYSTEM2,
	C140_TYPE_SYSTEM21,
	C140_TYPE_ASIC219
};
//static const INT16 asic219banks[4] = {0x07, 0x01, 0x03, 0x05};
#define C140_MAX_VOICE 24
typedef struct
{
	UINT8 bank;
	UINT8 mode;
	UINT8 start_msb;
	UINT8 start_lsb;
	UINT8 end_msb;
	UINT8 end_lsb;

	UINT32 smpl_start;
	UINT32 smpl_end;
} C140_VOICE;
typedef struct _c140_state
{
	int banking_type;
	UINT32 ROMSize;
	UINT8* ROMData;
	UINT8* ROMUsage;

	UINT8 BankRegs[0x10];
	C140_VOICE voi[C140_MAX_VOICE];
} C140_DATA;

#define QSOUND_CHANNELS 19 /* 16pcm + 3adpcm */
typedef struct _qsound_channel
{
	UINT8 bank;		// bank (x16)
	INT32 address;	// start address
	INT32 end;		// end address

	UINT8 key;		// Key on / key off
} QSOUND_CHANNEL;
typedef struct qsound_data
{
	QSOUND_CHANNEL channel[QSOUND_CHANNELS];
	UINT16 data;	// register latch data

	UINT32 ROMSize;
	UINT8* ROMData;
	UINT8* ROMUsage;
} QSOUND_DATA;

#define K054539_RESET_FLAGS		0
#define K054539_REVERSE_STEREO	1
#define K054539_DISABLE_REVERB	2
#define K054539_UPDATE_AT_KEYON	4
typedef struct _k054539_channel
{
	UINT8 pos_reg[3];
	UINT8 pos_latch[3];
	UINT8 mode_reg;
	bool key_on;
} K054539_CHANNEL;
typedef struct _k054539_state
{
	//UINT8 regs[0x230];
	UINT8 flags;
	UINT8 reg_enable;

	UINT32 ROMSize;
	UINT8* ROMData;
	UINT8* ROMUsage;

	K054539_CHANNEL channels[8];
} K054539_DATA;

typedef struct _k053260_channel
{
	UINT16		size;
	UINT16		start;
	UINT8		bank;
	bool		play;
//	UINT8		ppcm; // packed PCM (4 bit signed)
} K053260_CHANNEL;
typedef struct k053260_data
{
	UINT8	mode;

	UINT32 ROMSize;
	UINT8* ROMData;
	UINT8* ROMUsage;

	K053260_CHANNEL	channels[4];
} K053260_DATA;

typedef struct upd7759_data
{
	UINT8		fifo_in;		// last data written to the sound chip
	UINT8		reset;			// current state of the RESET line
	UINT8		start;			// current state of the START line

	//INT8		state;			// current overall chip state
	UINT32		romoffset;		// ROM offset to make save/restore

	UINT32 ROMSize;
	UINT8* ROMData;
	UINT8* ROMUsage;
} UPD7759_DATA;

typedef struct nes_apu_data
{
	UINT32 ROMSize;
	UINT8* ROMData;
	UINT8* ROMUsage;
	UINT8 RegData[0x20];
} NES_APU_DATA;

typedef struct _multipcm_slot
{
	UINT16 SmplID;
	UINT8 Playing;
} MULTIPCM_SLOT;
typedef struct multipcm_data
{
	UINT32 ROMSize;
	UINT8* ROMData;
	UINT8* ROMUsage;
	UINT8 SmplMask[0x200];	// Bit 7 - Sample Table, Bit 0 - Sample Data

	UINT8 SegaBanking;
	UINT32 Bank0;
	UINT32 Bank1;

	UINT8 CurSlot;
	UINT8 Address;
	MULTIPCM_SLOT slot[28];
} MULTIPCM_DATA;

#define SETA_NUM_CHANNELS 16

// don't remove anything, size of this struct is used when reading channel regs
typedef struct
{
	UINT8 status;
	UINT8 volume;		//        volume / wave form no.
	UINT8 frequency;	//     frequency / pitch lo
	UINT8 pitch_hi;		//      reserved / pitch hi
	UINT8 start;		// start address / envelope time
	UINT8 end;			//   end address / envelope no.
	UINT8 reserve[2];
} X1_010_CHANNEL;
typedef struct
{
	UINT32 lastStart;
	UINT32 lastEnd;
} X1_010_CHNINFO;

typedef struct x1010_data
{
	UINT32 ROMSize;
	INT8 *ROMData;
	UINT8* ROMUsage;

	UINT8 reg[0x2000];
	X1_010_CHNINFO chnInfo[SETA_NUM_CHANNELS];
} X1_010_DATA;

#define K007232_PCM_MAX   2

typedef struct {
    UINT8  vol[2];    // [0]=left, [1]=right
    UINT32 addr;      // current PCM address (17 bits)
    INT32  counter;
    UINT32 start;     // start address (17 bits)
    UINT16 step;      // frequency/step value (12 bits)
    UINT32 bank;      // base bank address (upper bits, shifted left by 17)
    UINT8  play;      // playing flag
    UINT8  mute;
} K007232_Channel;

typedef struct k007232_data
{
    K007232_Channel channel[K007232_PCM_MAX];
    UINT8 wreg[0x10];
    UINT32 ROMSize;
    UINT8* ROMData;
    UINT8* ROMUsage;
    UINT8 loop_en;
} K007232_DATA;

enum {
	C352_FLG_BUSY       = 0x8000,   // channel is busy
	C352_FLG_KEYON      = 0x4000,   // Keyon
	C352_FLG_KEYOFF     = 0x2000,   // Keyoff
	C352_FLG_LOOPTRG    = 0x1000,   // Loop Trigger
	C352_FLG_LOOPHIST   = 0x0800,   // Loop History
	C352_FLG_FM         = 0x0400,   // Frequency Modulation
	C352_FLG_PHASERL    = 0x0200,   // Rear Left invert phase 180 degrees
	C352_FLG_PHASEFL    = 0x0100,   // Front Left invert phase 180 degrees
	C352_FLG_PHASEFR    = 0x0080,   // invert phase 180 degrees (e.g. flip sign of sample)
	C352_FLG_LDIR       = 0x0040,   // loop direction
	C352_FLG_LINK       = 0x0020,   // "long-format" sample (can't loop, not sure what else it means)
	C352_FLG_NOISE      = 0x0010,   // play noise instead of sample
	C352_FLG_MULAW      = 0x0008,   // sample is mulaw instead of linear 8-bit PCM
	C352_FLG_FILTER     = 0x0004,   // don't apply filter
	C352_FLG_REVLOOP    = 0x0003,   // loop backwards
	C352_FLG_LOOP       = 0x0002,   // loop forward
	C352_FLG_REVERSE    = 0x0001,   // play sample backwards
};

typedef struct
{
	UINT8   bank;
	UINT16  start_addr;
	UINT16  end_addr;
	UINT16  repeat_addr;
	UINT32  flag;

	UINT16  start;
	UINT16  repeat;
	//UINT32  current_addr;
	UINT32  pos;
} C352_CHANNEL;
typedef struct c352_data
{
	UINT32 ROMSize;
	INT8 *ROMData;
	UINT8* ROMUsage;

	C352_CHANNEL channels[32];
} C352_DATA;

typedef struct
{
	UINT32 start;
	UINT32 end;
	//UINT8 play;
} GA20_CHANNEL;
typedef struct ga20_data
{
	UINT32 ROMSize;
	UINT8 *ROMData;
	UINT8* ROMUsage;

	GA20_CHANNEL channels[4];
} GA20_DATA;

static const UINT32 es5503_wavemasks[8] =
{	0x1FF00, 0x1FE00, 0x1FC00, 0x1F800, 0x1F000, 0x1E000, 0x1C000, 0x18000};
typedef struct
{
	UINT32 wavetblpointer;
	UINT16 wtsize;
	UINT8  control;
	UINT8  vol;
	UINT8  wavetblsize;
	//UINT8  resolution;
} ES5503_OSC;
typedef struct es5503_data
{
	UINT32 ROMSize;
	UINT8* ROMData;
	UINT8* ROMUsage;

	ES5503_OSC oscillators[32];
} ES5503_DATA;

typedef struct _ymf278b_slot
{
	UINT16 SmplID;
	UINT8 Playing;
} YMF278B_SLOT;
typedef struct ymf278b_data
{
	UINT32 ROMSize;
	UINT8* ROMData;
	UINT8* ROMUsage;
	UINT32 RAMBase;
	UINT32 RAMSize;
	UINT8* RAMData;
	UINT8* RAMUsage;
	UINT8 SmplMask[0x200];	// Bit 7 - Sample Table, Bit 0 - Sample Data

	YMF278B_SLOT slots[24];
	INT8 wavetblhdr;
	INT8 memmode;
} YMF278B_DATA;

#define ES6CTRL_BS1				0x8000
#define ES6CTRL_BS0				0x4000
#define ES6CTRL_CMPD			0x2000
#define ES6CTRL_CA2				0x1000
#define ES6CTRL_CA1				0x0800
#define ES6CTRL_CA0				0x0400
#define ES6CTRL_LP4				0x0200
#define ES6CTRL_LP3				0x0100
#define ES6CTRL_IRQ				0x0080
#define ES6CTRL_DIR				0x0040
#define ES6CTRL_IRQE			0x0020
#define ES6CTRL_BLE				0x0010
#define ES6CTRL_LPE				0x0008
#define ES6CTRL_LEI				0x0004
#define ES6CTRL_STOP1			0x0002
#define ES6CTRL_STOP0			0x0001

#define ES6CTRL_BSMASK			(ES6CTRL_BS1 | ES6CTRL_BS0)
#define ES6CTRL_CAMASK			(ES6CTRL_CA2 | ES6CTRL_CA1 | ES6CTRL_CA0)
#define ES6CTRL_LPMASK			(ES6CTRL_LP4 | ES6CTRL_LP3)
#define ES6CTRL_LOOPMASK		(ES6CTRL_BLE | ES6CTRL_LPE)
#define ES6CTRL_STOPMASK		(ES6CTRL_STOP1 | ES6CTRL_STOP0)
typedef struct es5506_voice
{
	UINT32	control;	// control register
	UINT32	start;		// start register
	UINT32	end;		// end register
	UINT32	accum;		// accumulator register
	UINT32	exbank;		// external address bank
} ES5506_VOICE;
typedef struct es5506_rom
{
	UINT32 ROMSize;
	UINT8* ROMData;
	UINT8* ROMUsage;
} ES5506_ROM;
typedef struct es5506_data
{
	ES5506_ROM Rgn[4];	// ROM regions

	UINT32 writeLatch;
	UINT8 chipType;		// 0 - ES5505, 1 - ES5506
	UINT8 curPage;
	UINT8 voiceCount;
	ES5506_VOICE voice[32];
} ES5506_DATA;

typedef struct all_chips
{
	SEGAPCM_DATA SegaPCM;
	YM2608_DATA YM2608;
	YM2610_DATA YM2610;
	Y8950_DATA Y8950;
	YMZ280B_DATA YMZ280B;
	RF5C_DATA RF5C68;
	RF5C_DATA RF5C164;
	YMF278B_DATA YMF278B;
	YMF271_DATA YMF271;
	NES_APU_DATA NES_APU;
	UPD7759_DATA UPD7759;
	OKIM6295_DATA OKIM6295;
	MULTIPCM_DATA MultiPCM;
	K054539_DATA K054539;
	C140_DATA C140;
	K053260_DATA K053260;
	QSOUND_DATA QSound;
	ES5503_DATA ES5503;
	ES5506_DATA ES5506;
	X1_010_DATA X1_010;
	C352_DATA C352;
	GA20_DATA GA20;
	K007232_DATA K007232;
} ALL_CHIPS;

void InitAllChips(void);
void FreeAllChips(void);
void SetChipSet(UINT8 ChipID);
void segapcm_mem_write(UINT16 Offset, UINT8 Data);
void ym2608_write(UINT8 Port, UINT8 Register, UINT8 Data);
void ym2610_write(UINT8 Port, UINT8 Register, UINT8 Data);
void y8950_write(UINT8 Register, UINT8 Data);
static void FM_ADPCMAWrite(YM2610_DATA *F2610, UINT8 r, UINT8 v);
static void YM_DELTAT_ADPCM_Write(YM_DELTAT *DELTAT, UINT8 r, UINT8 v);
void ymz280b_write(UINT8 Register, UINT8 Data);
static void rf5c_write(RF5C_DATA* chip, UINT8 Register, UINT8 Data);
void rf5c68_write(UINT8 Register, UINT8 Data);
void rf5c164_write(UINT8 Register, UINT8 Data);
static void ymf271_write_fm_reg(YMF271_DATA* chip, UINT8 SlotNum, UINT8 Register, UINT8 Data);
static void ymf271_write_fm(YMF271_DATA* chip, UINT8 Port, UINT8 Register, UINT8 Data);
void ymf271_write(UINT8 Port, UINT8 Register, UINT8 Data);
void nes_apu_write(UINT8 Register, UINT8 Data);
void multipcm_write(UINT8 Port, UINT8 Data);
void multipcm_bank_write(UINT8 Bank, UINT16 Data);
void upd7759_write(UINT8 Port, UINT8 Data);
void okim6295_write(UINT8 Offset, UINT8 Data);
void k007232_write(UINT8 offset, UINT8 data);
static void k054539_proc_channel(K054539_DATA* chip, UINT8 Chn);
void k054539_write(UINT8 Port, UINT8 Offset, UINT8 Data);
void c140_write(UINT8 Port, UINT8 Offset, UINT8 Data);
void k053260_write(UINT8 Register, UINT8 Data);
void qsound_write(UINT8 Offset, UINT16 Value);
void x1_010_write(UINT16 Offset, UINT8 Data);
void c352_write(UINT16 Offset, UINT16 Value);
void ga20_write(UINT8 Register, UINT8 Data);
void es5503_write(UINT8 Register, UINT8 Data);
void ymf278b_write(UINT8 Port, UINT8 Register, UINT8 Data);
void es550x_w(UINT8 Offset, UINT8 Data);
void es550x_w16(UINT8 Offset, UINT16 Data);
void write_rom_data(UINT8 ROMType, UINT32 ROMSize, UINT32 DataStart, UINT32 DataLength,
					const UINT8* ROMData);
UINT32 GetROMMask(UINT8 ROMType, UINT8** MaskData);
UINT32 GetROMData(UINT8 ROMType, UINT8** ROMData);


UINT8 ChipCount = 0x02;
ALL_CHIPS* ChipData = NULL;
ALL_CHIPS* ChDat;

void InitAllChips(void)
{
	UINT8 CurChip;
	UINT8 CurChn;
	YM_DELTAT* TempYDT;

	if (ChipData == NULL)
		ChipData = (ALL_CHIPS*)malloc(ChipCount * sizeof(ALL_CHIPS));
	memset(ChipData, 0x00, ChipCount * sizeof(ALL_CHIPS));
	for (CurChip = 0x00; CurChip < ChipCount; CurChip ++)
	{
		ChDat = ChipData + CurChip;
		memset(ChDat->SegaPCM.RAMData, 0xFF, 0x800);

		ChDat->SegaPCM.intf_bank = SPCM_BANK_512;
		// DELTA-T unit
		TempYDT = &ChDat->YM2608.DeltaT;
		TempYDT->portshift = 5;		// always 5bits shift	// ASG
			//TempYDT->limit     = ~0;
			TempYDT->emulation_mode = YM_DELTAT_MODE_NORMAL;
			TempYDT->portstate = 0;
			TempYDT->control2  = 0;
			TempYDT->DRAMportshift = dram_rightshift[TempYDT->control2 & 3];
		// DELTA-T unit
		TempYDT = &ChDat->YM2610.DeltaT;
		TempYDT->portshift = 8;		// allways 8bits shift
			//TempYDT->limit     = ~0;
			TempYDT->emulation_mode = YM_DELTAT_MODE_YM2610;
			TempYDT->portstate = 0x20;
			TempYDT->control2  = 0x01;
			TempYDT->DRAMportshift = dram_rightshift[TempYDT->control2 & 3];
		TempYDT = &ChDat->Y8950.DeltaT;
		TempYDT->portshift = 5;		// always 5bits shift	// ASG
			//TempYDT->limit     = ~0;
			TempYDT->emulation_mode = YM_DELTAT_MODE_NORMAL;
			TempYDT->portstate = 0;
			TempYDT->control2  = 0;
			TempYDT->DRAMportshift = dram_rightshift[TempYDT->control2 & 3];

		ChDat->YMF278B.RAMBase = 0x200000;	// default ROM size is 2 MB
		//ChDat->UPD7759.state = STATE_IDLE;
		ChDat->UPD7759.reset = 1;
		ChDat->UPD7759.start = 1;
		ChDat->OKIM6295.command = 0xFF;
		ChDat->K054539.flags = K054539_RESET_FLAGS;
		ChDat->C140.banking_type = 0x00;
		ChDat->ES5506.voiceCount = 32;
		for (CurChn = 0; CurChn < 28; CurChn ++)
			ChDat->MultiPCM.slot[CurChn].SmplID = 0xFFFF;
    }

	SetChipSet(0x00);

	return;
}

void FreeAllChips(void)
{
	if (ChipData == NULL)
		return;

	free(ChipData);
	ChipData = NULL;

	return;
}

void SetChipSet(UINT8 ChipID)
{
	ChDat = ChipData + ChipID;

	return;
}

void segapcm_mem_write(UINT16 Offset, UINT8 Data)
{
	SEGAPCM_DATA* chip = &ChDat->SegaPCM;
	UINT8 CurChn;
	UINT8* RAMBase;
	UINT32 StAddr;
	UINT32 EndAddr;
	UINT32 Addr;

	if (Offset >= 0xFFF0)
	{
		Addr = (Offset & 0x03) * 8;
		chip->intf_bank &= ~(0xFF << Addr);
		chip->intf_bank |=  (Data << Addr);
		return;
	}

	CurChn = (Offset & 0x78) / 8;
	RAMBase = chip->RAMData + CurChn * 8;
	chip->RAMData[Offset] = Data;
	switch(Offset & 0x87)
	{
/*	case 0x04:	// Loop Address L
		break;
	case 0x05:	// Loop Address H
		break;
	case 0x06:	// End Address H
		break;
	case 0x84:	// Current Address L
		break;
	case 0x85:	// Current Address H
		break;*/
	case 0x86:	// Flags (Channel Disable, Loop Disable)
		if (! (chip->RAMData[0x86 | (CurChn * 8)] & 0x01))
		{
			StAddr = (RAMBase[0x84] << 0) | (RAMBase[0x85] << 8);
			Addr = (RAMBase[0x04] << 0) | (RAMBase[0x05] << 8);	// Loop Address
			if (Addr < StAddr)
				StAddr = Addr;
			EndAddr = (RAMBase[0x06] + 0x01) << 8;
			//StAddr &= ~0x00FF;

			Addr = (RAMBase[0x86] & chip->bankmask) << chip->bankshift;
			StAddr = (Addr + StAddr) & chip->rgnmask;
			EndAddr = (Addr + EndAddr) & chip->rgnmask;

			for (Addr = StAddr; Addr < EndAddr; Addr ++)
				chip->ROMUsage[Addr] |= 0x01;
		}
		break;
	}

	return;
}

void ym2608_write(UINT8 Port, UINT8 Register, UINT8 Data)
{
	YM2608_DATA* chip;
	UINT16 RegVal;

	chip = &ChDat->YM2608;
	RegVal = (Port << 8) | Register;
	chip->RegData[RegVal] = Data;

	switch(RegVal & 0x1F0)
	{
	case 0x010:	// ADPCM A
		break;
	case 0x100:	// DeltaT ADPCM
		if ((RegVal & 0x0F) == 0x0E)
			break;

		YM_DELTAT_ADPCM_Write(&chip->DeltaT, RegVal & 0x0F, Data);
		break;
	}

	return;
}

void ym2610_write(UINT8 Port, UINT8 Register, UINT8 Data)
{
	YM2610_DATA* chip;
	UINT16 RegVal;

	chip = &ChDat->YM2610;
	RegVal = (Port << 8) | Register;
	chip->RegData[RegVal] = Data;

	switch(RegVal & 0x1F0)
	{
	case 0x010:	// DeltaT ADPCM
		if (RegVal >= 0x1C)
			break;
		YM_DELTAT_ADPCM_Write(&chip->DeltaT, RegVal & 0x0F, Data);
		break;
	case 0x100:	// ADPCM A
	case 0x110:
	case 0x120:
		FM_ADPCMAWrite(chip, RegVal & 0xFF, Data);
		break;
	}

	return;
}

void y8950_write(UINT8 Register, UINT8 Data)
{
	Y8950_DATA* chip;

	chip = &ChDat->Y8950;
	chip->RegData[Register] = Data;

	if (Register >= 0x07 && Register < 0x015)	// DeltaT ADPCM
	{
		if (Register >= 0x13)
			return;
		if (Register == 0x08)
			Data &= 0x0F;
		YM_DELTAT_ADPCM_Write(&chip->DeltaT, Register - 0x07, Data);
	}

	return;
}

// ADPCM type A Write
static void FM_ADPCMAWrite(YM2610_DATA *F2610, UINT8 r, UINT8 v)
{
	ADPCM_CH *adpcm = F2610->ADPCM;
	UINT8 c = r&0x07;
	UINT32 SampleLen;
	UINT32 CurSmpl;
	UINT8* MaskBase;

	// an almost complete copy-paste-and-delete from MAME
	F2610->ADPCMReg[r] = v; // stock data
	switch(r)
	{
	case 0x00:	// DM,--,C5,C4,C3,C2,C1,C0
		if( !(v&0x80) )
		{
			// KEY ON
			for( c = 0; c < 6; c++ )
			{
				if( (v>>c)&1 )
				{
					// *** start adpcm ***
					adpcm[c].flag = 1;

					adpcm[c].end &= (1 << 20) - 1;	// YM2610 checks only 20 bits while playing
					adpcm[c].end |= adpcm[c].start & ~((1 << 20) - 1);
					if (adpcm[c].end < adpcm[c].start)
					{
						printf("YM2610: ADPCM-A Start %06X > End %06X\n", adpcm[c].start, adpcm[c].end);
						adpcm[c].end += (1 << 20);
					}
					adpcm[c].end ++;

					if(adpcm[c].start >= F2610->AP_ROMSize)	// Check Start in Range
					{
						printf("YM2610: ADPCM-A start out of range: $%08x\n", adpcm[c].start);
						adpcm[c].flag = 0;
						continue;
					}
					if(adpcm[c].end > F2610->AP_ROMSize)	// Check End in Range
					{
						printf("YM2610 Ch %u: ADPCM-A end out of range\n", c);
						printf("ADPCM-A Start: %06x\tADPCM-A End: %06x\n",
								adpcm[c].start, adpcm[c].end);
						adpcm[c].end = F2610->AP_ROMSize;
						//continue;	// uncomment to ignore the fact that the complete rom
									// may be used
					}

					SampleLen = adpcm[c].end - adpcm[c].start;
					MaskBase = &F2610->AP_ROMUsage[adpcm[c].start];
					for (CurSmpl = 0x00; CurSmpl < SampleLen; CurSmpl ++)
						MaskBase[CurSmpl] |= 0x01;
				}
			}
		}
		else
		{
			// KEY OFF
			for( c = 0; c < 6; c++ )
				if( (v>>c)&1 )
					adpcm[c].flag = 0;
		}
		break;
	case 0x01:	// Total Level
		break;
	default:
		c = r&0x07;
		if( c >= 0x06 ) return;

		switch( r&0x38 ){
		case 0x08:	// Pan, Instrument Level
			break;
		case 0x10:	// Sample Start Address
		case 0x18:
			adpcm[c].start  = ((F2610->ADPCMReg[0x18 + c] * 0x0100 |
								F2610->ADPCMReg[0x10 + c]) << ADPCMA_ADDRESS_SHIFT);

			if (adpcm[c].flag)
			{
				printf("ADPCM-A Ch %u StartAddr: %06X\n", c, adpcm[c].start);
			}
			break;
		case 0x20:	// Sample End Address
		case 0x28:
			adpcm[c].end    = ((F2610->ADPCMReg[0x28 + c] * 0x0100 |
								F2610->ADPCMReg[0x20 + c]) << ADPCMA_ADDRESS_SHIFT);
			adpcm[c].end   += (1<<ADPCMA_ADDRESS_SHIFT) - 1;
			//adpcm[c].end   += (2<<ADPCMA_ADDRESS_SHIFT) - 1; // Puzzle De Pon-Patch

			if (adpcm[c].flag)
			{
				printf("ADPCM-A Ch %u EndAddr: %06X\n", c, adpcm[c].end);
			}
			break;
		}
	}

	return;
}

// DELTA-T ADPCM write register
static void YM_DELTAT_ADPCM_Write(YM_DELTAT *DELTAT, UINT8 r, UINT8 v)
{
	UINT32 SampleLen;
	UINT32 CurSmpl;
	UINT8* MaskBase;

	if(r>=0x10) return;
	DELTAT->reg[r] = v;	// stock data

	switch( r )
	{
	case 0x00:	// START,REC,MEMDATA,REPEAT,SPOFF,--,--,RESET
		// handle emulation mode
		if(DELTAT->emulation_mode == YM_DELTAT_MODE_YM2610)
		{
			v |= 0x20;		//  YM2610 always uses external memory and doesn't even have memory flag bit.
		}

		DELTAT->portstate = v & (0x80|0x40|0x20|0x10|0x01); // start, rec, memory mode, repeat flag copy, reset(bit0)

		if( DELTAT->portstate&0x20 ) // do we access external memory?
		{
			// if yes, then let's check if ADPCM memory is mapped and big enough
			if(DELTAT->memory == NULL)
			{
				if (DELTAT->portstate & 0x80)	// check for START bit - the warning is rather useless else
					printf("YM Delta-T ADPCM rom not mapped\n");
				DELTAT->portstate = 0x00;
				break;
			}
			else
			{
				if( DELTAT->start >= DELTAT->memory_size )	// Check Start in Range
				{
					printf("YM Delta-T ADPCM start out of range: $%08x\n", DELTAT->start);
					DELTAT->portstate = 0x00;
				}
				if( DELTAT->end > DELTAT->memory_size )	// Check End in Range
				{
					printf("YM Delta-T ADPCM end out of range: $%08x\n", DELTAT->end);
					DELTAT->end = DELTAT->memory_size;
				}
			}
		}
		else	// we access CPU memory (ADPCM data register $08) so we only reset now_addr here
		{
			break;	// ROM is not used
		}

		if( DELTAT->portstate&0x80 )
		{
			// start ADPCM
			SampleLen = DELTAT->end - DELTAT->start;
			if (DELTAT->end < DELTAT->start)
			{
				printf("Warning: Invalid Sample Length: %06X (%06X .. %06X)\n", SampleLen, DELTAT->start, DELTAT->end);
				if (DELTAT->end + 0x10000 < DELTAT->start)
					return;
				SampleLen += 0x10000;	// workaround
			}
			MaskBase = &DELTAT->memory_usg[DELTAT->start];
			for (CurSmpl = 0x00; CurSmpl < SampleLen; CurSmpl ++)
				MaskBase[CurSmpl] |= 0x01;
		}
		break;
	case 0x01:	// L,R,-,-,SAMPLE,DA/AD,RAMTYPE,ROM
		// handle emulation mode
		if(DELTAT->emulation_mode == YM_DELTAT_MODE_YM2610)
		{
			v |= 0x01;		//  YM2610 always uses ROM as an external memory and doesn't tave ROM/RAM memory flag bit.
		}

		if ((DELTAT->control2 & 3) != (v & 3))
		{
			//0-DRAM x1, 1-ROM, 2-DRAM x8, 3-ROM (3 is bad setting - not allowed by the manual)
			if (DELTAT->DRAMportshift != dram_rightshift[v&3])
			{
				DELTAT->DRAMportshift = dram_rightshift[v&3];

				// refresh addresses
				DELTAT->start  = (DELTAT->reg[0x3]*0x0100 | DELTAT->reg[0x2]) << (DELTAT->portshift - DELTAT->DRAMportshift);
				DELTAT->end    = (DELTAT->reg[0x5]*0x0100 | DELTAT->reg[0x4]) << (DELTAT->portshift - DELTAT->DRAMportshift);
				DELTAT->end   += (1 << (DELTAT->portshift-DELTAT->DRAMportshift) ) - 1;
				DELTAT->end   ++;
				//DELTAT->limit  = (DELTAT->reg[0xd]*0x0100 | DELTAT->reg[0xc]) << (DELTAT->portshift - DELTAT->DRAMportshift);
			}
		}
		DELTAT->control2 = v;
		break;
	case 0x02:	// Start Address L
	case 0x03:	// Start Address H
		DELTAT->start  = (DELTAT->reg[0x3]*0x0100 | DELTAT->reg[0x2]) << (DELTAT->portshift - DELTAT->DRAMportshift);
		//logerror("DELTAT start: 02=%2x 03=%2x addr=%8x\n",DELTAT->reg[0x2], DELTAT->reg[0x3],DELTAT->start )
		break;
	case 0x04:	// Stop Address L
	case 0x05:	// Stop Address H
		DELTAT->end    = (DELTAT->reg[0x5]*0x0100 | DELTAT->reg[0x4]) << (DELTAT->portshift - DELTAT->DRAMportshift);
		DELTAT->end   += (1 << (DELTAT->portshift-DELTAT->DRAMportshift) ) - 1;
		DELTAT->end   ++;
		//logerror("DELTAT end  : 04=%2x 05=%2x addr=%8x\n",DELTAT->reg[0x4], DELTAT->reg[0x5],DELTAT->end   )
		break;
	case 0x06:	// Prescale L (ADPCM and Record frq)
	case 0x07:	// Prescale H
		break;
	case 0x08:	// ADPCM data
		break;
	case 0x09:	// DELTA-N L (ADPCM Playback Prescaler)
	case 0x0a:	// DELTA-N H
		break;
	case 0x0b:	// Output level control (volume, linear)
		break;
	case 0x0c:	// Limit Address L
	case 0x0d:	// Limit Address H
		//DELTAT->limit  = (DELTAT->reg[0xd]*0x0100 | DELTAT->reg[0xc]) << (DELTAT->portshift - DELTAT->DRAMportshift);
		//logerror("DELTAT limit: 0c=%2x 0d=%2x addr=%8x\n",DELTAT->reg[0xc], DELTAT->reg[0xd],DELTAT->limit );
		break;
	}

	return;
}

void ymz280b_write(UINT8 Register, UINT8 Data)
{
	YMZ280B_DATA* chip;
	YMZ_VOICE *voice;
	UINT32 SampleLen;
	UINT32 CurSmpl;
	UINT8* MaskBase;

	chip = &ChDat->YMZ280B;
	if (Register < 0x80)
	{
		voice = &chip->Voice[(Register >> 2) & 0x07];

		switch(Register & 0xE3)
		{
		case 0x01:		// pitch upper 1 bit, loop, key on, mode
			voice->keyon = (Data & 0x80) >> 7;
			if (voice->keyon)
			{
				if (voice->start >= chip->ROMSize)	// Check Start in Range
				{
					printf("YMZ280B: start out of range: $%08x\n", voice->start);
					voice->keyon = 0;
				}
				if (voice->stop > chip->ROMSize)	// Check End in Range
				{
					printf("YMZ280B: end out of range\n");
					printf("Start: %06x\tEnd: %06x\n", voice->start, voice->stop);
					voice->stop = chip->ROMSize;
					//continue;	// uncomment to ignore the fact that the complete rom
								// may be used
				}

				SampleLen = voice->stop - voice->start;
				MaskBase = &chip->ROMUsage[voice->start];
				for (CurSmpl = 0x00; CurSmpl < SampleLen; CurSmpl ++)
					MaskBase[CurSmpl] |= 0x01;
			}
			break;
		case 0x20:		// start address high
			voice->start = (voice->start & 0x00FFFF) | (Data << 16);
			break;
		case 0x21:		// loop start address high
			voice->loop_start = (voice->loop_start & 0x00FFFF) | (Data << 16);
			break;
		case 0x22:		// loop end address high
			voice->loop_end = (voice->loop_end & 0x00FFFF) | (Data << 16);
			break;
		case 0x23:		// stop address high
			voice->stop = (voice->stop & 0x00FFFF) | (Data << 16);
			break;
		case 0x40:		// start address middle
			voice->start = (voice->start & 0xFF00FF) | (Data << 8);
			break;
		case 0x41:		// loop start address middle
			voice->loop_start = (voice->loop_start & 0xFF00FF) | (Data << 8);
			break;
		case 0x42:		// loop end address middle
			voice->loop_end = (voice->loop_end & 0xFF00FF) | (Data << 8);
			break;
		case 0x43:		// stop address middle
			voice->stop = (voice->stop & 0xFF00FF) | (Data << 8);
			break;
		case 0x60:		// start address low
			voice->start = (voice->start & 0xFFFF00) | (Data << 0);
			break;
		case 0x61:		// loop start address low
			voice->loop_start = (voice->loop_start & 0xFFFF00) | (Data << 0);
			break;
		case 0x62:		// loop end address low
			voice->loop_end = (voice->loop_end & 0xFFFF00) | (Data << 0);
			break;
		case 0x63:		// stop address low
			voice->stop = (voice->stop & 0xFFFF00) | (Data << 0);
			break;
		default:
			break;
		}
	}

	return;
}

static void rf5c_mark_sample(RF5C_DATA* chip, UINT32 Addr, UINT16 SmplStep)
{
	if (chip->RAMUsage[Addr] & 0x01)
		return;	// We marked it already.

	for (; Addr < chip->RAMSize; Addr ++)
	{
		if (chip->RAMData[Addr] == 0xFF)
			break;
		chip->RAMUsage[Addr] |= 0x01;
	}
	if (Addr >= chip->RAMSize)
		return;

	// The chip can skip the first terminator sample, so leave enough
	// samples that it works with the current frequency. (requires re-marking for every frequency)
	//SmplStep = (SmplStep + 0x7FF) >> 11;
	//if (! SmplStep)
	//	SmplStep = 1;

	// The "Start" register is in units of 0x100 bytes, so pad up to that.
	SmplStep = 0x100 - (Addr & 0x00FF);
	for (; SmplStep > 0; SmplStep --, Addr ++)
		chip->RAMUsage[Addr] |= 0x01;

	return;
}

static void rf5c_write(RF5C_DATA* chip, UINT8 Register, UINT8 Data)
{
	RF5C_CHANNEL* chan;
	UINT8 CurChn;

	switch(Register)
	{
	case 0x02:	// Sample Step Low
		chan = &chip->Channel[chip->SelChn];
		chan->step = (chan->step & 0xFF00) | (Data & 0x00FF);
		break;
	case 0x03:	// Sample Step High
		chan = &chip->Channel[chip->SelChn];
		chan->step = (chan->step & 0x00FF) | ((Data << 8) & 0xFF00);
		break;
	case 0x04:	// Loop Address Low
		chan = &chip->Channel[chip->SelChn];
		chan->loopst = (chan->loopst & 0xFF00) | (Data & 0x00FF);
		break;
	case 0x05:	// Loop Address High
		chan = &chip->Channel[chip->SelChn];
		chan->loopst = (chan->loopst & 0x00FF) | ((Data << 8) & 0xFF00);
		break;
	case 0x06:	// Start Address
		chan = &chip->Channel[chip->SelChn];
		chan->start = Data << 8;
		if (chan->enable)
		{
			rf5c_mark_sample(chip, chan->start, chan->step);
			/*Addr = chan->start;
			while(Addr < chip->RAMSize)
			{
				chip->RAMUsage[Addr] |= 0x01;
				if (chip->RAMData[Addr] == 0xFF)
					break;
				Addr ++;
			}*/
		}
		break;
	case 0x07:	// Control Register
		// (Data & 0x80) -> Chip Enable
		if (Data & 0x40)
			chip->SelChn = Data & 0x07;
		else
			chip->SelBank = (Data & 0x0F) << 12;
		break;
	case 0x08:	// Channel Enable
		for (CurChn = 0x00; CurChn < 0x08; CurChn ++)
		{
			chan = &chip->Channel[CurChn];
			chan->enable = ! (Data & (0x01 << CurChn));
			if (chan->enable)
			{
				rf5c_mark_sample(chip, chan->start, chan->step);
				// the Loop Start can be at a completely other place,
				// like Popful Mail showed me
				rf5c_mark_sample(chip, chan->loopst, chan->step);
			}
		}
		break;
	}

	return;
}

void rf5c68_write(UINT8 Register, UINT8 Data)
{
	rf5c_write(&ChDat->RF5C68, Register, Data);
	return;
}

void rf5c164_write(UINT8 Register, UINT8 Data)
{
	rf5c_write(&ChDat->RF5C164, Register, Data);
	return;
}

static void ymf271_write_fm_reg(YMF271_DATA* chip, UINT8 SlotNum, UINT8 Register, UINT8 Data)
{
	YMF271_SLOT *slot = &chip->slots[SlotNum];
	UINT32 Addr;
	UINT32 CurByt;
	UINT32 DataLen;

	switch(Register)
	{
	case 0x00:
		if (Data & 1)
		{
			// key on
			slot->active = 1;
			//if (! slot->IsPCM)
			//	break;
			if (slot->waveform != 0x07)
				break;
			if (SlotNum & 0x03)
			{
				SlotNum &= ~0x03;
				slot = &chip->slots[SlotNum];
			}

			DataLen = slot->endaddr + 1;
			if (slot->bits != 8)
			DataLen = (DataLen * slot->bits + 7) / 8;	// scale up for 12-bit samples
			Addr = slot->startaddr;
			for (CurByt = 0x00; CurByt < DataLen; CurByt ++, Addr ++)
				chip->ROMUsage[Addr] |= 0x01;
		}
		else if (slot->active)
		{
			slot->active = 0;
		}
		break;
	case 0x0B:
		slot->waveform = Data & 0x07;
		break;
	}

	return;
}

static void ymf271_write_fm(YMF271_DATA* chip, UINT8 Port, UINT8 Register, UINT8 Data)
{
	YMF271_SLOT *slot;
	UINT8 SlotReg;
	UINT8 SlotNum;
	UINT8 SyncMode;
	UINT8 SyncReg;

	if ((Register & 0x03) == 0x03)
		return;

	SlotNum = ((Register & 0x0C) / 0x04 * 0x03) + (Register & 0x03);
	slot = &chip->slots[12 * Port + SlotNum];
	SlotReg = (Register >> 4) & 0x0F;
	if (SlotNum >= 12 || 12 * Port > 48)
	{
		printf("Error in YMF271 Data!");
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
	switch(chip->groups[SlotNum].sync)
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

	/*if (SyncMode && SyncReg)		// key-on slot & synced register
	{
		switch(chip->groups[SlotNum].sync)
		{
		case 0:		// 4 slot (FM) mode
			ymf271_write_fm_reg(chip, (12 * 0) + SlotNum, SlotReg, Data);
			ymf271_write_fm_reg(chip, (12 * 1) + SlotNum, SlotReg, Data);
			ymf271_write_fm_reg(chip, (12 * 2) + SlotNum, SlotReg, Data);
			ymf271_write_fm_reg(chip, (12 * 3) + SlotNum, SlotReg, Data);
			break;
		case 1:		// 2x 2 slot (FM) mode
			if (Port == 0)		// Slot 1 - Slot 3
			{
				ymf271_write_fm_reg(chip, (12 * 0) + SlotNum, SlotReg, Data);
				ymf271_write_fm_reg(chip, (12 * 2) + SlotNum, SlotReg, Data);
			}
			else				// Slot 2 - Slot 4
			{
				ymf271_write_fm_reg(chip, (12 * 1) + SlotNum, SlotReg, Data);
				ymf271_write_fm_reg(chip, (12 * 3) + SlotNum, SlotReg, Data);
			}
			break;
		case 2:		// 3 slot (FM) + 1 slot (PCM) mode
			// 1 slot (PCM) is handled normally
			ymf271_write_fm_reg(chip, (12 * 0) + SlotNum, SlotReg, Data);
			ymf271_write_fm_reg(chip, (12 * 1) + SlotNum, SlotReg, Data);
			ymf271_write_fm_reg(chip, (12 * 2) + SlotNum, SlotReg, Data);
			break;
		default:
			break;
		}
	}
	else*/		// write register normally
	{
		ymf271_write_fm_reg(chip, (12 * Port) + SlotNum, SlotReg, Data);
	}

	return;
}

void ymf271_write(UINT8 Port, UINT8 Register, UINT8 Data)
{
	YMF271_DATA* chip = &ChDat->YMF271;
	YMF271_SLOT* slot;
	YMF271_GROUP* group;
	UINT8 SlotNum;
	//UINT8 Addr;

	switch(Port)
	{
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
		ymf271_write_fm(chip, Port, Register, Data);
		break;
	case 0x04:
		if ((Register & 0x03) == 0x03)
			return;

		SlotNum = ((Register & 0x0C) / 0x04 * 0x03) + (Register & 0x03);
		//Addr = (Register >> 4) % 3;
		slot = &chip->slots[SlotNum * 4];

		switch((Register >> 4) & 0x0F)
		{
		case 0:
			slot->startaddr &= ~0x0000FF;
			slot->startaddr |= Data << 0;
			break;
		case 1:
			slot->startaddr &= ~0x00FF00;
			slot->startaddr |= Data << 8;
			break;
		case 2:
			slot->startaddr &= ~0xFF0000;
			slot->startaddr |= (Data & 0x7F) << 16;
			break;
		case 3:
			slot->endaddr &= ~0x0000FF;
			slot->endaddr |= Data << 0;
			break;
		case 4:
			slot->endaddr &= ~0x00FF00;
			slot->endaddr |= Data << 8;
			break;
		case 5:
			slot->endaddr &= ~0xFF0000;
			slot->endaddr |= (Data & 0x7F) << 16;
			break;
		case 6:
			slot->loopaddr &= ~0x0000FF;
			slot->loopaddr |= Data << 0;
			break;
		case 7:
			slot->loopaddr &= ~0x00FF00;
			slot->loopaddr |= Data << 8;
			break;
		case 8:
			slot->loopaddr &= ~0xFF0000;
			slot->loopaddr |= (Data & 0x7F) << 16;
			break;
		case 9:
			slot->bits = (Data & 0x04) ? 12 : 8;
			break;
		}
		break;
	case 0x06:
		if (! (Register & 0xF0))
		{
			if ((Register & 0x03) == 0x03)
				return;

			SlotNum = ((Register & 0x0C) / 0x04 * 0x03) + (Register & 0x03);
			if (SlotNum >= 12)
			{
				printf("Error in YMF271 Data!");
				break;
			}
			group = &chip->groups[SlotNum];
			group->sync = Data & 0x03;
			switch(group->sync)
			{
			case 0x00:	// 4 op FM
				chip->slots[0*12 + SlotNum].IsPCM = 0x00;
				chip->slots[1*12 + SlotNum].IsPCM = 0x00;
				chip->slots[2*12 + SlotNum].IsPCM = 0x00;
				chip->slots[3*12 + SlotNum].IsPCM = 0x00;
				break;
			case 0x01:	// 2x 2 op FM
				chip->slots[0*12 + SlotNum].IsPCM = 0x00;
				chip->slots[1*12 + SlotNum].IsPCM = 0x00;
				chip->slots[2*12 + SlotNum].IsPCM = 0x00;
				chip->slots[3*12 + SlotNum].IsPCM = 0x00;
				break;
			case 0x02:	// 3 op FM + PCM
				chip->slots[0*12 + SlotNum].IsPCM = 0x00;
				chip->slots[1*12 + SlotNum].IsPCM = 0x00;
				chip->slots[2*12 + SlotNum].IsPCM = 0x00;
				chip->slots[3*12 + SlotNum].IsPCM = 0x01;
				break;
			case 0x03:	// PCM
				chip->slots[0*12 + SlotNum].IsPCM = 0x01;
				chip->slots[1*12 + SlotNum].IsPCM = 0x01;
				chip->slots[2*12 + SlotNum].IsPCM = 0x01;
				chip->slots[3*12 + SlotNum].IsPCM = 0x01;
				break;
			}
		}
		else
		{
			// Timer Registers and External Memory Writes
		}
		break;
	}

	return;
}

static void okim6295_command_write(OKIM6295_DATA* chip, UINT8 Command)
{
	UINT8 voicemask;
	UINT8 voicenum;
	OKIM_VOICE* voice;
	UINT32 base;
	UINT32 SampleLen;
	UINT32 CurSmpl;
	UINT8* MaskBase;
	UINT8 BankID;

	// if a command is pending, process the second half
	if (chip->command != 0xFF)
	{
		voicemask = Command >> 4;

		// determine which voice(s) (voice is set by a 1 bit in the upper 4 bits of the second byte)
		//for (voicenum = 0; voicenum < OKIM6295_VOICES; voicenum ++, voicemask >>= 1)
		if (voicemask & 0x0F)
		{
			//if (! (voicemask & 1))
			//	continue;

			for (voicenum = 0; voicenum < OKIM6295_VOICES; voicenum ++, voicemask >>= 1)
			{
				if (voicemask & 1)
					break;
			}
			voice = &chip->voice[voicenum];

			// determine the start/stop positions
			base = chip->command * 8;
			if (! chip->nmk_mode)
			{
				base |= chip->bank_offs;
			}
			else
			{
				if (chip->nmk_mode & 0x80)
					BankID = base >> 8;
				else
					BankID = 0x00;
				base |= (chip->nmk_bank[BankID & 0x03] << 16);
			}

			voice->start  = chip->ROMData[base + 0] << 16;
			voice->start |= chip->ROMData[base + 1] <<  8;
			voice->start |= chip->ROMData[base + 2] <<  0;
			voice->start &= 0x3FFFF;

			voice->stop  = chip->ROMData[base + 3] << 16;
			voice->stop |= chip->ROMData[base + 4] <<  8;
			voice->stop |= chip->ROMData[base + 5] <<  0;
			voice->stop &= 0x3FFFF;

			if (! chip->nmk_mode)
			{
				voice->start |= chip->bank_offs;
				voice->stop |= chip->bank_offs;
			}
			else
			{
				BankID = voice->start >> 16;
				voice->start &= 0xFFFF;
				voice->start |= (chip->nmk_bank[BankID & 0x03] << 16);
				BankID = voice->stop >> 16;
				voice->stop &= 0xFFFF;
				voice->stop |= (chip->nmk_bank[BankID & 0x03] << 16);
				if ((voice->start & ~0xFFFF) != (voice->stop & ~0xFFFF))
					printf("Warning: NMK112 Start Bank != Stop Bank!\n");
			}

			SampleLen = 0x80 * 8;	// header memory
			base &= ~0x3FF;			// enforce storing the full table
			MaskBase = &chip->ROMUsage[base];
			for (CurSmpl = 0x00; CurSmpl < SampleLen; CurSmpl ++)
				MaskBase[CurSmpl] |= 0x01;

			// set up the voice to play this sample
			if (voice->start < voice->stop)
			{
				voice->playing = true;

				SampleLen = voice->stop - voice->start + 1;
				MaskBase = &chip->ROMUsage[voice->start];
				for (CurSmpl = 0x00; CurSmpl < SampleLen; CurSmpl ++)
					MaskBase[CurSmpl] |= 0x01;
			}
			else
			{
				voice->playing = false;
				printf("OKIM6295: Voice %u requested to play invalid sample %02x\n",
						voicenum, chip->command);
			}
		}

		// reset the command
		chip->command = 0xFF;
	}
	else if (Command & 0x80)
	{
		chip->command = Command & 0x7F;
	}
	else	// silence command
	{
		// determine which voice(s) (voice is set by a 1 bit in bits 3-6 of the command
		voicemask = Command >> 3;

		for (voicenum = 0; voicenum < OKIM6295_VOICES; voicenum++, voicemask >>= 1)
		{
			if (voicemask & 1)
				chip->voice[voicenum].playing = false;
		}
	}

	return;
}

void okim6295_write(UINT8 Offset, UINT8 Data)
{
	OKIM6295_DATA* chip = &ChDat->OKIM6295;
	UINT32 TempLng;
	UINT8 CurChn;
	UINT8 ChnMask;

	switch(Offset)
	{
	case 0x00:
		okim6295_command_write(chip, Data);
		break;
	case 0x0E:	// NMK112 bank switch enable
		chip->nmk_mode = Data;
		break;
	case 0x0F:
		TempLng = Data << 18;
		if (chip->bank_offs == TempLng)
			return;

		ChnMask = 0x00;
		for (CurChn = 0; CurChn < OKIM6295_VOICES; CurChn ++)
			ChnMask |= chip->voice[CurChn].playing << CurChn;

		if (ChnMask)
		{
			UINT32 SampleLen;
			UINT32 CurSmpl;
			UINT8* MaskBase;

			printf("Warning! OKIM6295 Bank change (%X -> %X) while channel ",
					chip->bank_offs, TempLng);
			for (CurChn = 0; CurChn < OKIM6295_VOICES; CurChn ++)
			{
				if (ChnMask & (1 << CurChn))
				{
					printf("%c", '0' + CurChn);

					SampleLen = chip->voice[CurChn].stop - chip->voice[CurChn].start + 1;
					MaskBase = &chip->ROMUsage[TempLng | chip->voice[CurChn].start];
					for (CurSmpl = 0x00; CurSmpl < SampleLen; CurSmpl ++)
						MaskBase[CurSmpl] |= 0x01;
				}
			}
			printf(" is playing!\n");
		}
		chip->bank_offs = TempLng;
		break;
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
		chip->nmk_bank[Offset & 0x03] = Data;
		break;
	}

	return;
}

void k007232_write(UINT8 offset, UINT8 data)
{
    K007232_DATA* chip = &ChDat->K007232;
    int ch;
    K007232_Channel* v;

    // Handle register 0x1F as a special "read" trigger
    if (offset == 0x1F) {
        // "data" is the original offset; treat as a read at that offset
        if (data == 5 || data == 11) {
            ch = (data >= 6) ? 1 : 0;
            v = &chip->channel[ch];
            v->play = 1;
            v->addr = v->start;
            v->counter = 0x1000;

            // Mark ROM usage as for a key-on
            if (chip->ROMData && chip->ROMUsage) {
                UINT32 current = v->bank + v->addr;
                UINT32 end = current;
                while (end < chip->ROMSize) {
                    if (chip->ROMData[end] & 0x80) break;
                    end++;
                }
                if (end < chip->ROMSize) end++; // include terminator
                for (UINT32 addr = current; addr < end && addr < chip->ROMSize; addr++)
                    chip->ROMUsage[addr] |= 0x01;
            }
        }
        return;
    }

    if(offset >= 0x14 && offset <= 0x15) {
        switch(offset) {
            case 0x14:
                chip->channel[0].bank = data << 17;
                break;
            case 0x15:
                chip->channel[1].bank = data << 17;
                break;
        }
        return;
    }

    ch = offset / 6;
    if(ch >= K007232_PCM_MAX) return;

    v = &chip->channel[ch];
    const int reg_base = ch * 6;

    chip->wreg[offset] = data;

    switch(offset - reg_base)
    {
        case 0x00: // Pitch LSB
        case 0x01: // Pitch MSB
            v->step = ((chip->wreg[reg_base + 1] & 0x0F) << 8) |
                       chip->wreg[reg_base];
            break;
        case 0x02: // Start LSB
        case 0x03: // Start MID
        case 0x04: // Start MSB
            v->start = ((chip->wreg[reg_base + 4] & 0x01) << 16) |
                       (chip->wreg[reg_base + 3] << 8) |
                        chip->wreg[reg_base + 2];
            break;
        case 0x05: // Key On
            v->play = 1;
            v->addr = v->start;
            v->counter = 0x1000;

            // Mark ROM usage
            if(chip->ROMData && chip->ROMUsage) {
                UINT32 current = v->bank + (v->addr);
                UINT32 end = current;

                // Find terminator (0x80)
                while(end < chip->ROMSize) {
                    if(chip->ROMData[end] & 0x80) break;
                    end++;
                }
                if(end < chip->ROMSize) end++; // Include terminator

                // Mark used bytes
                for(UINT32 addr = current; addr < end && addr < chip->ROMSize; addr++)
                    chip->ROMUsage[addr] |= 0x01;
            }
            break;
        case 0x0D: // Loop Enable
            chip->loop_en = data;
            break;
    }
    return;
}

static UINT32 c140_sample_addr(C140_DATA* chip, UINT8 adr_msb, UINT8 adr_lsb,
							   UINT8 bank, UINT8 voice)
{
	UINT32 TempAddr;
	UINT32 NewAddr;
	UINT8 BnkReg;

	TempAddr = (bank << 16) | (adr_msb << 8) | (adr_lsb << 0);
	switch(chip->banking_type)
	{
	case C140_TYPE_SYSTEM2:
		NewAddr = ((TempAddr & 0x200000) >> 2) | (TempAddr & 0x7FFFF);
		break;
	case C140_TYPE_SYSTEM21:
		NewAddr = ((TempAddr & 0x300000) >> 1) | (TempAddr & 0x7FFFF);
		break;
	case C140_TYPE_ASIC219:
		// on the 219 asic, addresses are in words
		TempAddr = (bank << 16) | (adr_msb << 9) | (adr_lsb << 1);
		BnkReg = (((voice & 12) >> 1) - 1) & 0x07;
		NewAddr = ((chip->BankRegs[BnkReg] & 0x03) * 0x20000) | (TempAddr&0x1ffff);
		break;
	default:
		NewAddr = 0x00;
		break;
	}

	return NewAddr;
}

void c140_write(UINT8 Port, UINT8 Offset, UINT8 Data)
{
	C140_DATA* chip = &ChDat->C140;
	C140_VOICE* TempChn;
	UINT16 RegVal;
	UINT32 Addr;

	if (Port == 0xFF)
	{
		chip->banking_type = Data;
		return;
	}

	RegVal = (Port << 8) | (Offset << 0);
	if (RegVal >= 0x1F0)
	{
		// mirror the bank registers on the 219, fixes bkrtmaq
		if ((RegVal >= 0x1f8) && (chip->banking_type == C140_TYPE_ASIC219))
			RegVal -= 8;
		chip->BankRegs[RegVal & 0x0F] = Data;
	}
	if (RegVal >= 0x180)
		return;

	TempChn = &chip->voi[RegVal >> 4];
	switch(RegVal & 0x0F)
	{
	case 0x04:
		TempChn->bank = Data;
		break;
	case 0x05:
		TempChn->mode = Data;
		if (TempChn->mode & 0x80)
		{
			TempChn->smpl_start = c140_sample_addr(chip, TempChn->start_msb,
									TempChn->start_lsb, TempChn->bank, Offset >> 4);
			TempChn->smpl_end = c140_sample_addr(chip, TempChn->end_msb,
									TempChn->end_lsb, TempChn->bank, Offset >> 4);

			for (Addr = TempChn->smpl_start; Addr < TempChn->smpl_end; Addr ++)
				chip->ROMUsage[Addr] |= 0x01;
		}
		break;
	case 0x06:
		TempChn->start_msb = Data;
		break;
	case 0x07:
		TempChn->start_lsb = Data;
		break;
	case 0x08:
		TempChn->end_msb = Data;
		break;
	case 0x09:
		TempChn->end_lsb = Data;
		break;
	}

	return;
}

void qsound_write(UINT8 Offset, UINT16 Value)
{
	QSOUND_DATA* chip = &ChDat->QSound;

	UINT8 ch;
	UINT8 reg;
	QSOUND_CHANNEL* TempChn;
	UINT32 StAddr;
	UINT32 EndAddr;
	UINT32 Addr;

	if (Offset < 0x80)
	{
		ch = Offset >> 3;
		reg = Offset & 0x07;

		if(reg == 0) // Bank
			ch = (ch + 1) & 0x0F;
	}
	else if (Offset < 0x90)
	{
		ch = Offset - 0x80;
		reg = 8;
	}
	else if (Offset >= 0xBA && Offset < 0xCA)
	{
		ch = Offset - 0xBA;
		reg = 9;
	}
	else if (Offset >= 0xCA && Offset < 0xD6) // ADPCM
	{
		ch = 16+((Offset-0xCA)>>2);
		Offset &= 3;
		if(Offset == 0)
			reg = 0; // bank
		else if(Offset == 2)
			reg = 1; // start
		else if(Offset == 3)
			reg = 5; // end
	}
	else if (Offset >= 0xD6 && Offset < 0xD9) // ADPCM key on
	{
		ch = 16 + Offset - 0xD6;
		reg = 6; // it's actually a key on trigger
	}
	else
	{
		return;
	}

	TempChn = &chip->channel[ch];
	switch(reg)
	{
	case 0: // Bank
		//ch = (ch + 1) & 0x0F;
		TempChn = &chip->channel[ch];
		TempChn->bank = Value & 0x7F;
		break;
	case 1: // start
		TempChn->address = Value;
		break;
	case 2: // pitch
		if (! Value)
		{
			// Key off
			TempChn->key = 0;
		}
		break;
	case 5: // end
		TempChn->end = Value;
		break;
	case 6: // master volume
		if (Value == 0)
		{
			// Key off
			TempChn->key = 0;
		}
		else // if (TempChn->key == 0)
		{
			// Key on
			TempChn->key = 1;

			StAddr = TempChn->address;
			EndAddr = TempChn->end;
			StAddr %= chip->ROMSize;
			EndAddr %= chip->ROMSize;
			EndAddr ++;
			for (Addr = StAddr; Addr < EndAddr; Addr ++)
				chip->ROMUsage[(TempChn->bank << 16) + (Addr&0xffff)] |= 0x01;
		}
		break;
	}

	return;
}

static void k054539_proc_channel(K054539_DATA* chip, UINT8 Chn)
{
	K054539_CHANNEL* TempChn;
	UINT32 Addr;
	UINT16 TempSht;
	UINT32 Step;

	TempChn = &chip->channels[Chn];
	Addr =	(TempChn->pos_reg[2] << 16) |
			(TempChn->pos_reg[1] <<  8) |
			(TempChn->pos_reg[0] <<  0);
	Step = (TempChn->mode_reg & 0x20) ? -1 : +1;

	switch((TempChn->mode_reg & 0x0C) >> 2)
	{
	case 0x00:	// 8-bit PCM
		while(Addr < chip->ROMSize)
		{
			chip->ROMUsage[Addr] |= 0x01;
			if (chip->ROMData[Addr] == 0x80)
				break;

			Addr += Step;
		}
		break;
	case 0x01:	// 16-bit PCM (LSB first)
		Step *= 0x02;
		while(Addr < chip->ROMSize)
		{
			chip->ROMUsage[Addr + 0x00] |= 0x01;
			chip->ROMUsage[Addr + 0x01] |= 0x01;
			TempSht =	(chip->ROMData[Addr + 0x00] << 0) |
						(chip->ROMData[Addr + 0x01] << 8);
			if (TempSht == 0x8000)
				break;

			Addr += Step;
		}
		break;
	case 0x02:	// 4-bit DPCM
		while(Addr < chip->ROMSize)
		{
			chip->ROMUsage[Addr] |= 0x01;
			if (chip->ROMData[Addr] == 0x88)
				break;

			Addr += Step;
		}
		break;
	default:
		printf("K054539: Unknown sample type %u on channel %u!\n",
				TempChn->mode_reg, Chn);
		break;
	}

	return;
}

void k054539_write(UINT8 Port, UINT8 Offset, UINT8 Data)
{
	K054539_DATA* info = &ChDat->K054539;
	K054539_CHANNEL* TempChn;
	UINT16 RegVal;
	UINT8 CurChn;
	UINT8 offs;
	bool latch;

	if (Port == 0xFF)
	{
		info->flags = Data;
		return;
	}

	RegVal = (Port << 8) | (Offset << 0);

	if (RegVal < 0x100)
	{
		offs = RegVal & 0x1F;
		if (offs >= 0x0C && offs <= 0x0E)
		{
			TempChn = &info->channels[RegVal >> 5];
			latch = (info->flags & K054539_UPDATE_AT_KEYON) && (info->reg_enable & 1);
			if (latch)
			{
				// latch writes to the position index registers
				TempChn->pos_latch[offs - 0x0C] = Data;
			}
			else
			{
				TempChn->pos_reg[offs - 0x0C] = Data;
			}
		}
		return;
	}
	else if ((RegVal & 0xFF0) == 0x200)
	{
		TempChn = &info->channels[(RegVal & 0x0F) >> 1];
		if (! (RegVal & 0x01))
		{
			TempChn->mode_reg = Data;
			if (Data & 0x20)
				printf("K054539 #%u, Ch %u: reverse play (Reg 0x%03X, Data 0x%02X)\n",
						(UINT32)(ChDat - ChipData), (RegVal & 0x0F) >> 1,
						RegVal, Data);
		}
	}

	switch(RegVal)
	{
	case 0x214:
		latch = (info->flags & K054539_UPDATE_AT_KEYON) && (info->reg_enable & 1);

		for (CurChn = 0; CurChn < 8; CurChn ++)
		{
			if (Data & (1 << CurChn))
			{
				TempChn = &info->channels[CurChn];
				if (latch)
				{
					// update the chip at key-on
					TempChn->pos_reg[0] = TempChn->pos_latch[0];
					TempChn->pos_reg[1] = TempChn->pos_latch[1];
					TempChn->pos_reg[2] = TempChn->pos_latch[2];
				}

				if (! (info->reg_enable & 0x80))
					TempChn->key_on = true;

				k054539_proc_channel(info, CurChn);
			}
		}
		break;
	case 0x215:
		for (CurChn = 0; CurChn < 8; CurChn ++)
		{
			if (Data & (1 << CurChn))
			{
				if (! (info->reg_enable & 0x80))
					info->channels[CurChn].key_on = false;
			}
		}
		break;
	case 0x22C:
		// the MAME core doesn't prevent writing on Reg 0x22C
		for (CurChn = 0; CurChn < 8; CurChn ++)
		{
			info->channels[CurChn].key_on = (Data >> CurChn) & 0x01;
			if (info->channels[CurChn].key_on)
				k054539_proc_channel(info, CurChn);
		}
		printf("K054539: Direct Channel Enable write! Please report!\n");
		break;
	case 0x22F:
		info->reg_enable = Data;
		break;
	}

	return;
}

void k053260_write(UINT8 Register, UINT8 Data)
{
	K053260_DATA* ic = &ChDat->K053260;
	K053260_CHANNEL* TempChn;
	UINT8 CurChn;
	UINT8 TempByt;
	UINT32 Addr;
	UINT32 CurByt;
	UINT32 DataLen;

	switch(Register & 0xF8)
	{
	case 0x00:	// communication registers
		break;
	case 0x08:	// channel 0
	case 0x10:	// channel 1
	case 0x18:	// channel 2
	case 0x20:	// channel 3
		CurChn = (Register - 0x08) / 0x08;
		TempChn = &ic->channels[CurChn];
		switch(Register & 0x07)
		{
		case 0x02: // size low
			TempChn->size &= 0xFF00;
			TempChn->size |= Data << 0;
			break;
		case 0x03: // size high
			TempChn->size &= 0x00FF;
			TempChn->size |= Data << 8;
			break;
		case 0x04: // start low
			TempChn->start &= 0xFF00;
			TempChn->start |= Data << 0;
			break;
		case 0x05: // start high
			TempChn->start &= 0x00FF;
			TempChn->start |= Data << 8;
			break;
		case 0x06: // bank
			TempChn->bank = Data;
			break;
		}
		break;
	case 0x28:	// control registers
		switch(Register)
		{
		case 0x28:
			TempChn = ic->channels;
			TempByt = Data & 0x0F;
			for (CurChn = 0; CurChn < 4; CurChn ++, TempChn ++)
			{
				if ((TempByt & 0x01) != TempChn->play)
				{
					TempChn->play = TempByt & 0x01;
					if (TempByt & 0x01)
					{
						Addr = (TempChn->bank << 16) | (TempChn->start << 0);
						DataLen = TempChn->size;
						if (Addr >= ic->ROMSize)
						{
							printf("K053260: start out of range: $%08x\n", Addr);
							Addr = 0x00;
							DataLen = 0x00;
						}
						if (Addr + DataLen > ic->ROMSize)
						{
							printf("K053260: end out of range\n");
							printf("Start: %06x\tSize: %06x\n", Addr, DataLen);
							DataLen = ic->ROMSize - Addr;
						}
						for (CurByt = 0x00; CurByt < DataLen; CurByt ++, Addr ++)
							ic->ROMUsage[Addr] |= 0x01;
					}
				}
				TempByt >>= 1;
			}
			break;
		case 0x2A: // loop, ppcm
			//for (CurChn = 0; CurChn < 4; CurChn ++ )
			//	ic->channels[CurChn].ppcm = (Data & (0x10 << CurChn)) != 0;
			break;
		case 0x2F: // control
			ic->mode = Data & 7;
			// bit 0 = read ROM
			// bit 1 = enable sound output
			// bit 2 = unknown
			break;
		}
		break;
	}

	return;
}

void upd7759_write(UINT8 Port, UINT8 Data)
{
	UPD7759_DATA* chip = &ChDat->UPD7759;
	UINT8 OldVal;
	UINT8 LastSmpl;
	UINT8 ReqSmpl;
	UINT8 BlkHdr;
	UINT16 NibbleCnt;
	UINT32 CurOfs;

	if (! chip->ROMSize)
		return;

	switch(Port)
	{
	case 0x00:	// upd7759_reset_w
		OldVal = chip->reset;
		chip->reset = (Data != 0);

		// on the falling edge, reset everything
		if (OldVal && ! chip->reset)
		{
			//upd7759_reset
			//chip->state = STATE_IDLE;
			chip->fifo_in = 0x00;
		}
		break;
	case 0x01:	// upd7759_start_w
		OldVal = chip->start;
		chip->start = (Data != 0);

		// on the rising edge, if we're idle, start going, but not if we're held in reset
		if (! (/*chip->state == STATE_IDLE &&*/ ! OldVal && chip->start && chip->reset))
			break;

		//chip->state = STATE_START;
		ReqSmpl = chip->fifo_in;

		//chip->state = STATE_FIRST_REQ;

		//chip->state = STATE_LAST_SAMPLE;
		LastSmpl = chip->ROMData[chip->romoffset | 0x00000];
		if (ReqSmpl > LastSmpl)
		{
			//chip->state = STATE_IDLE;
			break;
		}

		//chip->state = STATE_DUMMY1;
		NibbleCnt = 0x08 + LastSmpl * 2;
		for (CurOfs = 0x00; CurOfs < NibbleCnt; CurOfs ++)
			chip->ROMUsage[chip->romoffset | CurOfs] |= 0x01;

		//chip->state = STATE_ADDR_MSB;
		CurOfs = chip->ROMData[chip->romoffset | (ReqSmpl * 2 + 5)] << 9;

		//chip->state = STATE_ADDR_LSB;
		CurOfs |= chip->ROMData[chip->romoffset | (ReqSmpl * 2 + 6)] << 1;

		//chip->state = STATE_DUMMY2;
		chip->ROMUsage[chip->romoffset | (CurOfs & 0x1FFFF)] |= 0x01;
		CurOfs ++;
		//chip->first_valid_header = 0;
		OldVal = 0;

		//chip->state = STATE_BLOCK_HEADER;
		ReqSmpl = 0x00;	// Exit Loop = 0;
		do
		{
			BlkHdr = chip->ROMData[chip->romoffset | (CurOfs & 0x1FFFF)];
			chip->ROMUsage[chip->romoffset | (CurOfs & 0x1FFFF)] |= 0x01;
			CurOfs ++;

			// our next step depends on the top two bits
			switch(BlkHdr & 0xC0)
			{
			case 0x00:	// silence
				//chip->clocks_left = 1024 * ((BlkHdr & 0x3F) + 1);
				NibbleCnt = 0;
				//chip->state = (BlkHdr == 0 && OldVal) ? STATE_IDLE : STATE_BLOCK_HEADER;
				break;
			case 0x40:	// 256 nibbles
				//chip->sample_rate = (BlkHdr & 0x3F) + 1;
				NibbleCnt = 256;
				//chip->state = STATE_NIBBLE_MSN;
				break;
			case 0x80:	// n nibbles
				//chip->sample_rate = (BlkHdr & 0x3F) + 1;
				//chip->state = STATE_NIBBLE_COUNT;
				NibbleCnt = chip->ROMData[chip->romoffset | (CurOfs & 0x1FFFF)] + 1;
				chip->ROMUsage[chip->romoffset | (CurOfs & 0x1FFFF)] |= 0x01;
				CurOfs ++;
				break;
			case 0xC0:	// repeat loop
				//chip->repeat_count = (BlkHdr & 0x07) + 1;
				//chip->repeat_offset = CurOfs;
				//chip->state = STATE_BLOCK_HEADER;
				NibbleCnt = 0;
				break;
			}

			if (BlkHdr)
				OldVal = 1;
			if (NibbleCnt)
			{
				NibbleCnt = (NibbleCnt + 1) >> 1;	// convert to bytes
				while(NibbleCnt)
				{
					chip->ROMUsage[chip->romoffset | (CurOfs & 0x1FFFF)] |= 0x01;
					CurOfs ++;
					NibbleCnt --;
				}
			}
		//} while(chip->state == STATE_BLOCK_HEADER);
		} while(! OldVal || BlkHdr);

		if (CurOfs & 0x01)
		{
			// pad to even bytes
			chip->ROMUsage[chip->romoffset | (CurOfs & 0x1FFFF)] |= 0x01;
		}

		break;
	case 0x02:	// upd7759_port_w
		chip->fifo_in = Data;
		break;
	case 0x03:	// upd7759_set_bank_base
		chip->romoffset = Data * 0x20000;
		break;
	}

	return;
}

void nes_apu_write(UINT8 Register, UINT8 Data)
{
	NES_APU_DATA* chip = &ChDat->NES_APU;
	UINT16 RemBytes;
	UINT16 CurAddr;

	if (Register >= 0x20)
		return;

	switch(Register)
	{
//	case 0x12:	// APU_WRE2
//		break;
//	case 0x13:	// APU_WRE3
//		break;
	case 0x15:	// APU_SMASK
		if (Data & 0x10)
		{
			//RemBytes = (chip->RegData[0x13] << 4) + 1;
			RemBytes = (chip->RegData[0x13] << 4) + 0x10;
			CurAddr = 0xC000 + (chip->RegData[0x12] << 6);
			while(RemBytes)
			{
				chip->ROMUsage[CurAddr] |= 0x01;
				CurAddr ++;
				CurAddr |= 0x8000;	// the hardware does this
				RemBytes --;
			}
		}
		break;
	}
	chip->RegData[Register] = Data;

	return;
}

void multipcm_write(UINT8 Port, UINT8 Data)
{
	MULTIPCM_DATA* chip = &ChDat->MultiPCM;
	MULTIPCM_SLOT* TempChn;
	UINT32 TOCAddr;
	UINT32 AddrSt;
	UINT32 AddrEnd;
	UINT32 CurAddr;

	switch(Port)
	{
	case 0x00:
		break;	// continue as usual
	case 0x01:
		if ((Data & 0x07) == 0x07)
			chip->CurSlot = 0xFF;
		chip->CurSlot = ((Data & 0x18) / 0x08 * 7) + (Data & 0x07);
		return;
	case 0x02:
		chip->Address = Data;
		return;
	// special SEGA banking
	case 0x10:	// 1 MB banking (Sega Model 1)
		chip->SegaBanking = 1;
		chip->Bank0 = (Data << 20) | 0x000000;
		chip->Bank1 = (Data << 20) | 0x080000;
		return;
	case 0x11:	// 512 KB banking - low bank (Sega Multi 32)
		chip->SegaBanking = 1;
		chip->Bank0 = Data << 19;
		return;
	case 0x12:	// 512 KB banking - high bank (Sega Multi 32)
		chip->SegaBanking = 1;
		chip->Bank1 = Data << 19;
		return;
	default:
		return;
	}
	if (chip->CurSlot == 0xFF)
		return;
	TempChn = &chip->slot[chip->CurSlot];

	switch(chip->Address)
	{
	case 0x01:	// set Sample
		if (TempChn->SmplID == Data)
			return;
		TempChn->SmplID = Data;

#if 0
		// mark single sample
		if (chip->SmplMask[TempChn->SmplID] & 0x80)
			break;
		chip->SmplMask[TempChn->SmplID] |= 0x80;

		AddrSt = TempChn->SmplID * 0x0C;
		AddrEnd = AddrSt + 0x0C;
#else
		// mark whole sample table
		if (chip->SmplMask[0x00] & 0x80)
			break;
		chip->SmplMask[0x00] |= 0x80;

		AddrSt = 0x00;
		AddrEnd = 0x200 * 0x0C;
		// find the real end of the sample table (assume padding with FF)
		while(AddrEnd > AddrSt)
		{
			memcpy(&CurAddr, &chip->ROMData[AddrEnd - 0x04], 0x04);
			if (CurAddr != 0xFFFFFFFF)
				break;
			AddrEnd -= 0x04;
		}
		AddrEnd = (AddrEnd + 0x0B) & ~0x0B;	// round up to 0x0C
#endif
		for (CurAddr = AddrSt; CurAddr < AddrEnd; CurAddr ++)
			chip->ROMUsage[CurAddr] |= 0x01;
		break;
	case 0x04:	// Key On/Off
		TempChn->Playing = Data & 0x80;
		break;
	default:
		return;
	}
	if (! TempChn->Playing)
		return;

	// Was this sample marked already?
	if (chip->SmplMask[TempChn->SmplID] & 0x01)
		return;
	// if not, set the bit to speed the process up
	chip->SmplMask[TempChn->SmplID] |= 0x01;

	TOCAddr = TempChn->SmplID * 0x0C;
	AddrSt =	(chip->ROMData[TOCAddr + 0x00] << 16) |
				(chip->ROMData[TOCAddr + 0x01] <<  8) |
				(chip->ROMData[TOCAddr + 0x02] <<  0);
	AddrEnd =	(chip->ROMData[TOCAddr + 0x05] <<  8) |
				(chip->ROMData[TOCAddr + 0x06] <<  0);
	if (chip->SegaBanking)
	{
		AddrSt &= 0x1FFFFF;
		if (AddrSt & 0x100000)
		{
			if (AddrSt & 0x080000)
				AddrSt = (AddrSt & 0x07FFFF) | chip->Bank1;
			else
				AddrSt = (AddrSt & 0x07FFFF) | chip->Bank0;
		}
	}
	AddrEnd = AddrSt + (0x10000 - AddrEnd);

	for (CurAddr = AddrSt; CurAddr < AddrEnd; CurAddr ++)
		chip->ROMUsage[CurAddr] |= 0x01;

	return;
}

void multipcm_bank_write(UINT8 Bank, UINT16 Data)
{
	MULTIPCM_DATA* chip = &ChDat->MultiPCM;

	if ((Bank & 0x03) == 0x03 && ! (Data & 0x08))
	{
		// 1 MB banking (reg 0x10)
		multipcm_write(0x10, Data / 0x10);
	}
	else
	{
		// 512 KB banking (regs 0x11/0x12)
		if (Bank & 0x02)	// low bank
			multipcm_write(0x11, Data / 0x08);
		if (Bank & 0x01)	// high bank
			multipcm_write(0x12, Data / 0x08);
	}

	return;
}

void x1_010_write(UINT16 offset, UINT8 data)
{
	X1_010_DATA *chip =  &ChDat->X1_010;
	X1_010_CHANNEL *reg;
	X1_010_CHNINFO *chnInf;
	int channel, regi;
	UINT32 addr, startpos, endpos;

	channel = offset/sizeof(X1_010_CHANNEL);
	regi    = offset%sizeof(X1_010_CHANNEL);

	/* Control register write */
	if( channel < SETA_NUM_CHANNELS && regi == 0)
	{
		reg = (X1_010_CHANNEL *)&(chip->reg[channel*sizeof(X1_010_CHANNEL)]);
		chnInf = &chip->chnInfo[channel];

		/* Key on and PCM mode set? */

		// check at key off (or next status write). Leaving this in case the other condition breaks something.
		//if( (reg->status&1) && !(reg->status&2) )

		// Check at key on. should work better for guardians.
		if((data&1) && !(data&02))
		{
			startpos = reg->start*0x1000;
			endpos = (0x100-reg->end)*0x1000;
			if (startpos != chnInf->lastStart || endpos != chnInf->lastEnd)
			{
				chnInf->lastStart = startpos;
				chnInf->lastEnd = endpos;
				for (addr = startpos; addr < endpos; addr ++)
					chip->ROMUsage[addr] |= 0x01;
			}
		}
	}
	chip->reg[offset] = data;
	return;
}

void c352_write(UINT16 Offset, UINT16 val)
{
	C352_DATA *chip = &ChDat->C352;
	C352_CHANNEL* TempChn;
	UINT16 address = Offset* 2;
	UINT32 addr, startpos, endpos, reptpos;

	UINT16 chan;
	int i, rev, wrap, ldir;

	chan = (address >> 4) & 0xfff;

	if (address >= 0x400)
	{
		switch(address)
		{
		case 0x404:	// execute key-ons/offs
			for ( i = 0 ; i <= 31 ; i++ )
			{
				TempChn = &chip->channels[i];
				if (TempChn->flag & C352_FLG_KEYON)
				{
					if (TempChn->start_addr != TempChn->end_addr)
					{
						rev = TempChn->flag & C352_FLG_REVERSE;
						ldir = TempChn->flag & C352_FLG_LDIR;

						// reverse loops are actually bidirectional, the loop direction flag decides
						// the initial direction. If it is unset, the sample will play "forwards" first
						// and should thus be treated as a forwards sample
						if((TempChn->flag & C352_FLG_LOOP) && !ldir)
							rev = 0;

						wrap = rev ? TempChn->end_addr > TempChn->start_addr
								   : TempChn->end_addr < TempChn->start_addr;

						startpos = wrap && rev ? ((TempChn->bank-1)<<16) : (TempChn->bank<<16);
						endpos = wrap && !rev ? ((TempChn->bank+1)<<16) : (TempChn->bank<<16);
						reptpos = (endpos & 0x00FF0000) + TempChn->repeat_addr;

						startpos += rev ? TempChn->end_addr : TempChn->start_addr;
						endpos += rev ? TempChn->start_addr : TempChn->end_addr;

						if ((TempChn->flag & C352_FLG_LOOP) && reptpos<startpos)
							startpos=reptpos;

						if (startpos < chip->ROMSize)
						{
							if (endpos <= chip->ROMSize)
							{
								for (addr = startpos; addr <= endpos; addr ++)
									chip->ROMUsage[addr] |= 0x01;
							}
							else
							{
								// found overflowing address counter
								endpos -= chip->ROMSize;
								for (addr = startpos; addr < chip->ROMSize; addr ++)
									chip->ROMUsage[addr] |= 0x01;
								for (addr = 0x00; addr <= endpos; addr ++)
									chip->ROMUsage[addr] |= 0x01;
							}
						}
					}
					TempChn->flag &= ~(C352_FLG_KEYON);
					TempChn->flag |= C352_FLG_BUSY;
				}
				else if (TempChn->flag & C352_FLG_KEYOFF)
				{
					TempChn->flag &= ~(C352_FLG_KEYOFF);
					TempChn->flag &= ~(C352_FLG_BUSY);
				}
			}
			break;
		default:
			break;
		}
		return;
	}

	if (chan > 31)
		return;
	TempChn = &chip->channels[chan];
	startpos = (UINT32)-1;
	switch(address & 0xf)
	{
	case 0x0:	// volumes (output 1)
	case 0x2:	// volumes (output 2)
	case 0x4:	// pitch
		break;
	case 0x6:	// flags
		TempChn->flag = val;
		if ((TempChn->flag & C352_FLG_REVERSE) && (TempChn->flag & C352_FLG_BUSY))
		{
			val &= 0xFF;
			startpos = (val<<16) +TempChn->repeat_addr;
			endpos = (val<<16) + TempChn->end_addr;
		}
		break;
	case 0x8:	// bank (bits 16-31 of address);
		TempChn->bank = val & 0xff;
		break;
	case 0xa:	// start address
		TempChn->start_addr = val;
		// Handle linked samples
		if ((TempChn->flag & C352_FLG_LINK) && (TempChn->flag & C352_FLG_BUSY))
		{
			val &= 0xFF;
			startpos = (val<<16) +TempChn->repeat_addr;
			endpos = (val<<16) + TempChn->end_addr;
		}
		break;
	case 0xc:	// end address
		TempChn->end_addr = val;
		break;
	case 0xe:	// loop address
		TempChn->repeat_addr = val;
		break;
	default:
		break;
	}

	if (startpos != (UINT32)-1)
	{
		if (startpos >= chip->ROMSize)
			return;

		if (endpos < startpos)
			endpos += 0x10000;
		if (endpos <= chip->ROMSize)
		{
			for (addr = startpos; addr <= endpos; addr ++)
				chip->ROMUsage[addr] |= 0x01;
		}
		else
		{
			// found overflowing address counter
			endpos -= chip->ROMSize;
			for (addr = startpos; addr < chip->ROMSize; addr ++)
				chip->ROMUsage[addr] |= 0x01;
			for (addr = 0x00; addr <= endpos; addr ++)
				chip->ROMUsage[addr] |= 0x01;
		}
	}
	return;
}

void ga20_write(UINT8 offset, UINT8 data)
{
	GA20_DATA *chip = &ChDat->GA20;
	GA20_CHANNEL* TempChn;
	int channel;
	UINT32 addr, startpos, endpos;

	channel = offset >> 3;
	TempChn = &chip->channels[channel];
	switch (offset & 0x7)
	{
	case 0:	// start address low
		TempChn->start = (TempChn->start&0xff000) | (data<<4);
		break;
	case 1:	// start address high
		TempChn->start = (TempChn->start&0x00ff0) | (data<<12);
		break;
	case 2:	// end address low
		TempChn->end = (TempChn->end&0xff000) | (data<<4);
		break;
	case 3:	// end address high
		TempChn->end = (TempChn->end&0x00ff0) | (data<<12);
		break;
	case 4:
	case 5:
		break;
	case 6:	//AT: this is always written 2(enabling both channels?)
		if(data)
		{
			startpos = TempChn->start;
			endpos = TempChn->end;
			for (addr = startpos; addr < endpos; addr ++)
				chip->ROMUsage[addr] |= 0x01;
		}
		break;
	}
	return;
}

void es5503_write(UINT8 Register, UINT8 Data)
{
	ES5503_DATA *chip = &ChDat->ES5503;
	ES5503_OSC* TempOsc;
	UINT32 AddrSt;
	UINT32 AddrEnd;
	UINT32 CurAddr;

	if (Register < 0xE0)
	{
		TempOsc = &chip->oscillators[Register & 0x1F];
		switch(Register & 0xE0)
		{
		case 0x40:	// volume
			TempOsc->vol = Data;
			break;
		case 0x80:	// wavetable pointer
			TempOsc->wavetblpointer = (Data << 8);
			break;
		case 0xA0:	// oscillator control
			// if a fresh key-on, reset the ccumulator
			/*if ((TempOsc->control & 1) && (!(Data&1)))
			{
				chip->oscillators[osc].accumulator = 0;
			}*/
			TempOsc->control = Data;
			if (! (TempOsc->control & 0x01))
			{
				if ((TempOsc->control & 0x08) && ! TempOsc->vol)
					break;	// IRQ + Vol 0 -> probably used for timing

				AddrSt = TempOsc->wavetblpointer & es5503_wavemasks[TempOsc->wavetblsize];
				AddrEnd = AddrSt + TempOsc->wtsize;
				for (CurAddr = AddrSt; CurAddr < AddrEnd; CurAddr ++)
				{
					chip->ROMUsage[CurAddr] |= 0x01;
					if (! chip->ROMData[CurAddr])
						break;
				}
				if ((TempOsc->control & 0x06) == 0x06)	// 'swap' mode
				{
					TempOsc = &chip->oscillators[(Register & 0x1F) ^ 0x01];

					AddrSt = TempOsc->wavetblpointer & es5503_wavemasks[TempOsc->wavetblsize];
					AddrEnd = AddrSt + TempOsc->wtsize;
					for (CurAddr = AddrSt; CurAddr < AddrEnd; CurAddr ++)
					{
						chip->ROMUsage[CurAddr] |= 0x01;
						if (! chip->ROMData[CurAddr])
							break;
					}
				}
			}
			break;
		case 0xC0:	// bank select / wavetable size / resolution
			if (Data & 0x40)	// bank select - not used on the Apple IIgs
				TempOsc->wavetblpointer |= 0x10000;
			else
				TempOsc->wavetblpointer &= 0x0FFFF;

			TempOsc->wavetblsize = (Data & 0x38) >> 3;
			TempOsc->wtsize = 0x100 << TempOsc->wavetblsize;
			//TempOsc->resolution = (Data & 0x07);
			break;
		}
	}
	else	// global registers
	{
		switch(Register)
		{
		case 0xE1:	// oscillator enable
			//chip->oscsenabled = 1 + ((Data>>1) & 0x1F);
			break;
		}
	}

	return;
}

void ymf278b_write(UINT8 Port, UINT8 Register, UINT8 Data)
{
	YMF278B_DATA* chip = &ChDat->YMF278B;
	YMF278B_SLOT* slot;
	UINT8 SlotNum;
	UINT8 SmplBits;
	UINT32 TOCAddr;
	UINT32 AddrSt;
	UINT32 AddrEnd;
	UINT32 CurAddr;
	UINT16 tblBaseSmpl;

	if (Port != 0x02)	// Port 0/1 = FM, Port 2 = PCM
		return;
	if (Register >= 0x08 && Register < 0xF8)
	{
		SlotNum = (Register - 0x08) % 24;
		slot = &chip->slots[SlotNum];
		switch((Register - 0x08) / 24)
		{
		case 0x00:	// Sample ID LSB (bits 0-7)
			slot->SmplID &= 0x100;
			slot->SmplID |= Data;

			if (slot->SmplID < 384 || ! chip->wavetblhdr)
				tblBaseSmpl = 0;
			else
				tblBaseSmpl = 384;

			// --- mark Sample Table ---
#if 0
			// mark single sample
			if (! (chip->SmplMask[slot->SmplID] & 0x80))
			{
				chip->SmplMask[slot->SmplID] |= 0x80;

				if (slot->SmplID < 384 || ! chip->wavetblhdr)
					AddrSt = slot->SmplID * 0x0C;
				else
					AddrSt = chip->wavetblhdr * 0x80000 + (slot->SmplID - 384) * 0x0C;
				AddrEnd = AddrSt + 0x0C;
				if (AddrSt < chip->RAMBase)
				{
					for (CurAddr = AddrSt; CurAddr < AddrEnd; CurAddr ++)
						chip->ROMUsage[CurAddr] |= 0x01;
				}
				else
				{
					AddrSt -= chip->RAMBase;
					AddrEnd -= chip->RAMBase;
					for (CurAddr = AddrSt; CurAddr < AddrEnd; CurAddr ++)
						chip->RAMUsage[CurAddr] |= 0x01;
				}
			}
#else
			// mark whole sample table
			if (! (chip->SmplMask[tblBaseSmpl] & 0x80))
			{
				const UINT8* MemData;
				UINT8* MemUsage;

				chip->SmplMask[tblBaseSmpl] |= 0x80;

				AddrSt = (tblBaseSmpl) ? (chip->wavetblhdr * 0x80000) : 0x00;
				// get address of first sample
				if (AddrSt < chip->RAMBase)
				{
					printf("mark Sample Headers, ROM offset 0x%06X\n", AddrSt);
					if (AddrSt >= chip->ROMSize)
						break;	// ROM not available
					AddrEnd =	(chip->ROMData[AddrSt + 0x00] << 16) |
								(chip->ROMData[AddrSt + 0x01] <<  8) |
								(chip->ROMData[AddrSt + 0x02] <<  0);
				}
				else
				{
					printf("mark Sample Headers, RAM offset 0x%06X\n", AddrSt - chip->RAMBase);
					if (AddrSt - chip->RAMBase >= chip->RAMSize)
						break;	// RAM not available
					AddrEnd =	(chip->RAMData[AddrSt - chip->RAMBase + 0x00] << 16) |
								(chip->RAMData[AddrSt - chip->RAMBase + 0x01] <<  8) |
								(chip->RAMData[AddrSt - chip->RAMBase + 0x02] <<  0);
				}
				AddrEnd &= 0x3FFFFF;
				if (tblBaseSmpl)
				{
					if (AddrEnd > chip->RAMBase + 128 * 0x0C)
						AddrEnd = chip->RAMBase + 128 * 0x0C;
				}
				else if (chip->wavetblhdr)
				{
					if (AddrEnd > 384 * 0x0C)
						AddrEnd = 384 * 0x0C;
				}
				else
				{
					if (AddrEnd > 512 * 0x0C)
						AddrEnd = 512 * 0x0C;
				}

				if (AddrSt < chip->RAMBase)
				{
					if (AddrEnd > chip->ROMSize)
						AddrEnd = chip->ROMSize;
					MemData = chip->ROMData;
					MemUsage = chip->ROMUsage;
				}
				else
				{
					if (AddrEnd < chip->RAMBase)
						AddrEnd = chip->RAMBase;
					AddrSt -= chip->RAMBase;
					AddrEnd -= chip->RAMBase;
					if (AddrEnd > chip->RAMSize)
						AddrEnd = chip->RAMSize;
					MemData = chip->RAMData;
					MemUsage = chip->RAMUsage;
				}
				// find the real end of the sample table (assume padding with FF)
				while(AddrEnd > AddrSt)
				{
					memcpy(&CurAddr, &MemData[AddrEnd - 0x04], 0x04);
					if (CurAddr != 0xFFFFFFFF && CurAddr != 0x00000000)
						break;
					AddrEnd -= 0x04;
				}
				AddrEnd = (AddrEnd + 0x0B) / 0x0C * 0x0C;	// round up to 0x0C
				for (CurAddr = AddrSt; CurAddr < AddrEnd; CurAddr ++)
					MemUsage[CurAddr] |= 0x01;
			}
#endif

			// --- mark actual Sample Data ---
			if (! (chip->SmplMask[slot->SmplID] & 0x01))
			{
				const UINT8* MemData;

				chip->SmplMask[slot->SmplID] |= 0x01;

				if (slot->SmplID < 384 || ! chip->wavetblhdr)
					TOCAddr = slot->SmplID * 0x0C;
				else
					TOCAddr = chip->wavetblhdr * 0x80000 + (slot->SmplID - 384) * 0x0C;
				if (TOCAddr < chip->RAMBase)
				{
					if (TOCAddr >= chip->ROMSize)
						break;
					MemData = chip->ROMData;
				}
				else
				{
					TOCAddr -= chip->RAMBase;
					if (TOCAddr >= chip->RAMSize)
						break;
					MemData = chip->RAMData;
				}
				SmplBits =	(MemData[TOCAddr + 0x00] & 0xC0) >> 6;
				AddrSt =	(MemData[TOCAddr + 0x00] << 16) |
							(MemData[TOCAddr + 0x01] <<  8) |
							(MemData[TOCAddr + 0x02] <<  0);
				AddrEnd =	(MemData[TOCAddr + 0x05] <<  8) |
							(MemData[TOCAddr + 0x06] <<  0);
				AddrSt &= 0x3FFFFF;
				AddrEnd = (0x10000 - AddrEnd);
				if (SmplBits == 0)	// 8-bit
					AddrEnd = AddrEnd * 1;
				else if (SmplBits == 1)	// 12-bit
					AddrEnd = (AddrEnd * 3 + 1) / 2;
				else if (SmplBits == 2)	// 16-bit
					AddrEnd = AddrEnd * 2;
				else if (SmplBits == 3)	// invalid
					AddrEnd = 0;
				AddrEnd += AddrSt;

				if (AddrSt < chip->RAMBase)
				{
					printf("mark Sample %u, ROM offset 0x%06X\n", slot->SmplID, AddrSt);
					if (AddrSt >= chip->ROMSize)
						break;
					if (AddrEnd >= chip->ROMSize)
						AddrEnd = chip->ROMSize;
					for (CurAddr = AddrSt; CurAddr < AddrEnd; CurAddr ++)
						chip->ROMUsage[CurAddr] |= 0x01;
				}
				else
				{
					if (AddrEnd < chip->RAMBase)
						AddrEnd = chip->RAMBase;
					AddrSt -= chip->RAMBase;
					AddrEnd -= chip->RAMBase;
					printf("mark Sample %u, RAM offset 0x%06X\n", slot->SmplID, AddrSt);
					if (AddrSt >= chip->RAMSize)
						break;
					if (AddrEnd >= chip->RAMSize)
						AddrEnd = chip->RAMSize;
					for (CurAddr = AddrSt; CurAddr < AddrEnd; CurAddr ++)
						chip->RAMUsage[CurAddr] |= 0x01;
				}
			}
			break;
		case 0x01:	// FNum LSB, Sample ID MSB (bit 8)
			slot->SmplID &= 0x0FF;
			slot->SmplID |= (Data & 0x01) << 8;
			break;
		}
	}
	else
	{
		switch(Register)
		{
		case 0x02:
			chip->wavetblhdr = (Data & 0x1C) >> 2;
			chip->memmode = (Data & 0x01);
			break;
		}
	}

	return;
}

static void es550x_do_ctrl(ES5506_DATA* chip, ES5506_VOICE* voice)
{
	ES5506_ROM* romBase = &chip->Rgn[(voice->control >> 14) & 0x03];
	UINT32 baseOfs = voice->exbank;
	UINT32 AddrSt;
	UINT32 AddrEnd;
	UINT32 CurAddr;

	if ((chip->curPage & 0x1F) >= chip->voiceCount)
		return;
	if (voice->control & ES6CTRL_STOPMASK)
		return;

	if (voice->start == voice->end)
		return;
	if (voice->start > voice->end)
		return;

	CurAddr = voice->accum >> 11;
	AddrSt = voice->start >> 11;
	AddrEnd = voice->end >> 11;
	if (! (voice->control & ES6CTRL_DIR))
	{
		// playing forwards
		if (CurAddr < AddrSt)
			AddrSt = CurAddr;
	}
	else
	{
		// playing backwards
		if (CurAddr > AddrEnd)
			AddrEnd = CurAddr;
	}
	AddrEnd ++;	// turn <= into <

	// Offsets are indices into 16-Bit data.
	baseOfs *= 2;
	AddrSt *= 2;	AddrEnd *= 2;
	for (CurAddr = AddrSt; CurAddr < AddrEnd; CurAddr ++)
		romBase->ROMUsage[baseOfs + CurAddr] |= 0x01;

	return;
}

static void es5505_w(ES5506_DATA* chip, UINT8 Offset, UINT8 Data)
{
}

static void es5506_w(ES5506_DATA* chip, UINT8 Offset, UINT8 Data)
{
	ES5506_VOICE* voice = &chip->voice[chip->curPage & 0x1F];
	int shift = 8 * (Offset & 0x03);
	Offset >>= 2;

	// build a 32-Bit Big Endian value
	chip->writeLatch &= ~(0xFF000000 >> shift);
	chip->writeLatch |= (Data << (24 - shift));
	if (shift != 24)
		return;

	if (Offset >= 0x68/8)	// PAGE
	{
		switch(Offset)
		{
		case 0x68/8:	// PAR - read only
		case 0x70/8:	// IRQV - read only
			break;
		case 0x78/8:	// PAGE
			chip->curPage = chip->writeLatch & 0x7F;
			break;
		}
	}
	else if (chip->curPage < 0x20)
	{
		// es5506_reg_write_low
		switch(Offset)
		{
		case 0x00/8:	// CR
			voice->control = chip->writeLatch & 0x0000FFFF;
			es550x_do_ctrl(chip, voice);
			break;
		case 0x08/8:	// Sample Step (Frequency Count)
		case 0x10/8:	// Volume L
		case 0x18/8:	// Volume Ramp L
		case 0x20/8:	// Volume R
		case 0x28/8:	// Volume Ramp R
		case 0x30/8:	// Envelope Length (Envelope Count)
		case 0x38/8:	// K2 Filter
		case 0x40/8:	// K2 Filter Ramp
		case 0x48/8:	// K1 Filter
		case 0x50/8:	// K1 Filter Ramp
			break;
		case 0x58/8:	// ACTV
			chip->voiceCount = 1 + (chip->writeLatch & 0x1F);
			break;
		case 0x60/8:	// MODE
			//chip->mode = chip->writeLatch & 0x1f;
			break;
		}
	}
	else if (chip->curPage < 0x40)
	{
		// es5506_reg_write_high
		switch(Offset)
		{
		case 0x00/8:	// CR
			voice->control = chip->writeLatch & 0x0000FFFF;
			es550x_do_ctrl(chip, voice);
			break;
		case 0x08/8:	// START
			voice->start = chip->writeLatch & 0xFFFFF800;
			break;
		case 0x10/8:	// END
			voice->end = chip->writeLatch & 0xFFFFFF80;
			break;
		case 0x18/8:	// ACCUM
			voice->accum = chip->writeLatch;
			break;
		case 0x20/8:	// O4(n-1)
		case 0x28/8:	// O3(n-1)
		case 0x30/8:	// O3(n-2)
		case 0x38/8:	// O2(n-1)
		case 0x40/8:	// O2(n-2)
		case 0x48/8:	// O1(n-1)
			break;
		case 0x50/8:	// W_ST
		case 0x58/8:	// W_END
		case 0x60/8:	// LR_END
			break;
		}
	}
	else
	{
		// es5506_reg_write_test
		// nothing important here
	}

	chip->writeLatch = 0;

	return;
}


void es550x_w(UINT8 Offset, UINT8 Data)
{
	ES5506_DATA* chip = &ChDat->ES5506;

	if (Offset == 0xFF)
	{
		chip->chipType = Data;
		return;
	}

	if (Offset < 0x40)
	{
		if (! chip->chipType)
			es5505_w(chip, Offset, Data);
		else
			es5506_w(chip, Offset, Data);
	}
	else
	{
		chip->voice[Offset & 0x1F].exbank = Data << 20;
	}
	return;
}

void es550x_w16(UINT8 Offset, UINT16 Data)
{
	ES5506_DATA* chip = &ChDat->ES5506;

	if (Offset < 0x40)
	{
		if (! chip->chipType)
		{
			es5505_w(chip, Offset | 0, (Data & 0xFF00) >> 8);
			es5505_w(chip, Offset | 1, (Data & 0x00FF) >> 0);
		}
		else
		{
			es5506_w(chip, Offset | 0, (Data & 0xFF00) >> 8);
			es5506_w(chip, Offset | 1, (Data & 0x00FF) >> 0);
		}
	}
	else
	{
		chip->voice[Offset & 0x1F].exbank = Data << 20;
	}
	return;
}

#define ROM_BORDER_CHECK					\
	if (DataStart > ROMSize)				\
		return;								\
	if (DataStart + DataLength > ROMSize)	\
		DataLength = ROMSize - DataStart;

void write_rom_data(UINT8 ROMType, UINT32 ROMSize, UINT32 DataStart, UINT32 DataLength,
					const UINT8* ROMData)
{
	SEGAPCM_DATA *spcm;
	YM2608_DATA* ym2608;
	YM2610_DATA* ym2610;
	Y8950_DATA* y8950;
	YMZ280B_DATA* ymz280b;
	RF5C_DATA* rf5c;
	NES_APU_DATA* nes_apu;
	YMF278B_DATA* ymf278;
	YMF271_DATA* ymf271;
	UPD7759_DATA* upd7759;
	OKIM6295_DATA* okim6295;
	MULTIPCM_DATA* multipcm;
	K054539_DATA* k054539;
	C140_DATA* c140;
	K053260_DATA* k053260;
	QSOUND_DATA* qsound;
	ES5503_DATA* es5503;
	ES5506_DATA* es5506;
	X1_010_DATA* x1_010;
	C352_DATA* c352;
	GA20_DATA* ga20;
	K007232_DATA* k007232;
	switch(ROMType)
	{
	case 0x80:	// SegaPCM ROM
		spcm = &ChDat->SegaPCM;
		if (spcm->ROMSize != ROMSize)
		{
			UINT32 mask, rom_mask;

			// fix wrong ROM Sizes (like 0x60000)
			for (rom_mask = 1; rom_mask < ROMSize; rom_mask *= 2);
			ROMSize = rom_mask;

			spcm->ROMData = (UINT8*)realloc(spcm->ROMData, ROMSize);
			spcm->ROMUsage = (UINT8*)realloc(spcm->ROMUsage, ROMSize);
			spcm->ROMSize = ROMSize;
			memset(spcm->ROMData, 0xFF, ROMSize);
			memset(spcm->ROMUsage, 0x02, ROMSize);

			// recalculate bankmask
			spcm->bankshift = (UINT8)(spcm->intf_bank);
			mask = spcm->intf_bank >> 16;
			if (! mask)
				mask = SPCM_BANK_MASK7 >> 16;

			spcm->rgnmask = ROMSize - 1;
			for (rom_mask = 1; rom_mask < ROMSize; rom_mask *= 2);
			rom_mask --;

			spcm->bankmask = mask & (rom_mask >> spcm->bankshift);
		}

		ROM_BORDER_CHECK
		memcpy(spcm->ROMData + DataStart, ROMData, DataLength);
		memset(spcm->ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x81:	// YM2608 DELTA-T ROM
		ym2608 = &ChDat->YM2608;

		if (ym2608->DT_ROMSize != ROMSize)
		{
			ym2608->DT_ROMData = (UINT8*)realloc(ym2608->DT_ROMData, ROMSize);
			ym2608->DT_ROMUsage = (UINT8*)realloc(ym2608->DT_ROMUsage, ROMSize);
			ym2608->DT_ROMSize = ROMSize;
			memset(ym2608->DT_ROMData, 0xFF, ROMSize);
			memset(ym2608->DT_ROMUsage, 0x02, ROMSize);

			ym2608->DeltaT.memory_size = ym2608->DT_ROMSize;
			ym2608->DeltaT.memory = ym2608->DT_ROMData;
			ym2608->DeltaT.memory_usg = ym2608->DT_ROMUsage;
		}

		ROM_BORDER_CHECK
		memcpy(ym2608->DT_ROMData + DataStart, ROMData, DataLength);
		memset(ym2608->DT_ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x82:	// YM2610 ADPCM ROM
		ym2610 = &ChDat->YM2610;

		if (ym2610->AP_ROMSize != ROMSize)
		{
			ym2610->AP_ROMData = (UINT8*)realloc(ym2610->AP_ROMData, ROMSize);
			ym2610->AP_ROMUsage = (UINT8*)realloc(ym2610->AP_ROMUsage, ROMSize);
			ym2610->AP_ROMSize = ROMSize;
			memset(ym2610->AP_ROMData, 0xFF, ROMSize);
			memset(ym2610->AP_ROMUsage, 0x02, ROMSize);
		}

		ROM_BORDER_CHECK
		memcpy(ym2610->AP_ROMData + DataStart, ROMData, DataLength);
		memset(ym2610->AP_ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x83:	// YM2610 DELTA-T ROM
		ym2610 = &ChDat->YM2610;

		if (ym2610->DT_ROMSize != ROMSize)
		{
			ym2610->DT_ROMData = (UINT8*)realloc(ym2610->DT_ROMData, ROMSize);
			ym2610->DT_ROMUsage = (UINT8*)realloc(ym2610->DT_ROMUsage, ROMSize);
			ym2610->DT_ROMSize = ROMSize;
			memset(ym2610->DT_ROMData, 0xFF, ROMSize);
			memset(ym2610->DT_ROMUsage, 0x02, ROMSize);

			ym2610->DeltaT.memory_size = ym2610->DT_ROMSize;
			ym2610->DeltaT.memory = ym2610->DT_ROMData;
			ym2610->DeltaT.memory_usg = ym2610->DT_ROMUsage;
		}

		ROM_BORDER_CHECK
		memcpy(ym2610->DT_ROMData + DataStart, ROMData, DataLength);
		memset(ym2610->DT_ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x84:	// YMF278B ROM
		ymf278 = &ChDat->YMF278B;

		if (ymf278->ROMSize != ROMSize)
		{
			ymf278->ROMData = (UINT8*)realloc(ymf278->ROMData, ROMSize);
			ymf278->ROMUsage = (UINT8*)realloc(ymf278->ROMUsage, ROMSize);
			ymf278->ROMSize = ROMSize;
			ymf278->RAMBase = ROMSize;
			memset(ymf278->ROMData, 0xFF, ROMSize);
			memset(ymf278->ROMUsage, 0x02, ROMSize);
		}

		ROM_BORDER_CHECK
		memcpy(ymf278->ROMData + DataStart, ROMData, DataLength);
		memset(ymf278->ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x85:	// YMF271 ROM
		ymf271 = &ChDat->YMF271;

		if (ymf271->ROMSize != ROMSize)
		{
			ymf271->ROMData = (UINT8*)realloc(ymf271->ROMData, ROMSize);
			ymf271->ROMUsage = (UINT8*)realloc(ymf271->ROMUsage, ROMSize);
			ymf271->ROMSize = ROMSize;
			memset(ymf271->ROMData, 0xFF, ROMSize);
			memset(ymf271->ROMUsage, 0x02, ROMSize);
		}

		ROM_BORDER_CHECK
		memcpy(ymf271->ROMData + DataStart, ROMData, DataLength);
		memset(ymf271->ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x86:	// YMZ280B ROM
		ymz280b = &ChDat->YMZ280B;

		if (ymz280b->ROMSize != ROMSize)
		{
			ymz280b->ROMData = (UINT8*)realloc(ymz280b->ROMData, ROMSize);
			ymz280b->ROMUsage = (UINT8*)realloc(ymz280b->ROMUsage, ROMSize);
			ymz280b->ROMSize = ROMSize;
			memset(ymz280b->ROMData, 0xFF, ROMSize);
			memset(ymz280b->ROMUsage, 0x02, ROMSize);
		}

		ROM_BORDER_CHECK
		memcpy(ymz280b->ROMData + DataStart, ROMData, DataLength);
		memset(ymz280b->ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x87:	// YMF278B RAM
		ymf278 = &ChDat->YMF278B;

		if (ymf278->RAMSize != ROMSize)
		{
			ymf278->RAMData = (UINT8*)realloc(ymf278->RAMData, ROMSize);
			ymf278->RAMUsage = (UINT8*)realloc(ymf278->RAMUsage, ROMSize);
			ymf278->RAMSize = ROMSize;
			memset(ymf278->RAMData, 0xFF, ROMSize);
			memset(ymf278->RAMUsage, 0x02, ROMSize);
		}

		ROM_BORDER_CHECK
		memcpy(ymf278->RAMData + DataStart, ROMData, DataLength);
		memset(ymf278->RAMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x88:	// Y8950 DELTA-T ROM
		y8950 = &ChDat->Y8950;

		if (y8950->DT_ROMSize != ROMSize)
		{
			y8950->DT_ROMData = (UINT8*)realloc(y8950->DT_ROMData, ROMSize);
			y8950->DT_ROMUsage = (UINT8*)realloc(y8950->DT_ROMUsage, ROMSize);
			y8950->DT_ROMSize = ROMSize;
			memset(y8950->DT_ROMData, 0xFF, ROMSize);
			memset(y8950->DT_ROMUsage, 0x02, ROMSize);

			y8950->DeltaT.memory_size = y8950->DT_ROMSize;
			y8950->DeltaT.memory = y8950->DT_ROMData;
			y8950->DeltaT.memory_usg = y8950->DT_ROMUsage;
		}

		ROM_BORDER_CHECK
		memcpy(y8950->DT_ROMData + DataStart, ROMData, DataLength);
		memset(y8950->DT_ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x89:	// MultiPCM ROM
		multipcm = &ChDat->MultiPCM;

		if (multipcm->ROMSize != ROMSize)
		{
			multipcm->ROMData = (UINT8*)realloc(multipcm->ROMData, ROMSize);
			multipcm->ROMUsage = (UINT8*)realloc(multipcm->ROMUsage, ROMSize);
			multipcm->ROMSize = ROMSize;
			memset(multipcm->ROMData, 0xFF, ROMSize);
			memset(multipcm->ROMUsage, 0x02, ROMSize);
			memset(multipcm->SmplMask, 0x00, 0x200);
		}

		ROM_BORDER_CHECK
		memcpy(multipcm->ROMData + DataStart, ROMData, DataLength);
		memset(multipcm->ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x8A:	// uPD7759 ROM
		upd7759 = &ChDat->UPD7759;

		if (upd7759->ROMSize != ROMSize)
		{
			upd7759->ROMData = (UINT8*)realloc(upd7759->ROMData, ROMSize);
			upd7759->ROMUsage = (UINT8*)realloc(upd7759->ROMUsage, ROMSize);
			upd7759->ROMSize = ROMSize;
			memset(upd7759->ROMData, 0xFF, ROMSize);
			memset(upd7759->ROMUsage, 0x02, ROMSize);
		}

		ROM_BORDER_CHECK
		memcpy(upd7759->ROMData + DataStart, ROMData, DataLength);
		memset(upd7759->ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x8B:	// OKIM6295 ROM
		okim6295 = &ChDat->OKIM6295;

		if (okim6295->ROMSize != ROMSize)
		{
			okim6295->ROMData = (UINT8*)realloc(okim6295->ROMData, ROMSize);
			okim6295->ROMUsage = (UINT8*)realloc(okim6295->ROMUsage, ROMSize);
			okim6295->ROMSize = ROMSize;
			memset(okim6295->ROMData, 0xFF, ROMSize);
			memset(okim6295->ROMUsage, 0x02, ROMSize);
		}

		ROM_BORDER_CHECK
		memcpy(okim6295->ROMData + DataStart, ROMData, DataLength);
		memset(okim6295->ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x8C:	// K054539 ROM
		k054539 = &ChDat->K054539;

		if (k054539->ROMSize != ROMSize)
		{
			k054539->ROMData = (UINT8*)realloc(k054539->ROMData, ROMSize);
			k054539->ROMUsage = (UINT8*)realloc(k054539->ROMUsage, ROMSize);
			k054539->ROMSize = ROMSize;
			memset(k054539->ROMData, 0xFF, ROMSize);
			memset(k054539->ROMUsage, 0x02, ROMSize);
		}

		ROM_BORDER_CHECK
		memcpy(k054539->ROMData + DataStart, ROMData, DataLength);
		memset(k054539->ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x8D:	// C140 ROM
		c140 = &ChDat->C140;

		if (c140->ROMSize != ROMSize)
		{
			c140->ROMData = (UINT8*)realloc(c140->ROMData, ROMSize);
			c140->ROMUsage = (UINT8*)realloc(c140->ROMUsage, ROMSize);
			c140->ROMSize = ROMSize;
			memset(c140->ROMData, 0xFF, ROMSize);
			memset(c140->ROMUsage, 0x02, ROMSize);
		}

		ROM_BORDER_CHECK
		memcpy(c140->ROMData + DataStart, ROMData, DataLength);
		memset(c140->ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x8E:	// K053260 ROM
		k053260 = &ChDat->K053260;

		if (k053260->ROMSize != ROMSize)
		{
			k053260->ROMData = (UINT8*)realloc(k053260->ROMData, ROMSize);
			k053260->ROMUsage = (UINT8*)realloc(k053260->ROMUsage, ROMSize);
			k053260->ROMSize = ROMSize;
			memset(k053260->ROMData, 0xFF, ROMSize);
			memset(k053260->ROMUsage, 0x02, ROMSize);
		}

		ROM_BORDER_CHECK
		memcpy(k053260->ROMData + DataStart, ROMData, DataLength);
		memset(k053260->ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x8F:	// QSound ROM
		qsound = &ChDat->QSound;

		if (qsound->ROMSize != ROMSize)
		{
			qsound->ROMData = (UINT8*)realloc(qsound->ROMData, ROMSize);
			qsound->ROMUsage = (UINT8*)realloc(qsound->ROMUsage, ROMSize);
			qsound->ROMSize = ROMSize;
			memset(qsound->ROMData, 0xFF, ROMSize);
			memset(qsound->ROMUsage, 0x02, ROMSize);
		}

		ROM_BORDER_CHECK
		memcpy(qsound->ROMData + DataStart, ROMData, DataLength);
		memset(qsound->ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x90:	// ES5506 ROM
		{
			UINT8 curRgn;
			UINT8 is8Bit;	// 8-bit ROM mapped to 16-bit address space
			ES5506_ROM* esRgn;

			es5506 = &ChDat->ES5506;
			curRgn = (DataStart >> 28) & 0x03;
			is8Bit = (DataStart >> 31) & 0x01;
			DataStart &= 0x0FFFFFFF;
			if (is8Bit)
			{
				printf("Error! ES5506 using 8-Bit Sample ROM!\n");
				return;
			}

			esRgn = &es5506->Rgn[curRgn];
			if (esRgn->ROMSize != ROMSize)
			{
				esRgn->ROMData = (UINT8*)realloc(esRgn->ROMData, ROMSize);
				esRgn->ROMUsage = (UINT8*)realloc(esRgn->ROMUsage, ROMSize);
				esRgn->ROMSize = ROMSize;
				memset(esRgn->ROMData, 0xFF, ROMSize);
				memset(esRgn->ROMUsage, 0x02, ROMSize);
			}

			ROM_BORDER_CHECK
			memcpy(esRgn->ROMData + DataStart, ROMData, DataLength);
			memset(esRgn->ROMUsage + DataStart, 0x00, DataLength);
		}
		break;
	case 0x91:	// X1-010 ROM
		x1_010 = &ChDat->X1_010;

		if (x1_010->ROMSize != ROMSize)
		{
			x1_010->ROMData = (INT8*)realloc(x1_010->ROMData, ROMSize);
			x1_010->ROMUsage = (UINT8*)realloc(x1_010->ROMUsage, ROMSize);
			x1_010->ROMSize = ROMSize;
			memset(x1_010->ROMData, 0xFF, ROMSize);
			memset(x1_010->ROMUsage, 0x02, ROMSize);
		}
		memcpy(x1_010->ROMData + DataStart, ROMData, DataLength);
		memset(x1_010->ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x92:	// C352 ROM
		c352 = &ChDat->C352;

		if (c352->ROMSize != ROMSize)
		{
			c352->ROMData = (INT8*)realloc(c352->ROMData, ROMSize);
			c352->ROMUsage = (UINT8*)realloc(c352->ROMUsage, ROMSize);
			c352->ROMSize = ROMSize;
			memset(c352->ROMData, 0xFF, ROMSize);
			memset(c352->ROMUsage, 0x02, ROMSize);
		}

		memcpy(c352->ROMData + DataStart, ROMData, DataLength);
		memset(c352->ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x93:	// GA20 ROM
		ga20 = &ChDat->GA20;

		if (ga20->ROMSize != ROMSize)
		{
			ga20->ROMData = (UINT8*)realloc(ga20->ROMData, ROMSize);
			ga20->ROMUsage = (UINT8*)realloc(ga20->ROMUsage, ROMSize);
			ga20->ROMSize = ROMSize;
			memset(ga20->ROMData, 0xFF, ROMSize);
			memset(ga20->ROMUsage, 0x02, ROMSize);
		}

		memcpy(ga20->ROMData + DataStart, ROMData, DataLength);
		memset(ga20->ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x94:	// K007232 ROM
		k007232 = &ChDat->K007232;

		if (k007232->ROMSize != ROMSize)
		{
			k007232->ROMData = (UINT8*)realloc(k007232->ROMData, ROMSize);
			k007232->ROMUsage = (UINT8*)realloc(k007232->ROMUsage, ROMSize);
			k007232->ROMSize = ROMSize;
			memset(k007232->ROMData, 0xFF, ROMSize);
			memset(k007232->ROMUsage, 0x02, ROMSize);
		}

		ROM_BORDER_CHECK
		memcpy(k007232->ROMData + DataStart, ROMData, DataLength);
		memset(k007232->ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0xC0:	// RF5C68 RAM
	case 0xC1:	// RF5C164 RAM
		rf5c = (ROMType == 0xC0) ? &ChDat->RF5C68 : &ChDat->RF5C164;

		ROMSize = 0x10000;
		if (rf5c->RAMSize != ROMSize)
		{
			rf5c->RAMData = (UINT8*)realloc(rf5c->RAMData, ROMSize);
			rf5c->RAMUsage = (UINT8*)realloc(rf5c->RAMUsage, ROMSize);
			rf5c->RAMSize = ROMSize;
			memset(rf5c->RAMData, 0xFF, ROMSize);
			memset(rf5c->RAMUsage, 0x02, ROMSize);
		}

		DataStart += rf5c->SelBank;
		ROM_BORDER_CHECK
		memcpy(rf5c->RAMData + DataStart, ROMData, DataLength);
		memset(rf5c->RAMUsage + DataStart, 0x00, DataLength);
		break;
	case 0xC2:	// NES APU ROM Bank
		nes_apu = &ChDat->NES_APU;

		ROMSize = 0x10000;
		if (nes_apu->ROMSize != ROMSize)
		{
			nes_apu->ROMData = (UINT8*)realloc(nes_apu->ROMData, ROMSize);
			nes_apu->ROMUsage = (UINT8*)realloc(nes_apu->ROMUsage, ROMSize);
			nes_apu->ROMSize = ROMSize;
			memset(nes_apu->ROMData, 0xFF, ROMSize);
			memset(nes_apu->ROMUsage, 0x02, ROMSize);
		}

		ROM_BORDER_CHECK
		memcpy(nes_apu->ROMData + DataStart, ROMData, DataLength);
		memset(nes_apu->ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0xE1:	// ES5503 RAM
		es5503 = &ChDat->ES5503;

		ROMSize = 0x20000;
		if (es5503->ROMSize != ROMSize)
		{
			es5503->ROMData = (UINT8*)realloc(es5503->ROMData, ROMSize);
			es5503->ROMUsage = (UINT8*)realloc(es5503->ROMUsage, ROMSize);
			es5503->ROMSize = ROMSize;
			memset(es5503->ROMData, 0xFF, ROMSize);
			memset(es5503->ROMUsage, 0x02, ROMSize);
		}

		ROM_BORDER_CHECK
		memcpy(es5503->ROMData + DataStart, ROMData, DataLength);
		memset(es5503->ROMUsage + DataStart, 0x00, DataLength);
		break;
	}

	return;
}

UINT32 GetROMMask(UINT8 ROMType, UINT8** MaskData)
{
	SEGAPCM_DATA *spcm;
	YM2608_DATA* ym2608;
	YM2610_DATA* ym2610;
	Y8950_DATA* y8950;
	YMZ280B_DATA* ymz280b;
	RF5C_DATA* rf5c;
	NES_APU_DATA* nes_apu;
	YMF278B_DATA* ymf278;
	YMF271_DATA* ymf271;
	UPD7759_DATA* upd7759;
	OKIM6295_DATA* okim6295;
	MULTIPCM_DATA* multipcm;
	K054539_DATA* k054539;
	C140_DATA* c140;
	K053260_DATA* k053260;
	QSOUND_DATA* qsound;
	ES5503_DATA* es5503;
	ES5506_DATA* es5506;
	X1_010_DATA* x1_010;
	C352_DATA* c352;
	GA20_DATA* ga20;
	K007232_DATA* k007232;

	switch(ROMType)
	{
	case 0x80:	// SegaPCM ROM
		spcm = &ChDat->SegaPCM;

		*MaskData = spcm->ROMUsage;
		return spcm->ROMSize;
	case 0x81:	// YM2608 DELTA-T ROM
		ym2608 = &ChDat->YM2608;

		*MaskData = ym2608->DT_ROMUsage;
		return ym2608->DT_ROMSize;
	case 0x82:	// YM2610 ADPCM ROM
		ym2610 = &ChDat->YM2610;

		*MaskData = ym2610->AP_ROMUsage;
		return ym2610->AP_ROMSize;
	case 0x83:	// YM2610 DELTA-T ROM
		ym2610 = &ChDat->YM2610;

		*MaskData = ym2610->DT_ROMUsage;
		return ym2610->DT_ROMSize;
	case 0x84:	// YMF278B ROM
		ymf278 = &ChDat->YMF278B;

		*MaskData = ymf278->ROMUsage;
		return ymf278->ROMSize;
	case 0x85:	// YMF271 ROM
		ymf271 = &ChDat->YMF271;

		*MaskData = ymf271->ROMUsage;
		return ymf271->ROMSize;
	case 0x86:	// YMZ280B ROM
		ymz280b = &ChDat->YMZ280B;

		*MaskData = ymz280b->ROMUsage;
		return ymz280b->ROMSize;
	case 0x87:	// YMF278B RAM
		ymf278 = &ChDat->YMF278B;

		*MaskData = ymf278->RAMUsage;
		return ymf278->RAMSize;
	case 0x88:	// Y8950 DELTA-T ROM
		y8950 = &ChDat->Y8950;

		*MaskData = y8950->DT_ROMUsage;
		return y8950->DT_ROMSize;
	case 0x89:	// MultiPCM ROM
		multipcm = &ChDat->MultiPCM;

		*MaskData = multipcm->ROMUsage;
		return multipcm->ROMSize;
	case 0x8A:	// uPD7759 ROM
		upd7759 = &ChDat->UPD7759;

		*MaskData = upd7759->ROMUsage;
		return upd7759->ROMSize;
	case 0x8B:	// OKIM6295 ROM
		okim6295 = &ChDat->OKIM6295;

		*MaskData = okim6295->ROMUsage;
		return okim6295->ROMSize;
	case 0x8C:	// K054539 ROM
		k054539 = &ChDat->K054539;

		*MaskData = k054539->ROMUsage;
		return k054539->ROMSize;
	case 0x8D:	// C140 ROM
		c140 = &ChDat->C140;

		*MaskData = c140->ROMUsage;
		return c140->ROMSize;
	case 0x8E:	// K053260 ROM
		k053260 = &ChDat->K053260;

		*MaskData = k053260->ROMUsage;
		return k053260->ROMSize;
		break;
	case 0x8F:	// QSound ROM
		qsound = &ChDat->QSound;

		*MaskData = qsound->ROMUsage;
		return qsound->ROMSize;
	case 0x90:	// ES5506 ROM
		es5506 = &ChDat->ES5506;

		*MaskData = es5506->Rgn->ROMUsage;
		return es5506->Rgn->ROMSize;
	case 0x91:	// X1-010 ROM
		x1_010 = &ChDat->X1_010;

		*MaskData = x1_010->ROMUsage;
		return x1_010->ROMSize;
	case 0x92:	// C352 ROM
		c352 = &ChDat->C352;

		*MaskData = c352->ROMUsage;
		return c352->ROMSize;
	case 0x93:	// GA20 ROM
		ga20 = &ChDat->GA20;
		
		*MaskData = ga20->ROMUsage;
		return ga20->ROMSize;
	case 0x94:	// K007232 ROM
		k007232 = &ChDat->K007232;

		*MaskData = k007232->ROMUsage;
		return k007232->ROMSize;
	case 0xC0:	// RF5C68 RAM
		rf5c = &ChDat->RF5C68;

		*MaskData = rf5c->RAMUsage;
		return rf5c->RAMSize;
	case 0xC1:	// RF5C164 RAM
		rf5c = &ChDat->RF5C164;

		*MaskData = rf5c->RAMUsage;
		return rf5c->RAMSize;
	case 0xC2:	// NES APU ROM Bank
		nes_apu = &ChDat->NES_APU;

		*MaskData = nes_apu->ROMUsage;
		return nes_apu->ROMSize;
	case 0xE1:	// ES5503 RAM
		es5503 = &ChDat->ES5503;

		*MaskData = es5503->ROMUsage;
		return es5503->ROMSize;
	}

	return 0x00;
}

UINT32 GetROMData(UINT8 ROMType, UINT8** ROMData)
{
	SEGAPCM_DATA *spcm;
	YM2608_DATA* ym2608;
	YM2610_DATA* ym2610;
	Y8950_DATA* y8950;
	YMZ280B_DATA* ymz280b;
	RF5C_DATA* rf5c;
	NES_APU_DATA* nes_apu;
	YMF278B_DATA* ymf278;
	YMF271_DATA* ymf271;
	UPD7759_DATA* upd7759;
	OKIM6295_DATA* okim6295;
	MULTIPCM_DATA* multipcm;
	K054539_DATA* k054539;
	C140_DATA* c140;
	K053260_DATA* k053260;
	QSOUND_DATA* qsound;
	ES5503_DATA* es5503;
	ES5506_DATA* es5506;
	X1_010_DATA* x1_010;
	C352_DATA* c352;
	GA20_DATA* ga20;
	K007232_DATA* k007232;

	switch(ROMType)
	{
	case 0x80:	// SegaPCM ROM
		spcm = &ChDat->SegaPCM;

		*ROMData = spcm->ROMData;
		return spcm->ROMSize;
	case 0x81:	// YM2608 DELTA-T ROM
		ym2608 = &ChDat->YM2608;

		*ROMData = ym2608->DT_ROMData;
		return ym2608->DT_ROMSize;
	case 0x82:	// YM2610 ADPCM ROM
		ym2610 = &ChDat->YM2610;

		*ROMData = ym2610->AP_ROMData;
		return ym2610->AP_ROMSize;
	case 0x83:	// YM2610 DELTA-T ROM
		ym2610 = &ChDat->YM2610;

		*ROMData = ym2610->DT_ROMData;
		return ym2610->DT_ROMSize;
	case 0x84:	// YMF278B ROM
		ymf278 = &ChDat->YMF278B;

		*ROMData = ymf278->ROMData;
		return ymf278->ROMSize;
	case 0x85:	// YMF271 ROM
		ymf271 = &ChDat->YMF271;

		*ROMData = ymf271->ROMData;
		return ymf271->ROMSize;
	case 0x86:	// YMZ280B ROM
		ymz280b = &ChDat->YMZ280B;

		*ROMData = ymz280b->ROMData;
		return ymz280b->ROMSize;
	case 0x87:	// YMF278B RAM
		ymf278 = &ChDat->YMF278B;

		*ROMData = ymf278->RAMData;
		return ymf278->RAMSize;
	case 0x88:	// Y8950 DELTA-T ROM
		y8950 = &ChDat->Y8950;

		*ROMData = y8950->DT_ROMData;
		return y8950->DT_ROMSize;
	case 0x89:	// MultiPCM ROM
		multipcm = &ChDat->MultiPCM;

		*ROMData = multipcm->ROMData;
		return multipcm->ROMSize;
	case 0x8A:	// uPD7759 ROM
		upd7759 = &ChDat->UPD7759;

		*ROMData = upd7759->ROMData;
		return upd7759->ROMSize;
	case 0x8B:	// OKIM6295 ROM
		okim6295 = &ChDat->OKIM6295;

		*ROMData = okim6295->ROMData;
		return okim6295->ROMSize;
	case 0x8C:	// K054539 ROM
		k054539 = &ChDat->K054539;

		*ROMData = k054539->ROMData;
		return k054539->ROMSize;
	case 0x8D:	// C140 ROM
		c140 = &ChDat->C140;

		*ROMData = c140->ROMData;
		return c140->ROMSize;
	case 0x8E:	// K053260 ROM
		k053260 = &ChDat->K053260;

		*ROMData = k053260->ROMData;
		return k053260->ROMSize;
	case 0x8F:	// QSound ROM
		qsound = &ChDat->QSound;

		*ROMData = qsound->ROMData;
		return qsound->ROMSize;
	case 0x90:	// ES5506 ROM
		es5506 = &ChDat->ES5506;

		*ROMData = es5506->Rgn->ROMData;
		return es5506->Rgn->ROMSize;
	case 0x91:	// X1-010 ROM
		x1_010 = &ChDat->X1_010;

		*ROMData = (UINT8*)x1_010->ROMData;
		return x1_010->ROMSize;
	case 0x92:	// C352 ROM
		c352 = &ChDat->C352;

		*ROMData = (UINT8*)c352->ROMData;
		return c352->ROMSize;
	case 0x93:	// GA20 ROM
		ga20 = &ChDat->GA20;

		*ROMData = ga20->ROMData;
		return ga20->ROMSize;
	case 0x94:	// GA20 ROM
		k007232 = &ChDat->K007232;

		*ROMData = k007232->ROMData;
		return k007232->ROMSize;
	case 0xC0:	// RF5C68 RAM
		rf5c = &ChDat->RF5C68;

		*ROMData = rf5c->RAMData;
		return rf5c->RAMSize;
	case 0xC1:	// RF5C164 RAM
		rf5c = &ChDat->RF5C164;

		*ROMData = rf5c->RAMData;
		return rf5c->RAMSize;
	case 0xC2:	// NES APU ROM Bank
		nes_apu = &ChDat->NES_APU;

		*ROMData = nes_apu->ROMData;
		return nes_apu->ROMSize;
	case 0xE1:	// ES5503 RAM
		es5503 = &ChDat->ES5503;

		*ROMData = es5503->ROMData;
		return es5503->ROMSize;
	}

	return 0x00;
}
