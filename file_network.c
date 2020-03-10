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
    for(next_string = port_str; *next_string != '\n' && *next_string != '\0'; next_string++);

    if(*next_string != '\0') {
        *next_string = '\0';
        next_string++;
    }

    *port = atoi(port_str);
    return next_string;
}

//A negative connection number indicates connection failed, don't use
int get_connection_fd(const char *ip, const int port) {
    int result;
    struct sockaddr_in address;
    if((result = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Socket stream creation error\n");
        return result;
    }

    int opt = 1;
    if(setsockopt(result, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        fprintf(stderr, "setsockopt failed\n");
        close(result);
        return -1;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if(inet_pton(AF_INET, ip, &address.sin_addr) <= 0) {
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
    //All chunks have length of CHUNK_SIZE except last has remaining needed for file
    result.num_chunks = (file_size+CHUNK_SIZE-1)/CHUNK_SIZE;
    result.chunks = malloc(sizeof(Chunk) * result.num_chunks);
    for(int i=0;i<result.num_chunks-1;i++) {
        result.chunks[i].index = 0;
        result.chunks[i].length = CHUNK_SIZE;
    }
    Chunk *last_chunk = result.chunks + result.num_chunks-1;
    last_chunk->index = 0;
    last_chunk->length = file_size % CHUNK_SIZE;

    for(int i=0;i<MAX_DOWNLOADS;i++) {
        result.downloads[i].connection = -1;
    }

    //Use the peer list to start establishing connections
    char* current_ip_str = peer_list;
    char* next_ip_str;
    int port;
    int index = 0;
    struct sockaddr_in peer_address;
    while(*current_ip_str != '\0') {
        next_ip_str = get_ip_info(current_ip_str, &port);
        printf("Connecting to ip %s and port %d\n", current_ip_str, port);

        result.downloads[index] = start_download(current_ip_str, port, result.chunks+result.next_unused_chunk, result.next_unused_chunk);
        if(result.downloads[index].connection >= 0) {
            result.downloads[index].ip = current_ip_str;
            result.downloads[index].port = port;
            index++;
            result.next_unused_chunk++;
        }
        current_ip_str = next_ip_str;
    }
    return result;
}

//Negative connection means error, don't use.
Download start_download(char* ip, int port, Chunk *download_chunk, int64_t chunk_index) {
    printf("Attempting to start download\n");
    Download result;
    result.current_chunk = download_chunk;
    result.connection = get_connection_fd(ip, port);

    //Write character 'C', then write chunk index.
    //if it fails, set connection to negative value and return
    const char get_chunk = 'C';
    if(result.connection >= 0) {
        int status;

        status = write(result.connection, &get_chunk, 1);
        if(status >= 0) {
            status = write(result.connection, &chunk_index, sizeof(chunk_index));
        }

        if(status < 0) {
            perror("Failed to write message");
            close(result.connection);
            result.connection = -1;
            return result;
        }
    }
    else {
        perror("Failed to get connection");
    }
    printf("Download start successful\n");
    return result;
}

void restart_download(Download* download, Chunk* download_chunk, int64_t chunk_index) {
    download->current_chunk = download_chunk;

    //Write character 'C', then write chunk index.
    //if it fails, set connection to negative value and return
    const char get_chunk = 'C';
    if(download->connection >= 0) {
        int status;

        status = write(download->connection, &get_chunk, 1);
        if(status >= 0) {
            status = write(download->connection, &chunk_index, sizeof(chunk_index));
        }

        if(status < 0) {
            perror("Failed to write message");
            close(download->connection);
            download->connection = -1;
        }
    }
    else {
        perror("Attempted to ask for chunk on errored connection");
    }
}

//If returns true, chunk has been downloaded and a new connection should be established.
enum DownloadStatus read_next_packet(Download *current_download) {
    Chunk *current_chunk = current_download->current_chunk;
    size_t index = current_chunk->index;
    char *buffer = current_chunk->data;
    printf("Attempting to read a packet. We can read up to %d bytes right now\n", CHUNK_SIZE-index);
    int len = read(current_download->connection, buffer+index, CHUNK_SIZE-index);
    printf("Got read of len %d\n", len);

    if(len <= 0) {
        fprintf(stderr, "Error with connection between peer, or unexpected close, %d. Closing...\n", len);
        if(len < 0) {
            perror("Printout of the error");
        }
        close(current_download->connection);
        current_download->connection = -1;
        return ERROR;
    }

    current_chunk->index += len;
    if(current_chunk->index == current_chunk->length) {
        fprintf(stderr, "Obtained a chunk successfully. Trying to get next one\n");
        return DONE;
    }
    printf("Packet obtained successfully. Now at %d\n", current_chunk->index);
    return CONTINUE;
}


void free_file_network(FileNetwork *to_delete) {
    for(int i=0;i<MAX_DOWNLOADS;i++) {
        if(to_delete->downloads[i].connection != -1) {
            const char done = 'D';
            write(to_delete->downloads[i].connection, &done, 1);
            close(to_delete->downloads[i].connection);
        }
    }
    free(to_delete->chunks);
}

