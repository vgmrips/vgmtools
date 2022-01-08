// vgm_tag.c - VGM Tagger
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>	// for toupper()
#include <wctype.h>
#include <locale.h>
#include <wchar.h>
#include <limits.h>	// for MB_LEN_MAX
#include <zlib.h>

#include "stdtype.h"
#include "stdbool.h"
#include "VGMFile.h"
#include "common.h"


typedef struct system_name_shorts
{
	char* Abbr;
	char* NameE;
	char* NameJ;
} SYSTEM_SHORT;

typedef union filePtr
{
	gzFile gz;
	FILE* f;
} FGZ_PTR;


int main(int argc, char* argv[]);
static bool OpenVGMFile(const char* FileName, bool* Compressed);
static wchar_t* ReadWStrFromFile(gzFile hFile, UINT32* FilePos, const UINT32 EOFPos);
static UINT8 ConvertUnicode2UTF8(char* Buffer, const UINT16 UnicodeChr);
static bool WriteVGMFile(const char* FileName, const bool Compress);
static UINT32 WriteWStrToFile(const FGZ_PTR hFile, const wchar_t* WString);
static void Copy2TagStr(wchar_t** TagStr, const char* DataStr);
static void CopySys2TagStr(wchar_t** TagStr, const SYSTEM_SHORT* DataStr, bool Mode,
						   const char* CmdStr);
static bool CompareSystemNames(const char* StrA, const char* StrB);
static UINT16 GetSystemString(const char* SystemName);
static UINT8 TagVGM(const int ArgCount, char* ArgList[]);
static bool RemoveEqualTag(wchar_t** TagE, wchar_t** TagJ, UINT8 Mode);
static bool RemoveUnknown(wchar_t** Tag);
static void ShowTag(const UINT8 UnicodeMode);
char* ConvertWStr2ASCII_NCR(const wchar_t* WideStr);
char* ConvertWStr2UTF8(const wchar_t* WideStr);
static wchar_t* wcscap(wchar_t* string);


#define SYSMODE_ENG	false
#define SYSMODE_JAP	true


const SYSTEM_SHORT SYSTEM_NAMES[] =
//	short		long (english)			long (japanese)
{	{"SMS",		"Sega Master System", "&#x30BB;&#x30AC;&#x30DE;&#x30B9;&#x30BF;&#x30FC;&#x30B7;&#x30B9;&#x30C6;&#x30E0;"},
	{"SGG",		"Sega Game Gear", "&#x30BB;&#x30AC;&#x30B2;&#x30FC;&#x30E0;&#x30AE;&#x30A2;"},
	{"SMSGG",	"Sega Master System / Game Gear", "&#x30BB;&#x30AC;&#x30DE;&#x30B9;&#x30BF;&#x30FC;&#x30B7;&#x30B9;&#x30C6;&#x30E0; / "
				"&#x30B2;&#x30FC;&#x30E0;&#x30AE;&#x30A2;"},
	{"SMD",		"Sega Mega Drive / Genesis", "&#x30BB;&#x30AC;&#x30E1;&#x30AC;&#x30C9;&#x30E9;&#x30A4;&#x30D6;"},
	{"SG1k",	"Sega Game 1000", "&#x30a8;&#x30b9;&#x30b8;&#x30fc;&#x30fb;&#x30bb;&#x30f3;"},
	{"SC3k",	"Sega Computer 3000", "&#x30bb;&#x30ac;&#x30b3;&#x30f3;&#x30d4;&#x30e5;&#x30fc;&#x30bf; 3000"},
	{"SS*",		"Sega System *", "&#x30bb;&#x30ac;&#x30b7;&#x30b9;&#x30c6;&#x30e0;*"},	// for Sega System 1/2/16/18/24/32/C/C-2/E
//	{"CPS?",	"Capcom Play System ?", ""},	// old name
	{"CPS",		"CP System", "CP &#x30b7;&#x30b9;&#x30c6;&#x30e0;"},
	{"CPS2",	"CP System II", "CP &#x30b7;&#x30b9;&#x30c6;&#x30e0; II"},
	{"CPS3",	"CP System III", "CP &#x30b7;&#x30b9;&#x30c6;&#x30e0; III"},
	{"Ccv",		"Colecovision", "&#x30b3;&#x30ec;&#x30b3;&#x30d3;&#x30b8;&#x30e7;&#x30f3;"},
	{"BMM*",	"BBC Micro Model *", ""},
	{"BM128",	"BBC Master 128", ""},
	{"Arc",		"Arcade Machine", "&#x30a2;&#x30fc;&#x30b1;&#x30fc;&#x30c9;&#x30de;&#x30b7;&#x30fc;&#x30f3;"},
	{"NGP",		"Neo Geo Pocket", "&#x30CD;&#x30AA;&#x30B8;&#x30AA;&#x30DD;&#x30B1;&#x30C3;&#x30C8;"},
	{"NGPC",	"Neo Geo Pocket Color", "&#x30CD;&#x30AA;&#x30B8;&#x30AA;&#x30DD;&#x30B1;&#x30C3;&#x30C8;&#x30AB;&#x30E9;&#x30FC;"},
	{"SCD",		"Sega MegaCD / SegaCD", "&#x30E1;&#x30AC;CD"},
	{"32X",		"Sega 32X", "&#x30B9;&#x30FC;&#x30D1;&#x30FC;32X"},
	{"SCD32",	"Sega Mega-CD 32X / Sega CD 32X", "&#x30E1;&#x30AC;CD 32X"},
	{"Nmc*",	"Namco System *", "&#x30ca;&#x30e0;&#x30b3;&#x30b7;&#x30b9;&#x30c6;&#x30e0;*"},
	{"SX",		"Sega X", "&#x30bb;&#x30ac; X"},
	{"SY",		"Sega Y", "&#x30bb;&#x30ac; Y"},
	{"SGX",		"System GX", "&#x30b7;&#x30b9;&#x30c6;&#x30e0;GX"},
	{"AS?",		"Atari System ?", "&#x30a2;&#x30bf;&#x30ea;&#x30b7;&#x30b9;&#x30c6;&#x30e0; ?"},
	{"BS",		"Bubble System", "&#x30d0;&#x30d6;&#x30eb;&#x30b7;&#x30b9;&#x30c6;&#x30e0;"},
	{"IM*",		"Irem M*", "&#x30a2;&#x30a4;&#x30ec;&#x30e0; M*"},
	{"TW16",	"Twin 16", "&#x30c4;&#x30a4;&#x30f3;16"},
	{"NG",		"Neo Geo", "&#x30cd;&#x30aa;&#x30b8;&#x30aa;"},
	{"NG*",		"Neo Geo *", "&#x30cd;&#x30aa;&#x30b8;&#x30aa;*"},
	{"NES",		"Nintendo Entertainment System", "&#x30d5;&#x30a1;&#x30df;&#x30ea;&#x30fc;&#x30b3;&#x30f3;&#x30d4;&#x30e5;&#x30fc;&#x30bf;"},
	{"FDS",		"Famicom Disk System", "&#x30d5;&#x30a1;&#x30df;&#x30ea;&#x30fc;&#x30b3;&#x30f3;&#x30d4;&#x30e5;&#x30fc;&#x30bf; "
										"&#x30c7;&#x30a3;&#x30b9;&#x30af;&#x30b7;&#x30b9;&#x30c6;&#x30e0;"},
	{"NESFDS",	"Nintendo Entertainment System / Famicom Disk System",
				"&#x30d5;&#x30a1;&#x30df;&#x30ea;&#x30fc;&#x30b3;&#x30f3;&#x30d4;&#x30e5;&#x30fc;&#x30bf; / "
				"&#x30d5;&#x30a1;&#x30df;&#x30ea;&#x30fc;&#x30b3;&#x30f3;&#x30d4;&#x30e5;&#x30fc;&#x30bf; "
				"&#x30c7;&#x30a3;&#x30b9;&#x30af;&#x30b7;&#x30b9;&#x30c6;&#x30e0;"},
	{"GB",		"Game Boy", "&#x30b2;&#x30fc;&#x30e0;&#x30dc;&#x30fc;&#x30a4;"},
	{"GBC",		"Game Boy Color", "&#x30b2;&#x30fc;&#x30e0;&#x30dc;&#x30fc;&#x30a4;&#x30ab;&#x30e9;&#x30fc;"},
	{"GBGBC",	"Game Boy / Game Boy Color", "&#x30b2;&#x30fc;&#x30e0;&#x30dc;&#x30fc;&#x30a4; / "
											"&#x30b2;&#x30fc;&#x30e0;&#x30dc;&#x30fc;&#x30a4;&#x30ab;&#x30e9;&#x30fc;"},
	{"GBA",		"Game Boy Advance", "&#x30b2;&#x30fc;&#x30e0;&#x30dc;&#x30fc;&#x30a4; &#x30a2;&#x30c9;&#x30d0;&#x30f3;&#x30b9;"},
	{"TG16",	"TurboGrafx-16", "PC&#x30a8;&#x30f3;&#x30b8;&#x30f3;"},
	{"TGCD",	"TurboGrafx-CD", "PC&#x30a8;&#x30f3;&#x30b8;&#x30f3; CD-ROM2"},
	{"Tp?",		"Toaplan ?", "&#x6771;&#x4e9c;&#x30d7;&#x30e9;&#x30f3; ?"},
	{"VB",		"Virtual Boy", "&#x30d0;&#x30fc;&#x30c1;&#x30e3;&#x30eb;&#x30dc;&#x30fc;&#x30a4;"},
	{NULL, NULL, NULL}};


