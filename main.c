////////////////////////////////////////////////////////////////////////////////
//Copyright (c) 2013, Broadcom Europe Ltd
//Copyright (c) 2013, James Hughes
//All rights reserved.
//Many source code lines are copied from camcv_vid0.c - by Pierre Raufast
//Modified by Silvio Delgado
////////////////////////////////////////////////////////////////////////////////
#include "raspisilvio.h"
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

    int w = state.width ;
    int h = state.height ;
    state.py = cvCreateImage ( cvSize ( w , h ) , IPL_DEPTH_8U , 1 ) ;

    // OK, we have a nice set of parameters. Now set up our components
    // We have three components. Camera, Preview and encoder.
    if ( ( status = create_camera_component ( &state ) ) != MMAL_SUCCESS )
    {
        vcos_log_error ( "%s: Failed to create camera component" , __func__ ) ;
        exit_code = EX_SOFTWARE ;
    }
    else
    {
        camera_video_port = state.camera_component->output[MMAL_CAMERA_VIDEO_PORT] ;
        // assign data to use for callback
        camera_video_port->userdata = ( struct MMAL_PORT_USERDATA_T * ) &state ;

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
        vcos_sleep ( 10000 ) ;
error:
        time ( &timer_end ) ;
        secondsElapsed = difftime ( timer_end , timer_begin ) ;

        printf ( "%.f seconds for %d frames : FPS = %f\n" , secondsElapsed , state.nCount , ( ( float ) state.nCount ) / secondsElapsed ) ;

        mmal_status_to_int ( status ) ;

        if ( state.camera_component )
            mmal_component_disable ( state.camera_component ) ;

        destroy_camera_component ( &state ) ;
    }

    if ( status != MMAL_SUCCESS )
        raspicamcontrol_check_configuration ( 128 ) ;

    cvReleaseImage ( &state.py ) ;

    return exit_code ;
}
////////////////////////////////////////////////////////////////////////////////
