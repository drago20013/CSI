#pragma once

#include <string>
#include <windows.h>

class SerialBuffer
{
public:
	SerialBuffer();
	~SerialBuffer();
	inline void LockBuffer() { ::EnterCriticalSection(&m_csLock); }
	inline void UnLockBuffer() { ::LeaveCriticalSection(&m_csLock); }
	//---- public interface --
	void AddData(char ch);
	void AddData(std::string& szData);
	void AddData(std::string& szData, int iLen);
	void AddData(char* strData, int iLen);
	std::string GetData() { return m_szInternalBuffer; }

	void Flush();
	long Read_N(std::string& szData, long alCount, HANDLE& hEventToReset);
	bool Read_Upto(std::string& szData, char chTerm, long& alBytesRead, HANDLE& hEventToReset);
	bool Read_Available(std::string& szData, HANDLE& hEventToReset);
	inline size_t GetSize() { return m_szInternalBuffer.size(); }
	inline bool IsEmpty() { return m_szInternalBuffer.size() == 0; }

private:
	void  Init();
	void ClearAndReset(HANDLE& hEventToReset);

private:
	std::string m_szInternalBuffer;
	CRITICAL_SECTION m_csLock;
	bool m_abLockAlways;
	long m_iCurPos;
	long m_alBytesUnRead;
};