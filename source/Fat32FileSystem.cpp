#include <shlwapi.h>
#include <windows.h>
#include <queue>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "Fat32FileSystem.h"

#pragma comment(lib,"shlwapi.lib")

CFat32FileSystem::CFat32FileSystem(IBaseReader	*prmReader)
	: CBaseFileSystem(prmReader)
	, m_clusterFlag()
	, m_fatSector()
	, m_fatTable()
	, m_fatNum()
{
}

CFat32FileSystem::~CFat32FileSystem()
{
	if (m_fatTable != 0) {
		free(this->m_fatTable);
		this->m_fatTable = 0;
	}
	if (this->m_clusterFlag != 0) {
		free(m_clusterFlag);
		this->m_clusterFlag = 0;
	}
}

void CFat32FileSystem::Init()
{
	//读取fat32头部数据
	this->ReadBuf((UCHAR*)&m_fatSector,0,512);

	this->m_fatNum = this->m_fatSector.BPB_FATSz32 * this->m_fatSector.BPB_BytsPerSec / 4;
	this->m_fatTable = (UINT32*)malloc(sizeof(UINT32)* this->m_fatNum);
	if (this->m_fatTable == NULL) return;

	memset(this->m_fatTable, 0, sizeof(UINT32) * this->m_fatNum);

	this->m_clusterFlag = (UCHAR *)malloc(sizeof(UCHAR)* this->m_fatNum);
	if (this->m_clusterFlag == 0) return;

	memset(this->m_clusterFlag, 0, this->m_fatNum);
	//读取文件分配表
	this->ReadBuf((UCHAR*)m_fatTable, this->m_fatSector.BPB_ResvdSecCnt, sizeof(UINT32)* this->m_fatNum);
	this->m_bytesPerSector = this->m_fatSector.BPB_BytsPerSec;
	this->m_sectorsPerCluster = this->m_fatSector.BPB_SecPerClus;
}

UINT64	CFat32FileSystem::ReadFileContent(CBaseFileObject *prmFileObject, UCHAR prmDstBuf[], UINT64 prmByteOff, UINT64 prmByteToRead)
{
	UINT64 tmpResult = 0;
	if (prmFileObject->GetFileType() != FILE_OBJECT_TYPE::FILE_OBJECT_TYPE_FILE)
	{
		return 0;
	}
	UINT64 tmpFileSize = prmFileObject->GetFileSize();
	File_Content_Extent	*tmpFileExtent = prmFileObject->GetFileExtent();
	//请求读取的文件偏移大于文件内容。
	if (prmByteOff > tmpFileSize)
	{
		prmByteOff = tmpFileSize;
	}
	//请求读取的字节数大于从prmByteoff到文件结束剩余字节数
	if (prmByteToRead > tmpFileSize - prmByteOff)
	{
		prmByteToRead = tmpFileSize - prmByteOff;
	}
	UINT64 tmpFileOffset = tmpFileExtent->startSector * 512 + prmByteOff;
	//当请求的偏移值不是512的倍数时，先读取不满512字节数据，后面再读取
	if (tmpFileOffset % 512 != 0)
	{
		UCHAR tmpBuf[512] = { 0 };
		UINT32 tmpAlignSize = tmpFileOffset % 512;
		this->ReadBuf(tmpBuf, tmpFileOffset / 512, 512);
		//需要读取512-tmpAlignSize
		tmpResult = 512 - tmpAlignSize;
		UINT32 tmpCopyBytes = prmByteToRead > 512 - tmpAlignSize ? 512 - tmpAlignSize : (UINT32)prmByteToRead;
		::memcpy(prmDstBuf, tmpBuf + tmpAlignSize, tmpCopyBytes);
		tmpFileOffset = tmpFileOffset + 512;
		
		if (prmByteToRead <= tmpResult)
		{
			return prmByteToRead;
		}
		prmByteToRead -= tmpResult;
	}
	tmpResult += this->ReadBuf(prmDstBuf + tmpResult, tmpFileOffset / 512, prmByteToRead);
	return tmpResult;
}

