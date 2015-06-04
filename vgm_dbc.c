// vgm_dbc.c - VGM Data Block Compressor
//
// TODO: 2xChip support (especially for data blocks)

#include <stdio.h>
#include <stdlib.h>
#include "stdbool.h"
#include <string.h>

#ifdef WIN32
#include <conio.h>
#include <windows.h>	// for GetTickCount
#endif

#include "zlib.h"

#include "stdtype.h"
#include "VGMFile.h"


typedef struct vgm_data_block
{
	UINT8 Type;
	UINT32 Size;
	void* Data;
} VGM_DATA_BLK;
typedef struct compression_table_data
{
	UINT8 ComprType;	// n-Bit Compression Sub Type
	bool TblToWrite;
	UINT8 BitDecomp;
	UINT8 EntrySize;
	UINT8 BitComp;
	UINT16 AddVal;
	UINT32 EntryCount;
	void* Entries;	// actual Table Entry Pointer
	void* EntRef;	// Table Reference
} COMPR_TBL_DATA;
typedef struct compression_table
{
	UINT32 BankCount;
	COMPR_TBL_DATA* BankTbl;
} COMPRESS_TBL;


static bool OpenVGMFile(const char* FileName);
static void WriteVGMFile(const char* FileName);
static void CompressVGMDataBlocks(void);
#ifdef WIN32
static void PrintMinSec(const UINT32 SamplePos, char* TempStr);
#endif
static void MakeCompressionTable(void);
static void DecideCompressionType(COMPR_TBL_DATA* ComprTbl);
static bool CombineTables(COMPR_TBL_DATA* BaseTbl, COMPR_TBL_DATA* NewTbl);
static UINT32 WriteCompressionTable(COMPR_TBL_DATA* ComprTbl, UINT8* Data);
static void WriteBits(UINT8* Data, UINT32* Pos, UINT8* BitPos, UINT16 Value, UINT8 BitsToWrite);
static UINT32 CompressAndWriteData(COMPR_TBL_DATA* ComprTbl, VGM_PCM_DATA* Bank, UINT8* Data);


VGM_HEADER VGMHead;
UINT32 VGMDataLen;
UINT8* VGMData;
UINT32 VGMPos;
INT32 VGMSmplPos;
UINT32 DstDataLen;
UINT8* DstData;
char FileBase[0x100];
VGM_PCM_BANK PCMBank[0x40];
COMPRESS_TBL CmpTbl[0x40];

