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
    
    the_flag = 1;
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
    state.py = cvCreateImage ( cvSize ( w , h ) , IPL_DEPTH_8U , 1 ) ; // Y component of YUV I420 frame
    state.pu = cvCreateImage ( cvSize ( w / 2 , h / 2 ) , IPL_DEPTH_8U , 1 ) ; // U component of YUV I420 frame
    state.pv = cvCreateImage ( cvSize ( w / 2 , h / 2 ) , IPL_DEPTH_8U , 1 ) ; // V component of YUV I420 frame
    state.pu_big = cvCreateImage ( cvSize ( w , h ) , IPL_DEPTH_8U , 1 ) ;
    state.pv_big = cvCreateImage ( cvSize ( w , h ) , IPL_DEPTH_8U , 1 ) ;
    state.image = cvCreateImage ( cvSize ( w , h ) , IPL_DEPTH_8U , 3 ) ; // final picture to display
    state.dstImage = cvCreateImage ( cvSize ( w , h ) , IPL_DEPTH_8U , 3 ) ;
    state.imgThresh = cvCreateImage ( cvSize ( w , h ) , IPL_DEPTH_8U , 1 ) ;
    state.hsvImage = cvCreateImage ( cvSize ( w , h ) , IPL_DEPTH_8U , 3 ) ; // final picture to display

    cvNamedWindow ( "videoWindow" , CV_WINDOW_AUTOSIZE ) ;
    cvNamedWindow ( "videoWindow2" , CV_WINDOW_AUTOSIZE ) ;

    cvNamedWindow ( "Tune HSV" , CV_WINDOW_AUTOSIZE ) ;

    cvCreateTrackbar2 ( "H min value" , "Tune HSV" , &state.H_min_value , 255 , NULL , NULL ) ;
    cvCreateTrackbar2 ( "S min value" , "Tune HSV" , &state.S_min_value , 255 , NULL , NULL ) ;
    cvCreateTrackbar2 ( "V min value" , "Tune HSV" , &state.V_min_value , 255 , NULL , NULL ) ;

    cvCreateTrackbar2 ( "H max value" , "Tune HSV" , &state.H_max_value , 255 , NULL , NULL ) ;
    cvCreateTrackbar2 ( "S max value" , "Tune HSV" , &state.S_max_value , 255 , NULL , NULL ) ;
    cvCreateTrackbar2 ( "V max value" , "Tune HSV" , &state.V_max_value , 255 , NULL , NULL ) ;

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

        while ( 1 )
        {
            vcos_sleep ( 2000 ) ;
        }

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
    cvReleaseImage ( &state.pu ) ;
    cvReleaseImage ( &state.pv ) ;
    cvReleaseImage ( &state.pu_big ) ;
    cvReleaseImage ( &state.pv_big ) ;
    cvReleaseImage ( &state.dstImage ) ;
    cvReleaseImage ( &state.image ) ;
    cvReleaseImage ( &state.imgThresh ) ;
    cvReleaseImage ( &state.hsvImage ) ;

    cvDestroyWindow ( "videoWindow" ) ;
    cvDestroyWindow ( "videoWindow2" ) ;
    cvDestroyWindow ( "Tune HSV" ) ;

    return exit_code ;
}
////////////////////////////////////////////////////////////////////////////////
