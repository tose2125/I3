#include "net.h"

int parse_optarg_client(int argc, char *argv[], in_addr_t *dst_addr, in_port_t *dst_port)
{
    int ret = 0; // Success or failure to return value

    int opt;
    while ((opt = getopt(argc, argv, "a:p:")) != -1)
    {
        switch (opt)
        {
        case 'a':
            if (inet_aton(optarg, (struct in_addr *)dst_addr) != 1)
            {
                fprintf(stderr, "ERROR: Failed to convert IP address: %s\n", optarg);
                ret = 1;
            }
            break;
        case 'p':
            errno = 0;
            *dst_port = htons(strtoul(optarg, NULL, 10));
            if (errno == ERANGE)
            {
                fprintf(stderr, "ERROR: Failed to convert port number: %s\n", optarg);
                ret = 1;
            }
            break;
        default:
            fprintf(stderr, "Usage: %s -a dst_addr -p dst_port\n", argv[0]);
            ret = 1;
        }
    }

    return ret;
}

int parse_optarg_server(int argc, char *argv[], in_addr_t *srv_addr, in_port_t *srv_port_ctrl, in_port_t *srv_port_voip)
{
    int ret = 0; // Success or failure to return value

    int opt;
    while ((opt = getopt(argc, argv, "a:c:v:l:")) != -1)
    {
        switch (opt)
        {
        case 'a':
            if (inet_aton(optarg, (struct in_addr *)srv_addr) != 1)
            {
                fprintf(stderr, "ERROR: Failed to convert IP address: %s\n", optarg);
                ret = 1;
            }
            break;
        case 'c':
            errno = 0;
            *srv_port_ctrl = htons(strtoul(optarg, NULL, 10));
            if (errno == ERANGE)
            {
                fprintf(stderr, "ERROR: Failed to convert port number: %s\n", optarg);
                ret = 1;
            }
            break;
        case 'v':
            errno = 0;
            *srv_port_voip = htons(strtoul(optarg, NULL, 10));
            if (errno == ERANGE)
            {
                fprintf(stderr, "ERROR: Failed to convert port number: %s\n", optarg);
                ret = 1;
            }
            break;
        default:
            fprintf(stderr, "Usage: %s -a srv_addr -c srv_port_ctrl -v srv_port_voip\n", argv[0]);
            ret = 1;
        }
    }

    return ret;
}

int connect_tcp_client(in_addr_t *dst_addr, in_port_t *dst_port)
{
    int ret; // Success or failure returned value

    int sock = socket(PF_INET, SOCK_STREAM, 0); // TCP socket
    if (sock == -1)
    {
        fprintf(stderr, "ERROR: Failed to create a socket: %s\n", strerror(errno));
        return -1;
    }

    if (dst_addr == NULL || dst_port == NULL)
    {
        fprintf(stderr, "ERROR: Specify destination\n");
        return -1;
    }

    struct sockaddr_in addr;          // IPv4 address & port
    addr.sin_family = AF_INET;        // Mode IPv4
    addr.sin_addr.s_addr = *dst_addr; // Destination IP address
    addr.sin_port = *dst_port;        // Destination Port number

    ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr)); // Connect to destination
    if (ret == -1)
    {
        fprintf(stderr, "ERROR: Failed to connect to remote: %s\n", strerror(errno));
        return -1;
    }

    return sock;
}

int connect_udp_client(in_addr_t *dst_addr, in_port_t *dst_port)
{
    int ret; // Success or failure returned value

    int sock = socket(PF_INET, SOCK_DGRAM, 0); // UDP socket
    if (sock == -1)
    {
        fprintf(stderr, "ERROR: Failed to create a socket: %s\n", strerror(errno));
        return -1;
    }

    if (dst_addr == NULL || dst_port == NULL)
    {
        return sock;
    }

    struct sockaddr_in addr;          // IPv4 address & port
    addr.sin_family = AF_INET;        // Mode IPv4
    addr.sin_addr.s_addr = *dst_addr; // Destination IP address
    addr.sin_port = *dst_port;        // Destination Port number

    char data = '\n';
    ret = sendto(sock, &data, 1, 0, (struct sockaddr *)&addr, sizeof(addr)); // Connect to destination
    if (ret < 1)
    {
        fprintf(stderr, "ERROR: Failed to connect to remote: %s\n", strerror(errno));
        return -1;
    }

    return sock;
}

