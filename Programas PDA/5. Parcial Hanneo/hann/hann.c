#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int window_size = 1024;
int sample_rate = 48000;

double *hann(double *buffer)
{
    double buffer1[window_size * 2];

    for (int i = 0; i < window_size * 2; i++)
    {
        buffer1[i] = 0.5 * (1 - cos((2 * 3.1415) * ((double)i * ((double)window_size / (double)sample_rate)))) * buffer[i];
    }
    return buffer1;
}

int main(int argc, char const *argv[])
{
    double buffer[window_size * 2];
    for (int i = 0; i < window_size * 2; i++)
    {
        buffer[i] = (double)i * ((double)window_size / (double)sample_rate);
        printf("%f\n", buffer[i]);
    }

    return 0;
}
