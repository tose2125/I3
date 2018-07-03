#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <opus/opus.h>

#define N 2048

int main()
{
    int opus_errno;
    OpusDecoder *opus = opus_decoder_create(48000, 1, &opus_errno);
    if (opus == NULL)
    {
        fprintf(stderr, "ERROR: Failed to start opus decoder: %s\n", opus_strerror(opus_errno));
        exit(EXIT_FAILURE);
    }

    unsigned char in_opus_data[N + 1] = {0};
    opus_int16 pcm_data[N + 1] = {0};
    unsigned char out_pcm_data[2 * N + 1] = {0};

    while (1)
    {
        int n = read(STDIN_FILENO, in_opus_data, N);
        if (n <= 0)
        {
            fprintf(stderr, "ERROR: Failed to read data from stdin\n");
            opus_decoder_destroy(opus);
            exit(EXIT_FAILURE);
        }
        n = opus_decode(opus, in_opus_data, n, pcm_data, N, 0);
        if (n <= 0)
        {
            fprintf(stderr, "ERROR: Failed to decode: %s\n", opus_strerror(n));
            opus_decoder_destroy(opus);
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < n; i++)
        {
            out_pcm_data[i * 2] = pcm_data[i] & 0xFF;
            out_pcm_data[i * 2 + 1] = (pcm_data[i] >> 8) & 0xFF;
        }
        if (fwrite(out_pcm_data, sizeof(char), 2 * n, stdout) < 2 * n)
        {
            fprintf(stderr, "ERROR: Failed to write all data to stdout\n");
            opus_decoder_destroy(opus);
            exit(EXIT_FAILURE);
        }
        usleep(12288);
    }

    opus_decoder_destroy(opus);
    return EXIT_SUCCESS;
}
