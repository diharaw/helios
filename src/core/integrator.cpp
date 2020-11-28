#include <core/integrator.h>
#include <utility/logger.h>

namespace lumen
{
void Integrator::gather_debug_rays(const glm::ivec2& pixel_coord, const uint32_t& num_debug_rays, const glm::mat4& view, const glm::mat4& projection, RenderState& render_state)
{
    LUMEN_LOG_ERROR("gather_debug_rays() has not been implemented for this Integrator.");
}
} // namespace lumen