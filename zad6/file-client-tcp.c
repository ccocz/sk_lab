/*
 Program uruchamiamy z trzema parametrami: nazwa serwera, numer jego portu i nazwa pliku.
 Program spróbuje połączyć się z serwerem, po czym będzie od nas pobierał
 plik i wysyłał je do serwera.
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "err.h"

#define BUFFER_SIZE      1024

int main (int argc, char *argv[]) {
  int rc;
  int sock;
  struct addrinfo addr_hints, *addr_result;
  char line[BUFFER_SIZE];

  /* Kontrola dokumentów ... */
  if (argc != 4) {
    fatal("Usage: %s host port file name", argv[0]);
  }
  
  /* open file */
  FILE *fp = fopen(argv[3], "r");
  if (fp == NULL) {
    syserr("file");
  }
  
  /* find file size */
  fseek(fp, 0, SEEK_END);
  int file_size = (int)ftell(fp);
  fseek(fp, 0, SEEK_SET);
  

  sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0) {
    syserr("socket");
  }

  /* Trzeba się dowiedzieć o adres internetowy serwera. */
  memset(&addr_hints, 0, sizeof(struct addrinfo));
  addr_hints.ai_flags = 0;
  addr_hints.ai_family = AF_INET;
  addr_hints.ai_socktype = SOCK_STREAM;
  addr_hints.ai_protocol = IPPROTO_TCP;

  rc =  getaddrinfo(argv[1], argv[2], &addr_hints, &addr_result);
  if (rc != 0) {
    fprintf(stderr, "rc=%d\n", rc);
    syserr("getaddrinfo: %s", gai_strerror(rc));
  }

  if (connect(sock, addr_result->ai_addr, addr_result->ai_addrlen) != 0) {
    syserr("connect");
  }
  freeaddrinfo(addr_result);
  
  /* send file name */
  if (write(sock, argv[3], strlen(argv[3])) < 0) {
    syserr("writing on stream socket");
  }
  
  /* send file size */
  sprintf(line, "%d", file_size);
  
  sleep(1);
  
  if (write(sock, line, strlen(line)) < 0) {
    syserr("writing on stream socket");
  }
  
  while (fgets(line, BUFFER_SIZE, fp) != NULL) {
    if (write(sock, line, strlen(line)) < 0) {
      syserr("writing on stream socket");
    }
  }
  
  printf("file sent successfully\n");
  
  fclose(fp);
  
  if (close(sock) < 0)
    syserr("closing stream socket");

  return 0;
}

