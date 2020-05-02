/*******************************************************************************
 * PROXY, à escuta de novos clientes.  Quando surjem
 * novos clientes os dados por eles enviados são lidos e descarregados no ecrã.
 *******************************************************************************/
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/mman.h>
#include <dirent.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>

#define SEVER_PORT  9000
#define BUF_SIZE  1023
#define SHM_KEY 0x1234

typedef struct Connections{

  struct sockaddr_in server_side;
  int cport;
  char cip[36];


}Connection;


struct sockaddr_in addr_t;
int shmid;
int users;
int *con;
int *checkers;
int *fdudp;
int *sfdup;
int *save_state;
int *cport;
int *shmp;
int *packetloss;
char *cip;
struct sockaddr_in *servaddr;
Connection *current;

void write_log(char *log){

  char fp[36];
  char *token;
  int i = 0;

  printf("-----------------------------%s\n",log);
  token = strtok(log, "|");
   
  while( token != NULL ) {
      if(i==3){
        strcpy(fp,token);
        printf("%s\n",token);
      }
      printf("%s\n",token);
      token = strtok(NULL, "|");
      i++;
  }


  FILE *file = fopen("log.txt", "a");
  time_t rawtime;
  struct tm * timeinfo;

  time ( &rawtime );
  timeinfo = localtime ( &rawtime );
  fprintf(file, "FILE TRANSFER: %s - %s\n",fp,asctime(timeinfo));  
  fclose(file);  

}

void erro(char *msg) {
  printf("Erro: %s\n", msg);
  exit(-1);
}

