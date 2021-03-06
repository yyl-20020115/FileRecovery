#include "NtfsFileSystem.h"
#include "CommonUtils.h"
#include <shlwapi.h>
#include <iostream>

#pragma comment(lib,"shlwapi.lib")

CNtfsFileSystem::CNtfsFileSystem(IBaseReader *prmReader)
	: CBaseFileSystem(prmReader)
	, m_mftStartCluster()
	, m_clustersPerIndex()
	, m_mftRunList() {}

CNtfsFileSystem::~CNtfsFileSystem()
{
	if (this->m_mftRunList != 0) {
		this->FreeRunList(this->m_mftRunList);
		this->m_mftRunList = 0;
	}
}

void CNtfsFileSystem::Init()
{
	UCHAR szBuf[512] = { 0 };
	this->ReadBuf(szBuf, 0, 512);
	//this->m_vm->ReadSector(this->m_startSecotr, 512, szBuf);
	UINT16 *bytesPerSector = (UINT16*)&szBuf[0xB];
	this->SetBytesPerSector(*bytesPerSector);
	UINT8 *sectorsPerCluster = (UINT8*)&szBuf[0xD];
	this->SetSectorsPerCluster(*sectorsPerCluster);
	this->m_mftStartCluster = *((UINT64*)&szBuf[0x30]);
	this->m_clustersPerIndex = *((UINT32*)&szBuf[0x44]);
	//获取$MFT文件0x80数据运行流
	this->GetMFTRunList();
}

UINT64 CNtfsFileSystem::ReadFileContent(CBaseFileObject *prmFileObject, UCHAR prmDstBuf[], UINT64 prmByteOff, UINT64 prmByteToRead)
{
	return this->ReadFileContent(prmDstBuf,prmByteOff,prmByteToRead,
		prmFileObject->GetFileSize(), prmFileObject->GetFileExtent());
}

void CNtfsFileSystem::GetDeletedFiles(vector<CBaseFileObject*> &fileArray)
{
	UINT32 clusterSize = this->m_sectorsPerCluster * this->m_bytesPerSector;
	UCHAR szFileName[MAX_PATH * 4] = { 0 };
	CHAR szAnsiName[MAX_PATH * 4] = { 0 };
	UCHAR szAttrValue[1024] = { 0 };
	UINT32 nameLenOffset = 0;
	UINT32 nameLen = 0;
	UINT16 usnOffset = 0;
	UINT32 ms = sizeof(UCHAR) * clusterSize;
	//FileInfo	fileInfo;
	//分配一个簇的大小
	UCHAR *szBuf = (UCHAR*)malloc(ms);
	if (szBuf == 0) return;

	Ntfs_Data_Run *p = this->m_mftRunList;
	//遍历mft 0x80属性所有运行数据，运行数据所存数据为mft记录头。每个mft记录头1024字节
	while (p != 0)
	{
		for (UINT64 i = 0; i < p->length; i++)
		{
			UINT64 clusterOffset = p->lcn * this->m_sectorsPerCluster + i * this->m_sectorsPerCluster;
			this->ReadBuf(szBuf, clusterOffset, clusterSize);
			usnOffset = *(UINT16*)&szBuf[4];
			for (size_t k = 0; k < this->m_sectorsPerCluster; k++)
			{
				memcpy(szBuf + 0x1FE + k * this->m_bytesPerSector, szBuf + usnOffset + 2 + k * 2, 2);//恢复每个扇区最后两个字节数据
			}
			for (size_t k = 0; k < clusterSize ; k += 1024)
			{
				NtfsRecordFlag flag = (NtfsRecordFlag) *(szBuf + k + 0x16);
				if (flag == NtfsRecordFlag::FileDeleted || flag == NtfsRecordFlag::DirectoryDeleted) 
				{
#ifdef _DEBUG
					UINT64 seqNo = p->vcn * this->m_sectorsPerCluster / 2;
					seqNo += i * this->m_sectorsPerCluster / 2 + k / 1024;
#endif
					//fileInfo.seqNo = p->vcn*m_ntfsType->sectorsPerCluster / 2;
					//fileInfo.seqNo += i*m_ntfsType->sectorsPerCluster / 2 + j / 1024;
					if (!this->GetAttrValue(NTFS_ATTRDEF::ATTR_FILE_NAME, szBuf + k, (UCHAR*)szAttrValue))
					{
						continue;
					}
					nameLenOffset = 0x58;
					nameLen = *(UINT8*)(szAttrValue + nameLenOffset);
					//获取文件名
					memset(szFileName, 0, sizeof(szFileName));
					memcpy(szFileName, szAttrValue + nameLenOffset + 2,((size_t)nameLen) << 1);
#ifndef _UNICODE
					::WideCharToMultiByte(CP_THREAD_ACP, 0, (LPCWSTR)szFileName, -1, szAnsiName, MAX_PATH * 2, 0, 0);
#endif
					//NOTICE: for debugging
					//std::wcout << szFileName << std::endl;

					//用于设置文件内容占用了哪些扇区信息
					File_Content_Extent	*fileExtents = 0;

					this->GetFileExtent(szBuf + k, clusterOffset + k / 512, &fileExtents);

					CBaseFileObject	*fileObject = new CBaseFileObject();
#ifdef _UNICODE
					fileObject->SetFileName(szFileName);
#else
					fileObject->SetFileName(szAnsiName);
#endif
					fileObject->SetFileType(
						flag == NtfsRecordFlag::FileDeleted
						? FILE_OBJECT_TYPE::FILE_OBJECT_TYPE_FILE
						: FILE_OBJECT_TYPE::FILE_OBJECT_TYPE_DIRECTORY
					);
					fileObject->SetFileSize(this->GetFileSize(szBuf+k));
					//设置文件内容占用扇区信息
					fileObject->SetFileExtent(fileExtents);
					fileObject->SetAccessTime(this->GetAccessTime(szBuf+k));
					fileObject->SetCreateTime(this->GetCreateTime(szBuf+k));
					fileObject->SetModifyTime(this->GetModifyTime(szBuf+k));

					if (flag == NtfsRecordFlag::DirectoryDeleted)
					{
						//if (this->GetAttrValue(NTFS_ATTRDEF::ATTR_INDEX_ROOT, szBuf + k,
						//	(UCHAR*)szAttrValue))
						//{
						//	//std::vector<CBaseFileObject*> fs;
						//	//this->GetFileFromIndexRoot(szAttrValue, &fs);
						//}
					}


					fileArray.push_back(fileObject);
				}
			}
		}
		p = p->next;
	}
	::free(szBuf);
}

