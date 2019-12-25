/*
 * IndexForTT.h
 *
 *  Created on: 2019年03月19日
 *      Author: favoniankong
 */

#ifndef INDEXFORTT_H_
#define INDEXFORTT_H_

#include "../TripleBit.h"

using namespace std;
using namespace boost;

class IndexForTT{
private:
	size_t referenceCount;

	pthread_mutex_t mutex;
	pthread_cond_t cond;

public:
	IndexForTT():referenceCount(0){
		pthread_mutex_init(&mutex, NULL);
		pthread_cond_init(&cond, NULL);
	}
	IndexForTT(size_t reCount): referenceCount(reCount){
		pthread_mutex_init(&mutex, NULL);
		pthread_cond_init(&cond, NULL);
	}

	~IndexForTT(){
		pthread_mutex_destroy(&mutex);
		pthread_cond_destroy(&cond);
	}

	void completeOneTriple(){
		pthread_mutex_lock(&mutex);
		referenceCount--;
		if(referenceCount == 0){
			pthread_cond_broadcast(&cond);
		}
		pthread_mutex_unlock(&mutex);
	}

	void resumeReference(){
		pthread_mutex_lock(&mutex);
		referenceCount++;
		pthread_mutex_unlock(&mutex);
	}
	
	void wait(){
		pthread_mutex_lock(&mutex);
		while(referenceCount != 0){
			pthread_cond_wait(&cond, &mutex);
		}
		pthread_mutex_unlock(&mutex);
	}
};


#endif /* INDEXFORTT_H_ */
