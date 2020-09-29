#include <application.h>
#include <logger.h>
#include <macros.h>
#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_vulkan.h>
#include <iostream>

namespace lumen
{
// -----------------------------------------------------------------------------------------------------------------------------------

void imgui_vulkan_error_check(VkResult err)
{
    if (err == 0)
        return;

    LUMEN_LOG_ERROR("(Vulkan) Error " + std::to_string(err));

    if (err < 0)
        abort();
}

// -----------------------------------------------------------------------------------------------------------------------------------

Application::Application() :
    m_mouse_x(0.0), m_mouse_y(0.0), m_last_mouse_x(0.0), m_last_mouse_y(0.0),
    m_mouse_delta_x(0.0), m_mouse_delta_y(0.0),
    m_delta_seconds(0.0), m_window(nullptr) {}

// -----------------------------------------------------------------------------------------------------------------------------------

Application::~Application() {}

// -----------------------------------------------------------------------------------------------------------------------------------

int Application::run(int argc, const char* argv[])
{
    if (!init_base(argc, argv))
        return 1;

    while (!exit_requested())
        update_base(m_delta_seconds);

    m_vk_backend->wait_idle();

    shutdown_base();

    return 0;
}

// -----------------------------------------------------------------------------------------------------------------------------------

bool Application::init(int argc, const char* argv[]) { return true; }

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::update(double delta) {}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::shutdown() {}

// -----------------------------------------------------------------------------------------------------------------------------------

bool Application::init_base(int argc, const char* argv[])
{
    logger::initialize();
    logger::open_console_stream();
    logger::open_file_stream();

    // Defaults
    Settings settings = intial_settings();

    bool resizable = settings.resizable;
    bool maximized = settings.maximized;
    m_width        = settings.width;
    m_height       = settings.height;
    m_title        = "Lumen (c) 2020";

    if (glfwInit() != GLFW_TRUE)
    {
        LUMEN_LOG_FATAL("Failed to initialize GLFW");
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, resizable);
    glfwWindowHint(GLFW_MAXIMIZED, maximized);
    glfwWindowHint(GLFW_REFRESH_RATE, 60);

    m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);

    if (!m_window)
    {
        LUMEN_LOG_FATAL("Failed to create GLFW window!");
        return false;
    }

    glfwSetKeyCallback(m_window, key_callback_glfw);
    glfwSetCursorPosCallback(m_window, mouse_callback_glfw);
    glfwSetScrollCallback(m_window, scroll_callback_glfw);
    glfwSetMouseButtonCallback(m_window, mouse_button_callback_glfw);
    glfwSetCharCallback(m_window, char_callback_glfw);
    glfwSetWindowSizeCallback(m_window, window_size_callback_glfw);
    glfwSetWindowUserPointer(m_window, this);

    LUMEN_LOG_INFO("Successfully initialized platform!");

    m_vk_backend = vk::Backend::create(m_window,
#if defined(_DEBUG)
                                       true
#else
                                       false
#endif
                                       ,
                                       true,
                                       settings.device_extensions,
                                       settings.device_pnext);

    m_image_available_semaphores.resize(vk::Backend::kMaxFramesInFlight);
    m_render_finished_semaphores.resize(vk::Backend::kMaxFramesInFlight);

    for (size_t i = 0; i < vk::Backend::kMaxFramesInFlight; i++)
    {
        m_image_available_semaphores[i] = vk::Semaphore::create(m_vk_backend);
        m_render_finished_semaphores[i] = vk::Semaphore::create(m_vk_backend);
    }

    ImGui::CreateContext();

    ImGui_ImplGlfw_InitForVulkan(m_window, false);

    ImGui_ImplVulkan_InitInfo init_info = {};

    init_info.Instance        = m_vk_backend->instance();
    init_info.PhysicalDevice  = m_vk_backend->physical_device();
    init_info.Device          = m_vk_backend->device();
    init_info.QueueFamily     = m_vk_backend->queue_infos().graphics_queue_index;
    init_info.Queue           = m_vk_backend->graphics_queue();
    init_info.PipelineCache   = nullptr;
    init_info.DescriptorPool  = m_vk_backend->thread_local_descriptor_pool()->handle();
    init_info.Allocator       = nullptr;
    init_info.MinImageCount   = 2;
    init_info.ImageCount      = m_vk_backend->swap_image_count();
    init_info.CheckVkResultFn = nullptr;

    ImGui_ImplVulkan_Init(&init_info, m_vk_backend->swapchain_render_pass()->handle());

    vk::CommandBuffer::Ptr cmd_buf = m_vk_backend->allocate_graphics_command_buffer();

    VkCommandBufferBeginInfo begin_info;
    LUMEN_ZERO_MEMORY(begin_info);

    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vkBeginCommandBuffer(cmd_buf->handle(), &begin_info);

    ImGui_ImplVulkan_CreateFontsTexture(cmd_buf->handle());

    vkEndCommandBuffer(cmd_buf->handle());

    m_vk_backend->flush_graphics({ cmd_buf });

    ImGui_ImplVulkan_DestroyFontUploadObjects();

    ImGui::StyleColorsDark();

    GLFWmonitor* primary = glfwGetPrimaryMonitor();

