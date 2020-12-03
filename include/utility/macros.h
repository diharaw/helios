#pragma once

#if defined(_MSC_VER)
#    define HELIOS_ALIGNED(x) __declspec(align(x))
#else
#    if defined(__GNUC__) || defined(__clang__)
#        define HELIOS_ALIGNED(x) __attribute__((aligned(x)))
#    endif
#endif

#define HELIOS_ZERO_MEMORY(x) memset(&x, 0, sizeof(x))

#define HELIOS_SAFE_DELETE(x) \
    if (x)                   \
    {                        \
        delete x;            \
        x = nullptr;         \
    }
#define HELIOS_SAFE_DELETE_ARRAY(x) \
    if (x)                         \
    {                              \
        delete[] x;                \
        x = nullptr;               \
    }