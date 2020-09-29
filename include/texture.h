#pragma once

#include <vk.h>
#include <memory>

namespace lumen
{
class Texture
{
public:
    using Ptr = std::shared_ptr<Texture>;

    friend class ResourceManager;

private:
    vk::Image::Ptr     m_image;
    vk::ImageView::Ptr m_image_view;

public:
    Texture(vk::Image::Ptr image, vk::ImageView::Ptr image_view);
    ~Texture();

    inline vk::Image::Ptr     image() { return m_image; }
    inline vk::ImageView::Ptr image_view() { return m_image_view; }
};
} // namespace lumen