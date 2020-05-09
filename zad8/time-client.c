//
// Created by resul on 09.05.2020.
//

#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "err.h"

#define B_SIZE 256
#define TTL_VALUE 4
#define WAITING_TIME 3000

int main(int argc, char **argv) {
    /* program arguments */
    char *dotted_address;
    in_port_t remote_port;
    /* socket variables and structures */
    int sock;
    int optval;
    socklen_t fromlen;
    struct sockaddr_in remote_address;
    /* communication variables */
    char buffer[B_SIZE];
    size_t length;
    ssize_t rcv_len;
    /* parse arguments */
    if (argc != 3) {
        fatal("usage: %s remote_address remote_port", argv[0]);
    }
    dotted_address = argv[1];
    remote_port = (in_port_t)atoi(argv[2]);
    /* open socket */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        syserr("while opening socket");
    }
    /* broadcast option enable */
    optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *)&optval, sizeof(optval)) < 0) {
        syserr("setsockopt broadcast");
    }
    /* set ttl value for datagrams sent to the group */
    optval = TTL_VALUE;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void *)&optval, sizeof(optval)) < 0) {
        syserr("setsockopt multicast ttl");
    }
    /* recipient address and port setting */
    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(remote_port);
    if (inet_aton(dotted_address, &remote_address.sin_addr) == 0) {
        fprintf(stderr, "ERROR: inet_aton - invalid multicast address\n");
        exit(EXIT_FAILURE);
    }
    /* use poll to wait 3 seconds */
    struct pollfd file_d[1];
    file_d[0].fd = sock;
    file_d[0].events = POLLIN;
    /* try to get response from any of the servers 3 times */
    for (int try = 1; try <= 3; try++) {
        printf("Sending request [%d]\n", try);
        /* write message */
        strcpy(buffer, "GET TIME");
        length = strnlen(buffer, B_SIZE);
        if (sendto(sock, buffer, length, 0, (struct sockaddr*)&remote_address, sizeof(remote_address)) != length) {
            syserr("sento");
        }
        /* wait 3 seconds for response */
        int ret = poll(file_d, 1, WAITING_TIME);
        if (ret == 0) {
            continue;
        } else {
            int pollin_happened = file_d[0].events & POLLIN;
            if (pollin_happened) {
                struct sockaddr_in sender;
                fromlen = sizeof(sender);
                rcv_len = recvfrom(sock, buffer, B_SIZE, 0, (struct sockaddr*)&sender, &fromlen);
                if (rcv_len < 0) {
                    syserr("receive from");
                }
                printf("Response from: %s\n", inet_ntoa(sender.sin_addr));
                printf("Received time: %.*s\n", (int)rcv_len, buffer);
                close(sock);
                return 0;
            } else {
                syserr("poll stopped");
            }
        }
    }
    printf("Timeout: unable to receive response.\n");
    close(sock);
    return 0;
}

