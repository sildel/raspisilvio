////////////////////////////////////////////////////////////////////////////////
//Copyright (c) 2013, Broadcom Europe Ltd
//Copyright (c) 2013, James Hughes
//All rights reserved.
//Many source code lines are copied from camcv_vid0.c - by Pierre Raufast
//Modified by Silvio Delgado
////////////////////////////////////////////////////////////////////////////////
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
// Max bitrate we allow for recording
const int MAX_BITRATE = 25000000 ; // 25Mbits/s
////////////////////////////////////////////////////////////////////////////////
/// Interval at which we check for an failure abort during capture
const int ABORT_INTERVAL = 100 ; // ms
////////////////////////////////////////////////////////////////////////////////
/// Capture/Pause switch method
/// Simply capture for time specified
#define WAIT_METHOD_NONE           0
/// Cycle between capture and pause for times specified
#define WAIT_METHOD_TIMED          1
/// Switch between capture and pause on keypress
#define WAIT_METHOD_KEYPRESS       2
/// Switch between capture and pause on signal
#define WAIT_METHOD_SIGNAL         3
/// Run/record forever
#define WAIT_METHOD_FOREVER        4
////////////////////////////////////////////////////////////////////////////////
int mmal_status_to_int ( MMAL_STATUS_T status ) ;
static void signal_handler ( int signal_number ) ;
////////////////////////////////////////////////////////////////////////////////
// Forward
typedef struct RASPIVID_STATE_S RASPIVID_STATE ;
////////////////////////////////////////////////////////////////////////////////

/** Struct used to pass information in encoder port userdata to callback
 */
typedef struct
{
    RASPIVID_STATE *pstate ; /// pointer to our state in case required in callback
    VCOS_SEMAPHORE_T complete_semaphore ; /// semaphore which is posted when we reach end of frame (indicates end of capture or fault)    
} PORT_USERDATA ;
////////////////////////////////////////////////////////////////////////////////

/** Structure containing all state information for the current run
 */
struct RASPIVID_STATE_S
{
    int width ; /// Requested width of image
    int height ; /// requested height of image
    int bitrate ; /// Requested bitrate
    int framerate ; /// Requested frame rate (fps)
    int nCount ; ///number of frames

    RASPICAM_CAMERA_PARAMETERS camera_parameters ; /// Camera setup parameters

    MMAL_COMPONENT_T *camera_component ; /// Pointer to the camera component    

    MMAL_POOL_T *video_pool ; /// Pointer to the pool of buffers used by video output port
} ;
////////////////////////////////////////////////////////////////////////////////

static void default_status ( RASPIVID_STATE *state )
{
    if ( ! state )
    {
        vcos_assert ( 0 ) ;
        return ;
    }

    // Default everything to zero
    memset ( state , 0 , sizeof (RASPIVID_STATE ) ) ;

    // Now set anything non-zero    
    state->width = 640 ; // Default to 1080p
    state->height = 480 ;
    state->bitrate = 17000000 ; // This is a decent default bitrate for 1080p
    state->framerate = VIDEO_FRAME_RATE_NUM ;

    // Set up the camera_parameters to default
    raspicamcontrol_set_defaults ( &state->camera_parameters ) ;

    state->nCount = 0 ;
}
////////////////////////////////////////////////////////////////////////////////

