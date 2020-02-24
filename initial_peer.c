// Socket-related code downloaded from https://www.geeksforgeeks.org/socket-programming-cc/
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <string.h> 
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "file_network.h"

#define FILE_TO_SHARE "primes.txt"
#define MAX_PEERS 10
   
int main(int argc, char const *argv[]) 
{ 

    const char* ip = argv[1];
    int PORT = atoi(argv[2]);
    int sock = 0, valread; 
    struct sockaddr_in serv_addr; 
    char buffer[1024] = {0}; 
    int opt = 1;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        printf("\n Socket creation error \n"); 
        return -1; 
    } 

    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        printf("setsockopt with server failed\n");
        return -1;
    }

    struct linger lin;
    lin.l_onoff = 1;
    lin.l_linger = 0;
    setsockopt(sock, SOL_SOCKET, SO_LINGER, (const char *) &lin, sizeof(int));
   
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

    //Tell the server we're ready to get added to the list
    const char add = 'A';
    write(sock, &add , 1); 
    //Server will be giving out this port information. Save what it is so that we can connect to other peers.
    struct sockaddr_in my_address;
    int len = sizeof(my_address);
    getsockname(sock, (struct sockaddr *) &my_address, &len);
    int my_port = ntohs(my_address.sin_port);
    close(sock);
    //Quickly host a server with the same port
    int server_sock;
    if((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        printf("socket failed\n");
        return -1;
    }
    
    //Set socket to port from before
    if(setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        printf("setsockopt failed\n");
        return -1;
    }


    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(my_port);
    printf("Attempting to reuse port %d\n", my_port);
    if(bind(server_sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("Binding the hosting socket failed\n");
        return -1;
    }
    
    if(listen(server_sock, MAX_PEERS) < 0) {
        printf("Listening to socket failed\n");
        return -1;
    }


    
    //Serve up file to anyone who wants it

    int fd = open(FILE_TO_SHARE, O_RDONLY);
    if(fd < 0) {
        printf("File couldn't be opened.\n");
        return -1;
    }

    struct stat filestats;
    if(fstat(fd, &filestats) < 0) {
        printf("File stats failed.\n");
        return -1;
    }

    printf("FILE: %d bytes", filestats.st_size);

    char *file_buffer = malloc(filestats.st_size);
    if(!file_buffer) {
        printf("Failed to allocate enough memory to store file");
        return -1;
    }

    int buffer_fill_point = 0;
    while(buffer_fill_point != filestats.st_size) {
        int len = read(fd, file_buffer+buffer_fill_point, filestats.st_size-buffer_fill_point);
        if(len < 0) {
            printf("Error storing file to memory");
            return -1;
        }
        else if(len == 0) {
            printf("Error: file quit when whole thing wasn't stored to memory yet");
            break;
        }

        buffer_fill_point += len;
    }
    close(fd);


    fd_set read_set, write_set;
    int uploads[MAX_PEERS] = {0};
    int64_t upload_chunks[MAX_PEERS];
    size_t progress_through_chunks[MAX_PEERS];
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    while(1) {
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
        FD_SET(server_sock, &read_set);

        for(int i=0;i<MAX_PEERS;i++) {
            if(uploads[i]) {
                FD_SET(uploads[i], &write_set);
            }
        }

        if(select(FD_SETSIZE, &read_set, &write_set, NULL, NULL) < 0) {
            perror("Error attempting to receive client data");
        }

        //Accepting new downloads
        if(FD_ISSET(server_sock, &read_set)) {
            int new_socket = accept(server_sock, (struct sockaddr*) &address, (socklen_t *)&addrlen);
            if(new_socket < 0) {
                perror("Something went wrong earlier");
            }
                
            bool inserted = false;
            for(int i=0;i<MAX_PEERS;i++) {
                if(uploads[i] == 0) {
                    uploads[i] = new_socket;
                    progress_through_chunks[i] = 0;
                    inserted = true;
                    char request;
                    int len = read(uploads[i], &request, 1);
                    if(len != 1 || request != 'C') {
                        printf("Peer made unknown request of length %d and text %d\n", len, request);
                        if(len < 0) {
                            perror("Somethings wrong");
                        }
                        inserted = false;
                        uploads[i] = 0;
                        break;
                    }
                    len = read(uploads[i], upload_chunks + i, sizeof(int64_t));
                    if(len != sizeof(int64_t)) {
                        printf("Peer failed to ask for a chunk\n");
                    }
                    else {
                        size_t write_from = CHUNK_SIZE*upload_chunks[i] + progress_through_chunks[i];
                        size_t full_data = (upload_chunks[i]+1)*CHUNK_SIZE >= buffer_fill_point ? buffer_fill_point % CHUNK_SIZE : CHUNK_SIZE;
                        ssize_t len = write(uploads[i], file_buffer + write_from, full_data - write_from);
                        if(len <= 0) {
                            printf("Error: peer lost\n");
                            uploads[i] = 0;
                        }
                        progress_through_chunks[i] += len;
                        if(progress_through_chunks[i] == full_data) {
                            close(uploads[i]);
                            uploads[i] = 0;
                        }

                    }
                    break;
                }
                if(!inserted) {
                    close(uploads[i]);
                }
            }
        }

        //Responding to already existing peers
        for(int i=0;i<MAX_PEERS;i++) {
            if(uploads[i] != 0 && FD_ISSET(uploads[i], &write_set)) {
                size_t write_from = CHUNK_SIZE*upload_chunks[i] + progress_through_chunks[i];
                size_t full_data = (upload_chunks[i]+1)*CHUNK_SIZE >= buffer_fill_point ? buffer_fill_point % CHUNK_SIZE : CHUNK_SIZE;
                ssize_t len = write(uploads[i], file_buffer + write_from, full_data - write_from);
                if(len <= 0) {
                    printf("Error: peer lost\n");
                    uploads[i] = 0;
                }
                progress_through_chunks[i] += len;
                if(progress_through_chunks[i] == full_data) {
                    close(uploads[i]);
                    uploads[i] = 0;
                }
            }
        }


    }



    return 0;
}  