int main(int argc, char* argv[])
{
	int ErrVal;
	char FileName[0x100];
	UINT8 CurPCM;
	UINT8 CurTbl;
	
	printf("VGM Data Block Compressor\n-------------------------\n\n");
	
	ErrVal = 0;
	printf("File Name:\t");
	if (argc <= 0x01)
	{
		gets(FileName);
	}
	else
	{
		strcpy(FileName, argv[0x01]);
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
	
	CompressVGMDataBlocks();
	
	if (DstDataLen < VGMDataLen)
	{
		if (argc > 0x02)
			strcpy(FileName, argv[0x02]);
		else
			strcpy(FileName, "");
		if (! FileName[0x00])
		{
			strcpy(FileName, FileBase);
			strcat(FileName, "_compressed.vgm");
		}
		WriteVGMFile(FileName);
	}
	else
	{
		printf("No compression possible.\n");
	}
	
	free(VGMData);
	free(DstData);
	for (CurPCM = 0x00; CurPCM < 0x40; CurPCM ++)
	{
		if (! CmpTbl[CurPCM].BankCount)
			continue;
		
		for (CurTbl = 0x00; CurTbl < CmpTbl[CurPCM].BankCount; CurTbl ++)
			free(CmpTbl[CurPCM].BankTbl[CurTbl].Entries);
		free(CmpTbl[CurPCM].BankTbl);
	}
	
EndProgram:
#ifdef WIN32
	if (argv[0][1] == ':')
	{
		// Executed by Double-Clicking (or Drap and Drop)
		if (_kbhit())
			_getch();
		_getch();
	}
#endif
	
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

static void CompressVGMDataBlocks(void)
{
	UINT8 PassNum;
	UINT32 DstPos;
	UINT8 Command;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
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
	VGM_DATA_BLK DataBlk;
	VGM_PCM_BANK* TempBnk;
	COMPR_TBL_DATA* TempTbl;
	
	DstData = (UINT8*)malloc(VGMDataLen + 0x1000);
	memcpy(DstData, VGMData, VGMHead.lngDataOffset);	// Copy Header
	
	for (TempByt = 0x00; TempByt < 0x40; TempByt ++)
	{
		PCMBank[TempByt].BankCount = 0x00;
		PCMBank[TempByt].Bank = NULL;
	}
	
	for (PassNum = 0x00; PassNum < 0x02; PassNum ++)
	{
		// Pass #1: Enumerate all PCM Data
		// Pass #2: Save compressed Data
		if (PassNum)
		{
			MakeCompressionTable();
			for (TempByt = 0x00; TempByt < 0x40; TempByt ++)
			{
				PCMBank[TempByt].BankCount = 0x00;
			}
		}
		
		VGMPos = VGMHead.lngDataOffset;
		DstPos = VGMHead.lngDataOffset;
		NewLoopS = 0x00;
#ifdef WIN32
		CmdTimer = 0;
#endif
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
					DataBlk.Type = VGMData[VGMPos + 0x02];
					memcpy(&DataBlk.Size, &VGMData[VGMPos + 0x03], 0x04);
					DataBlk.Size &= 0x7FFFFFFF;
					DataBlk.Data = &VGMData[VGMPos + 0x07];
					
					switch(DataBlk.Type & 0xC0)
					{
					case 0x00:	// Database Block
						TempBnk = &PCMBank[DataBlk.Type];
						if (! PassNum)
						{
							TempLng = TempBnk->BankCount;
							TempBnk->BankCount ++;
							
							TempBnk->Bank = (VGM_PCM_DATA*)realloc(TempBnk->Bank,
											TempBnk->BankCount * sizeof(VGM_PCM_DATA));
							TempBnk->Bank[TempLng].DataSize = DataBlk.Size;
							TempBnk->Bank[TempLng].Data = DataBlk.Data;
						}
						else
						{
							TempLng = TempBnk->BankCount;
							TempBnk->BankCount ++;
							TempTbl = &CmpTbl[DataBlk.Type].BankTbl[TempLng];
							
							if (TempTbl->ComprType == 0xFF)
								break;	// no compression possible
							
							if (TempTbl->TblToWrite)
							{
								TempTbl->TblToWrite = false;
								if (TempTbl->ComprType == 0x02)
								{
									DstData[DstPos + 0x00] = 0x67;
									DstData[DstPos + 0x01] = 0x66;
									DstData[DstPos + 0x02] = 0x7F;
									DataLen = WriteCompressionTable(TempTbl, &DstData[DstPos + 0x07]);
									memcpy(&DstData[DstPos + 0x03], &DataLen, 0x04);
									DstPos += 0x07 + DataLen;
								}
							}
							
							DstData[DstPos + 0x00] = 0x67;
							DstData[DstPos + 0x01] = 0x66;
							DstData[DstPos + 0x02] = 0x40 | DataBlk.Type;
							DataLen = CompressAndWriteData(TempTbl, &TempBnk->Bank[TempLng],
															&DstData[DstPos + 0x07]);
							memcpy(&DstData[DstPos + 0x03], &DataLen, 0x04);
							DstPos += 0x07 + DataLen;
							WriteEvent = false;
						}
						break;
					case 0x40:	// Compressed Database Block
						break;
					case 0x80:	// ROM/RAM Dump
						break;
					case 0xC0:	// RAM Write
						break;
					}
					CmdLen = 0x07 + DataBlk.Size;
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
						printf("Unknown Command: %hX\n", Command);
						CmdLen = 0x01;
						//StopVGM = true;
						break;
					}
					break;
				}
			}
			
			if (PassNum)
			{
				if (VGMPos == VGMHead.lngLoopOffset)
					NewLoopS = DstPos;
				if (WriteEvent)
				{
					memcpy(&DstData[DstPos], &VGMData[VGMPos], CmdLen);
					DstPos += CmdLen;
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
				DataLen = VGMHead.lngEOFOffset - VGMHead.lngDataOffset;
				printf("Pass #1: %04.3f %% - %s / %s (%08lX / %08lX) ...\r", (float)TempLng / DataLen * 100,
						MinSecStr, TempStr, VGMPos, VGMHead.lngEOFOffset);
				CmdTimer = GetTickCount() + 200;
			}
#endif
		}
	}
	if (VGMHead.lngLoopOffset)
	{
		TempLng = NewLoopS - 0x1C;
		memcpy(&DstData[0x1C], &TempLng, 0x04);
	}
	printf("\t\t\t\t\t\t\t\t\t\r");
	
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
	
	return;
}

