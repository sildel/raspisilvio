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

///////////////////////////////////////////////////////////////////////////////////////////////////
int abodInit(RASPITEX_STATE *raspitex_state);

int abodDraw(RASPITEX_STATE *state);

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

///////////////////////////////////////////////////////////////////////////////////////////////////
int main() {
    int exit_code;

    RaspisilvioApplication abod;
    raspisilvioGetDefault(&abod);
    abod.init = abodInit;
//    abod.draw = abodDraw;

    exit_code = raspisilvioInitApp(&abod);
    if (exit_code != EX_OK) {
        return exit_code;
    }

    exit_code = raspisilvioStart(&abod);
    if (exit_code != EX_OK) {
        return exit_code;
    }

    int ch;

    while (1) {
//        vcos_sleep(1000);
//
//        fflush(stdin);
//        ch = getchar();
//
//        render_id++;
//        if (render_id >= 7) {
//            render_id = 0;
//        }
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

    pixels_from_fb = malloc(raspitex_state->width * raspitex_state->height * 4);
    pixels_from_fb_full = malloc(raspitex_state->width * raspitex_state->height * 4);
    pixels_hist_h = malloc(257 * 3 * 4);
    pixels_hist_i = malloc(257 * 3 * 4);

    GLCHK(glEnable(GL_TEXTURE_2D));
    GLCHK(glGenTextures(1, &trap_tex_id));

    GLCHK(glGenTextures(1, &h_hist_tex_id));
    GLCHK(glGenTextures(1, &i_hist_tex_id));

    GLCHK(glBindTexture(GL_TEXTURE_2D, h_hist_tex_id));
    GLCHK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 257, 3, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));
    GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat) GL_NEAREST));
    GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat) GL_NEAREST));

    GLCHK(glBindTexture(GL_TEXTURE_2D, i_hist_tex_id));
    GLCHK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 257, 3, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));
    GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat) GL_NEAREST));
    GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat) GL_NEAREST));

    GLCHK(glEnable(GL_TEXTURE_2D));
    GLCHK(glGenTextures(1, &hsi_tex_id));
    GLCHK(glBindTexture(GL_TEXTURE_2D, hsi_tex_id));
    GLCHK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, raspitex_state->width, raspitex_state->height, 0, GL_RGBA,
                       GL_UNSIGNED_BYTE, NULL));
    GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat) GL_NEAREST));
    GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat) GL_NEAREST));

    GLCHK(glGenFramebuffers(1, &hsi_fbo_id));
    GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, hsi_fbo_id));
    GLCHK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hsi_tex_id, 0));
    GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

    return rc;
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
            raspisilvioDrawHelp(state);
            break;
        case 1:
            raspisilvioDrawHelp(state);

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
            break;
        case 2:
            GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            GLCHK(glUseProgram(gauss_hsi_shader.program));
            GLCHK(glUniform1i(gauss_hsi_shader.uniform_locations[0], 0)); // Texture unit
            /* Dimensions of a single pixel in texture co-ordinates */
            GLCHK(glUniform2f(gauss_hsi_shader.uniform_locations[1], 1.0 / (float) state->width,
                              1.0 / (float) state->height));

            GLCHK(glActiveTexture(GL_TEXTURE0));
            GLCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, state->texture));
            GLCHK(glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad()));
            GLCHK(glEnableVertexAttribArray(gauss_hsi_shader.attribute_locations[0]));
            GLCHK(glVertexAttribPointer(gauss_hsi_shader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
            GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));
            break;
        case 3:
            GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, hsi_fbo_id));
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            GLCHK(glUseProgram(gauss_hsi_shader.program));
            GLCHK(glUniform1i(gauss_hsi_shader.uniform_locations[0], 0)); // Texture unit
            /* Dimensions of a single pixel in texture co-ordinates */
            GLCHK(glUniform2f(gauss_hsi_shader.uniform_locations[1], 1.0 / (float) state->width,
                              1.0 / (float) state->height));

            GLCHK(glActiveTexture(GL_TEXTURE0));
            GLCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, state->texture));
            GLCHK(glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad()));
            GLCHK(glEnableVertexAttribArray(gauss_hsi_shader.attribute_locations[0]));
            GLCHK(glVertexAttribPointer(gauss_hsi_shader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
            GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));
            glFlush();
            glFinish();

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
            break;
        case 4:
            GLCHK (glBindFramebuffer(GL_FRAMEBUFFER, hsi_fbo_id));
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            GLCHK(glUseProgram(gauss_hsi_shader.program));
            GLCHK(glUniform1i(gauss_hsi_shader.uniform_locations[0], 0)); // Texture unit
            /* Dimensions of a single pixel in texture co-ordinates */
            GLCHK (glUniform2f(gauss_hsi_shader.uniform_locations[1], 1.0 / (float) state->width,
                               1.0 / (float) state->height));

            GLCHK(glActiveTexture(GL_TEXTURE0));
            GLCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, state->texture));
            GLCHK(glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad()));
            GLCHK(glEnableVertexAttribArray(gauss_hsi_shader.attribute_locations[0]));
            GLCHK(glVertexAttribPointer(gauss_hsi_shader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
            GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));
            glFlush();
            glFinish();

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

            int x, y, width, height;

            x = state->width * 0.5f * (heads.xb1 + 1.0f);
            y = 0;

            width = 0.5f * state->width * (heads.xb2 - heads.xb1);
            height = 0.5f * state->height * (heads.y1 + 1.0f);

            GLCHK(glReadPixels(x, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels_from_fb));

            GLCHK(glBindTexture(GL_TEXTURE_2D, trap_tex_id));
            GLCHK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                               pixels_from_fb));
            GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat) GL_NEAREST));
            GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat) GL_NEAREST));

            GLCHK(glUseProgram(raspisilvioGetResultShader()->program));
            GLCHK(glUniform1i(raspisilvioGetResultShader()->uniform_locations[0], 0)); // Texture unit

            GLCHK(glActiveTexture(GL_TEXTURE0));
            GLCHK(glBindTexture(GL_TEXTURE_2D, trap_tex_id));
            GLCHK(glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad()));
            GLCHK(glEnableVertexAttribArray(raspisilvioGetResultShader()->attribute_locations[0]));
            GLCHK(glVertexAttribPointer(raspisilvioGetResultShader()->attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0,
                                        0));
            GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));
            break;

        case 5:
            GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, hsi_fbo_id));
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            GLCHK(glUseProgram(gauss_hsi_shader.program));
            GLCHK(glUniform1i(gauss_hsi_shader.uniform_locations[0], 0)); // Texture unit
            /* Dimensions of a single pixel in texture co-ordinates */
            GLCHK(glUniform2f(gauss_hsi_shader.uniform_locations[1], 1.0 / (float) state->width,
                              1.0 / (float) state->height));

            GLCHK(glActiveTexture(GL_TEXTURE0));
            GLCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, state->texture));
            GLCHK(glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad()));
            GLCHK(glEnableVertexAttribArray(gauss_hsi_shader.attribute_locations[0]));
            GLCHK(glVertexAttribPointer(gauss_hsi_shader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
            GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));
            glFlush();
            glFinish();

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

            x = state->width * 0.5f * (heads.xb1 + 1.0f);
            y = 0;

            width = 0.5f * state->width * (heads.xb2 - heads.xb1);
            height = 0.5f * state->height * (heads.y1 + 1.0f);

            GLCHK(glReadPixels(x, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels_from_fb));

            raspisilvioInitHist(&intensity_hist, intensity_bin);
            raspisilvioInitHist(&hue_hist, hue_bin);

            uint8_t *out = pixels_from_fb;
            uint8_t *end = out + 4 * width * height;

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

            GLCHK(glBindTexture(GL_TEXTURE_2D, trap_tex_id));
            GLCHK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                               pixels_from_fb));
            GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat) GL_NEAREST));
            GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat) GL_NEAREST));

            GLCHK(glUseProgram(raspisilvioGetResultShader()->program));
            GLCHK(glUniform1i(raspisilvioGetResultShader()->uniform_locations[0], 0)); // Texture unit

            GLCHK(glActiveTexture(GL_TEXTURE0));
            GLCHK(glBindTexture(GL_TEXTURE_2D, trap_tex_id));
            GLCHK(glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad()));
            GLCHK(glEnableVertexAttribArray(raspisilvioGetResultShader()->attribute_locations[0]));
            GLCHK(glVertexAttribPointer(raspisilvioGetResultShader()->attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0,
                                        0));
            GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));

            break;
        case 6:
            GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, hsi_fbo_id));
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            GLCHK(glUseProgram(gauss_hsi_shader.program));
            GLCHK(glUniform1i(gauss_hsi_shader.uniform_locations[0], 0)); // Texture unit
            /* Dimensions of a single pixel in texture co-ordinates */
            GLCHK(glUniform2f(gauss_hsi_shader.uniform_locations[1], 1.0 / (float) state->width,
                              1.0 / (float) state->height));

            GLCHK(glActiveTexture(GL_TEXTURE0));
            GLCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, state->texture));
            GLCHK(glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad()));
            GLCHK(glEnableVertexAttribArray(gauss_hsi_shader.attribute_locations[0]));
            GLCHK(glVertexAttribPointer(gauss_hsi_shader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
            GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));
            glFlush();
            glFinish();

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

            x = state->width * 0.5f * (heads.xb1 + 1.0f);
            y = 0;

            width = 0.5f * state->width * (heads.xb2 - heads.xb1);
            height = 0.5f * state->height * (heads.y1 + 1.0f);

            GLCHK(glReadPixels(x, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels_from_fb));

            raspisilvioInitHist(&intensity_hist, intensity_bin);
            raspisilvioInitHist(&hue_hist, hue_bin);

            out = pixels_from_fb;
            end = out + 4 * width * height;

            while (out < end) {
                uint8_t intensity = out[0];
                uint8_t saturation = out[1];
                uint8_t hue = out[2];

                intensity_hist.bins[intensity / intensity_hist.bin_width]++;
                intensity_hist.count++;

                if (intensity > intensity_hist.bin_width && saturation > intensity_hist.bin_width) {
                    hue_hist.bins[hue / hue_hist.bin_width]++;
                    hue_hist.count++;
                }

                out += 4;
            }

            raspisilvioWriteHistToTexture(&hue_hist, pixels_hist_h);
            raspisilvioWriteHistToTexture(&intensity_hist, pixels_hist_i);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            GLCHK(glBindTexture(GL_TEXTURE_2D, h_hist_tex_id));
            GLCHK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 255 / hue_hist.bin_width + 1, 2, 0, GL_RGBA,
                               GL_UNSIGNED_BYTE, pixels_hist_h));
            GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat) GL_NEAREST));
            GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat) GL_NEAREST));

            GLCHK(glBindTexture(GL_TEXTURE_2D, i_hist_tex_id));
            GLCHK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 255 / intensity_hist.bin_width + 1, 2, 0, GL_RGBA,
                               GL_UNSIGNED_BYTE, pixels_hist_i));
            GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat) GL_NEAREST));
            GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat) GL_NEAREST));

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

            break;
        case 7:
            GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, hsi_fbo_id));
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            GLCHK(glUseProgram(gauss_hsi_shader.program));
            GLCHK(glUniform1i(gauss_hsi_shader.uniform_locations[0], 0)); // Texture unit
            /* Dimensions of a single pixel in texture co-ordinates */
            GLCHK(glUniform2f(gauss_hsi_shader.uniform_locations[1], 1.0 / (float) state->width,
                              1.0 / (float) state->height));

            GLCHK(glActiveTexture(GL_TEXTURE0));
            GLCHK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, state->texture));
            GLCHK(glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad()));
            GLCHK(glEnableVertexAttribArray(gauss_hsi_shader.attribute_locations[0]));
            GLCHK(glVertexAttribPointer(gauss_hsi_shader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
            GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));
            glFlush();
            glFinish();

            glReadPixels(0, 0, state->width, state->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels_from_fb_full);

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

            x = state->width * 0.5f * (heads.xb1 + 1.0f);
            y = 0;

            width = 0.5f * state->width * (heads.xb2 - heads.xb1);
            height = 0.5f * state->height * (heads.y1 + 1.0f);

            GLCHK(glReadPixels(x, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels_from_fb));

            raspisilvioInitHist(&intensity_hist, intensity_bin);
            raspisilvioInitHist(&hue_hist, hue_bin);

            out = pixels_from_fb;
            end = out + 4 * width * height;

            while (out < end) {
                uint8_t intensity = out[0];
                uint8_t saturation = out[1];
                uint8_t hue = out[2];

                intensity_hist.bins[intensity / intensity_hist.bin_width]++;
                intensity_hist.count++;

                if (intensity > intensity_hist.bin_width && saturation > intensity_hist.bin_width) {
                    hue_hist.bins[hue / hue_hist.bin_width]++;
                    hue_hist.count++;
                }

                out += 4;
            }

            out = pixels_from_fb_full;
            end = out + 4 * state->width * state->height;

            while (out < end) {
                uint8_t intensity = out[0];
                uint8_t saturation = out[1];
                uint8_t hue = out[2];

                int intensity_value = raspisilvioGetHistogramFilteredValue(&intensity_hist, intensity);
                int hue_value = raspisilvioGetHistogramFilteredValue(&hue_hist, hue);

                if (intensity < intensity_hist.bin_width) {
                    if (intensity_value < intensity_threshold) {
                        out[0] = out[1] = out[2] = 255;
                    }
                    else {
                        out[0] = out[1] = out[2] = 0;
                    }
                }
                else if (hue_value < hue_threshold || intensity_value < intensity_threshold) {
                    out[0] = out[1] = out[2] = 255;
                }
                else {
                    out[0] = out[1] = out[2] = 0;
                }
                out += 4;
            }

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            GLCHK(glBindTexture(GL_TEXTURE_2D, trap_tex_id));
            GLCHK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, state->width, state->height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                               pixels_from_fb_full));
            GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat) GL_NEAREST));
            GLCHK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat) GL_NEAREST));

            GLCHK(glUseProgram(raspisilvioGetResultShader()->program));
            GLCHK(glUniform1i(raspisilvioGetResultShader()->uniform_locations[0], 0)); // Texture unit

            GLCHK(glActiveTexture(GL_TEXTURE0));
            GLCHK(glBindTexture(GL_TEXTURE_2D, trap_tex_id));
            GLCHK(glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad()));
            GLCHK(glEnableVertexAttribArray(raspisilvioGetResultShader()->attribute_locations[0]));
            GLCHK(glVertexAttribPointer(raspisilvioGetResultShader()->attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0,
                                        0));
            GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));

            break;
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////
