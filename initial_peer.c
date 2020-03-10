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
#include <unistd.h>

#include "file_network.h"

#define FILE_TO_SHARE "primes.txt"
#define MAX_PEERS 10

int64_t get_requested_chunk(int read_stream) {
    char request;
    int64_t requested_chunk;

    int len = read(read_stream, &request, 1);
    if(request == 'D') {
        printf("Peer is finished downloading.\n");
        return -1;
    }
    if(len == 0) {
        printf("Connection closed unexpectedly\n");
        return -1;
    }
    if(len != 1 || request != 'C') {
        printf("Peer made unknown request of length %d and text %d\n", len, request);
        if(len < 0) {
            perror("Failure when trying to read request type");
        }
        return -1;
    }

    len = read(read_stream, &requested_chunk, sizeof(int64_t));

    if(len != sizeof(int64_t)) {
        printf("Peer failed to ask for a chunk\n");
        if(len < 0) {
            perror("Failure when trying to get chunk id");
        }
        return -1;
    }

    return requested_chunk;
}


char * send_to_peer(int write_stream, char *start, size_t length ) {
    ssize_t len = write(write_stream, start, length);
    if(len <= 0) {
        printf("Error: peer lost\n");
        return NULL;
    }
    start += len;
    return start;
}


   
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



    //Store file into RAM for easy sending
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



    //Create Server
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


    



    fd_set read_set, write_set;
    int uploads[MAX_PEERS] = {0};
    int64_t upload_chunks[MAX_PEERS];
    char *upload_progress[MAX_PEERS];
    char *upload_end[MAX_PEERS];
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    while(1) {
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);

        //Find unused space in uploads array for new socket. If we can't find any, don't accept any new connections
        int new_index;
        for(new_index = 0;uploads[new_index] != 0 && new_index < MAX_PEERS;new_index++);
        if(new_index < MAX_PEERS) {
            FD_SET(server_sock, &read_set);
        }

        //Add any uploads currently in progress
        for(int i=0;i<MAX_PEERS;i++) {
            if(uploads[i]) {
                FD_SET(uploads[i], &read_set);
                if(upload_progress[i] != upload_end[i]){
                    FD_SET(uploads[i], &write_set);
                }
            }
        }

        if(select(FD_SETSIZE, &read_set, &write_set, NULL, NULL) < 0) {
            perror("Error attempting to receive client data");
        }

        //Accepting new downloads
        if(FD_ISSET(server_sock, &read_set)) {
            //has_failed set to true will skip through all the other code to exit this if statement
            bool has_failed = false;
            int new_socket = accept(server_sock, (struct sockaddr*) &address, (socklen_t *)&addrlen);
            if(new_socket < 0) {
                perror("Something went wrong trying to accept a new socket");
                has_failed = true;
            }
            
            //Get the chunk wanted from the client
            int64_t chunk_id;
            if(!has_failed) {
                chunk_id = get_requested_chunk(new_socket);
                if(chunk_id < 0 || chunk_id*CHUNK_SIZE >= filestats.st_size) {
                    close(new_socket);
                    printf("Failed to establish connection either because of error or malformed request\n");
                    has_failed = true;
                }
            }
            
            //Send chunk to client
            if(!has_failed) {
                printf("Attempting first write\n");
                size_t len = (chunk_id+1)*CHUNK_SIZE >= buffer_fill_point ? buffer_fill_point % CHUNK_SIZE : CHUNK_SIZE;
                upload_end[new_index] = file_buffer+chunk_id*CHUNK_SIZE+len;
                upload_progress[new_index] = send_to_peer(new_socket, upload_end[new_index]-len, len);
                if(upload_progress[new_index] == NULL) {
                    close(new_socket);
                    has_failed = true;
                }

            }
            
            //If past steps succeeded, record the new_socket id
            if(!has_failed) {
                printf("First write success! Wrote %d bytes\n", CHUNK_SIZE - (upload_end[new_index] - upload_progress[new_index]));
                uploads[new_index] = new_socket;
            }
        }

        //Responding to already existing peers
        for(int i=0;i<MAX_PEERS;i++) {
            if(uploads[i] != 0 && FD_ISSET(uploads[i], &write_set)) {
                printf("Ready for more writing\n"); //DEBUG
                upload_progress[i] = send_to_peer(uploads[i], upload_progress[i], upload_end[i] - upload_progress[i]);
                if(upload_progress[i] == NULL) {
                    printf("More writing failed. Closing\n"); //DEBUG
                    close(uploads[i]);
                    uploads[i] = 0;
                }
            }

            if(uploads[i] != 0 && FD_ISSET(uploads[i], &read_set)) {
                int64_t chunk_id = get_requested_chunk(uploads[i]);
                if(chunk_id < 0 || chunk_id*CHUNK_SIZE >= filestats.st_size) {
                    printf("Connection dropped due to unexpected closing or unknown query\n");
                    close(uploads[i]);
                    uploads[i] = 0;
                    continue;
                }
                

                size_t len = (chunk_id+1)*CHUNK_SIZE >= buffer_fill_point ? buffer_fill_point % CHUNK_SIZE : CHUNK_SIZE;
                upload_end[i] = file_buffer+chunk_id*CHUNK_SIZE+len;
                upload_progress[i] = send_to_peer(uploads[i], upload_end[i]-len, len);
                if(upload_progress[i] == NULL) {
                    printf("Initial write failed\n"); //DEBUG
                    close(uploads[i]);
                    uploads[i] = 0;
                }
            }
        }
    }



    return 0;
}  

