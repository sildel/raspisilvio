///////////////////////////////////////////////////////////////////////////////////////////////////
// Created by
// Jorge Silvio Delgado Morales
// based on raspicam applications of userland.git in raspberrypi repo at github
// silviodelgado70@gmail.com
///////////////////////////////////////////////////////////////////////////////////////////////////
#include "raspisilvio.h"
#include <GLES2/gl2.h>
///////////////////////////////////////////////////////////////////////////////////////////////////
//int abodInit(RASPITEX_STATE* state);
//int abodDraw(RASPITEX_STATE* state);
///////////////////////////////////////////////////////////////////////////////////////////////////
int main() {
    int exit_code;

    RaspisilvioApplication abod;
    raspisilvioGetDefault(&abod);
//    abod.init = abodInit;
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

///**
// * Creates the OpenGL ES 2.X context and builds the shaders.
// * @param raspitex_state A pointer to the GL preview state.
// * @return Zero if successful.
// */
//int abodInit(RASPITEX_STATE *raspitex_state) {
//    raspisilvio_init();
//}
/////////////////////////////////////////////////////////////////////////////////////////////////////
//
///* Redraws the scene with the latest buffer.
// *
// * @param raspitex_state A pointer to the GL preview state.
// * @return Zero if successful.
// */
//int abodDraw(RASPITEX_STATE* state) {
//
//    raspisilvio_draw();
//}
/////////////////////////////////////////////////////////////////////////////////////////////////////
