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

typedef struct {
    float xb1, xb2;
    float xt1, xt2;
    float y1, y2;
} HeadingRegion;

static const int HISTOGRAM_WIDTH = 257;

static const int HISTOGRAM_HEIGHT = 3;

static const int FRAMBE_BUFFER_PREVIEW = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////
int abodInit(RASPITEX_STATE *raspitex_state);

int abodDraw(RASPITEX_STATE *state);

void abodDrawMaskX();

void abodDrawHeads();

void abodExtractReference(const RASPITEX_STATE *state, int *width, int *height);

void abodBuildHistogram(int w, int h);

void abodMatchHistogramCPU(const RASPITEX_STATE *state);

void abodFillMask(int32_t i, int32_t i1);

int isInsideTrap(int row, int col, int32_t i, int32_t i1);

int isAtLeft(int x4, int y4, int x1, int y1, int x, int y);

int isAtRight(int x3, int y3, int x2, int y2, int x, int y);

///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram lines_shader = {
        .vs_file = "gl_scenes/linesVS.glsl",
        .fs_file = "gl_scenes/linesFS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"color"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram gauss_hsi_shader = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/gaussian5hsiFS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "tex_unit"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram gauss_shader = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/gaussian5FS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "tex_unit"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram hsi_shader = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/hsiFS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "tex_unit"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram mask_shader = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/maskFS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "heads"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram hist_shader = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/histCFS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "hist_h", "hist_i", "threshold", "i_umbral"},
        .attribute_names =
                {"vertex"},
};

///////////////////////////////////////////////////////////////////////////////////////////////////
int render_id = 0;
GLuint h_hist_tex_id;
GLuint i_hist_tex_id;
GLuint trap_tex_id;
GLuint hsi_tex_id;
GLuint hsi_fbo_id;
HeadingRegion heads = {
        .xb1 = -0.8f,
        .xb2 = 0.8f,
        .xt1 = -0.2f,
        .xt2 = 0.2f,
        .y1 = -0.5f,
        .y2 = 0.5f
};
uint8_t *pixels_from_fb;
uint8_t *pixels_from_fb_full;
uint8_t *pixels_hist_h;
uint8_t *pixels_hist_i;
RaspisilvioHistogram intensity_hist;
RaspisilvioHistogram hue_hist;
int hue_bin = 10;
int intensity_bin = 10;
int hue_threshold = 100;
int intensity_threshold = 100;

uint8_t *pixels_from_mask;

GLuint mask_tex_id;

///////////////////////////////////////////////////////////////////////////////////////////////////

