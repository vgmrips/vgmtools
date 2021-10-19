// vgm_tt.cpp - VGM Tag Transfer
#ifdef _MSC_VER
#pragma warning(disable: 4786)
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>
#include <map>

#include "stdtype.h"
//#include "stdbool.h"
#include "VGMFile.h"
#include "common.h"

#ifdef WIN32
#include <windows.h>	// for Directory Listing
#include <sys/stat.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#endif
#include <zlib.h>


struct VGM_FILE_INFO
{
	std::string fileName;
	UINT32 fileSize;
	UINT32 totalSmpls;
	UINT32 loopSmpls;
	UINT32 gd3Offset;
};

struct VGM_FINF_KEY
{
	UINT32 totalSmpls;
	UINT32 loopSmpls;
	bool operator<(const VGM_FINF_KEY& a) const
	{
		if (this->totalSmpls != a.totalSmpls)
			return this->totalSmpls < a.totalSmpls;
		else
			return this->loopSmpls < a.loopSmpls;
	}
};
struct VGM_ASSIGNMENT
{
	size_t refFileID;
	size_t dstFileID;
};
typedef std::vector<VGM_FILE_INFO> VFINF_LIST;
typedef std::map<VGM_FINF_KEY, size_t> VF_MAP;
typedef std::vector<VGM_ASSIGNMENT> VA_LIST;


static bool IsDirectory(const std::string& dirName);
static std::string Dir2Path(const std::string& dirName);
static void ReadDirectory(const char* dirName, VFINF_LIST& vgmInfoList);
static UINT8 GetVGMFileInfo(const char* fileName, VGM_FILE_INFO* vgmInfo);
static void CompareDirs_TrimPts(const VFINF_LIST& srcFiles, const VFINF_LIST& dstFiles, VA_LIST& assignList);
static void CompareDirs_FileNames(const VFINF_LIST& srcFiles, const VFINF_LIST& dstFiles, VA_LIST& assignList);
static void PrintMappings(const VFINF_LIST& srcFiles, const VFINF_LIST& dstFiles, const VA_LIST& assignList);
static void ExecuteTransfer(const char* srcDir, const VFINF_LIST& srcFiles, const char* dstDir,
							const VFINF_LIST& dstFiles, const VA_LIST& assignList, UINT8 actions);
static UINT8 CopyTag(const char* srcFile, const char* dstFile);


#define MATCH_TRIM	0x01
#define MATCH_NAME	0x02
#define ACT_RENAME	0x01
#define ACT_RETAG	0x02

static VFINF_LIST refFiles;
static VFINF_LIST dstFiles;
static VA_LIST refDstAssign;
static UINT8 matchMode;
static UINT8 fileActions;

