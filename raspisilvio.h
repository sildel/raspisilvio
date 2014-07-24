///////////////////////////////////////////////////////////////////
#ifndef RASPISILVIO_H_
#define RASPISILVIO_H_
///////////////////////////////////////////////////////////////////
// We use some GNU extensions (basename)
#define _GNU_SOURCE
////////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <sysexits.h>
////////////////////////////////////////////////////////////////////////////////
#define VERSION_STRING "v1.3.11"
////////////////////////////////////////////////////////////////////////////////
#include "bcm_host.h"
#include "interface/vcos/vcos.h"
////////////////////////////////////////////////////////////////////////////////
#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"
////////////////////////////////////////////////////////////////////////////////
#include "RaspiCamControl.h"
#include "RaspiPreview.h"
#include "RaspiCLI.h"
////////////////////////////////////////////////////////////////////////////////
#include "opencv/cv.h"
#include "opencv/highgui.h"
#include "raspisilvio.h"
////////////////////////////////////////////////////////////////////////////////
#include <semaphore.h>
////////////////////////////////////////////////////////////////////////////////
/// Camera number to use - we only have one camera, indexed from 0.
#define CAMERA_NUMBER 0
////////////////////////////////////////////////////////////////////////////////
// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2
////////////////////////////////////////////////////////////////////////////////
// Video format information
// 0 implies variable
#define VIDEO_FRAME_RATE_NUM 30
#define VIDEO_FRAME_RATE_DEN 1
////////////////////////////////////////////////////////////////////////////////
/// Video render needs at least 2 buffers.
#define VIDEO_OUTPUT_BUFFERS_NUM 3
////////////////////////////////////////////////////////////////////////////////
int mmal_status_to_int(MMAL_STATUS_T status);
void signal_handler(int signal_number);
extern int the_flag;
////////////////////////////////////////////////////////////////////////////////
// Forward
typedef struct RASPIVID_STATE_S RASPIVID_STATE;
////////////////////////////////////////////////////////////////////////////////

/** Structure containing all state information for the current run
 */
struct RASPIVID_STATE_S {
    int width; /// Requested width of image
    int height; /// requested height of image
    int bitrate; /// Requested bitrate
    int framerate; /// Requested frame rate (fps)
    int nCount; ///number of frames

    RASPICAM_CAMERA_PARAMETERS camera_parameters; /// Camera setup parameters

    MMAL_COMPONENT_T *camera_component; /// Pointer to the camera component    

    MMAL_POOL_T *video_pool; /// Pointer to the pool of buffers used by video output port

    IplImage *py;
    IplImage *pu;
    IplImage *pv;
    IplImage *pu_big;
    IplImage *pv_big;
    IplImage *image;
    IplImage *dstImage;
    IplImage *imgThresh;
    IplImage *hsvImage;

    int H_min_value;
    int S_min_value;
    int V_min_value;
    int H_max_value;
    int S_max_value;
    int V_max_value;
};
////////////////////////////////////////////////////////////////////////////////
void camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
void video_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
MMAL_STATUS_T create_camera_component(RASPIVID_STATE *state);
void destroy_camera_component(RASPIVID_STATE *state);
MMAL_STATUS_T connect_ports(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port, MMAL_CONNECTION_T **connection);
void check_disable_port(MMAL_PORT_T *port);
////////////////////////////////////////////////////////////////////////////////
#endif /* RASPISILVIO_H_ */
////////////////////////////////////////////////////////////////////////////////
