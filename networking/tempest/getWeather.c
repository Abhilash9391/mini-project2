#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>

#define PORT "80"

volatile sig_atomic_t timed_out = 0;

void timeout_handler(int sig)
{
    timed_out = 1;
}

void parse_headers(char *header_buffer, int header_len,
                   int *status_code, int *content_length)
{

    *status_code = -1;
    *content_length = -1;

    char *status_line_end = strstr(header_buffer, "\r\n");

    if (status_line_end == NULL)
    {
        return;
    }

    char *space = strchr(header_buffer, ' ');

    if (space == NULL)
    {
        return;
    }

    *status_code = atoi(space + 1);

    char *content_length_ptr =
        strstr(header_buffer, "Content-Length:");

    if (content_length_ptr != NULL)
    {

        content_length_ptr += strlen("Content-Length:");

        while (*content_length_ptr == ' ')
        {
            content_length_ptr++;
        }

        *content_length = atoi(content_length_ptr);
    }
}

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        fprintf(stderr, "tempest: missing argument\n");
        return 1;
    }

    if (argc > 3)
    {
        fprintf(stderr, "tempest: too many arguments\n");
        return 1;
    }

    if (argc == 3 && strcmp(argv[2], "--raw") != 0)
    {
        fprintf(stderr, "tempest: too many arguments\n");
        return 1;
    }

    int raw = (argc == 3);

    signal(SIGALRM, timeout_handler);
    alarm(10);

    char *hostName = "wttr.is";
    char path[2048];
    char encoded_city[1024];
    int encoded_len = 0;

    for (int i = 0; argv[1][i] != '\0'; i++) {
        unsigned char c = argv[1][i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            encoded_city[encoded_len++] = c;
        } else {
            if (encoded_len + 3 >= 1024) break;
            sprintf(&encoded_city[encoded_len], "%%%02X", c);
            encoded_len += 3;
        }
    }
    encoded_city[encoded_len] = '\0';

    snprintf(
        path,
        sizeof(path),
        "/%s?0T",
        encoded_city);

    struct addrinfo hints;
    struct addrinfo *result;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(
        hostName,
        PORT,
        &hints,
        &result);

    if (status != 0)
    {
        fprintf(stderr, "tempest: invalid location\n");
        return 1;
    }

    int sockfd = socket(
        result->ai_family,
        result->ai_socktype,
        result->ai_protocol);

    if (sockfd == -1)
    {
        perror("socket");
        freeaddrinfo(result);
        return 1;
    }

    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl get");
        close(sockfd);
        freeaddrinfo(result);
        return 1;
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl set");
        close(sockfd);
        freeaddrinfo(result);
        return 1;
    }

    struct timespec start_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    status = connect(
        sockfd,
        result->ai_addr,
        result->ai_addrlen);

    if (status == -1 && errno != EINPROGRESS)
    {
        if (errno == EINTR && timed_out)
        {
            fprintf(stderr, "tempest: request timed out\n");
        }
        else
        {
            perror("connect");
        }

        close(sockfd);
        freeaddrinfo(result);
        return 1;
    }

    if (status == -1 && errno == EINPROGRESS)
    {
        struct pollfd pfd;
        pfd.fd = sockfd;
        pfd.events = POLLOUT;

        while (1) {
            clock_gettime(CLOCK_MONOTONIC, &current_time);
            long remaining_ms = 10000 - (current_time.tv_sec - start_time.tv_sec) * 1000
                                - (current_time.tv_nsec - start_time.tv_nsec) / 1000000;

            if (remaining_ms <= 0) {
                fprintf(stderr, "tempest: request timed out\n");
                close(sockfd);
                freeaddrinfo(result);
                return 1;
            }

            int poll_res = poll(&pfd, 1, remaining_ms);
            if (poll_res == 0) {
                fprintf(stderr, "tempest: request timed out\n");
                close(sockfd);
                freeaddrinfo(result);
                return 1;
            } else if (poll_res == -1) {
                if (errno == EINTR) continue;
                perror("poll");
                close(sockfd);
                freeaddrinfo(result);
                return 1;
            }

            int so_error;
            socklen_t len = sizeof(so_error);
            if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0) {
                perror("getsockopt");
                close(sockfd);
                freeaddrinfo(result);
                return 1;
            }
            if (so_error != 0) {
                fprintf(stderr, "connect failed: %s\n", strerror(so_error));
                close(sockfd);
                freeaddrinfo(result);
                return 1;
            }
            break;
        }
    }

    char buffer[1024];

    int req_len = snprintf(
        buffer,
        sizeof(buffer),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        path,
        hostName);

    int total_sent = 0;

    while (total_sent < req_len)
    {
        struct pollfd pfd;
        pfd.fd = sockfd;
        pfd.events = POLLOUT;

        clock_gettime(CLOCK_MONOTONIC, &current_time);
        long remaining_ms = 10000 - (current_time.tv_sec - start_time.tv_sec) * 1000
                            - (current_time.tv_nsec - start_time.tv_nsec) / 1000000;

        if (remaining_ms <= 0) {
            fprintf(stderr, "tempest: request timed out\n");
            close(sockfd);
            freeaddrinfo(result);
            return 1;
        }

        int poll_res = poll(&pfd, 1, remaining_ms);
        if (poll_res == 0) {
            fprintf(stderr, "tempest: request timed out\n");
            close(sockfd);
            freeaddrinfo(result);
            return 1;
        } else if (poll_res == -1) {
            if (errno == EINTR) continue;
            perror("poll");
            close(sockfd);
            freeaddrinfo(result);
            return 1;
        }

        int len = send(
            sockfd,
            buffer + total_sent,
            req_len - total_sent,
            0);

        if (len == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            if (errno == EINTR && timed_out)
            {
                fprintf(stderr, "tempest: request timed out\n");
            }
            else
            {
                perror("send");
            }
            close(sockfd);
            freeaddrinfo(result);
            return 1;
        }

        if (len == 0)
        {
            fprintf(stderr, "send: connection closed\n");
            close(sockfd);
            freeaddrinfo(result);
            return 1;
        }

        total_sent += len;
    }

    char temp[4096];

    int header_capacity = 4097;
    int header_len = 0;

    char *header_buffer = malloc(header_capacity);

    if (header_buffer == NULL)
    {
        perror("malloc");
        close(sockfd);
        freeaddrinfo(result);
        return 1;
    }

    int body_capacity = 4096;
    int body_size = 0;

    char *body_buffer = malloc(body_capacity);

    if (body_buffer == NULL)
    {
        perror("malloc");
        free(header_buffer);
        close(sockfd);
        freeaddrinfo(result);
        return 1;
    }

    int headers_done = 0;

    while (1)
    {
        struct pollfd pfd;
        pfd.fd = sockfd;
        pfd.events = POLLIN;

        clock_gettime(CLOCK_MONOTONIC, &current_time);
        long remaining_ms = 10000 - (current_time.tv_sec - start_time.tv_sec) * 1000
                            - (current_time.tv_nsec - start_time.tv_nsec) / 1000000;

        if (remaining_ms <= 0) {
            fprintf(stderr, "tempest: request timed out\n");
            free(header_buffer);
            free(body_buffer);
            close(sockfd);
            freeaddrinfo(result);
            return 1;
        }

        int poll_res = poll(&pfd, 1, remaining_ms);
        if (poll_res == 0) {
            fprintf(stderr, "tempest: request timed out\n");
            free(header_buffer);
            free(body_buffer);
            close(sockfd);
            freeaddrinfo(result);
            return 1;
        } else if (poll_res == -1) {
            if (errno == EINTR) continue;
            perror("poll");
            free(header_buffer);
            free(body_buffer);
            close(sockfd);
            freeaddrinfo(result);
            return 1;
        }

        int n = recv(
            sockfd,
            temp,
            sizeof(temp),
            0);

        if (n > 0)
        {
            if (!headers_done)
            {
                while (header_len + n + 1 > header_capacity)
                {
                    header_capacity *= 2;
                    char *new_buffer = realloc(header_buffer, header_capacity);
                    if (new_buffer == NULL) {
                        perror("realloc");
                        free(header_buffer);
                        free(body_buffer);
                        close(sockfd);
                        freeaddrinfo(result);
                        return 1;
                    }
                    header_buffer = new_buffer;
                }

                memcpy(header_buffer + header_len, temp, n);
                header_len += n;

                for (int i = 0; i <= header_len - 4; i++)
                {
                    if (header_buffer[i] == '\r' && header_buffer[i + 1] == '\n' &&
                        header_buffer[i + 2] == '\r' && header_buffer[i + 3] == '\n')
                    {
                        int body_start = i + 4;
                        int initial_body_size = header_len - body_start;
                        if (initial_body_size > 0)
                        {
                            while (body_size + initial_body_size > body_capacity)
                            {
                                body_capacity *= 2;
                                char *new_buffer = realloc(body_buffer, body_capacity);
                                if (new_buffer == NULL) {
                                    perror("realloc");
                                    free(header_buffer);
                                    free(body_buffer);
                                    close(sockfd);
                                    freeaddrinfo(result);
                                    return 1;
                                }
                                body_buffer = new_buffer;
                            }
                            memcpy(body_buffer + body_size, header_buffer + body_start, initial_body_size);
                            body_size += initial_body_size;
                        }
                        header_len = body_start;
                        header_buffer[header_len] = '\0';
                        headers_done = 1;
                        break;
                    }
                }
            }
            else
            {
                while (body_size + n > body_capacity)
                {
                    body_capacity *= 2;
                    char *new_buffer = realloc(body_buffer, body_capacity);
                    if (new_buffer == NULL) {
                        perror("realloc");
                        free(header_buffer);
                        free(body_buffer);
                        close(sockfd);
                        freeaddrinfo(result);
                        return 1;
                    }
                    body_buffer = new_buffer;
                }
                memcpy(body_buffer + body_size, temp, n);
                body_size += n;
            }
        }
        else if (n == 0)
        {
            break;
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            if (errno == EINTR && timed_out)
            {
                fprintf(stderr, "tempest: request timed out\n");
            }
            else
            {
                perror("recv");
            }
            free(header_buffer);
            free(body_buffer);
            close(sockfd);
            freeaddrinfo(result);
            return 1;
        }
    }

    alarm(0);
    body_buffer[body_size] = '\0';

    int status_code;
    int content_length;

    parse_headers(
        header_buffer,
        header_len,
        &status_code,
        &content_length);

    if (status_code != 200)
    {
        fprintf(stderr, "tempest: invalid location\n");

        free(header_buffer);
        free(body_buffer);
        close(sockfd);
        freeaddrinfo(result);

        return 1;
    }
    if (raw == 1)
    {
        fwrite(header_buffer, 1, header_len, stdout);
        fwrite(body_buffer, 1, body_size, stdout);
    }
    else
    {
        fwrite(body_buffer, 1, body_size, stdout);
    }

    free(header_buffer);
    free(body_buffer);

    close(sockfd);
    freeaddrinfo(result);

    return 0;
}