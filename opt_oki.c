// opt_oki.c - VGM OKIM6258 Optimizer
//

#include "vgmtools.h"

// Structures
typedef struct vgm_data_write
{
	UINT8 Port;
	UINT8 Value;
	UINT32 SmplPos;
	UINT32 FilePos;
} DATA_WRITE;

typedef struct vgm_drum_play
{
	UINT32 WriteSt;
	UINT32 WriteEnd;
} DRUM_PLAY;
typedef struct vgm_drum_info
{
	UINT32 Length;
	UINT8* Data;
	UINT32 PlayCount;
	UINT32 PlayAlloc;
	DRUM_PLAY* PlayData;
} DRUM_INF;
typedef struct vgm_drum_table
{
	UINT32 DrmCount;
	UINT32 DrmAlloc;
	DRUM_INF* Drums;
	DRUM_INF DrumNull;
} DRUM_TABLE;

typedef struct stream_control_command
{
	UINT32 FileOfs;
	UINT8 Command;
	UINT8 StreamID;
	UINT8 DataB1;
	UINT8 DataB2;
	UINT8 DataB3;
	UINT16 DataS1;
	UINT32 DataL1;
	UINT32 DataL2;
} STRM_CTRL_CMD;


// Function Prototypes
static bool OpenVGMFile(const char* FileName);
static void WriteVGMFile(const char* FileName);
static void EnumerateOkiWrite(void);
static void AddPlayWrite(DRUM_INF* DrumInf, DRUM_PLAY* DrumPlay);
static void AddDrum(UINT32 Size, UINT8* Data, DRUM_PLAY* DrumPlay);
static void MakeDrumTable(void);
static void MakeDataStream(void);
static void WriteDACStreamCmd(STRM_CTRL_CMD* StrmCmd, UINT32* DestPos);
static void RewriteVGMData(void);
#ifdef WIN32
static void PrintMinSec(const UINT32 SamplePos, char* TempStr);
#endif

#define printdbg
//#define printdbg	printf


// Variables
VGM_HEADER VGMHead;
UINT32 VGMLoopSmplOfs;
UINT32 VGMDataLen;
UINT8* VGMData;
UINT32 VGMPos;
INT32 VGMSmplPos;
UINT8* DstData;
UINT32 DstDataLen;
char FileBase[0x100];
UINT32 DataSizeA;
UINT32 DataSizeB;

UINT8 ChipCnt;
UINT32 WriteAlloc[2];
UINT32 WriteCount[2];
DATA_WRITE* VGMWrite[2];
UINT32 CtrlCmdCount;
UINT32 CtrlCmdAlloc;
STRM_CTRL_CMD* VGMCtrlCmd;
DRUM_TABLE DrumTbl;

UINT32 MaxDrumDelay;
bool DumpDrums;
UINT8 Skip80Sample;
bool SplitCmd;
bool EarlyDataWrt;

bool CHANGING_CLK_RATE;

