#include <core/application.h>
#include <utility/macros.h>
#include <imgui_internal.h>
#include <utility/imgui_plot.h>
#include <utility/profiler.h>
#include <filesystem>
#include <nfd.h>

#define NODE_NAME_PAYLOAD "NODE_NAME_PAYLOAD"

namespace ImGui
{
void PushDisabled()
{
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
}

void PopDisabled()
{
    ImGui::PopItemFlag();
    ImGui::PopStyleVar();
}
} // namespace ImGui

namespace helios
{
std::vector<std::string> node_types = {
    "Mesh",
    "Directional Light",
    "Spot Light",
    "Point Light",
    "Camera",
    "IBL"
};

std::vector<std::string> tone_map_operators = {
    "ACES",
    "Reinhard"
};

std::vector<std::string> output_buffers = {
    "Albedo",
    "Normals",
    "Roughness",
    "Metallic",
    "Emissive",
    "Final"
};

class Viewer : public Application
{
protected:
    // -----------------------------------------------------------------------------------------------------------------------------------

    bool init(int argc, const char* argv[]) override
    {
        m_string_buffer.reserve(256);

        if (std::filesystem::exists("assets/scene/default.json"))
            m_scene = m_resource_manager->load_scene("scene/default.json", false);
        else
        {
            nfdchar_t*  out_path = NULL;
            nfdresult_t result   = NFD_OpenDialog("json", NULL, &out_path);

            if (result == NFD_OKAY)
            {
                std::string path;
                path.resize(strlen(out_path));
                strcpy(path.data(), out_path);
                free(out_path);

                m_vk_backend->queue_object_deletion(m_scene);

                m_scene = m_resource_manager->load_scene(path, true);

                if (!m_scene)
                    return false;
            }
            else
                return false;
        }

        return true;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void update(vk::CommandBuffer::Ptr cmd_buffer) override
    {
        update_camera();

        m_render_state.setup(cmd_buffer);

        if (m_scene)
            m_scene->update(m_render_state);

        m_renderer->render(m_render_state);
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void gui()
    {
        if (!m_show_gui)
            return;

        bool open = true;

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(m_width * 0.3f, m_height));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

        ImGui::Begin("Editor", &open, window_flags);

        if (m_ray_debug_mode)
            ImGui::PushDisabled();

        if (ImGui::CollapsingHeader("Scene"))
        {
            ImGui::Spacing();

            m_string_buffer = "";

            if (m_scene)
                m_string_buffer = m_scene->path();

            ImGui::InputText("##Scene", (char*)m_string_buffer.c_str(), 128, ImGuiInputTextFlags_ReadOnly);

            if (ImGui::Button("Browse..."))
            {
                nfdchar_t*  out_path = NULL;
                nfdresult_t result   = NFD_OpenDialog("json", NULL, &out_path);

                if (result == NFD_OKAY)
                {
                    std::string path;
                    path.resize(strlen(out_path));
                    strcpy(path.data(), out_path);
                    free(out_path);

                    m_vk_backend->queue_object_deletion(m_scene);

                    m_scene = m_resource_manager->load_scene(path, true);
                }
            }

            ImVec2 region = ImGui::GetContentRegionAvail();

            ImGui::Spacing();

            ImGui::Button("Save", ImVec2(region.x, 30.0f));

            ImGui::Spacing();
        }
        if (m_scene)
        {
            if (ImGui::CollapsingHeader("Hierarchy"))
            {
                ImGui::Spacing();

                hierarchy_gui(m_scene->root_node());

                ImGui::Spacing();

                if (m_selected_node && m_node_to_attach_to)
                {
                    Node* parent = m_selected_node->parent();
                    parent->remove_child(m_selected_node->name());

                    m_node_to_attach_to->add_child(m_selected_node);
                    m_node_to_attach_to = nullptr;
                }

                if (m_should_remove_selected_node)
                {
                    Node* parent = m_selected_node->parent();
                    parent->remove_child(m_selected_node->name());

                    m_should_remove_selected_node = false;
                }
            }
            if (ImGui::CollapsingHeader("Inspector"))
            {
                if (m_selected_node)
                {
                    ImGui::PushID(m_selected_node->id());

                    m_string_buffer = m_selected_node->name();

                    ImGui::InputText("Name", (char*)m_string_buffer.c_str(), 128);

                    ImGui::Separator();

                    if (m_selected_node->type() == NODE_MESH)
                        inspector_mesh();
                    else if (m_selected_node->type() == NODE_CAMERA)
                        inspector_camera();
                    else if (m_selected_node->type() == NODE_DIRECTIONAL_LIGHT)
                        inspector_directional_light();
                    else if (m_selected_node->type() == NODE_SPOT_LIGHT)
                        inspector_spot_light();
                    else if (m_selected_node->type() == NODE_POINT_LIGHT)
                        inspector_point_light();
                    else if (m_selected_node->type() == NODE_IBL)
                        inspector_ibl();

                    ImGui::PopID();
                }
            }
        }
        if (ImGui::CollapsingHeader("Bake"))
        {
            ImVec2 region = ImGui::GetContentRegionAvail();

            if (m_renderer->path_integrator()->is_tiled())
            {
                ImGui::ProgressBar(float(m_renderer->path_integrator()->num_accumulated_samples()) / float(m_renderer->path_integrator()->max_samples()), ImVec2(0.0f, 0.0f));
            }
            else
            {
                std::string overlay_text = std::to_string(m_renderer->path_integrator()->num_accumulated_samples()) + " / " + std::to_string(m_renderer->path_integrator()->max_samples());

                ImGui::ProgressBar(float(m_renderer->path_integrator()->num_accumulated_samples()) / float(m_renderer->path_integrator()->max_samples()), ImVec2(region.x, 50.0f), overlay_text.c_str());
            }

            ImGui::Spacing();

            if (ImGui::Button("Restart", ImVec2(region.x, 30.0f)))
                m_renderer->path_integrator()->restart_bake();

            if (ImGui::Button("Save to Disk", ImVec2(region.x, 30.0f)))
            {
                nfdchar_t*  out_path = NULL;
                nfdresult_t result   = NFD_SaveDialog("png", NULL, &out_path);

                if (result == NFD_OKAY)
                {
                    std::string path;
                    path.resize(strlen(out_path));
                    strcpy(path.data(), out_path);
                    free(out_path);

                    m_renderer->save_image_to_disk(path + ".png");
                }
            }
        }
        if (ImGui::CollapsingHeader("Ray Debug View"))
        {
            ImGui::ListBoxHeader("##pixels", ImVec2(ImGui::GetContentRegionAvailWidth(), 100.0f));

            const auto& views = m_renderer->ray_debug_views();

            for (int i = 0; i < views.size(); i++)
            {
                const auto& view = views[i];
                ImGui::Text("%i, %i", view.pixel_coord.x, view.pixel_coord.x);
            }

            ImGui::ListBoxFooter();

            if (ImGui::Button("Add", ImVec2(ImGui::GetContentRegionAvailWidth(), 30.0f)))
            {
                m_ray_debug_mode = true;
                ImGui::PushDisabled();
            }

            if (ImGui::Button("Clear", ImVec2(ImGui::GetContentRegionAvailWidth(), 30.0f)))
                m_renderer->clear_ray_debug_views();

            if (m_ray_debug_mode)
                ImGui::Text("Left Click to add Ray Debug View for pixel (%i, %i), Right Click to cancel", (int)m_mouse_x, (int)m_mouse_y);

            ImGui::InputInt("Num Debug Rays", &m_num_debug_rays);
        }
        if (ImGui::CollapsingHeader("Profiler"))
            profiler_gui();

        if (ImGui::CollapsingHeader("Settings"))
        {
            bool tiled = m_renderer->path_integrator()->is_tiled();

            ImGui::Checkbox("Use Tiled Rendering", &tiled);

            if (m_renderer->path_integrator()->is_tiled() != tiled)
            {
                m_renderer->path_integrator()->set_tiled(tiled);
                m_renderer->path_integrator()->restart_bake();
            }

            if (ImGui::BeginCombo("Output Buffer", output_buffers[m_renderer->current_output_buffer()].c_str()))
            {
                for (uint32_t i = 0; i < output_buffers.size(); i++)
                {
                    const bool is_selected = (i == m_renderer->current_output_buffer());

                    if (ImGui::Selectable(output_buffers[i].c_str(), is_selected))
                        m_renderer->set_current_output_buffer((OutputBuffer)i);

                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            int32_t max_samples = m_renderer->path_integrator()->max_samples();

            ImGui::InputInt("Max Samples", &max_samples);

            if (m_renderer->path_integrator()->max_samples() != max_samples)
            {
                m_renderer->path_integrator()->set_max_samples(max_samples);
                m_renderer->path_integrator()->restart_bake();
            }

            int32_t max_ray_bounces = m_renderer->path_integrator()->max_ray_bounces();

            ImGui::SliderInt("Max Ray Bounces", &max_ray_bounces, 1, 8);

            if (m_renderer->path_integrator()->max_ray_bounces() != max_ray_bounces)
            {
                m_renderer->path_integrator()->set_max_ray_bounces(max_ray_bounces);
                m_renderer->path_integrator()->restart_bake();
            }
            
            if (ImGui::BeginCombo("Tone Map Operator", tone_map_operators[m_renderer->tone_map_operator()].c_str()))
            {
                for (uint32_t i = 0; i < tone_map_operators.size(); i++)
                {
                    const bool is_selected = (i == m_renderer->tone_map_operator());
                    
                    if (ImGui::Selectable(tone_map_operators[i].c_str(), is_selected))
                        m_renderer->set_tone_map_operator((ToneMapOperator)i);

                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            float exposure = m_renderer->exposure();

            ImGui::InputFloat("Exposure", &exposure);

            m_renderer->set_exposure(exposure);

            ImGui::SliderFloat("Camera Speed", &m_camera_speed, 20.0f, 200.0f);
            ImGui::SliderFloat("Look Sensitivity", &m_camera_sensitivity, 0.01f, 0.5f);
        }

        if (m_ray_debug_mode)
            ImGui::PopDisabled();

        ImGui::End();

        ImGui::PopStyleVar();
        ImGui::PopStyleVar();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void shutdown() override
    {
        m_selected_node.reset();
        m_scene.reset();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void key_pressed(int code) override
    {
        if (!m_ray_debug_mode)
        {
            // Handle forward movement.
            if (code == GLFW_KEY_W)
                m_heading_speed = m_camera_speed;
            else if (code == GLFW_KEY_S)
                m_heading_speed = -m_camera_speed;

            // Handle sideways movement.
            if (code == GLFW_KEY_A)
                m_sideways_speed = m_camera_speed;
            else if (code == GLFW_KEY_D)
                m_sideways_speed = -m_camera_speed;

            if (code == GLFW_KEY_G)
                m_show_gui = !m_show_gui;
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void key_released(int code) override
    {
        // Handle forward movement.
        if (code == GLFW_KEY_W || code == GLFW_KEY_S)
            m_heading_speed = 0.0f;

        // Handle sideways movement.
        if (code == GLFW_KEY_A || code == GLFW_KEY_D)
            m_sideways_speed = 0.0f;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void mouse_pressed(int code) override
    {
        if (m_ray_debug_mode)
        {
            if (code == GLFW_MOUSE_BUTTON_LEFT)
            {
                if (m_scene)
                {
                    CameraNode::Ptr camera = std::dynamic_pointer_cast<CameraNode>(m_scene->find_node("main_camera"));

                    if (camera)
                        m_renderer->add_ray_debug_view(glm::ivec2((int)m_mouse_x, (int)m_mouse_y), m_num_debug_rays, camera->view_matrix(), camera->projection_matrix());
                }
                m_ray_debug_mode = false;
            }
            else if (code == GLFW_MOUSE_BUTTON_RIGHT)
                m_ray_debug_mode = false;
        }
        else
        {
            // Enable mouse look.
            if (code == GLFW_MOUSE_BUTTON_RIGHT)
                m_mouse_look = true;
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void mouse_released(int code) override
    {
        // Disable mouse look.
        if (code == GLFW_MOUSE_BUTTON_RIGHT)
            m_mouse_look = false;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void window_resized() override
    {
        m_renderer->on_window_resize();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    Settings intial_settings() override
    {
        // Set custom settings here...
        Settings settings;

        settings.width  = 1920;
        settings.height = 1080;

        return settings;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

private:
    void update_camera()
    {
        if (m_scene)
        {
            CameraNode::Ptr camera = std::dynamic_pointer_cast<CameraNode>(m_scene->find_node("main_camera"));

            if (camera)
            {
                // Translate
                float forward_delta  = m_heading_speed * m_delta_seconds;
                float sideways_delta = m_sideways_speed * m_delta_seconds;

                if (m_heading_speed != 0.0f || sideways_delta != 0.0f)
                {
                    camera->move(camera->camera_forward() * forward_delta);
                    camera->move(camera->camera_left() * sideways_delta);
                }

                if (m_mouse_look && (m_mouse_delta_x != 0.0f || m_mouse_delta_y != 0.0f))
                {
                    // Rotate
                    m_camera_pitch += float(m_mouse_delta_y) * m_camera_sensitivity;
                    m_camera_pitch = glm::clamp(m_camera_pitch, -90.0f, 90.0f);
                    m_camera_yaw += float(m_mouse_delta_x) * m_camera_sensitivity;

                    glm::quat frame_rotation = glm::angleAxis(glm::radians(-m_camera_yaw), glm::vec3(0.0f, 1.0f, 0.0f));
                    frame_rotation           = frame_rotation * glm::angleAxis(glm::radians(-m_camera_pitch), glm::vec3(1.0f, 0.0f, 0.0f));

                    camera->set_orientation(frame_rotation);
                }
            }
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void profiler_gui()
    {
        ImGui::Spacing();

        ImGui::PlotVar("Frametimes (ms)", m_smooth_frametime * 1000.0f, 0.0f, 20.0f);
        ImGui::PlotVarFlushOldEntries();

        m_smooth_frametime += m_delta_seconds;
        m_smooth_frametime /= 2.0f;

        ImGui::Separator();
        ImGui::Spacing();

        profiler::ui();

        ImGui::Spacing();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void hierarchy_gui(Node::Ptr node)
    {
        if (node)
        {
            ImGui::PushID(node->id());
            ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ((m_selected_node && node->name() == m_selected_node->name()) ? ImGuiTreeNodeFlags_Selected : 0);
            bool               tree_open  = ImGui::TreeNodeEx(node->name().c_str(), node_flags);
            ImGui::PopID();

            ImGui::PushID(node->id());
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::BeginMenu("New Node"))
                {
                    for (auto type : node_types)
                    {
                        if (ImGui::MenuItem(type.c_str()))
                        {
                        }
                    }

                    ImGui::EndMenu();
                }
                if (node->parent() && ImGui::MenuItem("Remove"))
                    m_should_remove_selected_node = true;

                ImGui::EndPopup();
            }
            ImGui::PopID();

            if (ImGui::IsItemClicked())
                m_selected_node = node;

            if (ImGui::BeginDragDropTarget() && node->name() != m_selected_node->name())
            {
                if (ImGui::AcceptDragDropPayload(NODE_NAME_PAYLOAD))
                {
                    m_node_to_attach_to = node.get();
                }

                ImGui::EndDragDropTarget();
            }

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::SetDragDropPayload(NODE_NAME_PAYLOAD, m_selected_node->name().c_str(), m_selected_node->name().length());

                ImGui::BeginTooltip();
                ImGui::Text(m_selected_node->name().c_str());
                ImGui::EndTooltip();
                ImGui::EndDragDropSource();
            }

            if (tree_open)
            {
                const auto& children = node->children();

                for (auto node : children)
                    hierarchy_gui(node);

                ImGui::TreePop();
            }
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void inspector_mesh()
    {
        inspector_transform();
        ImGui::Separator();

        MeshNode::Ptr mesh_node = std::dynamic_pointer_cast<MeshNode>(m_selected_node);

        m_string_buffer = "";

        if (mesh_node->mesh())
            m_string_buffer = mesh_node->mesh()->path();

        ImGui::InputText("Mesh", (char*)m_string_buffer.c_str(), 128, ImGuiInputTextFlags_ReadOnly);

        if (ImGui::Button("Browse..."))
        {
            nfdchar_t*  out_path = NULL;
            nfdresult_t result   = NFD_OpenDialog("ast", NULL, &out_path);

            if (result == NFD_OKAY)
            {
                std::string path;
                path.resize(strlen(out_path));
                strcpy(path.data(), out_path);
                free(out_path);

                Mesh::Ptr mesh = m_resource_manager->load_mesh(path, true);
                mesh_node->set_mesh(mesh);

                m_scene->force_update();
            }
        }

        m_string_buffer = "";

        if (mesh_node->material_override())
            m_string_buffer = mesh_node->material_override()->path();

        ImGui::InputText("Material Override", (char*)m_string_buffer.c_str(), 128, ImGuiInputTextFlags_ReadOnly);

        if (ImGui::Button("Browse..."))
        {
            nfdchar_t*  out_path = NULL;
            nfdresult_t result   = NFD_OpenDialog("json", NULL, &out_path);

            if (result == NFD_OKAY)
            {
                std::string path;
                path.resize(strlen(out_path));
                strcpy(path.data(), out_path);
                free(out_path);

                Material::Ptr material = m_resource_manager->load_material(path, true);
                mesh_node->set_material_override(material);

                m_scene->force_update();
            }
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void inspector_camera()
    {
        inspector_transform();
        ImGui::Separator();

        CameraNode::Ptr camera_node = std::dynamic_pointer_cast<CameraNode>(m_selected_node);

        float near_plane = camera_node->near_plane();

        ImGui::InputFloat("Near Plane", &near_plane);

        if (near_plane != camera_node->near_plane())
        {
            m_scene->force_update();
            camera_node->set_near_plane(near_plane);
        }

        float far_plane = camera_node->far_plane();

        ImGui::InputFloat("Far Plane", &far_plane);

        if (far_plane != camera_node->far_plane())
        {
            m_scene->force_update();
            camera_node->set_far_plane(far_plane);
        }

        float fov = camera_node->fov();

        ImGui::InputFloat("FOV", &fov);

        if (fov != camera_node->fov())
        {
            m_scene->force_update();
            camera_node->set_fov(fov);
        }

        if (ImGui::Button("Apply Camera Transform", ImVec2(ImGui::GetContentRegionAvailWidth(), 30.0f)))
        {
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void inspector_directional_light()
    {
        inspector_transform();
        ImGui::Separator();

        DirectionalLightNode::Ptr light_node = std::dynamic_pointer_cast<DirectionalLightNode>(m_selected_node);

        float intensity = light_node->intensity();

        ImGui::InputFloat("FOV", &intensity);

        if (intensity != light_node->intensity())
        {
            m_scene->force_update();
            light_node->set_intensity(intensity);
        }

        glm::vec3 color = light_node->color();

        ImGui::ColorPicker3("Color", &color.x);

        if (color != light_node->color())
        {
            m_scene->force_update();
            light_node->set_color(color);
        }

        float radius = light_node->radius();

        ImGui::InputFloat("Radius", &radius);

        if (radius != light_node->radius())
        {
            m_scene->force_update();
            light_node->set_radius(radius);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void inspector_spot_light()
    {
        inspector_transform();
        ImGui::Separator();

        SpotLightNode::Ptr light_node = std::dynamic_pointer_cast<SpotLightNode>(m_selected_node);

        float intensity = light_node->intensity();

        ImGui::InputFloat("FOV", &intensity);

        if (intensity != light_node->intensity())
        {
            m_scene->force_update();
            light_node->set_intensity(intensity);
        }

        glm::vec3 color = light_node->color();

        ImGui::ColorPicker3("Color", &color.x);

        if (color != light_node->color())
        {
            m_scene->force_update();
            light_node->set_color(color);
        }

        float radius = light_node->radius();

        ImGui::InputFloat("Radius", &radius);

        if (radius != light_node->radius())
        {
            m_scene->force_update();
            light_node->set_radius(radius);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void inspector_point_light()
    {
        inspector_transform();
        ImGui::Separator();

        PointLightNode::Ptr light_node = std::dynamic_pointer_cast<PointLightNode>(m_selected_node);

        float intensity = light_node->intensity();

        ImGui::InputFloat("Intensity", &intensity);

        if (intensity != light_node->intensity())
        {
            m_scene->force_update();
            light_node->set_intensity(intensity);
        }

        glm::vec3 color = light_node->color();

        ImGui::ColorPicker3("Color", &color.x);

        if (color != light_node->color())
        {
            m_scene->force_update();
            light_node->set_color(color);
        }

        float radius = light_node->radius();

        ImGui::InputFloat("Radius", &radius);

        if (radius != light_node->radius())
        {
            m_scene->force_update();
            light_node->set_radius(radius);
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void inspector_ibl()
    {
        IBLNode::Ptr ibl_node = std::dynamic_pointer_cast<IBLNode>(m_selected_node);

        m_string_buffer = "";

        if (ibl_node->image())
            m_string_buffer = ibl_node->image()->path();

        ImGui::InputText("Image", (char*)m_string_buffer.c_str(), 128, ImGuiInputTextFlags_ReadOnly);

        if (ImGui::Button("Browse..."))
        {
            nfdchar_t*  out_path = NULL;
            nfdresult_t result   = NFD_OpenDialog("json", NULL, &out_path);

            if (result == NFD_OKAY)
            {
                std::string path;
                path.resize(strlen(out_path));
                strcpy(path.data(), out_path);
                free(out_path);

                TextureCube::Ptr texture = m_resource_manager->load_texture_cube(path, true);
                ibl_node->set_image(texture);

                m_scene->force_update();
            }
        }
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void inspector_transform()
    {
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

private:
    RenderState m_render_state;
    Scene::Ptr  m_scene;
    bool        m_show_gui                    = true;
    bool        m_mouse_look                  = false;
    bool        m_ray_debug_mode              = false;
    Node::Ptr   m_selected_node               = nullptr;
    bool        m_should_remove_selected_node = false;
    Node*       m_node_to_attach_to           = nullptr;
    float       m_camera_yaw                  = 0.0f;
    float       m_camera_pitch                = 0.0f;
    float       m_heading_speed               = 0.0f;
    float       m_sideways_speed              = 0.0f;
    float       m_camera_sensitivity          = 0.05f;
    float       m_camera_speed                = 5.0f;
    float       m_smooth_frametime            = 0.0f;
    int32_t     m_num_debug_rays              = 32;
    std::string m_string_buffer;
};
} // namespace helios

HELIOS_DECLARE_MAIN(helios::Viewer)
