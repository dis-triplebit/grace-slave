/*
 * ChunkManager.h
 *
 *  Created on: 2019年7月30日 15:26:49
 *      Author: favoniankong
 */

#ifndef CHUNKMANAGER_H_
#define CHUNKMANAGER_H_

class MemoryBuffer;
class MMapBuffer;
class Chunk;
class ChunkManager;

#include "TripleBit.h"
#include "HashIndex.h"
#include "LineHashIndex.h"
#include "ThreadPool.h"

///////////////////////////////////////////////////////////////////////////////////////////////
///// class BitmapBuffer
///////////////////////////////////////////////////////////////////////////////////////////////
class BitmapBuffer {
public:
	map<ID, ChunkManager*> predicate_managers[2];
	ID startColID;
	const string dir;
	MMapBuffer *temp1, *temp2, *temp3, *temp4;
	size_t usedPage1, usedPage2, usedPage3, usedPage4;
public:
	BitmapBuffer(const string dir);
	BitmapBuffer() : startColID(0), dir("") {}
	~BitmapBuffer();

    //获取所有的谓词ID
    void getAllPredicateID(std::vector<ID>& allPredicateID);

	/// insert a predicate given specified sorting type and predicate id 
	Status insertPredicate(ID predicateID, unsigned char typeID);
	Status insertTriple(ID, ID, unsigned char, ID, unsigned char,bool);
	/// insert a triple;
	Status insertTriple(ID predicateID, ID xID, ID yID, bool isBigger, unsigned char typeID);
	/// get the chunk manager (i.e. the predicate) given the specified type and predicate id
	ChunkManager* getChunkManager(ID, unsigned char);
	/// get the count of chunk manager (i.e. the predicate count) given the specified type
	size_t getSize(unsigned char type) { return predicate_managers[type].size(); }
	void insert(ID predicateID, ID subjectID, ID objectID, bool isBigger, unsigned char flag);

	size_t getTripleCount();

	void flush();
	ID getColCnt() { return startColID; }

	char* getPage(unsigned char type, unsigned char flag, size_t& pageNo);
	void save();
	void endUpdate(MMapBuffer *bitmapPredicateImage, MMapBuffer *bitmapOld);
//	static BitmapBuffer* load(const string bitmapBufferDir, MMapBuffer*& bitmapIndexImage, MMapBuffer* bitmapPredicateImage);
	static BitmapBuffer* load(MMapBuffer* bitmapImage, MMapBuffer*& bitmapIndexImage, MMapBuffer* bitmapPredicateImage);
private:
	/// generate the x and y;
	void generateXY(ID& subjectID, ID& objectID);
	/// get the bytes of a id;
	unsigned char getBytes(ID id);
	/// get the storage space (in bytes) of a id;
	unsigned char getLen(ID id);
};

/////////////////////////////////////////////////////////////////////////////////////////////
///////// class ChunkManager
/////////////////////////////////////////////////////////////////////////////////////////////
struct ChunkManagerMeta
{
	size_t length[2];
	size_t usedSpace[2];
	int tripleCount[2];
	unsigned type;
	unsigned pid;
	char* startPtr[2];
	char* endPtr[2];
};

struct MetaData
{
	ID minID;
	size_t usedSpace;
	bool haveNextPage;
	size_t NextPageNo;
};

class ChunkManager {
private:
	char* ptrs[2];

	ChunkManagerMeta* meta;
	///the No. of buffer
	static unsigned int bufferCount;

	///hash index; index the subject and object
	LineHashIndex* chunkIndex[2];

	BitmapBuffer* bitmapBuffer;
	vector<size_t> usedPage[2];
public:
	friend class BuildSortTask;
	friend class BuildMergeTask;
	friend class HashIndex;
	friend class BitmapBuffer;
	friend class PartitionMaster;

public:
	ChunkManager(){}
	ChunkManager(unsigned pid, unsigned _type, BitmapBuffer* bitmapBuffer);
	~ChunkManager();
	Status resize(unsigned char type);
	Status tripleCountAdd(unsigned char type) {
		meta->tripleCount[type - 1]++;
		return OK;
	}

