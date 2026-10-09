#include <stdio.h>
#include <string.h>
#include "utils.h"
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>

#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define RED "\033[31m"
#define RESET "\033[0m"

int logging_enabled = 0;

void log_event(const char *fmt, ...) {
    if (!logging_enabled) return;

    FILE *log_file = fopen("log.txt", "a");
    if (!log_file) return;

    char time_buffer[30];
    struct timeval tv;
    time_t curtime;

    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec;
    strftime(time_buffer, 30, "%Y-%m-%d %H:%M:%S", localtime(&curtime));

    fprintf(log_file, "[%s.%06ld] [LOG] ", time_buffer, (long)tv.tv_usec);

    va_list args;
    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    va_end(args);

    fprintf(log_file, "\n");
    fclose(log_file);
}

void remove_newline(char *str) {
    str[strcspn(str, "\n")] = '\0';
}

int valid_code(const char *code) {
    if (strlen(code) != CODE_LENGTH) return 0;
    for (int i = 0; i < CODE_LENGTH; i++) {
        if (code[i] < '0' || code[i] > '9') return 0;
    }
    return 1;
}

int send_all(int fd, const char *buffer, int length) {
    int sent = 0;
    while (sent < length) {
        int n = send(fd, buffer + sent, length - sent, 0);
        if (n <= 0) return -1;
        sent += n;
    }
    return sent;
}

int send_line(int fd, const char *message) {
    char buffer[256];
    int length = snprintf(buffer, sizeof(buffer), "%s\n", message);
    if (length < 0 || length >= (int)sizeof(buffer)) return -1;
    log_event("TCP Send: %s", message);
    return send_all(fd, buffer, length);
}

int recv_line(int fd, char *buffer, int size) {
    int position = 0;
    while (position < size - 1) {
        char c;
        int n = recv(fd, &c, 1, 0);
        if (n == 0) return 0;
        if (n < 0) return -1;
        if (c == '\n') break;
        buffer[position++] = c;
    }
    buffer[position] = '\0';
    log_event("TCP Recv: %s", buffer);
    return position;
}

void print_feedback(const char *feedback) {
    for (int i = 0; i < CODE_LENGTH; i++) {
        if (feedback[i] == 'x') printf(GREEN "x" RESET);
        else if (feedback[i] == 'o') printf(YELLOW "o" RESET);
        else printf(RED "-" RESET);
    }
}

void print_board_codebreaker(const char guesses[][CODE_LENGTH + 1], const char feedbacks[][CODE_LENGTH + 1], int attempts) {
    printf("\033[H\033[J");
    printf("=====================================\n             MASTERMIND\n=====================================\nSecret: *****\n-------------------------------------\nAttempt     Guess       Feedback\n-------------------------------------\n");
    for (int i = 0; i < attempts; i++) {
        printf("%-11d %-11s ", i + 1, guesses[i]);
        print_feedback(feedbacks[i]);
        printf("\n");
    }
    printf("=====================================\n");
}

void print_board_mastermind(const char guesses[][CODE_LENGTH + 1], const char feedbacks[][CODE_LENGTH + 1], int attempts, const char *sequence) {
    printf("\033[H\033[J");
    printf("=====================================\n             MASTERMIND\n=====================================\nSecret: %s\n-------------------------------------\nAttempt     Guess       Feedback\n-------------------------------------\n", sequence);
    for (int i = 0; i < attempts; i++) {
        printf("%-11d %-11s ", i + 1, guesses[i]);
        print_feedback(feedbacks[i]);
        printf("\n");
    }
    printf("=====================================\n");
}

int create_udp_game_socket(const char *port) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) { perror("socket"); return -1; }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(atoi(port));
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    return fd;
}

typedef struct {
    uint32_t seq;
    struct timeval last_sent;
    int acked;
    int retries;
    char buffer[UDP_CHUNK_SIZE + 16];
    int length;
} PendingChunk;

