#include "SerialBuffer.h"
#include "SerialCon.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SerialBuffer::SerialBuffer()
{
	Init();
}
void SerialBuffer::Init()
{
	::InitializeCriticalSection(&m_csLock);
	m_abLockAlways = true;
	m_iCurPos = 0;
	m_alBytesUnRead = 0;
	m_szInternalBuffer.erase();

}
SerialBuffer::~SerialBuffer()
{
	::DeleteCriticalSection(&m_csLock);

}
void SerialBuffer::AddData(char ch)
{
	m_szInternalBuffer += ch;
	m_alBytesUnRead += 1;

}

void SerialBuffer::AddData(std::string& szData, int iLen)
{
	m_szInternalBuffer.append(szData.c_str(), iLen);
	m_alBytesUnRead += iLen;

}

void SerialBuffer::AddData(char* strData, int iLen)
{
	m_szInternalBuffer.append(strData, iLen);
	m_alBytesUnRead += iLen;

}

void SerialBuffer::AddData(std::string& szData)
{
	m_szInternalBuffer += szData;
	m_alBytesUnRead += szData.size();

}
void SerialBuffer::Flush()
{
	LockBuffer();
	m_szInternalBuffer.erase();
	m_alBytesUnRead = 0;
	m_iCurPos = 0;
	UnLockBuffer();
}

long SerialBuffer::Read_N(std::string& szData, long  alCount, HANDLE& hEventToReset)
{
	LockBuffer();
	long alTempCount = min(alCount, m_alBytesUnRead);
	long alActualSize = GetSize();

	szData.append(m_szInternalBuffer, m_iCurPos, alTempCount);

	m_iCurPos += alTempCount;

	m_alBytesUnRead -= alTempCount;
	if (m_alBytesUnRead == 0)
	{
		ClearAndReset(hEventToReset);
	}

	UnLockBuffer();
	return alTempCount;
}

bool SerialBuffer::Read_Available(std::string& szData, HANDLE& hEventToReset)
{
	LockBuffer();
	szData += m_szInternalBuffer;

	ClearAndReset(hEventToReset);

	UnLockBuffer();

	return (szData.size() > 0);
}


void SerialBuffer::ClearAndReset(HANDLE& hEventToReset)
{
	m_szInternalBuffer.erase();
	m_alBytesUnRead = 0;
	m_iCurPos = 0;
	::ResetEvent(hEventToReset);

}

bool SerialBuffer::Read_Upto(std::string& szData, char chTerm, long& alBytesRead, HANDLE& hEventToReset)
{

	LockBuffer();
	alBytesRead = 0;


	bool abFound = false;
	if (m_alBytesUnRead > 0)
	{//if there are some bytes un-read...

		int iActualSize = GetSize();
		int iIncrementPos = 0;
		for (int i = m_iCurPos; i < iActualSize; ++i)
		{
			//szData .append ( m_szInternalBuffer,i,1);
			szData += m_szInternalBuffer[i];
			m_alBytesUnRead -= 1;
			if (m_szInternalBuffer[i] == chTerm)
			{
				iIncrementPos++;
				abFound = true;
				break;
			}
			iIncrementPos++;
		}
		m_iCurPos += iIncrementPos;
		if (m_alBytesUnRead == 0)
		{
			ClearAndReset(hEventToReset);
		}
	}
	UnLockBuffer();
	return abFound;
}