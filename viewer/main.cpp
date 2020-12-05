#include <core/application.h>
#include <utility/macros.h>
#include <integrator/path.h>
#include <imgui_internal.h>

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
class Viewer : public Application
{
protected:
    // -----------------------------------------------------------------------------------------------------------------------------------

    bool init(int argc, const char* argv[]) override
    {
        m_path_integrator = std::shared_ptr<PathIntegrator>(new PathIntegrator(m_vk_backend));
        m_scene           = m_resource_manager->load_scene("scene/pica_pica_ibl.json");

        return true;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void update(double delta) override
    {
        if (m_show_gui)
            gui();

        update_camera();

        vk::CommandBuffer::Ptr cmd_buf = m_vk_backend->allocate_graphics_command_buffer();

        VkCommandBufferBeginInfo begin_info;
        HELIOS_ZERO_MEMORY(begin_info);

        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkBeginCommandBuffer(cmd_buf->handle(), &begin_info);

        m_render_state.setup(cmd_buf);

        m_scene->update(m_render_state);

        m_renderer->render(m_render_state, m_path_integrator);

        vkEndCommandBuffer(cmd_buf->handle());

        submit_and_present({ cmd_buf });
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void shutdown() override
    {
        m_path_integrator.reset();
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

    void gui()
    {
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
            char scene[128] = "assets/scene/cornell_box.scene";

            ImGui::InputText("##Scene", &scene[0], 128);
            ImGui::SameLine();
            ImGui::Button("Browse...");

            ImVec2 region = ImGui::GetContentRegionAvail();

            ImGui::Button("Save", ImVec2(region.x, 30.0f));
        }
        if (ImGui::CollapsingHeader("Hierarchy"))
        {
        }
        if (ImGui::CollapsingHeader("Inspector"))
        {
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
        {
        }
        if (ImGui::CollapsingHeader("Settings"))
        {
        }

        if (m_ray_debug_mode)
            ImGui::PopDisabled();

        ImGui::End();

        ImGui::PopStyleVar();
        ImGui::PopStyleVar();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

private:
    RenderState         m_render_state;
    Scene::Ptr          m_scene;
    PathIntegrator::Ptr m_path_integrator;
    bool                m_show_gui           = true;
    bool                m_mouse_look         = false;
    bool                m_ray_debug_mode     = false;
    float               m_camera_yaw         = 0.0f;
    float               m_camera_pitch       = 0.0f;
    float               m_heading_speed      = 0.0f;
    float               m_sideways_speed     = 0.0f;
    float               m_camera_sensitivity = 0.05f;
    float               m_camera_speed       = 0.02f;
    int32_t             m_num_debug_rays     = 32;
};
} // namespace helios

HELIOS_DECLARE_MAIN(helios::Viewer)
