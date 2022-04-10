#include <cstdio>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

using namespace std;

int main( int argc, char const* argv[] ){

    if(argc < 2){
        printf("Usage: ./client_basic arg1_config_file_location arg2_directory_path\n");
        return 0;
    }
    
    size_t buffer_size = 255;
    char* buffer;
    buffer = (char *)malloc(sizeof(char) * buffer_size);

    int id;
    int client_num;
    FILE* config_file = fopen(argv[1],"r");
    

    getline(&buffer,&buffer_size,config_file);
    fscanf(config_file,"%d",&client_num);
    fseek(config_file,0,SEEK_SET);
    fscanf(config_file,"%d",&id);

    struct sockaddr_in host_addr;
    host_addr.sin_family = AF_INET;
    host_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(host_addr.sin_zero),'\0',8);
    int *clients = (int *)malloc(sizeof(int) * client_num);
    int *clients_con = (int *)malloc(sizeof(int) * client_num);
    int maxfd = 0;
    int maxfd_con = 0;
    for (int i = 0; i < client_num; i++){
        if((clients[i] = socket(AF_INET,SOCK_STREAM,0)) == -1){
            perror("socket");
            return 0;
        }
        int yes = 1;
        if(setsockopt(clients[i],SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1){
            perror("setsockopt");
            return 0;
        }
        int port;
        fscanf(config_file,"%d",&port);
        host_addr.sin_port = htons(port);
        if((bind(clients[i],(struct sockaddr *)&host_addr,sizeof(struct sockaddr))) == -1){
            perror("bind");
            return 0;
        }
    }
    for (int i = 0; i < client_num; i++){
        if((clients_con[i] = socket(AF_INET,SOCK_STREAM,0)) == -1){
            perror("socket");
            return 0;
        }
        int yes = 1;
        if(setsockopt(clients_con[i],SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1){
            perror("setsockopt");
            return 0;
        }
    }
    fscanf(config_file,"%d",&client_num);
    int* client_ids = (int *)malloc(sizeof(int)*client_num);
    int* client_ports = (int *)malloc(sizeof(int)*client_num);

    for(int i = 0; i<client_num; i++){
        fscanf(config_file,"%d",client_ids+i);
        fscanf(config_file,"%d",client_ports+i);
    }
    int fileCount;
    fscanf(config_file,"%d",&fileCount);
    char** fileNames;
    fileNames = (char **)malloc(sizeof(char *) * fileCount);
    for (int i = 0; i<fileCount; i++){
        buffer_size = 255;
        fileNames[i] = (char *)malloc(sizeof(char)*255);
        fscanf(config_file,"%s\n",fileNames[i]);
        printf("%s\n",fileNames[i]);
    }

    //Setup for select()
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    fd_set readfds;
    fd_set listenfds;
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_ZERO(&readfds);
    FD_ZERO(&listenfds);
    for (int i = 0; i<client_num; i++){
        if(listen(clients[i],10) == -1){
            perror("listen");
        }
        else {
            FD_SET(clients[i],&listenfds);
            maxfd = (maxfd < clients[i]) ? clients[i] : maxfd;
        }
    }

    bool *connected = (bool *)malloc(sizeof(bool)*client_num);
    memset(connected,false,client_num);

    int *accepted_con = (int *)malloc(sizeof(int)*client_num);
    
    while (true){
        fd_set listenfds_cpy = listenfds;
        select(maxfd,&listenfds_cpy,NULL,NULL,&tv);
        // Try connecting if not connected already or if there is a breakage of connection
        // Though it might fail to notice dead connections in phase-1 coz of no file or data transfer
        //      between clients. But once data transefer occurs then we can detect dead sockets and 
        //      try to reconnect here.
        struct sockaddr_in their_addr;
        socklen_t addrlen = sizeof(struct sockaddr);
        for (int i = 0; i<client_num && !connected[i]; i++){
            
            their_addr.sin_family = AF_INET;
            their_addr.sin_addr.s_addr = INADDR_ANY;
            their_addr.sin_port = htons(client_ports[i]);
            memset(&(their_addr.sin_zero),'\0',8);
            if(connect(clients_con[i],(struct sockaddr*)&their_addr,sizeof(struct sockaddr)) == -1){
                perror("connect");
                // return 0;
            } else {
                FD_SET(clients_con[i],&writefds);
                maxfd = (maxfd < clients_con[i]) ? clients_con[i] : maxfd;
                connected[i] = true;
            }
        }

        // Also let's accecpt if there are any connections at any port

        for (int i = 0; i<maxfd; i++){
            int newfd;
            if (FD_ISSET(i,&listenfds_cpy)){
                if( (newfd = accept(i,(struct sockaddr *)&their_addr,&addrlen)) == -1){
                    perror("accept");
                } else {
                    FD_SET(newfd, &readfds);
                    if (newfd > maxfd) {
                        // keep track of the maximum
                        maxfd = newfd;
                    }
                }
            }
        }
        // if ((newfd = accept(listener, &remoteaddr, &addrlen)) == -1) {
        //     perror("accept");
        // } else {
        //     FD_SET(newfd, &master); // add to master set
        //     if (newfd > fdmax) {
        //         // keep track of the maximum
        //         fdmax = newfd;
        //     }
        //     printf("selectserver: new connection from %s on "
        //     "socket %d\n", inet_ntoa(remoteaddr.sin_addr), newfd);
        // }
        
    }

    return 0;
}