int main(int argc, char* argv[])
{
	int argbase;
	int ErrVal;
	char FileName[0x100];
	UINT8 CurChip;
	
	printf("VGM OKI Optimizer\n-----------------\n\n");
	
	MaxDrumDelay = 500;
	DumpDrums = false;
	Skip80Sample = 1;
	EarlyDataWrt = false;
	SplitCmd = 0x00;
	ErrVal = 0;
	argbase = 0x01;
	
	CHANGING_CLK_RATE = false;
	
	printf("File Name:\t");
	if (argc <= argbase + 0x00)
	{
		gets_s(FileName, sizeof(FileName));
	}
	else
	{
		strcpy(FileName, argv[argbase + 0x00]);
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
	
	VGMCtrlCmd = NULL;
	EnumerateOkiWrite();
	MakeDrumTable();
	MakeDataStream();
	
	for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
	{
		free(VGMWrite[CurChip]);	VGMWrite[CurChip] = NULL;
		WriteAlloc[CurChip] = 0x00;
		WriteCount[CurChip] = 0x00;
	}
	
	RewriteVGMData();
	
	free(VGMCtrlCmd);	VGMCtrlCmd = NULL;
	CtrlCmdAlloc = 0x00;
	CtrlCmdCount = 0x00;
	printf("Data Compression: %u -> %u (%.1f %%)\n",
			DataSizeA, DataSizeB, 100.0 * DataSizeB / (float)DataSizeA);
	
	if (DataSizeB < DataSizeA)
	{
		if (argc > argbase + 0x01)
			strcpy(FileName, argv[argbase + 0x01]);
		else
			strcpy(FileName, "");
		if (! FileName[0x00])
		{
			strcpy(FileName, FileBase);
			strcat(FileName, "_optimized.vgm");
		}
		WriteVGMFile(FileName);
	}
	
	free(VGMData);
	free(DstData);
	
EndProgram:
	waitkey(argv[0]);

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
	if (hFile == NULL)
	{
		printf("Error writing %s!\n", FileName);
		return;
	}
	fwrite(DstData, 0x01, DstDataLen, hFile);
	fclose(hFile);
	
	printf("File written.\n");
	
	return;
}

static void EnumerateOkiWrite(void)
{
	UINT8 Command;
	UINT8 CurChip;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
#ifdef WIN32
	UINT32 CmdTimer;
	char TempStr[0x80];
	char MinSecStr[0x80];
#endif
	UINT32 CmdLen;
	bool StopVGM;
	DATA_WRITE* TempWrt;
	
	VGMLoopSmplOfs = VGMHead.lngTotalSamples - VGMHead.lngLoopSamples;
	VGMPos = VGMHead.lngDataOffset;
	VGMSmplPos = 0;
	
	ChipCnt = (VGMHead.lngHzOKIM6258 & 0x40000000) ? 2 : 1;
	for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
	{
		WriteAlloc[CurChip] = VGMHead.lngEOFOffset / (6 * ChipCnt);
		VGMWrite[CurChip] = (DATA_WRITE*)malloc(WriteAlloc[CurChip] * sizeof(DATA_WRITE));
	}
	for (; CurChip < 0x02; CurChip ++)
	{
		WriteAlloc[CurChip] = 0;
		VGMWrite[CurChip] = NULL;
	}
	for (CurChip = 0x00; CurChip < 0x02; CurChip ++)
		WriteCount[CurChip] = 0x00;
	
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
				CmdLen = 0x02;
				break;
			case 0xB7:	// OKIM6258 write
				CurChip = (VGMData[VGMPos + 0x01] & 0x80) >> 7;
				if (CurChip < ChipCnt)
				{
					if (WriteCount[CurChip] >= WriteAlloc[CurChip])
					{
						WriteAlloc[CurChip] += 0x10000;
						VGMWrite[CurChip] = (DATA_WRITE*)realloc(VGMWrite[CurChip],
														WriteAlloc[CurChip] * sizeof(DATA_WRITE));
					}
					TempWrt = &VGMWrite[CurChip][WriteCount[CurChip]];
					WriteCount[CurChip] ++;
					
					TempWrt->Port = VGMData[VGMPos + 0x01] & 0x7F;
					TempWrt->Value = VGMData[VGMPos + 0x02];
					TempWrt->SmplPos = VGMSmplPos;
					TempWrt->FilePos = VGMPos;
					if (TempWrt->Port == 0x0C && ! CHANGING_CLK_RATE)
					{
						if (VGMSmplPos == 0)
						{
							VGMHead.bytOKI6258Flags &= ~0x03;
							VGMHead.bytOKI6258Flags |= TempWrt->Value;
							VGMData[0x94] = VGMHead.bytOKI6258Flags;
						}
						if (TempWrt->Value != (VGMHead.bytOKI6258Flags & 0x03))
						{
							printf("Warning! Clock Divider changed!! (sample %u)\n", TempWrt->SmplPos);
							CHANGING_CLK_RATE = true;
							_getch();
						}
					}
					if (TempWrt->Port == 0x0B)
					{
						printf("Warning! Master Clock changed!! (sample %u)\n", TempWrt->SmplPos);
						_getch();
					}
				}
				CmdLen = 0x03;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMData[VGMPos + 0x02];
				memcpy(&TempLng, &VGMData[VGMPos + 0x03], 0x04);
				TempLng &= 0x7FFFFFFF;
				
				CmdLen = 0x07 + TempLng;
				break;
			case 0x90:	// DAC Ctrl: Setup Chip
				VGMPos += 0x05;
				break;
			case 0x91:	// DAC Ctrl: Set Data
				VGMPos += 0x05;
				break;
			case 0x92:	// DAC Ctrl: Set Freq
				VGMPos += 0x06;
				break;
			case 0x93:	// DAC Ctrl: Play from Start Pos
				VGMPos += 0x0B;
				break;
			case 0x94:	// DAC Ctrl: Stop immediately
				VGMPos += 0x02;
				break;
			case 0x95:	// DAC Ctrl: Play Block (small)
				VGMPos += 0x05;
				break;
			default:	// Handle all other known and unknown commands
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
			CmdLen = VGMHead.lngEOFOffset - VGMHead.lngDataOffset;
			printf("%04.3f %% - %s / %s (%08X / %08X) ...\r", (float)TempLng / CmdLen * 100,
					MinSecStr, TempStr, VGMPos, VGMHead.lngEOFOffset);
			CmdTimer = GetTickCount() + 200;
		}
#endif
	}
	printf("\t\t\t\t\t\t\t\t\r");
	
	printf("%u OKI writes found.\n", WriteCount[0]);
	
	return;
}

static void AddPlayWrite(DRUM_INF* DrumInf, DRUM_PLAY* DrumPlay)
{
	if (DrumInf->PlayCount >= DrumInf->PlayAlloc)
	{
		DrumInf->PlayAlloc += 0x100;
		DrumInf->PlayData = (DRUM_PLAY*)realloc(DrumInf->PlayData,
								DrumInf->PlayAlloc * sizeof(DRUM_PLAY));
	}
	DrumInf->PlayData[DrumInf->PlayCount] = *DrumPlay;
	DrumInf->PlayCount ++;
	
	return;
}

