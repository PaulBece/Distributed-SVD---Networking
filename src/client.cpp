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
#include <cerrno> // Necesario para errno
#include <cstring>

#define PORT 8080

using namespace std;
Eigen::MatrixXd  owo;
// <--- 2. Variable Global para poder accederla desde el signal handler
int globalSocketClient = -1; 


void writeN(int Socket, void * data, int size){
    int bytes_left=size;
    int offset=0;
    while (bytes_left>0){
        ssize_t bytes_written = write (Socket,((char*)data)+offset,min(1000,bytes_left));
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


// <--- 3. Función que maneja el Ctrl + C
void signalHandler(int signum) {
    cout << "\nInterrupcion detectada (Ctrl+C). Cerrando cliente..." << endl;
    
    if (globalSocketClient != -1) {
        // Intentar avisar al servidor que nos vamos
        string msg = "q";
        writeN(globalSocketClient, msg.data(), msg.size());
        
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
    writeN(SocketClient, header.data(), header.size());
    writeN(SocketClient, &rows, sizeof(int));
    writeN(SocketClient, &cols, sizeof(int));
    writeN(SocketClient, owo.data(), owo.size() * sizeof(double));
    writeN(SocketClient,&K,sizeof(int));
    writeN(SocketClient,&P,sizeof(int));
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

    // 1. Reconstruir la matriz aproximada: A' = U * Sigma * VT
    // Sigma es un vector, usamos .asDiagonal() para hacerlo matriz cuadrada
    Eigen::MatrixXd A_approx = U * Sigma.asDiagonal() * VT;

    // 2. Calcular la matriz de diferencia (Residuos)
    Eigen::MatrixXd Diff = A - A_approx;

    // 3. Calcular la Norma de Frobenius (La magnitud del error)
    // Eigen .norm() calcula la norma de Frobenius por defecto
    double errorNorm = Diff.norm();
    double originalNorm = A.norm();

    // 4. Calcular el Error Relativo (Más útil para entender el porcentaje)
    double relativeError = 0.0;
    if (originalNorm > 1e-9) { // Evitar división por cero
        relativeError = errorNorm / originalNorm;
    }

    // 5. Mostrar resultados
    cout << "Dimensiones Originales: " << A.rows() << "x" << A.cols() << endl;
    cout << "Dimensiones Reconstruidas: " << A_approx.rows() << "x" << A_approx.cols() << endl;
    cout << "-----------------------------------" << endl;
    cout << "Norma de la Matriz Original: " << originalNorm << endl;
    cout << "Norma del Error (Frobenius): " << errorNorm << endl;
    cout << "Error Relativo: " << (relativeError * 100.0) << " %" << endl;
    cout << "Precision (1 - Error): " << ((1.0 - relativeError) * 100.0) << " %" << endl;
    
    // Opcional: Mostrar una pequeña comparativa visual
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

    writeN(SocketClient,writeBuffer.data(),writeBuffer.size());

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
        writeN(SocketClient,writeBuffer.data(),writeBuffer.size());
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

    return 0;
}