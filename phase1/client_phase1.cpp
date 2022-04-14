#include <cstdio>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <map>

using namespace std;

struct req_args{
    int maxfd;
    int fileCount;
    char** fileNames;
    fd_set rfds;
};

void* send_requirement_ins(void *vargp);

int fileLength( FILE *f ){
    int pos;
    int end;

    // seeks the beginning of the file to the end and counts
    // it and returns into variable end
    pos = ftell(f);
    fseek (f, 0, SEEK_END);
    end = ftell(f);
    fseek (f, pos, SEEK_SET);

    return end;
}

int SetNonBlocking(int sd){
  int curflags = fcntl(sd, F_GETFL, 0);
  return fcntl(sd, F_SETFL, curflags | O_NONBLOCK);
}

int find_fd_index(int fd, int* fds, int total_fds){
    for(int i = 0; i < total_fds; i++){
        if(fds[i] == fd){
            return i;
        }
    }
    return -1;
}
int find_id_index(int id,int* client_ids,int client_num){
    for(int i = 0; i < client_num; i++){
        if(client_ids[i] == id){
            return i;
        }
    }
    return -1;
}

int send_file(int fd, char* filename){
    FILE* fp = fopen(filename,"rb");
    char* buf = (char*)malloc(sizeof(char)*255);
    int fs = fileLength(fp);
    do{
        fread(buf,sizeof(char),255,fp);
        int sn = send(fd,buf,255,0);
        fs -= sn;
    } while (fs > 0);
    return 0;
}


