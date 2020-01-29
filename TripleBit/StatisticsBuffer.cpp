/*
 * StatisticsBuffer.cpp
 *
 *  Created on: Aug 31, 2010
 *      Author: root
 */

#include "StatisticsBuffer.h"
#include "BitmapBuffer.h"
#include "MMapBuffer.h"
#include "HashIndex.h"
#include "EntityIDBuffer.h"
#include "URITable.h"
#include "MemoryBuffer.h"

extern char* writeData(char* writer, unsigned data);
char* writeData8byte(char* writer, unsigned long long data)
{
	memcpy(writer, &data, 8);
	return writer + 8;
}
extern char* readData(char* reader, unsigned& data);
char* readData8byte(char* reader, unsigned long long& data)
{
	memcpy(&data, reader, 8);
	return reader + 8;
}
//将统计索引存进磁盘/读取，为了将一部分元信息(indexSize,usedSpace)改为unsigned long long类型，所以要多出来两个读写8字节的函数

static inline unsigned readDelta1(const unsigned char* pos) { return pos[0]; }
static unsigned readDelta2(const unsigned char* pos) { return (pos[0]<<8)|pos[1]; }
static unsigned readDelta3(const unsigned char* pos) { return (pos[0]<<16)|(pos[1]<<8)|pos[2]; }
static unsigned readDelta4(const unsigned char* pos) { return (pos[0]<<24)|(pos[1]<<16)|(pos[2]<<8)|pos[3]; }

// Write a 32bit value，写到内存
static void writeUint32(unsigned char* target,unsigned value){
   target[0]=value>>24;
   target[1]=(value>>16)&0xFF;
   target[2]=(value>>8)&0xFF;
   target[3]=value&0xFF;
}

// Write an integer with varying size
static unsigned char* writeDelta0(unsigned char* buffer, unsigned value){
	if (value >= (1 << 24)) {
		writeUint32(buffer, value);
		return buffer + 4;
	} else if (value >= (1 << 16)) {
		buffer[0] = value >> 16;
		buffer[1] = (value >> 8) & 0xFF;
		buffer[2] = value & 0xFF;
		return buffer + 3;
	} else if (value >= (1 << 8)) {
		buffer[0] = value >> 8;
		buffer[1] = value & 0xFF;
		return buffer + 2;
	} else if (value > 0) {
		buffer[0] = value;
		return buffer + 1;
	} else
		return buffer;
}

StatisticsBuffer::StatisticsBuffer() : HEADSPACE(2) {
	// TODO Auto-generated constructor stub
	
}

StatisticsBuffer::~StatisticsBuffer() {
	// TODO Auto-generated destructor stub
}

/////////////////////////////////////////////////////////////////

OneConstantStatisticsBuffer::OneConstantStatisticsBuffer(const string path, StatisticsType type) : StatisticsBuffer(), type(type), reader(NULL), ID_HASH(50)
{
	buffer = new MMapBuffer(path.c_str(), STATISTICS_BUFFER_INIT_PAGE_COUNT * MemoryBuffer::pagesize);
	writer = (unsigned char*)buffer->getBuffer();
//	reader = NULL;
	index.resize(2000);
	nextHashValue = 0;
	lastId = 0;
	usedSpace = 0;
	reader = NULL;
	indexSize = 2000;//indexSize = index.size()

	//triples = new Triple[ID_HASH];
	triples = (Triple *) malloc(sizeof(Triple) * ID_HASH * 4);
	first = true;
}

OneConstantStatisticsBuffer::~OneConstantStatisticsBuffer()
{
	if(buffer != NULL) {
		delete buffer;
		buffer = NULL;
	}
	if(triples != NULL) {
		delete[] triples;
		triples = NULL;
	}
	writer = NULL;
	pos = NULL;
	posLimit = NULL;
}
//没有被调用过
void OneConstantStatisticsBuffer::writeId(unsigned id, char*& ptr, bool isID)
{
	if ( isID == true ) {
		while (id >= 128) {
			unsigned char c = static_cast<unsigned char> (id & 127);
			*ptr = c;
			ptr++;
			id >>= 7;
		}
		*ptr = static_cast<unsigned char> (id & 127);
		ptr++;
	} else {
		while (id >= 128) {
			unsigned char c = static_cast<unsigned char> (id | 128);
			*ptr = c;
			ptr++;
			id >>= 7;
		}
		*ptr = static_cast<unsigned char> (id | 128);
		ptr++;
	}
}

bool OneConstantStatisticsBuffer::isPtrFull(unsigned len) 
{
	return (unsigned int) ( writer - (unsigned char*)buffer->getBuffer() + len ) > buffer->getSize() ? true : false;
}

//得到v的长度，便于压缩前识别
unsigned OneConstantStatisticsBuffer::getLen(unsigned v)
{
	if (v >= (1 << 24))
		return 4;
	else if (v >= (1 << 16))
		return 3;
	else if (v >= (1 << 8))
		return 2;
	else if(v > 0)
		return 1;
	else
		return 0;
}

//没有被调用过
static size_t countEntity(const unsigned char* begin, const unsigned char* end)
{
	if(begin >= end) 
		return 0;

	//cout<<"begin - end: "<<end - begin<<endl;

	size_t entityCount = 0;
	entityCount = 1;
	begin = begin + 8;

	while(begin < end) {
		// Decode the header byte
		unsigned info = *(begin++);
		// Small gap only?
		if (info < 0x80) {
			if (!info)
				break;
			/*
			count = (info >> 4) + 1;
			value1 += (info & 15);
			(*writer).value1 = value1;
			(*writer).count = count;
			++writer;
			*/
			entityCount++ ;
			continue;
		}
		// Decode the parts
		//value1 += 1;
		switch (info & 127) {
			case 0: break;
			case 1: begin += 1; break;
			case 2: begin += 2;break;
			case 3: begin += 3;break;
			case 4: begin += 4;break;
			case 5: begin += 1;break;
			case 6: begin += 2;break;
			case 7: begin += 3;break;
			case 8: begin += 4;break;
			case 9: begin += 5; break;
			case 10: begin += 2; break;
			case 11: begin += 3; break;
			case 12: begin += 4; break;
			case 13: begin += 5; break;
			case 14: begin += 6; break;
			case 15: begin += 3; break;
			case 16: begin += 4; break;
			case 17: begin += 5; break;
			case 18: begin += 6; break;
			case 19: begin += 7;break;
			case 20: begin += 4;break;
			case 21: begin += 5;break;
			case 22: begin += 6;break;
			case 23: begin += 7;break;
			case 24: begin += 8;break;
		}
		entityCount++;
	}

	return entityCount;
}

const unsigned char* OneConstantStatisticsBuffer::decode(const unsigned char* begin, const unsigned char* end)
{
	Triple* writer = triples;
	unsigned value1, count;
	value1 = readDelta4(begin);
	begin += 4;
	count = readDelta4(begin);
	begin += 4;

	(*writer).value1 = value1;
	(*writer).count = count;
	writer++;

	while (begin < end) {
		// Decode the header byte
		unsigned info = *(begin++);
		// Small gap only?
		if (info < 0x80) {
			if (!info)
				break;
			count = (info >> 4) + 1;
			value1 += (info & 15);
			(*writer).value1 = value1;
			(*writer).count = count;
			++writer;
			continue;
		}
		// Decode the parts
		value1 += 1;
		switch (info & 127) {
			case 0: count = 1;break;
			case 1: count = readDelta1(begin) + 1; begin += 1; break;
			case 2: count = readDelta2(begin) + 1;begin += 2;break;
			case 3: count = readDelta3(begin) + 1;begin += 3;break;
			case 4: count = readDelta4(begin) + 1;begin += 4;break;
			case 5: value1 += readDelta1(begin);count = 1;begin += 1;break;
			case 6: value1 += readDelta1(begin);count = readDelta1(begin + 1) + 1;begin += 2;break;
			case 7: value1 += readDelta1(begin);count = readDelta2(begin + 1) + 1;begin += 3;break;
			case 8: value1 += readDelta1(begin);count = readDelta3(begin + 1) + 1;begin += 4;break;
			case 9: value1 += readDelta1(begin); count = readDelta4(begin + 1) + 1; begin += 5; break;
			case 10: value1 += readDelta2(begin); count = 1; begin += 2; break;
			case 11: value1 += readDelta2(begin); count = readDelta1(begin + 2) + 1; begin += 3; break;
			case 12: value1 += readDelta2(begin); count = readDelta2(begin + 2) + 1; begin += 4; break;
			case 13: value1 += readDelta2(begin); count = readDelta3(begin + 2) + 1; begin += 5; break;
			case 14: value1 += readDelta2(begin); count = readDelta4(begin + 2) + 1; begin += 6; break;
			case 15: value1 += readDelta3(begin); count = 1; begin += 3; break;
			case 16: value1 += readDelta3(begin); count = readDelta1(begin + 3) + 1; begin += 4; break;
			case 17: value1 += readDelta3(begin); count = readDelta2(begin + 3) + 1; begin += 5; break;
			case 18: value1 += readDelta3(begin); count = readDelta3(begin + 3) + 1; begin += 6; break;
			case 19: value1 += readDelta3(begin);count = readDelta4(begin + 3) + 1;begin += 7;break;
			case 20: value1 += readDelta4(begin);count = 1;begin += 4;break;
			case 21: value1 += readDelta4(begin);count = readDelta1(begin + 4) + 1;begin += 5;break;
			case 22: value1 += readDelta4(begin);count = readDelta2(begin + 4) + 1;begin += 6;break;
			case 23: value1 += readDelta4(begin);count = readDelta3(begin + 4) + 1;begin += 7;break;
			case 24: value1 += readDelta4(begin);count = readDelta4(begin + 4) + 1;begin += 8;break;
		}
		(*writer).value1 = value1;
		(*writer).count = count;
		++writer;
	}

	pos = triples;
	posLimit = writer;

	return begin;
}

