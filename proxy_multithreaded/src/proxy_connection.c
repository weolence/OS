#include "proxy.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_PORT 80
#define BUFFER_SIZE 32768
#define MAX_METHOD 16
#define MAX_VERSION 16
#define MAX_HOST 256
#define MAX_URL 2048

int connect_to_server(const char *host, int port) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("socket");
    return -1;
  }

  struct hostent *server = gethostbyname(host);
  if (!server) {
    perror("gethostbyname");
    close(sock);
    return -1;
  }

  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  memcpy(&addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("connect");
    close(sock);
    return -1;
  }

  return sock;
}

/* ===== parsing utilities begin ===== */

int parse_http_request(const char *buffer, char *method, char *url,
                       char *version) {
  const char *space1 = strchr(buffer, ' ');
  if (!space1)
    return -1;

  size_t method_len = space1 - buffer;
  strncpy(method, buffer, method_len);
  method[method_len] = '\0';

  const char *space2 = strchr(space1 + 1, ' ');
  if (!space2)
    return -1;

  size_t url_len = space2 - (space1 + 1);
  strncpy(url, space1 + 1, url_len);
  url[url_len] = '\0';

  const char *end = strstr(space2 + 1, "\r\n");
  if (!end)
    end = strchr(space2 + 1, '\n');
  if (!end)
    return -1;

  size_t version_len = end - (space2 + 1);
  strncpy(version, space2 + 1, version_len);
  version[version_len] = '\0';

  return 0;
}

void extract_host_path(const char *url, char *host, char *path) {
  const char *url_start = url;
  if (strncmp(url, "http://", 7) == 0) {
    url_start = url + 7;
  }

  const char *slash = strchr(url_start, '/');
  if (slash) {
    size_t host_len = slash - url_start;
    strncpy(host, url_start, host_len);
    host[host_len] = '\0';

    strcpy(path, slash);
  } else {
    strcpy(host, url_start);
    strcpy(path, "/");
  }

  char *colon = strchr(host, ':');
  if (colon) {
    *colon = '\0';
  }
}

/* ===== parsing utilities end ===== */

