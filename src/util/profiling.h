#ifndef NEON_OUBLIETTE_PROFILING_H
#define NEON_OUBLIETTE_PROFILING_H

/**
 * @file profiling.h
 * @brief Wrapper for Tracy profiler macros.
 * 
 * This header allows the project to be instrumented with Tracy macros while
 * remaining compilable when TRACY_ENABLE is NOT defined.
 */

#ifdef TRACY_ENABLE
    #include <tracy/Tracy.hpp>
#else
    // Define empty macros if Tracy is disabled
    #define ZoneScoped
    #define ZoneScopedN(name)
    #define FrameMark
    #define ZoneName(name, size)
    #define ZoneText(text, size)
    #define ZoneValue(value)
#endif

#endif // NEON_OUBLIETTE_PROFILING_H
