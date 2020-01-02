/*
 * TripleBitRespository.cpp
 *
 *  Created on: May 13, 2010
 *      Author: root
 */

#include "MemoryBuffer.h"
#include "TripleBitRepository.h"
#include "PredicateTable.h"
#include "URITable.h"
#include "TripleBitBuilder.h"
#include "BitmapBuffer.h"
#include "StatisticsBuffer.h"
#include "EntityIDBuffer.h"
#include "MMapBuffer.h"
#include "ThreadPool.h"
#include "TempMMapBuffer.h"
#include "OSFile.h"
#include <sys/time.h>
#include "comm/TransQueueSW.h"
#include "comm/TasksQueueWP.h"
#include "comm/ResultBuffer.h"
#include "comm/IndexForTT.h"
#include <boost/thread/thread.hpp>

int TripleBitRepository::colNo = INT_MAX - 1;

TripleBitRepository::TripleBitRepository() {
	this->UriTable = NULL;
	this->preTable = NULL;
	this->bitmapBuffer = NULL;
	buffer = NULL;
	this->transQueSW = NULL;

	subjectStat = NULL;
	subPredicateStat = NULL;
	objectStat = NULL;
	objPredicateStat = NULL;

	bitmapImage = NULL;
	bitmapIndexImage = NULL;
	bitmapPredicateImage = NULL;
    
	sosetvector=NULL;
	psetvector=NULL;
}

TripleBitRepository::~TripleBitRepository() {
	TempMMapBuffer::deleteInstance();

	if (buffer != NULL)
		delete buffer;
	buffer = NULL;

	if (UriTable != NULL)
		delete UriTable;
	UriTable = NULL;
#ifdef DEBUG
	cout<<"uri table delete"<<endl;
#endif
	if (preTable != NULL)
		delete preTable;
	preTable = NULL;
#ifdef DEBUG
	cout<<"predicate table delete"<<endl;
#endif
	if (bitmapBuffer != NULL)
		delete bitmapBuffer;
	bitmapBuffer = NULL;
#ifdef DEBUG
	cout<<"bitmapBuffer delete"<<endl;
#endif
	if (subjectStat != NULL)
		delete subjectStat;
	subjectStat = NULL;

	if (subPredicateStat != NULL)
		delete subPredicateStat;
	subPredicateStat = NULL;

	if (objectStat != NULL)
		delete objectStat;
	objectStat = NULL;

	if (objPredicateStat != NULL)
		delete objPredicateStat;
	objPredicateStat = NULL;

	if (bitmapImage != NULL)
		delete bitmapImage;
	bitmapImage = NULL;

#ifdef DEBUG
	cout<<"bitmapImage delete"<<endl;
#endif

	if (bitmapIndexImage != NULL)
		delete bitmapIndexImage;
	bitmapIndexImage = NULL;

#ifdef DEBUG
	cout<<"bitmap index image delete"<<endl;
#endif

	if (bitmapPredicateImage != NULL)
		delete bitmapPredicateImage;
	bitmapPredicateImage = NULL;
#ifdef DEBUG
	cout<<"bitmap predicate image"<<endl;
#endif

	if (tripleBitWorker.size() == workerNum) {
		for (size_t i = 1; i <= workerNum; ++i) {
			if (tripleBitWorker[i] != NULL)
				delete tripleBitWorker[i];
			tripleBitWorker[i] = NULL;
		}
	}
#ifdef MYDEBUG
	cout << "TripleBitWorker delete" << endl;
#endif


	for (size_t i = 1; i <= partitionNum; ++i) {
		if (partitionMaster[i] != NULL)
			delete partitionMaster[i];
		partitionMaster[i] = NULL;
	}
#ifdef MYDEBUG
	cout << "partitionMaster delete" << endl;
#endif

	if(indexForTT != NULL)
		delete indexForTT;
	indexForTT = NULL;
    
	if(sosetvector!=NULL)
		delete sosetvector;
	sosetvector =NULL;

	if(psetvector!=NULL)
		delete psetvector;
	psetvector =NULL;
}

