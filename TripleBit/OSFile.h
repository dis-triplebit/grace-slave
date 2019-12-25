/*
 * OSFile.h
 *
 *  Created on: Oct 8, 2010
 *      Author: root
 */

#ifndef OSFILE_H_
#define OSFILE_H_

#include "TripleBit.h"

class OSFile {
public:
	OSFile();
	virtual ~OSFile();
	static bool fileExists(const string filename);
	static bool directoryExists(const string dir);
	static bool mkdir(const string dir);
	static size_t fileSize(const string filename);
};

#endif /* OSFILE_H_ */
