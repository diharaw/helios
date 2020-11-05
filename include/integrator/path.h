#pragma once

#include <core/integrator.h>

namespace lumen
{
class PathIntegrator : public Integrator
{
public:
    PathIntegrator(vk::Backend::Ptr backend);
    ~PathIntegrator();

protected:
    void execute(vk::DescriptorSet::Ptr per_scene_ds, vk::DescriptorSet::Ptr per_frame_ds) override;
};
} // namespace lumen