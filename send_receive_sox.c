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

    char *sox_command = "rec -t raw -b 16 -c 1 -e s -r 48000 -";
    FILE *sox = popen(sox_command, "r");
    if (sox == NULL)
    {
        fprintf(stderr, "ERROR: Failed to start record process: %s\n", sox_command);
        pthread_exit(NULL);
    }

    char pcm_data[N + 1] = {0};
    unsigned char opus_data[N + 1] = {0};

    while (1)
    {
        int n = fread(pcm_data, sizeof(char), 1920, sox); // 20ms
        if (n <= 0)
        {
            fprintf(stderr, "ERROR: Failed to read data from record process\n");
            pthread_exit(NULL);
        }
        n = opus_encode(opus, (opus_int16 *)pcm_data, n / 2, opus_data, N);
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

    pclose(sox);
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

    char *sox_command = "sox -t raw -b 16 -c 1 -e s -r 48000 - test.wav";
    FILE *sox = popen(sox_command, "w");
    if (sox == NULL)
    {
        fprintf(stderr, "ERROR: Failed to start play process: %s\n", sox_command);
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
        if (fwrite(pcm_data, sizeof(char), n, sox) < n)
        {
            fprintf(stderr, "ERROR: Failed to write all data to play process\n");
            pthread_exit(NULL);
        }
        pthread_testcancel();
    }

    pclose(sox);
    opus_decoder_destroy(opus);
    pthread_exit(NULL);
}
