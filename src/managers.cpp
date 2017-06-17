#include <algorithm>
#include <iostream>
#include "managers.h"
#include "exceptions.h"
#include "crc32.h"
#include "xml.h"
#include "utils.h"
using namespace std;

//
// FileManager class
//
IFile* FileManager::getFile(const string& path)
{
	// First see if we can open it physically
	for (vector<wstring>::const_iterator base = basepaths.begin(); base != basepaths.end(); base++)
	{
		try
		{
			wstring wpath = AnsiToWide(path);
			wstring filename = (path[1] != ':' && path[0] != '\\') ? *base + wpath : wpath;
			return new PhysicalFile(filename);
		}
		catch (IOException&)
		{
		}
	}

	// Search in the index
	try
	{
		for (vector<MegaFile*>::iterator i = megafiles.begin(); i != megafiles.end(); i++)
		{
			IFile* file = (*i)->getFile( path );
			if (file != NULL)
			{
				return file;
			}
		}
	}
	catch (IOException&)
	{
	}

	return NULL;
}

FileManager::FileManager(const vector<wstring>& basepaths)
{
	XMLTree xml;
	this->basepaths = basepaths;
	for (vector<wstring>::const_iterator path = basepaths.begin(); path != basepaths.end(); path++)
	{
		try
		{
			PhysicalFile* file = new PhysicalFile( *path + L"Data\\MegaFiles.xml" );
			xml.parse( file );
			file->Release();

			const XMLNode* root = xml.getRoot();
			if (root->getName() != L"Mega_Files")
			{
				throw BadFileException();
			}

			// Create a file index from all mega files
			for (unsigned int i = 0; i < root->getNumChildren(); i++)
			{
				const XMLNode* child = root->getChild(i);
				if (child->getName() != L"File")
				{
					throw BadFileException();
				}
		
				wstring filename = *path + child->getData();
				try
				{
					megafiles.push_back(new MegaFile(new PhysicalFile(filename)));
				}
				catch (IOException)
				{
				}
			}
		}
		catch (FileNotFoundException)
		{
			continue;
		}
		catch (...)
		{
			for (vector<MegaFile*>::iterator i = megafiles.begin(); i != megafiles.end(); i++)
			{
				delete *i;
			}
			throw;
		}
	}

	if (megafiles.empty())
	{
		throw FileNotFoundException(L"MegaFiles.xml");
	}
}

FileManager::~FileManager()
{
	for (vector<MegaFile*>::iterator i = megafiles.begin(); i != megafiles.end(); i++)
	{
		delete (*i);
	}
}