/**
 *  buffer header callback function for camera control
 *
 *  Callback will dump buffer data to the specific file
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
////////////////////////////////////////////////////////////////////////////////

/**
 *  buffer header callback function for video
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
static void video_buffer_callback ( MMAL_PORT_T *port , MMAL_BUFFER_HEADER_T *buffer )
{
    MMAL_BUFFER_HEADER_T *new_buffer ;
    PORT_USERDATA *pData = ( PORT_USERDATA * ) port->userdata ;

    if ( pData )
    {
        if ( buffer->length )
        {

            mmal_buffer_header_mem_lock ( buffer ) ;
            pData->pstate->nCount ++ ; // count frames displayed

            mmal_buffer_header_mem_unlock ( buffer ) ;
        }
        else vcos_log_error ( "buffer null" ) ;

    }
    else
    {
        vcos_log_error ( "Received a encoder buffer callback with no state" ) ;
    }

    // release buffer back to the pool
    mmal_buffer_header_release ( buffer ) ;

    // and send one back to the port (if still open)
    if ( port->is_enabled )
    {
        MMAL_STATUS_T status ;

        new_buffer = mmal_queue_get ( pData->pstate->video_pool->queue ) ;

        if ( new_buffer )
            status = mmal_port_send_buffer ( port , new_buffer ) ;

        if ( ! new_buffer || status != MMAL_SUCCESS )
            vcos_log_error ( "Unable to return a buffer to the encoder port" ) ;
    }
}
////////////////////////////////////////////////////////////////////////////////

/**
 * Create the camera component, set up its ports
 *
 * @param state Pointer to state control struct
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
static MMAL_STATUS_T create_camera_component ( RASPIVID_STATE *state )
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
                                                     .one_shot_stills = 0 ,
                                                     .max_preview_video_w = state->width ,
                                                     .max_preview_video_h = state->height ,
                                                     .num_preview_video_frames = 3 ,
                                                     .stills_capture_circular_buffer_height = 0 ,
                                                     .fast_preview_resume = 0 ,
                                                     .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
        } ;
        mmal_port_parameter_set ( camera->control , &cam_config.hdr ) ;
    }

    // Now set up the port formats

    // Set the encode format on the Preview port
    // HW limitations mean we need the preview to be the same size as the required recorded output

    format = preview_port->format ;

    format->encoding = MMAL_ENCODING_OPAQUE ;
    format->encoding_variant = MMAL_ENCODING_I420 ;

    format->encoding = MMAL_ENCODING_OPAQUE ;
    format->es->video.width = VCOS_ALIGN_UP ( state->width , 32 ) ;
    format->es->video.height = VCOS_ALIGN_UP ( state->height , 16 ) ;
    format->es->video.crop.x = 0 ;
    format->es->video.crop.y = 0 ;
    format->es->video.crop.width = state->width ;
    format->es->video.crop.height = state->height ;
    format->es->video.frame_rate.num = PREVIEW_FRAME_RATE_NUM ;
    format->es->video.frame_rate.den = PREVIEW_FRAME_RATE_DEN ;

    status = mmal_port_format_commit ( preview_port ) ;

    if ( status != MMAL_SUCCESS )
    {
        vcos_log_error ( "camera viewfinder format couldn't be set" ) ;
        goto error ;
    }

    // Set the encode format on the video  port

    format = video_port->format ;
    format->encoding_variant = MMAL_ENCODING_I420 ;

    format->encoding = MMAL_ENCODING_OPAQUE ;
    format->es->video.width = VCOS_ALIGN_UP ( state->width , 32 ) ;
    format->es->video.height = VCOS_ALIGN_UP ( state->height , 16 ) ;
    format->es->video.crop.x = 0 ;
    format->es->video.crop.y = 0 ;
    format->es->video.crop.width = state->width ;
    format->es->video.crop.height = state->height ;
    format->es->video.frame_rate.num = state->framerate ;
    format->es->video.frame_rate.den = VIDEO_FRAME_RATE_DEN ;

    status = mmal_port_format_commit ( video_port ) ;

    if ( status != MMAL_SUCCESS )
    {
        vcos_log_error ( "camera video format couldn't be set" ) ;
        goto error ;
    }

    // Plug the callback to the video port 
    status = mmal_port_enable ( video_port , video_buffer_callback ) ;
    if ( status )
    {
        vcos_log_error ( "camera video callback error" ) ;
        goto error ;
    }

    // Ensure there are enough buffers to avoid dropping frames
    if ( video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM )
        video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM ;


    // Set the encode format on the still  port

    format = still_port->format ;

    format->encoding = MMAL_ENCODING_OPAQUE ;
    format->encoding_variant = MMAL_ENCODING_I420 ;

    format->es->video.width = VCOS_ALIGN_UP ( state->width , 32 ) ;
    format->es->video.height = VCOS_ALIGN_UP ( state->height , 16 ) ;
    format->es->video.crop.x = 0 ;
    format->es->video.crop.y = 0 ;
    format->es->video.crop.width = state->width ;
    format->es->video.crop.height = state->height ;
    format->es->video.frame_rate.num = 0 ;
    format->es->video.frame_rate.den = 1 ;

    status = mmal_port_format_commit ( still_port ) ;

    if ( status != MMAL_SUCCESS )
    {
        vcos_log_error ( "camera still format couldn't be set" ) ;
        goto error ;
    }

    /* Ensure there are enough buffers to avoid dropping frames */
    if ( still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM )
        still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM ;

    //Pool
    MMAL_POOL_T *pool ;
    video_port->buffer_size = video_port->buffer_size_recommended ;
    video_port->buffer_num = video_port->buffer_num_recommended ;
    pool = mmal_port_pool_create ( video_port , video_port->buffer_num , video_port->buffer_size ) ;
    if ( ! pool )
    {
        vcos_log_error ( "Failed to create buffer header pool for video output port" ) ;
    }
    state->video_pool = pool ;

    /* Enable component */
    status = mmal_component_enable ( camera ) ;

    if ( status != MMAL_SUCCESS )
    {
        vcos_log_error ( "camera component couldn't be enabled" ) ;
        goto error ;
    }

    raspicamcontrol_set_all_parameters ( camera , &state->camera_parameters ) ;

    state->camera_component = camera ;

    return status ;

