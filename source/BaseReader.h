#ifndef __BASEREADER_H__
#define __BASEREADER_H__

#include <windows.h>

class IBaseReader
{
public:
	virtual	~IBaseReader() {}

	/*************************************
	*
	*	��������	OpenDevice
	*	����˵����	��һ����������̣������ṩ��ȡ������Դ
	*	����������	@param prmDevice[�������]��ʾ�豸�ı�ʶ��
	*	����ֵ��	�򿪳ɹ�����true�����򷵻�false
	*
	**************************************/
	virtual	bool OpenDevice(const TCHAR* prmDevice) = 0;

	/*************************************
	*
	*	��������	ReadSector
	*	����˵����	��ȡ��������е�����
	*	����������	@param prmStartSector[�������]��������еĴ���ȡ��ʼ������
	*	����������	@param prmBytesToRead[�������]����ȡ���ֽ�����
	*	����������	@param prmBuf[�������]���ɹ���ȡ������д�뵽�û�������
	*	����ֵ��	���سɹ�д�뵽prmBuf�е��ֽ���
	*
	**************************************/
	virtual	UINT64	ReadSector(UINT64 prmStartSector, UINT64 prmBytesToRead, UCHAR* prmBuf) = 0;
};

#endif