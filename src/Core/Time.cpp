#include "Time.h"

double Time::s_LastTime = 0.0;
double Time::s_TotalTime = 0.0;
double Time::s_DeltaTime = 0.0;
bool Time::s_Initialized = false;

void Time::Reset()
{
    s_LastTime = 0.0;
    s_TotalTime = 0.0;
    s_DeltaTime = 0.0f;
    s_Initialized = false;
}

void Time::Update(double currentTimeSeconds)
{
    if (!s_Initialized)
    {
        s_LastTime = currentTimeSeconds;
        s_TotalTime = 0.0;
        s_DeltaTime = 0.0;
        s_Initialized = true;
        return;
    }

    s_DeltaTime = currentTimeSeconds - s_LastTime;
    s_LastTime = currentTimeSeconds;
    s_TotalTime += s_DeltaTime;
}

double Time::GetDeltaTime()
{
    return s_DeltaTime;
}

double Time::GetTotalTime()
{
    return s_TotalTime;
}
