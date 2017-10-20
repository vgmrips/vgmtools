// vgm_stat.c - VGM Statistics
//
// TODO: Proper hours support.
// TODO: Fix UTF-8 rendering (multibyte characters aren't handled correctly)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>	// for isalnum()
#include <wchar.h>
#include <locale.h>	// for setlocale()
#include <zlib.h>

#ifdef WIN32
#include <windows.h>	// for Directory Listing
#else
#include <glob.h>
#include <sys/stat.h>
#endif

#include "stdtype.h"
#include "stdbool.h"
#include "VGMFile.h"
#include "common.h"


//#define SHOW_FILE_STATS


#ifdef WIN32
#define DIR_SEP	'\\'
#else
#define DIR_SEP	'/'
#endif


static bool OpenVGMFile(const char* FileName);
static wchar_t* ReadWStrFromFile(gzFile hFile, UINT32* FilePos, UINT32 EOFPos);
static void ReadDirectory(const char* DirName);
static void ReadPlaylist(const char* FileName);
#ifdef SHOW_FILE_STATS
static void ShowFileStats(char* FileTitle);
#endif
static void PrintSampleTime(char* buffer, const UINT32 Samples, bool LoopMode);
static void ShowStatistics(void);
UINT32 GetTitleLines(UINT32* StrAlloc, char** String, const char* TitleStr);


typedef struct track_list
{
	char* Title;
	UINT32 SmplTotal;
	UINT32 SmplLoop;
} TRACK_LIST;


VGM_HEADER VGMHead;
GD3_TAG VGMTag;
UINT32 VGMDataLen;
char FilePath[MAX_PATH];
UINT32 AllTotal;
UINT32 AllLoop;
UINT32 TrackCount;
UINT32 TrkCntDigits;
UINT32 TrackAlloc;
TRACK_LIST* TrackList;
TRACK_LIST* CurTrkEntry;
bool IsPlayList;

int main(int argc, char* argv[])
{
	int ErrVal;
	char FileName[MAX_PATH];
	char* FileExt;
	bool PLMode;
	UINT32 TempLng;
	
	setlocale(LC_CTYPE, "");	// set to use system codepage
	
	printf("VGM Statistics\n--------------\n\n");
	
//printf("ZLib Version: %s\n", zlibVersion());
	ErrVal = 0;
	printf("File Path or PlayList:\t");
	if (argc <= 0x01)
	{
		ReadFilename(FileName, sizeof(FileName));
	}
	else
	{
		strcpy(FileName, argv[0x01]);
		printf("%s\n", FileName);
	}
	if (! strlen(FileName))
		return 0;
	
	AllTotal = 0;
	AllLoop = 0;
	PLMode = false;
	
	if (FileName[strlen(FileName) - 1] != DIR_SEP)
	{
#ifdef WIN32
		if (! (GetFileAttributes(FileName) & FILE_ATTRIBUTE_DIRECTORY))
#else
		struct stat st;
		if(!stat(FileName, &st) && S_ISREG(st.st_mode))
#endif
		{
			FileExt = strrchr(FileName, '.');
			if (FileExt < strrchr(FileName, DIR_SEP))
				FileExt = NULL;
			
			if (FileExt != NULL)
			{
				FileExt ++;
				if (! stricmp(FileExt, "m3u"))
					PLMode = true;
			}
		}
		else
		{
			strcat(FileName, "/");
		}
	}
	
	TrackCount = 0;
	TrackAlloc = 0;
	TrackList = NULL;
	IsPlayList = PLMode;
	if (! PLMode)
		ReadDirectory(FileName);
	else
		ReadPlaylist(FileName);
	
	TempLng = TrackCount;
	TrkCntDigits = 0;
	do
	{
		TempLng /= 10;
		TrkCntDigits ++;
	} while(TempLng);
	if (TrkCntDigits < 2)
		TrkCntDigits = 2;
	
#ifdef SHOW_FILE_STATS
	ShowFileStats(NULL);
	printf("\n\n");
#endif
	
	ShowStatistics();
	
//EndProgram:
	DblClickWait(argv[0]);
	
	return ErrVal;
}

