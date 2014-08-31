///////////////////////////////////////////////////////////////////////////////////////////////////
// We use some GNU extensions (asprintf, basename)
#define _GNU_SOURCE
///////////////////////////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <sysexits.h>
///////////////////////////////////////////////////////////////////////////////////////////////////
#define VERSION_STRING "v1.3.7"
///////////////////////////////////////////////////////////////////////////////////////////////////
#include "bcm_host.h"
#include "interface/vcos/vcos.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"


#include "RaspiCamControl.h"
#include "RaspiPreview.h"
#include "RaspiCLI.h"
#include "RaspiTex.h"

#include "gl_scenes/matching.h"

#include <semaphore.h>
///////////////////////////////////////////////////////////////////////////////////////////////////
/// Camera number to use - we only have one camera, indexed from 0.
#define CAMERA_NUMBER 0
///////////////////////////////////////////////////////////////////////////////////////////////////
// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2
///////////////////////////////////////////////////////////////////////////////////////////////////
// Stills format information
// 0 implies variable
#define STILLS_FRAME_RATE_NUM 0
#define STILLS_FRAME_RATE_DEN 1
///////////////////////////////////////////////////////////////////////////////////////////////////
/// Video render needs at least 2 buffers.
#define VIDEO_OUTPUT_BUFFERS_NUM 3
///////////////////////////////////////////////////////////////////////////////////////////////////
int mmal_status_to_int ( MMAL_STATUS_T status ) ;
static void signal_handler ( int signal_number ) ;
///////////////////////////////////////////////////////////////////////////////////////////////////

/** Structure containing all state information for the current run
 */
typedef struct
{
    int width ; /// Requested width of image
    int height ; /// requested height of image
    int useGL ; /// Render preview using OpenGL

    RASPIPREVIEW_PARAMETERS preview_parameters ; /// Preview setup parameters
    RASPICAM_CAMERA_PARAMETERS camera_parameters ; /// Camera setup parameters

    MMAL_COMPONENT_T *camera_component ; /// Pointer to the camera component
    MMAL_COMPONENT_T *null_sink_component ; /// Pointer to the null sink component
    MMAL_CONNECTION_T *preview_connection ; /// Pointer to the connection from camera to preview
    MMAL_CONNECTION_T *encoder_connection ; /// Pointer to the connection from camera to encoder

    RASPITEX_STATE raspitex_state ; /// GL renderer state and parameters

} RASPISTILL_STATE ;
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Assign a default set of parameters to the state passed in
 *
 * @param state Pointer to state structure to assign defaults to
 */
