/***************************************************
*
*	@Version:	1.0
*	@Author:	Mengxl
*	@Date:		2018-5-20 22:43
*	@File:		FileSystemFactory.h
*	获取文件系统(fat32/ntfs)对象的工厂类
*
****************************************************/
#ifndef __FILESYSTEMFACTORY_H__
#define __FILESYSTEMFACTORY_H__
#include "BaseFileSystem.h"

class CFileSystemFactory
{
public:
	static CBaseFileSystem	*GetFileSystem(const TCHAR *prmDisk);
};

#endif
