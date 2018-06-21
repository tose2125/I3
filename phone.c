#include <stdio.h>   // stderr
#include <stdlib.h>  // exit
#include <unistd.h>  // close
#include <errno.h>   // errno
#include <string.h>  // strerror
#include <unistd.h>  // fileno
#include <pthread.h> // pthread
#include "net.h"

#define N 128

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
            fprintf(stderr, "ERROR: Cannot close the socket: %s\n", strerror(errno));
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
            fprintf(stderr, "ERROR: Cannot close the socket: %s\n", strerror(errno));
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
    int n;

    /*int rec_pipe[2];
    ret = pipe(rec_pipe);
    if (ret == -1)
    {
        fprintf(stderr, "ERROR: Failed to create pipe for rec: %s\n", strerror(errno));
        pthread_exit(NULL);
    }
    pid_t rec_pid = fork();
    if (rec_pid == -1)
    {
        fprintf(stderr, "ERROR: Failed to fork this process for rec\n");
        pthread_exit(NULL);
    }
    else if (rec_pid == 0)
    {
        // Child process
        close(rec_pipe[0]);               // the read end of rec_pipe is unnecessary for child process
        dup2(rec_pipe[1], STDOUT_FILENO); // Stdout of child process as the write end of rec_pipe
        // Exec rec command
        ret = execl("/usr/bin/rec", "-t", "raw", "-b", "16", "-c", "1", "-e", "s", "-r", "44100", "-", (char *)NULL);
        if (ret == -1)
        {
            fprintf(stderr, "ERROR: Failed to execute record command: %s\n", strerror(errno));
            pthread_exit(NULL);
        }
    }
    else
    {
        // Parent process
        close(rec_pipe[1]); // the write end of rec_pipe is unnecessary for parent process
    }*/

    FILE *rec = popen("rec -t raw -b 16 -c 1 -e s -r 44100 -", "r");

    while (1)
    {
        // n = read(rec_pipe[0], data, N);
        n = fread(data, sizeof(char), N, rec);
        if (n <= 0)
        {
            fprintf(stderr, "ERROR: Cannot read data from record process\n");
            pthread_exit(NULL);
        }
        if (send(net, data, n, 0) < n)
        {
            fprintf(stderr, "ERROR: Cannot send all data to internet\n");
            pthread_exit(NULL);
        }
        pthread_testcancel();
    }

    pclose(rec);
    pthread_exit(NULL);
}

void *receive_voice(void *arg)
{
    int ret;

    int net = *(int *)arg;

    char data[N + 1];
    int n;

    /*int play_pipe[2];
    ret = pipe(play_pipe);
    if (ret == -1)
    {
        fprintf(stderr, "ERROR: Failed to create pipe for play: %s\n", strerror(errno));
        pthread_exit(NULL);
    }
    pid_t play_pid = fork();
    if (play_pid == -1)
    {
        fprintf(stderr, "ERROR: Failed to fork this process for play\n");
        pthread_exit(NULL);
    }
    else if (play_pid == 0)
    {
        // Child process
        close(play_pipe[1]);              // the write end of rec_pipe is unnecessary for child process
        dup2(play_pipe[0], STDIN_FILENO); // Stdout of child process as the read end of rec_pipe
        // Exec rec command
        ret = execl("/usr/bin/play", "-t", "raw", "-b", "16", "-c", "1", "-e", "s", "-r", "44100", "-", (char *)NULL);
        if (ret == -1)
        {
            fprintf(stderr, "ERROR: Failed to execute play command: %s\n", strerror(errno));
            pthread_exit(NULL);
        }
    }
    else
    {
        // Parent process
        close(play_pipe[0]); // the write end of rec_pipe is unnecessary for parent process
    }*/

    FILE *play = popen("play -t raw -b 16 -c 1 -e s -r 44100 -", "w");

    while (1)
    {
        n = recv(net, data, N, 0);
        if (n <= 0)
        {
            fprintf(stderr, "ERROR: Cannot receive data from internet\n");
            pthread_exit(NULL);
        }
        // if (write(play_pipe[1], data, n) < n)
        if (fwrite(data, sizeof(char), n, play) < n)
        {
            fprintf(stderr, "ERROR: Cannot write all data to play process\n");
            pthread_exit(NULL);
        }
        pthread_testcancel();
    }

    pthread_exit(NULL);
}
