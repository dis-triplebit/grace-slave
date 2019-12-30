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
	size_t size;
public:
	char* resize(size_t incrementSize);
	char* getBuffer();
	char* getBuffer(int pos);
	void discard();
	Status flush();
	size_t getSize() { return size;}
	size_t get_length() { return size;}
	char * get_address() const { return (char*)mmap_addr; }

	virtual Status resize(unsigned long	long new_size,bool clear);
	virtual void   memset(char value);

	MMapBuffer(const char* filename, size_t initSize);
	virtual ~MMapBuffer();

public:
	static MMapBuffer* create(const char* filename, size_t initSize);
};

#endif /* MMAPBUFFER_H_ */
