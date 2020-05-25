#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/util.h>

#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "err.h"

#define BUF_SIZE 4
#define META_SIZE 100

struct event_base *base;
struct bufferevent *bev;

void filein_cb (evutil_socket_t descriptor, short ev, void *arg) {
  unsigned char buf[BUF_SIZE+1];

  int r = read(descriptor, buf, BUF_SIZE);
  if (r < 0)
    syserr("read (from file)");
  if (r == 0) {
    fprintf(stderr, "file closed. Exiting event loop.\n");
    if (event_base_loopbreak(base) == -1) syserr("event_base_loopbreak");
    return;
  }
  printf("SENDING: %s", buf);
  if (bufferevent_write(bev, buf, r) == -1)
    syserr("bufferevent_write");
}

void a_read_cb (struct bufferevent *bev, void *arg) {
  char buf[BUF_SIZE+1];

  while (evbuffer_get_length(bufferevent_get_input(bev))) {
    int r = bufferevent_read(bev, buf, BUF_SIZE);
    if (r == -1)
      syserr("bufferevent_read");
    buf[r] = 0;
    printf("\n--> %s\n", buf);
  }
}

void an_event_cb (struct bufferevent *bev, short what, void *arg) {
  if (what & BEV_EVENT_CONNECTED) {
    fprintf(stderr, "Connection made.\n");
    return;
  }
  if (what & BEV_EVENT_EOF)
    fprintf(stderr, "EOF encountered.\n");
  else if (what & BEV_EVENT_ERROR)
    fprintf(stderr, "Unrecoverable error.\n");
  else if (what & BEV_EVENT_TIMEOUT)
    fprintf(stderr, "A timeout occured.\n");
  if (event_base_loopbreak(base) == -1)
    syserr("event_base_loopbreak");
}

int main(int argc, char *argv[])
{
  /* Kontrola dokumentów ... */
  if (argc != 4)
    fatal("Usage: %s hostname port file_name\n", argv[0]);

  // Jeśli chcemy, żeby wszystko działało nieco wolniej, ale za to
  // działało dla wejścia z pliku, to należy odkomentować linijki
  // poniżej, a zakomentować aktualne przypisanie do base
  struct event_config *cfg = event_config_new();
  event_config_avoid_method(cfg, "epoll");
  base = event_base_new_with_config(cfg);
  event_config_free(cfg);

  /*base = event_base_new();*/
  if (!base)
    syserr("event_base_new");

  bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
  if (!bev)
    syserr("bufferevent_socket_new");
  bufferevent_setcb(bev, a_read_cb, NULL, an_event_cb, (void *)bev);

  struct addrinfo addr_hints = {
    .ai_flags = 0,
    .ai_family = AF_INET,
    .ai_socktype = SOCK_STREAM,
    .ai_protocol = 0,
    .ai_addrlen = 0,
    .ai_addr = NULL,
    .ai_canonname = NULL,
    .ai_next = NULL
  };
  struct addrinfo *addr;

  if (getaddrinfo(argv[1], argv[2], &addr_hints, &addr))
    syserr("getaddrinfo");

  if (bufferevent_socket_connect(bev, addr->ai_addr, addr->ai_addrlen) == -1)
    syserr("bufferevent_socket_connect");
  freeaddrinfo(addr);
  if (bufferevent_enable(bev, EV_READ | EV_WRITE) == -1)
    syserr("bufferevent_enable");

  /* open file */
  puts("OK");
  FILE *file = fopen(argv[3], "r");
  int file_fd = fileno(file);
  if (file_fd == -1) {
      syserr("opening file");
  }
  /* find file size */
  struct stat stbuf;
  stat( argv[3], &stbuf);		/* 1 syscall */
  long int size = stbuf.st_size;

  /* beforehand send file name and size to server */
  if (bufferevent_write(bev, argv[3], strlen(argv[3])) == -1) {
      syserr("bufferevent_write name");
  }
  char buff[20];
  memset(buff, 0, sizeof(buff));
  sprintf(buff, "#%ld!", size);
  if (bufferevent_write(bev, buff, 19) == -1) {
      syserr("bufferevent_write size");
  }

  struct event *filein_event =
          event_new(base, file_fd, EV_READ | EV_PERSIST, filein_cb, NULL);
  if (!filein_event)
    syserr("event_new");
  if (event_add(filein_event, NULL) == -1)
    syserr("event_add");

  printf("Entering dispatch loop.\n");
  if (event_base_dispatch(base) == -1)
    syserr("event_base_dispatch");
  printf("Dispatch loop finished.\n");

  bufferevent_free(bev);
  event_base_free(base);
  close(file_fd);
  return 0;
}
