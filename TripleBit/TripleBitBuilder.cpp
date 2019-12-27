/*
 * TripleBitBuilder.cpp
 *
 *  Created on: Apr 6, 2010
 *      Author: root
 */

#include "MemoryBuffer.h"
#include "MMapBuffer.h"
#include "BitmapBuffer.h"
#include "TripleBitBuilder.h"
#include "PredicateTable.h"
#include "TripleBit.h"
#include "URITable.h"
#include "Sorter.h"
#include "StatisticsBuffer.h"


#include <string.h>
#include <pthread.h>

static int getCharPos(const char* data, char ch)
{
	const char * p = data;
	int i = 0;
	while(*p != '\0'){
		if( *p == ch)
			return i+1;
		p++;
		i++;
	}

	return -1;
}

TripleBitBuilder::TripleBitBuilder(string _dir) : dir(_dir) {
	// TODO Auto-generated constructor stub
	preTable = new PredicateTable(dir);
	uriTable = new URITable(dir);
	bitmap = new BitmapBuffer(dir);

	statBuffer[0] = new OneConstantStatisticsBuffer(string(dir + "/subject_statis"), StatisticsBuffer::SUBJECT_STATIS);			//subject statistics buffer;
	statBuffer[1] = new OneConstantStatisticsBuffer(string(dir + "/object_statis"), StatisticsBuffer::OBJECT_STATIS);			//object statistics buffer;
	statBuffer[2] = new TwoConstantStatisticsBuffer(string(dir + "/subjectpredicate_statis"), StatisticsBuffer::SUBJECTPREDICATE_STATIS);	//subject-predicate statistics buffer;
	statBuffer[3] = new TwoConstantStatisticsBuffer(string(dir + "/objectpredicate_statis"), StatisticsBuffer::OBJECTPREDICATE_STATIS);	//object-predicate statistics buffer;

	staReifTable = new StatementReificationTable();
}

TripleBitBuilder::TripleBitBuilder() {
	preTable = NULL;
	uriTable = NULL;
	bitmap = NULL;
	staReifTable = NULL;
}

TripleBitBuilder::~TripleBitBuilder() {
	// TODO Auto-generated destructor stub
#ifdef TRIPLEBITBUILDER_DEBUG
	cout << "Bit map builder destroyed begin " << endl;
#endif
	//mysql = NULL;
	if( preTable != NULL ) 
		delete preTable;
	preTable = NULL;

	if( uriTable != NULL )
		delete uriTable;
	uriTable = NULL;
	//delete uriStaBuffer;
	if ( staReifTable != NULL )
		delete staReifTable;
	staReifTable = NULL;

	if (bitmap != NULL) {
		delete bitmap;
		bitmap = NULL;
	}

	for(int i = 0; i < 4; i++)
	{
		if(statBuffer[i] != NULL)
			delete statBuffer[i];
		statBuffer[i] = NULL;
	}
}

bool TripleBitBuilder::isStatementReification(const char* object) {
	int pos;

	const char* p;

	if ((pos = getCharPos(object, '#')) != -1) {
		p = object + pos;

		if (strcmp(p, "Statement") == 0 || strcmp(p, "subject") == 0 || strcmp(p, "predicate") == 0 || strcmp(p, "object") == 0) {
			return true;
		}
	}

	return false;
}

bool TripleBitBuilder::generateXY(ID& subjectID, ID& objectID)
{
	if(subjectID > objectID)
	{
		ID temp = subjectID;
		subjectID = objectID;
		objectID = temp - objectID;
		return true;
	}else{
		objectID = objectID - subjectID;
		return false;
	}
}

void TripleBitBuilder::NTriplesParse(const char* subject,  const char* predicate, const char* object, TempFile& facts) {
	ID subjectID, objectID, predicateID;

	if (isStatementReification(object) == false && isStatementReification(predicate) == false) {
		if (preTable->getIDByPredicate(predicate, predicateID) == PREDICATE_NOT_BE_FINDED) {
			preTable->insertTable(predicate, predicateID);
		}


		if (uriTable->getIdByURI(subject, subjectID) == URI_NOT_FOUND)
			uriTable->insertTable(subject, subjectID);
		if (uriTable->getIdByURI(object, objectID) == URI_NOT_FOUND)
			uriTable->insertTable(object, objectID);

		facts.writeId(subjectID);
		facts.writeId(predicateID);
		facts.writeId(objectID);
	} else {
//		statementFile << subject << " : " << predicate << " : " << object << endl;
	}

}

