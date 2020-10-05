#pragma once

#include <vk.h>
#include <memory>

namespace lumen
{
class Texture
{
protected:
    vk::Image::Ptr     m_image;
    vk::ImageView::Ptr m_image_view;

public:
    Texture(vk::Image::Ptr image, vk::ImageView::Ptr image_view);
    ~Texture();

    inline vk::Image::Ptr     image() { return m_image; }
    inline vk::ImageView::Ptr image_view() { return m_image_view; }
};

class Texture2D : public Texture
{
public:
    using Ptr = std::shared_ptr<Texture2D>;

    friend class ResourceManager;

public:
    Texture2D(vk::Image::Ptr image, vk::ImageView::Ptr image_view);
    ~Texture2D();
};

class TextureCube : public Texture
{
public:
    using Ptr = std::shared_ptr<TextureCube>;

    friend class ResourceManager;

public:
    TextureCube(vk::Image::Ptr image, vk::ImageView::Ptr image_view);
    ~TextureCube();
};
} // namespace lumen