//除了输出调试信息，没有被用到过
size_t OneConstantStatisticsBuffer::getEntityCount()
{
	size_t entityCount = 0;
	size_t i = 0;

	const unsigned char* begin, *end;
	size_t beginChunk = 0, endChunk = 0;

#ifdef DEBUG
	cout<<"indexSize: "<<indexSize<<endl;
#endif
	for(i = 1; i <= indexSize; i++) {
		if(i < indexSize)
			endChunk = index[i];

		while(endChunk == 0 && i < indexSize) {
			i++;
			endChunk = index[i];
		}
		
		if(i == indexSize) { 
			endChunk = usedSpace;
		}
			
		if(endChunk != 0) {
			begin = (const unsigned char*)(buffer->getBuffer()) + beginChunk;
			end = (const unsigned char*)(buffer->getBuffer()) + endChunk;
			entityCount = entityCount + countEntity(begin, end);

			beginChunk = endChunk;
		}

		//beginChunk = endChunk;
	}

	return entityCount;
}

Status OneConstantStatisticsBuffer::addStatis(unsigned v1, unsigned v2, unsigned v3 /* = 0 */)
{
//	static bool first = true;
	unsigned interVal;
	if ( v1 >= nextHashValue ) {
		interVal = v1;
	} else {
		interVal = v1 - lastId;
	}

	unsigned len;
	if(v1 >= nextHashValue) {
		len = 8;
	} else if(interVal < 16 && v2 <= 8) {
		len = 1;
	} else {
		len = 1 + getLen(interVal - 1) + getLen(v2 - 1);
	}

	if ( isPtrFull(len) == true ) {
		//MessageEngine::showMessage("OneConstantStatisticsBuffer::addStatis()", MessageEngine::INFO);
		usedSpace = writer - (unsigned char*)buffer->getBuffer();
		buffer->resize(STATISTICS_BUFFER_INCREMENT_PAGE_COUNT * MemoryBuffer::pagesize);
		writer = (unsigned char*)buffer->getBuffer() + usedSpace;
	}

	if ( first || (v1 >= nextHashValue) ) {
		unsigned long long offset = writer - (uchar*)buffer->getBuffer();
		while (index.size() <= (v1 / ID_HASH)) {
			index.resize(index.size() + 2000, 0);
			indexSize += 2000;
#ifdef DEBUG
			cout<<"index size"<<index.size()<<" v1 / ID_HASH: "<<(v1 / ID_HASH)<<endl;
#endif
		}

		index[v1 / ID_HASH] = offset;
		while(nextHashValue <= v1) nextHashValue += HASH_RANGE;

		writeUint32(writer, v1);
		writer += 4;
		writeUint32(writer, v2);
		writer += 4;

		first = false;
	} else {
		//其实OneConstantStatisticsBuffer的块索引也是压缩存储的，所以OneConstantStatisticsBuffer.decode是对块索引解压缩
		if(len == 1) {
			*writer = ((v2 - 1) << 4) | (interVal);
			writer++;
		} else {
			*writer = 0x80|((getLen(interVal-1)*5)+getLen(v2 - 1));
			writer++;
			writer = writeDelta0(writer,interVal - 1);
			writer = writeDelta0(writer, v2 - 1);
		}
	}

	lastId = v1;
	usedSpace = writer - (uchar*)buffer->getBuffer();

	return OK;
}

bool OneConstantStatisticsBuffer::find(unsigned value)
{
	//const Triple* l = pos, *r = posLimit;
	int l = 0, r = posLimit - pos;
	int m;
	while (l != r) {
		m = l + ((r - l) / 2);
		if (value > pos[m].value1) {
			l = m + 1;
		} else if ((!m) || value > pos[m - 1].value1) {
			break;
		} else {
			r = m;
		}
	}

	if(l == r)
		return false;
	else {
		pos = &pos[m];
		return true;
	}

}

//没有被用到过
bool OneConstantStatisticsBuffer::find_last(unsigned value)
{
	//const Triple* l = pos, *r = posLimit;
	int left = 0, right = posLimit - pos;
	int middle = 0;

	while (left < right) {
		middle = left + ((right - left) / 2);
		if (value < pos[middle].value1) {
			right = middle;
		} else if ((!middle) || value < pos[middle + 1].value1) {
			break;
		} else {
			left = middle + 1;
		}
	}

	if(left == right) {
		return false;
	} else {
		pos = &pos[middle];
		return true;
	}
}

Status OneConstantStatisticsBuffer::getStatis(unsigned& v1, unsigned v2 /* = 0 */)
{
	unsigned long long i;
	unsigned long long begin = index[ v1 / ID_HASH];
	unsigned long long end = 0;

	i = v1 / ID_HASH + 1;
	while(i < indexSize) { // get next chunk start offset;
		if(index[i] != 0) {
			end = index[i];
			break;
		}
		i++;
	}

	if(i == indexSize)
		end = usedSpace;

	reader = (unsigned char*)buffer->getBuffer() + begin;

	lastId = readDelta4(reader);
	reader += 4;

	//reader = readId(lastId, reader, true);
	if ( lastId == v1 ) {
		//reader = readId(v1, reader, false);
		v1 = readDelta4(reader);
		return OK;
	}

	const uchar* limit = (uchar*)buffer->getBuffer() + end;
	this->decode(reader - 4, limit);//对块索引调用的decode！所以里边不涉及统计偏移索引逻辑
	if(this->find(v1)) {
		if(pos->value1 == v1) {
			v1 = pos->count;
			return OK;
		}
	}

	v1 = 0;
	return ERROR;
}

Status OneConstantStatisticsBuffer::save(MMapBuffer*& indexBuffer)
{//save的时候save的是index指向的东西，不是块索引！块索引里的东西是通过mmap机制自动同步磁盘
#ifdef DEBUG
	cout<<"index size: "<<index.size()<<endl;
#endif
	char * writer;//注意，这个writer可不是类成员变量里的writer！
	indexSize = index.size();
	if(indexBuffer == NULL) {
		indexBuffer = MMapBuffer::create(string(string(DATABASE_PATH) + "/statIndex").c_str(), sizeof(usedSpace) + sizeof(indexSize) + indexSize * sizeof(index[0]));

		writer = indexBuffer->get_address();
	} else {
		size_t size = indexBuffer->getSize();
		indexBuffer->resize(sizeof(usedSpace) + sizeof(indexSize) + indexSize * sizeof(index[0]));
		writer = indexBuffer->get_address() + size;
	}
	//cout << "↓" << endl;
	//cout << indexSize << "\t" << usedSpace << endl;
	//for (int i = 0; i < indexSize;i++) {
	//	cout << i << " -> " << index[i] << endl;
	//}
	//cout << "↑" << endl;
	writer = writeData8byte(writer, usedSpace);
	writer = writeData8byte(writer, indexSize);//indexSize==index.size()
	//这个writer指向index文件（MMapBuffer）
	//这个文件的存储结构是，前几个字节是index的源信息，indexSize和UsedSpace。后边的内容为index本体内容
	vector<unsigned long long>::iterator iter, limit;

	for(iter = index.begin(), limit = index.end(); iter != limit; ++iter) {
		writer = writeData8byte(writer, *iter);
	}//这里用循环writeData代替了memcpy
	//底层的存进磁盘不用管，因为的mmap机制
	//memcpy(writer, index, indexSize * sizeof(unsigned));

	return OK;
}

OneConstantStatisticsBuffer* OneConstantStatisticsBuffer::load(StatisticsType type,const string path, char*& indexBuffer)
{
	OneConstantStatisticsBuffer* statBuffer = new OneConstantStatisticsBuffer(path, type);

	unsigned long long size;
	indexBuffer = readData8byte(indexBuffer, statBuffer->usedSpace);
	indexBuffer = readData8byte(indexBuffer, size);
	statBuffer->index.resize(0);
	statBuffer->indexSize = size;

	unsigned long long first;

	for( unsigned i = 0; i < size; i++ ) {
		indexBuffer = readData8byte(indexBuffer, first);
		statBuffer->index.push_back(first);
	}

	//cout << "↓" << endl;
	//cout << size << "\t" << statBuffer->usedSpace << endl;
	//for (int i = 0; i < size;i++) {
	//	cout << i << " -> " << statBuffer->index[i] << endl;
	//}
	//cout << "↑" << endl;

	return statBuffer;
}

Status OneConstantStatisticsBuffer::getIDs(EntityIDBuffer* entBuffer, ID minID, ID maxID)//没有被用到过
{
	unsigned i = 0, endEntry = 0;
	unsigned begin = index[ minID / HASH_RANGE], end = 0;
	reader = (uchar*)buffer->getBuffer() + begin;

	i = minID / ID_HASH;
	while(i < indexSize) {
		if(index[i] != 0) {
			end = index[i];
			break;
		}
		i++;
	}
	if(i == indexSize)
		end = usedSpace;
	endEntry = i;

	const uchar* limit = (uchar*)buffer->getBuffer() + end;

	lastId = readDelta4(reader);
	decode(reader, limit);
	if ( lastId != minID ) {
		find(minID);
	}

	i = maxID / ID_HASH + 1;
	unsigned end1;
	while(i < indexSize && index[i] == 0) {
		i++;
	}
	if(i == indexSize)
		end1 = usedSpace;
	else
		end1 = index[i];

	while(true) {
		if(end == end1) {
			Triple* temp = pos;
			if(find(maxID) == true)
				posLimit = pos + 1;
			pos = temp;
		}

		while(pos < posLimit) {
			entBuffer->insertID(pos->value1);
			pos++;
		}

		begin = end;
		if(begin == end1)
			break;

		endEntry = endEntry + 1;
		while(endEntry < indexSize && index[endEntry] != 0) {
			endEntry++;
		}
		if(endEntry == indexSize) {
			end = usedSpace;
		} else {
			end = index[endEntry];
		}

		reader = (const unsigned char*)buffer->getBuffer() + begin;
		limit = (const unsigned char*)buffer->getBuffer() + end;
		decode(reader, limit);
	}

	return OK;
}

//////////////////////////////////////////////////////////////////////

