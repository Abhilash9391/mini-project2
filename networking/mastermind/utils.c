#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "utils.h"

#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define RED "\033[31m"
#define RESET "\033[0m"

void remove_newline(char *str)
{
    str[strcspn(str, "\n")] = '\0';
}

int valid_code(const char *code)
{
    if (strlen(code) != CODE_LENGTH)
        return 0;

    for (int i = 0; i < CODE_LENGTH; i++)
    {
        if (code[i] < '0' ||
            code[i] > '9')
        {
            return 0;
        }
    }

    return 1;
}

int send_all(
    int fd,
    const char *buffer,
    int length)
{
    int sent = 0;

    while (sent < length)
    {
        int n = send(
            fd,
            buffer + sent,
            length - sent,
            0);

        if (n <= 0)
            return -1;

        sent += n;
    }

    return sent;
}

int send_line(
    int fd,
    const char *message)
{
    char buffer[256];

    int length = snprintf(
        buffer,
        sizeof(buffer),
        "%s\n",
        message);

    if (length < 0 ||
        length >= (int)sizeof(buffer))
    {
        return -1;
    }

    return send_all(
        fd,
        buffer,
        length);
}

int recv_line(
    int fd,
    char *buffer,
    int size)
{
    int position = 0;

    while (position < size - 1)
    {
        char c;

        int n = recv(
            fd,
            &c,
            1,
            0);

        if (n == 0)
            return 0;

        if (n < 0)
            return -1;

        if (c == '\n')
            break;

        buffer[position++] = c;
    }

    buffer[position] = '\0';

    return position;
}

void print_feedback(
    const char *feedback)
{
    for (int i = 0; i < CODE_LENGTH; i++)
    {
        if (feedback[i] == 'x')
        {
            printf(
                GREEN "x" RESET);
        }
        else if (feedback[i] == 'o')
        {
            printf(
                YELLOW "o" RESET);
        }
        else
        {
            printf(
                RED "-" RESET);
        }
    }
}

void print_board_codebreaker(
    const char guesses[][CODE_LENGTH + 1],
    const char feedbacks[][CODE_LENGTH + 1],
    int attempts)
{
    printf("\033[H\033[J");

    printf(
        "=====================================\n");

    printf(
        "             MASTERMIND\n");

    printf(
        "=====================================\n");

    printf(
        "Secret: *****\n");

    printf(
        "-------------------------------------\n");

    printf(
        "Attempt     Guess       Feedback\n");

    printf(
        "-------------------------------------\n");

    for (int i = 0; i < attempts; i++)
    {
        printf(
            "%-11d %-11s ",
            i + 1,
            guesses[i]);

        print_feedback(
            feedbacks[i]);

        printf("\n");
    }

    printf(
        "=====================================\n");
}

void print_board_mastermind(
    const char guesses[][CODE_LENGTH + 1],
    const char feedbacks[][CODE_LENGTH + 1],
    int attempts,
    const char *sequence)
{
    printf("\033[H\033[J");

    printf(
        "=====================================\n");

    printf(
        "             MASTERMIND\n");

    printf(
        "=====================================\n");

    printf(
        "Secret: %s\n",
        sequence);

    printf(
        "-------------------------------------\n");

    printf(
        "Attempt     Guess       Feedback\n");

    printf(
        "-------------------------------------\n");

    for (int i = 0; i < attempts; i++)
    {
        printf(
            "%-11d %-11s ",
            i + 1,
            guesses[i]);

        print_feedback(
            feedbacks[i]);

        printf("\n");
    }

    printf(
        "=====================================\n");
}