bool TripleBitRepository::find_pid_by_string(PID& pid, const string& str) {
	ID temp=stoul(str);
	for (int i = 0; i < psetvector->size();i++) {
		if(psetvector->at(i)->find(temp)!=psetvector->at(i)->end()){
			//当找到的时候
			pid = temp;
			return true;
		}
	}
	return false;
	// if (preTable->getIDByPredicate(str.c_str(), pid) != OK)
	// 	return false;
	// return true;
}

bool TripleBitRepository::find_pid_by_string_update(PID& pid, const string& str) {
	//不支持插入！这个函数不应该被调用！！！
    if (preTable->getIDByPredicate(str.c_str(), pid) != OK) {
		if (preTable->insertTable(str.c_str(), pid) != OK)
			return false;
	}
	return true;
}

bool TripleBitRepository::find_soid_by_string(SOID& soid, const string& str) {
	//为了slave直接使用id，因此改了所有调用过getid/uribyuri/id的地方，包括predicate
	cout<<"=============find_soid_by_string str:=============="<<endl;
	cout<<str<<endl;
	soid=stoul(str);
	for (int i = 0; i < sosetvector->size();i++) {
		if(sosetvector->at(i)->find(soid)==sosetvector->at(i)->end()){
			return false;
		}
	}
	return true;
	// if (UriTable->getIdByURI(str.c_str(), soid) != URI_FOUND)
	// 	return false;
	// return true;
}

bool TripleBitRepository::find_soid_by_string_update(SOID& soid, const string& str) {
	//不支持插入！这个函数不应该被调用！！！
    if (UriTable->getIdByURI(str.c_str(), soid) != URI_FOUND) {
		if (UriTable->insertTable(str.c_str(), soid) != OK)
			return false;
	}
	return true;
}

bool TripleBitRepository::find_string_by_pid(string& str, PID& pid) {
	// str = preTable->getPrediacateByID(pid);
	//同样，所有predicate也都取消id转换，为了直接使用ID的查询
	int flag = 0;
	for (int i = 0; i < psetvector->size();i++) {
		if(psetvector->at(i)->find(pid)!=psetvector->at(i)->end()){
			//当找到的时候
			str = std::to_string((long long)pid);
			flag = 1;
		}
	}
	if(flag==1){
		return true;
	}else{
		str = "";
		return false;
	}

	//if(pset->find(pid)!=pset->end()){
	//	str=std::to_string((long long)pid);
	//}else str="";

	//if (str.length() == 0)
	//	return false;
	//return true;
}

bool TripleBitRepository::find_string_by_soid(string& str, SOID& soid) {
	for (int i = 0; i < sosetvector->size();i++){
		if(sosetvector->at(i)->find(soid)==sosetvector->at(i)->end()){
			return false;
		}else{
			str = std::to_string((long long)soid);
			break;
		}
	}
	return true;
	// if (UriTable->getURIById(str, soid) == URI_NOT_FOUND)
	// 	return false;
	// return true;
}

int TripleBitRepository::get_predicate_count(PID pid) {
	int count1 = bitmapBuffer->getChunkManager(pid, 0)->getTripleCount();
	int count2 = bitmapBuffer->getChunkManager(pid, 1)->getTripleCount();

	return count1 + count2;
}

bool TripleBitRepository::lookup(const string& str, ID& id) {
	ID temp=stoul(str);
	for (int i = 0; i < psetvector->size();i++) {
		if(psetvector->at(i)->find(temp)!=psetvector->at(i)->end()){
			//当找到的时候
			id = temp;
			return true;
		}
	}
	for (int i = 0; i < sosetvector->size();i++) {
		if(sosetvector->at(i)->find(temp)!=sosetvector->at(i)->end()){
			//当找到的时候
			id = temp;
			return true;
		}
	}
	return false;
	//if(pset->find(temp)==pset->end()){
	//	if(soset->find(temp)==soset->end()){
	//		//当且仅当temp在pset和soset都查不到的时候返回false
	//		return false;
	//	}else id=temp;
	//}else id=temp;
	//return true;

	// if (preTable->getIDByPredicate(str.c_str(), id) != OK && UriTable->getIdByURI(str.c_str(), id) != URI_FOUND)
	// 	return false;
	// return true;
}
int TripleBitRepository::get_object_count(ID objectID) {
	return 666;
	((OneConstantStatisticsBuffer*) objectStat)->getStatis(objectID);
	return objectID;
}