void CNtfsFileSystem::GetMFTRunList()
{
	UCHAR tmpBuf[1024] = { 0 };
	UCHAR tmpAttrValue[1024] = { 0 };
	this->ReadBuf(tmpBuf, this->m_mftStartCluster * this->m_sectorsPerCluster,1024);
	UINT16 usnOffset = *(UINT16*)&tmpBuf[4];//更新序列号的偏移值
	memcpy(tmpBuf + 0x1FE, tmpBuf + usnOffset + 2, 2);//恢复第一个扇区最后两字节真实数据
	memcpy(tmpBuf + 0x3FE, tmpBuf + usnOffset + 4, 2);//恢复第二个扇区最后两字节真实数据
	if (this->GetAttrValue(NTFS_ATTRDEF::ATTR_DATA, tmpBuf, tmpAttrValue))
	{
		UINT16 dataRunOffset = *(UINT16*)&tmpAttrValue[0x20];
		this->GetDataRunList(tmpAttrValue, dataRunOffset, &m_mftRunList);
	}
}

UINT32 CNtfsFileSystem::GetAttrValue(NTFS_ATTRDEF prmAttrTitle,UCHAR prmBuf[],UCHAR *prmAttrValue)
{
	UINT32 fileSign = *(UINT32*)prmBuf;
	if (fileSign != 0x454c4946) return 0;

	UINT16 flag = *(UINT16*)&prmBuf[0x16];
	/*if (flag == 0 || flag == 2)
	{
		//文件或目录被删除
		return 0;
	}*/
	//mft记录头开始偏移20处的两个字节为mft第一个属性的偏移值
	UINT32 attrOffset = *(UINT16*)&prmBuf[20];//第一个属性的偏移值

	UINT32 attrLen = 0;//属性的长度
	while (attrOffset < 1024)
	{
		if (prmBuf[attrOffset] == 0xff) return 0;

		attrLen = *(UINT16*)&prmBuf[attrOffset + 4];
		//找到对应的属性，将其属性值拷到szAttrValue中
		if (prmBuf[attrOffset] == (int)prmAttrTitle)
		{
			memcpy(prmAttrValue, prmBuf + attrOffset, attrLen);
			return 1;
		}
		attrOffset += attrLen;
	}
	return 0;
}

UINT32 CNtfsFileSystem::GetAttrFromAttributeList(NTFS_ATTRDEF prmAttrType,UINT32 prmOffset,UCHAR *prmAttrList,UCHAR *prmAttrValue)
{
	UINT32 *tmpAttrLen = (UINT32*)(prmAttrList+4);
	while(prmOffset+4<*tmpAttrLen)
	{
		UINT16 *tmpLen = (UINT16*)(prmAttrList+prmOffset+4); 
		if(prmAttrList[prmOffset]== (int)prmAttrType)
		{
			memcpy(prmAttrValue,prmAttrList+prmOffset,*tmpLen);
			return prmOffset;
		}
		if (*tmpLen == 0) break;
		prmOffset += *tmpLen;
	}
	return 0;
}

void CNtfsFileSystem::GetDataRunList(UCHAR *prmBuf,UINT16 prmRunListOffset,Ntfs_Data_Run **prmList)
{
	int j = 0;
	UINT64 index_alloc_size = 0;
	UINT64 lcn = 0;
	UINT16 bufOffset = prmRunListOffset;
	UINT32 t = 0;
	*prmList = 0;
	Ntfs_Data_Run *p = 0;

	//buff[off]  低四位记录后面几个字节为长度 , 高4位记录几个字节为起始簇号
	//先计算长度
	for (;;)
	{
		index_alloc_size = 0;
		for (j = 0; j < prmBuf[bufOffset] % 16; j++)
		{
			index_alloc_size = index_alloc_size + prmBuf[bufOffset + 1 + j] * (UINT64)pow((long double)256, j);
		}

		//, 再计算起始簇号
		if (prmBuf[bufOffset + prmBuf[bufOffset] % 16 + prmBuf[bufOffset] / 16] > 127)  //负数则求补码后减
		{
			for (j = 0; j < prmBuf[bufOffset] / 16; j++)
			{
				t = ~prmBuf[bufOffset + prmBuf[bufOffset] % 16 + 1 + j];
				t = t & 255;
				lcn = lcn - t * (UINT64)pow((long double)256, j);
			}
			lcn = lcn - 1;
		}
		else
		{
			for (j = 0; j < prmBuf[bufOffset] / 16; j++)
			{
				lcn = lcn + prmBuf[bufOffset + prmBuf[bufOffset] % 16 + 1 + j] * (UINT64)pow((long double)256, j);
			}
		}
		Ntfs_Data_Run *datarun = new Ntfs_Data_Run();
		if (*prmList == NULL)
		{
			*prmList = datarun;
		}
		datarun->lcn = lcn;
		datarun->vcn = 0;
		datarun->length = index_alloc_size;//表示该数据流占用多少簇

		if (p != 0)
		{
			datarun->vcn += p->length;
			p->next = datarun;
		}
		p = datarun;

		//获取下一个run list 的偏移位置
		bufOffset = bufOffset + prmBuf[bufOffset] / 16 + prmBuf[bufOffset] % 16 + 1;
		if (0 == prmBuf[bufOffset])
		{
			// run list 结束
			break;
		}
	}
}

