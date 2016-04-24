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
#define ABOD_MAX_TRAP_POINTS (20000 * 10)
///////////////////////////////////////////////////////////////////////////////////////////////////
// Struct containing the points that describe the trapecius that represents the heading of the
// robot
typedef struct {
    float xb1, xb2;
    float xt1, xt2;
    float y1, y2;
} HeadingRegion;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Function declarations
int abodInit(RASPITEX_STATE *raspitex_state);

int abodDraw(RASPITEX_STATE *state);

int isInsideTrap(int row, int col, int32_t i, int32_t i1);

int isAtLeft(int x4, int y4, int x1, int y1, int x, int y);

int isAtRight(int x3, int y3, int x2, int y2, int x, int y);

void abodPrintStep();

int buildHistogramTrap(RASPITEX_STATE *state, int channel, GLuint hFBId, GLuint hTexId);

void abodFillTrapVertex(const int width, const int height);

int histogramCompare(RASPITEX_STATE *state);

void showHistogramPoints(RASPITEX_STATE *pSTATE);

///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram pointsShader = {
        .vs_file = "gl_scenes/pointsVS.glsl",
        .fs_file = "gl_scenes/pointsFS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram gaussHsiShaderTex = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/gaussian5hsiFS_t.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "tex_unit"},
        .attribute_names =
                {"vertex"},
};
GLuint hsiTexId;
GLuint hsiFBOId;
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram histCompareShader = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/histCompareFS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "hist_h", "hist_i", "threshold"},
        .attribute_names =
                {"vertex"},
};
int hThreshold = 100;
int iThreshold = 100;
///////////////////////////////////////////////////////////////////////////////////////////////////
int lastRenderStep;
int abodHistogramSize = 250;
static char *const fileToLoad = "ttt.tga";
int renderId = 0;
HeadingRegion heads = {
        .xb1 = -0.8f,
        .xb2 = 0.8f,
        .xt1 = -0.2f,
        .xt2 = 0.2f,
        .y1 = -0.5f,
        .y2 = 0.5f
};
uint8_t *pixelsFromFb;
GLuint histogramHTexId;
GLuint histogramHFBOId;
GLuint histogramITexId;
GLuint histogramIFBOId;
GLuint fileTexId;
GLuint abodTrapPoints = 0;
GLuint abodTrapVbo;
GLfloat abodTrapVertex[ABOD_MAX_TRAP_POINTS];

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

    while (1) {
        vcos_sleep(1000);

        fflush(stdin);
        ch = getchar();

        if (renderId < lastRenderStep) {
            renderId++;
        }
        else {
            renderId = 0;
        }
    }

    raspisilvioStop(&abod);
    return exit_code;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Function to write current step to console
