
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>


#define PACKET_SIZE 1024


int readData(int fdIn, int fdOut){
    char buf[PACKET_SIZE];
    memset(buf, 0, PACKET_SIZE);     

    while ((bytes_rec = recv(client, buf, sizeof(buf), 0)) > 0){
        
        
    }
    
}

int udSocketServer(char* name, size_t dataSize, int socketType){
    int server, client, len, rc;
    int bytes_rec = 0;
    struct sockaddr_un serverAddr;
    struct sockaddr_un clientAddr;     
    
    memset(&serverAddr, 0, sizeof(struct sockaddr_un));
    memset(&clientAddr, 0, sizeof(struct sockaddr_un));
               
    
    //create a UNIX domain socket 
    if ((server = socket(AF_UNIX, socketType, 0)) == -1){
        printf("SOCKET ERROR: %d\n", sock_errno());
        exit(1);
    }
    
    //link socket to our file
    serverAddr.sun_family = AF_UNIX;   
    strcpy(serverAddr.sun_path, name); 
    len = sizeof(serverAddr);
    
    //make sure it's not linked elsewhere
    unlink(name);
    if (bind(server, (struct sockaddr *) &serverAddr, len) == -1){
        printf("BIND ERROR: %d\n", sock_errno());
        close(server);
        exit(1);
    }
    
    //start listening for any client sockets
    if (listen(server, backlog) == -1){ 
        printf("LISTEN ERROR: %d\n", sock_errno());
        close(server);
        exit(1);
    }
    
    //accept client
    if ((client = accept(server, (struct sockaddr *) &clientAddr, &len)) == -1){
        printf("ACCEPT ERROR: %d\n", sock_errno());
        close(server);
        close(client);
        exit(1);
    }
    return server;

}


int main(){

}



