#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/types.h>

#ifndef SO_REUSEPORT
#define SO_REUSEPORT 0x0F
#endif

#include "utils.h"
#include "discovery.h"
#include "udp_codebreaker.h"
#include "udp_mastermind.h"
#include "mastermind.h"
#include "codebreaker.h"

#define TCP_PORT "8080"
#define DISCOVERY_PORT "5000"
#define MAGIC "ABCD"

#define BROADCAST_INTERVAL 2
#define PLAYER_TIMEOUT 5

/* ---------------- TCP LISTENER ---------------- */

int create_tcp_listener(const char *port)
{
    struct addrinfo hints;
    struct addrinfo *result;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status = getaddrinfo(
        NULL,
        port,
        &hints,
        &result);

    if (status != 0)
    {
        fprintf(
            stderr,
            "getaddrinfo: %s\n",
            gai_strerror(status));

        return -1;
    }

    int sockfd = socket(
        result->ai_family,
        result->ai_socktype,
        result->ai_protocol);

    if (sockfd < 0)
    {
        perror("socket");
        freeaddrinfo(result);
        return -1;
    }

    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt SO_REUSEADDR");
        close(sockfd);
        freeaddrinfo(result);
        return -1;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt SO_REUSEPORT");
        close(sockfd);
        freeaddrinfo(result);
        return -1;
    }

    if (bind(
            sockfd,
            result->ai_addr,
            result->ai_addrlen) < 0)
    {
        perror("bind");
        close(sockfd);
        freeaddrinfo(result);
        return -1;
    }

    freeaddrinfo(result);

    if (listen(sockfd, 5) < 0)
    {
        perror("listen");
        close(sockfd);
        return -1;
    }

    printf(
        "TCP listening on port %s\n",
        port);

    return sockfd;
}

/* ---------------- UDP SOCKET ---------------- */

int create_udp_socket(void)

