// Socket-related code downloaded from https://www.geeksforgeeks.org/socket-programming-cc/
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <string.h> 
#include <unistd.h>
   
int main(int argc, char const *argv[]) 
{ 

    const char* ip = argv[1];
    int PORT = atoi(argv[2]);
    int sock = 0, valread; 
    struct sockaddr_in serv_addr; 
    char buffer[1048576] = {0}; 
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        printf("\n Socket creation error \n"); 
        return -1; 
    } 
   
    memset(&serv_addr, 0, sizeof(serv_addr)); 
   
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(PORT); 
       
    // Convert IPv4 and IPv6 addresses from text to binary form 
    if(inet_pton(AF_INET, ip, &serv_addr.sin_addr)<=0)  
    { 
        printf("\nInvalid address/ Address not supported \n"); 
        return -1; 
    } 
   
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    { 
        printf("\nConnection Failed \n"); 
        return -1; 
    } 

    printf("Enter string: ");
    char data[130];
    char* message = data;
    size_t size = 130;
    if(getline(&message, &size, stdin) == -1){
        printf("Failed to read line");
        return -1;
    }
    //Remove end newline
    message[0] = 'C';
    message[1] = '\0';
    message[2] = '\0';
    message[3] = '\0';
    message[4] = '\0';
    message[5] = '\0';
    message[6] = '\0';
    message[7] = '\0';
    message[8] = '\0';
    
    send(sock , message , 9, 0); 
    while(1){
        valread = read( sock , buffer, 1048576); 
        if( valread == 0)
            break;
        if(valread == -1)
            continue;

        buffer[valread] = '\0';


        printf("From server: %d\n", valread);
    }
    return 0; 
}  
