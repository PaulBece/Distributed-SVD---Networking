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
#include <csignal>

#define PORT 8080

using namespace std;

// <--- 2. Variable Global para poder accederla desde el signal handler
int globalSocketClient = -1; 

// <--- 3. Función que maneja el Ctrl + C
void signalHandler(int signum) {
    cout << "\nInterrupcion detectada (Ctrl+C). Cerrando cliente..." << endl;
    
    if (globalSocketClient != -1) {
        // Intentar avisar al servidor que nos vamos
        string msg = "q";
        write(globalSocketClient, msg.data(), msg.size());
        
        // Cerrar el socket
        shutdown(globalSocketClient, SHUT_RDWR);
        close(globalSocketClient);
        cout << "Socket cerrado correctamente." << endl;
    }
    
    exit(signum); // Terminar el programa
}

Eigen::MatrixXd readCSV(const std::string &path) {
    std::ifstream indata;
    indata.open(path);
    
    if (!indata.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo " << path << std::endl;
        // Devuelve una matriz vacía o maneja el error según prefieras
        return Eigen::MatrixXd(0, 0); 
    }

    std::string line;
    std::vector<double> values;
    int rows = 0;

    while (std::getline(indata, line)) {
        std::stringstream lineStream(line);
        std::string cell;
        while (std::getline(lineStream, cell, ',')) {
            // Convertir string a double y guardar
            values.push_back(std::stod(cell));
        }
        rows++;
    }
    
    // Calcular columnas basándonos en el total de elementos y filas
    if (rows == 0) return Eigen::MatrixXd(0,0);
    int cols = values.size() / rows;

    // Mapear el vector plano a una Matriz Eigen.
    // Importante: Usamos RowMajor porque C++ lee línea por línea (fila por fila),
    // pero Eigen por defecto es ColumnMajor.
    return Eigen::Map<const Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>(values.data(), rows, cols);
}

string CastIntToString(int n){
    string tmp;
    tmp.resize(sizeof(int));
    char *OwO= (char*)&n;
    for ( int i = 0; i < sizeof(int); i++)
    {
        tmp[i] = *(OwO + i);
    }
    return tmp;
}
int GetSize (int SocketClient,string &buffer){
    buffer.resize(sizeof(int));
    read(SocketClient,buffer.data(),sizeof(int));
    int* aux=(int*)buffer.data();
    int size=*aux;
    return size;
}

void sendMatrix(int SocketClient) {
    string filepath;
    int K,P;
    cout << "Enter file name: ";
    getline(cin, filepath);
    cout << "Enter K value: ";
    cin>>K;
    cout << "Enter P value: ";
    cin>>P;
    cout<<K<<" "<<P<<endl;
    filepath="data/"+filepath;
    Eigen::MatrixXd owo = readCSV(filepath);
    int rows = owo.rows();
    int cols = owo.cols();
    string header = "f";
    write(SocketClient, header.data(), header.size());
    write(SocketClient, &rows, sizeof(int));
    write(SocketClient, &cols, sizeof(int));
    write(SocketClient, owo.data(), owo.size() * sizeof(double));
    write(SocketClient,&K,sizeof(int));
    write(SocketClient,&P,sizeof(int));
}

int main(int argc, char * argv[]){
    signal(SIGINT, signalHandler);
    int portNumber;

    if (argc>=2){
        portNumber=atoi(argv[1]);
    }
    else{
        portNumber=PORT;
    }


    struct sockaddr_in stSockAddr;
    int SocketClient = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    globalSocketClient = SocketClient;

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

    string writeBuffer="c";
    string readBuffer;
    readBuffer.resize(1);

    write(SocketClient,writeBuffer.data(),writeBuffer.size());

    read(SocketClient,readBuffer.data(),1);

    if (readBuffer=="C"){
        cout << "Successfull Connection" << endl;
    }
    else if (readBuffer=="X"){
        int size = GetSize(SocketClient, readBuffer);
        readBuffer.resize(size);
        read(SocketClient,readBuffer.data(),size);
        cout<<readBuffer<<endl;
        writeBuffer="q";
        write(SocketClient,writeBuffer.data(),writeBuffer.size());
        close(SocketClient);
    }
    sendMatrix(SocketClient);

    while (1){
        sleep(10);
    };
    return 0;
}