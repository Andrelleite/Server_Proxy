#include <stdlib.h>
#include <stdio.h> 
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <errno.h> 
  
int main(void) 
{ 
    struct dirent *de;  // Pointer for directory entry 
    struct dirent *files; // Pointer for server files directory entry

    DIR *dr = opendir("."); 
    DIR *drsf;

  
    if (dr == NULL)  
    { 
        printf("Could not open current directory" ); 
        return 0; 
    } 
  
    

    while ((de = readdir(dr)) != NULL){

        if(strcmp(de->d_name,"server_files")==0){
            drsf = opendir(de->d_name);
            if(drsf == NULL){
                printf("Could not open the file directory");
                return -1;
            }
            while((files=readdir(drsf)) != NULL){
                printf("%s\n",files->d_name);
            }
        }

    }

    
    closedir(drsf);   
    closedir(dr);  
    return 0; 
} 
