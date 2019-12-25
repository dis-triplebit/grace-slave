/* searchOldDatabase.cpp */
/* 
  本文件为查询旧数据库专用 
*/
#include "./Server.h"
#include "../TripleBit/tripleBitQueryAPI.h"
#include<iostream>
#include<string.h>
#include<vector>
#include<stdlib.h>
#include<fstream>
using namespace std;

/*string readQuery(const char* fileName){
  ifstream in(fileName);
  string result;
  string temp;
  if(!in.is_open()){
    cout<<"文件打开失败"<<endl;
   }
  while(getline(in, temp)){
    result += temp;
  }
  in.close();
  return result;
}

int main(int argc, char *argv[]){
  if(argc < 3){
     cout<<"searchOldDatabase <查询文件> <查询结果存放文件>"<<endl;
   }
  else{
    if(repo == NULL){
     cout<<"程序重新加载"<<endl;
     create("/home/zhouhuajian/Build/mydatabase", "../Database/new_triples20190830.txt"); 
    }
   else{
       cout<<"程序不需要进行保活"<<endl;
     }
    // char sen[100] = "select ?x ?y where {'飞机' ?x ?y}";
     string temp = readQuery(argv[1]);
     vector<string> result = search(temp.c_str());
     ofstream out(argv[2]);
     for(int i = 0; i < result.size(); i++){
     cout<<"结果: "<<result.at(i)<<endl;
     out<<result.at(i)<<endl; 
    }
     out.close();      
  }
  return 0;
}*/


#define PORT 10000
#define QUEUE 20//连接请求队列

int main() {
    cout << "slave start" << endl;
    struct sockaddr_in server_sockaddr;//一般是储存地址和端口的。用于信息的显示及存储使用
    /*设置 sockaddr_in 结构体中相关参数*/
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_port = htons(PORT);//将一个无符号短整型数值转换为网络字节序，即大端模式(big-endian)　
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);//将主机的无符号长整形数转换成网络字节顺序。　

    Server server(server_sockaddr);
    server.myBind();
    server.myListen();
    server.myAccept();
    while (server.myRecv());
    server.myClose();
    return 0;
}


