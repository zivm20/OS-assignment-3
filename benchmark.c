
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
#include <netinet/in.h>
#include <netinet/tcp.h> 
#include <arpa/inet.h>

#include <errno.h>

#define PACKET_SIZE 32768
#define TOATL_SIZE 100000000
#define IP "127.0.0.1"
#define PORT 50000

int compare(int fd1, int fd2){
    char c1, c2;
    int r1, r2;
    do{
        r1 = read(fd1,&c1,1);
        r2 = read(fd2,&c2,1);
    }while(r1 && r2 && c1 == c2);
    if(r1 == 0 && r2 == 0){
        return 1;
    }
    
    return 0;

}      

//checksum
unsigned int checksum(unsigned char *message) {
   int i, j;
   unsigned int byte, crc, mask;

   i = 0;
   crc = 0xFFFFFFFF;
   while (message[i] != 0) {
      byte = message[i];            // Get next byte.
      crc = crc ^ byte;
      for (j = 7; j >= 0; j--) {    // Do eight times.
         mask = -(crc & 1);
         crc = (crc >> 1) ^ (0xEDB88320 & mask);
      }
      i = i + 1;
   }
   return ~crc;
}

int min(int a, int b){
    if(a<b) return a;
    else return b;
}

clock_t transferData(int fdIn, int fdOut, int fdChecksum, size_t totalSize){
    char buf[PACKET_SIZE];
    int rec;  
    clock_t start = clock();
    while ((rec = read(fdIn, buf, min(sizeof(buf),totalSize))) > 0){
        if(fdOut!=-1){
            write(fdOut,buf,rec);
            
        }
        /*
        else{
            printf("amount left: %ld\n",totalSize);
        }*/
        if(fdChecksum != -1){ //write checksum to some file descriptor
            dprintf(fdChecksum,"%d",checksum(buf));
        }
        
        totalSize -= rec;
       
    }
    clock_t end = clock();
    return end - start;
}

int clientMain(int client, struct sockaddr *serverAddr, socklen_t len){
    if (connect(client,serverAddr, len) == -1){
        //printf("CONNECT ERROR: %d\n", sock_errno());
        close(client);
        exit(EXIT_FAILURE);
    }
    return client;
}

int serverMain(int server, int socketType, struct sockaddr *serverAddr, socklen_t len, struct sockaddr *clientAddr){
    int client;
    
    if (bind(server, serverAddr, len) == -1){
        printf("BIND ERROR %d\n",errno);
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
        
        //accept client
        if ((client = accept(server,clientAddr, &len)) == -1){
            close(server);
            close(client);
            exit(EXIT_FAILURE);
        }
        return client;
    }
    
    return server;
}


