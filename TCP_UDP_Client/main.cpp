
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <iostream>
#include <arpa/inet.h>
#include <string>
#include <pthread.h>
#include <unistd.h>
#include <thread>

#define PORT 9031


struct arg_for_thread {
    sockaddr_in* serv;
    int sock;
    bool* stop;
    bool* reciv_or_send;
    bool TCP_or_UDP;
};
static void send_or_sendto(arg_for_thread send_arg){

    int s;
    std::string msg;
    while (!*send_arg.stop){
        if(*send_arg.reciv_or_send){
            std::cout<<std::endl<<"Enter data>>";
            getline(std::cin,msg);
            while(msg.size()==0)
            {

                std::cout<<"Enter a non-empty string"<<std::flush;
                std::cout<<std::endl<<"Enter data>>";
                getline(std::cin,msg);
            }
            if(msg=="exit") {
                *send_arg.stop = true;
//                std::cout<<"Client disconnect"<<std::endl;
            }
            if(send_arg.TCP_or_UDP ==0)
                s=send(send_arg.sock,msg.data(),msg.length(),0);
            else
                s=sendto(send_arg.sock,msg.data(),msg.length(),0,(const struct sockaddr*)send_arg.serv,sizeof(*send_arg.serv));
            *send_arg.reciv_or_send=!*send_arg.reciv_or_send;
        }
        sleep(0.5);
    }
}
static void recv_or_recvfrom (arg_for_thread& recv_arg) {

    int r;
    char M[1024];
    std::string msg;
    while (!*recv_arg.stop) {
        memset(M, 0, sizeof(M));
        //std::cout<<"Recv_error"<<std::endl<<"*recv_arg.reciv_or_send) "<<*recv_arg.reciv_or_send;

        if (!*recv_arg.reciv_or_send) {
            if(recv_arg.TCP_or_UDP ==0)
                r = recv(recv_arg.sock, M, sizeof(M), 0);
            else
                r=recvfrom(recv_arg.sock,M,sizeof(M),0,(struct sockaddr*)recv_arg.serv,(socklen_t *) sizeof(&recv_arg.serv));
            msg = M;
            std::cout << msg << std::endl;
            if (msg == "server down" || msg == "disconnected")
                *recv_arg.stop = true;
            *recv_arg.reciv_or_send = !*recv_arg.reciv_or_send;
        }
        sleep(0.5);
    }
}

void manage(arg_for_thread& manage_arg){
    int connection;
    int thread_attr;
    int s;
    int fails = 0;
    if(manage_arg.TCP_or_UDP==0){
        do{
            std::cout<<"connection..."<<std::flush<<std::endl;
            connection = connect(manage_arg.sock,(const struct sockaddr*) manage_arg.serv,sizeof(*manage_arg.serv));
            perror("Connect");
            fails++;
            sleep(1);
        }while(connection!=0 && fails<8);
        if(fails>=8)
            *manage_arg.stop = true;
    }

    std::thread recv_thread (recv_or_recvfrom, std::ref(manage_arg));
    std::thread send_thread (send_or_sendto, std::ref(manage_arg));
    recv_thread.join();
    send_thread.join();

    shutdown(manage_arg.sock,SHUT_RDWR);

}



int main(int argc, char** argv){
    int socketfd;
    std::string selected_mode = argv[1];
    struct sockaddr_in serveraddr;
    bool TCP_or_UDP;
    if(selected_mode=="TCP"){
        std::cout<<"TCP mode selected"<<std::endl;
        if((socketfd = socket(AF_INET,SOCK_STREAM,0))<0){
            std::cout<<"socket creation failed";
            exit(0);
        }
        TCP_or_UDP = 0;
    }
    if(selected_mode=="UDP") {
        std::cout<<"UDP mode selected"<<std::endl;
        if ((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            std::cout << "socket creation failed";
            exit(0);
        }
        TCP_or_UDP = 1;
    }
    if(selected_mode!= "TCP" && selected_mode!="UDP"){
        std::cout<<"None of the modes are selected";
        exit(0);
    }
    memset(&serveraddr,0,sizeof(serveraddr));
    serveraddr.sin_family=AF_INET;
    serveraddr.sin_port =htons(PORT);
    serveraddr.sin_addr.s_addr=inet_addr("127.0.0.1");
    pthread_t main_thread, recv_thread, send_thread;
    arg_for_thread main_thread_arg;
    bool stop_send_and_recv = 0;
    main_thread_arg.stop =&stop_send_and_recv;
    main_thread_arg.sock = socketfd;
    main_thread_arg.serv=&serveraddr;
    main_thread_arg.TCP_or_UDP = TCP_or_UDP; //tcp = 0, udp = 1
    bool send_or_recv = 1; //send =1, recv = 0
    main_thread_arg.reciv_or_send=&send_or_recv;
    int s;
    pthread_attr_t attr;
    s=pthread_attr_init(&attr);
    std::thread manage_thread (manage, std::ref(main_thread_arg));
    manage_thread.join();


    shutdown(socketfd,1);
    close(socketfd);

    return 0;
}