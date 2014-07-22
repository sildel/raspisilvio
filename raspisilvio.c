////////////////////////////////////////////////////////////////////////////////
//Copyright (c) 2013, Broadcom Europe Ltd
//Copyright (c) 2013, James Hughes
//All rights reserved.
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
    FILE *file_handle ; /// File handle to write buffer data to.
    RASPIVID_STATE *pstate ; /// pointer to our state in case required in callback
    int abort ; /// Set to 1 in callback if an error occurs to attempt to abort the capture
    char *cb_buff ; /// Circular buffer
    int cb_len ; /// Length of buffer
    int cb_wptr ; /// Current write pointer
    int cb_wrap ; /// Has buffer wrapped at least once?
    int cb_data ; /// Valid bytes in buffer
#define IFRAME_BUFSIZE (60*1000)
    int iframe_buff[IFRAME_BUFSIZE] ; /// buffer of iframe pointers
    int iframe_buff_wpos ;
    int iframe_buff_rpos ;
    char header_bytes[29] ;
    int header_wptr ;
    FILE *imv_file_handle ; /// File handle to write inline motion vectors to.
} PORT_USERDATA ;
////////////////////////////////////////////////////////////////////////////////

/** Structure containing all state information for the current run
 */
struct RASPIVID_STATE_S
{
    int timeout ; /// Time taken before frame is grabbed and app then shuts down. Units are milliseconds
    int width ; /// Requested width of image
    int height ; /// requested height of image
    int bitrate ; /// Requested bitrate
    int framerate ; /// Requested frame rate (fps)
    int intraperiod ; /// Intra-refresh period (key frame rate)
    int quantisationParameter ; /// Quantisation parameter - quality. Set bitrate 0 and set this for variable bitrate
    int bInlineHeaders ; /// Insert inline headers to stream (SPS, PPS)
    char *filename ; /// filename of output file
    int verbose ; /// !0 if want detailed run information
    int demoMode ; /// Run app in demo mode
    int demoInterval ; /// Interval between camera settings changes
    int immutableInput ; /// Flag to specify whether encoder works in place or creates a new buffer. Result is preview can display either
    /// the camera output or the encoder output (with compression artifacts)
    int profile ; /// H264 profile to use for encoding
    int waitMethod ; /// Method for switching between pause and capture

    int onTime ; /// In timed cycle mode, the amount of time the capture is on per cycle
    int offTime ; /// In timed cycle mode, the amount of time the capture is off per cycle

    int segmentSize ; /// Segment mode In timed cycle mode, the amount of time the capture is off per cycle
    int segmentWrap ; /// Point at which to wrap segment counter
    int segmentNumber ; /// Current segment counter
    int splitNow ; /// Split at next possible i-frame if set to 1.
    int splitWait ; /// Switch if user wants splited files 

    RASPIPREVIEW_PARAMETERS preview_parameters ; /// Preview setup parameters
    RASPICAM_CAMERA_PARAMETERS camera_parameters ; /// Camera setup parameters

    MMAL_COMPONENT_T *camera_component ; /// Pointer to the camera component
    MMAL_COMPONENT_T *encoder_component ; /// Pointer to the encoder component
    MMAL_CONNECTION_T *preview_connection ; /// Pointer to the connection from camera to preview
    MMAL_CONNECTION_T *encoder_connection ; /// Pointer to the connection from camera to encoder

    MMAL_POOL_T *encoder_pool ; /// Pointer to the pool of buffers used by encoder output port

    PORT_USERDATA callback_data ; /// Used to move data to the encoder callback

    int bCapturing ; /// State of capture/pause
    int bCircularBuffer ; /// Whether we are writing to a circular buffer

