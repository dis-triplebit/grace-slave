/*
 * LineHashIndex.cpp
 *
 *  Created on: 2019å¹´05æœˆ19æ—¥
 *      Author: favoniankong
 */

#include "LineHashIndex.h"
#include "MMapBuffer.h"
#include "MemoryBuffer.h"
#include "BitmapBuffer.h"

#include <math.h>

/**
 * linear fit;
 * f(x)=kx + b;
 * used to calculate the parameter k and b;
 */
static bool calculateLineKB(vector<LineHashIndex::Point>& a, double& k, double& b, int pointNo)
{
	if (pointNo < 2)
		return false;

	double mX, mY, mXX, mXY;
	mX = mY = mXX = mXY = 0;
	int i;
	for (i = 0; i < pointNo; i++)
	{
		mX += a[i].x;
		mY += a[i].y;
		mXX += a[i].x * a[i].x;
		mXY += a[i].x * a[i].y;
	}

	if (mX * mX - mXX * pointNo == 0)
		return false;

	k = (mY * mX - mXY * pointNo) / (mX * mX - mXX * pointNo);
	b = (mXY * mX - mY * mXX) / (mX * mX - mXX * pointNo);
	return true;
}

LineHashIndex::LineHashIndex(ChunkManager& _chunkManager, IndexType index_type, XYType xy_type) :
		chunkManager(_chunkManager), indexType(index_type), xyType(xy_type)
{
	// TODO Auto-generated constructor stub
	idTable = NULL;
	idTableEntries = NULL;
	lineHashIndexBase = NULL;

	startID[0] = startID[1] = startID[2] = startID[3] = UINT_MAX;
}

LineHashIndex::~LineHashIndex()
{
	// TODO Auto-generated destructor stub
	idTable = NULL;
	idTableEntries = NULL;
	startPtr = NULL;
	endPtr = NULL;
	chunkMeta.clear();
	swap(chunkMeta, chunkMeta);
}

/**
 * From startEntry to endEntry in idtableEntries build a line;
 * @param lineNo: the lineNo-th line to be build;
 */
bool LineHashIndex::buildLine(int startEntry, int endEntry, int lineNo)
{
	vector<Point> vpt;
	Point pt;
	int i;

	//build lower limit line;
	for (i = startEntry; i < endEntry; i++)
	{
		pt.x = idTableEntries[i];
		pt.y = i;
		vpt.push_back(pt);
	}

	double ktemp, btemp;
	int size = vpt.size();
	if (calculateLineKB(vpt, ktemp, btemp, size) == false)
		return false;

	double difference = btemp; //(vpt[0].y - (ktemp * vpt[0].x + btemp));
	double difference_final = difference;

	for (i = 1; i < size; i++)
	{
		difference = vpt[i].y - ktemp * vpt[i].x; //vpt[0].y - (ktemp * vpt[0].x + btemp);
		//cout<<"differnce: "<<difference<<endl;
		if ((difference < difference_final) == true)
			difference_final = difference;
	}
	btemp = difference_final;

	lowerk[lineNo] = ktemp;
	lowerb[lineNo] = btemp;
	startID[lineNo] = vpt[0].x;

	vpt.resize(0);
	//build upper limit line;
	for (i = startEntry; i < endEntry; i++)
	{
		pt.x = idTableEntries[i + 1];
		pt.y = i;
		vpt.push_back(pt);
	}

	size = vpt.size();
	calculateLineKB(vpt, ktemp, btemp, size);

	difference = btemp;		//(vpt[0].y - (ktemp * vpt[0].x + btemp));
	difference_final = difference;

	for (i = 1; i < size; i++)
	{
		difference = vpt[i].y - ktemp * vpt[i].x; //vpt[0].y - (ktemp * vpt[0].x + btemp);
		if (difference > difference_final)
			difference_final = difference;
	}
	btemp = difference_final;

	upperk[lineNo] = ktemp;
	upperb[lineNo] = btemp;
	return true;
}

static ID splitID[3] =
{ 255, 65535, 16777215 };

