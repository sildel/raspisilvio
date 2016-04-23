///////////////////////////////////////////////////////////////////////////////////////////////////
// Created by
// Jorge Silvio Delgado Morales
// based on raspicam applications of https://github.com/raspberrypi/userland
// silviodelgado70@gmail.com
///////////////////////////////////////////////////////////////////////////////////////////////////
#include "RaspiTexUtil.h"
#include "raspisilvio.h"
#include "tga.h"
#include <GLES2/gl2.h>

///////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
    float xb1, xb2;
    float xt1, xt2;
    float y1, y2;
} HeadingRegion;

static const int HISTOGRAM_WIDTH = 257;

static const int HISTOGRAM_HEIGHT = 3;

int RENDER_STEPS;

static const int TEST_WIDTH = 32;

static const int TEST_HEIGHT = 32;

static const int ABOD_HISTOGRAM_SIZE = 250;
static char *const FILE_TO_LOAD = "ttt.tga";
#define TEST_POINTS 10

#define ABOD_MAX_TRAP_POINTS (20000 * 10)

///////////////////////////////////////////////////////////////////////////////////////////////////
int abodInit(RASPITEX_STATE *raspitex_state);

int abodDraw(RASPITEX_STATE *state);

void abodDrawMaskX();

void abodDrawHeads();

void abodExtractReference(const RASPITEX_STATE *state, int *width, int *height);

void abodBuildHistogram(int w, int h);

void abodMatchHistogram();

void abodMatchHistogramCPU(const RASPITEX_STATE *state);

void abodFillMask(int32_t i, int32_t i1);

int isInsideTrap(int row, int col, int32_t i, int32_t i1);

int isAtLeft(int x4, int y4, int x1, int y1, int x, int y);

int isAtRight(int x3, int y3, int x2, int y2, int x, int y);

void abodPrintStep();

void abodFillTest(int32_t width, int32_t height);

int abodRunTest();

int abodRunTest2();

int abodRunTest3();

void abodFillTest2(const int points);

int buildHistogramTrap(RASPITEX_STATE *pSTATE, GLuint channel);

void abodFillTrapVertex(const int width, const int height);

int step26(RASPITEX_STATE *state);

///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram test_shader = {
        .vs_file = "gl_scenes/testVS.glsl",
        .fs_file = "gl_scenes/testFS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex"},
        .attribute_names =
                {"vertex"},
};
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
RaspisilvioShaderProgram gauss_hsi_shader_tex = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/gaussian5hsiFS_t.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "tex_unit"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram gauss_hsi_shader_mask = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/gaussian5hsi_maskFS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "tex_unit", "mask"},
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
RaspisilvioShaderProgram gauss_shader_es = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/gaussian5EFS.glsl",
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
RaspisilvioShaderProgram hsi_shader_es = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/hsiEFS.glsl",
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
RaspisilvioShaderProgram hist_compare_shader = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/histCompareFS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "hist_h", "hist_i", "threshold"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
int render_id = 16;
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
uint8_t *pixels_to_test;
GLfloat *vertex_hist;
GLfloat vertex2_hist[TEST_POINTS * 2];

GLuint mask_tex_id;
GLuint test_tex_id;
GLuint file_tex_id;

GLuint test_vbo;
GLuint test2_vbo;

///////////////////////////////////////////////////////////////////////////////////////////////////
GLuint histogram_tex_id;

GLuint histogram_fbo_id;

GLuint ABOD_TRAP_POINTS = 0;

GLuint abod_trap_vbo;

GLfloat abod_trap_vertex[ABOD_MAX_TRAP_POINTS];

///////////////////////////////////////////////////////////////////////////////////////////////////

GLuint histogram_h_tex;

GLuint histogram_h_fbo_id;

GLuint histogram_i_tex;