void abodPrintStep() {
    lastRenderStep = 5;
    static char *render_steps[] = {
            "0- > File texture",
            "1- > File texture & Gauss->HSI",
            "2- > Points for Histogram",
            "3- > Histogram H",
            "4- > Histogram I",
            "5- > ABOD Result",
    };
    printf("Render id = %d\n%s\n", renderId, render_steps[renderId]);
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

    // Load the image to run the algorithm
    raspisilvioLoadTextureFromFile(fileToLoad, &fileTexId);

    // Load and compile the shader program that put points to the screen
    raspisilvioLoadShader(&pointsShader);

    // Load and compile the shader program that blurs the image and converts from RGB to HSV
    // Build a fbo and a texture to save the result
    raspisilvioLoadShader(&gaussHsiShaderTex);
    raspisilvioCreateTextureFB(&hsiTexId, raspitex_state->width, raspitex_state->height, NULL, GL_RGBA, &hsiFBOId);

    // Load and compile the shader program that looks to histogram value of each pixel to classify as a obstacle or not
    // Fill the array containing the points to build the histogram and create a vertex buffer with it
    // Build two fbo and two texture to save the result of both I and H histograms
    raspisilvioLoadShader(&histCompareShader);
    abodFillTrapVertex(raspitex_state->width, raspitex_state->height);
    raspisilvioCreateVertexBufferHistogramData(&abodTrapVbo, abodTrapPoints, abodTrapVertex);
    raspisilvioCreateTextureFB(&histogramITexId, abodHistogramSize, HISTOGRAM_TEX_HEIGHT, NULL, GL_RGBA,
                               &histogramIFBOId);
    raspisilvioCreateTextureFB(&histogramHTexId, abodHistogramSize, HISTOGRAM_TEX_HEIGHT, NULL, GL_RGBA,
                               &histogramHFBOId);

    // Allocate memory to read an entire frame buffer
    raspisilvioCreateTextureData(&pixelsFromFb, raspitex_state->width, raspitex_state->height, GL_RGBA);

    return rc;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void abodFillTrapVertex(const int width, const int height) {
    float dx = 1.0f / width;
    float dy = 1.0f / height;
    int x, y;
    abodTrapPoints = 0;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (isInsideTrap(y, x, width, height)) {
                abodTrapVertex[2 * abodTrapPoints + 0] = dx * x;
                abodTrapVertex[2 * abodTrapPoints + 1] = dy * y;
                abodTrapPoints++;
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

    if (a != renderId) {
        abodPrintStep();
        a = renderId;
    }

    switch (renderId) {
        case 0:
            raspisilvioDrawTexture(state, fileTexId);
            break;
        case 1:
            raspisilvioProcessingTexture(&gaussHsiShaderTex, state, FRAMBE_BUFFER_PREVIEW, fileTexId);
            break;
        case 2:
            showHistogramPoints(state);
            break;
        case 3:
            buildHistogramTrap(state, RASPISILVIO_RED, histogramHFBOId, histogramHTexId);
            break;
        case 4:
            buildHistogramTrap(state, RASPISILVIO_BLUE, histogramIFBOId, histogramITexId);
            break;
        case 5:
            histogramCompare(state);
            break;
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void showHistogramPoints(RASPITEX_STATE *state) {
    GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, FRAMBE_BUFFER_PREVIEW));
    glViewport(0, 0, state->width, state->height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLCHK(glUseProgram(pointsShader.program));
    GLCHK(glBindBuffer(GL_ARRAY_BUFFER, abodTrapVbo));
    GLCHK(glEnableVertexAttribArray(pointsShader.attribute_locations[0]));
    GLCHK(glVertexAttribPointer(pointsShader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
    GLCHK(glDrawArrays(GL_POINTS, 0, abodTrapPoints));
    glFlush();
    glFinish();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int histogramCompare(RASPITEX_STATE *state) {
    static int a = 0;
    raspisilvioProcessingTexture(&gaussHsiShaderTex, state, hsiFBOId, fileTexId);
    raspisilvioBuildHistogram(histogramHFBOId, hsiTexId, abodHistogramSize, abodTrapVbo,
                              abodTrapPoints, RASPISILVIO_RED);
    raspisilvioBuildHistogram(histogramIFBOId, hsiTexId, abodHistogramSize, abodTrapVbo,
                              abodTrapPoints, RASPISILVIO_BLUE);

    GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, FRAMBE_BUFFER_PREVIEW));
    glViewport(0, 0, state->width, state->height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLCHK(glUseProgram(histCompareShader.program));
    GLCHK(glUniform1i(histCompareShader.uniform_locations[0], 0)); // Texture unit
    GLCHK(glUniform1i(histCompareShader.uniform_locations[1], 1));
    GLCHK(glUniform1i(histCompareShader.uniform_locations[2], 2));
    GLCHK(glUniform2f(histCompareShader.uniform_locations[3], 0.01f, 0.0f)); // threshold

    GLCHK(glActiveTexture(GL_TEXTURE0));
    GLCHK(glBindTexture(GL_TEXTURE_2D, hsiTexId));
    GLCHK(glActiveTexture(GL_TEXTURE1));
    GLCHK(glBindTexture(GL_TEXTURE_2D, histogramHTexId));
    GLCHK(glActiveTexture(GL_TEXTURE2));
    GLCHK(glBindTexture(GL_TEXTURE_2D, histogramITexId));
    GLCHK(glBindBuffer(GL_ARRAY_BUFFER, raspisilvioGetQuad()));
    GLCHK(glEnableVertexAttribArray(histCompareShader.attribute_locations[0]));
    GLCHK(glVertexAttribPointer(histCompareShader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
    GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));
    glFlush();
    glFinish();

    return 0;

}

///////////////////////////////////////////////////////////////////////////////////////////////////
int buildHistogramTrap(RASPITEX_STATE *state, int channel, GLuint hFBId, GLuint hTexId) {
    static int a = 0;
    raspisilvioProcessingTexture(&gaussHsiShaderTex, state, hsiFBOId, fileTexId);
    raspisilvioBuildHistogram(hFBId, hsiTexId, abodHistogramSize, abodTrapVbo,
                              abodTrapPoints, channel);
    raspisilvioDrawdHistogram(state, hTexId, abodHistogramSize);

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
