#include "proxy.h"
#include "cache.h"
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CACHE_BUCKETS_AMOUNT 100
#define DEFAULT_PORT 8080

proxy_t *proxy_create(int port, size_t connections_limit) {
  if (port <= 0 || port > 65535 || !connections_limit) {
    errno = EINVAL;
    return NULL;
  }

  proxy_t *proxy = malloc(sizeof(proxy_t));
  if (!proxy) {
    return NULL;
  }

  proxy->running = 0;
  proxy->port = port;
  proxy->connections_limit = connections_limit;

  proxy->connections = calloc(proxy->connections_limit, sizeof(proxy_conn_t *));
  if (!proxy->connections) {
    free(proxy);
    return NULL;
  }

  proxy->cache = cache_create(CACHE_BUCKETS_AMOUNT);
  if (!proxy->cache) {
    free(proxy->connections);
    free(proxy);
    return NULL;
  }

  return proxy;
}

void proxy_destroy(proxy_t *proxy) {
  if (!proxy) {
    errno = EINVAL;
    return;
  }

  proxy->running = 0;

  if (proxy->connections) {
    free(proxy->connections);
  }

  if (proxy->cache) {
    cache_destroy(proxy->cache);
  }

  free(proxy);
}

void proxy_run(proxy_t *proxy) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("socket");
    return;
  }

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
    perror("setsockopt");
    close(sock);
    return;
  }

  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(proxy->port);

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    close(sock);
    return;
  }

  if (listen(sock, proxy->connections_limit) < 0) {
    perror("listen");
    close(sock);
    return;
  }

  printf("Proxy server listening on port %d\n", proxy->port);

  proxy->running = 1;

  while (proxy->running) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(sock, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
      perror("accept");
      continue;
    }

    proxy_conn_t *conn = proxy_conn_create(client_fd);
    if (!conn) {
      perror("connection_create");
      close(client_fd);
      continue;
    }

    int is_cell_found = 0;
    while (proxy->running && !is_cell_found) {
      for (size_t i = 0; i < proxy->connections_limit; i++) {
        proxy_conn_t *curr_conn = proxy->connections[i];

        if (!curr_conn) {
          proxy->connections[i] = conn;
          is_cell_found = 1;
          break;
        }

        if (atomic_load(&curr_conn->state) == CONN_DONE) {
          proxy_conn_destroy(curr_conn);
          proxy->connections[i] = conn;
          is_cell_found = 1;
          break;
        }
      }
    }

    proxy_conn_run(proxy, conn);
  }

  close(sock);
}

void proxy_stop(proxy_t *proxy) {
  proxy->running = 0;

  for (size_t i = 0; i < proxy->connections_limit; i++) {
    proxy_conn_t *conn = proxy->connections[i];
    if (!conn) {
      continue;
    }
    proxy_conn_destroy(conn);
  }
}
