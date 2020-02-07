#include <string.h>

#include <stdio.h>
#include <dc/fmath.h>
#include <dc/matrix.h>
#include <dc/matrix3d.h>
#include <dc/vec3f.h>

#include "private.h"
#include "../include/gl.h"
#include "../containers/stack.h"

#define DEG2RAD (0.01745329251994329576923690768489)

/* Viewport mapping */
static GLfloat gl_viewport_scale[3], gl_viewport_offset[3];

/* Depth range */
GLfloat DEPTH_RANGE_MULTIPLIER_L = (1 - 0) / 2;
GLfloat DEPTH_RANGE_MULTIPLIER_H = (0 + 1) / 2;

/* Viewport size */
static GLint gl_viewport_x1, gl_viewport_y1, gl_viewport_width, gl_viewport_height;

static Stack MATRIX_STACKS[3]; // modelview, projection, texture
static Matrix4x4 NORMAL_MATRIX __attribute__((aligned(32)));
static Matrix4x4 SCREENVIEW_MATRIX __attribute__((aligned(32)));

static GLenum MATRIX_MODE = GL_MODELVIEW;
static GLubyte MATRIX_IDX = 0;

static const Matrix4x4 IDENTITY = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

GLfloat NEAR_PLANE_DISTANCE = 0.0f;

static void _glStoreNearPlane() {
    Matrix4x4* proj = (Matrix4x4*) stack_top(MATRIX_STACKS + (GL_PROJECTION & 0xF));

    GLfloat a = *(*proj + 10);
    GLfloat b = *(*proj + 14);

    NEAR_PLANE_DISTANCE = -b / (1.0f - a);
}

void APIENTRY glDepthRange(GLclampf n, GLclampf f);

static inline void upload_matrix(Matrix4x4* m) {
    mat_load((matrix_t*) m);
}

static inline void multiply_matrix(Matrix4x4* m) {
    mat_apply((matrix_t*) m);
}

static inline void download_matrix(Matrix4x4* m) {
    mat_store((matrix_t*) m);
}

Matrix4x4* _glGetProjectionMatrix() {
    return (Matrix4x4*) stack_top(&MATRIX_STACKS[1]);
}

Matrix4x4* _glGetModelViewMatrix() {
    return (Matrix4x4*) stack_top(&MATRIX_STACKS[0]);
}

void _glInitMatrices() {
    init_stack(&MATRIX_STACKS[0], sizeof(Matrix4x4), 32);
    init_stack(&MATRIX_STACKS[1], sizeof(Matrix4x4), 32);
    init_stack(&MATRIX_STACKS[2], sizeof(Matrix4x4), 32);

    stack_push(&MATRIX_STACKS[0], IDENTITY);
    stack_push(&MATRIX_STACKS[1], IDENTITY);
    stack_push(&MATRIX_STACKS[2], IDENTITY);

    memcpy(NORMAL_MATRIX, IDENTITY, sizeof(Matrix4x4));
    memcpy(SCREENVIEW_MATRIX, IDENTITY, sizeof(Matrix4x4));

    glDepthRange(0.0f, 1.0f);
    glViewport(0, 0, vid_mode->width, vid_mode->height);
}

#define swap(a, b) { \
    GLfloat x = (a); \
    a = b; \
    b = x; \
}

static void inverse(GLfloat* m) {
    GLfloat f4 = m[4];
    GLfloat f8 = m[8];
    GLfloat f1 = m[1];
    GLfloat f9 = m[9];
    GLfloat f2 = m[2];
    GLfloat f6 = m[6];
    GLfloat f12 = m[12];
    GLfloat f13 = m[13];
    GLfloat f14 = m[14];

    m[1]  =  f4;
    m[2]  =  f8;
    m[4]  =  f1;
    m[6]  =  f9;
    m[8]  =  f2;
    m[9]  =  f6;
    m[12] = -(f12 * m[0]  +  f13 * m[4]  +  f14 * m[8]);
    m[13] = -(f12 * m[1]  +  f13 * m[5]  +  f14 * m[9]);
    m[14] = -(f12 * m[2]  +  f13 * m[6]  +  f14 * m[10]);
}

