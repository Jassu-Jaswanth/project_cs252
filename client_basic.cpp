#include <iostream>                                                                                                                                                                                    
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>

using namespace std;

int main( int argc, char const* argv[] ){
    //socket()
    //bind()
    //  IF BINDED NO NEED TO CONNECT. BIND MAKES A WIRED CONNECTIONS
    //  WHILE connect(),listen() and accecpt() IS USED IN CASES OF CONNECTION LESS SOCKETS i.e, IN TCP CONNECTIONS / STREAM SOCKETS
    //connect()
    //listen()
    //accept()
    //
 
    int PORT = 3366;
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    
    struct sockaddr_in host_addr;
    struct sockaddr_in their_addr;
    host_addr.sin_port = htons(PORT);
    host_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(host_addr.sin_zero),'\0',8);
    
    // Check for error in bind()...
    bind(sockfd,(struct sockaddr*)&host_addr,sizeof(struct sockaddr));
    listen(sockfd, 5);
    unsigned int size_in = sizeof(struct sockaddr);
    int incommingfd = accept(sockfd, (struct sockaddr*)&their_addr,&size_in);
    
    char recvd_msg[128];
    recv(sockfd,recvd_msg,128,0);
}