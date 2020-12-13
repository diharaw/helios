#include <utility/utility.h>
#include <utility/logger.h>

#include <fstream>
#include <iostream>
#include <algorithm>

#ifdef WIN32
#    include <Windows.h>
#    include <direct.h>
#    define GetCurrentDir _getcwd
#    define ChangeWorkingDir _chdir
#else
#    include <unistd.h>
#    define GetCurrentDir getcwd
#    define ChangeWorkingDir chdir
#endif

#ifdef __APPLE__
#    include <mach-o/dyld.h>
#endif

namespace helios
{
namespace utility
{
static std::string g_exe_path = "";

// -----------------------------------------------------------------------------------------------------------------------------------

std::string path_for_resource(const std::string& resource)
{
    std::string exe_path = executable_path();
#ifdef __APPLE__
    return exe_path + "/Contents/Resources/" + resource;
#else
    return exe_path + "/" + resource;
#endif
}

// -----------------------------------------------------------------------------------------------------------------------------------

#ifdef WIN32
std::string executable_path()
{
    if (g_exe_path == "")
    {
        char buffer[1024];
        GetModuleFileName(NULL, &buffer[0], 1024);
        g_exe_path = buffer;
        g_exe_path = path_without_file(g_exe_path);
    }

    return g_exe_path;
}
#elif __APPLE__
std::string executable_path()
{
    if (g_exe_path == "")
    {
        char path[1024];
        uint32_t size = sizeof(path);
        if (_NSGetExecutablePath(path, &size) == 0)
        {
            g_exe_path = path;

            // Substring three times to get back to root path.
            for (int i = 0; i < 3; i++)
            {
                std::size_t found = g_exe_path.find_last_of("/");
                g_exe_path = g_exe_path.substr(0, found);
            }
        }
    }

    return g_exe_path;
}
#else
std::string executable_path()
{
    if (g_exe_path == "")
    {
    }

    return g_exe_path;
}
#endif

// -----------------------------------------------------------------------------------------------------------------------------------

std::string current_working_directory()
{
    char buffer[FILENAME_MAX];

    if (!GetCurrentDir(buffer, sizeof(buffer)))
        return "";

    buffer[sizeof(buffer) - 1] = '\0';
    return std::string(buffer);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void change_current_working_directory(std::string path)
{
    ChangeWorkingDir(path.c_str());
}

// -----------------------------------------------------------------------------------------------------------------------------------

std::string path_without_file(std::string filepath)
{
#ifdef WIN32
    std::replace(filepath.begin(), filepath.end(), '\\', '/');
#endif
    std::size_t found = filepath.find_last_of("/\\");
    std::string path  = filepath.substr(0, found);
    return path;
}

// -----------------------------------------------------------------------------------------------------------------------------------

std::string file_extension(std::string filepath)
{
    std::size_t found = filepath.find_last_of(".");
    std::string ext   = filepath.substr(found, filepath.size());
    return ext;
}

// -----------------------------------------------------------------------------------------------------------------------------------

std::string file_name_from_path(std::string filepath)
{
    std::size_t slash = filepath.find_last_of("/");

    if (slash == std::string::npos)
        slash = 0;
    else
        slash++;

    std::size_t dot      = filepath.find_last_of(".");
    std::string filename = filepath.substr(slash, dot - slash);

    return filename;
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace utility
} // namespace helios