Status LineHashIndex::buildIndex(unsigned chunkType) //½¨Á¢Ë÷Òý chunkType: 1: x>y ; 2: x<y
{
	if (idTable == NULL)
	{
		idTable = new MemoryBuffer(HASH_CAPACITY);
		idTableEntries = (ID*) idTable->getBuffer();
		tableSize = 0;
	}

	const uchar* begin, *limit, *reader;
	ID x, y;

	int lineNo = 0;
	int startEntry = 0, endEntry = 0;

	if (chunkType == 1)
	{
		reader = chunkManager.getStartPtr(1);
		limit = chunkManager.getEndPtr(1);
		begin = reader;
		if (begin == limit){
			return OK;
		}

		MetaData* metaData = (MetaData*) reader;
		x = metaData->minID;
		insertEntries(x);

		reader = reader + (int) (MemoryBuffer::pagesize - sizeof(ChunkManagerMeta));

		while (reader < limit)
		{
			metaData = (MetaData*) reader;
			x = metaData->minID;
			insertEntries(x);

			if (x > splitID[lineNo])
			{
				startEntry = endEntry;
				endEntry = tableSize;
				if (buildLine(startEntry, endEntry, lineNo) == true)
				{
					++lineNo;
				}
			}
			reader = reader + (int) MemoryBuffer::pagesize;
		}

		reader = Chunk::skipBackward(limit, begin, chunkType);
		x = 0;
		Chunk::readXId(reader, x);
		insertEntries(x);

		startEntry = endEntry;
		endEntry = tableSize;
		if (buildLine(startEntry, endEntry, lineNo) == true)
		{
			++lineNo;
		}
	}

	if (chunkType == 2)
	{
		reader = chunkManager.getStartPtr(2);
		limit = chunkManager.getEndPtr(2);
		begin = reader;
		if (begin == limit){
			return OK;
		}

		while (reader < limit)
		{
			MetaData* metaData = (MetaData*) reader;
			x = metaData->minID;
			insertEntries(x);

			if (x > splitID[lineNo])
			{
				startEntry = endEntry;
				endEntry = tableSize;
				if (buildLine(startEntry, endEntry, lineNo) == true)
				{
					++lineNo;
				}
			}
			reader = reader + (int) MemoryBuffer::pagesize;
		}
		x = y = 0;
		reader = Chunk::skipBackward(limit, begin, chunkType);
		Chunk::readYId(Chunk::readXId(reader, x), y);
		insertEntries(x + y);

		startEntry = endEntry;
		endEntry = tableSize;
		if (buildLine(startEntry, endEntry, lineNo) == true)
		{
			++lineNo;
		}
	}
	return OK;
}

bool LineHashIndex::isBufferFull()
{
	return tableSize >= idTable->getSize() / 4;
}

void LineHashIndex::insertEntries(ID id)
{
	if (isBufferFull())
	{
		idTable->resize(HASH_CAPACITY);
		idTableEntries = (ID*) idTable->get_address();
	}
	idTableEntries[tableSize] = id;

	tableSize++;
}

ID LineHashIndex::MetaID(size_t index)
{
	assert(index < chunkMeta.size());
	return chunkMeta[index].minIDx;
}

ID LineHashIndex::MetaYID(size_t index)
{
	assert(index < chunkMeta.size());
	return chunkMeta[index].minIDy;
}

size_t LineHashIndex::searchChunkFrank(ID id)
{
	size_t low = 0, mid = 0, high = tableSize - 1;

	if (low == high){
		return low;
	}
	while (low < high)
	{
		mid = low + (high-low) / 2;
		while (MetaID(mid) == id)
		{
			if (mid > 0 && MetaID(mid - 1) < id){
				return mid - 1;
			}
			if (mid == 0){
				return mid;
			}
			mid--;
		}
		if (MetaID(mid) < id){
			low = mid + 1;
		}
		else if (MetaID(mid) > id){
			high = mid;
		}
	}
	if (low > 0 && MetaID(low) >= id){
		return low - 1;
	}
	else{
		return low;
	}
}

size_t LineHashIndex::searchChunk(ID xID, ID yID){
	if(MetaID(0) > xID || tableSize == 0){
		return 0;
	}

	size_t offsetID = searchChunkFrank(xID);
	if(offsetID == tableSize-1){
		return offsetID-1;
	}
	while(offsetID < tableSize-2){
		if(MetaID(offsetID+1) == xID){
			if(MetaYID(offsetID+1) > yID){
				return offsetID;
			}
			else{
				offsetID++;
			}
		}
		else{
			return offsetID;
		}
	}
	return offsetID;
}