void CNtfsFileSystem::FreeRunList(Ntfs_Data_Run *prmList)
{
	Ntfs_Data_Run *p = prmList;
	Ntfs_Data_Run *q = 0;
	while (p)
	{
		q = p->next;
		delete p;
		p = q;
	}
}

UINT32 CNtfsFileSystem::GetExtendMFTAttrValue(UINT64 prmSeqNum,NTFS_ATTRDEF prmAttrType,UCHAR *prmAttrValue)
{
	UCHAR tmpBuf[1024] = {0};
	prmSeqNum = prmSeqNum & MFTREFMASK;
	UINT64 tmpOffset = this->GetOffsetByMFTRef(prmSeqNum);
	this->ReadBuf(tmpBuf,tmpOffset/512,1024);
	UINT16 tmpUsnOffset = *(UINT16*)&tmpBuf[4];
	if ((size_t)tmpUsnOffset + 6 < sizeof(tmpBuf)) {
		memcpy(tmpBuf + 0x1FE, tmpBuf + tmpUsnOffset + 2, 2);
		memcpy(tmpBuf + 0x3FE, tmpBuf + tmpUsnOffset + 4, 2);
		if (this->GetAttrValue(prmAttrType, tmpBuf, prmAttrValue))
		{
			return 1;
		}
	}
	return 0;
}

UINT64 CNtfsFileSystem::GetOffsetByMFTRef(UINT64 prmSeqNo)
{
	prmSeqNo = prmSeqNo & MFTREFMASK;
	UINT64 tmpOffset = prmSeqNo<<1;
	tmpOffset = tmpOffset * this->m_bytesPerSector;
	UINT64 tmpVCN = tmpOffset / this->m_sectorsPerCluster / this->m_bytesPerSector;
	Ntfs_Data_Run *p = this->m_mftRunList;
	while(p!= 0)
	{
		if (tmpVCN > p->vcn && tmpVCN < p->vcn + p->length) break;
		p = p->next;
	}
	if (p == 0) return 0;

	UINT64 tmpOfs = tmpOffset-p->vcn * this->m_sectorsPerCluster * this->m_bytesPerSector;
	return tmpOfs + p->lcn * this->m_sectorsPerCluster * this->m_bytesPerSector;
}

void CNtfsFileSystem::GetFileFromIndexRoot(UCHAR *prmAttrValue,vector<CBaseFileObject*> *prmFileArray)
{
	if (prmAttrValue[0] != (int)NTFS_ATTRDEF::ATTR_INDEX_ROOT) return;
	//0x90属性长度
	UINT32 tmpAttrLen = *(UINT32*)(prmAttrValue+4);
	//第一个索引项偏移值
	UINT32 tmpAttrOffset = *(UINT32*)(prmAttrValue+0x30);
	//找ddddddddddddd到从0x30到最后一个索引项尾的偏移值
	UINT32 tmpTotalSize = *(UINT32*)(prmAttrValue+0x34);
	//真实索引项大小需要减去从0x30到第一索引的偏移量值
	tmpTotalSize -= tmpAttrOffset;
	tmpAttrOffset += 0x30;
	UINT8 tmpFlags = *(UINT8*)(prmAttrValue+0x3C);
	if(tmpFlags==1)
	{
		//需要外部索引才行。这个就不需要再解析了
		return;
	}
	if (tmpAttrOffset >= tmpAttrLen || tmpAttrOffset + tmpTotalSize > tmpAttrLen) return;
	this->ParseFileFromIndex(prmAttrValue,tmpAttrOffset,tmpTotalSize,prmFileArray);
}

void CNtfsFileSystem::GetFileFromAllocIndex(UCHAR *prmAttrValue,vector<CBaseFileObject*> *prmFileArray)
{
	if (prmAttrValue[0] != (int)NTFS_ATTRDEF::ATTR_INDEX_ALLOCATION) return;
	UINT32 tmpAttrLen = *(UINT32*)(prmAttrValue+4);
	UINT16 tmpIndexOffset = *(UINT16*)(prmAttrValue+0x20);
	if (tmpIndexOffset >= tmpAttrLen) return;

	UINT32 tmpClusterSize = m_sectorsPerCluster * m_bytesPerSector;
	UCHAR *tmpClusterBuf = (UCHAR*)malloc(tmpClusterSize);
	if (tmpClusterBuf == 0) return;
	Ntfs_Data_Run *tmpDataRunList = 0;
	this->GetDataRunList(prmAttrValue,tmpIndexOffset,&tmpDataRunList);
	Ntfs_Data_Run *p = tmpDataRunList;
	
	while(p!=0)
	{
		//每次只处理一个簇的大小
		for(UINT64 i=0;i<p->length;i++)
		{
			this->ReadBuf(tmpClusterBuf,p->lcn*m_sectorsPerCluster + i * this->m_sectorsPerCluster,tmpClusterSize);
			//更新序列号更正
			UINT16 usnOffset = *(UINT16*)&tmpClusterBuf[4];
			for (UINT32 j = 0; j < this->m_sectorsPerCluster; j++)
			{
				memcpy(tmpClusterBuf + 0x1FE + j * (size_t)this->m_bytesPerSector, tmpClusterBuf + usnOffset + 2 + j * 2, 2);//恢复每个扇区最后两个字节数据
			}
			UINT32 tmpFirstIndex = *(UINT32*)(tmpClusterBuf+0x18);
			tmpFirstIndex += 0x18;
			UINT32 tmpLastIndex = *(UINT32*)(tmpClusterBuf+0x1C);
			this->ParseFileFromIndex(tmpClusterBuf,tmpFirstIndex,tmpLastIndex,prmFileArray);
		}
		p = p->next;
	}
	this->FreeRunList(tmpDataRunList);
	::free(tmpClusterBuf);
}

