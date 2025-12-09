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
#include <random>
#include <mutex>

#define PORT 8080

using namespace std;

struct ThreadData{
    string writeBuffer;
    string readBuffer;
    int SocketClient;
    string log_buffer;
    int numProcessor=-1;
    ThreadData(int SocketClient): SocketClient(SocketClient){}

};

unordered_map<int,ThreadData*> connectedProcessors;
int connectedClient=0;
int globalServerSocket = -1;

int seed;

int K,P;

vector<Eigen::MatrixXd> vecMatrix;

Eigen::MatrixXd receivedMat;
Eigen::MatrixXd R_stack;

int n_R =0;
mutex Mutex;

Eigen::MatrixXd U_distributed;
Eigen::VectorXd Sigma;
Eigen::MatrixXd VT;

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

void sendSeedKP(ThreadData &thread){
    string value;
    value.resize(sizeof(int));
    for (int i=0;i<sizeof(int);i++){
        value[i]=*(((char*)&seed)+i);
    }
    thread.writeBuffer="G"+value;
    for (int i=0;i<sizeof(int);i++){
        value[i]=*(((char*)&K)+i);
    }
    thread.writeBuffer+=value;
    for (int i=0;i<sizeof(int);i++){
        value[i]=*(((char*)&P)+i);
    }
    thread.writeBuffer+=value;

    write(thread.SocketClient,thread.writeBuffer.data(),thread.writeBuffer.size());
    thread.log_buffer=thread.writeBuffer;
    cout<<thread.log_buffer<<endl;
}

void sendSeedToProcessors(){
    for (auto processor:connectedProcessors){
        sendSeedKP(*(processor.second));
    }
}

void sendChunkOfMatrix(ThreadData thread,int nProcessor){
    int nProcessors=connectedProcessors.size();
    int nRowsPerProcessor=receivedMat.rows()/nProcessors;
    int lastProcessor=receivedMat.rows()%nProcessors+nRowsPerProcessor;
    int startRow;
    startRow=nRowsPerProcessor*nProcessor;
    int nCols=receivedMat.cols();

    if (nProcessor==nProcessors-1){
        thread.writeBuffer="T";
        string value;
        value.resize(sizeof(int));
        for (int i=0;i<sizeof(int);i++){
            value[i]=*(((char*)&lastProcessor)+i);
        }
        thread.writeBuffer=thread.writeBuffer+value;

        for (int i=0;i<sizeof(int);i++){
            value[i]=*(((char*)&(nCols))+i);
        }
        thread.writeBuffer=thread.writeBuffer+value;
        thread.log_buffer=thread.writeBuffer;
        cout<<thread.log_buffer;
        write(thread.SocketClient,thread.writeBuffer.data(),thread.writeBuffer.size());

        Eigen::MatrixXd aux=receivedMat.block(startRow,0,lastProcessor,nCols);
        write(thread.SocketClient,aux.data(),aux.size()*sizeof(double));
        cout<<aux;
    }
    else{
        thread.writeBuffer="T";
        string value;
        value.resize(sizeof(int));
        for (int i=0;i<sizeof(int);i++){
            value[i]=*(((char*)&nRowsPerProcessor)+i);
        }
        thread.writeBuffer=thread.writeBuffer+value;

        for (int i=0;i<sizeof(int);i++){
            value[i]=*(((char*)&(nCols))+i);
        }
        thread.writeBuffer=thread.writeBuffer+value;
        thread.log_buffer=thread.writeBuffer;
        cout<<thread.log_buffer;
        write(thread.SocketClient,thread.writeBuffer.data(),thread.writeBuffer.size());

        Eigen::MatrixXd aux=receivedMat.block(startRow,0,nRowsPerProcessor,nCols);
        write(thread.SocketClient,aux.data(),aux.size()*sizeof(double));
        cout<<aux;
    }
}

void assingWork(){
    int nProcessors=connectedProcessors.size();
    int nProcessor=0;
    for (auto processor:connectedProcessors){

        processor.second->writeBuffer="E";
        string value;
        value.resize(sizeof(int));
        for (int i=0;i<sizeof(int);i++){
            value[i]=*(((char*)&nProcessor)+i);
        }
        processor.second->writeBuffer=processor.second->writeBuffer+value;
        for (int i=0;i<sizeof(int);i++){
            value[i]=*(((char*)&nProcessors)+i);
        }
        
        processor.second->writeBuffer=processor.second->writeBuffer+value;
        processor.second->log_buffer=processor.second->writeBuffer;
        cout<< processor.second->writeBuffer<<endl;

        cout<< nProcessor << " "<< value<< endl;
        cout<<"Sent:" <<processor.second->log_buffer<<endl;
        write(processor.second->SocketClient,processor.second->writeBuffer.data(),processor.second->writeBuffer.size());
        sendChunkOfMatrix(*(processor.second),nProcessor);
        processor.second->numProcessor=nProcessor;
        nProcessor++;
    }
}

