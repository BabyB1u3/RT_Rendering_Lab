#pragma once

class Time
{
public:
    static void Reset();
    static void Update(double currentTimeSeconds);

    static double GetDeltaTime();
    static double GetTotalTime();

private:
    static double s_LastTime;
    static double s_TotalTime;
    static double s_DeltaTime;
    static bool s_Initialized;
};