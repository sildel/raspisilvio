cmake_minimum_required(VERSION 2.8)
project(raspisilvio)
SET(COMPILE_DEFINITIONS -Werror)

set(OpenCV_DIR D:/build)

include_directories(${PROJECT_SOURCE_DIR}/../userland-master)
include_directories(${PROJECT_SOURCE_DIR}/../userland-master/interface/vcos/pthreads)
include_directories(${PROJECT_SOURCE_DIR}/../userland-master/interface/vmcs_host/linux)
include_directories(${PROJECT_SOURCE_DIR}/../userland-master/host_applications/linux/libs/bcm_host/include)
include_directories(${PROJECT_SOURCE_DIR}/../userland-master/interface/khronos/include)
include_directories(${PROJECT_SOURCE_DIR})

find_package(OpenCV REQUIRED core highgui)

set(GL_SCENE_SOURCES
        gl_scenes/models.c
        gl_scenes/mirror.c
        gl_scenes/yuv.c
        gl_scenes/sobel.c
        gl_scenes/square.c
        gl_scenes/teapot.c)

set(COMMON_SOURCES
        RaspiCamControl.c
        RaspiPreview.c
        RaspiCLI.c)

add_executable(raspisilvio ${COMMON_SOURCES} raspisilvio.c main.cpp RaspiTex.c RaspiTexUtil.c tga.c ${GL_SCENE_SOURCES})

set(THE_LIBS
        ${PROJECT_SOURCE_DIR}/../userland-master/build/lib/libmmal_core.so
        ${PROJECT_SOURCE_DIR}/../userland-master/build/lib/libmmal_util.so
        ${PROJECT_SOURCE_DIR}/../userland-master/build/lib/libmmal_vc_client.so
        ${PROJECT_SOURCE_DIR}/../userland-master/build/lib/libvcos.so
        ${PROJECT_SOURCE_DIR}/../userland-master/build/lib/libbcm_host.so
        ${PROJECT_SOURCE_DIR}/../userland-master/build/lib/libGLESv2.so
        ${PROJECT_SOURCE_DIR}/../userland-master/build/lib/libEGL.so
        pthread
        m)

target_link_libraries(raspisilvio ${THE_LIBS} ${OpenCV_LIBS})
