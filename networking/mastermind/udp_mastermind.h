#ifndef UDP_MASTERMIND_H
#define UDP_MASTERMIND_H

#include <netinet/in.h>

int udp_mastermind_game(int fd, struct sockaddr_in *opponent_addr, const char *opponent_name);

#endif