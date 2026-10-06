#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#define TCP_PORT "8080"
#define DISCOVERY_PORT "5000"
#define MAGIC "ABCD"

int main(void)
{
    // create TCP listening socket
    struct addrinfo hints;
    struct addrinfo *result;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status = getaddrinfo(NULL, TCP_PORT, &hints, &result);

    if (status != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    int sockfd = socket(
        result->ai_family,
        result->ai_socktype,
        result->ai_protocol);

    if (sockfd < 0)
    {
        perror("socket");
        freeaddrinfo(result);
        return 1;
    }

    if (bind(sockfd, result->ai_addr, result->ai_addrlen) < 0)
    {
        perror("bind");
        close(sockfd);
        freeaddrinfo(result);
        return 1;
    }

    freeaddrinfo(result);

    // Start listening for incoming TCP connections
    if (listen(sockfd, 5) < 0)
    {
        perror("listen");
        close(sockfd);
        return 1;
    }

    // Create a UDP socket for broadcasting
    struct addrinfo hintsUdp;
    struct addrinfo *resultsUdp;

    memset(&hintsUdp, 0, sizeof(hintsUdp));

    hintsUdp.ai_family = AF_INET;
    hintsUdp.ai_socktype = SOCK_DGRAM;
    hintsUdp.ai_flags = AI_PASSIVE;

    int status = getaddrinfo(
        NULL,
        DISCOVERY_PORT,
        &hintsUdp,
        &resultsUdp);

    if (status != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    int sockfd_UDP = socket(
        resultsUdp->ai_family,
        resultsUdp->ai_socktype,
        resultsUdp->ai_protocol);

    if (sockfd_UDP < 0)
    {
        perror("socket");
        freeaddrinfo(resultsUdp);
        return 1;
    }
    if (bind(sockfd_UDP, resultsUdp->ai_addr, resultsUdp->ai_addrlen) < 0)
    {
        perror("bind");
        close(sockfd_UDP);
        freeaddrinfo(resultsUdp);
        return 1;
    }

    freeaddrinfo(resultsUdp);

    int broadcast = 1;

    if (setsockopt(
            sockfd_UDP,
            SOL_SOCKET,
            SO_BROADCAST,
            &broadcast,
            sizeof(broadcast)) < 0)
    {
        perror("setsockopt");
        close(sockfd_UDP);
        return 1;
    }

    char name[50];

    printf("Enter your name: ");
fgets(name, sizeof(name), stdin);
name[strcspn(name, "\n")] = '\0';

char message[100];

snprintf(
    message,
    sizeof(message),
    "%s|%s|%s",
    MAGIC,
    name,
    TCP_PORT
);

struct sockaddr_in broadcast_addr;

memset(&broadcast_addr, 0, sizeof(broadcast_addr));

broadcast_addr.sin_family = AF_INET;
broadcast_addr.sin_port = htons(atoi(DISCOVERY_PORT));

broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;

ssize_t sent = sendto(
    sockfd_UDP,
    message,
    strlen(message),
    0,
    (struct sockaddr *)&broadcast_addr,
    sizeof(broadcast_addr)
);

if (sent < 0)
{
    perror("sendto");
}

char buffer[1024];

struct sockaddr_in sender_addr;
socklen_t sender_len = sizeof(sender_addr);

ssize_t n = recvfrom(
    sockfd_UDP,
    buffer,
    sizeof(buffer) - 1,
    0,
    (struct sockaddr *)&sender_addr,
    &sender_len
);

if (n < 0)
{
    perror("recvfrom");
}
else
{
    buffer[n] = '\0';

    printf("Received: %s\n", buffer);
}

char ip[INET_ADDRSTRLEN];

inet_ntop(
    AF_INET,
    &sender_addr.sin_addr,
    ip,
    sizeof(ip)
);

    return 0;
}