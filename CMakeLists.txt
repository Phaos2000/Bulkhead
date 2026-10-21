cmake_minimum_required(VERSION 3.25)
project(Bulkhead VERSION 1.0.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(VST3_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vendor/vst3sdk" CACHE PATH "Steinberg VST3 SDK")
set(SMTG_USE_STATIC_CRT ON CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)
add_subdirectory("${VST3_SDK_ROOT}" "${CMAKE_BINARY_DIR}/sdk")
smtg_enable_vst3_sdk()
if(MINGW)
    add_library(Bulkhead MODULE source/Plugin.cpp source/Editor.cpp "${VST3_SDK_ROOT}/public.sdk/source/main/dllmain.cpp")
    set_target_properties(Bulkhead PROPERTIES PREFIX "" SUFFIX ".vst3" LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Bulkhead.vst3/Contents/x86_64-win")
    target_link_options(Bulkhead PRIVATE -static)
    target_compile_definitions(Bulkhead PRIVATE RELEASE=1)
else()
    smtg_add_vst3plugin(Bulkhead source/Plugin.cpp source/Editor.cpp)
endif()
target_include_directories(Bulkhead PRIVATE source)
target_compile_definitions(Bulkhead PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(Bulkhead PRIVATE sdk gdiplus user32 gdi32 shell32 ole32)
add_custom_command(TARGET Bulkhead POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        "$<TARGET_FILE_DIR:Bulkhead>/../.."
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:Bulkhead>/../../LICENSES"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/Steinberg-VST3-SDK-MIT.txt"
        "$<TARGET_FILE_DIR:Bulkhead>/../../LICENSES"
    VERBATIM
)
add_executable(bulkhead_tests tests/dsp_tests.cpp)
target_include_directories(bulkhead_tests PRIVATE source)
add_executable(bulkhead_preview tests/preview.cpp)
target_include_directories(bulkhead_preview PRIVATE source)
target_compile_definitions(bulkhead_preview PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(bulkhead_preview PRIVATE gdiplus user32 gdi32)
enable_testing()
add_test(NAME bulkhead_dsp COMMAND bulkhead_tests "${CMAKE_CURRENT_SOURCE_DIR}/artifacts")
set(SDK_ROOT "${VST3_SDK_ROOT}")
if(NOT TARGET validator)
    add_subdirectory("${SDK_ROOT}/public.sdk/samples/vst-hosting/validator" "${CMAKE_BINARY_DIR}/validator")
endif()
if(MINGW)
    target_link_options(bulkhead_tests PRIVATE -static)
    target_link_options(bulkhead_preview PRIVATE -static -municode)
    target_link_options(validator PRIVATE -static -municode)
endif()



add_executable(bulkhead_gain_probe tests/gain_probe.cpp)
target_include_directories(bulkhead_gain_probe PRIVATE source)

add_executable(bulkhead_preset_tests tests/preset_tests.cpp)
target_include_directories(bulkhead_preset_tests PRIVATE source)
target_compile_definitions(bulkhead_preset_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(bulkhead_preset_tests PRIVATE shell32 ole32)
add_test(NAME bulkhead_presets COMMAND bulkhead_preset_tests "${CMAKE_CURRENT_SOURCE_DIR}/artifacts")

add_executable(bulkhead_host_tests tests/host_tests.cpp "${VST3_SDK_ROOT}/public.sdk/source/common/memorystream.cpp")
target_include_directories(bulkhead_host_tests PRIVATE source)
target_compile_definitions(bulkhead_host_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(bulkhead_host_tests PRIVATE sdk_hosting sdk user32 ole32)
add_dependencies(bulkhead_host_tests Bulkhead)
add_test(NAME bulkhead_editor COMMAND bulkhead_host_tests "$<TARGET_FILE:Bulkhead>")

# Compare two built plugin binaries, including a previously released version.
add_executable(bulkhead_compare tests/plugin_compare.cpp "${VST3_SDK_ROOT}/public.sdk/source/common/memorystream.cpp")
target_include_directories(bulkhead_compare PRIVATE source)
target_compile_definitions(bulkhead_compare PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(bulkhead_compare PRIVATE sdk_hosting sdk user32 ole32)
if(MINGW)
    target_link_options(bulkhead_compare PRIVATE -static -municode)
endif()
