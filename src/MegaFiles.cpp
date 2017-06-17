#include <algorithm>
#include <iostream>
#include "MegaFiles.h"
#include "exceptions.h"
#include "crc32.h"
#include "xml.h"
using namespace std;

//
// MegaFile class
//
MegaFile::MegaFile(IFile* file)
{
	this->file = file;
	this->file->AddRef();

	try
	{
		//
		// Read sizes
		//
		uint32_t numStrings;
		uint32_t numFiles;
		if (file->read((void*)&numStrings, sizeof(uint32_t)) != sizeof(uint32_t) ||
			file->read((void*)&numFiles, sizeof(uint32_t)) != sizeof(uint32_t))
		{
			throw ReadException();
		}

		//
		// Read filenames
		//
		for (unsigned long i = 0; i < numStrings; i++)
		{
			uint16_t length;
			if (file->read((void*)&length, sizeof(uint16_t)) != sizeof(uint16_t))
			{
				throw ReadException();
			}

			char* data = new char[length + 1];
			if (file->read(data, length) != length)
			{
				delete[] data;
				throw ReadException();
			}
			data[length] = '\0';
			filenames.push_back(data);
			delete[] data;
		}

		//
		// Read master index table
		//
		unsigned long start = file->tell() + numFiles * sizeof(FileInfo);
		unsigned long totalsize = file->size() - start;

		for (unsigned long i = 0; i < numFiles; i++)
		{
			FileInfo info;
			if (file->read((void*)&info, sizeof(FileInfo)) != sizeof(FileInfo))
			{
				throw ReadException();
			}
			files.push_back(info);
		}
	}
	catch (IOException&)
	{
		throw BadFileException();
	}
}

MegaFile::~MegaFile()
{
	file->Release();
}

IFile* MegaFile::getFile(std::string path) const
{
	try
	{
		transform(path.begin(), path.end(), path.begin(), toupper);
		unsigned long crc = crc32(path.c_str(), path.size());

		// Do a binary search
		int last = (int)files.size() - 1;
		int low = 0, high = last;
		while (high >= low)
		{
			int mid = (low + high) / 2;
			if (files[mid].crc == crc)
			{
				// Found a match; find all adjacent matches
				high = low = mid;
				while (low  > 0 && files[low - 1].crc == crc) low--;
				while (high < last && files[high + 1].crc == crc) high++;
				for (mid = low; mid <= high; mid++)
				{
					if (filenames[files[mid].nameIndex] == path)
					{
						return new SubFile(file, files[mid].start, files[mid].size);
					}
				}
				break;
			}
			if (crc < files[mid].crc) high = mid - 1;
			else                      low = mid + 1;
		}
	}
	catch (IOException&)
	{
		throw BadFileException();
	}
	return NULL;
}

IFile* MegaFile::getFile(int index) const
{
	return new SubFile(file, files[index].start, files[index].size);
}

const string& MegaFile::getFilename(int index) const
{
	return filenames[files[index].nameIndex];
}