void CFat32FileSystem::GetDeletedFiles(vector<CBaseFileObject *> &fileArray)
{
	TCHAR fileName[MAX_PATH] = { 0 };
	UINT32 clusNum = 0;
	UINT64 offset = 0;
	std::queue<UINT32> dirs;

	dirs.push(2);//首先把root的簇号放到队列中

	UINT32 clusterSize = this->m_fatSector.BPB_BytsPerSec*m_fatSector.BPB_SecPerClus;
	UCHAR *szBuf = (UCHAR*)malloc(sizeof(UCHAR)*clusterSize);
	while (!dirs.empty())
	{
		clusNum = dirs.front();
		dirs.pop();
		//遍历文件夹的第个簇
		while (1)
		{
			offset = this->m_fatSector.BPB_FATSz32 * this->m_fatSector.BPB_NumFATs;
			offset += this->m_fatSector.BPB_ResvdSecCnt;
			offset += (clusNum - this->m_fatSector.BPB_RootClus)* this->m_fatSector.BPB_SecPerClus;
			this->ReadBuf(szBuf, offset, clusterSize);

			//遍历簇中的每个文件项
			for (UINT32 i = 0; i < clusterSize ; i += 32)
			{
				DIR_ENTRY *dirEntry = (DIR_ENTRY*)(szBuf + i);
				if (dirEntry->name[0] == 0)
				{
					break;
				}
				if (dirEntry->size == 0 || dirEntry->size == 0xFFFFFFFF)
				{
					//如果是文件夹，则需要分析文件夹中是否在删除的文件
					UINT32 tmpClusterNum = this->ParseStartCluster(dirEntry);
					if (tmpClusterNum != 0 && tmpClusterNum < m_fatNum && m_clusterFlag[tmpClusterNum] == 0)
					{
						dirs.push(tmpClusterNum);
						m_clusterFlag[tmpClusterNum] = 1;//避免重复对同一簇进行分析
					}
				}

				//记录当前文件项
				DIR_ENTRY *firstEntry = dirEntry;
				//找到第一个非长文件名标志的文件项
				while (dirEntry->attr == 0x0F)
				{
					i += 32;
					dirEntry = (DIR_ENTRY *)(szBuf + i);
				}
				//被删除的文件
				if (dirEntry->name[0] == 0xE5 && dirEntry->size != 0 && dirEntry->size != 0xFFFFFFFF)
				{
					if (firstEntry == dirEntry)
					{
						this->ParseShortFileName(dirEntry, fileName, MAX_PATH);
					}
					else
					{
						this->ParseLongFileName(fileName, MAX_PATH, firstEntry, dirEntry);
					}

					CBaseFileObject	*fileObject = new CBaseFileObject();
					File_Content_Extent	*fileExtent = 0;
					this->GetFileExtent(dirEntry, &fileExtent);
					fileObject->SetFileName(fileName);
					fileObject->SetFileSize(this->ParseFileSize(dirEntry));
					fileObject->SetAccessTime(this->ParseAccessDate(dirEntry));
					fileObject->SetCreateTime(this->ParseCreateDate(dirEntry));
					fileObject->SetModifyTime(this->ParseModifyDate(dirEntry));
					fileObject->SetFileExtent(fileExtent);
					fileArray.push_back(fileObject);
				}
			}
			clusNum = this->GetNextCluster(clusNum);
			if (clusNum == 0)
			{
				break;
			}
		}
	}
	free(szBuf);
}

UINT32	CFat32FileSystem::GetNextCluster(UINT32 prmCurCluster)
{
	if (prmCurCluster >= m_fatNum)
	{
		return 0;
	}
	if (m_fatTable[prmCurCluster] == EOC || m_fatTable[prmCurCluster] == 0x0FFFFFF8)
	{
		return 0;
	}
	return m_fatTable[prmCurCluster];
}

