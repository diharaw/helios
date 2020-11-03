#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <cassert>
#include <algorithm>
#include <stdio.h>

namespace lumen
{
namespace utility
{
// Returns the absolute path to the resource. It also resolves the path to the 'Resources' directory is macOS app bundles.
extern std::string path_for_resource(const std::string& resource);

// Returns the absolute path of the executable.
extern std::string executable_path();

// Removes the filename from a file path.
extern std::string path_without_file(std::string filepath);

// Returns the extension of a given file.
extern std::string file_extension(std::string filepath);

// Returns the file name from a given path.
extern std::string file_name_from_path(std::string filepath);

// Queries the current working directory.
extern std::string current_working_directory();

// Changes the current working directory.
extern void change_current_working_directory(std::string path);
} // namespace utility
} // namespace lumen