static void transpose(GLfloat* m) {
    swap(m[1], m[4]);
    swap(m[2], m[8]);
    swap(m[3], m[12]);
    swap(m[6], m[9]);
    swap(m[7], m[3]);
    swap(m[11], m[14]);
}

static void recalculateNormalMatrix() {
    memcpy(NORMAL_MATRIX, stack_top(MATRIX_STACKS + (GL_MODELVIEW & 0xF)), sizeof(Matrix4x4));
    inverse((GLfloat*) NORMAL_MATRIX);
    transpose((GLfloat*) NORMAL_MATRIX);
}

void APIENTRY glMatrixMode(GLenum mode) {
    MATRIX_MODE = mode;
    MATRIX_IDX = mode & 0xF;
}

void APIENTRY glPushMatrix() {
    stack_push(MATRIX_STACKS + MATRIX_IDX, stack_top(MATRIX_STACKS + MATRIX_IDX));
}

void APIENTRY glPopMatrix() {
    stack_pop(MATRIX_STACKS + MATRIX_IDX);
    if(MATRIX_MODE == GL_MODELVIEW) {
        recalculateNormalMatrix();
    }
}

void APIENTRY glLoadIdentity() {
    stack_replace(MATRIX_STACKS + MATRIX_IDX, IDENTITY);
}

void APIENTRY glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    static Matrix4x4 trn __attribute__((aligned(32))) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    trn[M12] = x;
    trn[M13] = y;
    trn[M14] = z;

    upload_matrix(stack_top(MATRIX_STACKS + MATRIX_IDX));
    multiply_matrix(&trn);
    download_matrix(stack_top(MATRIX_STACKS + MATRIX_IDX));

    if(MATRIX_MODE == GL_MODELVIEW) {
        recalculateNormalMatrix();
    }
}


void APIENTRY glScalef(GLfloat x, GLfloat y, GLfloat z) {
    static Matrix4x4 scale __attribute__((aligned(32))) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    scale[M0] = x;
    scale[M5] = y;
    scale[M10] = z;

    upload_matrix(stack_top(MATRIX_STACKS + MATRIX_IDX));
    multiply_matrix(&scale);
    download_matrix(stack_top(MATRIX_STACKS + MATRIX_IDX));

    if(MATRIX_MODE == GL_MODELVIEW) {
        recalculateNormalMatrix();
    }
}

void APIENTRY glRotatef(GLfloat angle, GLfloat x, GLfloat  y, GLfloat z) {
    static Matrix4x4 rotate __attribute__((aligned(32))) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    float r = DEG2RAD * angle;

    float c = cos(r);
    float s = sin(r);
    float invc = 1.0f - c;
    float xs = x * s;
    float zs = z * s;
    float ys = y * s;
    float xz = x * z;
    float xy = y * x;
    float yz = y * z;

    vec3f_normalize(x, y, z);

    rotate[M0] = (x * x) * invc + c;
    rotate[M1] = xy * invc + zs;
    rotate[M2] = xz * invc - ys;

    rotate[M4] = xy * invc - zs;
    rotate[M5] = (y * y) * invc + c;
    rotate[M6] = yz * invc + xs;

    rotate[M8] = xz * invc + ys;
    rotate[M9] = yz * invc - xs;
    rotate[M10] = (z * z) * invc + c;

    upload_matrix(stack_top(MATRIX_STACKS + MATRIX_IDX));
    multiply_matrix(&rotate);
    download_matrix(stack_top(MATRIX_STACKS + MATRIX_IDX));

    if(MATRIX_MODE == GL_MODELVIEW) {
        recalculateNormalMatrix();
    }
}

