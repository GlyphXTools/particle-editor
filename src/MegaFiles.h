#ifndef MEGAFILES_H
#define MEGAFILES_H

#include <vector>
#include "types.h"
#include "files.h"

class MegaFile
{
	struct FileInfo
	{
		uint32_t crc;
		uint32_t index;
		uint32_t size;
		uint32_t start;
		uint32_t nameIndex;
	};

	IFile* file;
	std::vector<FileInfo>      files;
	std::vector<std::string>   filenames;

public:
	IFile*             getFile(std::string path) const;
	IFile*             getFile(int index) const;
	const std::string& getFilename(int index) const;
	unsigned int       getNumFiles() const { return (unsigned int)files.size(); }

	MegaFile(IFile* file);
	~MegaFile();
};

#endif