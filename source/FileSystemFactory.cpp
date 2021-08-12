#include "FileSystemFactory.h"
#include "NtfsFileSystem.h"
#include "Fat32FileSystem.h"

//ntfs文件格式标识
UCHAR const CBaseFileSystem::NTFS_Signature[] = { 0x4e, 0x54, 0x46, 0x53 };
//fat32文件格式标识
UCHAR const CBaseFileSystem::FAT32_Signature[] = { 0x46, 0x41, 0x54, 0x33, 0x32 };


CBaseFileSystem	*CFileSystemFactory::GetFileSystem(const TCHAR *prmDisk)
{
	//读取分区第一个扇区的数据，用于判断分区的文件系统类型
	UCHAR SectorBuffer[512] = { 0 };

	IBaseReader	*tmpReader = new CSectorReader();
	if (!tmpReader->OpenDevice(prmDisk))
	{
		delete tmpReader;
		return 0;
	}
	if (tmpReader->ReadSector(0, 512, SectorBuffer) != 512)
	{
		delete tmpReader;
		return 0;
	}
	CBaseFileSystem	*fileSystem = 0;
	if (memcmp(&SectorBuffer[3], CBaseFileSystem::NTFS_Signature, sizeof(CBaseFileSystem::NTFS_Signature)) == 0)
	{
		fileSystem = new CNtfsFileSystem(tmpReader);
		fileSystem->SetStartSector(0);
		UINT64 *tmpTotalSectors = (UINT64*)(SectorBuffer + 0x28);
		fileSystem->SetTotalSector(*tmpTotalSectors);
		fileSystem->Init();
	}
	else if (memcmp(&SectorBuffer[0x52], CBaseFileSystem::FAT32_Signature, sizeof(CBaseFileSystem::FAT32_Signature)) == 0)
	{
		fileSystem = new CFat32FileSystem(tmpReader);
		fileSystem->SetStartSector(0);
		UINT32 *tmpTotalSectors = (UINT32*)(SectorBuffer + 0x20);
		fileSystem->SetTotalSector(*tmpTotalSectors);
		fileSystem->Init();
	}
	return fileSystem;
}