void CNtfsFileSystem::ParseFileFromIndex(UCHAR *prmBuf,UINT16 prmOffset,UINT32 prmBufLen,vector<CBaseFileObject*> *prmFileArray)
{
	wchar_t tmpUnicodeFileName[MAX_PATH] = {0};
	char tmpAnsiName[MAX_PATH*2] = {0};
	
	UINT8 tmpNameSpace = 0;
	UINT8 tmpFileNameLen = 0;
	UINT32 tmpAttrOffset = prmOffset;
	UINT64 tmpMFTNum = 0;//mft参考号

	while(tmpAttrOffset<prmBufLen)
	{
		UINT16 tmpIndexItemLen = *(UINT16*)(prmBuf+tmpAttrOffset+8);
		if(prmBuf[tmpAttrOffset+6]==0 && prmBuf[tmpAttrOffset+7]==0)
		{
			tmpAttrOffset += tmpIndexItemLen;
			continue;
		}
		//文件命名空间
		tmpNameSpace = *(UINT8*)(prmBuf+tmpAttrOffset+0x51);
		tmpFileNameLen = *(UINT8*)(prmBuf+tmpAttrOffset + 0x50);
		tmpMFTNum = *(UINT64*)(prmBuf+tmpAttrOffset);
		tmpMFTNum = tmpMFTNum & MFTREFMASK;
		if(tmpMFTNum<=25)
		{
			//mft参考号小于25的都可以不用考虑
			tmpAttrOffset += tmpIndexItemLen;
			continue;
		}
		UINT64 tmpFileOffset = this->GetOffsetByMFTRef(tmpMFTNum);
		//文件没有存放到最终结果数组中，再将新的文件加入到数组中
		if(!IsFileExists(tmpFileOffset/m_bytesPerSector,prmFileArray))
		{
			CBaseFileObject	*tmpFileObject = new CBaseFileObject();
			//只有在文件命名空间为dos的时候才去mft头部解析win32文件名，加快解析速度。
			//否则像windows这样的目录下有几千个文件，就至少需要几千次的磁盘读请求，效率极低
			if(tmpNameSpace==2)
			{
				//if(!this->SetFileWin32Name(tmpFileOffset,tmpFileObject))
				CStringUtil	tmpFileNameStr =  this->GetFileWin32Name(tmpFileOffset);
				tmpFileObject->SetFileName(tmpFileNameStr);
				if(tmpFileNameStr.GetLength()==0)
				{
					//如果index中是dos命名空间，在mft头部没有找到win32命名空间的文件名。这样的文件不要
					delete tmpFileObject;
					tmpAttrOffset += tmpIndexItemLen;
					continue;
				}
			}
			else
			{
				memset(tmpUnicodeFileName,0,sizeof(tmpUnicodeFileName));
				memcpy(tmpUnicodeFileName,prmBuf+tmpAttrOffset+0x52, ((size_t)tmpFileNameLen)<<1);
#ifdef _UNICODE
				tmpFileObject->SetFileName(tmpUnicodeFileName);
#else
				memset(tmpAnsiName,0,sizeof(tmpAnsiName));
				::WideCharToMultiByte(CP_ACP,0,tmpUnicodeFileName,-1,tmpAnsiName,MAX_PATH*2,0,0);
				tmpFileObject->SetFileName(tmpAnsiName);
#endif
			}
			tmpFileObject->SetFileStartSector(tmpFileOffset/m_bytesPerSector);
			UINT32 tmpFileFlags = *(UINT32*)(prmBuf+tmpAttrOffset+0x48);
			if(tmpFileFlags & 0x10000000)
			{
				tmpFileObject->SetFileType(FILE_OBJECT_TYPE::FILE_OBJECT_TYPE_DIRECTORY);
			}
			else
			{
				tmpFileObject->SetFileType(FILE_OBJECT_TYPE::FILE_OBJECT_TYPE_FILE);
			}
			UINT64	tmpFileSize = *(UINT64*)(prmBuf+tmpAttrOffset+0x40);
			tmpFileObject->SetFileSize(tmpFileSize);
			prmFileArray->push_back(tmpFileObject);
			
		}
		tmpAttrOffset += tmpIndexItemLen;
	}
}

CStringUtil CNtfsFileSystem::GetFileWin32Name(UINT64 prmOffset)
{
	UCHAR tmpBuf[1024*2] = {0};
	wchar_t tmpFileName[MAX_PATH] = {0};
	char tmpAnsiName[MAX_PATH*2] = {0};
	this->ReadBuf(tmpBuf,prmOffset/m_bytesPerSector,1024);
	UINT16 tmpUsnOffset = *(UINT16*)&tmpBuf[4];
	memcpy(tmpBuf+0x1FE,tmpBuf+tmpUsnOffset+2,2);
	memcpy(tmpBuf+0x3FE,tmpBuf+tmpUsnOffset+4,2);
	UINT32 fileSign = *(UINT32*)tmpBuf;
	if (fileSign != 0x454c4946) return FALSE;
	UINT16 flag = *(UINT16*)&tmpBuf[0x16];
	if (flag == 0 || flag == 2) return FALSE;
	//mft记录头开始偏移20处的两个字节为mft第一个属性的偏移值
	UINT32 attrOffset = *(UINT16*)&tmpBuf[20];//第一个属性的偏移值

	UINT32 attrLen = 0;//属性的长度
	while (attrOffset < 1024)
	{
		if (tmpBuf[attrOffset] == 0xff) return 0;

		attrLen = *(UINT16*)&tmpBuf[attrOffset + 4];
		//找到对应的属性，将其属性值拷到szAttrValue中
		if (tmpBuf[attrOffset] == (int)NTFS_ATTRDEF::ATTR_FILE_NAME && tmpBuf[attrOffset+0x59]!=2)
		{
			UINT8	tmpFileLen = tmpBuf[attrOffset + 0x58];
			memset(tmpFileName,0,sizeof(tmpFileName));
			memcpy(tmpFileName,tmpBuf+attrOffset+0x5A, ((size_t)tmpFileLen)<<1);
#ifdef _UNICODE
			return tmpFileName;
#else
			::WideCharToMultiByte(CP_ACP,0,tmpFileName,-1,tmpAnsiName,MAX_PATH*2,0,0);
			return tmpAnsiName;
#endif
			
		}
		attrOffset += attrLen;
	}
	return TEXT("");
}

