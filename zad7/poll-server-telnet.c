/*
 Ten program używa poll(), aby równocześnie obsługiwać wielu klientów
 bez tworzenia procesów ani wątków.
*/

#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include "err.h"

#define TRUE 1
#define FALSE 0
#define BUF_SIZE 1024

static int finish = FALSE;

/* Obsługa sygnału kończenia */
static void catch_int (int sig) {
  finish = TRUE;
  fprintf(stderr,
          "Signal %d catched. No new connections will be accepted.\n", sig);
}

void bind_socket(const int socket_fd, const char *port, const char *which) {
  struct sockaddr_in server;
  size_t length;
  /* Co do adresu nie jesteśmy wybredni */
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(atoi(port));
  if (bind(socket_fd, (struct sockaddr*)&server,
           (socklen_t)sizeof(server)) == -1) 
    syserr("Binding stream socket");

  /* Dowiedzmy się, jaki to port i obwieśćmy to światu */
  length = sizeof(server);
  if (getsockname (socket_fd, (struct sockaddr*)&server,
                   (socklen_t*)&length) == -1) 
    syserr("Getting socket name");
  printf("%s socket port #%u\n", which, (unsigned)ntohs(server.sin_port));
}

int main (int argc, char **argv) {
  if (argc != 3) {
    syserr("usage: %s client_port control_port", argv[0]);
  }
  struct pollfd client[_POSIX_OPEN_MAX];
  char buf[BUF_SIZE];
  ssize_t rval;
  int msgsock, activeClients, i, ret;
  struct sigaction action;
  sigset_t block_mask;

  fprintf(stderr,"_POSIX_OPEN_MAX = %d\n", _POSIX_OPEN_MAX);

  /* Po Ctrl-C kończymy */
  sigemptyset (&block_mask);
  action.sa_handler = catch_int;
  action.sa_mask = block_mask;
  action.sa_flags = SA_RESTART;
  
  if (sigaction (SIGINT, &action, 0) == -1)  
    syserr("sigaction");
 
  /* Inicjujemy tablicę z gniazdkami klientów, client[0] to gniazdko centrali */
  for (i = 0; i < _POSIX_OPEN_MAX; ++i) {
    client[i].fd = -1;
    client[i].events = POLLIN;
    client[i].revents = 0;
  }
  activeClients = 0;

  /* Tworzymy gniazdko centrali */
  client[0].fd = socket(PF_INET, SOCK_STREAM, 0);
  if (client[0].fd == -1) 
    syserr("Opening client stream socket");
  
  /* open socket for telnet */
  client[1].fd = socket(PF_INET, SOCK_STREAM, 0);
  if (client[1].fd == -1) {
    syserr("Opening telnet stream socket");
  }
  
  bind_socket(client[0].fd, argv[1], "client");
  bind_socket(client[1].fd, argv[2], "telnet");
  
  /* Zapraszamy klientów */
  if (listen(client[0].fd, 5) == -1) 
    syserr("Starting to listen clietn");
  
  /* welcome telnet */
  if (listen(client[1].fd, 5) == -1) {
    syserr("Starting to list telnet");
  }
  
  int total_clients = 0;
 
  /* Do pracy */
  do {
    for (i = 0; i < _POSIX_OPEN_MAX; ++i)
      client[i].revents = 0;
      
    /* Po Ctrl-C zamykamy gniazdko centrali */
    if (finish == TRUE && client[0].fd >= 0) {
      if (close(client[0].fd) < 0)
        syserr("close");
      client[0].fd = -1;
    }

    /* Czekamy przez 5000 ms */
    ret = poll(client, _POSIX_OPEN_MAX, 5000);
    if (ret == -1)
      if (errno == EINTR) fprintf(stderr, "Interrupted system call\n");
      else syserr("poll");
    else if (ret > 0) {
      if (finish == FALSE && (client[0].revents & POLLIN)) {
        /* Przyjmuję nowe połączenie */
        msgsock = accept(client[0].fd, (struct sockaddr*)0, (socklen_t*)0);
        if (msgsock == -1)
          syserr("accept");
        else {
          for (i = 3; i < _POSIX_OPEN_MAX; ++i) {
            if (client[i].fd == -1) {
              fprintf(stderr, "Received new connection (%d)\n", i);	      
              total_clients++;
              client[i].fd = msgsock;
              client[i].events = POLLIN;
              activeClients += 1;
              break;
            }
          }
          if (i >= _POSIX_OPEN_MAX) {
            fprintf(stderr, "Too many clients\n");
            if (close(msgsock) < 0)
              syserr("close");
          }
        }
      }
      if (finish == FALSE && (client[1].revents & POLLIN)) {
        /* new telnet */
        msgsock = accept(client[1].fd, (struct sockaddr*)0, (socklen_t*)0);
        if (msgsock == -1) {
          syserr("accept");
        }
        if (client[2].fd != -1) {
          fprintf(stderr, "Too many telnet");
          if (close(msgsock) < 0) {
            syserr("close");
          }
        }
        client[2].fd = msgsock;
        client[2].events = POLLIN;
      }
      /* check telnet messsage */
      if (client[2].fd != -1 && (client[2].revents & (POLLIN | POLLERR))) {
        rval = read(client[2].fd, buf, BUF_SIZE);
        if (rval < 0) {
          fprintf(stderr, "Reading message (%d, %s)\n", errno, strerror(errno));
        }
        if (!strncmp(buf, "count", strlen("count"))) {
          printf("Number of active clients: %d\n", activeClients);
          printf("Total number of clients: %d\n", total_clients);
        }
        if (close(client[2].fd) < 0) {
          syserr("close");
        }
        client[2].fd = -1;
        client[2].revents = 0;
      }
      /* check clients messages */
      for (i = 3; i < _POSIX_OPEN_MAX; ++i) {
        if (client[i].fd != -1 && (client[i].revents & (POLLIN | POLLERR))) {	  
          rval = read(client[i].fd, buf, BUF_SIZE);
          if (rval < 0) {
	    fprintf(stderr, "Reading message (%d, %s)\n", errno, strerror(errno));
            if (close(client[i].fd) < 0)
              syserr("close");
            client[i].fd = -1;
            activeClients -= 1;
          }
          else if (rval == 0) {
            fprintf(stderr, "Ending connection\n");
            if (close(client[i].fd) < 0)
              syserr("close");
            client[i].fd = -1;
            activeClients -= 1;
          }
          else
            printf("-->%.*s\n", (int)rval, buf);
        }
      }
    }
    else
      fprintf(stderr, "Do something else\n");
  } while (finish == FALSE || activeClients > 0);

  if (client[0].fd >= 0)
    if (close(client[0].fd) < 0)
      syserr("Closing main socket");
  exit(EXIT_SUCCESS);
}