VGM_HEADER VGMHead;
GD3_TAG VGMTag;
UINT32 VGMDataLen;
UINT8* VGMData;
bool FileCompression;	// used to select fwrite or gzwite

int main(int argc, char* argv[])
{
	int CmdCnt;
	int CurArg;
	// if SourceFile is NOT compressed, FileCompr = false -> DestFile will be uncompressed
	// if SourceFile IS compressed, FileCompr = true -> DestFile will also be compressed
	bool FileCompr;
	int ErrVal;
	UINT8 RetVal;

	setlocale(LC_ALL, "");	// enable UTF-8 support on Linux

	printf("VGM Tagger\n----------\n\n");

	ErrVal = 0;
	if (argc <= 0x01)
	{
		printf("Usage: vgm_tag [-command1] [-command2] file1.vgm file2.vgz ...\n");
		printf("Use argument -help for command list.\n");
		goto EndProgram;
	}
	else if (! stricmp(argv[1], "-Help"))
	{
		printf("Help\n----\n");
		printf("Usage: vgm_tag [-command1] [-command2] file1.vgm file2.vgz\n");

		printf("Command List\n------------\n");
		printf("General Commands:\n");
		printf("(no command)  like -ShowTag\n");
		printf("\t-Help       Show this help\n");
		printf("\t-SysList    Show the System List\n");
		printf("\t-RemoveTag  Remove the GD3 tag (following commands are ignored)\n");
		printf("\t-ClearTag   Clear the GD3 tag\n");
		printf("\t-ShowTag    Shows the GD3 (HTML NCRs are used to display Unicode-Chars\n");
		printf("\t-ShowTagU   like above, but tries to print real Unicode-Chars\n");
		printf("\t-ShowTag8   like above, but UTF-8 is used to print Unicode-Chars\n");
		printf("\n");

		printf("Tagging Commands:\n");
		printf("Command format: -command:value or -command:\"value with spaces\"\n");
		printf("\t-Title      Track Title\n");
		printf("\t-TitleJ     Track Title (Japanese)\n");
		printf("\t-Author     Track Composer/Author\n");
		printf("\t-AuthorJ    Track Composer/Author (Japanese)\n");
		printf("\t-Game       Game Name\n");
		printf("\t-GameJ      Game Name (Japanese)\n");
		printf("\t-System     Game System*\n");
		printf("\t-SystemE    Game System (English)\n");
		printf("\t-SystemJ    Game System (Japanese)\n");
		printf("\t-Year       Release Date\n");
		printf("\t-Creator    VGM Creator\n");
		printf("\t-Notes      Notes and Comments (replace)\n");
		printf("\t-NotesB     Notes and Comments (insert at beginning)\n");
		printf("\t-NotesE     Notes and Comments (append to end)\n");
		printf("\n");

		printf("*If the system's short name is given,");
		printf("the Japanese system name is filled too.\n");
		printf("  Otherwise the English system name is set and the Japanese one is cleared.\n");
		printf("Command names are case insensitive.\n\n");
		goto EndProgram;
	}
	else if (! stricmp(argv[1], "-SysList"))
	{
		printf("System List\n-----------\n");
		CurArg = 0x00;
		while(SYSTEM_NAMES[CurArg].Abbr != NULL)
		{
			printf("%s\t%s\n", SYSTEM_NAMES[CurArg].Abbr, SYSTEM_NAMES[CurArg].NameE);
			CurArg ++;
		}
		goto EndProgram;
	}

	for (CmdCnt = 0x01; CmdCnt < argc; CmdCnt ++)
	{
		if (*argv[CmdCnt] != '-')
			break;	// skip all commands
	}
	if (CmdCnt >= argc)
	{
		printf("Error: No files specified!\n");
		goto EndProgram;
	}

	for (CurArg = CmdCnt; CurArg < argc; CurArg ++)
	{
		printf("File: %s ...\n", argv[CurArg]);
		if (! OpenVGMFile(argv[CurArg], &FileCompr))
		{
			printf("Error opening file %s!\n", argv[CurArg]);
			printf("\n");
			ErrVal |= 1;	// There was at least 1 opening-error.
			continue;
		}

		RetVal = TagVGM(CmdCnt - 0x01, argv + 0x01);
		if (RetVal == 0x10)
		{
			// Showed Tag - no write neccessary
		}
		else if (RetVal)
		{
			if (RetVal == 0x80)
			{
				ErrVal |= 8;
				goto EndProgram;	// Argument Error
			}
			ErrVal |= 4;	// At least 1 file wasn't tagged.
		}
		else //if (! RetVal)
		{
			if (! WriteVGMFile(argv[CurArg], FileCompr))
			{
				printf("Error opening file %s!\n", argv[CurArg]);
				ErrVal |= 2;	// There was at least 1 writing-error.
			}
		}
		printf("\n");
	}

EndProgram:
	DblClickWait(argv[0]);

	return ErrVal;
}

