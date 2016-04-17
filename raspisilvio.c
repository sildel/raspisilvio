///////////////////////////////////////////////////////////////////////////////////////////////////
// Created by
// Jorge Silvio Delgado Morales
// based on raspicam applications of https://github.com/raspberrypi/userland
// silviodelgado70@gmail.com
///////////////////////////////////////////////////////////////////////////////////////////////////
#include "RaspiTexUtil.h"
#include "raspisilvio.h"
#include <GLES2/gl2.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
const EGLint raspisilvio_matching_egl_config_attribs[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
};
///////////////////////////////////////////////////////////////////////////////////////////////////
static RaspisilvioShaderProgram preview_shader = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/simpleEFS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram result_shader = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/simpleFS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram mask_camera_shader = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/mask_operationEFS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "mask"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram mask_tex_shader = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/mask_operationFS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "mask"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
static GLuint quad_vbo;
///////////////////////////////////////////////////////////////////////////////////////////////////
static GLfloat quad_varray[] = {
        -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, -1.0f,
        -1.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f,
};
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Assign a default set of parameters to the state passed in
 *
 * @param state Pointer to state structure to assign defaults to
 */
static void default_status(RaspisilvioState *state) {
    if (!state) {
        vcos_assert(0);
        return;
    }

    state->width = 640;
    state->height = 480;
    state->camera_component = NULL;

    // Setup preview window defaults
    raspipreview_set_defaults(&state->preview_parameters);

    // Set up the camera_parameters to default
    raspicamcontrol_set_defaults(&state->camera_parameters);
    state->camera_parameters.hflip = 1;

    // Set initial GL preview state
    raspitex_set_defaults(&state->raspitex_state);
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
static void camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    if (buffer->cmd == MMAL_EVENT_PARAMETER_CHANGED) {
    } else {
        vcos_log_error("Received unexpected camera control callback event, 0x%08x", buffer->cmd);
    }

    mmal_buffer_header_release(buffer);
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
static MMAL_STATUS_T create_camera_component(RaspisilvioState *state) {
    MMAL_COMPONENT_T *camera = 0;
    MMAL_ES_FORMAT_T *format;
    MMAL_PORT_T *preview_port = NULL, *video_port = NULL, *still_port = NULL;
    MMAL_STATUS_T status;

    /* Create the component */
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("Failed to create camera component");
        goto error;
    }

    if (!camera->output_num) {
        status = MMAL_ENOSYS;
        vcos_log_error("Camera doesn't have output ports");
        goto error;
    }

    preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT];
    video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
    still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];

    // Enable the camera, and tell it its control callback function
    status = mmal_port_enable(camera->control, camera_control_callback);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("Unable to enable control port : error %d", status);
        goto error;
    }

    //  set up the camera configuration
    {
        MMAL_PARAMETER_CAMERA_CONFIG_T cam_config = {
                {MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config)},
                .max_stills_w = state->width,
                .max_stills_h = state->height,
                .stills_yuv422 = 0,
                .one_shot_stills = 1,
                .max_preview_video_w = state->preview_parameters.previewWindow.width,
                .max_preview_video_h = state->preview_parameters.previewWindow.height,
                .num_preview_video_frames = 3,
                .stills_capture_circular_buffer_height = 0,
                .fast_preview_resume = 0,
                .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
        };

        mmal_port_parameter_set(camera->control, &cam_config.hdr);
    }

    raspicamcontrol_set_all_parameters(camera, &state->camera_parameters);

    // Now set up the port formats

    format = preview_port->format;
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->encoding_variant = MMAL_ENCODING_I420;

    // Use a full FOV 4:3 mode
    format->es->video.width = VCOS_ALIGN_UP(state->preview_parameters.previewWindow.width, 32);
    format->es->video.height = VCOS_ALIGN_UP(state->preview_parameters.previewWindow.height, 16);
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = state->preview_parameters.previewWindow.width;
    format->es->video.crop.height = state->preview_parameters.previewWindow.height;
    format->es->video.frame_rate.num = PREVIEW_FRAME_RATE_NUM;
    format->es->video.frame_rate.den = PREVIEW_FRAME_RATE_DEN;

    status = mmal_port_format_commit(preview_port);
    if (status != MMAL_SUCCESS) {
        vcos_log_error("camera viewfinder format couldn't be set");
        goto error;
    }

    // Set the same format on the video  port (which we dont use here)
    mmal_format_full_copy(video_port->format, format);
    status = mmal_port_format_commit(video_port);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("camera video format couldn't be set");
        goto error;
    }

    // Ensure there are enough buffers to avoid dropping frames
    if (video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
        video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

    format = still_port->format;

    // Set our stills format on the stills (for encoder) port
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->es->video.width = VCOS_ALIGN_UP(state->width, 32);
    format->es->video.height = VCOS_ALIGN_UP(state->height, 16);
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = state->width;
    format->es->video.crop.height = state->height;
    format->es->video.frame_rate.num = STILLS_FRAME_RATE_NUM;
    format->es->video.frame_rate.den = STILLS_FRAME_RATE_DEN;


    status = mmal_port_format_commit(still_port);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("camera still format couldn't be set");
        goto error;
    }

    /* Ensure there are enough buffers to avoid dropping frames */
    if (still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
        still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

    /* Enable component */
    status = mmal_component_enable(camera);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("camera component couldn't be enabled");
        goto error;
    }

    status = raspitex_configure_preview_port(&state->raspitex_state, preview_port);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "Failed to configure preview port for GL rendering");
        goto error;
    }

    state->camera_component = camera;

    return status;

    error:

    if (camera)
        mmal_component_destroy(camera);

    return status;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Destroy the camera component
 *
 * @param state Pointer to state control struct
 *
 */
