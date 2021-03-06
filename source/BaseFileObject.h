/***************************************************
*
*	@Version:	1.0
*	@Author:	Mengxl
*	@Date:		2017-10-28 22:29
*	@File:		File.h
*
****************************************************/
#ifndef __BASEFILEOBJECT_H__
#define __BASEFILEOBJECT_H__

#include "StringUtil.h"
#include "CommonUtils.h"

enum class FILE_OBJECT_TYPE : int
{
	FILE_OBJECT_TYPE_UNKNOWN = -1,
	FILE_OBJECT_TYPE_FILE = 0,
	FILE_OBJECT_TYPE_DIRECTORY = 1,
	FILE_OBJECT_TYPE_ROOT = 2,
};

class CBaseFileObject
{
public:
	/*************************************
	*
	*	函数名：	CBaseFileObject
	*	函数说明：	
	*	返回值：	
	*
	**************************************/
	CBaseFileObject();
	/*************************************
	*
	*	函数名：	~CBaseFileObject
	*	函数说明：	私有析构函数，不允许在栈上分配对象
					不允许使用delete 删除文件对象
	*	返回值：
	*
	**************************************/
	virtual ~CBaseFileObject();

public:
	/*************************************
	*
	*	函数名：	SetFileStartSector
	*	函数说明：	设置文件的起始扇区
	*	参数描述：	
	*	返回值：	void
	*
	**************************************/
	void SetFileStartSector(UINT64 prmSector);

	/*************************************
	*
	*	函数名：	GetFileStartSector
	*	函数说明：	获取文件的起始扇区
	*	返回值：	UINT64
	*
	**************************************/
	UINT64 GetFileStartSector();

	/*************************************
	*
	*	函数名：	SetFileName
	*	函数说明：	设置文件名称
	*	参数描述：	const CStringUtil & prmFileName
	*	返回值：	void
	*
	**************************************/
	void SetFileName(const CStringUtil &prmFileName);

	/*************************************
	*
	*	函数名：	GetFileName
	*	函数说明：	获取文件名称
	*	返回值：	CStringUtil	&
	*
	**************************************/
	const CStringUtil &GetFileName();

	/*************************************
	*
	*	函数名：	SetFileType
	*	函数说明：	设置文件类型
	*	参数描述：	FILE_OBJECT_TYPE prmType
	*	返回值：	void
	*
	**************************************/
	void SetFileType(FILE_OBJECT_TYPE prmType);

	/*************************************
	*
	*	函数名：	GetFileType
	*	函数说明：	获取文件类型
	*	返回值：	如果该对象是文件返回FILE_OBJECT_TYPE_FILE，
					如果是文件夹返回FILE_OBJECT_TYPE_DIRECTORY
	*
	**************************************/
	FILE_OBJECT_TYPE  GetFileType();

	/*************************************
	*
	*	函数名：	SetFileSize
	*	函数说明：	设置文件的大小，如果是目录则文件大小为0
	*	参数描述：	UINT64 prmFileSize
	*	返回值：	void
	*
	**************************************/
	void SetFileSize(UINT64 prmFileSize);

	/*************************************
	*
	*	函数名：	GetFileSize
	*	函数说明：	获取文件的大小
	*	返回值：	UINT64
	*
	**************************************/
	UINT64 GetFileSize();

	void SetAccessTime(const CStringUtil &accessTime){ m_accessTime = accessTime; }

	CStringUtil	&GetAccessTime(){ return m_accessTime; }

	void SetModifyTime(const CStringUtil &modifyTime){ m_modifyTime = modifyTime; }

	CStringUtil	&GetModifyTime(){ return m_modifyTime; }

	void SetCreateTime(const CStringUtil &createTime){ m_createTime = createTime; }

	CStringUtil &GetCreateTime(){ return m_createTime; }

	void SetFileExtent(File_Content_Extent *fileExtent){ m_fileExtent = fileExtent; }

	File_Content_Extent *GetFileExtent(){ return m_fileExtent; }

private:
	CStringUtil m_fileName;//文件名称，文件绝对路径 = m_path+m_fileName
	CStringUtil m_accessTime;//最后访问时间
	CStringUtil m_modifyTime;//修改时间
	CStringUtil m_createTime;//创建时间
	UINT64 m_startSector;//文件在分区中的起始扇区
	UINT64 m_fileSize;//文件大小
	FILE_OBJECT_TYPE m_objectType;//文件类型：文件，目录，根目录
	File_Content_Extent	*m_fileExtent;//文件内容占用了哪些扇区信息
};

#endif