GLuint histogram_i_fbo_id;

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

    while (1) {
        vcos_sleep(1000);

        fflush(stdin);
        ch = getchar();

        if (render_id < RENDER_STEPS) {
            render_id++;
        }
        else {
            render_id = 0;
        }
    }

    raspisilvioStop(&abod);
    return exit_code;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void abodPrintStep() {
    RENDER_STEPS = 26;
    static char *render_steps[] = {
            "0 - > Camera feed",
            "1 - > Camera feed & Gauss",
            "2 - > Camera feed & HSI",
            "3 - > Camera feed & Gauss->HSI",
            "4 - > Camera feed & Gauss->HSI 2 Steps",
            "5 - > Camera feed & HSI->Gauss 2 Steps",
            "6 - > Camera feed & Head",
            "7 - > Mask",
            "8 - > Camera feed & Mask",
            "9 - > Camera feed & Mask - 2 Steps",
            "10- > Camera feed & Gauss->HSI & Mask->Shader",
            "11- > Camera feed & Reference Big & Mask->Shader",
            "12- > Camera feed & Reference & Mask->Texture - 2 Steps",
            "13- > Camera feed & Reference & Mask->Texture",
            "14- > Camera feed & Reference Big & Mask->Texture",
            "15- > Camera feed & Reference Big & Mask->Shader & Hist",
            "16- > Camera feed & Reference Big & Mask->Shader & Hist & Matching",
            "17- > Camera feed & Reference Big & Mask->Shader & Hist & Matching CPU",
            "18- > Test Texture",
            "19- > Test - Count Pixels with 255,255,255",
            "20- > Test - Count Pixels with 255,255,255 - Only 10 pixels",
            "21- > Test - Histogram 10 bins",
            "22- > Camera feed & Gauss->HSI & Histogram H",
            "23- > Camera feed & Gauss->HSI & Histogram I",
            "24- > File texture",
            "25- > File texture & Gauss->HSI",
            "26- > File texture & Gauss->HSI & Histogram I"
    };
    printf("Render id = %d\n%s\n", render_id, render_steps[render_id]);
}
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Creates the OpenGL ES 2.X context and builds the shaders.
 * @param raspitex_state A pointer to the GL preview state.
 * @return Zero if successful.
 */
