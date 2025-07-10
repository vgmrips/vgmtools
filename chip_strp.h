typedef struct stip_generic	// contains all common values
{
	bool All;
	UINT32 ChnMask;
} STRIP_GENERIC;
typedef struct stip_sn76496
{
	bool All;
	UINT32 ChnMask;	// 0, 1, 2, noise
	UINT8 Other;	// Bit 0 - GG Stereo
} STRIP_PSG;
typedef struct stip_ym3812
{
	bool All;
	UINT32 ChnMask;	// 0-8 (0-17 for OPL3/4), BD, SD, TT, TC, HH
	UINT8 Other;	// Bit 7 - Delta-T
} STRIP_OPL;
typedef struct stip_ym2612
{
	bool All;
	UINT32 ChnMask;	// 0-5, DAC
	UINT8 Other;	// Bit 7 - Delta-T
} STRIP_OPN;
typedef struct stip_ym2151
{
	bool All;
	UINT32 ChnMask;	// 0-7
	UINT8 Other;
} STRIP_OPM;
typedef struct stip_pcm
{
	bool All;
	UINT32 ChnMask;	// 0-7
	UINT8 Other;
} STRIP_PCM;
typedef struct stip_ymf278b_wt
{
	bool All;
	UINT32 ChnMask;	// 0-23
	UINT8 Other;
} STRIP_OPL4WT;
typedef struct stip_ymf271
{
	bool All;
	UINT32 ChnMask;	// 0-11
	UINT8 Other;
} STRIP_OPX;
typedef struct stip_ymz280b
{
	bool All;
	UINT32 ChnMask;	// 0-7
	UINT8 Other;
} STRIP_YMZ;
typedef struct stip_pwm
{
	bool All;
	// there is only ONE sort of commands
} STRIP_PWM;
typedef struct stip_dac_control
{
	bool All;
	UINT8 StrMsk[0x20];	// 0x100 / 8
} STRIP_DACCTRL;
typedef struct strip_data
{
	STRIP_DACCTRL DacCtrl;
	STRIP_PSG SN76496;
	STRIP_OPL YM2413;
	STRIP_OPN YM2612;
	STRIP_OPM YM2151;
	STRIP_PCM SegaPCM;
	STRIP_PCM RF5C68;
	STRIP_OPN YM2203;
	STRIP_OPN YM2608;
	STRIP_OPN YM2610;
	STRIP_OPL YM3812;
	STRIP_OPL YM3526;
	STRIP_OPL Y8950;
	STRIP_OPL YMF262;
	bool YMF278B_All;
	STRIP_OPL YMF278B_FM;
	STRIP_OPL4WT YMF278B_WT;
	STRIP_OPX YMF271;
	STRIP_YMZ YMZ280B;
	STRIP_PCM RF5C164;
	STRIP_PWM PWM;
	STRIP_PSG AY8910;
	STRIP_PSG GBDMG;
	STRIP_PSG NESAPU;
	STRIP_OPL4WT MultiPCM;
	STRIP_PCM UPD7759;
	STRIP_PCM OKIM6258;
	STRIP_PCM OKIM6295;
	STRIP_PSG K051649;
	STRIP_PCM K054539;
	STRIP_PSG HuC6280;
	STRIP_OPL4WT C140;
	STRIP_PCM K053260;
	STRIP_PCM K007232;
	STRIP_PSG Pokey;
	STRIP_OPX QSound;
	bool Unknown;
} STRIP_DATA;

void InitAllChips(void);
void FreeAllChips(void);
void SetChipSet(UINT8 ChipID);
bool GGStereo(UINT8 Data);
bool sn76496_write(UINT8 Command);
bool ym2413_write(UINT8 Register, UINT8 Data);
bool ym2612_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool ym2151_write(UINT8 Register, UINT8 Data);
bool segapcm_mem_write(UINT16 Offset, UINT8 Data);
bool rf5c68_reg_write(UINT8 Register, UINT8* Data);
bool rf5c68_mem_write(UINT16 Offset, UINT8 Data);
bool ym2203_write(UINT8 Register, UINT8 Data);
bool ym2608_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool ym2610_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool ym3812_write(UINT8 Register, UINT8 Data);
bool ym3526_write(UINT8 Register, UINT8 Data);
bool y8950_write(UINT8 Register, UINT8 Data);
bool ymf262_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool ymz280b_write(UINT8 Register, UINT8 Data);
bool rf5c164_reg_write(UINT8 Register, UINT8* Data);
bool rf5c164_mem_write(UINT16 Offset, UINT8 Data);
bool c140_write(UINT8 Port, UINT8 Register, UINT8 Data);
