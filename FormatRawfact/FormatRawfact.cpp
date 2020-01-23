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
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <rawFact> <FormatType>\n" \
			"FormatType : 0 -> rawfact to id triples file\n" \
			"FormatType : 1 -> id triples file to rawfact", argv[0]);
		return -1;
	}

	int formatType = stoi(argv[2]);
	if(formatType==0){//rawfact转ID三元组文件
		string filename(argv[1]);

		MemoryMappedFile mappedIn;
		cout << "formatTempFile -" << filename << endl;
		assert(mappedIn.open(filename.c_str()));
		ID s, p, o;

		const char* reader = mappedIn.getBegin(), * limit = mappedIn.getEnd();

		ofstream outFile(filename + ".ff");
		for (int i = 0; reader < limit; i++) {
			s = *(ID*)reader;
			reader += sizeof(ID);
			p = *(ID*)reader;
			reader += sizeof(ID);
			o = *(ID*)reader;
			reader += sizeof(ID);
			outFile << "<" << s << ">" << " "
				<< "<" << p << ">" << " "
				<< "\"" << o << "\"" << " ." << endl;
			//if (i > 10000) break;//小文件不需要这一行！
		}
		outFile.close();
	}else{//ID三元组文件转rawFact文件
		string filename(argv[1]);
		TempFile rawFact(filename + ".rawfact", ios::trunc);
		ID s, p, o;
		cout << filename << " to rawfact :" << filename << ".rawfact" << endl;
		std::ifstream in(filename);
		std::string str;
		while(getline(in,str)){
			int pos1, pos2;
			pos1 = str.find("<", 0);
			pos2 = str.find(">", pos1);
			std::string subject = str.substr(pos1 + 1, pos2 - pos1 - 1);
			pos1 = str.find("<", pos2);
			pos2 = str.find(">", pos1);
			std::string predicate = str.substr(pos1 + 1, pos2 - pos1 - 1);
			pos1 = str.find("\"", pos2);
			pos2 = str.find("\"", pos1 + 1);
			std::string object = str.substr(pos1 + 1, pos2 - pos1 - 1);
			s = stoul(subject);
			p = stoul(predicate);
			o = stoul(object);
			//cout << s << "\t" << p << "\t" << o << endl;
			rawFact.writeId(s);
			rawFact.writeId(p);
			rawFact.writeId(o);
		}
		rawFact.close();
		in.close();
	}

	return 0;
}
