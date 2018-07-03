#include <pulse/simple.h> // pulseaudio
#include <pulse/error.h>  // pulseaudio
#include "send_receive.h"

void *send_voice(void *arg)
{
    int ret;

    int net = *(int *)arg;

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

    char data[N + 1] = {0};

    while (1)
    {
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

    pa_simple_free(pa);
    pthread_exit(NULL);
}

void *receive_voice(void *arg)
{
    int ret;

    int net = *(int *)arg;

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

    char data[N + 1] = {0};

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
        pthread_testcancel();
    }

    pa_simple_free(pa);
    pthread_exit(NULL);
}
