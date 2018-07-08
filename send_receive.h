#ifndef SEND_RECEIVE_H
#define SEND_RECEIVE_H

#include <stdio.h>     // stderr, popen
#include <pthread.h>   // pthread
#include <opus/opus.h> // opus
#include "net.h"       // send, recv

#define N 4096
#define APP_NAME "phone"

struct send_receive
{
    int sock;
    int *print;
    pthread_mutex_t locker;
};

void *send_voice(void *arg);
void *receive_voice(void *arg);

#endif
