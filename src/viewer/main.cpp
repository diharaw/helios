#include <core/application.h>
#include <utility/macros.h>
#include <imgui_internal.h>
#include <utility/imgui_plot.h>
#include <utility/profiler.h>
#include <filesystem>
#include <nfd.h>

namespace helios
{
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

        m_render_state.setup(m_width, m_height, cmd_buffer);

        if (m_scene)
            m_scene->update(m_render_state);

        m_renderer->render(m_render_state);
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void gui()
    {
        if (!m_show_gui)
            return;

        auto extents = m_vk_backend->swap_chain_extents();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        bool             open         = true;
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(m_width * 0.3f, m_height));

        ImGui::Begin("Viewer", &open, window_flags);

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

        ImGui::End();

        ImGui::PopStyleVar();
        ImGui::PopStyleVar();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void shutdown() override
    {
        m_scene.reset();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void key_pressed(int code) override
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
        // Enable mouse look.
        if (code == GLFW_MOUSE_BUTTON_RIGHT)
            m_mouse_look = true;
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
        settings.title  = "Helios Viewer";

        return settings;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

private:
    void update_camera()
    {
        if (m_scene)
        {
            CameraNode::Ptr camera = m_scene->find_camera();

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

private:
    RenderState     m_render_state;
    Scene::Ptr      m_scene;
    bool            m_show_gui           = true;
    bool            m_mouse_look         = false;
    float           m_camera_yaw         = 0.0f;
    float           m_camera_pitch       = 0.0f;
    float           m_heading_speed      = 0.0f;
    float           m_sideways_speed     = 0.0f;
    float           m_camera_sensitivity = 0.05f;
    float           m_camera_speed       = 5.0f;
    float           m_smooth_frametime   = 0.0f;
    int32_t         m_num_debug_rays     = 32;
    std::string     m_string_buffer;
    CameraNode::Ptr m_current_camera;
};
} // namespace helios

HELIOS_DECLARE_MAIN(helios::Viewer)
