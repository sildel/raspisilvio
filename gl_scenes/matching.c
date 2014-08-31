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
char *common_vs = NULL ;
char *simple_fs = NULL ;
char *blur_fs = NULL ;
char *gauss_fs = NULL ;
///////////////////////////////////////////////////////////////////////////////////////////////////
static RASPITEXUTIL_SHADER_PROGRAM_T matching_shader_preview = {
                                                                .vertex_source = "" ,
                                                                .fragment_source = "" ,
                                                                .uniform_names =
    {"tex" } ,
                                                                .attribute_names =
    {"vertex" , "top_left" } ,
} ;
///////////////////////////////////////////////////////////////////////////////////////////////////
static RASPITEXUTIL_SHADER_PROGRAM_T matching_shader = {
                                                        .vertex_source = "" ,
                                                        .fragment_source = "" ,
                                                        .uniform_names =
    {"tex" , "tex_unit" } ,
                                                        .attribute_names =
    {"vertex" , "top_left" } ,
} ;
///////////////////////////////////////////////////////////////////////////////////////////////////
static RASPITEXUTIL_SHADER_PROGRAM_T matching_blur_shader = {
                                                             .vertex_source = "" ,
                                                             .fragment_source = "" ,
                                                             .uniform_names =
    {"tex" , "tex_unit" } ,
                                                             .attribute_names =
    {"vertex" , "top_left" } ,
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
    assert ( ! common_vs ) ;
    FILE* f = fopen ( "gl_scenes/simple4VS.glsl" , "rb" ) ;
    assert ( f ) ;
    fseek ( f , 0 , SEEK_END ) ;
    int sz = ftell ( f ) ;
    fseek ( f , 0 , SEEK_SET ) ;
    common_vs = malloc ( sz + 1 ) ;
    fread ( common_vs , 1 , sz , f ) ;
    common_vs[sz] = 0 ; //null terminate it!
    fclose ( f ) ;

    assert ( ! simple_fs ) ;
    f = fopen ( "gl_scenes/simpleFS.glsl" , "rb" ) ;
    assert ( f ) ;
    fseek ( f , 0 , SEEK_END ) ;
    sz = ftell ( f ) ;
    fseek ( f , 0 , SEEK_SET ) ;
    simple_fs = malloc ( sz + 1 ) ;
    fread ( simple_fs , 1 , sz , f ) ;
    simple_fs[sz] = 0 ; //null terminate it!
    fclose ( f ) ;

    assert ( ! gauss_fs ) ;
    f = fopen ( "gl_scenes/gaussian5FS.glsl" , "rb" ) ;
    assert ( f ) ;
    fseek ( f , 0 , SEEK_END ) ;
    sz = ftell ( f ) ;
    fseek ( f , 0 , SEEK_SET ) ;
    gauss_fs = malloc ( sz + 1 ) ;
    fread ( gauss_fs , 1 , sz , f ) ;
    gauss_fs[sz] = 0 ; //null terminate it!
    fclose ( f ) ;

    assert ( ! blur_fs ) ;
    f = fopen ( "gl_scenes/blurFS.glsl" , "rb" ) ;
    assert ( f ) ;
    fseek ( f , 0 , SEEK_END ) ;
    sz = ftell ( f ) ;
    fseek ( f , 0 , SEEK_SET ) ;
    blur_fs = malloc ( sz + 1 ) ;
    fread ( blur_fs , 1 , sz , f ) ;
    blur_fs[sz] = 0 ; //null terminate it!
    fclose ( f ) ;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Initialisation of shader uniforms.
 *
 * @param width Width of the EGL image.
 * @param width Height of the EGL image.
 */
static int shader_set_uniforms ( RASPITEXUTIL_SHADER_PROGRAM_T *shader ,
                                 int width , int height )
{
    GLCHK ( glUseProgram ( shader->program ) ) ;
    GLCHK ( glUniform1i ( shader->uniform_locations[0] , 0 ) ) ; // Texture unit

    /* Dimensions of a single pixel in texture co-ordinates */
    //    GLCHK ( glUniform2f ( shader->uniform_locations[1] ,
    //                          1.0 / ( float ) width , 1.0 / ( float ) height ) ) ;

    /* Enable attrib 0 as vertex array */
    GLCHK ( glEnableVertexAttribArray ( shader->attribute_locations[0] ) ) ;
    return 0 ;
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
    
    LoadShadersFromFiles ( );

    matching_shader_preview.vertex_source = common_vs ;
    matching_shader_preview.fragment_source = simple_fs ;
    rc = raspitexutil_build_shader_program ( &matching_shader_preview ) ;
    if ( rc != 0 )
        goto end ;

    matching_shader.vertex_source = common_vs ;
    matching_shader.fragment_source = gauss_fs ;
    rc = raspitexutil_build_shader_program ( &matching_shader ) ;
    if ( rc != 0 )
        goto end ;

    matching_blur_shader.vertex_source = common_vs ;
    matching_blur_shader.fragment_source = blur_fs ;
    rc = raspitexutil_build_shader_program ( &matching_blur_shader ) ;
    if ( rc != 0 )
        goto end ;

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
    glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) ;
    GLCHK ( glUseProgram ( matching_shader_preview.program ) ) ;
    GLCHK ( glUniform1i ( matching_shader_preview.uniform_locations[0] , 0 ) ) ; // Texture unit    

    GLCHK ( glActiveTexture ( GL_TEXTURE0 ) ) ;
    GLCHK ( glBindTexture ( GL_TEXTURE_EXTERNAL_OES , state->texture ) ) ;
    GLCHK ( glBindBuffer ( GL_ARRAY_BUFFER , quad_vbo ) ) ;
    GLCHK ( glEnableVertexAttribArray ( matching_shader_preview.attribute_locations[0] ) ) ;
    GLCHK ( glVertexAttribPointer ( matching_shader_preview.attribute_locations[0] , 2 , GL_FLOAT , GL_FALSE , 0 , 0 ) ) ;
    GLCHK ( glVertexAttrib2f ( matching_shader_preview.attribute_locations[1] , - 1.0f , 1.0f ) ) ;
    GLCHK ( glDrawArrays ( GL_TRIANGLES , 0 , 6 ) ) ;

    GLCHK ( glUseProgram ( matching_shader.program ) ) ;
    GLCHK ( glUniform1i ( matching_shader.uniform_locations[0] , 0 ) ) ; // Texture unit
    /* Dimensions of a single pixel in texture co-ordinates */
    GLCHK ( glUniform2f ( matching_shader.uniform_locations[1] , 2.0 / ( float ) state->width , 2.0 / ( float ) state->height ) ) ;

    GLCHK ( glActiveTexture ( GL_TEXTURE0 ) ) ;
    GLCHK ( glBindTexture ( GL_TEXTURE_EXTERNAL_OES , state->texture ) ) ;
    GLCHK ( glBindBuffer ( GL_ARRAY_BUFFER , quad_vbo ) ) ;
    GLCHK ( glEnableVertexAttribArray ( matching_shader.attribute_locations[0] ) ) ;
    GLCHK ( glVertexAttribPointer ( matching_shader.attribute_locations[0] , 2 , GL_FLOAT , GL_FALSE , 0 , 0 ) ) ;
    GLCHK ( glVertexAttrib2f ( matching_shader.attribute_locations[1] , 0.0f , 1.0f ) ) ;
    GLCHK ( glDrawArrays ( GL_TRIANGLES , 0 , 6 ) ) ;

    GLCHK ( glUseProgram ( matching_blur_shader.program ) ) ;
    GLCHK ( glUniform1i ( matching_blur_shader.uniform_locations[0] , 0 ) ) ; // Texture unit
    /* Dimensions of a single pixel in texture co-ordinates */
    GLCHK ( glUniform2f ( matching_blur_shader.uniform_locations[1] , 2.0 / ( float ) state->width , 2.0 / ( float ) state->height ) ) ;

    GLCHK ( glActiveTexture ( GL_TEXTURE0 ) ) ;
    GLCHK ( glBindTexture ( GL_TEXTURE_EXTERNAL_OES , state->texture ) ) ;
    GLCHK ( glBindBuffer ( GL_ARRAY_BUFFER , quad_vbo ) ) ;
    GLCHK ( glEnableVertexAttribArray ( matching_blur_shader.attribute_locations[0] ) ) ;
    GLCHK ( glVertexAttribPointer ( matching_blur_shader.attribute_locations[0] , 2 , GL_FLOAT , GL_FALSE , 0 , 0 ) ) ;
    GLCHK ( glVertexAttrib2f ( matching_blur_shader.attribute_locations[1] , 0.0f , 0.0f ) ) ;
    GLCHK ( glDrawArrays ( GL_TRIANGLES , 0 , 6 ) ) ;

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
