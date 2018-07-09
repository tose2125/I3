#include <pulse/simple.h> // pulseaudio
#include <pulse/error.h>  // pulseaudio
#include "send_receive.h"

void *send_voice(void *arg)
{
    int ret;

    struct send_receive settings = *(struct send_receive *)arg;
    int net = settings.sock;
    // setsockopt(net, IPPROTO_TCP, TCP_NODELAY, NULL, sizeof(NULL));

    int opus_errno;
    OpusEncoder *opus = opus_encoder_create(48000, 1, OPUS_APPLICATION_VOIP, &opus_errno);
    if (opus == NULL)
    {
        fprintf(stderr, "ERROR: Failed to start opus encoder: %s\n", opus_strerror(opus_errno));
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
        opus_encoder_destroy(opus);
        pthread_exit(NULL);
    }

    unsigned char in_pcm_data[2 * N + 1] = {0};
    opus_int16 pcm_data[N + 1] = {0};
    unsigned char out_opus_data[N + 1] = {0};

    unsigned long long data_counter = 0;
    unsigned long c = 0;
    while (++c > 0)
    {
        ret = pa_simple_read(pa, in_pcm_data, 1920, &pa_errno); // 20ms
        if (ret < 0)
        {
            fprintf(stderr, "ERROR: Failed to read data from pulseaudio: %s\n", pa_strerror(pa_errno));
            break;
        }
        for (int i = 0; i < 960; i++)
        {
            pcm_data[i] = in_pcm_data[i * 2] | in_pcm_data[i * 2 + 1] << 8;
        }
        int n = opus_encode(opus, pcm_data, 960, &out_opus_data[2], N - 2);
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
        data_counter += n + 2;
        // Print counter
        if (pthread_mutex_trylock(&settings.locker) == 0)
        {
            if (*settings.print & 1)
            {
                printf("INFO: Send counter: %lu times, %llu byte, compression %.2lf%%\n", c, data_counter, 100.0 * data_counter / (c * 1920));
                *settings.print -= 1;
            }
            pthread_mutex_unlock(&settings.locker);
        }
        pthread_testcancel();
    }

    pa_simple_free(pa);
    opus_encoder_destroy(opus);

    fprintf(stderr, "INFO: Stopped sending voice\n");
    pthread_exit(NULL);
}

void *receive_voice(void *arg)
{
    int ret;

    struct send_receive settings = *(struct send_receive *)arg;
    int net = settings.sock;

    int opus_errno;
    OpusDecoder *opus = opus_decoder_create(48000, 1, &opus_errno);
    if (opus == NULL)
    {
        fprintf(stderr, "ERROR: Failed to start opus decoder: %s\n", opus_strerror(opus_errno));
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
        opus_decoder_destroy(opus);
        pthread_exit(NULL);
    }

    unsigned char in_opus_data[N + 1] = {0};
    opus_int16 pcm_data[N + 1] = {0};
    unsigned char out_pcm_data[2 * N + 1] = {0};

    unsigned long long data_counter = 0;
    unsigned long c = 0;
    while (++c > 0)
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
            fprintf(stderr, "ERROR: Incoming data longer than buffer: %zu > %d\n", len, N);
            break;
        }
        for (int i = 0; i < len; i += n)
        {
            n = recv(net, in_opus_data + i, len - i, 0);
            if (n <= 0)
            {
                fprintf(stderr, "ERROR: Failed to receive data from internet: %d / %zu\n", n, len - i);
                break;
            }
        }
        data_counter += len + 2;
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
        ret = pa_simple_write(pa, out_pcm_data, 2 * n, &pa_errno);
        if (ret < 0)
        {
            fprintf(stderr, "ERROR: Failed to write data to pulseaudio: %s\n", pa_strerror(pa_errno));
            break;
        }
        // Print counter
        if (pthread_mutex_trylock(&settings.locker) == 0)
        {
            if (*settings.print & 2)
            {
                printf("INFO: Receive counter: %lu times, %llu byte, compression %.2lf%%\n", c, data_counter, 100.0 * data_counter / (c * 1920));
                *settings.print -= 2;
            }
            pthread_mutex_unlock(&settings.locker);
        }
        pthread_testcancel();
    }

    pa_simple_free(pa);
    opus_decoder_destroy(opus);

    fprintf(stderr, "INFO: Stopped receiving voice\n");
    pthread_exit(NULL);
}
