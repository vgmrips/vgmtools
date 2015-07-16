// optvgmrf.c - VGM RF-PCM Optimizer
//

// TODO:
//	- implement UnOpt
//	- optimize output (smaller blocks, expand block writes)


#include "vgmtools.h"

static bool OpenVGMFile(const char* FileName);
static void WriteVGMFile(const char* FileName);
static void MergePCMData(void);
static UINT32 ReadConsecutiveMemWrites(UINT8 RFMode);
static void EnumeratePCMData(void);
static bool CompareData(UINT32 DataLen, const UINT8* DataA,
						const UINT8* DataB);
static void RewriteVGMData(void);
#ifdef WIN32
static void PrintMinSec(const UINT32 SamplePos, char* TempStr);
#endif


#define RF5C68_MODE		0x00
#define RF5C164_MODE	0x01
#define SCSP_MODE		0x02
#define RFDATA_BLOCKS	0x03


// sometimes the chip races with the memory writes, so I tried different methods
#define BP_FRONT	0x00	// works best (some single samples are still incorrect)
				// I should be able to get around that by simulating that stream
#define BP_BACK		0x01	// works worst (audible bugs)
#define BP_MIDDLE	0x02	// works average (some audible bugs)

#define BLOCK_POS	BP_FRONT


typedef struct rfpcm_block_data
{
	UINT8 Mode;
	UINT32 DataSize;
	UINT8* Data;
	UINT32 DBPos;	// Database Position
	UINT32 UsageCounter;
} RF_BLK_DATA;
typedef struct block_in_file_list
{
	UINT32 BlockID;
	UINT32 FilePos;
	UINT32 StartAddr;
	UINT32 DataSize;
} IN_FILE_LIST;
typedef struct rfpcm_ram_data
{
	UINT16 BankReg;
	bool FirstBnkWrt;
	UINT32 DataLen;
	UINT32 RAMStart;
	UINT32 RAMSkip;
	UINT32 RAMSize;
	UINT8* PCMRam;
} RF_CHIP_DATA;


VGM_HEADER VGMHead;
UINT32 VGMDataLen;
UINT8* VGMData;
UINT32 VGMPos;
INT32 VGMSmplPos;
UINT8* DstData;
UINT32 DstDataLen;
char FileBase[0x100];
UINT32 DataSizeA;
UINT32 DataSizeB;
bool CancelFlag;
UINT8 OptMode;

RF_CHIP_DATA RF_RData[RFDATA_BLOCKS];
UINT32 RFBlkAlloc;
UINT32 RFBlkCount;
RF_BLK_DATA* RFBlock;
UINT32 InFileAlloc;
UINT32 InFileCount;
IN_FILE_LIST* InFileList;