static void AddDrum(UINT32 Size, UINT8* Data, DRUM_PLAY* DrumPlay)
{
	UINT32 CurDrm;
	DRUM_INF* TempDrm;
	
	if (Size == 0x00)
	{
		return;
	}
	else if (Size == 0x01)
	{
		DATA_WRITE* TempWrt;
		
		CurDrm = DrumPlay->WriteSt;
		TempWrt = &VGMWrite[CurDrm >> 31][CurDrm & 0x7FFFFFFF];
		if (TempWrt->Value == 0x80 || TempWrt->Value == 0x88 || TempWrt->Value == 0x08)
		{
			//TempWrt->Port |= 0x80;
			AddPlayWrite(&DrumTbl.DrumNull, DrumPlay);
			return;
		}
		printf("Mini-Write found: 0x%X (sample %u)\n", TempWrt->Value, TempWrt->SmplPos);
		if (TempWrt->SmplPos)
			;//_getch();
		AddPlayWrite(&DrumTbl.DrumNull, DrumPlay);
		return;
	}
	
	for (CurDrm = 0x00; CurDrm < DrumTbl.DrmCount; CurDrm ++)
	{
		TempDrm = &DrumTbl.Drums[CurDrm];
		if (TempDrm->Length >= Size)
		{
			if (! memcmp(TempDrm->Data, Data, Size))	// do existing and new data match?
			{
				//printf("Adding Play %u (played %u times)\n", CurDrm, TempDrm->PlayCount);
				AddPlayWrite(TempDrm, DrumPlay);
				return;
			}
		}
		else //if (TempDrm->Length < Size)
		{
			if (! memcmp(TempDrm->Data, Data, TempDrm->Length))	// does first part match?
			{
				printdbg("Enlarging sample %u (0x%X -> 0x%X, played %u times)\n",
							CurDrm, TempDrm->Length, Size, TempDrm->PlayCount);
				// replace old data with new (longer) data
				free(TempDrm->Data);
				TempDrm->Data = (UINT8*)malloc(Size);
				memcpy(TempDrm->Data, Data, Size);
				TempDrm->Length = Size;
				if (DumpDrums)
				{
					char DrumDump_Name[MAX_PATH + 0x10];
					FILE* hFile;
					
					sprintf(DrumDump_Name, "%s_%03X.raw", FileBase, CurDrm);
					
					hFile = fopen(DrumDump_Name, "wb");
					if (hFile != NULL)
					{
						fwrite(Data, 0x01, Size, hFile);
						fclose(hFile);
					}
				}
				
				AddPlayWrite(TempDrm, DrumPlay);
				return;
			}
		}
	}
	
	// not found, add new drum sound
	if (DrumTbl.DrmCount >= DrumTbl.DrmAlloc)
	{
		DrumTbl.DrmAlloc += 0x100;
		DrumTbl.Drums = (DRUM_INF*)realloc(DrumTbl.Drums,
											DrumTbl.DrmAlloc * sizeof(DRUM_INF));
	}
	TempDrm = &DrumTbl.Drums[DrumTbl.DrmCount];
	//printdbg("Adding Drum %u (0x%X bytes)\n", DrumTbl.DrmCount, Size);
	
	TempDrm->Data = (UINT8*)malloc(Size);
	memcpy(TempDrm->Data, Data, Size);
	TempDrm->Length = Size;
	if (DumpDrums)
	{
		char DrumDump_Name[MAX_PATH + 0x10];
		FILE* hFile;
		
		sprintf(DrumDump_Name, "%s_%03X.raw", FileBase, DrumTbl.DrmCount);
		
		hFile = fopen(DrumDump_Name, "wb");
		if (hFile != NULL)
		{
			fwrite(Data, 0x01, Size, hFile);
			fclose(hFile);
		}
	}
	
	TempDrm->PlayCount = 0x00;
	TempDrm->PlayAlloc = 0x100;
	TempDrm->PlayData = (DRUM_PLAY*)malloc(TempDrm->PlayAlloc * sizeof(DRUM_PLAY));
	DrumTbl.DrmCount ++;
	
	AddPlayWrite(TempDrm, DrumPlay);
	
	return;
}

