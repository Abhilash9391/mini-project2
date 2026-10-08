#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "utils.h"

#define MAX_ATTEMPTS 12

int valid_feedback(const char *feedback)
{
    if (strlen(feedback) != CODE_LENGTH)
        return 0;

    for (int i = 0; i < CODE_LENGTH; i++)
    {
        if (feedback[i] != 'x' &&
            feedback[i] != 'o' &&
            feedback[i] != '-')
        {
            return 0;
        }
    }

    return 1;
}

void codebreaker_game(
    int game_fd)
{
    char guesses[
        MAX_ATTEMPTS][CODE_LENGTH + 1];

    char feedbacks[
        MAX_ATTEMPTS][CODE_LENGTH + 1];

    int attempts = 0;

    while (attempts < MAX_ATTEMPTS)
    {
        char guess[100];
        char feedback[100];
        char secret_message[100];

        printf(
            "\nEnter guess: ");

        fflush(stdout);

        if (fgets(
                guess,
                sizeof(guess),
                stdin) == NULL)
        {
            return;
        }

        remove_newline(guess);

        if (!valid_code(guess))
        {
            printf(
                "Invalid guess. "
                "Enter exactly 5 digits.\n");

            continue;
        }

        strcpy(
            guesses[attempts],
            guess);

        if (send_line(
                game_fd,
                guess) < 0)
        {
            printf(
                "\nPlayer disconnected. "
                "Press Enter to go home.");

            fflush(stdout);
            getchar();

            return;
        }

        int n = recv_line(
            game_fd,
            feedback,
            sizeof(feedback));

        if (n <= 0)
        {
            printf(
                "\nPlayer disconnected. "
                "Press Enter to go home.");

            fflush(stdout);
            getchar();

            return;
        }

        if (!valid_feedback(feedback))
        {
            printf(
                "Invalid feedback received.\n");

            return;
        }

        strcpy(
            feedbacks[attempts],
            feedback);

        attempts++;

        print_board_codebreaker(
            guesses,
            feedbacks,
            attempts);

        if (strcmp(
                feedback,
                "xxxxx") == 0 ||
            attempts == MAX_ATTEMPTS)
        {
            n = recv_line(
                game_fd,
                secret_message,
                sizeof(secret_message));

            if (n <= 0)
            {
                printf(
                    "\nPlayer disconnected. "
                    "Press Enter to go home.");

                fflush(stdout);
                getchar();

                return;
            }

            if (strncmp(
                    secret_message,
                    "SECRET ",
                    7) != 0)
            {
                printf(
                    "Invalid secret received.\n");

                return;
            }

            printf(
                "\n=====================================\n");

            if (strcmp(
                feedback,
                "xxxxx") == 0)
            {
                printf(
                    "Congratulations! "
                    "You guessed the sequence.\n");
            }
            else
            {
                printf(
                    "12 attempts exhausted.\n");
            }

            printf(
                "Secret sequence: %s\n",
                secret_message + 7);

            printf(
                "=====================================\n");

            printf(
                "\nPress Enter to return to home...");

            fflush(stdout);
            getchar();

            return;
        }
    }
}