int main(int argc, char* argv[])
{
	int argbase;
	const char* refDir;
	const char* dstDir;
	bool refFMode;
	bool dstFMode;

	printf("VGM Tag Transfer\n----------------\n");

	if (argc < 3)
	{
		printf("Usage: %s [options] referenceDir destinationDir\n", argv[0]);
		printf("Options:\n");
		printf("    -mtrim   - Mode: match files using trim points (default)\n");
		printf("    -mname   - Mode: match files using file names\n");
		printf("    -rename  - rename files\n");
		printf("    -tag     - transfer tags\n");
		printf("By default, only matches are shown, but no action is taken.\n");
		printf("You can transfer tags between two files directly by specifying\n");
		printf("two files instead of folders.\n");
		return 1;
	}

	argbase = 1;
	matchMode = MATCH_TRIM;
	fileActions = 0x00;
	while(argbase < argc && argv[argbase][0] == '-')
	{
		if (! stricmp(argv[argbase], "-mtrim"))
		{
			matchMode = MATCH_TRIM;
			argbase ++;
		}
		else if (! stricmp(argv[argbase], "-mname"))
		{
			matchMode = MATCH_NAME;
			argbase ++;
		}
		else if (! stricmp(argv[argbase], "-rename"))
		{
			fileActions |= ACT_RENAME;
			argbase ++;
		}
		else if (! stricmp(argv[argbase], "-tag"))
		{
			fileActions |= ACT_RETAG;
			argbase ++;
		}
		else
		{
			break;
		}
	}

	if (argc < argbase + 2)
	{
		printf("Insufficient arguments.\n");
		return 1;
	}

	refDir = argv[argbase + 0];
	dstDir = argv[argbase + 1];
	refFMode = IsDirectory(refDir);
	dstFMode = IsDirectory(dstDir);
	if (refFMode != dstFMode)
	{
		printf("Reference and destination must be both a directory or both a file!\n");
		return 2;
	}

	if (refFMode)
	{
		ReadDirectory(refDir, refFiles);
		ReadDirectory(dstDir, dstFiles);
		if (matchMode == MATCH_TRIM)
			CompareDirs_TrimPts(refFiles, dstFiles, refDstAssign);
		else if (matchMode == MATCH_NAME)
			CompareDirs_FileNames(refFiles, dstFiles, refDstAssign);
	}
	else
	{
		VGM_FILE_INFO vgmInfRef;
		VGM_FILE_INFO vgmInfDst;
		UINT8 retVal8 = GetVGMFileInfo(refDir, &vgmInfRef);
		vgmInfRef.fileName = refDir;
		if (! retVal8)
			refFiles.push_back(vgmInfRef);
		else
			printf("Error reading %s!\n", refDir);

		retVal8 = GetVGMFileInfo(dstDir, &vgmInfDst);
		vgmInfDst.fileName = dstDir;
		if (! retVal8)
			dstFiles.push_back(vgmInfDst);
		else
			printf("Error reading %s!\n", dstDir);

		refDir = "";
		dstDir = "";
		{
			VGM_ASSIGNMENT val;
			val.refFileID = refFiles.empty() ? (size_t)-1 : 0;
			val.dstFileID = dstFiles.empty() ? (size_t)-1 : 0;
			refDstAssign.push_back(val);
		}
	}

	if (! fileActions)
		PrintMappings(refFiles, dstFiles, refDstAssign);
	else
		ExecuteTransfer(refDir, refFiles, dstDir, dstFiles, refDstAssign, fileActions);

	return 0;
}

static bool IsDirectory(const std::string& dirName)
{
#ifdef _WIN32
   struct _stat fInfo;
   int result = _stat(dirName.c_str(), &fInfo);
   if (result)
	   return false;
   if (fInfo.st_mode & _S_IFDIR)
	   return true;
#else
   struct stat fInfo;
   int result = stat(dirName.c_str(), &fInfo);
   if (result)
	   return false;
   if (S_ISDIR(fInfo.st_mode))
	   return true;
#endif
   return false;
}

static std::string Dir2Path(const std::string& dirName)
{
	if (dirName.empty())
		return dirName;

	char lastChr;

	lastChr = dirName[dirName.length() - 1];
	if (lastChr == '/' || lastChr == '\\')
		return dirName;
#ifdef _WIN32
	return dirName + '\\';
#else
	return dirName + '/';
#endif
}

