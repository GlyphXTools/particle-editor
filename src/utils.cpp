#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "utils.h"
using namespace std;

wstring GetWindowStr(HWND hWnd)
{
	int len = GetWindowTextLength(hWnd);
	wchar_t* buf = new wchar_t[len+1]; buf[len] = L'\0';
	GetWindowText(hWnd, buf, len + 1);
	wstring value = buf;
	delete[] buf;
	return value;
}

wstring GetDlgItemStr(HWND hWnd, int idItem)
{
	return GetWindowStr(GetDlgItem(hWnd, idItem));
}

// Convert an ANSI string to a wide (UCS-2) string
wstring AnsiToWide(const char* cstr)
{
	int size = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, cstr, -1, NULL, 0);
	WCHAR* wstr = new WCHAR[size];
	try
	{
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, cstr, -1, wstr, size);
		wstring result(wstr);
		delete[] wstr;
		return result;
	}
	catch (...)
	{
		delete[] wstr;
		throw;
	}
}

// Convert  a wide (UCS-2) string to an an ANSI string
string WideToAnsi(const wchar_t* cstr, const char* defChar)
{
	int size = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_NO_BEST_FIT_CHARS | WC_DEFAULTCHAR, cstr, -1, NULL, 0, defChar, NULL);
	CHAR* str = new CHAR[size];
	try
	{
		WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK | WC_NO_BEST_FIT_CHARS | WC_DEFAULTCHAR, cstr, -1, str, size, defChar, NULL);
		string result(str);
		delete[] str;
		return result;
	}
	catch (...)
	{
		delete[] str;
		throw;
	}
}

float GetRandom(float min, float max)
{
	return (max - min) / RAND_MAX * rand() + min;
}

static wstring FormatString(const wchar_t* format, va_list args)
{
    int      n   = _vscwprintf(format, args);
    wchar_t* buf = new wchar_t[n + 1];
    try
    {
        vswprintf(buf, n + 1, format, args);
        wstring str = buf;
        delete[] buf;
        return str;
    }
    catch (...)
    {
        delete[] buf;
        throw;
    }
}

wstring FormatString(const wchar_t* format, ...)
{
    va_list args;
    va_start(args, format);
    wstring str = FormatString(format, args);
    va_end(args);
    return str;
}

wstring LoadString(UINT id, ...)
{
    int len = 256;
    TCHAR* buf = new TCHAR[len];
    try
    {
        while (::LoadString(NULL, id, buf, len) >= len - 1)
        {
            delete[] buf;
            buf = NULL;
            buf = new TCHAR[len *= 2];
        }
        va_list args;
        va_start(args, id);
        wstring str = FormatString(buf, args);
        va_end(args);
        delete[] buf;
        return str;
    }
    catch (...)
    {
        delete[] buf;
        throw;
    }
}