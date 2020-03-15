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
        result.chunks[i].finished_downloading = false;
        result.chunks[i].peers_with_file = 0;
    }
    Chunk *last_chunk = result.chunks + result.num_chunks-1;
    last_chunk->index = 0;
    last_chunk->length = file_size % CHUNK_SIZE;
    result.available_chunks_len = (result.file_size+CHUNK_SIZE*8-1)/CHUNK_SIZE/8;

    for(int i=0;i<MAX_DOWNLOADS;i++) {
        result.downloads[i].connection = -1;
        result.downloads[i].available_chunks = malloc(result.available_chunks_len);
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

void get_queue(FileNetwork *fn) {
    for(int i=0;i<MAX_DOWNLOADS;i++) {
        Download* download = fn->downloads + i;
        if(download->connection == -1) continue;
        
        const char getList = 'L';
        int len = write(download->connection, &getList, 1);
        if(len < 0) {
            perror("Error tying to request peer's chunks");
            close(download->connection);
            download->connection = -1;
            continue;
        }
    }

    for(int i=0;i<MAX_DOWNLOADS;i++) {
        Download *download = fn->downloads + i;
        if(download->connection == -1) continue;

        int len = read(download->connection, download->available_chunks, fn->available_chunks_len);
        if(len != fn->available_chunks_len) {
            perror("Error trying to read peer's chunks");
            close(download->connection);
            download->connection = -1;
            continue;
        }

        for(int j=0;j<fn->num_chunks;j++) {
            if(download->available_chunks[j/8] & (1 << (j%8))) {
                fn->chunks[j].peers_with_file++;
            }
        }
    }

        
}

int64_t choose_next_chunk(FileNetwork* fn, Download *download) {
    int64_t next = -1;
    int next_value;
    for(int i=0;i<fn->num_chunks;i++) {
        if(download->available_chunks[i/8] & (1 << (i%8))
            && !(fn->chunks[i].finished_downloading)
            && fn->chunks[i].peers_with_file < next) {

            next = i;
            next_value = fn->chunks[i].peers_with_file;
        }
    }

   return next;
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

//As an uploading peer, receive a request from a downloader and execute it if possible
void receive_request(FileNetwork *fn, Upload *upload) {
    char request;
    int len = read(upload->connection, &request, 1);

    if(len <= 0) {
        if(len < 0)
            perror("Failed to read message");
        else
            fprintf(stderr, "Unexpected close from downloader\n");
        close(upload->connection);
        upload->connection = -1;
        return;
    }

    
    switch(request) {
        //Requesting a specific chunk. Semicolon to fix declaration error, please ignore
        case 'C': ;
            int64_t chunk_index;
            len = read(upload->connection, &chunk_index, sizeof(int64_t));
            if (len < sizeof(int64_t) || chunk_index >= fn->num_chunks || !fn->chunks[chunk_index].finished_downloading) {
                if(len < 0)
                    perror("Failed to read chunk index");
                else
                    fprintf(stderr, "Unexpected close from downloader, or unexpected chunk_index\n");
                close(upload->connection);
                upload->connection = -1;
                return;
            }
            upload->current_chunk = fn->chunks + chunk_index;
            Chunk *current_chunk = upload->current_chunk;

            len = write(upload->connection, current_chunk->data, current_chunk->length);
            if (len < 0) {
                perror("Failed to write data to downloader");
                close(upload->connection);
                upload->connection = -1;
                return;
            }
            upload->current_chunk->index = len;
            break;

        //Requesting a list of chunks that can be downloaded. Semicolon is just to fix error, ignore
        case 'L': ;
           size_t message_len = (fn->file_size+CHUNK_SIZE*8-1)/CHUNK_SIZE/8;
           char *message = malloc(message_len);
           for(int i=0;i<fn->num_chunks;i++) {
               message[i/8] |= fn->chunks[i].finished_downloading << (i%8);
           }
           len = write(upload->connection, message, message_len);
           if (len < 0) {
               perror("Failed to send available chunks");
               close(upload->connection);
               upload->connection = -1;
               return;
           }
           break;
    }

}

void send_next_packet(Upload *current_upload) {
    Chunk *current_chunk = current_upload->current_chunk;
    char *start = current_chunk->data + current_chunk->index;
    size_t length = current_chunk->length - current_chunk->index;
    int len = write(current_upload->connection, start, length);
    if(len <= 0) {
        printf("Error: downloader lost\n");
        close(current_upload->connection);
        current_upload->connection = -1;
        return;
    }
    current_chunk->index += len;
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
        current_chunk->finished_downloading = true;
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
        free(to_delete->downloads[i].available_chunks);
    }
    free(to_delete->chunks);
}

