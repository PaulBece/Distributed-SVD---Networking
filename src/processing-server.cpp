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
#include <random>
#define PORT 8080

using namespace std;

int seed;
int K,P,L;
int nProcessor,nProcessors;

// Función corregida y robusta para redes reales
void readN2(int Socket, void * data, int size){
    int bytes_left = size;
    int offset = 0;
    
    while(bytes_left > 0){
        // 1. Quitamos el min(1000) para que lea todo lo que pueda (más rápido)
        // 2. Usamos recv en lugar de read (es mejor para sockets), aunque read funciona igual.
        ssize_t bytes_read = read(Socket, ((char *)data) + offset, bytes_left);

        // 3. MANEJO DE ERRORES CRÍTICO
        if (bytes_read <= 0) {
            // Si devuelve 0 (desconexión) o -1 (error), no podemos seguir.
            // En un sistema real deberíamos lanzar excepción o retornar false,
            // pero para evitar el Core Dump inmediato, rompemos el ciclo o imprimimos error.
            cerr << "Error critico en socket (read devolvio " << bytes_read << ")" << endl;
            break; // Salimos para no corromper memoria
        }

        bytes_left -= bytes_read;
        offset += bytes_read;
    }
}

void generateOmega_identical(int N, int l, unsigned long long seed, Eigen::MatrixXd &Omega) {
    Omega.resize(N, l);
    std::mt19937_64 rng(seed);
    std::normal_distribution<double> gauss(0.0, 1.0);

    for (int j = 0; j < l; ++j) {
        for (int i = 0; i < N; ++i) {
            Omega(i, j) = gauss(rng);
        }
    }
}

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

    readN2(SocketClient,readBuffer.data(),readBuffer.size());

    if (readBuffer=="G"){
        readN2(SocketClient,&seed,sizeof(int));
        readN2(SocketClient,&K,sizeof(int));
        readN2(SocketClient,&P,sizeof(int));
    }
    L=K+P;

    cout<<seed<<" "<< K<<" " << P<<endl;

    readBuffer.resize(1);

    readN2(SocketClient,readBuffer.data(),readBuffer.size());

    if (readBuffer=="E"){
        readN2(SocketClient,&nProcessor,sizeof(int));
        readN2(SocketClient,&nProcessors,sizeof(int));
                cout<<readBuffer<< " "<< nProcessor << " "<< nProcessors<<endl;
    }
    readBuffer.resize(1);

    readN2(SocketClient,readBuffer.data(),readBuffer.size());

    int rows,cols;
    Eigen::MatrixXd A_R;
    if (readBuffer=="T"){

        readN2(SocketClient,&rows,sizeof(int));
        readN2(SocketClient,&cols,sizeof(int));
        cout<<readBuffer<< " "<< rows << " "<< cols<<endl;
        A_R=Eigen::MatrixXd (rows,cols);

        readN2(SocketClient,A_R.data(),A_R.size()*sizeof(double));

        cout<<A_R<<endl;
    }
    Eigen::MatrixXd Omega;
    generateOmega_identical(cols, min(P + K, cols), seed, Omega);
    cout<<"Omega:"<<endl;
    cout<<Omega<<endl;
    Eigen::MatrixXd Y_local = A_R * Omega;
    cout<< "Y_R: "<< endl;
    cout<< Y_local<<endl;
    cout << "Computed Y_local of size " << Y_local.rows() << " x " << Y_local.cols() << endl;

    Eigen::HouseholderQR<Eigen::MatrixXd> qr_local(Y_local);
    int validRows = std::min((int)Y_local.rows(), L);
    Eigen::MatrixXd R0 = qr_local.matrixQR().topRows(validRows).template triangularView<Eigen::Upper>();
    cout << "Computed local R0 (size " << R0.rows() << "x" << R0.cols() << ")\n";
    cout<< "R: "<< endl;
    cout<< R0<<endl;
    int R0_rows=R0.rows();
    int R0_cols=R0.cols();
    writeBuffer="o";
    write(SocketClient,writeBuffer.data(),writeBuffer.size());
    write(SocketClient,&R0_rows,sizeof(int));
    write(SocketClient,&R0_cols,sizeof(int));
    write(SocketClient,R0.data(),R0.size()*sizeof(double));
    Eigen::MatrixXd Q_thin = qr_local.householderQ() * Eigen::MatrixXd::Identity(Y_local.rows(), validRows);


    readBuffer.resize(1);
    readN2(SocketClient,readBuffer.data(),readBuffer.size());
    int rowsU,colsU;
    Eigen::MatrixXd U_part;
    
    if (readBuffer=="U"){

        readN2(SocketClient,&rowsU,sizeof(int));
        readN2(SocketClient,&colsU,sizeof(int));
        cout<<readBuffer<< " "<< rowsU << " "<< colsU<<endl;
        U_part=Eigen::MatrixXd (rowsU,colsU);

        readN2(SocketClient,U_part.data(),U_part.size()*sizeof(double));
        cout<< "U_part: "<<endl;
        cout<<U_part<<endl;
        Eigen::MatrixXd U_resultante = Q_thin * U_part;
        cout<< "U_Resultante: "<<endl;
        cout << U_resultante<< endl;

        Eigen::MatrixXd UTA =  U_resultante.transpose() * A_R;
        int UTA_rows=UTA.rows();
        int UTA_cols=UTA.cols();
        writeBuffer="a";
        write(SocketClient,writeBuffer.data(),writeBuffer.size());
        write(SocketClient,&UTA_rows,sizeof(int));
        write(SocketClient,&UTA_cols,sizeof(int));
        write(SocketClient,UTA.data(),UTA.size()*sizeof(double));


        int U_rows=U_resultante.rows();
        int U_cols=U_resultante.cols();
        writeBuffer="u";
        write(SocketClient,writeBuffer.data(),writeBuffer.size());
        write(SocketClient,&U_rows,sizeof(int));
        write(SocketClient,&U_cols,sizeof(int));
        write(SocketClient,U_resultante.data(),U_resultante.size()*sizeof(double));
 
    }
    while(1){
        sleep(1000);
    }
    writeBuffer= "q";
    write(SocketClient,writeBuffer.data(),writeBuffer.size());
    shutdown(SocketClient, SHUT_RDWR);
    close(SocketClient);
    return 0;
}