// TODO: Check sample end < sample start

#include <malloc.h>
#include <memory.h>
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
	bool		bank_installed;
	UINT32		bank_offs;
	
	UINT32 ROMSize;
	UINT8* ROMData;
	UINT8* ROMUsage;
} OKIM6295_DATA;

enum
{
	C140_TYPE_SYSTEM2,
	C140_TYPE_SYSTEM21_A,
	C140_TYPE_SYSTEM21_B,
	C140_TYPE_ASIC219
};
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

#define QSOUND_CHANNELS 16
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

/*enum
{
	STATE_IDLE,
	STATE_DROP_DRQ,
	STATE_START,
	STATE_FIRST_REQ,
	STATE_LAST_SAMPLE,
	STATE_DUMMY1,
	STATE_ADDR_MSB,
	STATE_ADDR_LSB,
	STATE_DUMMY2,
	STATE_BLOCK_HEADER,
	STATE_NIBBLE_COUNT,
	STATE_NIBBLE_MSN,
	STATE_NIBBLE_LSN
};*/
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

typedef struct all_chips
{
	SEGAPCM_DATA SegaPCM;
	YM2608_DATA YM2608;
	YM2610_DATA YM2610;
	Y8950_DATA Y8950;
	YMZ280B_DATA YMZ280B;
	RF5C_DATA RF5C68;
	RF5C_DATA RF5C164;
	YMF271_DATA YMF271;
	UPD7759_DATA UPD7759;
	OKIM6295_DATA OKIM6295;
	K054539_DATA K054539;
	C140_DATA C140;
	K053260_DATA K053260;
	QSOUND_DATA QSound;
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
void okim6295_write(UINT8 Offset, UINT8 Data);
static void k054539_proc_channel(K054539_DATA* chip, UINT8 Chn);
void k054539_write(UINT8 Port, UINT8 Offset, UINT8 Data);
void c140_write(UINT8 Port, UINT8 Offset, UINT8 Data);
void k053260_write(UINT8 Register, UINT8 Data);
void qsound_write(UINT8 Offset, UINT16 Value);
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
		
		//ChDat->UPD7759.state = STATE_IDLE;
		ChDat->UPD7759.reset = 1;
		ChDat->UPD7759.start = 1;
		ChDat->OKIM6295.command = 0xFF;
		ChDat->K054539.flags = K054539_RESET_FLAGS;
		ChDat->C140.banking_type = 0x00;
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

					if(adpcm[c].start >= F2610->AP_ROMSize)	// Check Start in Range
					{
						printf("YM2610: ADPCM-A start out of range: $%08x\n", adpcm[c].start);
						adpcm[c].flag = 0;
					}
					if(adpcm[c].end > F2610->AP_ROMSize)	// Check End in Range
					{
						printf("YM2610 Ch %hu: ADPCM-A end out of range\n", c);
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
				printf("ADPCM-A Ch %hu StartAddr: %06lX\n", c, adpcm[c].start);
			}
			break;
		case 0x20:	// Sample End Address
		case 0x28:
			adpcm[c].end    = ((F2610->ADPCMReg[0x28 + c] * 0x0100 |
								F2610->ADPCMReg[0x20 + c]) << ADPCMA_ADDRESS_SHIFT);
			adpcm[c].end   += (1<<ADPCMA_ADDRESS_SHIFT) - 1;
			//adpcm[c].end   += (2<<ADPCMA_ADDRESS_SHIFT) - 1; // Puzzle De Pon-Patch
			
			adpcm[c].end &= (1 << 20) - 1;	// YM2610 checks only 20 bits while playing
			adpcm[c].end |= adpcm[c].start & ~((1 << 20) - 1);
			adpcm[c].end ++;
			
			if (adpcm[c].flag)
			{
				printf("ADPCM-A Ch %hu EndAddr: %06lX\n", c, adpcm[c].end);
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
				printf("Warning: Invalid Sample Length: %06X (%06X . %06X)\n", SampleLen, DELTAT->start, DELTAT->end);
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
				if (voice->stop >= chip->ROMSize)	// Check End in Range
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

static void rf5c_write(RF5C_DATA* chip, UINT8 Register, UINT8 Data)
{
	RF5C_CHANNEL* chan;
	UINT8 CurChn;
	UINT32 Addr;
	
	switch(Register)
	{
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
			Addr = chan->start;
			while(Addr < chip->RAMSize)
			{
				chip->RAMUsage[Addr] |= 0x01;
				if (chip->RAMData[Addr] == 0xFF)
					break;
				Addr ++;
			}
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
				Addr = chan->start;
				while(Addr < chip->RAMSize && ! (chip->RAMUsage[Addr] & 0x01))
				{
					chip->RAMUsage[Addr] |= 0x01;
					if (chip->RAMData[Addr] == 0xFF)
						break;
					Addr ++;
				}
				while((Addr & 0x0F) >= 0x0C)
				{
					// I do some small rounding
					chip->RAMUsage[Addr] |= 0x01;
					Addr ++;
				}
				
				// the Loop Start can be at a completely other place,
				// like Popful Mail showed me
				Addr = chan->loopst;
				while(Addr < chip->RAMSize && ! (chip->RAMUsage[Addr] & 0x01))
				{
					chip->RAMUsage[Addr] |= 0x01;
					if (chip->RAMData[Addr] == 0xFF)
						break;
					Addr ++;
				}
				while((Addr & 0x0F) >= 0x0C)
				{
					// I do some small rounding
					chip->RAMUsage[Addr] |= 0x01;
					Addr ++;
				}
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
	case 0:
		if (Data & 1)
		{
			// key on
			slot->active = 1;
			if (! slot->IsPCM)
				break;
			if (slot->waveform != 0x07)
				break;
			if (SlotNum & 0x03)
			{
				SlotNum &= ~0x03;
				slot = &chip->slots[SlotNum];
			}
			
			DataLen = slot->endaddr + 1;
			Addr = slot->startaddr;
			for (CurByt = 0x00; CurByt < DataLen; CurByt ++, Addr ++)
				chip->ROMUsage[Addr] |= 0x01;
		}
		else if (slot->active)
		{
			slot->active = 0;
		}
		break;
	case 11:
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

	if (SyncMode && SyncReg)		// key-on slot & synced register
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
	else		// write register normally
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
			slot->startaddr |= Data << 16;
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
			slot->endaddr |= Data << 16;
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
			slot->loopaddr |= Data << 16;
			break;
		case 9:
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
			base = chip->bank_offs | chip->command * 8;
			
			voice->start  = chip->ROMData[base + 0] << 16;
			voice->start |= chip->ROMData[base + 1] <<  8;
			voice->start |= chip->ROMData[base + 2] <<  0;
			voice->start &= 0x3FFFF;
			
			voice->stop  = chip->ROMData[base + 3] << 16;
			voice->stop |= chip->ROMData[base + 4] <<  8;
			voice->stop |= chip->ROMData[base + 5] <<  0;
			voice->stop &= 0x3FFFF;
			
			SampleLen = 0x80 * 8;	// header memory
			MaskBase = &chip->ROMUsage[chip->bank_offs | 0x00];
			for (CurSmpl = 0x00; CurSmpl < SampleLen; CurSmpl ++)
				MaskBase[CurSmpl] |= 0x01;
			
			// set up the voice to play this sample
			if (voice->start < voice->stop)
			{
				voice->playing = true;
				
				SampleLen = voice->stop - voice->start + 1;
				MaskBase = &chip->ROMUsage[chip->bank_offs | voice->start];
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
	
	switch(Offset)
	{
	case 0x00:
		okim6295_command_write(chip, Data);
		break;
	case 0x0F:
		chip->bank_offs = Data << 18;
		break;
	}
	
	return;
}

static UINT32 c140_sample_addr(C140_DATA* chip, UINT8 adr_msb, UINT8 adr_lsb,
							   UINT8 bank, UINT8 voice)
{
	UINT32 TempAddr;
	UINT32 NewAddr;
	const INT16 asic219banks[4] = {0x07, 0x01, 0x03, 0x05};
	UINT8 BnkReg;
	
	TempAddr = (bank << 16) | (adr_msb << 8) | (adr_lsb << 0);
	switch(chip->banking_type)
	{
	case C140_TYPE_SYSTEM2:
		NewAddr = ((TempAddr & 0x200000) >> 2) | (TempAddr & 0x7FFFF);
		break;
	case C140_TYPE_SYSTEM21_A:
		NewAddr = ((TempAddr & 0x300000) >> 1) | (TempAddr & 0x7FFFF);
		break;
	case C140_TYPE_SYSTEM21_B:
		NewAddr = ((TempAddr & 0x100000) >> 2) | (TempAddr & 0x3FFFF);
		
		if (TempAddr & 0x40000)
			NewAddr |= 0x80000;
		if (TempAddr & 0x200000)
			NewAddr |= 0x100000;
		break;
	case C140_TYPE_ASIC219:
		// on the 219 asic, addresses are in words
		TempAddr = (bank << 16) | (adr_msb << 9) | (adr_lsb << 1);
		
		BnkReg = ((voice >> 2) + 0x07) & 0x07;
		NewAddr = ((chip->BankRegs[BnkReg] & 0x03) * 0x20000) | TempAddr;
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
		if (chip->banking_type == C140_TYPE_ASIC219)
			RegVal &= 0x1F7;
		
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
	else
	{
		return;
	}
	
	TempChn = &chip->channel[ch];
	switch(reg)
	{
	case 0: // Bank
		ch = (ch + 1) & 0x0F;
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
		else if (TempChn->key == 0)
		{
			// Key on
			TempChn->key = 1;
			
			StAddr = (TempChn->bank << 16) + TempChn->address;
			EndAddr = (TempChn->bank << 16) + TempChn->end + 1;
			StAddr %= chip->ROMSize;
			EndAddr %= chip->ROMSize;
			for (Addr = StAddr; Addr < EndAddr; Addr ++)
				chip->ROMUsage[Addr] |= 0x01;
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
	
	TempChn = &chip->channels[Chn];
	Addr =	(TempChn->pos_reg[2] << 16) |
			(TempChn->pos_reg[1] <<  8) |
			(TempChn->pos_reg[0] <<  0);
	
	switch(TempChn->mode_reg)
	{
	case 0x00:	// 8-bit PCM
		while(Addr < chip->ROMSize)
		{
			chip->ROMUsage[Addr] |= 0x01;
			if (chip->ROMData[Addr] == 0x80)
				break;
			
			Addr ++;
		}
		break;
	case 0x01:	// 16-bit PCM (LSB first)
		while(Addr < chip->ROMSize)
		{
			chip->ROMUsage[Addr + 0x00] |= 0x01;
			chip->ROMUsage[Addr + 0x01] |= 0x01;
			TempSht =	(chip->ROMData[Addr + 0x00] << 0) |
						(chip->ROMData[Addr + 0x01] << 8);
			if (TempSht == 0x8000)
				break;
			
			Addr += 0x02;
		}
		break;
	case 0x02:	// 4-bit DPCM
		while(Addr < chip->ROMSize)
		{
			chip->ROMUsage[Addr] |= 0x01;
			if (chip->ROMData[Addr] == 0x88)
				break;
			
			Addr ++;
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
			TempChn->mode_reg = (Data & 0x0C) >> 2;
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
						if (Addr + DataLen >= ic->ROMSize)
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


void write_rom_data(UINT8 ROMType, UINT32 ROMSize, UINT32 DataStart, UINT32 DataLength,
					const UINT8* ROMData)
{
	SEGAPCM_DATA *spcm;
	YM2608_DATA* ym2608;
	YM2610_DATA* ym2610;
	Y8950_DATA* y8950;
	YMZ280B_DATA* ymz280b;
	RF5C_DATA* rf5c;
	YMF271_DATA* ymf271;
	UPD7759_DATA* upd7759;
	OKIM6295_DATA* okim6295;
	K054539_DATA* k054539;
	C140_DATA* c140;
	K053260_DATA* k053260;
	QSOUND_DATA* qsound;
	
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
		
		memcpy(ym2610->DT_ROMData + DataStart, ROMData, DataLength);
		memset(ym2610->DT_ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0x84:	// YMF278B ROM
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
		
		memcpy(ymz280b->ROMData + DataStart, ROMData, DataLength);
		memset(ymz280b->ROMUsage + DataStart, 0x00, DataLength);
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
		
		memcpy(y8950->DT_ROMData + DataStart, ROMData, DataLength);
		memset(y8950->DT_ROMUsage + DataStart, 0x00, DataLength);
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
		
		memcpy(qsound->ROMData + DataStart, ROMData, DataLength);
		memset(qsound->ROMUsage + DataStart, 0x00, DataLength);
		break;
	case 0xC0:	// RF5C68 RAM
		rf5c = &ChDat->RF5C68;
		
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
		memcpy(rf5c->RAMData + DataStart, ROMData, DataLength);
		memset(rf5c->RAMUsage + DataStart, 0x00, DataLength);
		break;
	case 0xC1:	// RF5C164 RAM
		rf5c = &ChDat->RF5C164;
		
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
		memcpy(rf5c->RAMData + DataStart, ROMData, DataLength);
		memset(rf5c->RAMUsage + DataStart, 0x00, DataLength);
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
	YMF271_DATA* ymf271;
	UPD7759_DATA* upd7759;
	OKIM6295_DATA* okim6295;
	K054539_DATA* k054539;
	C140_DATA* c140;
	K053260_DATA* k053260;
	QSOUND_DATA* qsound;
	
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
		break;
	case 0x85:	// YMF271 ROM
		ymf271 = &ChDat->YMF271;
		
		*MaskData = ymf271->ROMUsage;
		return ymf271->ROMSize;
	case 0x86:	// YMZ280B ROM
		ymz280b = &ChDat->YMZ280B;
		
		*MaskData = ymz280b->ROMUsage;
		return ymz280b->ROMSize;
	case 0x88:	// Y8950 DELTA-T ROM
		y8950 = &ChDat->Y8950;
		
		*MaskData = y8950->DT_ROMUsage;
		return y8950->DT_ROMSize;
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
	case 0xC0:	// RF5C68 RAM
		rf5c = &ChDat->RF5C68;
		
		*MaskData = rf5c->RAMUsage;
		return rf5c->RAMSize;
	case 0xC1:	// RF5C164 RAM
		rf5c = &ChDat->RF5C164;
		
		*MaskData = rf5c->RAMUsage;
		return rf5c->RAMSize;
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
	YMF271_DATA* ymf271;
	UPD7759_DATA* upd7759;
	OKIM6295_DATA* okim6295;
	K054539_DATA* k054539;
	C140_DATA* c140;
	K053260_DATA* k053260;
	QSOUND_DATA* qsound;
	
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
		break;
	case 0x85:	// YMF271 ROM
		ymf271 = &ChDat->YMF271;
		
		*ROMData = ymf271->ROMData;
		return ymf271->ROMSize;
	case 0x86:	// YMZ280B ROM
		ymz280b = &ChDat->YMZ280B;
		
		*ROMData = ymz280b->ROMData;
		return ymz280b->ROMSize;
	case 0x88:	// Y8950 DELTA-T ROM
		y8950 = &ChDat->Y8950;
		
		*ROMData = y8950->DT_ROMData;
		return y8950->DT_ROMSize;
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
	case 0xC0:	// RF5C68 RAM
		rf5c = &ChDat->RF5C68;
		
		*ROMData = rf5c->RAMData;
		return rf5c->RAMSize;
	case 0xC1:	// RF5C164 RAM
		rf5c = &ChDat->RF5C164;
		
		*ROMData = rf5c->RAMData;
		return rf5c->RAMSize;
	}
	
	return 0x00;
}
