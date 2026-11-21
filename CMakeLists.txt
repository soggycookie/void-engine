cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

set(VOID_PROJECT_NAME "void-engine")
project(${VOID_PROJECT_NAME})

set(VOID_ENGINE_EXEC "void-engine")
set(VOID_ENGINE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/void/src")
set(VOID_ENGINE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/void/include")
set(VOID_ENGINE_PLATFORM_SRC "${CMAKE_CURRENT_SOURCE_DIR}/void/src/platform")
set(VOID_ENGINE_INCLUDE_PLATFORM "${CMAKE_CURRENT_SOURCE_DIR}/void/include/platform")
set(VOID_ENGINE_EXTERNAL "${CMAKE_CURRENT_SOURCE_DIR}/external")
set(VOID_ENGINE_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/resource")

set(VOID_PRECOMPILED_HEADER "${VOID_ENGINE_INCLUDE}/pch.h")

file(GLOB_RECURSE VOID_ENGINE_SRC_FILES CONFIGURE_DEPENDS
    "${VOID_ENGINE_SRC}/*.cpp"  
)

add_executable(${VOID_ENGINE_EXEC} ${VOID_ENGINE_SRC_FILES})

set_target_properties(${VOID_ENGINE_EXEC} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>"
    VS_GLOBAL_IntDir         "${PROJECT_SOURCE_DIR}/bin/intermediate/$<CONFIG>"
    PDB_OUTPUT_DIRECTORY     "${PROJECT_SOURCE_DIR}/bin/intermediate/$<CONFIG>"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/void-engine"
)


target_precompile_headers(${VOID_ENGINE_EXEC} PRIVATE ${VOID_PRECOMPILED_HEADER})

target_include_directories(${VOID_ENGINE_EXEC}
  PRIVATE
	${VOID_ENGINE_INCLUDE}
	${VOID_ENGINE_EXTERNAL}
)

target_compile_definitions(${VOID_ENGINE_EXEC}
    PRIVATE
        $<$<CONFIG:Debug>:VOID_DEBUG>
)


if(WIN32)
	file(GLOB_RECURSE VOID_ENGINE_PLATFORM_SRC_FILES CONFIGURE_DEPENDS
		"${VOID_ENGINE_PLATFORM_SRC}/d3d11/*.cpp"
		"${VOID_ENGINE_PLATFORM_SRC}/vulkan/*.cpp"
		"${VOID_ENGINE_PLATFORM_SRC}/win32/*.cpp"
	)

	target_sources(${VOID_ENGINE_EXEC}
        PRIVATE
            ${VOID_ENGINE_PLATFORM_SRC_FILES}
    )

	target_include_directories(${VOID_ENGINE_EXEC} 
	  PRIVATE "${VOID_ENGINE_INCLUDE_PLATFORM}/d3d11"
	  PRIVATE "${VOID_ENGINE_INCLUDE_PLATFORM}/vulkan"
	  PRIVATE "${VOID_ENGINE_INCLUDE_PLATFORM}/win32"
	)

    target_link_libraries(${VOID_ENGINE_EXEC}
        PRIVATE
            d3d11
            dxgi
            dxguid
            user32
            gdi32
			d3dcompiler
    )

	target_compile_definitions(${VOID_ENGINE_EXEC}
    PRIVATE
        VOID_WIN32
)

endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")

find_package(Vulkan REQUIRED)

find_path(SLANG_INCLUDE_DIR
    NAMES slang.h
    HINTS $ENV{VULKAN_SDK}/Include/slang
    REQUIRED
)

find_library(SLANG_LIBRARY
    NAMES slang
    HINTS $ENV{VULKAN_SDK}/Lib
    REQUIRED
)

add_library(slang::slang UNKNOWN IMPORTED)

set_target_properties(slang::slang PROPERTIES
    IMPORTED_LOCATION "${SLANG_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SLANG_INCLUDE_DIR}"
)

find_program(SLANGC_EXECUTABLE
    NAMES slangc
    HINTS $ENV{VULKAN_SDK}/Bin
    REQUIRED
)
message(STATUS "Found slangc: ${SLANGC_EXECUTABLE}")

set(SLANG_DIR "${VOID_ENGINE_RESOURCE}/base/shader/slang")
set(SPIRV_DIR "${VOID_ENGINE_RESOURCE}/base/shader/spirv")

function(add_shader_target TARGET)
    cmake_parse_arguments("SHADER" "" "" "SOURCES" ${ARGN})

    # create output dir once, outside the loop
    add_custom_command(
        OUTPUT  ${SPIRV_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SPIRV_DIR}
    )

    set(ALL_SHADER_TARGETS "")

    foreach(SHADER_FILE ${SHADER_SOURCES})
        # resolve to absolute path
        get_filename_component(SHADER_FILE ${SHADER_FILE} ABSOLUTE)
        # no CACHE here
        get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)

        set(OUTPUT_FILE  ${SPIRV_DIR}/${SHADER_NAME}.spv)
        set(ENTRY_POINTS -entry vertMain -entry fragMain)

        add_custom_command(
            OUTPUT  ${OUTPUT_FILE}
            COMMAND ${SLANGC_EXECUTABLE}
                        ${SHADER_FILE}
                        -target spirv
                        -profile spirv_1_4
                        -emit-spirv-directly
                        -fvk-use-entrypoint-name
                        ${ENTRY_POINTS}
                        -o ${SHADER_NAME}.spv
            WORKING_DIRECTORY ${SPIRV_DIR}
            DEPENDS   ${SPIRV_DIR} ${SHADER_FILE}
            COMMENT   "Compiling ${SHADER_NAME}"
            VERBATIM
        )

        add_custom_target(${SHADER_NAME}_shader DEPENDS ${OUTPUT_FILE})
        list(APPEND ALL_SHADER_TARGETS ${SHADER_NAME}_shader)
    endforeach()

    add_custom_target(${TARGET} DEPENDS ${ALL_SHADER_TARGETS})
endfunction()

file(GLOB SLANG_SOURCES "${SLANG_DIR}/*.slang")
add_shader_target(all_shaders SOURCES ${SLANG_SOURCES})
add_dependencies("${VOID_ENGINE_EXEC}" all_shaders)

include(FetchContent)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    SOURCE_DIR "${VOID_ENGINE_EXTERNAL}/glm"
    UPDATE_DISCONNECTED TRUE
)

FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.5.0
    SOURCE_DIR "${VOID_ENGINE_EXTERNAL}/vma"
    UPDATE_DISCONNECTED TRUE
)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    SOURCE_DIR "${VOID_ENGINE_EXTERNAL}/stb"
    UPDATE_DISCONNECTED TRUE
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
    SOURCE_DIR "${VOID_ENGINE_EXTERNAL}/imgui"
    UPDATE_DISCONNECTED TRUE
)