int main(int argc, char* argv[])
{
	int ErrVal;
	char FileName[0x100];
	
	printf("VGM RF-PCM Optimizer\n--------------------\n\n");
	
	ErrVal = 0;
	printf("File Name:\t");
	if (argc <= 0x01)
	{
		gets_s(FileName, sizeof(FileName));
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
	
	DstData = NULL;
	if (VGMHead.lngVersion < 0x00000151)
	{
		printf("VGM Version %X.%02X!? Are kidding me??\n",
				VGMHead.lngVersion >> 8, VGMHead.lngVersion & 0xFF);
		ErrVal = 2;
		goto BreakProgress;
	}
	if (! VGMHead.lngHzRF5C68 && ! VGMHead.lngHzRF5C164 && ! VGMHead.lngHzSCSP)
	{
		printf("No RF-PCM chips used!\n");
		ErrVal = 2;
		goto BreakProgress;
	}
	
	CancelFlag = false;
	OptMode = 0x00;
	printf("Step 1: Merge single memory writes into larger blocks ...\n");
	MergePCMData();
	if (CancelFlag)
		goto BreakProgress;
	switch(OptMode)
	{
	case 0x00:	// 1 PCM block - remove unused space
		// this feature was planned and scrapped, because it's easier to implement in vgm_sro
		printf("This tool can only optimize PCM streams and this vgm has no PCM stream.\n");
		printf("You should try the VGM Sample-ROM Optimizer for further compression.\n");
		break;
	case 0x01:	// PCM stream - optimize this way
		//break;
		// Swap Pointers (I love C for this)
		free(VGMData);
		VGMData = DstData;
		DstData = NULL;
		
		printf("Step 2: Generate Block Data ...\n");
		EnumeratePCMData();
		if (CancelFlag)
			goto BreakProgress;
		printf("Step 3: Rewrite VGM with PCM database ...\n");
		RewriteVGMData();
		break;
	}
	printf("Data Compression: %u -> %u (%.1f %%)\n",
			DataSizeA, DataSizeB, 100.0f * DataSizeB / DataSizeA);
	
	if (DataSizeB < DataSizeA)
	{
		if (argc > 0x02)
			strcpy(FileName, argv[0x02]);
		else
			strcpy(FileName, "");
		if (! FileName[0x00])
		{
			strcpy(FileName, FileBase);
			strcat(FileName, "_optimized.vgm");
		}
		WriteVGMFile(FileName);
	}
	
BreakProgress:
	free(VGMData);
	free(DstData);
	
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
	if (VGMHead.lngVersion < 0x00000151)
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

static void WritePCMDataBlk(UINT32* DstPos, const UINT8 BlkType, const UINT32 DataLen,
							const UINT32 DataStart)
{
	const UINT8 DBLK_TYPES[RFDATA_BLOCKS] = {0xC0, 0xC1, 0xE0};
	UINT32 DBlkLen;
	UINT8 DBlkType;
	const UINT8* Data;
	
	Data = RF_RData[BlkType].PCMRam + DataStart;
	DBlkType = DBLK_TYPES[BlkType];
	DstData[*DstPos + 0x00] = 0x67;
	DstData[*DstPos + 0x01] = 0x66;
	DstData[*DstPos + 0x02] = DBlkType;
	if (! (DBlkType & 0x20))
	{
		DBlkLen = DataLen + 0x02;
		memcpy(&DstData[*DstPos + 0x03], &DBlkLen, 0x04);
		
		memcpy(&DstData[*DstPos + 0x07], &DataStart, 0x02);
		memcpy(&DstData[*DstPos + 0x09], Data, DataLen);
		*DstPos += 0x07 + DBlkLen;
	}
	else
	{
		DBlkLen = DataLen + 0x04;
		memcpy(&DstData[*DstPos + 0x03], &DBlkLen, 0x04);
		
		memcpy(&DstData[*DstPos + 0x07], &DataStart, 0x04);
		memcpy(&DstData[*DstPos + 0x0B], Data, DataLen);
		*DstPos += 0x09 + DBlkLen;
	}
	
	return;
}

static void MergePCMData(void)
{
	UINT32 DstPos;
	UINT8 Command;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	UINT32 BlockLen;
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
	RF_CHIP_DATA* TempRFD;
	UINT8 WarningFlags;
	UINT32 WriteLen;
	UINT32 BlkFound;
	UINT8 BlkType;
	
	DstData = (UINT8*)malloc(VGMDataLen + 0x100);
	VGMPos = VGMHead.lngDataOffset;
	DstPos = VGMHead.lngDataOffset;
	VGMSmplPos = 0;
	NewLoopS = 0x00;
	memcpy(DstData, VGMData, VGMPos);	// Copy Header
	
	memset(RF_RData, 0x00, sizeof(RF_CHIP_DATA) * RFDATA_BLOCKS);
	RF_RData[RF5C68_MODE].RAMSize	= 0x10000;	// 64 KB
	RF_RData[RF5C164_MODE].RAMSize	= 0x10000;	// 64 KB
	RF_RData[SCSP_MODE].RAMSize		= 0x80000;	// 512 KB
	for (TempByt = 0x00; TempByt < RFDATA_BLOCKS; TempByt ++)
	{
		RF_RData[TempByt].PCMRam = (UINT8*)malloc(RF_RData[TempByt].RAMSize);
		RF_RData[TempByt].FirstBnkWrt = true;
	}
	
#ifdef WIN32
	CmdTimer = 0;
#endif
	WarningFlags = 0x00;
	StopVGM = false;
	BlkFound = 0x00;
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
			case 0x4F:	// GG Stereo
				CmdLen = 0x02;
				break;
			case 0x51:	// YM2413 write
			case 0x52:	// YM2612 write port 0
			case 0x53:	// YM2612 write port 1
			case 0x54:	// YM2151 write
			case 0x55:	// YM2203
			case 0x56:	// YM2608 write port 0
			case 0x57:	// YM2608 write port 1
			case 0x58:	// YM2610 write port 0
			case 0x59:	// YM2610 write port 1
			case 0x5A:	// YM3812 write
			case 0x5B:	// YM3526 write
			case 0x5C:	// Y8950 write
			case 0x5E:	// YMF262 write port 0
			case 0x5F:	// YMF262 write port 1
			case 0x5D:	// YMZ280B write
				CmdLen = 0x03;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMData[VGMPos + 0x02];
				memcpy(&BlockLen, &VGMData[VGMPos + 0x03], 0x04);
				BlockLen &= 0x7FFFFFFF;
				
				CmdLen = 0x07 + BlockLen;
				
				if ((TempByt & 0xC0) != 0xC0)
					break;
				
				if (TempByt == 0xC0)
					BlkType = RF5C68_MODE;
				else if (TempByt == 0xC1)
					BlkType = RF5C164_MODE;
				else if (TempByt == 0xE0)
					BlkType = SCSP_MODE;
				else
					break;
				BlkFound ++;
				TempRFD = &RF_RData[BlkType];
				if (! TempRFD->RAMSkip)
				{
					TempRFD->DataLen = ReadConsecutiveMemWrites(BlkType);
					if (TempRFD->DataLen > 0x01)
					{
						TempRFD->RAMSkip = TempRFD->DataLen;
						memcpy(&TempSht, &VGMData[VGMPos + 0x07], 0x02);
						TempSht += TempRFD->BankReg;
						TempRFD->RAMStart = TempSht;
						
						if (BLOCK_POS == BP_FRONT)
						{
							while(TempRFD->DataLen)
							{
								if (TempRFD->DataLen > 0x1000)
									WriteLen = 0x1000;
								else
									WriteLen = TempRFD->DataLen;
								
								WritePCMDataBlk(&DstPos, BlkType, WriteLen, TempSht);
								TempSht += (UINT16)WriteLen;
								TempRFD->DataLen -= WriteLen;
							}
						}
					}
				}
				if (TempRFD->RAMSkip)
				{
					DataLen = BlockLen - 0x02;
					
					if ((BLOCK_POS == BP_BACK && ! TempRFD->RAMSkip) ||
						(BLOCK_POS == BP_MIDDLE &&
							(TempRFD->RAMSkip >= TempRFD->DataLen / 2 &&
							TempRFD->RAMSkip - DataLen < TempRFD->DataLen / 2)))
					{
						TempSht = TempRFD->RAMStart;
						while(TempRFD->DataLen)
						{
							if (TempRFD->DataLen > 0x1000)
								WriteLen = 0x1000;
							else
								WriteLen = TempRFD->DataLen;
							
							WritePCMDataBlk(&DstPos, BlkType, WriteLen, TempSht);
							TempSht += (UINT16)WriteLen;
							TempRFD->DataLen -= WriteLen;
						}
					}
					
					WriteEvent = false;
					TempRFD->RAMSkip -= DataLen;
				}
				break;
			case 0xE0:	// Seek to PCM Data Bank Pos
				CmdLen = 0x05;
				break;
			case 0xC0:	// Sega PCM memory write
				CmdLen = 0x04;
				break;
			case 0xB0:	// RF5C68 register write
			case 0xB1:	// RF5C164 register write
				CmdLen = 0x03;
				
				if (VGMData[VGMPos + 0x01] == 0x07 && ! (VGMData[VGMPos + 0x02] & 0x40))
				{
					if (Command == 0xB0)
						TempRFD = &RF_RData[RF5C68_MODE];
					else if (Command == 0xB1)
						TempRFD = &RF_RData[RF5C164_MODE];
					else
						TempRFD = NULL;
					
					// Bank Select
					TempRFD->BankReg = (VGMData[VGMPos + 0x02] & 0x0F) << 12;
				
					// write Bank Select just one time, the memory write includes all
					// neccessary data
					if (TempRFD->FirstBnkWrt)
					{
						VGMData[VGMPos + 0x02] &= ~0x0F;
						WriteEvent = true;
						TempRFD->FirstBnkWrt = false;
					}
					else
					{
						WriteEvent = false;
					}
				}
				break;
			case 0xC1:	// RF5C68 memory write
			case 0xC2:	// RF5C164 memory write
				CmdLen = 0x04;
				
				if (Command == 0xC1)
					BlkType = RF5C68_MODE;
				else if (Command == 0xC2)
					BlkType = RF5C164_MODE;
				/*else
				{
					BlkType = 0xFF;
					TempRFD = NULL;
				}*/
				TempRFD = &RF_RData[BlkType];
				BlkFound ++;
				if (! TempRFD->RAMSkip)
				{
					TempRFD->DataLen = ReadConsecutiveMemWrites(BlkType);
					if (TempRFD->DataLen > 0x01)
					{
						TempRFD->RAMSkip = TempRFD->DataLen;
						memcpy(&TempSht, &VGMData[VGMPos + 0x01], 0x02);
						TempRFD->RAMStart = TempRFD->BankReg + TempSht;
						
						if (BLOCK_POS == BP_FRONT)
						{
							WritePCMDataBlk(&DstPos, BlkType, TempRFD->DataLen,
											TempRFD->RAMStart);
						}
					}
					else if (! (WarningFlags & 0x01))
					{
						WarningFlags |= 0x01;
						printf("\t\t\t\t\t\t\t\t\r");
#ifdef WIN32
						CmdTimer = 0;
#endif
						printf("Warning! Single Memory Writes left!\n");
					}
				}
				if (TempRFD->RAMSkip)
				{
					if ((BLOCK_POS == BP_BACK && ! TempRFD->RAMSkip) ||
						(BLOCK_POS == BP_MIDDLE && TempRFD->RAMSkip * 2 == TempRFD->DataLen))
					{
						WritePCMDataBlk(&DstPos, BlkType, TempRFD->DataLen,
										TempRFD->RAMStart);
					}
					
					WriteEvent = false;
					TempRFD->RAMSkip --;
				}
				break;
			case 0x68:	// PCM RAM write
				CmdLen = 0x0C;
				//break;
				printf("\t\t\t\t\t\t\t\t\r");
#ifdef WIN32
				CmdTimer = 0;
#endif
				printf("VGM already optimized!\n");
				CancelFlag = true;
				return;
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
					printf("\t\t\t\t\t\t\t\t\r");
#ifdef WIN32
					CmdTimer = 0;
#endif
					printf("Unknown Command: %X\n", Command);
					CmdLen = 0x01;
					//StopVGM = true;
					break;
				}
				break;
			}
		}
		
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
			BlockLen = VGMHead.lngEOFOffset - VGMHead.lngDataOffset;
			printf("%04.3f %% - %s / %s (%08X / %08X) ...\r", (float)TempLng / BlockLen * 100,
					MinSecStr, TempStr, VGMPos, VGMHead.lngEOFOffset);
			CmdTimer = GetTickCount() + 200;
		}