bool TripleBitBuilder::N3Parse(istream& in, const char* name, TempFile& rawFacts) {
	cerr << "Parsing " << name << "..." << endl;

	TurtleParser parser(in);
	try {
		string subject, predicate, object, source;
		while (true) {
			try {
				if (!parser.parse(subject, predicate, object))
					break;
			} catch (const TurtleParser::Exception& e) {
				while (in.get() != '\n');
				continue;
			}
			//Construct IDs
			//and write the triples
			if(subject.length() && predicate.length() && object.length())
				NTriplesParse((char*) subject.c_str(), (char*) predicate.c_str(), (char*) object.c_str(), rawFacts);

		}
	} catch (const TurtleParser::Exception&) {
		return false;
	}
	return true;
}

const char* TripleBitBuilder::skipIdIdId(const char* reader) {
	return TempFile::skipId(TempFile::skipId(TempFile::skipId(reader)));
}

int TripleBitBuilder::compare213(const char* left, const char* right) {
	ID l1, l2, l3, r1, r2, r3;
	loadTriple(left, l1, l2, l3);
	loadTriple(right, r1, r2, r3);

	return cmpTriples(l2, l1, l3, r2, r1, r3);
}

int TripleBitBuilder::compare231(const char* left, const char* right) {
	ID l1, l2, l3, r1, r2, r3;
	loadTriple(left, l1, l2, l3);
	loadTriple(right, r1, r2, r3);

	return cmpTriples(l2, l3, l1, r2, r3, r1);
}

int TripleBitBuilder::compare123(const char* left, const char* right) {
	ID l1, l2, l3, r1, r2, r3;
	loadTriple(left, l1, l2, l3);
	loadTriple(right, r1, r2, r3);

	return cmpTriples(l1, l2, l3, r1, r2, r3);
}

int TripleBitBuilder::compare321(const char* left, const char* right) {
	ID l1, l2, l3, r1, r2, r3;
	loadTriple(left, l1, l2, l3);
	loadTriple(right, r1, r2, r3);

	return cmpTriples(l3, l2, l1, r3, r2, r1);
}