TwoConstantStatisticsBuffer::TwoConstantStatisticsBuffer(const string path, StatisticsType type) : StatisticsBuffer(), type(type), reader(NULL)
{
	buffer = new MMapBuffer(path.c_str(), STATISTICS_BUFFER_INIT_PAGE_COUNT * MemoryBuffer::pagesize);
	//index = (Triple*)malloc(MemoryBuffer::pagesize * sizeof(Triple));
	writer = (uchar*)buffer->getBuffer();
	lastId = 0;
	lastPredicate = 0;
	usedSpace = 0;
	indexPos = 0;
	indexSize = 0; //MemoryBuffer::pagesize;
	index = NULL;
	triples = (Triple*)malloc(sizeof(Triple) * 1024 * 4);
	first = true;
}

TwoConstantStatisticsBuffer::~TwoConstantStatisticsBuffer()
{
	if(buffer != NULL) {
		delete buffer;
		buffer = NULL;
	}
	if (triples != NULL) {
		delete[] triples;
		triples = NULL;
	}
	if (index != NULL) {
		delete[] index;
		index = NULL;
	}
	writer = NULL;
	pos = NULL;
	posLimit = NULL;
	posForIndex = NULL;
	posLimitForIndex = NULL;
}

const uchar* TwoConstantStatisticsBuffer::decode(const uchar* begin, const uchar* end)
{
	unsigned value1 = readDelta4(begin); begin += 4;
	unsigned value2 = readDelta4(begin); begin += 4;
	unsigned count = readDelta4(begin); begin += 4;
	Triple* writer = &triples[0];
	(*writer).value1 = value1;
	(*writer).value2 = value2;
	(*writer).count = count;
	++writer;

	// Decompress the remainder of the page
	while (begin < end) {
		// Decode the header byte
		unsigned info = *(begin++);
		// Small gap only?
		if (info < 0x80) {
			if (!info)
				break;
			count = (info >> 5) + 1;
			value2 += (info & 31);
			(*writer).value1 = value1;
			(*writer).value2 = value2;
			(*writer).count = count;
			++writer;
			continue;
		}
		// Decode the parts
		switch (info&127) {
		case 0: count=1; break;
		case 1: count=readDelta1(begin)+1; begin+=1; break;
		case 2: count=readDelta2(begin)+1; begin+=2; break;
		case 3: count=readDelta3(begin)+1; begin+=3; break;
		case 4: count=readDelta4(begin)+1; begin+=4; break;
		case 5: value2 += readDelta1(begin); count=1; begin+=1; break;
		case 6: value2 += readDelta1(begin); count=readDelta1(begin+1)+1; begin+=2; break;
		case 7: value2 += readDelta1(begin); count=readDelta2(begin+1)+1; begin+=3; break;
		case 8: value2 += readDelta1(begin); count=readDelta3(begin+1)+1; begin+=4; break;
		case 9: value2 += readDelta1(begin); count=readDelta4(begin+1)+1; begin+=5; break;
		case 10: value2 += readDelta2(begin); count=1; begin+=2; break;
		case 11: value2 += readDelta2(begin); count=readDelta1(begin+2)+1; begin+=3; break;
		case 12: value2 += readDelta2(begin); count=readDelta2(begin+2)+1; begin+=4; break;
		case 13: value2 += readDelta2(begin); count=readDelta3(begin+2)+1; begin+=5; break;
		case 14: value2 += readDelta2(begin); count=readDelta4(begin+2)+1; begin+=6; break;
		case 15: value2 += readDelta3(begin); count=1; begin+=3; break;
		case 16: value2 += readDelta3(begin); count=readDelta1(begin+3)+1; begin+=4; break;
		case 17: value2 += readDelta3(begin); count=readDelta2(begin+3)+1; begin+=5; break;
		case 18: value2 += readDelta3(begin); count=readDelta3(begin+3)+1; begin+=6; break;
		case 19: value2 += readDelta3(begin); count=readDelta4(begin+3)+1; begin+=7; break;
		case 20: value2 += readDelta4(begin); count=1; begin+=4; break;
		case 21: value2 += readDelta4(begin); count=readDelta1(begin+4)+1; begin+=5; break;
		case 22: value2 += readDelta4(begin); count=readDelta2(begin+4)+1; begin+=6; break;
		case 23: value2 += readDelta4(begin); count=readDelta3(begin+4)+1; begin+=7; break;
		case 24: value2 += readDelta4(begin); count=readDelta4(begin+4)+1; begin+=8; break;
		case 25: value1 += readDelta1(begin); value2=0; count=1; begin+=1; break;
		case 26: value1 += readDelta1(begin); value2=0; count=readDelta1(begin+1)+1; begin+=2; break;
		case 27: value1 += readDelta1(begin); value2=0; count=readDelta2(begin+1)+1; begin+=3; break;
		case 28: value1 += readDelta1(begin); value2=0; count=readDelta3(begin+1)+1; begin+=4; break;
		case 29: value1 += readDelta1(begin); value2=0; count=readDelta4(begin+1)+1; begin+=5; break;
		case 30: value1 += readDelta1(begin); value2=readDelta1(begin+1); count=1; begin+=2; break;
		case 31: value1 += readDelta1(begin); value2=readDelta1(begin+1); count=readDelta1(begin+2)+1; begin+=3; break;
		case 32: value1 += readDelta1(begin); value2=readDelta1(begin+1); count=readDelta2(begin+2)+1; begin+=4; break;
		case 33: value1+=readDelta1(begin); value2=readDelta1(begin+1); count=readDelta3(begin+2)+1; begin+=5; break;
		case 34: value1+=readDelta1(begin); value2=readDelta1(begin+1); count=readDelta4(begin+2)+1; begin+=6; break;
		case 35: value1+=readDelta1(begin); value2=readDelta2(begin+1); count=1; begin+=3; break;
		case 36: value1+=readDelta1(begin); value2=readDelta2(begin+1); count=readDelta1(begin+3)+1; begin+=4; break;
		case 37: value1+=readDelta1(begin); value2=readDelta2(begin+1); count=readDelta2(begin+3)+1; begin+=5; break;
		case 38: value1+=readDelta1(begin); value2=readDelta2(begin+1); count=readDelta3(begin+3)+1; begin+=6; break;
		case 39: value1+=readDelta1(begin); value2=readDelta2(begin+1); count=readDelta4(begin+3)+1; begin+=7; break;
		case 40: value1+=readDelta1(begin); value2=readDelta3(begin+1); count=1; begin+=4; break;
		case 41: value1+=readDelta1(begin); value2=readDelta3(begin+1); count=readDelta1(begin+4)+1; begin+=5; break;
		case 42: value1+=readDelta1(begin); value2=readDelta3(begin+1); count=readDelta2(begin+4)+1; begin+=6; break;
		case 43: value1+=readDelta1(begin); value2=readDelta3(begin+1); count=readDelta3(begin+4)+1; begin+=7; break;
		case 44: value1+=readDelta1(begin); value2=readDelta3(begin+1); count=readDelta4(begin+4)+1; begin+=8; break;
		case 45: value1+=readDelta1(begin); value2=readDelta4(begin+1); count=1; begin+=5; break;
		case 46: value1+=readDelta1(begin); value2=readDelta4(begin+1); count=readDelta1(begin+5)+1; begin+=6; break;
		case 47: value1+=readDelta1(begin); value2=readDelta4(begin+1); count=readDelta2(begin+5)+1; begin+=7; break;
		case 48: value1+=readDelta1(begin); value2=readDelta4(begin+1); count=readDelta3(begin+5)+1; begin+=8; break;
		case 49: value1+=readDelta1(begin); value2=readDelta4(begin+1); count=readDelta4(begin+5)+1; begin+=9; break;
		case 50: value1+=readDelta2(begin); value2=0; count=1; begin+=2; break;
		case 51: value1+=readDelta2(begin); value2=0; count=readDelta1(begin+2)+1; begin+=3; break;
		case 52: value1+=readDelta2(begin); value2=0; count=readDelta2(begin+2)+1; begin+=4; break;
		case 53: value1+=readDelta2(begin); value2=0; count=readDelta3(begin+2)+1; begin+=5; break;
		case 54: value1+=readDelta2(begin); value2=0; count=readDelta4(begin+2)+1; begin+=6; break;
		case 55: value1+=readDelta2(begin); value2=readDelta1(begin+2); count=1; begin+=3; break;
		case 56: value1+=readDelta2(begin); value2=readDelta1(begin+2); count=readDelta1(begin+3)+1; begin+=4; break;
		case 57: value1+=readDelta2(begin); value2=readDelta1(begin+2); count=readDelta2(begin+3)+1; begin+=5; break;
		case 58: value1+=readDelta2(begin); value2=readDelta1(begin+2); count=readDelta3(begin+3)+1; begin+=6; break;
		case 59: value1+=readDelta2(begin); value2=readDelta1(begin+2); count=readDelta4(begin+3)+1; begin+=7; break;
		case 60: value1+=readDelta2(begin); value2=readDelta2(begin+2); count=1; begin+=4; break;
		case 61: value1+=readDelta2(begin); value2=readDelta2(begin+2); count=readDelta1(begin+4)+1; begin+=5; break;
		case 62: value1+=readDelta2(begin); value2=readDelta2(begin+2); count=readDelta2(begin+4)+1; begin+=6; break;
		case 63: value1+=readDelta2(begin); value2=readDelta2(begin+2); count=readDelta3(begin+4)+1; begin+=7; break;
		case 64: value1+=readDelta2(begin); value2=readDelta2(begin+2); count=readDelta4(begin+4)+1; begin+=8; break;
		case 65: value1+=readDelta2(begin); value2=readDelta3(begin+2); count=1; begin+=5; break;
		case 66: value1+=readDelta2(begin); value2=readDelta3(begin+2); count=readDelta1(begin+5)+1; begin+=6; break;
		case 67: value1+=readDelta2(begin); value2=readDelta3(begin+2); count=readDelta2(begin+5)+1; begin+=7; break;
		case 68: value1+=readDelta2(begin); value2=readDelta3(begin+2); count=readDelta3(begin+5)+1; begin+=8; break;
		case 69: value1+=readDelta2(begin); value2=readDelta3(begin+2); count=readDelta4(begin+5)+1; begin+=9; break;
		case 70: value1+=readDelta2(begin); value2=readDelta4(begin+2); count=1; begin+=6; break;
		case 71: value1+=readDelta2(begin); value2=readDelta4(begin+2); count=readDelta1(begin+6)+1; begin+=7; break;
		case 72: value1+=readDelta2(begin); value2=readDelta4(begin+2); count=readDelta2(begin+6)+1; begin+=8; break;
		case 73: value1+=readDelta2(begin); value2=readDelta4(begin+2); count=readDelta3(begin+6)+1; begin+=9; break;
		case 74: value1+=readDelta2(begin); value2=readDelta4(begin+2); count=readDelta4(begin+6)+1; begin+=10; break;
		case 75: value1+=readDelta3(begin); value2=0; count=1; begin+=3; break;
		case 76: value1+=readDelta3(begin); value2=0; count=readDelta1(begin+3)+1; begin+=4; break;
		case 77: value1+=readDelta3(begin); value2=0; count=readDelta2(begin+3)+1; begin+=5; break;
		case 78: value1+=readDelta3(begin); value2=0; count=readDelta3(begin+3)+1; begin+=6; break;
		case 79: value1+=readDelta3(begin); value2=0; count=readDelta4(begin+3)+1; begin+=7; break;
		case 80: value1+=readDelta3(begin); value2=readDelta1(begin+3); count=1; begin+=4; break;
		case 81: value1+=readDelta3(begin); value2=readDelta1(begin+3); count=readDelta1(begin+4)+1; begin+=5; break;
		case 82: value1+=readDelta3(begin); value2=readDelta1(begin+3); count=readDelta2(begin+4)+1; begin+=6; break;
		case 83: value1+=readDelta3(begin); value2=readDelta1(begin+3); count=readDelta3(begin+4)+1; begin+=7; break;
		case 84: value1+=readDelta3(begin); value2=readDelta1(begin+3); count=readDelta4(begin+4)+1; begin+=8; break;
		case 85: value1+=readDelta3(begin); value2=readDelta2(begin+3); count=1; begin+=5; break;
		case 86: value1+=readDelta3(begin); value2=readDelta2(begin+3); count=readDelta1(begin+5)+1; begin+=6; break;
		case 87: value1+=readDelta3(begin); value2=readDelta2(begin+3); count=readDelta2(begin+5)+1; begin+=7; break;
		case 88: value1+=readDelta3(begin); value2=readDelta2(begin+3); count=readDelta3(begin+5)+1; begin+=8; break;
		case 89: value1+=readDelta3(begin); value2=readDelta2(begin+3); count=readDelta4(begin+5)+1; begin+=9; break;
		case 90: value1+=readDelta3(begin); value2=readDelta3(begin+3); count=1; begin+=6; break;
		case 91: value1+=readDelta3(begin); value2=readDelta3(begin+3); count=readDelta1(begin+6)+1; begin+=7; break;
		case 92: value1+=readDelta3(begin); value2=readDelta3(begin+3); count=readDelta2(begin+6)+1; begin+=8; break;
		case 93: value1+=readDelta3(begin); value2=readDelta3(begin+3); count=readDelta3(begin+6)+1; begin+=9; break;
		case 94: value1+=readDelta3(begin); value2=readDelta3(begin+3); count=readDelta4(begin+6)+1; begin+=10; break;
		case 95: value1+=readDelta3(begin); value2=readDelta4(begin+3); count=1; begin+=7; break;
		case 96: value1+=readDelta3(begin); value2=readDelta4(begin+3); count=readDelta1(begin+7)+1; begin+=8; break;
		case 97: value1+=readDelta3(begin); value2=readDelta4(begin+3); count=readDelta2(begin+7)+1; begin+=9; break;
		case 98: value1+=readDelta3(begin); value2=readDelta4(begin+3); count=readDelta3(begin+7)+1; begin+=10; break;
		case 99: value1+=readDelta3(begin); value2=readDelta4(begin+3); count=readDelta4(begin+7)+1; begin+=11; break;
		case 100: value1+=readDelta4(begin); value2=0; count=1; begin+=4; break;
		case 101: value1+=readDelta4(begin); value2=0; count=readDelta1(begin+4)+1; begin+=5; break;
		case 102: value1+=readDelta4(begin); value2=0; count=readDelta2(begin+4)+1; begin+=6; break;
		case 103: value1+=readDelta4(begin); value2=0; count=readDelta3(begin+4)+1; begin+=7; break;
		case 104: value1+=readDelta4(begin); value2=0; count=readDelta4(begin+4)+1; begin+=8; break;
		case 105: value1+=readDelta4(begin); value2=readDelta1(begin+4); count=1; begin+=5; break;
		case 106: value1+=readDelta4(begin); value2=readDelta1(begin+4); count=readDelta1(begin+5)+1; begin+=6; break;
		case 107: value1+=readDelta4(begin); value2=readDelta1(begin+4); count=readDelta2(begin+5)+1; begin+=7; break;
		case 108: value1+=readDelta4(begin); value2=readDelta1(begin+4); count=readDelta3(begin+5)+1; begin+=8; break;
		case 109: value1+=readDelta4(begin); value2=readDelta1(begin+4); count=readDelta4(begin+5)+1; begin+=9; break;
		case 110: value1+=readDelta4(begin); value2=readDelta2(begin+4); count=1; begin+=6; break;
		case 111: value1+=readDelta4(begin); value2=readDelta2(begin+4); count=readDelta1(begin+6)+1; begin+=7; break;
		case 112: value1+=readDelta4(begin); value2=readDelta2(begin+4); count=readDelta2(begin+6)+1; begin+=8; break;
		case 113: value1+=readDelta4(begin); value2=readDelta2(begin+4); count=readDelta3(begin+6)+1; begin+=9; break;
		case 114: value1+=readDelta4(begin); value2=readDelta2(begin+4); count=readDelta4(begin+6)+1; begin+=10; break;
		case 115: value1+=readDelta4(begin); value2=readDelta3(begin+4); count=1; begin+=7; break;
		case 116: value1+=readDelta4(begin); value2=readDelta3(begin+4); count=readDelta1(begin+7)+1; begin+=8; break;
		case 117: value1+=readDelta4(begin); value2=readDelta3(begin+4); count=readDelta2(begin+7)+1; begin+=9; break;
		case 118: value1+=readDelta4(begin); value2=readDelta3(begin+4); count=readDelta3(begin+7)+1; begin+=10; break;
		case 119: value1+=readDelta4(begin); value2=readDelta3(begin+4); count=readDelta4(begin+7)+1; begin+=11; break;
		case 120: value1+=readDelta4(begin); value2=readDelta4(begin+4); count=1; begin+=8; break;
		case 121: value1+=readDelta4(begin); value2=readDelta4(begin+4); count=readDelta1(begin+8)+1; begin+=9; break;
		case 122: value1+=readDelta4(begin); value2=readDelta4(begin+4); count=readDelta2(begin+8)+1; begin+=10; break;
		case 123: value1+=readDelta4(begin); value2=readDelta4(begin+4); count=readDelta3(begin+8)+1; begin+=11; break;
		case 124: value1+=readDelta4(begin); value2=readDelta4(begin+4); count=readDelta4(begin+8)+1; begin+=12; break;
		}
		(*writer).value1=value1;
		(*writer).value2=value2;
		(*writer).count=count;
		++writer;
	}

	// Update the entries
	pos=triples;
	posLimit=writer;

	return begin;
}

