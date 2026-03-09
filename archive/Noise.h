#ifndef NOISE_H
#define NOISE_H
#include <cmath>

class Noise 
{
public:

    float persistence;
    int octave;

    Noise(float p, int o);

    double PerlinNoise(float x, float y);    // 最终调用：根据(x,y)获得其对应的PerlinNoise值


private:

    double PrimeNoise(int x, int y);    // 根据(x,y)获取一个初步噪声值

    double SmoothedNoise(int x, int y);   //光滑噪声

    double Cosine_Interpolate(double a, double b, double x);  // 余弦插值

    double InterpolatedNoise(float x, float y);   // 获取插值噪声

};
#endif // NOISE_H
