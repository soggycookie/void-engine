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
set(VOID_ENGINE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/void-engine/void/src")
set(VOID_ENGINE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/void-engine/void/include")
set(VOID_ENGINE_PLATFORM_SRC "${CMAKE_CURRENT_SOURCE_DIR}/void-engine/void/src/platform")
set(VOID_ENGINE_INCLUDE_PLATFORM "${CMAKE_CURRENT_SOURCE_DIR}/void-engine/void/include/platform")
set(VOID_ENGINE_EXTERNAL "${CMAKE_CURRENT_SOURCE_DIR}/void-engine/external")
set(VOID_ENGINE_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/void-engine/resource")

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

set(VOID_ECS_LIB "ecs")
set(VOID_ECS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/void-engine/ecs/src")
set(VOID_ECS_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/void-engine/ecs/include/ecs")
set(VOID_ECS_API_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/void-engine/ecs/include/public_api")
set(VOID_ECS_PRECOMPILED_HEADER "${VOID_ECS_INCLUDE}/ecs_pch.h")

file(GLOB_RECURSE VOID_ECS_FILES CONFIGURE_DEPENDS
    "${VOID_ECS_SRC}/*.cpp"  
    "${VOID_ECS_INCLUDE}/*.h"
    "${VOID_ECS_INCLUDE}/*.hpp"
)

add_library(${VOID_ECS_LIB} STATIC ${VOID_ECS_FILES})

set_target_properties(${VOID_ECS_LIB} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>"
    VS_GLOBAL_IntDir         "${PROJECT_SOURCE_DIR}/bin/intermediate/$<CONFIG>"
    PDB_OUTPUT_DIRECTORY     "${PROJECT_SOURCE_DIR}/bin/intermediate/$<CONFIG>"
)

target_precompile_headers(${VOID_ECS_LIB} PRIVATE ${VOID_ECS_PRECOMPILED_HEADER})

target_compile_definitions(${VOID_ECS_LIB}
    PUBLIC
        $<$<CONFIG:Debug>:ECS_DEBUG>
	PUBLIC
		"ECS_STATIC"
)

target_include_directories(${VOID_ECS_LIB} 
  PRIVATE ${VOID_ECS_INCLUDE}
  PUBLIC ${VOID_ECS_API_INCLUDE}
)

set(VOID_LOGGER_LIB "logger")
set(VOID_LOGGER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/void-engine/logger/src")
set(VOID_LOGGER_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/void-engine/logger/include")

file(GLOB_RECURSE VOID_LOGGER_FILES CONFIGURE_DEPENDS
    "${VOID_LOGGER_SRC}/*.cpp"  
    "${VOID_LOGGER_INCLUDE}/*.h"
    "${VOID_LOGGER_INCLUDE}/*.hpp"
)

add_library(${VOID_LOGGER_LIB} STATIC ${VOID_LOGGER_FILES})

set_target_properties(${VOID_LOGGER_LIB} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>"
    VS_GLOBAL_IntDir         "${PROJECT_SOURCE_DIR}/bin/intermediate/$<CONFIG>"
    PDB_OUTPUT_DIRECTORY     "${PROJECT_SOURCE_DIR}/bin/intermediate/$<CONFIG>"
)

target_include_directories(${VOID_LOGGER_LIB}
    PUBLIC ${VOID_LOGGER_INCLUDE}
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")

find_package(Vulkan REQUIRED)

find_program(SLANGC_EXECUTABLE
    NAMES slangc
    HINTS $ENV{VULKAN_SDK}/Bin      
    REQUIRED
)
message(STATUS "Found slangc: ${SLANGC_EXECUTABLE}")

set(SLANG_DIR "${VOID_ENGINE_RESOURCE}/shader/slang")
set(SPIRV_DIR "${VOID_ENGINE_RESOURCE}/shader/spirv")

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
    GIT_TAG 1.0.3
	SOURCE_DIR "${VOID_ENGINE_EXTERNAL}/glm"
)

FetchContent_MakeAvailable(glm)

set(VOID_MATH_LIB "math")
#set(VOID_MATH_SRC "${CMAKE_CURRENT_SOURCE_DIR}/void-engine/math/src")
set(VOID_MATH_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/void-engine/math/include")

#file(GLOB_RECURSE VOID_MATH_FILES CONFIGURE_DEPENDS
#   "${VOID_MATH_SRC}/*.cpp"  
#   "${VOID_MATH_INCLUDE}/*.h"
#	"${VOID_MATH_INCLUDE}/*.hpp"
#)

add_library(${VOID_MATH_LIB} INTERFACE)

#set_target_properties(${VOID_MATH_LIB} PROPERTIES
#    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>"
#   LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>"
#    ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>"
#    VS_GLOBAL_IntDir         "${PROJECT_SOURCE_DIR}/bin/intermediate/$<CONFIG>"
#    PDB_OUTPUT_DIRECTORY     "${PROJECT_SOURCE_DIR}/bin/intermediate/$<CONFIG>"
#)

target_include_directories(${VOID_MATH_LIB}
    INTERFACE ${VOID_MATH_INCLUDE}
)

target_link_libraries(${VOID_MATH_LIB} INTERFACE glm)

target_link_libraries(${VOID_ENGINE_EXEC}
    PRIVATE
	${VOID_ECS_LIB} 
    ${VOID_LOGGER_LIB}
	Vulkan::Vulkan
	${VOID_MATH_LIB}
)

target_link_libraries(${VOID_ECS_LIB} 
	PRIVATE
	${VOID_LOGGER_LIB}
	${VOID_MATH_LIB}
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
        "-DSRC=${PROJECT_SOURCE_DIR}/void-engine/asset"
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
