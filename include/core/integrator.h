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

    struct PushConstants
    {
        glm::uvec4 num_lights; // x: directional lights, y: point lights, z: spot lights, w: area lights
        float      accumulation;
        uint32_t   num_frames;
    };

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