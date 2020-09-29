#pragma once

#include <vk.h>
#include <memory>

namespace lumen
{
class Scene
{
public:
    using Ptr = std::shared_ptr<Scene>;

    friend class ResourceManager;

    struct Node
    {
        using Ptr = std::shared_ptr<Node>;
    };

public:
    Scene(vk::Backend::Ptr backend, Node::Ptr root);
    ~Scene();

private:
    vk::AccelerationStructure::Ptr m_tlas;
    vk::DescriptorSet::Ptr         m_ds;
    Node::Ptr                      m_root;
};
} // namespace lumen