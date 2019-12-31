/*
 * StatisticsBuffer.h
 *
 *  Created on: Aug 31, 2010
 *      Author: root
 */

#ifndef STATISTICSBUFFER_H_
#define STATISTICSBUFFER_H_

class HashIndex;
class EntityIDBuffer;
class MMapBuffer;

#include "TripleBit.h"

//原始统计信息和index信息分开存储了，原始统计信息在每个StatisticsBuffer的writer/reader指针底下，具体内容为buffer指针指向的MMApBuffer类

class StatisticsBuffer {
public:
	enum StatisticsType { SUBJECT_STATIS, OBJECT_STATIS, SUBJECTPREDICATE_STATIS, OBJECTPREDICATE_STATIS };
	StatisticsBuffer();
	virtual ~StatisticsBuffer();
	/// add a statistics record;
	virtual Status addStatis(unsigned v1, unsigned v2, unsigned v3) = 0;
	/// get a statistics record;
	virtual Status getStatis(unsigned& v1, unsigned v2) = 0;
	/// save the statistics record to file;
	//virtual Status save(ofstream& file) = 0;
	/// load the statistics record from file;
	//virtual StatisticsBuffer* load(ifstream& file) = 0;
protected:
	const unsigned HEADSPACE;
};

class OneConstantStatisticsBuffer : public StatisticsBuffer {
public:
	struct Triple {
		ID value1;
		unsigned count;
	};

private:
	StatisticsType type;
	MMapBuffer* buffer;
	const unsigned char* reader;
	unsigned char* writer;

	/// index for query;
	vector<ID> index;
	unsigned long long indexSize;
	unsigned nextHashValue;
	unsigned lastId;
	unsigned long long usedSpace;

	const unsigned ID_HASH;

	Triple* triples, *pos, *posLimit;
	bool first;
public:
	OneConstantStatisticsBuffer(const string path, StatisticsType type);
	virtual ~OneConstantStatisticsBuffer();
	Status addStatis(unsigned v1, unsigned v2, unsigned v3 = 0);
	Status getStatis(unsigned& v1, unsigned v2 = 0);
	Status save(MMapBuffer*& indexBuffer);
	static OneConstantStatisticsBuffer* load(StatisticsType type, const string path, char*& indexBuffer);
	/// get the subject or object ids from minID to maxID;
	Status getIDs(EntityIDBuffer* entBuffer, ID minID, ID maxID);//没有被用到过
	size_t getEntityCount();//除了输出调试信息，没有被用到过
private:
	/// write a id to buffer; isID indicate the id really is a ID, maybe is a count.
	void writeId(unsigned id, char*& ptr, bool isID);
	/// read a id from buffer;
	const char* readId(unsigned& id, const char* ptr, bool isID);//没有用到
	/// judge the buffer is full;
	bool isPtrFull(unsigned len);
	/// get the value length in bytes;
	unsigned getLen(unsigned v);

	const unsigned char* decode(const unsigned char* begin, const unsigned char* end);
	bool find(unsigned value);
	bool find_last(unsigned value);//没有被用到过
};

class TwoConstantStatisticsBuffer : public StatisticsBuffer {
public:
	struct Triple {
		ID value1;
		ID value2;
		unsigned count;
	};
	struct TripleIndex{
		ID value1;
		ID value2;
		unsigned long long count;
	};

private:
	StatisticsType type;
	MMapBuffer* buffer;
	const unsigned char* reader;
	unsigned char* writer;

	TripleIndex* index;//index里存的可能只是偏移数据，并不是真正的源数据，源数据在buffer里边，reader指向
	
	unsigned lastId, lastPredicate;
	unsigned long long usedSpace;
	unsigned long long currentChunkNo;//没有被用到过
	unsigned long long indexPos, indexSize;

	TripleIndex* pos, *posLimit;
	TripleIndex triples[3 * 4096];//里边的东西是在查询的时候进行填充的，并且内容很少,存的就是统计信息，不是索引
	bool first;
public:
	TwoConstantStatisticsBuffer(const string path, StatisticsType type);
	virtual ~TwoConstantStatisticsBuffer();
	/// add a statistics record;
	Status addStatis(unsigned v1, unsigned v2, unsigned v3);
	/// get a statistics record;
	Status getStatis(unsigned& v1, unsigned v2);
	/// get the buffer position by a id, used in query
	//Status getPredicatesByID(unsigned id, EntityIDBuffer* buffer, ID minID, ID maxID);//没有被用到过
	/// save the statistics buffer;
	Status save(MMapBuffer*& indexBuffer);
	/// load the statistics buffer;
	static TwoConstantStatisticsBuffer* load(StatisticsType type, const string path, char*& indxBuffer);
private:
	/// get the value length in bytes;
	unsigned getLen(unsigned v);
	/// decode a chunk
	const uchar* decode(const uchar* begin, const uchar* end);
	/// decode id and predicate in a chunk
	const uchar* decodeIdAndPredicate(const uchar* begin, const uchar* end);//没有被用到过
	///
	bool find(unsigned value1, unsigned value2);
	int findPredicate(unsigned,Triple*,Triple*);
	///
	bool find_last(unsigned value1, unsigned value2);//没有被用到过
	bool find(unsigned,Triple* &,Triple* &);
	const uchar* decode(const uchar* begin, const uchar* end,Triple*,Triple*& ,Triple*&);

};
#endif /* STATISTICSBUFFER_H_ */
