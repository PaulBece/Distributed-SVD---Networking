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

#define PORT 8080

using namespace std;

int main(int argc, char * argv[]){
    int portNumber;

    cout<<"UwU"<<endl;

    if (argc>=2){
        portNumber=atoi(argv[1]);
    }
    else{
        portNumber=PORT;
    }

    cout<<"UwU"<<endl;

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

    cout<<"UwU"<<endl;
    int aux;
    string line;
    bool flag1=true;

    string writeBuffer="s";
    string readBuffer;
    readBuffer.resize(1);

    write(SocketClient,writeBuffer.data(),writeBuffer.size());

    cout<<"Processor Connected\n";

    while (1);
    

    // while (flag1){

    //     cout<<nickname1<<" select an action:\n"<<
    //     "1 Change nickname.\n"<<
    //     "2 Send private message.\n"<<
    //     "3 Send broadcast.\n"<<
    //     "4 List all chat members.\n"<<
    //     "5 Send file.\n"<<
    //     "6 Play TikTakToe.\n"<<
    //     "0 Exit chat."<<endl;

    //     getline(cin, line);
    //     aux = stoi(line);  
        
    //     switch (aux)
    //     {
    //     case 1://n
    //         updateNickname(SocketClient);
    //         break;
    //     case 2://t
    //         privateMessage(SocketClient);
    //         break;
    //     case 3://m
    //         broadcast(SocketClient);
    //         break;
    //     case 4://l
    //         buffer="l";
    //         write(SocketClient,buffer.data(),1);
    //         break;
    //     case 5://f
    //         sendFile(SocketClient);
    //         break;
    //     case 6://f
    //         buffer="p";
    //         write(SocketClient,buffer.data(),1);
    //         break;
        
    //     case 0://x
    //         {
    //             string confirm;
    //             cout<<"Are you sure you want to exit?(y/n)"<<endl;
    //             getline(cin, confirm);
    //             if (confirm[0]=='y') {
    //                 buffer="x";
    //                 fullmessage1=buffer;
    //                 cout<<fullmessage1<<endl;
    //                 write(SocketClient,buffer.data(),1);
    //                 flag=false;
    //                 t1.join();
    //                 shutdown(SocketClient, SHUT_RDWR);
    //                 close(SocketClient);
    //                 flag1=false;
    //             } else if (confirm[0]=='n') continue;
    //             break;
    //         }
        
    //     default:
    //         cout<<"Please, enter a valid option."<<endl;
    //         break;
    //     }
    // }

    return 0;

}