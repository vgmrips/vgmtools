// vgm_vol.c - VGM Volume Detector
//

#include "compat.h"
#include <stdio.h>
#include <stdlib.h>
#include "stdbool.h"
#include <string.h>
#include <math.h>

#ifdef WIN32

#include <conio.h>
#include <windows.h>	// for Directory Listing

#else

// Note: Directory Listing is only supported in Windows
#include <limits.h>
#define MAX_PATH	PATH_MAX

#endif

#ifndef M_LN2
#define	M_LN2		0.69314718055994530942
#endif

#include "stdtype.h"


static void ReadWAVFile(const char* FileName);
static void ReadDirectory(const char* DirName);
static void ReadPlaylist(const char* FileName);
static void PrintVolMod(UINT16 MaxLvl);
static INT8 stricmp_u(const char *string1, const char *string2);


#define FCC_RIFF	0x46464952
#define FCC_WAVE	0x45564157
#define FCC_fmt		0x20746D66
#define FCC_data	0x61746164

#ifndef WAVE_FORMAT_PCM
	#define WAVE_FORMAT_PCM	0x0001
#endif


float RecVolume;	// usually 1.0 or 0.5 
UINT32 VGMDataLen;
char FilePath[MAX_PATH];
char* FileTitle;
UINT16 MaxLvlAlbum;
bool IsPlayList;

int main(int argc, char* argv[])
{
	int ErrVal;
	char FileName[MAX_PATH];
	char InputStr[0x100];
#ifdef WIN32
	char* FileExt;
	bool PLMode;
#endif
	
	printf("VGM Volume Detector\n-------------------\n\n");
	
	ErrVal = 0;
#ifdef WIN32
	printf("File Path or PlayList:\t");
#else
	printf("PlayList:\t");
#endif
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
		return 0;
	
	printf("Volume Setting (default 1.0):\t");
	if (argc <= 0x02)
	{
		gets_s(InputStr, sizeof(InputStr));
	}
	else
	{
		strcpy(InputStr, argv[0x02]);
		printf("%s\n", InputStr);
	}
	RecVolume = (float)strtod(InputStr, NULL);
	if (RecVolume == 0.0f)
		RecVolume = 1.0f;
	
#ifdef WIN32
	PLMode = false;
	
	if (FileName[strlen(FileName - 0x01)] != '\\')
	{
		if (! (GetFileAttributes(FileName) & FILE_ATTRIBUTE_DIRECTORY))
		{
			FileExt = strrchr(FileName, '.');
			if (FileExt < strrchr(FileName, '\\'))
				FileExt = NULL;
			
			if (FileExt != NULL)
			{
				FileExt ++;
				if (! stricmp_u(FileExt, "m3u"))
					PLMode = true;
			}
		}
		else
		{
			strcat(FileName, "\\");
		}
	}
	
	MaxLvlAlbum = 0x0000;
	IsPlayList = PLMode;
	if (! PLMode)
		ReadDirectory(FileName);
	else
		ReadPlaylist(FileName);
#else
	// Sorry - no directory browsing for Linux
	MaxLvlAlbum = 0x0000;
	IsPlayList = true;
	ReadPlaylist(FileName);
#endif
	
	printf("\nAll tracks\t");
	PrintVolMod(MaxLvlAlbum);
	printf("\n");
	
//EndProgram:
	waitkey(argv[0]);
	
	return ErrVal;
}

