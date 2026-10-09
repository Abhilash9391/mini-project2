#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "utils.h"

#include "discovery.h"

int main(int argc, char *argv[])
{

    int cost_cut_mode = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--cost-cutting") == 0)
        {
            cost_cut_mode = 1;
        }
        else if (strcmp(argv[i], "--log") == 0)
        {
            logging_enabled = 1;
        }
        else
        {
            printf("Usage: %s [--cost-cutting] [--log]\n", argv[0]);
            return 1;
        }
    }

    struct sockaddr_in opponent_addr;
    memset(&opponent_addr, 0, sizeof(opponent_addr));

    int tcp_fd =
        create_tcp_listener();

    if (tcp_fd < 0)
    {
        return 1;
    }

    int udp_fd =
        create_udp_socket();

    if (udp_fd < 0)
    {
        close(tcp_fd);
        return 1;
    }

    char name[50];

    get_player_name(
        name,
        sizeof(name));

    char message[100];

    create_discovery_message(
        message,
        sizeof(message),
        name);

    while (1)
    {
        struct Player *head = NULL;
        struct Player *tail = NULL;

        int codebreaker = 0;
        struct sockaddr_in game_opponent_addr;
        memset(&game_opponent_addr, 0, sizeof(game_opponent_addr));

        int game_fd = discovery_loop(
            udp_fd,
            tcp_fd,
            message,
            &head,
            &tail,
            &codebreaker,
            cost_cut_mode,
            &game_opponent_addr);

        if (game_fd >= 0)
        {
            game_loop(
                game_fd,
                codebreaker,
                &game_opponent_addr,
                cost_cut_mode);

            close(game_fd);
        }

        free_players(head);
    }

    close(udp_fd);
    close(tcp_fd);

    return 0;
}