FetchContent_MakeAvailable(glm stb vma imgui)

FetchContent_GetProperties(imgui)
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
)

target_link_libraries(imgui PUBLIC Vulkan::Vulkan)

add_library(vma INTERFACE)
target_include_directories(vma INTERFACE
    "${vma_SOURCE_DIR}/include"
)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE "${stb_SOURCE_DIR}")
target_link_libraries(vma INTERFACE Vulkan::Vulkan)
target_link_libraries(${VOID_ENGINE_EXEC}
    PRIVATE
    glm::glm
    vma
    stb
    slang::slang
)

# --- Embed per-file copy script into binary dir at configure time ---
file(WRITE "${CMAKE_BINARY_DIR}/copy_changed.cmake" [[
file(GLOB_RECURSE ALL_FILES LIST_DIRECTORIES false "${SRC}/*")
foreach(F ${ALL_FILES})
    file(RELATIVE_PATH REL "${SRC}" "${F}")
    set(D "${DST}/${REL}")
    if(NOT EXISTS "${D}")
        set(DO TRUE)
    else()
        file(TIMESTAMP "${F}" ST "%s")
        file(TIMESTAMP "${D}" DT "%s")
        if(ST GREATER DT)
            set(DO TRUE)
        else()
            set(DO FALSE)
        endif()
    endif()
    if(DO)
        get_filename_component(DIR "${D}" DIRECTORY)
        file(MAKE_DIRECTORY "${DIR}")
        file(COPY_FILE "${F}" "${D}" ONLY_IF_DIFFERENT)
        message(STATUS "Copied: ${REL}")
    endif()
endforeach()
]])

# --- Asset folder (game content) ---
add_custom_command(TARGET ${VOID_ENGINE_EXEC} POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        "-DSRC=${PROJECT_SOURCE_DIR}/asset"
        "-DDST=${PROJECT_SOURCE_DIR}/bin/$<CONFIG>/asset"
        -P "${CMAKE_BINARY_DIR}/copy_changed.cmake"
    COMMENT "Syncing assets..."
    VERBATIM
)

# --- Resources folder (internal engine use) ---
add_custom_command(TARGET ${VOID_ENGINE_EXEC} POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        "-DSRC=${VOID_ENGINE_RESOURCE}"
        "-DDST=${PROJECT_SOURCE_DIR}/bin/$<CONFIG>/resource"
        -P "${CMAKE_BINARY_DIR}/copy_changed.cmake"
    COMMENT "Syncing resources..."
    VERBATIM
)
