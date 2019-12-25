/*
 * Timestamp.h
 *
 *  Created on: 2014-1-23
 *      Author: root
 */

#ifndef TIMESTAMP_H_
#define TIMESTAMP_H_

class AvgTime;

class Timestamp{
private:
	// The data
	char data[64];

	friend class AvgTime;

	// Get the raw storage space
	void *ptr() { return data; }
	// Get the raw storage space
	const void *ptr() const { return data; }

public:
	// Contructor
	Timestamp();

	// Hash
	unsigned long long getHash() const { return *static_cast<const unsigned long long*>(ptr()); }

	// Difference in ms
	unsigned operator-(const Timestamp &other) const;
};

class AvgTime{
private:
	// The data
	char data[64];
	// Count
	unsigned count;

	// Get the raw storage space
	void *ptr() { return data; }
	// Get the raw storage space
	const void *ptr() const { return data; }

public:
	// Contructor
	AvgTime();

	// Add an interval
	void add(const Timestamp &start, const Timestamp &end);
	// The avg time in ms
	double avg() const;
};

#endif /* TIMESTAMP_H_ */
