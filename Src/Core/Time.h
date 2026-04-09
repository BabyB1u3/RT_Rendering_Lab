#pragma once

/// @file Time.h
/// @brief Global frame-time tracking.
///
/// Time does not query the clock itself - the Application main loop calls
/// Update(glfwGetTime()) each frame, keeping the time source explicit and
/// easy to replace for testing or fixed-timestep modes.
///
/// The first call to Update() after Reset() records the baseline and reports
/// a delta of 0 so that the first frame never sees a huge dt.

class Time
{
public:
    /// Clear all accumulators. Called once at application startup.
    static void Reset();
    /// Advance the clock. Must be called exactly once per frame with the
    /// current wall-clock time in seconds (e.g., glfwGetTime()).
    static void Update(double currentTimeSeconds);

    /// Seconds elapsed between the two most recent Update() calls.
    static double GetDeltaTime();
    /// Cumulative seconds since the first Update() after Reset().
    static double GetTotalTime();

private:
    static double s_LastTime;
    static double s_TotalTime;
    static double s_DeltaTime;
    static bool s_Initialized;
};