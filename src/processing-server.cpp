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

    read(SocketClient,readBuffer.data(),readBuffer.size());

    if (readBuffer=="G"){
        read(SocketClient,&seed,sizeof(int));
        read(SocketClient,&K,sizeof(int));
        read(SocketClient,&P,sizeof(int));
    }
    L=K+P;

    cout<<seed<<" "<< K<<" " << P<<endl;

    readBuffer.resize(1);

    read(SocketClient,readBuffer.data(),readBuffer.size());

    if (readBuffer=="E"){
        read(SocketClient,&nProcessor,sizeof(int));
        read(SocketClient,&nProcessors,sizeof(int));
                cout<<readBuffer<< " "<< nProcessor << " "<< nProcessors<<endl;
    }
    readBuffer.resize(1);

    read(SocketClient,readBuffer.data(),readBuffer.size());

    int rows,cols;
    Eigen::MatrixXd A_R;
    if (readBuffer=="T"){

        read(SocketClient,&rows,sizeof(int));
        read(SocketClient,&cols,sizeof(int));
        cout<<readBuffer<< " "<< rows << " "<< cols<<endl;
        A_R=Eigen::MatrixXd (rows,cols);

        read(SocketClient,A_R.data(),A_R.size()*sizeof(double));

        cout<<A_R<<endl;
    }
    Eigen::MatrixXd Omega;
    generateOmega_identical(cols, min(P + K, cols), seed, Omega);
    cout<<"Omega:"<<endl;
    cout<<Omega<<endl;

    cout << "Computed Y_local of size " << Y_local.rows() << " x " << Y_local.cols() << endl;

    // === Local QR: Y_local = Q0 * R0  (Q0: rows x l, R0: l x l) ===
    Eigen::HouseholderQR<Eigen::MatrixXd> qr_local(Y_local);
    // get R0 (l x l) from the top rows of the compact qr matrix
    Eigen::MatrixXd R0 = qr_local.matrixQR().topRows(l).template triangularView<Eigen::Upper>();
    // Optionally keep Q0 compact or full; we will re-orthonormalize after getting R_final,
    // so we don't need the full Q0 now. Keep Y_local to compute Z later.
    cout << "Computed local R0 (size " << R0.rows() << "x" << R0.cols() << ")\n";



    writeBuffer= "q";
    write(SocketClient,writeBuffer.data(),writeBuffer.size());
    shutdown(SocketClient, SHUT_RDWR);
    close(SocketClient);
    return 0;

}