#include <stdio.h>
#include <iostream>
#include <fstream>
#include "FileSystemFactory.h"
#include "json11.hpp"

void SaveInfo(const CStringUtil& path,const vector<CBaseFileObject*>& fileArray) {
	if (fileArray.size() > 0)
	{
		json11::Json::array files;
		for (auto& f : fileArray) {
			json11::Json::array sectors;
			auto fe = f->GetFileExtent();
			while (fe != 0) {
				auto fx = json11::Json::object{
					{"persist",fe->isPersist},
					{"start",fe->startSector},
					{"total",fe->totalSector},
				};
				sectors.push_back(fx);
				fe = fe->next;
			}
			json11::Json::object file{
				{"name", f->GetFileName().GetString()},
				{"type",(int)f->GetFileType()},
				{"size", f->GetFileSize()},
				{"start",f->GetFileStartSector()},
				{"create", f->GetAccessTime().GetString()},
				{"access", f->GetAccessTime().GetString()},
				{"modify", f->GetModifyTime().GetString()},
				{"sectors", sectors}
			};
			files.push_back(file);

		}
	  	json11::Json fr = json11::Json::object{
			{ "fcs", files }
		};

		std::string result;
		fr.dump(result);

		std::ofstream ofs(path.GetString());
		ofs << result;
	}
}
void LoadInfo(const CStringUtil& path, vector<CBaseFileObject*>& fileArray) {
	
	std::string result;
	std::string errs;

	std::ifstream ifs(path.GetString());
	ifs >> result;

	auto fcs =json11::Json::parse(result, errs);
	auto files = fcs.object_items().at("fcs");
	for (auto& file : files.array_items()) {
		auto cfo = new CBaseFileObject();
		auto fn = file.object_items().at("name");
		auto ft = file.object_items().at("type");
		auto fs = file.object_items().at("size");
		auto fr = file.object_items().at("start");
		
		if (fn.is_string()) cfo->SetFileName(fn.string_value().c_str());
		if (ft.is_number()) cfo->SetFileType((FILE_OBJECT_TYPE)ft.int_value());
		if (fs.is_number()) cfo->SetFileSize(fs.int_value());
		if (fr.is_number()) cfo->SetFileStartSector(fr.int_value());

		auto sectors = file.object_items().at("sectors");
		for (auto& s : sectors.array_items())
		{
			auto fce = new File_Content_Extent();
			auto fp = s.object_items().at("persist");
			auto fs = s.object_items().at("start");
			auto ft = s.object_items().at("total");
			if (fp.is_number()) fce->isPersist = fp.int_value();
			if (fs.is_number()) fce->startSector = fs.ulong_value();
			if (ft.is_number()) fce->totalSector = fp.ulong_value();
			fce->next = cfo->GetFileExtent();
			cfo->SetFileExtent(fce);
		}
	}
}
void RestoreFile(const CStringUtil& target_dir,CBaseFileSystem *prmFileSystem, CBaseFileObject *prmFileObject)
{
	char *tmpBuf = (char*)malloc(1024*1024); //1M
	if (tmpBuf != 0) {
		memset(tmpBuf, 0, 1024 * 1024);
		UINT64	tmpFileSize = prmFileObject->GetFileSize();
		UINT64	tmpBytesRead = 0;
		FILE* fp = NULL;
		CStringUtil path = target_dir;
		path += '\\';
		path += prmFileObject->GetFileName();
		if (0 == fopen_s(&fp, path.GetString(),"wb") && fp!=0) 
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
void RestoreFiles(const CStringUtil& target_dir, CBaseFileSystem* fileSystem, const vector<CBaseFileObject*>& fileArray) {
	if (fileArray.size() > 0)
	{
		for (auto& f : fileArray) {
			RestoreFile(target_dir, fileSystem, f);
		}
	}
}
int main(int argc, char* argv[])
{
	if (argc < 3) {
		std::cout << "undelete <drive_letter> <target_dir>" << std::endl;
		return -1;
	}

	CBaseFileSystem	*fileSystem = CFileSystemFactory::GetFileSystem(argv[1]);
	if (fileSystem != 0)
	{
		vector<CBaseFileObject*> fileArray;

		fileSystem->GetDeletedFiles(fileArray);

		RestoreFiles(argv[2], fileSystem, fileArray);

		delete fileSystem;

		for (int i = 0; i < fileArray.size(); i++) fileArray[i]->Destroy();
	}
	return 0;
}