static void ReadWAVFile(const char* FileName)
{
	FILE* hFile;
	UINT32 fccHeader;
	UINT32 CurPos;
	UINT16 TempSht;
	INT16 TempSSht;
	UINT32 TempLng;
	UINT16 MaxLvl;
	
	printf("%s\t", FileTitle);
	hFile = fopen(FileName, "rb");
	if (hFile == NULL)
	{
		printf("Error opening file!\n");
		return;
	}
	
	fseek(hFile, 0x00, SEEK_SET);
	fread(&fccHeader, 0x04, 0x01, hFile);
	if (fccHeader != FCC_RIFF)
	{
		printf("No Wave file (RIFF signature missing)!\n");
		goto OpenErr;
	}
	fread(&TempLng, 0x04, 0x01, hFile);
	
	fread(&fccHeader, 0x04, 0x01, hFile);
	if (fccHeader != FCC_WAVE)
	{
		printf("No Wave file (WAVE signature missing)!\n");
		goto OpenErr;
	}
	fread(&fccHeader, 0x04, 0x01, hFile);
	if (fccHeader != FCC_fmt)
	{
		printf("Bad Wave file (bad format header)!\n");
		goto OpenErr;
	}
	fread(&TempLng, 0x04, 0x01, hFile);	// get format tag length
	
	// read format data
	CurPos = ftell(hFile);
	fread(&TempSht, 0x02, 0x01, hFile);
	if (TempSht != WAVE_FORMAT_PCM)
	{
		printf("Can't read compressed wave files!\n");
		goto OpenErr;
	}
	fseek(hFile, 0x0C, SEEK_CUR);
	fread(&TempSht, 0x02, 0x01, hFile);
	if (TempSht != 0x10)
	{
		printf("Must be an 16-bit wave file!\n");
		goto OpenErr;
	}
	fseek(hFile, CurPos + TempLng, SEEK_SET);
	
	// read data
	fread(&fccHeader, 0x04, 0x01, hFile);
	while(fccHeader != FCC_data)
	{
		fread(&TempLng, 0x04, 0x01, hFile);
		fseek(hFile, TempLng, SEEK_CUR);
		
		if (! fread(&fccHeader, 0x04, 0x01, hFile))
		{
			printf("Unable to find wave data!\n");
			goto OpenErr;
		}
	}
	
	fread(&TempLng, 0x04, 0x01, hFile);	// get data length
	CurPos = 0x00;
	MaxLvl = 0x0000;
	while(CurPos < TempLng)
	{
		if (! fread(&TempSSht, 0x02, 0x01, hFile))
			break;	// early file end
		CurPos += 0x02;
		
		if (abs(TempSSht) > MaxLvl)
		{
			MaxLvl = abs(TempSSht);
			if (TempSht == 0x8000 || TempSht == 0x7FFF)
			{
				printf("Sound clipped! Please relog with lower volume!\n");
				break;
			}
		}
	}
	
	fclose(hFile);
	
	PrintVolMod(MaxLvl);
	if (MaxLvl > MaxLvlAlbum)
		MaxLvlAlbum = MaxLvl;
	
	return;

OpenErr:

	fclose(hFile);
	return;
}

static void PrintVolMod(UINT16 MaxLvl)
{
	float Factor;
	INT16 VolMod;
	
	if (! MaxLvl)
	{
		printf("--\n");
		return;
	}
	
	Factor = (float)0x8000 / MaxLvl * RecVolume;
	if (Factor < 0.25f)
		Factor = 0.25f;
	else if (Factor > 64.0f)
		Factor = 64.0f;
	VolMod = (INT16)floor(log(Factor) / M_LN2 * 0x20);
	if (VolMod == -0x0040)
		VolMod = -0x003F;
	VolMod &= 0xFF;
	printf("MaxLevel: %04X\tFactor: %.3f\tVolMod: 0x%02X\n", MaxLvl, Factor, VolMod);
	
	return;
}

