///////////////////////////////////////////////////////////////////////////////////////////////////
/*
Copyright (c) 2013, Broadcom Europe Ltd
Copyright (c) 2013, Tim Gover
All rights reserved.


Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
 * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
///////////////////////////////////////////////////////////////////////////////////////////////////
//Modified by
//Jorge Silvio Delgado Morales
//silviodelgado70
///////////////////////////////////////////////////////////////////////////////////////////////////
#include "matching.h"
#include "RaspiTex.h"
#include "RaspiTexUtil.h"
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
///////////////////////////////////////////////////////////////////////////////////////////////////
static GLfloat quad_varray[] = {
                                - 1.0f , - 1.0f , 1.0f , 1.0f , 1.0f , - 1.0f ,
                                - 1.0f , 1.0f , 1.0f , 1.0f , - 1.0f , - 1.0f ,
} ;
///////////////////////////////////////////////////////////////////////////////////////////////////
static GLuint quad_vbo ;
///////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t *pixels_from_fb ;
uint8_t *pixels_hist ;
HISTOGRAM intensity_hist ;
HISTOGRAM hue_hist ;
int hue_threshold = 100 ;
int intensity_threshold = 100 ;
int hue_umbral = 10 ;
int intensity_umbral = 10 ;
///////////////////////////////////////////////////////////////////////////////////////////////////
int render_id = 0 ;
GLuint hist_tex_id ;
GLuint hsi_tex_id ;
GLuint hsi_fbo_id ;
HEADING_REGIONS heads = {
                         .xb1 = - 0.8f ,
                         .xb2 = 0.8f ,
                         .xu1 = - 0.2f ,
                         .xu2 = 0.2f ,
                         .y1 = - 0.5f ,
                         .y2 = 0.5f
} ;
///////////////////////////////////////////////////////////////////////////////////////////////////
static RASPITEXUTIL_SHADER_PROGRAM_T preview_shader = {
                                                       .vs_file = "gl_scenes/simpleVS.glsl" ,
                                                       .fs_file = "gl_scenes/simpleEFS.glsl" ,
                                                       .vertex_source = "" ,
                                                       .fragment_source = "" ,
                                                       .uniform_names =
    {"tex" } ,
                                                       .attribute_names =
    {"vertex" } ,
} ;
///////////////////////////////////////////////////////////////////////////////////////////////////
static RASPITEXUTIL_SHADER_PROGRAM_T lines_shader = {
                                                     .vs_file = "gl_scenes/linesVS.glsl" ,
                                                     .fs_file = "gl_scenes/linesFS.glsl" ,
                                                     .vertex_source = "" ,
                                                     .fragment_source = "" ,
                                                     .uniform_names =
    {"color" } ,
                                                     .attribute_names =
    {"vertex" } ,
} ;
///////////////////////////////////////////////////////////////////////////////////////////////////
static RASPITEXUTIL_SHADER_PROGRAM_T gauss_hsi_shader = {
                                                         .vs_file = "gl_scenes/simpleVS.glsl" ,
                                                         .fs_file = "gl_scenes/gaussian5hsiFS.glsl" ,
                                                         .vertex_source = "" ,
                                                         .fragment_source = "" ,
                                                         .uniform_names =
    {"tex" , "tex_unit" } ,
                                                         .attribute_names =
    {"vertex" } ,
} ;
///////////////////////////////////////////////////////////////////////////////////////////////////
static RASPITEXUTIL_SHADER_PROGRAM_T mask_shader = {
                                                    .vs_file = "gl_scenes/simpleVS.glsl" ,
                                                    .fs_file = "gl_scenes/maskFS.glsl" ,
                                                    .vertex_source = "" ,
                                                    .fragment_source = "" ,
                                                    .uniform_names =
    {"tex" , "heads" } ,
                                                    .attribute_names =
    {"vertex" } ,
} ;
///////////////////////////////////////////////////////////////////////////////////////////////////
static RASPITEXUTIL_SHADER_PROGRAM_T hist_shader = {
                                                    .vs_file = "gl_scenes/simpleVS.glsl" ,
                                                    .fs_file = "gl_scenes/histCFS.glsl" ,
                                                    .vertex_source = "" ,
                                                    .fragment_source = "" ,
                                                    .uniform_names =
    {"tex" , "hist" } ,
                                                    .attribute_names =
    {"vertex" } ,
} ;
///////////////////////////////////////////////////////////////////////////////////////////////////
static const EGLint matching_egl_config_attribs[] = {
                                                     EGL_RED_SIZE , 8 ,
                                                     EGL_GREEN_SIZE , 8 ,
                                                     EGL_BLUE_SIZE , 8 ,
                                                     EGL_ALPHA_SIZE , 8 ,
                                                     EGL_RENDERABLE_TYPE , EGL_OPENGL_ES2_BIT ,
                                                     EGL_NONE
} ;
///////////////////////////////////////////////////////////////////////////////////////////////////

void LoadShader ( RASPITEXUTIL_SHADER_PROGRAM_T *shader )
{
    char *vs_source = NULL ;
    char *fs_source = NULL ;
    FILE * f ;
    int sz ;

    assert ( ! vs_source ) ;
    f = fopen ( shader->vs_file , "rb" ) ;
    assert ( f ) ;
    fseek ( f , 0 , SEEK_END ) ;
    sz = ftell ( f ) ;
    fseek ( f , 0 , SEEK_SET ) ;
    vs_source = malloc ( sz + 1 ) ;
    fread ( vs_source , 1 , sz , f ) ;
    vs_source[sz] = 0 ; //null terminate it!
    fclose ( f ) ;

    assert ( ! fs_source ) ;
    f = fopen ( shader->fs_file , "rb" ) ;
    assert ( f ) ;
    fseek ( f , 0 , SEEK_END ) ;
    sz = ftell ( f ) ;
    fseek ( f , 0 , SEEK_SET ) ;
    fs_source = malloc ( sz + 1 ) ;
    fread ( fs_source , 1 , sz , f ) ;
    fs_source[sz] = 0 ; //null terminate it!
    fclose ( f ) ;

    shader->vertex_source = vs_source ;
    shader->fragment_source = fs_source ;
    raspitexutil_build_shader_program ( shader ) ;

    free ( vs_source ) ;
    free ( fs_source ) ;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void LoadShadersFromFiles ( )
{
    LoadShader ( &preview_shader ) ;
    LoadShader ( &lines_shader ) ;
    LoadShader ( &gauss_hsi_shader ) ;
    LoadShader ( &mask_shader ) ;
    //    LoadShader ( &hist_shader ) ;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Creates the OpenGL ES 2.X context and builds the shaders.
 * @param raspitex_state A pointer to the GL preview state.
 * @return Zero if successful.
 */
