///////////////////////////////////////////////////////////////////
//Modified by Silvio Delgado
///////////////////////////////////////////////////////////////////
#ifndef RASPISILVIO_H_
#define RASPISILVIO_H_
///////////////////////////////////////////////////////////////////
#define VERSION_STRING "v0.6"
///////////////////////////////////////////////////////////////////
#include "bcm_host.h"
#include "interface/vcos/vcos.h"
///////////////////////////////////////////////////////////////////
#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"
///////////////////////////////////////////////////////////////////
#include "RaspiCamControl.h"
#include "RaspiPreview.h"
#include "RaspiCLI.h"
#include "RaspiTex.h"
///////////////////////////////////////////////////////////////////
#include <semaphore.h>
///////////////////////////////////////////////////////////////////
/// Camera number to use - we only have one camera, indexed from 0.
#define CAMERA_NUMBER 0
///////////////////////////////////////////////////////////////////
// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2
///////////////////////////////////////////////////////////////////
// Stills format information
// 0 implies variable
#define STILLS_FRAME_RATE_NUM 0
#define STILLS_FRAME_RATE_DEN 1
///////////////////////////////////////////////////////////////////
/// Video render needs at least 2 buffers.
#define VIDEO_OUTPUT_BUFFERS_NUM 3
///////////////////////////////////////////////////////////////////
#define MAX_USER_EXIF_TAGS      32
#define MAX_EXIF_PAYLOAD_LENGTH 128
///////////////////////////////////////////////////////////////////
/// Frame advance method
#define FRAME_NEXT_SINGLE        0
#define FRAME_NEXT_TIMELAPSE     1
#define FRAME_NEXT_KEYPRESS      2
#define FRAME_NEXT_FOREVER       3
#define FRAME_NEXT_GPIO          4
#define FRAME_NEXT_SIGNAL        5
#define FRAME_NEXT_IMMEDIATELY   6
///////////////////////////////////////////////////////////////////
int mmal_status_to_int(MMAL_STATUS_T status);
void signal_handler(int signal_number);
///////////////////////////////////////////////////////////////////

/** Structure containing all state information for the current run
 */
typedef struct {
    int timeout; /// Time taken before frame is grabbed and app then shuts down. Units are milliseconds
    int width; /// Requested width of image
    int height; /// requested height of image
    int quality; /// JPEG quality setting (1-100)
    int wantRAW; /// Flag for whether the JPEG metadata also contains the RAW bayer image
    char *filename; /// filename of output file
    char *linkname; /// filename of output file
    MMAL_PARAM_THUMBNAIL_CONFIG_T thumbnailConfig;
    int verbose; /// !0 if want detailed run information
    int demoMode; /// Run app in demo mode
    int demoInterval; /// Interval between camera settings changes
    MMAL_FOURCC_T encoding; /// Encoding to use for the output file.
    const char *exifTags[MAX_USER_EXIF_TAGS]; /// Array of pointers to tags supplied from the command line
    int numExifTags; /// Number of supplied tags
    int enableExifTags; /// Enable/Disable EXIF tags in output
    int timelapse; /// Delay between each picture in timelapse mode. If 0, disable timelapse
    int fullResPreview; /// If set, the camera preview port runs at capture resolution. Reduces fps.
    int frameNextMethod; /// Which method to use to advance to next frame
    int useGL; /// Render preview using OpenGL
    int glCapture; /// Save the GL frame-buffer instead of camera output

    RASPIPREVIEW_PARAMETERS preview_parameters; /// Preview setup parameters
    RASPICAM_CAMERA_PARAMETERS camera_parameters; /// Camera setup parameters

    MMAL_COMPONENT_T *camera_component; /// Pointer to the camera component
    MMAL_COMPONENT_T *encoder_component; /// Pointer to the encoder component
    MMAL_COMPONENT_T *null_sink_component; /// Pointer to the null sink component
    MMAL_CONNECTION_T *preview_connection; /// Pointer to the connection from camera to preview
    MMAL_CONNECTION_T *encoder_connection; /// Pointer to the connection from camera to encoder

    MMAL_POOL_T *encoder_pool; /// Pointer to the pool of buffers used by encoder output port

    RASPITEX_STATE raspitex_state; /// GL renderer state and parameters

} RASPISTILL_STATE;
///////////////////////////////////////////////////////////////////

/** Struct used to pass information in encoder port userdata to callback
 */
typedef struct {
    FILE *file_handle; /// File handle to write buffer data to.
    VCOS_SEMAPHORE_T complete_semaphore; /// semaphore which is posted when we reach end of frame (indicates end of capture or fault)
    RASPISTILL_STATE *pstate; /// pointer to our state in case required in callback
} PORT_USERDATA;
///////////////////////////////////////////////////////////////////
void default_status(RASPISTILL_STATE *state);
void dump_status(RASPISTILL_STATE *state);
///////////////////////////////////////////////////////////////////
void camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
MMAL_STATUS_T create_camera_component(RASPISTILL_STATE *state);
void destroy_camera_component(RASPISTILL_STATE *state);
///////////////////////////////////////////////////////////////////
MMAL_STATUS_T connect_ports(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port, MMAL_CONNECTION_T **connection);
void check_disable_port(MMAL_PORT_T *port);
///////////////////////////////////////////////////////////////////
#endif /* RASPISILVIO_H_ */
