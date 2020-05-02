/*******************************************************************************
 * SERVIDOR no porto 9000, à escuta de novos clientes.  Quando surjem
 * novos clientes os dados por eles enviados são lidos e descarregados no ecrã.
 * Uso: >server <porto> <numero maximo de clientes>
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

#define SERVER_PORT  9000
#define BUF_SIZE	1023
#define BUFLEN 512  
#define PORT 9876 
#define CYPHER 'A'

int files_len;
int users;
int fd, client;
int fdudp;


/********************************************************
*                       TCP PROTOCOL                    *
*********************************************************/

void encryption(char *ch, int len){

    for(int i = 0; i < len; i++){
      ch[i] = ch[i] ^ CYPHER;
    }

} 

//-------------------- STARTUP SERVER -------------------

int init_dir(char *server_files) { 
    
    struct dirent *de;  // Pointer for directory entry 
    struct dirent *files; // Pointer for server files directory entry
    int i = 0;

    DIR *dr = opendir("."); 
    DIR *drsf;

    bzero(server_files, BUF_SIZE); 

    if (dr == NULL)  
    { 
        printf("Could not open current directory" ); 
        return 0; 
    }   
    strcat(server_files,"\nFILES STORED:\n\n");
  
    while ((de = readdir(dr)) != NULL){

        if(strcmp(de->d_name,"server_files")==0){
            drsf = opendir(de->d_name);
            if(drsf == NULL){
                printf("Could not open the file directory");
                return -1;
            }
            while((files=readdir(drsf)) != NULL){
              if(strlen(files->d_name)>=4){
                strcat(server_files,">>> ");
                strcat(server_files,files->d_name);
                strcat(server_files,"\n");
                i++;
              }
            }
            files_len = i;
        }

    }

    closedir(drsf);   
    closedir(dr); 
    return 1; 
} 

void startup(struct sockaddr_in addr, char *proto){
  
  printf("\n\t____SERVER IN ONLINE____\n\n");
  printf("[%s SOCKET INFO]\n\nIP address: %s\n", proto,inet_ntoa(addr.sin_addr));
  printf("Port: %d\n\n", (int) ntohs(addr.sin_port));

}

//-------------------- FILE FUNCTIONS -------------------

int send_file(int fd, int server_fd, char *message){
  
  char sendbuffer[BUF_SIZE];
  char *token;
  char dir[64];
  char enc[4];
  int i = 0;

  token = strtok(message, "|");

  while( token != NULL ) {

      if(i==3){
        strcpy(dir,"server_files/");
        strcat(dir,token);
      }else if(i==2){
        strcpy(enc,token);
      }
      token = strtok(NULL, "|");
      i++;
  }
  printf("Encryption: %s\n",enc);
  printf("Directory: %s\n", dir);
  FILE *fp = fopen(dir, "rb");
  if(fp == NULL){
        perror("File");
        return 2;
    }

  while( (server_fd = fread(sendbuffer, 1, sizeof(sendbuffer), fp))>0 ){
    if(strcmp(enc,"ENC")==0){
      encryption(sendbuffer,server_fd);
    }
    write(fd, sendbuffer, sizeof(sendbuffer));
  }

  memset(sendbuffer, 0, BUF_SIZE);
  printf("Upload Complete\n");
  return 1;
}


void prepare_file(int confd){

  FILE* fp = fopen("withinmp3.mp3", "wb");
  char buff[1023];
  int tot=0;
  int b;

  if(fp != NULL){
    while( (b = recv(confd, buff, 1024,0))> 0 ) {
      tot+=b;
      fwrite(buff, 1, b, fp);
    }

    printf("Received byte: %d\n",tot);
    if (b<0)
      perror("Receiving");
    fclose(fp);
  }else {
    perror("File");
  }
  printf("Download Complete\n");

}

//-------------------- LISTEN FUNCTIONS ------------------

void erro(char *msg){
  
  printf("Erro: %s\n", msg);
  exit(-1);
}


