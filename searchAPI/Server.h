//
// Created by peng on 19-11-28.
//

#ifndef PROJECT_SERVER_H
#define PROJECT_SERVER_H

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
#define QUEUE 20//连接请求队列

typedef struct sockaddr_in SOCKADDR;

class Server {
    int sockid;
    int masterid;
    SOCKADDR slaveAddr;

public:
    Server(SOCKADDR server);
    int myBind();
    int myListen();
    int myAccept();
    int mySend(char* buffer, size_t len);
    int myRecv();
    int myClose();
};


#endif //PROJECT_SERVER_H
