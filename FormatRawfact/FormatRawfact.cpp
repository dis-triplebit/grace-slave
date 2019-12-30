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

char* DATABASE_PATH;
int main(int argc, char* argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <rawFact>\n", argv[0]);
		return -1;
	}

	string filename(argv[1]);

	MemoryMappedFile mappedIn;
	cout << "formatTempFile -" << filename << endl;
	assert(mappedIn.open(filename.c_str()));
	ID s, p, o;

	const char* reader = mappedIn.getBegin(), * limit = mappedIn.getEnd();

	ofstream outFile(filename + ".ff");
	for (int i=0; reader < limit;i++) {
		s = *(ID*)reader;
		reader += sizeof(ID);
		p = *(ID*)reader;
		reader += sizeof(ID);
		o = *(ID*)reader;
		reader += sizeof(ID);
		outFile << "<" << s << ">" << " "
			<< "<" << p << ">" << " "
			<< "\"" << o << "\"" << " ." << endl;
		if (i > 10000) break;
	}
	outFile.close();
	

	//TripleBitBuilder* builder = new TripleBitBuilder(argv[2]);

	//slave节点上的输入是rawFact，因此导入只需要调用resolveTriples即可
	//TempFile rawFact(argv[1],ios::app);
	//string sosetFileBaseName = "./" + (string)argv[2] + "/soset";
	//string psetFileBaseName = "./" + (string)argv[2] + "/pset";
	//TempFile sosetFile("./" + (string)argv[2] + "/soset", ios::trunc);
	//TempFile psetFile("./" + (string)argv[2] + "/pset", ios::trunc);
	//这两个set文件里存的是所有s，p，o的id（单节点的，需要在查询时create里读进内存，然后做验证是否存在用）

	//builder->resolveTriples(argv[1], sosetFileBaseName, psetFileBaseName);
	//rawFacts.discard();子节点文件暂时不删除

	//builder->endBuild();
	//delete builder;

	return 0;
}