const uchar* TwoConstantStatisticsBuffer::decodeIdAndPredicate(const uchar* begin, const uchar* end)//没有被用到过
{
	unsigned value1 = readDelta4(begin); begin += 4;
	unsigned value2 = readDelta4(begin); begin += 4;
	unsigned count = readDelta4(begin); begin += 4;
	Triple* writer = &triples[0];
	(*writer).value1 = value1;
	(*writer).value2 = value2;
	(*writer).count = count;
	++writer;

	// Decompress the remainder of the page
	while (begin < end) {
		// Decode the header byte
		unsigned info = *(begin++);
		// Small gap only?
		if (info < 0x80) {
			if (!info)
				break;
			//count = (info >> 5) + 1;
			value2 += (info & 31);
			(*writer).value1 = value1;
			(*writer).value2 = value2;
			(*writer).count = count;
			++writer;
			continue;
		}
	      // Decode the parts
		switch (info&127) {
		case 0: break;
		case 1: begin+=1; break;
		case 2: begin+=2; break;
		case 3: begin+=3; break;
		case 4: begin+=4; break;
		case 5: value2 += readDelta1(begin); begin+=1; break;
		case 6: value2 += readDelta1(begin); begin+=2; break;
		case 7: value2 += readDelta1(begin); begin+=3; break;
		case 8: value2 += readDelta1(begin); begin+=4; break;
		case 9: value2 += readDelta1(begin); begin+=5; break;
		case 10: value2 += readDelta2(begin); begin+=2; break;
		case 11: value2 += readDelta2(begin); begin+=3; break;
		case 12: value2 += readDelta2(begin); begin+=4; break;
		case 13: value2 += readDelta2(begin); begin+=5; break;
		case 14: value2 += readDelta2(begin); begin+=6; break;
		case 15: value2 += readDelta3(begin); begin+=3; break;
		case 16: value2 += readDelta3(begin); begin+=4; break;
		case 17: value2 += readDelta3(begin); begin+=5; break;
		case 18: value2 += readDelta3(begin); begin+=6; break;
		case 19: value2 += readDelta3(begin); begin+=7; break;
		case 20: value2 += readDelta4(begin); begin+=4; break;
		case 21: value2 += readDelta4(begin); begin+=5; break;
		case 22: value2 += readDelta4(begin); begin+=6; break;
		case 23: value2 += readDelta4(begin); begin+=7; break;
		case 24: value2 += readDelta4(begin); begin+=8; break;
		case 25: value1 += readDelta1(begin); value2=0; begin+=1; break;
		case 26: value1 += readDelta1(begin); value2=0; begin+=2; break;
		case 27: value1 += readDelta1(begin); value2=0; begin+=3; break;
		case 28: value1 += readDelta1(begin); value2=0; begin+=4; break;
		case 29: value1 += readDelta1(begin); value2=0; begin+=5; break;
		case 30: value1 += readDelta1(begin); value2=readDelta1(begin+1); begin+=2; break;
		case 31: value1 += readDelta1(begin); value2=readDelta1(begin+1); begin+=3; break;
		case 32: value1 += readDelta1(begin); value2=readDelta1(begin+1); begin+=4; break;
		case 33: value1+=readDelta1(begin); value2=readDelta1(begin+1); begin+=5; break;
		case 34: value1+=readDelta1(begin); value2=readDelta1(begin+1); begin+=6; break;
		case 35: value1+=readDelta1(begin); value2=readDelta2(begin+1); begin+=3; break;
		case 36: value1+=readDelta1(begin); value2=readDelta2(begin+1); begin+=4; break;
		case 37: value1+=readDelta1(begin); value2=readDelta2(begin+1); begin+=5; break;
		case 38: value1+=readDelta1(begin); value2=readDelta2(begin+1); begin+=6; break;
		case 39: value1+=readDelta1(begin); value2=readDelta2(begin+1); begin+=7; break;
		case 40: value1+=readDelta1(begin); value2=readDelta3(begin+1); begin+=4; break;
		case 41: value1+=readDelta1(begin); value2=readDelta3(begin+1); begin+=5; break;
		case 42: value1+=readDelta1(begin); value2=readDelta3(begin+1); begin+=6; break;
		case 43: value1+=readDelta1(begin); value2=readDelta3(begin+1); begin+=7; break;
		case 44: value1+=readDelta1(begin); value2=readDelta3(begin+1); begin+=8; break;
		case 45: value1+=readDelta1(begin); value2=readDelta4(begin+1); begin+=5; break;
		case 46: value1+=readDelta1(begin); value2=readDelta4(begin+1); begin+=6; break;
		case 47: value1+=readDelta1(begin); value2=readDelta4(begin+1); begin+=7; break;
		case 48: value1+=readDelta1(begin); value2=readDelta4(begin+1); begin+=8; break;
		case 49: value1+=readDelta1(begin); value2=readDelta4(begin+1); begin+=9; break;
		case 50: value1+=readDelta2(begin); value2=0; begin+=2; break;
		case 51: value1+=readDelta2(begin); value2=0; begin+=3; break;
		case 52: value1+=readDelta2(begin); value2=0; begin+=4; break;
		case 53: value1+=readDelta2(begin); value2=0; begin+=5; break;
		case 54: value1+=readDelta2(begin); value2=0; begin+=6; break;
		case 55: value1+=readDelta2(begin); value2=readDelta1(begin+2); begin+=3; break;
		case 56: value1+=readDelta2(begin); value2=readDelta1(begin+2); begin+=4; break;
		case 57: value1+=readDelta2(begin); value2=readDelta1(begin+2); begin+=5; break;
		case 58: value1+=readDelta2(begin); value2=readDelta1(begin+2); begin+=6; break;
		case 59: value1+=readDelta2(begin); value2=readDelta1(begin+2); begin+=7; break;
		case 60: value1+=readDelta2(begin); value2=readDelta2(begin+2); begin+=4; break;
		case 61: value1+=readDelta2(begin); value2=readDelta2(begin+2); begin+=5; break;
		case 62: value1+=readDelta2(begin); value2=readDelta2(begin+2); begin+=6; break;
		case 63: value1+=readDelta2(begin); value2=readDelta2(begin+2); begin+=7; break;
		case 64: value1+=readDelta2(begin); value2=readDelta2(begin+2); begin+=8; break;
		case 65: value1+=readDelta2(begin); value2=readDelta3(begin+2); begin+=5; break;
		case 66: value1+=readDelta2(begin); value2=readDelta3(begin+2); begin+=6; break;
		case 67: value1+=readDelta2(begin); value2=readDelta3(begin+2); begin+=7; break;
		case 68: value1+=readDelta2(begin); value2=readDelta3(begin+2); begin+=8; break;
		case 69: value1+=readDelta2(begin); value2=readDelta3(begin+2); begin+=9; break;
		case 70: value1+=readDelta2(begin); value2=readDelta4(begin+2); begin+=6; break;
		case 71: value1+=readDelta2(begin); value2=readDelta4(begin+2); begin+=7; break;
		case 72: value1+=readDelta2(begin); value2=readDelta4(begin+2); begin+=8; break;
		case 73: value1+=readDelta2(begin); value2=readDelta4(begin+2); begin+=9; break;
		case 74: value1+=readDelta2(begin); value2=readDelta4(begin+2); begin+=10; break;
		case 75: value1+=readDelta3(begin); value2=0; begin+=3; break;
		case 76: value1+=readDelta3(begin); value2=0; begin+=4; break;
		case 77: value1+=readDelta3(begin); value2=0; begin+=5; break;
		case 78: value1+=readDelta3(begin); value2=0; begin+=6; break;
		case 79: value1+=readDelta3(begin); value2=0; begin+=7; break;
		case 80: value1+=readDelta3(begin); value2=readDelta1(begin+3); begin+=4; break;
		case 81: value1+=readDelta3(begin); value2=readDelta1(begin+3); begin+=5; break;
		case 82: value1+=readDelta3(begin); value2=readDelta1(begin+3); begin+=6; break;
		case 83: value1+=readDelta3(begin); value2=readDelta1(begin+3); begin+=7; break;
		case 84: value1+=readDelta3(begin); value2=readDelta1(begin+3); begin+=8; break;
		case 85: value1+=readDelta3(begin); value2=readDelta2(begin+3); begin+=5; break;
		case 86: value1+=readDelta3(begin); value2=readDelta2(begin+3); begin+=6; break;
		case 87: value1+=readDelta3(begin); value2=readDelta2(begin+3); begin+=7; break;
		case 88: value1+=readDelta3(begin); value2=readDelta2(begin+3); begin+=8; break;
		case 89: value1+=readDelta3(begin); value2=readDelta2(begin+3); begin+=9; break;
		case 90: value1+=readDelta3(begin); value2=readDelta3(begin+3); begin+=6; break;
		case 91: value1+=readDelta3(begin); value2=readDelta3(begin+3); begin+=7; break;
		case 92: value1+=readDelta3(begin); value2=readDelta3(begin+3); begin+=8; break;
		case 93: value1+=readDelta3(begin); value2=readDelta3(begin+3); begin+=9; break;
		case 94: value1+=readDelta3(begin); value2=readDelta3(begin+3); begin+=10; break;
		case 95: value1+=readDelta3(begin); value2=readDelta4(begin+3); begin+=7; break;
		case 96: value1+=readDelta3(begin); value2=readDelta4(begin+3); begin+=8; break;
		case 97: value1+=readDelta3(begin); value2=readDelta4(begin+3); begin+=9; break;
		case 98: value1+=readDelta3(begin); value2=readDelta4(begin+3); begin+=10; break;
		case 99: value1+=readDelta3(begin); value2=readDelta4(begin+3); begin+=11; break;
		case 100: value1+=readDelta4(begin); value2=0; begin+=4; break;
		case 101: value1+=readDelta4(begin); value2=0; begin+=5; break;
		case 102: value1+=readDelta4(begin); value2=0; begin+=6; break;
		case 103: value1+=readDelta4(begin); value2=0; begin+=7; break;
		case 104: value1+=readDelta4(begin); value2=0; begin+=8; break;
		case 105: value1+=readDelta4(begin); value2=readDelta1(begin+4); begin+=5; break;
		case 106: value1+=readDelta4(begin); value2=readDelta1(begin+4); begin+=6; break;
		case 107: value1+=readDelta4(begin); value2=readDelta1(begin+4); begin+=7; break;
		case 108: value1+=readDelta4(begin); value2=readDelta1(begin+4); begin+=8; break;
		case 109: value1+=readDelta4(begin); value2=readDelta1(begin+4); begin+=9; break;
		case 110: value1+=readDelta4(begin); value2=readDelta2(begin+4); begin+=6; break;
		case 111: value1+=readDelta4(begin); value2=readDelta2(begin+4); begin+=7; break;
		case 112: value1+=readDelta4(begin); value2=readDelta2(begin+4); begin+=8; break;
		case 113: value1+=readDelta4(begin); value2=readDelta2(begin+4); begin+=9; break;
		case 114: value1+=readDelta4(begin); value2=readDelta2(begin+4); begin+=10; break;
		case 115: value1+=readDelta4(begin); value2=readDelta3(begin+4); begin+=7; break;
		case 116: value1+=readDelta4(begin); value2=readDelta3(begin+4); begin+=8; break;
		case 117: value1+=readDelta4(begin); value2=readDelta3(begin+4); begin+=9; break;
		case 118: value1+=readDelta4(begin); value2=readDelta3(begin+4); begin+=10; break;
		case 119: value1+=readDelta4(begin); value2=readDelta3(begin+4); begin+=11; break;
		case 120: value1+=readDelta4(begin); value2=readDelta4(begin+4); begin+=8; break;
		case 121: value1+=readDelta4(begin); value2=readDelta4(begin+4); begin+=9; break;
		case 122: value1+=readDelta4(begin); value2=readDelta4(begin+4); begin+=10; break;
		case 123: value1+=readDelta4(begin); value2=readDelta4(begin+4); begin+=11; break;
		case 124: value1+=readDelta4(begin); value2=readDelta4(begin+4); begin+=12; break;
		}
		(*writer).value1=value1;
		(*writer).value2=value2;
		(*writer).count=count;
		++writer;
	}

	// Update the entries
	pos=triples;
	posLimit=writer;

	return begin;
}

