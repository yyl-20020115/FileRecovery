#include "BaseFileObject.h"
#include "CommonUtils.h"

CBaseFileObject::CBaseFileObject()
	: m_fileExtent()
	, m_fileName()
	, m_accessTime()
	, m_modifyTime()
	, m_createTime()
	, m_startSector()
	, m_fileSize()
	, m_objectType()
{
}

CBaseFileObject::~CBaseFileObject()
{
	if (this->m_fileExtent != 0) {
		File_Content_Extent* p = m_fileExtent;
		File_Content_Extent* q = p;
		while (p != NULL)
		{
			p = p->next;
			delete q;
			q = p;
		}
		this->m_fileExtent = 0;
	}
}

void CBaseFileObject::SetFileName(const CStringUtil &prmFileName)
{
	m_fileName = prmFileName;
}

const CStringUtil &CBaseFileObject::GetFileName()
{
	return m_fileName;
}

void CBaseFileObject::SetFileStartSector(UINT64 prmSector)
{
	m_startSector = prmSector;
}

UINT64 CBaseFileObject::GetFileStartSector()
{
	return m_startSector;
}

void CBaseFileObject::SetFileType(FILE_OBJECT_TYPE	prmObjectType)
{
	m_objectType = prmObjectType;
}

FILE_OBJECT_TYPE CBaseFileObject::GetFileType()
{
	return m_objectType;
}

void CBaseFileObject::SetFileSize(UINT64 prmFileSize)
{
	m_fileSize = prmFileSize;
}

UINT64 CBaseFileObject::GetFileSize()
{
	return m_fileSize;
}

void CBaseFileObject::Destroy()
{
	delete this;
}
