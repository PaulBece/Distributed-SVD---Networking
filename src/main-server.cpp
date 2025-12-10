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
#include <cerrno> 
#include <cstring>

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
mutex Mutex2;
mutex Mutex3;
mutex mapMutex;

Eigen::MatrixXd U_distributed;
Eigen::VectorXd Sigma;
Eigen::MatrixXd VT;

void writeN(int Socket, void * data, int size){
    int bytes_left=size;
    int offset=0;
    while (bytes_left>0){
        ssize_t bytes_written = write (Socket,((char*)data)+offset,bytes_left);
        if (bytes_written < 0) {
            // CHEQUEO DE ERRORES RECUPERABLES
            if (errno == EINTR) {
                // Fue una interrupción del sistema (señal), intentamos de nuevo inmediatamente
                continue; 
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // El socket no tiene datos AHORA, pero sigue vivo. Esperamos un poco.
                cout<<"Me meto CHIZZ en writeN"<<endl;
                usleep(1000); // Esperar 1ms
                continue;
            }
        }
        else{
            offset+=bytes_written;
            bytes_left-=bytes_written;
            //usleep(1);
        }
    }
}

bool readN2(int Socket, void * data, int size){
    int bytes_left = size;
    int offset = 0;
    
    while(bytes_left > 0){
        ssize_t bytes_read = read(Socket, ((char *)data) + offset, bytes_left);

        if (bytes_read < 0) {
            // CHEQUEO DE ERRORES RECUPERABLES
            if (errno == EINTR) {
                // Fue una interrupción del sistema (señal), intentamos de nuevo inmediatamente
                continue; 
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // El socket no tiene datos AHORA, pero sigue vivo. Esperamos un poco.
                cout<<"Me meto CHIZZ en readN2"<<endl;
                usleep(1000); // Esperar 1ms
                continue;
            }
            
            // Error fatal real
            cerr << "Error fatal en socket: " << strerror(errno) << endl;
            return false; 
        }
        else if (bytes_read == 0) {
            // Desconexión (EOF)
            cerr << "El otro extremo cerro la conexion." << endl;
            return false;
        }

        bytes_left -= bytes_read;
        offset += bytes_read;
    }
    return true;
}

void signalHandler(int signum) {
    cout << "\nInterrupcion detectada (Ctrl+C). Apagando servidor..." << endl;
    
    if (globalServerSocket != -1) {
        shutdown(globalServerSocket, SHUT_RDWR);
        close(globalServerSocket);
        cout << "Socket del servidor cerrado." << endl;
    }
    
    exit(signum);
}

int GetSize (ThreadData &thread){
    thread.readBuffer.resize(sizeof(int));
    readN2(thread.SocketClient,thread.readBuffer.data(),sizeof(int));
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

    writeN(thread.SocketClient,thread.writeBuffer.data(),thread.writeBuffer.size());
    thread.log_buffer=thread.writeBuffer;
    cout<<"Sent: "<<thread.log_buffer<<endl;
}

void sendSeedToProcessors(){
    for (auto processor:connectedProcessors){
        sendSeedKP(*(processor.second));
    }
}

void sendChunkOfMatrix(ThreadData thread,int nProcessor){
    cout<< "sendChunkOfMatrix"<<endl;
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
        writeN(thread.SocketClient,thread.writeBuffer.data(),thread.writeBuffer.size());

        Eigen::MatrixXd aux=receivedMat.block(startRow,0,lastProcessor,nCols);
        writeN(thread.SocketClient,aux.data(),aux.size()*sizeof(double));
        //cout<<aux<<endl;
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
        writeN(thread.SocketClient,thread.writeBuffer.data(),thread.writeBuffer.size());

        Eigen::MatrixXd aux=receivedMat.block(startRow,0,nRowsPerProcessor,nCols);
        writeN(thread.SocketClient,aux.data(),aux.size()*sizeof(double));
        //cout<<aux<<endl;
    }
}

void assingWork(){
    // 1. CREAR SNAPSHOT (COPIA SEGURA)
    vector<ThreadData*> workersSnapshot;
    
    mapMutex.lock(); // Cerramos el mapa
    for (auto& kv : connectedProcessors){
        workersSnapshot.push_back(kv.second);
    }
    int currentTotalProcessors = connectedProcessors.size();
    mapMutex.unlock(); // Abrimos el mapa inmediatamente. ¡El bloqueo dura microsegundos!

    // 2. TRABAJAR CON LA COPIA (Sin miedo a Segfaults)
    int nProcessor = 0;
    
    // Iteramos sobre el VECTOR, no sobre el mapa
    for (ThreadData* worker : workersSnapshot){ 
        
        // Verificamos si el socket sigue vivo (opcional pero recomendado)
        // Nota: Si un worker se desconecta AHORA, fallará el writeN, pero NO crasheará el server (si ignoras SIGPIPE)
        
        worker->writeBuffer = "E";
        
        string value;
        value.resize(sizeof(int));
        memcpy(value.data(), &nProcessor, sizeof(int));
        worker->writeBuffer += value;

        value.resize(sizeof(int));
        memcpy(value.data(), &currentTotalProcessors, sizeof(int));
        worker->writeBuffer += value;

        worker->log_buffer = worker->writeBuffer;
        
        cout << "Sending to processor " << nProcessor << endl;
        
        // Enviar datos (esto es lo que toma tiempo)
        writeN(worker->SocketClient, worker->writeBuffer.data(), worker->writeBuffer.size());
        sendChunkOfMatrix(*worker, nProcessor);
        
        worker->numProcessor = nProcessor;
        nProcessor++;
    }
}

