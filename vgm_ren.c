// vgm_ren.c - VGM Renamer
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>	// for isalnum()
#include <wchar.h>
#include <locale.h>	// for setlocale()
#include <zlib.h>

#include "stdtype.h"
#include "stdbool.h"
#include "VGMFile.h"
#include "common.h"


static const char* GetFTitle(const char* filePath);	// GetFileTitle is already used by the Windows API
static bool OpenVGMFile(const char* FileName);
static wchar_t* ReadWStrFromFile(gzFile hFile, UINT32* FilePos, UINT32 EOFPos);
static void ReadPlaylist(const char* FileName);
static void RenameFiles(void);
static void WritePlaylist(const char* fileName);


typedef struct track_list
{
	char* PathSrc;
	char* PathDst;
	char* Title;
	bool Compressed;
} TRACK_LIST;


UINT32 VGMDataLen;
char FilePath[MAX_PATH];
UINT32 TrackCount;
UINT32 TrkCntDigits;
UINT32 TrackAlloc;
TRACK_LIST* TrackList;
bool IsPlayList;

int main(int argc, char* argv[])
{
	int ErrVal;
	char FileName[MAX_PATH];
	char* FileExt;
	bool PLMode;
	UINT32 TempLng;

	setlocale(LC_CTYPE, "");	// set to use system codepage

	printf("VGM Renamer\n-----------\n\n");

	ErrVal = 0;
	printf("VGM or PlayList:\t");
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

	PLMode = false;
	FileExt = strrchr(GetFTitle(FileName), '.');
	if (FileExt != NULL)
	{
		FileExt ++;
		if (! stricmp(FileExt, "m3u"))
			PLMode = true;
	}

	TrackCount = 0;
	TrackAlloc = 0;
	TrackList = NULL;
	IsPlayList = PLMode;
	if (PLMode)
	{
		ReadPlaylist(FileName);
	}
	else
	{
		if (! OpenVGMFile(FileName))
			printf("Error opening the file!\n");
	}

	TrkCntDigits = 0;
	if (PLMode)
	{
		TempLng = TrackCount;
		do
		{
			TempLng /= 10;
			TrkCntDigits ++;
		} while(TempLng);
		if (TrkCntDigits < 2)
			TrkCntDigits = 2;
	}

	printf("\n");
	RenameFiles();
	if (PLMode)
	{
		printf("Writing playlist ...\n");
		WritePlaylist(FileName);
	}
	printf("Done.\n");

	{
		UINT32 curFile;
		for (curFile = 0; curFile < TrackCount; curFile ++)
		{
			TRACK_LIST* tlEntry = &TrackList[curFile];
			free(tlEntry->PathSrc);
			free(tlEntry->PathDst);
			free(tlEntry->Title);
		}
		free(TrackList);
	}

	DblClickWait(argv[0]);

	return ErrVal;
}

static const char* GetFTitle(const char* filePath)
{
	const char* dirSepPos;
	const char* sepPos1 = strrchr(filePath, '/');
	const char* sepPos2 = strrchr(filePath, '\\');

	if (sepPos1 == NULL)
		dirSepPos = sepPos2;
	else if (sepPos2 == NULL)
		dirSepPos = sepPos1;
	else
		dirSepPos = (sepPos1 < sepPos2) ? sepPos2 : sepPos1;

	return (dirSepPos != NULL) ? &dirSepPos[1] : filePath;
}

static bool OpenVGMFile(const char* FileName)
{
	gzFile hFile;
	UINT32 fccHeader;
	UINT32 CurPos;
	UINT32 TempLng;
	TRACK_LIST* CurTrkEntry;
	VGM_HEADER VGMHead;
	GD3_TAG VGMTag;

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
		VGMTag.strTrackNameE = NULL;
	}

	CurTrkEntry->PathSrc = strdup(FileName);
	CurTrkEntry->PathDst = NULL;
	CurTrkEntry->Compressed = ! gzdirect(hFile);

	TempLng = (VGMTag.strTrackNameE != NULL) ? wcslen(VGMTag.strTrackNameE) : 0;
	if (TempLng)
	{
#ifdef WIN32
		CurTrkEntry->Title = (char*)malloc(TempLng + 0x01);
		WideCharToMultiByte(CP_ACP, 0x00, VGMTag.strTrackNameE, -1, CurTrkEntry->Title, TempLng + 0x01, "_", NULL);
		CurTrkEntry->Title[TempLng] = '\0';
#else
		CurTrkEntry->Title = (char*)malloc(TempLng + 0x01);
		wcstombs(CurTrkEntry->Title, VGMTag.strTrackNameE, TempLng);
#endif
	}
	else
	{
		CurTrkEntry->Title = NULL;
	}
	free(VGMTag.strTrackNameE);

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

	RetStr = (char*)GetFTitle(FileName);
	if (RetStr != FileName)
	{
		strncpy(TempStr, FileName, RetStr - FileName);
		TempStr[RetStr - FileName] = '\0';
		strcpy(FilePath, TempStr);
	}
	else
	{
		strcpy(FilePath, "");
	}

	hFile = fopen(FileName, "rt");
	if (hFile == NULL)
		return;

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
		if (! OpenVGMFile(FileVGM))
			printf("%s\tError opening the file!\n", GetFTitle(TempStr));
		LineNo ++;
	}

	fclose(hFile);

	return;
}

