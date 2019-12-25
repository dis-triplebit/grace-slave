/*
 * ResultIDBuffer.cpp
 *
 *  Created on: 2019年08月19日
 *      Author: favoniankong
 */

#include "ResultIDBuffer.h"

ResultIDBuffer::ResultIDBuffer(boost::shared_ptr<subTaskPackage> package):isEntityID(false), buffer(NULL), taskPackage(package){
	if(taskPackage->xTempBuffer.size() > 0){
		IDCount = taskPackage->xTempBuffer.begin()->second->getIDCount();
	}
	else{
		IDCount = taskPackage->xyTempBuffer.begin()->second->getIDCount();
	}
}

ResultIDBuffer::~ResultIDBuffer(){
	if(buffer != NULL){
		buffer = NULL;
	}
}

EntityIDBuffer *ResultIDBuffer::getEntityIDBuffer(){
	transForEntityIDBuffer();
	return buffer;
}


void ResultIDBuffer::transForEntityIDBuffer(){
	if(!isEntityID){
		isEntityID = true;
		buffer = taskPackage->getTaskResult();
	}
}

void ResultIDBuffer::getMinMax(ID &min, ID &max){
	transForEntityIDBuffer();
	buffer->getMinMax(min, max);
}

int ResultIDBuffer::getIDCount(){
	return IDCount;
}

void ResultIDBuffer::setTaskPackage(boost::shared_ptr<subTaskPackage> package){
	if(buffer != NULL){
		delete buffer;
		buffer = NULL;
	}
	taskPackage = package;
	isEntityID = false;
}

void ResultIDBuffer::setEntityIDBuffer(EntityIDBuffer *buf){
	buffer = buf;
	isEntityID = true;
}

Status ResultIDBuffer::sort(int sortKey){
	if(isEntityID){
		buffer->sort(sortKey);
	}
	else{
		map<ID, EntityIDBuffer*>::iterator iter, iterEnd;
		iter = taskPackage->xTempBuffer.begin();
		iterEnd = taskPackage->xTempBuffer.end();
		for(; iter != iterEnd; iter++){
			iter->second->sort(sortKey);
		}
		iter = taskPackage->xyTempBuffer.begin();
		iterEnd = taskPackage->xyTempBuffer.end();
		for(; iter != iterEnd; iter++){
			iter->second->sort(sortKey);
		}
	}
	return OK;
}

size_t ResultIDBuffer::getSize(){
	if(isEntityID){
		return buffer->getSize();
	}
	else{
		map<ID, EntityIDBuffer*>::iterator iter;
		size_t totalSize = 0;
		for(iter = taskPackage->xTempBuffer.begin(); iter != taskPackage->xTempBuffer.end(); iter++){
			totalSize += iter->second->getSize();
		}
		for(iter = taskPackage->xyTempBuffer.begin(); iter != taskPackage->xyTempBuffer.end(); iter++){
			totalSize += iter->second->getSize();
		}
		return totalSize;
	}
}

size_t ResultIDBuffer::printMinMax(ID &a,ID &b, ID &c,ID &d){
	ID a_min = UINT_MAX;
	ID a_max = 1;
	ID b_min = UINT_MAX;
	ID b_max = 1;

	map<ID, EntityIDBuffer*>::iterator iter;
	size_t totalSize = 0;
	for(iter = taskPackage->xTempBuffer.begin(); iter != taskPackage->xTempBuffer.end(); iter++){
		a_min = min(a_min,iter->second->range[0].first);
		a_max = max(a_max,iter->second->range[0].second);
		b_min = min(b_min,iter->second->range[1].first);
		b_max = max(b_max,iter->second->range[1].second);
		
		// totalSize += iter->second->getSize();
	}
	for(iter = taskPackage->xyTempBuffer.begin(); iter != taskPackage->xyTempBuffer.end(); iter++){
		a_min = min(a_min,iter->second->range[0].first);
		a_max = max(a_max,iter->second->range[0].second);
		b_min = min(b_min,iter->second->range[1].first);
		b_max = max(b_max,iter->second->range[1].second);
	}

	cout << "a_min " << a_min << " a_max " << a_max << " b_min " << b_min << " b_max " << b_max << endl;
	
	if(a_min != UINT_MAX) a = a_min;
	if(a_max != 1) b = a_max;
	if(b_min != UINT_MAX) c = b_min;
	if(b_max != 1) d = b_max;
	
	// a = a_min;
	// b = a_max;
	// c = b_min;
	// d = b_max;

	return 0;
	// return totalSize;
}





