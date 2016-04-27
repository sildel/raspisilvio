///////////////////////////////////////////////////////////////////////////////////////////////////
// Created by
// Jorge Silvio Delgado Morales
// based on raspicam applications of https://github.com/raspberrypi/userland
// silviodelgado70@gmail.com
///////////////////////////////////////////////////////////////////////////////////////////////////
#include "RaspiTexUtil.h"
#include "raspisilvio.h"
#include <GLES2/gl2.h>
#include <highgui.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
int edgeInit(RASPITEX_STATE *raspitex_state);

int edgeDraw(RASPITEX_STATE *raspitex_state);

void edgeLineDetect(RASPITEX_STATE *state);

///////////////////////////////////////////////////////////////////////////////////////////////////
int RENDER_STEPS;

int render_id = 0;
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram edgeLineShader = {
        .vs_file = "gl_scenes/edgeLineVS.glsl",
        .fs_file = "gl_scenes/histogramFS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram testshader_t = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/simpleFSTTT.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram sobelMDS_shader_t = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/sobelMDSFST.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "tex_unit"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram sobelMXY_shader = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/sobelMXYFS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "tex_unit"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram sobelMXY_shader_t = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/sobelMXYFST.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "tex_unit"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram sobel_shader_t = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/sobelFST.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "tex_unit"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram sobel_shader = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/sobelFS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "tex_unit"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram sobelX_shader = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/sobelXFS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "tex_unit"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram sobelX_shader_t = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/sobelXFST.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "tex_unit"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram sobelY_shader = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/sobelYFS.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "tex_unit"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
RaspisilvioShaderProgram sobelY_shader_t = {
        .vs_file = "gl_scenes/simpleVS.glsl",
        .fs_file = "gl_scenes/sobelYFST.glsl",
        .vertex_source = "",
        .fragment_source = "",
        .uniform_names =
                {"tex", "tex_unit"},
        .attribute_names =
                {"vertex"},
};
///////////////////////////////////////////////////////////////////////////////////////////////////
GLuint sobelFBId;

GLuint sobelTexId;

GLsizei binsAngle = 300;

GLsizei binsDistance = 300;

GLfloat *edgeVertex;

GLuint edgeVBO;

GLuint yImageTexId;

GLuint houghTexId;

GLuint houghFBId;

///////////////////////////////////////////////////////////////////////////////////////////////////
int main() {
    int exit_code;

    RaspisilvioApplication edge;
    raspisilvioGetDefault(&edge);
    edge.init = edgeInit;
    edge.draw = edgeDraw;
    edge.useRGBATexture = 0;
    edge.useYTexture = 1;

    exit_code = raspisilvioInitApp(&edge);
    if (exit_code != EX_OK) {
        return exit_code;
    }

    exit_code = raspisilvioStart(&edge);
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

    raspisilvioStop(&edge);
    return exit_code;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void edgePrintStep() {
    RENDER_STEPS = 6;
    static char *render_steps[] = {
            "0 - > Camera",
            "2 - > Sobel",
            "2 - > Sobel X",
            "3 - > Sobel Y",
            "4 - > Sobel MXY",
            "5 - > Sobel MDS",
            "6 - > Hough"
    };
    printf("Render id = %d\n%s\n", render_id, render_steps[render_id]);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int edgeInit(RASPITEX_STATE *raspitex_state) {
    int rc = 0;

    rc = raspisilvioHelpInit(raspitex_state);

    raspisilvioLoadShader(&sobel_shader);
    raspisilvioLoadShader(&sobel_shader_t);
    raspisilvioLoadShader(&sobelX_shader);
    raspisilvioLoadShader(&sobelX_shader_t);
    raspisilvioLoadShader(&sobelY_shader);
    raspisilvioLoadShader(&sobelY_shader_t);
    raspisilvioLoadShader(&sobelMXY_shader);
    raspisilvioLoadShader(&sobelMXY_shader_t);
    raspisilvioLoadShader(&sobelMDS_shader_t);
    raspisilvioLoadShader(&edgeLineShader);

    raspisilvioLoadShader(&testshader_t);

    raspisilvioCreateTextureFB(&sobelTexId, raspitex_state->width, raspitex_state->height, NULL, GL_RGBA, &sobelFBId);
    raspisilvioCreateVertexBufferHistogram(&edgeVBO, raspitex_state->width, raspitex_state->height, &edgeVertex);
    raspisilvioLoadTextureFromFile("step0.tga", &yImageTexId);

    raspisilvioCreateTextureFB(&houghTexId, binsAngle, binsDistance, NULL, GL_RGBA, &houghFBId);

    return rc;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int edgeDraw(RASPITEX_STATE *state) {
    static int a = -1;
    static int c = 0, u = 0, v = 0, w = 0, x = 0;

    if (a != render_id) {
        edgePrintStep();
        a = render_id;
    }

    switch (render_id) {
        case 0:
            raspisilvioDrawTexture(state, yImageTexId);
            break;
        case 1:
            raspisilvioProcessingTexture(&sobel_shader_t, state, FRAMBE_BUFFER_PREVIEW, yImageTexId);
            if (c == 0) {
                c = 1;
                raspisilvioSaveToFile(state, "step1.tga");
            }
            break;
        case 2:
            raspisilvioProcessingTexture(&sobelX_shader_t, state, FRAMBE_BUFFER_PREVIEW, yImageTexId);
            if (u == 0) {
                u = 1;
                raspisilvioSaveToFile(state, "step2.tga");
            }
            break;
        case 3:
            raspisilvioProcessingTexture(&sobelY_shader_t, state, FRAMBE_BUFFER_PREVIEW, yImageTexId);
            if (v == 0) {
                v = 1;
                raspisilvioSaveToFile(state, "step3.tga");
            }
            break;
        case 4:
            raspisilvioProcessingTexture(&sobelMXY_shader_t, state, FRAMBE_BUFFER_PREVIEW, yImageTexId);
            if (w == 0) {
                w = 1;
                raspisilvioSaveToFile(state, "step4.tga");
            }
            break;
        case 5:
            raspisilvioProcessingTexture(&sobelMDS_shader_t, state, FRAMBE_BUFFER_PREVIEW, yImageTexId);
            break;
        case 6:
            edgeLineDetect(state);
            if (x == 0) {
                x = 1;
                raspisilvioSaveToFile(state, "step5.tga");
            }
            break;
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void edgeLineDetect(RASPITEX_STATE *state) {
    raspisilvioProcessingTexture(&sobelMDS_shader_t, state, sobelFBId, yImageTexId);

    glBlendFunc(GL_ONE, GL_ONE);
    glEnable(GL_BLEND);
    GLCHK(glBindFramebuffer(GL_FRAMEBUFFER, houghFBId));
    glViewport(0, 0, binsAngle, binsDistance);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLCHK(glUseProgram(edgeLineShader.program));
    GLCHK(glUniform1i(edgeLineShader.uniform_locations[0], 0)); // Texture unit
    GLCHK(glActiveTexture(GL_TEXTURE0));
    GLCHK(glBindTexture(GL_TEXTURE_2D, sobelTexId));
    GLCHK(glBindBuffer(GL_ARRAY_BUFFER, edgeVBO));
    GLCHK(glEnableVertexAttribArray(edgeLineShader.attribute_locations[0]));
    GLCHK(glVertexAttribPointer(edgeLineShader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, 0));
    GLCHK(glDrawArrays(GL_POINTS, 0, state->width * state->height));
    glFlush();
    glFinish();
    glDisable(GL_BLEND);

    raspisilvioDrawTexture(state, houghTexId);

}
///////////////////////////////////////////////////////////////////////////////////////////////////