int TripleBitRepository::get_subject_count(ID subjectID) {
	return 666;
	((OneConstantStatisticsBuffer*) subjectStat)->getStatis(subjectID);
	return subjectID;
}

int TripleBitRepository::get_subject_predicate_count(ID subjectID, ID predicateID) {
	return 666;
	subPredicateStat->getStatis(subjectID, predicateID);
	return subjectID;
}

int TripleBitRepository::get_object_predicate_count(ID objectID, ID predicateID) {
	return 666;
	objPredicateStat->getStatis(objectID, predicateID);
	return objectID;
}

int TripleBitRepository::get_subject_object_count(ID subjectID, ID objectID) {
	return 1;
}

Status TripleBitRepository::getSubjectByObjectPredicate(ID oid, ID pid) {
	pos = 0;
	return OK;
}

ID TripleBitRepository::next() {
	ID id;
	Status s = buffer->getID(id, pos);
	if (s != OK)
		return 0;

	pos++;
	return id;
}

TripleBitRepository* TripleBitRepository::create(const string &path) {
	DATABASE_PATH = (char*) path.c_str();
	TripleBitRepository* repo = new TripleBitRepository();
	repo->dataBasePath = path;

	string filename = path + "BitmapBuffer";

	// load the repository from image files;
	//load bitmap
#ifdef DEBUG
	cout<<filename.c_str()<<endl;
#endif
	repo->bitmapImage = new MMapBuffer(filename.c_str(), 0);
	string predicateFile(filename);
	predicateFile.append("_predicate");
	string indexFile(filename);
	indexFile.append("_index");
	string tempMMap(filename);
	tempMMap.append("_temp");
	TempMMapBuffer::create(tempMMap.c_str(), repo->bitmapImage->getSize());

	repo->bitmapPredicateImage = new MMapBuffer(predicateFile.c_str(), 0);
	repo->bitmapIndexImage = new MMapBuffer(indexFile.c_str(), 0);
	repo->bitmapBuffer = BitmapBuffer::load(repo->bitmapImage, repo->bitmapIndexImage, repo->bitmapPredicateImage);

	repo->UriTable = URITable::load(path);//的话会return null
	repo->preTable = PredicateTable::load(path);//失败了的话会return null

	repo->sosetvector=new vector<unordered_set<ID>*>();
	repo->psetvector=new vector<unordered_set<ID>*>();
	
	//load soset and pset
	repo->MAX_SOID_IN_SET = 0;

	for (int i = 0;;i++) {
		MemoryMappedFile eachsosetFile;
		if (eachsosetFile.open((repo->dataBasePath+"soset-vector-"+std::to_string((long long)i)).c_str())) {
			//对应soset-vector文件存在，因此将其读入
			repo->sosetvector->push_back(new unordered_set<ID>());
			const char* soreader = eachsosetFile.getBegin(), * solimit = eachsosetFile.getEnd();
			ID id;
			while (soreader<solimit) {
				TempFile::readId(soreader, id);
				soreader += 4;
				repo->sosetvector->at(i)->insert(id);
				if (id > repo->MAX_SOID_IN_SET) repo->MAX_SOID_IN_SET = id;
			}
		}else {
			eachsosetFile.close();
			break;
		}
		eachsosetFile.close();
	}
	for (int i = 0;;i++) {
		MemoryMappedFile eachpsetFile;
		if (eachpsetFile.open((repo->dataBasePath+"pset-vector-"+std::to_string((long long)i)).c_str())) {
			//对应pset-vector文件存在，因此读入
			unordered_set<ID>* temp=new unordered_set<ID>;
			repo->psetvector->push_back(temp);
			const char* preader = eachpsetFile.getBegin(), * plimit = eachpsetFile.getEnd();
			ID id;
			while(preader<plimit){
				TempFile::readId(preader, id);
				preader += 4;
				repo->psetvector->at(i)->insert(id);
			}
		}else{
			eachpsetFile.close();
			break;
		}
		eachpsetFile.close();
	}
	//MemoryMappedFile sosetFile;
	//assert(sosetFile.open(("./"+repo->dataBasePath+"soset").c_str()));
	//const char* soreader = sosetFile.getBegin(), *solimit = sosetFile.getEnd();
	//MemoryMappedFile psetFile;
	//assert(psetFile.open(("./"+repo->dataBasePath+"pset").c_str()));
	//const char* preader = psetFile.getBegin(), *plimit = psetFile.getEnd();
	//ID id;
	//while(soreader<solimit){
	//	TempFile::readId(soreader,id);
	//	soreader+=4;
	//	repo->soset->insert(id);
	//}
	//while(preader<plimit){
	//	TempFile::readId(preader,id);
	//	preader+=4;
	//	repo->pset->insert(id);
	//}
	//sosetFile.close();
	//psetFile.close();

    // string uri;
	// ID maxID = repo->UriTable->getMaxID();
	// for(ID i = 1; i <= maxID; ++i){
	// 	repo->UriTable->getURIById(uri, i);
	// }

#ifdef DEBUG
	cout<<"total triple count: "<<repo->bitmapBuffer->getTripleCount()<<endl;
	cout<<"URITableSize: "<<repo->UriTable->getSize()<<endl;
	cout<<"predicateTableSize: "<<repo->preTable->getSize()<<endl;
#endif

	filename = path + "/statIndex";
	MMapBuffer* indexBufferFile = MMapBuffer::create(filename.c_str(), 0);
	char* indexBuffer = indexBufferFile->get_address();


	string statFilename = path + "/subject_statis";
	repo->subjectStat = OneConstantStatisticsBuffer::load(StatisticsBuffer::SUBJECT_STATIS, statFilename, indexBuffer);
	statFilename = path + "/object_statis";
	repo->objectStat = OneConstantStatisticsBuffer::load(StatisticsBuffer::OBJECT_STATIS, statFilename, indexBuffer);
	statFilename = path + "/subjectpredicate_statis";
	repo->subPredicateStat = TwoConstantStatisticsBuffer::load(StatisticsBuffer::SUBJECTPREDICATE_STATIS, statFilename, indexBuffer);
	statFilename = path + "/objectpredicate_statis";
	repo->objPredicateStat = TwoConstantStatisticsBuffer::load(StatisticsBuffer::OBJECTPREDICATE_STATIS, statFilename, indexBuffer);

#ifdef DEBUG
	cout<<"subject count: "<<((OneConstantStatisticsBuffer*)repo->subjectStat)->getEntityCount()<<endl;
	cout<<"object count: "<<((OneConstantStatisticsBuffer*)repo->objectStat)->getEntityCount()<<endl;
#endif

	repo->buffer = new EntityIDBuffer();

	cout << "load complete!" << endl;

	repo->partitionNum = repo->bitmapPredicateImage->get_length() / ((sizeof(ID) + sizeof(SOType) + sizeof(size_t) * 2) * 2);
	repo->workerNum = WORKERNUM;
	repo->indexForTT = new IndexForTT(WORKERNUM);
	repo->ttForResult = new IndexForTT(WORKERNUM);
#ifdef DEBUG
	cout << "partitionNumber: " << repo->partitionNum << endl;
#endif

	repo->sharedMemoryInit();

	for (size_t i = 1; i <= repo->workerNum; ++i) {
		repo->tripleBitWorker[i] = new TripleBitWorker(repo, i);
	}

	for (size_t i = 1; i <= repo->workerNum; ++i) {
		boost::thread thrd(boost::thread(boost::bind<void>(&TripleBitRepository::tripleBitWorkerInit, repo, i)));
	}

	for (size_t i = 1; i <= repo->partitionNum; ++i) {
		repo->partitionMaster[i] = new PartitionMaster(repo, i);
	}

	return repo;
}


