/*
 * SortMergeJoin.cpp
 *
 *  Created on: Jul 21, 2010
 *      Author: root
 */

#include "SortMergeJoin.h"
#include "../EntityIDBuffer.h"

#define MERGE_DEBUG

SortMergeJoin::SortMergeJoin()
{
	// TODO Auto-generated constructor stub
	//pool = new CThreadPool(THREAD_NUMBER);
	temp1 = (ID*) malloc(4096 * sizeof(ID));
	temp2 = (ID*) malloc(4096 * sizeof(ID));
}

SortMergeJoin::~SortMergeJoin()
{
	// TODO Auto-generated destructor stub
	if (temp1 != NULL)
		free(temp1);
	temp1 = NULL;

	if (temp2 != NULL)
		free(temp2);
	temp2 = NULL;
}

void SortMergeJoin::Join(EntityIDBuffer* entBuffer1, EntityIDBuffer* entBuffer2, int joinKey1, int joinKey2,
		bool secondModify /* = true */)
{
#ifdef MERGE_DEBUG
	cout << "joinKey1: " << joinKey1 << " joinKey2: " << joinKey2 << endl;
#endif

	entBuffer1->sort(joinKey1);
	entBuffer2->sort(joinKey2);

	joinKey1--;
	joinKey2--;

	if (secondModify)
	{
		Merge1(entBuffer1, entBuffer2, joinKey1, joinKey2);
	}
	else
	{
		Merge2(entBuffer1, entBuffer2, joinKey1, joinKey2);
	}
}

void SortMergeJoin::Merge1(EntityIDBuffer* entBuffer1, EntityIDBuffer* entBuffer2, int joinKey1, int joinKey2)
{
#ifdef MERGE_DEBUG
	cout << "before Merge1 entBuffer1 size: " << entBuffer1->getSize() << endl;
	cout << "before Merge1 entBuffer2 size: " << entBuffer2->getSize() << endl;
	size_t count = 0;
#endif
	register size_t i = 0;
	register size_t j = 0;
	register int k;
	register size_t pos1 = 0, pos2 = 0;
	size_t size1 = 0, size2 = 0;
	register ID keyValue;

	ID* buffer1 = entBuffer1->getBuffer();
	ID* buffer2 = entBuffer2->getBuffer();
	size_t length1 = entBuffer1->getSize();
	size_t length2 = entBuffer2->getSize();
	int IDCount1 = entBuffer1->getIDCount();
	int IDCount2 = entBuffer2->getIDCount();

#ifdef MERGE_DEBUG
	cout << "IDCount1: " << IDCount1 << " IDCount2: " << IDCount2 << endl;
#endif

	while (i < length1 && j < length2)
	{
		keyValue = buffer1[i * IDCount1 + joinKey1];

		while (buffer2[j * IDCount2 + joinKey2] < keyValue && j < length2)
		{
			j++;
		}

		if (buffer2[j * IDCount2 + joinKey2] == keyValue)
		{
			while (buffer1[i * IDCount1 + joinKey1] == keyValue && i < length1)
			{
				if (pos1 == 4096)
				{
					memcpy(buffer1 + size1, temp1, 4096 * sizeof(ID));
					size1 = size1 + pos1;
					pos1 = 0;
				}

				for (k = 0; k < IDCount1; k++)
				{
					temp1[pos1] = buffer1[i * IDCount1 + k];
					pos1++;
				}
				i++;
			}

			while (buffer2[j * IDCount2 + joinKey2] == keyValue && j < length2)
			{
#ifdef MERGE_DEBUG
				count++;
#endif
				if (pos2 == 4096)
				{
					memcpy(buffer2 + size2, temp2, 4096 * sizeof(ID));
					size2 = size2 + pos2;
					pos2 = 0;
				}

				for (k = 0; k < IDCount2; k++)
				{
					temp2[pos2] = buffer2[j * IDCount2 + k];
					pos2++;
				}
				j++;
			}
		}
		else
		{
			while (buffer1[i * IDCount1 + joinKey1] == keyValue && i < length1)
			{
				i++;
			}
		}
	}

	memcpy(buffer1 + size1, temp1, pos1 * sizeof(ID));
	size1 = size1 + pos1;
	entBuffer1->usedSize = size1;

	memcpy(buffer2 + size2, temp2, pos2 * sizeof(ID));
	size2 = size2 + pos2;
	entBuffer2->usedSize = size2;

#ifdef MERGE_DEBUG
	cout << "count: " << count << endl;
	cout << "after Merge1 entBuffer1 size: " << entBuffer1->getSize() << endl;
	cout << "after Merge1 entBuffer2 size: " << entBuffer2->getSize() << endl;
#endif
}

