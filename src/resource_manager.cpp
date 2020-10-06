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

const VkFormat kNonSRGBFormats[][4] {
    { VK_FORMAT_R8_SNORM, VK_FORMAT_R8G8_SNORM, VK_FORMAT_R8G8B8_SNORM, VK_FORMAT_R8G8B8A8_SNORM },
    { VK_FORMAT_R16_SFLOAT, VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_R16G16B16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT },
    { VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT }
};

// -----------------------------------------------------------------------------------------------------------------------------------

const VkFormat kSRGBFormats[][4] {
    { VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, VK_FORMAT_R8G8B8_SRGB, VK_FORMAT_R8G8B8A8_SRGB },
    { VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED },
    { VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED }
};

// -----------------------------------------------------------------------------------------------------------------------------------

Texture::Ptr create_image(const ast::Image& image, bool srgb, VkImageViewType image_view_type, vk::Backend::Ptr backend, vk::BatchUploader& uploader)
{
    uint32_t type = 0;

    if (image.type == ast::PIXEL_TYPE_FLOAT16)
        type = 1;
    else if (image.type == ast::PIXEL_TYPE_FLOAT32)
        type = 2;

    if (image.compression == ast::COMPRESSION_NONE)
    {
        VkFormat format = VK_FORMAT_UNDEFINED;

        if (image.compression == ast::CompressionType::COMPRESSION_NONE)
        {
            if (srgb)
                format = kSRGBFormats[type][image.components - 1];
            else
                format = kNonSRGBFormats[type][image.components - 1];
        }
        else
            format = kCompressedFormats[image.compression][srgb];

        vk::Image::Ptr     vk_image      = vk::Image::create(backend, VK_IMAGE_TYPE_2D, image.data[0][0].width, image.data[0][0].height, 1, image.mip_slices, image.array_slices, format, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
        vk::ImageView::Ptr vk_image_view = vk::ImageView::create(backend, vk_image, image_view_type, VK_IMAGE_ASPECT_COLOR_BIT, 0, image.mip_slices, 0, image.array_slices);

        size_t              total_size = 0;
        std::vector<size_t> mip_level_sizes;

        for (int32_t i = 0; i < image.array_slices; i++)
        {
            for (int32_t j = 0; j < image.mip_slices; j++)
            {
                total_size += image.data[i][j].size;
                mip_level_sizes.push_back(image.data[i][j].size);
            }
        }

        std::vector<uint8_t> image_data(total_size);

        size_t offset = 0;

        for (int32_t i = 0; i < image.array_slices; i++)
        {
            for (int32_t j = 0; j < image.mip_slices; j++)
            {
                memcpy(image_data.data() + offset, image.data[i][j].data, image.data[i][j].size);
                offset += image.data[i][j].size;
            }
        }

        uploader.upload_image_data(vk_image, image_data.data(), mip_level_sizes);

        if (image_view_type == VK_IMAGE_VIEW_TYPE_2D)
            return std::make_shared<Texture2D>(vk_image, vk_image_view);
        else if (image_view_type == VK_IMAGE_VIEW_TYPE_CUBE)
            return std::make_shared<TextureCube>(vk_image, vk_image_view);
    }

    return nullptr;
}

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
            auto texture = create_image(image, srgb, VK_IMAGE_VIEW_TYPE_2D, backend, uploader);

            if (texture)
            {
                auto texture_2d = std::dynamic_pointer_cast<Texture2D>(texture);

                m_textures_2d[path] = texture_2d;

                return texture_2d;
            }
            else
                return nullptr;
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
        vk::Backend::Ptr backend = m_backend.lock();
        ast::Image       image;

        if (ast::load_image(absolute ? path : utility::path_for_resource("assets/" + path), image))
        {
            auto texture = create_image(image, srgb, VK_IMAGE_VIEW_TYPE_2D, backend, uploader);

            if (texture)
            {
                auto texture_cube = std::dynamic_pointer_cast<TextureCube>(texture);

                m_textures_cube[path] = texture_cube;

                return texture_cube;
            }
            else
                return nullptr;
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