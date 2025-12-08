#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <thread>
#include <cstring>
#include <atomic>
#include <vector>
#include <iomanip>
#include <limits>
#include <fstream>
#include <iterator>
#include <Eigen/Dense>
#define PORT 8080

using namespace std;

int seed;

int nProcessor,nProcessors;

int main(int argc, char * argv[]){
    int portNumber;


    if (argc>=2){
        portNumber=atoi(argv[1]);
    }
    else{
        portNumber=PORT;
    }


    struct sockaddr_in stSockAddr;
    int SocketClient = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    memset(&stSockAddr, 0, sizeof(struct sockaddr_in));
    
    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(portNumber);

    if (argc==3){
        inet_pton(AF_INET, argv[2], &stSockAddr.sin_addr);
    }
    else{
        inet_pton(AF_INET, "127.0.0.1", &stSockAddr.sin_addr);
    }

    connect(SocketClient, (const struct sockaddr *)&stSockAddr, sizeof(struct sockaddr_in));

    int aux;
    string line;
    bool flag1=true;

    string writeBuffer="s";
    string readBuffer;

    write(SocketClient,writeBuffer.data(),writeBuffer.size());

    cout<<"Processor Connected\n";

    
    readBuffer.resize(1);

    read(SocketClient,readBuffer.data(),readBuffer.size());

    if (readBuffer=="G"){
        read(SocketClient,&seed,sizeof(int));
    }

    cout<<seed<<endl;

    readBuffer.resize(1);

    read(SocketClient,readBuffer.data(),readBuffer.size());

    if (readBuffer=="E"){
        read(SocketClient,&nProcessor,sizeof(int));
        read(SocketClient,&nProcessors,sizeof(int));
    }

    while (1);
    

    return 0;

}