#ifdef _WIN32
static void ReadDirectory(const char* dirName, VFINF_LIST& vgmInfoList)
{
	HANDLE hFindFile;
	WIN32_FIND_DATA findData;
	BOOL retValB;
	std::string basePath;
	std::string fileName;
	VGM_FILE_INFO vgmInfo;
	UINT8 retVal8;

	basePath = Dir2Path(dirName);

	fileName = basePath + "*.vg?";
	hFindFile = FindFirstFile(fileName.c_str(), &findData);
	if (hFindFile == INVALID_HANDLE_VALUE)
	{
		printf("Error reading directory!\n");
		return;
	}

	do
	{
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			goto SkipFile;

		fileName = basePath + findData.cFileName;
		retVal8 = GetVGMFileInfo(fileName.c_str(), &vgmInfo);
		vgmInfo.fileName = findData.cFileName;
		if (! retVal8)
			vgmInfoList.push_back(vgmInfo);
		else
			printf("Error reading %s!\n", fileName.c_str());

SkipFile:
		retValB = FindNextFile(hFindFile, &findData);
	} while(retValB);
	retValB = FindClose(hFindFile);

	return;
}
#else
static void ReadDirectory(const char* dirName, VFINF_LIST& vgmInfoList)
{
	DIR* dirHandle;
	dirent* dirEntry;
	const char* dirEntryName;
	size_t dirEntryNameSize;
	struct stat fileStatus;
	std::string basePath;
	std::string fileName;
	VGM_FILE_INFO vgmInfo;
	UINT8 retVal8;

	basePath = Dir2Path(dirName);

	dirHandle = opendir(basePath.c_str());
	if (! dirHandle)
	{
		printf("Error reading directory!\n");
		return;
	}

	while ((dirEntry = readdir(dirHandle)))
	{
		dirEntryName = dirEntry->d_name;
		if (! strcmp(dirEntryName, ".") || ! strcmp(dirEntryName, ".."))
			continue;

		dirEntryNameSize = strlen(dirEntryName);
		if (dirEntryNameSize < 4 || memcmp(dirEntryName + dirEntryNameSize - 4, ".vg", 3))
			continue;

		fileName = basePath + dirEntryName;
		if (stat(fileName.c_str(), &fileStatus) != 0 || ! S_ISREG(fileStatus.st_mode))
			continue;

		retVal8 = GetVGMFileInfo(fileName.c_str(), &vgmInfo);
		vgmInfo.fileName = dirEntryName;
		if (! retVal8)
			vgmInfoList.push_back(vgmInfo);
		else
			printf("Error reading %s!\n", fileName.c_str());
	}
	closedir(dirHandle);

	return;
}
#endif

static UINT8 GetVGMFileInfo(const char* fileName, VGM_FILE_INFO* vgmInfo)
{
	gzFile hFile;
	UINT8 hdrData[0x40];

	hFile = gzopen(fileName, "rb");
	if (hFile == NULL)
		return 0xFF;

	gzseek(hFile, 0x00, SEEK_SET);
	gzread(hFile, hdrData, 0x40);
	if (memcmp(&hdrData[0x00], "Vgm ", 0x04))
	{
		gzclose(hFile);
		return 0xF0;
	}

	gzseek(hFile, 0x18, SEEK_SET);
	memcpy(&vgmInfo->fileSize, &hdrData[0x04], 0x04);
	vgmInfo->fileSize += 0x04;
	memcpy(&vgmInfo->gd3Offset, &hdrData[0x14], 0x04);
	if (vgmInfo->gd3Offset)
		vgmInfo->gd3Offset += 0x14;
	memcpy(&vgmInfo->totalSmpls, &hdrData[0x18], 0x04);
	memcpy(&vgmInfo->loopSmpls, &hdrData[0x20], 0x04);

	gzclose(hFile);

	return 0x00;
}

static void CompareDirs_TrimPts(const VFINF_LIST& srcFiles, const VFINF_LIST& dstFiles, VA_LIST& assignList)
{
	size_t curFile;
	VF_MAP fileMap;

	for (curFile = 0; curFile < srcFiles.size(); curFile ++)
	{
		const VGM_FILE_INFO& srcFile = srcFiles[curFile];
		VGM_FINF_KEY key;
		VGM_ASSIGNMENT val;
		key.totalSmpls = srcFile.totalSmpls;
		key.loopSmpls = srcFile.loopSmpls;
		val.refFileID = curFile;
		val.dstFileID = (size_t)-1;
		fileMap[key] = assignList.size();
		assignList.push_back(val);
	}
	for (curFile = 0; curFile < dstFiles.size(); curFile ++)
	{
		const VGM_FILE_INFO& dstFile = dstFiles[curFile];
		VGM_FINF_KEY key;
		key.totalSmpls = dstFile.totalSmpls;
		key.loopSmpls = dstFile.loopSmpls;

		VF_MAP::iterator mapIt = fileMap.find(key);
		if (mapIt != fileMap.end())
		{
			VGM_ASSIGNMENT& vAssign = assignList[mapIt->second];
			if (vAssign.dstFileID != (size_t)-1)
			{
				const std::string& dstName1 = dstFiles[vAssign.dstFileID].fileName;
				const std::string& dstName2 = dstFile.fileName;
				printf("Warning: %s and %s have the same trimming offsets!\n",
					dstName1.c_str(), dstName2.c_str());
			}
			else
			{
				vAssign.dstFileID = curFile;
			}
		}
		else
		{
			VGM_ASSIGNMENT val;
			val.refFileID = (size_t)-1;
			val.dstFileID = curFile;
			assignList.push_back(val);
		}
	}

	return;
}