static inline bool greater(unsigned a1,unsigned a2,unsigned b1,unsigned b2) {
   return (a1>b1)||((a1==b1)&&(a2>b2));
}

static inline bool less(unsigned a1, unsigned a2, unsigned b1, unsigned b2) {
	return (a1 < b1) || ((a1 == b1) && (a2 < b2));
}
/*
 * find the first entry >= (value1, value2);
 * pos: the start address of the first triple;
 * posLimit: the end address of last triple;
 */
bool TwoConstantStatisticsBuffer::find(unsigned value1, unsigned value2)
{
	//const Triple* l = pos, *r = posLimit;
	long long left = 0, right = posLimit - pos;
	long long middle;

	while (left != right) {
		middle = left + ((right - left) / 2);
		if (::greater(value1, value2, pos[middle].value1, pos[middle].value2)) {
			left = middle + 1;
		} else if ((!middle) || ::greater(value1, value2, pos[middle - 1].value1, pos[middle -1].value2)) {
			break;
		} else {
			right = middle;
		}
	}

	if(left == right) {
		return false;
	} else {
		pos = &pos[middle];
		return true;
	}
}

bool TwoConstantStatisticsBuffer::findForIndex(unsigned value1, unsigned value2)
{
	//const Triple* l = pos, *r = posLimit;
	long long left = 0, right = posLimitForIndex - posForIndex;
	long long middle;

	while (left != right) {
		middle = left + ((right - left) / 2);
		if (::greater(value1, value2, posForIndex[middle].value1, posForIndex[middle].value2)) {
			left = middle + 1;
		}
		else if ((!middle) || ::greater(value1, value2, posForIndex[middle - 1].value1, posForIndex[middle - 1].value2)) {
			break;
		}
		else {
			right = middle;
		}
	}

	if (left == right) {
		return false;
	}
	else {
		posForIndex = &posForIndex[middle];
		return true;
	}
}

