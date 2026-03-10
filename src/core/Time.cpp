#include "Time.h"

double Time::s_LastTime = 0.0;
double Time::s_TotalTime = 0.0;
float Time::s_DeltaTime = 0.0f;
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
        s_TotalTime = currentTimeSeconds;
        s_DeltaTime = 0.0f;
        s_Initialized = true;
        return;
    }

    s_DeltaTime = static_cast<float>(currentTimeSeconds - s_LastTime);
    s_LastTime = currentTimeSeconds;
    s_TotalTime = currentTimeSeconds;
}

float Time::GetDeltaTime()
{
    return s_DeltaTime;
}

float Time::GetTotalTime()
{
    return static_cast<float>(s_TotalTime);
}