#endif
	}
	printf("\t\t\t\t\t\t\t\t\r");
	DataSizeA = VGMPos - VGMHead.lngDataOffset;
	if (VGMHead.lngLoopOffset)
	{
		VGMHead.lngLoopOffset = NewLoopS;
		if (! NewLoopS)
			printf("Error! Failed to relocate Loop Point!\n");
		else
			NewLoopS -= 0x1C;
		memcpy(&DstData[0x1C], &NewLoopS, 0x04);
	}
	
	if (VGMHead.lngGD3Offset)
	{
		VGMPos = VGMHead.lngGD3Offset;
		memcpy(&TempLng, &VGMData[VGMPos + 0x00], 0x04);
		if (TempLng == FCC_GD3)
		{
			memcpy(&CmdLen, &VGMData[VGMPos + 0x08], 0x04);
			CmdLen += 0x0C;
			
			VGMHead.lngGD3Offset = DstPos;
			TempLng = VGMHead.lngGD3Offset - 0x14;
			memcpy(&DstData[0x14], &TempLng, 0x04);
			memcpy(&DstData[DstPos], &VGMData[VGMPos], CmdLen);
			DstPos += CmdLen;
		}
	}
	DstDataLen = DstPos;
	VGMHead.lngEOFOffset = DstDataLen;
	TempLng = VGMHead.lngEOFOffset - 0x04;
	memcpy(&DstData[0x04], &TempLng, 0x04);
	
	OptMode = (BlkFound == 0x01) ? 0x00 : 0x01;
	
	for (TempByt = 0x00; TempByt < RFDATA_BLOCKS; TempByt ++)
	{
		free(RF_RData[TempByt].PCMRam);
		RF_RData[TempByt].PCMRam = NULL;
	}
	
	return;
}

