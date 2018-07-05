#include "send_receive.h"

void *send_voice(void *arg)
{
    int net = *(int *)arg;
    setsockopt(net, IPPROTO_TCP, TCP_NODELAY, NULL, sizeof(NULL));

    int opus_errno;
    OpusEncoder *opus = opus_encoder_create(48000, 1, OPUS_APPLICATION_VOIP, &opus_errno);
    if (opus == NULL)
    {
        fprintf(stderr, "ERROR: Failed to start opus encoder: %s\n", opus_strerror(opus_errno));
        pthread_exit(NULL);
    }

    char *sox_command = "rec -t raw -b 16 -c 1 -e s -r 48000 -";
    FILE *sox = popen(sox_command, "r");
    if (sox == NULL)
    {
        fprintf(stderr, "ERROR: Failed to start record process: %s\n", sox_command);
        opus_encoder_destroy(opus);
        pthread_exit(NULL);
    }

    unsigned char in_pcm_data[2 * N + 1] = {0};
    opus_int16 pcm_data[N + 1] = {0};
    unsigned char out_opus_data[N + 1] = {0};

    while (1)
    {
        int n = fread(in_pcm_data, sizeof(char), 1920, sox); // 20ms
        if (n <= 0)
        {
            fprintf(stderr, "ERROR: Failed to read data from record process\n");
            break;
        }
        for (int i = 0; i < n / 2; i++)
        {
            pcm_data[i] = in_pcm_data[i * 2] | in_pcm_data[i * 2 + 1] << 8;
        }
        n = opus_encode(opus, pcm_data, n / 2, &out_opus_data[2], N - 2);
        if (n < 0)
        {
            fprintf(stderr, "ERROR: Failed to encode: %s\n", opus_strerror(n));
            break;
        }
        else if (n <= 2)
        {
            continue;
        }
        out_opus_data[0] = n & 0xFF;        // data length
        out_opus_data[1] = (n >> 8) & 0xFF; // little endian
        if (send(net, out_opus_data, n + 2, 0) < n + 2)
        {
            fprintf(stderr, "ERROR: Failed to send all data to internet\n");
            break;
        }
        pthread_testcancel();
    }

    pclose(sox);
    opus_encoder_destroy(opus);

    fprintf(stderr, "INFO: Stopped sending voice\n");
    pthread_exit(NULL);
}

void *receive_voice(void *arg)
{
    int net = *(int *)arg;

    int opus_errno;
    OpusDecoder *opus = opus_decoder_create(48000, 1, &opus_errno);
    if (opus == NULL)
    {
        fprintf(stderr, "ERROR: Failed to start opus decoder: %s\n", opus_strerror(opus_errno));
        pthread_exit(NULL);
    }

    char *sox_command = "sox -t raw -b 16 -c 1 -e s -r 48000 - test.wav";
    FILE *sox = popen(sox_command, "w");
    if (sox == NULL)
    {
        fprintf(stderr, "ERROR: Failed to start play process: %s\n", sox_command);
        opus_decoder_destroy(opus);
        pthread_exit(NULL);
    }

    unsigned char in_opus_data[N + 1] = {0};
    opus_int16 pcm_data[N + 1] = {0};
    char out_pcm_data[2 * N + 1] = {0};

    while (1)
    {
        int n = recv(net, in_opus_data, 2, 0);
        if (n < 2)
        {
            fprintf(stderr, "ERROR: Failed to receive data from internet: %d / 2\n", n);
            break;
        }
        size_t len = in_opus_data[0] | in_opus_data[1] << 8;
        if (len > N)
        {
            fprintf(stderr, "ERROR: Incoming data longer than buffer: %d\n", (unsigned short)len);
            break;
        }
        for (int i = 0; i < len; i += n)
        {
            n = recv(net, in_opus_data + i, len - i, 0);
            if (n <= 0)
            {
                fprintf(stderr, "ERROR: Failed to receive data from internet: %d byte / %zu byte\n", n, len - i);
                break;
            }
        }
        n = opus_decode(opus, in_opus_data, n, pcm_data, N, 0);
        if (n <= 0)
        {
            fprintf(stderr, "ERROR: Failed to decode: %s\n", opus_strerror(n));
            break;
        }
        for (int i = 0; i < n; i++)
        {
            out_pcm_data[i * 2] = pcm_data[i] & 0xFF;
            out_pcm_data[i * 2 + 1] = (pcm_data[i] >> 8) & 0xFF;
        }
        if (fwrite(out_pcm_data, sizeof(char), 2 * n, sox) < 2 * n)
        {
            fprintf(stderr, "ERROR: Failed to write all data to play process\n");
            break;
        }
        pthread_testcancel();
    }

    pclose(sox);
    opus_decoder_destroy(opus);

    fprintf(stderr, "INFO: Stopped receiving voice\n");
    pthread_exit(NULL);
}
