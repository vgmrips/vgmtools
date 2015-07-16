// vgm_stat.c - VGM Statistics
//
// TODO: Proper hours support.

#include <stdio.h>
#include <stdlib.h>
#include "stdbool.h"
#include <string.h>

#ifdef WIN32
#include <conio.h>
#include <windows.h>	// for Directory Listing
#else
#error "Sorry, but it's only compatible with Windows (due to directory listing)."
#endif

#include "zlib.h"

#include "stdtype.h"
#include "VGMFile.h"


static bool OpenVGMFile(const char* FileName);
static wchar_t* ReadWStrFromFile(gzFile hFile, UINT32* FilePos, UINT32 EOFPos);
static void ReadDirectory(const char* DirName);
static void ReadPlaylist(const char* FileName);
static void ShowFileStats(char* FileTitle);
static void PrintSampleTime(char* buffer, const UINT32 Samples, bool LoopMode);
static INT8 stricmp_u(const char *string1, const char *string2);
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
	
	printf("VGM Statistics\n--------------\n\n");
	
//printf("ZLib Version: %s\n", zlibVersion());
	ErrVal = 0;
	printf("File Path or PlayList:\t");
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
		return 0;
	
	AllTotal = 0;
	AllLoop = 0;
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
	
	TrackCount = 0x00;
	TrackAlloc = 0x00;
	TrackList = NULL;
	IsPlayList = PLMode;
	if (! PLMode)
		ReadDirectory(FileName);
	else
		ReadPlaylist(FileName);
	
	ShowFileStats(NULL);
	printf("\n\n");
	
	ShowStatistics();
	
//EndProgram:
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
		if (! IsPlayList)
		{
			CurTrkEntry->Title = (char*)malloc(TempLng + 0x01);
			wcstombs(CurTrkEntry->Title, VGMTag.strTrackNameE, TempLng);
		}
		else
		{
			TempLng += 0x03;
			CurTrkEntry->Title = (char*)malloc(TempLng + 0x01);
			/*TempPnt = strrchr(FileName+4, '_');
			if (TempPnt != NULL)
				fccHeader = strtoul(TempPnt + 1, NULL, 10);
			else
				fccHeader = 99;*/
			sprintf(CurTrkEntry->Title, "%02u ", TrackCount);
			wcstombs(CurTrkEntry->Title + 0x03, VGMTag.strTrackNameE, TempLng - 0x03);
		}
		CurTrkEntry->Title[TempLng] = 0x00;
	}
	else
	{
		TempPnt = strrchr(FileName, '\\');
		if (TempPnt == NULL)
			TempPnt = (char*)FileName;
		else
			TempPnt ++;
		if (! IsPlayList)
		{
			TempLng = strlen(TempPnt);
			CurTrkEntry->Title = (char*)malloc(TempLng + 0x01);
			strcpy(CurTrkEntry->Title, TempPnt);
		}
		else
		{
			TempLng = strlen(TempPnt) + 0x03;
			CurTrkEntry->Title = (char*)malloc(TempLng + 0x01);
			sprintf(CurTrkEntry->Title, "%02u ", TrackCount);
			strcpy(CurTrkEntry->Title + 0x03, TempPnt);
		}
		TempPnt = strrchr(CurTrkEntry->Title, '.');
		if (TempPnt != NULL)
			*TempPnt = 0x00;	// strip ".vgm"
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
	strcat(FileName, "*.vg?");
//printf("  Base Path: %s\n", FilePath);

	hFindFile = FindFirstFile(FileName, &FindFileData);
	if (hFindFile == INVALID_HANDLE_VALUE)
	{
		//ShowErrMessage();
		printf("Error reading directory!\n");
		return;
	}
	strcpy(FileName, FilePath);
	TempPnt = FileName + strlen(FileName);
	printf("\t\t    Sample\t    Time\n");
	printf("File Title\tTotal\tLoop\tTotal\tLoop\n\n");
	
	do
	{
//printf("  Found File: %s\n", FindFileData.cFileName);
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			goto SkipFile;
		
		FileExt = strrchr(FindFileData.cFileName, '.');
		if (FileExt == NULL)
			goto SkipFile;
		FileExt ++;
		
		if (stricmp_u(FileExt, "vgm") && stricmp_u(FileExt, "vgz"))
			goto SkipFile;
		
		strcpy(TempPnt, FindFileData.cFileName);
//printf("  Full File Path: %s\n", FileName);
		if (! OpenVGMFile(FileName))
			printf("%s\tError opening the file!\n", TempPnt);
		else
			ShowFileStats(TempPnt);
		
SkipFile:
		RetVal = FindNextFile(hFindFile, &FindFileData);
	}
	while(RetVal);
	
	RetVal = FindClose(hFindFile);
	
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
		TempStr[RetStr - FileName] = 0x00;
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
	
	printf("\t\t    Sample\t    Time\n");
	printf("File Title\tTotal\tLoop\tTotal\tLoop\n\n");
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
		else
			ShowFileStats(RetStr);
		LineNo ++;
	}
	
	fclose(hFile);
	
	return;
}

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
	
	printf("Song list:                          Total  Loop\n");
	for (CurTL = 0x00; CurTL < TrackCount; CurTL ++)
	{
		CurTrkEntry = &TrackList[CurTL];
		
		LineCnt = GetTitleLines(&TempStrAlloc, &TempStr, CurTrkEntry->Title);
		TitleStr = (LineCnt) ? TempStr : CurTrkEntry->Title;
		if (! LineCnt)
			LineCnt ++;
		
		for (CurLine = 0x00; CurLine < LineCnt; CurLine ++)
		{
			if (CurLine)
				printf("\n");
			
			TitleLen = strlen(TitleStr);
			if (TitleLen <= MAX_TITLE_CHARS)
				Spaces = MAX_TITLE_CHARS - TitleLen;
			else
				Spaces = 0x00;
			
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
	NoAlphaPos = SpcPos = CurPos = 0x00;
	CurLine = 0x01;
	if (IsPlayList)
	{
		for (CurPos = 0x00; CurPos < 0x03; CurPos ++)
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
			
			NoAlphaPos = SpcPos = CurPos = 0x00;
			if (IsPlayList)
			{
				// to make this format:
				//	01 Very
				//	   Long
				//     Title
				DstPtr[0] = DstPtr[1] = DstPtr[2] = ' ';
				SrcPtr -= 3;	CurPos += 3;
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
