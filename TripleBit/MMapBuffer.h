/*
 * MMapBuffer.h
 *
 *  Created on: Oct 6, 2010
 *      Author: root
 */

#ifndef MMAPBUFFER_H_
#define MMAPBUFFER_H_

#include "TripleBit.h"

//#define VOLATILE   
class MMapBuffer {
	int fd;
	char volatile* mmap_addr;
	char* curretHead;
	string filename;
	//size_t size;
	unsigned long long size;//之前是size_t，改成longlong试试
public:
	char* resize(size_t incrementSize);
	char* getBuffer();
	char* getBuffer(int pos);
	void discard();
	Status flush();
	unsigned long long getSize() { return size;}
	unsigned long long get_length() { return size;}
	char * get_address() const { return (char*)mmap_addr; }

	virtual Status resize(size_t new_size,bool clear);
	virtual void   memset(char value);

	MMapBuffer(const char* filename, size_t initSize);
	virtual ~MMapBuffer();

public:
	static MMapBuffer* create(const char* filename, size_t initSize);
};

#endif /* MMAPBUFFER_H_ */