#ifdef WIN32
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

static void MakeCompressionTable(void)
{
	UINT8 CurPCM;
	UINT32 CurBnk;
	UINT32 CurPos;
	VGM_PCM_DATA* TempBnk;
	UINT32 ValCount;
	UINT8* ValueMask;
	UINT32 ValUsed;
	UINT32 CurEnt;
	UINT8* Ent1B;
	UINT16* Ent2B;
	COMPR_TBL_DATA* TempTbl;
	UINT8 BitDecomp;
	UINT8 EntrySize;
	COMPR_TBL_DATA* LastTbl;
	
	ValCount = 1 << 12;
	ValueMask = (UINT8*)malloc(ValCount * 0x01);
	for (CurPCM = 0x00; CurPCM < 0x40; CurPCM ++)
	{
		if (! PCMBank[CurPCM].BankCount)
			continue;
		
		switch(CurPCM)
		{
		case 0x00:
		case 0x01:
		case 0x02:
			BitDecomp = 8;
			break;
		case 0x03:
			BitDecomp = 12;
			break;
		default:
			BitDecomp = 8;
			break;
		}
		EntrySize = (BitDecomp + 7) / 8;
		
		ValCount = 1 << BitDecomp;
		CmpTbl[CurPCM].BankCount = PCMBank[CurPCM].BankCount;
		CmpTbl[CurPCM].BankTbl = (COMPR_TBL_DATA*)malloc(CmpTbl[CurPCM].BankCount * sizeof(COMPR_TBL_DATA));
		LastTbl = NULL;
		for (CurBnk = 0x00; CurBnk < PCMBank[CurPCM].BankCount; CurBnk ++)
		{
			TempBnk = &PCMBank[CurPCM].Bank[CurBnk];
			TempTbl = &CmpTbl[CurPCM].BankTbl[CurBnk];
			TempTbl->TblToWrite = false;
			if (CurPCM == 0x3F)
			{
				// this block can't be compressed as type 7F is reserved for value tables
				TempTbl->ComprType = 0xFF;
				continue;
			}
			
			memset(ValueMask, 0x00, ValCount * 0x01);
			TempTbl->BitDecomp = BitDecomp;
			TempTbl->EntrySize = EntrySize;
			switch(TempTbl->EntrySize)
			{
			case 0x01:
				for (CurPos = 0x00; CurPos < TempBnk->DataSize; CurPos ++)
				{
					ValueMask[TempBnk->Data[CurPos]] = 0x01;
				}
				break;
			case 0x02:
				Ent2B = (UINT16*)TempBnk->Data;
				for (CurPos = 0x00; CurPos < TempBnk->DataSize; CurPos += 0x02, Ent2B ++)
				{
					ValueMask[*Ent2B] = 0x01;
				}
				break;
			}
			ValUsed = 0x00;
			for (CurPos = 0x00; CurPos < ValCount; CurPos ++)
			{
				if (ValueMask[CurPos])
					ValUsed ++;
			}
			TempTbl->EntryCount = ValUsed;
			TempTbl->Entries = malloc(ValUsed * TempTbl->EntrySize);
			
			switch(TempTbl->EntrySize)
			{
			case 0x01:
				CurEnt = 0x00;
				Ent1B = (UINT8*)TempTbl->Entries;
				for (CurPos = 0x00; CurPos < ValCount; CurPos ++)
				{
					if (ValueMask[CurPos])
					{
						Ent1B[CurEnt] = (UINT8)CurPos;
						CurEnt ++;
					}
				}
				break;
			case 0x02:
				CurEnt = 0x00;
				Ent2B = (UINT16*)TempTbl->Entries;
				for (CurPos = 0x00; CurPos < ValCount; CurPos ++)
				{
					if (ValueMask[CurPos])
					{
						Ent2B[CurEnt] = (UINT16)CurPos;
						CurEnt ++;
					}
				}
				break;
			}
			
			CurPos = ValUsed - 1;
			CurEnt = 0x00;
			while(CurPos)
			{
				CurPos >>= 1;
				CurEnt ++;
			}
			TempTbl->BitComp = (UINT8)CurEnt;
			DecideCompressionType(TempTbl);
			
			if (TempTbl->ComprType != 0x02)
			{
				TempTbl->EntryCount = 0x00;
				free(TempTbl->Entries);
				TempTbl->Entries = NULL;
				TempTbl->EntRef = TempTbl->Entries;
			}
			else
			{
				TempTbl->TblToWrite = ! CombineTables(LastTbl, TempTbl);
				if (! TempTbl->TblToWrite)
				{
					// tables combined - use LastTbl Entries
					TempTbl->EntryCount = LastTbl->EntryCount;
					TempTbl->EntRef = LastTbl->Entries;
					free(TempTbl->Entries);
					TempTbl->Entries = NULL;
				}
				else
				{
					TempTbl->EntRef = TempTbl->Entries;
				}
				LastTbl = TempTbl;
			}
		}
	}
	
	return;
}