/*
 * find the last entry <= (value1, value2);
 * pos: the start address of the first triple;
 * posLimit: the end address of last triple;
 */
bool TwoConstantStatisticsBuffer::find_last(unsigned value1, unsigned value2)//没有被用到过
{
	//const Triple* l = pos, *r = posLimit;
	long long left = 0, right = posLimit - pos;
	long long middle = 0;

	while (left < right) {
		middle = left + ((right - left) / 2);
		if (::less(value1, value2, pos[middle].value1, pos[middle].value2)) {
			right = middle;
		} else if ((!middle) || ::less(value1, value2, pos[middle + 1].value1, pos[middle + 1].value2)) {
			break;
		} else {
			left = middle + 1;
		}
	}

	if(left == right) {
		return false;
	} else {
		pos = &pos[middle];
		return true;
	}
}

int TwoConstantStatisticsBuffer::findPredicate(unsigned value1, Triple* pos, Triple* posLimit){//没有被用到过
	long long low = 0, high= posLimit - pos,mid;
	while (low <= high) { //当前查找区间R[low..high]非空
		mid = low + ((high - low)/2);
		if (pos[mid].value1 == value1)
			return mid; //查找成功返回
		if (pos[mid].value1 > value1)
			high = mid - 1; //继续在R[low..mid-1]中查�??
		else
			low = mid + 1; //继续在R[mid+1..high]中查�??
	}
	return -1; //当low>high时表示查找区间为空，查找失败

}

Status TwoConstantStatisticsBuffer::getStatis(unsigned& v1, unsigned v2)
{
	posForIndex = index, posLimitForIndex = index + indexPos;
	findForIndex(v1, v2);//先在统计偏移索引里找一遍，找到块索引的偏移
	//这个过程在OneConstantStatisticsBuffer.getStatis中是通过哈希做的，所以函数调用结构有点不一样
	if(::greater(posForIndex->value1, posForIndex->value2, v1, v2))
		posForIndex--;

	unsigned long long start = posForIndex->count; posForIndex++;
	unsigned long long end = posForIndex->count;
	if(posForIndex == (index + indexPos))
		end = usedSpace;

	const unsigned char* begin = (uchar*)buffer->getBuffer() + start, *limit = (uchar*)buffer->getBuffer() + end;
	decode(begin, limit);
	find(v1, v2);//然后再在块索引里找
	if(pos->value1 == v1 && pos->value2 == v2) {
		v1 = pos->count;
		return OK;
	}

	v1 = 0;
	return NOT_FOUND;
}

Status TwoConstantStatisticsBuffer::addStatis(unsigned v1, unsigned v2, unsigned v3)
{
//	static bool first = true;
	unsigned len = 0;

	// otherwise store the compressed value
	if ( v1 == lastId && (v2 - lastPredicate) < 32 && v3 < 5) {
		len = 1;
	} else if(v1 == lastId) {
		len = 1 + getLen(v2 - lastPredicate) + getLen(v3 - 1);
	} else {
		len = 1+ getLen(v1-lastId)+ getLen(v2)+getLen(v3 - 1);
	}

	if ( first == true || ( usedSpace + len ) > buffer->getSize() ) {
		usedSpace = writer - (uchar*)buffer->getBuffer();
		buffer->resize(STATISTICS_BUFFER_INCREMENT_PAGE_COUNT * MemoryBuffer::pagesize);
		writer = (uchar*)buffer->getBuffer() + usedSpace;

		writeUint32(writer, v1); writer += 4;
		writeUint32(writer, v2); writer += 4;
		writeUint32(writer, v3); writer += 4;

		if((indexPos + 1) >= indexSize) {
#ifdef DEBUG
			cout<<"indexPos: "<<indexPos<<" indexSize: "<<indexSize<<endl;
#endif
			index = (TripleIndex*)realloc(index, indexSize * sizeof(TripleIndex) + MemoryBuffer::pagesize * sizeof(TripleIndex));
			if(index==NULL){
				cout<<"StatisticsBuffer realloc fail ! "<<endl;
			}
			indexSize += MemoryBuffer::pagesize;
		}

		index[indexPos].value1 = v1;
		index[indexPos].value2 = v2;
		index[indexPos].count = usedSpace; //record offset，就是因为这里，迫不得已把count也改成了unsigned long long这样的话存储结构就变了，除非存取方式不是按字节
		indexPos++;

		first = false;
	} else {
		//这里块索引压缩存储！
		//和OneConstantStatisticsBuffer一样，TwoConstantStatisticsBuffer.decode也是对块索引进行解压缩
		if (v1 == lastId && v2 - lastPredicate < 32 && v3 < 5) {
			*writer++ = ((v3 - 1) << 5) | (v2 - lastPredicate);
		} else if (v1 == lastId) {
			*writer++ = 0x80 | (getLen(v1 - lastId) * 25 + getLen(v2 - lastPredicate) * 5 + getLen(v3 - 1));
			writer = writeDelta0(writer, v2 - lastPredicate);
			writer = writeDelta0(writer, v3 - 1);
		} else {
			*writer++ = 0x80 | (getLen(v1 - lastId) * 25 + getLen(v2) * 5 + getLen(v3 - 1));
			writer = writeDelta0(writer, v1 - lastId);
			writer = writeDelta0(writer, v2);
			writer = writeDelta0(writer, v3 - 1);
		}
	}

	lastId = v1; lastPredicate = v2;

	usedSpace = writer - (uchar*)buffer->getBuffer();
	return OK;
}

Status TwoConstantStatisticsBuffer::save(MMapBuffer*& indexBuffer)
{
	//同OneConstantStatisticsBuffer.save一样，这里只对统计偏移索引进行磁盘存储，不需要管块索引，因为块索引是mmap机制
	char* writer;//同OneConstantStatisticsBuffer.save一样，这里的writer也不是类成员变量，指的是统计偏移索引内容
	if(indexBuffer == NULL) {
		indexBuffer = MMapBuffer::create(string(string(DATABASE_PATH) + "/statIndex").c_str(), sizeof(usedSpace) + sizeof(indexPos) + indexPos * sizeof(TripleIndex));
		writer = indexBuffer->get_address();
	} else {
		size_t size = indexBuffer->getSize();
		indexBuffer->resize(sizeof(usedSpace) + sizeof(indexPos) + indexPos * sizeof(TripleIndex));
		writer = indexBuffer->get_address() + size;
	}
	//cout << "↓" << endl;
	//cout << indexPos << "\t" << usedSpace << endl;
	//for (int i = 0; i < indexPos;i++) {
	//	cout << (index + i)->value1 << "\t" << (index + i)->value2 << "\t" << (index + i)->count << endl;
	//}
	//cout << "↑" << endl;

	writer = writeData8byte(writer, usedSpace);
	writer = writeData8byte(writer, indexPos);

	//在OneConstantStatisticsBuffer.save里使用的是循环写入
	memcpy(writer, (char*)index, indexPos * sizeof(TripleIndex));//直接利用memcpy的方式把index里的东西存在writer，然后进磁盘(mmap)，这种存取方式不用担心内部存储结构的更改
#ifdef DEBUG
	for(int i = 0; i < 3; i++)
	{
		cout<<index[i].value1<<" : "<<index[i].value2<<" : "<<index[i].count<<endl;
	}

	cout<<"indexPos: "<<indexPos<<endl;
#endif

	return OK;
}

TwoConstantStatisticsBuffer* TwoConstantStatisticsBuffer::load(StatisticsType type, const string path, char*& indexBuffer)
{
	TwoConstantStatisticsBuffer* statBuffer = new TwoConstantStatisticsBuffer(path, type);
	//这里用mmap存取，在构造函数里
	indexBuffer = readData8byte(indexBuffer, statBuffer->usedSpace);
	indexBuffer = readData8byte(indexBuffer, statBuffer->indexPos);
	
#ifdef DEBUG
	cout<<__FUNCTION__<<"indexPos: "<<statBuffer->indexPos<<endl;
#endif
	// load index;
	statBuffer->index = (TripleIndex*)indexBuffer;
	indexBuffer = indexBuffer + statBuffer->indexPos * sizeof(TripleIndex);
	
	//cout << "↓" << endl;
	//cout << statBuffer->indexPos << "\t" << statBuffer->usedSpace << endl;
	//for (int i = 0; i < statBuffer->indexPos;i++) {
	//	cout << statBuffer->index[i].value1 << "\t" << statBuffer->index[i].value2 << "\t" << statBuffer->index[i].count << endl;
	//}
	//cout << "↑" << endl;

#ifdef DEBUG
	for(int i = 0; i < 3; i++)
	{
		cout<<statBuffer->index[i].value1<<" : "<<statBuffer->index[i].value2<<" : "<<statBuffer->index[i].count<<endl;
	}
#endif

	return statBuffer;
}

unsigned TwoConstantStatisticsBuffer::getLen(unsigned v)
{
	if (v>=(1<<24))
		return 4; 
	else if (v>=(1<<16))
		return 3;
	else if (v>=(1<<8)) 
		return 2;
	else if(v > 0)
		return 1;
	else
		return 0;
}

