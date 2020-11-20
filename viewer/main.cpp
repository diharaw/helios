#include <core/application.h>
#include <utility/macros.h>
#include <integrator/path.h>

namespace lumen
{
class Viewer : public Application
{
protected:
    // -----------------------------------------------------------------------------------------------------------------------------------

    bool init(int argc, const char* argv[]) override
    {
        m_path_integrator = std::shared_ptr<PathIntegrator>(new PathIntegrator(m_vk_backend));
        m_scene           = m_resource_manager->load_scene("scene/pica_pica_no_ibl.json");

        return true;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void update(double delta) override
    {
        gui();

        update_camera();

        vk::CommandBuffer::Ptr cmd_buf = m_vk_backend->allocate_graphics_command_buffer();

        VkCommandBufferBeginInfo begin_info;
        LUMEN_ZERO_MEMORY(begin_info);

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

                camera->move(camera->camera_forward() * forward_delta);
                camera->move(camera->camera_left() * sideways_delta);

                if (m_mouse_look)
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

        ImGui::SetNextWindowSize(ImVec2(m_width * 0.3f, m_height));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

        ImGui::Begin("Editor", &open, window_flags);

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
        if (ImGui::CollapsingHeader("Profiler"))
        {
        }
        if (ImGui::CollapsingHeader("Settings"))
        {
        }

        ImGui::End();

        ImGui::PopStyleVar();
        ImGui::PopStyleVar();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

private:
    RenderState         m_render_state;
    Scene::Ptr          m_scene;
    PathIntegrator::Ptr m_path_integrator;
    bool                m_mouse_look         = false;
    float               m_camera_yaw         = 0.0f;
    float               m_camera_pitch       = 0.0f;
    float               m_heading_speed      = 0.0f;
    float               m_sideways_speed     = 0.0f;
    float               m_camera_sensitivity = 0.05f;
    float               m_camera_speed       = 0.02f;
};
} // namespace lumen

LUMEN_DECLARE_MAIN(lumen::Viewer)
