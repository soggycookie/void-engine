cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

set(VOID_PROJECT_NAME "void-engine")
project(${VOID_PROJECT_NAME})

set(VOID_ENGINE_EXEC "void-engine")
set(VOID_ENGINE_SRC "void-engine/void/src")
set(VOID_ENGINE_INCLUDE "void-engine/void/include")
set(VOID_ENGINE_PLATFORM_SRC "void-engine/void/src/platform")
set(VOID_ENGINE_INCLUDE_PLATFORM "void-engine/void/include/platform")
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
)


target_precompile_headers(${VOID_ENGINE_EXEC} PRIVATE ${VOID_PRECOMPILED_HEADER})

target_include_directories(${VOID_ENGINE_EXEC} 
  PRIVATE ${VOID_ENGINE_INCLUDE}
)

add_custom_command(TARGET ${VOID_ENGINE_EXEC} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/void-engine/asset"
        "$<TARGET_FILE_DIR:${VOID_ENGINE_EXEC}>/asset"
)

target_compile_definitions(${VOID_ENGINE_EXEC}
    PRIVATE
		VOID_DEV_MODE
        $<$<CONFIG:Debug>:VOID_DEBUG>
)

if(WIN32)
	file(GLOB_RECURSE VOID_ENGINE_PLATFORM_SRC_FILES CONFIGURE_DEPENDS
		"${VOID_ENGINE_PLATFORM_SRC}/d3d11/*.cpp"
		"${VOID_ENGINE_PLATFORM_SRC}/win32/*.cpp"
	)
    
	target_sources(${VOID_ENGINE_EXEC}
        PRIVATE
            ${VOID_ENGINE_PLATFORM_SRC_FILES}
    )

	target_include_directories(${VOID_ENGINE_EXEC} 
	  PRIVATE "${VOID_ENGINE_INCLUDE_PLATFORM}/d3d11"
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

endif()

set(VOID_ECS_LIB "ecs")
set(VOID_ECS_SRC "void-engine/ecs/src")
set(VOID_ECS_INCLUDE "void-engine/ecs/include/ecs")
set(VOID_ECS_API_INCLUDE "void-engine/ecs/include/public_api")
set(VOID_ECS_PRECOMPILED_HEADER "${VOID_ECS_INCLUDE}/ecs_pch.h")

file(GLOB_RECURSE VOID_ECS_SRC_FILES CONFIGURE_DEPENDS
    "${VOID_ECS_SRC}/*.cpp"  
)

add_library(${VOID_ECS_LIB} STATIC ${VOID_ECS_SRC_FILES})

set_target_properties(${VOID_ECS_LIB} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>"
    VS_GLOBAL_IntDir         "${PROJECT_SOURCE_DIR}/bin/intermediate/$<CONFIG>"
    PDB_OUTPUT_DIRECTORY     "${PROJECT_SOURCE_DIR}/bin/intermediate/$<CONFIG>"
)

target_precompile_headers(${VOID_ECS_LIB} PRIVATE ${VOID_ECS_PRECOMPILED_HEADER})

target_compile_definitions(${VOID_ECS_LIB}
    PRIVATE
        $<$<CONFIG:Debug>:ECS_DEBUG>
	PUBLIC
		"ECS_STATIC"
)

target_include_directories(${VOID_ECS_LIB} 
  PRIVATE ${VOID_ECS_INCLUDE}
  PUBLIC ${VOID_ECS_API_INCLUDE}
)

target_link_libraries(${VOID_ENGINE_EXEC}
    PRIVATE
	${VOID_ECS_LIB} 
)