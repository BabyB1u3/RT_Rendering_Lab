#include "Noise.h"

Noise::Noise(float p, int o)
{
    this->persistence = p;
    this->octave = o;
}

double Noise::PerlinNoise(float x, float y)
{
    double noise = 0;
    for (int i = 0; i < octave; i++)
    {
        double frequency = pow(2, i);
        double amplitude = pow(persistence, i);
        noise += InterpolatedNoise(x * frequency, y * frequency) * amplitude;
    }

    return noise;
}

double Noise::PrimeNoise(int x, int y)
{
    int n = x + y * 57;
    n = (n << 13) ^ n;
    return (1.0 - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0);
}

double Noise::SmoothedNoise(int x, int y)
{
    double corners = (PrimeNoise(x - 1, y - 1) + PrimeNoise(x + 1, y - 1) + PrimeNoise(x - 1, y + 1) + PrimeNoise(x + 1, y + 1)) / 16;
    double sides = (PrimeNoise(x - 1, y) + PrimeNoise(x + 1, y) + PrimeNoise(x, y - 1) + PrimeNoise(x, y + 1)) / 8;
    double center = PrimeNoise(x, y) / 4;
    return corners + sides + center;
}

double Noise::Cosine_Interpolate(double a, double b, double x)
{
    double ft = x * 3.1415927;
    double f = (1 - cos(ft)) * 0.5;
    return a * (1 - f) + b * f;
}

double Noise::InterpolatedNoise(float x, float y)
{
    int integer_X = int(x);
    float  fractional_X = x - integer_X;
    int integer_Y = int(y);
    float fractional_Y = y - integer_Y;
    double v1 = SmoothedNoise(integer_X, integer_Y);
    double v2 = SmoothedNoise(integer_X + 1, integer_Y);
    double v3 = SmoothedNoise(integer_X, integer_Y + 1);
    double v4 = SmoothedNoise(integer_X + 1, integer_Y + 1);
    double i1 = Cosine_Interpolate(v1, v2, fractional_X);
    double i2 = Cosine_Interpolate(v3, v4, fractional_X);
    return Cosine_Interpolate(i1, i2, fractional_Y);
}