/* Load an arbitrary matrix */
void APIENTRY glLoadMatrixf(const GLfloat *m) {
    static Matrix4x4 TEMP;

    TEMP[M0] = m[0];
    TEMP[M1] = m[1];
    TEMP[M2] = m[2];
    TEMP[M3] = m[3];

    TEMP[M4] = m[4];
    TEMP[M5] = m[5];
    TEMP[M6] = m[6];
    TEMP[M7] = m[7];

    TEMP[M8] = m[8];
    TEMP[M9] = m[9];
    TEMP[M10] = m[10];
    TEMP[M11] = m[11];

    TEMP[M12] = m[12];
    TEMP[M13] = m[13];
    TEMP[M14] = m[14];
    TEMP[M15] = m[15];

    stack_replace(MATRIX_STACKS + MATRIX_IDX, TEMP);

    if(MATRIX_MODE == GL_MODELVIEW) {
        recalculateNormalMatrix();
    }
}

/* Ortho */
void APIENTRY glOrtho(GLfloat left, GLfloat right,
             GLfloat bottom, GLfloat top,
             GLfloat znear, GLfloat zfar) {

    /* Ortho Matrix */
    static Matrix4x4 OrthoMatrix __attribute__((aligned(32))) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    OrthoMatrix[M0] = 2.0f / (right - left);
    OrthoMatrix[M5] = 2.0f / (top - bottom);
    OrthoMatrix[M10] = -2.0f / (zfar - znear);
    OrthoMatrix[M12] = -(right + left) / (right - left);
    OrthoMatrix[M13] = -(top + bottom) / (top - bottom);
    OrthoMatrix[M14] = -(zfar + znear) / (zfar - znear);

    upload_matrix(stack_top(MATRIX_STACKS + MATRIX_IDX));
    multiply_matrix(&OrthoMatrix);
    download_matrix(stack_top(MATRIX_STACKS + MATRIX_IDX));
}


/* Set the GL frustum */
void APIENTRY glFrustum(GLfloat left, GLfloat right,
               GLfloat bottom, GLfloat top,
               GLfloat znear, GLfloat zfar) {

    /* Frustum Matrix */
    static Matrix4x4 FrustumMatrix __attribute__((aligned(32)));

    memset(FrustumMatrix, 0, sizeof(float) * 16);

    const float near2 = 2.0f * znear;
    const float A = (right + left) / (right - left);
    const float B = (top + bottom) / (top - bottom);
    const float C = -((zfar + znear) / (zfar - znear));
    const float D = -((2.0f * zfar * znear) / (zfar - znear));

    FrustumMatrix[M0] = near2 / (right - left);
    FrustumMatrix[M5] = near2 / (top - bottom);

    FrustumMatrix[M8] = A;
    FrustumMatrix[M9] = B;
    FrustumMatrix[M10] = C;
    FrustumMatrix[M11] = -1.0f;
    FrustumMatrix[M14] = D;

    upload_matrix(stack_top(MATRIX_STACKS + MATRIX_IDX));
    multiply_matrix(&FrustumMatrix);
    download_matrix(stack_top(MATRIX_STACKS + MATRIX_IDX));

    if(MATRIX_MODE == GL_PROJECTION) {
        _glStoreNearPlane();
    }
}


/* Multiply the current matrix by an arbitrary matrix */
void glMultMatrixf(const GLfloat *m) {
    static Matrix4x4 TEMP;

    TEMP[M0] = m[0];
    TEMP[M1] = m[1];
    TEMP[M2] = m[2];
    TEMP[M3] = m[3];

    TEMP[M4] = m[4];
    TEMP[M5] = m[5];
    TEMP[M6] = m[6];
    TEMP[M7] = m[7];

    TEMP[M8] = m[8];
    TEMP[M9] = m[9];
    TEMP[M10] = m[10];
    TEMP[M11] = m[11];

    TEMP[M12] = m[12];
    TEMP[M13] = m[13];
    TEMP[M14] = m[14];
    TEMP[M15] = m[15];

    upload_matrix(stack_top(MATRIX_STACKS + MATRIX_IDX));
    multiply_matrix((Matrix4x4*) &TEMP);
    download_matrix(stack_top(MATRIX_STACKS + MATRIX_IDX));

    if(MATRIX_MODE == GL_MODELVIEW) {
        recalculateNormalMatrix();
    }

    if(MATRIX_MODE == GL_PROJECTION) {
        _glStoreNearPlane();
    }
}

