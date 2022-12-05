
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define PACKET_SIZE 1024
#define TOATL_SIZE 100000000
int min(int a, int b){
    if(a<b) return a;
    else return b;
}
size_t transferData(int fdIn, int fdOut,size_t totalSize){
    char buf[PACKET_SIZE];
    int rec;  
    //memset(buf, 0, PACKET_SIZE);  
    clock_t start = clock();
    while ((rec = read(fdIn, buf, min(sizeof(buf),totalSize))) > 0){
        write(fdOut,buf,rec);
        //printf("reading (%ld): %s\n\n",totalSize,buf);
        totalSize -= rec;
    }
    clock_t end = clock();
    return end - start;
}

int clientMain(int client, struct sockaddr *serverAddr){
    if (connect(client,serverAddr, sizeof(*serverAddr)) == -1){
        //printf("CONNECT ERROR: %d\n", sock_errno());
        close(client);
        exit(EXIT_FAILURE);
    }
    return client;
}

int serverMain(int server, int socketType, struct sockaddr *serverAddr,struct sockaddr *clientAddr){
    int client;
    socklen_t len = sizeof(*serverAddr);
    if (bind(server, serverAddr, sizeof(*serverAddr)) == -1){
        //printf("BIND ERROR: %d\n", sock_errno());
        close(server);
        exit(EXIT_FAILURE);
    }
    
    if(socketType != SOCK_DGRAM){
        //start listening for any client sockets
        if (listen(server, 500) == -1){ 
            printf("listen failed");
            close(server);
            exit(EXIT_FAILURE);
        }
        printf("listening\n");
        
        //accept client
        if ((client = accept(server,clientAddr, &len)) == -1){
            close(server);
            close(client);
            exit(EXIT_FAILURE);
        }
        printf("accepted\n");
        return client;
    }
    return server;
}


int udSocketServer(int* server,char* name, int socketType){
    int client;
    struct sockaddr_un serverAddr;
    struct sockaddr_un clientAddr;     
    memset(&serverAddr, 0, sizeof(struct sockaddr_un));
    memset(&clientAddr, 0, sizeof(struct sockaddr_un));
    
    //create a UNIX domain socket 
    if ((*server = socket(AF_UNIX, socketType, 0)) == -1){
        //printf("SOCKET ERROR: %d\n", sock_errno());
        exit(EXIT_FAILURE);
    }
    printf("server socket: %d\n",*server);
    
    //link socket to our file
    serverAddr.sun_family = AF_UNIX;   
    strcpy(serverAddr.sun_path, name); 
    //make sure it's not linked elsewhere
    unlink(name);
    return serverMain(*server,socketType,(struct sockaddr *) &serverAddr,(struct sockaddr *) &clientAddr);
}

int udSocketClient(char* name, int socketType){
    int client;
    struct sockaddr_un serverAddr;
    //printf("udSocketClient\n");
    memset(&serverAddr, 0, sizeof(struct sockaddr_un));
    
    //create a UNIX domain socket 
    if ((client = socket(AF_UNIX, socketType, 0)) == -1){
        //printf("SOCKET ERROR: %d\n", sock_errno());
        exit(EXIT_FAILURE);
    }
    printf("client socket: %d\n",client);
    //link socket to our file
    serverAddr.sun_family = AF_UNIX;   
    strcpy(serverAddr.sun_path, name); 
    return clientMain(client, (struct sockaddr *) &serverAddr);
    
}




int main(){
    pid_t childId;
    int inFd;
    //create random data
    /*
    
    srand(time(NULL)); 
    if((inFd = open("in.txt", O_WRONLY | O_TRUNC | O_CREAT,S_IRUSR | S_IWUSR))<0){
        exit(EXIT_FAILURE);
    }
    for(int i = 0; i<100; i++){
        for(int j = 0; j<1000000; j++){
            char c = 'A' + (random() % 26); 
            write(inFd,&c,1);
        }
    }
    close(inFd);
    */


    if((childId = fork())==-1){
        exit(EXIT_FAILURE);
    }
    
    if(childId == 0){
        int outFile;
        //write to new/existing file, or append
        if((outFile = open("out.txt", O_WRONLY | O_TRUNC | O_CREAT,S_IRUSR | S_IWUSR))<0){
            exit(EXIT_FAILURE);
        }
        int sock, serverSock;
        sock = udSocketServer(&serverSock,"mysock.socket",SOCK_DGRAM);
        fprintf(stdout,"on socket: %d\n",serverSock);
        printf("client connected at: %ld\n",clock());
        size_t time = transferData(sock,outFile,TOATL_SIZE);
        
        printf("client left at: %ld\n",clock());
        printf("time to receive %ld\n",time);
        close(serverSock);

    }
    else{
        sleep(1);
        if((inFd = open("in.txt", O_RDONLY))<0){
            exit(EXIT_FAILURE);
        }
        int sock;
        sock = udSocketClient("mysock.socket",SOCK_DGRAM);
        
        printf("starting send: %ld\n",clock());
        
        sleep(5);
        size_t time = transferData(inFd,sock,TOATL_SIZE);
        printf("finished send: %ld\n",clock());
        printf("time to send: %ld\n",time);
        
        close(sock);
        wait(NULL);
    }

}



