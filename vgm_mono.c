// vgm_mono.c - VGM Compressor
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "stdtype.h"
#include "stdbool.h"
#include "VGMFile.h"
#include "common.h"
#include "vgm_lib.h"


static bool OpenVGMFile(const char* FileName);
static void WriteVGMFile(const char* FileName);
static void CompressVGMData(void);

// Function Prototypes from chip_cmp.c
/*void InitAllChips(void);
void ResetAllChips(void);
void FreeAllChips(void);
void SetChipSet(UINT8 ChipID);
bool GGStereo(UINT8 Data);
bool sn76496_write(UINT8 Command);
bool ym2413_write(UINT8 Register, UINT8 Data);
bool ym2612_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool ym2151_write(UINT8 Register, UINT8 Data);
bool segapcm_mem_write(UINT16 Offset, UINT8 Data);
bool rf5c68_reg_write(UINT8 Register, UINT8 Data);
bool ym2203_write(UINT8 Register, UINT8 Data);
bool ym2608_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool ym2610_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool ym3812_write(UINT8 Register, UINT8 Data);
bool ym3526_write(UINT8 Register, UINT8 Data);
bool y8950_write(UINT8 Register, UINT8 Data);
bool ymf262_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool ymz280b_write(UINT8 Register, UINT8 Data);
bool rf5c164_reg_write(UINT8 Register, UINT8 Data);
bool ay8910_write_reg(UINT8 Register, UINT8 Data);
bool ymf271_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool gameboy_write_reg(UINT8 Register, UINT8 Data);
bool nes_psg_write(UINT8 Register, UINT8 Data);
bool c140_write(UINT8 Port, UINT8 Register, UINT8 Data);
bool qsound_write(UINT8 Offset, UINT16 Value);
bool pokey_write(UINT8 Register, UINT8 Data);
bool c6280_write(UINT8 Register, UINT8 Data);*/


VGM_HEADER VGMHead;
UINT32 VGMDataLen;
UINT8* VGMData;
UINT32 VGMPos;
INT32 VGMSmplPos;
//UINT8* DstData;
//UINT32 DstDataLen;
char FileBase[0x100];

