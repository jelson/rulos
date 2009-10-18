#include <windows.h>
#include <stdio.h>

TCHAR *MultiCharToUniChar(char* mbString)
{
	size_t len_out;
	size_t len_in = strlen(mbString) + 1;
	TCHAR *ucString = new wchar_t[len_in];
	mbstowcs_s(&len_out, ucString, len_in, mbString, len_in);
	return ucString;
}

void handleError(TCHAR *prefix)
{
	TCHAR errmsg[1025];
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,0,::GetLastError(),0,errmsg,1024,NULL);

	wprintf(L"\n%s: %s", prefix, errmsg);
	ExitProcess(-1);
}

void writeToClock(HANDLE port, char *s, DWORD len)
{
	while (len > 0)
	{
		DWORD written;

		if (!WriteFile(port,
			s,
			len,
			&written,
			NULL))
		{
			handleError(L"writing to serial port");
		}

		// should never happen but let's be safe
		if (written > len)
		{
			written = len;
		}

		s += written;
		len -= written;
	}
}

HANDLE openSerialPort(char *portName)
{
	HANDLE port;
	DCB dcbConfig;

	port = CreateFile(MultiCharToUniChar(portName),
		GENERIC_READ | GENERIC_WRITE,
		0, 
		NULL, 
        OPEN_EXISTING, 
        0,	
        NULL);

	if (port == INVALID_HANDLE_VALUE)
	{
		handleError(L"Error opening com port");
	}

	if(!GetCommState(port, &dcbConfig))
	{
		handleError(L"Error retrieving serial port parameters");
	}

	dcbConfig.BaudRate = 38400;
	dcbConfig.ByteSize = 8;
	dcbConfig.Parity = NOPARITY;
	dcbConfig.StopBits = ONESTOPBIT;
	dcbConfig.fBinary = TRUE;
	dcbConfig.fParity = TRUE;

	if(!SetCommState(port, &dcbConfig))
	{
		handleError(L"Error setting serial port parameters");
	}

	return port;
}

int main(int argc, char *argv[])
{
	HANDLE port;

	if (argc != 2)
	{
		printf("serial port must be included as command-line argument\n");
		return -1;
	}

	port = openSerialPort(argv[1]);

	while (1)
	{
		SYSTEMTIME time;
		char buf[20];

		// Sleep until 59 seconds
		while (1)
		{
			GetLocalTime(&time);
			if (time.wSecond < 59)
			{
				Sleep(1000 * (59 - time.wSecond));
			}
			else
			{
				break;
			}
		}

		// Sleep until 950 milliseconds
		while (1)
		{
			GetLocalTime(&time);
			if (time.wMilliseconds < 950)
			{
				Sleep(950 - time.wMilliseconds);
			}
			else
			{
				break;
			}
		}

		// now tight loop until the minute rolls over
		while (1)
		{
			GetLocalTime(&time);
			if (time.wSecond == 0)
				break;
		}

		sprintf(buf, "T%02d%02d%02dE", time.wHour, time.wMinute, time.wSecond);
		writeToClock(port, buf, strlen(buf));
	}

}
