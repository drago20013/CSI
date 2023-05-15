#include "SerialCon.h"

#include <Process.h>
#include <string>
#include <locale>
#include <codecvt>

void SerialCon::InvalidateHandle(HANDLE& hHandle)
{
	hHandle = INVALID_HANDLE_VALUE;
}

void SerialCon::CloseAndCleanHandle(HANDLE& hHandle)
{

	BOOL abRet = CloseHandle(hHandle);
	if (!abRet)
	{
		return;
	}
	InvalidateHandle(hHandle);

}

std::vector<std::wstring> SerialCon::GetAvailablePorts()
{
	std::vector<std::wstring> ports;
	
	wchar_t portNameC[20];
	for (UINT i = 1; i < 256; i++)
	{
		//Form the Raw device name
		swprintf(portNameC, 20, L"COM%d", i);
		//Try to open the port
		bool bSuccess{ false };
		HANDLE port{ ::CreateFile(portNameC, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr) };
		if (port == INVALID_HANDLE_VALUE)
		{
			const DWORD dwError{ GetLastError() };

			//Check to see if the error was because some other app had the port open or a general failure
			if ((dwError == ERROR_ACCESS_DENIED) || (dwError == ERROR_GEN_FAILURE) || (dwError == ERROR_SHARING_VIOLATION) || (dwError == ERROR_SEM_TIMEOUT))
				bSuccess = true;
		}
		else
		{
			//The port was opened successfully
			bSuccess = true;
		}

		//Add the port number to the array which will be returned
		if (bSuccess) {
			std::wstring portName(portNameC);
			ports.push_back(portName);
		}
		CloseHandle(port);
	}

	return ports;
}

SerialCon::SerialCon()
{

	InvalidateHandle(m_hThreadTerm);
	InvalidateHandle(m_hThread);
	InvalidateHandle(m_hThreadStarted);
	InvalidateHandle(m_hCommPort);
	InvalidateHandle(m_hDataRx);

	InitLock();
	m_eState = SS_UnInit;
}

SerialCon::~SerialCon()
{
	m_eState = SS_Unknown;
	DelLock();
}

HRESULT SerialCon::Init(std::wstring szPortName, DWORD dwBaudRate, BYTE byParity, BYTE byStopBits, BYTE byByteSize, BYTE byDtr, BYTE byRts, BYTE byXonXof)
{
	HRESULT hr = S_OK;
	try
	{
		m_hDataRx = CreateEvent(0, 0, 0, 0);

		//open the COM Port
		m_hCommPort = ::CreateFile(szPortName.c_str(),
			GENERIC_READ | GENERIC_WRITE,//access ( read and write)
			0,	//(share) 0:cannot share the COM port						
			0,	//security  (None)				
			OPEN_EXISTING,// creation : open_existing
			FILE_FLAG_OVERLAPPED,// we want overlapped operation
			0// no templates file for COM port...
		);
		if (m_hCommPort == INVALID_HANDLE_VALUE)
		{
			return E_FAIL;
		}



		//now start to read but first we need to set the COM port settings and the timeouts
		if (!::SetCommMask(m_hCommPort, EV_RXCHAR | EV_TXEMPTY))
		{
			return E_FAIL;
		}

		//now we need to set baud rate etc,
		DCB dcb = { 0 };

		dcb.DCBlength = sizeof(DCB);

		if (!::GetCommState(m_hCommPort, &dcb))
		{
			return E_FAIL;
		}

		dcb.BaudRate = dwBaudRate;
		dcb.ByteSize = byByteSize;
		dcb.Parity = byParity;
		if (byStopBits == 1)
			dcb.StopBits = ONESTOPBIT;
		else if (byStopBits == 2)
			dcb.StopBits = TWOSTOPBITS;
		else
			dcb.StopBits = ONE5STOPBITS;
		//======================

		dcb.fOutX = 0;
		dcb.fInX = 0;

		dcb.fOutxDsrFlow = 0;
		dcb.fOutxCtsFlow = 0;

		dcb.fRtsControl = RTS_CONTROL_DISABLE;
		dcb.fDtrControl = DTR_CONTROL_DISABLE;

		if (byDtr == 1) {
			dcb.fOutxDsrFlow = 1;
			dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
			dcb.fRtsControl = RTS_CONTROL_DISABLE;
		}

		if (byRts == 1) {
			dcb.fOutxCtsFlow = 1;
			dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
			dcb.fDtrControl = DTR_CONTROL_DISABLE;
		}

		if (byXonXof == 1) {
			dcb.fOutX = 1;
			dcb.fInX = 1;
		}

		//======================

		if (!::SetCommState(m_hCommPort, &dcb))
		{
			return E_FAIL;
		}

		//now set the timeouts ( we control the timeout overselves using WaitForXXX()
		COMMTIMEOUTS timeouts;

		timeouts.ReadIntervalTimeout = MAXDWORD;
		timeouts.ReadTotalTimeoutMultiplier = 0;
		timeouts.ReadTotalTimeoutConstant = 0;
		timeouts.WriteTotalTimeoutMultiplier = 0;
		timeouts.WriteTotalTimeoutConstant = 0;

		if (!SetCommTimeouts(m_hCommPort, &timeouts))
		{
			return E_FAIL;
		}
		//create thread terminator event...
		m_hThreadTerm = CreateEvent(0, 0, 0, 0);
		m_hThreadStarted = CreateEvent(0, 0, 0, 0);

		m_hThread = (HANDLE)_beginthreadex(0, 0, SerialCon::ThreadFn, (void*)this, 0, 0);

		DWORD dwWait = WaitForSingleObject(m_hThreadStarted, INFINITE);

		CloseHandle(m_hThreadStarted);

		InvalidateHandle(m_hThreadStarted);
		m_abIsConnected = true;

	}
	catch (...)
	{
		hr = E_FAIL;
	}
	if (SUCCEEDED(hr))
	{
		m_eState = SS_Init;
	}
	return hr;

}