static void MakeDrumTable(void)
{
	UINT32 CurChip;
	UINT32 CurWrt;
	UINT32 LastWrt;
	DATA_WRITE* TempWrt;
	UINT32 DrmBufSize;
	UINT32 DrmBufAlloc;
	UINT8* DrumBuf;
	DRUM_PLAY DrumPlay;
	UINT32 SmplLastWrt;
	UINT8 CurClkDiv;
	UINT8 DrumEnd;
	UINT32 SkippedWrt;
	
	if (! WriteCount)
		return;
	
	DrumTbl.DrmCount = 0x00;
	DrumTbl.DrmAlloc = 0x100;
	DrumTbl.Drums = (DRUM_INF*)malloc(DrumTbl.DrmAlloc * sizeof(DRUM_INF));
	DrumTbl.DrumNull.PlayCount = 0x00;
	DrumTbl.DrumNull.PlayAlloc = 0x1000;
	DrumTbl.DrumNull.PlayData = (DRUM_PLAY*)malloc(DrumTbl.DrumNull.PlayAlloc * sizeof(DRUM_PLAY));
	
	DrmBufAlloc = 0x10000;	// 64 KB should be enough for now
	DrumBuf = (UINT8*)malloc(DrmBufAlloc);
	
	printf("Generating Drum Table ...\n");
	for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
	{
		CurClkDiv = VGMHead.bytOKI6258Flags & 0x03;
		DrmBufSize = 0x00;
		SmplLastWrt = -1;
		SkippedWrt = 0;
		printdbg("Chip %u, %u writes\n", CurChip, WriteCount[CurChip]);
		for (CurWrt = 0x00; CurWrt < WriteCount[CurChip]; CurWrt ++)
		{
			TempWrt = &VGMWrite[CurChip][CurWrt];
			DrumEnd = 0x00;
			if (SkippedWrt)
			{
				LastWrt = SkippedWrt;
				SkippedWrt = 0;
				if (! DrmBufSize)
					DrumPlay.WriteSt = LastWrt | (CurChip << 31);
				DrumBuf[DrmBufSize] = VGMWrite[CurChip][LastWrt].Value;
				DrmBufSize ++;
			}
			if (TempWrt->Port == SplitCmd)	// non-sample data
			{
				//printdbg("OKI write port %u, Buffer Size 0x%X\n", TempWrt->Port, DrmBufSize);
				if (SplitCmd == 0x0C && TempWrt->Value == CurClkDiv)
				{
					// skip/ignore
				}
				else if (DrmBufSize)
				{
					//if (! (DrmBufSize == 0x01 && TempWrt->SmplPos == SmplLastWrt))
					if (1)
					{
						printdbg("new drum at SmplOfs %u (buffer %u bytes)\n", SmplLastWrt, DrmBufSize);
						DrumEnd = 0x01;
					}
					else
					{
						//_getch();
						printf("Warning: Skipped sample restart at SmplOfs %u!\n", SmplLastWrt);
					}
				}
			}
			else if (TempWrt->Port == 0x01)	// data command
			{
				if (MaxDrumDelay && DrmBufSize && TempWrt->SmplPos >= SmplLastWrt + MaxDrumDelay)
				{
					printf("forced new drum after delay of %d samples (buffer %u bytes)\n",
							TempWrt->SmplPos - SmplLastWrt, DrmBufSize);
					DrumEnd = 0x02;
				}
				else if (SmplLastWrt == (UINT32)-1 && DrmBufSize)
				{
					DrmBufSize = 0x00;
					DrumEnd = 0x02;
				}
				/*if (SmplLastWrt < VGMLoopSmplOfs && TempWrt->SmplPos >= VGMLoopSmplOfs)
				{
					DrumEnd = 0x03;
				}*/
			}
			if (TempWrt->Port == 0x0C)
				CurClkDiv = TempWrt->Value;
			
			if (DrumEnd)
			{
				if (EarlyDataWrt && DrmBufSize >= 2 && DrumEnd == 0x01)
				{
					// Add the very last sample BEFORE the split command to the actual sample
					if (VGMWrite[CurChip][LastWrt].Value == 0x80)
					{
						SkippedWrt = LastWrt;
						LastWrt --;
						DrmBufSize --;
						while(LastWrt && VGMWrite[CurChip][LastWrt].Port != 0x01)
							LastWrt --;
					}
				}
				
				printdbg("new drum at SmplOfs %u (buffer %u bytes)\n", SmplLastWrt, DrmBufSize);
				if (Skip80Sample == 1 && DrmBufSize > 1 && VGMWrite[CurChip][LastWrt].Value == 0x80)
				{
					// Castlevania sends a 0x80 byte when stopping a sound
					
					// add drum without last data write
					DrumPlay.WriteEnd = LastWrt - 1;
					while(DrumPlay.WriteEnd && VGMWrite[CurChip][DrumPlay.WriteEnd].Port != 0x01)
						DrumPlay.WriteEnd --;
					AddDrum(DrmBufSize - 1, DrumBuf, &DrumPlay);
					
					// add that write seperately
					DrumPlay.WriteSt = LastWrt | (CurChip << 31);
					DrumPlay.WriteEnd = LastWrt;
					AddDrum(1, DrumBuf + DrmBufSize - 1, &DrumPlay);
					DrmBufSize = 0x00;
					continue;
				}
				else if (Skip80Sample == 2 && DrumEnd == 0x01 && DrmBufSize >= 2 && VGMWrite[CurChip][LastWrt].Value == 0x80)
				{
					// Chase H.Q. sends 2x 0x88 before playing ANYTHING
					
					// add drum without last data write
					DrumPlay.WriteEnd = LastWrt - 1;
					while(DrumPlay.WriteEnd && VGMWrite[CurChip][DrumPlay.WriteEnd].Port != 0x01)
						DrumPlay.WriteEnd --;
					DrumPlay.WriteEnd --;
					AddDrum(DrmBufSize - 2, DrumBuf, &DrumPlay);
					
					// add that write seperately
					DrumPlay.WriteSt = LastWrt | (CurChip << 31);
					DrumPlay.WriteEnd = LastWrt;
					AddDrum(1, DrumBuf + DrmBufSize - 2, &DrumPlay);
					DrmBufSize = 0x00;
					continue;
				}
				else if (Skip80Sample == 2 && DrumEnd == 0x02 && DrmBufSize > 1 && VGMWrite[CurChip][LastWrt].Value == 0x88)
				{
					// add drum without last data write
					DrumPlay.WriteEnd = LastWrt - 1;
					while(DrumPlay.WriteEnd && VGMWrite[CurChip][DrumPlay.WriteEnd].Port != 0x01)
						DrumPlay.WriteEnd --;
					AddDrum(DrmBufSize - 1, DrumBuf, &DrumPlay);
					
					// add that write seperately
					DrumPlay.WriteSt = LastWrt | (CurChip << 31);
					DrumPlay.WriteEnd = LastWrt;
					AddDrum(1, DrumBuf + DrmBufSize - 1, &DrumPlay);
					DrmBufSize = 0x00;
					continue;
				}
				
				DrumPlay.WriteEnd = LastWrt;
				AddDrum(DrmBufSize, DrumBuf, &DrumPlay);
				DrmBufSize = 0x00;
				if (SkippedWrt)
				{
					LastWrt = SkippedWrt;
					SkippedWrt = 0;
					DrumPlay.WriteSt = LastWrt | (CurChip << 31);
					DrumBuf[DrmBufSize] = VGMWrite[CurChip][LastWrt].Value;
					DrmBufSize ++;
				}
			}
			
			if (TempWrt->Port == 0x01)
			{
				// add data command to buffer
				if (DrmBufSize >= DrmBufAlloc)
				{
					DrmBufAlloc += 0x10000;	// add another 64 KB
					DrumBuf = (UINT8*)realloc(DrumBuf, DrmBufAlloc);
				}
				if (! DrmBufSize)
					DrumPlay.WriteSt = CurWrt | (CurChip << 31);
				DrumBuf[DrmBufSize] = TempWrt->Value;
				DrmBufSize ++;
				
				SmplLastWrt = TempWrt->SmplPos;
				LastWrt = CurWrt;
			}
		}
		if (DrmBufSize)
		{
			DrumPlay.WriteEnd = LastWrt;
			AddDrum(DrmBufSize, DrumBuf, &DrumPlay);
		}
	}
	printf("%u drum sounds found\n", DrumTbl.DrmCount);
	
	return;
}

