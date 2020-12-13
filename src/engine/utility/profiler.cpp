#include <imgui.h>
#include <Windows.h>
#include <utility/macros.h>
#include <utility/profiler.h>
#include <stack>
#include <vector>

#define BUFFER_COUNT 3
#define MAX_SAMPLES 100

namespace helios
{
namespace profiler
{
// -----------------------------------------------------------------------------------------------------------------------------------

struct Profiler
{
    struct Sample
    {
        std::string name;
        uint32_t query_index;
        bool    start = true;
        double  cpu_time;
        Sample* end_sample;
    };

    struct Buffer
    {
        std::vector<std::unique_ptr<Sample>> samples;
        int32_t                              index = 0;
        vk::QueryPool::Ptr query_pool;
        uint32_t           query_index = 0;

        Buffer()
        {
            samples.resize(MAX_SAMPLES);

            for (uint32_t i = 0; i < MAX_SAMPLES; i++)
                samples[i] = nullptr;
        }
    };

    // -----------------------------------------------------------------------------------------------------------------------------------

    Profiler(vk::Backend::Ptr backend)
    {
#ifdef WIN32
        QueryPerformanceFrequency(&m_frequency);
#endif

        for (int i = 0; i < BUFFER_COUNT; i++)
            m_sample_buffers[i].query_pool = vk::QueryPool::create(backend, VK_QUERY_TYPE_TIMESTAMP, MAX_SAMPLES);

        m_should_reset = true;
        m_read_buffer_idx++;
        m_write_buffer_idx++;
        m_cmd_buf = nullptr;

        if (m_read_buffer_idx == BUFFER_COUNT)
            m_read_buffer_idx = 0;

        if (m_write_buffer_idx == BUFFER_COUNT)
            m_write_buffer_idx = 0;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    ~Profiler()
    {
        for (int i = 0; i < BUFFER_COUNT; i++)
            m_sample_buffers[i].query_pool.reset();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void begin_sample(std::string name)
    {
        if (m_should_reset)
        {
            m_sample_buffers[m_write_buffer_idx].query_index = 0;
            vkCmdResetQueryPool(m_cmd_buf->handle(), m_sample_buffers[m_write_buffer_idx].query_pool->handle(), 0, MAX_SAMPLES);
            m_should_reset = false;
        }

        int32_t idx = m_sample_buffers[m_write_buffer_idx].index++;

        if (!m_sample_buffers[m_write_buffer_idx].samples[idx])
            m_sample_buffers[m_write_buffer_idx].samples[idx] = std::make_unique<Sample>();

        auto& sample = m_sample_buffers[m_write_buffer_idx].samples[idx];

        sample->name = name;
        sample->query_index = m_sample_buffers[m_write_buffer_idx].query_index++;
        vkCmdWriteTimestamp(m_cmd_buf->handle(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_sample_buffers[m_write_buffer_idx].query_pool->handle(), sample->query_index);

        sample->end_sample = nullptr;
        sample->start      = true;

#ifdef WIN32
        LARGE_INTEGER cpu_time;
        QueryPerformanceCounter(&cpu_time);
        sample->cpu_time = cpu_time.QuadPart * (1000000.0 / m_frequency.QuadPart);
#else
        timeval cpu_time;
        gettimeofday(&cpu_time, nullptr);
        sample->cpu_time = (cpu_time.tv_sec * 1000000.0) + cpu_time.tv_usec;
#endif

        m_sample_stack.push(sample.get());
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void end_sample(std::string name)
    {
        int32_t idx = m_sample_buffers[m_write_buffer_idx].index++;

        if (!m_sample_buffers[m_write_buffer_idx].samples[idx])
            m_sample_buffers[m_write_buffer_idx].samples[idx] = std::make_unique<Sample>();

        auto& sample = m_sample_buffers[m_write_buffer_idx].samples[idx];

        sample->name  = name;
        sample->start = false;
        sample->query_index = m_sample_buffers[m_write_buffer_idx].query_index++;
        vkCmdWriteTimestamp(m_cmd_buf->handle(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_sample_buffers[m_write_buffer_idx].query_pool->handle(), sample->query_index);

        sample->end_sample = nullptr;

#ifdef WIN32
        LARGE_INTEGER cpu_time;
        QueryPerformanceCounter(&cpu_time);
        sample->cpu_time = cpu_time.QuadPart * (1000000.0 / m_frequency.QuadPart);
#else
        timeval cpu_time;
        gettimeofday(&cpu_time, nullptr);
        sample->cpu_time = (cpu_time.tv_sec * 1000000.0) + cpu_time.tv_usec;
#endif

        Sample* start = m_sample_stack.top();

        start->end_sample = sample.get();

        m_sample_stack.pop();
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void begin_frame(vk::CommandBuffer::Ptr cmd_buf)
    {
        m_cmd_buf = cmd_buf;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void end_frame()
    {
        if (m_read_buffer_idx >= 0)
            m_sample_buffers[m_read_buffer_idx].index = 0;

        m_should_reset = true;
        m_read_buffer_idx++;
        m_write_buffer_idx++;
        m_cmd_buf = nullptr;

        if (m_read_buffer_idx == BUFFER_COUNT)
            m_read_buffer_idx = 0;

        if (m_write_buffer_idx == BUFFER_COUNT)
            m_write_buffer_idx = 0;
    }

    // -----------------------------------------------------------------------------------------------------------------------------------

    void ui()
    {
        if (m_read_buffer_idx >= 0)
        {
            for (int32_t i = 0; i < m_sample_buffers[m_read_buffer_idx].index; i++)
            {
                auto& sample = m_sample_buffers[m_read_buffer_idx].samples[i];

                if (sample->start)
                {
                    if (!m_should_pop_stack.empty())
                    {
                        if (!m_should_pop_stack.top())
                        {
                            m_should_pop_stack.push(false);
                            continue;
                        }
                    }

                    std::string id = std::to_string(i);

                    uint64_t start_time = 0;
                    uint64_t end_time   = 0;

                    m_sample_buffers[m_read_buffer_idx].query_pool->results(sample->query_index, 1, sizeof(uint64_t), &start_time, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
                    m_sample_buffers[m_read_buffer_idx].query_pool->results(sample->end_sample->query_index, 1, sizeof(uint64_t), &end_time, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    
                    uint64_t gpu_time_diff = end_time - start_time;

                    float gpu_time = float(gpu_time_diff / 1000000.0);
                    float cpu_time = (sample->end_sample->cpu_time - sample->cpu_time) * 0.001f;

                    if (ImGui::TreeNode(id.c_str(), "%s | %f ms (CPU) | %f ms (GPU)", sample->name.c_str(), cpu_time, gpu_time))
                        m_should_pop_stack.push(true);
                    else
                        m_should_pop_stack.push(false);
                }
                else
                {
                    if (!m_should_pop_stack.empty())
                    {
                        bool should_pop = m_should_pop_stack.top();
                        m_should_pop_stack.pop();

                        if (should_pop)
                            ImGui::TreePop();
                    }
                }
            }
        }
    }

// -----------------------------------------------------------------------------------------------------------------------------------

    int32_t             m_read_buffer_idx  = -BUFFER_COUNT;
    int32_t             m_write_buffer_idx = -1;
    Buffer              m_sample_buffers[BUFFER_COUNT];
    std::stack<Sample*> m_sample_stack;
    std::stack<bool>    m_should_pop_stack;
    vk::CommandBuffer::Ptr m_cmd_buf      = nullptr;
    bool m_should_reset = true;
#ifdef WIN32
    LARGE_INTEGER m_frequency;
#endif
};

Profiler* g_profiler = nullptr;

// -----------------------------------------------------------------------------------------------------------------------------------

ScopedProfile::ScopedProfile(std::string name) :
    m_name(name)
{
    begin_sample(m_name);
}

// -----------------------------------------------------------------------------------------------------------------------------------

ScopedProfile::~ScopedProfile()
{
    end_sample(m_name);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void initialize(vk::Backend::Ptr backend)
{
    g_profiler = new Profiler(backend);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void shutdown() { HELIOS_SAFE_DELETE(g_profiler); }

// -----------------------------------------------------------------------------------------------------------------------------------

void begin_sample(std::string name)
{
    g_profiler->begin_sample(name);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void end_sample(std::string name)
{
    g_profiler->end_sample(name);
}

// -----------------------------------------------------------------------------------------------------------------------------------

void begin_frame(vk::CommandBuffer::Ptr cmd_buf) { g_profiler->begin_frame(cmd_buf); }

// -----------------------------------------------------------------------------------------------------------------------------------

void end_frame() { g_profiler->end_frame(); }

// -----------------------------------------------------------------------------------------------------------------------------------

void ui()
{
    g_profiler->ui();
}

// -----------------------------------------------------------------------------------------------------------------------------------
} // namespace profiler
} // namespace helios