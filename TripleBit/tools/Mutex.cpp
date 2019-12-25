/*
 * Mutex.cpp
 *
 *  Created on: 2014-3-5
 *      Author: root
 */

#include "Mutex.h"

Mutex::Mutex(){
//	Constructor
	pthread_mutex_init(&mutex, 0);
}

Mutex::~Mutex(){
//	Destructor
	pthread_mutex_destroy(&mutex);
}

void Mutex::lock(){
//	Lock
	pthread_mutex_lock(&mutex);
}

bool Mutex::tryLock(){
//	Try to lock
	return pthread_mutex_trylock(&mutex)==0;
}

void Mutex::unlock(){
//	Unlock
	pthread_mutex_unlock(&mutex);
}