typedef struct _drum_play_sort
{
	UINT32 SortID;
	UINT32 DrumID;
	UINT32 PlayID;
} DRM_SORT;

static int drum_sort_compare(const void* p1, const void* p2)
{
	DRM_SORT* Drm1 = (DRM_SORT*)p1;
	DRM_SORT* Drm2 = (DRM_SORT*)p2;
	
	if (Drm1->SortID < Drm2->SortID)
		return -1;
	else if (Drm1->SortID == Drm2->SortID)
		return 0;
	else //if (Drm1->SortID > Drm2->SortID)
		return +1;
}

// dividers[4] = {1024, 768, 512, 512}, but stream rate is clock/div/2
static const int dividers[6] = {2048, 1536, 1024, 1024, 4096, 3072};

//#define CHANGING_RATE
static void MakeDataStream(void)
{
	UINT32 BaseFreq;
	UINT8 LastDiv;
	
	UINT32 CurDrm;
	UINT32 CurPlay;
	UINT32 CurSrt;
	UINT32 CurWrt;
	UINT32 DrmSrtCount;
	DRM_SORT* DrumSort;
	DRUM_INF* TempDrm;
	DRM_SORT* TempSort;
	UINT8 CurChip;
	UINT32 LastFreq[0x02];
	STRM_CTRL_CMD* TempCmd;
	UINT64 CmdDiff;
	UINT32 SmplDiff;
	UINT32 FreqVal;
	UINT8 IsStopped;
	
	LastDiv = VGMHead.bytOKI6258Flags & 0x03;
	BaseFreq = (VGMHead.lngHzOKIM6258 + dividers[LastDiv] / 2) / dividers[LastDiv];
	printf("Calculated base freq: %u\n", BaseFreq);
	
	// create list of all played drums
	DrmSrtCount = 0x00;
	DrmSrtCount += DrumTbl.DrumNull.PlayCount;
	for (CurDrm = 0x00; CurDrm < DrumTbl.DrmCount; CurDrm ++)
		DrmSrtCount += DrumTbl.Drums[CurDrm].PlayCount;
	DrumSort = (DRM_SORT*)malloc(DrmSrtCount * sizeof(DRM_SORT));
	
	printf("Sorting %u play commands...\n", DrmSrtCount);
	TempSort = DrumSort;
	TempDrm = &DrumTbl.DrumNull;
	for (CurPlay = 0x00; CurPlay < TempDrm->PlayCount; CurPlay ++)
	{
		CurChip = (TempDrm->PlayData[CurPlay].WriteSt & 0x80000000) >> 31;
		CurWrt = TempDrm->PlayData[CurPlay].WriteSt & 0x7FFFFFFF;
		TempSort->SortID = VGMWrite[CurChip][CurWrt].FilePos;
		TempSort->DrumID = 0xFFFFFFFF;
		TempSort->PlayID = CurPlay;
		TempSort ++;
	}
	for (CurDrm = 0x00; CurDrm < DrumTbl.DrmCount; CurDrm ++)
	{
		TempDrm = &DrumTbl.Drums[CurDrm];
		for (CurPlay = 0x00; CurPlay < TempDrm->PlayCount; CurPlay ++)
		{
			CurChip = (TempDrm->PlayData[CurPlay].WriteSt & 0x80000000) >> 31;
			CurWrt = TempDrm->PlayData[CurPlay].WriteSt & 0x7FFFFFFF;
			TempSort->SortID = VGMWrite[CurChip][CurWrt].FilePos;
			TempSort->DrumID = CurDrm;
			TempSort->PlayID = CurPlay;
			TempSort ++;
		}
	}
	
	// and sort by WriteID
	qsort(DrumSort, DrmSrtCount, sizeof(DRM_SORT), &drum_sort_compare);
	
	// Now general all the special DAC Stream commands.
	
	// (Command 90/91) * Chip Count + (Command 92/95) * played drums
	CtrlCmdAlloc = 0x02 * ChipCnt + 0x02 * DrmSrtCount;	// should be just enough
	VGMCtrlCmd = (STRM_CTRL_CMD*)malloc(CtrlCmdAlloc * sizeof(STRM_CTRL_CMD));
	
	CtrlCmdCount = 0x00;
	for (CurChip = 0x00; CurChip < ChipCnt; CurChip ++)
	{
		TempCmd = &VGMCtrlCmd[CtrlCmdCount];
		CtrlCmdCount ++;
		TempCmd->FileOfs = 0x00;
		TempCmd->Command = 0x90;	// Setup Stream 00
		TempCmd->StreamID = CurChip;
		TempCmd->DataB1 = 0x17 | (CurChip << 7);	// 0x17 - OKIM6258 chip
		TempCmd->DataB2 = 0x00;
		TempCmd->DataB3 = 0x01;		// command 01: data write
		
		TempCmd = &VGMCtrlCmd[CtrlCmdCount];
		CtrlCmdCount ++;
		TempCmd->FileOfs = 0x00;
		TempCmd->Command = 0x91;	// Set Stream 00 Data
		TempCmd->StreamID = CurChip;
		TempCmd->DataB1 = 0x04;		// Block Type 04: OKIM6258 Data Block
		TempCmd->DataB2 = 0x01;		// Step Size
		TempCmd->DataB3 = 0x00;		// Step Base
		LastFreq[CurChip] = 0;
		IsStopped = 0x00;
		
//#ifndef CHANGING_RATE
		if (! CHANGING_CLK_RATE)
		{
			TempCmd = &VGMCtrlCmd[CtrlCmdCount];
			CtrlCmdCount ++;
			TempCmd->FileOfs = 0x00;
			TempCmd->Command = 0x92;	// Set Stream Frequency
			TempCmd->StreamID = CurChip;
			TempCmd->DataL1 = BaseFreq;
			LastFreq[CurChip] = BaseFreq;
		}
//#endif
	}
	
	for (CurSrt = 0x00; CurSrt < DrmSrtCount; CurSrt ++)
	{
		TempSort = &DrumSort[CurSrt];
		if (TempSort->DrumID == 0xFFFFFFFF)
		{
			TempDrm = &DrumTbl.DrumNull;
			CurPlay = TempSort->PlayID;
			CurChip = (TempDrm->PlayData[CurPlay].WriteSt & 0x80000000) >> 31;
			CurWrt = TempDrm->PlayData[CurPlay].WriteSt & 0x7FFFFFFF;
			
			TempCmd = &VGMCtrlCmd[CtrlCmdCount];
			CtrlCmdCount ++;
			TempCmd->FileOfs = VGMWrite[CurChip][CurWrt].FilePos;
			TempCmd->Command = 0x94;	// Stop Stream block
			TempCmd->StreamID = CurChip;
			if (IsStopped & (1 << CurChip))
				TempCmd->DataB1 = 0x02 | 0x01;
			else
				TempCmd->DataB1 = 0x01;
			IsStopped |= 1 << CurChip;
			continue;
		}
		
		TempDrm = &DrumTbl.Drums[TempSort->DrumID];
		CurPlay = TempSort->PlayID;
		CurChip = (TempDrm->PlayData[CurPlay].WriteSt & 0x80000000) >> 31;
		CurWrt = TempDrm->PlayData[CurPlay].WriteSt & 0x7FFFFFFF;
		
		if (CHANGING_CLK_RATE)
		{
			// calculate frequency
			FreqVal = TempDrm->PlayData[CurPlay].WriteEnd;
			//SmplDiff = Sample End - Sample Start
			SmplDiff = VGMWrite[CurChip][FreqVal].SmplPos - VGMWrite[CurChip][CurWrt].SmplPos;
			if (SmplDiff >= 0x10)
			{
				printdbg("OKI Frequency [Drum %u, No. %u]: %f (Sample %u-%u)\n", TempSort->DrumID, CurPlay,
						44100.0 * (FreqVal - CurWrt) / SmplDiff, VGMWrite[CurChip][CurWrt].SmplPos,
						VGMWrite[CurChip][FreqVal].SmplPos);
				CmdDiff = FreqVal - CurWrt;	// CmdDiff = (LastCommandID + 1) - FirstCommandID
				CmdDiff = 44100 * CmdDiff + SmplDiff / 2;
				FreqVal = (UINT32)(CmdDiff / SmplDiff);
				//printf("OKI Frequency [Drum %u, No. %u]: %u\n", TempSort->DrumID, CurPlay, FreqVal);
				
				CmdDiff = 0;
				for (CurPlay = 0; CurPlay < 6; CurPlay ++)
				{
					BaseFreq = (VGMHead.lngHzOKIM6258 + dividers[CurPlay] / 2) / dividers[CurPlay];
					if (abs(FreqVal - BaseFreq) < 120 ||
						(FreqVal < BaseFreq && FreqVal >= BaseFreq - 800))
					{
						FreqVal = BaseFreq;
						CmdDiff = 1;
						break;
					}
				}
				if (! CmdDiff)
				{
					// TODO: if it fails because the value is too high, try again with 8 samples less.
					printf("Unable to round frequency %u! (sample %u)\n", FreqVal, VGMWrite[CurChip][CurWrt].SmplPos);
					if (! LastFreq[CurChip])
						FreqVal = BaseFreq;
					else
						FreqVal = LastFreq[CurChip];
					printf("Setting to %u\n", FreqVal);
					_getch();
				}
			}
			else
			{
				if (! LastFreq[CurChip])
					FreqVal = BaseFreq;
				else
					FreqVal = LastFreq[CurChip];
			}
			// frequency calculation end
//#ifdef CHANGING_RATE
			if (LastFreq[CurChip] != FreqVal)
			{
				LastFreq[CurChip] = FreqVal;
				TempCmd = &VGMCtrlCmd[CtrlCmdCount];
				CtrlCmdCount ++;
				TempCmd->FileOfs = VGMWrite[CurChip][CurWrt].FilePos;
				TempCmd->Command = 0x92;	// Set Stream Frequency
				TempCmd->StreamID = CurChip;
				TempCmd->DataL1 = FreqVal;
			}
		}	// end if (CHANGING_CLK_RATE)
//#endif
		
		TempCmd = &VGMCtrlCmd[CtrlCmdCount];
		CtrlCmdCount ++;
		TempCmd->FileOfs = VGMWrite[CurChip][CurWrt].FilePos;
		TempCmd->Command = 0x95;	// Quick-Play block
		TempCmd->StreamID = CurChip;
		TempCmd->DataS1 = TempSort->DrumID;	// Block ID
		TempCmd->DataB1 = 0x00;				// No looping
		IsStopped &= ~(1 << CurChip);
	}
	printf("Done.\n");
	
	return;
}

