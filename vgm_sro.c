// vgm_sro.c - VGM Sample-ROM Optimizer
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "stdtype.h"
#include "stdbool.h"
#include "VGMFile.h"
#include "common.h"


static bool OpenVGMFile(const char* FileName);
static void WriteVGMFile(const char* FileName);
static void FindUsedROMData(void);
static void EnumerateROMRegions(void);
static void OptimizeVGMSampleROM(void);
static UINT8 GetRomRgnFromType(UINT8 ROMType);


void InitAllChips(void);
void FreeAllChips(void);
void SetChipSet(UINT8 ChipID);
void segapcm_mem_write(UINT16 Offset, UINT8 Data);
void ym2608_write(UINT8 Port, UINT8 Register, UINT8 Data);
void ym2610_write(UINT8 Port, UINT8 Register, UINT8 Data);
void y8950_write(UINT8 Register, UINT8 Data);
void ymz280b_write(UINT8 Register, UINT8 Data);
void rf5c68_write(UINT8 Register, UINT8 Data);
void rf5c164_write(UINT8 Register, UINT8 Data);
void ymf271_write(UINT8 Port, UINT8 Register, UINT8 Data);
void nes_apu_write(UINT8 Register, UINT8 Data);
void multipcm_write(UINT8 Port, UINT8 Data);
void multipcm_bank_write(UINT8 Bank, UINT16 Data);
void upd7759_write(UINT8 Port, UINT8 Data);
void okim6295_write(UINT8 Offset, UINT8 Data);
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
void k007232_write(UINT8 offset, UINT8 data);
void write_rom_data(UINT8 ROMType, UINT32 ROMSize, UINT32 DataStart, UINT32 DataLength,
					const UINT8* ROMData);
UINT32 GetROMMask(UINT8 ROMType, UINT8** MaskData);
UINT32 GetROMData(UINT8 ROMType, UINT8** ROMData);


typedef struct rom_region
{
	UINT32 AddrStart;
	UINT32 AddrEnd;
} ROM_REGION;
typedef struct rom_region_list
{
	UINT32 RegionCount;
	ROM_REGION* Regions;
	bool IsWritten;
} ROM_RGN_LIST;


#define ROM_TYPES	0x19
const UINT8 ROM_LIST[ROM_TYPES] =
{	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
	0x90, 0x91, 0x92, 0x93, 0x94, 
	0xC0, 0xC1, 0xC2, 0xE1};


VGM_HEADER VGMHead;
UINT32 VGMDataLen;
UINT8* VGMData;
UINT32 VGMPos;
INT32 VGMSmplPos;
ROM_RGN_LIST ROMRgnLst[ROM_TYPES][0x02];
UINT8* DstData;
UINT32 DstDataLen;
char FileBase[0x100];
bool CancelFlag;

bool removeEmpty;