static UINT32 ReadConsecutiveMemWrites(UINT8 RFMode)
{
	RF_CHIP_DATA* TempRFD;
	UINT32 TmpPos;
	UINT32 PCMPos;
	UINT32 DataLen;
	UINT8 Command;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	UINT32 BlkLen;
	UINT32 CmdLen;
	bool StopVGM;
	UINT16 BankReg;	// this function mustn't change the global one
	UINT8 BlkType;
	
	TempRFD = &RF_RData[RFMode];
	TmpPos = VGMPos;
	BankReg = TempRFD->BankReg;
	PCMPos = 0xFFFFFFFF;
	DataLen = 0x00;
	
	StopVGM = false;
	while(TmpPos < VGMHead.lngEOFOffset)
	{
		CmdLen = 0x00;
		Command = VGMData[TmpPos + 0x00];
		
		if (Command >= 0x70 && Command <= 0x8F)
		{
			switch(Command & 0xF0)
			{
			case 0x70:
				TempSht = (Command & 0x0F) + 0x01;
				break;
			case 0x80:
				TempSht = Command & 0x0F;
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
				CmdLen = 0x01;
				break;
			case 0x63:	// 1/50s delay
				CmdLen = 0x01;
				break;
			case 0x61:	// xx Sample Delay
				CmdLen = 0x03;
				break;
			case 0x50:	// SN76496 write
				CmdLen = 0x02;
				break;
			case 0x51:	// YM2413 write
				CmdLen = 0x03;
				break;
			case 0x52:	// YM2612 write port 0
			case 0x53:	// YM2612 write port 1
				CmdLen = 0x03;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMData[TmpPos + 0x02];
				memcpy(&BlkLen, &VGMData[TmpPos + 0x03], 0x04);
				BlkLen &= 0x7FFFFFFF;
				
				CmdLen = 0x07 + BlkLen;
				
				if ((TempByt & 0xC0) != 0xC0)
					break;
				
				if (TempByt == 0xC0)
					BlkType = RF5C68_MODE;
				else if (TempByt == 0xC1)
					BlkType = RF5C164_MODE;
				else if (TempByt == 0xE0)
					BlkType = SCSP_MODE;
				else
					break;
				if (BlkType != RFMode)
					break;
				
				if (! (TempByt & 0x20))
				{
					memcpy(&TempSht, &VGMData[TmpPos + 0x07], 0x02);
					TempSht += BankReg;
					TempLng = TempSht;
					BlkLen -= 0x02;
				}
				else
				{
					memcpy(&TempLng, &VGMData[TmpPos + 0x07], 0x04);
					BlkLen -= 0x04;
				}
				if (PCMPos == 0xFFFFFFFF)
					PCMPos = TempSht;
				if (TempLng != PCMPos)
				{
					StopVGM = true;
					break;
				}
				
				if (BlkLen > TempRFD->RAMSize)
					BlkLen = TempRFD->RAMSize;
				if (! (TempByt & 0x20))
					memcpy(&TempRFD->PCMRam[PCMPos], &VGMData[TmpPos + 0x09], BlkLen);
				else
					memcpy(&TempRFD->PCMRam[PCMPos], &VGMData[TmpPos + 0x0B], BlkLen);
				PCMPos += BlkLen;
				DataLen += BlkLen;
				break;
			case 0xE0:	// Seek to PCM Data Bank Pos
				CmdLen = 0x05;
				break;
			case 0x4F:	// GG Stereo
				CmdLen = 0x02;
				break;
			case 0x54:	// YM2151 write
				CmdLen = 0x03;
				break;
			case 0xC0:	// Sega PCM memory write
				CmdLen = 0x04;
				break;
			case 0xB0:	// RF5C68 register write
			case 0xB1:	// RF5C164 register write
				CmdLen = 0x03;
				if ((Command == 0xB0 && RFMode != RF5C68_MODE) ||
					(Command == 0xB1 && RFMode != RF5C164_MODE))
					break;
				
				if (VGMData[TmpPos + 0x01] == 0x07 && ! (VGMData[TmpPos + 0x02] & 0x40))
				{
					// Bank Select
					//StopVGM = true;
					BankReg = (VGMData[TmpPos + 0x02] & 0x0F) << 12;
				}
				break;
			case 0xC1:	// RF5C68 memory write
			case 0xC2:	// RF5C164 memory write
				CmdLen = 0x04;
				if ((Command == 0xC1 && RFMode != RF5C68_MODE) ||
					(Command == 0xC2 && RFMode != RF5C164_MODE))
					break;
				
				memcpy(&TempSht, &VGMData[TmpPos + 0x01], 0x02);
				TempSht += BankReg;
				if (PCMPos == 0xFFFFFFFF)
					PCMPos = TempSht;
				if (TempSht != PCMPos)
				{
					StopVGM = true;
					break;
				}
				
				TempRFD->PCMRam[PCMPos] = VGMData[TmpPos + 0x03];
				PCMPos ++;
				DataLen ++;
				//if (! (PCMPos & 0xFFF))
				//	StopVGM = true;
				break;
			case 0x55:	// YM2203
				CmdLen = 0x03;
				break;
			case 0x56:	// YM2608 write port 0
			case 0x57:	// YM2608 write port 1
				CmdLen = 0x03;
				break;
			case 0x58:	// YM2610 write port 0
			case 0x59:	// YM2610 write port 1
				CmdLen = 0x03;
				break;
			case 0x5A:	// YM3812 write
				CmdLen = 0x03;
				break;
			case 0x5B:	// YM3526 write
				CmdLen = 0x03;
				break;
			case 0x5C:	// Y8950 write
				CmdLen = 0x03;
				break;
			case 0x5E:	// YMF262 write port 0
			case 0x5F:	// YMF262 write port 1
				CmdLen = 0x03;
				break;
			case 0x5D:	// YMZ280B write
				CmdLen = 0x03;
				break;
			case 0x68:	// PCM RAM write
				CmdLen = 0x0C;
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
					//printf("Unknown Command: %X\n", Command);
					CmdLen = 0x01;
					//StopVGM = true;
					break;
				}
				break;
			}
		}
		
		TmpPos += CmdLen;
		if (StopVGM || PCMPos >= TempRFD->RAMSize)
			break;
	}
	
	return DataLen;
}

