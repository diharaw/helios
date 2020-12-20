#include <core/application.h>
#include <utility/logger.h>
#include <utility/macros.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <utility/imgui_plot.h>
#include <utility/profiler.h>
#include <IconsFontAwesome5Pro.h>
#include <filesystem>
#include <nfd.h>
#include <common/scene.h>
#include <exporter/scene_exporter.h>
#include <filesystem>

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
static const std::vector<std::string> node_types = {
    "Mesh",
    "Camera",
    "Directional Light",
    "Spot Light",
    "Point Light",
    "IBL"
};

static const std::vector<std::string> tone_map_operators = {
    "ACES",
    "Reinhard"
};

static const std::vector<std::string> output_buffers = {
    "Albedo",
    "Normals",
    "Roughness",
    "Metallic",
    "Emissive",
    "Final"
};

class Editor : public Application
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

        m_editor_camera = std::unique_ptr<CameraNode>(new CameraNode("Editor Camera"));
        m_editor_camera->set_position(glm::vec3(0.0f, 10.0f, 0.0f));

        return true;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void update(vk::CommandBuffer::Ptr cmd_buffer) override
    {
        update_camera();

        m_render_state.setup(m_width, m_height, cmd_buffer);

        m_editor_camera->update(m_render_state);

        if (m_scene)
            m_scene->update(m_render_state);

        m_renderer->render(m_render_state);
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void gui()
    {
        ImGuizmo::BeginFrame();

        if (!m_show_gui)
            return;

        auto extents = m_vk_backend->swap_chain_extents();

        if (m_scene)
        {
            TransformNode::Ptr transform_node = std::dynamic_pointer_cast<TransformNode>(m_selected_node);

            if (transform_node)
            {
                glm::mat4 view       = m_editor_camera->view_matrix();
                glm::mat4 projection = m_editor_camera->projection_matrix();

                ImGuizmo::SetOrthographic(false);
                ImGuizmo::SetRect(0, 0, extents.width, extents.height);

                glm::mat4 transform = transform_node->global_transform();

                ImGuizmo::Manipulate(&view[0][0], &projection[0][0], (ImGuizmo::OPERATION)m_current_operation, (ImGuizmo::MODE)m_current_mode, &transform[0][0], NULL, m_use_snap ? &m_snap[0] : NULL);

                if (ImGuizmo::IsUsing())
                    transform_node->set_from_global_transform(transform);
            }
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        bool             open         = true;
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(m_width * 0.3f, m_height));

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
                    m_selected_node = nullptr;

                    m_scene = m_resource_manager->load_scene(path, true);
                }
            }

            ImVec2 region = ImGui::GetContentRegionAvail();

            ImGui::Spacing();

            if (!m_scene)
                ImGui::PushDisabled();

            if (ImGui::Button("Save", ImVec2(region.x, 30.0f)))
            {
                nfdchar_t*  out_path = NULL;
                nfdresult_t result   = NFD_SaveDialog("json", NULL, &out_path);

                if (result == NFD_OKAY)
                {
                    std::string path;
                    path.resize(strlen(out_path));
                    strcpy(path.data(), out_path);
                    free(out_path);

                    save_scene(path + ".json");
                }
            }

            if (!m_scene)
                ImGui::PopDisabled();

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

                if (m_should_add_new_node)
                    create_new_node();

                if (m_should_remove_selected_node)
                {
                    Node* parent = m_selected_node->parent();
                    parent->remove_child(m_selected_node->name());

                    m_selected_node               = nullptr;
                    m_should_remove_selected_node = false;
                }
            }
            if (ImGui::CollapsingHeader("Inspector"))
            {
                if (m_selected_node)
                {
                    ImGui::PushID(m_selected_node->id());

                    ImVec2 pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 25.0f));

                    m_string_buffer = m_selected_node->name();

                    std::string name = icon_for_node_type(m_selected_node->type()) + " Name";

                    ImGui::InputText(name.c_str(), (char*)m_string_buffer.c_str(), 128);

                    pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 25.0f));

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
                    else if (m_selected_node->type() == NODE_ROOT)
                        inspector_transform(true, true, false);

                    ImGui::PopID();
                }
                else
                {
                    ImVec2 pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 25.0f));

                    ImGui::Text("Select Node from the Hierarchy to populate Inspector.");

                    pos = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 25.0f));
                }
            }
        }
        if (ImGui::CollapsingHeader("Bake"))
        {
            ImVec2 region = ImGui::GetContentRegionAvail();

            std::string overlay_text = std::to_string(m_renderer->path_integrator()->num_accumulated_samples()) + " / " + std::to_string(m_renderer->path_integrator()->num_target_samples());

            ImGui::ProgressBar(float(m_renderer->path_integrator()->num_accumulated_samples()) / float(m_renderer->path_integrator()->num_target_samples()), ImVec2(region.x, 50.0f), overlay_text.c_str());

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

            ImGui::SliderFloat("Exposure", &exposure, 0.1f, 10.0f);

            m_renderer->set_exposure(exposure);

            ImGui::SliderFloat("Camera Speed", &m_camera_speed, 20.0f, 200.0f);
            ImGui::SliderFloat("Look Sensitivity", &m_camera_sensitivity, 0.01f, 0.5f);

            float fov = m_editor_camera->fov();

            if (ImGui::InputFloat("Editor Camera FOV", &fov))
                m_editor_camera->set_fov(fov);
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
        m_editor_camera.reset();
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
                    m_renderer->add_ray_debug_view(glm::ivec2((int)m_mouse_x, (int)m_mouse_y), m_num_debug_rays, m_editor_camera->view_matrix(), m_editor_camera->projection_matrix());

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
        settings.title  = "Helios Editor";

        return settings;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

private:
    void update_camera()
    {
        if (m_scene)
        {
            // Translate
            float forward_delta  = m_heading_speed * m_delta_seconds;
            float sideways_delta = m_sideways_speed * m_delta_seconds;

            if (m_heading_speed != 0.0f || sideways_delta != 0.0f)
            {
                m_editor_camera->move(m_editor_camera->camera_forward() * forward_delta);
                m_editor_camera->move(m_editor_camera->camera_left() * sideways_delta);
            }

            if (m_mouse_look && (m_mouse_delta_x != 0.0f || m_mouse_delta_y != 0.0f))
            {
                // Rotate
                m_camera_pitch += float(m_mouse_delta_y) * m_camera_sensitivity;
                m_camera_pitch = glm::clamp(m_camera_pitch, -90.0f, 90.0f);
                m_camera_yaw += float(m_mouse_delta_x) * m_camera_sensitivity;

                glm::quat frame_rotation = glm::angleAxis(glm::radians(-m_camera_yaw), glm::vec3(0.0f, 1.0f, 0.0f));
                frame_rotation           = frame_rotation * glm::angleAxis(glm::radians(-m_camera_pitch), glm::vec3(1.0f, 0.0f, 0.0f));

                m_editor_camera->set_orientation(frame_rotation);
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

            std::string name = icon_for_node_type(node->type()) + " " + node->name();

            ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ((m_selected_node && node->name() == m_selected_node->name()) ? ImGuiTreeNodeFlags_Selected : 0);
            bool               tree_open  = ImGui::TreeNodeEx(name.c_str(), node_flags);
            ImGui::PopID();

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
                m_selected_node = node;

            ImGui::PushID(node->id());
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::BeginMenu("New Node"))
                {
                    for (int i = 0; i < node_types.size(); i++)
                    {
                        if (ImGui::MenuItem(node_types[i].c_str()))
                        {
                            m_should_add_new_node = true;
                            m_node_type_to_add    = (NodeType)i;
                        }
                    }

                    ImGui::EndMenu();
                }
                if (node->parent() && ImGui::MenuItem("Remove"))
                    m_should_remove_selected_node = true;

                ImGui::EndPopup();
            }
            ImGui::PopID();

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

        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 25.0f));

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

        pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 25.0f));

        ImGui::Separator();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void inspector_camera()
    {
        inspector_transform(true, true, false);

        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 25.0f));

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
            camera_node->set_from_global_transform(m_editor_camera->global_transform());

        pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 25.0f));

        ImGui::Separator();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void inspector_directional_light()
    {
        inspector_transform(false, true, false);

        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 25.0f));

        DirectionalLightNode::Ptr light_node = std::dynamic_pointer_cast<DirectionalLightNode>(m_selected_node);

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

        pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 25.0f));

        ImGui::Separator();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void inspector_spot_light()
    {
        inspector_transform(true, true, false);

        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 25.0f));

        SpotLightNode::Ptr light_node = std::dynamic_pointer_cast<SpotLightNode>(m_selected_node);

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

        pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 25.0f));

        ImGui::Separator();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void inspector_point_light()
    {
        inspector_transform(true, false, false);

        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 25.0f));

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

        pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 25.0f));

        ImGui::Separator();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void inspector_ibl()
    {
        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 25.0f));

        IBLNode::Ptr ibl_node = std::dynamic_pointer_cast<IBLNode>(m_selected_node);

        m_string_buffer = "";

        if (ibl_node->image())
            m_string_buffer = ibl_node->image()->path();

        ImGui::InputText("IBL Cubemap", (char*)m_string_buffer.c_str(), 128, ImGuiInputTextFlags_ReadOnly);

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

                TextureCube::Ptr texture = m_resource_manager->load_texture_cube(path, false, true);
                ibl_node->set_image(texture);

                m_scene->force_update();
            }
        }

        pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 25.0f));

        ImGui::Separator();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void inspector_transform(bool use_translate = true, bool use_rotate = true, bool use_scale = true)
    {
        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 25.0f));

        bool is_single_operation = false;

        if (use_translate && !use_rotate && !use_scale)
        {
            m_current_operation = ImGuizmo::TRANSLATE;
            is_single_operation = true;
        }
        if (use_rotate && !use_translate && !use_scale)
        {
            m_current_operation = ImGuizmo::ROTATE;
            is_single_operation = true;
        }

        if (!is_single_operation)
        {
            if (use_translate)
            {
                if (ImGui::RadioButton("Translate", m_current_operation == ImGuizmo::TRANSLATE))
                    m_current_operation = ImGuizmo::TRANSLATE;

                if (use_rotate || use_scale)
                    ImGui::SameLine();
            }

            if (use_rotate)
            {
                if (ImGui::RadioButton("Rotate", m_current_operation == ImGuizmo::ROTATE))
                    m_current_operation = ImGuizmo::ROTATE;

                if (use_scale)
                    ImGui::SameLine();
            }

            if (use_scale)
            {
                if (ImGui::RadioButton("Scale", m_current_operation == ImGuizmo::SCALE))
                    m_current_operation = ImGuizmo::SCALE;
            }
        }

        TransformNode::Ptr transform_node = std::dynamic_pointer_cast<TransformNode>(m_selected_node);

        glm::vec3 position;
        glm::vec3 rotation;
        glm::vec3 scale;

        glm::mat4 out_transform = transform_node->local_transform();

        ImGuizmo::DecomposeMatrixToComponents(&out_transform[0][0], &position.x, &rotation.x, &scale.x);

        bool is_edited = false;

        if (use_translate)
        {
            if (ImGui::InputFloat3("Translation", &position.x, 3))
                is_edited = true;
        }
        else
            position = glm::vec3(0.0f);

        if (use_rotate)
        {
            if (ImGui::InputFloat3("Rotation", &rotation.x, 3))
                is_edited = true;
        }
        else
            rotation = glm::vec3(0.0f);

        if (use_scale)
        {
            if (ImGui::InputFloat3("Scale", &scale.x, 3))
                is_edited = true;
        }
        else
            scale = glm::vec3(1.0f);

        ImGuizmo::RecomposeMatrixFromComponents(&position.x, &rotation.x, &scale.x, &out_transform[0][0]);

        if (m_current_operation != ImGuizmo::SCALE)
        {
            if (ImGui::RadioButton("Local", m_current_mode == ImGuizmo::LOCAL))
                m_current_mode = ImGuizmo::LOCAL;
            ImGui::SameLine();
            if (ImGui::RadioButton("World", m_current_mode == ImGuizmo::WORLD))
                m_current_mode = ImGuizmo::WORLD;
        }

        ImGui::Checkbox("", &m_use_snap);
        ImGui::SameLine();

        switch (m_current_operation)
        {
            case ImGuizmo::TRANSLATE:
                ImGui::InputFloat3("Snap", &m_snap[0]);
                break;
            case ImGuizmo::ROTATE:
                ImGui::InputFloat("Angle Snap", &m_snap[0]);
                break;
            case ImGuizmo::SCALE:
                ImGui::InputFloat("Scale Snap", &m_snap[0]);
                break;
        }

        if (is_edited)
            transform_node->set_from_local_transform(out_transform);

        pos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(pos.x, pos.y + 25.0f));

        ImGui::Separator();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void create_new_node()
    {
        m_should_add_new_node = false;

        Node::Ptr new_node;

        if (m_node_type_to_add == NODE_MESH)
            new_node = std::shared_ptr<MeshNode>(new MeshNode("New Node " + std::to_string(m_new_node_counter)));
        else if (m_node_type_to_add == NODE_CAMERA)
            new_node = std::shared_ptr<CameraNode>(new CameraNode("New Node " + std::to_string(m_new_node_counter)));
        else if (m_node_type_to_add == NODE_DIRECTIONAL_LIGHT)
            new_node = std::shared_ptr<DirectionalLightNode>(new DirectionalLightNode("New Node " + std::to_string(m_new_node_counter)));
        else if (m_node_type_to_add == NODE_SPOT_LIGHT)
            new_node = std::shared_ptr<SpotLightNode>(new SpotLightNode("New Node " + std::to_string(m_new_node_counter)));
        else if (m_node_type_to_add == NODE_POINT_LIGHT)
            new_node = std::shared_ptr<PointLightNode>(new PointLightNode("New Node " + std::to_string(m_new_node_counter)));
        else if (m_node_type_to_add == NODE_IBL)
            new_node = std::shared_ptr<IBLNode>(new IBLNode("New Node " + std::to_string(m_new_node_counter)));

        m_new_node_counter++;

        if (m_selected_node)
            m_selected_node->add_child(new_node);
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    std::string icon_for_node_type(NodeType type)
    {
        if (type == NODE_MESH)
            return ICON_FA_CUBE;
        else if (type == NODE_CAMERA)
            return ICON_FA_CAMERA;
        else if (type == NODE_DIRECTIONAL_LIGHT)
            return ICON_FA_SUN;
        else if (type == NODE_SPOT_LIGHT)
            return ICON_FA_FLASHLIGHT;
        else if (type == NODE_POINT_LIGHT)
            return ICON_FA_LIGHTBULB;
        else if (type == NODE_IBL)
            return ICON_FA_IMAGE;
        else
            return ICON_FA_SITEMAP;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void populate_ast_transform_node(TransformNode::Ptr node, std::shared_ptr<ast::TransformNode> ast_node)
    {
        ast_node->position = node->local_position();

        glm::vec3 euler_rad = glm::eulerAngles(node->orientation());

        ast_node->rotation = glm::vec3(glm::degrees(euler_rad.x), glm::degrees(euler_rad.y), glm::degrees(euler_rad.z));
        ast_node->scale    = node->scale();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    std::string relative_resource_path(const std::string& path)
    {
        auto current = std::filesystem::current_path();
        std::string cwd     = current.string() + "/assets";
        std::string relative = std::filesystem::relative(path, cwd).string();
        std::replace(relative.begin(), relative.end(), '\\', '/');

        return relative;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    std::shared_ptr<ast::MeshNode> create_ast_mesh_node(MeshNode::Ptr node)
    {
        std::shared_ptr<ast::MeshNode> ast_node = std::shared_ptr<ast::MeshNode>(new ast::MeshNode());

        ast_node->name              = node->name();
        ast_node->type              = ast::SCENE_NODE_MESH;
        ast_node->casts_shadow      = true;
        ast_node->mesh              = node->mesh() ? relative_resource_path(node->mesh()->path()) : "";
        ast_node->material_override = node->material_override() ? relative_resource_path(node->material_override()->path())
                                                                                         : "";

        populate_ast_transform_node(std::dynamic_pointer_cast<TransformNode>(node), std::dynamic_pointer_cast<ast::TransformNode>(ast_node));

        const auto& children = node->children();

        for (auto child : children)
            ast_node->children.push_back(create_ast_node(child));

        return ast_node;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    std::shared_ptr<ast::CameraNode> create_ast_camera_node(CameraNode::Ptr node)
    {
        std::shared_ptr<ast::CameraNode> ast_node = std::shared_ptr<ast::CameraNode>(new ast::CameraNode());

        ast_node->name       = node->name();
        ast_node->type       = ast::SCENE_NODE_CAMERA;
        ast_node->fov        = node->fov();
        ast_node->near_plane = node->near_plane();
        ast_node->far_plane  = node->far_plane();

        populate_ast_transform_node(std::dynamic_pointer_cast<TransformNode>(node), std::dynamic_pointer_cast<ast::TransformNode>(ast_node));

        const auto& children = node->children();

        for (auto child : children)
            ast_node->children.push_back(create_ast_node(child));

        return ast_node;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    std::shared_ptr<ast::DirectionalLightNode> create_ast_directional_light_node(DirectionalLightNode::Ptr node)
    {
        std::shared_ptr<ast::DirectionalLightNode> ast_node = std::shared_ptr<ast::DirectionalLightNode>(new ast::DirectionalLightNode());

        ast_node->name          = node->name();
        ast_node->type          = ast::SCENE_NODE_DIRECTIONAL_LIGHT;
        ast_node->casts_shadows = true;
        ast_node->color         = node->color();
        ast_node->intensity     = node->intensity();
        ast_node->radius        = node->radius();

        populate_ast_transform_node(std::dynamic_pointer_cast<TransformNode>(node), std::dynamic_pointer_cast<ast::TransformNode>(ast_node));

        const auto& children = node->children();

        for (auto child : children)
            ast_node->children.push_back(create_ast_node(child));

        return ast_node;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    std::shared_ptr<ast::SpotLightNode> create_ast_spot_light_node(SpotLightNode::Ptr node)
    {
        std::shared_ptr<ast::SpotLightNode> ast_node = std::shared_ptr<ast::SpotLightNode>(new ast::SpotLightNode());

        ast_node->name             = node->name();
        ast_node->type             = ast::SCENE_NODE_SPOT_LIGHT;
        ast_node->casts_shadows    = true;
        ast_node->color            = node->color();
        ast_node->intensity        = node->intensity();
        ast_node->radius           = node->radius();
        ast_node->inner_cone_angle = node->inner_cone_angle();
        ast_node->outer_cone_angle = node->outer_cone_angle();

        populate_ast_transform_node(std::dynamic_pointer_cast<TransformNode>(node), std::dynamic_pointer_cast<ast::TransformNode>(ast_node));

        const auto& children = node->children();

        for (auto child : children)
            ast_node->children.push_back(create_ast_node(child));

        return ast_node;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    std::shared_ptr<ast::PointLightNode> create_ast_point_light_node(PointLightNode::Ptr node)
    {
        std::shared_ptr<ast::PointLightNode> ast_node = std::shared_ptr<ast::PointLightNode>(new ast::PointLightNode());

        ast_node->name          = node->name();
        ast_node->type          = ast::SCENE_NODE_POINT_LIGHT;
        ast_node->casts_shadows = true;
        ast_node->color         = node->color();
        ast_node->intensity     = node->intensity();
        ast_node->radius        = node->radius();

        populate_ast_transform_node(std::dynamic_pointer_cast<TransformNode>(node), std::dynamic_pointer_cast<ast::TransformNode>(ast_node));

        const auto& children = node->children();

        for (auto child : children)
            ast_node->children.push_back(create_ast_node(child));

        return ast_node;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    std::shared_ptr<ast::IBLNode> create_ast_ibl_node(IBLNode::Ptr node)
    {
        std::shared_ptr<ast::IBLNode> ast_node = std::shared_ptr<ast::IBLNode>(new ast::IBLNode());

        ast_node->name  = node->name();
        ast_node->type  = ast::SCENE_NODE_IBL;
        ast_node->image = node->image() ? relative_resource_path(node->image()->path())
                                                                 : "";

        const auto& children = node->children();

        for (auto child : children)
            ast_node->children.push_back(create_ast_node(child));

        return ast_node;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    std::shared_ptr<ast::TransformNode> create_ast_root_node(RootNode::Ptr node)
    {
        std::shared_ptr<ast::TransformNode> ast_node = std::shared_ptr<ast::TransformNode>(new ast::TransformNode());

        ast_node->name = node->name();
        ast_node->type = ast::SCENE_NODE_ROOT;

        populate_ast_transform_node(std::dynamic_pointer_cast<TransformNode>(node), std::dynamic_pointer_cast<ast::TransformNode>(ast_node));

        const auto& children = node->children();

        for (auto child : children)
            ast_node->children.push_back(create_ast_node(child));

        return ast_node;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    std::shared_ptr<ast::SceneNode> create_ast_node(Node::Ptr node)
    {
        if (node->type() == NODE_MESH)
            return create_ast_mesh_node(std::dynamic_pointer_cast<MeshNode>(node));
        else if (node->type() == NODE_CAMERA)
            return create_ast_camera_node(std::dynamic_pointer_cast<CameraNode>(node));
        else if (node->type() == NODE_DIRECTIONAL_LIGHT)
            return create_ast_directional_light_node(std::dynamic_pointer_cast<DirectionalLightNode>(node));
        else if (node->type() == NODE_SPOT_LIGHT)
            return create_ast_spot_light_node(std::dynamic_pointer_cast<SpotLightNode>(node));
        else if (node->type() == NODE_POINT_LIGHT)
            return create_ast_point_light_node(std::dynamic_pointer_cast<PointLightNode>(node));
        else if (node->type() == NODE_IBL)
            return create_ast_ibl_node(std::dynamic_pointer_cast<IBLNode>(node));
        else if (node->type() == NODE_ROOT)
            return create_ast_root_node(std::dynamic_pointer_cast<RootNode>(node));
        else
            return nullptr;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void save_scene(const std::string& path)
    {
        ast::Scene scene;

        scene.name        = m_scene->name();
        scene.scene_graph = create_ast_node(m_scene->root_node());

        if (!ast::export_scene(scene, path))
            HELIOS_LOG_ERROR("Failed to save Scene!");
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

private:
    ImGuizmo::OPERATION m_current_operation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE      m_current_mode      = ImGuizmo::WORLD;
    RenderState         m_render_state;
    Scene::Ptr          m_scene;
    glm::vec3           m_snap                        = glm::vec3(1.0f);
    bool                m_use_snap                    = false;
    bool                m_show_gui                    = true;
    bool                m_mouse_look                  = false;
    bool                m_ray_debug_mode              = false;
    Node::Ptr           m_selected_node               = nullptr;
    bool                m_should_remove_selected_node = false;
    bool                m_should_add_new_node         = false;
    NodeType            m_node_type_to_add            = NODE_MESH;
    Node*               m_node_to_attach_to           = nullptr;
    float               m_camera_yaw                  = 0.0f;
    float               m_camera_pitch                = 0.0f;
    float               m_heading_speed               = 0.0f;
    float               m_sideways_speed              = 0.0f;
    float               m_camera_sensitivity          = 0.05f;
    float               m_camera_speed                = 50.0f;
    float               m_smooth_frametime            = 0.0f;
    int32_t             m_num_debug_rays              = 32;
    uint32_t            m_new_node_counter            = 0;
    std::string         m_string_buffer;
    CameraNode::Ptr     m_editor_camera;
};
} // namespace helios

HELIOS_DECLARE_MAIN(helios::Editor)