static bool OpenVGMFile(const char* FileName, bool* Compressed)
{
	gzFile hFile;
	UINT32 fccHeader;
	UINT32 CurPos;
	UINT32 TempLng;

	hFile = gzopen(FileName, "rb");
	if (hFile == NULL)
		return false;

	gzseek(hFile, 0x00, SEEK_SET);
	gzread(hFile, &fccHeader, 0x04);
	if (fccHeader != FCC_VGM)
		goto OpenErr;

	*Compressed = ! gzdirect(hFile);

	gzseek(hFile, 0x00, SEEK_SET);
	gzread(hFile, &VGMHead, 0x40);	// I don't need more data
	ZLIB_SEEKBUG_CHECK(VGMHead);

	// relative -> absolute addresses
	VGMHead.lngEOFOffset += 0x00000004;
	if (VGMHead.lngGD3Offset)
		VGMHead.lngGD3Offset += 0x00000014;

	VGMDataLen = VGMHead.lngEOFOffset;

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
		VGMDataLen = VGMHead.lngGD3Offset;	// That's the actual VGM Data without Tag

		CurPos = VGMHead.lngGD3Offset;
		gzseek(hFile, CurPos, SEEK_SET);
		gzread(hFile, &VGMTag, 0x0C);
		CurPos += 0x0C;
		TempLng = CurPos + VGMTag.lngTagLength;
		VGMTag.strTrackNameE = ReadWStrFromFile(hFile, &CurPos, TempLng);
		VGMTag.strTrackNameJ = ReadWStrFromFile(hFile, &CurPos, TempLng);
		VGMTag.strGameNameE = ReadWStrFromFile(hFile, &CurPos, TempLng);
		VGMTag.strGameNameJ = ReadWStrFromFile(hFile, &CurPos, TempLng);
		VGMTag.strSystemNameE = ReadWStrFromFile(hFile, &CurPos, TempLng);
		VGMTag.strSystemNameJ = ReadWStrFromFile(hFile, &CurPos, TempLng);
		VGMTag.strAuthorNameE = ReadWStrFromFile(hFile, &CurPos, TempLng);
		VGMTag.strAuthorNameJ = ReadWStrFromFile(hFile, &CurPos, TempLng);
		VGMTag.strReleaseDate = ReadWStrFromFile(hFile, &CurPos, TempLng);
		VGMTag.strCreator = ReadWStrFromFile(hFile, &CurPos, TempLng);
		VGMTag.strNotes = ReadWStrFromFile(hFile, &CurPos, TempLng);
	}

	// Read Data
	VGMData = (UINT8*)malloc(VGMDataLen);
	if (VGMData == NULL)
		goto OpenErr;
	gzseek(hFile, 0x00, SEEK_SET);
	gzread(hFile, VGMData, VGMDataLen);

	gzclose(hFile);

	return true;

OpenErr:

	gzclose(hFile);
	return false;
}

static wchar_t* ReadWStrFromFile(gzFile hFile, UINT32* FilePos, const UINT32 EOFPos)
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

static UINT8 ConvertUnicode2UTF8(char* Buffer, const UINT16 UnicodeChr)
{
	// Convert Unicode to UTF-8
	// return legnth of written data

	if (UnicodeChr & 0xF800)
	{
		// 0800 - FFFF
		Buffer[0x00] = 0xE0 | ((UnicodeChr >> 12) & 0x0F);
		Buffer[0x01] = 0x80 | ((UnicodeChr >>  6) & 0x3F);
		Buffer[0x02] = 0x80 | ((UnicodeChr >>  0) & 0x3F);
		return 0x03;
	}
	else if (UnicodeChr & 0x0780)
	{
		// 0080 - 07FF
		Buffer[0x00] = 0xC0 | ((UnicodeChr >> 6) & 0x1F);
		Buffer[0x01] = 0x80 | ((UnicodeChr >> 0) & 0x3F);
		return 0x02;
	}
	else //if (UnicodeChr & 0x007F)
	{
		// 0000 - 007F
		Buffer[0x00] = 0x00 | (UnicodeChr & 0x7F);
		return 0x01;
	}
}

