#include "SectorReader.h"
#include "StringUtil.h"

CSectorReader::CSectorReader()
	: m_diskHandle(INVALID_HANDLE_VALUE)
{
}

CSectorReader::~CSectorReader()
{
	if(this->m_diskHandle != INVALID_HANDLE_VALUE)
	{
		::CloseHandle(m_diskHandle);
		this->m_diskHandle = INVALID_HANDLE_VALUE;
	}
}

bool CSectorReader::OpenDevice(const TCHAR *prmDevice)
{
	CommonUtils::CStringUtil tmpStr;
	tmpStr.Format(TEXT("\\\\.\\%c:"), prmDevice[0]);
	m_diskHandle = ::CreateFile(tmpStr.GetString(), GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_WRITE | FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	return (m_diskHandle != INVALID_HANDLE_VALUE);
}

UINT64 CSectorReader::ReadSector(UINT64 prmStartSector, UINT64 prmBytesToRead, UCHAR *prmBuf)
{
	if (m_diskHandle == INVALID_HANDLE_VALUE) return 0;

	UINT32 tmpResult = 0;
	//如果请求的数据字节数不是512字节，先读取512字节的整数倍
	UINT32 tmpRemainBytes = prmBytesToRead % 512;
	UINT64 tmpBytesToRead = prmBytesToRead - tmpRemainBytes;
	LARGE_INTEGER tmpInt = { 0 };
	tmpInt.QuadPart = prmStartSector * 512;
	::SetFilePointerEx(m_diskHandle, tmpInt, NULL, FILE_BEGIN);
	DWORD tmpBytesRead = 0;
	BOOL ret = ::ReadFile(m_diskHandle, prmBuf, (DWORD)tmpBytesToRead, &tmpBytesRead, NULL);
	if (ret) {
		tmpResult = tmpBytesRead;
		//读取剩余不满足512字节的数据
		if (tmpRemainBytes != 0)
		{
			UCHAR tmpBuf[512] = { 0 };
			ret = ::ReadFile(m_diskHandle, tmpBuf, 512, &tmpBytesRead, NULL);
			if (ret) {
				memcpy(prmBuf + tmpBytesToRead, tmpBuf, tmpRemainBytes);
				tmpResult += tmpRemainBytes;
			}
		}
	}
	return tmpResult;
}
