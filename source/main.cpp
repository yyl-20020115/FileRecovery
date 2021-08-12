#include <stdio.h>
#include <iostream>
#include "FileSystemFactory.h"

void RestoreFile(const CStringUtil& target_dir,CBaseFileSystem *prmFileSystem, CBaseFileObject *prmFileObject)
{
	char *tmpBuf = (char*)malloc(1024*1024); //1M
	if (tmpBuf != 0) {
		memset(tmpBuf, 0, 1024 * 1024);
		UINT64	tmpFileSize = prmFileObject->GetFileSize();
		UINT64	tmpBytesRead = 0;
		FILE* fp = NULL;
		CStringUtil path = target_dir;
		path += L'\\';
		path += prmFileObject->GetFileName();
		if (0 == _wfopen_s(&fp, path.GetString(),L"wb") && fp!=0) 
		{
			while (tmpBytesRead < tmpFileSize)
			{
				UINT64 tmpVal = prmFileSystem->ReadFileContent(prmFileObject, (UCHAR*)tmpBuf, tmpBytesRead, 1024 * 1024);
				if (tmpVal == 0)
				{
					break;
				}
				tmpBytesRead += tmpVal;
				fwrite(tmpBuf, 1, tmpVal, fp);
			}
			free(tmpBuf);
			fclose(fp);
		}
	}
}

int wmain(int argc, wchar_t* argv[])
{
	if (argc < 3) {
		std::cout << L"undelete <drive_letter> <target_dir>" << std::endl;
		return -1;
	}

	CBaseFileSystem	*fileSystem = CFileSystemFactory::GetFileSystem(argv[1]);
	if (fileSystem == 0)
	{
		return -1;
	}
	vector<CBaseFileObject *> fileArray;
	fileSystem->GetDeletedFiles(fileArray);
	if (fileArray.size() > 0)
	{
		for (auto& f : fileArray) {
			RestoreFile(argv[2], fileSystem, f);
		}
	}
	delete fileSystem;
	for (int i = 0; i < fileArray.size(); i++)
	{
		fileArray[i]->Destroy();
	}
	return 0;
}