static void EnumeratePCMData(void)
{
	UINT8 Command;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	UINT32 BlockLen;
	UINT32 DataLen;
#ifdef WIN32
	UINT32 CmdTimer;
	char TempStr[0x80];
	char MinSecStr[0x80];
#endif
	UINT32 CmdLen;
	bool StopVGM;
	UINT32 CurBlk;
	bool FoundBlk;
	RF_BLK_DATA* TempBlk;
	IN_FILE_LIST* TempLst;
	UINT32 BlkUsage;
	UINT32 BlkSize;
	UINT32 BlkSngUse;
	UINT8 BlkType;
	UINT32 BlkStart;
	const UINT8* BlkData;
	
	RFBlkAlloc = 0x40;	// usually there are only few blocks ...
	RFBlock = (RF_BLK_DATA*)malloc(RFBlkAlloc * sizeof(RF_BLK_DATA));
	RFBlkCount = 0x00;
	InFileAlloc = 0x8000;	// ... but block usage is very high
	InFileList = (IN_FILE_LIST*)malloc(InFileAlloc * sizeof(IN_FILE_LIST));
	InFileCount = 0x00;
	BlkUsage = BlkSize = 0x00;
	
	VGMPos = VGMHead.lngDataOffset;
	VGMSmplPos = 0;
	
#ifdef WIN32
	CmdTimer = 0;
#endif
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
			case 0x4F:	// GG Stereo
				CmdLen = 0x02;
				break;
			case 0x51:	// YM2413 write
			case 0x52:	// YM2612 write port 0
			case 0x53:	// YM2612 write port 1
			case 0x54:	// YM2151 write
			case 0x55:	// YM2203
			case 0x56:	// YM2608 write port 0
			case 0x57:	// YM2608 write port 1
			case 0x58:	// YM2610 write port 0
			case 0x59:	// YM2610 write port 1
			case 0x5A:	// YM3812 write
			case 0x5B:	// YM3526 write
			case 0x5C:	// Y8950 write
			case 0x5E:	// YMF262 write port 0
			case 0x5F:	// YMF262 write port 1
			case 0x5D:	// YMZ280B write
				CmdLen = 0x03;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMData[VGMPos + 0x02];
				memcpy(&BlockLen, &VGMData[VGMPos + 0x03], 0x04);
				BlockLen &= 0x7FFFFFFF;
				
				CmdLen = 0x07 + BlockLen;
				
				if ((TempByt & 0xC0) != 0xC0)
					break;
				
				if (TempByt == 0xC0)
					BlkType = RF5C68_MODE;
				else if (TempByt == 0xC1)
					BlkType = RF5C164_MODE;
				else if (TempByt == 0xE0)
					BlkType = SCSP_MODE;
				else
					break;
				
				if (! (TempByt & 0x20))
				{
					memcpy(&TempSht, &VGMData[VGMPos + 0x07], 0x02);
					BlkStart = TempSht;
					BlkData = &VGMData[VGMPos + 0x09];
					DataLen = BlockLen - 0x02;
				}
				else
				{
					memcpy(&BlkStart, &VGMData[VGMPos + 0x07], 0x04);
					BlkData = &VGMData[VGMPos + 0x0B];
					DataLen = BlockLen - 0x04;
				}
				FoundBlk = false;
				for (CurBlk = 0x00; CurBlk < RFBlkCount; CurBlk ++)
				{
					TempBlk = &RFBlock[CurBlk];
					if (TempBlk->Mode != BlkType)
						continue;
					
					//if (TempBlk->DataStart != BlkStart)
					//	break;
					if (TempBlk->DataSize < DataLen)
						TempLng = TempBlk->DataSize;
					else
						TempLng = DataLen;
					
					if (CompareData(TempLng, BlkData, TempBlk->Data))
					{
						if (TempBlk->DataSize < DataLen)
						{
							// expand block with additional data
							BlkSize -= TempBlk->DataSize;
							TempBlk->DataSize = DataLen;
							TempBlk->Data = (UINT8*)realloc(TempBlk->Data, DataLen);
							memcpy(TempBlk->Data, BlkData, DataLen);
							BlkSize += DataLen;
						}
						
						if (InFileCount >= InFileAlloc)
						{
							InFileAlloc += 0x1000;
							InFileList = (IN_FILE_LIST*)realloc(InFileList,
															InFileAlloc * sizeof(IN_FILE_LIST));
						}
						TempLst = &InFileList[InFileCount];
						TempLst->BlockID = CurBlk;
						TempLst->FilePos = VGMPos;
						TempLst->StartAddr = BlkStart;
						TempLst->DataSize = DataLen;
						TempBlk->UsageCounter ++;
						InFileCount ++;
						BlkUsage ++;
						
						FoundBlk = true;
					}
				}
				if (! FoundBlk)
				{
					if (RFBlkCount >= RFBlkAlloc)
					{
						RFBlkAlloc += 0x40;
						RFBlock = (RF_BLK_DATA*)realloc(RFBlock,
														RFBlkAlloc * sizeof(RF_BLK_DATA));
					}
					
					TempBlk = &RFBlock[RFBlkCount];
					RFBlkCount ++;
					
					TempBlk->Mode = BlkType;
					TempBlk->DataSize = DataLen;
					TempBlk->Data = (UINT8*)malloc(DataLen);
					memcpy(TempBlk->Data, BlkData, DataLen);
					TempBlk->UsageCounter = 0x00;
					BlkSize += DataLen;
					
					if (InFileCount >= InFileAlloc)
					{
						InFileAlloc += 0x1000;
						InFileList = (IN_FILE_LIST*)malloc(InFileAlloc * sizeof(IN_FILE_LIST));
					}
					TempLst = &InFileList[InFileCount];
					TempLst->BlockID = CurBlk;
					TempLst->FilePos = VGMPos;
					TempLst->StartAddr = BlkStart;
					TempLst->DataSize = DataLen;
					TempBlk->UsageCounter ++;
					InFileCount ++;
					BlkUsage ++;
				}
				break;
			case 0xE0:	// Seek to PCM Data Bank Pos
				CmdLen = 0x05;
				break;
			case 0xC0:	// Sega PCM memory write
				CmdLen = 0x04;
				break;
			case 0xB0:	// RF5C68 register write
			case 0xB1:	// RF5C164 register write
				CmdLen = 0x03;
				break;
			case 0xC1:	// RF5C68 memory write
			case 0xC2:	// RF5C164 memory write
				CmdLen = 0x04;
				// this time they are ignored
				break;
			case 0x68:	// PCM RAM write
				CmdLen = 0x0C;
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
					//printf("Unknown Command: %X\n", Command);
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
			BlockLen = VGMHead.lngEOFOffset - VGMHead.lngDataOffset;
			printf("%04.3f %% - %s / %s (%08X / %08X) ...\r", (float)TempLng / BlockLen * 100,
					MinSecStr, TempStr, VGMPos, VGMHead.lngEOFOffset);
			CmdTimer = GetTickCount() + 200;
		}
