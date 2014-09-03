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
int render_id = 0 ;
//GLuint preview_tex_id ;
//GLuint preview_fb_id ;
//GLuint result_tex_id ;
//GLuint result_fb_id ;
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
                                                       .vertex_source = "" ,
                                                       .fragment_source = "" ,
                                                       .uniform_names =
    {"tex" } ,
                                                       .attribute_names =
    {"vertex" } ,
} ;
///////////////////////////////////////////////////////////////////////////////////////////////////
static RASPITEXUTIL_SHADER_PROGRAM_T hsi_shader = {
                                                   .vertex_source = "" ,
                                                   .fragment_source = "" ,
                                                   .uniform_names =
    {"tex" , "tex_unit" , "heads" } ,
                                                   .attribute_names =
    {"vertex" } ,
} ;
///////////////////////////////////////////////////////////////////////////////////////////////////
static RASPITEXUTIL_SHADER_PROGRAM_T lines_shader = {
                                                     .vertex_source = "" ,
                                                     .fragment_source = "" ,
                                                     .uniform_names =
    {"color" } ,
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

void LoadShadersFromFiles ( )
{
    char *common_vs = NULL ;
    char *simpleE_fs = NULL ;
    char *hsi_fs = NULL ;
    char *lines_fs = NULL ;
    char *lines_vs = NULL ;

    assert ( ! hsi_fs ) ;
    FILE* f = fopen ( "gl_scenes/g5hsiMFS.glsl" , "rb" ) ;
    assert ( f ) ;
    fseek ( f , 0 , SEEK_END ) ;
    int sz = ftell ( f ) ;
    fseek ( f , 0 , SEEK_SET ) ;
    hsi_fs = malloc ( sz + 1 ) ;
    fread ( hsi_fs , 1 , sz , f ) ;
    hsi_fs[sz] = 0 ; //null terminate it!
    fclose ( f ) ;

    assert ( ! common_vs ) ;
    f = fopen ( "gl_scenes/simpleVS.glsl" , "rb" ) ;
    assert ( f ) ;
    fseek ( f , 0 , SEEK_END ) ;
    sz = ftell ( f ) ;
    fseek ( f , 0 , SEEK_SET ) ;
    common_vs = malloc ( sz + 1 ) ;
    fread ( common_vs , 1 , sz , f ) ;
    common_vs[sz] = 0 ; //null terminate it!
    fclose ( f ) ;

    assert ( ! simpleE_fs ) ;
    f = fopen ( "gl_scenes/simpleEFS.glsl" , "rb" ) ;
    assert ( f ) ;
    fseek ( f , 0 , SEEK_END ) ;
    sz = ftell ( f ) ;
    fseek ( f , 0 , SEEK_SET ) ;
    simpleE_fs = malloc ( sz + 1 ) ;
    fread ( simpleE_fs , 1 , sz , f ) ;
    simpleE_fs[sz] = 0 ; //null terminate it!
    fclose ( f ) ;

    assert ( ! lines_fs ) ;
    f = fopen ( "gl_scenes/linesFS.glsl" , "rb" ) ;
    assert ( f ) ;
    fseek ( f , 0 , SEEK_END ) ;
    sz = ftell ( f ) ;
    fseek ( f , 0 , SEEK_SET ) ;
    lines_fs = malloc ( sz + 1 ) ;
    fread ( lines_fs , 1 , sz , f ) ;
    lines_fs[sz] = 0 ; //null terminate it!
    fclose ( f ) ;

    assert ( ! lines_vs ) ;
    f = fopen ( "gl_scenes/linesVS.glsl" , "rb" ) ;
    assert ( f ) ;
    fseek ( f , 0 , SEEK_END ) ;
    sz = ftell ( f ) ;
    fseek ( f , 0 , SEEK_SET ) ;
    lines_vs = malloc ( sz + 1 ) ;
    fread ( lines_vs , 1 , sz , f ) ;
    lines_vs[sz] = 0 ; //null terminate it!
    fclose ( f ) ;

    preview_shader.vertex_source = common_vs ;
    preview_shader.fragment_source = simpleE_fs ;
    raspitexutil_build_shader_program ( &preview_shader ) ;

    hsi_shader.vertex_source = common_vs ;
    hsi_shader.fragment_source = hsi_fs ;
    raspitexutil_build_shader_program ( &hsi_shader ) ;

    lines_shader.vertex_source = lines_vs ;
    lines_shader.fragment_source = lines_fs ;
    raspitexutil_build_shader_program ( &lines_shader ) ;

    free ( common_vs ) ;
    free ( simpleE_fs ) ;
    free ( hsi_fs ) ;
    free ( lines_fs ) ;
    free ( lines_vs ) ;
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

    LoadShadersFromFiles ( ) ;

    //    GLCHK ( glEnable ( GL_TEXTURE_2D ) ) ;
    //    GLCHK ( glGenTextures ( 1 , &my_tex_id ) ) ;
    //    GLCHK ( glBindTexture ( GL_TEXTURE_2D , my_tex_id ) ) ;
    //    GLCHK ( glTexImage2D ( GL_TEXTURE_2D , 0 , GL_RGBA , raspitex_state->width , raspitex_state->height , 0 , GL_RGBA , GL_UNSIGNED_BYTE , NULL ) ) ;
    //    GLCHK ( glTexParameterf ( GL_TEXTURE_2D , GL_TEXTURE_MIN_FILTER , ( GLfloat ) GL_NEAREST ) ) ;
    //    GLCHK ( glTexParameterf ( GL_TEXTURE_2D , GL_TEXTURE_MAG_FILTER , ( GLfloat ) GL_NEAREST ) ) ;
    //
    //    GLCHK ( glGenFramebuffers ( 1 , &my_frame_buffer_id ) ) ;
    //    GLCHK ( glBindFramebuffer ( GL_FRAMEBUFFER , my_frame_buffer_id ) ) ;
    //    GLCHK ( glFramebufferTexture2D ( GL_FRAMEBUFFER , GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D , my_tex_id , 0 ) ) ;
    //
    //    GLCHK ( glBindFramebuffer ( GL_FRAMEBUFFER , 0 ) ) ;

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
    switch ( render_id )
    {
        case 0:
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
            glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) ;

            GLCHK ( glUseProgram ( hsi_shader.program ) ) ;
            GLCHK ( glUniform1i ( hsi_shader.uniform_locations[0] , 0 ) ) ; // Texture unit
            /* Dimensions of a single pixel in texture co-ordinates */
            GLCHK ( glUniform2f ( hsi_shader.uniform_locations[1] , 1.0 / ( float ) state->width , 1.0 / ( float ) state->height ) ) ;
            /* Heads */
            GLCHK ( glUniform4f ( hsi_shader.uniform_locations[2] , heads.xb1 , heads.xu1 , heads.xu2 , heads.xb2 ) ) ;

            GLCHK ( glActiveTexture ( GL_TEXTURE0 ) ) ;
            GLCHK ( glBindTexture ( GL_TEXTURE_EXTERNAL_OES , state->texture ) ) ;
            GLCHK ( glBindBuffer ( GL_ARRAY_BUFFER , quad_vbo ) ) ;
            GLCHK ( glEnableVertexAttribArray ( hsi_shader.attribute_locations[0] ) ) ;
            GLCHK ( glVertexAttribPointer ( hsi_shader.attribute_locations[0] , 2 , GL_FLOAT , GL_FALSE , 0 , 0 ) ) ;
            GLCHK ( glDrawArrays ( GL_TRIANGLES , 0 , 6 ) ) ;
            break ;
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
//TODO: add umbral for i and v
//TODO: test make histogram from shader