int udp_send_reliable(int fd, struct sockaddr_in *dest, const char *data) {
    int data_len = strlen(data);
    uint32_t total_chunks = (data_len + UDP_CHUNK_SIZE - 1) / UDP_CHUNK_SIZE;
    if (total_chunks == 0) total_chunks = 1;
    if (total_chunks > MAX_TOTAL_CHUNKS) return -1;

    PendingChunk *chunks = calloc(total_chunks, sizeof(PendingChunk));
    if (!chunks) return -1;

    for (uint32_t i = 0; i < total_chunks; i++) {
        chunks[i].seq = i;
        chunks[i].acked = 0;
        chunks[i].retries = 0;
        uint32_t offset = i * UDP_CHUNK_SIZE;
        uint32_t chunk_data_len = (data_len > (i + 1) * UDP_CHUNK_SIZE) ? UDP_CHUNK_SIZE : (data_len - offset);
        if (data_len == 0) chunk_data_len = 0;

        uint32_t net_seq = htonl(chunks[i].seq);
        uint32_t net_total = htonl(total_chunks);
        uint32_t net_len = htonl(chunk_data_len);

        chunks[i].buffer[0] = PKT_TYPE_DATA;
        memset(chunks[i].buffer + 1, 0, 3); // Padding
        memcpy(chunks[i].buffer + 4, &net_seq, 4);
        memcpy(chunks[i].buffer + 8, &net_total, 4);
        memcpy(chunks[i].buffer + 12, &net_len, 4);
        if (chunk_data_len > 0) memcpy(chunks[i].buffer + 16, data + offset, chunk_data_len);
        chunks[i].length = 16 + chunk_data_len;

        if (sendto(fd, chunks[i].buffer, chunks[i].length, 0, (struct sockaddr *)dest, sizeof(*dest)) < 0) {
            free(chunks);
            return -1;
        }
        log_event("Sent UDP chunk %u of %u", i, total_chunks);
        gettimeofday(&chunks[i].last_sent, NULL);
    }

    int acked_count = 0;
    while (acked_count < total_chunks) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        struct timeval timeout = {0, 10000};
        if (select(fd + 1, &readfds, NULL, NULL, &timeout) > 0 && FD_ISSET(fd, &readfds)) {
            char ack_buf[8];
            struct sockaddr_in from;
            socklen_t from_len = sizeof(from);
            if (recvfrom(fd, ack_buf, sizeof(ack_buf), MSG_DONTWAIT, (struct sockaddr *)&from, &from_len) == 8) {
                if (memcmp(&from, dest, sizeof(struct sockaddr_in)) == 0 && ack_buf[0] == PKT_TYPE_ACK) {
                    uint32_t seq;
                    memcpy(&seq, ack_buf + 4, 4);
                    seq = ntohl(seq);
                    if (seq < total_chunks && !chunks[seq].acked) {
                        chunks[seq].acked = 1;
                        acked_count++;
                        log_event("Received ACK for chunk %u", seq);
                    }
                }
            }
        }
        struct timeval now;
        gettimeofday(&now, NULL);
        for (uint32_t i = 0; i < total_chunks; i++) {
            if (!chunks[i].acked) {
                double elapsed = (now.tv_sec - chunks[i].last_sent.tv_sec) + (now.tv_usec - chunks[i].last_sent.tv_usec) / 1000000.0;
                if (elapsed >= SEND_RETRY_TIMEOUT_SEC) {
                    if (chunks[i].retries >= MAX_RETRIES) {
                        free(chunks);
                        return -1;
                    }
                    if (sendto(fd, chunks[i].buffer, chunks[i].length, 0, (struct sockaddr *)dest, sizeof(*dest)) < 0) {
                        free(chunks);
                        return -1;
                    }
                    log_event("Retrying UDP chunk %u (attempt %d)", i, chunks[i].retries + 1);
                    chunks[i].retries++;
                    chunks[i].last_sent = now;
                }
            }
        }
    }
    free(chunks);
    return 0;
}

int udp_recv_reliable(int fd, struct sockaddr_in *src, char *buffer, int size) {
    uint32_t total_chunks = 0;
    uint32_t chunks_received = 0;
    char *assembled = NULL;
    int *received_mask = NULL;
    uint32_t last_chunk_len = 0;

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        struct timeval timeout = {RECV_TIMEOUT_SEC, 0};
        if (select(fd + 1, &readfds, NULL, NULL, &timeout) <= 0) {
            if (assembled) free(assembled);
            if (received_mask) free(received_mask);
            return -1;
        }

        char packet[UDP_CHUNK_SIZE + 16];
        struct sockaddr_in from;
        socklen_t from_len = sizeof(from);
        int n = recvfrom(fd, packet, sizeof(packet), 0, (struct sockaddr *)&from, &from_len);
        if (n < 1) continue;

        if (packet[0] != PKT_TYPE_DATA) continue;
        if (n < 16) continue;

        if (memcmp(&from, src, sizeof(struct sockaddr_in)) != 0) continue;

        uint32_t net_seq, net_total, net_len;
        memcpy(&net_seq, packet + 4, 4);
        memcpy(&net_total, packet + 8, 4);
        memcpy(&net_len, packet + 12, 4);
        uint32_t seq = ntohl(net_seq);
        uint32_t total = ntohl(net_total);
        uint32_t len = ntohl(net_len);

        if (len > UDP_CHUNK_SIZE || n != 16 + len || total == 0 || total > MAX_TOTAL_CHUNKS) continue;

        log_event("Received UDP chunk %u of %u", seq, total);

        uint32_t ack_seq = htonl(seq);
        uint8_t ack_pkt[8] = {0};
        ack_pkt[0] = PKT_TYPE_ACK;
        memcpy(ack_pkt + 4, &ack_seq, 4);
        sendto(fd, ack_pkt, 8, 0, (struct sockaddr *)&from, from_len);
        log_event("Sent ACK for chunk %u", seq);

        if (total_chunks == 0) {
            total_chunks = total;
            assembled = calloc(total_chunks * UDP_CHUNK_SIZE + 1, 1);
            received_mask = calloc(total_chunks, sizeof(int));
            if (!assembled || !received_mask) {
                if (assembled) free(assembled);
                if (received_mask) free(received_mask);
                return -1;
            }
        } else if (total != total_chunks) continue;

        if (seq < total_chunks && !received_mask[seq]) {
            int offset = seq * UDP_CHUNK_SIZE;
            memcpy(assembled + offset, packet + 16, len);
            received_mask[seq] = 1;
            chunks_received++;
            if (seq == total_chunks - 1) {
                last_chunk_len = len;
            }
        }
        if (chunks_received == total_chunks) break;
    }

    uint32_t total_payload_len = (total_chunks - 1) * UDP_CHUNK_SIZE + last_chunk_len;
    int copied = 0;
    if (total_payload_len > 0) {
        int limit = (total_payload_len < (uint32_t)size - 1) ? (int)total_payload_len : size - 1;
        memcpy(buffer, assembled, limit);
        copied = limit;
    }
    buffer[copied] = '\0';

    free(assembled);
    free(received_mask);
    return copied;
}