/* Load an arbitrary transposed matrix */
void glLoadTransposeMatrixf(const GLfloat *m) {
    /* We store matrices transpose anyway, so m will be
     * transpose compared to all other matrices */

    static Matrix4x4 TEMP __attribute__((aligned(32)));

    TEMP[M0] = m[0];
    TEMP[M1] = m[4];
    TEMP[M2] = m[8];
    TEMP[M3] = m[12];

    TEMP[M4] = m[1];
    TEMP[M5] = m[5];
    TEMP[M6] = m[9];
    TEMP[M7] = m[13];

    TEMP[M8] = m[3];
    TEMP[M9] = m[6];
    TEMP[M10] = m[10];
    TEMP[M11] = m[14];

    TEMP[M12] = m[4];
    TEMP[M13] = m[7];
    TEMP[M14] = m[11];
    TEMP[M15] = m[15];

    stack_replace(MATRIX_STACKS + MATRIX_IDX, TEMP);

    if(MATRIX_MODE == GL_MODELVIEW) {
        recalculateNormalMatrix();
    }

    if(MATRIX_MODE == GL_PROJECTION) {
        _glStoreNearPlane();
    }
}

/* Multiply the current matrix by an arbitrary transposed matrix */
void glMultTransposeMatrixf(const GLfloat *m) {
    static Matrix4x4 TEMP __attribute__((aligned(32)));

    TEMP[M0] = m[0];
    TEMP[M1] = m[4];
    TEMP[M2] = m[8];
    TEMP[M3] = m[12];

    TEMP[M4] = m[1];
    TEMP[M5] = m[5];
    TEMP[M6] = m[9];
    TEMP[M7] = m[13];

    TEMP[M8] = m[3];
    TEMP[M9] = m[6];
    TEMP[M10] = m[10];
    TEMP[M11] = m[14];

    TEMP[M12] = m[4];
    TEMP[M13] = m[7];
    TEMP[M14] = m[11];
    TEMP[M15] = m[15];

    upload_matrix(stack_top(MATRIX_STACKS + MATRIX_IDX));
    multiply_matrix(&TEMP);
    download_matrix(stack_top(MATRIX_STACKS + MATRIX_IDX));

    if(MATRIX_MODE == GL_MODELVIEW) {
        recalculateNormalMatrix();
    }

    if(MATRIX_MODE == GL_PROJECTION) {
        _glStoreNearPlane();
    }
}

/* Set the GL viewport */
void APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    gl_viewport_x1 = x;
    gl_viewport_y1 = y;
    gl_viewport_width = width;
    gl_viewport_height = height;

    GLfloat rw = x + width;
    GLfloat lw = x;
    GLfloat tw = y + height;
    GLfloat bw = y;

    GLfloat hw = ((GLfloat) width) / 2.0f;
    GLfloat hh = ((GLfloat) height) / 2.0f;

    SCREENVIEW_MATRIX[M0] = hw;
    SCREENVIEW_MATRIX[M5] = -hh;
    SCREENVIEW_MATRIX[M10] = 1;
    SCREENVIEW_MATRIX[M12] = (rw + lw) / 2.0f;
    SCREENVIEW_MATRIX[M13] = (tw + bw) / 2.0f;
}

GLfloat _glGetNearPlane() {
    return NEAR_PLANE_DISTANCE;
}

/* Set the depth range */
void APIENTRY glDepthRange(GLclampf n, GLclampf f) {
    if(n < 0.0f) n = 0.0f;
    else if(n > 1.0f) n = 1.0f;

    if(f < 0.0f) f = 0.0f;
    else if(f > 1.0f) f = 1.0f;

    DEPTH_RANGE_MULTIPLIER_L = (f - n) / 2.0f;
    DEPTH_RANGE_MULTIPLIER_H = (n + f) / 2.0f;
}

