/* tripleBitQueryAPI.cpp */
/* 
  此文件为tripleBitQueryAPI.h的实现文件
*/
#include "./tripleBitQueryAPI.h"
#include "../TripleBit/TripleBit.h"
#include "../TripleBit/TripleBitRepository.h"
#include "../TripleBit/OSFile.h"
#include "../TripleBit/MMapBuffer.h"
//#include "../TripleBit/TripleBitInsert.h"
#include <vector>
#include<iostream>
#include<string.h>
using namespace std;

char* DATABASE_PATH;
char* QUERY_PATH;    //查询语句的路径，全局变量
TripleBitRepository* repo;

/*
  将旧数据库内容读取到内存，将新数据加入，都读取到内存中
*/

int create(const char* data_path)  // data_path旧的初始数据库，创建成功返回1，失败返回-1
{
    cout << "data_path:" << data_path << endl;
    //DATABASE_PATH = new char[length + 1];
    DATABASE_PATH = new char[8096 + 1];
    for(int i = 0; i < 8097; i++) *(DATABASE_PATH + i) = '\0';
    strcpy(DATABASE_PATH, data_path);
    cout << DATABASE_PATH << endl;
    repo = TripleBitRepository::create(DATABASE_PATH);    //创建旧数据库
   // tbInsert = new TripleBitInsert();   //创建新数据库对应
   // tbInsert->load(rawData); 
    if(repo == NULL) {
        cout << "failed" << endl;
        return -1;
    }
    return 1;
}


/*
 查询原始数据库数据
*/
//由于返回值修改为unsigned int后, 不能用-1表示空结果,改为用0表示空结果
vector<unsigned int>  search(const char* queryStr)      // 查询数据库
{
    //char* search(string& queryTemp) {
    cout << "--------- search ---------" << endl;
    char* cPtr = NULL;
    vector<unsigned int> result;
    if(repo)
    {
        string queryTemp(queryStr);
        repo->execute(queryTemp);//全局变量
        repo->getResultSet(result);
      	repo->ttForResult->resumeReference();
    }
    return result;
}

/*
 关闭数据库连接，项目结束时调用
*/
 
void closeDB()
{
    //cout << "--------- close ---------" << endl;
    delete repo;
}
