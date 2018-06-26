#include <stdio.h>        // stderr
#include <stdlib.h>       // exit
#include <unistd.h>       // close
#include <errno.h>        // errno
#include <string.h>       // strerror
#include <unistd.h>       // fileno
#include <pthread.h>      // pthread
#include <pulse/simple.h> // pulseaudio
#include <pulse/error.h>  // pulseaudio
#include "net.h"

#define N 128
#define APP_NAME "phone"

int exchange_voice(int sock);
void *send_voice(void *arg);
void *receive_voice(void *arg);

int main(int argc, char *argv[])
{
    int ret; // Success or failure returned value

    in_addr_t srv_addr = INADDR_ANY;
    in_port_t srv_port = htons(0);
    int lsn_limit = 10;
    ret = parse_optarg_server(argc, argv, &srv_addr, &srv_port, &lsn_limit);
    if (ret != 0)
    {
        fprintf(stderr, "ERROR: Failed to parse option arguments\n");
        exit(EXIT_FAILURE);
    }

    int sock;

    sock = connect_tcp_client(&srv_addr, &srv_port); // Try connect remote as client
    if (sock == -1)
    {
        fprintf(stderr, "ERROR: Failed to establish connection\n");
    }
    else
    {
        // Client mode
        exchange_voice(sock);

        ret = close(sock); // Close the socket
        if (ret == -1)
        {
            fprintf(stderr, "ERROR: Failed to close the socket: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    sock = listen_tcp_server(&srv_addr, &srv_port, lsn_limit); // Try establish local server
    if (sock == -1)
    {
        fprintf(stderr, "ERROR: Failed to establish server\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        // Server mode
        ret = handle_tcp_server(sock, exchange_voice);
        if (ret == -1)
        {
            fprintf(stderr, "ERROR: Failed to correctly serve: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        ret = close(sock); // Close the socket
        if (ret == -1)
        {
            fprintf(stderr, "ERROR: Failed to close the socket: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
}

int exchange_voice(int sock)
{
    int ret; // Success or failure returned value

    pthread_t send_tid;
    ret = pthread_create(&send_tid, NULL, send_voice, &sock);
    if (ret != 0)
    {
        fprintf(stderr, "ERROR: Failed to start thread for send: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    pthread_t receive_tid;
    ret = pthread_create(&receive_tid, NULL, receive_voice, &sock);
    if (ret != 0)
    {
        fprintf(stderr, "ERROR: Failed to start thread for receive: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    char c;
    read(STDIN_FILENO, &c, 1); // Blocking

    pthread_cancel(send_tid);
    pthread_cancel(receive_tid);
    pthread_join(send_tid, NULL);
    pthread_join(receive_tid, NULL);

    return EXIT_SUCCESS;
}

void *send_voice(void *arg)
{
    int ret;

    int net = *(int *)arg;

    char data[N + 1];
    // int n;

    pa_sample_spec ss;
    ss.format = PA_SAMPLE_S16LE;
    ss.rate = 44100;
    ss.channels = 1;

    int pa_errno;
    pa_simple *pa = pa_simple_new(NULL, APP_NAME, PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &pa_errno);
    if (pa == NULL)
    {
        fprintf(stderr, "ERROR: Failed to connect pulseaudio server for record: %s\n", pa_strerror(pa_errno));
        pthread_exit(NULL);
    }

    // FILE *rec = popen("rec -t raw -b 16 -c 1 -e s -r 44100 -", "r");

    while (1)
    {
        /** int n = fread(data, sizeof(char), N, rec);
        if (n <= 0)
        {
            fprintf(stderr, "ERROR: Failed to read data from record process\n");
            pthread_exit(NULL);
        } //*/
        ret = pa_simple_read(pa, data, N, &pa_errno);
        if (ret < 0)
        {
            fprintf(stderr, "ERROR: Failed to read data from pulseaudio: %s\n", pa_strerror(pa_errno));
            pthread_exit(NULL);
        }
        if (send(net, data, N, 0) < N)
        {
            fprintf(stderr, "ERROR: Failed to send all data to internet\n");
            pthread_exit(NULL);
        }
        pthread_testcancel();
    }

    // pclose(rec);
    pa_simple_free(pa);
    pthread_exit(NULL);
}

void *receive_voice(void *arg)
{
    int ret;

    int net = *(int *)arg;

    char data[N + 1];
    // int n;

    pa_sample_spec ss;
    ss.format = PA_SAMPLE_S16LE;
    ss.rate = 44100;
    ss.channels = 1;

    int pa_errno;
    pa_simple *pa = pa_simple_new(NULL, APP_NAME, PA_STREAM_PLAYBACK, NULL, "play", &ss, NULL, NULL, &pa_errno);
    if (pa == NULL)
    {
        fprintf(stderr, "ERROR: Failed to connect pulseaudio server for play: %s\n", pa_strerror(pa_errno));
        pthread_exit(NULL);
    }

    // FILE *play = popen("play -t raw -b 16 -c 1 -e s -r 44100 -", "w");

    while (1)
    {
        int n = recv(net, data, N, 0);
        if (n <= 0)
        {
            fprintf(stderr, "ERROR: Failed to receive data from internet\n");
            pthread_exit(NULL);
        }
        ret = pa_simple_write(pa, data, n, &pa_errno);
        if (ret < 0)
        {
            fprintf(stderr, "ERROR: Failed to write data to pulseaudio: %s\n", pa_strerror(pa_errno));
            pthread_exit(NULL);
        }
        /* if (fwrite(data, sizeof(char), n, play) < n)
        {
            fprintf(stderr, "ERROR: Failed to write all data to play process\n");
            pthread_exit(NULL);
        }
        */
        pthread_testcancel();
    }

    pa_simple_free(pa);
    pthread_exit(NULL);
}
