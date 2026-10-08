#ifndef UTILS_H
#define UTILS_H

#define CODE_LENGTH 5

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

#endif