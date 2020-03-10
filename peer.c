// Socket-related code downloaded from https://www.geeksforgeeks.org/socket-programming-cc/
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <string.h> 
#include <unistd.h>
#include <fcntl.h>

#include "file_network.h"

#define FILE_NAME "download.txt"
#define FILE_SIZE 51099000
#define BUFFER_SIZE 4096
   

int main(int argc, char const *argv[]) 
{ 

    const char* ip = argv[1];
    int PORT = atoi(argv[2]);
    struct sockaddr_in serv_addr; 
    char buffer[BUFFER_SIZE] = {0}; 

    int sock = get_connection_fd(ip, PORT);
    if(sock < 0) {
        fprintf(stderr, "IP: %s, PORT: %d", ip, PORT);
        perror("Socket is bad");
        return -1;
    }
    const char download_request = 'D';
    write(sock , &download_request, 1); 
    int len = read(sock, buffer, BUFFER_SIZE);
    if(len <= 0) {
        perror("ERROR: problem getting download list from server");
    }
    buffer[len+1] = '\0';
    fprintf(stderr, "Download list:\n%s", buffer);
    FileNetwork net = init_file_network(FILE_SIZE, buffer);
    
    int number_of_downloads = 0;
    for(int i=0;i<MAX_DOWNLOADS;i++) {
        if(net.downloads[i].connection != -1) {
            number_of_downloads++;
        }
    }

    fd_set dnlwd_set;
    while(number_of_downloads > 0){
        FD_ZERO(&dnlwd_set);

        for(int i=0;i<MAX_DOWNLOADS;i++) {
            if(net.downloads[i].connection != -1) {
                FD_SET(net.downloads[i].connection, &dnlwd_set);
            }
        }
        
        if(select(FD_SETSIZE, &dnlwd_set, NULL, NULL, NULL) < 0) {
            perror("Error attempting to download data");
        }

        //Update downloads with data
        for(int i=0;i<MAX_DOWNLOADS;i++) {
            if(net.downloads[i].connection != -1 && FD_ISSET(net.downloads[i].connection, &dnlwd_set)) {
                printf("Attempting to do a read of some data\n");
                enum DownloadStatus stat = read_next_packet(&(net.downloads[i])); 
                switch(stat) {
                    case DONE:
                        if(net.next_unused_chunk < net.num_chunks) {
                            printf("Attempting to establish a new connection");
                            restart_download(net.downloads + i, net.chunks + net.next_unused_chunk, net.next_unused_chunk);
                            if(net.downloads[i].connection < 0) {
                                perror("Re-establishing a connection failed");
                            }
                            else {
                                printf("Now working on chunk %d\n", net.next_unused_chunk);
                                net.next_unused_chunk++;
                            }
                        }
                        else {
                            number_of_downloads--;
                            printf("Dude we're done here!%d\n", number_of_downloads);
                        }
                        break;
                    case ERROR:
                        fprintf(stderr, "Connection failed to keep being maintained.");
                        break;
                    case CONTINUE:
                        break;
                }

            }
        }
    }

    printf("Download complete! Attempting to save to file\n");
    
    int fd = open(FILE_NAME, O_CREAT | O_WRONLY);
    if(fd < 0) {
        perror("Failed to make file");
    }

    for(int i=0;i<net.num_chunks;i++) {
        ssize_t index = 0;
        Chunk *chunk = net.chunks + i;
        while(index < chunk->index) {
            int len = write(fd, chunk->data + index, chunk->index - index);
            if(len < 0) {
                perror("Error writing to file");
                return -1;
            }
            index += len;
        }
    }
    close(fd);

    printf("File save complete! You can find your file at %s\n", FILE_NAME);

    return 0; 
}
