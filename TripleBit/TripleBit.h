#ifndef _TRIPLEBIT_H_
#define _TRIPLEBIT_H_

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <vector>
#include <set>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <math.h>
#include <sys/time.h>
#include <stack>
#include <tr1/memory>

using namespace std;

//#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>


#include "MessageEngine.h"

//#define MYDEBUG
//#define RESULT_TIME
#define PATTERN_TIME
#define TOTAL_TIME
//#define TTDEBUG
//#define PRINT_BUFFERSIZE
#define LANG 1

template<class T> string toStr(T tmp)
{
	stringstream ss;
	ss << tmp;
	return ss.str();
}

//bitmap settings
const unsigned int INIT_PAGE_COUNT = 1024;
const unsigned int INCREMENT_PAGE_COUNT = 1024;
const unsigned int CHUNK_SIZE = 16;

//uri settings
const unsigned int URI_INIT_PAGE_COUNT = 256;
const unsigned int URI_INCREMENT_PAGE_COUNT = 256;

//reification settings
const unsigned int REIFICATION_INIT_PAGE_COUNT = 2;
const unsigned int REIFICATION_INCREMENT_PAGE_COUNT = 2;

//column buffer settings
const unsigned int COLUMN_BUFFER_INIT_PAGE_COUNT = 2;
const unsigned int COLUMN_BUFFER_INCREMENT_PAGE_COUNT = 2;

//URI statistics buffer settings
const unsigned int STATISTICS_BUFFER_INIT_PAGE_COUNT = 256;
const unsigned int STATISTICS_BUFFER_INCREMENT_PAGE_COUNT = 256;

//entity buffer settings
const unsigned int ENTITY_BUFFER_INIT_PAGE_COUNT = 1;
const unsigned int ENTITY_BUFFER_INCREMENT_PAGE_COUNT = 2;

//temp buffer settings
const unsigned int TEMPMMAPBUFFER_INIT_PAGE = 1000;
const unsigned int TEMPBUFFER_INIT_PAGE_COUNT = 1;
const unsigned int TEMPMMAP_INIT_PAGE_COUNT = 1;
const unsigned int INCREMENT_TEMPMMAP_PAGE_COUNT = 1;

//hash index
const unsigned int HASH_RANGE = 200;
const unsigned int HASH_CAPACITY = 100000 / HASH_RANGE;
const unsigned int HASH_CAPACITY_INCREASE = 100000 / HASH_RANGE;
const unsigned int SECONDARY_HASH_RANGE = 10;
const unsigned int SECONDARY_HASH_CAPACITY = 100000 / SECONDARY_HASH_RANGE;
const unsigned int SECONDARY_HASH_CAPACITY_INCREASE = 100000 / SECONDARY_HASH_RANGE;

extern char* DATABASE_PATH;

//thread pool
const unsigned int WORKERNUM = 1;
const unsigned int WORK_THREAD_NUMBER = 8; //should be 2^n;
const unsigned int PARTITION_THREAD_NUMBER = 6;
const unsigned int CHUNK_THREAD_NUMBER = 16;
const unsigned int SCAN_JOIN_THREAD_NUMBER = 16;
	
enum Status {
	OK = 1,
	NOT_FIND = -1,
	OUT_OF_MEMORY = -5,
	PTR_IS_FULL = -11,
	PTR_IS_NOT_FULL = -10,
	CHUNK_IS_FULL = -21,
	CHUNK_IS_NOT_FULL = -20,
	PREDICATE_NOT_BE_FINDED = -30,
	CHARBUFFER_IS_FULL = -40,
	CHARBUFFER_IS_NOT_FULL = -41,
	URI_NOT_FOUND = -50,
	URI_FOUND = -51,
	PREDICATE_FILE_NOT_FOUND = -60,
	PREDICATE_FILE_END = -61,
	REIFICATION_NOT_FOUND,
	FINISH_WIRITE,
	FINISH_READ,
	ERROR,
	SUBJECTID_NOT_FOUND,
	OBJECTID_NOT_FOUND,
	COLUMNNO_NOT_FOUND,
	BUFFER_NOT_FOUND,
	ENTITY_NOT_INCLUDED,
	NO_HIT,
	NOT_OPENED, 	// file was not previously opened
	END_OF_FILE, 	// read beyond end of file or no space to extend file
	LOCK_ERROR, 	// file is used by another program
	NO_MEMORY,
	URID_NOT_FOUND ,
	ALREADY_EXISTS,
	NOT_FOUND,
	CREATE_FAILURE,
	NOT_SUPPORT,
	ID_NOT_MATCH,
	ERROR_UNKOWN,
	BUFFER_MODIFIED,
	NULL_RESULT,
	TOO_MUCH,
	ERR
};

//join shape of patterns within a join variable.
enum JoinShape{
	STAR,
	CHAIN
};

enum EntityType
{
	PREDICATE = 1 << 0,
	SUBJECT = 1 << 1,
	OBJECT = 1 << 2,
	DEFAULT = -1
};

typedef long long int64;
typedef unsigned char word;
typedef word* word_prt;
typedef word_prt bitVector_ptr;
typedef unsigned int ID;
typedef unsigned int TripleNodeID;
typedef unsigned SOType;
typedef unsigned int SOID;
typedef unsigned int PID;
typedef bool status;
typedef short COMPRESS_UNIT;
typedef unsigned int uint;
typedef unsigned char uchar;
typedef unsigned short     ushort;
typedef unsigned long long ulonglong;
typedef long long          longlong;
typedef size_t       OffsetType;
typedef size_t       HashCodeType;