bool LineHashIndex::searchChunk(ID xID, ID yID, size_t& offsetID)
//return the  exactly which chunk the triple(xID, yID) is in
{
	if(MetaID(0) > xID || tableSize == 0){
		offsetID = 0;
		return false;
	}

	offsetID = searchChunkFrank(xID);
	if (offsetID == tableSize-1)
	{
		return false;
	}

	while (offsetID < tableSize - 2)
	{
		if (MetaID(offsetID + 1) == xID)
		{
			if (MetaYID(offsetID + 1) > yID)
			{
				return true;
			}
			else
			{
				offsetID++;
			}
		}
		else
		{
			return true;
		}
	}
	return true;
}

bool LineHashIndex::isQualify(size_t offsetId, ID xID, ID yID)
{
	return (xID < MetaID(offsetId + 1) || (xID == MetaID(offsetId + 1) && yID < MetaYID(offsetId + 1))) && (xID > MetaID(offsetId) || (xID == MetaID(offsetId) && yID >= MetaYID(offsetId)));
}

void LineHashIndex::getOffsetPair(size_t offsetID, unsigned& offsetBegin, unsigned& offsetEnd)
//get the offset of the data begin and end of the offsetIDth Chunk to the startPtr
{
	if(tableSize == 0){
		offsetEnd = offsetBegin = sizeof(MetaData);
	}
	offsetBegin = chunkMeta[offsetID].offsetBegin;
	MetaData* metaData = (MetaData*) (startPtr + offsetBegin - sizeof(MetaData));
	offsetEnd = offsetBegin - sizeof(MetaData) + metaData->usedSpace;
}

size_t LineHashIndex::save(MMapBuffer*& indexBuffer)
//tablesize , (startID, lowerk , lowerb, upperk, upperb) * 4
{
	char* writeBuf;
	size_t offset;

	if (indexBuffer == NULL)
	{
		indexBuffer = MMapBuffer::create(string(string(DATABASE_PATH) + "/BitmapBuffer_index").c_str(),
				sizeof(ID) + 16 * sizeof(double) + 4 * sizeof(ID));
		writeBuf = indexBuffer->get_address();
		offset = 0;
	}
	else
	{
		size_t size = indexBuffer->get_length();
		indexBuffer->resize(size + sizeof(ID) + 16 * sizeof(double) + 4 * sizeof(ID), false);
		writeBuf = indexBuffer->get_address() + size;
		offset = size;
	}

	*(ID*) writeBuf = tableSize;
	writeBuf += sizeof(ID);

	for (int i = 0; i < 4; ++i)
	{
		*(ID*) writeBuf = startID[i];
		writeBuf = writeBuf + sizeof(ID);

		*(double*) writeBuf = lowerk[i];
		writeBuf = writeBuf + sizeof(double);
		*(double*) writeBuf = lowerb[i];
		writeBuf = writeBuf + sizeof(double);

		*(double*) writeBuf = upperk[i];
		writeBuf = writeBuf + sizeof(double);
		*(double*) writeBuf = upperb[i];
		writeBuf = writeBuf + sizeof(double);
	}

	indexBuffer->flush();
	delete idTable;
	idTable = NULL;

	return offset;
}

void LineHashIndex::updateLineIndex()
{
	char* base = lineHashIndexBase;

	*(ID*) base = tableSize;
	base += sizeof(ID);

	for (int i = 0; i < 4; ++i)
	{
		*(ID*) base = startID[i];
		base += sizeof(ID);

		*(double*) base = lowerk[i];
		base += sizeof(double);
		*(double*) base = lowerb[i];
		base += sizeof(double);

		*(double*) base = upperk[i];
		base += sizeof(double);
		*(double*) base = upperb[i];
		base += sizeof(double);
	}

	delete idTable;
	idTable = NULL;
}

