/*    tripleBitQueryAPI.h  */
/* 
  此文件为查询语句接口专用 
*/

#ifndef TRIPLEBITQUERYAPI_H
#define TRIPLEBITQUERYAPI_H
#include "../TripleBit/TripleBit.h"
#include "../TripleBit/TripleBitRepository.h"
#include "../TripleBit/OSFile.h"
#include "../TripleBit/MMapBuffer.h"
//#include "../TripleBit/TripleBitInsert.h"
#include <vector>


extern char* DATABASE_PATH;
extern char* QUERY_PATH;    //查询语句的路径，全局变量
extern TripleBitRepository* repo;

/* 
  创建数据库，将数据库文件加载
*/

int create(const char* data_path);  //data_path为数据库存储目录, rawData为数据源文件，返回-1代表创建失败，返回1代表创建成功


/*
  查询旧数据库
*/

vector<unsigned int>  search(const char* queryStr);      // 查询数据库

/*
  关闭数据库连接
*/

void closeDB();

#endif
