/*
 * Tools.h
 *
 *  Created on: 2014-2-11
 *      Author: root
 */

#ifndef TOOLS_H_
#define TOOLS_H_

#include "../TripleBit.h"

class Uncopyable
{
protected:
	Uncopyable() {}
    ~Uncopyable() {}
private:  // emphasize the following members are private
    Uncopyable( const Uncopyable& );
    const Uncopyable& operator=( const Uncopyable& );
 };

class transaction
{
public:
	struct timeval transTime;
	string transInfo;
public:
	transaction(const string& trans):transInfo(trans){
		gettimeofday(&transTime, NULL);
	}
};

class Transaction{
public:
	unsigned arrival;
	unsigned delay;
	unsigned responseTime;

	string queryString;
public:
	Transaction(){}
	Transaction(const string &queryStr):arrival(0),delay(0),responseTime(0),queryString(queryStr){
	}
};

#endif /* TOOLS_H_ */