void stackMatrixR(){
    int L = vecMatrix[0].cols(); // El ancho L es constante para todos
    int totalRowsStack = 0;

    for(const auto& r : vecMatrix) {
        totalRowsStack += r.rows();
    }

    cout << "Apilando " << vecMatrix.size() << " matrices." << endl;
    cout << "Dimensiones finales de R_stack: " << totalRowsStack << " x " << L << endl;
    R_stack= Eigen::MatrixXd (totalRowsStack, L);
    int currentRow = 0;
    for(const auto& r : vecMatrix) {
        R_stack.block(currentRow, 0, r.rows(), L) = r;
        currentRow += r.rows();
    }

    vecMatrix.clear();
}



void sendU(ThreadData &thread){
    int nProcessors=connectedProcessors.size();
    int nRowsPerProcessor=U_distributed.rows()/nProcessors;
    int lastProcessor=U_distributed.rows()%nProcessors+nRowsPerProcessor;
    int startRow;
    startRow=nRowsPerProcessor*thread.numProcessor;
    int nCols=U_distributed.cols();

    if (thread.numProcessor==nProcessors-1){
        string value;
        value.resize(sizeof(int));
        for (int i=0;i<sizeof(int);i++){
            value[i]=*(((char*)&lastProcessor)+i);
        }
        thread.writeBuffer=thread.writeBuffer+value;

        for (int i=0;i<sizeof(int);i++){
            value[i]=*(((char*)&(nCols))+i);
        }
        thread.writeBuffer=thread.writeBuffer+value;
        thread.log_buffer=thread.writeBuffer;
        cout<<thread.log_buffer;
        write(thread.SocketClient,thread.writeBuffer.data(),thread.writeBuffer.size());

        Eigen::MatrixXd aux=U_distributed.block(startRow,0,lastProcessor,nCols);
        write(thread.SocketClient,aux.data(),aux.size()*sizeof(double));
        cout<<aux;
    }
    else{
        string value;
        value.resize(sizeof(int));
        for (int i=0;i<sizeof(int);i++){
            value[i]=*(((char*)&nRowsPerProcessor)+i);
        }
        thread.writeBuffer=thread.writeBuffer+value;

        for (int i=0;i<sizeof(int);i++){
            value[i]=*(((char*)&(nCols))+i);
        }
        thread.writeBuffer=thread.writeBuffer+value;
        thread.log_buffer=thread.writeBuffer;
        cout<<thread.log_buffer;
        write(thread.SocketClient,thread.writeBuffer.data(),thread.writeBuffer.size());

        Eigen::MatrixXd aux=U_distributed.block(startRow,0,nRowsPerProcessor,nCols);
        write(thread.SocketClient,aux.data(),aux.size()*sizeof(double));
        cout<<aux;
    }
}

void assingWorkU(){
    for (auto processor:connectedProcessors){
        processor.second->writeBuffer="U";
        sendU(*(processor.second));
    }
}

void SVD(){
    Eigen::BDCSVD<Eigen::MatrixXd> svd(R_stack, Eigen::ComputeThinU | Eigen::ComputeThinV);

    U_distributed = svd.matrixU();
    Sigma = svd.singularValues();
    VT = svd.matrixV().transpose();
    cout<<"Owo"<<endl;
    assingWorkU();
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
                connectedProcessors.erase(SocketClient);
            }
            flag1=false;
            break;
        case 'f': 
            {
            int rows = GetSize(thread);
            int cols = GetSize(thread);
            cout<<"Received:" <<thread.log_buffer<<endl;
            int datasize = rows * cols * sizeof(double);
            receivedMat=Eigen::MatrixXd (rows, cols);
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
            assingWork();
            vecMatrix.resize(connectedProcessors.size());
            break;
            }
        case 'o':
        {
            int rows,cols;
            read(SocketClient,&rows,sizeof(int));
            read(SocketClient,&cols,sizeof(int));
            Eigen::MatrixXd R_k ( rows , cols);
            read (SocketClient, R_k.data(), rows*cols*sizeof(double));
            while(1){
                if(Mutex.try_lock()){
                n_R++;
                Mutex.unlock();
                break;
                }
                sleep(10);
            }
            vecMatrix[thread.numProcessor]=R_k;
            if(n_R==connectedProcessors.size()){
                stackMatrixR();
                cout<<"Unu"<<endl;
                SVD();
            }
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