extern const ID INVALID_ID;

#define BITVECTOR_INITIAL_SIZE 100
#define BITVECTOR_INCREASE_SIZE 100

#define BITMAP_INITIAL_SIZE 100
#define BITMAP_INCREASE_SIZE 30

#define BUFFER_SIZE 1024
//#define DEBUG 1
#ifndef NULL
#define NULL 0
#endif

//something about shared memory
#define MAXTRANSNUM 1023
#define MAXTASKNUMWP 100
#define MAXRESULTNUM 10
#define MAXCHUNKTASK 50

//#define PRINT_RESULT 1
//#define TEST_TIME 1
#define WORD_SIZE (sizeof(word))

inline unsigned char Length_2_Type(unsigned char xLen, unsigned char yLen) {
	return (xLen - 1) * 4 + yLen;
}

//
inline unsigned char Type_2_Length(unsigned char type) {
	return (type - 1) / 4 + (type - 1) % 4 + 2;
}

inline void Type_2_Length(unsigned char type, unsigned char& xLen, unsigned char& yLen)
{
	xLen = (type - 1) / 4 + 1;
	yLen = (type - 1) % 4 + 1;
}

struct LengthString {
	const char * str;
	uint length;
	void dump(FILE * file) {
		for (uint i = 0; i < length; i++)
			fputc(str[i], file);
	}
	LengthString() :
		str(NULL), length(0) {
	}
	LengthString(const char * str) {
		this->str = str;
		this->length = strlen(str);
	}
	LengthString(const char * str, uint length) {
		this->str = str;
		this->length = length;
	}
	LengthString(const std::string & rhs) :
		str(rhs.c_str()), length(rhs.length()) {
	}
	bool equals(LengthString * rhs) {
		if (length != rhs->length)
			return false;
		for (uint i = 0; i < length; i++)
			if (str[i] != rhs->str[i])
				return false;
		return true;
	}
	bool equals(const char * str, uint length) {
		if (this->length != length)
			return false;
		for (uint i = 0; i < length; i++)
			if (this->str[i] != str[i])
				return false;
		return true;
	}
	bool equals(const char * str) {
		if(length != strlen(str))
			return false;
		for (uint i = 0; i < length; i++)
			if (this->str[i] != str[i])
				return false;
		return str[length] == 0;
	}
	bool copyTo(char * buff, uint bufflen) {
		if (length < bufflen) {
			for (uint i = 0; i < length; i++)
				buff[i] = str[i];
			buff[length] = 0;
			return true;
		}
		return false;
	}
};

struct TripleNode {
		ID subject, predicate, object;
		// Which of the three values are constants?
		bool constSubject, constPredicate, constObject;
		enum Op {
			FINDSBYPO,
			FINDOBYSP,
			FINDPBYSO,
			FINDSBYP,
			FINDOBYP,
			FINDPBYS,
			FINDPBYO,
			FINDSBYO,
			FINDOBYS,
			FINDS,
			FINDP,
			FINDO,
			NOOP,
			FINDSPBYO,
			FINDSOBYP,
			FINDPOBYS,
			FINDPSBYO,
			FINDOSBYP,
			FINDOPBYS,
			FINDSOBYNONE,
			FINDOSBYNONE
		};

		TripleNodeID tripleNodeID;
		/// define the first scan operator
		Op scanOperation;
		int selectivity;
		vector<pair<int, int> > varLevel;
		// Is there an implicit join edge to another TripleNode?
		TripleNode() {
			subject = 0;
			predicate = 0;
			object = 0;
			constSubject = 0;
			constPredicate = 0;
			constObject = 0;
			tripleNodeID = 0;
			scanOperation = TripleNode::FINDP;
			selectivity = -1;
		}
		TripleNode(const TripleNode &orig)
		{
			subject = orig.subject;
			predicate= orig.predicate;
			object = orig.object;
			constSubject = orig.constSubject;
			constPredicate = orig.constPredicate;
			constObject = orig.constObject;
			tripleNodeID = orig.tripleNodeID;
			scanOperation = orig.scanOperation;
			selectivity = orig.selectivity;
			varLevel = orig.varLevel;
		}
		TripleNode& operator=(const TripleNode& orig)
		{
			subject = orig.subject;
			predicate= orig.predicate;
			object = orig.object;
			constSubject = orig.constSubject;
			constPredicate = orig.constPredicate;
			constObject = orig.constObject;
			tripleNodeID = orig.tripleNodeID;
			scanOperation = orig.scanOperation;
			selectivity = orig.selectivity;
			varLevel = orig.varLevel;
			return *this;
		}

		bool operator<(const TripleNode& other) const
		{
			return selectivity < other.selectivity ;
		}

		bool static idSort(const TripleNode & a,const TripleNode & b){
			return a.tripleNodeID < b.tripleNodeID;
		}
};

inline uint64_t getTicks(){
	timeval t;
	gettimeofday(&t, 0);
	return static_cast<uint64_t>(t.tv_sec)*1000 + (t.tv_usec/1000);
}

#endif // _TRIPLEBIT_H_