static void destroy_camera_component(RaspisilvioState *state) {
    if (state->camera_component) {
        mmal_component_destroy(state->camera_component);
        state->camera_component = NULL;
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Handler for sigint signals
 *
 * @param signal_number ID of incoming signal.
 *
 */
static void signal_handler(int signal_number) {
    if (signal_number == SIGUSR1) {
        // Handle but ignore - prevents us dropping out if started in none-signal mode
        // and someone sends us the USR1 signal anyway
    } else {
        // Going to abort on all other signals
        vcos_log_error("Aborting program\n");
        exit(130);
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Checks if specified port is valid and enabled, then disables it
 *
 * @param port  Pointer the port
 *
 */
static void check_disable_port(MMAL_PORT_T *port) {
    if (port && port->is_enabled)
        mmal_port_disable(port);
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Takes a description of shader program, compiles it and gets the locations
 * of uniforms and attributes.
 *
 * @param p The shader program state.
 * @return Zero if successful.
 */
static int raspisilvio_build_shader_program(RaspisilvioShaderProgram *p) {
    GLint status;
    int i = 0;
    char log[1024];
    int logLen = 0;
    vcos_assert(p);
    vcos_assert(p->vertex_source);
    vcos_assert(p->fragment_source);

    if (!(p && p->vertex_source && p->fragment_source))
        goto fail;

    p->vs = p->fs = 0;

    p->vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(p->vs, 1, &p->vertex_source, NULL);
    glCompileShader(p->vs);
    glGetShaderiv(p->vs, GL_COMPILE_STATUS, &status);
    if (!status) {
        glGetShaderInfoLog(p->vs, sizeof(log), &logLen, log);
        vcos_log_error("Program info log %s", log);
        goto fail;
    }

    p->fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(p->fs, 1, &p->fragment_source, NULL);
    glCompileShader(p->fs);

    glGetShaderiv(p->fs, GL_COMPILE_STATUS, &status);
    if (!status) {
        glGetShaderInfoLog(p->fs, sizeof(log), &logLen, log);
        vcos_log_error("Program info log %s", log);
        goto fail;
    }

    p->program = glCreateProgram();
    glAttachShader(p->program, p->vs);
    glAttachShader(p->program, p->fs);
    glLinkProgram(p->program);
    glGetProgramiv(p->program, GL_LINK_STATUS, &status);
    if (!status) {
        vcos_log_error("Failed to link shader program");
        glGetProgramInfoLog(p->program, sizeof(log), &logLen, log);
        vcos_log_error("Program info log %s", log);
        goto fail;
    }

    for (i = 0; i < SHADER_MAX_ATTRIBUTES; ++i) {
        if (!p->attribute_names[i])
            break;
        p->attribute_locations[i] = glGetAttribLocation(p->program, p->attribute_names[i]);
        if (p->attribute_locations[i] == -1) {
            vcos_log_error("Failed to get location for attribute %s",
                           p->attribute_names[i]);
            goto fail;
        } else {
            vcos_log_trace("Attribute for %s is %d",
                           p->attribute_names[i], p->attribute_locations[i]);
        }
    }

    for (i = 0; i < SHADER_MAX_UNIFORMS; ++i) {
        if (!p->uniform_names[i])
            break;
        p->uniform_locations[i] = glGetUniformLocation(p->program, p->uniform_names[i]);
        if (p->uniform_locations[i] == -1) {
            vcos_log_error("Failed to get location for uniform %s",
                           p->uniform_names[i]);
            goto fail;
        } else {
            vcos_log_trace("Uniform for %s is %d",
                           p->uniform_names[i], p->uniform_locations[i]);
        }
    }

    return 0;

    fail:
    vcos_log_error("%s: Failed to build shader program", VCOS_FUNCTION);
    if (p) {
        glDeleteProgram(p->program);
        glDeleteShader(p->fs);
        glDeleteShader(p->vs);
    }
    return -1;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void raspisilvioLoadShader(RaspisilvioShaderProgram *shader) {
    char *vs_source = NULL;
    char *fs_source = NULL;
    FILE *f;
    int sz;

    assert(!vs_source);
    f = fopen(shader->vs_file, "rb");
    assert(f);
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    vs_source = malloc(sz + 1);
    fread(vs_source, 1, sz, f);
    vs_source[sz] = 0; //null terminate it!
    fclose(f);

    assert(!fs_source);
    f = fopen(shader->fs_file, "rb");
    assert(f);
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    fs_source = malloc(sz + 1);
    fread(fs_source, 1, sz, f);
    fs_source[sz] = 0; //null terminate it!
    fclose(f);

    shader->vertex_source = vs_source;
    shader->fragment_source = fs_source;
    raspisilvio_build_shader_program(shader);

    free(vs_source);
    free(fs_source);
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Default is a no-op
 * @param raspitex_state A pointer to the GL preview state.
 * @return Zero.
 */
static int raspisilvio_update_model(RASPITEX_STATE *raspitex_state) {
    (void) raspitex_state;
    return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Creates the OpenGL ES 2.X context and builds the shaders.
 * @param raspitex_state A pointer to the GL preview state.
 * @return Zero if successful.
 */
static int raspisilvio_init(RASPITEX_STATE *raspitex_state) {
    int rc = 0;

    vcos_log_trace("%s", VCOS_FUNCTION);
    raspitex_state->egl_config_attribs = raspisilvio_matching_egl_config_attribs;
    rc = raspitexutil_gl_init_2_0(raspitex_state);
    if (rc != 0)
        goto end;

    raspisilvioLoadShader(&preview_shader);

    glGenBuffers(1, &quad_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_varray), quad_varray, GL_STATIC_DRAW);
    glClearColor(0, 0, 0, 0);

    end:
    return rc;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/* Redraws the scene with the latest buffer.
 *
 * @param raspitex_state A pointer to the GL preview state.
 * @return Zero if successful.
 */
static int raspisilvio_draw(RASPITEX_STATE *state) {

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, state->width, state->height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(preview_shader.program);
    glUniform1i(preview_shader.uniform_locations[0], 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, state->texture);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
    glEnableVertexAttribArray(preview_shader.attribute_locations[0]);
    glVertexAttribPointer(preview_shader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void raspisilvioGetDefault(RaspisilvioApplication *app) {
    default_status(&app->state);
    app->init = NULL;
    app->draw = NULL;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

int raspisilvioInitApp(RaspisilvioApplication *app) {
    int exit_code = EX_OK;

    MMAL_STATUS_T status = MMAL_SUCCESS;

    bcm_host_init();

    signal(SIGINT, signal_handler);
    signal(SIGUSR1, SIG_IGN);

    // Register our application with the logging system
    vcos_log_register("RaspiSilvio", VCOS_LOG_CATEGORY);

    raspitex_init(&app->state.raspitex_state);
    if (app->init) {
        app->state.raspitex_state.ops.gl_init = app->init;
    } else {
        app->state.raspitex_state.ops.gl_init = raspisilvio_init;
    }
    if (app->draw) {
        app->state.raspitex_state.ops.redraw = app->draw;
    } else {
        app->state.raspitex_state.ops.redraw = raspisilvio_draw;
    }
    app->state.raspitex_state.ops.update_model = raspisilvio_update_model;

    if ((status = create_camera_component(&app->state)) != MMAL_SUCCESS) {
        vcos_log_error("%s: Failed to create camera component", __func__);
        exit_code = EX_SOFTWARE;
        raspicamcontrol_check_configuration(128);
    }

    return exit_code;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

int raspisilvioStart(RaspisilvioApplication *app) {

    /* If GL preview is requested then start the GL threads */
    if ((raspitex_start(&app->state.raspitex_state) != 0)) {
        raspitex_stop(&app->state.raspitex_state);
        raspitex_destroy(&app->state.raspitex_state);

        /* Disable components */
        if (app->state.camera_component)
            mmal_component_disable(app->state.camera_component);

        destroy_camera_component(&app->state);

        return EX_SOFTWARE;
    }
    return EX_OK;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

int raspisilvioStop(RaspisilvioApplication *app) {
    raspitex_stop(&app->state.raspitex_state);
    raspitex_destroy(&app->state.raspitex_state);

    /* Disable components */
    if (app->state.camera_component)
        mmal_component_disable(app->state.camera_component);

    destroy_camera_component(&app->state);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

int raspisilvioHelpInit(RASPITEX_STATE *state) {
    int rc = 0;

    vcos_log_trace("%s", VCOS_FUNCTION);
    state->egl_config_attribs = raspisilvio_matching_egl_config_attribs;
    rc = raspitexutil_gl_init_2_0(state);

    raspisilvioLoadShader(&preview_shader);
    raspisilvioLoadShader(&result_shader);
    raspisilvioLoadShader(&mask_camera_shader);
    raspisilvioLoadShader(&mask_tex_shader);

    glGenBuffers(1, &quad_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_varray), quad_varray, GL_STATIC_DRAW);
    glClearColor(0, 0, 0, 0);

    return rc;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

int raspisilvioHelpDraw(RASPITEX_STATE *state) {

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, state->width, state->height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(raspisilvioGetPreviewShader()->program);
    glUniform1i(raspisilvioGetPreviewShader()->uniform_locations[0], 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, state->texture);
    glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad());
    glEnableVertexAttribArray(raspisilvioGetPreviewShader()->attribute_locations[0]);
    glVertexAttribPointer(raspisilvioGetPreviewShader()->attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

GLuint raspisilvioGetQuad() {
    return quad_vbo;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

RaspisilvioShaderProgram *raspisilvioGetResultShader() {
    return &result_shader;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

RaspisilvioShaderProgram *raspisilvioGetPreviewShader() {
    return &preview_shader;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void raspisilvioInitHist(RaspisilvioHistogram *hist, int b_width) {
    hist->bin_width = b_width;
    hist->count = 0;

    int i;
    for (i = 0; i < 257; i++) {
        hist->bins[i] = 0;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void raspisilvioWriteHistToTexture(RaspisilvioHistogram *hist, uint8_t *text) {
    int i, aux;
    int n_bins = 255 / hist->bin_width + 1;

    for (i = 0; i < n_bins; i++) {
        int sum = hist->bins[i] + hist->bins[i + 1];
        if (i) {
            sum += hist->bins[i - 1];
        }
        sum /= 3;

        text[4 * i + 3] = sum / 1000000 + 10;
        aux = sum % 1000000;
        text[4 * i + 2] = aux / 10000 + 10;
        aux = aux % 10000;
        text[4 * i + 1] = aux / 100 + 10;
        text[4 * i] = aux % 100 + 10;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

int raspisilvioGetHistogramFilteredValue(RaspisilvioHistogram *hist, int value) {
    int bin_index = value / hist->bin_width;
    int average;

    if (bin_index) {

        average = (hist->bins[bin_index] +
                   hist->bins[bin_index + 1] +
                   hist->bins[bin_index - 1]) / 3;
    } else {
        average = (hist->bins[bin_index] +
                   hist->bins[bin_index + 1]) / 3;
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/* Create a texture.
 *
 * @param name The name of the texture.
 * @param onlyName Generate only the name of the texture.
 * @param width Width of the texture.
 * @param height Height of the texture.
 * @param data The data containing pixel data
 * @param format The data format of the pixels
 * @return void.
 */
void raspisilvioCreateTexture(GLuint *name, int onlyName, int width, int height, uint8_t *data, int format) {
    GLCHK(glEnable(GL_TEXTURE_2D));
    GLCHK(glGenTextures(1, name));

    if (onlyName) {
        return;
    }

    GLCHK(glBindTexture(GL_TEXTURE_2D, *name));
    GLCHK(glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data));
    GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat) GL_NEAREST));
    GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat) GL_NEAREST));
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/* Create a texture to be used as a frame buffer.
 *
 * @param name The name of the texture.
 * @param onlyName Generate only the name of the texture.
 * @param width Width of the texture.
 * @param height Height of the texture.
 * @param data The data containing pixel data
 * @param format The data format of the pixels
 * @return void.
 */
void raspisilvioCreateTextureFB(GLuint *nameTexture, int width, int height, uint8_t *data, int format,
                                GLuint *nameFB) {
    GLCHK(glEnable(GL_TEXTURE_2D));
    GLCHK(glGenTextures(1, nameTexture));

    GLCHK(glBindTexture(GL_TEXTURE_2D, *nameTexture));
    GLCHK(glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data));
    GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat) GL_NEAREST));
    GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat) GL_NEAREST));

    GLCHK(glGenFramebuffers(1, nameFB));
    GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, *nameFB));
    GLCHK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *nameTexture, 0));
    GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/* Allocate memory for texture data. Note: Only RGBA format allowed.
 *
 * @param data The pointer to save data allocation.
 * @param onlyName Generate only the name of the texture.
 * @param width Width of the texture.
 * @param height Height of the texture.
 * @param format The data format of the pixels
 * @return void.
 */
void raspisilvioCreateTextureData(uint8_t **data, int32_t width, int32_t height, int format) {
    if (format == GL_RGBA) {
        *data = malloc(width * height * 4);
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/* Do processing to the camera feed using the specified shader program and put it on the specified frame buffer.
 * The shader is expected to have these parameters:
 * 1 - First attribute is the vertex and should contains only x and y coordinates
 * 2 - First uniform is the texture unit and the type should be samplerExternalOES
 * 3 - Second uniform is the dimension of the pixel in texture coordinates
 *
 * @param shader The shader to be used.
 * @param state The state of the application.
 * @param frameBuffer The name of the frame buffer.
 * @return void.
 */
void raspisilvioProcessingCamera(RaspisilvioShaderProgram *shader, RASPITEX_STATE *state, GLuint frameBuffer) {
    GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer));
    glViewport(0, 0, state->width, state->height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLCHK(glUseProgram(shader->program));
    GLCHK(glUniform1i(shader->uniform_locations[0], 0)); // Texture unit
    /* Dimensions of a single pixel in texture co-ordinates */
    GLCHK(glUniform2f(shader->uniform_locations[1], 1.0 / (float) state->width,
                      1.0 / (float) state->height));
    GLCHK(glActiveTexture(GL_TEXTURE0));
    GLCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, state->texture));
    GLCHK(glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad()));
    GLCHK(glEnableVertexAttribArray(shader->attribute_locations[0]));
    GLCHK(glVertexAttribPointer(shader->attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
    GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));
    glFlush();
    glFinish();
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/* Do processing to the camera feed using the specified shader program and put it on the specified frame buffer.
 * It uses a second texture that can act as a mask (The shader should implement this).
 * The shader is expected to have these parameters:
 * 1 - First attribute is the vertex and should contains only x and y coordinates
 * 2 - First uniform is the texture unit and the type should be samplerExternalOES
 * 3 - Second uniform is the dimension of the pixel in texture coordinates
 * 4 - Third uniform is the texture unit
 *
 * @param shader The shader to be used.
 * @param state The state of the application.
 * @param maskName The mask to be applied.
 * @param frameBuffer The name of the frame buffer. *
 * @return void.
 */
void raspisilvioProcessingCameraMask(RaspisilvioShaderProgram *shader, RASPITEX_STATE *state, GLuint maskName,
                                     GLuint frameBuffer) {
    GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer));
    glViewport(0, 0, state->width, state->height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLCHK(glUseProgram(shader->program));
    GLCHK(glUniform1i(shader->uniform_locations[0], 0)); // Texture unit
    /* Dimensions of a single pixel in texture co-ordinates */
    GLCHK(glUniform2f(shader->uniform_locations[1], 1.0 / (float) state->width,
                      1.0 / (float) state->height));
    GLCHK(glUniform1i(shader->uniform_locations[2], 1)); // Texture unit
    GLCHK(glActiveTexture(GL_TEXTURE0));
    GLCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, state->texture));
    GLCHK(glActiveTexture(GL_TEXTURE1));
    GLCHK(glBindTexture(GL_TEXTURE_2D, maskName));
    GLCHK(glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad()));
    GLCHK(glEnableVertexAttribArray(shader->attribute_locations[0]));
    GLCHK(glVertexAttribPointer(shader->attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
    GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));
    glFlush();
    glFinish();
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/* Put the camera feed on the screen.
 *
 * @param state The state of the application.
 * @return void.
 */
void raspisilvioDrawCamera(RASPITEX_STATE *state) {
    raspisilvioHelpDraw(state);

    glFlush();
    glFinish();
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/* Apply a mask to the texture and put the result in the specified frame buffer.
 *
 * @param state The state of the application.
 * @param maskName The mask to be applied.
 * @param frameBuffer The name of the frame buffer. *
 * @return void.
 */
void raspisilvioCameraMask(RASPITEX_STATE *state, GLuint maskName, GLuint frameBuffer) {
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
    glViewport(0, 0, state->width, state->height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(mask_camera_shader.program);
    glUniform1i(mask_camera_shader.uniform_locations[0], 0);
    glUniform1i(mask_camera_shader.uniform_locations[1], 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, state->texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, maskName);
    glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad());
    glEnableVertexAttribArray(mask_camera_shader.attribute_locations[0]);
    glVertexAttribPointer(mask_camera_shader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glFlush();
    glFinish();
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/* Do processing to a texture using the specified shader program and put it on the specified frame buffer.
 * The shader is expected to have these parameters:
 * 1 - First attribute is the vertex and should contains only x and y coordinates
 * 2 - First uniform is the texture unit and the type should be sampler2D
 * 3 - Second uniform is the dimension of the pixel in texture coordinates
 *
 * @param shader The shader to be used.
 * @param state The state of the application.
 * @param frameBuffer The name of the frame buffer.
 * @param textureName The name of the texture.
 * @return void.
 */
void raspisilvioProcessingTexture(RaspisilvioShaderProgram *shader, RASPITEX_STATE *state, GLuint frameBuffer,
                                  GLuint textureName) {
    GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer));
    glViewport(0, 0, state->width, state->height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLCHK(glUseProgram(shader->program));
    GLCHK(glUniform1i(shader->uniform_locations[0], 0)); // Texture unit
    /* Dimensions of a single pixel in texture co-ordinates */
    GLCHK(glUniform2f(shader->uniform_locations[1], 1.0 / (float) state->width,
                      1.0 / (float) state->height));
    GLCHK(glActiveTexture(GL_TEXTURE0));
    GLCHK(glBindTexture(GL_TEXTURE_2D, textureName));
    GLCHK(glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad()));
    GLCHK(glEnableVertexAttribArray(shader->attribute_locations[0]));
    GLCHK(glVertexAttribPointer(shader->attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
    GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));
    glFlush();
    glFinish();
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/* Do processing to a texture using the specified shader program and put it on the specified frame buffer.
 * At the same time it applies a mask (i.e.  Don't process the pixel indicated in the mask).
 * The shader is expected to have these parameters:
 * 1 - First attribute is the vertex and should contains only x and y coordinates
 * 2 - First uniform is the texture unit and the type should be sampler2D
 * 3 - Second uniform is the dimension of the pixel in texture coordinates
 *
 * @param shader The shader to be used.
 * @param state The state of the application.
 * @param maskName The mask to be applied.
 * @param frameBuffer The name of the frame buffer.
 * @param textureName The name of the texture.
 * @return void.
 */
void raspisilvioProcessingTextureMask(RaspisilvioShaderProgram *shader, RASPITEX_STATE *state, GLuint maskName,
                                      GLuint frameBuffer, GLuint textureName) {
    // TODO: implement
    GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer));
    glViewport(0, 0, state->width, state->height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLCHK(glUseProgram(shader->program));
    GLCHK(glUniform1i(shader->uniform_locations[0], 0)); // Texture unit
    /* Dimensions of a single pixel in texture co-ordinates */
    GLCHK(glUniform2f(shader->uniform_locations[1], 1.0 / (float) state->width,
                      1.0 / (float) state->height));
    GLCHK(glActiveTexture(GL_TEXTURE0));
    GLCHK(glBindTexture(GL_TEXTURE_2D, textureName));
    GLCHK(glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad()));
    GLCHK(glEnableVertexAttribArray(shader->attribute_locations[0]));
    GLCHK(glVertexAttribPointer(shader->attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
    GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));
    glFlush();
    glFinish();
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/* Put the texture on the screen.
 *
 * @param state The state of the application.
 * @return void.
 */
void raspisilvioDrawTexture(RASPITEX_STATE *state, GLuint textureName) {
    GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    glViewport(0, 0, state->width, state->height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLCHK(glUseProgram(raspisilvioGetResultShader()->program));
    GLCHK(glUniform1i(raspisilvioGetResultShader()->uniform_locations[0], 0)); // Texture unit
    GLCHK(glActiveTexture(GL_TEXTURE0));
    GLCHK(glBindTexture(GL_TEXTURE_2D, textureName));
    GLCHK(glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad()));
    GLCHK(glEnableVertexAttribArray(raspisilvioGetResultShader()->attribute_locations[0]));
    GLCHK(glVertexAttribPointer(raspisilvioGetResultShader()->attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
    GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));
    glFlush();
    glFinish();
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/* Apply a mask to the texture and put the result in the specified frame buffer.
 *
 * @param state The state of the application.
 * @param maskName The mask to be applied.
 * @param frameBuffer The name of the frame buffer.
 * @param textureName The name of the texture.
 * @return void.
 */
void raspisilvioTextureMask(RASPITEX_STATE *state, GLuint maskName, GLuint frameBuffer, GLuint textureName) {
    GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer));
    glViewport(0, 0, state->width, state->height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLCHK(glUseProgram(mask_tex_shader.program));
    GLCHK(glUniform1i(mask_tex_shader.uniform_locations[0], 0)); // Texture unit
    GLCHK(glUniform1i(mask_tex_shader.uniform_locations[1], 1)); // Texture unit
    GLCHK(glActiveTexture(GL_TEXTURE0));
    GLCHK(glBindTexture(GL_TEXTURE_2D, textureName));
    GLCHK(glActiveTexture(GL_TEXTURE1));
    GLCHK(glBindTexture(GL_TEXTURE_2D, maskName));
    GLCHK(glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad()));
    GLCHK(glEnableVertexAttribArray(mask_tex_shader.attribute_locations[0]));
    GLCHK(glVertexAttribPointer(mask_tex_shader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
    GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));
    glFlush();
    glFinish();
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/* Set the pixels data for a texture.
 *
 * @param name The name of the texture.
 * @param width Width of the texture.
 * @param height Height of the texture.
 * @param data The data containing pixel data
 * @param format The data format of the pixels
 * @return void.
 */
void raspisilvioSetTextureData(GLuint name, int width, int height, uint8_t *data, int format) {
    GLCHK(glBindTexture(GL_TEXTURE_2D, name));
    GLCHK(glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data));
    GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat) GL_NEAREST));
    GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat) GL_NEAREST));
}
///////////////////////////////////////////////////////////////////////////////////////////////////