int get_op(char *message){

  int op = 0;

  if(strcmp(message,"LIST")==0){
    op = 1;
  }else if(strncmp(message,"DOWNLOAD",strlen("DOWNLOAD"))==0){
    op = 2;
  }else if(strcmp(message,"QUIT")==0){
    op = 0;
  }else if(strcmp(message,"NEW")==0){
    op = 3;
  }else{
    op = -1;
  }

  return op;

}

int reply(int clientfd, int fd, int op, char *buff){

  int flag = 0;
  char server_files[BUF_SIZE];

  if(op == 1){
    init_dir(server_files);
    printf("[->]Sending list of files\n");
    write(clientfd, server_files,sizeof(server_files));
    bzero(server_files,BUF_SIZE);
    flag = 1;
  }else if(op == 0){
    bzero(buff, BUF_SIZE); 
    strcat(buff,"Bye");
    write(clientfd, buff,sizeof(buff));
    flag = 0;
  }else if(op == 2){
    send_file(clientfd,fd,buff);
    strcpy(buff,"ENDFILE");
    write(clientfd, buff,sizeof(buff));
    flag = 0;
  }else if(op == 3){
    strcpy(buff,"NEW");
    write(clientfd, buff,sizeof(buff));
    flag = 1;
  }else{
    bzero(buff, BUF_SIZE); 
    strcpy(buff,"ERROR");
    write(clientfd, buff,sizeof(buff));
    flag = -1;
  }
  bzero(buff, BUF_SIZE); 
  return flag;

}


void message_protocol(int clientfd, int fd,int id){
    
    char buff[BUF_SIZE];
    int op = -2;
    int rep = -1;
    sprintf(buff, "%d", users);
    write(clientfd,buff,sizeof(buff));

    while(rep != 0){

      bzero(buff, BUF_SIZE); 
      read(clientfd, buff, sizeof(buff));
      printf("[+]New input from client[%d] : %s\n",id, buff); 
      op = get_op(buff);
      rep = reply(clientfd,fd,op,buff);

    }
    printf("[-]Closing client[%d] connection\n",clientfd);
}