void udp_init(int port, char *ipaddr){

  struct sockaddr_in si_minha;
  struct hostent *hostPtr; 
  char udp[100];

  // Cria um socket para recepção de pacotes UDP
  if((*fdudp=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
  {
    erro("Erro na criação do socket");
  }

  si_minha.sin_family = AF_INET;
  si_minha.sin_port = htons(port);
  si_minha.sin_addr.s_addr = inet_addr("127.0.0.1");


  // Associa o socket à informação dslene endereço
  if(bind(*fdudp,(struct sockaddr*)&si_minha, sizeof(si_minha)) == -1)
  {
    erro("Erro no bind");
  }


  // set IPV4 address 
  strcpy(udp, ipaddr);
  if ((hostPtr = gethostbyname(udp)) == 0){
    erro("Nao consegui obter endereço");
  }

  // Creating socket file descriptor 
  if ((*sfdup = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 ) { 
    perror("socket creation failed"); 
    exit(EXIT_FAILURE); 
  } 

  // set IPV4 address 
  strcpy(udp, ipaddr);
  if ((hostPtr = gethostbyname(udp)) == 0){
    erro("Nao consegui obter endereço");
  }

  bzero((void *) servaddr, sizeof(*servaddr));
  servaddr->sin_family = AF_INET; 
  servaddr->sin_port = htons(9876);
  servaddr->sin_addr = *((struct in_addr *)hostPtr->h_addr);
  
  printf("[+]OPENING SOCKETS %d and %d\n",*sfdup,*fdudp);
}


void close_udp(){
  printf("[-]CLOSING SOCKETS %d and %d\n",*sfdup,*fdudp);
  close(*fdudp);
  close(*sfdup);
}

int prepare_file_udp(socklen_t slen,struct sockaddr_in si_outra){

  char buff[BUF_SIZE];
  int tot=0;
  int b = 1;
  int total;
  int num;
  socklen_t len;
  struct timeval tv;
  recvfrom(*sfdup, (char *)buff, BUF_SIZE,MSG_WAITALL, (struct sockaddr *) servaddr, &len);
  sendto(*fdudp, (const char *)buff, BUF_SIZE,MSG_CONFIRM, (const struct sockaddr *) &si_outra, slen);
  total = atoi(buff);
  printf("RECEIVING %d BYTES\n",total);
  tv.tv_sec = 0;
  tv.tv_usec = 100000;
  srand(time(0));
  if (setsockopt(*sfdup, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
    return -1;
  } 
  if(1){
    printf("[>]WAITING FOR FILE\n");
    bzero(buff,sizeof(buff));
    while( (b = recvfrom(*sfdup, (char *)buff, BUF_SIZE,MSG_WAITALL, (struct sockaddr *) servaddr, &len))> 0) {
      tot+=b;
      num = (rand() % (100 - 1 + 1)) + 1; 
      if(num <= *packetloss){
          memset(buff,0,BUF_SIZE);
      }
      sendto(*fdudp, (const char *)buff, BUF_SIZE,MSG_CONFIRM, (const struct sockaddr *) &si_outra, slen);
      memset(buff,0,BUF_SIZE);
      
    }
    printf("\n---------------SERVER REPLY------------------\n");
    printf("Receiving byte");
    if (b<0)
      perror("Receiving");
  }
  printf("Redirecting Complete\n");
  printf("---------------------------------------------\n\n");
  return 1;
}


void prepare_file(int confd, int fd){

  char buff[BUF_SIZE];
  int tot=0;
  int b = 1;

  if(1){

    while( (b = read(confd, buff, BUF_SIZE))> 0) {
      tot+=b;
      write(fd, buff, sizeof(buff));
      memset(buff,0,BUF_SIZE);
    }
    printf("\n---------------SERVER REPLY------------------\n");
    printf("Received byte: %d\n",tot);
    if (b<0)
      perror("Receiving");
  }
  printf("Redirecting Complete\n");
  printf("---------------------------------------------\n\n");

}

void udp_handler(int port, char *ipaddr, int confd, int fd){

  struct sockaddr_in si_outra;
  int recv_len;
  int n;
  int num;
  socklen_t len; 
  char buf[BUF_SIZE];
  char buffer[BUF_SIZE]; 
  char filebuf[BUF_SIZE];
  socklen_t slen = sizeof(si_outra);

  //------COMMS---------------------------------

    if(fork()==0){
      

      printf("[+]OPENING LISTENING PROCESS: %d\n",getpid());
      printf("[!]SERVER SOCKET: %d\n[!]YOUR SOCKET: %d\n",*fdudp,*sfdup);
      
      while(1){
        bzero(buffer,BUF_SIZE);
        bzero(buf,BUF_SIZE);
        srand(time(0));

          if((recv_len = recvfrom(*fdudp, buf, BUF_SIZE, 0, (struct sockaddr *) &si_outra, (socklen_t *)&slen)) == -1){
            erro("Erro no recvfrom");
          } 
        

        printf("[+]NEW REQUEST FROM POR [%d]: %s\n",ntohs(si_outra.sin_port),buf);


        if(strcmp(buf,"QUIT")!=0){

          strcpy(filebuf,buf);
          strcpy(buffer,buf);
          
    
          if(strncmp(filebuf,"DOWNLOAD",strlen("DOWNLOAD"))==0){
            if(strncmp(filebuf,"DOWNLOAD|TCP",strlen("DOWNLOAD|TCP"))==0){
              write(confd,filebuf,sizeof(filebuf));
              prepare_file(confd,fd);
              if(*save_state==1){
                write_log(filebuf);
              }
            }else{
              if((sendto(*sfdup, (const char *)buffer, strlen(buffer), MSG_CONFIRM, (const struct sockaddr *) servaddr,  sizeof(*servaddr))) == -1){
                erro("Erro no send");
              } 
              prepare_file_udp(slen,si_outra);
            }
            if(*save_state==1){
              write_log(filebuf);
            }
          }else{
            if((sendto(*sfdup, (const char *)buffer, strlen(buffer), MSG_CONFIRM, (const struct sockaddr *) servaddr,  sizeof(*servaddr))) == -1){
              erro("Erro no send");
            } 
            printf("[!]SENT TO SERVER\n"); 

            if((n = recvfrom(*sfdup, (char *)buffer, BUF_SIZE,  MSG_WAITALL, (struct sockaddr *) servaddr, &len)) == -1){
              erro("Erro no recvfrom");
            } 

            buffer[n] = '\0'; 
            buf[recv_len]='\0';
            num = (rand() % (100 - 1 + 1)) + 1; 
            if(num <= *packetloss){
              printf("[-]PACKET LOSS\n");
              memset(buffer,0,BUF_SIZE);
            }

            if((sendto(*fdudp, (const char *)buffer, BUF_SIZE,MSG_CONFIRM, (const struct sockaddr *) &si_outra, slen))==-1){
              erro("Erro no send");
            } 
          }
        }else{
          strcpy(buffer,"Bye");
          if((sendto(*fdudp, (const char *)buffer, BUF_SIZE,MSG_CONFIRM, (const struct sockaddr *) &si_outra, slen))==-1){
            erro("Erro no send");
          } 
        }
      }

      printf("[-]CLOSING LISTENING PROCESS: %d\n",getpid());
      exit(0);
    }
    wait(NULL);

}


int *reconnect(struct sockaddr_in addr, int *rec){

  int server_fd;
  int fd;
  char send_packet[BUF_SIZE];

  if((fd = socket(AF_INET,SOCK_STREAM,0)) == -1)
   erro("socket");
  
  //Socket Writting packet
  if( (server_fd = connect(fd,(struct sockaddr *)&addr,sizeof (addr))) < 0)
   erro("Connect");

  read(fd,send_packet,sizeof(send_packet));

  rec[0] = fd;
  rec[1] = server_fd;

  return rec;

}

void message_protocol(int clientfd, int fd, int *server, int *id, char *protocol, char *ipaddr,int port,int cportv, char *ipv){
    
    char buff[BUF_SIZE];
    int rec[2];
    int exit = 1;

    printf("New Connection: CLIENT[%d]\n",*id);
    read(server[1],buff,sizeof(buff));
    users = atoi(buff);
    printf("MAXIMUM CLIENTS:%d\n",users);

    if(*checkers == 1){
      printf("--------------------JUST TWICE------------------\n" );
      current = (Connection *)malloc(users*sizeof(Connection));
    }

    *cport = cportv;
    strcpy(cip,"127.0.0.1");    

    if(users >= *con){
      write(clientfd,buff,sizeof(buff));

      if(strcmp(protocol,"TCP")==0){
        while(strcmp(buff,"Bye")!=0){
          printf("----------------------\n");
          bzero(buff, BUF_SIZE); 
          read(clientfd, buff, sizeof(buff));
          write(server[1],buff,sizeof(buff));
          printf("From client[%d] : %s\n",clientfd, buff); 
          if(strncmp(buff,"DOWNLOAD",strlen("DOWNLOAD"))==0){
            printf("READING FILE\n");
            if(*save_state==1){
              write_log(buff);
            }
            prepare_file(server[1],clientfd);
            reconnect(addr_t,rec);
            server[1] = rec[0]; server[0] = rec[1];
            exit = 0;
            break;
          }else if(strcmp(buff,"QUIT")==0){
            exit = 0;
            bzero(buff, BUF_SIZE);
            read(server[1],buff,sizeof(buff));
            write(clientfd,buff,sizeof(buff));
            break;
          }else{
            bzero(buff, BUF_SIZE);
            read(server[1],buff,sizeof(buff));
            write(clientfd,buff,sizeof(buff));
          }
        }
        if(exit == 0){
          *con-=1;
        }
      }else{
        printf("----------UDP HANDLER ACTIVE---------");
        udp_handler(port,ipaddr,server[1],clientfd);
        *con-=1;
      }
    }else{
      strcpy(buff,"Bye");
      write(clientfd,buff,sizeof(buff));
      *con-=1;
    }
    close(clientfd);
    printf("Closing client[%d] connection\n",*id);
}


void init_server_socket(int *serverInfo, char port_ip[][BUF_SIZE]){

  int fd;
  struct sockaddr_in addr;
  int server_fd;

  printf("[+] Connecting to server with %s ip on port %s\n",port_ip[0],port_ip[1]);
  printf("[!] Client has requested %s transport protocol\n",port_ip[2]);

  bzero((void *) &addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(port_ip[0]);
  addr.sin_port = htons(atoi(port_ip[1]));

  if((fd = socket(AF_INET,SOCK_STREAM,0)) == -1)
   erro("socket");
  
  //Socket Writting packet
  if( (server_fd = connect(fd,(struct sockaddr *)&addr,sizeof (addr))) < 0)
   erro("Connect");

  addr_t = addr;
  serverInfo[0] = server_fd;
  serverInfo[1] = fd;

}


void init_client_socket(char *port[]){

  struct sockaddr_in addr, client_addr;
  int fd, client;
  int server[2];
  int client_addr_size;
  char message[3][BUF_SIZE];
  char *token;
  char input[BUF_SIZE];
  int i = 0;
  int use_port;
  char use_protocol[16];
  char use_addr[36];

  bzero((void *) &addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_port = htons((short) atoi(port[1]));

  /*startup(addr);*/
  printf("[!]PROXY PORT: %d\n",atoi(port[1]));

  if ( (fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  erro("na funcao socket");
  if ( bind(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0)
  erro("na funcao bind");
  if( listen(fd, 5) < 0)
  erro("na funcao listen");

  client_addr_size = sizeof(client_addr);

  while(1){

    while(waitpid(-1,NULL,WNOHANG)>0);
    client = accept(fd,(struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size);
    shmp[*con]=ntohs(client_addr.sin_port);
   
    *con+=1;

    if (client > 0) {

      if (fork() == 0) {

        printf("[!]NEW CONNECTION FROM [%d]\n",ntohs(client_addr.sin_port));
        read(client,input,sizeof(input));
        printf("%s\n",input);
        token = strtok(input, " ");
   
        while( token != NULL ) {

          if(i==0){
            strcpy(message[0],token);
            strcpy(use_addr,token);
          }
          else if(i==1){
            strcpy(message[1],token);
            use_port = atoi(message[1]);
          }else{
            strcpy(message[2],token);
            strcpy(use_protocol,token);
          }
          token = strtok(NULL, " ");
          i++;
        }

        if(*checkers == 0){
            printf("--------------------JUST ONCE------------------\n" );
            udp_init(use_port,use_addr);
        }
        *checkers+=1;

        write(client,input,sizeof(input));
        init_server_socket(server,message);
        memset(message, 0, sizeof(message[0][0]) * 3 * BUF_SIZE);
        message_protocol(client,fd,server,con,use_protocol,use_addr,use_port,ntohs(client_addr.sin_port),inet_ntoa(client_addr.sin_addr));
        close(server[0]);
        close(server[1]);
        exit(0);
      }
      close(client);
    }
    printf("[+][SOCKET LISTEN]\n");

  }

}


void console_proxy(){

  char c[16];


  while(strcmp(c,"EXIT")!=0){
    printf("CON %d\n", *con);
    printf("Enter command: ");
    scanf("%s",c);
    if(strcmp(c,"SAVE")==0){
      *save_state *= -1;
    }else if(strcmp(c,"SHOW")==0){
      printf("ACTIVE CONNECTIONS\n");
      for(int i = 0; i < *con; i++){
        printf(">>> CLient: %s | %d  || Server: %s | %d\n",inet_ntoa(servaddr->sin_addr),shmp[i],
          inet_ntoa(servaddr->sin_addr),(int) ntohs(servaddr->sin_port));
      }
    }else if(strcmp(c,"LOSSES")==0){
      printf("Pecentage: ");
      scanf("%d",packetloss);
      printf("PACKET LOSS SET TO %d %% \n",*packetloss);
    }else{
      printf("Wrong command\n");
    }

    bzero(c,16);

  }
}


int main(int argc, char *argv[]) {
 
  printf("\033[H\033[J");
  int shmid;
  pid_t cons,prox;

  current = mmap(NULL, sizeof *current, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  con = mmap(NULL, sizeof *con, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  checkers = mmap(NULL, sizeof *checkers, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  fdudp = mmap(NULL, sizeof *fdudp, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  sfdup = mmap(NULL, sizeof *sfdup, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  servaddr = mmap(NULL, sizeof *servaddr, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  save_state = mmap(NULL, sizeof *save_state, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  cip = mmap(NULL, sizeof *cip, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  cport = mmap(NULL, sizeof *cport, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  packetloss = mmap(NULL, sizeof *packetloss, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  *save_state = 1;
  *checkers = 0;
  *con = 0;
  *packetloss = 0;

  shmid = shmget(SHM_KEY, 256*sizeof(int), 0644|IPC_CREAT);
  if (shmid == -1) {
    perror("Shared memory");
    return 1;
  }
   
  // Attach to the segment to get a pointer to it.
  shmp = shmat(shmid, NULL, 0);

  if (shmp == (void *) -1) {
    perror("Shared memory attach");
    return 1;
  }
  cip = (char *)malloc(36*sizeof(char));

  if(!(cons =fork())){
    sleep(1);
    console_proxy();
    exit(0);
  }else if(!(prox=fork())){
    init_client_socket(argv);
    exit(0);
  }else{
    wait(&cons);
    wait(&prox);
  }
  wait(NULL);

  if (shmdt(shmp) == -1) {
    perror("shmdt");
    return 1;
  }

  if (shmctl(shmid, IPC_RMID, 0) == -1) {
    perror("shmctl");
    return 1;
  }

  printf("Proxy terminated\n");
  
  return 0;
}

