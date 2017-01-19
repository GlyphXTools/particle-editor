#include <cassert>
#include "ChunkFile.h"
#include "exceptions.h"
using namespace std;

void ChunkWriter::beginChunk(ChunkType type)
{
	m_curDepth++;
	m_chunks[m_curDepth].offset   = m_file->tell();
	m_chunks[m_curDepth].hdr.type = type;
	m_chunks[m_curDepth].hdr.size = 0;
	if (m_curDepth > 0)
	{
		// Set 'container' bit in parent chunk
		m_chunks[m_curDepth-1].hdr.size |= 0x80000000;
	}

	// Write dummy header
	CHUNKHDR hdr = {0,0};
	m_file->write(&hdr, sizeof(CHUNKHDR));
}

void ChunkWriter::beginMiniChunk(ChunkType type)
{
	assert(m_curDepth >= 0);
	assert(m_miniChunk.offset == -1);
	assert(type <= 0xFF);

	m_miniChunk.offset   = m_file->tell();
	m_miniChunk.hdr.type = (uint8_t)type;
	m_miniChunk.hdr.size = 0;
	
	// Write dummy header
	MINICHUNKHDR hdr = {0, 0};
	m_file->write(&hdr, sizeof(MINICHUNKHDR));
}

void ChunkWriter::endChunk()
{
	assert(m_curDepth >= 0);
	if (m_miniChunk.offset != -1)
	{
		// Ending mini-chunk
		long pos  = m_file->tell();
		long size = pos - (m_miniChunk.offset + sizeof(MINICHUNKHDR));
		assert(size <= 0xFF);

		m_miniChunk.hdr.size = (uint8_t)size;
		m_file->seek(m_miniChunk.offset);
		m_file->write(&m_miniChunk.hdr, sizeof(MINICHUNKHDR) );
		m_file->seek(pos);
		m_miniChunk.offset = -1;
	}
	else
	{
		// Ending normal chunk
		long pos  = m_file->tell();
		long size = pos - (m_chunks[m_curDepth].offset + sizeof(CHUNKHDR));

		m_chunks[m_curDepth].hdr.size = (m_chunks[m_curDepth].hdr.size & 0x80000000) | (size & ~0x80000000);
		m_file->seek(m_chunks[m_curDepth].offset);
		m_file->write(&m_chunks[m_curDepth].hdr, sizeof(CHUNKHDR));
		m_file->seek(pos);

		m_curDepth--;
	}
}

void ChunkWriter::write(const void* buffer, long size)
{
	assert(m_curDepth >= 0);
	if (m_file->write(buffer, size) != size)
	{
		throw WriteException();
	}
}

void ChunkWriter::writeString(const std::string& str)
{
	write(str.c_str(), (int)str.length() + 1);
}

ChunkWriter::ChunkWriter(IFile* file)
{
	file->AddRef();
	m_file     = file;
	m_curDepth = -1;
	m_miniChunk.offset = -1;
}

ChunkWriter::~ChunkWriter()
{
	m_file->Release();
}

