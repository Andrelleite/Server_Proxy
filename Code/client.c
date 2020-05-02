/**********************************************************************
 * André Leite - 2015250489
 * 
 * USO: >cliente <endereçoProxy> <enderecoServidor>  <porto>  <Protocolo>
 **********************************************************************/
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/mman.h>
#include <dirent.h>
#include <sys/ipc.h>
#include <time.h>
#include <sys/shm.h>

#define BUF_SIZE  1023
#define USER_PORT 7000
#define CYPHER 'A'


int *udpfd;
int *sockfd;
int server_fd;
struct sockaddr_in addr;
struct sockaddr_in *udpaddr;
int rec[2];


void encryption(char *ch, int len){

    for(int i = 0; i < len; i++){
      ch[i] = ch[i] ^ CYPHER;
    }

} 

int get_dir() { 
    
    struct dirent *de;  // Pointer for directory entry 
    struct dirent *files; // Pointer for server files directory entry
    int i = 0;

    DIR *dr = opendir("."); 
    DIR *drsf;

    if (dr == NULL)  
    { 
        printf("Could not open current directory" ); 
        return 0; 
    }   

    printf("\nDOWNLOADED FILES\n\n");
  
    while ((de = readdir(dr)) != NULL){

        if(strcmp(de->d_name,"downloads")==0){
            drsf = opendir(de->d_name);
            if(drsf == NULL){
                printf("Could not open the file directory");
                return -1;
            }
            while((files=readdir(drsf)) != NULL){
              if(strlen(files->d_name)>=4){
                printf(">>> ");
                printf("%s\n",files->d_name);
                i++;
              }
            }
        }

    }

    printf("\n%d files downloaded so far.\n\n",i);

    closedir(drsf);   
    closedir(dr); 
    return 1; 
} 

void erro(char *msg) {
  printf("Erro: %s\n", msg);
  exit(-1);
}