#endif
	}
	printf("\t\t\t\t\t\t\t\t\r");
	
	BlkSngUse = 0x00;
	for (CurBlk = 0x00; CurBlk < RFBlkCount; CurBlk ++)
	{
		TempBlk = &RFBlock[CurBlk];
		if (TempBlk->UsageCounter == 0x01)
		{
			TempBlk->Mode |= 0x80;	// mark the block, so that the PCM bank doesn't include it
			BlkSngUse ++;
		}
	}
	
	printf("%u Blocks created (%.2f MB, %ux used, %ux single use).\n",
			RFBlkCount, BlkSize / 1048576.0f, BlkUsage, BlkSngUse);
	if (BlkUsage == BlkSngUse)
	{
		printf("Can't optimize further.\n");
		CancelFlag = true;
	}
	
	return;
}

static bool CompareData(UINT32 DataLen, const UINT8* DataA,
						const UINT8* DataB)
{
	UINT32 CurPos;
	const UINT8* TempDA;
	const UINT8* TempDB;
	
	TempDA = DataA;
	TempDB = DataB;
	for (CurPos = 0x00; CurPos < DataLen; CurPos ++, TempDA ++, TempDB ++)
	{
		if (*TempDA != *TempDB)
			return false;
	}
	
	return true;
}