void stackMatrixR(bool U=false){
    int L = vecMatrix[0].cols();
    int totalRowsStack = 0;

    for(const auto& r : vecMatrix) {
        totalRowsStack += r.rows();
    }

    cout << "Apilando " << vecMatrix.size() << " matrices." << endl;
    if(U){
        cout << "Dimensiones finales de U: " << totalRowsStack << " x " << L << endl;
    }
    else{
        cout << "Dimensiones finales de R_stack: " << totalRowsStack << " x " << L << endl;
    }    
    
    R_stack= Eigen::MatrixXd (totalRowsStack, L);
    int currentRow = 0;
    for(const auto& r : vecMatrix) {
        R_stack.block(currentRow, 0, r.rows(), L) = r;
        currentRow += r.rows();
    }

    if(U){
        cout<<"U: "<<R_stack<<endl;
    }
    else{
        cout<<"R_stack: "<<R_stack<<endl;
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
        writeN(thread.SocketClient,thread.writeBuffer.data(),thread.writeBuffer.size());

        Eigen::MatrixXd aux=U_distributed.block(startRow,0,lastProcessor,nCols);
        writeN(thread.SocketClient,aux.data(),aux.size()*sizeof(double));
        cout<<"Sent: "<<aux<<endl;
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
        writeN(thread.SocketClient,thread.writeBuffer.data(),thread.writeBuffer.size());

        Eigen::MatrixXd aux=U_distributed.block(startRow,0,nRowsPerProcessor,nCols);
        writeN(thread.SocketClient,aux.data(),aux.size()*sizeof(double));
        cout<<"Sent: "<<aux<<endl;
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
    cout<<"Owo"<<endl;
    assingWorkU();
}
void sendMatrixToClient(Eigen::MatrixXd &matrix){
    int rows,cols;
    rows=matrix.rows();
    cols=matrix.cols();

    writeN(connectedClient,&rows,sizeof(int));
    writeN(connectedClient,&cols,sizeof(int));
    writeN(connectedClient,matrix.data(),matrix.size()* sizeof(double));
}

void sendVectorXDToClient(Eigen::VectorXd &Vector){
    int size= Vector.size();
    writeN(connectedClient,&size,sizeof(int));
    writeN(connectedClient,Vector.data(),Vector.size()* sizeof(double));
}

void readSocket(int SocketClient){
    ThreadData thread(SocketClient);
    bool flag1=true;

    while (flag1){
        thread.readBuffer.resize(1);
        flag1=readN2(SocketClient,thread.readBuffer.data(),1);
        thread.log_buffer=thread.readBuffer;

        switch (thread.readBuffer[0])
        {
        case 's':
            cout<<"Received:" <<thread.log_buffer<<endl;
        mapMutex.lock(); // <--- BLOQUEO
        connectedProcessors.insert(pair<int,ThreadData*>(SocketClient,&thread));
        mapMutex.unlock(); // <--- DESBLOQUEO
        break;
        
        case 'c':
            {
                cout<<"Received:" <<thread.log_buffer<<endl;
                if (connectedClient==0){
                    connectedClient=SocketClient;
                    thread.writeBuffer="C";
                    thread.log_buffer=thread.writeBuffer;
                    cout<<"Sent:" <<thread.log_buffer<<endl;
                    writeN(SocketClient,thread.writeBuffer.data(),thread.writeBuffer.size());
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
                    writeN(SocketClient,thread.writeBuffer.data(),thread.writeBuffer.size());
                }
            }
            break;

        case 'q':
            mapMutex.lock(); // <--- BLOQUEO
        if (connectedProcessors.find(SocketClient)!=connectedProcessors.end()){
            connectedProcessors.erase(SocketClient);
        }
        mapMutex.unlock(); // <--- DESBLOQUEO
        flag1=false;
        break;
        case 'f': 
            {
            int rows = GetSize(thread);
            int cols = GetSize(thread);
            cout<<"Received:" <<thread.log_buffer<<endl;
            int datasize = rows * cols * sizeof(double);
            receivedMat=Eigen::MatrixXd (rows, cols);
            readN2(SocketClient, receivedMat.data(), datasize);
            readN2(SocketClient,&K,sizeof(int));
            readN2(SocketClient,&P,sizeof(int));
            cout << "Matriz Recibida:" << endl;
            //cout << receivedMat << endl;
            cout<<endl;
            cout<<"K and P: "<<K<<" "<<P<<endl;
            genSeed();
            cout<<"Seed: "<<seed<<endl;
            sendSeedToProcessors();
            assingWork();
            vecMatrix.resize(connectedProcessors.size());
            break;
            }
        case 'o':
        {
            int rows,cols;
            readN2(SocketClient,&rows,sizeof(int));
            readN2(SocketClient,&cols,sizeof(int));
            Eigen::MatrixXd R_k ( rows , cols);
            readN2 (SocketClient, R_k.data(), rows*cols*sizeof(double));
            Mutex.lock();
            vecMatrix[thread.numProcessor]=R_k;
            n_R++;
            Mutex.unlock();
            
            if(n_R==connectedProcessors.size()){
                n_R=0;
                stackMatrixR();
                SVD();
            }
            break;
        }
        case 'a':{
            cout<<"Case a:"<<endl;
            cout<<"n_R: "<<n_R<<" from processor: "<<thread.numProcessor<<"\n";
            int rows,cols;
            readN2(SocketClient,&rows,sizeof(int));
            readN2(SocketClient,&cols,sizeof(int));
            Eigen::MatrixXd UTA_k ( rows , cols);
            readN2 (SocketClient, UTA_k.data(), rows*cols*sizeof(double));
            Mutex.lock();
            if (vecMatrix.size()==0){
                cout<<"From mutex\nSize of vecMatrix: "<<vecMatrix.size()<<endl;
                cout<<"RESIZE\n";
                vecMatrix.resize(connectedProcessors.size());
                cout<<"Size of vecMatrix: "<<vecMatrix.size()<<endl;
                cout<<"Size of connectecProcessors: "<<connectedProcessors.size()<<endl;
            }
            n_R++;
            vecMatrix[thread.numProcessor]=UTA_k;
            if (n_R==connectedProcessors.size()){
                Mutex2.unlock();
            }
            cout<<"Before leaving mutex n_R: "<<n_R<<" from processor: "<<thread.numProcessor<<"\n";
            Mutex.unlock();
            if(thread.numProcessor+1==connectedProcessors.size()){
                Mutex2.lock();
                VT= vecMatrix[0];
                for (int i = 1; i < vecMatrix.size(); i++)
                {
                    VT+= vecMatrix[i];
                }
                Eigen::MatrixXd Sigma_Inverse = Sigma.asDiagonal().inverse();
                VT = Sigma_Inverse * VT; //Ojito FE
                cout<<"VT: "<<endl;
                cout<<VT<<endl;
                n_R=0;
                vecMatrix.clear();
                Mutex3.unlock();
            } 
            Mutex3.lock();
            Mutex3.unlock();
            break;
        }
        case 'u':
        {
            cout<<"n_R: "<<n_R<<" from processor: "<<thread.numProcessor<<"\n";
            int rows,cols;
            readN2(SocketClient,&rows,sizeof(int));
            readN2(SocketClient,&cols,sizeof(int));
            Eigen::MatrixXd U_k ( rows , cols);
            readN2 (SocketClient, U_k.data(), rows*cols*sizeof(double));

            Mutex.lock();
            if (vecMatrix.size()==0){
                cout<<"From mutex\nSize of vecMatrix: "<<vecMatrix.size()<<endl;
                cout<<"RESIZE\n";
                vecMatrix.resize(connectedProcessors.size());
                cout<<"Size of vecMatrix: "<<vecMatrix.size()<<endl;
                cout<<"Size of connectecProcessors: "<<connectedProcessors.size()<<endl;
            }
            vecMatrix[thread.numProcessor]=U_k;
            n_R++;
            cout<<"Before leaving mutex n_R: "<<n_R<<" from processor: "<<thread.numProcessor<<"\n";
            Mutex.unlock();

            if (n_R==connectedProcessors.size()){
                Mutex2.unlock();
            }

            if(thread.numProcessor+1==connectedProcessors.size()){
                Mutex2.lock();
                cout<<"inside if n_R: "<<n_R<<" from processor: "<<thread.numProcessor<<"\n";
                stackMatrixR(true);
                thread.writeBuffer="K";
                writeN(connectedClient,thread.writeBuffer.data(),thread.writeBuffer.size());
                cout<<"sent K\n";
                sendMatrixToClient(R_stack);
                thread.writeBuffer="L";
                writeN(connectedClient,thread.writeBuffer.data(),thread.writeBuffer.size());
                cout<<"sent L\n";
                sendVectorXDToClient(Sigma);
                thread.writeBuffer="M";
                writeN(connectedClient,thread.writeBuffer.data(),thread.writeBuffer.size());
                cout<<"sent M\n";
                sendMatrixToClient(VT);
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

    Mutex2.lock();
    Mutex3.lock();
    vector<thread> threads;

    while (1){
        int SocketClient = accept(SocketServer, NULL, NULL);
        threads.push_back(thread (readSocket,SocketClient));
    }
    shutdown(SocketServer, SHUT_RDWR);
    close(SocketServer);

    return 0;

}
