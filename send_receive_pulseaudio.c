#include <pulse/simple.h> // pulseaudio
#include <pulse/error.h>  // pulseaudio
#include "send_receive.h"

void *send_voice(void *arg)
{
    int ret;

    int net = *(int *)arg;

    OpusEncoder *opus = opus_encoder_create(48000, 1, OPUS_APPLICATION_AUDIO, &ret);
    if (opus == NULL)
    {
        fprintf(stderr, "ERROR: Failed to start opus encoder: %s\n", opus_strerror(ret));
        pthread_exit(NULL);
    }

    pa_sample_spec ss;
    ss.format = PA_SAMPLE_S16LE;
    ss.rate = 48000;
    ss.channels = 1;

    int pa_errno;
    pa_simple *pa = pa_simple_new(NULL, APP_NAME, PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &pa_errno);
    if (pa == NULL)
    {
        fprintf(stderr, "ERROR: Failed to connect pulseaudio server for record: %s\n", pa_strerror(pa_errno));
        pthread_exit(NULL);
    }

    char pcm_data[N + 1] = {0};
    unsigned char opus_data[N + 1] = {0};

    while (1)
    {
        ret = pa_simple_read(pa, pcm_data, 1920, &pa_errno);
        if (ret < 0)
        {
            fprintf(stderr, "ERROR: Failed to read data from pulseaudio: %s\n", pa_strerror(pa_errno));
            pthread_exit(NULL);
        }
        int n = opus_encode(opus, (opus_int16 *)pcm_data, 960, opus_data, N);
        if (n < 0)
        {
            fprintf(stderr, "ERROR: Failed to encode: %s\n", opus_strerror(n));
            pthread_exit(NULL);
        }
        else if (n <= 2)
        {
            continue;
        }
        if (send(net, opus_data, n, 0) < n)
        {
            fprintf(stderr, "ERROR: Failed to send all data to internet\n");
            pthread_exit(NULL);
        }
        pthread_testcancel();
    }

    pa_simple_free(pa);
    opus_encoder_destroy(opus);
    pthread_exit(NULL);
}

void *receive_voice(void *arg)
{
    int ret;

    int net = *(int *)arg;

    OpusDecoder *opus = opus_decoder_create(48000, 1, &ret);
    if (opus == NULL)
    {
        fprintf(stderr, "ERROR: Failed to start opus decoder: %s\n", opus_strerror(ret));
        pthread_exit(NULL);
    }

    pa_sample_spec ss;
    ss.format = PA_SAMPLE_S16LE;
    ss.rate = 48000;
    ss.channels = 1;

    int pa_errno;
    pa_simple *pa = pa_simple_new(NULL, APP_NAME, PA_STREAM_PLAYBACK, NULL, "play", &ss, NULL, NULL, &pa_errno);
    if (pa == NULL)
    {
        fprintf(stderr, "ERROR: Failed to connect pulseaudio server for play: %s\n", pa_strerror(pa_errno));
        pthread_exit(NULL);
    }

    char pcm_data[N + 1] = {0};
    unsigned char opus_data[N + 1] = {0};

    while (1)
    {
        int n = recv(net, opus_data, N, 0);
        if (n <= 0)
        {
            fprintf(stderr, "ERROR: Failed to receive data from internet\n");
            pthread_exit(NULL);
        }
        n = opus_decode(opus, opus_data, n, (opus_int16 *)pcm_data, N / 2, 0);
        if (n <= 0)
        {
            fprintf(stderr, "ERROR: Failed to decode: %s\n", opus_strerror(n));
            pthread_exit(NULL);
        }
        ret = pa_simple_write(pa, pcm_data, n, &pa_errno);
        if (ret < 0)
        {
            fprintf(stderr, "ERROR: Failed to write data to pulseaudio: %s\n", pa_strerror(pa_errno));
            pthread_exit(NULL);
        }
        pthread_testcancel();
    }

    pa_simple_free(pa);
    opus_decoder_destroy(opus);
    pthread_exit(NULL);
}
