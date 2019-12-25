/*
 * subTaskPackage.h
 *
 *  Created on: 2019年09月12日
 *      Author: favoniankong
 */

#ifndef SUBTASKPACKAGE_H_
#define SUBTASKPACKAGE_H_

#include "../TripleBit.h"
#include "../TripleBitQueryGraph.h"
#include "../util/BufferManager.h"
#include "../util/PartitionBufferManager.h"

using namespace std;
using namespace boost;

class subTaskPackage {
public:
	size_t referenceCount;
	TripleBitQueryGraph::OpType operationType;
	ID sourceWorkerID;
	ID minID;
	ID maxID;
	ID deleteID;
	ID updateID;
	map<ID, EntityIDBuffer*> xTempBuffer;
	map<ID, EntityIDBuffer*> xyTempBuffer;

	PartitionBufferManager* partitionBuffer;
	pthread_mutex_t subTaskMutex;

public:
	subTaskPackage(){}
	subTaskPackage(size_t reCount, TripleBitQueryGraph::OpType opType, ID sourceID, ID minid, ID maxid,
			ID deleteid, ID updateid, PartitionBufferManager*& buffer) :
			referenceCount(reCount), operationType(opType), sourceWorkerID(sourceID), minID(minid), maxID(maxid), deleteID(
					deleteid), updateID(updateid), partitionBuffer(buffer)
	{
		pthread_mutex_init(&subTaskMutex, NULL);
	}
	~subTaskPackage(){
		xTempBuffer.clear();
		xyTempBuffer.clear();
		pthread_mutex_destroy(&subTaskMutex);
	}

	bool completeSubTask(ID chunkID, EntityIDBuffer* result, unsigned char index){
		pthread_mutex_lock(&subTaskMutex);
		if(index == 1)
		{
			xTempBuffer[chunkID] = result;
			referenceCount--;
		}
		else if(index == 2)
		{
			xyTempBuffer[chunkID] = result;
			referenceCount--;
		}
		if(referenceCount == 0){
			pthread_mutex_unlock(&subTaskMutex);
			return true;
		}
		else{
			pthread_mutex_unlock(&subTaskMutex);
			return false;
		}
	}

	size_t getTotalSize(){
		map<ID, EntityIDBuffer*>::iterator iter;
		size_t totalSize = 0;
		for(iter = xTempBuffer.begin(); iter!= xTempBuffer.end(); iter++){
			totalSize += iter->second->getUsedSize();
		}
		for(iter = xyTempBuffer.begin(); iter != xyTempBuffer.end(); iter++){
			totalSize += iter->second->getUsedSize();
		}
		return totalSize;
	}

	void getAppendBuffer(int beg, int end, int index, EntityIDBuffer *buffer){
		if(index == 1){
			for(int i = beg; i < end; i++){
				buffer->appendBuffer(xTempBuffer[i]);
			}
		}
		else{
			for(int i = beg; i < end; i++){
				buffer->appendBuffer(xyTempBuffer[i]);
			}
		}
	}

