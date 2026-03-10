#pragma once

class Time
{
public:
    static void Reset();
    static void Update(double currentTimeSeconds);

    static float GetDeltaTime();
    static float GetTotalTime();

private:
    static double s_LastTime;
    static double s_TotalTime;
    static float s_DeltaTime;
    static bool s_Initialized;
};