static bool WriteVGMFile(const char* FileName, const bool Compress)
{
	FGZ_PTR hFile;

	FileCompression = Compress;
	if (! FileCompression)
		hFile.f = fopen(FileName, "wb");
	else
		hFile.gz = gzopen(FileName, "wb9");
	if (hFile.f == NULL)
		return false;

	// Write VGM Data (excluding GD3 Tag)
	if (! FileCompression)
	{
		fseek(hFile.f, 0x00, SEEK_SET);
		fwrite(VGMData, 0x01, VGMDataLen, hFile.f);
	}
	else
	{
		gzseek(hFile.gz, 0x00, SEEK_SET);
		gzwrite(hFile.gz, VGMData, VGMDataLen);
	}

	// Write GD3 Tag
	if (VGMHead.lngGD3Offset)
	{
		if (! FileCompression)
		{
			fseek(hFile.f, VGMHead.lngGD3Offset, SEEK_SET);
			fwrite(&VGMTag.fccGD3, 0x04, 0x01, hFile.f);
			fwrite(&VGMTag.lngVersion, 0x04, 0x01, hFile.f);
			fwrite(&VGMTag.lngTagLength, 0x04, 0x01, hFile.f);
		}
		else
		{
			gzseek(hFile.gz, VGMHead.lngGD3Offset, SEEK_SET);
			gzwrite(hFile.gz, &VGMTag.fccGD3, 0x04);
			gzwrite(hFile.gz, &VGMTag.lngVersion, 0x04);
			gzwrite(hFile.gz, &VGMTag.lngTagLength, 0x04);
		}
		WriteWStrToFile(hFile, VGMTag.strTrackNameE);
		WriteWStrToFile(hFile, VGMTag.strTrackNameJ);
		WriteWStrToFile(hFile, VGMTag.strGameNameE);
		WriteWStrToFile(hFile, VGMTag.strGameNameJ);
		WriteWStrToFile(hFile, VGMTag.strSystemNameE);
		WriteWStrToFile(hFile, VGMTag.strSystemNameJ);
		WriteWStrToFile(hFile, VGMTag.strAuthorNameE);
		WriteWStrToFile(hFile, VGMTag.strAuthorNameJ);
		WriteWStrToFile(hFile, VGMTag.strReleaseDate);
		WriteWStrToFile(hFile, VGMTag.strCreator);
		WriteWStrToFile(hFile, VGMTag.strNotes);
	}

	if (! FileCompression)
		fclose(hFile.f);
	else
		gzclose(hFile.gz);
	printf("Tag written.\n");

	return true;
}

static UINT32 WriteWStrToFile(const FGZ_PTR hFile, const wchar_t* WString)
{
	const wchar_t* TextStr;
	UINT32 WrittenChars;
	UINT16 UnicodeChr;

	TextStr = WString;
	WrittenChars = 0x00;
	while(*TextStr)
	{
		UnicodeChr = (wchar_t)(*TextStr);
		if (! FileCompression)
			fwrite(&UnicodeChr, 0x02, 0x01, hFile.f);
		else
			gzwrite(hFile.gz, &UnicodeChr, 0x02);

		TextStr ++;
		WrittenChars ++;
	}
	// Write Null-Terminator
	UnicodeChr = 0x0000;
	if (! FileCompression)
		fwrite(&UnicodeChr, 0x02, 0x01, hFile.f);
	else
		gzwrite(hFile.gz, &UnicodeChr, 0x02);

	return WrittenChars;
}

static void Copy2TagStr(wchar_t** TagStr, const char* DataStr)
{
	int ChrLen;
	size_t DataLen;
	const char* SrcStr;
	wchar_t* DstStr;
	const char* ChrStr;
	UINT16 UnicodeChr;
	char NCRStr[0x10];

	if (DataStr != NULL)
		DataLen = strlen(DataStr);
	else
		DataLen = 0x00;

	*TagStr = (wchar_t*)realloc(*TagStr, (DataLen + 0x01) * sizeof(wchar_t));
	SrcStr = DataStr;
	DstStr = *TagStr;
	DataLen = 0x00;
	while(SrcStr && *SrcStr)
	{
		if (*SrcStr == '\\')
		{
			SrcStr ++;
			// Handle C-style Escape-Sequences (only \n is useful)
			switch(*SrcStr)
			{
			case 'n':
				*DstStr = L'\n';
				SrcStr ++;
				DstStr ++;
				DataLen ++;
				continue;
			}
		}
		else if (*SrcStr == '&')
		{
			// HTML NCR
			ChrStr = SrcStr;
			ChrStr ++;
			if (*ChrStr == '#')
			{
				ChrStr ++;
				// Format:	&#01234 -> Unicode Char 01234
				//			&#xABCD -> Unicode Char 0xABCD
				UnicodeChr = 0x00;
				while(*ChrStr && *ChrStr != ';')
				{
					if (UnicodeChr < 0x0F)
					{
						NCRStr[UnicodeChr] = *ChrStr;
						UnicodeChr ++;
					}
					ChrStr ++;
				}
				NCRStr[UnicodeChr] = 0x00;
				ChrStr ++;
				if (tolower(*NCRStr) == 'x')
					UnicodeChr = (UINT16)strtoul(NCRStr + 0x01, NULL, 0x10);
				else
					UnicodeChr = (UINT16)strtoul(NCRStr, NULL, 10);

				*DstStr = (wchar_t)UnicodeChr;

				SrcStr = ChrStr;
				DstStr ++;
				DataLen ++;
				continue;
			}
		}
		ChrLen = mbtowc(DstStr, SrcStr, MB_LEN_MAX);
		if (ChrLen < 0)
		{
			*DstStr = L'?';
			ChrLen = 1;
		}
		SrcStr += ChrLen;
		DstStr ++;
		DataLen ++;
	}
	*DstStr = 0x0000;

	return;
}