#ifdef WIN32
static void ReadDirectory(const char* DirName)
{
	HANDLE hFindFile;
	WIN32_FIND_DATA FindFileData;
	BOOL RetVal;
	char FileName[MAX_PATH];
	char* TempPnt;
	char* FileExt;
	
	strcpy(FilePath, DirName);
	TempPnt = strrchr(FilePath, '\\');
	if (TempPnt == NULL)
		strcpy(FilePath, "");
	
	TempPnt = strrchr(FilePath, '\\');
	if (TempPnt != NULL)
		TempPnt[0x01] = 0x00;
	strcpy(FileName, FilePath);
	strcat(FileName, "*.wav");

	hFindFile = FindFirstFile(FileName, &FindFileData);
	if (hFindFile == INVALID_HANDLE_VALUE)
	{
		//ShowErrMessage();
		printf("Error reading directory!\n");
		return;
	}
	strcpy(FileName, FilePath);
	TempPnt = FileName + strlen(FileName);
	
	do
	{
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			goto SkipFile;
		
		FileExt = strrchr(FindFileData.cFileName, '.');
		if (FileExt == NULL)
			goto SkipFile;
		FileExt ++;
		
		if (stricmp_u(FileExt, "wav"))
			goto SkipFile;
		
		strcpy(TempPnt, FindFileData.cFileName);
		FileTitle = TempPnt;
		ReadWAVFile(FileName);
		
SkipFile:
		RetVal = FindNextFile(hFindFile, &FindFileData);
	}
	while(RetVal);
	
	RetVal = FindClose(hFindFile);
	
	return;
}
#endif

static void ReadPlaylist(const char* FileName)
{
	const char M3UV2_HEAD[] = "#EXTM3U";
	const char M3UV2_META[] = "#EXTINF:";
	UINT32 METASTR_LEN;
	
	FILE* hFile;
	UINT32 LineNo;
	bool IsV2Fmt;
	char TempStr[MAX_PATH];
	char FileVGM[MAX_PATH];
	char* RetStr;
	
	RetStr = strrchr(FileName, '\\');
	if (RetStr != NULL)
	{
		RetStr ++;
		strncpy(TempStr, FileName, RetStr - FileName);
		TempStr[RetStr - FileName] = 0x00;
		strcpy(FilePath, TempStr);
	}
	else
	{
		strcpy(FilePath, "");
	}
	
	hFile = fopen(FileName, "rt");
	if (hFile == NULL)
		return;
	
	LineNo = 0x00;
	IsV2Fmt = false;
	METASTR_LEN = strlen(M3UV2_META);
	while(! feof(hFile))
	{
		RetStr = fgets(TempStr, MAX_PATH, hFile);
		if (RetStr == NULL)
			break;
		//RetStr = strchr(TempStr, 0x0D);
		//if (RetStr)
		//	*RetStr = 0x00;	// remove NewLine-Character
		RetStr = TempStr + strlen(TempStr) - 0x01;
		while(*RetStr < 0x20)
		{
			*RetStr = 0x00;	// remove NewLine-Characters
			RetStr --;
		}
		if (! strlen(TempStr))
			continue;
		
		if (! LineNo)
		{
			if (! strcmp(TempStr, M3UV2_HEAD))
			{
				IsV2Fmt = true;
				LineNo ++;
				continue;
			}
		}
		if (IsV2Fmt)
		{
			if (! strncmp(TempStr, M3UV2_META, METASTR_LEN))
			{
				// Ignore Metadata of m3u Version 2
				LineNo ++;
				continue;
			}
		}
		
		RetStr = strrchr(TempStr, '.');
		if (RetStr != NULL)
			strcpy(RetStr + 1, "wav");
		
		strcpy(FileVGM, FilePath);
		strcat(FileVGM, TempStr);
		RetStr = strrchr(TempStr, '\\');
		if (RetStr == NULL)
			RetStr = TempStr;
		else
			RetStr ++;
		FileTitle = RetStr;
		
		ReadWAVFile(FileVGM);
		LineNo ++;
	}
	
	fclose(hFile);
	
	return;
}

static INT8 stricmp_u(const char *string1, const char *string2)
{
	// my own stricmp, because VC++6 doesn't find _stricmp when compiling without
	// standard librarys
	const char* StrPnt1;
	const char* StrPnt2;
	char StrChr1;
	char StrChr2;
	
	StrPnt1 = string1;
	StrPnt2 = string2;
	while(true)
	{
		StrChr1 = toupper(*StrPnt1);
		StrChr2 = toupper(*StrPnt2);
		
		if (StrChr1 < StrChr2)
			return -1;
		else if (StrChr1 > StrChr2)
			return +1;
		if (StrChr1 == 0x00)
			return 0;
		
		StrPnt1 ++;
		StrPnt2 ++;
	}
	
	return 0;
}
