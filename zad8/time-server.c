//
// Created by resul on 09.05.2020.
//

#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "err.h"

#define B_SIZE 256

int main(int argc, char **argv) {
    /* program arguments */
    char *multicast_dotted_addres;
    in_port_t local_port;
    /* socket variables and structures */
    int sock;
    struct sockaddr_in local_address;
    struct ip_mreq ip_mreq;
    /* communication variables */
    char buffer[B_SIZE];
    ssize_t rcv_len;
    /* parse arguments */
    if (argc != 3) {
        fatal("usage: %s dotted_server_address server_port", argv[1]);
    }
    multicast_dotted_addres = argv[1];
    local_port = (in_port_t)atoi(argv[2]);
    /* open socket */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        syserr("opening socket");
    }
    /* connect to multicast group */
    ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (inet_aton(multicast_dotted_addres, &ip_mreq.imr_multiaddr) == 0) {
        fprintf(stderr, "ERROR: inet_aton - invalid multicast address\n");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&ip_mreq, sizeof(ip_mreq)) < 0) {
        syserr("setsockopt");
    }
    /* set server port and address */
    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_port = htons(local_port);
    if (bind(sock, (struct sockaddr*)&local_address, sizeof(local_address)) < 0) {
        syserr("bind");
    }
    while(1) {
        struct sockaddr_in requester;
        socklen_t fromlen = sizeof(requester);
        time_t time_buffer;
        size_t length;
        rcv_len = recvfrom(sock, buffer, B_SIZE, 0, (struct sockaddr *) &requester, &fromlen);
        if (rcv_len < 0) {
            syserr("receivefrom");
        }
        if (strncmp("GET TIME", buffer, rcv_len) != 0) {
            printf("Received unknown command: %.*s", (int) rcv_len, buffer);
        } else {
            printf("Request from: %s\n", inet_ntoa(requester.sin_addr));
            time(&time_buffer);
            strncpy(buffer, ctime(&time_buffer), B_SIZE);
            length = strnlen(buffer, B_SIZE);
            if (sendto(sock, buffer, length, 0, (struct sockaddr *) &requester, sizeof(requester)) != length) {
                syserr("sendto");
            }
        }
    }
}