// loads data from host to cache
void *loader_routine(void *arg) {
  if (!arg) {
    errno = EINVAL;
    return NULL;
  }

  cache_entry_t *entry = (cache_entry_t *)arg;

  char host[MAX_HOST];
  char path[MAX_URL];
  extract_host_path(entry->key, host, path);

  int server_fd = connect_to_server(host, DEFAULT_PORT);
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

  char *data = NULL;
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
      size_t new_capacity = capacity == 0 ? (size_t)n * 2 : capacity * 2;
      if (new_capacity < size + n) {
        new_capacity = size + n;
      }

      char *new_data = realloc(data, new_capacity);
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

// handles client connection(1 thread = 1 connection)
void *client_routine(void *arg) {
  proxy_t *proxy = (proxy_t *)arg;

  proxy_conn_t *conn = NULL;
  while (!conn) {
    for (size_t i = 0; i < proxy->connections_limit; i++) {
      proxy_conn_t *curr = proxy->connections[i];
      if (curr && pthread_equal(curr->thread, pthread_self())) {
        conn = curr;
        break;
      }
    }
  }

  int client_fd = conn->client_fd;
  cache_t *cache = proxy->cache;
  cache_entry_t *entry = NULL;
  const char *error_message = NULL;
  int entry_locked = 0;
  pthread_t loader;

  char buffer[BUFFER_SIZE];
  ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
  if (n <= 0) {
    perror("client_routine:recv");
    goto cleanup;
  }
  buffer[n] = '\0';

  char method[MAX_VERSION], url[MAX_URL], version[MAX_VERSION];
  if (parse_http_request(buffer, method, url, version) < 0) {
    error_message = "HTTP/1.0 400 Bad Request\r\n\r\n";
    goto send_error;
  }

  // only GET supported
  if (strcmp(method, "GET") != 0) {
    error_message = "HTTP/1.0 405 Method Not Allowed\r\n\r\n";
    goto send_error;
  }

  printf("Request: %s %s\n", method, url);

  entry = cache_acquire(cache, url);
  if (!entry) {
    error_message = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
    goto send_error;
  }

  pthread_mutex_lock(&entry->lock);
  entry_locked = 1;

  if (entry->state == DONE) {
    pthread_mutex_unlock(&entry->lock);
    entry_locked = 0;
    send(client_fd, entry->data, entry->data_size, 0);
    goto cleanup;
  }

  if (entry->state == LOADING) {
    while (entry->state == LOADING) {
      pthread_cond_wait(&entry->cond, &entry->lock);
    }

    if (entry->state == DONE) {
      pthread_mutex_unlock(&entry->lock);
      entry_locked = 0;
      send(client_fd, entry->data, entry->data_size, 0);
    } else {
      pthread_mutex_unlock(&entry->lock);
      entry_locked = 0;
      error_message = "HTTP/1.0 502 Bad Gateway\r\n\r\n";
      goto send_error;
    }

    goto cleanup;
  }

  if (entry->state == REQUIRED) {
    entry->state = LOADING;
    pthread_mutex_unlock(&entry->lock);
    entry_locked = 0;

    if (pthread_create(&loader, NULL, loader_routine, entry) != 0) {
      perror("pthread_create");
      pthread_mutex_lock(&entry->lock);
      entry_locked = 1;
      entry->state = ERROR;
      pthread_mutex_unlock(&entry->lock);
      entry_locked = 0;

      error_message = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
      goto send_error;
    }

    pthread_detach(loader);

    pthread_mutex_lock(&entry->lock);
    entry_locked = 1;
    while (entry->state == LOADING) {
      pthread_cond_wait(&entry->cond, &entry->lock);
    }

    if (entry->state == DONE) {
      pthread_mutex_unlock(&entry->lock);
      entry_locked = 0;
      send(client_fd, entry->data, entry->data_size, 0);
    } else {
      pthread_mutex_unlock(&entry->lock);
      entry_locked = 0;
      error_message = "HTTP/1.0 502 Bad Gateway\r\n\r\n";
      goto send_error;
    }

    goto cleanup;
  }

  pthread_mutex_unlock(&entry->lock);
  entry_locked = 0;
  error_message = "HTTP/1.0 502 Bad Gateway\r\n\r\n";
  goto send_error;

send_error:
  if (error_message) {
    send(client_fd, error_message, strlen(error_message), 0);
  }

cleanup:
  if (entry_locked) {
    pthread_mutex_unlock(&entry->lock);
  }
  if (entry) {
    cache_release(cache, entry);
  }
  if (client_fd >= 0) {
    close(client_fd);
  }
  if (conn) {
    conn->state = CONN_DONE;
  }
  return NULL;
}

proxy_conn_t *proxy_conn_create(int client_fd) {
  if (client_fd < 0) {
    errno = EINVAL;
    return NULL;
  }

  proxy_conn_t *connection = malloc(sizeof(proxy_conn_t));
  if (!connection) {
    return NULL;
  }

  connection->client_fd = client_fd;
  connection->state = CONN_CREATED;

  return connection;
}

void proxy_conn_destroy(proxy_conn_t *connection) {
  if (!connection) {
    errno = EINVAL;
    return;
  }

  if (atomic_load(&connection->state) != CONN_DONE) {
    pthread_cancel(connection->thread);
  }

  pthread_join(connection->thread, NULL);

  free(connection);
}

void proxy_conn_run(proxy_t *proxy, proxy_conn_t *connection) {
  if (!connection || !proxy ||
      atomic_load(&connection->state) != CONN_CREATED) {
    errno = EINVAL;
    return;
  }

  if (pthread_create(&connection->thread, NULL, client_routine, proxy)) {
    connection->state = CONN_DONE;
    return;
  }

  connection->state = CONN_RUN;
}
