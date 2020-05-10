#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "err.h"

#define LINE_SIZE 100

void *handle_connection (void *s_ptr) {
  int ret, s;
  socklen_t len;
  char line[LINE_SIZE + 1], peername[LINE_SIZE + 1], peeraddr[LINE_SIZE + 1];
  struct sockaddr_in addr;

  s = *(int *)s_ptr;
  free(s_ptr);

  len = sizeof(addr);

  /* Któż to do nas dzwoni (adres)?  */
  ret = getpeername(s, (struct sockaddr *)&addr, &len);
  if (ret == -1) 
    syserr("getsockname");

  inet_ntop(AF_INET, &addr.sin_addr, peeraddr, LINE_SIZE);
  snprintf(peername, 2*LINE_SIZE, "%s:%d", peeraddr, ntohs(addr.sin_port));

  /* read file size and name */
  char f_size[LINE_SIZE + 1], f_name[LINE_SIZE + 1];
  memset(f_size, 0, sizeof(f_size));
  memset(f_name, 0, sizeof(f_name));
  ret = read(s, f_name, sizeof(f_name) - 1);
  if (ret == -1) {
    syserr("read");
  }
  
  ret = read(s, f_size, sizeof(f_size) - 1);
  if (ret == -1) {
    syserr("read");
  }
  printf("new client [%s] size = [%s] file = [%s]\n", peername, f_size, f_name);
  
  /* open file */
  FILE *fp = fopen(f_name, "w");
  if (fp == NULL) {
    syserr("file");
  }
  
  while (read(s, line, sizeof(line) - 1) > 0) {
    fprintf(fp, "%s", line);
    fflush(fp);
  }
  
  printf("client [%s] has sent its file of size=[%s]\n", peername, f_size);
  printf("total size of uploaded files [%s]\n", f_size);
  close(s);
  fclose(fp);
  return 0;
}

int main (int argc, char **argv) {
  int ear, rc;
  socklen_t len;
  struct sockaddr_in server;
  
  /* parameter check */
  
  if (argc != 2) {
    fatal("Usage: %s port", argv[0]);
  }

  /* Tworzymy gniazdko */
  ear = socket(PF_INET, SOCK_STREAM, 0);
  if (ear == -1) 
    syserr("socket");

  /* Podłączamy do centrali */
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(atoi(argv[1]));
  rc = bind(ear, (struct sockaddr *)&server, sizeof(server));
  if (rc == -1) 
    syserr("bind");


  /* Każdy chce wiedzieć jaki to port */
  len = (socklen_t)sizeof(server);
  rc = getsockname(ear, (struct sockaddr *)&server, &len);
  if (rc == -1) 
    syserr("getsockname");
 
  printf("Listening at port %d\n", (int)ntohs(server.sin_port));

  rc = listen(ear, 5);
  if (rc == -1) 
    syserr("listen");
  
  /* No i do pracy */
  for (;;) {
    int msgsock;
    int *con;
    pthread_t t;

    msgsock = accept(ear, (struct sockaddr *)NULL, NULL);
    if (msgsock == -1) {
      syserr("accept");
    }

    /* Tylko dla tego wątku */
    con = malloc(sizeof(int));
    if (!con) {
      syserr("malloc");
    }
    *con = msgsock;

    rc = pthread_create(&t, 0, handle_connection, con);
    if (rc == -1) {
      syserr("pthread_create");
    }

    /* No przecież nie będę na niego czekał ... */
    rc = pthread_detach(t);
    if (rc == -1) {
      syserr("pthread_detach");
    }
  }
}

