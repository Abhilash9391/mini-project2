#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "utils.h"
#include "udp_codebreaker.h"

#define MAX_ATTEMPTS 12

static int valid_feedback(const char *feedback) {
    if (strlen(feedback) != CODE_LENGTH) return 0;
    for (int i = 0; i < CODE_LENGTH; i++) {
        if (feedback[i] != 'x' && feedback[i] != 'o' && feedback[i] != '-') return 0;
    }
    return 1;
}

void udp_codebreaker_game(int fd, struct sockaddr_in *opponent_addr) {
    char guesses[MAX_ATTEMPTS][CODE_LENGTH + 1];
    char feedbacks[MAX_ATTEMPTS][CODE_LENGTH + 1];
    int attempts = 0;

    while (attempts < MAX_ATTEMPTS) {
        char guess[100];
        char feedback[100];
        char secret_message[100];

        printf("\nEnter guess: ");
        fflush(stdout);

        if (fgets(guess, sizeof(guess), stdin) == NULL) return;
        remove_newline(guess);

        if (!valid_code(guess)) {
            printf("Invalid guess. Enter exactly 5 digits.\n");
            continue;
        }

        strcpy(guesses[attempts], guess);

        if (udp_send_reliable(fd, opponent_addr, guess) < 0) {
            printf("\nPlayer disconnected. Press Enter to go home.");
            fflush(stdout);
            getchar();
            return;
        }

        if (udp_recv_reliable(fd, opponent_addr, feedback, sizeof(feedback)) <= 0) {
            printf("\nPlayer disconnected. Press Enter to go home.");
            fflush(stdout);
            getchar();
            return;
        }

        if (!valid_feedback(feedback)) {
            printf("Invalid feedback received.\n");
            return;
        }

        strcpy(feedbacks[attempts], feedback);
        attempts++;

        print_board_codebreaker(guesses, feedbacks, attempts);

        if (strcmp(feedback, "xxxxx") == 0 || attempts == MAX_ATTEMPTS) {
            if (udp_recv_reliable(fd, opponent_addr, secret_message, sizeof(secret_message)) <= 0) {
                printf("\nPlayer disconnected. Press Enter to go home.");
                fflush(stdout);
                getchar();
                return;
            }

            if (strncmp(secret_message, "SECRET ", 7) != 0) {
                printf("Invalid secret received.\n");
                return;
            }

            printf("\n=====================================\n");
            if (strcmp(feedback, "xxxxx") == 0) {
                printf("Congratulations! You guessed the sequence.\n");
            } else {
                printf("12 attempts exhausted.\n");
            }
            printf("Secret sequence: %s\n", secret_message + 7);
            printf("=====================================\n");
            printf("\nPress Enter to return to home...");
            fflush(stdout);
            getchar();
            return;
        }
    }
}
