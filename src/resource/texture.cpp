#include <resource/texture.h>

namespace lumen
{
// -----------------------------------------------------------------------------------------------------------------------------------

static uint32_t g_last_texture_id = 0;

// -----------------------------------------------------------------------------------------------------------------------------------

Texture::Texture(vk::Backend::Ptr backend, vk::Image::Ptr image, vk::ImageView::Ptr image_view) :
    vk::Object(backend), m_image(image), m_image_view(image_view), m_id(g_last_texture_id++)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Texture::~Texture()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Texture2D::Ptr Texture2D::create(vk::Backend::Ptr backend, vk::Image::Ptr image, vk::ImageView::Ptr image_view)
{
    return std::shared_ptr<Texture2D>(new Texture2D(backend, image, image_view));
}

// -----------------------------------------------------------------------------------------------------------------------------------

Texture2D::Texture2D(vk::Backend::Ptr backend, vk::Image::Ptr image, vk::ImageView::Ptr image_view) :
    Texture(backend, image, image_view)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Texture2D::~Texture2D()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

TextureCube::Ptr TextureCube::create(vk::Backend::Ptr backend, vk::Image::Ptr image, vk::ImageView::Ptr image_view)
{
    return std::shared_ptr<TextureCube>(new TextureCube(backend, image, image_view));
}

// -----------------------------------------------------------------------------------------------------------------------------------

TextureCube::TextureCube(vk::Backend::Ptr backend, vk::Image::Ptr image, vk::ImageView::Ptr image_view) :
    Texture(backend, image, image_view)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

TextureCube::~TextureCube()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace lumen