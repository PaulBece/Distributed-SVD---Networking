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

struct ThreadData{
    string writeBuffer;
    string readBuffer;
    int SocketClient;
    string log_buffer;
    ThreadData(int SocketClient): SocketClient(SocketClient){}

};

unordered_map<int,ThreadData*> connectedProcessors;
int connectedClient=0;
int globalServerSocket = -1;

int seed;

int K,P;



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

int GetSize (ThreadData &thread){
    thread.readBuffer.resize(sizeof(int));
    read(thread.SocketClient,thread.readBuffer.data(),sizeof(int));
    thread.log_buffer += thread.readBuffer;
    int* aux=(int*)thread.readBuffer.data();
    int size=*aux;
    return size;
}

void genSeed(){
    seed=rand();
}

void readN(ThreadData &thread, int UnU = -1){
    int n;
    if(UnU == -1){
        n = GetSize(thread);
    }
    else{
        n = UnU;
    }
    thread.readBuffer.resize(n);
    int tmp2=n; 
    int tmp3=0;
    while(tmp2>0){
        int c;
        c=read(thread.SocketClient,thread.readBuffer.data()+tmp3,min(1000,tmp2));
        thread.log_buffer += thread.readBuffer;
        tmp2-=c;
        tmp3+=c;
    }
}

string formatSize(int value, int n) {
    std::ostringstream oss;
    oss << std::setw(n) << std::setfill('0') << value;
    return oss.str();
}

void sendSeed(ThreadData &thread){
    string value;
    value.resize(sizeof(int));
    for (int i=0;i<sizeof(int);i++){
        value[i]=*(((char*)&seed)+i);
    }
    thread.writeBuffer="G"+value;
    write(thread.SocketClient,thread.writeBuffer.data(),thread.writeBuffer.size());
    thread.log_buffer=thread.writeBuffer;
    cout<<thread.log_buffer<<endl;
}

void sendSeedToProcessors(){
    for (auto processor:connectedProcessors){
        sendSeed(*(processor.second));
    }
}


void readSocket(int SocketClient){
    ThreadData thread(SocketClient);
    bool flag1=true;

    while (flag1){
        thread.readBuffer.resize(1);
        read(SocketClient,thread.readBuffer.data(),1);
        thread.log_buffer=thread.readBuffer;

        switch (thread.readBuffer[0])
        {
        case 's':
            cout<<"Received:" <<thread.log_buffer<<endl;
            connectedProcessors.insert(pair<int,ThreadData*>(SocketClient,&thread));
            break;
        
        case 'c':
            {
                cout<<"Received:" <<thread.log_buffer<<endl;
                if (connectedClient==0){
                    connectedClient=SocketClient;
                    thread.writeBuffer="C";
                    thread.log_buffer=thread.writeBuffer;
                    cout<<"Sent:" <<thread.log_buffer<<endl;
                    write(SocketClient,thread.writeBuffer.data(),thread.writeBuffer.size());
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

                    thread.writeBuffer="X"+str2+str1;
                    thread.log_buffer=thread.writeBuffer;
                    cout<<"Sent:" <<thread.log_buffer<<endl;
                    write(SocketClient,thread.writeBuffer.data(),thread.writeBuffer.size());
                }
            }
            break;

        case 'q':
            cout<<"Received:" <<thread.log_buffer<<endl;
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
            int rows = GetSize(thread);
            int cols = GetSize(thread);
            cout<<"Received:" <<thread.log_buffer<<endl;
            int datasize = rows * cols * sizeof(double);
            Eigen::MatrixXd receivedMat(rows, cols);
            read(SocketClient, receivedMat.data(), datasize);
            read(SocketClient,&K,sizeof(int));
            read(SocketClient,&P,sizeof(int));
            cout << "Matriz Recibida:" << endl;
            cout << receivedMat << endl;
            cout<<endl;
            cout<<K<<" "<<P<<endl;
            genSeed();
            cout<<seed<<endl;
            sendSeedToProcessors();
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