static void DecideCompressionType(COMPR_TBL_DATA* ComprTbl)
{
	UINT32 CurEnt;
	UINT16 MinVal;
	UINT16 MaxVal;
	UINT16 CurVal;
	UINT8* Ent1B;
	UINT16* Ent2B;
	UINT16 BitMask;
	
	// Compare compressed Bits with saved (not decompressed) size
	if (ComprTbl->BitComp >= ((ComprTbl->BitDecomp - 1) | 0x07) + 1)
	{
		ComprTbl->ComprType = 0xFF;
		return;
	}
	
	Ent1B = (UINT8*)ComprTbl->Entries;
	Ent2B = (UINT16*)ComprTbl->Entries;
	
	// Test 1 - Copy
	for (CurEnt = 0x00; CurEnt < ComprTbl->EntryCount; CurEnt ++)
	{
		switch(ComprTbl->EntrySize)
		{
		case 0x01:
			CurVal = Ent1B[CurEnt];
			break;
		case 0x02:
			CurVal = Ent2B[CurEnt];
			break;
		}
		if (! CurEnt)
		{
			MinVal = CurVal;
			MaxVal = CurVal;
		}
		else
		{
			if (CurVal < MinVal)
				MinVal = CurVal;
			else if (CurVal > MaxVal)
				MaxVal = CurVal;
		}
	}
	if ((MaxVal - MinVal) < (1 << ComprTbl->BitComp))
	{
		if (MaxVal < (1 << ComprTbl->BitComp))
			MinVal = 0x00;
		ComprTbl->ComprType = 0x00;
		ComprTbl->AddVal = MinVal;
		return;
	}
	
	// Test 2 - Shift Left
	BitMask = (1 << ComprTbl->BitComp) - 1;
	//BitMask = (1 << (ComprTbl->BitDecomp - ComprTbl->BitComp)) - 1;	// is this more right?
	for (CurEnt = 0x00; CurEnt < ComprTbl->EntryCount; CurEnt ++)
	{
		switch(ComprTbl->EntrySize)
		{
		case 0x01:
			CurVal = Ent1B[CurEnt];
			break;
		case 0x02:
			CurVal = Ent2B[CurEnt];
			break;
		}
		if (! CurEnt)
		{
			MinVal = CurVal & BitMask;
		}
		else
		{
			if (MinVal != (CurVal & BitMask))
			{
				BitMask = 0x0000;
				break;
			}
		}
	}
	if (BitMask)
	{
		ComprTbl->ComprType = 0x01;
		ComprTbl->AddVal = MinVal;
		return;
	}
	
	ComprTbl->ComprType = 0x02;
	ComprTbl->AddVal = 0x00;
	
	return;
}

