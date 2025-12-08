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
#include <unordered_map>
#include <fstream>
#include <iterator>
#include <unordered_map>
#include <unordered_set>


#define PORT 8080

using namespace std;

unordered_set<int> connectedProcessors;
int connectedClient=0;

string formatSize(int value, int n) {
    std::ostringstream oss;
    oss << std::setw(n) << std::setfill('0') << value;
    return oss.str();
}


void readSocket(int SocketClient){
    string readBuffer;
    string writeBuffer;
    string log;

    bool flag1=true;

    while (flag1){
        readBuffer.resize(1);
        read(SocketClient,readBuffer.data(),1);
        log=readBuffer;

        switch (readBuffer[0])
        {
        case 's':
            cout<<"Received:" <<log<<endl;
            connectedProcessors.insert(SocketClient);
            break;
        
        case 'c':
            {
                cout<<"Received:" <<log<<endl;
                if (connectedClient==0){
                    connectedClient=SocketClient;
                    writeBuffer="C";
                    log=writeBuffer;
                    cout<<"Sent:" <<log<<endl;
                    write(SocketClient,writeBuffer.data(),writeBuffer.size());
                }
                else {
                    string error="Connection Error";
                    int size=error.size();
                    char* aux = (char*)&size;

                    string str1;
                    str1.resize(size);
                    for (int i = 0; i < size;i++) {
                        str1[i] = *(error.data() + i);
                    }

                    string str2;
                    str2.resize(4);
                    for (int i = 0; i < size;i++) {
                        str2[i] = *(((char*)&size) + i);
                    }

                    writeBuffer="X"+str2+str1;
                    log=writeBuffer;
                    cout<<"Sent:" <<log<<endl;
                    write(SocketClient,writeBuffer.data(),writeBuffer.size());
                }
            }
            break;

        case 'q':
            cout<<"Received:" <<log<<endl;
            if (connectedClient==SocketClient){
                connectedClient=0;
            }
            else if (connectedProcessors.find(SocketClient)!=connectedProcessors.end()){
                connectedProcessors.erase(connectedProcessors.find(SocketClient));
            }
            flag1=false;
            break;
        
        default:
            break;
        }
    }





    
}




int main(int argc, char * argv[]){
    int portNumber;

    if (argc >= 2) {
        portNumber = atoi(argv[1]);
    } else {
        portNumber = PORT;
    }

    struct sockaddr_in stSockAddr;
    int SocketServer = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    memset(&stSockAddr, 0, sizeof(struct sockaddr_in));
    
    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(portNumber);
    stSockAddr.sin_addr.s_addr = INADDR_ANY;

    bind(SocketServer,(const struct sockaddr *)&stSockAddr, sizeof(struct sockaddr_in));
    listen(SocketServer, 10);



    vector<thread> threads;

    while (1){
        int SocketClient = accept(SocketServer, NULL, NULL);
        threads.push_back(thread (readSocket,SocketClient));
    }
    shutdown(SocketServer, SHUT_RDWR);
    close(SocketServer);

    return 0;

}
