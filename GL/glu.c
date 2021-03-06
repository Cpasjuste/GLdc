#include <math.h>
#include "private.h"

/* Set the Perspective */
void APIENTRY gluPerspective(GLfloat angle, GLfloat aspect,
                    GLfloat znear, GLfloat zfar) {
    GLdouble fW, fH;

    fH = tan(angle / 360 * F_PI) * znear;
    fW = fH * aspect;

    glFrustum(-fW, fW, -fH, fH, znear, zfar);
}

void APIENTRY gluOrtho2D(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top) {
    glOrtho(left, right, bottom, top, -1.0f, 1.0f);
}