int prepare_file(int confd, char *message){

  char buff[BUF_SIZE];
  char filename[32];
  int tot=0;
  int b = 1;
  char *token;
  char dir[64];
  char protocol[3];
  char enc[4];
  int i = 0;
  struct timeval tv;
  double time_spent;
  clock_t end;
  clock_t begin = clock(); 


  token = strtok(message, "|");
   
  while( token != NULL ) {

      if(i==3){
        strcpy(dir,"downloads/");
        strcat(dir,token);
        strcpy(filename,token);
      }else if(i==2){
        printf("%s\n",token);
        strcpy(enc,token);
      }
      token = strtok(NULL, "|");
      i++;
  }

  tv.tv_sec = 4;
  tv.tv_usec = 0;
  printf("[>]WAITING FOR FILE\n");
  if (setsockopt(confd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
    return -1;
  } 

  FILE* fp = fopen(dir, "wb");
  if(fp != NULL){
    while( (b = read(confd, buff, BUF_SIZE))> 0) {
      if(strcmp(enc,"ENC")==0){
        encryption(buff,b);
      }
      tot+=b;
      fflush(stdout);
      fwrite(buff, 1, b, fp);
      bzero(buff,BUF_SIZE);
    } 
    
    end = clock();
    time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("\n---------------SERVER REPLY------------------\n");
    printf("File:%s\n",dir);
    printf("Received byte: %d\n",tot);
    printf("Transport portocol:%s\n","TCP");
    printf("Time elapsed during download: %.2f ms\n",time_spent);
    if (b<0)
      perror("Receiving");
    fclose(fp);
  }else {
    perror("File");
  }
  printf("\n------------DOWNLOAD COMPLETE-------------\n\n");
  bzero(filename,sizeof(filename));
  bzero(protocol,sizeof(protocol));
  bzero(enc,sizeof(enc));
  return 0;
}


int prepare_file_udp(char *message){

  char buff[BUF_SIZE];
  char filename[32];
  char *token;
  char dir[64];
  char protocol[3];
  char enc[4];
  int tot=0;
  int b = 1;
  int i = 0;
  int total;
  double time_spent;
  socklen_t slen; 

  struct timeval tv;
  token = strtok(message, "|");
   
  while( token != NULL ) {
      if(i==3){
        strcpy(dir,"downloads/");
        strcat(dir,token);
        strcpy(filename,token);
      }
      token = strtok(NULL, "|");
      i++;
  }

  recvfrom(*udpfd, (char *)buff, BUF_SIZE,MSG_WAITALL, (struct sockaddr *) udpaddr, &slen);
  total = atoi(buff);
  printf("RECEIVING %d BYTES\n",total);
  bzero(buff,BUF_SIZE);

  tv.tv_sec = 4;
  tv.tv_usec = 0;
  printf("[>]WAITING FOR FILE\n");
  FILE* fp = fopen(dir, "wb");
  if (setsockopt(*udpfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
    return -1;
  } 
  clock_t end;
  clock_t begin = clock();
  if(fp != NULL){
    while( (b = recvfrom(*udpfd, (char *)buff, BUF_SIZE,  MSG_WAITALL, (struct sockaddr *) udpaddr, &slen))> 0 && total >= tot) {
      if(strcmp(enc,"ENC")==0){
        strcpy(enc,"ENC");
        encryption(buff,b);
      }else{
        strcpy(enc,"NOT");
      }
      tot+=b;
      fflush(stdout);
      fwrite(buff, 1, b, fp);
      bzero(buff,BUF_SIZE);
    }
    
    end = clock();
    time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("\n---------------SERVER REPLY------------------\n");
    printf("File:%s\n",dir);
    printf("Received byte: %d\n",total);
    printf("Transport portocol:%s\n","UDP");
    printf("Time elapsed during download: %.2f ms\n",time_spent);
    if (b<0)
      perror("Receiving");
    fclose(fp);
  }else {
    perror("File");
  }
  printf("\n------------DOWNLOAD COMPLETE-------------\n\n");
  bzero(filename,sizeof(filename));
  bzero(protocol,sizeof(protocol));
  bzero(enc,sizeof(enc));
  return 1;
}

int *reconnect(struct sockaddr_in addr, int *rec){

  int server_fd;
  int fd;

  if((fd = socket(AF_INET,SOCK_STREAM,0)) == -1)
   erro("socket");
  
  //Socket Writting packet
  if( (server_fd = connect(fd,(struct sockaddr *)&addr,sizeof (addr))) < 0)
   erro("Connect");

  rec[0] = fd;
  rec[1] = server_fd;

  return rec;

}

void upd_init(int argc, char*argv[]){
    
    struct hostent *hostPtr; 
    char udp[100];
  
    // set IPV4 address 
    strcpy(udp, argv[1]);
    if ((hostPtr = gethostbyname(udp)) == 0){
      erro("Nao consegui obter endereço");
    }

    // Creating socket file descriptor 
    if ((*udpfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
  
    bzero((void *) udpaddr, sizeof(*udpaddr));
    udpaddr->sin_family = AF_INET; 
    udpaddr->sin_port = htons((short) atoi(argv[3]));
    udpaddr->sin_addr = *((struct in_addr *)hostPtr->h_addr);
}

void udp_client(int argc, char*argv[]){

    char buffer[BUF_SIZE]; 
    int n;
    socklen_t slen; 

    while(strcmp(buffer,"Bye")!=0){

      bzero(buffer, sizeof(buffer)); 
      printf("\n\nOPTIONS:\n\n-> LIST\n-> DOWNLOAD|<UDP OR TCP>|<ENC OR NOT>|FILE_NAME\n-> MYFILES\n-> QUIT\n");
      printf("\nEnter option: ");
      scanf("%s",buffer);

      if(strncmp(buffer,"DOWNLOAD",strlen("DOWNLOAD"))==0){
        sendto(*udpfd, (const char *)buffer, BUF_SIZE, MSG_CONFIRM, (const struct sockaddr *) udpaddr,  sizeof(*udpaddr)); 
        if(strncmp(buffer,"DOWNLOAD|TCP",strlen("DOWNLOAD|TCP"))==0){
          prepare_file(*sockfd,buffer);
          bzero(buffer, sizeof(buffer));
        }else{
          prepare_file_udp(buffer);
        }
      }else{
        if(strcmp(buffer,"MYFILES")==0){
          get_dir();
        }else{

          sendto(*udpfd, (const char *)buffer, BUF_SIZE, MSG_CONFIRM, (const struct sockaddr *) udpaddr,  sizeof(*udpaddr)); 
          bzero(buffer, sizeof(buffer)); 
          n = recvfrom(*udpfd, (char *)buffer, BUF_SIZE,  MSG_WAITALL, (struct sockaddr *) udpaddr, &slen); 
          buffer[n] = '\0'; 
          printf("%s\n", buffer);
          
        }
      }
      
      getchar();   
      printf("Press enter to continue.\n");
      getchar();   
      printf("\033[H\033[J");
    
    }

    close(*udpfd); 
}

void tcp_client(int argc, char*argv[]){

  printf("\033[H\033[J");

  char endServer[100];
  char send_packet[1023];
  struct hostent *hostPtr;
  int id;


  udpfd = mmap(NULL, sizeof *udpfd, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  udpaddr = mmap(NULL, sizeof *udpaddr, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  sockfd =  mmap(NULL, sizeof *sockfd, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

  if (argc != 5) {
      printf("cliente <proxyhost> <serverhost> <port> <protocol>\n");
      exit(-1);
  }

  strcpy(endServer, argv[1]);
  if ((hostPtr = gethostbyname(endServer)) == 0){
    erro("Nao consegui obter endereço");
  }

  bzero((void *) &addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(argv[1]);
  addr.sin_port = htons((short) atoi(argv[3]));

  if((*sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1)
   erro("socket");
  
  //Socket Writting packet
  if( (server_fd = connect(*sockfd,(struct sockaddr *)&addr,sizeof (addr))) < 0)
   erro("Connect");
  
  srand ( time(NULL) );
  id = rand()%1000+1;

  printf("CLIENT ID:%d\n",id );
  //addr_size = sizeof(addr);
  //server_addr_size = sizeof(server_addr);

  upd_init(argc,argv);

  strcpy(send_packet,argv[2]);
  strcat(send_packet," ");
  strcat(send_packet,argv[3]);
  strcat(send_packet," ");
  strcat(send_packet,argv[4]);
  write(*sockfd,send_packet,sizeof(send_packet));

  bzero(send_packet, sizeof(send_packet)); 
  read(*sockfd,send_packet,sizeof(send_packet));
  read(*sockfd,send_packet,sizeof(send_packet));
  printf("____________WELCOME___________\n"); 

  if(strcmp(argv[4],"TCP")==0){
    while(strcmp(send_packet,"Bye")!=0){
    
    bzero(send_packet, sizeof(send_packet)); 
    printf("\n\nOPTIONS:\n\n-> LIST\n-> DOWNLOAD|<UDP OR TCP>|<ENC OR NOT>|FILE_NAME\n-> MYFILES\n-> QUIT\n");
    printf("\nEnter option: ");
    scanf("%s",send_packet);

    printf("\033[H\033[J");

    if(strcmp(send_packet,"MYFILES")==0){
      get_dir();
    }else{
      write(*sockfd,send_packet,sizeof(send_packet));
      if(strncmp(send_packet,"DOWNLOAD",strlen("DOWNLOAD"))!=0){
        bzero(send_packet, sizeof(send_packet)); 
        read(*sockfd,send_packet,sizeof(send_packet));
        printf("\n---------------SERVER REPLY------------------\n");
        printf("%s\n", send_packet); 
        printf("---------------------------------------------\n\n");
      }else{

        if(strncmp(buffer,"DOWNLOAD|TCP",strlen("DOWNLOAD|TCP"))==0){
          prepare_file(*sockfd,send_packet);
          bzero(send_packet, sizeof(send_packet));
          reconnect(addr,rec);
          *sockfd = rec[0]; server_fd = rec[1];
          strcpy(send_packet,argv[2]);
          strcat(send_packet," ");
          strcat(send_packet,argv[3]);
          strcat(send_packet," ");
          strcat(send_packet,argv[4]);
          write(*sockfd,send_packet,sizeof(send_packet));
          bzero(send_packet,sizeof(send_packet));
          read(*sockfd,send_packet,sizeof(send_packet));
          read(*sockfd,send_packet,sizeof(send_packet));
        }else{
          prepare_file_udp(buffer);
        }
      }
    }
   
    getchar();   
    printf("Press enter to continue.\n");
    getchar();   
    printf("\033[H\033[J");

    }
  }else{
    udp_client(argc,argv);
  }
  
  close(*sockfd);

}

int main(int argc, char *argv[]) {
  
  tcp_client(argc,argv);
  

  return 0;
}