void TripleBitRepository::tripleBitWorkerInit(int i) {
	tripleBitWorker[i]->Work();
}

void TripleBitRepository::partitionMasterInit(TripleBitRepository*& repo, int i) {
	repo->partitionMaster[i] = new PartitionMaster(repo, i);
	repo->partitionMaster[i]->Work();
}

Status TripleBitRepository::sharedMemoryInit() {
	//Init the transQueueSW shared Memory
	sharedMemoryTransQueueSWInit();

	//Init the tasksQueueWP shareed memory
	sharedMemoryTasksQueueWPInit();

	//Init the resultWP shared memory
	sharedMemoryResultWPInit();

	uriMutex = new boost::mutex();

	return OK;
}

Status TripleBitRepository::sharedMemoryDestroy() {
	//Destroy the transQueueSW shared Memory
	sharedMemoryTransQueueSWDestroy();
#ifdef MYDEBUG
	cout << "shared memory TransQueueSW destoried" << endl;
#endif

	//Destroy the tasksQueueWP shared memory
	sharedMemoryTasksQueueWPDestroy();
#ifdef MYDEBUG
	cout << "shared memory TasksQueueWP destoried" << endl;
#endif

	//Destroy the ResultWP shared memory
	sharedMemoryResultWPDestroy();
#ifdef MYDEBUG
	cout << "shared memory ResultWP destoried" << endl;
#endif

	return OK;
}

