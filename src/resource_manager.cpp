#include <resource_manager.h>
#include <loader/loader.h>
#include <logger.h>
#include <utility.h>
#include <vk_mem_alloc.h>

namespace lumen
{
// -----------------------------------------------------------------------------------------------------------------------------------

const VkFormat kCompressedFormats[][2] = {
    { VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED },
    { VK_FORMAT_BC1_RGB_UNORM_BLOCK, VK_FORMAT_BC1_RGB_SRGB_BLOCK },
    { VK_FORMAT_BC1_RGBA_UNORM_BLOCK, VK_FORMAT_BC1_RGBA_SRGB_BLOCK },
    { VK_FORMAT_BC2_UNORM_BLOCK, VK_FORMAT_BC2_SRGB_BLOCK },
    { VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_BC3_SRGB_BLOCK },
    { VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_BC3_SRGB_BLOCK },
    { VK_FORMAT_BC4_UNORM_BLOCK, VK_FORMAT_UNDEFINED },
    { VK_FORMAT_BC5_UNORM_BLOCK, VK_FORMAT_UNDEFINED },
    { VK_FORMAT_BC6H_SFLOAT_BLOCK, VK_FORMAT_UNDEFINED },
    { VK_FORMAT_BC7_UNORM_BLOCK, VK_FORMAT_BC7_SRGB_BLOCK },
    { VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED },
    { VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED },
    { VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED }
};

// -----------------------------------------------------------------------------------------------------------------------------------

ResourceManager::ResourceManager(vk::Backend::Ptr backend) :
    m_backend(backend)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

ResourceManager::~ResourceManager()
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Texture2D::Ptr ResourceManager::load_texture_2d(const std::string& path, bool srgb, bool absolute)
{
    if (!m_backend.expired())
    {
        vk::BatchUploader uploader(m_backend.lock());

        auto resource = load_texture_2d_internal(path, srgb, absolute, uploader);

        uploader.submit();

        return resource;
    }
    else
        return nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------------

TextureCube::Ptr ResourceManager::load_texture_cube(const std::string& path, bool srgb, bool absolute)
{
    if (!m_backend.expired())
    {
        vk::BatchUploader uploader(m_backend.lock());

        auto resource = load_texture_cube_internal(path, srgb, absolute, uploader);

        uploader.submit();

        return resource;
    }
    else
        return nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------------

Material::Ptr ResourceManager::load_material(const std::string& path, bool absolute)
{
    if (!m_backend.expired())
    {
        vk::BatchUploader uploader(m_backend.lock());

        auto resource = load_material_internal(path, absolute, uploader);

        uploader.submit();

        return resource;
    }
    else
        return nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------------

Mesh::Ptr ResourceManager::load_mesh(const std::string& path, bool absolute)
{
    if (!m_backend.expired())
    {
        vk::BatchUploader uploader(m_backend.lock());

        auto resource = load_mesh_internal(path, absolute, uploader);

        uploader.submit();

        return resource;
    }
    else
        return nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------------

Scene::Ptr ResourceManager::load_scene(const std::string& path, bool absolute)
{
    if (!m_backend.expired())
    {
        vk::BatchUploader uploader(m_backend.lock());
    }
    else
        return nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------------

Texture2D::Ptr ResourceManager::load_texture_2d_internal(const std::string& path, bool srgb, bool absolute, vk::BatchUploader& uploader)
{
    if (m_textures_2d.find(path) != m_textures_2d.end())
        return m_textures_2d[path];
    else
    {
        vk::Backend::Ptr backend = m_backend.lock();
        ast::Image       image;

        if (ast::load_image(absolute ? path : utility::path_for_resource("assets/" + path), image))
        {
            uint32_t type = 0;

            if (image.type == ast::PIXEL_TYPE_FLOAT16)
                type = 1;
            else if (image.type == ast::PIXEL_TYPE_FLOAT32)
                type = 2;

            if (image.compression == ast::COMPRESSION_NONE)
            {
                GLenum internal_format = kInternalFormatTable[type][image.components - 1];

                if (srgb)
                {
                    if (image.components == 3)
                        internal_format = GL_SRGB8;
                    else if (image.components == 4)
                        internal_format = GL_SRGB8_ALPHA8;
                    else
                        LUMEN_LOG_ERROR("SRGB textures can only be created from images with 3 or 4 color components!");
                }

                vk::Image::Ptr     vk_image      = vk::Image::create(backend, VK_IMAGE_TYPE_2D, image.data[0][0].width, image.data[0][0].height, 1, image.mip_slices, image.array_slices, VK_FORMAT_A1R5G5B5_UNORM_PACK16, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
                vk::ImageView::Ptr vk_image_view = vk::ImageView::create(backend, vk_image, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, 0, image.mip_slices, 0, image.array_slices);

                std::shared_ptr<Texture2D> texture = std::make_shared<Texture2D>(vk_image, vk_image_view);

                for (int32_t i = 0; i < image.array_slices; i++)
                {
                    for (int32_t j = 0; j < image.mip_slices; j++)
                        texture->(i, j, image.data[i][j].data);
                }

                m_textures_2d[path] = texture;

                if (image.data[0][0].width != image.data[0][0].height)
                {
                    texture->set_min_filter(GL_LINEAR);
                    texture->set_mag_filter(GL_LINEAR);
                }

                return texture;
            }
            else
            {
                std::shared_ptr<Texture2D> texture = std::make_shared<Texture2D>(image.data[0][0].width,
                                                                                 image.data[0][0].height,
                                                                                 image.array_slices,
                                                                                 image.mip_slices,
                                                                                 1,
                                                                                 kCompressedTable[image.compression - 1][(int)srgb],
                                                                                 kFormatTable[image.components - 1],
                                                                                 kTypeTable[type],
                                                                                 true);

                for (int32_t i = 0; i < image.array_slices; i++)
                {
                    for (int32_t j = 0; j < image.mip_slices; j++)
                        texture->set_compressed_data(i, j, image.data[i][j].size, image.data[i][j].data);
                }

                m_textures_2d[path] = texture;

                if (image.data[0][0].width != image.data[0][0].height)
                {
                    texture->set_min_filter(GL_LINEAR);
                    texture->set_mag_filter(GL_LINEAR);
                }

                return texture;
            }
        }
        else
        {
            LUMEN_LOG_ERROR("Failed to load Texture: " + path);
            return nullptr;
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

TextureCube::Ptr ResourceManager::load_texture_cube_internal(const std::string& path, bool srgb, bool absolute, vk::BatchUploader& uploader)
{
    if (m_textures_cube.find(path) != m_textures_cube.end())
        return m_textures_cube[path];
    else
    {
        ast::Image image;

        if (ast::load_image(absolute ? path : utility::path_for_resource("assets/" + path), image))
        {
            uint32_t type = 0;

            if (image.type == ast::PIXEL_TYPE_FLOAT16)
                type = 1;
            else if (image.type == ast::PIXEL_TYPE_FLOAT32)
                type = 2;

            if (image.array_slices != 6)
            {
                LUMEN_LOG_ERROR("Texture does not have 6 array slices: " + path);
                return nullptr;
            }

            if (image.compression == ast::COMPRESSION_NONE)
            {
                GLenum internal_format = kInternalFormatTable[type][image.components - 1];

                if (srgb)
                {
                    if (image.components == 3)
                        internal_format = GL_SRGB8;
                    else if (image.components == 4)
                        internal_format = GL_SRGB8_ALPHA8;
                    else
                        LUMEN_LOG_ERROR("SRGB textures can only be created from images with 3 or 4 color components!");
                }

                std::shared_ptr<TextureCube> texture = std::make_shared<TextureCube>(image.data[0][0].width,
                                                                                     image.data[0][0].height,
                                                                                     1,
                                                                                     image.mip_slices,
                                                                                     internal_format,
                                                                                     kFormatTable[image.components - 1],
                                                                                     kTypeTable[type]);

                for (int32_t i = 0; i < image.array_slices; i++)
                {
                    for (int32_t j = 0; j < image.mip_slices; j++)
                        texture->set_data(i, 0, j, image.data[i][j].data);
                }

                m_textures_cube[path] = texture;

                return texture;
            }
            else
            {
                if (kCompressedTable[image.compression - 1][(int)srgb] == 0)
                {
                    LUMEN_LOG_ERROR("No SRGB format available for this compression type: " + path);
                    return nullptr;
                }

                std::shared_ptr<TextureCube> texture = std::make_shared<TextureCube>(image.data[0][0].width,
                                                                                     image.data[0][0].height,
                                                                                     1,
                                                                                     image.mip_slices,
                                                                                     kCompressedTable[image.compression - 1][(int)srgb],
                                                                                     kFormatTable[image.components - 1],
                                                                                     kTypeTable[type],
                                                                                     true);

                for (int32_t i = 0; i < image.array_slices; i++)
                {
                    for (int32_t j = 0; j < image.mip_slices; j++)
                        texture->set_compressed_data(i, 0, j, image.data[i][j].size, image.data[i][j].data);
                }

                m_textures_cube[path] = texture;

                return texture;
            }
        }
        else
        {
            LUMEN_LOG_ERROR("Failed to load Texture: " + path);
            return nullptr;
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

Material::Ptr ResourceManager::load_material_internal(const std::string& path, bool absolute, vk::BatchUploader& uploader)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------

Mesh::Ptr ResourceManager::load_mesh_internal(const std::string& path, bool absolute, vk::BatchUploader& uploader)
{
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace lumen