BOOL CNtfsFileSystem::IsFileExists(UINT64 prmStartSector,vector<CBaseFileObject*> *prmFileArray)
{
	for(size_t i=0;i<prmFileArray->size();i++)
	{
		if(prmStartSector == prmFileArray->at(i)->GetFileStartSector())
		{
			return TRUE;
		}
	}
	return FALSE;
}

void CNtfsFileSystem::GetFileExtent(UCHAR *prmBuf,UINT64 prmMftSector,File_Content_Extent **prmFileExtent)
{
	UCHAR szAttrValue[1024] = { 0 };
	UCHAR szAttrList[1024] = {0};
	UCHAR szExtentAttrValue[1024] = {0};
	UINT32 readBytes = 0;
	Ntfs_Data_Run *runList = NULL;
	Ntfs_Data_Run *q = NULL;
	UINT32 attrLen = 0;
	UINT32 attrOff = 0;
	UINT32 result = 0;

	//读取0x20属性列表
	if(this->GetAttrValue(NTFS_ATTRDEF::ATTR_ATTRIBUTE_LIST,prmBuf,szAttrList))
	{
		UINT32 tmpOffset = 0x18;
		while(tmpOffset=this->GetAttrFromAttributeList(NTFS_ATTRDEF::ATTR_DATA,tmpOffset,szAttrList,szAttrValue))
		{
			UINT16 tmpLen = *(UINT16*)(szAttrValue+4);
			UINT64 seqNum = *(UINT64*)(szAttrValue + 0x10);
			if(this->GetExtendMFTAttrValue(seqNum, NTFS_ATTRDEF::ATTR_DATA,szExtentAttrValue))
			{
				UINT16 runlistOffset = *(UINT16*)&szExtentAttrValue[0x20];
				Ntfs_Data_Run *p = 0;
				this->GetDataRunList(szExtentAttrValue,runlistOffset,&p);
				if (runList == 0)
				{
					runList = p;
					q = p;
				}
				else
				{//0x20中有多个0x80数据属性，需要合并多个run list
					while (q->next)
					{
						q = q->next;
					}
					q->next = p;
				}
			}
			tmpOffset += tmpLen;
		}
	}
	if(runList!=NULL)
	{
		goto Quit;
	}
	//读取0x80数据运行
	if (this->GetAttrValue(NTFS_ATTRDEF::ATTR_DATA, prmBuf, szAttrValue))
	{
		attrLen = *(UINT32*)&szAttrValue[4];
		if (szAttrValue[8] == 0)
		{//常驻属性，说明文件内容在0x80属性内
			UINT32 fileLength = *(UINT32*)&szAttrValue[0x10];
			*prmFileExtent = new File_Content_Extent();
			(*prmFileExtent)->startSector = prmMftSector;
			(*prmFileExtent)->totalSector = 2;
			(*prmFileExtent)->isPersist = 1;
		}
		else //文件内容为非常驻
		{
			UINT16 runlistOffset = *(UINT16*)&szAttrValue[0x20];
			this->GetDataRunList(szAttrValue, runlistOffset, &runList);
			goto Quit;
		}
	}
Quit:
	Ntfs_Data_Run *p = runList;
	File_Content_Extent *tmpExtent = *prmFileExtent;
	while (p != 0 && p->lcn != 0 && p->length != 0 && p->length < m_totalSector / m_sectorsPerCluster)
	{
		File_Content_Extent *t = new File_Content_Extent();
		t->totalSector = p->length * this->m_sectorsPerCluster;
		t->startSector = p->lcn * this->m_sectorsPerCluster;
		//设置哪些扇区是删除文件
		if (tmpExtent == 0)
		{
			tmpExtent = t;
			*prmFileExtent = t;
		}
		else
		{
			tmpExtent->next = t;
			tmpExtent = t;
		}
		p = p->next;
	}
	this->FreeRunList(runList);
}

