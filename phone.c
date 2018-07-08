#include <stdio.h>     // stderr, fprintf
#include <stdlib.h>    // exit, strtoul
#include <unistd.h>    // close
#include <errno.h>     // errno
#include <string.h>    // strerror, memset, strtok_r
#include <unistd.h>    // fileno, getlogin
#include <pthread.h>   // pthread
#include <sys/epoll.h> // epoll
#include "net.h"
#include "send_receive.h"

#define EPOLL_LIMIT 3
#define EPOLL_TIMEOUT 10

struct voip_connection
{
    char is_client; // 0: Not, 1: Pending, 2: Active
    char is_server; // 0: Not, 1: Pending, 2: Active
    in_addr_t dst_addr;
    in_port_t dst_port_ctrl;
    in_port_t dst_port_voip;
    char name[32];
};

int main(int argc, char *argv[])
{
    int ret; // Success or failure returned value

    // Get current login user name
    char *name = getlogin();
    if (name == NULL)
    {
        fprintf(stderr, "ERROR: Failed to get current user name\n");
        name = "guest";
    }

    // Parse option arguments
    in_addr_t srv_addr = INADDR_ANY;
    in_port_t srv_port_ctrl = htons(0);
    in_port_t srv_port_voip = htons(0);
    ret = parse_optarg_server(argc, argv, &srv_addr, &srv_port_ctrl, &srv_port_voip);
    if (ret != 0)
    {
        fprintf(stderr, "ERROR: Failed to parse option arguments\n");
        exit(EXIT_FAILURE);
    }

    // Initialize connection
    struct voip_connection connection;
    memset(&connection, 0, sizeof(connection));

    // Create epoll
    int epfd = epoll_create(EPOLL_LIMIT);
    if (epfd == -1)
    {
        fprintf(stderr, "ERROR: Failed to start epoll: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Prepare stdin
    struct epoll_event stdin_event;
    memset(&stdin_event, 0, sizeof(stdin_event));
    stdin_event.events = EPOLLIN;
    stdin_event.data.fd = STDIN_FILENO;
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &stdin_event);
    if (ret == -1)
    {
        fprintf(stderr, "ERROR: Failed to add stdin to epoll: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Prepare servers
    struct sockaddr_in lsn_addr;
    socklen_t lsn_addr_len = sizeof(lsn_addr);

    // Prepare control server
    int srv_sock_ctrl = listen_udp_server(&srv_addr, &srv_port_ctrl);
    if (srv_sock_ctrl == -1)
    {
        fprintf(stderr, "ERROR: Failed to establish control server\n");
        exit(EXIT_FAILURE);
    }
    if (getsockname(srv_sock_ctrl, (struct sockaddr *)&lsn_addr, &lsn_addr_len) == 0)
    {
        srv_port_ctrl = lsn_addr.sin_port;
    }

    struct epoll_event srv_ctrl_event;
    memset(&srv_ctrl_event, 0, sizeof(srv_ctrl_event));
    srv_ctrl_event.events = EPOLLIN;
    srv_ctrl_event.data.fd = srv_sock_ctrl;
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, srv_sock_ctrl, &srv_ctrl_event);
    if (ret == -1)
    {
        fprintf(stderr, "ERROR: Failed to add control server to epoll: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Prepare voip server
    int srv_sock_voip = listen_tcp_server(&srv_addr, &srv_port_voip);
    if (srv_sock_voip == -1)
    {
        fprintf(stderr, "ERROR: Failed to establish voip server\n");
    }
    if (getsockname(srv_sock_voip, (struct sockaddr *)&lsn_addr, &lsn_addr_len) == 0)
    {
        srv_port_voip = lsn_addr.sin_port;
    }

    struct epoll_event srv_voip_event;
    memset(&srv_voip_event, 0, sizeof(srv_voip_event));
    srv_voip_event.events = EPOLLIN;
    srv_voip_event.data.fd = srv_sock_voip;
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, srv_sock_voip, &srv_voip_event);
    if (ret == -1)
    {
        fprintf(stderr, "ERROR: Failed to add voip server to epoll: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // voip socket only client
    struct send_receive cln_voip = {0};
    cln_voip.print = calloc(sizeof(int), 1);
    pthread_mutex_init(&cln_voip.locker, NULL);

    // Prepare main loop
    char read_data[N];
    char send_data[N];
    struct sockaddr_in dst_addr;
    socklen_t dst_addr_len = sizeof(dst_addr);

    pthread_t send_tid;
    pthread_t receive_tid;

    int available_events;
    struct epoll_event current_events[EPOLL_LIMIT];
    while ((available_events = epoll_wait(epfd, current_events, EPOLL_LIMIT, 1000 * EPOLL_TIMEOUT)) >= 0)
    {
        for (int i = 0; i < available_events; i++)
        {
            // Handle each file descriptor
            memset(read_data, 0, N);
            memset(send_data, 0, N);
            memset(&dst_addr, 0, sizeof(dst_addr));
            if (current_events[i].data.fd == STDIN_FILENO)
            {
                // Stdin
                int n = read(current_events[i].data.fd, read_data, N - 1);
                if (n <= 0)
                {
                    fprintf(stderr, "ERROR: Failed to read data from stdin\n");
                    return EXIT_FAILURE;
                }
                // Analyze written string
                char *statement, *statement_save = NULL;
                for (statement = strtok_r(read_data, "\n", &statement_save); statement != NULL; statement = strtok_r(NULL, "\n", &statement_save))
                {
                    char *tmp_data = calloc(sizeof(char), strlen(statement) + 1);
                    strcpy(tmp_data, statement);
                    char *command, *command_save = NULL, *arg;
                    command = strtok_r(tmp_data, " ", &command_save);
                    if (command == NULL)
                    {
                        fprintf(stderr, "ERROR: No command found\n");
                        continue;
                    }
                    else if (strcmp(command, "call") == 0)
                    {
                        // Request connection to control server as client
                        if (connection.is_client != 0 || connection.is_server != 0)
                        {
                            fprintf(stderr, "ERROR: Already has active connection\n");
                            continue;
                        }
                        connection.is_client = 1;
                        // Try parse destination address
                        if ((arg = strtok_r(NULL, " ", &command_save)) == NULL)
                        {
                            fprintf(stderr, "ERROR: Usage: connect dst_addr dst_port\n");
                            continue;
                        }
                        if (inet_aton(arg, &dst_addr.sin_addr) != 1)
                        {
                            fprintf(stderr, "ERROR: Failed to convert IP address: %s\n", arg);
                            continue;
                        }
                        connection.dst_addr = dst_addr.sin_addr.s_addr;
                        // Try parse destination port
                        if ((arg = strtok_r(NULL, " ", &command_save)) == NULL)
                        {
                            fprintf(stderr, "ERROR: Usage: connect dst_addr dst_port\n");
                            continue;
                        }
                        errno = 0;
                        dst_addr.sin_port = htons(strtoul(arg, NULL, 10));
                        if (errno == ERANGE)
                        {
                            fprintf(stderr, "ERROR: Failed to convert port number: %s\n", arg);
                            continue;
                        }
                        connection.dst_port_ctrl = dst_addr.sin_port;
                        // Send voip request
                        sprintf(send_data, "call %s\n", name);
                        ret = sendto(srv_sock_ctrl, send_data, strlen(send_data), 0, (struct sockaddr *)&dst_addr, dst_addr_len);
                        if (ret < strlen(send_data))
                        {
                            fprintf(stderr, "ERROR: Failed to send all data to remote: %s\n", strerror(errno));
                            continue;
                        }
                        printf("INFO: Successfully sent your call request\n");
                    }
                    else if (strcmp(command, "start") == 0)
                    {
                        // Accept voip request from control client as server
                        if (connection.is_client != 0 || connection.is_server != 1)
                        {
                            fprintf(stderr, "ERROR: Not have pending request\n");
                            continue;
                        }
                        dst_addr.sin_family = AF_INET;
                        dst_addr.sin_addr.s_addr = connection.dst_addr;
                        dst_addr.sin_port = connection.dst_port_ctrl;
                        // Send voip request
                        sprintf(send_data, "start %d\n", ntohs(srv_port_voip));
                        ret = sendto(srv_sock_ctrl, send_data, strlen(send_data), 0, (struct sockaddr *)&dst_addr, dst_addr_len);
                        if (ret < strlen(send_data))
                        {
                            fprintf(stderr, "ERROR: Failed to send all data to remote: %s\n", strerror(errno));
                            break;
                        }
                        printf("INFO: Successfully sent your start response\n");
                    }
                    else if (strcmp(command, "msg") == 0)
                    {
                        if (connection.is_client != 2 && connection.is_server != 2)
                        {
                            fprintf(stderr, "INFO: Not have any connection\n");
                            continue;
                        }
                        // Send message
                        dst_addr.sin_family = AF_INET;
                        dst_addr.sin_addr.s_addr = connection.dst_addr;
                        dst_addr.sin_port = connection.dst_port_ctrl;
                        if ((arg = strtok_r(NULL, "", &command_save)) == NULL)
                        {
                            fprintf(stderr, "ERROR: Usage: msg content\n");
                            continue;
                        }
                        sprintf(send_data, "message %s\n", arg);
                        ret = sendto(srv_sock_ctrl, send_data, strlen(send_data), 0, (struct sockaddr *)&dst_addr, dst_addr_len);
                        if (ret < strlen(send_data))
                        {
                            fprintf(stderr, "ERROR: Failed to send all data to remote: %s\n", strerror(errno));
                            continue;
                        }
                    }
                    else if (strcmp(command, "stop") == 0)
                    {
                        // Stop any voip connection
                        if (connection.is_client == 0 && connection.is_server == 0)
                        {
                            fprintf(stderr, "INFO: Not have any connection\n");
                            continue;
                        }
                        if (connection.is_client == 2 || connection.is_server == 2)
                        {
                            // Send voip shutdown
                            dst_addr.sin_family = AF_INET;
                            dst_addr.sin_addr.s_addr = connection.dst_addr;
                            dst_addr.sin_port = connection.dst_port_ctrl;
                            sprintf(send_data, "stop\n");
                            ret = sendto(srv_sock_ctrl, send_data, strlen(send_data), 0, (struct sockaddr *)&dst_addr, dst_addr_len);
                            if (ret < strlen(send_data))
                            {
                                fprintf(stderr, "ERROR: Failed to send all data to remote: %s\n", strerror(errno));
                                continue;
                            }
                            // Stop threads
                            pthread_cancel(send_tid);
                            pthread_cancel(receive_tid);
                            pthread_join(send_tid, NULL);
                            pthread_join(receive_tid, NULL);
                            close(cln_voip.sock);
                        }
                        memset(&connection, 0, sizeof(connection));
                        printf("INFO: Successfully closed the connection\n");
                    }
                    else if (strcmp(command, "counter") == 0)
                    {
                        pthread_mutex_lock(&cln_voip.locker);
                        *cln_voip.print = 3;
                        pthread_mutex_unlock(&cln_voip.locker);
                    }
                    else if (strcmp(command, "exit") == 0)
                    {
                        // Exit process
                        return EXIT_SUCCESS;
                    }
                    else
                    {
                        fprintf(stderr, "ERROR: Invalid command: %s\n", command);
                    }
                }
            }
            else if (current_events[i].data.fd == srv_sock_ctrl)
            {
                // Control server
                int n = recvfrom(current_events[i].data.fd, read_data, N - 1, 0, (struct sockaddr *)&dst_addr, &dst_addr_len);
                if (n <= 0)
                {
                    fprintf(stderr, "ERROR: Failed to read data from stdin\n");
                    continue;
                }
                // Analyze received string
                char *statement, *statement_save = NULL;
                for (statement = strtok_r(read_data, "\n", &statement_save); statement != NULL; statement = strtok_r(NULL, "\n", &statement_save))
                {
                    char *tmp_data = calloc(sizeof(char), strlen(statement) + 1);
                    strcpy(tmp_data, statement);
                    char *command, *command_save = NULL, *arg;
                    command = strtok_r(tmp_data, " ", &command_save);
                    if (command == NULL)
                    {
                        continue;
                    }
                    else if (strcmp(command, "call") == 0)
                    {
                        // Receive connection request from control client
                        if (connection.is_client != 0 || connection.is_server != 0)
                        {
                            fprintf(stderr, "INFO: Received new call request\n");
                            continue;
                        }
                        connection.is_server = 1;
                        connection.dst_addr = dst_addr.sin_addr.s_addr;
                        connection.dst_port_ctrl = dst_addr.sin_port;
                        // Show request
                        char dst_addr_str[40] = {0};
                        if (inet_ntop(AF_INET, &dst_addr.sin_addr, dst_addr_str, 39) == NULL)
                        {
                            fprintf(stderr, "ERROR: Failed to convert client address to string\n");
                        }
                        arg = strtok_r(NULL, " ", &command_save);
                        if (arg == NULL)
                        {
                            arg = "";
                        }
                        printf("INFO: Call request from %s@%s:%d\n", arg, dst_addr_str, ntohs(dst_addr.sin_port));
                    }
                    else if (strcmp(command, "start") == 0)
                    {
                        // Receive connection acceptance from control server
                        if (connection.is_client != 1 || connection.is_server != 0 || connection.dst_addr != dst_addr.sin_addr.s_addr || connection.dst_port_ctrl != dst_addr.sin_port)
                        {
                            fprintf(stderr, "INFO: Received invalid start command\n");
                            continue;
                        }
                        connection.is_client = 2;
                        // Get remote voip port
                        arg = strtok_r(NULL, " ", &command_save);
                        if (arg == NULL)
                        {
                            arg = "";
                        }
                        connection.dst_port_voip = htons(strtoul(arg, NULL, 10));
                        // Start voip
                        printf("INFO: Remote accepted, now start voip\n");
                        pthread_mutex_lock(&cln_voip.locker);
                        cln_voip.sock = connect_tcp_client(&connection.dst_addr, &connection.dst_port_voip);
                        if (cln_voip.sock == -1)
                        {
                            fprintf(stderr, "ERROR: Failed to connect remote voip server\n");
                            continue;
                        }
                        pthread_mutex_unlock(&cln_voip.locker);
                        // Start threads

                        ret = pthread_create(&send_tid, NULL, send_voice, &cln_voip);
                        if (ret != 0)
                        {
                            fprintf(stderr, "ERROR: Failed to start thread for send: %s\n", strerror(errno));
                        }
                        ret = pthread_create(&receive_tid, NULL, receive_voice, &cln_voip);
                        if (ret != 0)
                        {
                            fprintf(stderr, "ERROR: Failed to start thread for receive: %s\n", strerror(errno));
                        }
                    }
                    else if (strcmp(command, "message") == 0)
                    {
                        // Stop current voip connection
                        if ((connection.is_client != 2 && connection.is_server != 2) || connection.dst_addr != dst_addr.sin_addr.s_addr || connection.dst_port_ctrl != dst_addr.sin_port)
                        {
                            fprintf(stderr, "INFO: Received invalid message command\n");
                            continue;
                        }
                        // Show message
                        if ((arg = strtok_r(NULL, "", &command_save)) == NULL)
                        {
                            fprintf(stderr, "ERROR: Received no content message\n");
                        }
                        printf("INFO: Received message: %s\n", arg);
                    }
                    else if (strcmp(command, "stop") == 0)
                    {
                        // Stop current voip connection
                        if ((connection.is_client != 2 && connection.is_server != 2) || connection.dst_addr != dst_addr.sin_addr.s_addr || connection.dst_port_ctrl != dst_addr.sin_port)
                        {
                            fprintf(stderr, "INFO: Received invalid stop command\n");
                            continue;
                        }
                        memset(&connection, 0, sizeof(connection));
                        // Stop threads
                        pthread_cancel(send_tid);
                        pthread_cancel(receive_tid);
                        pthread_join(send_tid, NULL);
                        pthread_join(receive_tid, NULL);
                        close(cln_voip.sock);
                        printf("INFO: Successfully closed the connection by remote request\n");
                    }
                    else
                    {
                        fprintf(stderr, "ERROR: Received invalid command\n");
                    }
                }
            }
            else if (current_events[i].data.fd == srv_sock_voip)
            {
                // Voip server
                pthread_mutex_lock(&cln_voip.locker);
                cln_voip.sock = accept(srv_sock_voip, (struct sockaddr *)&dst_addr, &dst_addr_len);
                if (cln_voip.sock == -1)
                {
                    fprintf(stderr, "ERROR: Failed to accept access from client: %s\n", strerror(errno));
                    return EXIT_FAILURE;
                }
                pthread_mutex_unlock(&cln_voip.locker);
                // Catch access from voip client
                if (connection.is_client != 0 || connection.is_server != 1 || connection.dst_addr != dst_addr.sin_addr.s_addr)
                {
                    fprintf(stderr, "INFO: Received invalid voip connection\n");
                    continue;
                }
                connection.is_client = 2;
                printf("INFO: Received from client, now start voip\n");
                // Start threads
                ret = pthread_create(&send_tid, NULL, send_voice, &cln_voip);
                if (ret != 0)
                {
                    fprintf(stderr, "ERROR: Failed to start thread for send: %s\n", strerror(errno));
                }
                ret = pthread_create(&receive_tid, NULL, receive_voice, &cln_voip);
                if (ret != 0)
                {
                    fprintf(stderr, "ERROR: Failed to start thread for receive: %s\n", strerror(errno));
                }
            }
        }
    }

    ret = close(epfd);
    if (ret == -1)
    {
        fprintf(stderr, "ERROR: Failed to close the epfd: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    ret = close(srv_sock_voip);
    if (ret == -1)
    {
        fprintf(stderr, "ERROR: Failed to close the voip socket: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    ret = close(srv_sock_ctrl);
    if (ret == -1)
    {
        fprintf(stderr, "ERROR: Failed to close the control socket: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
