#pragma once

#include "cache.h"

enum {
  CONN_CREATED = 0,
  CONN_RUN = 1,
  CONN_DONE = 2,
};

typedef struct {
  int client_fd;
  atomic_int state;
  pthread_t thread;
} proxy_conn_t;

typedef struct {
  int port;
  atomic_int running;
  proxy_conn_t **connections;
  size_t connections_limit;
  cache_t *cache;
} proxy_t;

// returns initialized and prepared for run proxy
proxy_t *proxy_create(int port, size_t connections_limit);

// destroys proxy
void proxy_destroy(proxy_t *proxy);

// blocking function, starts proxy
void proxy_run(proxy_t *proxy);

// stops proxy
void proxy_stop(proxy_t *proxy);

// returns initialized connection structure, ready for data streaming
proxy_conn_t *proxy_conn_create(int client_fd);

// releases connection resources and waits for it's ending
void proxy_conn_destroy(proxy_conn_t *connection);

// starts data streaming between client and cache
void proxy_conn_run(proxy_t *proxy, proxy_conn_t *connection);
