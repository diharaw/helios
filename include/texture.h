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

protected:
    vk::Image::Ptr     m_image;
    vk::ImageView::Ptr m_image_view;
    uint32_t           m_id;

public:
    Texture(vk::Image::Ptr image, vk::ImageView::Ptr image_view);
    ~Texture();

    inline vk::Image::Ptr     image() { return m_image; }
    inline vk::ImageView::Ptr image_view() { return m_image_view; }
    inline uint32_t           id() { return m_id; }
};

class Texture2D : public Texture
{
public:
    using Ptr = std::shared_ptr<Texture2D>;

    friend class ResourceManager;

public:
    static Texture2D::Ptr create(vk::Image::Ptr image, vk::ImageView::Ptr image_view);
    ~Texture2D();

private:
    Texture2D(vk::Image::Ptr image, vk::ImageView::Ptr image_view);
};

class TextureCube : public Texture
{
public:
    using Ptr = std::shared_ptr<TextureCube>;

    friend class ResourceManager;

public:
    static TextureCube::Ptr create(vk::Image::Ptr image, vk::ImageView::Ptr image_view);
    ~TextureCube();

private:
    TextureCube(vk::Image::Ptr image, vk::ImageView::Ptr image_view);
};
} // namespace lumen