error:

    if ( camera )
        mmal_component_destroy ( camera ) ;

    return status ;
}
////////////////////////////////////////////////////////////////////////////////

/**
 * Destroy the camera component
 *
 * @param state Pointer to state control struct
 *
 */
static void destroy_camera_component ( RASPIVID_STATE *state )
{
    if ( state->camera_component )
    {
        mmal_component_destroy ( state->camera_component ) ;
        state->camera_component = NULL ;
    }
}
////////////////////////////////////////////////////////////////////////////////

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
////////////////////////////////////////////////////////////////////////////////

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
////////////////////////////////////////////////////////////////////////////////

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
////////////////////////////////////////////////////////////////////////////////

int main ( int argc , const char **argv )
{
    // Our main data storage vessel..
    RASPIVID_STATE state ;
    int exit_code = EX_OK ;

    MMAL_STATUS_T status = MMAL_SUCCESS ;

    MMAL_PORT_T *camera_video_port = NULL ;

    time_t timer_begin , timer_end ;
    double secondsElapsed ;

    bcm_host_init ( ) ;

    // Register our application with the logging system
    vcos_log_register ( "RaspiVid" , VCOS_LOG_CATEGORY ) ;

    signal ( SIGINT , signal_handler ) ;

    // Disable USR1 for the moment - may be reenabled if go in to signal capture mode
    signal ( SIGUSR1 , SIG_IGN ) ;

    default_status ( &state ) ;

    // OK, we have a nice set of parameters. Now set up our components
    // We have three components. Camera, Preview and encoder.
    if ( ( status = create_camera_component ( &state ) ) != MMAL_SUCCESS )
    {
        vcos_log_error ( "%s: Failed to create camera component" , __func__ ) ;
        exit_code = EX_SOFTWARE ;
    }
    else
    {
        PORT_USERDATA callback_data ;
        callback_data.pstate = & state ;
        VCOS_STATUS_T vcos_status ;
        vcos_status = vcos_semaphore_create ( &callback_data.complete_semaphore , "RaspiStill-sem" , 0 ) ;
        vcos_assert ( vcos_status == VCOS_SUCCESS ) ;

        camera_video_port = state.camera_component->output[MMAL_CAMERA_VIDEO_PORT] ;
        // assign data to use for callback
        camera_video_port->userdata = ( struct MMAL_PORT_USERDATA_T * ) &callback_data ;

        // initialize timer
        time ( &timer_begin ) ;

        // start capture
        if ( mmal_port_parameter_set_boolean ( camera_video_port , MMAL_PARAMETER_CAPTURE , 1 ) != MMAL_SUCCESS )
        {
            goto error ;
        }

        // Send all the buffers to the video port
        int num = mmal_queue_length ( state.video_pool->queue ) ;
        int q ;
        for ( q = 0 ; q < num ; q ++ )
        {
            MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get ( state.video_pool->queue ) ;

            if ( ! buffer )
                vcos_log_error ( "Unable to get a required buffer %d from pool queue" , q ) ;

            if ( mmal_port_send_buffer ( camera_video_port , buffer ) != MMAL_SUCCESS )
                vcos_log_error ( "Unable to send a buffer to encoder output port (%d)" , q ) ;
        }


        // Now wait until we need to stop
        vcos_sleep ( 5000 ) ;
error:
        time ( &timer_end ) ;
        secondsElapsed = difftime ( timer_end , timer_begin ) ;

        printf ( "%.f seconds for %d frames : FPS = %f\n" , secondsElapsed , state.nCount , ( float ) ( ( float ) ( state.nCount ) / secondsElapsed ) ) ;

        mmal_status_to_int ( status ) ;

        if ( state.camera_component )
            mmal_component_disable ( state.camera_component ) ;

        destroy_camera_component ( &state ) ;
    }

    if ( status != MMAL_SUCCESS )
        raspicamcontrol_check_configuration ( 128 ) ;    

    return exit_code ;
}
////////////////////////////////////////////////////////////////////////////////
