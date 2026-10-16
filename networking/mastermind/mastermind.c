#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "utils.h"

#define MAX_ATTEMPTS 12

void calculate_feedback(
    const char *sequence,
    const char *guess,
    char *feedback)
{
    int used[CODE_LENGTH] = {0};

    for (int i = 0; i < CODE_LENGTH; i++)
    {
        if (guess[i] == sequence[i])
        {
            feedback[i] = 'x';
            used[i] = 1;
        }
        else
        {
            feedback[i] = '-';
        }
    }

    for (int i = 0; i < CODE_LENGTH; i++)
    {
        if (feedback[i] == 'x')
            continue;

        for (int j = 0; j < CODE_LENGTH; j++)
        {
            if (!used[j] &&
                guess[i] == sequence[j])
            {
                feedback[i] = 'o';
                used[j] = 1;
                break;
            }
        }
    }

    feedback[CODE_LENGTH] = '\0';
}

void mastermind_game(
    int game_fd,
    const char *opponent_name)
{
    char sequence[100];

    while (1)
    {
        printf(
            "Enter master sequence: ");

        fflush(stdout);

        if (fgets(
                sequence,
                sizeof(sequence),
                stdin) == NULL)
        {
            printf(
                "\nInput closed.\n");

            return;
        }

        remove_newline(sequence);

        if (!valid_code(sequence))
        {
            printf(
                "Invalid sequence. "
                "Enter exactly 5 digits.\n");

            continue;
        }

        break;
    }

    if (send_line(game_fd, "READY") < 0)
    {
        printf("\nPlayer disconnected while sending ready signal.\n");
        return;
    }

    char guesses[
        MAX_ATTEMPTS][CODE_LENGTH + 1];

    char feedbacks[
        MAX_ATTEMPTS][CODE_LENGTH + 1];

    int attempts = 0;

    while (attempts < MAX_ATTEMPTS)
    {
        char guess[100];
        char feedback[100];

        int n = recv_line(
            game_fd,
            guess,
            sizeof(guess));

        if (n <= 0)
        {
            printf(
                "\nPlayer disconnected. "
                "Press Enter to go home.");

            fflush(stdout);
            getchar();

            return;
        }

        if (!valid_code(guess))
        {
            printf(
                "Invalid guess received.\n");

            continue;
        }

        strcpy(
            guesses[attempts],
            guess);

        calculate_feedback(
            sequence,
            guess,
            feedback);

        strcpy(
            feedbacks[attempts],
            feedback);

        printf(
            "\nGuess received: %s\n",
            guess);

        printf(
            "Feedback: ");

        print_feedback(feedback);

        printf(
            "\nPress Enter to send feedback...");

        fflush(stdout);

        char input[10];

        if (fgets(
                input,
                sizeof(input),
                stdin) == NULL)
        {
            return;
        }

        if (send_line(
                game_fd,
                feedback) < 0)
        {
            printf(
                "\nPlayer disconnected. "
                "Press Enter to go home.");

            fflush(stdout);
            getchar();

            return;
        }

        attempts++;

        print_board_mastermind(
            guesses,
            feedbacks,
            attempts,
            sequence);

        if (strcmp(
                feedback,
                "xxxxx") == 0 ||
            attempts == MAX_ATTEMPTS)
        {
            char secret_message[100];

            snprintf(
                secret_message,
                sizeof(secret_message),
                "SECRET %.5s",
                sequence);

            if (send_line(
                    game_fd,
                    secret_message) < 0)
            {
                printf(
                    "\nPlayer disconnected. "
                    "Press Enter to go home.");

                fflush(stdout);
                getchar();

                return;
            }

            if (strcmp(
                feedback,
                "xxxxx") == 0)
            {
                printf(
                    "\nCodebreaker guessed "
                    "the sequence!\n");
            }
            else
            {
                printf(
                    "\n12 attempts exhausted.\n");
            }

            printf(
                "Secret sequence: %s\n",
                sequence);

            printf(
                "\nPress Enter to return to home...");

            fflush(stdout);
            getchar();

            close(game_fd);

            return;
        }
    }
}
