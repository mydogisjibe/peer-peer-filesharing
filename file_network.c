#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "file_network.h"



//Processes peer list to get ip and port info
char *get_ip_info(char* start, int *port) {
    char* port_str;

    for(port_str = start; *port_str != ':'; port_str++);
    
    *port_str = '\0';
    port_str++;

    char *next_string;
    for(next_string = port_str; *next_string != '\n'; next_string++);

    *next_string = '\0';
    next_string++;

    *port = atoi(port_str);
    return next_string;
}

//A negative connection number indicates connection failed, don't use
int get_connection_fd(char *ip, int port) {
    int result;
    struct sockaddr_in address;
    if((result = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Socket stream creation error\n");
        return result;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if(inet_pton(AF_INET, ip, &address) <= 0) {
        fprintf(stderr, "Address %s not supported\n", ip);
        close(result);
        return -1;
    }

    if(connect(result, (struct sockaddr *) &address, sizeof(address)) < 0) {
        fprintf(stderr, "Connection failed\n");
        close(result);
        return -1;
    }

    return result;
}



FileNetwork init_file_network(size_t file_size, char* peer_list) {
    FileNetwork result;
    result.next_unused_chunk = 0;
    result.file_size = file_size;
    //Calculate how many chunks are needed to store all the file
    result.num_chunks = (file_size+CHUNK_SIZE-1)/CHUNK_SIZE;
    result.chunks = malloc(sizeof(Chunk) * result.num_chunks);
    for(int i=0;i<result.num_chunks;i++) {
        result.chunks[i].index = 0;
    }
    for(int i=0;i<MAX_DOWNLOADS;i++) {
        result.downloads[i].connection = -1;
    }

    //Use the peer list to start establishing connections
    char* current_ip_str;
    char* next_ip_str;
    int port;
    int index = 0;
    struct sockaddr_in peer_address;
    while(*current_ip_str != '\0') {
        next_ip_str = get_ip_info(current_ip_str, &port);

        result.downloads[index] = start_download(current_ip_str, port, result.chunks+result.next_unused_chunk, result.next_unused_chunk);
        if(result.downloads[index].connection >= 0) {
            index++;
            result.next_unused_chunk++;
        }
        current_ip_str = next_ip_str;
    }
    return result;
}

//Negative connection means error, don't use.
Download start_download(char* ip, int port, Chunk *download_chunk, int64_t chunk_index) {
    Download result;
    result.current_chunk = download_chunk;
    result.connection = get_connection_fd(ip, port);

    //Write character 'c', then write chunk index.
    //if it fails, set connection to negative value and return
    const char get_chunk = 'c';
    if(result.connection >= 0) {
        int status;

        status = write(result.connection, &get_chunk, 1);
        if(status >= 0) {
            status = write(result.connection, &chunk_index, sizeof(chunk_index));
        }

        if(status < 0) {
            close(result.connection);
            result.connection = -1;
            return result;
        }
    }
    return result;
}

//If returns true, chunk has been downloaded and a new connection should be established.
enum DownloadStatus read_next_packet(Download *currentDownload) {
    size_t index = currentDownload->current_chunk->index;
    char * buffer = currentDownload->current_chunk->data;
    int len = read(currentDownload->connection, buffer + index, BUFFER_SIZE - index);

    if(len < 0) {
        fprintf(stderr, "Error with connection between peers. Closing...");
        return ERROR;
    }
    if(len == 0) {
        fprintf(stderr, "Obtained a chunk successfully. Closing connection");
        return DONE;
    }

    return CONTINUE;
}


void free_file_network(FileNetwork *to_delete) {
    free(to_delete->chunks);
}