	EntityIDBuffer *getTaskResult(){
		EntityIDBuffer *resultXBuffer = new EntityIDBuffer;
		EntityIDBuffer *resultXYBuffer = new EntityIDBuffer;
		resultXBuffer->empty(); resultXYBuffer->empty();

		map<ID, EntityIDBuffer*>::iterator iter = xTempBuffer.begin();
		if(iter != xTempBuffer.end()){
			resultXBuffer->setIDCount(iter->second->getIDCount());
			resultXBuffer->setSortKey(iter->second->getSortKey());
		}
		size_t totalSize = 0;
		for(iter = xTempBuffer.begin(); iter != xTempBuffer.end(); iter++){
			totalSize += iter->second->getUsedSize();
		}
		resultXBuffer->resize(totalSize);
		for(iter = xTempBuffer.begin(); iter != xTempBuffer.end(); iter++){
			resultXBuffer->appendBuffer(iter->second);
			delete iter->second;
			iter->second = NULL;
		}

		iter = xyTempBuffer.begin();
		if(iter != xyTempBuffer.end()){
			//mistakes have been removed
			resultXYBuffer->setIDCount(iter->second->getIDCount());
			resultXYBuffer->setSortKey(iter->second->getSortKey());
		}
		totalSize = 0;
		for(iter = xyTempBuffer.begin(); iter != xyTempBuffer.end(); iter++){
			totalSize += iter->second->getUsedSize();
		}
		resultXYBuffer->resize(totalSize);
		for(iter = xyTempBuffer.begin(); iter != xyTempBuffer.end(); iter++){
			resultXYBuffer->appendBuffer(iter->second);
			delete iter->second;
			iter->second = NULL;
		}

		EntityIDBuffer* resultBuf = NULL;
		if(operationType == TripleBitQueryGraph::QUERY){
			resultBuf = BufferManager::getInstance()->getNewBuffer();
		}
		else{
			resultBuf = partitionBuffer->getNewBuffer();
		}
		resultBuf->empty();
		resultBuf->setSortKey(0);
		if(resultXBuffer->getIDCount() == 1 && resultXYBuffer->getIDCount() == 1){
			resultBuf->setIDCount(1);
		}
		else{
			resultBuf->setIDCount(2);
		}
		resultBuf->mergeBuffer(resultXBuffer, resultXYBuffer);

		delete resultXBuffer;
		delete resultXYBuffer;
		return resultBuf;
	}