static void WriteDACStreamCmd(STRM_CTRL_CMD* StrmCmd, UINT32* DestPos)
{
	UINT32 DstPos;
	
	DstPos = *DestPos;
	DstData[DstPos + 0x00] = StrmCmd->Command;
	DstData[DstPos + 0x01] = StrmCmd->StreamID;
	switch(StrmCmd->Command)
	{
	case 0x90:
		DstData[DstPos + 0x02] = StrmCmd->DataB1;
		DstData[DstPos + 0x03] = StrmCmd->DataB2;
		DstData[DstPos + 0x04] = StrmCmd->DataB3;
		DstPos += 0x05;
		break;
	case 0x91:
		DstData[DstPos + 0x02] = StrmCmd->DataB1;
		DstData[DstPos + 0x03] = StrmCmd->DataB2;
		DstData[DstPos + 0x04] = StrmCmd->DataB3;
		DstPos += 0x05;
		break;
	case 0x92:
		memcpy(&DstData[DstPos + 0x02], &StrmCmd->DataL1, 0x04);
		DstPos += 0x06;
		break;
	case 0x94:
		DstPos += 0x02;
		break;
	case 0x95:
		memcpy(&DstData[DstPos + 0x02], &StrmCmd->DataS1, 0x02);
		DstData[DstPos + 0x04] = StrmCmd->DataB1;
		DstPos += 0x05;
		break;
	}
	*DestPos = DstPos;
	
	return;
}