static void CompareDirs_FileNames(const VFINF_LIST& srcFiles, const VFINF_LIST& dstFiles, VA_LIST& assignList)
{
	size_t curFile;
	std::map<std::string, size_t> fileMap;

	for (curFile = 0; curFile < srcFiles.size(); curFile ++)
	{
		const VGM_FILE_INFO& srcFile = srcFiles[curFile];
		VGM_ASSIGNMENT val;
		val.refFileID = curFile;
		val.dstFileID = (size_t)-1;
		fileMap[srcFile.fileName] = assignList.size();
		assignList.push_back(val);
	}
	for (curFile = 0; curFile < dstFiles.size(); curFile ++)
	{
		const VGM_FILE_INFO& dstFile = dstFiles[curFile];
		std::map<std::string, size_t>::iterator mapIt = fileMap.find(dstFile.fileName);
		if (mapIt != fileMap.end())
		{
			VGM_ASSIGNMENT& vAssign = assignList[mapIt->second];
			vAssign.dstFileID = curFile;
		}
		else
		{
			// TODO: Try to match unambiguous, but only partly matching file names.
			// e.g. "01 Title Screen.vgz" vs. "01 Title.vgm"
			VGM_ASSIGNMENT val;
			val.refFileID = (size_t)-1;
			val.dstFileID = curFile;
			assignList.push_back(val);
		}
	}

	return;
}

static void PrintMappings(const VFINF_LIST& srcFiles, const VFINF_LIST& dstFiles, const VA_LIST& assignList)
{
	VA_LIST::const_iterator listIt;

	for (listIt = assignList.begin(); listIt != assignList.end(); ++listIt)
	{
		const char* srcName = "-";
		const char* dstName = "-";
		if (listIt->refFileID != (size_t)-1)
			srcName = srcFiles[listIt->refFileID].fileName.c_str();
		if (listIt->dstFileID != (size_t)-1)
			dstName = dstFiles[listIt->dstFileID].fileName.c_str();

		printf("%s == %s\n", srcName, dstName);
	}

	return;
}

static void ExecuteTransfer(const char* srcDir, const VFINF_LIST& srcFiles, const char* dstDir,
							const VFINF_LIST& dstFiles, const VA_LIST& assignList, UINT8 actions)
{
	VA_LIST::const_iterator listIt;
	std::string srcBasePath;
	std::string dstBasePath;
	const char* srcName;
	const char* dstName;
	std::string srcFilePath;
	std::string dstFilePath;
	UINT8 retVal;

	srcBasePath = Dir2Path(srcDir);
	dstBasePath = Dir2Path(dstDir);

	for (listIt = assignList.begin(); listIt != assignList.end(); ++listIt)
	{
		if (listIt->refFileID == (size_t)-1 ||
			listIt->dstFileID == (size_t)-1)
			continue;

		srcName = srcFiles[listIt->refFileID].fileName.c_str();
		dstName = dstFiles[listIt->dstFileID].fileName.c_str();
		printf("%s -> %s\n", srcName, dstName);

		if (actions & ACT_RETAG)
		{
			srcFilePath = srcBasePath + srcName;
			dstFilePath = dstBasePath + dstName;
			retVal = CopyTag(srcFilePath.c_str(), dstFilePath.c_str());
			if (retVal & 0x80)
				printf("Tag transfer failed with error code 0x%02X!\n", retVal);
		}
		if (actions & ACT_RENAME)
		{
			// rename dstPath/dstName to dstPath/srcName
			srcFilePath = dstBasePath + dstName;
			dstFilePath = dstBasePath + srcName;
			if (srcFilePath != dstFilePath)
				rename(srcFilePath.c_str(), dstFilePath.c_str());
		}
	}

	return;
}

