#include <cassert>
#include "ChunkFile.h"
#include "exceptions.h"
using namespace std;

ChunkType ChunkReader::nextMini()
{
	assert(m_curDepth >= 0);
	assert(m_size >= 0);

	if (m_miniSize >= 0)
	{
		// We're in a mini chunk, so skip it
		skip();
	}

	if (m_file->tell() == m_offsets[m_curDepth])
	{
		// We're at the end of the current chunk, move up one
		m_curDepth--;
		m_size     = -1;
		m_position =  0;
		return -1;
	}

	MINICHUNKHDR hdr;
	if (m_file->read((void*)&hdr, sizeof(MINICHUNKHDR)) != sizeof(MINICHUNKHDR))
	{
		throw ReadException();
	}

	m_miniSize   = letohl(hdr.size);
	m_miniOffset = m_file->tell() + m_miniSize;
	m_position   = 0;

	return letohl(hdr.type);
}

ChunkType ChunkReader::next()
{
	assert(m_curDepth >= 0);

	if (m_size >= 0)
	{
		// We're in a data chunk, so skip it
		skip();
	}
	
	if (m_file->tell() == m_offsets[m_curDepth])
	{
		// We're at the end of the current chunk, move up one
		m_curDepth--;
		m_size     = -1;
		m_position =  0;
		return -1;
	}

	CHUNKHDR hdr;
	if (m_file->read((void*)&hdr, sizeof(CHUNKHDR)) != sizeof(CHUNKHDR))
	{
		throw ReadException();
	}

	unsigned long size = letohl(hdr.size);
	m_offsets[ ++m_curDepth ] = m_file->tell() + (size & 0x7FFFFFFF);
	m_size     = (~size & 0x80000000) ? size : -1;
	m_miniSize = -1;
	m_position = 0;

	return letohl(hdr.type);
}

void ChunkReader::skip()
{
	if (m_miniSize >= 0)
	{
		m_file->seek(m_miniOffset);
	}
	else
	{
		m_file->seek(m_offsets[m_curDepth--]);
	}
}

long ChunkReader::size()
{
	return (m_miniSize >= 0) ? m_miniSize : m_size;
}

string ChunkReader::readString()
{
	string str;
	char* data = new char[ size() ];
	try
	{
		read(data, size());
	}
	catch (...)
	{
		delete[] data;
		throw;
	}
	str = data;
	delete[] data;
	return str;
}

long ChunkReader::read(void* buffer, long size, bool check)
{
	if (m_size >= 0)
	{
		unsigned long s = m_file->read(buffer, min(m_position + size, this->size()) - m_position);
		m_position += s;
		if (check && s != size)
		{
			throw ReadException();
		}
		return size;
	}
	throw ReadException();
}

ChunkReader::ChunkReader(IFile* file)
{
	file->AddRef();
	m_file       = file;
	m_offsets[0] = m_file->size();
	m_curDepth   = 0;
	m_size       = -1;
	m_miniSize   = -1;
}

ChunkReader::~ChunkReader()
{
	m_file->Release();
}
