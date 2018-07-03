#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <opus/opus.h>

#define N 2048

int main()
{
    int opus_errno;
    OpusEncoder *opus = opus_encoder_create(48000, 1, OPUS_APPLICATION_AUDIO, &opus_errno);
    if (opus == NULL)
    {
        fprintf(stderr, "ERROR: Failed to start opus encoder: %s\n", opus_strerror(opus_errno));
        exit(EXIT_FAILURE);
    }

    unsigned char in_pcm_data[2 * N + 1] = {0};
    opus_int16 pcm_data[N + 1] = {0};
    unsigned char out_opus_data[N + 1] = {0};

    while (1)
    {
        int n = fread(in_pcm_data, sizeof(char), 1920, stdin); // 20ms
        if (n <= 0)
        {
            fprintf(stderr, "ERROR: Failed to read data from stdin\n");
            opus_encoder_destroy(opus);
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < n / 2; i++)
        {
            pcm_data[i] = in_pcm_data[i * 2] | in_pcm_data[i * 2 + 1] << 8;
        }
        n = opus_encode(opus, pcm_data, n / 2, out_opus_data, N);
        if (n < 0)
        {
            fprintf(stderr, "ERROR: Failed to encode: %s\n", opus_strerror(n));
            opus_encoder_destroy(opus);
            exit(EXIT_FAILURE);
        }
        else if (n <= 2)
        {
            continue;
        }
        if (write(STDOUT_FILENO, out_opus_data, n) < n)
        {
            fprintf(stderr, "ERROR: Failed to write all data to stdout\n");
            opus_encoder_destroy(opus);
            exit(EXIT_FAILURE);
        }
        usleep(16384);
    }

    opus_encoder_destroy(opus);
    return 0;
}
