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
#include <Eigen/Dense>
#include <csignal>

#define PORT 8080

using namespace std;

unordered_set<int> connectedProcessors;
int connectedClient=0;
string log_buffer;
int globalServerSocket = -1;

// <--- 3. Manejador de señal para el servidor
void signalHandler(int signum) {
    cout << "\nInterrupcion detectada (Ctrl+C). Apagando servidor..." << endl;
    
    if (globalServerSocket != -1) {
        shutdown(globalServerSocket, SHUT_RDWR);
        close(globalServerSocket);
        cout << "Socket del servidor cerrado." << endl;
    }
    
    // Opcional: Podrías recorrer connectedProcessors y cerrar esos sockets también aquí
    
    exit(signum);
}

int GetSize (int SocketClient,string &buffer){
    buffer.resize(sizeof(int));
    read(SocketClient,buffer.data(),sizeof(int));
    log_buffer += buffer;
    int* aux=(int*)buffer.data();
    int size=*aux;
    return size;
}

void readN(int SocketClient, string &readBuffer, int UnU = -1){
    int n;
    if(UnU == -1){
        n = GetSize(SocketClient, readBuffer);
    }
    else{
        n = UnU;
    }
    readBuffer.resize(n);
    int tmp2=n; 
    int tmp3=0;
    while(tmp2>0){
        int c;
        c=read(SocketClient,readBuffer.data()+tmp3,min(1000,tmp2));
        log_buffer += readBuffer;
        tmp2-=c;
        tmp3+=c;
    }
}

string formatSize(int value, int n) {
    std::ostringstream oss;
    oss << std::setw(n) << std::setfill('0') << value;
    return oss.str();
}


void readSocket(int SocketClient){
    string readBuffer;
    string writeBuffer;
    bool flag1=true;

    while (flag1){
        readBuffer.resize(1);
        read(SocketClient,readBuffer.data(),1);
        log_buffer=readBuffer;

        switch (readBuffer[0])
        {
        case 's':
            cout<<"Received:" <<log_buffer<<endl;
            connectedProcessors.insert(SocketClient);
            break;
        
        case 'c':
            {
                cout<<"Received:" <<log_buffer<<endl;
                if (connectedClient==0){
                    connectedClient=SocketClient;
                    writeBuffer="C";
                    log_buffer=writeBuffer;
                    cout<<"Sent:" <<log_buffer<<endl;
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
                    str2.resize(sizeof(int));
                    for (int i = 0; i < size;i++) {
                        str2[i] = *(((char*)&size) + i);
                    }

                    writeBuffer="X"+str2+str1;
                    log_buffer=writeBuffer;
                    cout<<"Sent:" <<log_buffer<<endl;
                    write(SocketClient,writeBuffer.data(),writeBuffer.size());
                }
            }
            break;

        case 'q':
            cout<<"Received:" <<log_buffer<<endl;
            if (connectedClient==SocketClient){
                connectedClient=0;
            }
            else if (connectedProcessors.find(SocketClient)!=connectedProcessors.end()){
                connectedProcessors.erase(connectedProcessors.find(SocketClient));
            }
            flag1=false;
            break;
        case 'f': 
            {
            int rows = GetSize(SocketClient, readBuffer);
            int cols = GetSize(SocketClient, readBuffer);
            cout<<"Received:" <<log_buffer<<endl;
            int datasize = rows * cols * sizeof(double);
            Eigen::MatrixXd receivedMat(rows, cols);
            read(SocketClient, receivedMat.data(), datasize);
            cout << "Matriz Recibida:" << endl;
            cout << receivedMat << endl;
            break;
            }
        default:
            break;
        }
    }
    
}




int main(int argc, char * argv[]){
    signal(SIGINT, signalHandler);
    int portNumber;

    if (argc >= 2) {
        portNumber = atoi(argv[1]);
    } else {
        portNumber = PORT;
    }

    struct sockaddr_in stSockAddr;
    int SocketServer = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    globalServerSocket = SocketServer;

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
