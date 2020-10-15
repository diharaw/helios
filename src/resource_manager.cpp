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
            return Texture2D::create(vk_image, vk_image_view);
        else if (image_view_type == VK_IMAGE_VIEW_TYPE_CUBE)
            return TextureCube::create(vk_image, vk_image_view);
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
        ast::Image       ast_image;

        if (ast::load_image(absolute ? path : utility::path_for_resource("assets/" + path), ast_image))
        {
            auto texture = create_image(ast_image, srgb, VK_IMAGE_VIEW_TYPE_2D, backend, uploader);

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
        ast::Image       ast_image;

        if (ast::load_image(absolute ? path : utility::path_for_resource("assets/" + path), ast_image))
        {
            auto texture = create_image(ast_image, srgb, VK_IMAGE_VIEW_TYPE_CUBE, backend, uploader);

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
    if (m_materials.find(path) != m_materials.end())
        return m_materials[path];
    else
    {
        vk::Backend::Ptr backend = m_backend.lock();
        ast::Material    ast_material;

        if (ast::load_material(absolute ? path : utility::path_for_resource("assets/" + path), ast_material))
        {
            MaterialType type = ast_material.material_type == ast::MATERIAL_OPAQUE ? MATERIAL_OPAQUE : MATERIAL_TRANSPARENT;

            Texture2D::Ptr albedo_texture    = nullptr;
            Texture2D::Ptr emissive_texture  = nullptr;
            Texture2D::Ptr normal_texture    = nullptr;
            Texture2D::Ptr metallic_texture  = nullptr;
            Texture2D::Ptr roughness_texture = nullptr;

            glm::vec4 albedo_value    = glm::vec4(0.0f);
            glm::vec4 emissive_value  = glm::vec4(0.0f);
            float     metallic_value  = 0.0f;
            float     roughness_value = 1.0f;

            for (auto ast_texture : ast_material.textures)
            {
                if (ast_texture.type == ast::TEXTURE_ALBEDO)
                    albedo_texture = load_texture_2d_internal(ast_texture.path, ast_texture.srgb, false, uploader);
                else if (ast_texture.type == ast::TEXTURE_EMISSIVE)
                    emissive_texture = load_texture_2d_internal(ast_texture.path, ast_texture.srgb, false, uploader);
                else if (ast_texture.type == ast::TEXTURE_NORMAL)
                    normal_texture = load_texture_2d_internal(ast_texture.path, ast_texture.srgb, false, uploader);
                else if (ast_texture.type == ast::TEXTURE_METALNESS_SPECULAR)
                    metallic_texture = load_texture_2d_internal(ast_texture.path, ast_texture.srgb, false, uploader);
                else if (ast_texture.type == ast::TEXTURE_ROUGHNESS_GLOSSINESS)
                    roughness_texture = load_texture_2d_internal(ast_texture.path, ast_texture.srgb, false, uploader);
            }

            for (auto ast_property : ast_material.properties)
            {
                if (ast_property.type == ast::PROPERTY_ALBEDO)
                    albedo_value = glm::vec4(ast_property.vec4_value[0], ast_property.vec4_value[1], ast_property.vec4_value[2], ast_property.vec4_value[3]);
                if (ast_property.type == ast::PROPERTY_EMISSIVE)
                    emissive_value = glm::vec4(ast_property.vec4_value[0], ast_property.vec4_value[1], ast_property.vec4_value[2], ast_property.vec4_value[3]);
                if (ast_property.type == ast::PROPERTY_METALNESS_SPECULAR)
                    metallic_value = ast_property.float_value;
                if (ast_property.type == ast::PROPERTY_ROUGHNESS_GLOSSINESS)
                    roughness_value = ast_property.float_value;
            }

            Material::Ptr material = Material::create(type, albedo_texture, normal_texture, metallic_texture, roughness_texture, emissive_texture, albedo_value, emissive_value, metallic_value, roughness_value, ast_material.orca);

            m_materials[path] = material;

            return material;
        }
        else
        {
            LUMEN_LOG_ERROR("Failed to load Material: " + path);
            return nullptr;
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

Mesh::Ptr ResourceManager::load_mesh_internal(const std::string& path, bool absolute, vk::BatchUploader& uploader)
{
    if (m_meshes.find(path) != m_meshes.end())
        return m_meshes[path];
    else
    {
        vk::Backend::Ptr backend = m_backend.lock();
        ast::Mesh    ast_mesh;

        if (ast::load_mesh(absolute ? path : utility::path_for_resource("assets/" + path), ast_mesh))
        {
            std::vector<Vertex>   vertices;
            std::vector<uint32_t> indices;
            std::vector<SubMesh> sub_meshes;
            std::vector<Material::Ptr> materials;

            // Copy vertices

            // Copy indices
            indices = ast_mesh.indices;

            Mesh::Ptr mesh = Mesh::create(backend, vertices, indices, sub_meshes, materials, uploader);

            m_meshes[path] = mesh;

            return mesh;
        }
        else
        {
            LUMEN_LOG_ERROR("Failed to load Mesh: " + path);
            return nullptr;
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace lumen