HRESULT SerialCon::Start()
{
	m_eState = SS_Started;
	return S_OK;
}

HRESULT SerialCon::Stop()
{
	m_eState = SS_Stopped;
	return S_OK;
}

HRESULT SerialCon::UnInit()
{
	HRESULT hr = S_OK;
	try
	{
		m_abIsConnected = false;
		SignalObjectAndWait(m_hThreadTerm, m_hThread, INFINITE, FALSE);
		CloseAndCleanHandle(m_hThreadTerm);
		CloseAndCleanHandle(m_hThread);
		CloseAndCleanHandle(m_hDataRx);
		CloseAndCleanHandle(m_hCommPort);
	}
	catch (...)
	{
		hr = E_FAIL;
	}
	if (SUCCEEDED(hr))
		m_eState = SS_UnInit;
	return hr;
}

unsigned __stdcall SerialCon::ThreadFn(void* pvParam)
{
	SerialCon* apThis = (SerialCon*)pvParam;
	bool abContinue = true;
	DWORD dwEventMask = 0;

	OVERLAPPED ov;
	memset(&ov, 0, sizeof(ov));
	ov.hEvent = CreateEvent(0, true, 0, 0);
	HANDLE arHandles[2];
	arHandles[0] = apThis->m_hThreadTerm;

	DWORD dwWait;
	SetEvent(apThis->m_hThreadStarted);
	while (abContinue)
	{

		BOOL abRet = ::WaitCommEvent(apThis->m_hCommPort, &dwEventMask, &ov);

		arHandles[1] = ov.hEvent;

		dwWait = WaitForMultipleObjects(2, arHandles, FALSE, INFINITE);
		switch (dwWait)
		{
		case WAIT_OBJECT_0:
		{
			_endthreadex(1);
		}
		break;
		case WAIT_OBJECT_0 + 1:
		{
			if (dwEventMask & EV_TXEMPTY) {
				ResetEvent(ov.hEvent);
				continue;
			}
			else if (dwEventMask & EV_RXCHAR) {
				//read data here...
				int iAccum = 0;

				apThis->m_theSerialBuffer.LockBuffer();

				try
				{
					BOOL abRet = false;

					DWORD dwBytesRead = 0;
					OVERLAPPED ovRead;
					memset(&ovRead, 0, sizeof(ovRead));
					ovRead.hEvent = CreateEvent(0, true, 0, 0);
					char szTmp[10] = "\0"; 
					abRet = 1;
					do
					{
						memset(szTmp, 0, sizeof szTmp);
						abRet = ::ReadFile(apThis->m_hCommPort, szTmp, sizeof(szTmp), &dwBytesRead, &ovRead);
						if (abRet && dwBytesRead > 0)
						{
							apThis->m_theSerialBuffer.AddData(szTmp, dwBytesRead);
							iAccum += dwBytesRead;
						}
						else if (abRet == FALSE) {
							DWORD errorCode = GetLastError();
							if (errorCode == ERROR_IO_PENDING) {
								// The operation is pending, wait for it to complete
								DWORD dwWait = WaitForSingleObject(ovRead.hEvent, INFINITE);
								if (dwWait == WAIT_OBJECT_0) {
									// Operation completed successfully
									// Retrieve the result and handle it
									abRet = GetOverlappedResult(apThis->m_hCommPort, &ovRead, &dwBytesRead, TRUE);
									if (abRet == TRUE) {
										// Read operation completed successfully
										// Process the data in szTmp and dwBytesRead
										apThis->m_theSerialBuffer.AddData(szTmp, dwBytesRead);
										iAccum += dwBytesRead;
									}
									else {
										// GetOverlappedResult failed, check GetLastError for the specific error
										DWORD errorCode = GetLastError();
										// Handle the error
									}
								}
								else {
									// WaitForSingleObject failed, check GetLastError for the specific error
									DWORD errorCode = GetLastError();
									// Handle the error
								}
							}
							else {
								// Handle other error codes
								// errorCode contains the specific error code returned by ReadFile
							}
							/*DWORD errorCode = GetLastError();
							abRet = 0;*/
						}
						ResetEvent(ovRead.hEvent);
					} while (dwBytesRead > 0);
					CloseHandle(ovRead.hEvent);
				}
				catch (...)
				{
				}

				//if we are not in started state then we should flush the queue...( we would still read the data)
				if (apThis->GetCurrentState() != SS_Started)
				{
					iAccum = 0;
					apThis->m_theSerialBuffer.Flush();
				}

				apThis->m_theSerialBuffer.UnLockBuffer();

				if (iAccum > 0)
				{
					apThis->SetDataReadEvent();
				}
				ResetEvent(ov.hEvent);
			}
		}
		break;
		}//switch
	}
	return 0;
}