int main(int argc, char* argv[])
{
	int argbase;
	int ErrVal;
	char FileName[0x100];
	UINT8 CurROM;

	printf("VGM Sample-ROM Optimizer\n------------------------\n\n");

	ErrVal = 0;
	argbase = 1;
	removeEmpty = false;

	argbase = 1;
	while(argbase < argc && argv[argbase][0] == '-')
	{
		if (! stricmp(argv[argbase], "-strip-empty"))
		{
			removeEmpty = true;
			argbase ++;
		}
		else
		{
			break;
		}
	}

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
		return 1;

	if (! OpenVGMFile(FileName))
	{
		printf("Error opening the file!\n");
		ErrVal = 1;
		goto EndProgram;
	}
	printf("\n");

	DstData = NULL;
	for (CurROM = 0x00; CurROM < ROM_TYPES; CurROM ++)
	{
		ROMRgnLst[CurROM][0x00].Regions = NULL;
		ROMRgnLst[CurROM][0x01].Regions = NULL;
	}

	if (! VGMHead.lngHzSPCM && ! VGMHead.lngHzYM2608 && ! VGMHead.lngHzYM2610 &&
		! VGMHead.lngHzY8950 && ! VGMHead.lngHzYMZ280B && ! VGMHead.lngHzRF5C68 &&
		! VGMHead.lngHzRF5C164 && ! VGMHead.lngHzYMF271 && ! VGMHead.lngHzOKIM6295 &&
		! VGMHead.lngHzK054539 && ! VGMHead.lngHzC140 && ! VGMHead.lngHzK053260 &&
		! VGMHead.lngHzQSound && ! VGMHead.lngHzUPD7759 && ! VGMHead.lngHzMultiPCM &&
		! VGMHead.lngHzNESAPU && ! VGMHead.lngHzES5503 && ! VGMHead.lngHzES5506 &&
		! VGMHead.lngHzGA20 && ! VGMHead.lngHzX1_010 && ! VGMHead.lngHzC352 &&
		! VGMHead.lngHzYMF278B && ! VGMHead.lngHzK007232)
	{
		printf("No chips with Sample-ROM used!\n");
		ErrVal = 2;
		goto BreakProgress;
	}

	CancelFlag = false;
	FindUsedROMData();
	if (CancelFlag)
	{
		ErrVal = 9;
		goto BreakProgress;
	}
	EnumerateROMRegions();
	OptimizeVGMSampleROM();

	if (DstDataLen < VGMDataLen)
	{
		if (argc > argbase + 1)
			strcpy(FileName, argv[argbase + 1]);
		else
			strcpy(FileName, "");
		if (FileName[0] == '\0')
		{
			strcpy(FileName, FileBase);
			strcat(FileName, "_optimized.vgm");
		}
		WriteVGMFile(FileName);
	}

BreakProgress:
	free(VGMData);
	free(DstData);
	for (CurROM = 0x00; CurROM < ROM_TYPES; CurROM ++)
	{
		free(ROMRgnLst[CurROM][0x00].Regions);	ROMRgnLst[CurROM][0x00].Regions = NULL;
		free(ROMRgnLst[CurROM][0x01].Regions);	ROMRgnLst[CurROM][0x01].Regions = NULL;
	}

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

static void WriteVGMFile(const char* FileName)
{
	FILE* hFile;

	hFile = fopen(FileName, "wb");
	fwrite(DstData, 0x01, DstDataLen, hFile);
	fclose(hFile);

	printf("File written.\n");

	return;
}

static void FindUsedROMData(void)
{
	UINT8 ChipID;
	UINT8 Command;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	UINT32 ROMSize;
	UINT32 DataStart;
	UINT32 DataLen;
#ifdef WIN32
	UINT32 CmdTimer;
	char TempStr[0x80];
	char MinSecStr[0x80];
#endif
	UINT32 CmdLen;
	bool StopVGM;
	const UINT8* VGMPnt;

	printf("Creating ROM-Usage Mask ...\n");
	VGMPos = VGMHead.lngDataOffset;

#ifdef WIN32
	CmdTimer = 0;
#endif
	InitAllChips();

	if (VGMHead.lngHzSPCM)
	{
		SetChipSet(0x00);
		segapcm_mem_write(0xFFF3, (VGMHead.lngSPCMIntf & 0xFF000000) >> 24);
		segapcm_mem_write(0xFFF2, (VGMHead.lngSPCMIntf & 0x00FF0000) >> 16);
		segapcm_mem_write(0xFFF1, (VGMHead.lngSPCMIntf & 0x0000FF00) >>  8);
		segapcm_mem_write(0xFFF0, (VGMHead.lngSPCMIntf & 0x000000FF) >>  0);
		if (VGMHead.lngHzSPCM & 0x40000000)
		{
			SetChipSet(0x01);
			segapcm_mem_write(0xFFF3, (VGMHead.lngSPCMIntf & 0xFF000000) >> 24);
			segapcm_mem_write(0xFFF2, (VGMHead.lngSPCMIntf & 0x00FF0000) >> 16);
			segapcm_mem_write(0xFFF1, (VGMHead.lngSPCMIntf & 0x0000FF00) >>  8);
			segapcm_mem_write(0xFFF0, (VGMHead.lngSPCMIntf & 0x000000FF) >>  0);
		}
	}
	if (VGMHead.lngHzK054539)
	{
		SetChipSet(0x00);
		k054539_write(0xFF, 0x00, VGMHead.bytK054539Flags);
		if (VGMHead.lngHzK054539 & 0x40000000)
		{
			SetChipSet(0x01);
			k054539_write(0xFF, 0x00, VGMHead.bytK054539Flags);
		}
	}
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
	if (VGMHead.lngHzES5506)
	{
		SetChipSet(0x00);
		es550x_w(0xFF, VGMHead.lngHzES5506 >> 31);
		if (VGMHead.lngHzES5506 & 0x40000000)
		{
			SetChipSet(0x01);
			es550x_w(0xFF, VGMHead.lngHzES5506 >> 31);
		}
	}

	StopVGM = false;
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
			ChipID = 0x00;
			switch(Command)
			{
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
			case 0xAC:
				if (VGMHead.lngHzY8950 & 0x40000000)
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
				CmdLen = 0x02;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMPnt[0x02];
				memcpy(&TempLng, &VGMPnt[0x03], 0x04);

				ChipID = (TempLng & 0x80000000) >> 31;
				TempLng &= 0x7FFFFFFF;
				SetChipSet(ChipID);

				switch(TempByt & 0xC0)
				{
				case 0x00:	// Database Block
				case 0x40:
					break;
				case 0x80:	// ROM/RAM Dump
					memcpy(&ROMSize, &VGMPnt[0x07], 0x04);
					memcpy(&DataStart, &VGMPnt[0x0B], 0x04);
					DataLen = TempLng - 0x08;
					write_rom_data(TempByt, ROMSize, DataStart, DataLen, &VGMPnt[0x0F]);
					break;
				case 0xC0:	// RAM Write
					if (! (TempByt & 0x20))
					{
						memcpy(&TempSht, &VGMPnt[0x07], 0x02);
						DataLen = TempLng - 0x02;
						write_rom_data(TempByt, 0x00, TempSht, DataLen, &VGMPnt[0x09]);
					}
					else
					{
						memcpy(&DataStart, &VGMPnt[0x07], 0x04);
						DataLen = TempLng - 0x04;
						write_rom_data(TempByt, 0x00, DataStart, DataLen, &VGMPnt[0x0B]);
					}
					break;
				}
				CmdLen = 0x07 + TempLng;
				break;
			case 0xE0:	// Seek to PCM Data Bank Pos
				CmdLen = 0x05;
				break;
			case 0x41:	// K007232 write
				SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				k007232_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0x4F:	// GG Stereo
				CmdLen = 0x02;
				break;
			case 0xC0:	// Sega PCM memory write
				memcpy(&TempSht, &VGMPnt[0x01], 0x02);
				segapcm_mem_write(TempSht, VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0xB0:	// RF5C68 register write
				// PCM, but doesn't have ROM - but RAM
				rf5c68_write(VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xC1:	// RF5C68 memory write
				printf("RF5Cxx Memory Writes aren't supported.\n");
				CancelFlag = true;
				StopVGM = true;
				CmdLen = 0x04;
				break;
			case 0x56:	// YM2608 write port 0
			case 0x57:	// YM2608 write port 1
				ym2608_write(Command & 0x01, VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0x58:	// YM2610 write port 0
			case 0x59:	// YM2610 write port 1
				ym2610_write(Command & 0x01, VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0x5C:	// Y8950 write
				y8950_write(VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0x5D:	// YMZ280B write
				ymz280b_write(VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xD0:	// YMF278B write
				SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				ymf278b_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02], VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0xD1:	// YMF271 write
				SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				ymf271_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02], VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0xB1:	// RF5C164 register write
				rf5c164_write(VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xC2:	// RF5C164 memory write
				printf("RF5Cxx Memory Writes aren't supported.\n");
				CancelFlag = true;
				StopVGM = true;
				CmdLen = 0x04;
				break;
			case 0x68:	// PCM RAM write
				printf("Streamed RF-PCM-Data can't be optimized.\n");
				CancelFlag = true;
				StopVGM = true;
				CmdLen = 0x0C;
				break;
			case 0xB4:	// NES APU write
				SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				nes_apu_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xB5:	// MultiPCM write
				SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				multipcm_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xC3:	// MultiPCM memory write
				SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				memcpy(&TempSht, &VGMPnt[0x02], 0x02);
				multipcm_bank_write(VGMPnt[0x01] & 0x7F, TempSht);
				CmdLen = 0x04;
				break;
			case 0xB6:	// UPD7759 write
				SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				upd7759_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xB8:	// OKIM6295 write
				SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				okim6295_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xD3:	// K054539 write
				SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				k054539_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02], VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0xD4:	// C140 write
				SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				c140_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02], VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0xBA:	// K053260 write
				SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				k053260_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xBF:	// GA20 write
				SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				ga20_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xC4:	// Q-Sound write
				//SetChipSet(0x00);
				qsound_write(VGMPnt[0x03], (VGMPnt[0x01] << 8) | (VGMPnt[0x02] << 0));
				CmdLen = 0x04;
				break;
			case 0xD5:	// ES5503 write
				SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				es5503_write(VGMPnt[0x02], VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0xBE:	// ES5506 write (8-bit data)
				SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				es550x_w(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xD6:	// ES5506 write (16-bit data)
				SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				es550x_w16(VGMPnt[0x01] & 0x7F, (VGMPnt[0x02] << 8) | (VGMPnt[0x03] << 0));
				CmdLen = 0x04;
				break;
			case 0xC8:	// X1-010 write
				SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				x1_010_write(((VGMPnt[0x01]&0x7f) << 8) | (VGMPnt[0x02] << 0), VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0xE1:	// C352 write
				SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				c352_write(((VGMPnt[0x01]&0x7f) << 8 | (VGMPnt[0x02] << 0)), (VGMPnt[0x03] << 8) | (VGMPnt[0x04] << 0));
				CmdLen = 0x05;
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
					printf("Unknown Command: %X\n", Command);
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

#ifdef WIN32
		if (CmdTimer < GetTickCount())
		{
			PrintMinSec(VGMSmplPos, MinSecStr);
			PrintMinSec(VGMHead.lngTotalSamples, TempStr);
			TempLng = VGMPos - VGMHead.lngDataOffset;
			ROMSize = VGMHead.lngEOFOffset - VGMHead.lngDataOffset;
			printf("%04.3f %% - %s / %s (%08X / %08X) ...\r", (float)TempLng / ROMSize * 100,
					MinSecStr, TempStr, VGMPos, VGMHead.lngEOFOffset);
			CmdTimer = GetTickCount() + 200;
		}
#endif
	}
	printf("\t\t\t\t\t\t\t\t\r");

	return;
}

char* GetROMRegionText(UINT8 ROM_ID)
{
	char* RetStr;

	switch(ROM_ID)
	{
	case 0x80:	// SegaPCM ROM
		RetStr = "SegaPCM";
		break;
	case 0x81:	// YM2608 DELTA-T ROM
		RetStr = "YM2608 DELTA-T";
		break;
	case 0x82:	// YM2610 ADPCM ROM
		RetStr = "YM2610 ADPCM";
		break;
	case 0x83:	// YM2610 DELTA-T ROM
		RetStr = "YM2610 DELTA-T";
		break;
	case 0x84:	// YMF278B ROM
		RetStr = "YMF278B";
		break;
	case 0x85:	// YMF271 ROM
		RetStr = "YMF271";
		break;
	case 0x86:	// YMZ280B ROM
		RetStr = "YMZ280B";
		break;
	case 0x87:	// YMF278B RAM
		RetStr = "YMF278B RAM";
		break;
	case 0x88:	// Y8950 DELTA-T ROM
		RetStr = "Y8950 DELTA-T";
		break;
	case 0x89:	// MultiPCM ROM
		RetStr = "MultiPCM";
		break;
	case 0x8A:	// UPD7759 ROM
		RetStr = "UPD7759";
		break;
	case 0x8B:	// OKIM6295 ROM
		RetStr = "OKIM6295";
		break;
	case 0x8C:	// K054539
		RetStr = "K054539";
		break;
	case 0x8D:	// C140
		RetStr = "C140";
		break;
	case 0x8E:	// K053260
		RetStr = "K053260";
		break;
	case 0x8F:	// QSound
		RetStr = "Q-Sound";
		break;
	case 0x90:	// ES5506 ROM
		RetStr = "ES5506";
		break;
	case 0x91:	// X1-010
		RetStr = "X1-010";
		break;
	case 0x92:	// C352
		RetStr = "C352";
		break;
	case 0x93:	// GA20
		RetStr = "GA20";
		break;
	case 0x94:	// K007232
		RetStr = "K007232";
		break;
	case 0xC0:	// RF5C68 RAM
		RetStr = "RF5C68";
		break;
	case 0xC1:	// RF5C164 RAM
		RetStr = "RF5C164";
		break;
	case 0xC2:	// NES APU RAM
		RetStr = "NES APU";
		break;
	case 0xE1:	// ES5503 RAM
		RetStr = "ES5503";
		break;
	default:
		RetStr = "???";
		break;
	}

	return RetStr;
}

static void EnumerateROMRegions(void)
{
	const char* STATUS_TEXT[] = {"unused", "USED", "Empty", "Empty/USED"};
	UINT8 CurROM;
	UINT8 CurChip;
	UINT32 ROMSize;
	UINT8* ROMMask;
	ROM_RGN_LIST* TempRRL;
	UINT32 RgnMemCount;
	ROM_REGION* RgnMemory;
	UINT32 CurRgn;
	UINT32 CurPos;
	UINT32 PosStart;
	UINT8 MaskVal;

	RgnMemCount = 0x100;
	RgnMemory = (ROM_REGION*)malloc(RgnMemCount * sizeof(ROM_REGION));

	printf("\nROM Region List\n---------------\n");
	printf("From\tTo\tLength\tStatus\n");
	for (CurROM = 0x00; CurROM < ROM_TYPES; CurROM ++)
	{
		for (CurChip = 0x00; CurChip < 0x02; CurChip ++)
		{
			TempRRL = &ROMRgnLst[CurROM][CurChip];

			TempRRL->RegionCount = 0x00;

			SetChipSet(CurChip);
			ROMSize = GetROMMask(ROM_LIST[CurROM], &ROMMask);
			if (! ROMSize || ROMMask == NULL)
				continue;

			if (! CurChip)
				printf("  %s\n", GetROMRegionText(ROM_LIST[CurROM]));
			else
				printf("  %s #%u\n", GetROMRegionText(ROM_LIST[CurROM]), CurChip + 1);

			CurRgn = 0x00;
			RgnMemory[CurRgn].AddrStart = 0xFFFFFFFF;
			PosStart = 0x00;
			MaskVal = ROMMask[0x00];
			for (CurPos = 0x01; CurPos <= ROMSize; CurPos ++)
			{
				if (CurPos >= ROMSize || MaskVal != ROMMask[CurPos])
				{
					printf("%06X\t%06X\t%6X\t%s\n", PosStart, CurPos - 1, CurPos - PosStart,
							STATUS_TEXT[MaskVal]);

					if (MaskVal == 0x01)
					{
						if (! CurRgn)
						{
							if (PosStart < 0x20)
								PosStart = 0x00;
						}
						else
						{
							if ((PosStart - RgnMemory[CurRgn - 0x01].AddrEnd) < 0x20)
								CurRgn --;
						}
					}
					if (MaskVal == 0x01)	// (MaskVal & 0x01) would also write empty (used) blocks
					{
						// Used ROM Data
						if (RgnMemory[CurRgn].AddrStart == 0xFFFFFFFF)
							RgnMemory[CurRgn].AddrStart = PosStart;	// Set Address if not already set
						RgnMemory[CurRgn].AddrEnd = CurPos;
						CurRgn ++;
						if (CurRgn >= RgnMemCount)
						{	// I really don't expect this, but I want to be safe
							RgnMemCount += 0x100;
							RgnMemory = (ROM_REGION*)realloc(RgnMemory,
															RgnMemCount * sizeof(ROM_REGION));
						}
						RgnMemory[CurRgn].AddrStart = 0xFFFFFFFF;
					}

					if (CurPos < ROMSize)
						MaskVal = ROMMask[CurPos];
					PosStart = CurPos;
				}
			}

			if (! CurRgn)
			{
				// to make sure that the ROM is at least allocated,
				// an empty region is created
				RgnMemory[CurRgn].AddrStart = 0x00;
				RgnMemory[CurRgn].AddrEnd = 0x00;
				CurRgn ++;
			}

			TempRRL->RegionCount = CurRgn;
			TempRRL->Regions = (ROM_REGION*)malloc(TempRRL->RegionCount * sizeof(ROM_REGION));
			for (CurRgn = 0x00; CurRgn < TempRRL->RegionCount; CurRgn ++)
				TempRRL->Regions[CurRgn] = RgnMemory[CurRgn];
			TempRRL->IsWritten = false;
		}
	}

	free(RgnMemory);
	return;
}

static void OptimizeVGMSampleROM(void)
{
	UINT32 DstPos;
	UINT8 ChipID;
	UINT8 Command;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	UINT32 ROMSize;
	UINT32 DataStart;
	UINT32 DataLen;
#ifdef WIN32
	UINT32 CmdTimer;
	char TempStr[0x80];
	char MinSecStr[0x80];
#endif
	UINT32 CmdLen;
	bool StopVGM;
	bool WriteEvent;
	UINT32 NewLoopS;
	UINT8* ROMData;
	UINT8 CurRgn;
	ROM_RGN_LIST* TempRRL;
	ROM_REGION* TempRgn;

	DstData = (UINT8*)malloc(VGMDataLen + 0x100);
	VGMPos = VGMHead.lngDataOffset;
	DstPos = VGMHead.lngDataOffset;
	NewLoopS = 0x00;
	memcpy(DstData, VGMData, VGMPos);	// Copy Header

#if 1
	for (TempByt = 0x00; TempByt < ROM_TYPES; TempByt ++)
	{
		for (ChipID = 0x00; ChipID < 0x02; ChipID ++)
		{
			if ((ROM_LIST[TempByt] & 0xC0) != 0xC0)
				continue;

			SetChipSet(ChipID);
			ROMSize = GetROMData(ROM_LIST[TempByt], &ROMData);
			TempRRL = &ROMRgnLst[TempByt][ChipID];
			for (CurRgn = 0x00; CurRgn < TempRRL->RegionCount; CurRgn ++)
			{
				TempRgn = &TempRRL->Regions[CurRgn];
				DataLen = TempRgn->AddrEnd - TempRgn->AddrStart;
				if (! DataLen)	// empty RAM writes make no sense, since the
					continue;	// RAM size isn't changeable
				CmdLen = (ROM_LIST[TempByt] & 0x20) ? 0x04 : 0x02;
				CmdLen += DataLen;

				DstData[DstPos + 0x00] = 0x67;
				DstData[DstPos + 0x01] = 0x66;
				DstData[DstPos + 0x02] = ROM_LIST[TempByt];
				memcpy(&DstData[DstPos + 0x03], &CmdLen, 0x04);
				DstData[DstPos + 0x06] |= (ChipID << 7);	// set '2nd Chip'-bit
				if (! (ROM_LIST[TempByt] & 0x20))
				{
					TempSht = TempRgn->AddrStart & 0xFFFF;
					memcpy(&DstData[DstPos + 0x07], &TempSht, 0x02);
					memcpy(&DstData[DstPos + 0x09], &ROMData[TempSht], DataLen);
				}
				else
				{
					memcpy(&DstData[DstPos + 0x07], &TempRgn->AddrStart, 0x04);
					memcpy(&DstData[DstPos + 0x0B], &ROMData[TempRgn->AddrStart], DataLen);
				}
				DstPos += 0x07 + CmdLen;
			}
			TempRRL->IsWritten = true;
		}
	}
#endif

#ifdef WIN32
	CmdTimer = 0;
#endif
	SetChipSet(0x00);
	StopVGM = false;
	while(VGMPos < VGMHead.lngEOFOffset)
	{
		CmdLen = 0x00;
		Command = VGMData[VGMPos + 0x00];
		WriteEvent = true;

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
				memcpy(&TempSht, &VGMData[VGMPos + 0x01], 0x02);
				VGMSmplPos += TempSht;
				CmdLen = 0x03;
				break;
			case 0x50:	// SN76496 write
				CmdLen = 0x02;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMData[VGMPos + 0x02];
				memcpy(&TempLng, &VGMData[VGMPos + 0x03], 0x04);

				ChipID = (TempLng & 0x80000000) >> 31;
				TempLng &= 0x7FFFFFFF;
				SetChipSet(ChipID);

				switch(TempByt & 0xC0)
				{
				case 0x00:	// Database Block
				case 0x40:
					/*switch(TempByt & 0x3F)
					{
					case 0x00:	// YM2612 PCM Database
						break;
					case 0x01:	// RF5C68 PCM Database
						break;
					case 0x02:	// RF5C164 PCM Database
						break;
					}*/
					break;
				case 0x80:	// ROM/RAM Dump
					ROMSize = GetROMData(TempByt, &ROMData);
					TempRRL = &ROMRgnLst[GetRomRgnFromType(TempByt)][ChipID];
					if (! TempRRL->IsWritten)
					{
						for (CurRgn = 0x00; CurRgn < TempRRL->RegionCount; CurRgn ++)
						{
							TempRgn = &TempRRL->Regions[CurRgn];
							DataLen = TempRgn->AddrEnd - TempRgn->AddrStart;
							if (removeEmpty && DataLen == 0)
								continue;
							// Note: There are empty ROM Blocks to set the ROM Size
							CmdLen = 0x08 + DataLen;
							DataStart = TempRgn->AddrStart;

							DstData[DstPos + 0x00] = 0x67;
							DstData[DstPos + 0x01] = 0x66;
							DstData[DstPos + 0x02] = TempByt;
							memcpy(&DstData[DstPos + 0x03], &CmdLen, 0x04);
							DstData[DstPos + 0x06] |= (ChipID << 7);	// set '2nd Chip'-bit
							memcpy(&DstData[DstPos + 0x07], &ROMSize, 0x04);
							memcpy(&DstData[DstPos + 0x0B], &DataStart, 0x04);
							memcpy(&DstData[DstPos + 0x0F], &ROMData[DataStart], DataLen);
							DstPos += 0x07 + CmdLen;
						}
						TempRRL->IsWritten = true;
					}
					WriteEvent = false;
					break;
				case 0xC0:	// RAM Write
					ROMSize = GetROMData(TempByt, &ROMData);
					TempRRL = &ROMRgnLst[GetRomRgnFromType(TempByt)][ChipID];
					if (! TempRRL->IsWritten)
					{
						for (CurRgn = 0x00; CurRgn < TempRRL->RegionCount; CurRgn ++)
						{
							TempRgn = &TempRRL->Regions[CurRgn];
							DataLen = TempRgn->AddrEnd - TempRgn->AddrStart;
							if (! DataLen)	// empty RAM writes make no sense, since the
								continue;	// RAM size isn't changeable
							CmdLen = (TempByt & 0x20) ? 0x04 : 0x02;
							CmdLen += DataLen;

							DstData[DstPos + 0x00] = 0x67;
							DstData[DstPos + 0x01] = 0x66;
							DstData[DstPos + 0x02] = TempByt;
							memcpy(&DstData[DstPos + 0x03], &CmdLen, 0x04);
							DstData[DstPos + 0x06] |= (ChipID << 7);	// set '2nd Chip'-bit

							if (! (TempByt & 0x20))	// C0-DF - small RAM (<= 64 KB)
							{
								TempSht = TempRgn->AddrStart & 0xFFFF;
								memcpy(&DstData[DstPos + 0x07], &TempSht, 0x02);
								memcpy(&DstData[DstPos + 0x09], &ROMData[TempSht], DataLen);
							}
							else	// E0-FF - large RAM (> 64 KB)
							{
								memcpy(&DstData[DstPos + 0x07], &TempRgn->AddrStart, 0x04);
								memcpy(&DstData[DstPos + 0x0B], &ROMData[TempRgn->AddrStart], DataLen);
							}
							DstPos += 0x07 + CmdLen;
						}
						TempRRL->IsWritten = true;
					}
					WriteEvent = false;
					break;
				}
				CmdLen = 0x07 + TempLng;
				break;
			case 0xE0:	// Seek to PCM Data Bank Pos
				CmdLen = 0x05;
				break;
			case 0x4F:	// GG Stereo
				CmdLen = 0x02;
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
					printf("Unknown Command: %X\n", Command);
					CmdLen = 0x01;
					//StopVGM = true;
					break;
				}
				break;
			}
		}

		// Note: In the case that the loop offset points to a Data Block,
		//       it gets moved to the first command after it.
		if (VGMPos == VGMHead.lngLoopOffset)
			NewLoopS = DstPos;
		if (WriteEvent)
		{
			memcpy(&DstData[DstPos], &VGMData[VGMPos], CmdLen);
			DstPos += CmdLen;
		}
		VGMPos += CmdLen;
		if (StopVGM)
			break;

#ifdef WIN32
		if (CmdTimer < GetTickCount())
		{
			PrintMinSec(VGMSmplPos, MinSecStr);
			PrintMinSec(VGMHead.lngTotalSamples, TempStr);
			TempLng = VGMPos - VGMHead.lngDataOffset;
			ROMSize = VGMHead.lngEOFOffset - VGMHead.lngDataOffset;
			printf("%04.3f %% - %s / %s (%08X / %08X) ...\r", (float)TempLng / ROMSize * 100,
					MinSecStr, TempStr, VGMPos, VGMHead.lngEOFOffset);
			CmdTimer = GetTickCount() + 200;
		}
#endif
	}
	if (! NewLoopS && VGMHead.lngLoopOffset)
		printf("Warning! Failed to relocate loop!\n");
	if (VGMHead.lngLoopOffset)
	{
		TempLng = NewLoopS - 0x1C;
		memcpy(&DstData[0x1C], &TempLng, 0x04);
	}
	printf("\t\t\t\t\t\t\t\t\r");

	if (VGMHead.lngGD3Offset && VGMHead.lngGD3Offset + 0x0B < VGMHead.lngEOFOffset)
	{
		VGMPos = VGMHead.lngGD3Offset;
		memcpy(&TempLng, &VGMData[VGMPos + 0x00], 0x04);
		if (TempLng == FCC_GD3)
		{
			memcpy(&CmdLen, &VGMData[VGMPos + 0x08], 0x04);
			CmdLen += 0x0C;

			TempLng = DstPos - 0x14;
			memcpy(&DstData[0x14], &TempLng, 0x04);
			memcpy(&DstData[DstPos], &VGMData[VGMPos], CmdLen);
			DstPos += CmdLen;
		}
	}
	DstDataLen = DstPos;
	TempLng = DstDataLen - 0x04;
	memcpy(&DstData[0x04], &TempLng, 0x04);

	FreeAllChips();

	return;
}

static UINT8 GetRomRgnFromType(UINT8 ROMType)
{
	UINT8 CurType;

	for (CurType = 0x00; CurType < ROM_TYPES; CurType ++)
	{
		if (ROM_LIST[CurType] == ROMType)
			return CurType;
	}

	return 0x00;
}
