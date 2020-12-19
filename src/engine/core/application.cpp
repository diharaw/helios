#include <core/application.h>
#include <utility/logger.h>
#include <utility/macros.h>
#include <utility/profiler.h>
#include <IconsFontAwesome5Pro.h>
#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_vulkan.h>
#include <iostream>

namespace helios
{
// -----------------------------------------------------------------------------------------------------------------------------------

void imgui_vulkan_error_check(VkResult err)
{
    if (err == 0)
        return;

    HELIOS_LOG_ERROR("(Vulkan) Error " + std::to_string(err));

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
    {
        shutdown_base();
        return 1;
    }

    while (!exit_requested())
        update_base(m_delta_seconds);

    m_vk_backend->wait_idle();

    shutdown_base();

    return 0;
}

// -----------------------------------------------------------------------------------------------------------------------------------

bool Application::init(int argc, const char* argv[]) { return true; }

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::update(vk::CommandBuffer::Ptr cmd_buffer) {}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::gui() {}

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
    m_title        = settings.title;

    if (glfwInit() != GLFW_TRUE)
    {
        HELIOS_LOG_FATAL("Failed to initialize GLFW");
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, resizable);
    glfwWindowHint(GLFW_MAXIMIZED, maximized);
    glfwWindowHint(GLFW_REFRESH_RATE, 60);

