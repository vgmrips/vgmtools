#include <malloc.h>
#include <memory.h>
#include <stdio.h>
#include "stdbool.h"

#include "stdtype.h"
#include "chip_strp.h"


typedef struct sn76496_data
{
	UINT8 LastChn;
} SN76496_DATA;
typedef struct ym2413_data
{
	UINT8 RegData[0x100];
	UINT8 RegFirst[0x100];
} YM2413_DATA;
typedef struct ym2612_data
{
	UINT8 RegData[0x200];
	UINT8 RegFirst[0x200];
	UINT8 KeyOn[0x08];
	UINT8 KeyFirst[0x08];
} YM2612_DATA;
typedef struct ym2151_data
{
	UINT8 RegData[0x100];
	UINT8 RegFirst[0x100];
	UINT8 MCMask[0x08];
	UINT8 MCFirst[0x08];
} YM2151_DATA;
typedef struct segapcm_data
{
	UINT8* ROMData;
	UINT8* ROMUsage;
	UINT8 RAMData[0x800];
	UINT8 RAMFirst[0x800];
} SEGAPCM_DATA;
typedef struct _rf5c68_channel
{
	UINT8 ChnReg[0x07];
	UINT8 RegFirst[0x07];
} RF5C68_CHANNEL;
typedef struct rf5c68_data
{
	RF5C68_CHANNEL chan[8];
	UINT8 cbank;
	UINT8 wbank;
	UINT16 OffsetBase;
	UINT8 CtrlReg;
	UINT8 ChnMask;
	UINT8 RAMData[0x10000];
	UINT8 RAMFirst[0x10000];
} RF5C68_DATA;
typedef struct ym2203_data
{
	UINT8 RegData[0x100];
	UINT8 RegFirst[0x100];
	UINT8 KeyOn[0x04];
	UINT8 KeyFirst[0x04];
	UINT8 PreSclCmd;
} YM2203_DATA;
typedef struct ym2608_data
{
	UINT8 RegData[0x200];
	UINT8 RegFirst[0x200];
	UINT8 KeyOn[0x08];
	UINT8 KeyFirst[0x08];
	UINT8 PreSclCmd;
} YM2608_DATA;
typedef struct ym2610_data
{
	UINT8 RegData[0x200];
	UINT8 RegFirst[0x200];
	UINT8 KeyOn[0x08];
	UINT8 KeyFirst[0x08];
	UINT8 PreSclCmd;
} YM2610_DATA;
typedef struct ym3812_data
{
	UINT8 RegData[0x100];
	UINT8 RegFirst[0x100];
} YM3812_DATA;
typedef struct ym3526_data
{
	UINT8 RegData[0x100];
	UINT8 RegFirst[0x100];
} YM3526_DATA;
typedef struct y8950_data
{
	UINT8 RegData[0x100];
	UINT8 RegFirst[0x100];
} Y8950_DATA;
typedef struct ymf262_data
{
	UINT8 RegData[0x200];
	UINT8 RegFirst[0x200];
} YMF262_DATA;
typedef struct ymf271_data
{
	UINT8 RegData[0x100];
	UINT8 RegFirst[0x100];
} YMF271_DATA;
typedef struct ymf278b_data
{
	UINT8 RegData[0x100];
	UINT8 RegFirst[0x100];
} YMF278B_DATA;
typedef struct ymz280b_data
{
	UINT8 RegData[0x100];
	UINT8 RegFirst[0x100];
} YMZ280B_DATA;
typedef struct c140_data
{
	UINT8 BankType;
	UINT8 RegData[0x200];
	UINT8 RegFirst[0x200];
} C140_DATA;

typedef struct all_chips
{
	UINT8 GGSt;
	SN76496_DATA SN76496;
	YM2413_DATA YM2413;
	YM2612_DATA YM2612;
	YM2151_DATA YM2151;
	SEGAPCM_DATA SegaPCM;
	RF5C68_DATA RF5C68;
	YM2203_DATA YM2203;
	YM2608_DATA YM2608;
	YM2610_DATA YM2610;
	YM3812_DATA YM3812;
	YM3526_DATA YM3526;
	Y8950_DATA Y8950;
	YMF262_DATA YMF262;
	YMF271_DATA YMF271;
	YMF278B_DATA YMF278B;
	YMZ280B_DATA YMZ280B;
	C140_DATA C140;
} ALL_CHIPS;