void SortMergeJoin::Merge2(EntityIDBuffer* entBuffer1, EntityIDBuffer* entBuffer2, int joinKey1, int joinKey2)
{
#ifdef MERGE_DEBUG
	cout << "before Merge2 entBuffer1 size: " << entBuffer1->getSize() << endl;
	cout << "before Merge2 entBuffer2 size: " << entBuffer2->getSize() << endl;
#endif
	register int i = 0;
	register int j = 0;
	register int k;
	register size_t pos1 = 0;
	size_t size1 = 0;
	register ID keyValue;

	ID* buffer1 = entBuffer1->getBuffer();
	ID* buffer2 = entBuffer2->getBuffer();
	int length1 = entBuffer1->getSize();
	int length2 = entBuffer2->getSize();
	int IDCount1 = entBuffer1->getIDCount();
	int IDCount2 = entBuffer2->getIDCount();

	while (i < length1 && j < length2)
	{
		keyValue = buffer1[i * IDCount1 + joinKey1];

		while (buffer2[j * IDCount2 + joinKey2] < keyValue && j < length2)
		{
			j++;
		}

		if (buffer2[j * IDCount2 + joinKey2] == keyValue)
		{
			while (buffer1[i * IDCount1 + joinKey1] == keyValue && i < length1)
			{
				if (pos1 == 4096)
				{
					memcpy(buffer1 + size1, temp1, 4096 * sizeof(ID));
					size1 = size1 + pos1;
					pos1 = 0;
				}

				for (k = 0; k < IDCount1; k++)
				{
					temp1[pos1] = buffer1[i * IDCount1 + k];
					pos1++;
				}
				i++;
			}

			while (buffer2[j * IDCount2 + joinKey2] == keyValue && j < length2)
			{
				j++;
			}
		}
		else
		{
			while (buffer1[i * IDCount1 + joinKey1] == keyValue && i < length1)
			{
				i++;
			}
		}
	}

	memcpy(buffer1 + size1, temp1, pos1 * sizeof(ID));
	size1 = size1 + pos1;
	entBuffer1->usedSize = size1;

#ifdef MERGE_DEBUG
	cout << "after Merge2 entBuffer1 size: " << entBuffer1->getSize() << endl;
	cout << "after Merge2 entBuffer2 size: " << entBuffer2->getSize() << endl;
#endif
}

void SortMergeJoin::Join(EntityIDBuffer *entBuffer1, ResultIDBuffer *entBuffer2, int joinKey1, int joinKey2, bool secondModify /* = true */){
	if(entBuffer2->isEntityIDBuffer()){
		Join(entBuffer1, entBuffer2->getEntityIDBuffer(), joinKey1, joinKey2, secondModify);
		return;
	}

	entBuffer1->sort(joinKey1);
//	entBuffer2->sort(joinKey2);

	joinKey1--; joinKey2--;

	if(secondModify){
		Merge1(entBuffer1, entBuffer2, joinKey1, joinKey2);
	}
	else{
		Merge2(entBuffer1, entBuffer2, joinKey1, joinKey2);
	}
}

