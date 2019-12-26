/*
 * BuildTripleBit.cpp
 *
 *  Created on: Dec 6, 2019
 *      Author: gao
 */
#include <MemoryBuffer.h>
#include "../TripleBit/TripleBitBuilder.h"
#include "../TripleBit/OSFile.h"
#include "BitmapBuffer.h"
#include "MMapBuffer.h"


static TempFile rawFacts("./test");

void parserTriplesFile(string fileName, TripleBitBuilder* builder)
{
   	std::ifstream  in(fileName);
	std::string str;
	//in.open(fileName);
	while(getline(in, str)) {
		int pos1 = 0, pos2 = str.find("\t",0);
		std::string subject = str.substr(pos1, pos2 - pos1);
		pos1 = pos2 + 1;
		pos2 = str.find("\t", pos1); 
		std::string predicate = str.substr(pos1, pos2 - pos1);
		pos1 = pos2 + 1;
		int len = str.length();
		std::string object;
		if(str[len - 1] == '\r')
			object = str.substr(pos1, len - pos1 - 1);
		else
			object = str.substr(pos1, len - pos1);
		cout << object << "|" << predicate << "|" << subject << endl;
		if(predicate.size() && subject.size() && object.size())
			builder-> NTriplesParse((char*)subject.c_str(), (char*)predicate.c_str(), 
				(char*)object.c_str(), rawFacts);
	}

}

char* DATABASE_PATH;
int main(int argc, char* argv[])
{
	if(argc != 3) {
		fprintf(stderr, "Usage: %s <rawFact> <Database Directory>\n", argv[0]);
		return -1;
	}

	if(!OSFile::directoryExists(argv[2])) {
		OSFile::mkdir(argv[2]);
	}

	DATABASE_PATH = argv[2];
	TripleBitBuilder* builder = new TripleBitBuilder(argv[2]);
	// parserTriplesFile(argv[1], builder);
	

    //slave节点上的输入是rawFact，因此导入只需要调用resolveTriples即可
	//TempFile rawFact(argv[1],ios::app);
	string sosetFileBaseName = "./" + (string)argv[2] + "/soset";
	string psetFileBaseName = "./" + (string)argv[2] + "/pset";
	//TempFile sosetFile("./" + (string)argv[2] + "/soset", ios::trunc);
	//TempFile psetFile("./" + (string)argv[2] + "/pset", ios::trunc);
	//这两个set文件里存的是所有s，p，o的id（单节点的，需要在查询时create里读进内存，然后做验证是否存在用）

	builder->resolveTriples(argv[1], sosetFileBaseName,psetFileBaseName);
	//rawFacts.discard();子节点文件暂时不删除
	
	builder->endBuild();
 	delete builder;

 	return 0;
}
