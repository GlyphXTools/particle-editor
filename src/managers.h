#ifndef MANAGERS_H
#define MANAGERS_H

#include <string>
#include <map>
#include <vector>
#include "Effect.h"
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

//
// Manager interfaces
//
class IFileManager
{
public:
	virtual IFile* getFile(const std::string& path) = 0;
};

class ITextureManager
{
public:
	virtual IDirect3DTexture9* getTexture(IDirect3DDevice9* pDevice, std::string name) = 0;
};

class IShaderManager
{
public:
	virtual Effect* getShader(IDirect3DDevice9* pDevice, std::string name) = 0;
};

//
// File Manager
//
class FileManager : public IFileManager
{
	std::vector<std::wstring> basepaths;
	std::vector<MegaFile*>    megafiles;

public:
	IFile* getFile(const std::string& path);
	FileManager(const std::vector<std::wstring>& basepaths);
	~FileManager();
};

#endif