/* Vector Cross Product - Used by glhLookAtf2 */
static inline void vec3f_cross(const GLfloat* v1, const GLfloat* v2, GLfloat* result) {
    result[0] = v1[1] * v2[2] - v1[2] * v2[1];
    result[1] = v1[2] * v2[0] - v1[0] * v2[2];
    result[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

/* glhLookAtf2 adapted from http://www.opengl.org/wiki/GluLookAt_code */
void glhLookAtf2(const GLfloat* eyePosition3D,
                 const GLfloat* center3D,
                 const GLfloat* upVector3D) {
#if 0
    /* Look-At Matrix */
    static Matrix4x4 MatrixLookAt __attribute__((aligned(32))) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    GLfloat forward[3];
    GLfloat side[3];
    GLfloat up[3];

    vec3f_sub_normalize(center3D[0], center3D[1], center3D[2],
                        eyePosition3D[0], eyePosition3D[1], eyePosition3D[2],
                        forward[0], forward[1], forward[2]);

    //Side = forward x up
    vec3f_cross(forward, upVector3D, side);
    vec3f_normalize(side[0], side[1], side[2]);

    //Recompute up as: up = side x forward
    vec3f_cross(side, forward, up);

    MatrixLookAt[M0] = side[0];
    MatrixLookAt[M4] = side[1];
    MatrixLookAt[M8] = side[2];
    MatrixLookAt[M12] = 0;

    MatrixLookAt[M1] = up[0];
    MatrixLookAt[M5] = up[1];
    MatrixLookAt[M9] = up[2];
    MatrixLookAt[M13] = 0;

    MatrixLookAt[M2] = -forward[0];
    MatrixLookAt[M6] = -forward[1];
    MatrixLookAt[M10] = -forward[2];
    MatrixLookAt[M14] = 0;

    MatrixLookAt[M3] = MatrixLookAt[11] = MatrixLookAt[15] = 0;
    MatrixLookAt[M15] = 1;

    static Matrix4x4 trn __attribute__((aligned(32))) = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    trn[M12] = -eyePosition3D[0];
    trn[M13] = -eyePosition3D[1];
    trn[M14] = -eyePosition3D[2];

    // Does not modify internal Modelview matrix
    upload_matrix(&MatrixLookAt);
    multiply_matrix(&trn);
    multiply_matrix(stack_top(MATRIX_STACKS + (GL_MODELVIEW & 0xF)));
    download_matrix(stack_top(MATRIX_STACKS + (GL_MODELVIEW & 0xF)));
#endif
}

void gluLookAt(GLfloat eyex, GLfloat eyey, GLfloat eyez, GLfloat centerx,
               GLfloat centery, GLfloat centerz, GLfloat upx, GLfloat upy,
               GLfloat upz) {
    GLfloat eye [] = { eyex, eyey, eyez };
    GLfloat point [] = { centerx, centery, centerz };
    GLfloat up [] = { upx, upy, upz };
    glhLookAtf2(eye, point, up);
}

void _glApplyRenderMatrix() {
    upload_matrix(&SCREENVIEW_MATRIX);
    multiply_matrix(stack_top(MATRIX_STACKS + (GL_PROJECTION & 0xF)));
    multiply_matrix(stack_top(MATRIX_STACKS + (GL_MODELVIEW & 0xF)));
}

void _glMatrixLoadTexture() {
    upload_matrix(stack_top(MATRIX_STACKS + (GL_TEXTURE & 0xF)));
}

void _glMatrixLoadModelView() {
    upload_matrix(stack_top(MATRIX_STACKS + (GL_MODELVIEW & 0xF)));
}

void _glMatrixLoadNormal() {
    upload_matrix(&NORMAL_MATRIX);
}