Status TripleBitRepository::sharedMemoryTransQueueSWInit() {
	transQueSW = new transQueueSW();
#ifdef DEBUG
	cout << "transQueSW(Master) Address: " << transQueSW << endl;
#endif
	if (transQueSW == NULL) {
		cout << "TransQueueSW Init Failed!" << endl;
		return ERROR_UNKOWN;
	}
	return OK;
}


Status TripleBitRepository::sharedMemoryTransQueueSWDestroy() {
	if (transQueSW != NULL) {
		delete transQueSW;
		transQueSW = NULL;
	}
	return OK;
}

Status TripleBitRepository::sharedMemoryTasksQueueWPInit() {
	for (size_t partitionID = 1; partitionID <= this->partitionNum; ++partitionID) {
		TasksQueueWP* tasksQueue = new TasksQueueWP();
		if (tasksQueue == NULL) {
			cout << "TasksQueueWP Init Failed!" << endl;
			return ERROR_UNKOWN;
		}
		this->tasksQueueWP.push_back(tasksQueue);

		boost::mutex *wpMutex = new boost::mutex();
		this->tasksQueueWPMutex.push_back(wpMutex);
	}
	return OK;
}

Status TripleBitRepository::sharedMemoryTasksQueueWPDestroy() {
	vector<TasksQueueWP*>::iterator iter = this->tasksQueueWP.begin(), limit = this->tasksQueueWP.end();
	for (; iter != limit; ++iter) {
		delete *iter;
	}
	this->tasksQueueWP.clear();
	this->tasksQueueWP.swap(this->tasksQueueWP);
	return OK;
}

Status TripleBitRepository::sharedMemoryResultWPInit() {
	for (size_t workerID = 1; workerID <= this->workerNum; ++workerID) {
		for (size_t partitionID = 1; partitionID <= this->partitionNum; ++partitionID) {
			ResultBuffer* resBuffer = new ResultBuffer();
			if (resBuffer == NULL) {
				cout << "ResultBufferWP Init Failed!" << endl;
				return ERROR_UNKOWN;
			}

			this->resultWP.push_back(resBuffer);
		}
	}
	return OK;
}

Status TripleBitRepository::sharedMemoryResultWPDestroy() {
	vector<ResultBuffer*>::iterator iter = this->resultWP.begin(), limit = this->resultWP.end();
	for (; iter != limit; ++iter) {
		delete *iter;
	}
	this->resultWP.clear();
	this->resultWP.swap(this->resultWP);
	return OK;
}

Status TripleBitRepository::tempMMapDestroy() {
	if (TempMMapBuffer::getInstance().getUsedPage() == 0) {
		TempMMapBuffer::deleteInstance();
		return OK;
	}

	endPartitionMaster();

	bitmapBuffer->endUpdate(bitmapPredicateImage, bitmapImage);
	TempMMapBuffer::deleteInstance();
	return OK;
}

void TripleBitRepository::endPartitionMaster() {
	for (size_t i = 1; i < partitionNum; ++i) {
		partitionMaster[i]->endupdate();
	}
}

static void getQuery(string& queryStr, const char* filename) {
	ifstream f;
	f.open(filename);

	queryStr.clear();

	if (f.fail() == true) {
		MessageEngine::showMessage("open query file error!", MessageEngine::WARNING);
		return;
	}

	char line[150];
	while (f.peek() != EOF) {
		f.getline(line, 150);

		queryStr.append(line);
		queryStr.append(" ");
	}

	f.close();
}

