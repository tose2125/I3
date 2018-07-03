#ifndef SEND_RECEIVE_H
#define SEND_RECEIVE_H

#include <stdio.h>      // stderr, popen
#include <pthread.h>    // pthread
#include <sys/types.h>  // send, recv
#include <sys/socket.h> // send, recv

#define N 1024
#define APP_NAME "phone"

void *send_voice(void *arg);
void *receive_voice(void *arg);

#endif