#pragma once

#include <core/vk.h>

namespace lumen
{
class Renderer;

class Integrator
{
public:
    friend class Renderer;

protected:
    std::weak_ptr<vk::Backend> m_backend;

public:
    Integrator(vk::Backend::Ptr backend) :
        m_backend(backend) {}
    ~Integrator() {}

protected:
    virtual void execute(vk::DescriptorSet::Ptr read_image, vk::DescriptorSet::Ptr write_image, vk::DescriptorSet::Ptr per_scene_ds, vk::DescriptorSet::Ptr per_frame_ds) = 0;
};
} // namespace lumen