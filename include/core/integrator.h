#pragma once

#include <core/vk.h>
#include <resource/scene.h>

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
    virtual void execute(RenderState& render_state) = 0;
};
} // namespace lumen