//Most of socket-related code pulled from https://www.geeksforgeeks.org/socket-programming-cc/
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_PEERS 5

char ipList[MAX_PEERS*INET_ADDRSTRLEN] = "No IPs yet";
size_t ipSize = 0;
int peerNum = 0;
int main(int argc, char const *argv[])
{
    if (argc < 2){
        printf("No port specified\n");
        return 1;
    }
    int PORT = atoi(argv[1]);
    
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    //255 max file name length, 3 characters either GET or SET
    char buffer[259] = {0};

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                                                  &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address,
                                 sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while(1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
            (socklen_t*)&addrlen))<0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        int len = read( new_socket, buffer, 256);
        if(len != -1) buffer[len] = '\0';

        printf("Query: %s\n", buffer);
        
        if(buffer[0] == 'P') {
            if(peerNum < MAX_PEERS) {
                peerNum++;
                inet_ntop(AF_INET, &(address.sin_addr), ipList+ipSize, INET_ADDRSTRLEN);
                ipSize += strlen(ipList + ipSize) + 1;
                ipList[ipSize-1] = '\n';
                send(new_socket, "ADDED", 5, 0);
            }
            else {
                send(new_socket, "FULL", 4, 0);
            }
        }
        else if(buffer[0] == 'D') {
            if(ipList[0] != '\0')
                send(new_socket, ipList, ipSize, 0);
            else
                send(new_socket, "EMPTY", 5, 0);
        }
            
        close(new_socket);
    }
    return 0;
}