    int inlineMotionVectors ; /// Encoder outputs inline Motion Vectors
    char *imv_filename ; /// filename of inline Motion Vectors output

} ;
////////////////////////////////////////////////////////////////////////////////
/// Structure to cross reference H264 profile strings against the MMAL parameter equivalent
static XREF_T profile_map[] = {
    {"baseline" , MMAL_VIDEO_PROFILE_H264_BASELINE } ,
    {"main" , MMAL_VIDEO_PROFILE_H264_MAIN } ,
    {"high" , MMAL_VIDEO_PROFILE_H264_HIGH } ,
                               //   {"constrained",  MMAL_VIDEO_PROFILE_H264_CONSTRAINED_BASELINE} // Does anyone need this?
} ;
////////////////////////////////////////////////////////////////////////////////
static int profile_map_size = sizeof (profile_map ) / sizeof (profile_map[0] ) ;
////////////////////////////////////////////////////////////////////////////////
static XREF_T initial_map[] = {
    {"record" , 0 } ,
    {"pause" , 1 } ,
} ;
////////////////////////////////////////////////////////////////////////////////
static int initial_map_size = sizeof (initial_map ) / sizeof (initial_map[0] ) ;
////////////////////////////////////////////////////////////////////////////////
static void display_valid_parameters ( char *app_name ) ;
////////////////////////////////////////////////////////////////////////////////
/// Command ID's and Structure defining our command line options
#define CommandHelp         0
#define CommandWidth        1
#define CommandHeight       2
#define CommandBitrate      3
#define CommandOutput       4
#define CommandVerbose      5
#define CommandTimeout      6
#define CommandDemoMode     7
#define CommandFramerate    8
#define CommandPreviewEnc   9
#define CommandIntraPeriod  10
#define CommandProfile      11
#define CommandTimed        12
#define CommandSignal       13
#define CommandKeypress     14
#define CommandInitialState 15
#define CommandQP           16
#define CommandInlineHeaders 17
#define CommandSegmentFile  18
#define CommandSegmentWrap  19
#define CommandSegmentStart 20
#define CommandSplitWait    21
#define CommandCircular     22
#define CommandIMV          23
////////////////////////////////////////////////////////////////////////////////
static COMMAND_LIST cmdline_commands[] = {
    { CommandHelp , "-help" , "?" , "This help information" , 0 } ,
    { CommandWidth , "-width" , "w" , "Set image width <size>. Default 1920" , 1 } ,
    { CommandHeight , "-height" , "h" , "Set image height <size>. Default 1080" , 1 } ,
    { CommandBitrate , "-bitrate" , "b" , "Set bitrate. Use bits per second (e.g. 10MBits/s would be -b 10000000)" , 1 } ,
    { CommandOutput , "-output" , "o" , "Output filename <filename> (to write to stdout, use '-o -')" , 1 } ,
    { CommandVerbose , "-verbose" , "v" , "Output verbose information during run" , 0 } ,
    { CommandTimeout , "-timeout" , "t" , "Time (in ms) to capture for. If not specified, set to 5s. Zero to disable" , 1 } ,
    { CommandDemoMode , "-demo" , "d" , "Run a demo mode (cycle through range of camera options, no capture)" , 1 } ,
    { CommandFramerate , "-framerate" , "fps" , "Specify the frames per second to record" , 1 } ,
    { CommandPreviewEnc , "-penc" , "e" , "Display preview image *after* encoding (shows compression artifacts)" , 0 } ,
    { CommandIntraPeriod , "-intra" , "g" , "Specify the intra refresh period (key frame rate/GoP size)" , 1 } ,
    { CommandProfile , "-profile" , "pf" , "Specify H264 profile to use for encoding" , 1 } ,
    { CommandTimed , "-timed" , "td" , "Cycle between capture and pause. -cycle on,off where on is record time and off is pause time in ms" , 0 } ,
    { CommandSignal , "-signal" , "s" , "Cycle between capture and pause on Signal" , 0 } ,
    { CommandKeypress , "-keypress" , "k" , "Cycle between capture and pause on ENTER" , 0 } ,
    { CommandInitialState , "-initial" , "i" , "Initial state. Use 'record' or 'pause'. Default 'record'" , 1 } ,
    { CommandQP , "-qp" , "qp" , "Quantisation parameter. Use approximately 10-40. Default 0 (off)" , 1 } ,
    { CommandInlineHeaders , "-inline" , "ih" , "Insert inline headers (SPS, PPS) to stream" , 0 } ,
    { CommandSegmentFile , "-segment" , "sg" , "Segment output file in to multiple files at specified interval <ms>" , 1 } ,
    { CommandSegmentWrap , "-wrap" , "wr" , "In segment mode, wrap any numbered filename back to 1 when reach number" , 1 } ,
    { CommandSegmentStart , "-start" , "sn" , "In segment mode, start with specified segment number" , 1 } ,
    { CommandSplitWait , "-split" , "sp" , "In wait mode, create new output file for each start event" , 0 } ,
    { CommandCircular , "-circular" , "c" , "Run encoded data through circular buffer until triggered then save" , 0 } ,
    { CommandIMV , "-vectors" , "x" , "Output filename <filename> for inline motion vectors" , 1 } ,
} ;
////////////////////////////////////////////////////////////////////////////////
static int cmdline_commands_size = sizeof (cmdline_commands ) / sizeof (cmdline_commands[0] ) ;
////////////////////////////////////////////////////////////////////////////////