int abodInit(RASPITEX_STATE *raspitex_state) {
    int rc = 0;

    rc = raspisilvioHelpInit(raspitex_state);

    raspisilvioLoadShader(&gauss_hsi_shader_tex);

    raspisilvioLoadShader(&lines_shader);
    raspisilvioLoadShader(&gauss_hsi_shader);
    raspisilvioLoadShader(&mask_shader);
    raspisilvioLoadShader(&hist_shader);

    raspisilvioLoadShader(&gauss_shader);
    raspisilvioLoadShader(&hsi_shader);

    raspisilvioLoadShader(&gauss_shader_es);
    raspisilvioLoadShader(&hsi_shader_es);

    raspisilvioLoadShader(&gauss_hsi_shader_mask);

    raspisilvioCreateTextureData(&pixels_from_fb, raspitex_state->width, raspitex_state->height, GL_RGBA);
    raspisilvioCreateTextureData(&pixels_from_fb_full, raspitex_state->width, raspitex_state->height, GL_RGBA);

    raspisilvioCreateTextureData(&pixels_hist_h, HISTOGRAM_WIDTH, HISTOGRAM_HEIGHT, GL_RGBA);
    raspisilvioCreateTextureData(&pixels_hist_i, HISTOGRAM_WIDTH, HISTOGRAM_HEIGHT, GL_RGBA);

    raspisilvioCreateTextureData(&pixels_from_mask, raspitex_state->width, raspitex_state->height, GL_RGBA);

    abodFillMask(raspitex_state->width, raspitex_state->height);

    raspisilvioCreateTexture(&mask_tex_id, GL_FALSE, raspitex_state->width, raspitex_state->height, pixels_from_mask,
                             GL_RGBA);

    raspisilvioCreateTexture(&trap_tex_id, GL_TRUE, 0, 0, NULL, GL_RGBA);

    raspisilvioCreateTexture(&h_hist_tex_id, GL_FALSE, HISTOGRAM_WIDTH, HISTOGRAM_HEIGHT, NULL, GL_RGBA);
    raspisilvioCreateTexture(&i_hist_tex_id, GL_FALSE, HISTOGRAM_WIDTH, HISTOGRAM_HEIGHT, NULL, GL_RGBA);

    raspisilvioCreateTextureFB(&hsi_tex_id, raspitex_state->width, raspitex_state->height, NULL, GL_RGBA, &hsi_fbo_id);

    raspisilvioCreateTextureData(&pixels_to_test, TEST_WIDTH, TEST_HEIGHT, GL_RGBA);
    abodFillTest(TEST_WIDTH, TEST_HEIGHT);
    raspisilvioCreateTexture(&test_tex_id, GL_FALSE, TEST_WIDTH, TEST_HEIGHT, pixels_to_test, GL_RGBA);
    raspisilvioCreateVertexBufferHistogram(&test_vbo, TEST_WIDTH, TEST_HEIGHT, &vertex_hist);

    abodFillTest2(TEST_POINTS);
    raspisilvioCreateVertexBufferHistogramData(&test2_vbo, TEST_POINTS, vertex2_hist);
    raspisilvioLoadShader(&test_shader);

    raspisilvioCreateTextureFB(&histogram_tex_id, HISTOGRAM_TEX_WIDTH, HISTOGRAM_TEX_HEIGHT, NULL, GL_RGBA,
                               &histogram_fbo_id);

    abodFillTrapVertex(raspitex_state->width, raspitex_state->height);
    raspisilvioCreateVertexBufferHistogramData(&abod_trap_vbo, ABOD_TRAP_POINTS, abod_trap_vertex);
    raspisilvioLoadTextureFromFile(FILE_TO_LOAD, &file_tex_id);
    raspisilvioLoadShader(&hist_compare_shader);

    raspisilvioCreateTextureFB(&histogram_h_tex, HISTOGRAM_TEX_WIDTH, HISTOGRAM_TEX_HEIGHT, NULL, GL_RGBA,
                               &histogram_h_fbo_id);
    raspisilvioCreateTextureFB(&histogram_i_tex, HISTOGRAM_TEX_WIDTH, HISTOGRAM_TEX_HEIGHT, NULL, GL_RGBA,
                               &histogram_i_fbo_id);

    return rc;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void abodFillTest2(const int points) {
    int i = 0;

    for (i = 0; i < points; ++i) {
        if (i < 3) {
            vertex2_hist[2 * i + 0] = 0.0f;
            vertex2_hist[2 * i + 1] = 0.0f;
        }
        else if (i < 7) {
            vertex2_hist[2 * i + 0] = 0.5f;
            vertex2_hist[2 * i + 1] = 0.5f;
        }
        else {
            vertex2_hist[2 * i + 0] = 0.99f;
            vertex2_hist[2 * i + 1] = 0.99;
        }

    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void abodFillTest(int32_t width, int32_t height) {

    int i, j, counter = 0;
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++, counter++) {
            counter %= 256;

            pixels_to_test[4 * (i * width + j) + 0] = counter;
            pixels_to_test[4 * (i * width + j) + 1] = counter;
            pixels_to_test[4 * (i * width + j) + 2] = counter;
            pixels_to_test[4 * (i * width + j) + 3] = 255;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void abodFillMask(int32_t width, int32_t height) {
    int i, j, c = 0;
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            if (isInsideTrap(i, j, width, height)) {
                pixels_from_mask[4 * (i * width + j) + 0] = 255;
                pixels_from_mask[4 * (i * width + j) + 1] = 255;
                pixels_from_mask[4 * (i * width + j) + 2] = 255;
                pixels_from_mask[4 * (i * width + j) + 3] = 255;
                c++;
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
void abodFillTrapVertex(const int width, const int height) {
    float dx = 1.0f / width;
    float dy = 1.0f / height;
    int x, y;
    ABOD_TRAP_POINTS = 0;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (isInsideTrap(y, x, width, height)) {
                abod_trap_vertex[2 * ABOD_TRAP_POINTS + 0] = dx * x;
                abod_trap_vertex[2 * ABOD_TRAP_POINTS + 1] = dy * y;
                ABOD_TRAP_POINTS++;
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
    static int a = -1;
    static int c = 0;

    if (a != render_id) {
        abodPrintStep();
        a = render_id;
    }

    switch (render_id) {
        case 0:
            raspisilvioDrawCamera(state);
//            if (c == 0) {
//                raspisilvioSaveToFile(state, FILE_TO_LOAD);
//                c = 1;
//            }
            break;
        case 1:
            raspisilvioProcessingCamera(&gauss_shader_es, state, FRAMBE_BUFFER_PREVIEW);
            break;
        case 2:
            raspisilvioProcessingCamera(&hsi_shader_es, state, FRAMBE_BUFFER_PREVIEW);
            break;
        case 3:
            raspisilvioProcessingCamera(&gauss_hsi_shader, state, FRAMBE_BUFFER_PREVIEW);
            break;
        case 4:
            raspisilvioProcessingCamera(&gauss_shader_es, state, hsi_fbo_id);
            raspisilvioProcessingTexture(&hsi_shader, state, FRAMBE_BUFFER_PREVIEW, hsi_tex_id);
            break;
        case 5:
            raspisilvioProcessingCamera(&hsi_shader_es, state, hsi_fbo_id);
            raspisilvioProcessingTexture(&gauss_shader, state, FRAMBE_BUFFER_PREVIEW, hsi_tex_id);
            break;
        case 6:
            raspisilvioHelpDraw(state);
            abodDrawHeads();
            break;
        case 7:
            raspisilvioDrawTexture(state, mask_tex_id);
            break;
        case 8:
            raspisilvioCameraMask(state, mask_tex_id, FRAMBE_BUFFER_PREVIEW);
            break;
        case 9:
            raspisilvioProcessingCamera(raspisilvioGetPreviewShader(), state, hsi_fbo_id);
            raspisilvioTextureMask(state, mask_tex_id, FRAMBE_BUFFER_PREVIEW, hsi_tex_id);
            break;
        case 10:
            raspisilvioProcessingCamera(&gauss_hsi_shader, state, hsi_fbo_id);
            abodDrawMaskX();
            break;
        case 11:
            raspisilvioProcessingCamera(&gauss_hsi_shader, state, hsi_fbo_id);
            abodDrawMaskX();
            int w, h;
            abodExtractReference(state, &w, &h);
            raspisilvioSetTextureData(trap_tex_id, w, h, pixels_from_fb, GL_RGBA);
            raspisilvioDrawTexture(state, trap_tex_id);
            break;
        case 12:
            raspisilvioProcessingCamera(&gauss_hsi_shader, state, hsi_fbo_id);
            raspisilvioTextureMask(state, mask_tex_id, FRAMBE_BUFFER_PREVIEW, hsi_tex_id);
            break;

        case 13:
            raspisilvioProcessingCameraMask(&gauss_hsi_shader_mask, state, mask_tex_id, FRAMBE_BUFFER_PREVIEW);
            break;

        case 14:
            raspisilvioProcessingCameraMask(&gauss_hsi_shader_mask, state, mask_tex_id, hsi_fbo_id);
            abodExtractReference(state, &w, &h);
            raspisilvioSetTextureData(trap_tex_id, w, h, pixels_from_fb, GL_RGBA);
            raspisilvioDrawTexture(state, trap_tex_id);
            break;

        case 15:
            raspisilvioProcessingCamera(&gauss_hsi_shader, state, hsi_fbo_id);
            abodDrawMaskX();
            abodExtractReference(state, &w, &h);
            abodBuildHistogram(w, h);
            raspisilvioSetTextureData(trap_tex_id, w, h, pixels_from_fb, GL_RGBA);
            raspisilvioDrawTexture(state, trap_tex_id);
            break;
        case 16:
//            raspisilvioProcessingCamera(&gauss_hsi_shader, state, hsi_fbo_id);
            raspisilvioProcessingTexture(&gauss_hsi_shader_tex, state, hsi_fbo_id, file_tex_id);
            abodDrawMaskX();
            abodExtractReference(state, &w, &h);
            abodBuildHistogram(w, h);
            raspisilvioWriteHistToTexture(&hue_hist, pixels_hist_h);
            raspisilvioWriteHistToTexture(&intensity_hist, pixels_hist_i);
            raspisilvioSetTextureData(h_hist_tex_id, 255 / hue_hist.bin_width + 1, 2, pixels_hist_h, GL_RGBA);
            raspisilvioSetTextureData(i_hist_tex_id, 255 / intensity_hist.bin_width + 1, 2, pixels_hist_i, GL_RGBA);
            abodMatchHistogram();
            break;
        case 17:
            raspisilvioProcessingTexture(&gauss_hsi_shader_tex, state, hsi_fbo_id, file_tex_id);
            glReadPixels(0, 0, state->width, state->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels_from_fb_full);
            abodDrawMaskX();
            abodExtractReference(state, &w, &h);
            abodBuildHistogram(w, h);
            abodMatchHistogramCPU(state);
            raspisilvioSetTextureData(trap_tex_id, state->width, state->height, pixels_from_fb_full, GL_RGBA);
            raspisilvioDrawTexture(state, trap_tex_id);
            break;
        case 18:
            raspisilvioDrawTexture(state, test_tex_id);
            break;
        case 19:
            abodRunTest();
            break;
        case 20:
            abodRunTest2();
            break;
        case 21:
            abodRunTest3();
            break;
        case 22:
            buildHistogramTrap(state, RASPISILVIO_RED);
            break;
        case 23:
            buildHistogramTrap(state, RASPISILVIO_BLUE);
            break;
        case 24:
            raspisilvioDrawTexture(state, file_tex_id);
            break;
        case 25:
            raspisilvioProcessingTexture(&gauss_hsi_shader_tex, state, FRAMBE_BUFFER_PREVIEW, file_tex_id);
            break;
        case 26:
            step26(state);
            break;
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int step26(RASPITEX_STATE *state) {
    static int a = 0;
    raspisilvioProcessingTexture(&gauss_hsi_shader_tex, state, hsi_fbo_id, file_tex_id);
    raspisilvioBuildHistogram(histogram_h_fbo_id, hsi_tex_id, ABOD_HISTOGRAM_SIZE, abod_trap_vbo,
                              ABOD_TRAP_POINTS, RASPISILVIO_RED);

    GLCHK(glReadPixels(0, 1, ABOD_HISTOGRAM_SIZE, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels_from_fb));

    int i = 0;

    uint8_t *out = pixels_from_fb;
    uint8_t *end = out + 4 * ABOD_HISTOGRAM_SIZE * 1;

    if (a == 0) {
        printf("*********\n");
        while (out < end) {
            if (out[RASPISILVIO_RED]) {
                printf("[%d] = %d\n", i, out[RASPISILVIO_RED]);
            }
            out += 4;
            i++;
        }
        printf("\n*********\n");
        a = 1;
    }

    raspisilvioBuildHistogram(histogram_i_fbo_id, hsi_tex_id, ABOD_HISTOGRAM_SIZE, abod_trap_vbo,
                              ABOD_TRAP_POINTS, RASPISILVIO_BLUE);

    GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, FRAMBE_BUFFER_PREVIEW));
    glViewport(0, 0, state->width, state->height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLCHK(glUseProgram(hist_compare_shader.program));
    GLCHK(glUniform1i(hist_compare_shader.uniform_locations[0], 0)); // Texture unit
    GLCHK(glUniform1i(hist_compare_shader.uniform_locations[1], 1));
    GLCHK(glUniform1i(hist_compare_shader.uniform_locations[2], 2));
    GLCHK(glUniform2f(hist_compare_shader.uniform_locations[3], 0.01f, 0.0f)); // threshold

    GLCHK(glActiveTexture(GL_TEXTURE0));
    GLCHK(glBindTexture(GL_TEXTURE_2D, hsi_tex_id));
    GLCHK(glActiveTexture(GL_TEXTURE1));
    GLCHK(glBindTexture(GL_TEXTURE_2D, histogram_h_tex));
    GLCHK(glActiveTexture(GL_TEXTURE2));
    GLCHK(glBindTexture(GL_TEXTURE_2D, histogram_i_tex));
    GLCHK(glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad()));
    GLCHK(glEnableVertexAttribArray(hist_compare_shader.attribute_locations[0]));
    GLCHK(glVertexAttribPointer(hist_compare_shader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
    GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));
    glFlush();
    glFinish();

    return 0;

}

///////////////////////////////////////////////////////////////////////////////////////////////////
int buildHistogramTrap(RASPITEX_STATE *state, GLuint channel) {
    static int a = 0;
    raspisilvioProcessingCamera(&gauss_hsi_shader, state, hsi_fbo_id);
    raspisilvioBuildHistogram(FRAMBE_BUFFER_PREVIEW, hsi_tex_id, ABOD_HISTOGRAM_SIZE, abod_trap_vbo,
                              ABOD_TRAP_POINTS, channel);

    GLCHK(glReadPixels(0, 1, ABOD_HISTOGRAM_SIZE, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels_from_fb));

    int i = 0;

    uint8_t *out = pixels_from_fb;
    uint8_t *end = out + 4 * ABOD_HISTOGRAM_SIZE * 1;

    if (a == 0) {
        printf("*********\n");
        while (out < end) {
            if (out[channel]) {
                printf("[%d] = %d\n", i, out[channel]);
            }
            out += 4;
            i++;
        }
        printf("\n*********\n");
        a = 1;
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int abodRunTest3() {
    static int a = 0;

    raspisilvioBuildHistogram(FRAMBE_BUFFER_PREVIEW, test_tex_id, 10, test_vbo, TEST_WIDTH * TEST_HEIGHT,
                              RASPISILVIO_BLUE);

    GLCHK(glReadPixels(0, 1, 10, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels_from_fb));

    int i = 0;

    uint8_t *out = pixels_from_fb;
    uint8_t *end = out + 4 * 10 * 1;

    if (a == 0) {
        printf("*********\n");
        while (out < end) {
            if (out[0]) {
                printf("[%d] = %d\n", i, out[0]);
            }
            out += 4;
            i++;
        }
        printf("\n*********\n");
        a = 1;
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int abodRunTest2() {
    static int a = 0;
    glBlendFunc(GL_ONE, GL_ONE);
    glEnable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, 4, 3);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(test_shader.program);
    glUniform1i(test_shader.uniform_locations[0], 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, test_tex_id);
    glBindBuffer(GL_ARRAY_BUFFER, test2_vbo);
    glEnableVertexAttribArray(test_shader.attribute_locations[0]);
    glVertexAttribPointer(test_shader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_POINTS, 0, TEST_POINTS);
    glFlush();
    glFinish();

    GLCHK(glReadPixels(0, 1, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels_from_fb));

    uint8_t *out = pixels_from_fb;
    uint8_t *end = out + 4 * 4 * 1;

    if (a == 0) {
        printf("*********\n");
        while (out < end) {
            printf("%d,", out[0]);
            out += 4;
        }
        printf("\n*********\n");
        a = 1;
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int abodRunTest() {
    static int a = 0;
    glBlendFunc(GL_ONE, GL_ONE);
    glEnable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, 4, 3);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(test_shader.program);
    glUniform1i(test_shader.uniform_locations[0], 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, test_tex_id);
    glBindBuffer(GL_ARRAY_BUFFER, test_vbo);
    glEnableVertexAttribArray(test_shader.attribute_locations[0]);
    glVertexAttribPointer(test_shader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_POINTS, 0, TEST_WIDTH * TEST_HEIGHT);
    glFlush();
    glFinish();

    GLCHK(glReadPixels(0, 1, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels_from_fb));

    uint8_t *out = pixels_from_fb;
    uint8_t *end = out + 4 * 4 * 1;

    if (a == 0) {
        printf("*********\n");
        while (out < end) {
            printf("%d,", out[0]);
//            printf("%d,", out[1]);
//            printf("%d,", out[2]);
//            printf("%d,", out[3]);
            out += 4;
        }
        printf("\n*********\n");
        a = 1;
    }

    return 0;
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
        else {
            out[0] = 255;
            out[1] = 0;
            out[2] = 0;
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