static bool OpenVGMFile(const char* FileName)
{
	gzFile hFile;
	UINT32 fccHeader;
	UINT32 CurPos;
	UINT32 TempLng;
	char* TempPnt;
	
	hFile = gzopen(FileName, "rb");
	if (hFile == NULL)
		return false;
	
	gzseek(hFile, 0x00, SEEK_SET);
	gzread(hFile, &fccHeader, 0x04);
	if (fccHeader != FCC_VGM)
		goto OpenErr;
	
	if (gzseek(hFile, 0x00, SEEK_SET) == -1)
	{
		printf("gzseek Error!!\n");
		if (gzrewind(hFile) == -1)
		{
			printf("gzrewind Error!!\n");
			goto OpenErr;
		}
	}
	TempLng = gztell(hFile);
	if (TempLng != 0)
	{
		printf("gztell returns invalid offset: 0x%X\n", TempLng);
		goto OpenErr;
	}
	gzread(hFile, &VGMHead, sizeof(VGM_HEADER));
	ZLIB_SEEKBUG_CHECK(VGMHead);
	
	// relative -> absolute addresses
	VGMHead.lngEOFOffset += 0x00000004;
	if (VGMHead.lngGD3Offset)
		VGMHead.lngGD3Offset += 0x00000014;
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
	
	if (! VGMHead.bytLoopModifier)
		VGMHead.bytLoopModifier = 0x10;
	
//printf("\tTrack %u, Data Ofs 0x%X, Version %X\n", TrackCount, VGMHead.lngDataOffset, VGMHead.lngVersion);
//printf("\tSamples Total: %u, Samples Loop: %u\n", VGMHead.lngTotalSamples, VGMHead.lngLoopOffset);
//printf("\tGD3 Offset: 0x%X, EOF Offset: 0x%X\n", VGMHead.lngGD3Offset, VGMHead.lngEOFOffset);
	// Allocate Memory for Track List
	if (TrackAlloc <= TrackCount)
	{
		TrackAlloc += 0x100;
		TrackList = (TRACK_LIST*)realloc(TrackList, sizeof(TRACK_LIST) * TrackAlloc);
	}
	CurTrkEntry = &TrackList[TrackCount];
	TrackCount ++;
	
	// Read GD3 Tag
	if (VGMHead.lngGD3Offset)
	{
		gzseek(hFile, VGMHead.lngGD3Offset, SEEK_SET);
		gzread(hFile, &fccHeader, 0x04);
		if (fccHeader != FCC_GD3)
			VGMHead.lngGD3Offset = 0x00000000;
			//goto OpenErr;
	}
	
	if (VGMHead.lngGD3Offset)
	{
		CurPos = VGMHead.lngGD3Offset;
		gzseek(hFile, CurPos, SEEK_SET);
		gzread(hFile, &VGMTag, 0x0C);
		CurPos += 0x0C;
		TempLng = CurPos + VGMTag.lngTagLength;
		VGMTag.strTrackNameE = ReadWStrFromFile(hFile, &CurPos, TempLng);
		// I only need the English Track Title
		/*VGMTag.strTrackNameJ = ReadWStrFromFile(hFile, &CurPos, TempLng);
		VGMTag.strGameNameE = ReadWStrFromFile(hFile, &CurPos, TempLng);
		VGMTag.strGameNameJ = ReadWStrFromFile(hFile, &CurPos, TempLng);
		VGMTag.strSystemNameE = ReadWStrFromFile(hFile, &CurPos, TempLng);
		VGMTag.strSystemNameJ = ReadWStrFromFile(hFile, &CurPos, TempLng);
		VGMTag.strAuthorNameE = ReadWStrFromFile(hFile, &CurPos, TempLng);
		VGMTag.strAuthorNameJ = ReadWStrFromFile(hFile, &CurPos, TempLng);
		VGMTag.strReleaseDate = ReadWStrFromFile(hFile, &CurPos, TempLng);
		VGMTag.strCreator = ReadWStrFromFile(hFile, &CurPos, TempLng);
		VGMTag.strNotes = ReadWStrFromFile(hFile, &CurPos, TempLng);*/
	}
	else
	{
		VGMTag.strTrackNameE = L"";
	}
		
	TempLng = wcslen(VGMTag.strTrackNameE);
	if (TempLng)
	{
#ifdef WIN32
		UINT32 CPMode;
		CurTrkEntry->Title = (char*)malloc(TempLng + 0x01);
		if (ftell(stdout) == -1) // Outputting to console
			CPMode = GetConsoleOutputCP();
		else // Outputting to file
			CPMode = CP_ACP;
		WideCharToMultiByte(CPMode, 0x00, VGMTag.strTrackNameE, -1, CurTrkEntry->Title, TempLng + 0x01, NULL, NULL);
#else
		CurTrkEntry->Title = (char*)malloc(TempLng + 0x01);
		wcstombs(CurTrkEntry->Title, VGMTag.strTrackNameE, TempLng);
#endif
		CurTrkEntry->Title[TempLng] = '\0';
	}
	else
	{
		TempPnt = strrchr(FileName, '\\');
		if (TempPnt == NULL)
			TempPnt = (char*)FileName;
		else
			TempPnt ++;
		TempLng = strlen(TempPnt);
		CurTrkEntry->Title = (char*)malloc(TempLng + 0x01);
		strcpy(CurTrkEntry->Title, TempPnt);
		TempPnt = strrchr(CurTrkEntry->Title, '.');
		if (TempPnt != NULL)
			*TempPnt = '\0';	// strip ".vgm"
	}
	CurTrkEntry->SmplTotal = VGMHead.lngTotalSamples;
	CurTrkEntry->SmplLoop = VGMHead.lngLoopSamples;
	
	gzclose(hFile);
	
	return true;

OpenErr:

	gzclose(hFile);
	return false;
}

