#include "send_receive.h"

void *send_voice(void *arg)
{
    // int ret;

    int net = *(int *)arg;

    char *sox_command = "rec -t raw -b 16 -c 1 -e s -r 44100 -";
    FILE *sox = popen(sox_command, "r");
    if (sox == NULL)
    {
        fprintf(stderr, "ERROR: Failed to start record process: %s\n", sox_command);
        pthread_exit(NULL);
    }

    char data[N + 1] = {0};

    while (1)
    {
        int n = fread(data, sizeof(char), N, sox);
        if (n <= 0)
        {
            fprintf(stderr, "ERROR: Failed to read data from record process\n");
            pthread_exit(NULL);
        }
        if (send(net, data, n, 0) < n)
        {
            fprintf(stderr, "ERROR: Failed to send all data to internet\n");
            pthread_exit(NULL);
        }
        pthread_testcancel();
    }

    pclose(sox);
    pthread_exit(NULL);
}

void *receive_voice(void *arg)
{
    // int ret;

    int net = *(int *)arg;

    char *sox_command = "play -t raw -b 16 -c 1 -e s -r 44100 -";
    FILE *sox = popen(sox_command, "w");
    if (sox == NULL)
    {
        fprintf(stderr, "ERROR: Failed to start play process: %s\n", sox_command);
        pthread_exit(NULL);
    }

    char data[N + 1] = {0};

    while (1)
    {
        int n = recv(net, data, N, 0);
        if (n <= 0)
        {
            fprintf(stderr, "ERROR: Failed to receive data from internet\n");
            pthread_exit(NULL);
        }
        if (fwrite(data, sizeof(char), n, sox) < n)
        {
            fprintf(stderr, "ERROR: Failed to write all data to play process\n");
            pthread_exit(NULL);
        }
        pthread_testcancel();
    }

    pclose(sox);
    pthread_exit(NULL);
}
