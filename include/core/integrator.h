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
        glm::mat4  view_inverse;
        glm::mat4  proj_inverse;
        glm::uvec4 num_lights; // x: directional lights, y: point lights, z: spot lights, w: area lights
        glm::ivec2 ray_debug_pixel_coord;
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
    virtual void gather_debug_rays(const glm::ivec2& pixel_coord, const uint32_t& num_debug_rays, const glm::mat4& view, const glm::mat4& projection, RenderState& render_state);
};
} // namespace lumen