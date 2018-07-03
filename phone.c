#include <stdio.h>   // stderr
#include <stdlib.h>  // exit
#include <unistd.h>  // close
#include <errno.h>   // errno
#include <string.h>  // strerror
#include <unistd.h>  // fileno
#include <pthread.h> // pthread
#include "net.h"
#include "send_receive.h"

int exchange_voice(int sock);

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