Status TripleBitRepository::nextResult(string& str) {
	if (resBegin != resEnd) {
		str = *resBegin;
		resBegin++;
		return OK;
	}
	return ERROR;
}

Status TripleBitRepository::execute(string &queryStr) {
	resultSet.resize(0);
	cout<<"execute!"<<endl;
	transQueSW->EnQueue(queryStr);
	cout<<"back from enqueue"<<endl;
	
	ttForResult->wait();
	//cout<<"back from wait"<<endl;
	return OK;
}

void TripleBitRepository::endForWorker(){
	string queryStr("exit");
	for(size_t i = 1; i <= workerNum; ++i){
		transQueSW->EnQueue(queryStr);
	}
	indexForTT->wait();
}

void TripleBitRepository::workerComplete(){
	indexForTT->completeOneTriple();
}

extern char* QUERY_PATH;
void TripleBitRepository::cmd_line(FILE* fin, FILE* fout) {
	char cmd[256];
	while (true) {
		fflush(fin);
		fprintf(fout, ">>>");
		fscanf(fin, "%s", cmd);
		resultSet.resize(0);
		if (strcmp(cmd, "dp") == 0 || strcmp(cmd, "dumppredicate") == 0) {
			getPredicateTable()->dump();
		} else if (strcmp(cmd, "query") == 0) {

		} else if (strcmp(cmd, "source") == 0) {

			string queryStr;
			::getQuery(queryStr, string(QUERY_PATH).append("queryLUBM6").c_str());

			if (queryStr.length() == 0)
				continue;
			execute(queryStr);
		} else if (strcmp(cmd, "exit") == 0) {
			break;
		} else {
			string queryStr;
			::getQuery(queryStr, string(QUERY_PATH).append(cmd).c_str());

			if (queryStr.length() == 0)
				continue;
			execute(queryStr);
		}
	}
}

void TripleBitRepository::cmd_line_sm(FILE* fin, FILE *fout, const string query_path) {
	ThreadPool::createAllPool();
	char cmd[256];
	while (true) {
		fflush(fin);
		fprintf(fout, ">>>");
		fscanf(fin, "%s", cmd);
		resultSet.resize(0);
		if (strcmp(cmd, "dp") == 0 || strcmp(cmd, "dumppredicate") == 0) {
			getPredicateTable()->dump();
		} else if (strcmp(cmd, "query") == 0) {
		} else if (strcmp(cmd, "source") == 0) {
			string queryStr;
			::getQuery(queryStr, string(query_path).append("queryLUBM6").c_str());
			if (queryStr.length() == 0)
				continue;
		} else if (strcmp(cmd, "exit") == 0) {
			endForWorker();
			break;
		} else {
			string queryStr;
			::getQuery(queryStr, string(query_path).append(cmd).c_str());

			if (queryStr.length() == 0)
				continue;
			transQueSW->EnQueue(queryStr);
		}
	}
	ThreadPool::getChunkPool().wait();
	tempMMapDestroy();
	ThreadPool::deleteAllPool();
}

extern char* QUERY_PATH;
void TripleBitRepository::cmd_line_cold(FILE *fin, FILE *fout, const string cmd){
	string queryStr;
	getQuery(queryStr, string(QUERY_PATH).append(cmd).c_str());
	if(queryStr.length() == 0){
		cout << "queryStr.length() == 0!" << endl;
		return;
	}
	cout << "DataBase: " << DATABASE_PATH << " Query:" << cmd << endl;
	transQueSW->EnQueue(queryStr);
	endForWorker();
	tempMMapDestroy();
}

extern char* QUERY_PATH;
void TripleBitRepository::cmd_line_warm(FILE *fin, FILE *fout, const string cmd){
	string queryStr;
	getQuery(queryStr, string(QUERY_PATH).append(cmd).c_str());
	if(queryStr.length() == 0){
		cout << "queryStr.length() == 0" << endl;
		return;
	}
	cout << "DataBase: " << DATABASE_PATH << " Query:" << cmd << endl;
	for(int i = 0; i < 10; i++){
		transQueSW->EnQueue(queryStr);
	}
	endForWorker();
	tempMMapDestroy();
}
