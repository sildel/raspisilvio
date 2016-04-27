///////////////////////////////////////////////////////////////////////////////////////////////////
// Created by
// Jorge Silvio Delgado Morales
// based on raspicam applications of https://github.com/raspberrypi/userland
// silviodelgado70@gmail.com
///////////////////////////////////////////////////////////////////////////////////////////////////
extern "C" {
#include "RaspiTexUtil.h"
#include "raspisilvio.h"
#include "tga.h"
#include <GLES2/gl2.h>
}
///////////////////////////////////////////////////////////////////////////////////////////////////
#include <highgui.h>
#include <cv.h>
///////////////////////////////////////////////////////////////////////////////////////////////////
using namespace cv;

///////////////////////////////////////////////////////////////////////////////////////////////////
int edgeInit(RASPITEX_STATE *raspitex_state);

int edgeDraw(RASPITEX_STATE *raspitex_state);

void edgeLineDetect(RASPITEX_STATE *state);

void cpuAndGpu(RASPITEX_STATE *state);
void cpuAndGpuOnlyTransfer(RASPITEX_STATE *state);

///////////////////////////////////////////////////////////////////////////////////////////////////
int RENDER_STEPS;

int render_id = 6;
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

static const char *const fileToLoad = "./Experiments/Edge3/step0.tga";
Mat src, grad_x, grad_y, abs_grad_x, abs_grad_y, grad, grad_t, fromGPU, gpusobel;
vector<Vec2f> lines;
uint8_t *pixels_from_fb;

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
    RENDER_STEPS = 12;
    static char *render_steps[] = {
            "0 - > Camera",
            "1 - > Sobel",
            "2 - > Sobel X",
            "3 - > Sobel Y",
            "4 - > Sobel MXY",
            "5 - > Sobel MDS",
            "6 - > Hough",
            "7 - > CPU Sobel",
            "8 - > CPU Hough",
            "9 - > GPU Sobel + Threshold + Transfer CPU",
            "10- > GPU Sobel + Threshold + Transfer CPU & Hough",
            "11- > CPU Only Hough with result of CPU Sobel",
            "12- > CPU Only Hough with result of GPU Sobel",
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
    raspisilvioLoadTextureFromFile(fileToLoad, &yImageTexId);

    raspisilvioCreateTextureFB(&houghTexId, binsAngle, binsDistance, NULL, GL_RGBA, &houghFBId);

    src = imread("./Experiments/Edge3/step0.jpg", CV_LOAD_IMAGE_GRAYSCALE);
    fromGPU = Mat::zeros(Size(src.cols, src.rows), CV_8UC1);

    gpusobel = imread("./Experiments/Edge3/gpu.jpg");

    raspisilvioCreateTextureData(&pixels_from_fb, raspitex_state->width, raspitex_state->height, GL_RGBA);

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
        case 7:
            Sobel(src, grad_x, CV_16S, 1, 0, 3, 1, 0, BORDER_DEFAULT);
            convertScaleAbs(grad_x, abs_grad_x);
            Sobel(src, grad_y, CV_16S, 0, 1, 3, 1, 0, BORDER_DEFAULT);
            convertScaleAbs(grad_y, abs_grad_y);
            addWeighted(abs_grad_x, 0.5, abs_grad_y, 0.5, 0, grad);
            break;
        case 8:
            Sobel(src, grad_x, CV_16S, 1, 0, 3, 1, 0, BORDER_DEFAULT);
            convertScaleAbs(grad_x, abs_grad_x);
            Sobel(src, grad_y, CV_16S, 0, 1, 3, 1, 0, BORDER_DEFAULT);
            convertScaleAbs(grad_y, abs_grad_y);
            addWeighted(abs_grad_x, 0.5, abs_grad_y, 0.5, 0, grad);
            threshold(grad, grad_t, 20, 255, THRESH_BINARY);
            HoughLines(grad_t, lines, 1, CV_PI / 180, 200, 0, 0);
            break;
        case 9:
            cpuAndGpuOnlyTransfer(state);
            break;
        case 10:
            cpuAndGpu(state);
            break;
        case 11:
            HoughLines(grad_t, lines, 1, CV_PI / 180, 200, 0, 0);
            break;
        case 12:
            HoughLines(fromGPU, lines, 1, CV_PI / 180, 200, 0, 0);
            break;
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void cpuAndGpuOnlyTransfer(RASPITEX_STATE *state) {
    static int x = 0;
    raspisilvioProcessingTexture(&sobel_shader_t, state, FRAMBE_BUFFER_PREVIEW, yImageTexId);

    GLCHK(glReadPixels(0, 0, state->width, state->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels_from_fb));

    uint8_t *out = pixels_from_fb;

    for (int i = 0; i < src.rows; i++) {
        for (int j = 0; j < src.cols; j++) {

            if (out[0] > 100) {
                fromGPU.at<uchar>(i, j) = 255;
            }
            else {
                fromGPU.at<uchar>(i, j) = 0;
            }

            out += 4;
        }
    }

    if (x == 0) {
        imwrite("gpu.jpg", fromGPU);
        x = 1;
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void cpuAndGpu(RASPITEX_STATE *state) {
    static int x = 0;
    raspisilvioProcessingTexture(&sobel_shader_t, state, FRAMBE_BUFFER_PREVIEW, yImageTexId);

    GLCHK(glReadPixels(0, 0, state->width, state->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels_from_fb));

    uint8_t *out = pixels_from_fb;

    for (int i = 0; i < src.rows; i++) {
        for (int j = 0; j < src.cols; j++) {

            if (out[0] > 100) {
                fromGPU.at<uchar>(i, j) = 255;
            }
            else {
                fromGPU.at<uchar>(i, j) = 0;
            }

            out += 4;
        }
    }


    HoughLines(fromGPU, lines, 1, CV_PI / 180, 250, 0, 0);

    if (x == 0) {
        Mat final;
        final = imread("./Experiments/Edge3/step0t.jpg");
        for( size_t i = 0; i < lines.size(); i++ )
        {
            float rho = lines[i][0], theta = lines[i][1];
            Point pt1, pt2;
            double a = cos(theta), b = sin(theta);
            double x0 = a*rho, y0 = b*rho;
            pt1.x = cvRound(x0 + 1000*(-b));
            pt1.y = cvRound(y0 + 1000*(a));
            pt2.x = cvRound(x0 - 1000*(-b));
            pt2.y = cvRound(y0 - 1000*(a));
            line( final, pt1, pt2, Scalar(0,0,255), 3);
        }
        imwrite("final.jpg", final);
        x = 1;
    }
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
