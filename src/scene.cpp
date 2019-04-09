#include "scene.h"
#include "external/Nvidia-SBVH/BVH.h"
#include "external/Nvidia-SBVH/GPUScene.h"
#include <iostream>

namespace lumen
{
std::shared_ptr<Scene> Scene::create(const std::string& path)
{
    return nullptr;
}

Scene::~Scene()
{
	if (m_bvh)
		delete m_bvh;

	if (m_gpu_scene)
		delete m_gpu_scene;
}

uint32_t Scene::add_mesh(const std::shared_ptr<Mesh> mesh,
                         const glm::mat4&            transform)
{
    if (mesh)
    {
        uint32_t current_vertex_count   = m_vtx_positions.size();
        uint32_t current_material_count = m_materials.size();

        m_instances.push_back({ mesh, transform });

        for (int i = 0; i < mesh->m_vtx_positions.size(); i++)
        {
            glm::vec4 t_p  = transform * glm::vec4(mesh->m_vtx_positions[i], 1.0f);
            glm::vec3 t_n  = glm::mat3(transform) * mesh->m_vtx_normals[i];
            glm::vec2 t_tc = mesh->m_vtx_tex_coords[i];

            m_vtx_positions.push_back(t_p);
            m_vtx_normals.push_back(t_p);
            m_vtx_tex_coords.push_back(t_tc);
        }

        uint32_t idx_count = 0;

        for (auto& submesh : mesh->m_sub_meshes)
        {
            m_materials.push_back(mesh->m_materials[submesh.material_index]);
            uint32_t mat_id = submesh.material_index + current_material_count;

            for (uint32_t i = submesh.base_index;
                 i < (idx_count + submesh.index_count);
                 i += 3)
            {
                uint32_t i0 = submesh.base_vertex + mesh->m_indices[i] + current_vertex_count;
                uint32_t i1 = submesh.base_vertex + mesh->m_indices[i + 1] + current_vertex_count;
                uint32_t i2 = submesh.base_vertex + mesh->m_indices[i + 2] + current_vertex_count;

                m_triangles.push_back({ i0, i1, i2, mat_id });
            }

            idx_count += submesh.index_count;
        }

        return 1;
    }
    else
        return 0;
}

void Scene::set_transform(const uint32_t& id, const glm::mat4& transform) {}

void Scene::remove_mesh(const uint32_t& id) {}

void Scene::build()
{
	if (m_bvh)
		delete m_bvh;

	if (m_gpu_scene)
		delete m_gpu_scene;

    std::vector<MeshInstance> instances = m_instances;

    m_instances.clear();
    m_vtx_positions.clear();
    m_vtx_normals.clear();
    m_vtx_tex_coords.clear();
    m_triangles.clear();
    m_materials.clear();

    for (auto& instance : instances)
        add_mesh(instance.mesh, instance.transform);

	Array<GPUScene::Triangle> tris;
    Array<Vec3f> verts;
    tris.clear();
    verts.clear();

    GPUScene::Triangle newtri;

    // convert Triangle to GPUScene::Triangle
    int tri_count = int(m_triangles.size());

    for (int i = 0; i < tri_count; i++) 
	{
        GPUScene::Triangle newtri;
        newtri.vertices = Vec3i(int(m_triangles[i].x), int(m_triangles[i].y), int(m_triangles[i].z));
        tris.add(newtri);
    }

    // fill up Array of vertices
	int ver_count = int(m_vtx_positions.size());

    for (int i = 0; i < ver_count; i++) 
	{
        verts.add(Vec3f(m_vtx_positions[i].x, m_vtx_positions[i].y, m_vtx_positions[i].z));
    }

    std::cout << "Building a new GPU Scene\n";
    m_gpu_scene = new GPUScene(tri_count, ver_count, tris, verts);

    std::cout << "Building BVH with spatial splits\n";
    // create a default platform
    Platform defaultplatform;
    BVH::BuildParams defaultparams;
    BVH::Stats stats;
    m_bvh = new BVH(m_gpu_scene, defaultplatform, defaultparams);
}
} // namespace lumen