int main() {
    int exit_code;

    RaspisilvioApplication abod;
    raspisilvioGetDefault(&abod);
    abod.init = abodInit;
    abod.draw = abodDraw;

    exit_code = raspisilvioInitApp(&abod);
    if (exit_code != EX_OK) {
        return exit_code;
    }

    exit_code = raspisilvioStart(&abod);
    if (exit_code != EX_OK) {
        return exit_code;
    }

    int ch;

    printf("Render id = %d\n", render_id);

    while (1) {
        vcos_sleep(1000);

        fflush(stdin);
        ch = getchar();

        if (render_id < 13) {
            render_id++;
        }
        else {
            render_id = 0;
        }
        printf("Render id = %d\n", render_id);
    }

    raspisilvioStop(&abod);
    return exit_code;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Creates the OpenGL ES 2.X context and builds the shaders.
 * @param raspitex_state A pointer to the GL preview state.
 * @return Zero if successful.
 */
int abodInit(RASPITEX_STATE *raspitex_state) {
    int rc = 0;

    rc = raspisilvioInitHelp(raspitex_state);
    raspisilvioLoadShader(&lines_shader);
    raspisilvioLoadShader(&gauss_hsi_shader);
    raspisilvioLoadShader(&mask_shader);
    raspisilvioLoadShader(&hist_shader);

    raspisilvioLoadShader(&gauss_shader);
    raspisilvioLoadShader(&hsi_shader);

    raspisilvioCreateTextureData(pixels_from_fb, raspitex_state->width, raspitex_state->height, GL_RGBA);
    raspisilvioCreateTextureData(pixels_from_fb_full, raspitex_state->width, raspitex_state->height, GL_RGBA);

    raspisilvioCreateTextureData(pixels_hist_h, HISTOGRAM_WIDTH, HISTOGRAM_HEIGHT, GL_RGBA);
    raspisilvioCreateTextureData(pixels_hist_i, HISTOGRAM_WIDTH, HISTOGRAM_HEIGHT, GL_RGBA);

//    raspisilvioCreateTextureData(pixels_from_mask, raspitex_state->width, raspitex_state->height, GL_RGBA);
    pixels_from_mask = malloc(4 * 640 * 480);//TODO: CHECK HOW TO DO ALLOCATION IN API

    abodFillMask(raspitex_state->width, raspitex_state->height);

    raspisilvioCreateTexture(&mask_tex_id, GL_FALSE, raspitex_state->width, raspitex_state->height, pixels_from_mask,
                             GL_RGBA);

    raspisilvioCreateTexture(&trap_tex_id, GL_TRUE, 0, 0, NULL, GL_RGBA);

    raspisilvioCreateTexture(&h_hist_tex_id, GL_FALSE, HISTOGRAM_WIDTH, HISTOGRAM_HEIGHT, NULL, GL_RGBA);
    raspisilvioCreateTexture(&i_hist_tex_id, GL_FALSE, HISTOGRAM_WIDTH, HISTOGRAM_HEIGHT, NULL, GL_RGBA);

    raspisilvioCreateTextureFB(&hsi_tex_id, raspitex_state->width, raspitex_state->height, NULL, GL_RGBA, &hsi_fbo_id);

    return rc;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void abodFillMask(int32_t width, int32_t height) {
    int i, j;
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            if (isInsideTrap(i, j, width, height)) {
                pixels_from_mask[4 * (i * width + j) + 0] = 255;
                pixels_from_mask[4 * (i * width + j) + 1] = 255;
                pixels_from_mask[4 * (i * width + j) + 2] = 255;
                pixels_from_mask[4 * (i * width + j) + 3] = 255;
            } else {
                pixels_from_mask[4 * (i * width + j) + 0] = 0;
                pixels_from_mask[4 * (i * width + j) + 1] = 0;
                pixels_from_mask[4 * (i * width + j) + 2] = 0;
                pixels_from_mask[4 * (i * width + j) + 3] = 255;
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

int isInsideTrap(int row, int col, int32_t width, int32_t height) {
    int y1 = 0, y2 = 0, y3 = height, y4 = height;
    int yRef = 0.5f * height * (heads.y1 + 1.0f);

    int x1 = width * 0.5f * (heads.xb1 + 1.0f);
    int x2 = width * 0.5f * (heads.xb2 + 1.0f);
    int x3 = width * 0.5f * (heads.xt2 + 1.0f);
    int x4 = width * 0.5f * (heads.xt1 + 1.0f);

    int x = col;
    int y = row;

    if (y > yRef) {
        return 0;
    }

    if (isAtLeft(x4, y4, x1, y1, x, y)) {
        return 0;
    }

    if (isAtRight(x3, y3, x2, y2, x, y)) {
        return 0;
    }

    return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

int isAtRight(int x3, int y3, int x2, int y2, int x, int y) {
    float m = ((float) y3 - y2) / ((float) x2 - x3);
    float ytemp = m * (x2 - x);

    if (ytemp > y) {
        return 0;
    }
    return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

int isAtLeft(int x4, int y4, int x1, int y1, int x, int y) {
    float m = ((float) y4 - y1) / ((float) x4 - x1);
    float ytemp = m * (x - x1);

    if (ytemp > y) {
        return 0;
    }
    return 1;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/* Redraws the scene with the latest buffer.
 *
 * @param raspitex_state A pointer to the GL preview state.
 * @return Zero if successful.
 */
int abodDraw(RASPITEX_STATE *state) {

    int i, j;

    switch (render_id) {
        case 0:
            raspisilvioDrawCamera(state);
            break;
        case 1:
            raspisilvioHelpDraw(state);

            abodDrawHeads();

            break;
        case 2:
            raspisilvioDrawTexture(state, mask_tex_id);

            break;
        case 3:
            raspisilvioCameraMask(state, mask_tex_id, FRAMBE_BUFFER_PREVIEW);

            break;
        case 4:

            raspisilvioProcessingCamera(&gauss_hsi_shader, state, FRAMBE_BUFFER_PREVIEW);

            break;
        case 5:
            raspisilvioProcessingCamera(&gauss_shader, state, hsi_fbo_id);

            raspisilvioProcessingTexture(&hsi_shader, state, FRAMBE_BUFFER_PREVIEW, hsi_tex_id);

            break;
        case 6:

            raspisilvioProcessingCamera(&gauss_hsi_shader, state, hsi_fbo_id);

            abodDrawMaskX();

            break;
        case 7:
            raspisilvioProcessingCamera(&gauss_hsi_shader, state, hsi_fbo_id);

            abodDrawMaskX();

            int w, h;

            abodExtractReference(state, &w, &h);

            raspisilvioSetTextureData(trap_tex_id, w, h, pixels_from_fb, GL_RGBA);

            raspisilvioDrawTexture(state, trap_tex_id);
            break;
        case 8:
            raspisilvioProcessingCamera(&gauss_hsi_shader, state, hsi_fbo_id);

            raspisilvioTextureMask(state, mask_tex_id, FRAMBE_BUFFER_PREVIEW, hsi_tex_id);
            break;

        case 9:

            raspisilvioProcessingCameraMask(&gauss_hsi_shader, state, mask_tex_id, FRAMBE_BUFFER_PREVIEW);

            break;

        case 10:

            raspisilvioProcessingCameraMask(&gauss_hsi_shader, state, mask_tex_id, hsi_fbo_id);

            abodExtractReference(state, &w, &h);

            raspisilvioSetTextureData(trap_tex_id, w, h, pixels_from_fb, GL_RGBA);

            raspisilvioDrawTexture(state, trap_tex_id);

            break;

        case 11:

            raspisilvioProcessingCamera(&gauss_hsi_shader, state, hsi_fbo_id);

            abodDrawMaskX();

            abodExtractReference(state, &w, &h);

            abodBuildHistogram(w, h);

            raspisilvioSetTextureData(trap_tex_id, w, h, pixels_from_fb, GL_RGBA);

            raspisilvioDrawTexture(state, trap_tex_id);

            break;
        case 12:
            raspisilvioProcessingCamera(&gauss_hsi_shader, state, hsi_fbo_id);

            abodDrawMaskX();

            abodExtractReference(state, &w, &h);

            abodBuildHistogram(w, h);

            raspisilvioWriteHistToTexture(&hue_hist, pixels_hist_h);
            raspisilvioWriteHistToTexture(&intensity_hist, pixels_hist_i);

            raspisilvioSetTextureData(h_hist_tex_id, 255 / hue_hist.bin_width + 1, 2, pixels_hist_h, GL_RGBA);

            raspisilvioSetTextureData(i_hist_tex_id, 255 / intensity_hist.bin_width + 1, 2, pixels_hist_i, GL_RGBA);

            abodMatchHistogram();

            break;
        case 13:
            raspisilvioProcessingCamera(&gauss_hsi_shader, state, hsi_fbo_id);

            glReadPixels(0, 0, state->width, state->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels_from_fb_full);

            abodDrawMaskX();

            abodExtractReference(state, &w, &h);

            abodBuildHistogram(w, h);

            abodMatchHistogramCPU(state);

            raspisilvioSetTextureData(trap_tex_id, state->width, state->height, pixels_from_fb_full, GL_RGBA);

            raspisilvioDrawTexture(state, trap_tex_id);
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void abodMatchHistogramCPU(const RASPITEX_STATE *state) {
    uint8_t *out = pixels_from_fb_full;
    uint8_t *end = out + 4 * state->width * state->height;

    while (out < end) {
        uint8_t intensity = out[0];
        uint8_t saturation = out[1];
        uint8_t hue = out[2];

        int intensity_value = raspisilvioGetHistogramFilteredValue(&intensity_hist, intensity);
        int hue_value = raspisilvioGetHistogramFilteredValue(&hue_hist, hue);

        if (intensity < intensity_hist.bin_width) {
            if (intensity_value < intensity_threshold) {
                out[0] = out[1] = out[2] = 255;
            } else {
                out[0] = out[1] = out[2] = 0;
            }
        } else if (hue_value < hue_threshold || intensity_value < intensity_threshold) {
            out[0] = out[1] = out[2] = 255;
        } else {
            out[0] = out[1] = out[2] = 0;
        }
        out += 4;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void abodBuildHistogram(int w, int h) {
    raspisilvioInitHist(&intensity_hist, intensity_bin);
    raspisilvioInitHist(&hue_hist, hue_bin);

    uint8_t *out = pixels_from_fb;
    uint8_t *end = out + 4 * w * h;

    while (out < end) {
        uint8_t intensity = out[0];
        uint8_t saturation = out[1];
        uint8_t hue = out[2];

        intensity_hist.bins[intensity / intensity_hist.bin_width]++;
        intensity_hist.count++;

        if (intensity > intensity_hist.bin_width && saturation > intensity_hist.bin_width) {
            hue_hist.bins[hue / hue_hist.bin_width]++;
            hue_hist.count++;

            out[0] = 255;
            out[1] = 255;
            out[2] = 255;
            out[3] = 255;
        }

        out += 4;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void abodExtractReference(const RASPITEX_STATE *state, int *width, int *height) {
    int x, y;

    x = state->width * 0.5f * (heads.xb1 + 1.0f);
    y = 0;

    *width = 0.5f * state->width * (heads.xb2 - heads.xb1);
    *height = 0.5f * state->height * (heads.y1 + 1.0f);

    GLCHK(glReadPixels(x, y, *width, *height, GL_RGBA, GL_UNSIGNED_BYTE, pixels_from_fb));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void abodDrawHeads() {
    GLCHK(glUseProgram(lines_shader.program));
    GLCHK(glUniform3f(lines_shader.uniform_locations[0], 0.0f, 1.0f, 0.0f));

    GLfloat points[] = {
            heads.xb1, -1.0f,
            heads.xt1, 1.0f,
            heads.xb2, -1.0f,
            heads.xt2, 1.0f,
            -1.0f, heads.y1,
            1.0f, heads.y1,
            -1.0f, heads.y2,
            1.0f, heads.y2
    };

    GLCHK(glBindBuffer(GL_ARRAY_BUFFER, 0));
    GLCHK(glEnableVertexAttribArray(lines_shader.attribute_locations[0]));
    GLCHK(glVertexAttribPointer(lines_shader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, points));
    GLCHK(glDrawArrays(GL_LINES, 0, 8));
    glFlush();
    glFinish();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void abodDrawMaskX() {
    GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLCHK(glUseProgram(mask_shader.program));
    GLCHK(glUniform1i(mask_shader.uniform_locations[0], 0)); // Texture unit
    /* Heads */
    GLCHK(glUniform4f(mask_shader.uniform_locations[1], heads.xb1, heads.xt1, heads.xt2, heads.xb2));
    GLCHK(glActiveTexture(GL_TEXTURE0));
    GLCHK(glBindTexture(GL_TEXTURE_2D, hsi_tex_id));
    GLCHK(glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad()));
    GLCHK(glEnableVertexAttribArray(mask_shader.attribute_locations[0]));
    GLCHK(glVertexAttribPointer(mask_shader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
    GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));
    glFlush();
    glFinish();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void abodMatchHistogram() {
    GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLCHK(glUseProgram(hist_shader.program));
    GLCHK(glUniform1i(hist_shader.uniform_locations[0], 0)); // Texture unit
    GLCHK(glUniform1i(hist_shader.uniform_locations[1], 1)); // Texture unit
    GLCHK(glUniform1i(hist_shader.uniform_locations[2], 2)); // Texture unit
    GLCHK(glUniform2i(hist_shader.uniform_locations[3], hue_threshold, intensity_threshold)); // threshold
    GLCHK(glUniform1i(hist_shader.uniform_locations[4], intensity_bin)); // threshold

    GLCHK(glActiveTexture(GL_TEXTURE0));
    GLCHK(glBindTexture(GL_TEXTURE_2D, hsi_tex_id));
    GLCHK(glActiveTexture(GL_TEXTURE1));
    GLCHK(glBindTexture(GL_TEXTURE_2D, h_hist_tex_id));
    GLCHK(glActiveTexture(GL_TEXTURE2));
    GLCHK(glBindTexture(GL_TEXTURE_2D, i_hist_tex_id));
    GLCHK(glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad()));
    GLCHK(glEnableVertexAttribArray(hist_shader.attribute_locations[0]));
    GLCHK(glVertexAttribPointer(hist_shader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
    GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));
    glFlush();
    glFinish();
}
///////////////////////////////////////////////////////////////////////////////////////////////////