static struct
{
    char *description ;
    int nextWaitMethod ;
} wait_method_description[] = {
    {"Simple capture" , WAIT_METHOD_NONE } ,
    {"Capture forever" , WAIT_METHOD_FOREVER } ,
    {"Cycle on time" , WAIT_METHOD_TIMED } ,
    {"Cycle on keypress" , WAIT_METHOD_KEYPRESS } ,
    {"Cycle on signal" , WAIT_METHOD_SIGNAL } ,
} ;
////////////////////////////////////////////////////////////////////////////////
static int wait_method_description_size = sizeof (wait_method_description ) / sizeof (wait_method_description[0] ) ;
////////////////////////////////////////////////////////////////////////////////

/**
 * Assign a default set of parameters to the state passed in
 *
 * @param state Pointer to state structure to assign defaults to
 */
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
    state->timeout = 5000 ; // 5s delay before take image
    state->width = 640 ; // Default to 1080p
    state->height = 480 ;
    state->bitrate = 17000000 ; // This is a decent default bitrate for 1080p
    state->framerate = VIDEO_FRAME_RATE_NUM ;
    state->intraperiod = 0 ; // Not set
    state->quantisationParameter = 0 ;
    state->demoMode = 0 ;
    state->demoInterval = 250 ; // ms
    state->immutableInput = 1 ;
    state->profile = MMAL_VIDEO_PROFILE_H264_HIGH ;
    state->waitMethod = WAIT_METHOD_NONE ;
    state->onTime = 5000 ;
    state->offTime = 5000 ;

    state->bCapturing = 0 ;
    state->bInlineHeaders = 0 ;

    state->segmentSize = 0 ; // 0 = not segmenting the file.
    state->segmentNumber = 1 ;
    state->segmentWrap = 0 ; // Point at which to wrap segment number back to 1. 0 = no wrap
    state->splitNow = 0 ;
    state->splitWait = 0 ;

    state->inlineMotionVectors = 0 ;

    // Setup preview window defaults
    raspipreview_set_defaults ( &state->preview_parameters ) ;

    // Set up the camera_parameters to default
    raspicamcontrol_set_defaults ( &state->camera_parameters ) ;
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

    /* Enable component */
    status = mmal_component_enable ( camera ) ;

    if ( status != MMAL_SUCCESS )
    {
        vcos_log_error ( "camera component couldn't be enabled" ) ;
        goto error ;
    }

    raspicamcontrol_set_all_parameters ( camera , &state->camera_parameters ) ;

    state->camera_component = camera ;

    if ( state->verbose )
        fprintf ( stderr , "Camera component done\n" ) ;

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
