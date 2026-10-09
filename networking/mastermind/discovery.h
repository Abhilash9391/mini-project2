#ifndef DISCOVERY_H
#define DISCOVERY_H

#include <stddef.h>
#include <time.h>
#include <netinet/in.h>

struct Player
{
    int id;
    char name[50];
    char ip[INET_ADDRSTRLEN];
    int port;
    time_t last_seen;

    struct Player *prev;
    struct Player *next;
};

int create_tcp_listener(void);

int create_udp_socket(void);

void get_player_name(char *name, size_t size);

void create_discovery_message(
    char *message,
    size_t size,
    const char *name);

void broadcast_discovery(
    int udp_fd,
    const char *message);

void receive_discovery(
    int udp_fd,
    struct Player **head,
    struct Player **tail,
    const char *my_name);

int remove_expired_players(
    struct Player **head,
    struct Player **tail);

struct Player *find_player_by_id(
    struct Player *head,
    int id);

int connect_to_player(
    struct Player *player);

int handle_incoming_connection(
    int conn_fd,
    int cost_cut_mode,
    struct sockaddr_in *client_addr);

int discovery_loop(
    int udp_fd,
    int tcp_listen_fd,
    const char *message,
    struct Player **head,
    struct Player **tail,
    int *codebreaker,
    int cost_cut_mode,
    struct sockaddr_in *game_opponent_addr);

void game_loop(
    int game_fd,
    int codebreaker,
    struct sockaddr_in *opponent_addr,
    int cost_cut_mode);

void free_players(
    struct Player *head);

#endif