static bool CombineTables(COMPR_TBL_DATA* BaseTbl, COMPR_TBL_DATA* NewTbl)
{
	UINT32 ValCount;
	UINT8* ValueMask;
	UINT32 ValUsed;
	UINT32 CurEnt;
	UINT8* Ent1B;
	UINT16* Ent2B;
	UINT32 CurPos;
	
	if (BaseTbl == NULL || BaseTbl->BitComp != NewTbl->BitComp)
		return false;	// can't combine tables
	
	ValCount = 1 << 12;
	ValueMask = (UINT8*)malloc(ValCount * 0x01);
	memset(ValueMask, 0x00, ValCount * 0x01);
	
	switch(BaseTbl->EntrySize)
	{
	case 0x01:
		Ent1B = (UINT8*)BaseTbl->Entries;
		for (CurEnt = 0x00; CurEnt < BaseTbl->EntryCount; CurEnt ++)
		{
			ValueMask[Ent1B[CurEnt]] = 0x01;
		}
		Ent1B = (UINT8*)NewTbl->Entries;
		for (CurEnt = 0x00; CurEnt < NewTbl->EntryCount; CurEnt ++)
		{
			ValueMask[Ent1B[CurEnt]] = 0x01;
		}
		break;
	case 0x02:
		Ent2B = (UINT16*)BaseTbl->Entries;
		for (CurEnt = 0x00; CurEnt < BaseTbl->EntryCount; CurEnt ++)
		{
			ValueMask[Ent2B[CurEnt]] = 0x01;
		}
		Ent2B = (UINT16*)NewTbl->Entries;
		for (CurEnt = 0x00; CurEnt < NewTbl->EntryCount; CurEnt ++)
		{
			ValueMask[Ent2B[CurEnt]] = 0x01;
		}
		break;
	}
	ValUsed = 0x00;
	for (CurEnt = 0x00; CurEnt < ValCount; CurEnt ++)
	{
		if (ValueMask[CurEnt])
			ValUsed ++;
	}
	
	CurPos = ValUsed - 1;
	CurEnt = 0x00;
	while(CurPos)
	{
		CurPos >>= 1;
		CurEnt ++;
	}
	if (CurEnt > BaseTbl->BitComp)
		return false;	// combining the tables would increase the value's bit-size
	
	// combine the two tables
	BaseTbl->EntryCount = ValUsed;
	BaseTbl->Entries = realloc(BaseTbl->Entries, ValUsed * BaseTbl->EntrySize);
	BaseTbl->EntRef = BaseTbl->Entries;
	switch(BaseTbl->EntrySize)
	{
	case 0x01:
		CurEnt = 0x00;
		Ent1B = (UINT8*)BaseTbl->Entries;
		for (CurPos = 0x00; CurPos < ValCount; CurPos ++)
		{
			if (ValueMask[CurPos])
			{
				Ent1B[CurEnt] = (UINT8)CurPos;
				CurEnt ++;
			}
		}
		break;
	case 0x02:
		CurEnt = 0x00;
		Ent2B = (UINT16*)BaseTbl->Entries;
		for (CurPos = 0x00; CurPos < ValCount; CurPos ++)
		{
			if (ValueMask[CurPos])
			{
				Ent2B[CurEnt] = (UINT16)CurPos;
				CurEnt ++;
			}
		}
		break;
	}
	
	return true;	// combined tables successfully
}

static UINT32 WriteCompressionTable(COMPR_TBL_DATA* ComprTbl, UINT8* Data)
{
	UINT16 TempSht;
	UINT32 DataLen;
	
	Data[0x00] = 0x00;	// n-Bit Compession
	Data[0x01] = ComprTbl->ComprType;
	Data[0x02] = ComprTbl->BitDecomp;
	Data[0x03] = ComprTbl->BitComp;
	
	TempSht = (UINT16)ComprTbl->EntryCount;
	DataLen = ComprTbl->EntryCount * ComprTbl->EntrySize;
	memcpy(&Data[0x04], &TempSht, 0x02);
	memcpy(&Data[0x06], ComprTbl->Entries, DataLen);
	
	return 0x06 + DataLen;
}

