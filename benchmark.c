
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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>

#define PACKET_SIZE 32768
#define TOATL_SIZE 100000000
#define IP "127.0.0.1"
#define PORT 50000
#define SHARE_MAP_FILE "sharedmem.txt"

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


//get data using mmap  
clock_t mmapRec(char* memblock, int fdChecksum, size_t totalSize){
    
    while (totalSize>0){
        while(*memblock == '*' && totalSize>0){//wait for sender to overwrite * sign
            
        }
        if(fdChecksum != -1){ //write checksum to some file descriptor
            dprintf(fdChecksum,"%d",checksum(memblock));
        }
        totalSize -= min(strlen(memblock), totalSize);
        *memblock = '*';//write * sign to notify that we have finished reading this part and may move on
    }
    
    return clock();
}
//send data with mmap
clock_t mmapSend(int fdIn, char* memblock, int fdChecksum, size_t totalSize){
    size_t rec;
    
    clock_t start = clock();
    while ((rec = read(fdIn, memblock, min(PACKET_SIZE,totalSize))) > 0){
        if(rec < PACKET_SIZE){//shorten the string
            memblock[rec] = '\0';
        }
        if(fdChecksum != -1){ //write checksum to some file descriptor
            dprintf(fdChecksum,"%d",checksum(memblock));
        }
        
        totalSize -= rec;
        while(*memblock != '*'){//wait for receiving side to get the data
        }
    }
    
    return start;
    
   
}

//use shared memory to get data
clock_t sharedMemoryServer(int fdChecksum,size_t totalSize){
    char c;
    int shmid;
    key_t key;
    char *shm;
    size_t rec;
    key = 5678;

    //create the segment.
    if ((shmid = shmget(key, PACKET_SIZE, IPC_CREAT | 0666)) < 0) {
        perror("shmget - sharedMemoryServer\n");
        exit(EXIT_FAILURE);
    }

    
    //attach the segment to our data space.
    if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
        perror("shmat - sharedMemoryServer\n");
        exit(EXIT_FAILURE);
    }
    *shm = '*';
    
    while (totalSize>0){
        while(*shm == '*' && totalSize>0){//wait for sender to overwrite * sign
        }
        if(fdChecksum != -1){ //write checksum to some file descriptor
            dprintf(fdChecksum,"%d",checksum(shm));
        }
        
        totalSize -= min(strlen(shm), totalSize);
        *shm = '*';//write * sign to notify that we have finished reading this part and may move on
        
    }
    shmdt(shm); //detach segment

    return clock();


}
//use shared memory to send data
clock_t sharedMemoryClient(int fdIn,int fdChecksum,size_t totalSize){
    int shmid;
    key_t key;
    char *shm, *s;
    size_t rec;
    key = 5678;
    
    //locate the segment.
    if ((shmid = shmget(key, PACKET_SIZE, 0666)) < 0) {
        perror("shmget - sharedMemoryClient\n");
        exit(EXIT_FAILURE);
    }

   
    //attach the segment to our data space.
    if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
        perror("shmat - sharedMemoryClient\n");
        exit(EXIT_FAILURE);
    }
    clock_t start = clock();//accurate start time
    
    while ((rec = read(fdIn, shm, min(PACKET_SIZE,totalSize))) > 0){
        if(rec < PACKET_SIZE){
            shm[rec] = '\0';//shorten the string
        }
        if(fdChecksum != -1){ //write checksum to some file descriptor
            dprintf(fdChecksum,"%d",checksum(shm));
        }
        
        totalSize -= rec;
        while(*shm != '*'){//wait for receiving side to get the data
            
        }
    }
    shmdt(shm);
    return start;
}

//pass data from fdIn to fdOut (optional) while also calculating checksum and writing to fdChecksum (optional)
//return total time of tranfering
clock_t transferData(int fdIn, int fdOut, int fdChecksum, size_t totalSize){
    char buf[PACKET_SIZE];
    int rec;  
    clock_t start = clock();
    while ((rec = read(fdIn, buf, min(sizeof(buf),totalSize))) > 0){
        if(fdOut!=-1){
            write(fdOut,buf,rec);
            
        }
        else{
            printf("amount left: %ld\n",totalSize);
        }
        if(fdChecksum != -1){ //write checksum to some file descriptor
            dprintf(fdChecksum,"%d",checksum(buf));
        }
        
        totalSize -= rec;
       
    }
    clock_t end = clock();
    return end - start;
}
//universal client for datagram/stream like methods, return client socket
int clientMain(int client, struct sockaddr *serverAddr, socklen_t len){
    if (connect(client,serverAddr, len) == -1){
        perror("client failed to connect\n");
        close(client);
        exit(EXIT_FAILURE);
    }
    return client;
}
//universal server for datagram/stream like methods, return socket to read from
int serverMain(int server, int socketType, struct sockaddr *serverAddr, socklen_t len, struct sockaddr *clientAddr){
    int client;
    
    if (bind(server, serverAddr, len) == -1){
        perror("Bind error\n");
        close(server);
        exit(EXIT_FAILURE);
    }
    
    if(socketType != SOCK_DGRAM){
        //start listening for any client sockets
        if (listen(server, 500) == -1){ 
            perror("Listen failed\n");
            close(server);
            exit(EXIT_FAILURE);
        }
        
        //accept client
        if ((client = accept(server,clientAddr, &len)) == -1){
            perror("Accept failed\n");
            close(server);
            close(client);
            exit(EXIT_FAILURE);
        }
        return client;
    }
    
    return server;
}