static int matching_init ( RASPITEX_STATE *raspitex_state )
{
    int rc = 0 ;
    int width = raspitex_state->width ;
    int height = raspitex_state->height ;

    vcos_log_trace ( "%s" , VCOS_FUNCTION ) ;
    raspitex_state->egl_config_attribs = matching_egl_config_attribs ;
    rc = raspitexutil_gl_init_2_0 ( raspitex_state ) ;
    if ( rc != 0 )
        goto end ;

    pixels_from_fb = malloc ( raspitex_state->width * raspitex_state->height * 4 ) ;
    pixels_hist = malloc ( 257 * 3 * 4 ) ;

    LoadShadersFromFiles ( ) ;

    GLCHK ( glEnable ( GL_TEXTURE_2D ) ) ;
    GLCHK ( glGenTextures ( 1 , &hist_tex_id ) ) ;
    GLCHK ( glBindTexture ( GL_TEXTURE_2D , hist_tex_id ) ) ;
    GLCHK ( glTexImage2D ( GL_TEXTURE_2D , 0 , GL_RGBA , 257 , 3 , 0 , GL_RGBA , GL_UNSIGNED_BYTE , NULL ) ) ;
    GLCHK ( glTexParameterf ( GL_TEXTURE_2D , GL_TEXTURE_MIN_FILTER , ( GLfloat ) GL_NEAREST ) ) ;
    GLCHK ( glTexParameterf ( GL_TEXTURE_2D , GL_TEXTURE_MAG_FILTER , ( GLfloat ) GL_NEAREST ) ) ;

    GLCHK ( glEnable ( GL_TEXTURE_2D ) ) ;
    GLCHK ( glGenTextures ( 1 , &hsi_tex_id ) ) ;
    GLCHK ( glBindTexture ( GL_TEXTURE_2D , hsi_tex_id ) ) ;
    GLCHK ( glTexImage2D ( GL_TEXTURE_2D , 0 , GL_RGBA , raspitex_state->width , raspitex_state->height , 0 , GL_RGBA , GL_UNSIGNED_BYTE , NULL ) ) ;
    GLCHK ( glTexParameterf ( GL_TEXTURE_2D , GL_TEXTURE_MIN_FILTER , ( GLfloat ) GL_NEAREST ) ) ;
    GLCHK ( glTexParameterf ( GL_TEXTURE_2D , GL_TEXTURE_MAG_FILTER , ( GLfloat ) GL_NEAREST ) ) ;

    GLCHK ( glGenFramebuffers ( 1 , &hsi_fbo_id ) ) ;
    GLCHK ( glBindFramebuffer ( GL_FRAMEBUFFER , hsi_fbo_id ) ) ;
    GLCHK ( glFramebufferTexture2D ( GL_FRAMEBUFFER , GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D , hsi_tex_id , 0 ) ) ;

    GLCHK ( glBindFramebuffer ( GL_FRAMEBUFFER , 0 ) ) ;

    GLCHK ( glGenBuffers ( 1 , &quad_vbo ) ) ;
    GLCHK ( glBindBuffer ( GL_ARRAY_BUFFER , quad_vbo ) ) ;
    GLCHK ( glBufferData ( GL_ARRAY_BUFFER , sizeof (quad_varray ) , quad_varray , GL_STATIC_DRAW ) ) ;
    GLCHK ( glClearColor ( 0.0f , 0.0f , 0.0f , 1.0f ) ) ;

end:
    return rc ;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/* Redraws the scene with the latest buffer.
 *
 * @param raspitex_state A pointer to the GL preview state.
 * @return Zero if successful.
 */
static int matching_redraw ( RASPITEX_STATE* state )
{
    int i , j ;

    switch ( render_id )
    {
        case 0:
            GLCHK ( glBindFramebuffer ( GL_FRAMEBUFFER , 0 ) ) ;
            GLCHK ( glViewport ( 0 , 0 , state->width , state->height ) ) ;
            glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) ;

            GLCHK ( glUseProgram ( preview_shader.program ) ) ;
            GLCHK ( glUniform1i ( preview_shader.uniform_locations[0] , 0 ) ) ;

            GLCHK ( glActiveTexture ( GL_TEXTURE0 ) ) ;
            GLCHK ( glBindTexture ( GL_TEXTURE_EXTERNAL_OES , state->texture ) ) ;
            GLCHK ( glBindBuffer ( GL_ARRAY_BUFFER , quad_vbo ) ) ;
            GLCHK ( glEnableVertexAttribArray ( preview_shader.attribute_locations[0] ) ) ;
            GLCHK ( glVertexAttribPointer ( preview_shader.attribute_locations[0] , 2 , GL_FLOAT , GL_FALSE , 0 , 0 ) ) ;
            GLCHK ( glDrawArrays ( GL_TRIANGLES , 0 , 6 ) ) ;

            GLCHK ( glUseProgram ( lines_shader.program ) ) ;
            GLCHK ( glUniform3f ( lines_shader.uniform_locations[0] , 0.0f , 1.0f , 0.0f ) ) ;

            GLfloat points[] = {
                                heads.xb1 , - 1.0f ,
                                heads.xu1 , 1.0f ,
                                heads.xb2 , - 1.0f ,
                                heads.xu2 , 1.0f ,
                                - 1.0f , heads.y1 ,
                                1.0f , heads.y1 ,
                                - 1.0f , heads.y2 ,
                                1.0f , heads.y2
            } ;

            GLCHK ( glBindBuffer ( GL_ARRAY_BUFFER , 0 ) ) ;
            GLCHK ( glEnableVertexAttribArray ( lines_shader.attribute_locations[0] ) ) ;
            GLCHK ( glVertexAttribPointer ( lines_shader.attribute_locations[0] , 2 , GL_FLOAT , GL_FALSE , 0 , points ) ) ;
            GLCHK ( glDrawArrays ( GL_LINES , 0 , 8 ) ) ;
            break ;
        case 1:
            GLCHK ( glBindFramebuffer ( GL_FRAMEBUFFER , 0 ) ) ;
            glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) ;

            GLCHK ( glUseProgram ( gauss_hsi_shader.program ) ) ;
            GLCHK ( glUniform1i ( gauss_hsi_shader.uniform_locations[0] , 0 ) ) ; // Texture unit
            /* Dimensions of a single pixel in texture co-ordinates */
            GLCHK ( glUniform2f ( gauss_hsi_shader.uniform_locations[1] , 1.0 / ( float ) state->width , 1.0 / ( float ) state->height ) ) ;

            GLCHK ( glActiveTexture ( GL_TEXTURE0 ) ) ;
            GLCHK ( glBindTexture ( GL_TEXTURE_EXTERNAL_OES , state->texture ) ) ;
            GLCHK ( glBindBuffer ( GL_ARRAY_BUFFER , quad_vbo ) ) ;
            GLCHK ( glEnableVertexAttribArray ( gauss_hsi_shader.attribute_locations[0] ) ) ;
            GLCHK ( glVertexAttribPointer ( gauss_hsi_shader.attribute_locations[0] , 2 , GL_FLOAT , GL_FALSE , 0 , 0 ) ) ;
            GLCHK ( glDrawArrays ( GL_TRIANGLES , 0 , 6 ) ) ;
            break ;
        case 2:
            GLCHK ( glBindFramebuffer ( GL_FRAMEBUFFER , hsi_fbo_id ) ) ;
            glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) ;

            GLCHK ( glUseProgram ( gauss_hsi_shader.program ) ) ;
            GLCHK ( glUniform1i ( gauss_hsi_shader.uniform_locations[0] , 0 ) ) ; // Texture unit
            /* Dimensions of a single pixel in texture co-ordinates */
            GLCHK ( glUniform2f ( gauss_hsi_shader.uniform_locations[1] , 1.0 / ( float ) state->width , 1.0 / ( float ) state->height ) ) ;

            GLCHK ( glActiveTexture ( GL_TEXTURE0 ) ) ;
            GLCHK ( glBindTexture ( GL_TEXTURE_EXTERNAL_OES , state->texture ) ) ;
            GLCHK ( glBindBuffer ( GL_ARRAY_BUFFER , quad_vbo ) ) ;
            GLCHK ( glEnableVertexAttribArray ( gauss_hsi_shader.attribute_locations[0] ) ) ;
            GLCHK ( glVertexAttribPointer ( gauss_hsi_shader.attribute_locations[0] , 2 , GL_FLOAT , GL_FALSE , 0 , 0 ) ) ;
            GLCHK ( glDrawArrays ( GL_TRIANGLES , 0 , 6 ) ) ;
            glFlush ( ) ;
            glFinish ( ) ;

            GLCHK ( glBindFramebuffer ( GL_FRAMEBUFFER , 0 ) ) ;
            glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) ;

            GLCHK ( glUseProgram ( mask_shader.program ) ) ;
            GLCHK ( glUniform1i ( mask_shader.uniform_locations[0] , 0 ) ) ; // Texture unit
            /* Heads */
            GLCHK ( glUniform4f ( mask_shader.uniform_locations[1] , heads.xb1 , heads.xu1 , heads.xu2 , heads.xb2 ) ) ;

            GLCHK ( glActiveTexture ( GL_TEXTURE0 ) ) ;
            GLCHK ( glBindTexture ( GL_TEXTURE_2D , hsi_tex_id ) ) ;
            GLCHK ( glBindBuffer ( GL_ARRAY_BUFFER , quad_vbo ) ) ;
            GLCHK ( glEnableVertexAttribArray ( mask_shader.attribute_locations[0] ) ) ;
            GLCHK ( glVertexAttribPointer ( mask_shader.attribute_locations[0] , 2 , GL_FLOAT , GL_FALSE , 0 , 0 ) ) ;
            GLCHK ( glDrawArrays ( GL_TRIANGLES , 0 , 6 ) ) ;

            //            glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) ;
            //
            //            GLCHK ( glUseProgram ( hsi_shader.program ) ) ;
            //            GLCHK ( glUniform1i ( hsi_shader.uniform_locations[0] , 0 ) ) ; // Texture unit
            //            /* Dimensions of a single pixel in texture co-ordinates */
            //            GLCHK ( glUniform2f ( hsi_shader.uniform_locations[1] , 1.0 / ( float ) state->width , 1.0 / ( float ) state->height ) ) ;
            //            /* Heads */
            //            GLCHK ( glUniform4f ( hsi_shader.uniform_locations[2] , heads.xb1 , heads.xu1 , heads.xu2 , heads.xb2 ) ) ;
            //
            //            GLCHK ( glActiveTexture ( GL_TEXTURE0 ) ) ;
            //            GLCHK ( glBindTexture ( GL_TEXTURE_EXTERNAL_OES , state->texture ) ) ;
            //            GLCHK ( glBindBuffer ( GL_ARRAY_BUFFER , quad_vbo ) ) ;
            //            GLCHK ( glEnableVertexAttribArray ( hsi_shader.attribute_locations[0] ) ) ;
            //            GLCHK ( glVertexAttribPointer ( hsi_shader.attribute_locations[0] , 2 , GL_FLOAT , GL_FALSE , 0 , 0 ) ) ;
            //            GLCHK ( glDrawArrays ( GL_TRIANGLES , 0 , 6 ) ) ;
            //
            //            int x , y , width , height ;
            //
            //            x = state->width * 0.5f * ( heads.xb1 + 1.0f ) ;
            //            y = 0 ;
            //
            //            width = 0.5f * state->width * ( heads.xb2 - heads.xb1 ) ;
            //            height = 0.5f * state->width * ( heads.y1 + 1.0f ) ;
            //
            //            //            GLCHK ( glReadPixels ( x , 0 , width , height , GL_RGBA , GL_UNSIGNED_BYTE , pixels_from_fb ) ) ;
            //            GLCHK ( glReadPixels ( 0 , 0 , state->width , state->height , GL_RGBA , GL_UNSIGNED_BYTE , pixels_from_fb ) ) ;
            //
            //            InitHist ( &intensity_hist , intensity_umbral ) ;
            //            InitHist ( &hue_hist , hue_umbral ) ;
            //
            //            uint8_t* out = pixels_from_fb ;
            //            uint8_t* end = out + 4 * state->width * state->height ;
            //
            //
            //            for ( j = 0 ; j < height ; j ++ )
            //            {
            //                int base = 4 * ( j * state->width + x ) ;
            //                for ( i = 0 ; i < width ; i ++ )
            //                {
            //                    uint8_t intensity = out[base + i * 4 + 0] ;
            //                    uint8_t saturation = out[base + i * 4 + 1] ;
            //                    uint8_t hue = out[base + i * 4 + 2] ;
            //
            //                    intensity_hist.bins[intensity / intensity_hist.bin_width] ++ ;
            //                    intensity_hist.count ++ ;
            //
            //                    if ( intensity > intensity_hist.bin_width && saturation > intensity_hist.bin_width )
            //                    {
            //                        hue_hist.bins[hue / hue_hist.bin_width] ++ ;
            //                        hue_hist.count ++ ;
            //                    }
            //                }
            //            }
            //
            //            while ( out < end )
            //            {
            //                uint8_t intensity = out[0] ;
            //                uint8_t saturation = out[1] ;
            //                uint8_t hue = out[2] ;
            //
            //                int intensity_value = getFilteredValue ( &intensity_hist , intensity ) ;
            //                int hue_value = getFilteredValue ( &hue_hist , hue ) ;
            //
            //                if ( intensity < intensity_hist.bin_width )
            //                {
            //                    if ( intensity_value < intensity_threshold )
            //                    {
            //                        out[0] = out[1] = out[2] = 255 ;
            //                    }
            //                    else
            //                    {
            //                        out[0] = out[1] = out[2] = 0 ;
            //                    }
            //                }
            //                else if ( hue_value < hue_threshold || intensity_value < intensity_threshold )
            //                {
            //                    out[0] = out[1] = out[2] = 255 ;
            //                }
            //                else
            //                {
            //                    out[0] = out[1] = out[2] = 0 ;
            //                }
            //                out += 4 ;
            //            }
            //
            //            GLCHK ( glBindTexture ( GL_TEXTURE_2D , hist_tex_id ) ) ;
            //            GLCHK ( glTexImage2D ( GL_TEXTURE_2D , 0 , GL_RGBA , state->width , state->height , 0 , GL_RGBA , GL_UNSIGNED_BYTE , pixels_from_fb ) ) ;
            //            GLCHK ( glTexParameterf ( GL_TEXTURE_2D , GL_TEXTURE_MIN_FILTER , ( GLfloat ) GL_NEAREST ) ) ;
            //            GLCHK ( glTexParameterf ( GL_TEXTURE_2D , GL_TEXTURE_MAG_FILTER , ( GLfloat ) GL_NEAREST ) ) ;
            //
            //            GLCHK ( glUseProgram ( hist_shader.program ) ) ;
            //            GLCHK ( glUniform1i ( hist_shader.uniform_locations[0] , 0 ) ) ; // Texture unit
            //
            //            GLCHK ( glActiveTexture ( GL_TEXTURE0 ) ) ;
            //            GLCHK ( glBindTexture ( GL_TEXTURE_2D , hist_tex_id ) ) ;
            //            GLCHK ( glBindBuffer ( GL_ARRAY_BUFFER , quad_vbo ) ) ;
            //            GLCHK ( glEnableVertexAttribArray ( hist_shader.attribute_locations[0] ) ) ;
            //            GLCHK ( glVertexAttribPointer ( hist_shader.attribute_locations[0] , 2 , GL_FLOAT , GL_FALSE , 0 , 0 ) ) ;
            //            GLCHK ( glDrawArrays ( GL_TRIANGLES , 0 , 6 ) ) ;
            break ;
        case 3:
            GLCHK ( glBindFramebuffer ( GL_FRAMEBUFFER , hsi_fbo_id ) ) ;
            glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) ;

            GLCHK ( glUseProgram ( gauss_hsi_shader.program ) ) ;
            GLCHK ( glUniform1i ( gauss_hsi_shader.uniform_locations[0] , 0 ) ) ; // Texture unit
            /* Dimensions of a single pixel in texture co-ordinates */
            GLCHK ( glUniform2f ( gauss_hsi_shader.uniform_locations[1] , 1.0 / ( float ) state->width , 1.0 / ( float ) state->height ) ) ;

            GLCHK ( glActiveTexture ( GL_TEXTURE0 ) ) ;
            GLCHK ( glBindTexture ( GL_TEXTURE_EXTERNAL_OES , state->texture ) ) ;
            GLCHK ( glBindBuffer ( GL_ARRAY_BUFFER , quad_vbo ) ) ;
            GLCHK ( glEnableVertexAttribArray ( gauss_hsi_shader.attribute_locations[0] ) ) ;
            GLCHK ( glVertexAttribPointer ( gauss_hsi_shader.attribute_locations[0] , 2 , GL_FLOAT , GL_FALSE , 0 , 0 ) ) ;
            GLCHK ( glDrawArrays ( GL_TRIANGLES , 0 , 6 ) ) ;
            glFlush ( ) ;
            glFinish ( ) ;

            GLCHK ( glBindFramebuffer ( GL_FRAMEBUFFER , 0 ) ) ;
            glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) ;

            GLCHK ( glUseProgram ( mask_shader.program ) ) ;
            GLCHK ( glUniform1i ( mask_shader.uniform_locations[0] , 0 ) ) ; // Texture unit
            /* Heads */
            GLCHK ( glUniform4f ( mask_shader.uniform_locations[1] , heads.xb1 , heads.xu1 , heads.xu2 , heads.xb2 ) ) ;

            GLCHK ( glActiveTexture ( GL_TEXTURE0 ) ) ;
            GLCHK ( glBindTexture ( GL_TEXTURE_2D , hsi_tex_id ) ) ;
            GLCHK ( glBindBuffer ( GL_ARRAY_BUFFER , quad_vbo ) ) ;
            GLCHK ( glEnableVertexAttribArray ( mask_shader.attribute_locations[0] ) ) ;
            GLCHK ( glVertexAttribPointer ( mask_shader.attribute_locations[0] , 2 , GL_FLOAT , GL_FALSE , 0 , 0 ) ) ;
            GLCHK ( glDrawArrays ( GL_TRIANGLES , 0 , 6 ) ) ;
            break ;

            //        case 3:
            //            glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) ;
            //
            //            GLCHK ( glUseProgram ( reading_shader.program ) ) ;
            //            GLCHK ( glUniform1i ( reading_shader.uniform_locations[0] , 0 ) ) ; // Texture unit
            //
            //            GLCHK ( glActiveTexture ( GL_TEXTURE0 ) ) ;
            //            GLCHK ( glBindTexture ( GL_TEXTURE_2D , hist_tex_id ) ) ;
            //            GLCHK ( glBindBuffer ( GL_ARRAY_BUFFER , quad_vbo ) ) ;
            //            GLCHK ( glEnableVertexAttribArray ( reading_shader.attribute_locations[0] ) ) ;
            //            GLCHK ( glVertexAttribPointer ( reading_shader.attribute_locations[0] , 2 , GL_FLOAT , GL_FALSE , 0 , 0 ) ) ;
            //            GLCHK ( glDrawArrays ( GL_TRIANGLES , 0 , 6 ) ) ;
            //            break ;

    }

    return 0 ;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