void tcp_handler(int agc, char *argv[]){
  
  int number_of_connections = 0;
  struct sockaddr_in addr, client_addr;
  int client_addr_size;

  bzero((void *) &addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("192.168.1.6");
  addr.sin_port = htons(atoi(argv[1]));

  startup(addr,"TCP");

  if ( (fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  erro("na funcao socket");
  if ( bind(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0)
  erro("na funcao bind");
  if( listen(fd, atoi(argv[2])) < 0)
  erro("na funcao listen");
  
  printf("[!] MAXIMUM USER RANGE: %d\n\n",atoi(argv[2]));
  client_addr_size = sizeof(client_addr);
  users = atoi(argv[2]);

  while (1) {
    
    while(waitpid(-1,NULL,WNOHANG)>0);
    client = accept(fd,(struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size);
    number_of_connections++;

    if (client > 0) {
      if (fork() == 0) {
        printf("[+]New Connection: CLIENT[%d]\n",number_of_connections);
        message_protocol(client,fd,number_of_connections);
        exit(0);
      }
      close(client);
    }
    printf("[+]Socket Listenning\n");
  }

}


/********************************************************
*                       UDP PROTOCOL                    *
*********************************************************/


//-------------------- FILE FUNCTIONS -------------------

int send_file_udp(int fd, char *message,int slen,struct sockaddr_in si_outra ){
  
  char sendbuffer[BUF_SIZE];
  char *token;
  int server_fd;
  char dir[64];
  char enc[4];
  int i = 0;
  int tot = 0 ;
  int len;

  token = strtok(message, "|");
   
  while( token != NULL ) {

      if(i==3){
        strcpy(dir,"server_files/");
        strcat(dir,token);
      }else if(i==2){
        strcpy(enc,token);
      }
      token = strtok(NULL, "|");
      i++;
  }
  printf("Directory: %s\n", dir);
  FILE *fp = fopen(dir, "rb");
  fseek(fp, 0L, SEEK_END);
  len = ftell(fp);
  rewind(fp);
  sprintf(sendbuffer,"%d", len);
  sendto(fd, (const char *)sendbuffer, strlen(sendbuffer),MSG_CONFIRM, (const struct sockaddr *) &si_outra, slen); 
  if(fp == NULL){
        perror("File");
        return 2;
  }
  bzero(sendbuffer,BUF_SIZE);
  while( (server_fd = fread(sendbuffer, 1, sizeof(sendbuffer), fp))>0 ){
    if(strcmp(enc,"ENC")==0){
      encryption(sendbuffer,server_fd);
    }
    tot += server_fd;
    sendto(fd, (const char *)sendbuffer, strlen(sendbuffer),MSG_CONFIRM, (const struct sockaddr *) &si_outra, slen);
  }
  printf("Bytes: %d\n",tot );
  memset(sendbuffer, 0, BUF_SIZE);
  printf("Upload Complete\n");
  return 1;
}


//-------------------- LISTEN FUNCTIONS ------------------


int reply_udp(char* buff, int fdudp, int slen,struct sockaddr_in si_outra ){

  int flag = 0;
  char server_files[BUF_SIZE];
  char buffer[BUF_SIZE];
  int op = get_op(buff);
  strcpy(buffer,buff);
  bzero(buff,BUF_SIZE);

  if(op == 1){
    init_dir(server_files);
    printf("[->]Sending list of files\n");
    sendto(fdudp, (const char *)server_files, strlen(server_files),MSG_CONFIRM, (const struct sockaddr *) &si_outra, slen); 
    bzero(server_files,BUF_SIZE);
    flag = 1;
  }else if(op == 0){
    bzero(buff, BUF_SIZE); 
    strcat(buff,"Bye");
    sendto(fdudp, (const char *)buff, strlen(buff),MSG_CONFIRM, (const struct sockaddr *) &si_outra, slen); 
    flag = 0;
  }else if(op == 2){
    send_file_udp(fdudp,buffer,slen,si_outra);
    strcpy(buff,"ENDFILE");
    flag = 0;
  }else if(op == 3){
    strcpy(buff,"NEW");
    sendto(fdudp, (const char *)buff, strlen(buff),MSG_CONFIRM, (const struct sockaddr *) &si_outra, slen); 
    flag = 1;
  }else{
    bzero(buff, BUF_SIZE); 
    strcpy(buff,"ERROR");
    sendto(fdudp, (const char *)buff, strlen(buff),MSG_CONFIRM, (const struct sockaddr *) &si_outra, slen); 
    flag = -1;
  }
  bzero(buff, BUF_SIZE); 
  return flag;

}


void udp_handler(){

  struct sockaddr_in si_minha, si_outra;
  int recv_len;
  socklen_t slen = sizeof(si_outra);
  char buf[BUFLEN];

  // Cria um socket para recepção de pacotes UDP
  if((fdudp=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
  {
    erro("Erro na criação do socket");
  }

  si_minha.sin_family = AF_INET;
  si_minha.sin_port = htons(PORT);
  si_minha.sin_addr.s_addr = inet_addr("192.168.1.6");


  // Associa o socket à informação dslene endereço
  if(bind(fdudp,(struct sockaddr*)&si_minha, sizeof(si_minha)) == -1)
  {
    erro("Erro no bind");
  }

  startup(si_minha,"UDP");


  while(1){
      // Espera recepção de mensagem (a chamada é bloqueante)
    if((recv_len = recvfrom(fdudp, buf, BUFLEN, 0, (struct sockaddr *) &si_outra, (socklen_t *)&slen)) == -1)
    {
      erro("Erro no recvfrom");
    }

    // Para ignorar o restante conteúdo (anterior do buffer)
    buf[recv_len]='\0';

    printf("[+]NEW REQUEST FROM POR [%d]: %s\n",ntohs(si_outra.sin_port),buf);

    reply_udp(buf,fdudp,slen,si_outra);


    // Fecha socket e termina programa
  }

  close(fdudp);
}



/********************************************************
*                           MAIN                        *
*********************************************************/


int main(int argc, char *argv[]) {

  pid_t udp,tcp;

  if(!(tcp =fork())){
    sleep(1);
    tcp_handler(argc,argv);
    exit(0);
  }else if(!(udp=fork())){
    udp_handler();
    exit(0);
  }else{
    wait(&tcp);
    wait(&udp);
  }
  
  return 0;
}

