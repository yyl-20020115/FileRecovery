#include "BaseFileSystem.h"
#include "CommonUtils.h"

CBaseFileSystem::CBaseFileSystem(IBaseReader *prmReader)
	: m_reader(prmReader)
	, m_bytesPerSector()
	, m_sectorsPerCluster()
	, m_startSector()
	, m_totalSector()
	, m_maxFileSize(1024 * 1024 * 1024)

{

}

CBaseFileSystem::~CBaseFileSystem()
{
	if (this->m_reader != 0) {
		delete this->m_reader;
		this->m_reader = 0;
	}
}

UINT64 CBaseFileSystem::ReadBuf(UCHAR prmBuf[],UINT64 prmStartSector,UINT64 prmByteToRead)
{
	return this->m_reader != 0 
		? this->m_reader->ReadSector(prmStartSector+m_startSector, prmByteToRead, prmBuf)
		: 0
		;
}
