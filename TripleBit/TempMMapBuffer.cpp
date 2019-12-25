/*
 * TempMMapBuffer.cpp
 *
 *  Created on: 2014-1-14
 *      Author: root
 */

#include "TempMMapBuffer.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <stdio.h>
#include "MemoryBuffer.h"

TempMMapBuffer *TempMMapBuffer::instance = NULL;

TempMMapBuffer::TempMMapBuffer(const char *_filename, size_t initSize):filename(_filename){
	fd = open(filename.c_str(), O_CREAT|O_RDWR, 0666);
	if(fd < 0){
		perror(_filename);
		MessageEngine::showMessage("Create tempMap file error", MessageEngine::ERROR);
	}

	size = lseek(fd, 0, SEEK_END);
	if(size < initSize){
		size = initSize;
		if(ftruncate(fd, initSize) != 0){
			perror(_filename);
			MessageEngine::showMessage("ftruncate file error", MessageEngine::ERROR);
		}
	}
	if(lseek(fd, 0, SEEK_SET) != 0){
		perror(_filename);
		MessageEngine::showMessage("lseek file error", MessageEngine::ERROR);
	}

	mmapAddr = (char volatile*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(mmapAddr == MAP_FAILED){
		perror(_filename);
		cout << "size: " << size << endl;
		MessageEngine::showMessage("map file to memory error", MessageEngine::ERROR);
	}

	usedPage = 0;
	pthread_mutex_init(&mutex, NULL);
}

TempMMapBuffer::~TempMMapBuffer(){
	flush();
	munmap((char*)mmapAddr, size);
	close(fd);
	pthread_mutex_destroy(&mutex);
}

Status TempMMapBuffer::flush(){
	if(msync((char*)mmapAddr, size, MS_SYNC) == 0){
		return OK;
	}
	return ERROR;
}

char *TempMMapBuffer::resize(size_t incrementSize){
	size_t newSize = size + incrementSize;

	char *newAddr = NULL;
	if(munmap((char*)mmapAddr, size) != 0){
		MessageEngine::showMessage("resize-munmap error!", MessageEngine::ERROR);
		return NULL;
	}
	if(ftruncate(fd, newSize) != 0){
		MessageEngine::showMessage("resize-ftruncate file error!", MessageEngine::ERROR);
		return NULL;
	}
	if((newAddr = (char*)mmap(NULL, newSize, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, fd, 0)) == (char*)MAP_FAILED){
		MessageEngine::showMessage("mmap buffer resize error!", MessageEngine::ERROR);
		return NULL;
	}
	mmapAddr = (char volatile*)newAddr;
	::memset((char*)mmapAddr + size, 0, incrementSize);

	size = newSize;
	return (char*)mmapAddr;
}

void TempMMapBuffer::discard(){
	munmap((char*)mmapAddr, size);
	close(fd);
	unlink(filename.c_str());
}

char *TempMMapBuffer::getBuffer(){
	return (char*)mmapAddr;
}

char *TempMMapBuffer::getBuffer(int pos){
	return (char*)mmapAddr + pos;
}

Status TempMMapBuffer::resize(size_t newSize, bool clear){
	char *newAddr = NULL;
	if(munmap((char*)mmapAddr, size) != 0 || ftruncate(fd, newSize) != 0 ||
			(newAddr = (char*)mmap(NULL, newSize, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, fd, 0)) == (char*)MAP_FAILED){
		MessageEngine::showMessage("mmap buffer resize error!", MessageEngine::ERROR);
		return ERROR;
	}

	mmapAddr = (char volatile*)newAddr;

	::memset((char*)mmapAddr + size, 0, newSize - size);
	size = newSize;
	return OK;
}

void TempMMapBuffer::memset(char value){
	::memset((char*)mmapAddr, value, size);
}

void TempMMapBuffer::create(const char *filename, size_t initSize = TEMPMMAPBUFFER_INIT_PAGE*MemoryBuffer::pagesize){
//	initSize *= 10;
	instance = new TempMMapBuffer(filename, initSize);
}

TempMMapBuffer &TempMMapBuffer::getInstance(){
	if(instance == NULL){
		perror("instance must not be NULL");
	}
	return *instance;
}

void TempMMapBuffer::deleteInstance(){
	if(instance != NULL){
		instance->discard();
		instance = NULL;
	}
}

char *TempMMapBuffer::getPage(size_t &pageNo){
	pthread_mutex_lock(&mutex);
	char *rt;
	if(usedPage * MemoryBuffer::pagesize >= size){
		resize(size);
	}
	pageNo = usedPage;
	rt = getAddress() + usedPage * MemoryBuffer::pagesize;
	usedPage++;
	pthread_mutex_unlock(&mutex);
	return rt;
}
