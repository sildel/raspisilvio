///////////////////////////////////////////////////////////////////
#ifndef RASPISILVIO_H_
#define RASPISILVIO_H_
///////////////////////////////////////////////////////////////////
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
////////////////////////////////////////////////////////////////////////////////
#define SHADER_MAX_ATTRIBUTES 16
#define SHADER_MAX_UNIFORMS   16
///////////////////////////////////////////////////////////////////////////////////////////////////
#define HISTOGRAM_TEX_WIDTH 257
#define HISTOGRAM_TEX_HEIGHT 3
////////////////////////////////////////////////////////////////////////////////
#define FRAMBE_BUFFER_PREVIEW 0
////////////////////////////////////////////////////////////////////////////////
#define RASPISILVIO_RED 0
#define RASPISILVIO_GREEN 1
#define RASPISILVIO_BLUE 2
////////////////////////////////////////////////////////////////////////////////
/** Structure containing all state information for the current run
 */
typedef struct {
    int width; /// Requested width of image
    int height; /// requested height of image
    int useGL; /// Render preview using OpenGL

    RASPIPREVIEW_PARAMETERS preview_parameters; /// Preview setup parameters
    RASPICAM_CAMERA_PARAMETERS camera_parameters; /// Camera setup parameters

    MMAL_COMPONENT_T *camera_component; /// Pointer to the camera component
    MMAL_COMPONENT_T *null_sink_component; /// Pointer to the null sink component
    MMAL_CONNECTION_T *preview_connection; /// Pointer to the connection from camera to preview
    MMAL_CONNECTION_T *encoder_connection; /// Pointer to the connection from camera to encoder

    RASPITEX_STATE raspitex_state; /// GL renderer state and parameters

} RaspisilvioState;
////////////////////////////////////////////////////////////////////////////////

/**
 * Container for a simple shader program. The uniform and attribute locations
 * are automatically setup by raspitex_build_shader_program.
 */
typedef struct {
    const char *vs_file; /// vertex shader file path
    const char *fs_file; /// fragment shader file path

    const char *vertex_source; /// Pointer to vertex shader source
    const char *fragment_source; /// Pointer to fragment shader source

    /// Array of uniform names for raspitex_build_shader_program to process
    const char *uniform_names[SHADER_MAX_UNIFORMS];
    /// Array of attribute names for raspitex_build_shader_program to process
    const char *attribute_names[SHADER_MAX_ATTRIBUTES];

    GLint vs; /// Vertex shader handle
    GLint fs; /// Fragment shader handle
    GLint program; /// Shader program handle

    /// The locations for uniforms defined in uniform_names
    GLint uniform_locations[SHADER_MAX_UNIFORMS];

    /// The locations for attributes defined in attribute_names
    GLint attribute_locations[SHADER_MAX_ATTRIBUTES];

} RaspisilvioShaderProgram;
////////////////////////////////////////////////////////////////////////////////

typedef struct {
    RaspisilvioState state;

    int (*init)(struct RASPITEX_STATE *state);

    int (*draw)(struct RASPITEX_STATE *state);
} RaspisilvioApplication;
///////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
    int bins[257];
    int bin_width;
    int count;
} RaspisilvioHistogram;

///////////////////////////////////////////////////////////////////////////////////////////////////
void raspisilvioGetDefault(RaspisilvioApplication *app);

int raspisilvioInitApp(RaspisilvioApplication *app);

int raspisilvioStart(RaspisilvioApplication *app);

int raspisilvioStop(RaspisilvioApplication *app);

void raspisilvioLoadShader(RaspisilvioShaderProgram *shader);

int raspisilvioHelpInit(RASPITEX_STATE *state);

int raspisilvioHelpDraw(RASPITEX_STATE *state);

GLuint raspisilvioGetQuad();

RaspisilvioShaderProgram *raspisilvioGetResultShader();

RaspisilvioShaderProgram *raspisilvioGetPreviewShader();

void raspisilvioInitHist(RaspisilvioHistogram *hist, int b_width);

void raspisilvioWriteHistToTexture(RaspisilvioHistogram *hist, uint8_t *text);

int raspisilvioGetHistogramFilteredValue(RaspisilvioHistogram *hist, int value);

void raspisilvioCreateTexture(GLuint *name, int onlyName, int width, int height, uint8_t *data, int format);

void raspisilvioCreateTextureFB(GLuint *nameTexture, int width, int height, uint8_t *data, int format,
                                GLuint *nameFB);

void raspisilvioCreateTextureData(uint8_t **fb, int32_t width, int32_t height, int format);

void raspisilvioSetTextureData(GLuint name, int width, int height, uint8_t *data, int format);

void raspisilvioProcessingCamera(RaspisilvioShaderProgram *shader, RASPITEX_STATE *state, GLuint frameBuffer);

void raspisilvioProcessingCameraMask(RaspisilvioShaderProgram *shader, RASPITEX_STATE *state, GLuint maskName,
                                     GLuint frameBuffer);

void raspisilvioDrawCamera(RASPITEX_STATE *state);

void raspisilvioCameraMask(RASPITEX_STATE *state, GLuint maskName, GLuint frameBuffer);

void raspisilvioProcessingTexture(RaspisilvioShaderProgram *shader, RASPITEX_STATE *state, GLuint frameBuffer,
                                  GLuint textureName);

void raspisilvioProcessingTextureMask(RaspisilvioShaderProgram *shader, RASPITEX_STATE *state, GLuint maskName,
                                      GLuint frameBuffer, GLuint textureName);

void raspisilvioDrawTexture(RASPITEX_STATE *state, GLuint textureName);

void raspisilvioTextureMask(RASPITEX_STATE *state, GLuint maskName, GLuint frameBuffer, GLuint textureName);

void raspisilvioCreateVertexBufferHistogram(GLuint *name, const int width, const int height, GLfloat **data);

void raspisilvioCreateVertexBufferHistogramData(GLuint *name, const int points, GLfloat *data);

void raspisilvioBuildHistogram(GLuint histogramFB, GLuint textureName, GLuint bins, GLuint points_vbo, GLuint points,
                               GLuint channel);
///////////////////////////////////////////////////////////////////////////////////////////////////
#endif /* RASPISILVIO_H_ */
////////////////////////////////////////////////////////////////////////////////
