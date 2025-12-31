#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include "cache.h"

#define BUFFER_SIZE 8192
#define MAX_URL 2048
#define DEFAULT_PORT 8080

/* ===== parsing utilities begin ===== */
int parse_http_request(const char* buffer, char* method, char* url, char* version) {
    const char* space1 = strchr(buffer, ' ');
    if (!space1) return -1;
    
    size_t method_len = space1 - buffer;
    strncpy(method, buffer, method_len);
    method[method_len] = '\0';
    
    const char* space2 = strchr(space1 + 1, ' ');
    if (!space2) return -1;
    
    size_t url_len = space2 - (space1 + 1);
    strncpy(url, space1 + 1, url_len);
    url[url_len] = '\0';
    
    const char* end = strstr(space2 + 1, "\r\n");
    if (!end) end = strchr(space2 + 1, '\n');
    if (!end) return -1;
    
    size_t version_len = end - (space2 + 1);
    strncpy(version, space2 + 1, version_len);
    version[version_len] = '\0';
    
    return 0;
}

void extract_host_path(const char* url, char* host, char* path) {
    const char* url_start = url;
    if (strncmp(url, "http://", 7) == 0) {
        url_start = url + 7;
    }
    
    const char* slash = strchr(url_start, '/');
    if (slash) {
        size_t host_len = slash - url_start;
        strncpy(host, url_start, host_len);
        host[host_len] = '\0';
        
        strcpy(path, slash);
    } else {
        strcpy(host, url_start);
        strcpy(path, "/");
    }
    
    char* colon = strchr(host, ':');
    if (colon) {
        *colon = '\0';
    }
}
/* ===== parsing utilities end ===== */

int connect_to_server(const char* host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    
    struct hostent* server = gethostbyname(host);
    if (!server) {
        fprintf(stderr, "Cannot resolve host: %s\n", host);
        close(sock);
        return -1;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }
    
    return sock;
}

/* ===== loader-thread begin ===== */
// loads data from host to cache
void* loader_routine(void* arg) {
    if (!arg) {
        errno = EINVAL;
        return NULL;
    }

    cache_entry_t* entry = (cache_entry_t*)arg;

    char host[256] = {0};
    char path[MAX_URL] = {0};
    extract_host_path(entry->key, host, path);

    int server_fd = connect_to_server(host, 80);
    if (server_fd < 0) {
        pthread_mutex_lock(&entry->lock);
        entry->state = ERROR;
        pthread_cond_broadcast(&entry->cond);
        pthread_mutex_unlock(&entry->lock);
        return NULL;
    }

    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "\r\n",
             path, host);
    
    if (send(server_fd, request, strlen(request), 0) < 0) {
        perror("loader_routine:send");
        close(server_fd);
        pthread_mutex_lock(&entry->lock);
        entry->state = ERROR;
        pthread_cond_broadcast(&entry->cond);
        pthread_mutex_unlock(&entry->lock);
        return NULL;
    }
   
    char* data = NULL;
    size_t capacity = 0;
    size_t size = 0;
    char buffer[BUFFER_SIZE];
    
    while (1) {
        ssize_t n = recv(server_fd, buffer, sizeof(buffer), 0);
        if (n < 0) {
            perror("loader_routine:recv");
            break;
        }
        if (n == 0) {
            break;
        }

        if (size + n > capacity) {
            size_t new_capacity = capacity == 0 ? n * 2 : capacity * 2;
            if (new_capacity < size + n) {
                new_capacity = size + n;
            }
            
            char* new_data = realloc(data, new_capacity);
            if (!new_data) {
                perror("loader_routine:realloc");
                break;
            }
            data = new_data;
            capacity = new_capacity;
        }

        memcpy(data + size, buffer, n);
        size += n;
    }
    
    close(server_fd);
    
    pthread_mutex_lock(&entry->lock);
    if (data && size > 0) {
        entry->data = data;
        entry->data_size = size;
        entry->data_capacity = size;
        entry->state = DONE;
    } else {
        free(data);
        entry->state = ERROR;
    }
    pthread_cond_broadcast(&entry->cond);
    pthread_mutex_unlock(&entry->lock);
    
    return NULL;
}
/* ===== loader-thread end ===== */


/* ===== connection-handler-thread begin ===== */
typedef struct {
    int client_fd;
    cache_t* cache;
} thread_data_t;