int tcpServer(int *server){
    
    struct sockaddr_in serverAddr;
    struct sockaddr_in clientAddr;
    memset(&clientAddr, 0, sizeof(struct sockaddr_in));
    memset(&serverAddr, 0, sizeof(struct sockaddr_in));
    if ((*server=socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        //perror("socket\n");
        exit(EXIT_FAILURE);
    } 
    int t = 1;
    setsockopt(*server, SOL_SOCKET, SO_REUSEADDR, (void*) &t, sizeof(t));
    
    serverAddr.sin_port = htons(PORT);
    if(inet_pton(AF_INET,IP,&serverAddr.sin_addr)<=0){
        //perror("inet_pton\n");
        
        exit(EXIT_FAILURE);
    }
    serverAddr.sin_family = AF_INET;
    return serverMain(*server,SOCK_STREAM,(struct sockaddr *) &serverAddr,sizeof(struct sockaddr_in),(struct sockaddr *) &clientAddr);
}

int tcpClient(){
    int client;
    struct sockaddr_in serverAddr;
    memset(&serverAddr,0,sizeof(struct sockaddr_in));

    serverAddr.sin_port = htons(PORT);
    if(inet_pton(AF_INET,IP,&serverAddr.sin_addr)<=0){
        //perror("inet_pton\n");
        exit(EXIT_FAILURE);
    }
    serverAddr.sin_family = AF_INET;
    
    if ((client = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        //perror("socket\n");
        exit(EXIT_FAILURE);
    } 

    return clientMain(client, (struct sockaddr *) &serverAddr, sizeof(struct sockaddr_in));
}

int udpServer(int *server){
    int reuseaddr = 1;
    struct sockaddr_in6 serverAddr;
    struct sockaddr_in6 clientAddr;
    memset(&clientAddr, 0, sizeof(struct sockaddr_in6));
    memset(&serverAddr, 0, sizeof(struct sockaddr_in6));
    int pid;

    if((*server = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == -1){
        //printf("SOCKET ERROR: %d\n", sock_errno());
        exit(EXIT_FAILURE);
    }
    setsockopt(*server, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));
    serverAddr.sin6_family = AF_INET6;
    serverAddr.sin6_port = htons(PORT);
    serverAddr.sin6_addr = in6addr_any;
    /*
    if(inet_pton(AF_INET6, "::1", &serverAddr.sin6_addr)<=0){
        //perror("inet_pton\n");
        exit(EXIT_FAILURE);
    }
    */

    return serverMain(*server,SOCK_DGRAM,(struct sockaddr *) &serverAddr,sizeof(struct sockaddr_in6) ,(struct sockaddr *) &clientAddr); 
}


int udpClient(){
    int client;
    struct sockaddr_in6 serverAddr;
    

    if((client = socket(AF_INET6, SOCK_DGRAM, 0))==-1){
        //printf("SOCKET ERROR: %d\n", sock_errno());
        exit(EXIT_FAILURE);
    }
    serverAddr.sin6_family = AF_INET6;
    serverAddr.sin6_port = htons(PORT);
   
    if(inet_pton(AF_INET6, "::1", &serverAddr.sin6_addr)<=0){
        perror("inet_pton\n");
        exit(EXIT_FAILURE);
    }
    return clientMain(client, (struct sockaddr *) &serverAddr,sizeof(struct sockaddr_in6));

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
    
    //link socket to our file
    serverAddr.sun_family = AF_UNIX;   
    strcpy(serverAddr.sun_path, name); 
    //make sure it's not linked elsewhere
    unlink(name);
    return serverMain(*server,socketType,(struct sockaddr *) &serverAddr,sizeof(struct sockaddr_un),(struct sockaddr *) &clientAddr);
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
    //link socket to our file
    serverAddr.sun_family = AF_UNIX;   
    strcpy(serverAddr.sun_path, name); 
    return clientMain(client, (struct sockaddr *) &serverAddr,sizeof(struct sockaddr_un));
    
}


int main(int argc, char **argv){
    if (argc > 2){
        printf("usage: benchmark <A/B/C/D/E/F/G>\n");
        return -1;
    }

    pid_t childId;
    int inFd;
    char benchType;
    if(argc == 2){
        benchType = *argv[1];
    }
    //create random data
    /*
    
    srand(time(NULL)); 
    if((inFd = open("in.txt", O_WRONLY | O_TRUNC | O_CREAT,S_IRUSR | S_IWUSR))<0){
        exit(EXIT_FAILURE);
    }
    for(int i = 0; i<TOTAL_SIZE; i++){
        
        char c = 'A' + (random() % 26); 
        write(inFd,&c,1);
        
    }
    close(inFd);
    */

    if((inFd = open("in.txt", O_RDONLY))<0){
        exit(EXIT_FAILURE);
    }

    //lock inFD
    //struct flock inFdLock = {F_RDLCK, SEEK_SET, 0, 0, getPid()};
    //fcntl(inFd, F_SETLKW, &inFdLock);

    int checksumPipe[2];
    int pipeline[2];
    if (pipe(checksumPipe) == -1) {
        exit(EXIT_FAILURE);
    }
    if (benchType == 'F' && pipe(pipeline) == -1) {
        close(checksumPipe[0]);
        close(checksumPipe[1]);
        exit(EXIT_FAILURE);
    }

    if((childId = fork())==-1){
        close(checksumPipe[0]);
        close(checksumPipe[1]);
        if (benchType == 'F'){
            close(pipeline[0]);
            close(pipeline[1]);
        }
        exit(EXIT_FAILURE);
    }
    
    if(childId == 0){
        
        close(checksumPipe[0]);//close read side

        int sock, serverSock = -1;
        char* method;
        if(benchType=='A'){
            sock = tcpServer(&serverSock);
            method = "TCP/IPv4 Socket";
        }
        else if(benchType=='B'){
            sock = udSocketServer(&serverSock,"mysock.socket",SOCK_STREAM);
            method = "UDS - Socket stream";
        }
        else if(benchType=='C'){
            sock = udpServer(&serverSock);
            method = "UDP/IPv6 Socket";
        }
        else if(benchType=='D'){
            
            sock = udSocketServer(&serverSock,"mysock.socket",SOCK_DGRAM);
            method = "UDS - Dgram socket";
        }
        else if(benchType=='F'){
            close(pipeline[1]);//close write side
            sock = pipeline[0];
            method = "PIPE";
        }
        //printf("%s - receiving: %ld\n",method,clock());
        transferData(sock,-1,checksumPipe[1],TOATL_SIZE);
        //printf("%s - done: %ld\n",method,clock());
        if(serverSock == -1){
            close(sock);
        }
        else{
            close(serverSock);
        }
        
       
        close(checksumPipe[1]);
        return 1;

    }
    else{
        close(checksumPipe[1]);//close write side

        //give 1 second for the server to start up
        sleep(4);
        int checksumIn;
        
        if((checksumIn = open("main_checksum.txt", O_WRONLY | O_TRUNC | O_CREAT,S_IRUSR | S_IWUSR))<0){
            close(checksumPipe[0]);
            close(checksumPipe[1]);
            if (benchType == 'F'){
                close(pipeline[0]);
                close(pipeline[1]);
            }
            exit(EXIT_FAILURE);
        }

        int sock;
        char* method;
        if(benchType=='A'){
            sock = tcpClient();
            method = "TCP/IPv4 Socket";
        }
        else if(benchType=='B'){
            sock = udSocketClient("mysock.socket",SOCK_STREAM);
            method = "UDS - Socket stream";
        }
        if(benchType=='C'){
            sock = udpClient();
            method = "UDP/IPv6 Socket";
        }
        else if(benchType == 'D'){
            sock = udSocketClient("mysock.socket",SOCK_DGRAM);
            method = "UDS - Dgram socket";
        }
        else if(benchType=='F'){
            close(pipeline[0]);//close read side
            sock = pipeline[1];
            method = "PIPE";
        }

        printf("%s - start: %ld\n",method,clock());
        size_t time = transferData(inFd,sock,checksumIn,TOATL_SIZE);
        printf("%s - sedning: %ld\n",method,clock());
        
        //inFdLock.l_type = F_UNLCK;
        //fcntl(inFd,F_SETLK,&inFdLock);
        wait(NULL);
        time_t end = clock();
        close(checksumIn);

        if((checksumIn = open("main_checksum.txt", O_RDONLY))<0){
            close(sock);
            close(checksumPipe[0]);
            exit(EXIT_FAILURE);
        }
        //printf("comparing %d %d\n",checksumIn,checksumPipe[0]);
        if(compare(checksumIn,checksumPipe[0]) == 0){
            end = -1;
        }
        printf("%s - end: %ld\n",method,end);
        
        close(sock);
        close(checksumPipe[0]);
        
    }

}



