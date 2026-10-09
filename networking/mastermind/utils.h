#ifndef UTILS_H
#define UTILS_H

#include <netinet/in.h>

#define CODE_LENGTH 5
#define UDP_CHUNK_SIZE 512
#define MAX_RETRIES 10
#define MAX_TOTAL_CHUNKS 1024
#define RECV_TIMEOUT_SEC 5
#define SEND_RETRY_TIMEOUT_SEC 0.1
#define PKT_TYPE_DATA 0
#define PKT_TYPE_ACK  1

extern int logging_enabled;
void log_event(const char *fmt, ...);

void remove_newline(char *str);
int valid_code(const char *code);
int send_all(int fd, const char *buffer, int length);
int send_line(int fd, const char *message);
int recv_line(int fd, char *buffer, int size);
void print_feedback(const char *feedback);

void print_board_codebreaker(
    const char guesses[][CODE_LENGTH + 1],
    const char feedbacks[][CODE_LENGTH + 1],
    int attempts);

void print_board_mastermind(
    const char guesses[][CODE_LENGTH + 1],
    const char feedbacks[][CODE_LENGTH + 1],
    int attempts,
    const char *sequence);
int create_udp_game_socket(const char *port);

int udp_send_reliable(int fd, struct sockaddr_in *dest, const char *data);
int udp_recv_reliable(int fd, struct sockaddr_in *src, char *buffer, int size);

#endif