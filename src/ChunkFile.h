#ifndef CHUNKFILE_H
#define CHUNKFILE_H

#include <string>

#include "files.h"
#include "types.h"

typedef long ChunkType;

#pragma pack(1)
struct CHUNKHDR
{
	uint32_t type;
	uint32_t size;
};

struct MINICHUNKHDR
{
	uint8_t type;
	uint8_t size;
};
#pragma pack()

class ChunkReader
{
	static const int MAX_CHUNK_DEPTH = 256;

	IFile* m_file;
	long   m_position;
	long   m_size;
	long   m_offsets[ MAX_CHUNK_DEPTH ];
	long   m_miniSize;
	long   m_miniOffset;
	int    m_curDepth;

public:
	ChunkType   next();
	ChunkType   nextMini();
	void        skip();
	long        size();
	long        read(void* buffer, long size, bool check = true);
	std::string readString();

	ChunkReader(IFile* file);
	~ChunkReader();
};

class ChunkWriter
{
	static const int MAX_CHUNK_DEPTH = 256;
	
	template <typename HDRTYPE>
	struct ChunkInfo
	{
		HDRTYPE       hdr;
		unsigned long offset;
	};

	IFile*                  m_file;
	ChunkInfo<CHUNKHDR>     m_chunks[ MAX_CHUNK_DEPTH ];
	ChunkInfo<MINICHUNKHDR> m_miniChunk;
	int                     m_curDepth;

public:
	void beginChunk(ChunkType type);
	void beginMiniChunk(ChunkType type);
	void endChunk();
	void write(const void* buffer, long size);
	void writeString(const std::string& str);

	ChunkWriter(IFile* file);
	~ChunkWriter();
};
#endif