UINT64 CNtfsFileSystem::ReadFileContent(UCHAR prmDstBuf[],UINT64 prmByteOff,UINT64 prmByteToRead, UINT64 prmFileSize,File_Content_Extent *prmFileExtent)
{
	UINT64 tmpResult = 0;
	UINT64 tmpByteRead = 0;
	if (prmByteOff >= prmFileSize) return 0;
	if (prmByteOff+prmByteToRead > prmFileSize)
	{
		prmByteToRead = prmFileSize - prmByteOff;
	}
	
	//文件内容是常驻属性，直接读取MFT记录返回文件内容
	if(prmFileExtent->isPersist)
	{
		UCHAR szBuf[1024] = { 0 };
		UCHAR szAttr[1024] = { 0 };
		this->ReadBuf(szBuf, prmFileExtent->startSector, 1024);
		UINT16 usnOffset = *(UINT16*)&szBuf[4];//更新序列号的偏移值
		memcpy(szBuf + 0x1FE, szBuf + usnOffset + 2, 2);//恢复第一个扇区最后两字节真实数据
		memcpy(szBuf + 0x3FE, szBuf + usnOffset + 4, 2);//恢复第二个扇区最后两字节真实数据
		if (this->GetAttrValue(NTFS_ATTRDEF::ATTR_DATA, szBuf, szAttr))
		{
			memcpy(prmDstBuf, szAttr + 0x18 + (UINT32)prmByteOff, (UINT32)prmByteToRead);
			return (UINT32)prmByteToRead;
		}
		return 0;
	}

	File_Content_Extent *p = prmFileExtent;
	//prmByteOff所在扇区信息
	while(p && prmByteOff >= p->totalSector * this->m_bytesPerSector)
	{
		prmByteOff -= p->totalSector*m_bytesPerSector;
		p = p->next;
	}

	if (p == 0) return 0;

	//处理文件偏移不是512整数倍的情况
	if(prmByteOff %512 != 0)
	{
		UCHAR tmpBuf[512] = { 0 };
		tmpByteRead = 512 - prmByteOff % 512;
		if (prmByteToRead <= tmpByteRead)
		{
			tmpByteRead = prmByteToRead;
		}
		//prmByteOff向下取512倍数，读取的512字节从[prmByteOff%512,512)缓冲区即要读数据
		this->ReadBuf(tmpBuf,p->startSector + prmByteOff/512,512);
		memcpy(prmDstBuf,tmpBuf+prmByteOff%512,(UINT32)tmpByteRead);
		tmpResult = tmpByteRead;
		if (tmpByteRead == prmByteToRead)
		{
			return tmpResult;
		}
		//偏移调整至512倍数。
		prmByteOff += tmpByteRead;
		//调整后的偏移超出当前块的范围，需要找下一块内容
		if(prmByteOff >= p->totalSector * this->m_bytesPerSector)
		{
			prmByteOff = prmByteOff - p->totalSector * this->m_bytesPerSector;
			p = p->next;
			if(p == 0)
			{
				return tmpResult;
			}
		}
	}

	//计算当前块还有多少可读字节
	UINT64 tmpRunListRemainBytes = p->totalSector * this->m_bytesPerSector - prmByteOff;
	//计算文件偏移映射到磁盘后扇区值
	UINT64 tmpOff = p->startSector + prmByteOff / 512;

	while(p && (tmpRunListRemainBytes < (prmByteToRead-tmpResult)) )
	{
		//整块连续扇区读取
		tmpByteRead = this->ReadBuf(prmDstBuf+tmpResult,tmpOff,tmpRunListRemainBytes);
		tmpResult += tmpByteRead;
		if (tmpByteRead != tmpRunListRemainBytes)
		{
			return tmpResult;
		}
		p = p->next;
		if (p == 0)
		{
			return tmpResult;
		}
		tmpRunListRemainBytes = p->totalSector * this->m_bytesPerSector;
		tmpOff = p->startSector;
	}

	//循环结束，剩余请求字节数小于块的大小。
	if(prmByteToRead != tmpResult)
	{
		tmpByteRead  = this->ReadBuf(prmDstBuf + tmpResult, tmpOff, prmByteToRead-tmpResult);
		tmpResult += tmpByteRead;
	}
	return tmpResult;
}

void CNtfsFileSystem::FreeFileExtent(File_Content_Extent *prmFileExtent)
{
	File_Content_Extent *p = prmFileExtent;
	File_Content_Extent *q = 0;
	while (p != 0)
	{
		q = p;
		p = p->next;
		delete q;
	}
}

UINT64 CNtfsFileSystem::GetOffsetByFileName(UINT64 prmParentFileOffset,const CStringUtil &prmFileName)
{
	UCHAR tmpBuf[1024] = {0};
	UCHAR tmpAttrValue[1024] = {0};
	UCHAR tmpAttrList[1024] = {0};
	UINT64 tmpFileOffset = 0;
	if (prmParentFileOffset == 0) return 0;

	this->ReadBuf(tmpBuf,prmParentFileOffset/ this->m_bytesPerSector,1024);
	UINT16	tmpUsnOffset = *(UINT16*)&tmpBuf[4];//更新序列号的偏移值
	memcpy(tmpBuf+0x1FE,tmpBuf+tmpUsnOffset+2,2);
	memcpy(tmpBuf+0x3FE,tmpBuf+tmpUsnOffset+4,2);
	//获取ATTR_LIST $0x20列表属性
	if(this->GetAttrValue(NTFS_ATTRDEF::ATTR_ATTRIBUTE_LIST,tmpBuf,tmpAttrList))
	{
		UCHAR tmpExtentMFTValue[1024] = { 0 };
		UINT32 tmpOffset = 0x18;
		while(tmpOffset=this->GetAttrFromAttributeList(NTFS_ATTRDEF::ATTR_INDEX_ROOT,tmpOffset,tmpAttrList,tmpAttrValue))
		{
			UINT16 *tmpLen = (UINT16*)(tmpAttrValue+4);
			tmpOffset += *tmpLen;
			UINT64 seqNum = *(UINT64*)&tmpAttrValue[0x10];
			seqNum = seqNum & MFTREFMASK;
			this->GetExtendMFTAttrValue(seqNum, NTFS_ATTRDEF::ATTR_INDEX_ROOT,tmpExtentMFTValue);
			tmpFileOffset = this->GetOffsetFromRootByFileName(tmpExtentMFTValue,prmFileName);
			if(tmpFileOffset!=0)
			{
				return tmpFileOffset;
			}
		}

		tmpOffset = 0x18;
		while(tmpOffset=this->GetAttrFromAttributeList(NTFS_ATTRDEF::ATTR_INDEX_ALLOCATION,tmpOffset,tmpAttrList,tmpAttrValue))
		{
			UINT16 *tmpLen = (UINT16*)(tmpAttrValue+4);
			tmpOffset += *tmpLen;
			UINT64 seqNum = *(UINT64*)&tmpAttrValue[0x10];
			seqNum = seqNum & MFTREFMASK;
			this->GetExtendMFTAttrValue(seqNum, NTFS_ATTRDEF::ATTR_INDEX_ALLOCATION,tmpExtentMFTValue);
			tmpFileOffset = this->GetOffsetFromAllocByFileName(tmpExtentMFTValue,prmFileName);
			if(tmpFileOffset!=0)
			{
				return tmpFileOffset;
			}
		}
	}
	if(this->GetAttrValue(NTFS_ATTRDEF::ATTR_INDEX_ROOT,tmpBuf,tmpAttrValue))
	{
		tmpFileOffset = this->GetOffsetFromRootByFileName(tmpAttrValue,prmFileName);
		if(tmpFileOffset!=0)
		{
			return tmpFileOffset;
		}
	}
	if(this->GetAttrValue(NTFS_ATTRDEF::ATTR_INDEX_ALLOCATION,tmpBuf,tmpAttrValue))
	{
		tmpFileOffset = this->GetOffsetFromAllocByFileName(tmpAttrValue,prmFileName);
	}
	return tmpFileOffset;
}

