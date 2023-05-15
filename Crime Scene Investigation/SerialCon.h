#pragma once
#include "SerialBuffer.h"
#include <vector>

typedef enum tagSERIAL_STATE
{
	SS_Unknown,
	SS_UnInit,
	SS_Init,
	SS_Started,
	SS_Stopped,

} SERIAL_STATE;

class SerialCon {
public:
	std::vector<std::wstring> GetAvailablePorts();

	SerialCon();
	~SerialCon();
	HANDLE	GetWaitForEvent() { return m_hDataRx; }

	inline void LockThis() { EnterCriticalSection(&m_csLock); }
	inline void UnLockThis() { LeaveCriticalSection(&m_csLock); }
	inline void InitLock() { InitializeCriticalSection(&m_csLock); }
	inline void DelLock() { DeleteCriticalSection(&m_csLock); }
	inline bool IsInputAvailable()
	{
		LockThis();
		bool abData = (!m_theSerialBuffer.IsEmpty());
		UnLockThis();
		return abData;
	}
	inline bool	IsConnection() { return m_abIsConnected; }
	inline void	SetDataReadEvent() { SetEvent(m_hDataRx); }


	HRESULT Read_N(std::string& data, long alCount, long alTimeOut);
	HRESULT Read_Upto(std::string& data, char chTerminator, long* alCount, long alTimeOut);
	HRESULT ReadAvailable(std::string& data);
	HRESULT Write(const char* data, DWORD dwSize);
	HRESULT Init(std::wstring szPortName = L"COM1", DWORD dwBaudRate = 9600, BYTE byParity = 0, BYTE byStopBits = 1, BYTE byByteSize = 8, BYTE byDtr = 0, BYTE byRts=0, BYTE byXonXof=1);
	HRESULT Start();
	HRESULT Stop();
	HRESULT UnInit();

	static unsigned __stdcall ThreadFn(void* pvParam);
	//-- helper fn.
	HRESULT CanProcess();
private:
	bool m_abIsConnected;
	void InvalidateHandle(HANDLE& hHandle);
	void CloseAndCleanHandle(HANDLE& hHandle);
	SERIAL_STATE GetCurrentState() { return m_eState; }

private:
	SERIAL_STATE m_eState;
	HANDLE m_hCommPort;
	HANDLE m_hThreadTerm;
	HANDLE m_hThread;
	HANDLE m_hThreadStarted;
	HANDLE m_hDataRx;

	SerialBuffer m_theSerialBuffer;
	CRITICAL_SECTION m_csLock;
};