void CFat32FileSystem::ParseShortFileName(DIR_ENTRY *prmDirEntry, TCHAR prmFileName[], size_t prmFileNameLength)
{
	char tmpFileName[20] = {0};
	memset(prmFileName, 0, prmFileNameLength);
	int i = 0;
	for (i = 0; i < 8; i++)
	{
		if (prmDirEntry->name[i] == ' ')
		{
			break;
		}
		tmpFileName[i] = prmDirEntry->name[i];
	}
	if (prmDirEntry->name[8] != ' ')
	{
		tmpFileName[i++] = '.';
	}
	for (int j = 8; j < 11; j++)
	{
		if (prmDirEntry->name[j] == ' ')
		{
			break;
		}
		tmpFileName[i++] = prmDirEntry->name[j];
	}
#ifdef _UNICODE
	::MultiByteToWideChar(CP_THREAD_ACP,0,tmpFileName,-1,prmFileName,prmFileNameLength);
#else
	memcpy(prmFileName,tmpFileName,prmFileNameLength);
#endif
}

void CFat32FileSystem::ParseLongFileName(TCHAR prmLongFileName[], size_t prmFileNameLength, DIR_ENTRY *prmFirstEntry, DIR_ENTRY *prmLastEntry)
{
	TCHAR tmpFileName[1024] = { 0 };
	memset(prmLongFileName, 0, prmFileNameLength);
	UINT32 index = 0;
	//长文件名的获取方式为从下向上找的顺序排列
	DIR_LONG_ENTRY *startEntry = (DIR_LONG_ENTRY*)(prmFirstEntry);
	DIR_LONG_ENTRY *dirLongEntry = (DIR_LONG_ENTRY*)(prmLastEntry);
	dirLongEntry--;
	while (startEntry != dirLongEntry)
	{
		::memcpy((char*)tmpFileName + index, dirLongEntry->name0_4, 10);
		index += 10;
		::memcpy((char*)tmpFileName + index, dirLongEntry->name5_10, 12);
		index += 12;
		::memcpy((char*)tmpFileName + index, dirLongEntry->name11_12, 4);
		index += 4;
		dirLongEntry--;
	}
	::memcpy((char*)tmpFileName + index, startEntry->name0_4, 10);
	index += 10;
	::memcpy((char*)tmpFileName + index, startEntry->name5_10, 12);
	index += 12;
	::memcpy((char*)tmpFileName + index, startEntry->name11_12, 4);
	index += 4;
#ifndef _UNICODE
	DWORD tmpLength = ::WideCharToMultiByte(CP_THREAD_ACP, 0, (WCHAR*)tmpFileName, -1, NULL, 0, 0, 0);
	if (tmpLength > prmFileNameLength)
	{
		return;
	}
	::WideCharToMultiByte(CP_THREAD_ACP, 0, (WCHAR*)tmpFileName, -1, prmLongFileName, prmFileNameLength, 0, 0);
#else
	::memcpy(prmLongFileName,tmpFileName,_tcslen(tmpFileName)*sizeof(TCHAR));
#endif
}

UINT32 CFat32FileSystem::ParseStartCluster(DIR_ENTRY *prmDirEntry)
{
	UINT32 result = (prmDirEntry->starthi << 16)&(0xFFFF0000);
	result = result | (prmDirEntry->start & 0xFFFF);
	if (result == EOC || result == 0x0FFFFFF8)
	{
		return 0;
	}
	return result;
}

