/*
 * Timestamp.cpp
 *
 *  Created on: 2014-1-23
 *      Author: root
 */

#include "Timestamp.h"
#include <sys/time.h>

Timestamp::Timestamp(){
	gettimeofday(static_cast<timeval*>(ptr()), 0);
}

unsigned Timestamp::operator-(const Timestamp &other) const
// Difference in ms
{
	long long a = static_cast<long long>(static_cast<const timeval*>(ptr())->tv_sec) * 1000 + static_cast<const timeval*>(ptr())->tv_usec / 1000;
	long long b = static_cast<long long>(static_cast<const timeval*>(other.ptr())->tv_sec) * 1000 + static_cast<const timeval*>(other.ptr())->tv_usec / 1000;
	return a-b;
}

AvgTime::AvgTime():count(0)
// Contructor
{
	*static_cast<long long*>(ptr()) = 0;
}

void AvgTime::add(const Timestamp &start, const Timestamp &stop){
	long long a = static_cast<long long>(static_cast<const timeval*>(stop.ptr())->tv_sec) * 1000000 + static_cast<const timeval*>(stop.ptr())->tv_usec;
	long long b = static_cast<long long>(static_cast<const timeval*>(start.ptr())->tv_sec) * 1000000 + static_cast<const timeval*>(start.ptr())->tv_usec;
	*static_cast<long long*>(ptr()) += a-b;
	count++;
}

double AvgTime::avg() const
// Average
{
	if(!count) return 0;
	double val;
	val = (*static_cast<const long long*>(ptr()) / 1000);
	val /= count;
	return val;
}