static wchar_t* ReadWStrFromFile(gzFile hFile, UINT32* FilePos, UINT32 EOFPos)
{
	UINT32 CurPos;
	wchar_t* TextStr;
	wchar_t* TempStr;
	UINT32 StrLen;
	UINT16 UnicodeChr;

	// Unicode 2-Byte -> 4-Byte conversion is not neccessary,
	// but it's easier to handle wchar_t than unsigned short
	// (note: wchar_t is 16-bit on Windows, but 32-bit on Linux)
	CurPos = *FilePos;
	TextStr = (wchar_t*)malloc((EOFPos - CurPos) / 0x02 * sizeof(wchar_t));
	if (TextStr == NULL)
		return NULL;
	
	gzseek(hFile, CurPos, SEEK_SET);
	TempStr = TextStr;
	StrLen = 0x00;
	do
	{
		gzread(hFile, &UnicodeChr, 0x02);
		*TempStr = (wchar_t)UnicodeChr;
		TempStr ++;
		CurPos += 0x02;
		StrLen ++;
		if (CurPos >= EOFPos)
			break;
	} while(*(TempStr - 1));
	
	TextStr = (wchar_t*)realloc(TextStr, StrLen * sizeof(wchar_t));
	*FilePos = CurPos;
	
	return TextStr;
}