	LineHashIndex* getChunkIndex(int type) {
		if(type > 2 || type < 1) {
			return NULL;
		}
		return chunkIndex[type - 1];
	}

	bool isPtrFull(unsigned char type, unsigned len);

	int getTripleCount() {
		return meta->tripleCount[0] + meta->tripleCount[1];
	}
	int getTripleCount(unsigned char type) {
			return meta->tripleCount[type - 1];
	}
	unsigned int getPredicateID() const {
		return meta->pid;
	}

	ID getChunkNumber(unsigned char type);

	void insertXY(unsigned x, unsigned y, unsigned len, unsigned char type);

	uchar* getStartPtr(unsigned char type) {
		return reinterpret_cast<uchar*> (meta->startPtr[type -1]);
	}

	uchar* getEndPtr(unsigned char type) {
		return reinterpret_cast<uchar*> (meta->endPtr[type -1]);
	}

	Status buildChunkIndex();
	Status updateChunkIndex();
	static ChunkManager* load(unsigned pid, unsigned type, char* buffer, size_t& offset);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////

#include <boost/dynamic_bitset.hpp>

class Chunk {
private:
	unsigned char type;
	unsigned int count;
	ID xMax;
	ID xMin;
	ID yMax;
	ID yMin;
	ID colStart;
	ID colEnd;
	char* startPtr;
	char* endPtr;
	vector<bool>* soFlags;
public:
//	boost::dynamic_bitset<> flagVector;
	Chunk(unsigned char, ID, ID, ID, ID, char*, char*);
	static void writeXId(ID id, char*& ptr);
	static void writeYId(ID id, char*& ptr);
	~Chunk();
	unsigned int getCount() { return count; }
	void addCount() { count++; }
	bool getSOFlags(unsigned int pos) {
		return (*soFlags)[pos];
	}
	Status setSOFlags(unsigned int pos, bool value) {
		(*soFlags)[pos] = value;
		return OK;
	}
	vector<bool>* getSOFlagsPtr() {
		return soFlags;
	}
	bool isChunkFull() {
		//unsigned char type = this->type;
		return (unsigned int) (endPtr - startPtr + Type_2_Length(type)) > CHUNK_SIZE * getpagesize() ? true : false;
	}
	bool isChunkFull(unsigned char len) {
			//unsigned char type = this->type;
			return (unsigned int) (endPtr - startPtr + len) > CHUNK_SIZE * getpagesize() ? true : false;
	}
	unsigned char getType() {
		return type;
	}
	/// Read continuous two ids
	static const uchar* readXYId(const uchar* reader,register ID& xid,register ID &yid);
	/// Read a subject id
	static const uchar* readXId(const uchar* reader, register ID& id);
	/// Read an object id
	static const uchar* readYId(const uchar* reader, register ID& id);
	/// Delete a subject id (just set the id to 0)
	static uchar* deleteXId(uchar* reader);
	/// Delete a object id (just set the id to 0)
	static uchar* deleteYId(uchar* reader);
	/// Skip a s or o
	static const uchar* skipId(const uchar* reader, unsigned char flag);
	/// Skip backward to s
	static const uchar* skipBackward(const uchar* reader);
	static const uchar* skipBackward(const uchar* reader, const uchar* begin, unsigned type);
	static const uchar* skipForward(const uchar* reader);
	ID getXMax(void) {
		return xMax;
	}
	ID getXMin() {
		return xMin;
	}
	ID getYMax() {
		return yMax;
	}
	ID getYMin() {
		return yMin;
	}
	char* getStartPtr() {
		return startPtr;
	}
	char* getEndPtr() {
		return endPtr;
	}

	ID getColStart() const {
		return colStart;
	}

	ID getColEnd() const {
		return colEnd;
	}

	void setColStart(ID _colStart) {
		colStart = _colStart;
	}

	void setColEnd(ID _colEnd) {
		colEnd = _colEnd;
	}

	void setStartPtr(char* ptr){
		startPtr = ptr;
	}

	void setEndPtr(char* ptr) {
		endPtr = ptr;
	}
};
#endif /* CHUNKMANAGER_H_ */