int listen_tcp_server(in_addr_t *srv_addr, in_port_t *srv_port)
{
    int ret; // Success or failure returned value

    int sock = socket(PF_INET, SOCK_STREAM, 0); // TCP socket
    if (sock == -1)
    {
        fprintf(stderr, "ERROR: Failed to create a socket: %s\n", strerror(errno));
        return -1;
    }

    struct sockaddr_in addr;          // IPv4 address & port
    addr.sin_family = AF_INET;        // Mode IPv4
    addr.sin_addr.s_addr = *srv_addr; // Server IP address
    addr.sin_port = *srv_port;        // Server Port number

    ret = bind(sock, (struct sockaddr *)&addr, sizeof(addr)); // Bind to socket
    if (ret == -1)
    {
        fprintf(stderr, "ERROR: Failed to bind port %d: %s\n", ntohs(*srv_port), strerror(errno));
        return -1;
    }

    ret = listen(sock, LISTEN_LIMIT); // Listen to socket
    if (ret == -1)
    {
        fprintf(stderr, "ERROR: Failed to listen to port %d: %s\n", ntohs(*srv_port), strerror(errno));
        return -1;
    }

    struct sockaddr_in lsn_addr;
    socklen_t lsn_addr_len = sizeof(lsn_addr);
    if (getsockname(sock, (struct sockaddr *)&lsn_addr, &lsn_addr_len) == 0)
    {
        fprintf(stdout, "INFO: Listening to port %d\n", ntohs(lsn_addr.sin_port));
    }

    return sock;
}

int handle_tcp_server(int srv_sock, int handler(int))
{
    int ret; // Success or failure returned value

    int cln_sock;                // Descriptor number of the current client
    struct sockaddr_in cln_addr; // IP address & port number of the client
    socklen_t cln_addr_len = sizeof(cln_addr);

    char cln_addr_str[16];

    while ((cln_sock = accept(srv_sock, (struct sockaddr *)&cln_addr, &cln_addr_len)) >= 0)
    {
        if (inet_ntop(AF_INET, &cln_addr.sin_addr.s_addr, cln_addr_str, 16) == NULL)
        {
            fprintf(stderr, "ERROR: Failed to convert client address to string\n");
        }
        fprintf(stdout, "INFO: Accept connection from %s\n", cln_addr_str);

        ret = handler(cln_sock);
        if (ret != EXIT_SUCCESS)
        {
            fprintf(stderr, "ERROR: Failed to correctly handle client access\n");
            return EXIT_FAILURE;
        }

        ret = close(cln_sock); // Close the socket
        if (ret == -1)
        {
            fprintf(stderr, "ERROR: Failed to close the socket to client: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }

        fprintf(stdout, "INFO: Successfully close connection to %s\n", cln_addr_str);
    }

    if (cln_sock == -1)
    {
        fprintf(stderr, "ERROR: Failed to accept access from client: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int listen_udp_server(in_addr_t *srv_addr, in_port_t *srv_port)
{
    int ret; // Success or failure returned value

    int sock = socket(PF_INET, SOCK_DGRAM, 0); // UDP socket
    if (sock == -1)
    {
        fprintf(stderr, "ERROR: Failed to create a socket: %s\n", strerror(errno));
        return -1;
    }

    struct sockaddr_in addr;          // IPv4 address & port
    addr.sin_family = AF_INET;        // Mode IPv4
    addr.sin_addr.s_addr = *srv_addr; // Server IP address
    addr.sin_port = *srv_port;        // Server Port number

    ret = bind(sock, (struct sockaddr *)&addr, sizeof(addr)); // Bind to socket
    if (ret == -1)
    {
        fprintf(stderr, "ERROR: Failed to bind port %d: %s\n", ntohs(*srv_port), strerror(errno));
        return -1;
    }

    /*
    ret = listen(sock, lsn_limit); // Listen to socket
    if (ret == -1)
    {
        fprintf(stderr, "ERROR: Failed to listen to port %d: %s\n", ntohs(*srv_port), strerror(errno));
        return -1;
    }
    */

    struct sockaddr_in lsn_addr;
    socklen_t lsn_addr_len = sizeof(lsn_addr);
    if (getsockname(sock, (struct sockaddr *)&lsn_addr, &lsn_addr_len) == 0)
    {
        fprintf(stdout, "INFO: Listening to port %d\n", ntohs(lsn_addr.sin_port));
    }

    return sock;
}