    float xscale, yscale;
    glfwGetMonitorContentScale(primary, &xscale, &yscale);

    ImGuiStyle* style = &ImGui::GetStyle();

    style->ScaleAllSizes(xscale > yscale ? xscale : yscale);

    ImGuiIO& io        = ImGui::GetIO();
    io.FontGlobalScale = xscale > yscale ? xscale : yscale;

    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    m_width  = display_w;
    m_height = display_h;

    if (!init(argc, argv))
        return false;

    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::update_base(double delta)
{
    begin_frame();
    update(delta);
    end_frame();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::shutdown_base()
{
    // Execute user-side shutdown method.
    shutdown();

    // Shutdown ImGui.
    ImGui_ImplVulkan_Shutdown();

    for (int i = 0; i < vk::Backend::kMaxFramesInFlight; i++)
    {
        m_image_available_semaphores[i].reset();
        m_render_finished_semaphores[i].reset();
    }

    m_vk_backend->~Backend();

    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Shutdown GLFW.
    glfwDestroyWindow(m_window);
    glfwTerminate();

    // Close logger streams.
    logger::close_file_stream();
    logger::close_console_stream();
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::render_gui(vk::CommandBuffer::Ptr cmd_buf)
{
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd_buf->handle());
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::submit_and_present(const std::vector<vk::CommandBuffer::Ptr>& cmd_bufs)
{
    m_vk_backend->submit_graphics(cmd_bufs,
                                  { m_image_available_semaphores[m_vk_backend->current_frame_idx()] },
                                  { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
                                  { m_render_finished_semaphores[m_vk_backend->current_frame_idx()] });

    m_vk_backend->present({ m_render_finished_semaphores[m_vk_backend->current_frame_idx()] });
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::begin_frame()
{
    m_time_start = glfwGetTime();

    glfwPollEvents();

    if (m_should_recreate_swap_chain)
    {
        m_vk_backend->recreate_swapchain();
        m_should_recreate_swap_chain = false;
    }

    m_vk_backend->acquire_next_swap_chain_image(m_image_available_semaphores[m_vk_backend->current_frame_idx()]);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    m_mouse_delta_x = m_mouse_x - m_last_mouse_x;
    m_mouse_delta_y = m_mouse_y - m_last_mouse_y;

    m_last_mouse_x = m_mouse_x;
    m_last_mouse_y = m_mouse_y;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::end_frame()
{
    m_delta_seconds = glfwGetTime() - m_delta_seconds;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::request_exit() const
{
    glfwSetWindowShouldClose(m_window, true);
}

// -----------------------------------------------------------------------------------------------------------------------------------

bool Application::exit_requested() const
{
    return glfwWindowShouldClose(m_window);
}

// -----------------------------------------------------------------------------------------------------------------------------------

Settings Application::intial_settings() { return Settings(); }

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::window_resized(int width, int height) {}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::key_pressed(int code) {}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::key_released(int code) {}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::mouse_scrolled(double xoffset, double yoffset) {}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::mouse_pressed(int code) {}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::mouse_released(int code) {}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::mouse_move(double x, double y, double deltaX, double deltaY) {}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    if (key >= 0 && key < MAX_KEYS)
    {
        if (action == GLFW_PRESS)
        {
            key_pressed(key);
            m_keys[key] = true;
        }
        else if (action == GLFW_RELEASE)
        {
            key_released(key);
            m_keys[key] = false;
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    m_mouse_x = xpos;
    m_mouse_y = ypos;
    mouse_move(xpos, ypos, m_mouse_delta_x, m_mouse_delta_y);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    mouse_scrolled(xoffset, yoffset);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button >= 0 && button < MAX_MOUSE_BUTTONS)
    {
        if (action == GLFW_PRESS)
        {
            mouse_pressed(button);
            m_mouse_buttons[button] = true;
        }
        else if (action == GLFW_RELEASE)
        {
            mouse_released(button);
            m_mouse_buttons[button] = false;
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::window_size_callback(GLFWwindow* window, int width, int height)
{
    m_width  = width;
    m_height = height;

    m_should_recreate_swap_chain = true;
    window_resized(width, height);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::key_callback_glfw(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mode);
    Application* app = (Application*)glfwGetWindowUserPointer(window);
    app->key_callback(window, key, scancode, action, mode);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::mouse_callback_glfw(GLFWwindow* window, double xpos, double ypos)
{
    Application* app = (Application*)glfwGetWindowUserPointer(window);
    app->mouse_callback(window, xpos, ypos);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::scroll_callback_glfw(GLFWwindow* window, double xoffset, double yoffset)
{
    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
    Application* app = (Application*)glfwGetWindowUserPointer(window);
    app->scroll_callback(window, xoffset, yoffset);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::mouse_button_callback_glfw(GLFWwindow* window, int button, int action, int mods)
{
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    Application* app = (Application*)glfwGetWindowUserPointer(window);
    app->mouse_button_callback(window, button, action, mods);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::char_callback_glfw(GLFWwindow* window, unsigned int c)
{
    ImGui_ImplGlfw_CharCallback(window, c);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::window_size_callback_glfw(GLFWwindow* window, int width, int height)
{
    Application* app = (Application*)glfwGetWindowUserPointer(window);
    app->window_size_callback(window, width, height);
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace lumen