CBaseFileObject *CFat32FileSystem::ParseFileObject(DIR_ENTRY *prmFirstEntry,DIR_ENTRY *prmLastEntry)
{
	TCHAR tmpFileName[1024] = {0};
	CBaseFileObject *tmpFileObject = new CBaseFileObject;
	UINT32 tmpSectors = this->m_fatSector.BPB_FATSz32* this->m_fatSector.BPB_NumFATs;
	tmpSectors += this->m_fatSector.BPB_ResvdSecCnt;
	if(tmpFileObject==NULL)
	{
		return tmpFileObject;
	}
	if(prmFirstEntry == prmLastEntry)
	{
		this->ParseShortFileName(prmFirstEntry,tmpFileName,sizeof(tmpFileName));
	}
	else
	{
		this->ParseLongFileName(tmpFileName,sizeof(tmpFileName),prmFirstEntry,prmLastEntry);
	}
	tmpFileObject->SetFileName(tmpFileName);
	//解析出子文件开始的簇号，转换为相对于分区偏移的扇区数进行保存
	UINT32 tmpClusterNum = this->ParseStartCluster(prmLastEntry);
	tmpSectors += (tmpClusterNum- this->m_fatSector.BPB_RootClus)* this->m_fatSector.BPB_SecPerClus;
	tmpFileObject->SetFileStartSector(tmpSectors);
	if(prmLastEntry->attr & 0x10)
	{
		//目录属性
		tmpFileObject->SetFileSize(0);
		tmpFileObject->SetFileType(FILE_OBJECT_TYPE::FILE_OBJECT_TYPE_DIRECTORY);
	}
	else
	{
		tmpFileObject->SetFileSize(prmLastEntry->size);
		tmpFileObject->SetFileType(FILE_OBJECT_TYPE::FILE_OBJECT_TYPE_FILE);
	}
	return tmpFileObject;
}

UINT32 CFat32FileSystem::GetFileExtent(DIR_ENTRY *dirEntry, File_Content_Extent **prmExtent)
{
	File_Content_Extent *p = *prmExtent;
	UINT32 baseSector = this->m_fatSector.BPB_NumFATs* this->m_fatSector.BPB_FATSz32 + this->m_fatSector.BPB_ResvdSecCnt;
	UINT32 startCluster = this->ParseStartCluster(dirEntry);
	UINT32 clusterSize = this->m_fatSector.BPB_SecPerClus;
	if (startCluster == 0)
	{
		return 0;
	}
	if (p == NULL)
	{
		p = new File_Content_Extent();
		*prmExtent = p;
	}
	p->startSector = startCluster * this->m_fatSector.BPB_SecPerClus + baseSector - this->m_fatSector.BPB_RootClus* this->m_fatSector.BPB_SecPerClus;
	UINT32 fileSize = dirEntry->size;
	fileSize += 511;
	fileSize = fileSize & ~(511);
	p->totalSector = fileSize >> 9;

	return 1;
}

CStringUtil CFat32FileSystem::ParseCreateDate(DIR_ENTRY *dirEntry)
{
	USHORT time = dirEntry->ctime;
	USHORT date = dirEntry->cdate;
	UINT8 hour = (time & 0xF800) >> 11;
	UINT8 minute = (time & 0x7E0) >> 5;
	UINT8 second = (time & 0x1F);
	UINT8 year = (date & 0xFE00) >> 9;
	UINT8 month = (date & 0x1E0) >> 5;
	UINT8 day = (date & 0x1F);
	CStringUtil tmpResult;
	tmpResult.Format(TEXT("%04d-%02d-%02d %02d:%02d:%02d"), year + 1980, month, day, hour, minute, second);
	return tmpResult;
	//fileInfo->createDate = szBuf;
}

CStringUtil CFat32FileSystem::ParseModifyDate(DIR_ENTRY *dirEntry)
{
	USHORT time = dirEntry->time;
	USHORT date = dirEntry->date;
	UINT8 hour = (time & 0xF800) >> 11;
	UINT8 minute = (time & 0x7E0) >> 5;
	UINT8 second = (time & 0x1F);
	UINT8 year = (date & 0xFE00) >> 9;
	UINT8 month = (date & 0x1E0) >> 5;
	UINT8 day = (date & 0x1F);
	CStringUtil	tmpResult;
	tmpResult.Format(TEXT("%04d-%02d-%02d %02d:%02d:%02d"), year + 1980, month, day, hour, minute, second);
	return tmpResult;
	//fileInfo->modifyDate = szBuf;
}

CStringUtil CFat32FileSystem::ParseAccessDate(DIR_ENTRY *dirEntry)
{
	USHORT date = dirEntry->adate;
	UINT8 year = (date & 0xFE00) >> 9;
	UINT8 month = (date & 0x1E0) >> 5;
	UINT8 day = (date & 0x1F);
	CStringUtil	tmpResult;
	tmpResult.Format(TEXT("%04d-%02d-%02d"), year + 1980, month, day);
	return tmpResult;
}
