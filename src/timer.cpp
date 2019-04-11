#include "timer.h"

namespace lumen
{
Timer::Timer()
{
#ifdef WIN32
    QueryPerformanceFrequency(&_frequency);
    _start_count.QuadPart = 0;
    _end_count.QuadPart   = 0;
#else
    _start_count.tv_sec = _start_count.tv_usec = 0;
    _end_count.tv_sec = _end_count.tv_usec = 0;
#endif

    _stopped             = 0;
    _start_time_microsec = 0;
    _end_time_microsec   = 0;
}

Timer::~Timer()
{
}

void Timer::start()
{
    _stopped = 0;
#ifdef WIN32
    QueryPerformanceCounter(&_start_count);
#else
    gettimeofday(&_start_count, nullptr);
#endif
}

void Timer::stop()
{
    _stopped = 1;
#ifdef WIN32
    QueryPerformanceCounter(&_end_count);
#else
    gettimeofday(&_end_count, nullptr);
#endif
}

double Timer::elapsed_time()
{
    return elapsed_time_sec();
}

double Timer::elapsed_time_sec()
{
    return elapsed_time_microsec() * 0.000001;
}

double Timer::elapsed_time_milisec()
{
    return elapsed_time_microsec() * 0.001;
}

double Timer::elapsed_time_microsec()
{
#ifdef WIN32
    if (!_stopped)
        QueryPerformanceCounter(&_end_count);

    _start_time_microsec = _start_count.QuadPart * (1000000.0 / _frequency.QuadPart);
    _end_time_microsec   = _end_count.QuadPart * (1000000.0 / _frequency.QuadPart);
#else
    if (!_stopped)
        gettimeofday(&_end_count, nullptr);

    _start_time_microsec = (_start_count.tv_sec * 1000000.0) + _start_count.tv_usec;
    _end_time_microsec   = (_end_count.tv_sec * 1000000.0) + _end_count.tv_usec;
#endif

    return _end_time_microsec - _start_time_microsec;
}
}