static void ReadDirectory(const char* DirName)
{
#ifdef WIN32
	HANDLE hFindFile;
	WIN32_FIND_DATA FindFileData;
	BOOL RetVal;
#else
	struct stat st;
	int i;
#endif
	char FileName[MAX_PATH];
	char* TempPnt;
	char* FileExt;

	strcpy(FilePath, DirName);
	TempPnt = strrchr(FilePath, DIR_SEP);
	if (TempPnt == NULL)
		strcpy(FilePath, "");

	TempPnt = strrchr(FilePath, DIR_SEP);
	if (TempPnt != NULL)
		TempPnt[0x01] = '\0';
	strcpy(FileName, FilePath);
	strcat(FileName, "*.vg?");
//printf("  Base Path: %s\n", FilePath);

#ifdef WIN32
	hFindFile = FindFirstFile(FileName, &FindFileData);
	if (hFindFile == INVALID_HANDLE_VALUE)
	{
		//ShowErrMessage();
		printf("Error reading directory!\n");
		return;
	}
#else
	glob_t globbuf;
	glob(FileName, 0, NULL, &globbuf);
#endif
	strcpy(FileName, FilePath);
	TempPnt = FileName + strlen(FileName);
	
#ifdef SHOW_FILE_STATS
	printf("\t\t    Sample\t    Time\n");
	printf("File Title\tTotal\tLoop\tTotal\tLoop\n\n");
#endif

#ifdef WIN32
	do
	{
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			goto SkipFile;
		FileExt = strrchr(FindFileData.cFileName, '.');
		if (FileExt == NULL)
			goto SkipFile;
		FileExt ++;

		if (stricmp(FileExt, "vgm") && stricmp(FileExt, "vgz"))
			goto SkipFile;

		strcpy(TempPnt, FindFileData.cFileName);
		if (! OpenVGMFile(FileName))
			printf("%s\tError opening the file!\n", TempPnt);
#ifdef SHOW_FILE_STATS
		else
			ShowFileStats(TempPnt);
#endif
SkipFile:
		RetVal = FindNextFile(hFindFile, &FindFileData);
	}
	while(RetVal);
#else
	for(i = 0; i < globbuf.gl_pathc; i++) {
		if(!stat(globbuf.gl_pathv[i], &st)) {
			FileExt = strrchr(globbuf.gl_pathv[i], '.');
			if (FileExt == NULL)
				continue;
			FileExt ++;

			if(stricmp(FileExt, "vgm") && stricmp(FileExt, "vgz"))
				continue;

			strcpy(TempPnt, globbuf.gl_pathv[i]);
			if (! OpenVGMFile(globbuf.gl_pathv[i]))
				printf("%s\tError opening the file!\n", TempPnt);
#ifdef SHOW_FILE_STATS
			else
				ShowFileStats(TempPnt);
#endif
		}
	}
#endif

#ifdef WIN32
	RetVal = FindClose(hFindFile);
#else
	globfree(&globbuf);
#endif

	return;
}

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
		TempStr[RetStr - FileName] = '\0';
		strcpy(FilePath, TempStr);
	}
	else
	{
		strcpy(FilePath, "");
	}
//printf("  Base Path: %s\n", FilePath);
	
	hFile = fopen(FileName, "rt");
	if (hFile == NULL)
		return;
	
#ifdef SHOW_FILE_STATS
	printf("\t\t    Sample\t    Time\n");
	printf("File Title\tTotal\tLoop\tTotal\tLoop\n\n");
#endif
	LineNo = 0;
	IsV2Fmt = false;
	METASTR_LEN = strlen(M3UV2_META);
	while(! feof(hFile))
	{
		RetStr = fgets(TempStr, MAX_PATH, hFile);
		if (RetStr == NULL)
			break;
		//RetStr = strchr(TempStr, 0x0D);
		//if (RetStr)
		//	*RetStr = '\0';	// remove NewLine-Character
		RetStr = TempStr + strlen(TempStr) - 0x01;
		while(*RetStr < 0x20)
		{
			*RetStr = '\0';	// remove NewLine-Characters
			RetStr --;
		}
		if (! strlen(TempStr))
			continue;
		
//printf("  Read Line: %s\n", TempStr);
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
		
		strcpy(FileVGM, FilePath);
		strcat(FileVGM, TempStr);
		RetStr = strrchr(TempStr, '\\');
		if (RetStr == NULL)
			RetStr = TempStr;
		else
			RetStr ++;
		
//printf("  Full File Path: %s\n", FileVGM);
		if (! OpenVGMFile(FileVGM))
			printf("%s\tError opening the file!\n", RetStr);
#ifdef SHOW_FILE_STATS
		else
			ShowFileStats(RetStr);
#endif
		LineNo ++;
	}
	
	fclose(hFile);
	
	return;
}

