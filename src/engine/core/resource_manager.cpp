#include <core/resource_manager.h>
#include <utility/logger.h>
#include <utility/utility.h>
#include <loader/loader.h>
#include <vk_mem_alloc.h>
#include <imgui.h>
#include <ImGuizmo.h>

namespace helios
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

Texture::Ptr create_image(const std::string& path, const ast::Image& image, bool srgb, VkImageViewType image_view_type, vk::Backend::Ptr backend, vk::BatchUploader& uploader)
{
    uint32_t type = 0;

    if (image.type == ast::PIXEL_TYPE_FLOAT16)
        type = 1;
    else if (image.type == ast::PIXEL_TYPE_FLOAT32)
        type = 2;

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

    VkImageCreateFlags flags = image_view_type == VK_IMAGE_VIEW_TYPE_CUBE ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

    vk::Image::Ptr     vk_image      = vk::Image::create(backend, VK_IMAGE_TYPE_2D, image.data[0][0].width, image.data[0][0].height, 1, image.mip_slices, image.array_slices, format, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED, 0, nullptr, flags);
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
        return Texture2D::create(backend, vk_image, vk_image_view, path);
    else if (image_view_type == VK_IMAGE_VIEW_TYPE_CUBE)
        return TextureCube::create(backend, vk_image, vk_image_view, path);

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
        vk::Backend::Ptr  backend = m_backend.lock();
        vk::BatchUploader uploader(backend);

        ast::Scene  ast_scene;
        std::string full_path = absolute ? path : utility::path_for_resource("assets/" + path);

        if (ast::load_scene(full_path, ast_scene))
        {
            Node::Ptr root_node = create_node(ast_scene.scene_graph, uploader);

            uploader.submit();

            if (root_node)
                return Scene::create(backend, ast_scene.name, root_node, full_path);
            else
                return nullptr;
        }
        else
            return nullptr;
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
        std::string      full_path = absolute ? path : utility::path_for_resource("assets/" + path);

        if (ast::load_image(full_path, ast_image))
        {
            auto texture = create_image(full_path, ast_image, srgb, VK_IMAGE_VIEW_TYPE_2D, backend, uploader);

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
            HELIOS_LOG_ERROR("Failed to load Texture: " + path);
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
        std::string      full_path = absolute ? path : utility::path_for_resource("assets/" + path);

        if (ast::load_image(full_path, ast_image))
        {
            auto texture = create_image(full_path, ast_image, srgb, VK_IMAGE_VIEW_TYPE_CUBE, backend, uploader);

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
            HELIOS_LOG_ERROR("Failed to load Texture: " + path);
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
        std::string      full_path = absolute ? path : utility::path_for_resource("assets/" + path);

        if (ast::load_material(full_path, ast_material))
        {
            std::string path_to_root = utility::path_without_file(full_path);

            MaterialType type = ast_material.material_type == ast::MATERIAL_OPAQUE ? MATERIAL_OPAQUE : MATERIAL_TRANSPARENT;

            std::vector<Texture2D::Ptr>               textures;
            std::unordered_map<std::string, uint32_t> texture_index_map;

            TextureInfo albedo_texture_info;
            TextureInfo emissive_texture_info;
            TextureInfo normal_texture_info;
            TextureInfo metallic_texture_info;
            TextureInfo roughness_texture_info;

            glm::vec4 albedo_value    = glm::vec4(0.0f);
            glm::vec4 emissive_value  = glm::vec4(0.0f);
            float     metallic_value  = 0.0f;
            float     roughness_value = 1.0f;

            for (auto ast_texture : ast_material.textures)
            {
                if (ast_texture.type == ast::TEXTURE_ALBEDO)
                {
                    if (texture_index_map.find(ast_texture.path) == texture_index_map.end())
                    {
                        Texture2D::Ptr texture = load_texture_2d_internal(ast_texture.path, ast_texture.srgb, true, uploader);

                        texture_index_map[ast_texture.path] = textures.size();

                        textures.push_back(texture);
                    }

                    albedo_texture_info.array_index   = texture_index_map[ast_texture.path];
                    albedo_texture_info.channel_index = ast_texture.channel_index;
                }
                else if (ast_texture.type == ast::TEXTURE_EMISSIVE)
                {
                    if (texture_index_map.find(ast_texture.path) == texture_index_map.end())
                    {
                        Texture2D::Ptr texture = load_texture_2d_internal(ast_texture.path, ast_texture.srgb, true, uploader);

                        texture_index_map[ast_texture.path] = textures.size();

                        textures.push_back(texture);
                    }

                    emissive_texture_info.array_index   = texture_index_map[ast_texture.path];
                    emissive_texture_info.channel_index = ast_texture.channel_index;
                }
                else if (ast_texture.type == ast::TEXTURE_NORMAL)
                {
                    if (texture_index_map.find(ast_texture.path) == texture_index_map.end())
                    {
                        Texture2D::Ptr texture = load_texture_2d_internal(ast_texture.path, ast_texture.srgb, true, uploader);

                        texture_index_map[ast_texture.path] = textures.size();

                        textures.push_back(texture);
                    }

                    normal_texture_info.array_index   = texture_index_map[ast_texture.path];
                    normal_texture_info.channel_index = ast_texture.channel_index;
                }
                else if (ast_texture.type == ast::TEXTURE_METALLIC)
                {
                    if (texture_index_map.find(ast_texture.path) == texture_index_map.end())
                    {
                        Texture2D::Ptr texture = load_texture_2d_internal(ast_texture.path, ast_texture.srgb, true, uploader);

                        texture_index_map[ast_texture.path] = textures.size();

                        textures.push_back(texture);
                    }

                    metallic_texture_info.array_index   = texture_index_map[ast_texture.path];
                    metallic_texture_info.channel_index = ast_texture.channel_index;
                }
                else if (ast_texture.type == ast::TEXTURE_ROUGHNESS)
                {
                    if (texture_index_map.find(ast_texture.path) == texture_index_map.end())
                    {
                        Texture2D::Ptr texture = load_texture_2d_internal(ast_texture.path, ast_texture.srgb, true, uploader);

                        texture_index_map[ast_texture.path] = textures.size();

                        textures.push_back(texture);
                    }

                    roughness_texture_info.array_index   = texture_index_map[ast_texture.path];
                    roughness_texture_info.channel_index = ast_texture.channel_index;
                }
            }

            for (auto ast_property : ast_material.properties)
            {
                if (ast_property.type == ast::PROPERTY_ALBEDO)
                    albedo_value = glm::vec4(ast_property.vec4_value[0], ast_property.vec4_value[1], ast_property.vec4_value[2], ast_property.vec4_value[3]);
                if (ast_property.type == ast::PROPERTY_EMISSIVE)
                    emissive_value = glm::vec4(ast_property.vec4_value[0], ast_property.vec4_value[1], ast_property.vec4_value[2], ast_property.vec4_value[3]);
                if (ast_property.type == ast::PROPERTY_METALLIC)
                    metallic_value = ast_property.float_value;
                if (ast_property.type == ast::PROPERTY_ROUGHNESS)
                    roughness_value = ast_property.float_value;
            }

            Material::Ptr material = Material::create(backend, type, textures, albedo_texture_info, normal_texture_info, metallic_texture_info, roughness_texture_info, emissive_texture_info, albedo_value, emissive_value, metallic_value, roughness_value, ast_material.alpha_mask, full_path);

            m_materials[path] = material;

            return material;
        }
        else
        {
            HELIOS_LOG_ERROR("Failed to load Material: " + path);
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
        ast::Mesh        ast_mesh;
        std::string      full_path = absolute ? path : utility::path_for_resource("assets/" + path);

        if (ast::load_mesh(full_path, ast_mesh))
        {
            std::string path_to_root = utility::path_without_file(full_path);

            std::vector<Vertex>        vertices(ast_mesh.vertices.size());
            std::vector<SubMesh>       submeshes(ast_mesh.submeshes.size());
            std::vector<Material::Ptr> materials(ast_mesh.materials.size());

            for (int i = 0; i < ast_mesh.vertices.size(); i++)
            {
                vertices[i].position  = glm::vec4(ast_mesh.vertices[i].position, 0.0f);
                vertices[i].tex_coord = glm::vec4(ast_mesh.vertices[i].tex_coord, 0.0f, 0.0f);
                vertices[i].normal    = glm::vec4(ast_mesh.vertices[i].normal, 0.0f);
                vertices[i].tangent   = glm::vec4(ast_mesh.vertices[i].tangent, 0.0f);
                vertices[i].bitangent = glm::vec4(ast_mesh.vertices[i].bitangent, 0.0f);
            }

            for (int i = 0; i < ast_mesh.submeshes.size(); i++)
            {
                submeshes[i].name         = ast_mesh.submeshes[i].name;
                submeshes[i].mat_idx      = ast_mesh.submeshes[i].material_index;
                submeshes[i].index_count  = ast_mesh.submeshes[i].index_count;
                submeshes[i].vertex_count = ast_mesh.submeshes[i].vertex_count;
                submeshes[i].base_vertex  = ast_mesh.submeshes[i].base_vertex;
                submeshes[i].base_index   = ast_mesh.submeshes[i].base_index;
                submeshes[i].max_extents  = ast_mesh.submeshes[i].max_extents;
                submeshes[i].min_extents  = ast_mesh.submeshes[i].min_extents;
            }

            for (int submesh_idx = 0; submesh_idx < submeshes.size(); submesh_idx++)
            {
                const auto& submesh = submeshes[submesh_idx];

                for (int i = submesh.base_index; i < (submesh.base_index + submesh.index_count); i++)
                    vertices[submesh.base_vertex + ast_mesh.indices[i]].position.w = float(submesh_idx);
            }

            for (int i = 0; i < ast_mesh.material_paths.size(); i++)
                materials[i] = load_material_internal(ast_mesh.material_paths[i], true, uploader);

            Mesh::Ptr mesh = Mesh::create(backend, vertices, ast_mesh.indices, submeshes, materials, uploader, full_path);

            m_meshes[path] = mesh;

            return mesh;
        }
        else
        {
            HELIOS_LOG_ERROR("Failed to load Mesh: " + path);
            return nullptr;
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

Node::Ptr ResourceManager::create_node(std::shared_ptr<ast::SceneNode> ast_node, vk::BatchUploader& uploader)
{
    if (ast_node->type == ast::SCENE_NODE_MESH)
        return create_mesh_node(std::dynamic_pointer_cast<ast::MeshNode>(ast_node), uploader);
    else if (ast_node->type == ast::SCENE_NODE_CAMERA)
        return create_camera_node(std::dynamic_pointer_cast<ast::CameraNode>(ast_node), uploader);
    else if (ast_node->type == ast::SCENE_NODE_DIRECTIONAL_LIGHT)
        return create_directional_light_node(std::dynamic_pointer_cast<ast::DirectionalLightNode>(ast_node), uploader);
    else if (ast_node->type == ast::SCENE_NODE_SPOT_LIGHT)
        return create_spot_light_node(std::dynamic_pointer_cast<ast::SpotLightNode>(ast_node), uploader);
    else if (ast_node->type == ast::SCENE_NODE_POINT_LIGHT)
        return create_point_light_node(std::dynamic_pointer_cast<ast::PointLightNode>(ast_node), uploader);
    else if (ast_node->type == ast::SCENE_NODE_IBL)
        return create_ibl_node(std::dynamic_pointer_cast<ast::IBLNode>(ast_node), uploader);
    else if (ast_node->type == ast::SCENE_NODE_ROOT)
        return create_root_node(std::dynamic_pointer_cast<ast::TransformNode>(ast_node), uploader);
    else
        return nullptr;
}

// -----------------------------------------------------------------------------------------------------------------------------------

MeshNode::Ptr ResourceManager::create_mesh_node(std::shared_ptr<ast::MeshNode> ast_node, vk::BatchUploader& uploader)
{
    MeshNode::Ptr mesh_node = std::shared_ptr<MeshNode>(new MeshNode(ast_node->name));

    Mesh::Ptr mesh = nullptr;

    if (ast_node->mesh != "")
    {
        mesh = load_mesh_internal(ast_node->mesh, false, uploader);

        if (mesh)
            mesh_node->set_mesh(mesh);
        else
            HELIOS_LOG_ERROR("Failed to load mesh: " + ast_node->mesh);

        Material::Ptr material_override = nullptr;

        if (ast_node->material_override != "")
        {
            material_override = load_material_internal(ast_node->material_override, false, uploader);

            if (!material_override)
                HELIOS_LOG_ERROR("Failed to load material override: " + ast_node->material_override);

            mesh_node->set_material_override(material_override);
        }
    }

    populate_transform_node(mesh_node, ast_node);
    populate_scene_node(mesh_node, ast_node, uploader);

    return mesh_node;
}

// -----------------------------------------------------------------------------------------------------------------------------------

CameraNode::Ptr ResourceManager::create_camera_node(std::shared_ptr<ast::CameraNode> ast_node, vk::BatchUploader& uploader)
{
    CameraNode::Ptr camera_node = std::shared_ptr<CameraNode>(new CameraNode(ast_node->name));

    camera_node->set_near_plane(ast_node->near_plane);
    camera_node->set_far_plane(ast_node->far_plane);
    camera_node->set_fov(ast_node->fov);

    populate_transform_node(camera_node, ast_node);
    populate_scene_node(camera_node, ast_node, uploader);

    return camera_node;
}

// -----------------------------------------------------------------------------------------------------------------------------------

DirectionalLightNode::Ptr ResourceManager::create_directional_light_node(std::shared_ptr<ast::DirectionalLightNode> ast_node, vk::BatchUploader& uploader)
{
    DirectionalLightNode::Ptr dir_light_node = std::shared_ptr<DirectionalLightNode>(new DirectionalLightNode(ast_node->name));

    dir_light_node->set_color(ast_node->color);
    dir_light_node->set_intensity(ast_node->intensity);
    dir_light_node->set_radius(ast_node->radius);

    populate_transform_node(dir_light_node, ast_node);
    populate_scene_node(dir_light_node, ast_node, uploader);

    return dir_light_node;
}

// -----------------------------------------------------------------------------------------------------------------------------------

SpotLightNode::Ptr ResourceManager::create_spot_light_node(std::shared_ptr<ast::SpotLightNode> ast_node, vk::BatchUploader& uploader)
{
    SpotLightNode::Ptr spot_light_node = std::shared_ptr<SpotLightNode>(new SpotLightNode(ast_node->name));

    spot_light_node->set_color(ast_node->color);
    spot_light_node->set_intensity(ast_node->intensity);
    spot_light_node->set_radius(ast_node->radius);
    spot_light_node->set_inner_cone_angle(ast_node->inner_cone_angle);
    spot_light_node->set_outer_cone_angle(ast_node->inner_cone_angle);

    populate_transform_node(spot_light_node, ast_node);
    populate_scene_node(spot_light_node, ast_node, uploader);

    return spot_light_node;
}

// -----------------------------------------------------------------------------------------------------------------------------------

PointLightNode::Ptr ResourceManager::create_point_light_node(std::shared_ptr<ast::PointLightNode> ast_node, vk::BatchUploader& uploader)
{
    PointLightNode::Ptr point_light_node = std::shared_ptr<PointLightNode>(new PointLightNode(ast_node->name));

    point_light_node->set_color(ast_node->color);
    point_light_node->set_intensity(ast_node->intensity);
    point_light_node->set_radius(ast_node->radius);

    populate_transform_node(point_light_node, ast_node);
    populate_scene_node(point_light_node, ast_node, uploader);

    return point_light_node;
}

// -----------------------------------------------------------------------------------------------------------------------------------

IBLNode::Ptr ResourceManager::create_ibl_node(std::shared_ptr<ast::IBLNode> ast_node, vk::BatchUploader& uploader)
{
    IBLNode::Ptr ibl_node = std::shared_ptr<IBLNode>(new IBLNode(ast_node->name));

    TextureCube::Ptr texture_cube = nullptr;

    if (ast_node->image != "")
    {
        texture_cube = load_texture_cube_internal(ast_node->image, false, false, uploader);

        if (texture_cube)
            ibl_node->set_image(texture_cube);
        else
            HELIOS_LOG_ERROR("Failed to load cubemap: " + ast_node->image);
    }

    populate_scene_node(ibl_node, ast_node, uploader);

    return ibl_node;
}

// -----------------------------------------------------------------------------------------------------------------------------------

RootNode::Ptr ResourceManager::create_root_node(std::shared_ptr<ast::TransformNode> ast_node, vk::BatchUploader& uploader)
{
    RootNode::Ptr transform_node = std::shared_ptr<RootNode>(new RootNode(ast_node->name));

    populate_transform_node(transform_node, ast_node);
    populate_scene_node(transform_node, ast_node, uploader);

    return transform_node;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void ResourceManager::populate_scene_node(Node::Ptr node, std::shared_ptr<ast::SceneNode> ast_node, vk::BatchUploader& uploader)
{
    for (auto ast_child : ast_node->children)
    {
        auto child = create_node(ast_child, uploader);

        if (child)
            node->add_child(child);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void ResourceManager::populate_transform_node(TransformNode::Ptr node, std::shared_ptr<ast::TransformNode> ast_node)
{
    glm::mat4 local_transform;
    ImGuizmo::RecomposeMatrixFromComponents(&ast_node->position.x, &ast_node->rotation.x, &ast_node->scale.x, &local_transform[0][0]);

    node->set_from_local_transform(local_transform);
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace helios