//Status TwoConstantStatisticsBuffer::getPredicatesByID(unsigned id,EntityIDBuffer* entBuffer, ID minID, ID maxID) {//没有被用到过
//	Triple* pos, *posLimit;
//	pos = index;
//	posLimit = index + indexPos;
//	find(id, pos, posLimit);
//	//cout << "findchunk:" << pos->value1 << "  " << pos->value2 << endl;
//	assert(pos >= index && pos < posLimit);
//	Triple* startChunk = pos;
//	Triple* endChunk = pos;
//	while (startChunk->value1 > id && startChunk >= index) {
//		startChunk--;
//	}
//	while (endChunk->value1 <= id && endChunk < posLimit) {
//		endChunk++;
//	}
//
//	const unsigned char* begin, *limit;
//	Triple* chunkIter = startChunk;
//
//	while (chunkIter < endChunk) {
//		//		cout << "------------------------------------------------" << endl;
//		begin = (uchar*) buffer->get_address() + chunkIter->count;
//		//		printf("1: %x  %x  %u\n",begin, buffer->get_address() ,chunkIter->count);
//		chunkIter++;
//		if (chunkIter == index + indexPos)
//			limit = (uchar*) buffer->get_address() + usedSpace;
//		else
//			limit = (uchar*) buffer->get_address() + chunkIter->count;
//		//		printf("2: %x  %x  %u\n",limit, buffer->get_address() ,chunkIter->count);
//
//		Triple* triples = new Triple[3 * MemoryBuffer::pagesize];
//		decode(begin, limit, triples, pos, posLimit);
//
//		int mid = findPredicate(id, pos, posLimit), loc = mid;
//		//		cout << mid << "  " << loc << endl;
//
//
//		if (loc == -1)
//			continue;
//		entBuffer->insertID(pos[loc].value2);
//		//	cout << "result:" << pos[loc].value2<< endl;
//		while (pos[--loc].value1 == id && loc >= 0) {
//			entBuffer->insertID(pos[loc].value2);
//			//			cout << "result:" << pos[loc].value2<< endl;
//		}
//		loc = mid;
//		while (pos[++loc].value1 == id && loc < posLimit - pos) {
//			entBuffer->insertID(pos[loc].value2);
//			//			cout << "result:" << pos[loc].value2<< endl;
//		}
//		delete triples;
//	}
//
//	//	entBuffer->print();
//	return OK;
//}

bool TwoConstantStatisticsBuffer::find(unsigned value1,Triple*& pos,Triple*& posLimit)
{//find by the value1
	//const Triple* l = pos, *r = posLimit;
	long long left = 0, right = posLimit - pos;
//	cout << "right:" << right << endl;
	long long middle=0;

	while (left < right) {
		middle = left + ((right - left) / 2);
//		cout << "first:" << pos[middle].value1 << "  " << value1 << "  "<< pos[middle - 1].value1 << endl;
		if (value1 > pos[middle].value1) {
			left = middle +1;
		} else if ((!middle) || value1 > pos[middle - 1].value1) {
//			cout << "break1:" << pos[middle].value1 << "  " << value1 << "  "<< pos[middle - 1].value1 << endl;
			break;
		} else {
			right = middle;
		}
	}

	if(left == right) {
		pos = &pos[middle];
		return false;
	} else {
		pos = &pos[middle];
//		cout << "pos[middle]:" << pos[middle].value1 << "  " << pos[middle].value2 << endl;
		return true;
	}
}