#ifdef SHOW_FILE_STATS
static void ShowFileStats(char* FileTitle)
{
	UINT32 SmplTotal;
	UINT32 SmplLoop;
	char TimeTotal[0x10];
	char TimeLoop[0x10];
	INT16 LoopCnt;
	
	if (FileTitle != NULL)
	{
		SmplTotal = VGMHead.lngTotalSamples;
		SmplLoop = VGMHead.lngLoopSamples;
		
		AllTotal += SmplTotal;
		LoopCnt = (2 * VGMHead.bytLoopModifier + 0x08) / 0x10 - VGMHead.bytLoopBase;
		if (LoopCnt < 1)
			LoopCnt = 1;
		AllLoop += SmplLoop * (LoopCnt - 1);
	}
	else
	{
		FileTitle = "\nTotal Length";
		SmplTotal = AllTotal;
		SmplLoop = AllTotal + AllLoop;
	}
	PrintSampleTime(TimeTotal, SmplTotal, false);
	PrintSampleTime(TimeLoop, SmplLoop, true);
	
	printf("%s\t%u\t%u\t%s\t%s\n", FileTitle, SmplTotal, SmplLoop, TimeTotal, TimeLoop);
	
	return;
}
#endif

static void PrintSampleTime(char* buffer, const UINT32 Samples, bool LoopMode)
{
	UINT32 SmplVal;
	UINT16 HourVal;
	UINT16 MinVal;
	UINT16 SecVal;
	
	if (! Samples && LoopMode)
	{
		sprintf(buffer, " -");
		return;
	}
	
	SmplVal = Samples + 22050;	// Round to whole seconds
	SmplVal /= 44100;
	SecVal = (UINT16)(SmplVal % 60);
	SmplVal /= 60;
	MinVal = (UINT16)(SmplVal % 60);
	SmplVal /= 60;
	HourVal = (UINT16)SmplVal;
	
	if (HourVal)
		sprintf(buffer, "%2u:%02u:%02u", HourVal, MinVal, SecVal);
	else
		sprintf(buffer, "%2u:%02u", MinVal, SecVal);
	
	return;
}

#define MAX_TITLE_CHARS		35
static void ShowStatistics(void)
{
	UINT32 CurTL;
	char* TitleStr;
	UINT32 TitleLen;
	INT32 Spaces;
	char TimeTotal[0x10];
	char TimeLoop[0x10];
	//char* TempPnt;
	UINT32 TempStrAlloc;
	char* TempStr;
	UINT32 CurLine;
	UINT32 LineCnt;
	
	TempStrAlloc = 0x00;
	TempStr = NULL;
	
	printf("Song                        Length |Total |Loop\n");
	printf("-----------------------------------+------+----\n");
	for (CurTL = 0; CurTL < TrackCount; CurTL ++)
	{
		char* TitleWithNumber;
		CurTrkEntry = &TrackList[CurTL];
		
		if (IsPlayList)
		{
			TitleWithNumber = (char*)malloc(TrkCntDigits + 1 + strlen(CurTrkEntry->Title) + 1);
			sprintf(TitleWithNumber, "%0*u %s", TrkCntDigits, 1 + CurTL, CurTrkEntry->Title);
		}
		else
		{
			TitleWithNumber = strdup(CurTrkEntry->Title);
		}
		
		LineCnt = GetTitleLines(&TempStrAlloc, &TempStr, TitleWithNumber);
		TitleStr = (LineCnt) ? TempStr : TitleWithNumber;
		if (! LineCnt)
			LineCnt ++;
		
		for (CurLine = 0; CurLine < LineCnt; CurLine ++)
		{
			if (CurLine)
				printf("\n");
			
			TitleLen = strlen(TitleStr);
			if (TitleLen <= MAX_TITLE_CHARS)
				Spaces = MAX_TITLE_CHARS - TitleLen;
			else
				Spaces = 0;
			
			printf("%.*s", MAX_TITLE_CHARS, TitleStr);
			
			TitleStr += TitleLen + 1;
		}
		
		while(Spaces)
		{
			printf(" ");
			Spaces --;
		}
		
		PrintSampleTime(TimeTotal, CurTrkEntry->SmplTotal, false);
		PrintSampleTime(TimeLoop, CurTrkEntry->SmplLoop, true);
		
		printf("%s  %s\n", TimeTotal, TimeLoop);
		free(TitleWithNumber);
	}
	printf("\n");
	
	if (TempStr != NULL)
	{
		free(TempStr);	TempStr = NULL;
	}
	
	TitleStr = "Total Length";
	TitleLen = strlen(TitleStr);
	Spaces = MAX_TITLE_CHARS - TitleLen;
	
	printf("%.*s", MAX_TITLE_CHARS, TitleStr);
	while(Spaces)
	{
		printf(" ");
		Spaces --;
	}
	
	PrintSampleTime(TimeTotal, AllTotal, false);
	PrintSampleTime(TimeLoop, AllTotal + AllLoop, false);
	
	printf("%s  %s\n", TimeTotal, TimeLoop);
	
	return;
}

