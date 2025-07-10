#ifndef __CHIPTEXT_H__
#define __CHIPTEXT_H__

#include "stdtype.h"

void InitChips(UINT32* ChipCounts);
void SetChip(UINT8 ChipID);
void GetFullChipName(char* TempStr, UINT8 ChipType);
void GGStereo(char* TempStr, UINT8 Data);
void sn76496_write(char* TempStr, UINT8 Command);
void ym2413_write(char* TempStr, UINT8 Register, UINT8 Data);
void ym2612_write(char* TempStr, UINT8 Port, UINT8 Register, UINT8 Data);
void ym2151_write(char* TempStr, UINT8 Register, UINT8 Data);
void segapcm_mem_write(char* TempStr, UINT16 Offset, UINT8 Data);
void rf5c68_reg_write(char* TempStr, UINT8 Register, UINT8 Data);
void rf5c68_mem_write(char* TempStr, UINT16 Offset, UINT8 Data);
void ym2203_write(char* TempStr, UINT8 Register, UINT8 Data);
void ym2608_write(char* TempStr, UINT8 Port, UINT8 Register, UINT8 Data);
void ym2610_write(char* TempStr, UINT8 Port, UINT8 Register, UINT8 Data);
void ym3812_write(char* TempStr, UINT8 Register, UINT8 Data);
void ym3526_write(char* TempStr, UINT8 Register, UINT8 Data);
void y8950_write(char* TempStr, UINT8 Register, UINT8 Data);
void ymf262_write(char* TempStr, UINT8 Port, UINT8 Register, UINT8 Data);
void ymz280b_write(char* TempStr, UINT8 Register, UINT8 Data);
void ymf278b_write(char* TempStr, UINT8 Port, UINT8 Register, UINT8 Data);
void ymf271_write(char* TempStr, UINT8 Port, UINT8 Register, UINT8 Data);
void rf5c164_reg_write(char* TempStr, UINT8 Register, UINT8 Data);
void rf5c164_mem_write(char* TempStr, UINT16 Offset, UINT8 Data);
void ay8910_reg_write(char* TempStr, UINT8 Register, UINT8 Data);
void ay8910_stereo_mask_write(char* TempStr, UINT8 Data);
void pwm_write(char* TempStr, UINT16 Port, UINT16 Data);
void gb_sound_write(char* TempStr, UINT8 Register, UINT8 Data);
void nes_psg_write(char* TempStr, UINT8 Register, UINT8 Data);
void c140_write(char* TempStr, UINT8 Port, UINT8 Register, UINT8 Data);
void c6280_write(char* TempStr, UINT8 Register, UINT8 Data);
void qsound_write(char* TempStr, UINT8 Offset, UINT16 Value);
void k053260_write(char* TempStr, UINT8 Register, UINT8 Data);
void pokey_write(char* TempStr, UINT8 Register, UINT8 Data);
void k051649_write(char* TempStr, UINT8 Port, UINT8 Register, UINT8 Data);
void okim6295_write(char* TempStr, UINT8 Port, UINT8 Data);
void okim6258_write(char* TempStr, UINT8 Port, UINT8 Data);
void multipcm_write(char* TempStr, UINT8 Port, UINT8 Data);
void multipcm_bank_write(char* TempStr, UINT8 Port, UINT8 Data);
void upd7759_write(char* TempStr, UINT8 Port, UINT8 Data);
void scsp_write(char* TempStr, UINT16 Register, UINT8 Data);
void vsu_write(char* TempStr, UINT16 Register, UINT8 Data);
void saa1099_write(char* TempStr, UINT8 Register, UINT8 Data);
void x1_010_write(char* TempStr, UINT16 Offset, UINT8 val);
void c352_write(char* TempStr, UINT16 Offset, UINT16 val);
void es5503_write(char* TempStr, UINT8 Register, UINT8 Data);
void es5506_w(char* TempStr, UINT8 offset, UINT8 data);
void es5506_w16(char* TempStr, UINT8 offset, UINT16 data);
void k054539_write(char* TempStr, UINT16 Register, UINT8 Data);
void wswan_write(char* TempStr, UINT8 Register, UINT8 Data);
void ws_mem_write(char* TempStr, UINT16 Offset, UINT8 Data);
void k007232_write(char* TempStr, UINT8 offset, UINT8 data);

#endif	// __CHIPTEXT_H__