static void CopySys2TagStr(wchar_t** TagStr, const SYSTEM_SHORT* System, bool Mode,
						   const char* CmdStr)
{
	const char* DataStr;
	const char* CmdPos;
	const char* CmdPos2;
	int ChrLen;
	size_t DataLen;
	const char* SrcStr;
	wchar_t* DstStr;
	const char* ChrStr;
	UINT16 UnicodeChr;
	char NCRStr[0x10];
	bool DoWildcard;

	if (Mode == SYSMODE_ENG)
		DataStr = System->NameE;
	else if (Mode == SYSMODE_JAP)
		DataStr = System->NameJ;
	else
		return;

	DataLen = strlen(DataStr);

	CmdPos = System->Abbr;
	while(! (*CmdPos == 0x00 || *CmdPos == '*'))
		CmdPos ++;
	if (*CmdPos == 0x00)
	{
		CmdPos = NULL;

		DoWildcard = false;
	}
	else
	{
		CmdPos = CmdStr + (CmdPos - System->Abbr);
		DoWildcard = true;

		DataLen += strlen(CmdPos) - 0x01;
	}
	CmdPos2 = System->Abbr;

	*TagStr = (wchar_t*)realloc(*TagStr, (DataLen + 0x01) * sizeof(wchar_t));
	SrcStr = DataStr;
	DstStr = *TagStr;
	DataLen = 0x00;
	while(SrcStr && *SrcStr)
	{
		if (*SrcStr == '\\')
		{
			SrcStr ++;
			// Handle C-style Escape-Sequences (only \n is useful)
			switch(*SrcStr)
			{
			case 'n':
				*DstStr = (wchar_t)'\n';
				SrcStr ++;
				DstStr ++;
				DataLen ++;
				continue;
			}
		}
		else if (*SrcStr == '?')
		{
			while(CmdPos2 != NULL && *CmdPos2 != '?')
			{
				if (*CmdPos2 == 0x00)
				{
					CmdPos2 = NULL;
					break;
				}
				CmdPos2 ++;
			}
			if (CmdPos2 != NULL)
			{
				ChrStr = CmdStr + (CmdPos2 - System->Abbr);
				ChrLen = mbtowc(DstStr, ChrStr, MB_LEN_MAX);
				if (ChrLen < 0)
				{
					*DstStr = L'?';
					ChrLen = 1;
				}
				SrcStr += ChrLen;
				DstStr ++;
				DataLen ++;
				CmdPos2 ++;
				continue;
			}
		}
		else if (*SrcStr == '&')
		{
			// HTML NCR
			ChrStr = SrcStr;
			ChrStr ++;
			if (*ChrStr == '#')
			{
				ChrStr ++;
				// Format:	&#01234 -> Unicode Char 01234
				//			&#xABCD -> Unicode Char 0xABCD
				UnicodeChr = 0x00;
				while(*ChrStr && *ChrStr != ';')
				{
					if (UnicodeChr < 0x0F)
					{
						NCRStr[UnicodeChr] = *ChrStr;
						UnicodeChr ++;
					}
					ChrStr ++;
				}
				NCRStr[UnicodeChr] = 0x00;
				ChrStr ++;
				if (tolower(*NCRStr) == 'x')
					UnicodeChr = (UINT16)strtoul(NCRStr + 0x01, NULL, 0x10);
				else
					UnicodeChr = (UINT16)strtoul(NCRStr, NULL, 10);

				*DstStr = (wchar_t)UnicodeChr;

				SrcStr = ChrStr;
				DstStr ++;
				DataLen ++;
				continue;
			}
		}
		else if (DoWildcard && *SrcStr == '*')
		{
			SrcStr = CmdPos;
			DoWildcard = false;
			continue;
		}
		ChrLen = mbtowc(DstStr, SrcStr, MB_LEN_MAX);
		if (ChrLen < 0)
		{
			*DstStr = L'?';
			ChrLen = 1;
		}
		SrcStr += ChrLen;
		DstStr ++;
		DataLen ++;
	}
	*DstStr = 0x0000;

	return;
}

static bool CompareSystemNames(const char* StrA, const char* StrB)
{
	while(*StrA || *StrB)
	{
		if (*StrB == '*')
			return true;	// Wildcard found

		if (*StrB != '?' || *StrA == '\0')
		{
			if (toupper(*StrA) != toupper(*StrB))
				return false;
		}
		StrA ++;
		StrB ++;
	}

	return true;
}

static UINT16 GetSystemString(const char* SystemName)
{
	UINT16 CurSys;

	CurSys = 0x00;
	while(SYSTEM_NAMES[CurSys].Abbr != NULL)
	{
		if (CompareSystemNames(SystemName, SYSTEM_NAMES[CurSys].Abbr))
			return CurSys;
		CurSys ++;
	}

	return 0xFFFF;
}

