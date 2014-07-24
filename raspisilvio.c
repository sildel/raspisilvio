////////////////////////////////////////////////////////////////////////////////
//Copyright (c) 2013, Broadcom Europe Ltd
//Copyright (c) 2013, James Hughes
//All rights reserved.
//Many source code lines are copied from camcv_vid0.c - by Pierre Raufast
//Modified by Silvio Delgado
////////////////////////////////////////////////////////////////////////////////
#include "raspisilvio.h"
////////////////////////////////////////////////////////////////////////////////
int the_flag ;

void default_status ( RASPIVID_STATE *state )
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

    state->H_min_value = 100 ;
    state->S_min_value = 115 ;
    state->V_min_value = 40 ;
    state->H_max_value = 110 ;
    state->S_max_value = 205 ;
    state->V_max_value = 116 ;
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
void camera_control_callback ( MMAL_PORT_T *port , MMAL_BUFFER_HEADER_T *buffer )
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
void video_buffer_callback ( MMAL_PORT_T *port , MMAL_BUFFER_HEADER_T *buffer )
{
    MMAL_BUFFER_HEADER_T *new_buffer ;
    RASPIVID_STATE *pData = ( RASPIVID_STATE * ) port->userdata ;

    if ( pData )
    {
        if ( buffer->length )
        {
            mmal_buffer_header_mem_lock ( buffer ) ;

            int w = pData->width ; // get image size
            int h = pData->height ;
            int h4 = h / 4 ;

            memcpy ( pData->py->imageData , buffer->data , w * h ) ; // read Y

            if ( 1 == 1 )
            {
                if ( the_flag == 1 )
                {
                    memcpy ( pData->pu->imageData , buffer->data + w*h , w * h4 ) ; // read U
                    memcpy ( pData->pv->imageData , buffer->data + w * h + w*h4 , w * h4 ) ; // read v

                    cvResize ( pData->pu , pData->pu_big , CV_INTER_NN ) ;
                    cvResize ( pData->pv , pData->pv_big , CV_INTER_NN ) ; //CV_INTER_LINEAR looks better but it's slower
                    cvMerge ( pData->py , pData->pu_big , pData->pv_big , NULL , pData->image ) ;
                    cvCvtColor ( pData->image , pData->dstImage , CV_YCrCb2RGB ) ; // convert in RGB color space (slow)
                    cvCvtColor ( pData->dstImage , pData->hsvImage , CV_RGB2HSV ) ; // convert in HSV color space (slow)
                    cvShowImage ( "videoWindow2" , pData->dstImage ) ; //display only gray channel
                    printf ( "Doing %d\n" , pData->nCount ) ;
                }

                cvInRangeS ( pData->hsvImage , cvScalar ( pData->H_min_value , pData->S_min_value , pData->V_min_value , 0 ) , cvScalar ( pData->H_max_value , pData->S_max_value , pData->V_max_value , 0 ) , pData->imgThresh ) ;

                cvShowImage ( "videoWindow" , pData->imgThresh ) ;
            }
            else
            {
                cvShowImage ( "videoWindow" , pData->py ) ; //display only gray channel
            }

            cvWaitKey ( 1 ) ;

            pData->nCount ++ ; // count frames displayed

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

        new_buffer = mmal_queue_get ( pData->video_pool->queue ) ;

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
MMAL_STATUS_T create_camera_component ( RASPIVID_STATE *state )
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
    format->encoding = MMAL_ENCODING_I420 ;
    format->es->video.width = state->width ;
    format->es->video.height = state->height ;
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
void destroy_camera_component ( RASPIVID_STATE *state )
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
MMAL_STATUS_T connect_ports ( MMAL_PORT_T *output_port , MMAL_PORT_T *input_port , MMAL_CONNECTION_T **connection )
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
void check_disable_port ( MMAL_PORT_T *port )
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
void signal_handler ( int signal_number )
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
        if ( the_flag == 0 )
        {
            exit ( 130 ) ;
        }
        else
        {
            the_flag = 0 ;
        }
    }
}
////////////////////////////////////////////////////////////////////////////////
