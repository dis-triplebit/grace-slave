/*
 * OSFile.cpp
 *
 *  Created on: Oct 8, 2010
 *      Author: root
 */

#include "OSFile.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>

OSFile::OSFile() {
}

OSFile::~OSFile() {
}

bool OSFile::fileExists(const string filename)
{
	struct stat sbuff;
	if( stat(filename.c_str(),&sbuff) == 0 ){
		if( S_ISREG(sbuff.st_mode) )
			return true;
	}
	return false;
}


bool OSFile::directoryExists(const string path)
{
	struct stat sbuff;
	if( stat(path.c_str(),&sbuff) == 0 ){
		cout<<"asdfasf"<<endl;
		if( S_ISDIR(sbuff.st_mode) )
			return true;
	}
	return false;
}

bool OSFile::mkdir(const string path)
{
	return (::mkdir( path.c_str(), S_IRWXU|S_IRWXG|S_IRWXO ) == 0);
}

size_t OSFile::fileSize(std::string filename)
{
	struct stat buf;
	stat(filename.c_str(), &buf);
	return buf.st_size;
}