{
    struct addrinfo hints;
    struct addrinfo *result;

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    int status = getaddrinfo(
        NULL,
        DISCOVERY_PORT,
        &hints,
        &result);

    if (status != 0)
    {
        fprintf(
            stderr,
            "getaddrinfo: %s\n",
            gai_strerror(status));

        return -1;
    }

    int sockfd = socket(
        result->ai_family,
        result->ai_socktype,
        result->ai_protocol);

    if (sockfd < 0)
    {
        perror("socket");
        freeaddrinfo(result);
        return -1;
    }

    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt SO_REUSEADDR");
        close(sockfd);
        freeaddrinfo(result);
        return -1;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0)
    {
        perror("setsockopt SO_REUSEPORT");
        close(sockfd);
        freeaddrinfo(result);
        return -1;
    }

    if (bind(
            sockfd,
            result->ai_addr,
            result->ai_addrlen) < 0)
    {
        perror("bind");
        close(sockfd);
        freeaddrinfo(result);
        return -1;
    }

    freeaddrinfo(result);

    int broadcast = 1;

    if (setsockopt(
            sockfd,
            SOL_SOCKET,
            SO_BROADCAST,
            &broadcast,
            sizeof(broadcast)) < 0)
    {
        perror("setsockopt");
        close(sockfd);
        return -1;
    }

    int flags = fcntl(
        sockfd,
        F_GETFL,
        0);

    if (flags < 0)
    {
        perror("fcntl");
        close(sockfd);
        return -1;
    }

    if (fcntl(
            sockfd,
            F_SETFL,
            flags | O_NONBLOCK) < 0)
    {
        perror("fcntl");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

/* ---------------- PLAYER NAME ---------------- */

void get_player_name(
    char *name,
    size_t size)
{
    printf("Enter your name: ");
    fflush(stdout);

    if (fgets(
            name,
            size,
            stdin) == NULL)
    {
        fprintf(
            stderr,
            "Failed to read name\n");

        exit(1);
    }

    name[strcspn(name, "\n")] = '\0';
}

/* ---------------- DISCOVERY ---------------- */

void create_discovery_message(
    char *message,
    size_t size,
    const char *name,
    const char *port)
{
    snprintf(
        message,
        size,
        "%s|%s|%s",
        MAGIC,
        name,
        port);
}

void broadcast_discovery(
    int udp_fd,
    const char *message)
{
    struct sockaddr_in broadcast_addr;

    memset(
        &broadcast_addr,
        0,
        sizeof(broadcast_addr));

    broadcast_addr.sin_family = AF_INET;

    broadcast_addr.sin_port =
        htons(atoi(DISCOVERY_PORT));

    broadcast_addr.sin_addr.s_addr =
        INADDR_BROADCAST;

    ssize_t sent = sendto(
        udp_fd,
        message,
        strlen(message),
        0,
        (struct sockaddr *)&broadcast_addr,
        sizeof(broadcast_addr));

    if (sent < 0)
    {
        perror("sendto");
    }
    else
    {
        log_event("Discovery broadcast sent: %s", message);
    }
}

/* ---------------- PLAYER TABLE ---------------- */

struct Player *find_player_by_addr(
    struct Player *head,
    struct sockaddr_in *addr)
{
    struct Player *current = head;
    while (current != NULL)
    {
        if (current->port == ntohs(addr->sin_port) &&
            memcmp(current->ip, inet_ntoa(addr->sin_addr), strlen(current->ip)) == 0)
        {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

struct Player *find_player(
    struct Player *head,
    const char *ip,
    int port)
{
    struct Player *current = head;

    while (current != NULL)
    {
        if (strcmp(current->ip, ip) == 0 &&
            current->port == port)
        {
            return current;
        }

        current = current->next;
    }

    return NULL;
}

void add_player(
    struct Player **head,
    struct Player **tail,
    const char *name,
    const char *ip,
    int port)
{
    struct Player *new_player =
        malloc(sizeof(struct Player));

    if (new_player == NULL)
    {
        perror("malloc");
        return;
    }

    new_player->id = 1;

    if (*tail != NULL)
    {
        new_player->id =
            (*tail)->id + 1;
    }

    strncpy(
        new_player->name,
        name,
        sizeof(new_player->name) - 1);

    new_player->name[sizeof(new_player->name) - 1] = '\0';

    strncpy(
        new_player->ip,
        ip,
        sizeof(new_player->ip) - 1);

    new_player->ip[sizeof(new_player->ip) - 1] = '\0';

    new_player->port = port;
    new_player->last_seen = time(NULL);

    new_player->prev = *tail;
    new_player->next = NULL;

    if (*head == NULL)
    {
        *head = new_player;
        *tail = new_player;
    }
    else
    {
        (*tail)->next = new_player;
        *tail = new_player;
    }

    printf(
        "New player discovered: %s (%s:%d)\n",
        new_player->name,
        new_player->ip,
        new_player->port);
}

void print_players(
    struct Player *head)
{
    printf("\n--- Players Online ---\n");

    printf("Players Online:\n");

    printf(
        "%-8s %-15s %-15s %-8s %s\n",
        "ID",
        "Name",
        "IP Addr",
        "Port",
        "Last Seen");

    struct Player *current = head;

    time_t now = time(NULL);

    while (current != NULL)
    {
        printf(
            "%-8d %-15s %-15s %-8d %ld s. ago\n",
            current->id,
            current->name,
            current->ip,
            current->port,
            now - current->last_seen);

        current = current->next;
    }

    printf(
        "---------------------------------------------------------------\n");

    printf("> ");
    fflush(stdout);
}

void receive_discovery(
    int udp_fd,
    struct Player **head,
    struct Player **tail,
    const char *my_name)
{
    char buffer[1024];

    struct sockaddr_in sender_addr;

    socklen_t sender_len =
        sizeof(sender_addr);

    ssize_t n = recvfrom(
        udp_fd,
        buffer,
        sizeof(buffer) - 1,
        0,
        (struct sockaddr *)&sender_addr,
        &sender_len);

    if (n < 0)
    {
        return;
    }

    buffer[n] = '\0';

    char ip[INET_ADDRSTRLEN];

    if (inet_ntop(
            AF_INET,
            &sender_addr.sin_addr,
            ip,
            sizeof(ip)) == NULL)
    {
        perror("inet_ntop");
        return;
    }

    log_event("Discovery packet received from %s", ip);

    char *magic =
        strtok(buffer, "|");

    char *player_name =
        strtok(NULL, "|");

    char *port =
        strtok(NULL, "|");

    if (magic == NULL ||
        player_name == NULL ||
        port == NULL)
    {
        fprintf(
            stderr,
            "Malformed discovery packet\n");

        return;
    }

    if (strcmp(magic, MAGIC) != 0)
    {
        return;
    }

    if (strcmp(player_name, my_name) == 0)
    {
        return;
    }

    int port_num = atoi(port);

    struct Player *found =
        find_player(
            *head,
            ip,
            port_num);

    if (found != NULL)
    {
        found->last_seen = time(NULL);

        strncpy(
            found->name,
            player_name,
            sizeof(found->name) - 1);

        found->name[sizeof(found->name) - 1] = '\0';
    }
    else
    {
        add_player(
            head,
            tail,
            player_name,
            ip,
            port_num);
    }
    // Removed print_players(*head) to prevent console spam on every UDP packet
}

int remove_expired_players(
    struct Player **head,
    struct Player **tail)
{
    time_t now = time(NULL);

    int removed = 0;

    struct Player *current = *head;

    while (current != NULL)
    {
        struct Player *next =
            current->next;

        if (now - current->last_seen >
            PLAYER_TIMEOUT)
        {
            printf(
                "Player expired: %s\n",
                current->name);

            if (current->prev != NULL)
            {
                current->prev->next =
                    current->next;
            }
            else
            {
                *head = current->next;
            }

            if (current->next != NULL)
            {
                current->next->prev =
                    current->prev;
            }
            else
            {
                *tail = current->prev;
            }

            free(current);

            removed = 1;
        }

        current = next;
    }

    return removed;
}

struct Player *find_player_by_id(
    struct Player *head,
    int id)
{
    struct Player *current = head;

    while (current != NULL)
    {
        if (current->id == id)
        {
            return current;
        }

        current = current->next;
    }

    return NULL;
}

/* ---------------- OUTGOING TCP ---------------- */

int connect_to_player(
    struct Player *player)
{
    struct addrinfo hints;
    struct addrinfo *result;

    memset(
        &hints,
        0,
        sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port[10];

    snprintf(
        port,
        sizeof(port),
        "%d",
        player->port);

    int status = getaddrinfo(
        player->ip,
        port,
        &hints,
        &result);

    if (status != 0)
    {
        fprintf(
            stderr,
            "getaddrinfo: %s\n",
            gai_strerror(status));

        return -1;
    }

    int sockfd = socket(
        result->ai_family,
        result->ai_socktype,
        result->ai_protocol);

    if (sockfd < 0)
    {
        perror("socket");
        freeaddrinfo(result);
        return -1;
    }

    if (connect(
            sockfd,
            result->ai_addr,
            result->ai_addrlen) < 0)
    {
        perror("connect");

        close(sockfd);
        freeaddrinfo(result);

        return -1;
    }

    freeaddrinfo(result);

    return sockfd;
}

/* ---------------- INCOMING TCP ---------------- */

int handle_incoming_connection(
    int conn_fd, int cost_cut_mode, struct sockaddr_in *client_addr)
{
    char buffer[100];

    int n = recv(
        conn_fd,
        buffer,
        sizeof(buffer) - 1,
        0);

    if (n <= 0)
    {
        close(conn_fd);
        return -1;
    }

    buffer[n] = '\0';

    if (strcmp(
            buffer,
            "challenge") != 0)
    {
        close(conn_fd);
        return -1;
    }

    printf("\nYou have been challenged! Accept? (yes/no): ");
    fflush(stdout);

    char response[10];
    while (1)
    {
        if (fgets(response, sizeof(response), stdin) == NULL)
        {
            close(conn_fd);
            return -1;
        }
        remove_newline(response);

        if (strcmp(response, "yes") == 0 ||
            strcmp(response, "y") == 0 ||
            strcmp(response, "YES") == 0 ||
            strcmp(response, "Y") == 0)
        {
            if (send(conn_fd, "yes", 3, 0) < 0)
            {
                close(conn_fd);
                return -1;
            }
            return conn_fd;
        }
        else if (strcmp(response, "no") == 0 ||
                 strcmp(response, "n") == 0 ||
                 strcmp(response, "NO") == 0 ||
                 strcmp(response, "N") == 0)
        {
            send(conn_fd, "no", 2, 0);
            printf("Challenge rejected.\n");
            close(conn_fd);
            return -1;
        }
        else
        {
            printf("Invalid input. Please type 'yes' or 'no': ");
            fflush(stdout);
        }
    }
}

/* ---------------- DISCOVERY LOOP ---------------- */

int discovery_loop(
    int udp_fd,
    int tcp_listen_fd,
    const char *message,
    const char *my_name,
    struct Player **head,
    struct Player **tail,
    int *codebreaker,
    int cost_cut_mode,
    struct sockaddr_in *game_opponent_addr,
    char *opponent_name)
{
    time_t last_broadcast = 0;

    while (1)
    {
        fd_set readfds;

        FD_ZERO(&readfds);

        FD_SET(
            udp_fd,
            &readfds);

        FD_SET(
            STDIN_FILENO,
            &readfds);

        FD_SET(
            tcp_listen_fd,
            &readfds);

        struct timeval timeout;

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int max_fd = tcp_listen_fd;

        if (udp_fd > max_fd)
            max_fd = udp_fd;

        if (STDIN_FILENO > max_fd)
            max_fd = STDIN_FILENO;

        int activity = select(
            max_fd + 1,
            &readfds,
            NULL,
            NULL,
            &timeout);

        if (activity < 0)
        {
            perror("select");
            return -1;
        }

        time_t now = time(NULL);

        if (now - last_broadcast >=
            BROADCAST_INTERVAL)
        {
            broadcast_discovery(
                udp_fd,
                message);

            last_broadcast = now;
        }

        if (FD_ISSET(
                udp_fd,
                &readfds))
        {
            receive_discovery(
                udp_fd,
                head,
                tail,
                my_name);
        }

        if (FD_ISSET(
                STDIN_FILENO,
                &readfds))
        {
            char command[100];

            if (fgets(
                    command,
                    sizeof(command),
                    stdin) == NULL)
            {
                continue;
            }

            char *cmd =
                strtok(command, " \n");

            char *arg =
                strtok(NULL, " \n");

            if (cmd != NULL &&
                strcmp(cmd, "challenge") == 0 &&
                arg != NULL)
            {
                int id = atoi(arg);

                struct Player *player =
                    find_player_by_id(
                        *head,
                        id);

                if (player == NULL)
                {
                    printf(
                        "Player not found\n");
                }
                else
                {
                    printf(
                        "Challenging %s at %s:%d\n",
                        player->name,
                        player->ip,
                        player->port);

                    int tcp_fd =
                        connect_to_player(player);

                    if (tcp_fd < 0)
                    {
                        printf(
                            "Could not connect to player\n");
                    }
                    else
                    {
                        send(
                            tcp_fd,
                            "challenge",
                            9,
                            0);

                        char response[10];

                        int n = recv(
                            tcp_fd,
                            response,
                            sizeof(response) - 1,
                            0);

                        if (n <= 0)
                        {
                            close(tcp_fd);

                            printf(
                                "Connection closed by player\n");
                        }
                        else
                        {
                            response[n] = '\0';

                            if (strcmp(response, "yes") == 0)
                            {
                                *codebreaker = 0;

                                printf("Challenge accepted\n");
                                struct sockaddr_in opponent_addr;

                                memset(&opponent_addr, 0, sizeof(opponent_addr));

                                opponent_addr.sin_family = AF_INET;

                                opponent_addr.sin_port =
                                    htons(player->port);

                                inet_pton(
                                    AF_INET,
                                    player->ip,
                                    &opponent_addr.sin_addr);

                                if (cost_cut_mode)
                                {
                                    send(
                                        tcp_fd,
                                        "SWITCH_TO_UDP",
                                        strlen("SWITCH_TO_UDP"),
                                        0);

                                    close(tcp_fd);

                                    int udp_game_fd = create_udp_game_socket(TCP_PORT);
                                    if (udp_game_fd < 0)
                                    {
                                        return -1;
                                    }

                                    return udp_game_fd;
                                }

                                return tcp_fd;
                            }

                            printf(
                                "Challenge rejected\n");

                            close(tcp_fd);
                        }
                    }
                }
            }
        }

        if (FD_ISSET(
                tcp_listen_fd,
                &readfds))
        {
            struct sockaddr_in client_addr;

            socklen_t client_len =
                sizeof(client_addr);

            int conn_fd = accept(
                tcp_listen_fd,
                (struct sockaddr *)&client_addr,
                &client_len);

            if (conn_fd < 0)
            {
                perror("accept");
            }
            else
            {
                // Find the name of the person who connected
                char opponent_name[50] = "Unknown";
                struct Player *p = find_player_by_addr(
                    *head,
                    &client_addr);
                if (p) {
                    strncpy(opponent_name, p->name, sizeof(opponent_name)-1);
                }

                int game_fd =
                    handle_incoming_connection(
                        conn_fd, cost_cut_mode, &client_addr);

                if (game_fd >= 0)
                {
                    *codebreaker = 1;

                    // Use the found name or "Unknown"
                    // We need to pass this name to game_loop
                    // but discovery_loop returns the fd.
                    // We'll have to store the name somewhere or
                    // change how discovery_loop returns.

                    // Let's use a static or pass it back.
                    // Actually, the easiest is to use a global or
                    // change discovery_loop signature.
                    // For now, let's just return game_fd and
                    // handle the name in the loop below.
                    return game_fd;
                }
            }
        }

        if (remove_expired_players(
                head,
                tail))
        {
            print_players(*head);
        }
        // Added a manual trigger or timer could go here, but for now,
        // let's ensure the user can see the list when they start.
    }
}

/* ---------------- GAME DISPATCHER ---------------- */


void game_loop(
    int game_fd,
    int codebreaker,
    struct sockaddr_in *opponent_addr,
    int cost_cut_mode,
    const char *opponent_name)
{
    if (cost_cut_mode)
    {
        if (codebreaker)
        {
            udp_codebreaker_game(
                game_fd,
                opponent_addr,
                opponent_name);
        }
        else
        {
            udp_mastermind_game(
                game_fd,
                opponent_addr,
                opponent_name);
        }
    }
    else
    {
        if (codebreaker)
        {
            codebreaker_game(game_fd, opponent_name);
        }
        else
        {
            mastermind_game(game_fd, opponent_name);
        }
    }
}


/* ---------------- CLEANUP ---------------- */

void free_players(
    struct Player *head)
{
    struct Player *current = head;

    while (current != NULL)
    {
        struct Player *next =
            current->next;

        free(current);

        current = next;
    }
}