	bool isRight(){
		int IDCount = 1;
		if(xTempBuffer.size() > 0){
			IDCount = xTempBuffer.begin()->second->getIDCount();
		}
		else if(xyTempBuffer.size() > 0){
			IDCount = xyTempBuffer.begin()->second->getIDCount();
		}
		else{
			return false;
		}

		map<ID, EntityIDBuffer*>::iterator iterX, iterXEnd, iterXY, iterXYEnd;
		iterX = xTempBuffer.begin(); 	iterXEnd = xTempBuffer.end();
		iterXY = xyTempBuffer.begin();	iterXYEnd = xyTempBuffer.end();

		register size_t posX = 0, posXY = 0;
		register size_t lenX = 0, lenXY = 0;
		ID *bufferX, *bufferXY;

		if(iterXY != iterXYEnd){
			bufferXY = iterXY->second->getBuffer();
			lenXY = iterXY->second->getSize();
		}
		if(iterX != iterXEnd){
			bufferX = iterX->second->getBuffer();
			lenX = iterX->second->getSize();
		}

		if(IDCount == 1){
			ID key, idX, idXY;
			key = 0;
			while(iterX != iterXEnd && iterXY != iterXYEnd){
				if(posXY == lenXY){
					iterXY++;
					if(iterXY != iterXYEnd){
						bufferXY = iterXY->second->getBuffer();
						lenXY = iterXY->second->getSize();
						posXY = 0;
						continue;
					}
					else{
						break;
					}
				}
				if(posX == lenX){
					iterX++;
					if(iterX != iterXEnd){
						bufferX = iterX->second->getBuffer();
						lenX = iterX->second->getSize();
						posX = 0;
						continue;
					}
					else{
						break;
					}
				}
				idXY = bufferXY[posXY];
				idX = bufferX[posX];
				if(idXY < key || idX < key){
					return false;
				}
				else{
					key = idXY < idX ? idXY : idX;
				}
				if(idXY < idX){
					posXY++;
				}
				else{
					posX++;
				}
			}
			while(iterX != iterXEnd){
				if(posX == lenX){
					iterX++;
					if(iterX != iterXEnd){
						bufferX = iterX->second->getBuffer();
						lenX = iterX->second->getSize();
						posX = 0;
						continue;
					}
					else{
						break;
					}
				}
				idX = bufferX[posX];
				if(idX < key){
					return false;
				}
				else key = idX;

				posX++;
			}
			while(iterXY != iterXYEnd){
				if(posXY == lenXY){
					iterXY++;
					if(iterXY != iterXYEnd){
						bufferXY = iterXY->second->getBuffer();
						lenXY = iterXY->second->getSize();
						posXY = 0;
						continue;
					}
					else{
						break;
					}
				}
				idXY = bufferXY[posXY];
				if(idXY < key){
					return false;
				}
				else key = idXY;

				posXY++;
			}
		}
		else if(IDCount == 2){
			ID keyX, keyY, idX1, idY1, idX2, idY2;
			keyX = 0; keyY = 0;

			while(iterX != iterXEnd && iterXY != iterXYEnd){
				if(posX == lenX){
					iterX++;
					if(iterX != iterXEnd){
						bufferX = iterX->second->getBuffer();
						lenX = iterX->second->getSize();
						posX = 0;
						continue;
					}
					else{
						break;
					}
				}
				if(posXY == lenXY){
					iterXY++;
					if(iterXY != iterXYEnd){
						bufferXY = iterXY->second->getBuffer();
						lenXY = iterXY->second->getSize();
						posXY = 0;
						continue;
					}
					else{
						break;
					}
				}
				idX1 = bufferX[posX * IDCount]; idY1 = bufferX[posX * IDCount + 1];
				idX2 = bufferXY[posXY * IDCount]; idY2 = bufferXY[posXY * IDCount + 1];
				if((idX1 < keyX || (idX1 == keyX && idY1 < keyY)) || (idX2 < keyX || (idX2 == keyX && idY2 < keyY))){
					return false;
				}
				if(idX1 < idX2 || (idX1 == idX2 && idY1 < idY2)){
					keyX = idX1; keyY = idY1;
					posX++;
				}
				else{
					keyX = idX2; keyY = idY2;
					posXY++;
				}
			}
			while(iterXY != iterXYEnd){
				if(posXY == lenXY){
					iterXY++;
					if(iterXY != iterXYEnd){
						bufferXY = iterXY->second->getBuffer();
						lenXY = iterXY->second->getSize();
						posXY = 0;
						continue;
					}
					else{
						break;
					}
				}
				idX2 = bufferXY[posXY * IDCount]; idY2 = bufferXY[posXY * IDCount + 1];
				if(idX2 < keyX || (idX2 == keyX && idY2 < keyY)){
					return false;
				}
				else{
					keyX = idX2; keyY = idY2;
				}
				posXY++;
			}
			while(iterX != iterXEnd){
				if(posX == lenX){
					iterX++;
					if(iterX != iterXEnd){
						bufferX = iterX->second->getBuffer();
						lenX = iterX->second->getSize();
						posX = 0;
						continue;
					}
					else{
						break;
					}
				}
				idX1 = bufferX[posX * IDCount]; idY1 = bufferX[posX * IDCount + 1];
				if(idX1 < keyX || (idX1 == keyX && idY1 < keyY)){
					return false;
				}
				else{
					keyX = idX1; keyY = idY1;
				}
				posX++;
			}
		}
		return true;
	}

	void printEntityIDBuffer(EntityIDBuffer *buffer){
		size_t size = buffer->getSize();
		int count = 0;
		ID *idBuffer = buffer->getBuffer();
		int IDCount = buffer->getIDCount();


		for(size_t i = 0; i < size; i++){
			if(IDCount == 1){
				cout << "ID:" << idBuffer[i] << ' ';
			}
			else if(IDCount == 2){
				cout << "ID:" << idBuffer[i * IDCount] << " ID:" << idBuffer[i*IDCount+1] << " ";
			}
			count++;
			if(count % 10 == 0)
				cout << endl;
		}
		cout << endl;
	}

	void print(){
		map<ID, EntityIDBuffer*>::iterator iter;
		int k = 0;
		for(iter = xTempBuffer.begin(); iter != xTempBuffer.end(); iter++){
			cout << "xTempBuffer " << k++ << " :" << endl;
			printEntityIDBuffer(iter->second);
		}
		for(iter = xyTempBuffer.begin(), k = 0; iter != xyTempBuffer.end(); iter++, k++){
			cout << "xyTempBuffer " << k << " :" << endl;
			printEntityIDBuffer(iter->second);
		}
	}
};

#endif /* SUBTASKPACKAGE_H_ */