void SortMergeJoin::Merge1(EntityIDBuffer *entBuffer1, ResultIDBuffer *entBuffer2, int joinKey1, int joinKey2){
#ifdef MERGE_DEBUG
	cout << "Before Merge1 entBuffer1 size: " << entBuffer1->getSize() << endl;
	cout << "Before Merge1 entBuffer2 Size: " << entBuffer2->getSize() << endl;
	size_t count = 0;
#endif

	register size_t i = 0, posX = 0, posXY = 0;
	register int k;
	register size_t pos1 = 0;
	size_t size1 = 0;
	register ID keyValue;

	ID *buffer1 = entBuffer1->getBuffer();
	size_t length1 = entBuffer1->getSize();
	int IDCount1 = entBuffer1->getIDCount();
	int IDCount2 = entBuffer2->getIDCount();

#ifdef MERGE_DEBUG
	cout << "IDCount1: " << IDCount1 << " IDCount2: " << IDCount2 << endl;
#endif

	map<ID, EntityIDBuffer*>::iterator iterX, iterXY, iterXEnd, iterXYEnd;
	EntityIDBuffer *buffer = BufferManager::getInstance()->getNewBuffer();
	buffer->empty();
	buffer->setIDCount(IDCount2);
	buffer->setSortKey(0);
	buffer->resizeForSortMergeJoin(entBuffer2->getTaskPackage()->getTotalSize());

	iterX = entBuffer2->getTaskPackage()->xTempBuffer.begin();
	iterXY = entBuffer2->getTaskPackage()->xyTempBuffer.begin();
	iterXEnd = entBuffer2->getTaskPackage()->xTempBuffer.end();
	iterXYEnd = entBuffer2->getTaskPackage()->xyTempBuffer.end();
	ID *bufferX = NULL, *bufferXY = NULL;
	size_t lengthX = 0, lengthXY = 0;
	if(iterX != iterXEnd){
		bufferX = iterX->second->getBuffer();
		lengthX = iterX->second->getSize();
	}
	if(iterXY != iterXYEnd){
		bufferXY = iterXY->second->getBuffer();
		lengthXY = iterXY->second->getSize();
	}
	while(i < length1 && (iterX != iterXEnd || iterXY != iterXYEnd)){
		keyValue = buffer1[i * IDCount1 + joinKey1];

		while(iterXY != iterXYEnd){
			if(posXY == lengthXY){
				delete iterXY->second;
				iterXY->second = NULL;
				iterXY++;
				if(iterXY != iterXYEnd){
					bufferXY = iterXY->second->getBuffer();
					lengthXY = iterXY->second->getSize();
					posXY = 0;
					continue;
				}
				else{
					break;
				}
			}
			if(bufferXY[posXY * IDCount2 + joinKey2] < keyValue){
				posXY++;
			}
			else{
				break;
			}
		}

		while(iterX != iterXEnd){
			if(posX == lengthX){
				delete iterX->second;
				iterX->second = NULL;
				iterX++;
				if(iterX != iterXEnd){
					bufferX = iterX->second->getBuffer();
					lengthX = iterX->second->getSize();
					posX = 0;
					continue;
				}
				else{
					break;
				}
			}
			if(bufferX[posX * IDCount2 + joinKey2] < keyValue){
				posX++;
			}
			else{
				break;
			}
		}

		if((iterXY != iterXYEnd && bufferXY[posXY * IDCount2 + joinKey2] == keyValue) || (iterX != iterXEnd && bufferX[posX * IDCount2 + joinKey2] == keyValue)){
			while(buffer1[i * IDCount1 + joinKey1] == keyValue && i < length1){
				if(pos1 == 4096){
					memcpy(buffer1 + size1, temp1, 4096*sizeof(ID));
					size1 += pos1;
					pos1 = 0;
				}

				for(k = 0; k < IDCount1; k++){
					temp1[pos1] = buffer1[i * IDCount1 + k];
					pos1++;
				}

				i++;
			}

			while(iterXY != iterXYEnd){
				if(posXY == lengthXY){
					delete iterXY->second;
					iterXY->second = NULL;
					iterXY++;
					if(iterXY != iterXYEnd){
						bufferXY = iterXY->second->getBuffer();
						lengthXY = iterXY->second->getSize();
						posXY = 0;
						continue;
					}
					else{
						break;
					}
				}
				if(bufferXY[posXY * IDCount2 + joinKey2] == keyValue){
					for(k = 0; k < IDCount2; k++){
						buffer->insertID(bufferXY[posXY * IDCount2 + k]);
					}
					posXY++;
				}
				else{
					break;
				}
			}

			while(iterX != iterXEnd){
				if(posX == lengthX){
					delete iterX->second;
					iterX->second = NULL;
					iterX++;
					if(iterX != iterXEnd){
						bufferX = iterX->second->getBuffer();
						lengthX = iterX->second->getSize();
						posX = 0;
						continue;
					}
					else{
						break;
					}
				}
				if(bufferX[posX * IDCount2 + joinKey2] == keyValue){
					for(k = 0; k < IDCount2; k++){
						buffer->insertID(bufferX[posX * IDCount2 + k]);
					}
					posX++;
				}
				else{
					break;
				}
			}
		}
		else{
			while(buffer1[i * IDCount1 + joinKey1] == keyValue && i < length1){
				i++;
			}
		}
	}

	memcpy(buffer1 + size1, temp1, pos1 * sizeof(ID));
	size1 += pos1;
	entBuffer1->usedSize = size1;

	while(iterX != iterXEnd){
		delete iterX->second;
		iterX->second = NULL;
		iterX++;
	}
	while(iterXY != iterXYEnd){
		delete iterXY->second;
		iterXY->second = NULL;
		iterXY++;
	}

	entBuffer2->setEntityIDBuffer(buffer);
#ifdef MERGE_DEBUG
	cout << "count: " << count << endl;
	cout << "After Merge1 entBuffer1 size: " << entBuffer1->getSize() << endl;
	cout << "After Merge1 entBuffer2 Size: " << entBuffer2->getSize() << endl;
#endif
}