// handles client connection(1 thread = 1 connection)
void* client_routine(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int client_fd = data->client_fd;
    cache_t* cache = data->cache;
    
    free(data);

    char buffer[BUFFER_SIZE];
    ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        perror("client_routine:recv");
        close(client_fd);
        return NULL;
    }
    buffer[n] = '\0';

    char method[16], url[MAX_URL], version[16];
    if (parse_http_request(buffer, method, url, version) < 0) {
        const char* error = "HTTP/1.0 400 Bad Request\r\n\r\n";
        send(client_fd, error, strlen(error), 0);
        close(client_fd);
        return NULL;
    }
    
    // only GET supported
    if (strcmp(method, "GET") != 0) {
        const char* error = "HTTP/1.0 405 Method Not Allowed\r\n\r\n";
        send(client_fd, error, strlen(error), 0);
        close(client_fd);
        return NULL;
    }
    
    printf("Request: %s %s\n", method, url);
    
    cache_entry_t* entry = cache_acquire(cache, url);
    if (!entry) {
        const char* error = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
        send(client_fd, error, strlen(error), 0);
        close(client_fd);
        return NULL;
    }
    
    pthread_mutex_lock(&entry->lock);
    
    if (entry->state == DONE) {
        pthread_mutex_unlock(&entry->lock);
        send(client_fd, entry->data, entry->data_size, 0);
        cache_release(cache, entry);
        close(client_fd);
        return NULL;
    }
    else if (entry->state == LOADING) {
        while (entry->state == LOADING) {
            pthread_cond_wait(&entry->cond, &entry->lock);
        }
        
        if (entry->state == DONE) {
            pthread_mutex_unlock(&entry->lock);
            send(client_fd, entry->data, entry->data_size, 0);
        } else {
            pthread_mutex_unlock(&entry->lock);
            const char* error = "HTTP/1.0 502 Bad Gateway\r\n\r\n";
            send(client_fd, error, strlen(error), 0);
        }
        
        cache_release(cache, entry);
        close(client_fd);
        return NULL;
    }
    else if (entry->state == REQUIRED) {
        entry->state = LOADING;
        pthread_mutex_unlock(&entry->lock);
        
        pthread_t loader;
        if (pthread_create(&loader, NULL, loader_routine, entry) != 0) {
            perror("pthread_create");
            pthread_mutex_lock(&entry->lock);
            entry->state = ERROR;
            pthread_mutex_unlock(&entry->lock);
            
            const char* error = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
            send(client_fd, error, strlen(error), 0);
            
            cache_release(cache, entry);
            close(client_fd);
            return NULL;
        }
        
        pthread_detach(loader);
        
        pthread_mutex_lock(&entry->lock);
        while (entry->state == LOADING) {
            pthread_cond_wait(&entry->cond, &entry->lock);
        }
        
        if (entry->state == DONE) {
            pthread_mutex_unlock(&entry->lock);
            send(client_fd, entry->data, entry->data_size, 0);
        } else {
            pthread_mutex_unlock(&entry->lock);
            const char* error = "HTTP/1.0 502 Bad Gateway\r\n\r\n";
            send(client_fd, error, strlen(error), 0);
        }
        
        cache_release(cache, entry);
        close(client_fd);
        return NULL;
    }
    else {
        pthread_mutex_unlock(&entry->lock);
        const char* error = "HTTP/1.0 502 Bad Gateway\r\n\r\n";
        send(client_fd, error, strlen(error), 0);
        cache_release(cache, entry);
        close(client_fd);
        return NULL;
    }
}
/* ===== connection-handler-thread end ===== */

void run_proxy(int port) {
    cache_t* cache = cache_create(100);
    if (!cache) {
        fprintf(stderr, "Failed to create cache\n");
        return;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        cache_destroy(cache);
        return;
    }
    
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(sock);
        cache_destroy(cache);
        return;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        cache_destroy(cache);
        return;
    }
    
    if (listen(sock, 10) < 0) {
        perror("listen");
        close(sock);
        cache_destroy(cache);
        return;
    }
    
    printf("Proxy server listening on port %d\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        
        pthread_t thread;
        thread_data_t* data = malloc(sizeof(thread_data_t));
        if (!data) {
            perror("malloc");
            close(client_fd);
            continue;
        }
        
        data->client_fd = client_fd;
        data->cache = cache;
        
        if (pthread_create(&thread, NULL, client_routine, data) != 0) {
            perror("pthread_create");
            free(data);
            close(client_fd);
            continue;
        }
        
        pthread_detach(thread);
    }

    close(sock);
    cache_destroy(cache);
}

int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port number. Using default %d\n", DEFAULT_PORT);
            port = DEFAULT_PORT;
        }
    }
    
    run_proxy(port);

    return 0;
}