static void WriteBits(UINT8* Data, UINT32* Pos, UINT8* BitPos, UINT16 Value, UINT8 BitsToWrite)
{
	UINT8 BitWriteVal;
	UINT32 OutPos;
	UINT8 InVal;
	UINT8 BitMask;
	UINT8 OutShift;
	UINT8 InBit;
	
	OutPos = *Pos;
	OutShift = *BitPos;
	InBit = 0x00;
	
	Data[OutPos] &= ~(0xFF >> OutShift);
	while(BitsToWrite)
	{
		BitWriteVal = (BitsToWrite >= 8) ? 8 : BitsToWrite;
		BitsToWrite -= BitWriteVal;
		BitMask = (1 << BitWriteVal) - 1;
		
		InVal = (Value >> InBit) & BitMask;
		OutShift += BitWriteVal;
		Data[OutPos] |= InVal << 8 >> OutShift;
		if (OutShift >= 8)
		{
			OutShift -= 8;
			OutPos ++;
			Data[OutPos] = InVal << 8 >> OutShift;
		}
		
		InBit += BitWriteVal;
	}
	
	*Pos = OutPos;
	*BitPos = OutShift;
	return;
}

static UINT32 CompressAndWriteData(COMPR_TBL_DATA* ComprTbl, VGM_PCM_DATA* Bank, UINT8* Data)
{
	UINT32 CurPos;
	UINT8* DstBuf;
	UINT32 DstPos;
	UINT16 SrcVal;
	UINT16 DstVal;
	UINT32 CurEnt;
	UINT16 BitMask;
	UINT16 AddVal;
	UINT8 BitShift;
	UINT8* Ent1B;
	UINT16* Ent2B;
	UINT8 DstShift;
	
	Data[0x00] = 0x00;	// n-Bit Compession
	memcpy(&Data[0x01], &Bank->DataSize, 0x04);
	Data[0x05] = ComprTbl->BitDecomp;
	Data[0x06] = ComprTbl->BitComp;
	Data[0x07] = ComprTbl->ComprType;
	memcpy(&Data[0x08], &ComprTbl->AddVal, 0x02);
	DstBuf = &Data[0x0A];
	
	BitMask = (1 << ComprTbl->BitComp) - 1;
	AddVal = ComprTbl->AddVal;
	BitShift = ComprTbl->BitDecomp - ComprTbl->BitComp;
	Ent1B = (UINT8*)ComprTbl->EntRef;
	Ent2B = (UINT16*)ComprTbl->EntRef;
	
	SrcVal = 0x0000;	// must be initialized (else 8-bit values don't fill it completely)
	DstPos = 0x00;
	DstShift = 0;
	for (CurPos = 0x00; CurPos < Bank->DataSize; CurPos += ComprTbl->EntrySize)
	{
		memcpy(&SrcVal, &Bank->Data[CurPos], ComprTbl->EntrySize);
		switch(ComprTbl->ComprType)
		{
		case 0x00:	// Copy
			DstVal = SrcVal - AddVal;
			break;
		case 0x01:	// Shift Left
			DstVal = (SrcVal - AddVal) >> BitShift;
			break;
		case 0x02:	// Table
			switch(ComprTbl->EntrySize)
			{
			case 0x01:
				for (CurEnt = 0x00; CurEnt < ComprTbl->EntryCount; CurEnt ++)
				{
					if (SrcVal == Ent1B[CurEnt])
						break;
				}
				break;
			case 0x02:
				for (CurEnt = 0x00; CurEnt < ComprTbl->EntryCount; CurEnt ++)
				{
					if (SrcVal == Ent2B[CurEnt])
						break;
				}
				break;
			}
			DstVal = (UINT16)CurEnt;
			break;
		}
		
		WriteBits(DstBuf, &DstPos, &DstShift, DstVal, ComprTbl->BitComp);
	}
	if (DstShift)
		DstPos ++;
	
	return 0x0A + DstPos;
}
