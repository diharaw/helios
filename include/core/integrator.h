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
    std::weak_ptr<Renderer> m_renderer;

public:
    Integrator(vk::Backend::Ptr backend, std::weak_ptr<Renderer> renderer) :
        m_backend(backend), m_renderer(renderer) {}
    ~Integrator() {}

protected:
    virtual void execute(vk::DescriptorSet::Ptr per_scene_ds, vk::DescriptorSet::Ptr per_frame_ds) = 0;
    virtual void on_window_resized() = 0;
};
} // namespace lumen