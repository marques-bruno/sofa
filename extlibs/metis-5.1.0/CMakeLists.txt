cmake_minimum_required(VERSION 3.1)
project(metis)

unset(GKLIB_PATH CACHE)
get_filename_component(GKLIB_PATH "GKlib" ABSOLUTE)
include(GKlib/GKlibSystem.cmake)

file(GLOB metis_sources libmetis/*.c)

# The code must be relocatable if we want to link a shared library against it.
if("x${CMAKE_CXX_COMPILER_ID}" STREQUAL "xGNU" OR "x${CMAKE_CXX_COMPILER_ID}" STREQUAL "xClang")
    add_compile_options(-fPIC)
endif()

add_library(${PROJECT_NAME} STATIC ${metis_sources} ${GKlib_sources})
target_include_directories(${PROJECT_NAME} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/GKlib>")
target_include_directories(${PROJECT_NAME} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_include_directories(${PROJECT_NAME} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libmetis>")
target_include_directories(${PROJECT_NAME} PUBLIC "$<INSTALL_INTERFACE:include/extlibs/Metis>")
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER include/metis.h)
if(UNIX)
    target_link_libraries(${PROJECT_NAME} m)
endif()

include(${SOFA_KERNEL_SOURCE_DIR}/SofaFramework/SofaMacros.cmake)
sofa_generate_package(NAME Metis VERSION 5.1.0 TARGETS metis INCLUDE_INSTALL_DIR "extlibs/Metis")