UINT64 CNtfsFileSystem::GetOffsetFromRootByFileName(UCHAR *prmAttrValue,const CStringUtil &prmFileName)
{
	if (prmAttrValue[0] != (int)NTFS_ATTRDEF::ATTR_INDEX_ROOT) return 0;
	//0x90属性长度
	UINT32 tmpAttrLen = *(UINT32*)(prmAttrValue+4);
	//第一个索引项偏移值
	UINT32 tmpAttrOffset = *(UINT32*)(prmAttrValue+0x30);
	//找到从0x30到最后一个索引项尾的偏移值
	UINT32 tmpTotalSize = *(UINT32*)(prmAttrValue+0x34);
	//真实索引项大小需要减去从0x30到第一索引的偏移量值
	tmpTotalSize -= tmpAttrOffset;
	tmpAttrOffset += 0x30;
	UINT8	tmpFlags = *(UINT8*)(prmAttrValue+0x3C);
	if(tmpFlags==1)
	{
		//需要外部索引才行。这个就不需要再解析了
		return 0;
	}
	if(tmpAttrOffset>= tmpAttrLen || tmpAttrOffset+tmpTotalSize > tmpAttrLen)
	{
		return 0;
	}
	return this->GetOffsetByFileNameInIndex(prmAttrValue,tmpAttrOffset,tmpTotalSize,prmFileName);
}

UINT64 CNtfsFileSystem::GetOffsetFromAllocByFileName(UCHAR *prmAttrValue,const CStringUtil &prmFileName)
{
	UINT64 tmpFileOffset = 0;
	if(prmAttrValue[0]!= (int)NTFS_ATTRDEF::ATTR_INDEX_ALLOCATION)
	{
		return 0;
	}
	UINT32 tmpAttrLen = *(UINT32*)(prmAttrValue+4);
	UINT16 tmpIndexOffset = *(UINT16*)(prmAttrValue+0x20);
	if(tmpIndexOffset>=tmpAttrLen)
	{
		return 0;
	}
	UINT32 tmpClusterSize = m_sectorsPerCluster * m_bytesPerSector;
	UCHAR *tmpClusterBuf = (UCHAR*)malloc(tmpClusterSize);
	if (tmpClusterBuf == 0) return 0;
	Ntfs_Data_Run *tmpDataRunList = NULL;
	this->GetDataRunList(prmAttrValue,tmpIndexOffset,&tmpDataRunList);
	Ntfs_Data_Run *p = tmpDataRunList;

	while(p!=NULL)
	{
		//每次只处理一个簇的大小
		for(UINT64 i=0;i<p->length;i++)
		{
			this->ReadBuf(tmpClusterBuf,p->lcn*m_sectorsPerCluster + i*this->m_sectorsPerCluster,tmpClusterSize);
			//更新序列号更正
			UINT16 usnOffset = *(UINT16*)&tmpClusterBuf[4];
			for (UINT64 j = 0; j < this->m_sectorsPerCluster; j++)
			{
				memcpy(tmpClusterBuf + 0x1FE + j*this->m_bytesPerSector, tmpClusterBuf + usnOffset + 2 + j * 2, 2);//恢复每个扇区最后两个字节数据
			}
			UINT32	tmpFirstIndex = *(UINT32*)(tmpClusterBuf+0x18);
			tmpFirstIndex += 0x18;
			UINT32	tmpLastIndex = *(UINT32*)(tmpClusterBuf+0x1C);
			tmpFileOffset = this->GetOffsetByFileNameInIndex(tmpClusterBuf,tmpFirstIndex,tmpLastIndex,prmFileName);
			if(tmpFileOffset!=0)
			{
				break;
			}
		}
		if(tmpFileOffset!=0)
		{
			break;
		}
		p = p->next;
	}
	this->FreeRunList(tmpDataRunList);
	::free(tmpClusterBuf);
	return tmpFileOffset;
}

UINT64 CNtfsFileSystem::GetOffsetByFileNameInIndex(UCHAR *prmBuf,UINT16 prmOffset,UINT32 prmBufLen,const CStringUtil &prmFileName)
{
	UINT64 tmpResult = 0;
	wchar_t tmpUnicodeFileName[MAX_PATH] = {0};
	char tmpAnsiName[MAX_PATH*2] = {0};
	CStringUtil	tmpFileName;

	UINT8 tmpNameSpace = 0;
	UINT8 tmpFileNameLen = 0;
	UINT32 tmpAttrOffset = prmOffset;
	UINT64 tmpMFTNum = 0;//mft参考号

	while(tmpAttrOffset<prmBufLen)
	{
		UINT16 tmpIndexItemLen = *(UINT16*)(prmBuf+tmpAttrOffset+8);
		if(prmBuf[tmpAttrOffset+6]==0 && prmBuf[tmpAttrOffset+7]==0)
		{
			tmpAttrOffset += tmpIndexItemLen;
			continue;
		}
		//文件命名空间
		tmpNameSpace = *(UINT8*)(prmBuf+tmpAttrOffset+0x51);
		tmpFileNameLen = *(UINT8*)(prmBuf+tmpAttrOffset + 0x50);
		tmpMFTNum = *(UINT64*)(prmBuf+tmpAttrOffset);
		tmpMFTNum = tmpMFTNum & MFTREFMASK;
		if(tmpMFTNum<=25)
		{
			//mft参考号小于25的都可以不用考虑
			tmpAttrOffset += tmpIndexItemLen;
			continue;
		}
		UINT64 tmpFileOffset = this->GetOffsetByMFTRef(tmpMFTNum);
		//文件没有存放到最终结果数组中，再将新的文件加入到数组中
		if(tmpNameSpace==2)
		{
			tmpFileName = this->GetFileWin32Name(tmpFileOffset);
		}
		else
		{
			memset(tmpUnicodeFileName,0,sizeof(tmpUnicodeFileName));
			memcpy(tmpUnicodeFileName,prmBuf+tmpAttrOffset+0x52,tmpFileNameLen<<1);
#ifdef _UNICODE
			//tmpFileObject->SetFileName(tmpUnicodeFileName);
			tmpFileName = tmpUnicodeFileName;
#else
			memset(tmpAnsiName,0,sizeof(tmpAnsiName));
			::WideCharToMultiByte(CP_ACP,0,tmpUnicodeFileName,-1,tmpAnsiName,MAX_PATH*2,0,0);
			tmpFileName = tmpAnsiName;
#endif
		}
		if(tmpFileName.CompareNoCase(prmFileName)==0)
		{
			tmpResult = tmpFileOffset;
			break;
		}
		tmpAttrOffset += tmpIndexItemLen;
	}
	return tmpResult; 
}

