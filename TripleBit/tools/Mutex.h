/*
 * Mutex.h
 *
 *  Created on: 2014-3-5
 *      Author: root
 */

#ifndef MUTEX_H_
#define MUTEX_H_

#include <pthread.h>

class Mutex
{
private:
	pthread_mutex_t mutex;

	Mutex(const Mutex&);
	void operator=(const Mutex&);

public:
//	Constructor
	Mutex();
//	Destructor
	~Mutex();

//	Lock the mutex
	void lock();
//	Try to lock the mutex
	bool tryLock();
//	Unlock the mutex
	void unlock();
};


//Locker object
class auto_lock{
private:
	Mutex &lock;

	auto_lock(const auto_lock&);
	void operator=(const auto_lock&);

public:
	auto_lock(Mutex& l):lock(l) { l.lock(); }
	~auto_lock() { lock.unlock(); }
};
#endif /* MUTEX_H_ */
