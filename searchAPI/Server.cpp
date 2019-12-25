//
// Created by peng on 19-11-28.
//

#include "Server.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <thread>
#include <iostream>

#include "../TripleBit/tripleBitQueryAPI.h"
/*
#define PORT 10000
#define QUEUE 20//连接请求队列

int main() {
    struct sockaddr_in server_sockaddr;//一般是储存地址和端口的。用于信息的显示及存储使用
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
*/
Server::Server(SOCKADDR server) {
    slaveAddr = server;
    sockid = socket(AF_INET, SOCK_STREAM, 0);
}

int Server::myBind() {
    if (::bind(sockid, (struct sockaddr *) &slaveAddr, sizeof(slaveAddr)) == -1) {
        perror("bind");
        exit(1);
    }
    return 0;
}

int Server::myListen() {
    if (listen(sockid, QUEUE) == -1) {
        perror("listen");
        exit(1);
    }
    return 0;
}

int Server::myAccept() {
    struct sockaddr_in master_addr;
    socklen_t length = sizeof(master_addr);
    ///成功返回非负描述字，出错返回-1
    masterid = accept(sockid, (struct sockaddr *) &master_addr, &length);
    return 0;
}

int Server::mySend(char* buffer, size_t len) {
	size_t count = 0;
        send(masterid, &len, sizeof(size_t), 0);
	while(count < len){
	    count += send(masterid, buffer + count, len - count, 0);
	}
	std::cout << "slave send:" << count << endl;
	return 0;
}

int Server::myRecv() {
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    int len = recv(masterid, buffer, sizeof(buffer), 0);
    if (strcmp(buffer, "create\n") == 0)
    {
        //TODO: 调用create函数load数据库
        //然后发送create ok\n
        // -----------------------------------
	std::cout << "create" << std::endl;
        create("/opt/Grace/mydatabase/");
        // -----------------------------------

        int t = sprintf(buffer, "%s", "create ok\n");
	std::cout << buffer << endl;
        mySend(buffer, t);
    } else if (strcmp(buffer, "exit\n") == 0) {
        return 0;
    } else {
        // TODO:收到查询语句，调用相关函数执行查询语句
        std::cout << "收到查询语句: " << buffer << std::endl;
        //返回查询到的数据
        vector<string> rst = search(buffer);
	cout << "after search" << endl;
	cout << "second after search" << endl;
        string toMaster = "";
	if(rst.size() == 0 || rst[0] == ""){
	    toMaster = "-1\nEmpty Result\n";
	} else {
	    if(rst.size() == 1 && rst[0] == "Empty Result"){
		toMaster = "-1\nEmpty Result\n";
	    } else {
		//toMaster = std::to_string(rst.size()) + '\n';
            	for (int i = 0; i < rst.size(); i++) {
                    toMaster += rst[i] + '\n';
                }
	    }
	}
        //std::cout << "toMaster:" << toMaster << std::endl; 
	char* buffer_re = (char*) malloc(toMaster.size()*2 + 1);
        long long t = sprintf(buffer_re, "%s", toMaster.c_str());
        mySend(buffer_re, t);
	std::cout << "slave return:" << std::endl;
	//std::cout << buffer_re << "---end" << t << std::endl;
        free(buffer_re);
    }
    return 1;
}

int Server::myClose() {
    close(masterid);
    close(sockid);
    return 0;
}
