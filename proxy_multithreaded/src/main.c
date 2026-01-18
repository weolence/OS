#include "proxy.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CONNECTIONS_LIMIT 100

static proxy_t *global_proxy = NULL;

static void signal_handler(int sig) {
  printf("\nReceived signal %d, stopping proxy...\n", sig);
  if (global_proxy) {
    proxy_stop(global_proxy);
  }
}

void print_usage(const char *prog_name) {
  printf("Usage: %s [-p PORT]\n", prog_name);
}

int main(int argc, char *argv[]) {
  int port = 0;
  int opt;

  while ((opt = getopt(argc, argv, "p:h")) != -1) {
    switch (opt) {
    case 'p':
      port = atoi(optarg);
      break;
    case 'h':
      print_usage(argv[0]);
      return 0;
    default:
      printf("Error: Unknown option\n");
      print_usage(argv[0]);
      return 1;
    }
  }

  if (port == 0) {
    printf("Error: Port number required\n");
    print_usage(argv[0]);
    return 1;
  }

  proxy_t *proxy = proxy_create(port, CONNECTIONS_LIMIT);
  if (!proxy) {
    printf("Failed to create proxy\n");
    return 1;
  }

  global_proxy = proxy;

  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGINT, &sa, NULL) == -1) {
    perror("sigaction SIGINT");
    proxy_destroy(proxy);
    return 1;
  }

  if (sigaction(SIGTERM, &sa, NULL) == -1) {
    perror("sigaction SIGTERM");
    proxy_destroy(proxy);
    return 1;
  }

  signal(SIGPIPE, SIG_IGN);

  printf("Press Ctrl+C to stop\n");

  proxy_run(proxy);

  proxy_destroy(proxy);

  printf("Proxy server stopped\n");

  return 0;
}
