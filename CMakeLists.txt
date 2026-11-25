cmake_minimum_required(VERSION 3.25)

project(BrimstoneBass VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The core does not throw or use RTTI.
function(bb_core_flags target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /GR- /EHs-c- /W4)
        target_compile_definitions(${target} PRIVATE _HAS_EXCEPTIONS=0 _USE_MATH_DEFINES)
    else()
        target_compile_options(${target} PRIVATE -fno-exceptions -fno-rtti -Wall -Wextra)
    endif()
endfunction()

# Shared effects (Delay, Reverb, Filter, Unison) and the stereo Sample type.
include(FetchContent)
FetchContent_Declare(ivandaw_engine_core
    GIT_REPOSITORY https://github.com/zhirnovcodes/ivan-daw-engine-core.git
    GIT_TAG v0.1.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(ivandaw_engine_core)

# params.json is the one parameter list. CMake writes it out as a C++ header at configure time
# (ids as an enum, ranges and defaults as a table), so the core can never disagree with it.
# Editing params.json re-runs configure.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/params.json)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/params.json BB_PARAMS_JSON)
string(JSON bb_param_count LENGTH "${BB_PARAMS_JSON}" params)
math(EXPR bb_param_last "${bb_param_count} - 1")
set(bb_param_enum "")
set(bb_param_rows "")
foreach(index RANGE ${bb_param_last})
    string(JSON param_id GET "${BB_PARAMS_JSON}" params ${index} id)
    string(JSON param_kind GET "${BB_PARAMS_JSON}" params ${index} kind)
    string(JSON param_min GET "${BB_PARAMS_JSON}" params ${index} min)
    string(JSON param_max GET "${BB_PARAMS_JSON}" params ${index} max)
    string(JSON param_default GET "${BB_PARAMS_JSON}" params ${index} default)
    string(JSON param_lfo ERROR_VARIABLE param_lfo_error GET "${BB_PARAMS_JSON}" params ${index} lfo)
    if(param_lfo_error OR NOT param_lfo)
        set(param_lfo false)
    else()
        set(param_lfo true)
    endif()
    if(param_kind STREQUAL "float")
        set(param_kind_cpp Float)
    elseif(param_kind STREQUAL "int")
        set(param_kind_cpp Int)
    elseif(param_kind STREQUAL "boolean")
        set(param_kind_cpp Boolean)
    elseif(param_kind STREQUAL "choice")
        set(param_kind_cpp Choice)
    elseif(param_kind STREQUAL "trigger")
        set(param_kind_cpp Trigger)
    else()
        message(FATAL_ERROR "params.json: ${param_id} has unknown kind ${param_kind}")
    endif()
    string(APPEND bb_param_enum "    ${param_id} = ${index},\n")
    string(APPEND bb_param_rows
        "    {\"${param_id}\", ParamKind::${param_kind_cpp}, ${param_min}, ${param_max}, ${param_default}, ${param_lfo}},\n")
endforeach()
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/ParamsJson.h CONTENT
"#pragma once
// Written by CMake from params.json. Do not edit.

#include <cstdint>

namespace bb {

enum class ParamKind : std::uint8_t { Float, Int, Boolean, Choice, Trigger };

struct ParamSpec {
    const char* id;
    ParamKind kind;
    double min;
    double max;
    double def;
    bool lfo; // an LFO may be mapped to it
};

namespace param {
// Index of each parameter in params.json, which is also its C ABI and Engine::setParam index.
enum Id : int {
${bb_param_enum}    count = ${bb_param_count},
};
} // namespace param

inline constexpr ParamSpec kParams[param::count] = {
${bb_param_rows}};

} // namespace bb
" @ONLY)

add_library(bb_engine STATIC
    core/domain/Lfo.cpp
    core/domain/SamplePlayer.cpp
    core/engine/Engine.cpp
)
target_include_directories(bb_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core/domain
    ${CMAKE_CURRENT_SOURCE_DIR}/core/engine
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(bb_engine PUBLIC ivandaw::engine_core)
bb_core_flags(bb_engine)

if(EMSCRIPTEN)
    # Standalone module for the AudioWorklet: instantiated directly, no Emscripten JS glue.
    add_executable(brimstone-bass hosts/web/boundary/api.cpp)
    target_link_libraries(brimstone-bass PRIVATE bb_engine)
    bb_core_flags(brimstone-bass)
    set_target_properties(brimstone-bass PROPERTIES SUFFIX ".wasm")
    target_link_options(brimstone-bass PRIVATE
        -O2
        --no-entry
        -sSTANDALONE_WASM
        -sALLOW_MEMORY_GROWTH
        -sFILESYSTEM=0
        -sEXPORTED_FUNCTIONS=_bb_alloc,_bb_free,_bb_create,_bb_destroy,_bb_set_param,_bb_param,_bb_set_fx_order,_bb_set_sample,_bb_transport,_bb_midi,_bb_all_off,_bb_process,_bb_active_voices,_bb_out_count,_bb_out_frame,_bb_out_kind,_bb_out_bytes,_bb_out_slot,_bb_out_value
    )
else()
    add_executable(bb_tests
        tests/main.cpp
        tests/sample_player_tests.cpp
        tests/domain_tests.cpp
        tests/pipeline_tests.cpp
        tests/chain_tests.cpp
        tests/engine_tests.cpp
    )
    target_link_libraries(bb_tests PRIVATE bb_engine)
    bb_core_flags(bb_tests)
endif()