Status TripleBitBuilder::resolveTriples(string rawFactsFilename, string sosetFile,string psetFile) {
	cout<<"Sort by Subject"<<endl;
	ID subjectID, objectID, predicateID;
    unordered_set<ID>* soset=new unordered_set<ID>();
	unordered_set<ID>* pset=new unordered_set<ID>();
	vector<unordered_set<ID>*> sosetvector;
	vector<unordered_set<ID>*> psetvector;

	string osinfo = getosInfo();
	string cpuinfo = getcpuInfo();
	string memeryinfo = getmemoryInfo();
	cout << "osinfo:" << endl;
	cout << osinfo << endl;
	cout << "-----------" << endl;
	cout << "cpuinfo:" << endl;
	cout << cpuinfo << endl;
	cout << "-----------" << endl;
	cout << "memeryinfo:" << endl;
	cout << memeryinfo << endl;
	cout << "-----------" << endl;
	ID lastSubject = 0, lastObject = 0, lastPredicate = 0;
	unsigned count0 = 0, count1 = 0;
	TempFile sortedBySubject("./SortByS"), sortedByObject("./SortByO");
	Sorter::sort(rawFactsFilename, sortedBySubject, skipIdIdId, compare123);
	cout << "subject sort end" << endl;
	{
		//insert into chunk
		sortedBySubject.close();
		MemoryMappedFile mappedIn;
		assert(mappedIn.open(sortedBySubject.getFile().c_str()));
		const char* reader = mappedIn.getBegin(), *limit = mappedIn.getEnd();

		loadTriple(reader, subjectID, predicateID, objectID);
		
		//这里需要将节点中存有的s，p，o的id都存起来，以便后边查询时检查用
		soset->insert(subjectID);
		soset->insert(objectID);
		pset->insert(predicateID);
		//按s排序和按o排序只要一个有就可以了

        lastSubject = subjectID; lastPredicate = predicateID; lastObject = objectID;
		reader = skipIdIdId(reader);
		bool v = generateXY(subjectID, objectID);
		bitmap->insertTriple(predicateID, subjectID, objectID, v, 0);
		count0 = count1 = 1;
		
		cout << "start insertTriple , firstTriple insert success" << endl;

		while (reader < limit) {
			loadTriple(reader, subjectID, predicateID, objectID);
			
			//这里需要将节点中存有的s，p，o的id都存起来，以便后边查询时检查用

			//当set存满的时候，换一个新set，将老set的指针放在setvector里边
			if (soset->size() >= soset->max_size()-1 ) {
				sosetvector.push_back(soset);
				soset = new unordered_set<ID>();
			}
			if (pset->size() >= soset->max_size()-1 ) {
				psetvector.push_back(pset);
				pset = new unordered_set<ID>();
			}

			//insert的时候还要要检查setvector里边的set有没有包含该值，有的话就不insert
			int sflag = 0, pflag = 0, oflag = 0;
			for (int i = 0; i < sosetvector.size(); i++) {
				if (sosetvector[i]->find(subjectID)!=sosetvector[i]->end()) {//s存在
					sflag = 1;
				}
				if (sosetvector[i]->find(objectID)!=sosetvector[i]->end()) {//o存在
					oflag = 1;
				}
			}
			for (int i = 0; i < psetvector.size();i++) {
				if (psetvector[i]->find(predicateID)!=psetvector[i]->end()) {//p存在
					pflag = 1;
				}
			}
			if(sflag==0) soset->insert(subjectID);
			if(oflag==0) soset->insert(objectID);
			if(pflag==0) pset->insert(predicateID);

            if(lastSubject == subjectID && lastPredicate == predicateID && lastObject == objectID) {
				reader = skipIdIdId(reader);
				continue;
			}

			if ( subjectID != lastSubject ) {
				((OneConstantStatisticsBuffer*)statBuffer[0])->addStatis(lastSubject, count0);
				statBuffer[2]->addStatis(lastSubject, lastPredicate, count1);
				lastPredicate = predicateID;
				lastSubject = subjectID;
				count0 = count1 = 1;
			} else if ( predicateID != lastPredicate ) {
				statBuffer[2]->addStatis(lastSubject, lastPredicate, count1);
				lastPredicate = predicateID;
				count0++; count1 = 1;
			}else {
				count0++; count1++;
				lastObject = objectID;
			}
			
			reader = reader + 12;
			v = generateXY(subjectID, objectID);
			//0 indicate the triple is sorted by subjects' id;
			bitmap->insertTriple(predicateID, subjectID, objectID, v, 0);
		}
		cout << "insertTriple end , start close mappedinFile" << endl;
		mappedIn.close();
		cout << "close mappedinFile success" << endl;
	}
	cout << "start flush bitmap" << endl;
	bitmap->flush();
	cout << "bitmap flush success" << endl;
	//sort
	cerr << "Sort by Object" << endl;
	Sorter::sort(rawFactsFilename, sortedByObject, skipIdIdId, compare321);
	cout << "object sort end" << endl;
	{
		//insert into chunk
		sortedByObject.close();
		MemoryMappedFile mappedIn;
		assert(mappedIn.open(sortedByObject.getFile().c_str()));
		const char* reader = mappedIn.getBegin(), *limit = mappedIn.getEnd();

		loadTriple(reader, subjectID, predicateID, objectID);
        
		lastSubject = subjectID; lastPredicate = predicateID; lastObject = objectID;
		reader = skipIdIdId(reader);
		bool v = generateXY(objectID, subjectID);
		bitmap->insertTriple(predicateID, objectID, subjectID, v, 1);
		count0 = count1 = 1;

		cout << "start insertTriple , firstTriple insert success" << endl;

		while (reader < limit) {
			loadTriple(reader, subjectID, predicateID, objectID);
            
			 if(lastSubject == subjectID && lastPredicate == predicateID && lastObject == objectID) {
				reader = skipIdIdId(reader);
				continue;
			}

			if ( objectID != lastObject ) {
				((OneConstantStatisticsBuffer*)statBuffer[1])->addStatis(lastObject, count0);
				statBuffer[3]->addStatis(lastObject, lastPredicate, count1);
				lastPredicate = predicateID;
				lastObject = objectID;
				count0 = count1 = 1;
			} else if ( predicateID != lastPredicate ) {
				statBuffer[3]->addStatis(lastObject, lastPredicate, count1);
				lastPredicate = predicateID;
				count0++; count1 = 1;
			} else {
				lastSubject = subjectID;
				count0++; count1++;
			}
			reader = skipIdIdId(reader);
			v = generateXY(objectID, subjectID);
			// 1 indicate the triple is sorted by objects' id;
			bitmap->insertTriple(predicateID, objectID, subjectID, v, 1);
		}
		cout << "insertTriple end , start close mappedinFile" << endl;
		mappedIn.close();
		cout << "close mappedinFile success" << endl;
	}

	osinfo = getosInfo();
	cpuinfo = getcpuInfo();
	memeryinfo = getmemoryInfo();
	cout << "osinfo:" << endl;
	cout << osinfo << endl;
	cout << "-----------" << endl;
	cout << "cpuinfo:" << endl;
	cout << cpuinfo << endl;
	cout << "-----------" << endl;
	cout << "memeryinfo:" << endl;
	cout << memeryinfo << endl;
	cout << "-----------" << endl;

	cout << "start flush bitmap" << endl;
	bitmap->flush();
	cout << "bitmap flush success" << endl;
	cout << "start discard SOsort files" << endl;
	sortedByObject.discard();
	sortedBySubject.discard();
	cout << "SOsort files discard success" << endl;
    //将soset和pset里的东西存到文件里(sosetFile，psetFile)
	cout << "start store soset and pset to file" << endl;
	if (soset->size() > 0) {
		sosetvector.push_back(soset);
	}
	if (pset->size() > 0) {
		psetvector.push_back(pset);
	}
	//接下来把setvector里边的所有元素分文件存储在硬盘里
	for (long long unsigned i = 0; i < sosetvector.size();i++) {
		TempFile sosetVectorFile(sosetFile + "-vector-" + std::to_string(i), ios::trunc);
		for (unordered_set<ID>::iterator iter = sosetvector[i]->begin(); iter != sosetvector[i]->end();iter++) {
			sosetVectorFile.writeId(*iter);
		}
		sosetVectorFile.close();
	}
	for (long long unsigned i = 0; i < psetvector.size();i++) {
		TempFile psetVectorFile(psetFile + "-vector-" + std::to_string(i), ios::trunc);
		for (unordered_set<ID>::iterator iter = psetvector[i]->begin(); iter != psetvector[i]->end();iter++) {
			psetVectorFile.writeId(*iter);
		}
		psetVectorFile.close();
	}
	cout << "store and close soset and pset success" << endl;
	return OK;
}

