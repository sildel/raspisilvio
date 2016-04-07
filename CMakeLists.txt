cmake_minimum_required(VERSION 2.8)
project( raspisilvio )
SET(COMPILE_DEFINITIONS -Werror)

#find_package( OpenCV REQUIRED )

include_directories(/opt/vc/include)
include_directories(/home/pi/Projects/raspisilvio)
include_directories(/opt/vc/userland-master/host_applications/linux/libs/bcm_host/include)
include_directories(/opt/vc/userland-master/interface/vcos)
include_directories(/opt/vc/userland-master)
include_directories(/opt/vc/userland-master/interface/vcos/pthreads)
include_directories(/opt/vc/userland-master/interface/vmcs_host/linux)

set (GL_SCENE_SOURCES
	gl_scenes/models.c
	gl_scenes/mirror.c
	gl_scenes/yuv.c
	gl_scenes/sobel.c
	gl_scenes/square.c
	gl_scenes/teapot.c
	gl_scenes/matching.c)

set (COMMON_SOURCES
	RaspiCamControl.c
	RaspiPreview.c
	RaspiCLI.c)

add_executable(raspisilvio ${COMMON_SOURCES} main.c RaspiTex.c RaspiTexUtil.c tga.c ${GL_SCENE_SOURCES})

set (THE_LIBS
	/opt/vc/lib/libmmal_core.so
	/opt/vc/lib/libmmal_util.so
	/opt/vc/lib/libmmal_vc_client.so
	/opt/vc/lib/libvcos.so
	/opt/vc/lib/libbcm_host.so
	/opt/vc/lib/libGLESv2.so
	/opt/vc/lib/libEGL.so
	pthread
	m)

target_link_libraries(raspisilvio ${THE_LIBS})
