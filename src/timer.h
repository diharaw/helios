#pragma once

#if defined(WIN32)
#    define NOMINMAX
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#else
#    include <sys/time.h>
#endif

namespace lumen
{
class Timer
{
public:
    Timer();
    ~Timer();

    void   start();
    void   stop();
    double elapsed_time();
    double elapsed_time_sec();
    double elapsed_time_milisec();
    double elapsed_time_microsec();

private:
    double _start_time_microsec;
    double _end_time_microsec;
    int    _stopped;
#ifdef WIN32
    LARGE_INTEGER _frequency;
    LARGE_INTEGER _start_count;
    LARGE_INTEGER _end_count;
#else
    timeval _start_count;
    timeval _end_count;
#endif
};
} // namespace lumen