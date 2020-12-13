#include <utility/logger.h>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>

#define FILE_STREAM_INDEX 0
#define CONSOLE_STREAM_INDEX 1
#define CUSTOM_STREAM_INDEX 2

#define LOG_SEPERATOR                                                            \
    "**************************************************************************" \
    "******************************\n"

namespace helios
{
namespace logger
{
struct LoggerState
{
    bool                 _open_streams[3];
    std::mutex           _log_mutex;
    std::ofstream        _stream;
    std::time_t          _rawtime;
    std::tm*             _timeinfo;
    int                  _verbosity;
    char                 _temp_buffer[80];
    CustomStreamCallback _callback;
    bool                 _debug;
};

LoggerState g_logger;

void initialize()
{
    for (int i = 0; i < 3; i++)
        g_logger._open_streams[i] = false;

    g_logger._callback  = nullptr;
    g_logger._verbosity = VERBOSITY_ALL;
    g_logger._debug     = false;
}

void set_verbosity(int flags) { g_logger._verbosity = flags; }

void open_console_stream()
{
    g_logger._open_streams[CONSOLE_STREAM_INDEX] = true;

    std::time(&g_logger._rawtime);
    std::cout << LOG_SEPERATOR;
    std::cout << std::ctime(&g_logger._rawtime) << "Log Started.\n";
    std::cout << LOG_SEPERATOR;
}

void open_file_stream()
{
    g_logger._open_streams[FILE_STREAM_INDEX] = true;
    g_logger._stream.open("log.txt", std::ios::app | std::ofstream::out);

    std::time(&g_logger._rawtime);
    g_logger._stream << LOG_SEPERATOR;
    g_logger._stream << std::ctime(&g_logger._rawtime) << "Log Started.\n";
    g_logger._stream << LOG_SEPERATOR;
}

void open_custom_stream(CustomStreamCallback callback)
{
    g_logger._open_streams[CUSTOM_STREAM_INDEX] = true;
    g_logger._callback                          = callback;

    std::time(&g_logger._rawtime);

    if (g_logger._callback)
    {
        g_logger._callback(LOG_SEPERATOR, LEVEL_INFO);

        std::string init_string = std::ctime(&g_logger._rawtime);
        init_string += "Log Started.\n";

        g_logger._callback(init_string, LEVEL_INFO);
        g_logger._callback(LOG_SEPERATOR, LEVEL_INFO);
    }
}

void close_console_stream()
{
    g_logger._open_streams[CONSOLE_STREAM_INDEX] = false;

    std::time(&g_logger._rawtime);
    std::cout << LOG_SEPERATOR;
    std::cout << std::ctime(&g_logger._rawtime) << "Log Ended.\n";
    std::cout << LOG_SEPERATOR;
}

void close_file_stream()
{
    g_logger._open_streams[FILE_STREAM_INDEX] = false;

    std::time(&g_logger._rawtime);
    g_logger._stream << LOG_SEPERATOR;
    g_logger._stream << std::ctime(&g_logger._rawtime) << "Log Ended.\n";
    g_logger._stream << LOG_SEPERATOR;

    g_logger._stream.close();
}

void close_custom_stream()
{
    g_logger._open_streams[CUSTOM_STREAM_INDEX] = false;

    std::time(&g_logger._rawtime);

    if (g_logger._callback)
    {
        g_logger._callback(LOG_SEPERATOR, LEVEL_INFO);

        std::string init_string = std::ctime(&g_logger._rawtime);
        init_string += "Log Ended.\n";

        g_logger._callback(init_string, LEVEL_INFO);
        g_logger._callback(LOG_SEPERATOR, LEVEL_INFO);
    }
}

void enable_debug_mode() { g_logger._debug = true; }

void disable_debug_mode() { g_logger._debug = false; }

void log(std::string text, std::string file, int line, LogLevel level)
{
    std::lock_guard<std::mutex> lock(g_logger._log_mutex);

    std::string file_with_extension = file.substr(file.find_last_of("/\\") + 1);
    std::time(&g_logger._rawtime);
    g_logger._timeinfo = std::localtime(&g_logger._rawtime);
    std::strftime(g_logger._temp_buffer, 80, "%H:%M:%S", g_logger._timeinfo);

    std::string log_level_string;

    switch (level)
    {
        case LEVEL_INFO:
        {
            log_level_string = "INFO   ";
            break;
        }
        case LEVEL_WARNING:
        {
            log_level_string = "WARNING";
            break;
        }
        case LEVEL_ERR:
        {
            log_level_string = "ERROR  ";
            break;
        }
        case LEVEL_FATAL:
        {
            log_level_string = "FATAL  ";
            break;
        }
    }

    std::string output;

    if ((g_logger._verbosity & VERBOSITY_TIMESTAMP) || (g_logger._verbosity & VERBOSITY_LEVEL))
    {
        output = "[ ";

        if (g_logger._verbosity & VERBOSITY_TIMESTAMP)
            output += g_logger._temp_buffer;

        if ((g_logger._verbosity & VERBOSITY_TIMESTAMP) && (g_logger._verbosity & VERBOSITY_LEVEL))
            output += " | ";

        if (g_logger._verbosity & VERBOSITY_LEVEL)
            output += log_level_string;

        output += " ] : ";
    }

    output += text;

    if (g_logger._verbosity & VERBOSITY_FILE)
    {
        output += " , FILE : ";
        output += file_with_extension;
    }

    if (g_logger._verbosity & VERBOSITY_LINE)
    {
        output += " , LINE : ";
        output += std::to_string(line);
    }

    if (g_logger._open_streams[FILE_STREAM_INDEX])
    {
        g_logger._stream << output << "\n";
    }

    if (g_logger._open_streams[CONSOLE_STREAM_INDEX])
    {
        std::cout << output << "\n";
    }

    if (g_logger._open_streams[CUSTOM_STREAM_INDEX] && g_logger._callback)
    {
        g_logger._callback(output, level);
    }

    // Flush stream if error
    if (level == LEVEL_ERR || level == LEVEL_FATAL || g_logger._debug)
        flush();
}

void log_simple(std::string text, LogLevel level)
{
    std::lock_guard<std::mutex> lock(g_logger._log_mutex);

    std::time(&g_logger._rawtime);
    g_logger._timeinfo = std::localtime(&g_logger._rawtime);
    std::strftime(g_logger._temp_buffer, 80, "%H:%M:%S", g_logger._timeinfo);

    std::string log_level_string;

    switch (level)
    {
        case LEVEL_INFO:
        {
            log_level_string = "INFO   ";
            break;
        }
        case LEVEL_WARNING:
        {
            log_level_string = "WARNING";
            break;
        }
        case LEVEL_ERR:
        {
            log_level_string = "ERROR  ";
            break;
        }
        case LEVEL_FATAL:
        {
            log_level_string = "FATAL  ";
            break;
        }
    }

    std::string output;

    if ((g_logger._verbosity & VERBOSITY_TIMESTAMP) || (g_logger._verbosity & VERBOSITY_LEVEL))
    {
        output = "[ ";

        if (g_logger._verbosity & VERBOSITY_TIMESTAMP)
            output += g_logger._temp_buffer;

        if ((g_logger._verbosity & VERBOSITY_TIMESTAMP) && (g_logger._verbosity & VERBOSITY_LEVEL))
            output += " | ";

        if (g_logger._verbosity & VERBOSITY_LEVEL)
            output += log_level_string;

        output += " ] : ";
    }

    output += text;

    if (g_logger._open_streams[FILE_STREAM_INDEX])
    {
        g_logger._stream << output << "\n";
    }

    if (g_logger._open_streams[CONSOLE_STREAM_INDEX])
    {
        std::cout << output << "\n";
    }

    if (g_logger._open_streams[CUSTOM_STREAM_INDEX] && g_logger._callback)
    {
        g_logger._callback(output, level);
    }
}

void log_info(std::string text) { log_simple(text, LEVEL_INFO); }

void log_error(std::string text) { log_simple(text, LEVEL_ERR); }

void log_warning(std::string text) { log_simple(text, LEVEL_WARNING); }

void log_fatal(std::string text) { log_simple(text, LEVEL_FATAL); }

void flush()
{
    if (g_logger._open_streams[FILE_STREAM_INDEX])
    {
        g_logger._stream.flush();
    }
}
} // namespace logger
} // namespace helios
