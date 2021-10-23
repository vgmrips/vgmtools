// vgm_trim.c - VGM Trimmer
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "stdtype.h"
#include "stdbool.h"
#include "VGMFile.h"
#include "common.h"

#define BUFFER_SIZE 255
#ifndef FILENAME_MAX
	#define FILENAME_MAX 255
#endif

static bool OpenVGMFile(const char* FileName);
static void WriteVGMFile(const char* FileName);

// Function Prototypes from vgm_trml.c
void SetTrimOptions(UINT8 TrimMode, UINT8 WarnMask);
void TrimVGMData(const INT32 StartSmpl, const INT32 LoopSmpl, const INT32 EndSmpl,
				 const bool HasLoop, const bool KeepESmpl);


VGM_HEADER VGMHead;
UINT32 VGMDataLen;
UINT8* VGMData;
UINT32 VGMPos;
INT32 VGMSmplPos;
UINT8* DstData;
UINT32 DstDataLen;
char FileBase[FILENAME_MAX];

int main(int argc, char* argv[])
{
	int argbase;
	int ErrVal;
	char FileName[FILENAME_MAX];
	char InputTxt[BUFFER_SIZE];
	INT32 StartSmpl;
	INT32 LoopSmpl;
	INT32 EndSmpl;
	bool HasLoop;
	bool KeepLSmpl;
	UINT8 OptsTrim;
	UINT8 OptsWarn;

	printf("VGM Trimmer\n-----------\n\n");

	ErrVal = 0;
	argbase = 1;
	OptsTrim = 0x00;
	OptsWarn = 0x00;
	while(argbase < argc && argv[argbase][0] == '-')
	{
		if (! stricmp(argv[argbase] + 1, "help"))
		{
			printf("Usage: vgm_trim [-state] [-nonotewarn] File.vgm\n");
			printf("                StartSmpl LoopSmpl EndSmpl [OutFile.vgm]\n");
			printf("\n");
			printf("Options:\n");
			printf("    -state: put a save state of the chips at the start of the VGM\n");
			printf("    -NoNoteWarn: don't print warnings about notes playing at EOF\n");
			return 0;
		}
		else if (! stricmp(argv[argbase] + 1, "state"))
		{
			OptsTrim = 0x01;
			argbase ++;
		}
		else if (! stricmp(argv[argbase] + 1, "NoNoteWarn"))
		{
			OptsWarn |= 0x01;
			argbase ++;
		}
		else
		{
			break;
		}
	}

	SetTrimOptions(OptsTrim, ! OptsWarn);

	printf("File Name:\t");
	if (argc <= argbase + 0)
	{
		ReadFilename(FileName, sizeof(FileName));
	}
	else
	{
		strncpy(FileName, argv[argbase + 0], FILENAME_MAX-1);
		printf("%s\n", FileName);
	}
	if (! strlen(FileName))
		return 0;

	printf("Start Sample (in Samples):\t");
	if (argc <= argbase + 1)
	{
		fgets(InputTxt, sizeof(InputTxt), stdin);
	}
	else
	{
		strncpy(InputTxt, argv[argbase + 1], BUFFER_SIZE-1);
		printf("%s\n", InputTxt);
	}
	StartSmpl = strtol(InputTxt, NULL, 0);
	if (! StartSmpl)
		StartSmpl = 0x00;

	printf("Loop Sample (in Samples):\t");
	if (argc <= argbase + 2)
	{
		fgets(InputTxt, sizeof(InputTxt), stdin);
	}
	else
	{
		strncpy(InputTxt, argv[argbase + 2],BUFFER_SIZE-1);
		printf("%s\n", InputTxt);
	}
	LoopSmpl = strtol(InputTxt, NULL, 0);

	printf("End Sample (in Samples):\t");
	if (argc <= argbase + 3)
	{
		fgets(InputTxt, sizeof(InputTxt), stdin);
	}
	else
	{
		strncpy(InputTxt, argv[argbase + 3], BUFFER_SIZE-1);
		printf("%s\n", InputTxt);
	}
	EndSmpl = strtol(InputTxt, NULL, 0);

	if (! OpenVGMFile(FileName))
	{
		printf("Error opening the file!\n");
		ErrVal = 1;
		goto EndProgram;
	}
	printf("\n");

	KeepLSmpl = false;
	if (EndSmpl == -1)
	{
		EndSmpl = VGMHead.lngTotalSamples;
		KeepLSmpl = true;
	}
	if (StartSmpl >= (INT32)VGMHead.lngTotalSamples)
	{
		printf("Error: Start Sample beyond End of File!\n");
		ErrVal = 4;
		goto QuickExit;
	}
	if (EndSmpl <= StartSmpl)
	{
		printf("Error: End Sample <= Start Sample!\n");
		ErrVal = 5;
		goto QuickExit;
	}
	if (LoopSmpl)
	{
		if (LoopSmpl < StartSmpl && LoopSmpl != -1 && LoopSmpl != -2)
		{
			LoopSmpl = StartSmpl;
			printf("Warining: Loop Sample before Start Sample - Loop Sample moved!\n");
		}
		if (LoopSmpl == -2 &&
			StartSmpl > (INT32)(VGMHead.lngTotalSamples - VGMHead.lngLoopSamples))
		{
			LoopSmpl = StartSmpl;
			printf("Error: Old Loop Sample before new Start Sample!\n");
			ErrVal = 6;
			goto QuickExit;
		}
		if (LoopSmpl >= EndSmpl)
		{
			LoopSmpl = 0x00;
			printf("Warining: Loop Sample after End Sample - Loop disabled!\n");
		}
	}
	if (! LoopSmpl)
		KeepLSmpl = true;
	if (StartSmpl < 0)
		printf("Warning: Negative Start Sample - Silence added!\n");
	if ((UINT32)EndSmpl > VGMHead.lngTotalSamples)
		printf("Warning: End Sample after End of File - Silence added!\n");

	if (LoopSmpl == -1)
	{
		LoopSmpl = StartSmpl;
		HasLoop = true;
	}
	else if (LoopSmpl == -2)
	{
		LoopSmpl = VGMHead.lngTotalSamples - VGMHead.lngLoopSamples;
		HasLoop = VGMHead.lngLoopOffset ? true : false;
		KeepLSmpl = ! HasLoop;
	}
	else
	{
		HasLoop = LoopSmpl ? true : false;
	}

	TrimVGMData(StartSmpl, LoopSmpl, EndSmpl, HasLoop, KeepLSmpl);
	if (argc > argbase + 4)
		strncpy(FileName, argv[argbase + 4], FILENAME_MAX-1);
	else
		strncpy(FileName, "",FILENAME_MAX-1);
	if (FileName[0] == '\0')
	{
		snprintf(FileName, FILENAME_MAX, "%s_trimmed.vgm", FileBase);
		//strcpy(FileName, FileBase);
		//strcat(FileName, "_trimmed.vgm");
	}
	WriteVGMFile(FileName);

QuickExit:
	free(VGMData);
	free(DstData);

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
		VGMHead.lngHzYM2612 = VGMHead.lngHzYM2413;
		VGMHead.lngHzYM2151 = VGMHead.lngHzYM2413;
	}
	if (VGMHead.lngVersion < 0x00000150)
	{
		VGMHead.lngDataOffset = 0x00000000;
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

	strncpy(FileBase, FileName,FILENAME_MAX-1);
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
	const char* FileTitle;

	printf("\t\t\t\t\t\t\t\t\r");
	FileTitle = strrchr(FileName, '\\');
	if (FileTitle == NULL)
		FileTitle = FileName;
	else
		FileTitle ++;

	hFile = fopen(FileName, "wb");
	if (hFile == NULL)
	{
		printf("Error writing %s!\n", FileTitle);
		return;
	}

	fwrite(DstData, 0x01, DstDataLen, hFile);
	fclose(hFile);
	printf("%s written.\n", FileTitle);

	return;
}