static UINT8 TagVGM(const int ArgCount, char* ArgList[])
{
	int CurArg;
	char* CmdStr;
	char* CmdData;
	//UINT32 DataLen;
	//wchar_t* TempPnt;
	UINT16 CurSys;
	const SYSTEM_SHORT* TempSys;
	wchar_t* TempStr;
	wchar_t* NoteStr;
	UINT32 StrLen;
	UINT32 TempLng;
	UINT8 TempByt;
	UINT8 RetVal;

	if (! VGMHead.lngGD3Offset)
	{
		VGMTag.fccGD3 = 0x20336447;
		VGMTag.lngVersion = 0x00000100;	// Version 1.0
		// The rest is done when the 1st tag is written
	}
	else if (VGMTag.lngVersion > 0x00000100)
	{
		printf("Warning! This GD3 version is NOT supported!\n");
		printf("You may damage your GD3 tag!\n");
		printf("Continue (Y/N) ? \t");
		CurArg = toupper(getchar());
		if (CurArg != 'Y')
			return 0x01;
	}

	// Execute Commands
	RetVal = 0x10;	// nothing done - skip writing
	if (! ArgCount)
		ShowTag(0x00);
	CmdStr = NULL;
	for (CurArg = 0x00; CurArg < ArgCount; CurArg ++)
	{
		if (CmdStr != NULL)
			free(CmdStr);
		//CmdStr = ArgList[CurArg] + 0x01;	// Skip the '-' at the beginning
		CmdStr = strdup(ArgList[CurArg] + 0x01);	// I need a copy, since I remove the '=' character

		CmdData = strchr(CmdStr, ':');
		if (CmdData != NULL)
		{
			*CmdData = 0x00;
			CmdData ++;
		}

		if (! stricmp(CmdStr, "ShowTag"))
		{
			ShowTag(0x00);
			continue;
		}
		else if (! stricmp(CmdStr, "ShowTagU"))
		{
			ShowTag(0x01);
			continue;
		}
		else if (! stricmp(CmdStr, "ShowTag8"))
		{
			ShowTag(0x02);
			continue;
		}

		if (! stricmp(CmdStr, "RmvEqual"))
		{
			if (CmdData != NULL)
				TempByt = (toupper(CmdData[0x00]) == 'J') ? 1 : 0;
			else
				TempByt = 0;
			printf("Removing redundant tags (default to %s): ",
					TempLng ? "Eng" : "Jap");

			TempLng = 0x00;
			TempLng |= RemoveEqualTag(&VGMTag.strTrackNameE,	&VGMTag.strTrackNameJ,	TempByt) << 0;
			TempLng |= RemoveEqualTag(&VGMTag.strGameNameE,		&VGMTag.strGameNameJ,	TempByt) << 1;
			TempLng |= RemoveEqualTag(&VGMTag.strSystemNameE,	&VGMTag.strSystemNameJ,	TempByt) << 2;
			TempLng |= RemoveEqualTag(&VGMTag.strAuthorNameE,	&VGMTag.strAuthorNameJ,	TempByt) << 3;
			if (TempLng)
			{
				if (TempLng & 0x01)
					printf("Title, ");
				if (TempLng & 0x02)
					printf("Game, ");
				if (TempLng & 0x04)
					printf("System, ");
				if (TempLng & 0x08)
					printf("Author, ");
				printf("\b\b ");
				RetVal = 0x00;
			}
			printf("\n");
			continue;
		}
		else if (! stricmp(CmdStr, "RmvUnknown"))
		{
			printf("Removing \"Unknown Author\" tag: ");

			TempLng = 0x00;
			TempLng |= RemoveUnknown(&VGMTag.strAuthorNameE) << 0;
			TempLng |= RemoveUnknown(&VGMTag.strAuthorNameJ) << 1;
			if (TempLng)
			{
				if (TempLng & 0x01)
					printf("AuthorE, ");
				if (TempLng & 0x02)
					printf("AuthorJ, ");
				printf("\b\b ");
				RetVal = 0x00;
			}
			printf("\n");
			continue;
		}

		RetVal = 0x00;
		if (! stricmp(CmdStr, "RemoveTag"))
		{
			printf("Tag removed.\n");
			if (VGMHead.lngGD3Offset)
			{
				VGMHead.lngEOFOffset = VGMDataLen;
				VGMHead.lngGD3Offset = 0x00;
			}
			break;
		}

		if (! VGMHead.lngGD3Offset)
		{
			printf("Tag created.\n");
			VGMHead.lngGD3Offset = VGMDataLen;
			VGMTag.strTrackNameE = NULL;
			VGMTag.strTrackNameJ = NULL;
			VGMTag.strGameNameE = NULL;
			VGMTag.strGameNameJ = NULL;
			VGMTag.strSystemNameE = NULL;
			VGMTag.strSystemNameJ = NULL;
			VGMTag.strAuthorNameE = NULL;
			VGMTag.strAuthorNameJ = NULL;
			VGMTag.strReleaseDate = NULL;
			VGMTag.strCreator = NULL;
			VGMTag.strNotes = NULL;
			Copy2TagStr(&VGMTag.strTrackNameE, "");
			Copy2TagStr(&VGMTag.strTrackNameJ, "");
			Copy2TagStr(&VGMTag.strGameNameE, "");
			Copy2TagStr(&VGMTag.strGameNameJ, "");
			Copy2TagStr(&VGMTag.strSystemNameE, "");
			Copy2TagStr(&VGMTag.strSystemNameJ, "");
			Copy2TagStr(&VGMTag.strAuthorNameE, "");
			Copy2TagStr(&VGMTag.strAuthorNameJ, "");
			Copy2TagStr(&VGMTag.strReleaseDate, "");
			Copy2TagStr(&VGMTag.strCreator, "");
			Copy2TagStr(&VGMTag.strNotes, "");
		}

		if (! stricmp(CmdStr, "ClearTag"))
		{
			printf("Tag cleared.\n");
			Copy2TagStr(&VGMTag.strTrackNameE, "");
			Copy2TagStr(&VGMTag.strTrackNameJ, "");
			Copy2TagStr(&VGMTag.strGameNameE, "");
			Copy2TagStr(&VGMTag.strGameNameJ, "");
			Copy2TagStr(&VGMTag.strSystemNameE, "");
			Copy2TagStr(&VGMTag.strSystemNameJ, "");
			Copy2TagStr(&VGMTag.strAuthorNameE, "");
			Copy2TagStr(&VGMTag.strAuthorNameJ, "");
			Copy2TagStr(&VGMTag.strReleaseDate, "");
			Copy2TagStr(&VGMTag.strCreator, "");
			Copy2TagStr(&VGMTag.strNotes, "");
		}
		else
		{
			if (CmdData != NULL)
				printf("Set %s = %s\n", CmdStr, CmdData);
			else
				printf("Set %s\n", CmdStr);
			if (! stricmp(CmdStr, "Title"))
			{
				Copy2TagStr(&VGMTag.strTrackNameE, CmdData);
			}
			else if (! stricmp(CmdStr, "TitleJ"))
			{
				Copy2TagStr(&VGMTag.strTrackNameJ, CmdData);
			}
			else if (! stricmp(CmdStr, "TitleNorm"))
			{
				// secret, undocumented command
				wcscap(VGMTag.strTrackNameE);
			}
			else if (! stricmp(CmdStr, "TitleJNorm"))
			{
				wcscap(VGMTag.strTrackNameJ);
			}
			else if (! stricmp(CmdStr, "Author"))
			{
				Copy2TagStr(&VGMTag.strAuthorNameE, CmdData);
			}
			else if (! stricmp(CmdStr, "AuthorJ"))
			{
				Copy2TagStr(&VGMTag.strAuthorNameJ, CmdData);
			}
			else if (! stricmp(CmdStr, "Game"))
			{
				Copy2TagStr(&VGMTag.strGameNameE, CmdData);
			}
			else if (! stricmp(CmdStr, "GameJ"))
			{
				Copy2TagStr(&VGMTag.strGameNameJ, CmdData);
			}
			else if (! stricmp(CmdStr, "System"))
			{
				CurSys = GetSystemString(CmdData);
				if (CurSys == 0xFFFF)
				{
					Copy2TagStr(&VGMTag.strSystemNameE, CmdData);
					Copy2TagStr(&VGMTag.strSystemNameJ, "");
				}
				else
				{
					TempSys = &SYSTEM_NAMES[CurSys];
					CopySys2TagStr(&VGMTag.strSystemNameE, TempSys, SYSMODE_ENG, CmdData);
					CopySys2TagStr(&VGMTag.strSystemNameJ, TempSys, SYSMODE_JAP, CmdData);
				}
			}
			else if (! stricmp(CmdStr, "SystemE"))
			{
				CurSys = GetSystemString(CmdData);
				if (CurSys == 0xFFFF)
					Copy2TagStr(&VGMTag.strSystemNameE, CmdData);
				else
					CopySys2TagStr(&VGMTag.strSystemNameE, &SYSTEM_NAMES[CurSys], SYSMODE_ENG,
									CmdData);
			}
			else if (! stricmp(CmdStr, "SystemJ"))
			{
				CurSys = GetSystemString(CmdData);
				if (CurSys == 0xFFFF)
					Copy2TagStr(&VGMTag.strSystemNameJ, CmdData);
				else
					CopySys2TagStr(&VGMTag.strSystemNameJ, &SYSTEM_NAMES[CurSys], SYSMODE_JAP,
									CmdData);
			}
			else if (! stricmp(CmdStr, "Year"))
			{
				Copy2TagStr(&VGMTag.strReleaseDate, CmdData);
			}
			else if (! stricmp(CmdStr, "Creator"))
			{
				Copy2TagStr(&VGMTag.strCreator, CmdData);
			}
			else if (! stricmp(CmdStr, "Notes"))
			{
				Copy2TagStr(&VGMTag.strNotes, CmdData);
			}
			else if (! stricmp(CmdStr, "NotesB"))
			{
				// copy old Notes-Tag
				StrLen = wcslen(VGMTag.strNotes);	// +0x01 = Null-Terminator
				NoteStr = (wchar_t*)malloc((StrLen + 0x01) * sizeof(wchar_t));
				wcscpy(NoteStr, VGMTag.strNotes);

				// convert new String
				TempStr = NULL;
				Copy2TagStr(&TempStr, CmdData);

				// rewrite Notes-Tag
				StrLen = wcslen(TempStr) + wcslen(VGMTag.strNotes);
				VGMTag.strNotes = (wchar_t*)realloc(VGMTag.strNotes,
													(StrLen + 0x01) * sizeof(wchar_t));
				wcscpy(VGMTag.strNotes, TempStr);
				wcscat(VGMTag.strNotes, NoteStr);
				free(TempStr);
			}
			else if (! stricmp(CmdStr, "NotesE"))
			{
				// copy old Notes-Tag
				StrLen = wcslen(VGMTag.strNotes);	// +0x01 = Null-Terminator
				NoteStr = (wchar_t*)malloc((StrLen + 0x01) * sizeof(wchar_t));
				wcscpy(NoteStr, VGMTag.strNotes);

				// convert new String
				TempStr = NULL;
				Copy2TagStr(&TempStr, CmdData);

				// rewrite Notes-Tag
				StrLen = wcslen(VGMTag.strNotes) + wcslen(TempStr);
				VGMTag.strNotes = (wchar_t*)realloc(VGMTag.strNotes,
													(StrLen + 0x01) * sizeof(wchar_t));
				wcscpy(VGMTag.strNotes, NoteStr);
				wcscat(VGMTag.strNotes, TempStr);
				free(TempStr);
			}
			else
			{
				printf("Error - Unknown Command: -%s\n", CmdStr);
				free(CmdStr);
				return 0x80;
			}
		}
	}
	if (CmdStr != NULL)
		free(CmdStr);

	if (RetVal != 0x10)
	{
		// only care about that if we modified something
		if (VGMHead.lngGD3Offset)
		{
			//					(Len of String + Null-Terminator) * Character Size (0x02)
			VGMTag.lngTagLength = (wcslen(VGMTag.strTrackNameE) + 0x01) * 0x02 +
									(wcslen(VGMTag.strTrackNameJ) + 0x01) * 0x02 +
									(wcslen(VGMTag.strGameNameE) + 0x01) * 0x02 +
									(wcslen(VGMTag.strGameNameJ) + 0x01) * 0x02 +
									(wcslen(VGMTag.strSystemNameE) + 0x01) * 0x02 +
									(wcslen(VGMTag.strSystemNameJ) + 0x01) * 0x02 +
									(wcslen(VGMTag.strAuthorNameE) + 0x01) * 0x02 +
									(wcslen(VGMTag.strAuthorNameJ) + 0x01) * 0x02 +
									(wcslen(VGMTag.strReleaseDate) + 0x01) * 0x02 +
									(wcslen(VGMTag.strCreator) + 0x01) * 0x02 +
									(wcslen(VGMTag.strNotes) + 0x01) * 0x02;
			VGMHead.lngEOFOffset = VGMHead.lngGD3Offset + 0x0C + VGMTag.lngTagLength;
		}
		else
		{
			VGMHead.lngEOFOffset = VGMDataLen;
		}

		// Write VGM Header
		TempLng = VGMHead.lngEOFOffset - 0x04;
		memcpy(&VGMData[0x04], &TempLng, 0x04);
		TempLng = VGMHead.lngGD3Offset ? (VGMHead.lngGD3Offset - 0x14) : 0x00;
		memcpy(&VGMData[0x14], &TempLng, 0x04);
	}

	return RetVal;
}