int matching_open ( RASPITEX_STATE *state )
{
    state->ops.gl_init = matching_init ;
    state->ops.redraw = matching_redraw ;
    state->ops.update_texture = raspitexutil_update_texture ;
    return 0 ;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void InitHist ( HISTOGRAM * hist , int b_width )
{
    hist->bin_width = b_width ;
    hist->count = 0 ;

    int i ;
    for ( i = 0 ; i < 257 ; i ++ )
    {
        hist->bins[i] = 0 ;
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void WriteHistToTexrute ( HISTOGRAM * hist , uint8_t * text )
{
    //    uint8_t *out = pixels_hist ;
    //    uint8_t *end = out + 4 * 257 * 3 ;
    //
    //    int counter = 1000000 ;
    //    int aux ;
    //
    //    while ( out < end )
    //    {
    //        out[3] = counter / 1000000 + 10 ;
    //        aux = counter % 1000000 ;
    //        out[2] = aux / 10000 + 10 ;
    //        aux = aux % 10000 ;
    //        out[1] = aux / 100 + 10 ;
    //        out[0] = aux % 100 + 10 ;
    //
    //        out += 4 ;
    //
    //
    //        //        out[0] = counter ; //r
    //        //        out[1] = 0x01 ; //g
    //        //        out[2] = 0x00 ; //b
    //        //        out[3] = 0x00 ; //a
    //        //        out += 4 ;
    //        counter ++ ;
    //    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////

int getFilteredValue ( HISTOGRAM *hist , int value )
{
    int bin_index = value / hist->bin_width ;
    int average ;

    if ( bin_index )
    {

        average = ( intensity_hist.bins[bin_index] +
                    intensity_hist.bins[bin_index + 1] +
                    intensity_hist.bins[bin_index - 1] ) / 3 ;
    }
    else
    {
        average = ( intensity_hist.bins[bin_index] +
                    intensity_hist.bins[bin_index + 1] ) / 3 ;
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////
//TODO: test make histogram from shader
//TODO: do processing with opencv
//TODO: match through entire image