void SortMergeJoin::Merge2(EntityIDBuffer *entBuffer1, ResultIDBuffer *entBuffer2, int joinKey1, int joinKey2){
#ifdef MERGE_DEBUG
	cout << "Before Merge1 entBuffer1 size: " << entBuffer1->getSize() << endl;
	cout << "Before Merge2 entBuffer2 Size: " << entBuffer2->getSize() << endl;
#endif
	register size_t i = 0, posX = 0, posXY = 0;
	register int k = 0;
	register size_t pos1 = 0;
	size_t size1 = 0;
	register ID keyValue = 0;

	ID *buffer1 = entBuffer1->getBuffer();
	size_t length1 = entBuffer1->getSize();
	int IDCount1 = entBuffer1->getIDCount();
	int IDCount2 = entBuffer2->getIDCount();

	map<ID, EntityIDBuffer*>::iterator iterX, iterXY, iterXEnd, iterXYEnd;
	EntityIDBuffer *buffer = BufferManager::getInstance()->getNewBuffer();
	buffer->empty();
	buffer->setIDCount(IDCount2);
	buffer->setSortKey(0);
	buffer->resizeForSortMergeJoin(entBuffer2->getTaskPackage()->getTotalSize());

	iterX = entBuffer2->getTaskPackage()->xTempBuffer.begin(); iterXEnd = entBuffer2->getTaskPackage()->xTempBuffer.end();
	iterXY = entBuffer2->getTaskPackage()->xyTempBuffer.begin(); iterXYEnd = entBuffer2->getTaskPackage()->xyTempBuffer.end();

	ID *bufferX = NULL, *bufferXY = NULL;
	size_t lengthX = 0, lengthXY = 0;

	if(iterXY != iterXYEnd){
		bufferXY = iterXY->second->getBuffer();
		lengthXY = iterXY->second->getSize();
	}
	if(iterX != iterXEnd){
		bufferX = iterX->second->getBuffer();
		lengthX = iterX->second->getSize();
	}

	while(i < length1 && (iterX != iterXEnd || iterXY != iterXYEnd)){
		keyValue = buffer1[i * IDCount1 + joinKey1];

		while(iterXY != iterXYEnd){
			while(bufferXY[posXY * IDCount2 + joinKey2] < keyValue && posXY < lengthXY){
				for(k = 0; k < IDCount2; k++){
					buffer->insertID(bufferXY[posXY * IDCount2 + k]);
				}
				posXY++;
			}
			if(posXY == lengthXY){
				delete iterXY->second;
				iterXY->second = NULL;
				iterXY++;
				if(iterXY != iterXYEnd){
					bufferXY = iterXY->second->getBuffer();
					lengthXY = iterXY->second->getSize();
					posXY = 0;
				}
				else{
					break;
				}
			}
			else{
				break;
			}
		}

		while(iterX != iterXEnd){
			while(bufferX[posX * IDCount2 + joinKey2] < keyValue && posX < lengthX){
				for(k = 0; k < IDCount2; k++){
					buffer->insertID(bufferX[posX * IDCount2 + k]);
				}
				posX++;
			}
			if(posX == lengthX){
				delete iterX->second;
				iterX->second = NULL;
				iterX++;
				if(iterX != iterXEnd){
					bufferX = iterX->second->getBuffer();
					lengthX = iterX->second->getSize();
					posX = 0;
				}
				else{
					break;
				}
			}
			else{
				break;
			}
		}

		if((iterXY != iterXYEnd && bufferXY[posXY * IDCount2 + joinKey2] == keyValue) || (iterX != iterXEnd && bufferX[posX * IDCount2 + joinKey2] == keyValue)){
			while(buffer1[i * IDCount1 + joinKey1] == keyValue && i < length1){
				if(pos1 == 4096){
					memcpy(buffer1 + size1, temp1, 4096 * sizeof(ID));
					size1 += pos1;
					pos1 = 0;
				}

				for(k = 0; k < IDCount1; k++){
					temp1[pos1] = buffer1[i * IDCount1 + k];
					pos1++;
				}

				i++;
			}

			while(iterXY != iterXYEnd){
				while(bufferXY[posXY * IDCount2 + joinKey2] == keyValue && posXY < lengthXY){
					for(k = 0; k < IDCount2; k++){
						buffer->insertID(bufferXY[posXY * IDCount2 + k]);
					}
					posXY++;
				}
				if(posXY == lengthXY){
					delete iterXY->second;
					iterXY->second = NULL;
					iterXY++;
					if(iterXY != iterXYEnd){
						bufferXY = iterXY->second->getBuffer();
						lengthXY = iterXY->second->getSize();
						posXY = 0;
					}
					else{
						break;
					}
				}
				else{
					break;
				}
			}

			while(iterX != iterXEnd){
				while(bufferX[posX * IDCount2 + joinKey2] == keyValue && posX < lengthX){
					for(k = 0; k < IDCount2; k++){
						buffer->insertID(bufferX[posX * IDCount2 + k]);
					}
					posX++;
				}
				if(posX == lengthX){
					delete iterX->second;
					iterX->second = NULL;
					iterX++;
					if(iterX != iterXEnd){
						bufferX = iterX->second->getBuffer();
						lengthX = iterX->second->getSize();
						posX = 0;
					}
					else{
						break;
					}
				}
				else{
					break;
				}
			}
		}
		else{
			while(buffer1[i * IDCount1 + joinKey1] == keyValue && i < length1){
				i++;
			}
		}
	}

	memcpy(buffer1 + size1, temp1, pos1 * sizeof(ID));
	size1 += pos1;
	entBuffer1->usedSize = size1;

	if(posXY == lengthXY && iterXY != iterXYEnd){
		delete iterXY->second;
		iterXY->second = NULL;
		iterXY++;
		if(iterXY != iterXYEnd){
			bufferXY = iterXY->second->getBuffer();
			lengthXY = iterXY->second->getSize();
			posXY = 0;
		}
	}
	if(posX == lengthX && iterX != iterXEnd){
		delete iterX->second;
		iterX->second = NULL;
		iterX++;
		if(iterX != iterXEnd){
			bufferX = iterX->second->getBuffer();
			lengthX = iterX->second->getSize();
			posX = 0;
		}
	}

	register ID keyValueX, keyValueXY;
	while(iterX != iterXEnd && iterXY != iterXYEnd){
		keyValueXY = bufferXY[posXY * IDCount2 + joinKey2];
		keyValueX = bufferX[posX * IDCount2 + joinKey2];

		if(keyValueXY <= keyValueX){
			while(iterXY != iterXYEnd){
				while(bufferXY[posXY * IDCount2 + joinKey2] <= keyValueX && posXY < lengthXY){
					for(k = 0; k < IDCount2; k++){
						buffer->insertID(bufferXY[posXY * IDCount2 + k]);
					}
					posXY++;
				}
				if(posXY == lengthXY){
					delete iterXY->second;
					iterXY->second = NULL;
					iterXY++;
					if(iterXY != iterXYEnd){
						bufferXY = iterXY->second->getBuffer();
						lengthXY = iterXY->second->getSize();
						posXY = 0;
					}
					else{
						break;
					}
				}
				else{
					break;
				}
			}
		}

		while(iterX != iterXEnd){
			while(bufferX[posX * IDCount2 + joinKey2] == keyValueX && posX < lengthX){
				for(k = 0; k < IDCount2; k++){
					buffer->insertID(bufferX[posX * IDCount2 + k]);
				}
				posX++;
			}
			if(posX == lengthX){
				delete iterX->second;
				iterX->second = NULL;
				iterX++;
				if(iterX != iterXEnd){
					bufferX = iterX->second->getBuffer();
					lengthX = iterX->second->getSize();
					posX = 0;
				}
				else{
					break;
				}
			}
			else{
				break;
			}
		}
	}

	while(iterXY != iterXYEnd){
		while(posXY < lengthXY){
			for(k = 0; k < IDCount2; k++){
				buffer->insertID(bufferXY[posXY * IDCount2 + k]);
			}
			posXY++;
		}
		delete iterXY->second;
		iterXY->second = NULL;
		iterXY++;
		if(iterXY != iterXYEnd){
			bufferXY = iterXY->second->getBuffer();
			lengthXY = iterXY->second->getSize();
			posXY = 0;
		}
	}

	while(iterX != iterXEnd){
		while(posX < lengthX){
			for(k = 0; k < IDCount2; k++){
				buffer->insertID(bufferX[posX * IDCount2 + k]);
			}
			posX++;
		}
		delete iterX->second;
		iterX->second = NULL;
		iterX++;
		if(iterX != iterXEnd){
			bufferX = iterX->second->getBuffer();
			lengthX = iterX->second->getSize();
			posX = 0;
		}
	}

	entBuffer2->setEntityIDBuffer(buffer);
#ifdef MERGE_DEBUG
	cout << "After Merge2 entBuffer1 size: " << entBuffer1->getSize() << endl;
	cout << "After Merge2 entBuffer2 Size: " << entBuffer2->getSize() << endl;
#endif
}