static void RewriteVGMData(void)
{
	const UINT8 DBBLK_TYPES[RFDATA_BLOCKS] = {0x01, 0x02, 0x06};
	UINT32 DstPos;
	UINT32 CurType;
	UINT32 CurBlk;
	UINT32 CurEntry;
	UINT8 Command;
	UINT32 CmdDelay;
	UINT32 AllDelay;
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
	RF_BLK_DATA* TempBlk;
	IN_FILE_LIST* TempLst;
	bool WriteCmd68;
	
	DstData = (UINT8*)malloc(VGMDataLen + 0x100);
	AllDelay = 0;
	VGMPos = VGMHead.lngDataOffset;
	DstPos = VGMHead.lngDataOffset;
	VGMSmplPos = 0;
	NewLoopS = 0x00;
	memcpy(DstData, VGMData, VGMPos);	// Copy Header
	
	// Write Blocks for PCM Data
	for (CurType = 0x00; CurType < RFDATA_BLOCKS; CurType ++)
	{
		TempByt = DBBLK_TYPES[CurType];
		
		DataLen = 0x00;
		for (CurBlk = 0x00; CurBlk < RFBlkCount; CurBlk ++)
		{
			TempBlk = &RFBlock[CurBlk];
			if (TempBlk->Mode != CurType)
				continue;
			
			TempBlk->DBPos = DataLen;
			DataLen += TempBlk->DataSize;
		}
		if (DataLen)
		{
			DstData[DstPos + 0x00] = 0x67;
			DstData[DstPos + 0x01] = 0x66;
			DstData[DstPos + 0x02] = TempByt;
			memcpy(&DstData[DstPos + 0x03], &DataLen, 0x04);
			DstPos += 0x07;
			
			for (CurBlk = 0x00; CurBlk < RFBlkCount; CurBlk ++)
			{
				TempBlk = &RFBlock[CurBlk];
				if (TempBlk->Mode != CurType)
					continue;
				
				memcpy(&DstData[DstPos + 0x00], TempBlk->Data, TempBlk->DataSize);
				DstPos += TempBlk->DataSize;
			}
		}
	}
	
#ifdef WIN32
	CmdTimer = 0;
#endif
	StopVGM = false;
	WriteCmd68 = false;
	CurEntry = 0x00;
	while(VGMPos < VGMHead.lngEOFOffset)
	{
		CmdDelay = 0;
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
				CmdDelay = TempSht;
				WriteEvent = false;
				break;
			case 0x80:
				TempSht = Command & 0x0F;
				VGMSmplPos += TempSht;
				//CmdDelay = TempSht;
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
				CmdDelay = TempSht;
				CmdLen = 0x01;
				WriteEvent = false;
				break;
			case 0x63:	// 1/50s delay
				TempSht = 882;
				VGMSmplPos += TempSht;
				CmdDelay = TempSht;
				CmdLen = 0x01;
				WriteEvent = false;
				break;
			case 0x61:	// xx Sample Delay
				memcpy(&TempSht, &VGMData[VGMPos + 0x01], 0x02);
				VGMSmplPos += TempSht;
				CmdDelay = TempSht;
				CmdLen = 0x03;
				WriteEvent = false;
				break;
			case 0x50:	// SN76496 write
			case 0x4F:	// GG Stereo
				CmdLen = 0x02;
				break;
			case 0x51:	// YM2413 write
			case 0x52:	// YM2612 write port 0
			case 0x53:	// YM2612 write port 1
			case 0x54:	// YM2151 write
			case 0x55:	// YM2203
			case 0x56:	// YM2608 write port 0
			case 0x57:	// YM2608 write port 1
			case 0x58:	// YM2610 write port 0
			case 0x59:	// YM2610 write port 1
			case 0x5A:	// YM3812 write
			case 0x5B:	// YM3526 write
			case 0x5C:	// Y8950 write
			case 0x5E:	// YMF262 write port 0
			case 0x5F:	// YMF262 write port 1
			case 0x5D:	// YMZ280B write
				CmdLen = 0x03;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMData[VGMPos + 0x02];
				memcpy(&TempLng, &VGMData[VGMPos + 0x03], 0x04);
				TempLng &= 0x7FFFFFFF;
				
				switch(TempByt & 0xC0)
				{
				case 0x00:	// Database Block
				case 0x40:
					switch(TempByt)
					{
					case 0x00:	// YM2612 PCM Data
						break;
					case 0x01:
					case 0x02:
						WriteEvent = false;
						break;
					}
					break;
				case 0x80:	// ROM/RAM Dump
					break;
				case 0xC0:	// RAM Write
					switch(TempByt)
					{
					case 0xC0:
					case 0xC1:
					case 0xE0:
						while(CurEntry < InFileCount)
						{
							TempLst = &InFileList[CurEntry];
							if (TempLst->FilePos > VGMPos)
							{
								break;
							}
							else if (TempLst->FilePos == VGMPos)
							{
								TempBlk = &RFBlock[TempLst->BlockID];
								if (TempBlk->Mode & 0x80)
									break;	// Single-Use Blocks are left
								
								CurEntry ++; 
								WriteEvent = false;
								// writing the block here would cause some sort of bug, because
								// I would miss the delay
								WriteCmd68 = true;
								break;
							}
							CurEntry ++; 
						}
						break;
					}
					break;
				default:
					break;
				}
				CmdLen = 0x07 + TempLng;
				break;
			case 0xE0:	// Seek to PCM Data Bank Pos
				CmdLen = 0x05;
				break;
			case 0xC0:	// Sega PCM memory write
				CmdLen = 0x04;
				break;
			case 0xB0:	// RF5C68 register write
			case 0xB1:	// RF5C164 register write
				CmdLen = 0x03;
				break;
			case 0xC1:	// RF5C68 memory write
			case 0xC2:	// RF5C164 memory write
				CmdLen = 0x04;
				break;
			case 0x68:	// PCM RAM write
				CmdLen = 0x0C;
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
					//printf("Unknown Command: %X\n", Command);
					CmdLen = 0x01;
					//StopVGM = true;
					break;
				}
				break;
			}
		}
		
		if (WriteEvent || WriteCmd68 || VGMPos == VGMHead.lngLoopOffset)
		{
			if (VGMPos != VGMHead.lngLoopOffset)
			{
				AllDelay += CmdDelay;
				CmdDelay = 0x00;
			}
			while(AllDelay || CmdDelay)
			{
				if (! AllDelay && CmdDelay)
				{
					AllDelay += CmdDelay;
					CmdDelay = 0x00;
					if (VGMPos == VGMHead.lngLoopOffset)
						NewLoopS = DstPos;
				}
				
				if (AllDelay <= 0xFFFF)
					TempSht = (UINT16)AllDelay;
				else
					TempSht = 0xFFFF;
				
				if (! TempSht)
				{
					// don't do anything - I just want to be safe
				}
				if (TempSht <= 0x10)
				{
					DstData[DstPos] = 0x70 | (TempSht - 0x01);
					DstPos ++;
				}
				else if (TempSht <= 0x20)
				{
					DstData[DstPos] = 0x7F;
					DstPos ++;
					DstData[DstPos] = 0x70 | (TempSht - 0x11);
					DstPos ++;
				}
				else if ((TempSht >=  735 && TempSht <=  751) ||
						 (TempSht >= 1470 && TempSht <= 1486))
				{
					TempLng = TempSht;
					while(TempLng >= 735)
					{
						DstData[DstPos] = 0x62;
						DstPos ++;
						TempLng -= 735;
					}
					TempSht -= (UINT16)TempLng;
				}
				else if ((TempSht >=  882 && TempSht <=  898) ||
						 (TempSht >= 1764 && TempSht <= 1780))
				{
					TempLng = TempSht;
					while(TempLng >= 882)
					{
						DstData[DstPos] = 0x63;
						DstPos ++;
						TempLng -= 882;
					}
					TempSht -= (UINT16)TempLng;
				}
				else if (TempSht == 1617)
				{
					DstData[DstPos] = 0x63;
					DstPos ++;
					DstData[DstPos] = 0x62;
					DstPos ++;
				}
				else
				{
					DstData[DstPos + 0x00] = 0x61;
					memcpy(&DstData[DstPos + 0x01], &TempSht, 0x02);
					DstPos += 0x03;
				}
				AllDelay -= TempSht;
			}
			
			if (WriteEvent)
			{
				if (VGMPos == VGMHead.lngLoopOffset)
					NewLoopS = DstPos;
				memcpy(&DstData[DstPos], &VGMData[VGMPos], CmdLen);
				DstPos += CmdLen;
			}
		}
		else
		{
			AllDelay += CmdDelay;
		}
		VGMPos += CmdLen;
		
		if (WriteCmd68)
		{
			WriteCmd68 = false;
			DstData[DstPos + 0x00] = 0x68;
			DstData[DstPos + 0x01] = 0x66;
			DstData[DstPos + 0x02] = DBBLK_TYPES[TempBlk->Mode];
			memcpy(&DstData[DstPos + 0x03], &TempBlk->DBPos,		0x03);
			memcpy(&DstData[DstPos + 0x06], &TempLst->StartAddr,	0x03);
			memcpy(&DstData[DstPos + 0x09], &TempLst->DataSize,		0x03);
			DstPos += 0x0C;
		}
		if (StopVGM)
			break;
		