    m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);

    if (!m_window)
    {
        HELIOS_LOG_FATAL("Failed to create GLFW window!");
        return false;
    }

    glfwSetKeyCallback(m_window, key_callback_glfw);
    glfwSetCursorPosCallback(m_window, mouse_callback_glfw);
    glfwSetScrollCallback(m_window, scroll_callback_glfw);
    glfwSetMouseButtonCallback(m_window, mouse_button_callback_glfw);
    glfwSetCharCallback(m_window, char_callback_glfw);
    glfwSetWindowSizeCallback(m_window, window_size_callback_glfw);
    glfwSetWindowIconifyCallback(m_window, window_iconify_callback_glfw);
    glfwSetWindowUserPointer(m_window, this);

    HELIOS_LOG_INFO("Successfully initialized platform!");

    m_vk_backend = vk::Backend::create(m_window,
#if defined(_DEBUG)
                                       true
#else
                                       false
#endif
                                       ,
                                       true,
                                       {});

    m_renderer         = std::unique_ptr<Renderer>(new Renderer(m_vk_backend));
    m_resource_manager = std::unique_ptr<ResourceManager>(new ResourceManager(m_vk_backend));

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

    ImGui_ImplVulkan_Init(&init_info, m_renderer->swapchain_renderpass()->handle());

    GLFWmonitor* primary = glfwGetPrimaryMonitor();

    float xscale, yscale;
    glfwGetMonitorContentScale(primary, &xscale, &yscale);

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto-Medium.ttf", 16.0f);

    ImGuiStyle* style = &ImGui::GetStyle();

    style->ScaleAllSizes(xscale > yscale ? xscale : yscale);

    io.FontGlobalScale = xscale > yscale ? xscale : yscale;

    // merge in icons from Font Awesome
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    ImFontConfig         icons_config;
    icons_config.MergeMode  = true;
    icons_config.PixelSnapH = true;

    std::string font_awesome_pro_path = "assets/fonts/" + std::string(FONT_ICON_FILE_NAME_FAR);

    io.Fonts->AddFontFromFileTTF(font_awesome_pro_path.c_str(), 16.0f, &icons_config, icons_ranges);

    vk::CommandBuffer::Ptr cmd_buf = m_vk_backend->allocate_graphics_command_buffer();

    VkCommandBufferBeginInfo begin_info;
    HELIOS_ZERO_MEMORY(begin_info);

    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vkBeginCommandBuffer(cmd_buf->handle(), &begin_info);

    ImGui_ImplVulkan_CreateFontsTexture(cmd_buf->handle());

    vkEndCommandBuffer(cmd_buf->handle());

    m_vk_backend->flush_graphics({ cmd_buf });

    ImGui_ImplVulkan_DestroyFontUploadObjects();

    ImGui::StyleColorsDark();

    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    m_width  = display_w;
    m_height = display_h;

    ImVec4* colors = style->Colors;

    colors[ImGuiCol_Text]                  = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.500f, 0.500f, 0.500f, 1.000f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.180f, 0.180f, 0.180f, 1.000f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.280f, 0.280f, 0.280f, 0.000f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.313f, 0.313f, 0.313f, 1.000f);
    colors[ImGuiCol_Border]                = ImVec4(0.266f, 0.266f, 0.266f, 1.000f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.000f, 0.000f, 0.000f, 0.000f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.160f, 0.160f, 0.160f, 1.000f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.200f, 0.200f, 0.200f, 1.000f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.280f, 0.280f, 0.280f, 1.000f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.148f, 0.148f, 0.148f, 1.000f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.148f, 0.148f, 0.148f, 1.000f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.148f, 0.148f, 0.148f, 1.000f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.195f, 0.195f, 0.195f, 1.000f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.160f, 0.160f, 0.160f, 1.000f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.277f, 0.277f, 0.277f, 1.000f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.300f, 0.300f, 0.300f, 1.000f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
    colors[ImGuiCol_CheckMark]             = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.391f, 0.391f, 0.391f, 1.000f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
    colors[ImGuiCol_Button]                = ImVec4(1.000f, 1.000f, 1.000f, 0.000f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(1.000f, 1.000f, 1.000f, 0.156f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(1.000f, 1.000f, 1.000f, 0.391f);
    colors[ImGuiCol_Header]                = ImVec4(0.313f, 0.313f, 0.313f, 1.000f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.469f, 0.469f, 0.469f, 1.000f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.469f, 0.469f, 0.469f, 1.000f);
    colors[ImGuiCol_Separator]             = colors[ImGuiCol_Border];
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.391f, 0.391f, 0.391f, 1.000f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(1.000f, 1.000f, 1.000f, 0.250f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(1.000f, 1.000f, 1.000f, 0.670f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.098f, 0.098f, 0.098f, 1.000f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.352f, 0.352f, 0.352f, 1.000f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.195f, 0.195f, 0.195f, 1.000f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.098f, 0.098f, 0.098f, 1.000f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.195f, 0.195f, 0.195f, 1.000f);
    colors[ImGuiCol_PlotLines]             = ImVec4(0.469f, 0.469f, 0.469f, 1.000f);
    colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.586f, 0.586f, 0.586f, 1.000f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
    colors[ImGuiCol_TextSelectedBg]        = ImVec4(1.000f, 1.000f, 1.000f, 0.156f);
    colors[ImGuiCol_DragDropTarget]        = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
    colors[ImGuiCol_NavHighlight]          = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.000f, 0.391f, 0.000f, 1.000f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.000f, 0.000f, 0.000f, 0.586f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.000f, 0.000f, 0.000f, 0.586f);

    style->ChildRounding     = 4.0f;
    style->FrameBorderSize   = 1.0f;
    style->FrameRounding     = 2.0f;
    style->GrabMinSize       = 7.0f;
    style->PopupRounding     = 2.0f;
    style->ScrollbarRounding = 12.0f;
    style->ScrollbarSize     = 13.0f;
    style->TabBorderSize     = 1.0f;
    style->TabRounding       = 0.0f;
    style->WindowRounding    = 4.0f;

    profiler::initialize(m_vk_backend);

    if (!init(argc, argv))
        return false;

    return true;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::update_base(double delta)
{
    if (handle_events())
    {
        vk::CommandBuffer::Ptr cmd_buffer = begin_frame();
        update(cmd_buffer);
        end_frame(cmd_buffer);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::shutdown_base()
{
    // Execute user-side shutdown method.
    shutdown();

    profiler::shutdown();

    // Shutdown ImGui.
    ImGui_ImplVulkan_Shutdown();

    for (int i = 0; i < vk::Backend::kMaxFramesInFlight; i++)
    {
        m_image_available_semaphores[i].reset();
        m_render_finished_semaphores[i].reset();
    }

    m_resource_manager.reset();
    m_renderer.reset();
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

void Application::submit_and_present(const std::vector<vk::CommandBuffer::Ptr>& cmd_bufs)
{
    m_vk_backend->submit_graphics(cmd_bufs,
                                  { m_image_available_semaphores[m_vk_backend->current_frame_idx()] },
                                  { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
                                  { m_render_finished_semaphores[m_vk_backend->current_frame_idx()] });

    m_vk_backend->present({ m_render_finished_semaphores[m_vk_backend->current_frame_idx()] });
}

// -----------------------------------------------------------------------------------------------------------------------------------

bool Application::handle_events()
{
    glfwPollEvents();

    if (!m_window_minimized && m_should_recreate_swap_chain)
    {
        m_vk_backend->recreate_swapchain();
        m_should_recreate_swap_chain = false;
    }

    if (m_window_resize_in_progress)
    {
        if (m_width == m_last_width && m_height == m_last_height)
        {
            m_window_resize_in_progress = false;
            window_resized();
        }

        m_last_width  = m_width;
        m_last_height = m_height;
    }

    return !m_window_minimized && !m_window_resize_in_progress;
}

// -----------------------------------------------------------------------------------------------------------------------------------

vk::CommandBuffer::Ptr Application::begin_frame()
{
    m_time_start = glfwGetTime();

    m_vk_backend->acquire_next_swap_chain_image(m_image_available_semaphores[m_vk_backend->current_frame_idx()]);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    m_mouse_delta_x = m_mouse_x - m_last_mouse_x;
    m_mouse_delta_y = m_mouse_y - m_last_mouse_y;

    m_last_mouse_x = m_mouse_x;
    m_last_mouse_y = m_mouse_y;

    vk::CommandBuffer::Ptr cmd_buf = m_vk_backend->allocate_graphics_command_buffer();

    profiler::begin_frame(cmd_buf);

    gui();

    VkCommandBufferBeginInfo begin_info;
    HELIOS_ZERO_MEMORY(begin_info);

    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vkBeginCommandBuffer(cmd_buf->handle(), &begin_info);

    profiler::begin_sample("Update");

    return cmd_buf;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::end_frame(vk::CommandBuffer::Ptr cmd_buffer)
{
    profiler::end_sample("Update");

    profiler::end_frame();

    vkEndCommandBuffer(cmd_buffer->handle());

    submit_and_present({ cmd_buffer });

    m_delta_seconds = glfwGetTime() - m_time_start;
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

void Application::window_resized() {}

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
    m_window_resize_in_progress  = true;
}

// -----------------------------------------------------------------------------------------------------------------------------------

void Application::window_iconify_callback(GLFWwindow* window, int iconified)
{
    m_window_minimized = (bool)iconified;
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

void Application::window_iconify_callback_glfw(GLFWwindow* window, int iconified)
{
    Application* app = (Application*)glfwGetWindowUserPointer(window);
    app->window_iconify_callback(window, iconified);
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace helios