const uchar* TwoConstantStatisticsBuffer::decode(const uchar* begin, const uchar* end,Triple* triples,Triple* &pos,Triple* &posLimit)
{
//	printf("decode   %x  %x\n",begin,end);
	unsigned value1 = readDelta4(begin); begin += 4;
	unsigned value2 = readDelta4(begin); begin += 4;
	unsigned count = readDelta4(begin); begin += 4;
	Triple* writer = &triples[0];
	(*writer).value1 = value1;
	(*writer).value2 = value2;
	(*writer).count = count;
//	cout << "value1:" << value1 << "   value2:" << value2 << "   count:" << count<<endl;
	++writer;

	// Decompress the remainder of the page
	while (begin < end) {
		// Decode the header byte
		unsigned info = *(begin++);
		// Small gap only?
		if (info < 0x80) {
			if (!info){
				break;
			}
			count = (info >> 5) + 1;
			value2 += (info & 31);
			(*writer).value1 = value1;
			(*writer).value2 = value2;
			(*writer).count = count;
//			cout << "value1:" << value1 << "   value2:" << value2 << "   count:" << count<<endl;
			++writer;
			continue;
		}
	      // Decode the parts
 		switch (info&127) {
		case 0: count=1; break;
		case 1: count=readDelta1(begin)+1; begin+=1; break;
		case 2: count=readDelta2(begin)+1; begin+=2; break;
		case 3: count=readDelta3(begin)+1; begin+=3; break;
		case 4: count=readDelta4(begin)+1; begin+=4; break;
		case 5: value2 += readDelta1(begin); count=1; begin+=1; break;
		case 6: value2 += readDelta1(begin); count=readDelta1(begin+1)+1; begin+=2; break;
		case 7: value2 += readDelta1(begin); count=readDelta2(begin+1)+1; begin+=3; break;
		case 8: value2 += readDelta1(begin); count=readDelta3(begin+1)+1; begin+=4; break;
		case 9: value2 += readDelta1(begin); count=readDelta4(begin+1)+1; begin+=5; break;
		case 10: value2 += readDelta2(begin); count=1; begin+=2; break;
		case 11: value2 += readDelta2(begin); count=readDelta1(begin+2)+1; begin+=3; break;
		case 12: value2 += readDelta2(begin); count=readDelta2(begin+2)+1; begin+=4; break;
		case 13: value2 += readDelta2(begin); count=readDelta3(begin+2)+1; begin+=5; break;
		case 14: value2 += readDelta2(begin); count=readDelta4(begin+2)+1; begin+=6; break;
		case 15: value2 += readDelta3(begin); count=1; begin+=3; break;
		case 16: value2 += readDelta3(begin); count=readDelta1(begin+3)+1; begin+=4; break;
		case 17: value2 += readDelta3(begin); count=readDelta2(begin+3)+1; begin+=5; break;
		case 18: value2 += readDelta3(begin); count=readDelta3(begin+3)+1; begin+=6; break;
		case 19: value2 += readDelta3(begin); count=readDelta4(begin+3)+1; begin+=7; break;
		case 20: value2 += readDelta4(begin); count=1; begin+=4; break;
		case 21: value2 += readDelta4(begin); count=readDelta1(begin+4)+1; begin+=5; break;
		case 22: value2 += readDelta4(begin); count=readDelta2(begin+4)+1; begin+=6; break;
		case 23: value2 += readDelta4(begin); count=readDelta3(begin+4)+1; begin+=7; break;
		case 24: value2 += readDelta4(begin); count=readDelta4(begin+4)+1; begin+=8; break;
		case 25: value1 += readDelta1(begin); value2=0; count=1; begin+=1; break;
		case 26: value1 += readDelta1(begin); value2=0; count=readDelta1(begin+1)+1; begin+=2; break;
		case 27: value1 += readDelta1(begin); value2=0; count=readDelta2(begin+1)+1; begin+=3; break;
		case 28: value1 += readDelta1(begin); value2=0; count=readDelta3(begin+1)+1; begin+=4; break;
		case 29: value1 += readDelta1(begin); value2=0; count=readDelta4(begin+1)+1; begin+=5; break;
		case 30: value1 += readDelta1(begin); value2=readDelta1(begin+1); count=1; begin+=2; break;
		case 31: value1 += readDelta1(begin); value2=readDelta1(begin+1); count=readDelta1(begin+2)+1; begin+=3; break;
		case 32: value1 += readDelta1(begin); value2=readDelta1(begin+1); count=readDelta2(begin+2)+1; begin+=4; break;
		case 33: value1+=readDelta1(begin); value2=readDelta1(begin+1); count=readDelta3(begin+2)+1; begin+=5; break;
		case 34: value1+=readDelta1(begin); value2=readDelta1(begin+1); count=readDelta4(begin+2)+1; begin+=6; break;
		case 35: value1+=readDelta1(begin); value2=readDelta2(begin+1); count=1; begin+=3; break;
		case 36: value1+=readDelta1(begin); value2=readDelta2(begin+1); count=readDelta1(begin+3)+1; begin+=4; break;
		case 37: value1+=readDelta1(begin); value2=readDelta2(begin+1); count=readDelta2(begin+3)+1; begin+=5; break;
		case 38: value1+=readDelta1(begin); value2=readDelta2(begin+1); count=readDelta3(begin+3)+1; begin+=6; break;
		case 39: value1+=readDelta1(begin); value2=readDelta2(begin+1); count=readDelta4(begin+3)+1; begin+=7; break;
		case 40: value1+=readDelta1(begin); value2=readDelta3(begin+1); count=1; begin+=4; break;
		case 41: value1+=readDelta1(begin); value2=readDelta3(begin+1); count=readDelta1(begin+4)+1; begin+=5; break;
		case 42: value1+=readDelta1(begin); value2=readDelta3(begin+1); count=readDelta2(begin+4)+1; begin+=6; break;
		case 43: value1+=readDelta1(begin); value2=readDelta3(begin+1); count=readDelta3(begin+4)+1; begin+=7; break;
		case 44: value1+=readDelta1(begin); value2=readDelta3(begin+1); count=readDelta4(begin+4)+1; begin+=8; break;
		case 45: value1+=readDelta1(begin); value2=readDelta4(begin+1); count=1; begin+=5; break;
		case 46: value1+=readDelta1(begin); value2=readDelta4(begin+1); count=readDelta1(begin+5)+1; begin+=6; break;
		case 47: value1+=readDelta1(begin); value2=readDelta4(begin+1); count=readDelta2(begin+5)+1; begin+=7; break;
		case 48: value1+=readDelta1(begin); value2=readDelta4(begin+1); count=readDelta3(begin+5)+1; begin+=8; break;
		case 49: value1+=readDelta1(begin); value2=readDelta4(begin+1); count=readDelta4(begin+5)+1; begin+=9; break;
		case 50: value1+=readDelta2(begin); value2=0; count=1; begin+=2; break;
		case 51: value1+=readDelta2(begin); value2=0; count=readDelta1(begin+2)+1; begin+=3; break;
		case 52: value1+=readDelta2(begin); value2=0; count=readDelta2(begin+2)+1; begin+=4; break;
		case 53: value1+=readDelta2(begin); value2=0; count=readDelta3(begin+2)+1; begin+=5; break;
		case 54: value1+=readDelta2(begin); value2=0; count=readDelta4(begin+2)+1; begin+=6; break;
		case 55: value1+=readDelta2(begin); value2=readDelta1(begin+2); count=1; begin+=3; break;
		case 56: value1+=readDelta2(begin); value2=readDelta1(begin+2); count=readDelta1(begin+3)+1; begin+=4; break;
		case 57: value1+=readDelta2(begin); value2=readDelta1(begin+2); count=readDelta2(begin+3)+1; begin+=5; break;
		case 58: value1+=readDelta2(begin); value2=readDelta1(begin+2); count=readDelta3(begin+3)+1; begin+=6; break;
		case 59: value1+=readDelta2(begin); value2=readDelta1(begin+2); count=readDelta4(begin+3)+1; begin+=7; break;
		case 60: value1+=readDelta2(begin); value2=readDelta2(begin+2); count=1; begin+=4; break;
		case 61: value1+=readDelta2(begin); value2=readDelta2(begin+2); count=readDelta1(begin+4)+1; begin+=5; break;
		case 62: value1+=readDelta2(begin); value2=readDelta2(begin+2); count=readDelta2(begin+4)+1; begin+=6; break;
		case 63: value1+=readDelta2(begin); value2=readDelta2(begin+2); count=readDelta3(begin+4)+1; begin+=7; break;
		case 64: value1+=readDelta2(begin); value2=readDelta2(begin+2); count=readDelta4(begin+4)+1; begin+=8; break;
		case 65: value1+=readDelta2(begin); value2=readDelta3(begin+2); count=1; begin+=5; break;
		case 66: value1+=readDelta2(begin); value2=readDelta3(begin+2); count=readDelta1(begin+5)+1; begin+=6; break;
		case 67: value1+=readDelta2(begin); value2=readDelta3(begin+2); count=readDelta2(begin+5)+1; begin+=7; break;
		case 68: value1+=readDelta2(begin); value2=readDelta3(begin+2); count=readDelta3(begin+5)+1; begin+=8; break;
		case 69: value1+=readDelta2(begin); value2=readDelta3(begin+2); count=readDelta4(begin+5)+1; begin+=9; break;
		case 70: value1+=readDelta2(begin); value2=readDelta4(begin+2); count=1; begin+=6; break;
		case 71: value1+=readDelta2(begin); value2=readDelta4(begin+2); count=readDelta1(begin+6)+1; begin+=7; break;
		case 72: value1+=readDelta2(begin); value2=readDelta4(begin+2); count=readDelta2(begin+6)+1; begin+=8; break;
		case 73: value1+=readDelta2(begin); value2=readDelta4(begin+2); count=readDelta3(begin+6)+1; begin+=9; break;
		case 74: value1+=readDelta2(begin); value2=readDelta4(begin+2); count=readDelta4(begin+6)+1; begin+=10; break;
		case 75: value1+=readDelta3(begin); value2=0; count=1; begin+=3; break;
		case 76: value1+=readDelta3(begin); value2=0; count=readDelta1(begin+3)+1; begin+=4; break;
		case 77: value1+=readDelta3(begin); value2=0; count=readDelta2(begin+3)+1; begin+=5; break;
		case 78: value1+=readDelta3(begin); value2=0; count=readDelta3(begin+3)+1; begin+=6; break;
		case 79: value1+=readDelta3(begin); value2=0; count=readDelta4(begin+3)+1; begin+=7; break;
		case 80: value1+=readDelta3(begin); value2=readDelta1(begin+3); count=1; begin+=4; break;
		case 81: value1+=readDelta3(begin); value2=readDelta1(begin+3); count=readDelta1(begin+4)+1; begin+=5; break;
		case 82: value1+=readDelta3(begin); value2=readDelta1(begin+3); count=readDelta2(begin+4)+1; begin+=6; break;
		case 83: value1+=readDelta3(begin); value2=readDelta1(begin+3); count=readDelta3(begin+4)+1; begin+=7; break;
		case 84: value1+=readDelta3(begin); value2=readDelta1(begin+3); count=readDelta4(begin+4)+1; begin+=8; break;
		case 85: value1+=readDelta3(begin); value2=readDelta2(begin+3); count=1; begin+=5; break;
		case 86: value1+=readDelta3(begin); value2=readDelta2(begin+3); count=readDelta1(begin+5)+1; begin+=6; break;
		case 87: value1+=readDelta3(begin); value2=readDelta2(begin+3); count=readDelta2(begin+5)+1; begin+=7; break;
		case 88: value1+=readDelta3(begin); value2=readDelta2(begin+3); count=readDelta3(begin+5)+1; begin+=8; break;
		case 89: value1+=readDelta3(begin); value2=readDelta2(begin+3); count=readDelta4(begin+5)+1; begin+=9; break;
		case 90: value1+=readDelta3(begin); value2=readDelta3(begin+3); count=1; begin+=6; break;
		case 91: value1+=readDelta3(begin); value2=readDelta3(begin+3); count=readDelta1(begin+6)+1; begin+=7; break;
		case 92: value1+=readDelta3(begin); value2=readDelta3(begin+3); count=readDelta2(begin+6)+1; begin+=8; break;
		case 93: value1+=readDelta3(begin); value2=readDelta3(begin+3); count=readDelta3(begin+6)+1; begin+=9; break;
		case 94: value1+=readDelta3(begin); value2=readDelta3(begin+3); count=readDelta4(begin+6)+1; begin+=10; break;
		case 95: value1+=readDelta3(begin); value2=readDelta4(begin+3); count=1; begin+=7; break;
		case 96: value1+=readDelta3(begin); value2=readDelta4(begin+3); count=readDelta1(begin+7)+1; begin+=8; break;
		case 97: value1+=readDelta3(begin); value2=readDelta4(begin+3); count=readDelta2(begin+7)+1; begin+=9; break;
		case 98: value1+=readDelta3(begin); value2=readDelta4(begin+3); count=readDelta3(begin+7)+1; begin+=10; break;
		case 99: value1+=readDelta3(begin); value2=readDelta4(begin+3); count=readDelta4(begin+7)+1; begin+=11; break;
		case 100: value1+=readDelta4(begin); value2=0; count=1; begin+=4; break;
		case 101: value1+=readDelta4(begin); value2=0; count=readDelta1(begin+4)+1; begin+=5; break;
		case 102: value1+=readDelta4(begin); value2=0; count=readDelta2(begin+4)+1; begin+=6; break;
		case 103: value1+=readDelta4(begin); value2=0; count=readDelta3(begin+4)+1; begin+=7; break;
		case 104: value1+=readDelta4(begin); value2=0; count=readDelta4(begin+4)+1; begin+=8; break;
		case 105: value1+=readDelta4(begin); value2=readDelta1(begin+4); count=1; begin+=5; break;
		case 106: value1+=readDelta4(begin); value2=readDelta1(begin+4); count=readDelta1(begin+5)+1; begin+=6; break;
		case 107: value1+=readDelta4(begin); value2=readDelta1(begin+4); count=readDelta2(begin+5)+1; begin+=7; break;
		case 108: value1+=readDelta4(begin); value2=readDelta1(begin+4); count=readDelta3(begin+5)+1; begin+=8; break;
		case 109: value1+=readDelta4(begin); value2=readDelta1(begin+4); count=readDelta4(begin+5)+1; begin+=9; break;
		case 110: value1+=readDelta4(begin); value2=readDelta2(begin+4); count=1; begin+=6; break;
		case 111: value1+=readDelta4(begin); value2=readDelta2(begin+4); count=readDelta1(begin+6)+1; begin+=7; break;
		case 112: value1+=readDelta4(begin); value2=readDelta2(begin+4); count=readDelta2(begin+6)+1; begin+=8; break;
		case 113: value1+=readDelta4(begin); value2=readDelta2(begin+4); count=readDelta3(begin+6)+1; begin+=9; break;
		case 114: value1+=readDelta4(begin); value2=readDelta2(begin+4); count=readDelta4(begin+6)+1; begin+=10; break;
		case 115: value1+=readDelta4(begin); value2=readDelta3(begin+4); count=1; begin+=7; break;
		case 116: value1+=readDelta4(begin); value2=readDelta3(begin+4); count=readDelta1(begin+7)+1; begin+=8; break;
		case 117: value1+=readDelta4(begin); value2=readDelta3(begin+4); count=readDelta2(begin+7)+1; begin+=9; break;
		case 118: value1+=readDelta4(begin); value2=readDelta3(begin+4); count=readDelta3(begin+7)+1; begin+=10; break;
		case 119: value1+=readDelta4(begin); value2=readDelta3(begin+4); count=readDelta4(begin+7)+1; begin+=11; break;
		case 120: value1+=readDelta4(begin); value2=readDelta4(begin+4); count=1; begin+=8; break;
		case 121: value1+=readDelta4(begin); value2=readDelta4(begin+4); count=readDelta1(begin+8)+1; begin+=9; break;
		case 122: value1+=readDelta4(begin); value2=readDelta4(begin+4); count=readDelta2(begin+8)+1; begin+=10; break;
		case 123: value1+=readDelta4(begin); value2=readDelta4(begin+4); count=readDelta3(begin+8)+1; begin+=11; break;
		case 124: value1+=readDelta4(begin); value2=readDelta4(begin+4); count=readDelta4(begin+8)+1; begin+=12; break;
		}
		(*writer).value1=value1;
		(*writer).value2=value2;
		(*writer).count=count;
//		cout << "value1:" << value1 << "   value2:" << value2 << "   count:" << count<<endl;
		++writer;
	}

	// Update the entries
	pos=triples;
	posLimit=writer;

	return begin;
}

