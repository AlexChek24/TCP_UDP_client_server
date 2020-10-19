#include <netinet/in.h>
#include <cstdio>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <iostream>
#include <string>
#include <unistd.h>
#include <queue>
#include <algorithm>
#include <sstream>
#include <iterator>
#include <fcntl.h>
#include <unordered_map>
#include <thread>


struct arg_for_thread{
    std::unordered_map<int,std::string>* sockets;
    int sock;
    sockaddr_in* clientadr;
    bool* stop_thread;
    bool* server_stop;
    std::queue<std::string>* recvq;
    std::queue<std::string>* procq;
};
struct arg_for_manage{
    std::unordered_map<int,std::string>* sockets;
    int client_num;
    std::thread manage_thread;
    sockaddr_in* clientadr;
    int sock;
    bool* server_stop;

};

bool calculation(std::string &msg){
    bool server_stop;
    if (msg == "server stop") {
        msg = "server down";
        server_stop = true;
    } else {
        std::string tmp = msg;
        auto Not_digit = [](char x) { return x < '0' || x > '9'; };
        std::replace_if(msg.begin(), msg.end(), Not_digit, ' ');
        std::vector<uint64_t> numbers;
        std::stringstream ss(msg);
        copy(std::istream_iterator<uint64_t>(ss), {}, back_inserter(numbers));
        if (numbers.size() == 0) {
            msg = tmp;
        } else {
            std::sort(numbers.begin(), numbers.end());
            uint64_t Num_sum = 0;
            for (auto &n:numbers)
                Num_sum += n;
            ss = std::stringstream();
            copy(numbers.begin(), numbers.end(), std::ostream_iterator<uint64_t>(ss, " "));
            msg = ss.str();
            msg = msg + "\n" + "Sum: "+ std::to_string(Num_sum);
            server_stop = false;
        }
    }
    return server_stop;
}

static void receiving(arg_for_thread& recv_arg){//получение сообщения
    int s;
    std::string msg;
    char M [1024];
    do {
        memset(M, 0, sizeof(M));
        s = recv(recv_arg.sock, M, sizeof(M), 0);
        msg = M;
        std::cout << "Received TCP: " << msg << std::endl;
        recv_arg.recvq->push(msg);
        sleep(1);
    }while(!*recv_arg.stop_thread && !*recv_arg.server_stop);
}

static void process (arg_for_thread& proc_arg){ //брабока сообщения

    int s;
    do {
        if (proc_arg.recvq->size() > 0) {
            std::string msg = proc_arg.recvq->front();
            proc_arg.recvq->pop();
            if (msg == "disconnect" || msg =="exit") {
                if(msg == "disconnect"){
                    msg ="disconnected";
                    send(proc_arg.sock,msg.data(),msg.length(),0);
                }
                *proc_arg.stop_thread = true;
                proc_arg.sockets->erase(proc_arg.sock);
                shutdown(proc_arg.sock,1);
            } else if(*proc_arg.server_stop =*proc_arg.stop_thread = calculation(msg)){
                for(auto x: *proc_arg.sockets) {
                    if (x.second == "TCP") {
                        std::cout<< "Server stop TCP with fd: "<<x.first<<std::endl;
                        send(x.first, msg.data(), msg.length(), 0);
                        shutdown(x.first,1);
                    } else {
                        std::cout<< "Server stop UDP: "<<x.first<<std::endl;
                        sendto(x.first, msg.data(), msg.length(), 0, (struct sockaddr *) proc_arg.clientadr,
                               sizeof(*proc_arg.clientadr));
                    }
                }
            }

            proc_arg.procq->push(msg);
            sleep(1);
        }
    }while (!*proc_arg.stop_thread && !*proc_arg.server_stop);
}

static void sending(arg_for_thread& send_arg){ // отправка сообщения

    int s;
    std::string msg;
    char M [1024];
    do{
        if(send_arg.procq->size()>0) {
            msg = send_arg.procq->front();
            send_arg.procq->pop();
            send(send_arg.sock,msg.data(),msg.length(),0);
            std::cout<<"Sent TCP: "<<msg<<std::endl;
            sleep(1);
        }
        sleep(1);
    }while (!*send_arg.stop_thread && !*send_arg.server_stop);
}