UINT32 GetTitleLines(UINT32* StrAlloc, char** String, const char* TitleStr)
{
	UINT32 CurLine;
	UINT32 CurPos;
	UINT32 SpcPos;
	UINT32 NoAlphaPos;
	const char* SrcPtr;
	char* DstPtr;
	
	CurPos = strlen(TitleStr);
	if (CurPos <= MAX_TITLE_CHARS)
		return 0;	// didn't do anything
	
	if (*StrAlloc < CurPos + 0x10)
	{
		*StrAlloc = (CurPos + 0x20) & ~0x0F;
		*String = (char*)realloc(*String, *StrAlloc);
	}
	
	SrcPtr = TitleStr;
	DstPtr = *String;
	NoAlphaPos = SpcPos = CurPos = 0;
	CurLine = 0x01;
	if (IsPlayList)
	{
		for (CurPos = 0; CurPos < TrkCntDigits + 1; CurPos ++)
			DstPtr[CurPos] = SrcPtr[CurPos];
	}
	while(SrcPtr[CurPos] != '\0')
	{
		if (SrcPtr[CurPos] == ' ')
			SpcPos = CurPos;
		
		if (CurPos >= MAX_TITLE_CHARS)
		{
			if (SpcPos)	// Space found
			{
				// replace last space with line break (which is \0 here)
				CurPos = SpcPos;
				DstPtr[CurPos] = '\0';
				SrcPtr += CurPos + 1;
				DstPtr += CurPos + 1;
			}
			else if (NoAlphaPos)	// non-alnum char found
			{
				// break after last non-alpha-numeric character
				CurPos = NoAlphaPos + 1;
				DstPtr[CurPos] = '\0';
				SrcPtr += CurPos;
				DstPtr += CurPos + 1;
			}
			else	// word with more chars than the line has
			{
				// break right after the previous letter and insert a -
				DstPtr[CurPos - 1] = '-';
				DstPtr[CurPos] = '\0';
				SrcPtr += CurPos - 1;
				DstPtr += CurPos + 1;
			}
			
			CurPos = (DstPtr - *String) + strlen(SrcPtr);
			if (*StrAlloc < CurPos + 0x10)
			{
				*StrAlloc = (CurPos + 0x20) & ~0x0F;
				SpcPos = DstPtr - *String;
				*String = (char*)realloc(*String, *StrAlloc);
				DstPtr = *String + SpcPos;
			}
			
			NoAlphaPos = SpcPos = CurPos = 0;
			if (IsPlayList)
			{
				// to make this format:
				//	01 Very
				//	   Long
				//	   Title
				for (; CurPos < TrkCntDigits + 1; CurPos ++)
					DstPtr[CurPos] = ' ';
				SrcPtr -= CurPos;
			}
			CurLine ++;
		}
		
		if (! isalnum(SrcPtr[CurPos]))
			NoAlphaPos = CurPos;
		
		DstPtr[CurPos] = SrcPtr[CurPos];
		CurPos ++;
	}
	DstPtr[CurPos] = '\0';
	
	return CurLine;
}
