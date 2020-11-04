#pragma once

#include <core/vk.h>

namespace lumen
{
class Integrator
{
public:
    friend class Renderer;

private:
    std::weak_ptr<vk::Backend> m_backend;

public:
    Integrator(vk::Backend::Ptr backend) : m_backend(backend) {}
    ~Integrator() {}

private:
    virtual void execute(vk::DescriptorSet::Ptr per_scene_ds, vk::DescriptorSet::Ptr per_frame_ds) = 0;
};
} // namespace lumen