#ifndef UDP_CODEBREAKER_H
#define UDP_CODEBREAKER_H

#include <netinet/in.h>

void udp_codebreaker_game(int fd, struct sockaddr_in *opponent_addr);

#endif