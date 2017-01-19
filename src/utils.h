#ifndef UTILS_H
#define UTILS_H

#include <string>

// Returns GetWindowText as std::wstring
std::wstring GetWindowStr(HWND hWnd);
std::wstring GetDlgItemStr(HWND hWnd, int idItem);

// Convert an ANSI string to a wide (UCS-2) string
std::wstring AnsiToWide(const char* cstr);
static std::wstring AnsiToWide(const std::string& str)
{
	return AnsiToWide(str.c_str());
}

// Convert  a wide (UCS-2) string to an an ANSI string
std::string WideToAnsi(const wchar_t* cstr,     const char* defChar = " ");
static std::string WideToAnsi(const std::wstring& str, const char* defChar = " ")
{
	return WideToAnsi(str.c_str(), defChar);
}

float GetRandom(float min, float max);

template <typename T>
struct Buffer
{
	T*     m_data;
	size_t m_size;
	size_t m_capacity;

   	      T& operator[](size_t index)		{ return m_data[index]; }
	const T& operator[](size_t index) const { return m_data[index]; }

	size_t size() const            { return m_size; }
	void   append(size_t numElems) { resize(m_size + numElems); }

	void resize(size_t newSize)
	{
		if (newSize > m_capacity)
		{
			reserve(newSize);
		}
		m_size = newSize;
	}

	void reserve(size_t newCapacity)
	{
		T* tmp = (T*)::realloc(m_data, newCapacity * sizeof(T));
		if (tmp == NULL)
			throw bad_alloc();
		m_data     = tmp;
		m_capacity = newCapacity;
	}

	void clear()
	{
		::free(data);
		m_data     = NULL;
		m_capacity = 0;
		m_size     = 0;
	}

	Buffer()  { m_data = NULL; m_size = 0; m_capacity = 0; }
	~Buffer() { ::free(m_data); }
};

std::wstring FormatString(const wchar_t* format, ...);
std::wstring LoadString(UINT id, ...);

#endif