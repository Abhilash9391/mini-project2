#include <stdio.h>
#include <unistd.h>

#include "discovery.h"


int main(void)
{
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

        int game_fd = discovery_loop(
            udp_fd,
            tcp_fd,
            message,
            &head,
            &tail,
            &codebreaker);

        if (game_fd >= 0)
        {
            game_loop(
                game_fd,
                codebreaker);

            close(game_fd);
        }

        free_players(head);
    }

    close(udp_fd);
    close(tcp_fd);

    return 0;
}