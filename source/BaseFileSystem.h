/***************************************************
*
*	@Version:	1.0
*	@Author:	Mengxl
*	@Date:		2017-10-28 22:17
*	@File:		FileSystem.h
*
****************************************************/


#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__

#include <windows.h>
#include <vector>
#include "BaseFileObject.h"
#include "SectorReader.h"
#include "CommonUtils.h"

using namespace std;

class CBaseFileSystem
{
public:
	static const UCHAR NTFS_Signature[];
	static const UCHAR FAT32_Signature[];

public:
	/*************************************
	*
	*	函数名：	CBaseFileSystem
	*	函数说明：	构造函数
	*	参数描述：	prmReader[输入参数]用于表示文件系统读取数据的来源
	*	返回值：	
	*
	**************************************/
	CBaseFileSystem(IBaseReader	*prmReader = 0);

	/*************************************
	*
	*	函数名：	~CBaseFileSystem
	*	函数说明：	析构函数
	*	返回值：	
	*
	**************************************/
	virtual ~CBaseFileSystem();
	
	/*************************************
	*
	*	函数名：	ReadBuf
	*	函数说明：	读取从偏移startSector扇区开始读取byteToRead字节数据
	*	参数描述：	@param prmBuf[in/out]将读取的byteToRead字节数据存放到szBuf中
	*	参数描述：	@param prmStartSector[in]读取的数据在文件中的偏移值
	*	参数描述：	@param prmByteToRead[in]从文件中读取byteToRead字节
	*	返回值：	返回成功写入到prmBuf缓冲区中的字节数
	*
	**************************************/
	virtual	UINT64 ReadBuf(UCHAR prmBuf[],UINT64 prmStartSector,UINT64 prmByteToRead);

	/*************************************
	*
	*	函数名：	Init
	*	函数说明：	文件分区的初始化，如fat32系统载入
	*				分区引导记录信息，具体视不同的文件系统
	*				实现不同。如果文件系统不需要进行初始化，
	*				该函数的实现体可以为空。
	*	返回值：	void
	*
	**************************************/
	virtual	void Init() = 0;

	/*************************************
	*
	*	函数名：	ReadFileContent
	*	函数说明：	读取文件内容
	*	参数描述：	@param prmFileObject[输入参数]待读取文件的对象
	*	参数描述：	@param prmDstBuf[输出参数]读取的内容存入到该缓冲区中
	*	参数描述：	@param prmByteOff[输入参数]表示从文件的多少节字偏移处开始读取
	*	参数描述：	@param prmByteToRead[输入参数]表示读取多少字节
	*	返回值：	返回成功写入到prmDstBuf中的字节数
	*
	**************************************/
	virtual UINT64 ReadFileContent(CBaseFileObject *prmFileObject, UCHAR prmDstBuf[], UINT64 prmByteOff, UINT64 prmByteToRead) = 0;

	/***
	*
	* 获取ntfs文件系统中已补删除的文件
	* @param fileArray。用于存放被删除文件信息
	*
	***/
	virtual void GetDeletedFiles(vector<CBaseFileObject*> &fileArray) = 0;

	/*************************************
	*
	*	函数名：	SetBytesPerSector
	*	函数说明：	设置每扇区字节数
	*	参数描述：	UINT16 prmBytesPerSector
	*	返回值：	void
	*
	**************************************/
	void SetBytesPerSector(UINT16 prmBytesPerSector){m_bytesPerSector = prmBytesPerSector;}

	/*************************************
	*
	*	函数名：	SetSectorsPerCluster
	*	函数说明：	设置每簇扇区数
	*	参数描述：	UINT8 prmSectorsPerCluster
	*	返回值：	void
	*
	**************************************/
	void SetSectorsPerCluster(UINT8 prmSectorsPerCluster){m_sectorsPerCluster = prmSectorsPerCluster;}

	/*************************************
	*
	*	函数名：	SetStartSector
	*	函数说明：	设置当前文件分区在整个磁盘中的起始扇区
	*	参数描述：	UINT64 prmStartSector
	*	返回值：	void
	*
	**************************************/
	void SetStartSector(UINT64 prmStartSector){m_startSector = prmStartSector;}

	/*************************************
	*
	*	函数名：	SetTotalSector
	*	函数说明：	设置整个分区总扇区数
	*	参数描述：	UINT64 prmTotalSector
	*	返回值：	void
	*
	**************************************/
	void SetTotalSector(UINT64 prmTotalSector){m_totalSector = prmTotalSector;}

	UINT64 GetMaxFileSize() { return this->m_maxFileSize; }
	void SetMaxFileSize(UINT64 maxFileSize) { this->m_maxFileSize = maxFileSize; }
protected:
	UINT64 m_maxFileSize;

	UINT16 m_bytesPerSector;//每扇区字节数
	UINT8 m_sectorsPerCluster;//每簇扇区数
	UINT64 m_startSector;//该分区在磁盘中的起始扇区
	UINT64 m_totalSector;//该分区总共占用的扇区数
	IBaseReader	*m_reader;//读取分区数据的对象
};

#endif