static char* GenerateFileName(const char* title, UINT32 trackID, const char* extension)
{
	size_t extLen = strlen(extension);
	size_t fnSize = TrkCntDigits + 1 + strlen(title) * 2 + extLen;
	char* fn = (char*)malloc(fnSize + 1);
	const char* src;
	char* dst = fn;

	if (TrkCntDigits > 0)
		dst += sprintf(dst, "%0*u ", TrkCntDigits, trackID);

	for (src = title; *src != '\0' && (dst - fn) < fnSize; )
	{
		// replace:
		//	" -> '
		//	": " -> " - "
		//	"?" -> [remove]
		//	"!" -> [remove]
		//	"/" -> ", "
		//	"\" -> ", "
		if (*src == '"')	// " -> '
		{
			src ++;
			*dst = '\'';	dst ++;
		}
		else if (*src == ':')	// ":" -> " - "
		{
			strcpy(dst, " - ");	dst += 3;
			src ++;
			while(*src == ' ')
				src ++;
		}
		else if (*src == '?' || *src == '!')	// remove '?' and '!'
		{
			src ++;
		}
		else if (*src == '/' || *src == '\\')	// '/' and '\' -> ", " and fix whitespace padding
		{
			while(dst > fn && dst[-1] == ' ')
				dst --;	// remove whitespaces before the comma
			*dst = ',';	dst ++;
			*dst = ' ';	dst ++;
			src ++;
			while(*src == ' ')
				src ++;
		}
		else if (*src == '|')	// | -> -
		{
			src ++;
			*dst = '-';	dst ++;
		}
		else if (*src == '<')	// < -> (
		{
			src ++;
			*dst = '(';	dst ++;
		}
		else if (*src == '>')	// > -> )
		{
			src ++;
			*dst = ')';	dst ++;
		}
		else
		{
			*dst = *src;
			src ++;	dst ++;
		}
	}
	while(dst > fn && dst[-1] == '.')
		dst --;	// remove dots before the extension
	while(dst > fn && dst[-1] == ' ')
		dst --;	// remove trailing spaces

	if (dst > fn + fnSize - extLen)
		dst = fn + fnSize - extLen;
	strcpy(dst, extension);

	return fn;
}

static void RenameFiles(void)
{
	UINT32 curFile;

	for (curFile = 0; curFile < TrackCount; curFile ++)
	{
		TRACK_LIST* tlEntry = &TrackList[curFile];
		const char* fileExt = tlEntry->Compressed ? ".vgz" : ".vgm";
		const char* fileTitle = GetFTitle(tlEntry->PathSrc);
		size_t baseLen = fileTitle - tlEntry->PathSrc;
		size_t ftSize;

		if (tlEntry->Title != NULL)
			tlEntry->PathDst = GenerateFileName(tlEntry->Title, 1 + curFile, fileExt);
		else
			tlEntry->PathDst = GenerateFileName(fileTitle, 1 + curFile, "");
		ftSize = strlen(tlEntry->PathDst) + 1;

		tlEntry->PathDst = (char*)realloc(tlEntry->PathDst, baseLen + ftSize);
		memmove(&tlEntry->PathDst[baseLen], tlEntry->PathDst, ftSize);
		memcpy(tlEntry->PathDst, tlEntry->PathSrc, baseLen);

		printf("%s -> %s\n", fileTitle, GetFTitle(tlEntry->PathDst));
		rename(tlEntry->PathSrc, tlEntry->PathDst);
	}

	return;
}

static void WritePlaylist(const char* fileName)
{
	FILE* hFile;
	UINT32 curFile;

	hFile = fopen(fileName, "wt");

	for (curFile = 0; curFile < TrackCount; curFile ++)
		fprintf(hFile, "%s\n", GetFTitle(TrackList[curFile].PathDst));

	fclose(hFile);

	return;
}
