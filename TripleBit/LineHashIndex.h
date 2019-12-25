/*
 * LineHashIndex.h
 *
 *  Created on: 2019年05月19日
 *      Author: favoniankong
 */

#ifndef LINEHASHINDEX_H_
#define LINEHASHINDEX_H_

class MemoryBuffer;
class ChunkManager;
class MMapBuffer;

#include "TripleBit.h"

class LineHashIndex {
public:
	struct Point{
		ID x;
		ID y;
	};

	struct chunkMetaData
	//except the firstChunk , minIDx, minIDy and offsetBegin will not change with update
	//offsetEnd may change but I think it makes little difference to the result
	//by Frankfan
	{
		ID minIDx;    //The minIDx of a chunk
		ID minIDy;		//The minIDy of a chunk
		unsigned offsetBegin;	//The beginoffset of a chunk(not include MetaData and relative to the startPtr)
	};

	enum IndexType { SUBJECT_INDEX, OBJECT_INDEX};
	enum XYType {XBIGTHANY, YBIGTHANX};
private:
	MemoryBuffer* idTable;
	ID* idTableEntries;
	ChunkManager& chunkManager;
	IndexType indexType;
	XYType xyType;
	size_t tableSize;   //chunk number plus 1,because the end edge
	char* lineHashIndexBase; //used to do update

	//line parameters;
	double upperk[4];
	double upperb[4];
	double lowerk[4];
	double lowerb[4];

	ID startID[4];

public:
	//some useful thing about the chunkManager
	uchar *startPtr, *endPtr;
	vector<chunkMetaData> chunkMeta;

private:
	void insertEntries(ID id);
	size_t searchChunkFrank(ID id);
	bool buildLine(int startEntry, int endEntry, int lineNo);
	ID MetaID(size_t index);
	ID MetaYID(size_t index);
public:
	LineHashIndex(ChunkManager& _chunkManager, IndexType index_type, XYType xy_type);
	Status buildIndex(unsigned chunkType);
	void getOffsetPair(size_t offsetID, unsigned& offsetBegin, unsigned& offsetEnd);
	size_t searchChunk(ID xID, ID yID);
	bool searchChunk(ID xID, ID yID, size_t& offsetID);
	bool isQualify(size_t offsetId, ID xID, ID yID);
	unsigned int getTableSize() { return tableSize; }
	size_t save(MMapBuffer*& indexBuffer);
	void saveDelta(MMapBuffer*& indexBuffer, size_t& offset ,const size_t predicateSize);
	virtual ~LineHashIndex();
	void updateChunkMetaData(int offsetId);
	void updateLineIndex();
private:
	bool isBufferFull();
public:
	static LineHashIndex* load(ChunkManager& manager, IndexType index_type, XYType xy_type, char* buffer, size_t& offset);
};

#endif /* LINEHASHINDEX_H_ */