static void default_status ( RASPISTILL_STATE *state )
{
    if ( ! state )
    {
        vcos_assert ( 0 ) ;
        return ;
    }

    state->width = 640 ;
    state->height = 480 ;

    state->camera_component = NULL ;

    // Setup preview window defaults
    raspipreview_set_defaults ( &state->preview_parameters ) ;
    //    state->preview_parameters.previewWindow.width = 800 ;
    //    state->preview_parameters.previewWindow.width = 600 ;

    // Set up the camera_parameters to default
    raspicamcontrol_set_defaults ( &state->camera_parameters ) ;
    //    state->camera_parameters.exposureMode = MMAL_PARAM_EXPOSUREMODE_FIXEDFPS ;
    //    state->camera_parameters.exposureMode = MMAL_PARAM_EXPOSUREMODE_ ;
//    state->camera_parameters.exposureMode = MMAL_PARAM_EXPOSUREMODE_OFF ;
    state->camera_parameters.exposureCompensation = 20 ;
    //    state->camera_parameters.exposureMeterMode = MMAL_PARAM_EXPOSUREMETERINGMODE_MATRIX ;
    //    state->camera_parameters.videoStabilisation = 1 ;
    //    state->camera_parameters.awbMode = MMAL_PARAM_AWBMODE_OFF ;
    //    state->camera_parameters.awb_gains_b = 1.0f ;
    //    state->camera_parameters.awb_gains_r = 1.0f ;


    state->camera_parameters.hflip = 1 ;

    // Set initial GL preview state
    raspitex_set_defaults ( &state->raspitex_state ) ;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 *  buffer header callback function for camera control
 *
 *  No actions taken in current version
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
static void camera_control_callback ( MMAL_PORT_T *port , MMAL_BUFFER_HEADER_T *buffer )
{
    if ( buffer->cmd == MMAL_EVENT_PARAMETER_CHANGED )
    {
    }
    else
    {
        vcos_log_error ( "Received unexpected camera control callback event, 0x%08x" , buffer->cmd ) ;
    }

    mmal_buffer_header_release ( buffer ) ;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Create the camera component, set up its ports
 *
 * @param state Pointer to state control struct. camera_component member set to the created camera_component if successfull.
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
static MMAL_STATUS_T create_camera_component ( RASPISTILL_STATE *state )
{
    MMAL_COMPONENT_T *camera = 0 ;
    MMAL_ES_FORMAT_T *format ;
    MMAL_PORT_T *preview_port = NULL , *video_port = NULL , *still_port = NULL ;
    MMAL_STATUS_T status ;

    /* Create the component */
    status = mmal_component_create ( MMAL_COMPONENT_DEFAULT_CAMERA , &camera ) ;

    if ( status != MMAL_SUCCESS )
    {
        vcos_log_error ( "Failed to create camera component" ) ;
        goto error ;
    }

    if ( ! camera->output_num )
    {
        status = MMAL_ENOSYS ;
        vcos_log_error ( "Camera doesn't have output ports" ) ;
        goto error ;
    }

    preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT] ;
    video_port = camera->output[MMAL_CAMERA_VIDEO_PORT] ;
    still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT] ;

    // Enable the camera, and tell it its control callback function
    status = mmal_port_enable ( camera->control , camera_control_callback ) ;

    if ( status != MMAL_SUCCESS )
    {
        vcos_log_error ( "Unable to enable control port : error %d" , status ) ;
        goto error ;
    }

    //  set up the camera configuration
    {
        MMAL_PARAMETER_CAMERA_CONFIG_T cam_config = {
            { MMAL_PARAMETER_CAMERA_CONFIG , sizeof (cam_config ) } ,
                                                     .max_stills_w = state->width ,
                                                     .max_stills_h = state->height ,
                                                     .stills_yuv422 = 0 ,
                                                     .one_shot_stills = 1 ,
                                                     .max_preview_video_w = state->preview_parameters.previewWindow.width ,
                                                     .max_preview_video_h = state->preview_parameters.previewWindow.height ,
                                                     .num_preview_video_frames = 3 ,
                                                     .stills_capture_circular_buffer_height = 0 ,
                                                     .fast_preview_resume = 0 ,
                                                     .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
        } ;

        mmal_port_parameter_set ( camera->control , &cam_config.hdr ) ;
    }

    raspicamcontrol_set_all_parameters ( camera , &state->camera_parameters ) ;

    // Now set up the port formats

    format = preview_port->format ;
    format->encoding = MMAL_ENCODING_OPAQUE ;
    format->encoding_variant = MMAL_ENCODING_I420 ;

    // Use a full FOV 4:3 mode
    format->es->video.width = VCOS_ALIGN_UP ( state->preview_parameters.previewWindow.width , 32 ) ;
    format->es->video.height = VCOS_ALIGN_UP ( state->preview_parameters.previewWindow.height , 16 ) ;
    format->es->video.crop.x = 0 ;
    format->es->video.crop.y = 0 ;
    format->es->video.crop.width = state->preview_parameters.previewWindow.width ;
    format->es->video.crop.height = state->preview_parameters.previewWindow.height ;
    format->es->video.frame_rate.num = PREVIEW_FRAME_RATE_NUM ;
    format->es->video.frame_rate.den = PREVIEW_FRAME_RATE_DEN ;

    status = mmal_port_format_commit ( preview_port ) ;
    if ( status != MMAL_SUCCESS )
    {
        vcos_log_error ( "camera viewfinder format couldn't be set" ) ;
        goto error ;
    }

    // Set the same format on the video  port (which we dont use here)
    mmal_format_full_copy ( video_port->format , format ) ;
    status = mmal_port_format_commit ( video_port ) ;

    if ( status != MMAL_SUCCESS )
    {
        vcos_log_error ( "camera video format couldn't be set" ) ;
        goto error ;
    }

    // Ensure there are enough buffers to avoid dropping frames
    if ( video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM )
        video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM ;

    format = still_port->format ;

    // Set our stills format on the stills (for encoder) port
    format->encoding = MMAL_ENCODING_OPAQUE ;
    format->es->video.width = VCOS_ALIGN_UP ( state->width , 32 ) ;
    format->es->video.height = VCOS_ALIGN_UP ( state->height , 16 ) ;
    format->es->video.crop.x = 0 ;
    format->es->video.crop.y = 0 ;
    format->es->video.crop.width = state->width ;
    format->es->video.crop.height = state->height ;
    format->es->video.frame_rate.num = STILLS_FRAME_RATE_NUM ;
    format->es->video.frame_rate.den = STILLS_FRAME_RATE_DEN ;


    status = mmal_port_format_commit ( still_port ) ;

    if ( status != MMAL_SUCCESS )
    {
        vcos_log_error ( "camera still format couldn't be set" ) ;
        goto error ;
    }

    /* Ensure there are enough buffers to avoid dropping frames */
    if ( still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM )
        still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM ;

    /* Enable component */
    status = mmal_component_enable ( camera ) ;

    if ( status != MMAL_SUCCESS )
    {
        vcos_log_error ( "camera component couldn't be enabled" ) ;
        goto error ;
    }

    status = raspitex_configure_preview_port ( &state->raspitex_state , preview_port ) ;
    if ( status != MMAL_SUCCESS )
    {
        fprintf ( stderr , "Failed to configure preview port for GL rendering" ) ;
        goto error ;
    }

    state->camera_component = camera ;

    return status ;

error:

    if ( camera )
        mmal_component_destroy ( camera ) ;

    return status ;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Destroy the camera component
 *
 * @param state Pointer to state control struct
 *
 */
static void destroy_camera_component ( RASPISTILL_STATE *state )
{
    if ( state->camera_component )
    {
        mmal_component_destroy ( state->camera_component ) ;
        state->camera_component = NULL ;
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Connect two specific ports together
 *
 * @param output_port Pointer the output port
 * @param input_port Pointer the input port
 * @param Pointer to a mmal connection pointer, reassigned if function successful
 * @return Returns a MMAL_STATUS_T giving result of operation
 *
 */
static MMAL_STATUS_T connect_ports ( MMAL_PORT_T *output_port , MMAL_PORT_T *input_port , MMAL_CONNECTION_T **connection )
{
    MMAL_STATUS_T status ;

    status = mmal_connection_create ( connection , output_port , input_port , MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT ) ;

    if ( status == MMAL_SUCCESS )
    {
        status = mmal_connection_enable ( *connection ) ;
        if ( status != MMAL_SUCCESS )
            mmal_connection_destroy ( *connection ) ;
    }

    return status ;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Checks if specified port is valid and enabled, then disables it
 *
 * @param port  Pointer the port
 *
 */
static void check_disable_port ( MMAL_PORT_T *port )
{
    if ( port && port->is_enabled )
        mmal_port_disable ( port ) ;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Handler for sigint signals
 *
 * @param signal_number ID of incoming signal.
 *
 */
static void signal_handler ( int signal_number )
{
    if ( signal_number == SIGUSR1 )
    {
        // Handle but ignore - prevents us dropping out if started in none-signal mode
        // and someone sends us the USR1 signal anyway
    }
    else
    {
        // Going to abort on all other signals
        vcos_log_error ( "Aborting program\n" ) ;
        exit ( 130 ) ;
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * main
 */
int main ( int argc , const char **argv )
{
    // Our main data storage vessel..
    RASPISTILL_STATE state ;
    int exit_code = EX_OK ;

    MMAL_STATUS_T status = MMAL_SUCCESS ;
    MMAL_PORT_T *camera_preview_port = NULL ;
    MMAL_PORT_T *camera_video_port = NULL ;
    MMAL_PORT_T *camera_still_port = NULL ;
    MMAL_PORT_T *preview_input_port = NULL ;
    MMAL_PORT_T *encoder_input_port = NULL ;
    MMAL_PORT_T *encoder_output_port = NULL ;

    bcm_host_init ( ) ;

    // Register our application with the logging system
    vcos_log_register ( "RaspiStill" , VCOS_LOG_CATEGORY ) ;

    signal ( SIGINT , signal_handler ) ;

    // Disable USR1 for the moment - may be reenabled if go in to signal capture mode
    signal ( SIGUSR1 , SIG_IGN ) ;

    default_status ( &state ) ;

    raspitex_init ( &state.raspitex_state ) ;

    // OK, we have a nice set of parameters. Now set up our components
    // We have three components. Camera, Preview and encoder.
    // Camera and encoder are different in stills/video, but preview
    // is the same so handed off to a separate module

    if ( ( status = create_camera_component ( &state ) ) != MMAL_SUCCESS )
    {
        vcos_log_error ( "%s: Failed to create camera component" , __func__ ) ;
        exit_code = EX_SOFTWARE ;
    }
    else
    {
        camera_preview_port = state.camera_component->output[MMAL_CAMERA_PREVIEW_PORT] ;
        camera_video_port = state.camera_component->output[MMAL_CAMERA_VIDEO_PORT] ;
        camera_still_port = state.camera_component->output[MMAL_CAMERA_CAPTURE_PORT] ;

        VCOS_STATUS_T vcos_status ;

        /* If GL preview is requested then start the GL threads */
        if ( ( raspitex_start ( &state.raspitex_state ) != 0 ) )
            goto error ;

        while ( 1 )
        {
            vcos_sleep ( 10000 ) ;
        }

error:

        mmal_status_to_int ( status ) ;

        raspitex_stop ( &state.raspitex_state ) ;
        raspitex_destroy ( &state.raspitex_state ) ;

        // Disable all our ports that are not handled by connections
        check_disable_port ( camera_video_port ) ;

        /* Disable components */
        if ( state.camera_component )
            mmal_component_disable ( state.camera_component ) ;

        destroy_camera_component ( &state ) ;
    }

    if ( status != MMAL_SUCCESS )
        raspicamcontrol_check_configuration ( 128 ) ;

    return exit_code ;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