UINT64 CNtfsFileSystem::GetFileSize(UCHAR *prmMFTRecord)
{
	UCHAR szAttrValue[1024] = { 0 };
	UCHAR szAttrList[1024] = {0};
	UCHAR szExtendMFTAttrValue[1024] = {0};
	UINT64 tmpResult = 0;

	//解析0x20属性
	if(this->GetAttrValue(NTFS_ATTRDEF::ATTR_ATTRIBUTE_LIST,prmMFTRecord,szAttrList))
	{
		UINT32 tmpOffset = 0x18;
		while(tmpOffset=this->GetAttrFromAttributeList(NTFS_ATTRDEF::ATTR_DATA,tmpOffset,szAttrList,szAttrValue))
		{
			UINT16 *tmpLen = (UINT16*)(szAttrValue+4);
			UINT64 seqNum = *(UINT64*)(szAttrValue + 0x10);
			if(this->GetExtendMFTAttrValue(seqNum, NTFS_ATTRDEF::ATTR_DATA,szExtendMFTAttrValue))
			{
				UINT64 ts = *(UINT64*)&szExtendMFTAttrValue[0x30];
				if (ts == 0ULL) break;
				if (tmpResult > this->GetMaxFileSize()) {
					break;
				}
				tmpResult += ts;
			}
			else
			{
				break;
			}
		}
	}
	if(tmpResult != 0) return tmpResult;

	if (this->GetAttrValue(NTFS_ATTRDEF::ATTR_DATA, prmMFTRecord, szAttrValue))
	{//找到0x80数据属性
		if (szAttrValue[8] == 0)
		{//常驻属性，其文件大小为 szBuf[attrIndex+0x10]处的四字节
			tmpResult = *(UINT32*)&szAttrValue[0x10];
		}
		else
		{
			tmpResult = *(UINT64*)&szAttrValue[0x30];
		}
		return tmpResult;
	}
	return 0;
}

CStringUtil	CNtfsFileSystem::GetAccessTime(UCHAR szBuf[])
{
	UCHAR szAttrValue[1024] = { 0 };
	if (this->GetAttrValue(NTFS_ATTRDEF::ATTR_STANDARD, szBuf, szAttrValue))
	{
		UINT64 accessTime = *(UINT64*)(szAttrValue + 0x30);
		return this->FileTimeToString(accessTime);
	}
	return TEXT("");
}

CStringUtil CNtfsFileSystem::GetModifyTime(UCHAR szBuf[])
{
	UCHAR szAttrValue[1024] = { 0 };
	if (this->GetAttrValue(NTFS_ATTRDEF::ATTR_STANDARD, szBuf, szAttrValue))
	{
		UINT64 modifyTime = *(UINT64*)(szAttrValue + 0x20);
		return this->FileTimeToString(modifyTime);
	}
	return TEXT("");
}

CStringUtil CNtfsFileSystem::GetCreateTime(UCHAR szBuf[])
{
	UCHAR szAttrValue[1024] = { 0 };
	if (this->GetAttrValue(NTFS_ATTRDEF::ATTR_STANDARD, szBuf, szAttrValue))
	{
		//UINT16 offset = *(UINT16*)(szAttrValue + 0x0A);
		UINT64 createTime = *(UINT64*)(szAttrValue + 0x18);
		return this->FileTimeToString(createTime);
	}
	return TEXT("");
}

CStringUtil	CNtfsFileSystem::FileTimeToString(UINT64 prmFileTime)
{
	FILETIME fileTime = { 0 };
	SYSTEMTIME systemTime = { 0 };
	TIME_ZONE_INFORMATION tz = { 0 };
	fileTime.dwLowDateTime = prmFileTime & 0xFFFFFFFF;
	fileTime.dwHighDateTime = (prmFileTime & 0xFFFFFFFF00000000) >> 32;
	::FileTimeToSystemTime(&fileTime, &systemTime);
	::GetTimeZoneInformation(&tz);
	long lTime = systemTime.wHour * 60 + systemTime.wMinute;
	lTime -= tz.Bias;
	systemTime.wHour = (WORD)lTime / 60;
	systemTime.wMinute = lTime % 60;
	CStringUtil	tmpResult;
	tmpResult.Format(TEXT("%4d-%02d-%02d %02d:%02d:%02d"), systemTime.wYear, systemTime.wMonth, systemTime.wDay,
		systemTime.wHour, systemTime.wMinute, systemTime.wSecond);
	return tmpResult;
}

FILE_OBJECT_TYPE CNtfsFileSystem::GetFileType(UCHAR *prmMFTRecord)
{
	UINT16 flag = *(UINT16*)(prmMFTRecord+0x16);
	switch (flag) {
	case 1:
		return FILE_OBJECT_TYPE::FILE_OBJECT_TYPE_FILE;
	case 2:
		return FILE_OBJECT_TYPE::FILE_OBJECT_TYPE_DIRECTORY;
	default:
		return FILE_OBJECT_TYPE::FILE_OBJECT_TYPE_UNKNOWN;
	}
}
