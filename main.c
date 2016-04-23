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
int edgeInit(RASPITEX_STATE *raspitex_state);

int edgeDraw(RASPITEX_STATE *raspitex_state);

///////////////////////////////////////////////////////////////////////////////////////////////////
int RENDER_STEPS;

int render_id = 0;
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
    RENDER_STEPS = 4;
    static char *render_steps[] = {
            "0 - > Camera",
            "2 - > Sobel",
            "2 - > Sobel X",
            "3 - > Sobel Y",
            "4 - > Sobel MXY"
    };
    printf("Render id = %d\n%s\n", render_id, render_steps[render_id]);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int edgeInit(RASPITEX_STATE *raspitex_state) {
    int rc = 0;

    rc = raspisilvioHelpInit(raspitex_state);

    raspisilvioLoadShader(&sobel_shader);
    raspisilvioLoadShader(&sobelX_shader);
    raspisilvioLoadShader(&sobelY_shader);
    raspisilvioLoadShader(&sobelMXY_shader);

    return rc;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
int edgeDraw(RASPITEX_STATE *state) {
    static int a = -1;
    static int c = 0;

    if (a != render_id) {
        edgePrintStep();
        a = render_id;
    }

    switch (render_id) {
        case 0:
            raspisilvioDrawCameraY(state);
            break;
        case 1:
            raspisilvioProcessingCameraY(&sobel_shader, state, FRAMBE_BUFFER_PREVIEW);
            break;
        case 2:
            raspisilvioProcessingCameraY(&sobelX_shader, state, FRAMBE_BUFFER_PREVIEW);
            break;
        case 3:
            raspisilvioProcessingCameraY(&sobelY_shader, state, FRAMBE_BUFFER_PREVIEW);
            break;
        case 4:
            raspisilvioProcessingCameraY(&sobelMXY_shader, state, FRAMBE_BUFFER_PREVIEW);
            break;
    }

    return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////