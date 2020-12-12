#pragma once

#include <gfx/vk.h>
#include <memory>

namespace helios
{
class Texture : public vk::Object
{
public:
    using Ptr = std::shared_ptr<Texture>;

    friend class ResourceManager;

protected:
    vk::Image::Ptr     m_image;
    vk::ImageView::Ptr m_image_view;
    std::string        m_path;
    uint32_t           m_id;

public:
    Texture(vk::Backend::Ptr backend, vk::Image::Ptr image, vk::ImageView::Ptr image_view, const std::string& path);
    virtual ~Texture();

    inline vk::Image::Ptr     image() { return m_image; }
    inline vk::ImageView::Ptr image_view() { return m_image_view; }
    inline uint32_t           id() { return m_id; }
    inline std::string        path() { return m_path; }
};

class Texture2D : public Texture
{
public:
    using Ptr = std::shared_ptr<Texture2D>;

    friend class ResourceManager;

public:
    static Texture2D::Ptr create(vk::Backend::Ptr backend, vk::Image::Ptr image, vk::ImageView::Ptr image_view, const std::string& path);
    ~Texture2D();

private:
    Texture2D(vk::Backend::Ptr backend, vk::Image::Ptr image, vk::ImageView::Ptr image_view, const std::string& path);
};

class TextureCube : public Texture
{
public:
    using Ptr = std::shared_ptr<TextureCube>;

    friend class ResourceManager;

public:
    static TextureCube::Ptr create(vk::Backend::Ptr backend, vk::Image::Ptr image, vk::ImageView::Ptr image_view, const std::string& path);
    ~TextureCube();

private:
    TextureCube(vk::Backend::Ptr backend, vk::Image::Ptr image, vk::ImageView::Ptr image_view, const std::string& path);
};
} // namespace helios