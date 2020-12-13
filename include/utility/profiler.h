#pragma once

#include <memory>
#include <string>
#include <gfx/vk.h>

#define HELIOS_SCOPED_SAMPLE(name) helios::profiler::ScopedProfile __FILE__##__LINE__(name)


namespace helios
{
namespace profiler
{
struct ScopedProfile
{
    ScopedProfile(std::string name);
    ~ScopedProfile();

    std::string m_name;
};

extern void initialize(vk::Backend::Ptr backend);
extern void shutdown();
extern void begin_sample(std::string name);
extern void end_sample(std::string name);
extern void begin_frame(vk::CommandBuffer::Ptr cmd_buf);
extern void end_frame();
extern void ui();
}; // namespace profiler
} // namespace helios