static void RewriteVGMData(void)
{
	UINT32 DstPos;
	UINT8 Command;
	UINT32 CmdDelay;
	UINT32 AllDelay;
	UINT8 TempByt;
	UINT16 TempSht;
	UINT32 TempLng;
	DRUM_INF* TempDrm;
#ifdef WIN32
	UINT32 CmdTimer;
	char TempStr[0x80];
	char MinSecStr[0x80];
#endif
	UINT32 CmdLen;
	bool StopVGM;
	bool WriteEvent;
	bool WriteExtra;
	UINT32 NewLoopS;
	UINT32 StrmCmd;
	
	UINT16 LoopSmplID = (UINT16)-1;
	UINT16 LastSmplID = (UINT16)-1;
	
	DstDataLen = VGMDataLen + 0x100;
	DstData = (UINT8*)malloc(DstDataLen);
	AllDelay = 0;
	VGMPos = VGMHead.lngDataOffset;
	DstPos = VGMHead.lngDataOffset;
	VGMSmplPos = 0;
	NewLoopS = 0x00;
	memcpy(DstData, VGMData, VGMPos);	// Copy Header
	
#ifdef WIN32
	CmdTimer = 0;
#endif
	StopVGM = false;
	
	for (TempLng = 0x00; TempLng < DrumTbl.DrmCount; TempLng ++)
	{
		TempDrm = &DrumTbl.Drums[TempLng];
		//printdbg("Write drum %u data, DstPos 0x%X\n", TempLng, DstPos);
		DstData[DstPos + 0x00] = 0x67;
		DstData[DstPos + 0x01] = 0x66;
		DstData[DstPos + 0x02] = 0x04;	// OKIM6258 Data Block
		memcpy(&DstData[DstPos + 0x03], &TempDrm->Length, 0x04);
		memcpy(&DstData[DstPos + 0x07], TempDrm->Data, TempDrm->Length);
		DstPos += 0x07 + TempDrm->Length;
	}
	
	for (StrmCmd = 0x00; StrmCmd < CtrlCmdCount; StrmCmd ++)
	{
		if (VGMCtrlCmd[StrmCmd].FileOfs)
			break;
		
		//printdbg("Write DAC command %u, DstPos 0x%X\n", StrmCmd, DstPos);
		WriteDACStreamCmd(&VGMCtrlCmd[StrmCmd], &DstPos);
	}
	while(VGMPos < VGMHead.lngEOFOffset)
	{
		CmdDelay = 0;
		CmdLen = 0x00;
		Command = VGMData[VGMPos + 0x00];
		WriteEvent = true;
		//WriteExtra = false;
		//if (VGMPos == VGMHead.lngLoopOffset)
		//	WriteExtra = true;
		WriteExtra = (VGMPos == VGMHead.lngLoopOffset);
		
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
				CmdLen = 0x02;
				break;
			case 0xB7:	// OKIM6258 write
				TempByt = (VGMData[VGMPos + 0x01] & 0x80) >> 7;
				if (TempByt < ChipCnt)
				{
					if ((VGMData[VGMPos + 0x01] & 0x7F) == 0x01)
					{
						WriteEvent = false;
						if (StrmCmd < CtrlCmdCount && VGMCtrlCmd[StrmCmd].FileOfs <= VGMPos)
							WriteExtra = true;
					}
				}
				CmdLen = 0x03;
				break;
			case 0x67:	// PCM Data Stream
				TempByt = VGMData[VGMPos + 0x02];
				memcpy(&TempLng, &VGMData[VGMPos + 0x03], 0x04);
				TempLng &= 0x7FFFFFFF;
				
				CmdLen = 0x07 + TempLng;
				break;
			case 0x90:	// DAC Ctrl: Setup Chip
				VGMPos += 0x05;
				break;
			case 0x91:	// DAC Ctrl: Set Data
				VGMPos += 0x05;
				break;
			case 0x92:	// DAC Ctrl: Set Freq
				VGMPos += 0x06;
				break;
			case 0x93:	// DAC Ctrl: Play from Start Pos
				VGMPos += 0x0B;
				break;
			case 0x94:	// DAC Ctrl: Stop immediately
				VGMPos += 0x02;
				break;
			case 0x95:	// DAC Ctrl: Play Block (small)
				VGMPos += 0x05;
				break;
			default:	// Handle all other known and unknown commands
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
		
		if (WriteEvent || WriteExtra)
		{
			if (VGMPos != VGMHead.lngLoopOffset)
			{
				AllDelay += CmdDelay;
				CmdDelay = 0x00;
			}
			while(AllDelay)
			{
				if (AllDelay <= 0xFFFF)
					TempSht = (UINT16)AllDelay;
				else
					TempSht = 0xFFFF;
				
				if (! TempSht)
				{
					// don't do anything - I just want to be safe
				}
				else if (TempSht <= 0x10)
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
				else if ((TempSht >=  735 && TempSht <=  751) || TempSht == 1470)
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
				else if ((TempSht >=  882 && TempSht <=  898) || TempSht == 1764)
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
			AllDelay = CmdDelay;
			CmdDelay = 0x00;
			
			if (VGMPos == VGMHead.lngLoopOffset)
			{
				NewLoopS = DstPos;
				LoopSmplID = LastSmplID;
			}
			
			if (WriteEvent)
			{
				memcpy(&DstData[DstPos], &VGMData[VGMPos], CmdLen);
				DstPos += CmdLen;
			}
			else if (WriteExtra)
			{
				while(StrmCmd < CtrlCmdCount && VGMCtrlCmd[StrmCmd].FileOfs <= VGMPos)
				{
					printdbg("Write DAC command %u, VGMPos 0x%X, DstPos 0x%X\n",
							StrmCmd, VGMPos, DstPos);
					if (VGMCtrlCmd[StrmCmd].Command != 0x94)
					{
						/*if ((INT32)VGMLoopSmplOfs > (UINT32)VGMSmplPos && VGMLoopSmplOfs - VGMSmplPos < 700)
						{
							printf("Warning: Start Stream close to loop!\n");
							printf("Cmd Sample: %u, Loop Sample: %u, Difference: %u\n",
									VGMSmplPos, VGMLoopSmplOfs, VGMLoopSmplOfs - VGMSmplPos);
							_getch();
						}*/
						if (VGMCtrlCmd[StrmCmd].Command == 0x95)
							LastSmplID = VGMCtrlCmd[StrmCmd].DataS1;
						else if (VGMCtrlCmd[StrmCmd].Command == 0x93)
							LastSmplID = (UINT16)-1;
						WriteDACStreamCmd(&VGMCtrlCmd[StrmCmd], &DstPos);
					}
					else
					{
						if (~VGMCtrlCmd[StrmCmd].DataB1 & 0x02)
						{
							WriteDACStreamCmd(&VGMCtrlCmd[StrmCmd], &DstPos);
						}
						if (VGMCtrlCmd[StrmCmd].DataB1 & 0x01)
						{
							memcpy(&DstData[DstPos], &VGMData[VGMPos], CmdLen);
							DstPos += CmdLen;
						}
					}
					StrmCmd ++;
				}
			}
		}
		else
		{
			AllDelay += CmdDelay;
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
			CmdLen = VGMHead.lngEOFOffset - VGMHead.lngDataOffset;
			printf("%04.3f %% - %s / %s (%08X / %08X) ...\r", (float)TempLng / CmdLen * 100,
					MinSecStr, TempStr, VGMPos, VGMHead.lngEOFOffset);
			CmdTimer = GetTickCount() + 200;
		}
#endif
	}
	DataSizeA = VGMPos - VGMHead.lngDataOffset;
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
	printf("\t\t\t\t\t\t\t\t\r");
	
	if (VGMHead.lngGD3Offset)
	{
		VGMPos = VGMHead.lngGD3Offset;
		memcpy(&TempLng, &VGMData[VGMPos + 0x00], 0x04);
		if (TempLng == FCC_GD3)
		{
			memcpy(&CmdLen, &VGMData[VGMPos + 0x08], 0x04);
			CmdLen += 0x0C;
			
			VGMHead.lngGD3Offset = DstPos;
			TempLng = DstPos - 0x14;
			memcpy(&DstData[0x14], &TempLng, 0x04);
			memcpy(&DstData[DstPos], &VGMData[VGMPos], CmdLen);
			DstPos += CmdLen;
		}
	}
	DstDataLen = DstPos;
	
	if (VGMHead.lngVersion < 0x00000160)
	{
		VGMHead.lngVersion = 0x00000160;
		memcpy(&DstData[0x08], &VGMHead.lngVersion, 0x04);
	}
	
	TempLng = DstDataLen - 0x04;
	memcpy(&DstData[0x04], &TempLng, 0x04);
	
	if (NewLoopS)
	{
		if (LoopSmplID != LastSmplID)
		{
			printf("Loop Sample Warning: %02X != %02X\n", LoopSmplID, LastSmplID);
			_getch();
		}
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
