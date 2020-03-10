#include <stdbool.h>
#include <stdint.h>
#define CHUNK_SIZE 1048576 //1MB
#define MAX_DOWNLOADS 10




//Index is used when downloading to determine where to add on new data
struct Chunk {
    char data[CHUNK_SIZE];
    ssize_t index;
    ssize_t length;
};
typedef struct Chunk Chunk;


struct Download {
    int connection;
    char* ip;
    int port;
    Chunk *current_chunk;
};
typedef struct Download Download;


enum DownloadStatus {
    CONTINUE,
    DONE,
    ERROR
};


struct FileNetwork {
    size_t file_size;
    size_t num_chunks;
    int64_t next_unused_chunk;
    Chunk *chunks;
    Download downloads[MAX_DOWNLOADS];
};
typedef struct FileNetwork FileNetwork;



FileNetwork init_file_network(size_t file_size, char *peer_list);
//Used by initial peer to load file into memory for uploading

//A negative connection number indicates connection failed, don't use
Download start_download(char* ip, int port, Chunk *download_chunk, int64_t chunk_index);

enum DownloadStatus read_next_packet(Download *current_download);

void restart_download(Download* download, Chunk* download_chunk, int64_t chunk_index);

void free_file_network(FileNetwork *to_delete);

int get_connection_fd(const char *ip, const int port);