int main(int argc, char* argv[])
{
	int argbase;
	int ErrVal;
	char FileName[0x100];

	printf("VGM Mono\n--------\n");

	ErrVal = 0;
	argbase = 1;
	/*while(argbase < argc && argv[argbase][0] == '-')
	{
		if (! strcmp(argv[argbase + 0x00], "-justtmr"))
		{
			JustTimerCmds = true;
			argbase ++;
		}
		else
		{
			break;
		}
	}*/

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

	/*PassNo = 0x00;
	do
	{
		printf("Pass #%u ...\n", PassNo + 1);*/
		CompressVGMData();
	/*	if (! PassNo)
			SrcDataSize = DataSizeA;
		printf("    Data Compression: %u -> %u (%.1f %%)\n",
				DataSizeA, DataSizeB, 100.0 * DataSizeB / (float)DataSizeA);
		if (DataSizeB < DataSizeA)
		{
			free(VGMData);
			VGMDataLen = DstDataLen;
			VGMData = DstData;
			DstDataLen = 0x00;
			DstData = NULL;
		}
		PassNo ++;
	} while(DataSizeB < DataSizeA);
	printf("Data Compression Total: %u -> %u (%.1f %%)\n",
			SrcDataSize, DataSizeB, 100.0 * DataSizeB / (float)SrcDataSize);

	if (DataSizeB < SrcDataSize)*/
	{
		if (argc > argbase + 1)
			strcpy(FileName, argv[argbase + 1]);
		else
			strcpy(FileName, "");
		if (FileName[0] == '\0')
		{
			strcpy(FileName, FileBase);
			strcat(FileName, "_mono.vgm");
		}
		WriteVGMFile(FileName);
	}

	free(VGMData);
	//free(DstData);

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

static void WriteVGMFile(const char* FileName)
{
	FILE* hFile;

	hFile = fopen(FileName, "wb");
	//fwrite(DstData, 0x01, DstDataLen, hFile);
	fwrite(VGMData, 0x01, VGMDataLen, hFile);
	fclose(hFile);

	printf("File written.\n");

	return;
}

static void CompressVGMData(void)
{
	UINT8 ChipID;
	UINT8 Command;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	//UINT32 DataStart;
	//UINT32 DataLen;
#ifdef WIN32
	UINT32 ROMSize;
	UINT32 CmdTimer;
	char TempStr[0x80];
	char MinSecStr[0x80];
#endif
	UINT32 CmdLen;
	bool StopVGM;
	UINT8* VGMPnt;
	UINT32 ChnBoth[0x20];
	UINT8 MPCMAddr;
	UINT8 MPCMSlot;
	UINT16 MPCMBank;
	UINT32 LastMBnkWrtOfs;
	UINT8 X1Channel;
	UINT8 X1Flags[0x10];
	UINT8* X1VolWrites[0x10];	// need to cache because sample volume register has another purpose in wavetable mode
	UINT8 RFChannel[2];

	//DstData = (UINT8*)malloc(VGMDataLen + 0x100);
	VGMPos = VGMHead.lngDataOffset;
	//DstPos = VGMHead.lngDataOffset;
	VGMSmplPos = 0;
	//memcpy(DstData, VGMData, VGMPos);	// Copy Header

#ifdef WIN32
	CmdTimer = 0;
#endif
	//InitAllChips();
	/*if (VGMHead.lngHzK054539)
	{
		SetChipSet(0x00);
		k054539_write(0xFF, 0x00, VGMHead.bytK054539Flags);
		if (VGMHead.lngHzK054539 & 0x40000000)
		{
			SetChipSet(0x01);
			k054539_write(0xFF, 0x00, VGMHead.bytK054539Flags);
		}
	}*/
	/*if (VGMHead.lngHzC140)
	{
		SetChipSet(0x00);
		c140_write(0xFF, 0x00, VGMHead.bytC140Type);
		if (VGMHead.lngHzC140 & 0x40000000)
		{
			SetChipSet(0x01);
			c140_write(0xFF, 0x00, VGMHead.bytC140Type);
		}
	}*/

	memset(ChnBoth, 0x00, sizeof(UINT32) * 0x20);
	LastMBnkWrtOfs = 0x00;
	MPCMBank = MPCMAddr = MPCMSlot = 0x00;

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
			case 0xAE:
			case 0xAF:
				if (VGMHead.lngHzYMF262 & 0x40000000)
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
			//SetChipSet(ChipID);

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
			case 0x50:	// SN76496 write (is mono)
				CmdLen = 0x02;
				break;
			case 0x51:	// YM2413 write (is mono)
				CmdLen = 0x03;
				break;
			case 0x52:	// YM2612 write port 0
			case 0x53:	// YM2612 write port 1
				TempByt = Command & 0x01;
				//WriteEvent = ym2612_write(TempByt, VGMPnt[0x01], VGMPnt[0x02]);

				TempSht = (TempByt << 8) | (VGMPnt[0x01] << 0);
				if ((TempSht & 0xFC) == 0xB4)	// FM Stereo
				{
					TempByt = ((TempSht >> 8) * 3) + (TempSht & 0x03);
					if ((VGMPnt[0x02] & 0xC0) == 0xC0)
					{
						ChnBoth[TempByt] = VGMPos;
					}
					else
					{
						ChnBoth[TempByt] = 0x00;
						if ((VGMPnt[0x02] & 0xC0))
							VGMPnt[0x02] |= 0xC0;
					}
				}
				else if (TempSht == 0x028)
				{
					if (VGMPnt[0x02] & 0xF0)
					{
						TempByt = (((VGMPnt[0x02] & 0x04) >> 2) * 3) + (VGMPnt[0x02] & 0x03);
						if (ChnBoth[TempByt])
						{
							printf("Warning: Stereo on both channels! Pos: %06X (Key Cmd: %06X)\n", ChnBoth[TempByt], VGMPos);
							ChnBoth[TempByt] = 0x00;
						}
					}
				}
				CmdLen = 0x03;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMPnt[0x02];
				memcpy(&TempLng, &VGMPnt[0x03], 0x04);

				ChipID = (TempLng & 0x80000000) >> 31;
				TempLng &= 0x7FFFFFFF;
				/*SetChipSet(ChipID);

				switch(TempByt & 0xC0)
				{
				case 0x00:	// Database Block
				case 0x40:	// Comrpessed Database Block
					DataLen = TempLng;
					break;
				case 0x80:	// ROM/RAM Dump
					memcpy(&ROMSize, &VGMPnt[0x07], 0x04);
					memcpy(&DataStart, &VGMPnt[0x0B], 0x04);
					DataLen = TempLng - 0x08;
					break;
				case 0xC0:	// RAM Write
					memcpy(&TempSht, &VGMPnt[0x07], 0x02);
					DataLen = TempLng - 0x02;
					break;
				}*/

				CmdLen = 0x07 + TempLng;
				break;
			case 0xE0:	// Seek to PCM Data Bank Pos
				memcpy(&TempLng, &VGMPnt[0x01], 0x04);
				CmdLen = 0x05;
				break;
			case 0x4F:	// GG Stereo
				//WriteEvent = GGStereo(VGMPnt[0x01]);
				CmdLen = 0x02;
				break;
			case 0x54:	// YM2151 write
				TempSht = VGMPnt[0x01];
				if ((TempSht & 0xF8) == 0x20)
				{
					TempByt = TempSht & 0x07;

					if ((VGMPnt[0x02] & 0xC0) == 0xC0)
					{
						ChnBoth[TempByt] = VGMPos;
					}
					else
					{
						ChnBoth[TempByt] = 0x00;
						if ((VGMPnt[0x02] & 0xC0))
							VGMPnt[0x02] |= 0xC0;
					}
				}
				else if (TempSht == 0x08)
				{
					if (VGMPnt[0x02] & 0x78)
					{
						TempByt = VGMPnt[0x02] & 0x07;
						if (ChnBoth[TempByt])
						{
							printf("Warning: Stereo on both channels! Pos: %06X (Key Cmd: %06X)\n", ChnBoth[TempByt], VGMPos);
							ChnBoth[TempByt] = 0x00;
						}
					}
				}
				//WriteEvent = ym2151_write(VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xC0:	// Sega PCM memory write
				memcpy(&TempSht, &VGMPnt[0x01], 0x02);
				//WriteEvent = segapcm_mem_write(TempSht, VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0xB0:	// RF5C68 register write
				//WriteEvent = rf5c68_reg_write(VGMPnt[0x01], VGMPnt[0x02]);
				if (VGMPnt[0x01] == 0x01)
				{
					TempByt = 0x10 | RFChannel[0];

					if (VGMPnt[0x02] != 0x0F && VGMPnt[0x02] != 0xF0)
					{
						ChnBoth[TempByt] = VGMPos;
						printf("Warning: Stereo on both channels! Pos: %06X (Key Cmd: %06X)\n", ChnBoth[TempByt], VGMPos);
					}
					else
					{
						ChnBoth[TempByt] = 0x00;
						VGMPnt[0x02] = 0xBB;
					}
				}
				else if (VGMPnt[0x01] == 0x07)
				{
					if (VGMPnt[0x02] & 0x40)
						RFChannel[0] = VGMPnt[0x02] & 0x07;
				}
				else if (VGMPnt[0x01] == 0x08)
				{
					/*if (VGMPnt[0x02] & 0x78)
					{
						TempByt = VGMPnt[0x02] & 0x07;
						if (ChnBoth[TempByt])
						{
							printf("Warning: Stereo on both channels! Pos: %06X (Key Cmd: %06X)\n", ChnBoth[TempByt], VGMPos);
							ChnBoth[TempByt] = 0x00;
						}
					}*/
				}
				CmdLen = 0x03;
				break;
			case 0xC1:	// RF5C68 memory write
				//memcpy(&TempSht, &VGMPnt[0x01], 0x02);
				//WriteEvent = rf5c68_mem_write(TempSht, VGMPnt[0x03]);
				//WriteEvent = true;	// OptVgmRF works a lot better this way
				CmdLen = 0x04;
				break;
			case 0x55:	// YM2203 (is mono)
				CmdLen = 0x03;
				break;
			case 0x56:	// YM2608 write port 0
			case 0x57:	// YM2608 write port 1
				TempByt = Command & 0x01;
				//WriteEvent = ym2608_write(TempByt, VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0x58:	// YM2610 write port 0
			case 0x59:	// YM2610 write port 1
				TempByt = Command & 0x01;
				//WriteEvent = ym2610_write(TempByt, VGMPnt[0x01], VGMPnt[0x02]);

				TempSht = (TempByt << 8) | (VGMPnt[0x01] << 0);
				//		FM Stereo			or		ADPCM Stereo		or		Delta-T Stereo
				if ((TempSht & 0xFC) == 0xB4 || (TempSht & 0x1F8) == 0x108 || TempSht == 0x011)
				{
					if ((TempSht & 0xF0) >= 0x30)
						TempByt = ((TempSht >> 8) * 3) + (TempSht & 0x03);
					else if (TempSht < 0x20)
						TempByt = 12;
					else
						TempByt = 6 + (TempSht & 0x07);

					if ((VGMPnt[0x02] & 0xC0) == 0xC0)
					{
						ChnBoth[TempByt] = VGMPos;
					}
					else
					{
						ChnBoth[TempByt] = 0x00;
						if ((VGMPnt[0x02] & 0xC0))
							VGMPnt[0x02] |= 0xC0;
					}
				}
				else if (TempSht == 0x028)
				{
					if (VGMPnt[0x02] & 0xF0)
					{
						TempByt = (((VGMPnt[0x02] & 0x04) >> 2) * 3) + (VGMPnt[0x02] & 0x03);
						if (ChnBoth[TempByt])
						{
							printf("Warning: Stereo on both channels! Pos: %06X (Key Cmd: %06X)\n", ChnBoth[TempByt], VGMPos);
							ChnBoth[TempByt] = 0x00;
						}
					}
				}
				else if (TempSht == 0x010)
				{
					if (VGMPnt[0x02] & 0x80)
					{
						TempByt = 12;
						if (ChnBoth[TempByt])
						{
							printf("Warning: Stereo on both channels! Pos: %06X (Key Cmd: %06X)\n", ChnBoth[TempByt], VGMPos);
							ChnBoth[TempByt] = 0x00;
						}
					}
				}
				else if (TempSht == 0x100)
				{
					if (! (VGMPnt[0x02] & 0x80))
					{
						for (TempByt = 6; TempByt < 12; TempByt ++)
						{
							if (VGMPnt[0x02] & (1 << (TempByt - 6)) && ChnBoth[TempByt])
							{
								printf("Warning: Stereo on both channels! Pos: %06X (Key Cmd: %06X)\n", ChnBoth[TempByt], VGMPos);
								ChnBoth[TempByt] = 0x00;
							}
						}
					}
				}
				CmdLen = 0x03;
				break;
			case 0x5A:	// YM3812 write (is mono)
			case 0x5B:	// YM3526 write (is mono)
			case 0x5C:	// Y8950 write (is mono)
				CmdLen = 0x03;
				break;
			case 0x5E:	// YMF262 write port 0
			case 0x5F:	// YMF262 write port 1
				TempByt = Command & 0x01;
				//WriteEvent = ymf262_write(TempByt, VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0x5D:	// YMZ280B write
				//WriteEvent = ymz280b_write(VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xD0:	// YMF278B write
				TempByt = VGMPnt[0x01];
				if (TempByt <= 0x01)
				{
					//WriteEvent = ymf262_write(TempByt, VGMPnt[0x02], VGMPnt[0x03]);
				}
				else
				{
					if (VGMPnt[0x02] >= 0x08 && VGMPnt[0x02] < 0xF8)
					{
						TempByt = VGMPnt[0x02] - 0x08;
						if ((TempByt / 24) == 0x04)
						{
							TempByt = VGMPnt[0x03] & 0x0F;
							if (TempByt != 0x08)	// 08 = muted - don't change
							{
								VGMPnt[0x03] &= ~0x0F;
								if (TempByt > 0x00 && ! (TempByt == 0x07 || TempByt == 0x09))
								printf("Warning: Non-Full Panning! Pos: %06X\n", VGMPos);
							}
						}
					}
				}
				CmdLen = 0x04;
				break;
			case 0xD1:	// YMF271 write
				//WriteEvent = ymf271_write(VGMPnt[0x01], VGMPnt[0x02], VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0xB1:	// RF5C164 register write
				//WriteEvent = rf5c164_reg_write(VGMPnt[0x01], VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xC2:	// RF5C164 memory write
				//WriteEvent = true;	// OptVgmRF works a lot better this way
				CmdLen = 0x04;
				break;
			case 0x68:	// PCM RAM write
				CmdLen = 0x0C;
				break;
			case 0xA0:	// AY8910 register write (is mono)
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
			case 0xB3:	// GameBoy DMG write
				//SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				//WriteEvent = gameboy_write_reg(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xB4:	// NES APU write (is mono)
				CmdLen = 0x03;
				break;
			case 0xB5:	// MultiPCM write
				//SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
			//	WriteEvent = multipcm_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				TempByt = VGMPnt[0x01] & 0x7F;
				switch(TempByt)
				{
				case 0x00:
					if (MPCMAddr == 0x00)
					{
						if ((VGMPnt[0x02] & 0xF0) != 0x80)
						{
							//if ((VGMPnt[0x02] & 0xF0) < 0x80)
							//	printf("MultiPCM: Right Channel used!\n");
							VGMPnt[0x02] &= ~0xF0;
						}
					}
					break;
				case 0x01:
					MPCMSlot = (VGMPnt[0x02] & 0x07) + ((VGMPnt[0x02] >> 3) * 7);
					break;
				case 0x02:
					MPCMAddr = (VGMPnt[0x02] > 7) ? 7 : VGMPnt[0x02];
					break;
				}
				CmdLen = 0x03;
				break;
			case 0xC3:	// MultiPCM memory write
				//SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				memcpy(&TempSht, &VGMPnt[0x02], 0x02);
			//	WriteEvent = multipcm_bank_write(VGMPnt[0x01] & 0x7F, TempSht);
				//if (VGMPnt[0x01] & 0x03)
				//	VGMPnt[0x01] |= 0x03;
				if (! TempSht && MPCMBank)
				{
					memcpy(&VGMPnt[0x02], &MPCMBank, 0x02);
				}
				else
				{
					MPCMBank = TempSht;
					/*if (LastMBnkWrtOfs)
					{
						VGMPnt = &VGMData[LastMBnkWrtOfs];
						if (VGMPnt[0x01] == 0x01)
							memcpy(&VGMPnt[0x02], &MPCMBank, 0x02);
					}*/
				}
				LastMBnkWrtOfs = VGMPos;
				CmdLen = 0x04;
				break;
			case 0xB6:	// UPD7759 write (is mono?)
				//SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
			//	WriteEvent = upd7759_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xB7:	// OKIM6258 write
				//SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
			//	WriteEvent = okim6258_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xB8:	// OKIM6295 write
				//SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
			//	WriteEvent = okim6295_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xD2:	// SCC1 write
				//SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
			//	WriteEvent = k051649_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02], VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0xD3:	// K054539 write
				//SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
			//	WriteEvent = k054539_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02], VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0xB9:	// HuC6280 write
				//SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				//WriteEvent = c6280_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xD4:	// C140 write
				//SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
				//WriteEvent = c140_write(VGMPnt[0x01] & 0x7F, VGMPnt[0x02], VGMPnt[0x03]);
				CmdLen = 0x04;
				break;
			case 0xBA:	// K053260 write
				//SetChipSet((VGMPnt[0x01] & 0x80) >> 7);
			//	WriteEvent = chip_reg_write(0x1D, CurChip, 0x00, VGMPnt[0x01] & 0x7F, VGMPnt[0x02]);
				CmdLen = 0x03;
				break;
			case 0xBB:	// Pokey write (is mono)
				CmdLen = 0x03;
				break;
			case 0xC4:	// Q-Sound write
				//SetChipSet(0x00);
				//WriteEvent = qsound_write(VGMPnt[0x03], (VGMPnt[0x01] << 8) | (VGMPnt[0x02] << 0));
				CmdLen = 0x04;
				break;
case 0x41: // K007232 write (mono)
    // Channel 0 volume: reg 0x00 or 0x06 -- use left nibble for both
    if (VGMPnt[1] == 0x10 || VGMPnt[1] == 0x11) {
        UINT8 left = (VGMPnt[2] >> 4) & 0x0F;
        VGMPnt[2] = (left << 4) | left;
    }
    // Channel 1 volume: reg 0x01 or 0x07 -- use right nibble for both
    else if (VGMPnt[1] == 0x12 || VGMPnt[1] == 0x13) {
        UINT8 right = VGMPnt[2] & 0x0F;
        VGMPnt[2] = (right << 4) | right;
    }
    CmdLen = 0x03;
    break;
			case 0xc8:	// X1-010 write
				TempSht = VGMPnt[0x01]<<8 | VGMPnt[0x02]<<0;
				TempByt = VGMPnt[0x03];

				// register write
				if(TempSht < 0x80)
				{
					X1Channel = TempSht/8;
					switch(TempSht%8)
					{
						case 0:		// flag write
							X1Flags[X1Channel] = TempByt;
							// sample key on - modify last volume command
							if((TempByt&3) == 1)
							{
								if(X1VolWrites[X1Channel])
								{
									TempByt = *(X1VolWrites[X1Channel]);
									TempByt = (TempByt>>4) > (TempByt&0x0f) ? ((TempByt&0xf0) | (TempByt>>4)) : ((TempByt&0x0f) | (TempByt<<4));
									*(X1VolWrites[X1Channel]) = TempByt;
									X1VolWrites[X1Channel] = 0;
								}
							}
							break;
						case 1:		// volume write
							// in wavetable mode, this register selects the wavetable so we have to check what mode we're in before writing
							if( (X1Flags[X1Channel] & 1) == 0) // key off = remember last volume command
								X1VolWrites[X1Channel] = VGMPnt+3;
							if( (X1Flags[X1Channel] & 3) == 1) // sample key on = modify volume command
							{
								TempByt = (TempByt>>4) > (TempByt&0x0f) ? ((TempByt&0xf0) | (TempByt>>4)) : ((TempByt&0x0f) | (TempByt<<4));
								VGMPnt[0x03] = TempByt;
							}
							break;
						default:
							break;
					}
				}
				// envelope write
				else if (TempSht < 0x1000)
				{
					TempByt = (TempByt>>4) > (TempByt&0x0f) ? ((TempByt&0xf0) | (TempByt>>4)) : ((TempByt&0x0f) | (TempByt<<4));
					VGMPnt[0x03] = TempByt;
				}

				CmdLen = 0x04;
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