void LineHashIndex::updateChunkMetaData(int offsetId)
{
	if (offsetId == 0)
	{
		const uchar* reader = NULL;
		register ID x = 0, y = 0;

		reader = startPtr + chunkMeta[offsetId].offsetBegin;
		reader = Chunk::readYId(Chunk::readXId(reader, x), y);
		if (xyType == LineHashIndex::YBIGTHANX)
		{
			chunkMeta[offsetId].minIDx = x;
			chunkMeta[offsetId].minIDy = x + y;
		}
		else if (xyType == LineHashIndex::XBIGTHANY)
		{
			chunkMeta[offsetId].minIDx = x + y;
			chunkMeta[offsetId].minIDy = x;
		}
	}
}

LineHashIndex* LineHashIndex::load(ChunkManager& manager, IndexType index_type, XYType xy_type, char*buffer,
		size_t& offset)
{
	LineHashIndex* index = new LineHashIndex(manager, index_type, xy_type);
	char* base = buffer + offset;
	index->lineHashIndexBase = base;

	index->tableSize = *((ID*) base);
	base = base + sizeof(ID);

	for (int i = 0; i < 4; ++i)
	{
		index->startID[i] = *(ID*) base;
		base = base + sizeof(ID);

		index->lowerk[i] = *(double*) base;
		base = base + sizeof(double);
		index->lowerb[i] = *(double*) base;
		base = base + sizeof(double);

		index->upperk[i] = *(double*) base;
		base = base + sizeof(double);
		index->upperb[i] = *(double*) base;
		base = base + sizeof(double);
	}
	offset = offset + sizeof(ID) + 16 * sizeof(double) + 4 * sizeof(ID);

	//get something useful for the index
	const uchar* reader;
	const uchar* temp;
	register ID x, y;
	if (index->xyType == LineHashIndex::YBIGTHANX)
	{
		index->startPtr = index->chunkManager.getStartPtr(1);
		index->endPtr = index->chunkManager.getEndPtr(1);
		if (index->startPtr == index->endPtr)
		{
			index->chunkMeta.push_back(
			chunkMetaData{ 0, 0, sizeof(MetaData) });
			return index;
		}

		temp = index->startPtr + sizeof(MetaData);
		Chunk::readYId(Chunk::readXId(temp, x), y);
		index->chunkMeta.push_back(
		chunkMetaData{ x, x + y, sizeof(MetaData) });

		reader = index->startPtr - sizeof(ChunkManagerMeta) + MemoryBuffer::pagesize;
		while (reader < index->endPtr)
		{
			temp = reader + sizeof(MetaData);
			Chunk::readYId(Chunk::readXId(temp, x), y);
			index->chunkMeta.push_back(
			chunkMetaData{ x, x + y, unsigned(reader - index->startPtr + sizeof(MetaData)) });

			reader = reader + MemoryBuffer::pagesize;
		}
		reader = index->endPtr;
		reader = Chunk::skipBackward(reader, index->startPtr, 1);
		Chunk::readYId(Chunk::readXId(reader, x), y);
		index->chunkMeta.push_back(chunkMetaData
		{ x, x + y });
	}
	else if (index->xyType == LineHashIndex::XBIGTHANY)
	{
		index->startPtr = index->chunkManager.getStartPtr(2);
		index->endPtr = index->chunkManager.getEndPtr(2);
		if (index->startPtr == index->endPtr)
		{
			index->chunkMeta.push_back(
			chunkMetaData{ 0, 0, sizeof(MetaData) });
			return index;
		}

		temp = index->startPtr + sizeof(MetaData);
		Chunk::readYId(Chunk::readXId(temp, x), y);
		index->chunkMeta.push_back(
		chunkMetaData{ x + y, x, sizeof(MetaData) });

		reader = index->startPtr + MemoryBuffer::pagesize;
		while (reader < index->endPtr)
		{
			temp = reader + sizeof(MetaData);
			Chunk::readYId(Chunk::readXId(temp, x), y);
			index->chunkMeta.push_back(
			chunkMetaData{ x + y, x, unsigned(reader - index->startPtr + sizeof(MetaData)) });

			reader = reader + MemoryBuffer::pagesize;
		}

		reader = index->endPtr;
		reader = Chunk::skipBackward(reader, index->startPtr, 2);
		Chunk::readYId(Chunk::readXId(reader, x), y);
		index->chunkMeta.push_back(
		chunkMetaData{ x + y, x });
	}
	return index;
}