HRESULT  SerialCon::CanProcess()
{

	switch (m_eState)
	{
	case SS_Unknown:
		return E_FAIL;
	case SS_UnInit:
		return E_FAIL;
	case SS_Started:
		return S_OK;
	case SS_Init:
	case SS_Stopped:
		return E_FAIL;
	}
	return E_FAIL;
}

HRESULT SerialCon::Write(const char* data, DWORD dwSize)
{
	HRESULT hr = CanProcess();
	if (FAILED(hr)) return hr;
	int iRet = 0;
	OVERLAPPED ov;
	memset(&ov, 0, sizeof(ov));
	ov.hEvent = CreateEvent(0, true, 0, 0);
	DWORD dwBytesWritten = 0;
	//do
	{
		iRet = WriteFile(m_hCommPort, data, dwSize, &dwBytesWritten, &ov);
		if (iRet == 0)
		{
			WaitForSingleObject(ov.hEvent, INFINITE);
		}

	}//while ( ov.InternalHigh != dwSize ) ;
	CloseHandle(ov.hEvent);

	return S_OK;
}

HRESULT SerialCon::Read_Upto(std::string& data, char chTerminator, long* alCount, long alTimeOut)
{
	HRESULT hr = CanProcess();
	if (FAILED(hr)) return hr;

	try
	{

		std::string szTmp;
		szTmp.erase();
		long alBytesRead;

		bool abFound = m_theSerialBuffer.Read_Upto(szTmp, chTerminator, alBytesRead, m_hDataRx);

		if (abFound)
		{
			data = szTmp;
		}
		else
		{//there are either none or less bytes...
			long iRead = 0;
			bool abContinue = true;
			while (abContinue)
			{
				DWORD dwWait = ::WaitForSingleObject(m_hDataRx, alTimeOut);

				if (dwWait == WAIT_TIMEOUT)
				{
					data.erase();
					hr = E_FAIL;
					return hr;

				}

				bool abFound = m_theSerialBuffer.Read_Upto(szTmp, chTerminator, alBytesRead, m_hDataRx);


				if (abFound)
				{
					data = szTmp;
					return S_OK;
				}

			}
		}
	}
	catch (...)
	{
		printf("SerialCon Unhandled exception");
	}
	return hr;

}
HRESULT SerialCon::Read_N(std::string& data, long alCount, long  alTimeOut)
{
	HRESULT hr = CanProcess();

	if (FAILED(hr))
	{
		return hr;
	}

	try
	{

		std::string szTmp;
		szTmp.erase();


		int iLocal = m_theSerialBuffer.Read_N(szTmp, alCount, m_hDataRx);

		if (iLocal == alCount)
		{
			data = szTmp;
		}
		else
		{//there are either none or less bytes...
			long iRead = 0;
			int iRemaining = alCount - iLocal;
			while (1)
			{


				DWORD dwWait = WaitForSingleObject(m_hDataRx, alTimeOut);

				if (dwWait == WAIT_TIMEOUT)
				{
					data.erase();
					hr = E_FAIL;
					return hr;

				}


				iRead = m_theSerialBuffer.Read_N(szTmp, iRemaining, m_hDataRx);
				iRemaining -= iRead;


				if (iRemaining == 0)
				{
					data = szTmp;
					return S_OK;
				}
			}
		}
	}
	catch (...)
	{
		printf("SerialCon Unhandled exception");

	}
	return hr;

}
/*-----------------------------------------------------------------------
	-- Reads all the data that is available in the local buffer..
	does NOT make any blocking calls in case the local buffer is empty
-----------------------------------------------------------------------*/
HRESULT SerialCon::ReadAvailable(std::string& data)
{

	HRESULT hr = CanProcess();
	if (FAILED(hr)) return hr;
	try
	{
		std::string szTemp;
		bool abRet = m_theSerialBuffer.Read_Available(szTemp, m_hDataRx);

		data = szTemp;
	}
	catch (...)
	{
		printf("SerialCon Unhandled exception in ReadAvailable()");
		hr = E_FAIL;
	}
	return hr;
}