static void manage_connection(arg_for_manage& manage_arg){ //порождаюший поток

    std::queue<std::string> recvq;
    std::queue<std::string> procq;
    arg_for_thread aft;
    aft.sock = manage_arg.sock;
    aft.procq=&procq;
    aft.recvq=&recvq;
    bool thread_stop =false;
    aft.sockets = manage_arg.sockets;
    aft.stop_thread=&thread_stop;
    aft.server_stop=manage_arg.server_stop;
    aft.clientadr = manage_arg.clientadr;


    //целевые потоки
    std::thread recv_thread(receiving,std::ref(aft));
    std::thread send_thread(sending,std::ref(aft));
    std::thread proc_thread(process,std::ref(aft));

    recv_thread.join();
    send_thread.join();
    proc_thread.join();

}




int main() {
    std::cout<<"Server start"<<std::endl;
    int s;
    int option = 1;
    struct timeval tv;
    tv.tv_sec =5;
    tv.tv_usec = 0;
    bool server_stop = false;
    bool TCP_or_UDP;
    char UDP_buffer [1024];
    int listenfd, connectionfd, udpfd, nready, maxfd;
    std::unordered_map<int,std::string> sockets;
    fd_set rset;
    socklen_t client_addr_len;
    struct sockaddr_in client_adr,server_adr;
    //прослущивающий сокет для TCP
    listenfd = socket(AF_INET,SOCK_STREAM,0);
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&option,sizeof(option));
    int status = fcntl(listenfd, F_SETFL,fcntl(listenfd,F_GETFL,0)|O_NONBLOCK);//разблакировка сокета
    if(status ==-1)
        perror("calling fcntl");
    bzero(&server_adr,sizeof(server_adr));
    server_adr.sin_family = AF_INET;
    server_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_adr.sin_port = htons(9031);

    bind(listenfd,(struct sockaddr*)&server_adr,sizeof(server_adr));
    perror("bind");
    listen(listenfd,6);
    //UDP сокет
    udpfd = socket(AF_INET,SOCK_DGRAM,0);
    status = fcntl(udpfd, F_SETFL,fcntl(udpfd,F_GETFL,0)|O_NONBLOCK);//разблокировка сокета
    if(status ==-1)
        perror("calling fcntl");
    bind(udpfd,(struct sockaddr*)&server_adr,sizeof(server_adr));
    FD_ZERO(&rset);
    std::vector<std::thread> TCP_clients;
    auto max = [](int x,int y){if (x>y) return x; else return y;};
    maxfd = max(listenfd,udpfd)+1;
    do{

        FD_SET(listenfd,&rset);
        FD_SET(udpfd,&rset);
        arg_for_manage manage;
        nready=select(maxfd,&rset,NULL,NULL,&tv); //проверка соединения TCP или UDP  с ожиданием 5 секуннд

        //если TCP сокет
        if(FD_ISSET(listenfd,&rset)){
            std::cout<<"TCP"<<std::endl;
            client_addr_len= sizeof(client_adr);
            connectionfd = accept(listenfd,(struct sockaddr*)&client_adr,&client_addr_len);
            manage.sock = connectionfd;
            manage.sockets = &sockets;
            manage.client_num = TCP_clients.size();
            manage.server_stop =&server_stop;
            manage.clientadr=&client_adr;
            sockets[connectionfd] = "TCP"; // добавляем в словарь TCP клиента
            TCP_clients.push_back(std::thread(manage_connection,std::ref(manage)));
        }
        if(FD_ISSET(udpfd,&rset)){
            std::cout<<"UDP"<<std::endl;
            client_addr_len= sizeof(client_adr);
            bzero(UDP_buffer,sizeof(UDP_buffer));
            sockets[udpfd] = "UDP";
            std::string msg;
            s = recvfrom(udpfd,UDP_buffer,sizeof(UDP_buffer),0,(struct sockaddr*)&client_adr,(&client_addr_len));
            msg = UDP_buffer;
            std::cout<<"Received UDP: "<<msg<<std::endl;
            server_stop = calculation(msg);
            if(msg == "disconnect")
                msg ="disconnected";
            if(server_stop) {
                for (auto x: sockets) {
                    if (x.second == "TCP") {
                        send(x.first, msg.data(), msg.length(), 0);
                    }
                }
            }
            std::cout<<"Sent UDP: "<<msg<<std::endl;

            sendto(udpfd,msg.data(),msg.length(),0,(struct sockaddr*)&client_adr,sizeof(client_adr));
        }
        sleep(1);
    }while (!server_stop);
    for(auto &x: TCP_clients){
        x.join();
    }
    shutdown(listenfd,1);
    shutdown(connectionfd,1);
    shutdown(udpfd,1);
    close(listenfd);
    close(connectionfd);
    close(udpfd);
    return 0;
}
