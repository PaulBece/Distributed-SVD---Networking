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
#include <fstream> 
#include <iomanip>  
#include <sys/stat.h> 
#include <sys/types.h>

#define PORT 8080

using namespace std;
Eigen::MatrixXd  owo;
int globalSocketClient = -1; 

void ensureFolderExists(const std::string& folderName) {
    struct stat st = {0};
    if (stat(folderName.c_str(), &st) == -1) {
        if (mkdir(folderName.c_str(), 0777) == 0) {
            std::cout << "Carpeta '" << folderName << "' creada exitosamente." << std::endl;
        } else {
            std::cerr << "Error: No se pudo crear la carpeta '" << folderName << "'" << std::endl;
        }
    }
}

void writeToCSV(const std::string& filename, const Eigen::MatrixXd& matrix) {
    std::ofstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo crear el archivo " << filename << std::endl;
        return;
    }

    file << std::scientific << std::setprecision(15);

    for (int i = 0; i < matrix.rows(); ++i) {
        for (int j = 0; j < matrix.cols(); ++j) {
            file << matrix(i, j);
            if (j < matrix.cols() - 1) file << ",";
        }
        file << "\n";
    }
    file.close();
    std::cout << "Guardado: " << filename << std::endl;
}

void saveSVDResults(const Eigen::MatrixXd& U, 
                    const Eigen::VectorXd& Sigma, 
                    const Eigen::MatrixXd& VT) {
    
    std::string folder = "output";
    std::cout << "\n--- GUARDANDO RESULTADOS EN '" << folder << "/' ---" << std::endl;
    ensureFolderExists(folder);
    writeToCSV(folder + "/U.csv", U);
    writeToCSV(folder + "/Sigma.csv", Sigma);
    writeToCSV(folder + "/VT.csv", VT);
    std::cout << "-------------------------------------" << std::endl;
}

void signalHandler(int signum) {
    cout << "\nInterrupcion detectada (Ctrl+C). Cerrando cliente..." << endl;
    
    if (globalSocketClient != -1) {
        string msg = "q";
        write(globalSocketClient, msg.data(), msg.size());
        shutdown(globalSocketClient, SHUT_RDWR);
        close(globalSocketClient);
        cout << "Socket cerrado correctamente." << endl;
    }
    exit(signum); 
}

void readN2(int Socket, void * data, int size){
    int n= size;
    int tmp2=n; 
    int tmp3=0;
    while(tmp2>0){
        int c;
        c=read(Socket,((char *)data)+tmp3,min(1000,tmp2));
        tmp2-=c;
        tmp3+=c;
    }
}

Eigen::MatrixXd readCSV(const std::string &path) {
    std::ifstream indata;
    indata.open(path);
    
    if (!indata.is_open()) {
        std::cerr << "Error: No se pudo abrir el archivo " << path << std::endl;
        return Eigen::MatrixXd(0, 0); 
    }

    std::string line;
    std::vector<double> values;
    int rows = 0;

    while (std::getline(indata, line)) {
        std::stringstream lineStream(line);
        std::string cell;
        while (std::getline(lineStream, cell, ',')) {
            values.push_back(std::stod(cell));
        }
        rows++;
    }

    if (rows == 0) return Eigen::MatrixXd(0,0);
    int cols = values.size() / rows;
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
    readN2(SocketClient,buffer.data(),sizeof(int));
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
    owo = readCSV(filepath);
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

Eigen::MatrixXd getMatrix(int SocketClient, string &readBuffer){
    int rows,cols;
    readN2(SocketClient,&rows,sizeof(int));
    readN2(SocketClient,&cols,sizeof(int));
    cout<<readBuffer<< " "<< rows<< " "<< cols<<endl;
    Eigen::MatrixXd matrix (rows,cols);

    readN2(SocketClient,matrix.data(),matrix.size()*sizeof(double));

    return matrix;
}

Eigen::VectorXd getVectorXD(int SocketClient,string &readBuffer){
    int size;
    readN2(SocketClient,&size,sizeof(int));
    cout<<readBuffer<< " "<< size<<endl;
    Eigen::VectorXd Vector (size);

    readN2(SocketClient,Vector.data(),Vector.size()*sizeof(double));

    return Vector;
}

void calculateAndPrintPrecision(const Eigen::MatrixXd &A, 
                                const Eigen::MatrixXd &U, 
                                const Eigen::VectorXd &Sigma, 
                                const Eigen::MatrixXd &VT) {
    
    cout << "\n--- CALCULANDO PRECISION DEL SVD ---" << endl;

    Eigen::MatrixXd A_approx = U * Sigma.asDiagonal() * VT;

    Eigen::MatrixXd Diff = A - A_approx;

    double errorNorm = Diff.norm();
    double originalNorm = A.norm();

    double relativeError = 0.0;
    if (originalNorm > 1e-9) { 
        relativeError = errorNorm / originalNorm;
    }

    cout << "Dimensiones Originales: " << A.rows() << "x" << A.cols() << endl;
    cout << "Dimensiones Reconstruidas: " << A_approx.rows() << "x" << A_approx.cols() << endl;
    cout << "-----------------------------------" << endl;
    cout << "Norma de la Matriz Original: " << originalNorm << endl;
    cout << "Norma del Error (Frobenius): " << errorNorm << endl;
    cout << "Error Relativo: " << (relativeError * 100.0) << " %" << endl;
    cout << "Precision (1 - Error): " << ((1.0 - relativeError) * 100.0) << " %" << endl;
    

    if (A.rows() <= 10 && A.cols() <= 10) {
        cout << "\nComparativa Visual (Primeras filas):" << endl;
        cout << "ORIGINAL:\n" << A << endl;
        cout << "RECONSTRUIDA:\n" << A_approx << endl;
    }
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

    readN2(SocketClient,readBuffer.data(),1);

    if (readBuffer=="C"){
        cout << "Successfull Connection" << endl;
    }
    else if (readBuffer=="X"){
        int size = GetSize(SocketClient, readBuffer);
        readBuffer.resize(size);
        readN2(SocketClient,readBuffer.data(),size);
        cout<<readBuffer<<endl;
        writeBuffer="q";
        write(SocketClient,writeBuffer.data(),writeBuffer.size());
        close(SocketClient);
    }
    sendMatrix(SocketClient);
    Eigen::MatrixXd U;
    Eigen::VectorXd Sigma;
    Eigen::MatrixXd VT;
    readBuffer.resize(1);
    readN2(SocketClient,readBuffer.data(),readBuffer.size());
    if(readBuffer=="K"){
        U=getMatrix(SocketClient,readBuffer);
        cout<<"U"<<endl;
        cout<<U<<endl;
    }
    readN2(SocketClient,readBuffer.data(),readBuffer.size());
    if(readBuffer=="L"){
        Sigma= getVectorXD(SocketClient,readBuffer);
        cout<<"Sigma:"<<endl;
        cout<<Sigma<<endl;
    }
    readN2(SocketClient,readBuffer.data(),readBuffer.size());
    if(readBuffer=="M"){
        VT=getMatrix(SocketClient,readBuffer);
        cout<<"VT"<<endl;
        cout<<VT<<endl;
    }

    calculateAndPrintPrecision(owo, U, Sigma,VT);
    saveSVDResults(U, Sigma, VT);
    return 0;
}