// Status TripleBitBuilder::startBuildN3(string fileName) {
// 	TempFile rawFacts("./test");

// 	ifstream in((char*) fileName.c_str());
// 	if (!in.is_open()) {
// 		cerr << "Unable to open " << fileName << endl;
// 		return ERROR;
// 	}
// 	if (!N3Parse(in, fileName.c_str(), rawFacts)) {
// 		in.close();
// 		return ERROR;
// 	}

// 	in.close();
// 	delete uriTable;
// 	uriTable = NULL;
// 	delete preTable;
// 	preTable = NULL;
// 	delete staReifTable;
// 	staReifTable = NULL;

// 	rawFacts.flush();
// 	cout<<"over"<<endl;

// 	//sort by s,o
// 	TempFile facts(fileName);//fact没有用到
// 	resolveTriples(rawFacts, facts);
// 	rawFacts.discard();
// 	facts.discard();
// 	return OK;
// }

Status TripleBitBuilder::buildIndex() {
	// build hash index;
	MMapBuffer* bitmapIndex;
	cout<<"build hash index for subject"<<endl;
	for ( map<ID,ChunkManager*>::iterator iter = bitmap->predicate_managers[0].begin(); iter != bitmap->predicate_managers[0].end(); iter++ ) {
		if ( iter->second != NULL ) {
			iter->second->buildChunkIndex();
			iter->second->getChunkIndex(1)->save(bitmapIndex);
			iter->second->getChunkIndex(2)->save(bitmapIndex);
		}
	}

	cout<<"build hash index for object"<<endl;
	for ( map<ID, ChunkManager*>::iterator iter = bitmap->predicate_managers[1].begin(); iter != bitmap->predicate_managers[1].end(); iter++ ) {
		if ( iter->second != NULL ) {
			iter->second->buildChunkIndex();
			iter->second->getChunkIndex(1)->save(bitmapIndex);
			iter->second->getChunkIndex(2)->save(bitmapIndex);
		}
	}

	return OK;
}

Status TripleBitBuilder::endBuild() {
	cout << "bitmap save" << endl;
	bitmap->save();
	cout << "bitmap save end" << endl;
	ofstream ofile(string(dir + "/statIndex").c_str());
	MMapBuffer* indexBuffer = NULL;
	cout << "statBuffer[0] save" << endl;
	((OneConstantStatisticsBuffer*)statBuffer[0])->save(indexBuffer);
	cout << "statBuffer[0] save end" << endl;
	((OneConstantStatisticsBuffer*)statBuffer[1])->save(indexBuffer);
	cout << "statBuffer[1] save end" << endl;
	((TwoConstantStatisticsBuffer*)statBuffer[2])->save(indexBuffer);
	cout << "statBuffer[2] save end" << endl;
	((TwoConstantStatisticsBuffer*)statBuffer[3])->save(indexBuffer);
	cout << "statBuffer[3] save end" << endl;

	delete indexBuffer;
	return OK;
}
