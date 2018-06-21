#ifndef NET_H
#define NET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <getopt.h> // getopt
#include <stdio.h>  // stderr
#include <stdlib.h> // strtoul
#include <errno.h>  // errno
#include <string.h> // strerror
#include <unistd.h> // close

#define UDP_EOD '\n'

int parse_optarg_client(int argc, char *argv[], in_addr_t *dst_addr, in_port_t *dst_port);
int parse_optarg_server(int argc, char *argv[], in_addr_t *srv_addr, in_port_t *srv_port, int *lsn_limit);
int connect_tcp_client(in_addr_t *dst_addr, in_port_t *dst_port);
int connect_udp_client(in_addr_t *dst_addr, in_port_t *dst_port);
int listen_tcp_server(in_addr_t *srv_addr, in_port_t *srv_port, int lsn_limit);
int handle_tcp_server(int srv_sock, int handler(int));

#endif