static UINT8 CopyTag(const char* srcFile, const char* dstFile)
{
	gzFile hFileSrc;
	gzFile hFileDst;
	UINT8 hdrData[0x40];
	UINT32 gd3Ofs;
	UINT32 gd3Len;
	UINT8 gd3Hdr[0x0C];
	UINT32 eofOfs;
	UINT32 tempOfs;
	bool isGZ;
	std::vector<UINT8> gd3Data;
	std::vector<UINT8> fileData;

	hFileSrc = gzopen(srcFile, "rb");
	if (hFileSrc == NULL)
		return 0xF8;

	gzread(hFileSrc, hdrData, 0x40);
	if (memcmp(&hdrData[0x00], "Vgm ", 0x04))
	{
		gzclose(hFileSrc);
		return 0xF0;
	}

	memcpy(&gd3Ofs, &hdrData[0x14], 0x04);
	if (! gd3Ofs)
	{
		gzclose(hFileSrc);
		return 0x01;	// no tag to copy (destination file's tag is kept)
	}
	gzseek(hFileSrc, 0x14 + gd3Ofs, SEEK_SET);
	gzread(hFileSrc, gd3Hdr, 0x0C);
	if (memcmp(&gd3Hdr, "Gd3 ", 0x04))
	{
		gzclose(hFileSrc);
		return 0x80;	// bad source file tag
	}
	memcpy(&gd3Len, &gd3Hdr[0x08], 0x04);
	gd3Data.resize(gd3Len);
	gzread(hFileSrc, &gd3Data[0x00], gd3Len);

	gzclose(hFileSrc);	hFileSrc = NULL;

	hFileDst = gzopen(dstFile, "rb");
	if (hFileDst == NULL)
		return 0xF9;

	gzread(hFileDst, hdrData, 0x40);
	if (memcmp(&hdrData[0x00], "Vgm ", 0x04))
	{
		gzclose(hFileDst);
		return 0xF1;
	}
	isGZ = ! gzdirect(hFileDst);
	gzrewind(hFileDst);

	memcpy(&eofOfs, &hdrData[0x04], 0x04);
	eofOfs += 0x04;
	memcpy(&gd3Ofs, &hdrData[0x14], 0x04);
	if (gd3Ofs)
		gd3Ofs += 0x14;
	else if (! gd3Ofs)
		gd3Ofs = eofOfs;
	eofOfs = gd3Ofs + 0x0C + gd3Len;	// calculate new GD3 offset

	fileData.resize(gd3Ofs);	// load everything but the GD3 tag
	gzread(hFileDst, &fileData[0x00], fileData.size());
	gzclose(hFileDst);	hFileDst = NULL;

	tempOfs = eofOfs - 0x04;
	memcpy(&fileData[0x04], &tempOfs, 0x04);	// set new EOF offset
	tempOfs = gd3Ofs - 0x14;
	memcpy(&fileData[0x14], &tempOfs, 0x04);	// set new GD3 offset

	if (! isGZ)
	{
		FILE* hFile;

		hFile = fopen(dstFile, "wb");
		if (hFile == NULL)
			return 0xFF;

		fwrite(&fileData[0x00], 0x01, gd3Ofs, hFile);
		fwrite(gd3Hdr, 0x01, 0x0C, hFile);
		fwrite(&gd3Data[0x00], 0x01, gd3Len, hFile);

		fclose(hFile);
	}
	else
	{
		gzFile hFile;

		hFile = gzopen(dstFile, "wb");
		if (hFile == NULL)
			return 0xFF;

		gzwrite(hFile, &fileData[0x00], gd3Ofs);
		gzwrite(hFile, gd3Hdr, 0x0C);
		gzwrite(hFile, &gd3Data[0x00], gd3Len);

		gzclose(hFile);
	}

	return 0x00;
}