UINT8 ChipCount = 0x02;
ALL_CHIPS* ChipData = NULL;
ALL_CHIPS* ChDat;
extern STRIP_DATA StripVGM;

void InitAllChips(void)
{
	UINT8 CurChip;
	
	if (ChipData == NULL)
		ChipData = (ALL_CHIPS*)malloc(ChipCount * sizeof(ALL_CHIPS));
	for (CurChip = 0x00; CurChip < ChipCount; CurChip ++)
	{
		memset(ChipData, 0xFF, ChipCount * sizeof(ALL_CHIPS));
		
		ChipData[CurChip].GGSt = 0x00;
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

bool GGStereo(UINT8 Data)
{
	STRIP_PSG* strip = &StripVGM.SN76496;
	
	if (strip->All || (strip->Other & 0x01))
		return false;
	
	return true;
}

bool sn76496_write(UINT8 Command)
{
	SN76496_DATA* chip = &ChDat->SN76496;
	STRIP_PSG* strip = &StripVGM.SN76496;
	UINT8 Channel;
	bool RetVal;
	
	if (strip->All)
		return false;
	
	RetVal = true;
	if (Command & 0x80)
	{
		Channel = (Command & 0x60) >> 1;
		chip->LastChn = Channel;
	}
	else
	{
		Channel = chip->LastChn;
	}
	
	if (strip->ChnMask & (0x01 << Channel))
		return false;
	return true;
}

bool ym2413_write(UINT8 Register, UINT8 Data)
{
	STRIP_OPL* strip = &StripVGM.YM2413;
	
	if (strip->All)
		return false;
	return true;
	ChDat->YM2413.RegFirst[Register] = 0x00;
	ChDat->YM2413.RegData[Register] = Data;
	return true;
}

bool ym2612_write(UINT8 Port, UINT8 Register, UINT8 Data)
{
	YM2612_DATA* chip;
	STRIP_OPN* strip = &StripVGM.YM2612;
	UINT16 RegVal;
	UINT8 Channel;
	
	if (strip->All)
		return false;
	return true;
	chip = &ChDat->YM2612;
	RegVal = (Port << 8) | Register;
	switch(RegVal)
	{
	case 0x24:	// Timer Registers
	case 0x25:
	case 0x26:
		return false;
	// no OPN Prescaler Registers for YM2612
	case 0x28:
		Channel = Data & 0x07;
		
		if (! chip->KeyFirst[Channel] && Data == chip->KeyOn[Channel])
			return false;
		
		chip->KeyFirst[Channel] = 0x00;
		chip->KeyOn[Channel] = Data;
		break;
	case 0x2A:
		return true;	// I leave this on for later optimizations
	case 0x27:
		Data &= 0xC0;	// mask out all timer-relevant bits
		// fall through
	default:
		// no SSG emulator for YM2612
		switch(RegVal & 0xF4)
		{
		case 0xA0:	// A0-A3 and A8-AB
			return true;	// Rewrite is neccessary: Phase Increment is recalculated
		case 0xA4:	// A4-A7 and AC-AF - Frequence Latch
			return true;
			// For some reason (I really can't say why) the code below doesn't work properly
			/*if ((RegVal & 0x03) == 0x03)
				break;
			if ((RegVal & 0x100) && (RegVal & 0x08))
				break;
			RegVal &= 0x0FC;	// Registers A4-A6 (AC-AE for Ch3) write to the same offset
			break;*/
		}
		
		if (! chip->RegFirst[RegVal] && Data == chip->RegData[RegVal])
			return false;
		
		chip->RegFirst[RegVal] = 0x00;
		chip->RegData[RegVal] = Data;
		break;
	}
	return true;
}

bool ym2151_write(UINT8 Register, UINT8 Data)
{
	YM2151_DATA* chip;
	STRIP_OPM* strip = &StripVGM.YM2151;
	UINT8 Channel;
	
	if (strip->All)
		return false;
	return true;
	chip = &ChDat->YM2151;
	switch(Register)
	{
	case 0x08:
		Channel = Data & 0x07;
		if (! chip->MCFirst[Channel] && (Data & 0xF8) == chip->MCMask[Channel])
			return false;
		
		chip->MCFirst[Channel] = 0x00;
		chip->MCMask[Channel] = Data & 0xF8;
		break;
	default:
		if (! chip->RegFirst[Register] && Data == chip->RegData[Register])
			return false;
		
		chip->RegFirst[Register] = 0x00;
		chip->RegData[Register] = Data;
		break;
	}
	return true;
}

bool segapcm_mem_write(UINT16 Offset, UINT8 Data)
{
	STRIP_PCM* strip = &StripVGM.SegaPCM;
	
	if (strip->All)
		return false;
	return true;
	ChDat->SegaPCM.RAMFirst[Offset] = 0x00;
	ChDat->SegaPCM.RAMData[Offset] = Data;
	return true;
}

bool rf5c68_reg_write(UINT8 Register, UINT8 Data)
{
	// RF5C68 Chips are usually not spammed with Register Writes
	RF5C68_DATA* chip = &ChDat->RF5C68;
	RF5C68_CHANNEL* chan = &chip->chan[chip->cbank];
	STRIP_PCM* strip = &StripVGM.RF5C68;
	
	if (strip->All)
		return false;
	
	switch(Register)
	{
	case 0x00:	// Envelope
	case 0x01:	// Pan
	case 0x02:	// FD Low / step
	case 0x03:	// FD High / step
	case 0x04:	// Loop Start Low
	case 0x05:	// Loop Start High
		//if (Data == chan->ChnReg[Register])
		//	return false;
		//else
			chan->ChnReg[Register] = Data;
		break;
	case 0x06:	// Start
		// Address change - must be set
		chan->ChnReg[Register] = Data;
		//chan->addr = chan->start << (8 + 11);
		break;
	case 0x07:	// Control Register
		//if (Data == chip->CtrlReg)
		//	return false;
		//else
			chip->CtrlReg = Data;
		
		if (Data & 0x40)
		{
			chip->cbank = Data & 0x07;
		}
		else
		{
			chip->wbank = Data & 0x0F;
			chip->OffsetBase = chip->wbank << 12;
		}
		break;
	case 0x08:	// Channel On/Off Register
		chip->ChnMask = Data;
		
		// Address change
		/*for (i = 0; i < 8; i++)
		{
			chip->chan[i].enable = (~data >> i) & 1;
			if (!chip->chan[i].enable)
				chip->chan[i].addr = chip->chan[i].start << (8 + 11);
		}*/
		break;
	}
	
	return true;
}

bool rf5c68_mem_write(UINT16 Offset, UINT8 Data)
{
	//RF5C68_DATA* chip = &ChDat->RF5C68;
	STRIP_PCM* strip = &StripVGM.RF5C68;
	
	if (strip->All || (strip->Other & 0x01))
		return false;
	
	return true;
}
bool ym2203_write(UINT8 Register, UINT8 Data)
{
	YM2203_DATA* chip;
	STRIP_OPN* strip = &StripVGM.YM2203;
	UINT8 Channel;
	
	if (strip->All)
		return false;
	return true;
	chip = &ChDat->YM2203;
	switch(Register)
	{
	case 0x24:	// Timer Registers
	case 0x25:
	case 0x26:
		return false;
	case 0x2D:	// OPN Prescaler Registers
	case 0x2E:
	case 0x2F:
		if (chip->PreSclCmd == Register)
			return false;
		chip->PreSclCmd = Register;
		break;
	case 0x28:
		Channel = Data & 0x03;
		
		if (! chip->KeyFirst[Channel] && Data == chip->KeyOn[Channel])
			return false;
		
		chip->KeyFirst[Channel] = 0x00;
		chip->KeyOn[Channel] = Data;
		break;
	case 0x27:
		Data &= 0xC0;	// mask out all timer-relevant bits
		// fall through
	default:
		if ((Register & 0xF0) < 0x10)
			return true;	// I don't know anything about the SSG emulator
		switch(Register & 0xF4)
		{
		case 0xA0:	// A0-A3 and A8-AB
			return true;	// Rewrite is neccessary: Phase Increment is recalculated
		case 0xA4:	// A4-A7 and AC-AF - Frequence Latch
			return true;
			// For some reason (I really can't say why) the code below doesn't work properly
			/*if ((RegVal & 0x03) == 0x03)
				break;
			RegVal &= 0x0FC;	// Registers A4-A6 (AC-AE for Ch3) write to the same offset
			break;*/
		}
		
		if (! chip->RegFirst[Register] && Data == chip->RegData[Register])
			return false;
		
		chip->RegFirst[Register] = 0x00;
		chip->RegData[Register] = Data;
		break;
	}
	return true;
}

bool ym2608_write(UINT8 Port, UINT8 Register, UINT8 Data)
{
	YM2608_DATA* chip;
	STRIP_OPN* strip = &StripVGM.YM2608;
	UINT16 RegVal;
	UINT8 Channel;
	
	if (strip->All)
		return false;
	return true;
	chip = &ChDat->YM2608;
	RegVal = (Port << 8) | Register;
	switch(RegVal)
	{
	case 0x24:	// Timer Registers
	case 0x25:
	case 0x26:
		return false;
	case 0x2D:	// OPN Prescaler Registers
	case 0x2E:
	case 0x2F:
		if (chip->PreSclCmd == Register)
			return false;
		break;
	case 0x28:
		Channel = Data & 0x07;
		
		if (! chip->KeyFirst[Channel] && Data == chip->KeyOn[Channel])
			return false;
		
		chip->KeyFirst[Channel] = 0x00;
		chip->KeyOn[Channel] = Data;
		break;
	case 0x27:
		Data &= 0xC0;	// mask out all timer-relevant bits
		// fall through
	default:
		if ((RegVal & 0x1F0) < 0x10)
			return true;	// I don't know anything about the SSG emulator
		if ((RegVal & 0x1F0) >= 0x100 && (RegVal & 0x1F0) < 0x110)	// DELTA-T
		{
			if ((RegVal & 0x0F) != 0x0E)	// DAC Data is handled like DAC of YM2612
				return true;	// I assume that these registers are not spammed
		}
		switch(RegVal & 0xF4)
		{
		case 0xA0:	// A0-A3 and A8-AB
			return true;	// Rewrite is neccessary: Phase Increment is recalculated
		case 0xA4:	// A4-A7 and AC-AF - Frequence Latch
			return true;
			// For some reason (I really can't say why) the code below doesn't work properly
			/*if ((RegVal & 0x03) == 0x03)
				break;
			RegVal &= 0x0FC;	// Registers A4-A6 (AC-AE for Ch3) write to the same offset
			break;*/
		}
		
		if (! chip->RegFirst[RegVal] && Data == chip->RegData[RegVal])
			return false;
		
		chip->RegFirst[RegVal] = 0x00;
		chip->RegData[RegVal] = Data;
		break;
	}
	return true;
}

bool ym2610_write(UINT8 Port, UINT8 Register, UINT8 Data)
{
	YM2610_DATA* chip;
	STRIP_OPN* strip = &StripVGM.YM2610;
	UINT16 RegVal;
	UINT8 Channel;
	
	if (strip->All)
		return false;
	return true;
	chip = &ChDat->YM2610;
	RegVal = (Port << 8) | Register;
	switch(RegVal)
	{
	case 0x24:	// Timer Registers
	case 0x25:
	case 0x26:
		return false;
	// no OPN Prescaler Registers for YM2610
	case 0x28:
		Channel = Data & 0x07;
		
		if (! chip->KeyFirst[Channel] && Data == chip->KeyOn[Channel])
			return false;
		
		chip->KeyFirst[Channel] = 0x00;
		chip->KeyOn[Channel] = Data;
		break;
	case 0x27:
		Data &= 0xC0;	// mask out all timer-relevant bits
		// fall through
	default:
		if (RegVal == 0x1C)
			return false;	// controls only Status Bits
		if ((RegVal & 0x1F0) < 0x10)
			return true;	// I don't know anything about the SSG emulator
		if ((RegVal & 0x1F0) >= 0x010 && (RegVal & 0x1F0) < 0x020)	// DELTA-T
			return true;	// I assume that these registers are not spammed
		if ((RegVal & 0x1F0) >= 0x100 && (RegVal & 0x1F0) < 0x130)	// ADPCM
			return true;	// I assume that these registers are not spammed
		switch(RegVal & 0xF4)
		{
		case 0xA0:	// A0-A3 and A8-AB
			return true;	// Rewrite is neccessary: Phase Increment is recalculated
		case 0xA4:	// A4-A7 and AC-AF - Frequence Latch
			return true;
			// For some reason (I really can't say why) the code below doesn't work properly
			/*if ((RegVal & 0x03) == 0x03)
				break;
			RegVal &= 0x0FC;	// Registers A4-A6 (AC-AE for Ch3) write to the same offset
			break;*/
		}
		
		if (! chip->RegFirst[RegVal] && Data == chip->RegData[RegVal])
			return false;
		
		chip->RegFirst[RegVal] = 0x00;
		chip->RegData[RegVal] = Data;
		break;
	}
	return true;
}

bool ym3812_write(UINT8 Register, UINT8 Data)
{
	STRIP_OPL* strip = &StripVGM.YM3812;
	
	if (strip->All)
		return false;
	return true;
	ChDat->YM3812.RegFirst[Register] = 0x00;
	ChDat->YM3812.RegData[Register] = Data;
	return true;
}

bool ym3526_write(UINT8 Register, UINT8 Data)
{
	STRIP_OPL* strip = &StripVGM.YM3526;
	
	if (strip->All)
		return false;
	return true;
	ChDat->YM3526.RegFirst[Register] = 0x00;
	ChDat->YM3526.RegData[Register] = Data;
	return true;
}

bool y8950_write(UINT8 Register, UINT8 Data)
{
	STRIP_OPL* strip = &StripVGM.Y8950;
	
	if (strip->All)
		return false;
	return true;
	ChDat->Y8950.RegFirst[Register] = 0x00;
	ChDat->Y8950.RegData[Register] = Data;
	return true;
}

bool ymf262_write(UINT8 Port, UINT8 Register, UINT8 Data)
{
	STRIP_OPL* strip = &StripVGM.YMF262;
	UINT16 RegVal;
	
	if (strip->All)
		return false;
	
	RegVal = (Port << 8) | RegVal;
	return true;
	ChDat->YMF262.RegFirst[RegVal] = 0x00;
	ChDat->YMF262.RegData[RegVal] = Data;
	return true;
}

bool ymz280b_write(UINT8 Register, UINT8 Data)
{
	STRIP_YMZ* strip = &StripVGM.YMZ280B;
	
	if (strip->All)
		return false;
	return true;
	ChDat->YMZ280B.RegFirst[Register] = 0x00;
	ChDat->YMZ280B.RegData[Register] = Data;
	return true;
}

bool rf5c164_reg_write(UINT8 Register, UINT8 Data)
{
	RF5C68_DATA* chip = &ChDat->RF5C68;
	RF5C68_CHANNEL* chan = &chip->chan[chip->cbank];
	STRIP_PCM* strip = &StripVGM.RF5C164;
	
	if (strip->All)
		return false;
	
	switch(Register)
	{
	case 0x00:	// Envelope
	case 0x01:	// Pan
	case 0x02:	// FD Low / step
	case 0x03:	// FD High / step
	case 0x04:	// Loop Start Low
	case 0x05:	// Loop Start High
		//if (Data == chan->ChnReg[Register])
		//	return false;
		//else
			chan->ChnReg[Register] = Data;
		break;
	case 0x06:	// Start
		// Address change - must be set
		chan->ChnReg[Register] = Data;
		//chan->addr = chan->start << (8 + 11);
		break;
	case 0x07:	// Control Register
		//if (Data == chip->CtrlReg)
		//	return false;
		//else
			chip->CtrlReg = Data;
		
		if (Data & 0x40)
		{
			chip->cbank = Data & 0x07;
		}
		else
		{
			chip->wbank = Data & 0x0F;
			chip->OffsetBase = chip->wbank << 12;
		}
		break;
	case 0x08:	// Channel On/Off Register
		chip->ChnMask = Data;
		
		// Address change
		/*for (i = 0; i < 8; i++)
		{
			chip->chan[i].enable = (~data >> i) & 1;
			if (!chip->chan[i].enable)
				chip->chan[i].addr = chip->chan[i].start << (8 + 11);
		}*/
		break;
	}
	
	return true;
}

bool rf5c164_mem_write(UINT16 Offset, UINT8 Data)
{
	//RF5C68_DATA* chip = &ChDat->RF5C164;
	STRIP_PCM* strip = &StripVGM.RF5C164;
	
	if (strip->All || (strip->Other & 0x01))
		return false;
	
	return true;
}

bool c140_write(UINT8 Port, UINT8 Register, UINT8 Data)
{
	STRIP_OPL4WT* strip = &StripVGM.C140;
	C140_DATA* chip = &ChDat->C140;
	UINT16 RegVal;
	UINT8 Channel;
	
	if (strip->All)
		return false;
	
	if (Port == 0xFF)
	{
		chip->BankType = Data;
		return true;
	}
	
	RegVal = (Port << 8) | Register;
	RegVal &= 0x1FF;
	
	// mirror the bank registers on the 219, fixes bkrtmaq
	if ((RegVal >= 0x1F8) && (chip->BankType == 0x03))
		RegVal -= 0x008;
	
	if (RegVal < 0x180)
	{
		Channel = RegVal >> 4;
		if (strip->ChnMask & (0x01 << Channel))
			return false;
	}
	
	return true;
}