static bool RemoveEqualTag(wchar_t** TagE, wchar_t** TagJ, UINT8 Mode)
{
	// Remove redundant tags

	if (*TagE == NULL || *TagJ == NULL)
		return false;	// NULL - break

	if (wcscmp(*TagE, *TagJ))
		return false;	// both are different
	if (! wcslen(*TagE))
		return false;	// both are empty

	// both are equal
	if (! Mode)
		Copy2TagStr(TagJ, "");
	else
		Copy2TagStr(TagE, "");

	return true;
}

static bool RemoveUnknown(wchar_t** Tag)
{
	if (*Tag == NULL)
		return false;	// NULL - break

	if (wcsicmp(*Tag, L"unknown") && wcsicmp(*Tag, L"Unknown Author"))
		return false;

	Copy2TagStr(Tag, "");
	return true;
}

static void ShowTag(const UINT8 UnicodeMode)
{
	char* TempStr;

	if (! VGMHead.lngGD3Offset)
	{
		printf("This VGM File has no GD3 Tag!\n");
		return;
	}

	switch(UnicodeMode)
	{
	case 0x00:	// HTML NCRs
		TempStr = ConvertWStr2ASCII_NCR(VGMTag.strTrackNameE);
		printf("Track Title:\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2ASCII_NCR(VGMTag.strTrackNameJ);
		printf("\t\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2ASCII_NCR(VGMTag.strGameNameE);
		printf("Game Name:\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2ASCII_NCR(VGMTag.strGameNameJ);
		printf("\t\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2ASCII_NCR(VGMTag.strSystemNameE);
		printf("System:\t\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2ASCII_NCR(VGMTag.strSystemNameJ);
		printf("\t\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2ASCII_NCR(VGMTag.strAuthorNameE);
		printf("Composer:\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2ASCII_NCR(VGMTag.strAuthorNameJ);
		printf("\t\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2ASCII_NCR(VGMTag.strReleaseDate);
		printf("Release:\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2ASCII_NCR(VGMTag.strCreator);
		printf("VGM by:\t\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2ASCII_NCR(VGMTag.strNotes);
		printf("Notes:\t\t%s\n", TempStr);
		free(TempStr);
		break;
	case 0x01:	// real Unicode Characters
		wprintf(L"Track Title:\t%ls\n", VGMTag.strTrackNameE);
		wprintf(L"\t\t%ls\n", VGMTag.strTrackNameJ);
		wprintf(L"Game Name:\t%ls\n", VGMTag.strGameNameE);
		wprintf(L"\t\t%ls\n", VGMTag.strGameNameJ);
		wprintf(L"System:\t\t%ls\n", VGMTag.strSystemNameE);
		wprintf(L"\t\t%ls\n", VGMTag.strSystemNameJ);
		wprintf(L"Composer:\t%ls\n", VGMTag.strAuthorNameE);
		wprintf(L"\t\t%ls\n", VGMTag.strAuthorNameJ);
		wprintf(L"Release:\t%ls\n", VGMTag.strReleaseDate);
		wprintf(L"VGM by:\t\t%ls\n", VGMTag.strCreator);
		wprintf(L"Notes:\t\t%ls\n", VGMTag.strNotes);
		break;
	case 0x02:	// UTF-8 Display
		TempStr = ConvertWStr2UTF8(VGMTag.strTrackNameE);
		printf("Track Title:\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2UTF8(VGMTag.strTrackNameJ);
		printf("\t\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2UTF8(VGMTag.strGameNameE);
		printf("Game Name:\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2UTF8(VGMTag.strGameNameJ);
		printf("\t\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2UTF8(VGMTag.strSystemNameE);
		printf("System:\t\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2UTF8(VGMTag.strSystemNameJ);
		printf("\t\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2UTF8(VGMTag.strAuthorNameE);
		printf("Composer:\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2UTF8(VGMTag.strAuthorNameJ);
		printf("\t\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2UTF8(VGMTag.strReleaseDate);
		printf("Release:\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2UTF8(VGMTag.strCreator);
		printf("VGM by:\t\t%s\n", TempStr);
		free(TempStr);
		TempStr = ConvertWStr2UTF8(VGMTag.strNotes);
		printf("Notes:\t\t%s\n", TempStr);
		free(TempStr);
		break;
	}

	return;
}

char* ConvertWStr2ASCII_NCR(const wchar_t* WideStr)
{
	char* RetStr;
	const wchar_t* SrcStr;
	char* DstStr;
	UINT32 StrLen;
	UINT16 UnicodeChr;

	// HTML NCR: &#xABCD;
	// Length;   12345678
	StrLen = wcslen(WideStr);
	RetStr = (char*)malloc((StrLen * 0x08) + 0x01);	// space for 1 NCR for every char

	SrcStr = WideStr;
	DstStr = RetStr;
	while(*SrcStr)
	{
		UnicodeChr = (UINT16)(*SrcStr);
		if ((UnicodeChr >= 0x20 && UnicodeChr < 0x80) || UnicodeChr == '\n')
		{
			*DstStr = (UINT8)UnicodeChr;
			DstStr ++;
		}
		else
		{
			sprintf(DstStr, "&#x%04X;", UnicodeChr);
			while(*DstStr)
				DstStr ++;
		}
		SrcStr ++;
	}
	*DstStr = 0x00;

	return RetStr;
}

char* ConvertWStr2UTF8(const wchar_t* WideStr)
{
	char* RetStr;
	const wchar_t* SrcStr;
	char* DstStr;
	UINT32 StrLen;
	UINT16 UnicodeChr;

	StrLen = wcslen(WideStr);
	RetStr = (char*)malloc((StrLen * 0x04) + 0x01);	// space for 4 Bytes per char

	SrcStr = WideStr;
	DstStr = RetStr;
	while(*SrcStr)
	{
		UnicodeChr = (UINT16)(*SrcStr);
		DstStr += ConvertUnicode2UTF8(DstStr, UnicodeChr);
		SrcStr ++;
	}
	*DstStr = 0x00;

	return RetStr;
}

static wchar_t* wcscap(wchar_t* string)
{
	wchar_t* CurChr;
	bool LastWasLetter;

	CurChr = string;
	LastWasLetter = false;
	while(*CurChr != L'\0')
	{
		if (iswalpha(*CurChr))
		{
			if (! LastWasLetter)
				*CurChr = towupper(*CurChr);
			else
				*CurChr = towlower(*CurChr);
			LastWasLetter = true;
		}
		else
		{
			if (*CurChr != L'\'')	// for e.g. "Let's"
				LastWasLetter = false;
		}
		CurChr ++;
	}

	return string;
}