//tcp server handler
int tcpServer(int *server){
    
    struct sockaddr_in serverAddr;
    struct sockaddr_in clientAddr;
    
    
    //init server socket 
    if ((*server=socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket - tcpServer\n");
        exit(EXIT_FAILURE);
    } 
    
    //set options for socket
    int t = 1;
    setsockopt(*server, SOL_SOCKET, SO_REUSEADDR, (void*) &t, sizeof(t));

    //set server addr
    memset(&clientAddr, 0, sizeof(struct sockaddr_in));
    memset(&serverAddr, 0, sizeof(struct sockaddr_in));
    serverAddr.sin_port = htons(PORT);
    if(inet_pton(AF_INET,IP,&serverAddr.sin_addr)<=0){
        perror("inet_pton - tcpServer\n");
        exit(EXIT_FAILURE);
    }
    serverAddr.sin_family = AF_INET;
    return serverMain(*server,SOCK_STREAM,(struct sockaddr *) &serverAddr,sizeof(struct sockaddr_in),(struct sockaddr *) &clientAddr);
}
//tcp client handler
int tcpClient(){
    int client;
    struct sockaddr_in serverAddr;
    
    
    //init client socket
    if ((client = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket - tcpClient\n");
        exit(EXIT_FAILURE);
    } 

    //set server addr
    memset(&serverAddr,0,sizeof(struct sockaddr_in));
    serverAddr.sin_port = htons(PORT);
    if(inet_pton(AF_INET,IP,&serverAddr.sin_addr)<=0){
        perror("inet_pton - tcpClient\n");
        exit(EXIT_FAILURE);
    }
    serverAddr.sin_family = AF_INET;
    
    return clientMain(client, (struct sockaddr *) &serverAddr, sizeof(struct sockaddr_in));
}

//udp ipv6 server handler
int udpServer(int *server){
    int reuseaddr = 1;
    struct sockaddr_in6 serverAddr;
    struct sockaddr_in6 clientAddr;
    int pid;

    //init socket
    if((*server = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == -1){
        perror("socket - udpServer\n");
        exit(EXIT_FAILURE);
    }

    //add options
    setsockopt(*server, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));

    //set server addr
    memset(&clientAddr, 0, sizeof(struct sockaddr_in6));
    memset(&serverAddr, 0, sizeof(struct sockaddr_in6));
    serverAddr.sin6_family = AF_INET6;
    serverAddr.sin6_port = htons(PORT);
    serverAddr.sin6_addr = in6addr_any;


    return serverMain(*server,SOCK_DGRAM,(struct sockaddr *) &serverAddr,sizeof(struct sockaddr_in6) ,(struct sockaddr *) &clientAddr); 
}

//udp ipv6 client handler
int udpClient(){
    int client;
    struct sockaddr_in6 serverAddr;
    
    //init socket
    if((client = socket(AF_INET6, SOCK_DGRAM, 0))==-1){
        perror("socket - udpClient\n");
        exit(EXIT_FAILURE);
    }

    //set server addr
    memset(&serverAddr, 0, sizeof(struct sockaddr_in6));
    serverAddr.sin6_family = AF_INET6;
    serverAddr.sin6_port = htons(PORT);
    if(inet_pton(AF_INET6, "::1", &serverAddr.sin6_addr)<=0){
        perror("inet_pton - udpClient\n");
        exit(EXIT_FAILURE);
    }
    return clientMain(client, (struct sockaddr *) &serverAddr,sizeof(struct sockaddr_in6));

}

int udSocketServer(int* server,char* name, int socketType){
    int client;
    struct sockaddr_un serverAddr;
    struct sockaddr_un clientAddr;     
    
    
    //create a UNIX domain socket 
    if ((*server = socket(AF_UNIX, socketType, 0)) == -1){
        perror("socket - udSocketServer\n");
        exit(EXIT_FAILURE);
    }

    memset(&serverAddr, 0, sizeof(struct sockaddr_un));
    memset(&clientAddr, 0, sizeof(struct sockaddr_un));
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
    
    //create a UNIX domain socket 
    if ((client = socket(AF_UNIX, socketType, 0)) == -1){
        perror("socket - udSocketClient\n");

        exit(EXIT_FAILURE);
    }

    //link socket to our file
    memset(&serverAddr, 0, sizeof(struct sockaddr_un));
    serverAddr.sun_family = AF_UNIX;   
    strcpy(serverAddr.sun_path, name); 
    return clientMain(client, (struct sockaddr *) &serverAddr,sizeof(struct sockaddr_un));
    
}


int main(int argc, char **argv){
    if (argc > 2){
        printf("usage: 'benchmark <A/B/C/D/E/F/G>' or 'benchmark' to load data\n");
        return -1;
    }

    pid_t childId;
    int inFd;
    char benchType;
    if(argc == 2){
        benchType = *argv[1];
    }

    //create random data if arg not given
    if(argc == 1){
        srand(time(NULL)); 
        if((inFd = open("in.txt", O_WRONLY | O_TRUNC | O_CREAT,S_IRUSR | S_IWUSR))<0){
            exit(EXIT_FAILURE);
        }
        for(int i = 0; i<TOATL_SIZE; i++){
            
            char c = 'A' + (random() % 26); 
            write(inFd,&c,1);
            
        }
        close(inFd);
        return 1;
    }

    //create the memory mapped file for bench E
    char *memblock;
    if(benchType == 'E'){
        int shareFd;
        struct stat statbuf;
        char buf[PACKET_SIZE] = {0};//temp buffer to place initial data into our shared memory file
        shareFd = open(SHARE_MAP_FILE, O_RDWR | O_TRUNC | O_CREAT,S_IRUSR | S_IWUSR);
        write(shareFd,buf,PACKET_SIZE);
        if ((memblock = mmap(NULL, PACKET_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, shareFd, 0)) == MAP_FAILED){
            perror("mmap init");
            close(shareFd);
            exit(EXIT_FAILURE);
        }
        close(shareFd);
        *memblock = '*';//place a "to write" marker
    }
  
    

    if((inFd = open("in.txt", O_RDONLY))<0){//input file
        exit(EXIT_FAILURE);
    }

    //we will pipe the checksum from our child process to our main thread
    int checksumPipe[2];
    int pipeline[2];
    if (pipe(checksumPipe) == -1) {
        exit(EXIT_FAILURE);
    }
    //for bench F we also need another pipe
    if (benchType == 'F' && pipe(pipeline) == -1) {
        close(checksumPipe[0]);
        close(checksumPipe[1]);
        exit(EXIT_FAILURE);
    }
    
    //create child process
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
        if(benchType=='G'){
            sharedMemoryServer(checksumPipe[1],TOATL_SIZE);
        }
        else if(benchType =='E'){
            
            mmapRec(memblock,checksumPipe[1],TOATL_SIZE);
        }
        else{//all methods except G and E may use this to benchmark the speed, used here to read from sock and calculate checksum
            transferData(sock,-1,checksumPipe[1],TOATL_SIZE);
        }
        
        if(serverSock == -1){
            if(sock != -1)
                close(sock);
        }
        else{
            close(serverSock);
        }
        
        
        if(benchType=='F'){
            close(pipeline[0]);
        }
       
        close(checksumPipe[1]);
        return 1;

    }
    else{
        close(checksumPipe[1]);//close write side

        //give 1 second for the server to start up, needed by some benchmarks and will not counted for
        sleep(1);
        int checksumIn;
        
        if((checksumIn = open("main_checksum.txt", O_WRONLY | O_TRUNC | O_CREAT,S_IRUSR | S_IWUSR))<0){//we will always write our checksum to here
            close(checksumPipe[0]);
            close(checksumPipe[1]);
            if (benchType == 'F'){
                close(pipeline[0]);
                close(pipeline[1]);
            }
            exit(EXIT_FAILURE);
        }

        int sock = -1;
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
        if(benchType=='G'){
            clock_t start = sharedMemoryClient(inFd,checksumIn,TOATL_SIZE);
            method = "shared memory between threads";
            printf("%s - start: %ld\n",method,start);
        }
        else if(benchType == 'E'){
            clock_t start = mmapSend(inFd, memblock, checksumIn, TOATL_SIZE);
            method = "MMAP";
            printf("%s - start: %ld\n",method,start);
        }
        else{//all the tests except E and G can use transfer data, used to write to sock and calc checksum 
            printf("%s - start: %ld\n",method,clock());
            size_t time = transferData(inFd,sock,checksumIn,TOATL_SIZE);
        }
        
        
        wait(NULL);// wait for child process
        time_t end = clock();


        close(checksumIn);
        if((checksumIn = open("main_checksum.txt", O_RDONLY))<0){
            close(sock);
            close(checksumPipe[0]);
            exit(EXIT_FAILURE);
        }

        //comapre checksumIn to the checksum we got from the child process
        if(compare(checksumIn,checksumPipe[0]) == 0){
            end = -1;
        }
        //if they don't match, print -1 as our end 
        printf("%s - end: %ld\n",method,end);
        
        //close all fd's
        if(sock != -1){
            close(sock);
        }
        if(benchType=='F'){
            close(pipeline[1]);
        }
        close(checksumPipe[0]);
        
    }

}