#ifdef WIN32
		if (CmdTimer < GetTickCount())
		{
			PrintMinSec(VGMSmplPos, MinSecStr);
			PrintMinSec(VGMHead.lngTotalSamples, TempStr);
			TempLng = VGMPos - VGMHead.lngDataOffset;
			DataLen = VGMHead.lngEOFOffset - VGMHead.lngDataOffset;
			printf("%04.3f %% - %s / %s (%08X / %08X) ...\r", (float)TempLng / DataLen * 100,
					MinSecStr, TempStr, VGMPos, VGMHead.lngEOFOffset);
			CmdTimer = GetTickCount() + 200;
		}
#endif
	}
	printf("\t\t\t\t\t\t\t\t\r");
	DataSizeB = DstPos - VGMHead.lngDataOffset;
	if (VGMHead.lngLoopOffset)
	{
		VGMHead.lngLoopOffset = NewLoopS;
		if (! NewLoopS)
			printf("Error! Failed to relocate Loop Point!\n");
		else
			NewLoopS -= 0x1C;
		memcpy(&DstData[0x1C], &NewLoopS, 0x04);
	}
	
	if (VGMHead.lngGD3Offset)
	{
		VGMPos = VGMHead.lngGD3Offset;
		memcpy(&TempLng, &VGMData[VGMPos + 0x00], 0x04);
		if (TempLng == FCC_GD3)
		{
			memcpy(&CmdLen, &VGMData[VGMPos + 0x08], 0x04);
			CmdLen += 0x0C;
			
			VGMHead.lngGD3Offset = DstPos;
			TempLng = VGMHead.lngGD3Offset - 0x14;
			memcpy(&DstData[0x14], &TempLng, 0x04);
			memcpy(&DstData[DstPos], &VGMData[VGMPos], CmdLen);
			DstPos += CmdLen;
		}
	}
	DstDataLen = DstPos;
	VGMHead.lngEOFOffset = DstDataLen;
	TempLng = VGMHead.lngEOFOffset - 0x04;
	memcpy(&DstData[0x04], &TempLng, 0x04);
	
	if (VGMHead.lngVersion < 0x00000160)
	{
		VGMHead.lngVersion = 0x00000160;
		memcpy(&DstData[0x08], &VGMHead.lngVersion, 0x04);
	}
	
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
	sprintf(TempStr, "%02u:%05.2f", TimeMin, TimeSec);
	
	return;
}
#endif