int main( int argc, char const* argv[] ){

    // Minimum required arguments
    if(argc < 2){
        printf("Usage: ./client_basic arg1_config_file_location arg2_directory_path\n");
        return 0;
    }
    
    // Get files in Source Directory.
    DIR* sdir = opendir(argv[2]);
    if (sdir == NULL){
        printf("Some error accessing Source directory of client\n");
        return 0;
    }
    struct dirent* sfile;
    char** sfileNames;
    int sfileCount;
    sfileNames = (char **)malloc(sizeof(char *) * 255);
    {
        int i = 0;
        while(sfile = readdir(sdir)){
            if(sfile->d_type == DT_REG){
                sfileNames[i] = (char *)malloc(sizeof(char)*255);
                sprintf(sfileNames[i],"%s",sfile->d_name);
                i++;
            }
        }
        sfileCount = i+1;
    }

    


    size_t buffer_size = 256*256;
    // char* buffer;
    // buffer = (char *)malloc(sizeof(char) * buffer_size);

    int id;                     // Local unique id of host
    int unique_id;              // Network provided unique id of host
    int client_num;             // Number of connected clients in the network
                                // excluding temporary connections.
    int host_port;
    int port;

    // Reading configuration file.
    FILE* config_file = fopen(argv[1],"r");
    

    fscanf(config_file,"%d",&id);
    fscanf(config_file,"%d",&host_port);
    fscanf(config_file,"%d\n",&unique_id);
    fscanf(config_file,"%d\n",&client_num);

    struct sockaddr_in host_addr;
    host_addr.sin_family = AF_INET;
    host_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(host_addr.sin_zero),'\0',8);

    int listen_sock;
    int *clients_con = (int *)malloc(sizeof(int) * client_num);
    int maxfd = 0;
    if((listen_sock = socket(AF_INET,SOCK_STREAM,0)) == -1){
        perror("socket");
        return 0;
    }
    int yes = 1;
    if(setsockopt(listen_sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1){
        perror("setsockopt");
        return 0;
    }
    if(SetNonBlocking(listen_sock) < 0){
            printf("non-blocking failed");
    }
    
    host_addr.sin_port = htons(host_port);
    if((bind(listen_sock,(struct sockaddr *)&host_addr,sizeof(struct sockaddr))) == -1){
        perror("bind");
        return 0;
    }
    
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;

    for (int i = 0; i < client_num; i++){
        if((clients_con[i] = socket(AF_INET,SOCK_STREAM,0)) == -1){
            perror("socket");
            return 0;
        }
        if(SetNonBlocking(clients_con[i]) < 0){
            printf("non-blocking failed");
        }
        if(setsockopt(clients_con[i],SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv)) == -1){
            perror("setsockopt");
            return 0;
        }
        if(setsockopt(clients_con[i],SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv)) == -1){
            perror("setsockopt");
            return 0;
        }
        
    }
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
        fileNames[i] = (char *)malloc(sizeof(char)*255);
        fscanf(config_file,"%s\n",fileNames[i]);
        printf("%s\n",fileNames[i]);
    }



    int* client_uids = (int *)malloc(sizeof(int) * client_num);
    
    
    //Setup for select()
    
    fd_set readfds;
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_ZERO(&readfds);
    
        if(listen(listen_sock,10) == -1){
            perror("listen");
            return 0;
        }
        else {
            FD_SET(listen_sock,&readfds);
            maxfd = listen_sock;
        }

    bool *connected = (bool *)malloc(sizeof(bool)*client_num);
    bool *uid_read = (bool *)malloc(sizeof(bool)*client_num);
    memset(connected,false,client_num);   
    memset(uid_read,false,client_num);
    int rd = buffer_size;
    bool isConnected = true;
    // just accept a conn
    while (true){
        // puts("Still listening\n");
        // sleep(1);
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
                if(errno == EISCONN){
                    puts("already connected\n");
                    connected[i] = true;
                }
            } else {
                FD_SET(clients_con[i],&readfds);
                if(maxfd < clients_con[i]){
                    maxfd = clients_con[i];
                }
                connected[i] = true;
                char msg[50];
                sprintf(msg,"i#Connected to %d with unique-ID %d on port %d#",id,unique_id,host_port);
                if(send(clients_con[i],msg,strlen(msg),0) == -1){
                    if(errno == EWOULDBLOCK || errno == EAGAIN){
                        printf("Sending failed\n");
                    }
                }
                // if(recv(clients_con[i],buffer,sizeof(char)*buffer_size,0) == -1){
                //     if(errno == EWOULDBLOCK || errno == EAGAIN){
                //         printf("recv failed\n");
                //     }
                // }
                // else{
                //     printf("%s\n",buffer);
                // }
                // Example : Connected to 3 with unique-ID 4526 on port 5000
            }
        }
        fd_set readfds_cpy = readfds;
        select(maxfd+1,&readfds_cpy,NULL,NULL,&tv);
        // Also let's accecpt if there are any connections at any port
        // else check if there is any data to be read
        // also need to get what we want.
        for (int fd = 0; fd<maxfd+1; fd++){
            
            if (FD_ISSET(fd,&readfds_cpy)){
                if( fd == listen_sock){
                    int newfd = accept(fd,(struct sockaddr *)&their_addr,&addrlen);
                    if(newfd < 0){
                        perror("accept");
                    } else {

                        FD_SET(newfd, &readfds);
                        if (newfd > maxfd) {
                            // keep track of the maximum
                            maxfd = newfd;
                        }
                        
                        // Updating connected client list
                        port = ntohs(their_addr.sin_port);  // reusing the port variable which is
                                                            // used to initialise the listening port.
                        for(int i = 0; i<client_num; i++){
                            if(client_ports[i] == port){
                                clients_con[i] = newfd;
                                // connected[i] = true;
                                if(SetNonBlocking(clients_con[i]) < 0){
                                    printf("non-blocking failed");
                                }
                                if(setsockopt(clients_con[i],SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv)) == -1){
                                    perror("setsockopt");
                                    return 0;
                                }
                                if(setsockopt(clients_con[i],SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv)) == -1){
                                    perror("setsockopt");
                                    return 0;
                                }
                                
                                char msg[50];
                                sprintf(msg,"i#Connected to %d with unique-ID %d on port %d#",id,unique_id,host_port);
                                send(clients_con[i],msg,strlen(msg),0);
                                
                                break;
                            }
                        }
                    }
                } else {
                    //TODO: read data;
                    char *buffer = (char*)malloc(sizeof(char) * buffer_size);
                    int rv = recv(fd,buffer,buffer_size,0);
                    if(rv == -1){
                        if(errno == EWOULDBLOCK || errno == EAGAIN){
                            printf("Timed out");
                        }
                    }
                    else if (rv == 0){
                        // printf("connection failed. Try to reconnect");
                        // Just set the connected of this file descriptor to false
                        // On the next cycle it tries to connect to it
                        // int i = find_fd_index(fd, clients_con, client_num);
                        // connected[i] = false;
                        // FD_CLR(clients_con[i],&readfds);
                        // close(fd);
                        // clients_con[i] = 0;
                    }
                    else{
                        // Should also handle other requests for search depth one
                        char ch = buffer[0];
                        switch (ch){
                            case 'i':
                                {
                                char* msg_buf = (char*)malloc(sizeof(char) * (rv-2));
                                int tmp_id;
                                int tmp_uid;
                                int tmp_port;
                                int tmp_int;
                                //Connected to %d with unique-ID %d on port %d
                                for(int j = 0; buffer[j+2] != '#'; j++){
                                    msg_buf[j] = buffer[j+2];
                                }
                                sscanf(msg_buf,"Connected to %d with unique-ID %d on port %d",&tmp_id,&tmp_uid,&tmp_port);
                                tmp_int = find_id_index(tmp_id,client_ids,client_num);
                                client_uids[tmp_int] = tmp_uid;
                                uid_read[tmp_int] = true;
                                printf("%s\n",msg_buf);
                                }
                                break;
                            
                            case 'a':
                                {
                                // char* reply = (char *)malloc(sizeof(char) * buffer_size);
                                // sprintf(reply,"f#");
                                // char filename[256];
                                // int namelen = 0;
                                // for(int i = 0; i<rv-2; i++){
                                //     if (buffer[i+2] == '#'){
                                //         filename[namelen] = '\0';
                                //         for(int j = 0; j < sfileCount; j++){
                                //             if(strcmp(filename,sfileNames[j]) == 0){
                                //                 sprintf(reply,"%s#%d",reply,1);
                                //             } else {
                                //                 sprintf(reply,"%s#%d",reply,0);
                                //             }
                                //         }
                                //         namelen = 0;
                                //     } else {
                                //         filename[namelen] = buffer[i+2];
                                //         namelen++;
                                //     }
                                //     i++;
                                // }
                                // send(fd,reply,strlen(reply),0);
                                }
                                break;

                            case 'f':
                                break;

                            default:
                                break;
                        }
                    }
                }
            }
        }
        isConnected = true;
        for (int i = 0; i < client_num; i++){
            isConnected = isConnected && uid_read[i];
        }
        if (isConnected){
            //Close all the sockets for phase1 for now
            // for(int j = 0; j<client_num; j++){
            //     shutdown(clients_con[j],SHUT_RD);
            // }
            // close(listen_sock);
            // printf("uid's exchanged");
        }
    }  
    close(listen_sock);
    return 0;
}