#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "../include/gl.h"
#include "../include/glext.h"
#include "private.h"
#include "profiler.h"


static AttribPointer VERTEX_POINTER;
static AttribPointer UV_POINTER;
static AttribPointer ST_POINTER;
static AttribPointer NORMAL_POINTER;
static AttribPointer DIFFUSE_POINTER;

static GLuint ENABLED_VERTEX_ATTRIBUTES = 0;
static GLubyte ACTIVE_CLIENT_TEXTURE = 0;

void _glInitAttributePointers() {
    TRACE();

    VERTEX_POINTER.ptr = NULL;
    VERTEX_POINTER.stride = 0;
    VERTEX_POINTER.type = GL_FLOAT;
    VERTEX_POINTER.size = 4;

    DIFFUSE_POINTER.ptr = NULL;
    DIFFUSE_POINTER.stride = 0;
    DIFFUSE_POINTER.type = GL_FLOAT;
    DIFFUSE_POINTER.size = 4;

    UV_POINTER.ptr = NULL;
    UV_POINTER.stride = 0;
    UV_POINTER.type = GL_FLOAT;
    UV_POINTER.size = 4;

    ST_POINTER.ptr = NULL;
    ST_POINTER.stride = 0;
    ST_POINTER.type = GL_FLOAT;
    ST_POINTER.size = 4;

    NORMAL_POINTER.ptr = NULL;
    NORMAL_POINTER.stride = 0;
    NORMAL_POINTER.type = GL_FLOAT;
    NORMAL_POINTER.size = 3;
}

static inline GLuint byte_size(GLenum type) {
    switch(type) {
    case GL_BYTE: return sizeof(GLbyte);
    case GL_UNSIGNED_BYTE: return sizeof(GLubyte);
    case GL_SHORT: return sizeof(GLshort);
    case GL_UNSIGNED_SHORT: return sizeof(GLushort);
    case GL_INT: return sizeof(GLint);
    case GL_UNSIGNED_INT: return sizeof(GLuint);
    case GL_DOUBLE: return sizeof(GLdouble);
    case GL_FLOAT:
    default: return sizeof(GLfloat);
    }
}

typedef void (*FloatParseFunc)(GLfloat* out, const GLubyte* in);
typedef void (*ByteParseFunc)(GLubyte* out, const GLubyte* in);
typedef void (*PolyBuildFunc)(ClipVertex* first, ClipVertex* previous, ClipVertex* vertex, ClipVertex* next, const GLsizei i);


static void _readVertexData3f3f(const float* input, GLuint count, GLubyte stride, float* output) {
    const float* end = (float*) (((GLubyte*) input) + (count * stride));

    while(input < end) {
        output[0] = input[0];
        output[1] = input[1];
        output[2] = input[2];

        input = (float*) (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(ClipVertex));
    }
}

static void _readVertexData3us3f(const GLushort* input, GLuint count, GLubyte stride, float* output) {
    const GLushort* end = (GLushort*) ((GLubyte*) input) + (count * stride);

    while(input < end) {
        output[0] = input[0];
        output[1] = input[1];
        output[2] = input[2];

        input = (GLushort*) (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(ClipVertex));
    }
}

static void _readVertexData3ui3f(const GLuint* input, GLuint count, GLubyte stride, float* output) {
    const GLuint* end = (GLuint*) ((GLubyte*) input) + (count * stride);

    while(input < end) {
        output[0] = input[0];
        output[1] = input[1];
        output[2] = input[2];

        input = (GLuint*) (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(ClipVertex));
    }
}

static void _readVertexData3ub3f(const GLubyte* input, GLuint count, GLubyte stride, float* output) {
    const float ONE_OVER_TWO_FIVE_FIVE = 1.0f / 255.0f;
    const GLubyte* end = ((GLubyte*) input) + (count * stride);

    while(input < end) {
        output[0] = input[0] * ONE_OVER_TWO_FIVE_FIVE;
        output[1] = input[1] * ONE_OVER_TWO_FIVE_FIVE;
        output[2] = input[2] * ONE_OVER_TWO_FIVE_FIVE;

        input += stride;
        output = (float*) (((GLubyte*) output) + sizeof(ClipVertex));
    }
}

static void _readVertexData2f2f(const float* input, GLuint count, GLubyte stride, float* output) {
    const float* end = (float*) ((GLubyte*) input) + (count * stride);

    while(input < end) {
        output[0] = input[0];
        output[1] = input[1];

        input = (float*) (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(ClipVertex));
    }
}

static void _readVertexData2f3f(const float* input, GLuint count, GLubyte stride, float* output) {
    const float* end = (float*) ((GLubyte*) input) + (count * stride);

    while(input < end) {
        output[0] = input[0];
        output[1] = input[1];
        output[2] = 0.0f;

        input = (float*) (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(ClipVertex));
    }
}

static void _readVertexData2ub3f(const GLubyte* input, GLuint count, GLubyte stride, float* output) {
    const float ONE_OVER_TWO_FIVE_FIVE = 1.0f / 255.0f;
    const GLubyte* end = ((GLubyte*) input) + (count * stride);

    while(input < end) {
        output[0] = input[0] * ONE_OVER_TWO_FIVE_FIVE;
        output[1] = input[1] * ONE_OVER_TWO_FIVE_FIVE;
        output[2] = 0.0f;

        input += stride;
        output = (float*) (((GLubyte*) output) + sizeof(ClipVertex));
    }
}

static void _readVertexData2us3f(const GLushort* input, GLuint count, GLubyte stride, float* output) {
    const GLushort* end = (GLushort*) ((GLubyte*) input) + (count * stride);

    while(input < end) {
        output[0] = input[0];
        output[1] = input[1];
        output[2] = 0.0f;

        input = (GLushort*) (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(ClipVertex));
    }
}

static void _readVertexData2us2f(const GLushort* input, GLuint count, GLubyte stride, float* output) {
    const GLushort* end = (GLushort*) ((GLubyte*) input) + (count * stride);

    while(input < end) {
        output[0] = input[0];
        output[1] = input[1];

        input = (GLushort*) (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(ClipVertex));
    }
}

static void _readVertexData2ui2f(const GLuint* input, GLuint count, GLubyte stride, float* output) {
    const GLuint* end = (GLuint*) ((GLubyte*) input) + (count * stride);

    while(input < end) {
        output[0] = input[0];
        output[1] = input[1];

        input = (GLuint*) (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(ClipVertex));
    }
}

static void _readVertexData2ub2f(const GLubyte* input, GLuint count, GLubyte stride, float* output) {
    const float ONE_OVER_TWO_FIVE_FIVE = 1.0f / 255.0f;
    const GLubyte* end = (GLubyte*) ((GLubyte*) input) + (count * stride);

    while(input < end) {
        output[0] = input[0] * ONE_OVER_TWO_FIVE_FIVE;
        output[1] = input[1] * ONE_OVER_TWO_FIVE_FIVE;

        input = (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(ClipVertex));
    }
}

static void _readVertexData2ui3f(const GLuint* input, GLuint count, GLubyte stride, float* output) {
    const GLuint* end = (GLuint*) ((GLubyte*) input) + (count * stride);

    while(input < end) {
        output[0] = input[0];
        output[1] = input[1];
        output[2] = 0.0f;

        input = (GLuint*) (((GLubyte*) input) + stride);
        output = (float*) (((GLubyte*) output) + sizeof(ClipVertex));
    }
}

static void _readVertexData4ubARGB(const GLubyte* input, GLuint count, GLubyte stride, GLubyte* output) {
    const GLubyte* end = ((GLubyte*) input) + (count * stride);

    while(input < end) {
        output[R8IDX] = input[0];
        output[G8IDX] = input[1];
        output[B8IDX] = input[2];
        output[A8IDX] = input[3];

        input = (GLubyte*) (((GLubyte*) input) + stride);
        output = (GLubyte*) (((GLubyte*) output) + sizeof(ClipVertex));
    }
}

static void _readVertexData4fARGB(const float* input, GLuint count, GLubyte stride, GLubyte* output) {
    const float* end = (float*) ((GLubyte*) input) + (count * stride);

    while(input < end) {
        output[R8IDX] = (GLubyte) (input[0] * 255.0f);
        output[G8IDX] = (GLubyte) (input[1] * 255.0f);
        output[B8IDX] = (GLubyte) (input[2] * 255.0f);
        output[A8IDX] = (GLubyte) (input[3] * 255.0f);

        input = (float*) (((GLubyte*) input) + stride);
        output = (GLubyte*) (((GLubyte*) output) + sizeof(ClipVertex));
    }
}

static void _readVertexData3fARGB(const float* input, GLuint count, GLubyte stride, GLubyte* output) {
    const float* end = (float*) ((GLubyte*) input) + (count * stride);

    while(input < end) {
        output[R8IDX] = (GLubyte) (input[0] * 255.0f);
        output[G8IDX] = (GLubyte) (input[1] * 255.0f);
        output[B8IDX] = (GLubyte) (input[2] * 255.0f);
        output[A8IDX] = 1.0f;

        input = (float*) (((GLubyte*) input) + stride);
        output = (GLubyte*) (((GLubyte*) output) + sizeof(ClipVertex));
    }
}

static void _readVertexData3ubARGB(const GLubyte* input, GLuint count, GLubyte stride, GLubyte* output) {
    const GLubyte* end = ((GLubyte*) input) + (count * stride);

    while(input < end) {
        output[R8IDX] = input[0];
        output[G8IDX] = input[1];
        output[B8IDX] = input[2];
        output[A8IDX] = 1.0f;

        input = (((GLubyte*) input) + stride);
        output = (GLubyte*) (((GLubyte*) output) + sizeof(ClipVertex));
    }
}

static void _fillWithNegZ(GLuint count, GLfloat* output) {
    const GLfloat* end = (GLfloat*) ((GLubyte*) output) + (sizeof(ClipVertex) * count);
    while(output < end) {
        output[0] = output[1] = 0.0f;
        output[2] = -1.0f;

        output += sizeof(ClipVertex);
    }
}

static void _fillWhiteARGB(GLuint count, GLubyte* output) {
    const GLubyte* end = output + (sizeof(ClipVertex) * count);

    while(output < end) {
        output[R8IDX] = 255;
        output[G8IDX] = 255;
        output[B8IDX] = 255;
        output[A8IDX] = 255;

        output += sizeof(ClipVertex);
    }
}

static void _fillZero2f(GLuint count, GLfloat* output) {
    const GLfloat* end = output + (sizeof(ClipVertex) * count);
    while(output < end) {
        output[0] = output[1] = 0.0f;
        output += sizeof(ClipVertex);
    }
}

static void _readVertexData3usARGB(const GLushort* input, GLuint count, GLubyte stride, GLubyte* output) {
    assert(0 && "Not Implemented");
}

static void _readVertexData3uiARGB(const GLuint* input, GLuint count, GLubyte stride, GLubyte* output) {
    assert(0 && "Not Implemented");
}

static void _readVertexData4usARGB(const GLushort* input, GLuint count, GLubyte stride, GLubyte* output) {
    assert(0 && "Not Implemented");
}

static void _readVertexData4uiARGB(const GLuint* input, GLuint count, GLubyte stride, GLubyte* output) {
    assert(0 && "Not Implemented");
}

GLuint _glGetEnabledAttributes() {
    return ENABLED_VERTEX_ATTRIBUTES;
}

AttribPointer* _glGetVertexAttribPointer() {
    return &VERTEX_POINTER;
}

AttribPointer* _glGetDiffuseAttribPointer() {
    return &DIFFUSE_POINTER;
}

AttribPointer* _glGetNormalAttribPointer() {
    return &NORMAL_POINTER;
}

AttribPointer* _glGetUVAttribPointer() {
    return &UV_POINTER;
}

AttribPointer* _glGetSTAttribPointer() {
    return &ST_POINTER;
}

typedef GLuint (*IndexParseFunc)(const GLubyte* in);

static inline GLuint _parseUByteIndex(const GLubyte* in) {
    return (GLuint) *in;
}

static inline GLuint _parseUIntIndex(const GLubyte* in) {
    return *((GLuint*) in);
}

static inline GLuint _parseUShortIndex(const GLubyte* in) {
    return *((GLshort*) in);
}


static inline IndexParseFunc _calcParseIndexFunc(GLenum type) {
    switch(type) {
    case GL_UNSIGNED_BYTE:
        return &_parseUByteIndex;
    break;
    case GL_UNSIGNED_INT:
        return &_parseUIntIndex;
    break;
    case GL_UNSIGNED_SHORT:
    default:
        break;
    }

    return &_parseUShortIndex;
}


/* There was a bug in this macro that shipped with Kos
 * which has now been fixed. But just in case...
 */
#undef mat_trans_single3_nodiv
#define mat_trans_single3_nodiv(x, y, z) { \
    register float __x __asm__("fr12") = (x); \
    register float __y __asm__("fr13") = (y); \
    register float __z __asm__("fr14") = (z); \
    __asm__ __volatile__( \
                          "fldi1 fr15\n" \
                          "ftrv  xmtrx, fv12\n" \
                          : "=f" (__x), "=f" (__y), "=f" (__z) \
                          : "0" (__x), "1" (__y), "2" (__z) \
                          : "fr15"); \
    x = __x; y = __y; z = __z; \
}


/* FIXME: Is this right? Shouldn't it be fr12->15? */
#undef mat_trans_normal3
#define mat_trans_normal3(x, y, z) { \
    register float __x __asm__("fr8") = (x); \
    register float __y __asm__("fr9") = (y); \
    register float __z __asm__("fr10") = (z); \
    __asm__ __volatile__( \
                          "fldi0 fr11\n" \
                          "ftrv  xmtrx, fv8\n" \
                          : "=f" (__x), "=f" (__y), "=f" (__z) \
                          : "0" (__x), "1" (__y), "2" (__z) \
                          : "fr11"); \
    x = __x; y = __y; z = __z; \
}


static inline void transformToEyeSpace(GLfloat* point) {
    _glMatrixLoadModelView();
    mat_trans_single3_nodiv(point[0], point[1], point[2]);
}

static inline void transformNormalToEyeSpace(GLfloat* normal) {
    _glMatrixLoadNormal();
    mat_trans_normal3(normal[0], normal[1], normal[2]);
}

#define INT_CAST(a) *((int*)&(a))
#define XORSWAP_UNSAFE(a, b)	((a)^=(b),(b)^=(a),(a)^=(b))
#define swapVertex(a, b)   \
do {                 \
    XORSWAP_UNSAFE(a->flags, b->flags); \
    XORSWAP_UNSAFE(INT_CAST(a->xyz[0]), INT_CAST(b->xyz[0])); \
    XORSWAP_UNSAFE(INT_CAST(a->xyz[1]), INT_CAST(b->xyz[1])); \
    XORSWAP_UNSAFE(INT_CAST(a->xyz[2]), INT_CAST(b->xyz[2])); \
    XORSWAP_UNSAFE(INT_CAST(a->uv[0]), INT_CAST(b->uv[0])); \
    XORSWAP_UNSAFE(INT_CAST(a->uv[1]), INT_CAST(b->uv[1])); \
    XORSWAP_UNSAFE(a->bgra[0], b->bgra[0]); \
    XORSWAP_UNSAFE(a->bgra[1], b->bgra[1]); \
    XORSWAP_UNSAFE(a->bgra[2], b->bgra[2]); \
    XORSWAP_UNSAFE(a->bgra[3], b->bgra[3]); \
    XORSWAP_UNSAFE(a->oargb, b->oargb); \
    XORSWAP_UNSAFE(INT_CAST(a->nxyz[0]), INT_CAST(b->nxyz[0])); \
    XORSWAP_UNSAFE(INT_CAST(a->nxyz[1]), INT_CAST(b->nxyz[1])); \
    XORSWAP_UNSAFE(INT_CAST(a->nxyz[2]), INT_CAST(b->nxyz[2])); \
    XORSWAP_UNSAFE(INT_CAST(a->w), INT_CAST(b->w)); \
    XORSWAP_UNSAFE(INT_CAST(a->st[0]), INT_CAST(b->st[0])); \
    XORSWAP_UNSAFE(INT_CAST(a->st[1]), INT_CAST(b->st[1])); \
} while(0)

static inline void genTriangles(ClipVertex* output, GLuint count) {
    const ClipVertex* end = output + count;
    ClipVertex* it = output + 2;
    while(it < end) {
        it->flags = PVR_CMD_VERTEX_EOL;
        it += 3;
    }
}

static inline void genQuads(ClipVertex* output, GLuint count) {
    ClipVertex* previous;
    ClipVertex* this = output + 3;

    const ClipVertex* end = output + count;

    while(this < end) {
        previous = this - 1;
        swapVertex(previous, this);
        this->flags = PVR_CMD_VERTEX_EOL;
        this += 4;
    }
}

static void genTriangleStrip(ClipVertex* output, GLuint count) {
    output[count - 1].flags = PVR_CMD_VERTEX_EOL;
}

#define MAX_POLYGON_SIZE 32

static void genTriangleFan(ClipVertex* output, GLuint count) {
    assert(count < MAX_POLYGON_SIZE);
    static ClipVertex buffer[MAX_POLYGON_SIZE];

    if(count <= 3){
        swapVertex(&output[1], &output[2]);
        output[2].flags = PVR_CMD_VERTEX_EOL;
        return;
    }

    memcpy(buffer, output, sizeof(ClipVertex) * count);

    // First 3 vertices are in the right place, just end early
    output[2].flags = PVR_CMD_VERTEX_EOL;

    GLsizei i = 3, target = 3;
    ClipVertex* first = &output[0];

    for(; i < count; ++i) {
        output[target++] = *first;
        output[target++] = buffer[i - 1];
        output[target] = buffer[i];
        output[target++].flags = PVR_CMD_VERTEX_EOL;
    }
}

static inline void _readPositionData(const GLuint first, const GLuint count, ClipVertex* output) {
    const GLubyte vstride = (VERTEX_POINTER.stride) ? VERTEX_POINTER.stride : VERTEX_POINTER.size * byte_size(VERTEX_POINTER.type);
    const void* vptr = ((GLubyte*) VERTEX_POINTER.ptr + (first * vstride));

    if(VERTEX_POINTER.size == 3) {
        switch(VERTEX_POINTER.type) {
            case GL_FLOAT:
                _readVertexData3f3f(vptr, count, vstride, output[0].xyz);
            break;
            case GL_BYTE:
            case GL_UNSIGNED_BYTE:
                _readVertexData3ub3f(vptr, count, vstride, output[0].xyz);
            break;
            case GL_SHORT:
            case GL_UNSIGNED_SHORT:
                _readVertexData3us3f(vptr, count, vstride, output[0].xyz);
            break;
            case GL_INT:
            case GL_UNSIGNED_INT:
                _readVertexData3ui3f(vptr, count, vstride, output[0].xyz);
            break;
        default:
            assert(0 && "Not Implemented");
        }
    } else if(VERTEX_POINTER.size == 2) {
        switch(VERTEX_POINTER.type) {
            case GL_FLOAT:
                _readVertexData2f3f(vptr, count, vstride, output[0].xyz);
            break;
            case GL_BYTE:
            case GL_UNSIGNED_BYTE:
                _readVertexData2ub3f(vptr, count, vstride, output[0].xyz);
            break;
            case GL_SHORT:
            case GL_UNSIGNED_SHORT:
                _readVertexData2us3f(vptr, count, vstride, output[0].xyz);
            break;
            case GL_INT:
            case GL_UNSIGNED_INT:
                _readVertexData2ui3f(vptr, count, vstride, output[0].xyz);
            break;
        default:
            assert(0 && "Not Implemented");
        }
    } else {
        assert(0 && "Not Implemented");
    }
}

static inline void _readUVData(const GLuint first, const GLuint count, ClipVertex* output) {
    if((ENABLED_VERTEX_ATTRIBUTES & UV_ENABLED_FLAG) != UV_ENABLED_FLAG) {
        _fillZero2f(count, output->uv);
        return;
    }

    const GLubyte uvstride = (UV_POINTER.stride) ? UV_POINTER.stride : UV_POINTER.size * byte_size(UV_POINTER.type);
    const void* uvptr = ((GLubyte*) UV_POINTER.ptr + (first * uvstride));

    if(UV_POINTER.size == 2) {
        switch(UV_POINTER.type) {
            case GL_FLOAT:
                _readVertexData2f2f(uvptr, count, uvstride, output[0].uv);
            break;
            case GL_BYTE:
            case GL_UNSIGNED_BYTE:
                _readVertexData2ub2f(uvptr, count, uvstride, output[0].uv);
            break;
            case GL_SHORT:
            case GL_UNSIGNED_SHORT:
                _readVertexData2us2f(uvptr, count, uvstride, output[0].uv);
            break;
            case GL_INT:
            case GL_UNSIGNED_INT:
                _readVertexData2ui2f(uvptr, count, uvstride, output[0].uv);
            break;
        default:
            assert(0 && "Not Implemented");
        }
    } else {
        assert(0 && "Not Implemented");
    }
}

static inline void _readSTData(const GLuint first, const GLuint count, ClipVertex* output) {
    if((ENABLED_VERTEX_ATTRIBUTES & ST_ENABLED_FLAG) != ST_ENABLED_FLAG) {
        _fillZero2f(count, output->st);
        return;
    }

    const GLubyte ststride = (ST_POINTER.stride) ? ST_POINTER.stride : ST_POINTER.size * byte_size(ST_POINTER.type);
    const void* stptr = ((GLubyte*) ST_POINTER.ptr + (first * ststride));

    if(ST_POINTER.size == 2) {
        switch(ST_POINTER.type) {
            case GL_FLOAT:
                _readVertexData2f2f(stptr, count, ststride, output[0].st);
            break;
            case GL_BYTE:
            case GL_UNSIGNED_BYTE:
                _readVertexData2ub2f(stptr, count, ststride, output[0].st);
            break;
            case GL_SHORT:
            case GL_UNSIGNED_SHORT:
                _readVertexData2us2f(stptr, count, ststride, output[0].st);
            break;
            case GL_INT:
            case GL_UNSIGNED_INT:
                _readVertexData2ui2f(stptr, count, ststride, output[0].st);
            break;
        default:
            assert(0 && "Not Implemented");
        }
    } else {
        assert(0 && "Not Implemented");
    }
}

static inline void _readNormalData(const GLuint first, const GLuint count, ClipVertex* output) {
    if((ENABLED_VERTEX_ATTRIBUTES & NORMAL_ENABLED_FLAG) != NORMAL_ENABLED_FLAG) {
        _fillWithNegZ(count, output->nxyz);
        return;
    }

    const GLuint nstride = (NORMAL_POINTER.stride) ? NORMAL_POINTER.stride : NORMAL_POINTER.size * byte_size(NORMAL_POINTER.type);
    const void* nptr = ((GLubyte*) NORMAL_POINTER.ptr + (first * nstride));

    if(NORMAL_POINTER.size == 3) {
        switch(NORMAL_POINTER.type) {
            case GL_FLOAT:
                _readVertexData3f3f(nptr, count, nstride, output[0].nxyz);
            break;
            case GL_BYTE:
            case GL_UNSIGNED_BYTE:
                _readVertexData3ub3f(nptr, count, nstride, output[0].nxyz);
            break;
            case GL_SHORT:
            case GL_UNSIGNED_SHORT:
                _readVertexData3us3f(nptr, count, nstride, output[0].nxyz);
            break;
            case GL_INT:
            case GL_UNSIGNED_INT:
                _readVertexData3ui3f(nptr, count, nstride, output[0].nxyz);
            break;
        default:
            assert(0 && "Not Implemented");
        }
    } else {
        assert(0 && "Not Implemented");
    }
}

static inline void _readDiffuseData(const GLuint first, const GLuint count, ClipVertex* output) {
    if((ENABLED_VERTEX_ATTRIBUTES & DIFFUSE_ENABLED_FLAG) != DIFFUSE_ENABLED_FLAG) {
        /* Just fill the whole thing white if the attribute is disabled */
        _fillWhiteARGB(count, output[0].bgra);
        return;
    }

    const GLubyte cstride = (DIFFUSE_POINTER.stride) ? DIFFUSE_POINTER.stride : DIFFUSE_POINTER.size * byte_size(DIFFUSE_POINTER.type);
    const void* cptr = ((GLubyte*) DIFFUSE_POINTER.ptr + (first * cstride));

    if(DIFFUSE_POINTER.size == 3) {
        switch(DIFFUSE_POINTER.type) {
            case GL_FLOAT:
                _readVertexData3fARGB(cptr, count, cstride, output[0].bgra);
            break;
            case GL_BYTE:
            case GL_UNSIGNED_BYTE:
                _readVertexData3ubARGB(cptr, count, cstride, output[0].bgra);
            break;
            case GL_SHORT:
            case GL_UNSIGNED_SHORT:
                _readVertexData3usARGB(cptr, count, cstride, output[0].bgra);
            break;
            case GL_INT:
            case GL_UNSIGNED_INT:
                _readVertexData3uiARGB(cptr, count, cstride, output[0].bgra);
            break;
        default:
            assert(0 && "Not Implemented");
        }
    } else if(DIFFUSE_POINTER.size == 4) {
        switch(DIFFUSE_POINTER.type) {
            case GL_FLOAT:
                _readVertexData4fARGB(cptr, count, cstride, output[0].bgra);
            break;
            case GL_BYTE:
            case GL_UNSIGNED_BYTE:
                _readVertexData4ubARGB(cptr, count, cstride, output[0].bgra);
            break;
            case GL_SHORT:
            case GL_UNSIGNED_SHORT:
                _readVertexData4usARGB(cptr, count, cstride, output[0].bgra);
            break;
            case GL_INT:
            case GL_UNSIGNED_INT:
                _readVertexData4uiARGB(cptr, count, cstride, output[0].bgra);
            break;
        default:
            assert(0 && "Not Implemented");
        }
    } else {
        assert(0 && "Not Implemented");
    }
}

static void generate(ClipVertex* output, const GLenum mode, const GLsizei first, const GLuint count,
        const GLubyte* indices, const GLenum type, const GLboolean doTexture, const GLboolean doMultitexture, const GLboolean doLighting) {
    /* Read from the client buffers and generate an array of ClipVertices */

    const GLsizei istride = byte_size(type);
    ClipVertex* it;
    const ClipVertex* end;

    if(!indices) {
        _readPositionData(first, count, output);
        _readDiffuseData(first, count, output);
        if(doTexture) _readUVData(first, count, output);
        if(doLighting) _readNormalData(first, count, output);
        if(doTexture && doMultitexture) _readSTData(first, count, output);

        it = output;
        end = output + count;
        while(it < end) {
            (it++)->flags = PVR_CMD_VERTEX;
        }

        // Drawing arrays
        switch(mode) {
        case GL_TRIANGLES:
            genTriangles(output, count);
            break;
        case GL_QUADS:
            genQuads(output, count);
            break;
        case GL_POLYGON:
        case GL_TRIANGLE_FAN:
            genTriangleFan(output, count);
            break;
        case GL_TRIANGLE_STRIP:
            genTriangleStrip(output, count);
            break;
        default:
            assert(0 && "Not Implemented");
        }
    } else {
        const IndexParseFunc indexFunc = _calcParseIndexFunc(type);
        it = output;
        end = output + count;
        GLuint j;
        const GLubyte* idx = indices;
        while(it < end) {
            j = indexFunc(idx);
            _readPositionData(j, 1, it);
            _readDiffuseData(j, 1, it);
            if(doTexture) _readUVData(j, 1, it);
            if(doLighting) _readNormalData(j, 1, it);
            if(doTexture && doMultitexture) _readSTData(j, 1, it);
            ++it;
            idx += istride;
        }

        it = output;
        while(it < end) {
            (it++)->flags = PVR_CMD_VERTEX;
        }

        // Drawing arrays
        switch(mode) {
        case GL_TRIANGLES:
            genTriangles(output, count);
            break;
        case GL_QUADS:
            genQuads(output, count);
            break;
        case GL_POLYGON:
        case GL_TRIANGLE_FAN:
            genTriangleFan(output, count);
            break;
        case GL_TRIANGLE_STRIP:
            genTriangleStrip(output, count);
            break;
        default:
            assert(0 && "Not Implemented");
        }
    }
}

static void transform(ClipVertex* output, const GLuint count) {
    /* Perform modelview transform, storing W */

    ClipVertex* vertex = output;

    _glApplyRenderMatrix(); /* Apply the Render Matrix Stack */

    GLsizei i = count;
    while(i--) {
        register float __x __asm__("fr12") = (vertex->xyz[0]);
        register float __y __asm__("fr13") = (vertex->xyz[1]);
        register float __z __asm__("fr14") = (vertex->xyz[2]);
        register float __w __asm__("fr15");

        __asm__ __volatile__(
            "fldi1 fr15\n"
            "ftrv   xmtrx,fv12\n"
            : "=f" (__x), "=f" (__y), "=f" (__z), "=f" (__w)
            : "0" (__x), "1" (__y), "2" (__z), "3" (__w)
        );

        vertex->xyz[0] = __x;
        vertex->xyz[1] = __y;
        vertex->xyz[2] = __z;
        vertex->w = __w;

        ++vertex;
    }
}

static GLsizei clip(AlignedVector* polylist, uint32_t offset, const GLuint count) {
    /* Perform clipping, generating new vertices as necessary */
    clipTriangleStrip2(polylist, offset, _glGetShadeModel() == GL_FLAT);

    /* List size, minus the original offset (which includes the header), minus the header */
    return polylist->size - offset - 1;
}

static void mat_transform3(const float* xyz, const float* xyzOut, const uint32_t count, const uint32_t inStride, const uint32_t outStride) {
    uint8_t* dataIn = (uint8_t*) xyz;
    uint8_t* dataOut = (uint8_t*) xyzOut;
    uint32_t i = count;

    while(i--) {
        float* in = (float*) dataIn;
        float* out = (float*) dataOut;

        mat_trans_single3_nodiv_nomod(in[0], in[1], in[2], out[0], out[1], out[2]);

        dataIn += inStride;
        dataOut += outStride;
    }
}

static void mat_transform_normal3(const float* xyz, const float* xyzOut, const uint32_t count, const uint32_t inStride, const uint32_t outStride) {
    uint8_t* dataIn = (uint8_t*) xyz;
    uint8_t* dataOut = (uint8_t*) xyzOut;
    uint32_t i = count;

    while(i--) {
        float* in = (float*) dataIn;
        float* out = (float*) dataOut;

        mat_trans_normal3_nomod(in[0], in[1], in[2], out[0], out[1], out[2]);

        dataIn += inStride;
        dataOut += outStride;
    }
}

static void light(ClipVertex* output, const GLuint count) {
    if(!_glIsLightingEnabled()) {
        return;
    }

    typedef struct {
        float xyz[3];
        float n[3];
    } EyeSpaceData;

    static AlignedVector* eye_space_data = NULL;

    if(!eye_space_data) {
        eye_space_data = (AlignedVector*) malloc(sizeof(AlignedVector));
        aligned_vector_init(eye_space_data, sizeof(EyeSpaceData));
    }

    aligned_vector_resize(eye_space_data, count);

    /* Perform lighting calculations and manipulate the colour */
    ClipVertex* vertex = output;
    EyeSpaceData* eye_space = (EyeSpaceData*) eye_space_data->data;

    _glMatrixLoadModelView();
    mat_transform3(vertex->xyz, eye_space->xyz, count, sizeof(ClipVertex), sizeof(EyeSpaceData));

    _glMatrixLoadNormal();
    mat_transform_normal3(vertex->nxyz, eye_space->n, count, sizeof(ClipVertex), sizeof(EyeSpaceData));

    GLsizei i;
    EyeSpaceData* ES = aligned_vector_at(eye_space_data, 0);

    for(i = 0; i < count; ++i, ++vertex, ++ES) {
        /* We ignore diffuse colour when lighting is enabled. If GL_COLOR_MATERIAL is enabled
         * then the lighting calculation should possibly take it into account */

        GLfloat total [] = {0.0f, 0.0f, 0.0f, 0.0f};
        GLfloat to_add [] = {0.0f, 0.0f, 0.0f, 0.0f};
        GLubyte j;
        for(j = 0; j < MAX_LIGHTS; ++j) {
            if(_glIsLightEnabled(j)) {
                _glCalculateLightingContribution(j, ES->xyz, ES->n, vertex->bgra, to_add);

                total[0] += to_add[0];
                total[1] += to_add[1];
                total[2] += to_add[2];
                total[3] += to_add[3];
            }
        }

        vertex->bgra[A8IDX] = (GLubyte) (255.0f * fminf(total[3], 1.0f));
        vertex->bgra[R8IDX] = (GLubyte) (255.0f * fminf(total[0], 1.0f));
        vertex->bgra[G8IDX] = (GLubyte) (255.0f * fminf(total[1], 1.0f));
        vertex->bgra[B8IDX] = (GLubyte) (255.0f * fminf(total[2], 1.0f));
    }
}

static void divide(ClipVertex* output, const GLuint count) {
    /* Perform perspective divide on each vertex */
    ClipVertex* vertex = output;

    GLsizei i = count;
    while(i--) {
        vertex->xyz[2] = 1.0f / vertex->w;
        vertex->xyz[0] *= vertex->xyz[2];
        vertex->xyz[1] *= vertex->xyz[2];
        ++vertex;
    }
}

static void push(PVRHeader* header, ClipVertex* output, const GLuint count, PolyList* activePolyList, GLshort textureUnit) {
    // Compile the header
    pvr_poly_cxt_t cxt = *_glGetPVRContext();
    cxt.list_type = activePolyList->list_type;

    _glUpdatePVRTextureContext(&cxt, textureUnit);

    pvr_poly_compile(&header->hdr, &cxt);

    /* Post-process the vertex list */
    ClipVertex* vout = output;

    GLuint i = count;
    while(i--) {
        vout->oargb = 0;
    }
}

#define DEBUG_CLIPPING 0

static void submitVertices(GLenum mode, GLsizei first, GLuint count, GLenum type, const GLvoid* indices) {
    /* Do nothing if vertices aren't enabled */
    if(!(ENABLED_VERTEX_ATTRIBUTES & VERTEX_ENABLED_FLAG)) {
        return;
    }

    GLboolean doMultitexture, doTexture, doLighting;
    GLint activeTexture;
    glGetIntegerv(GL_ACTIVE_TEXTURE_ARB, &activeTexture);

    glActiveTextureARB(GL_TEXTURE0);
    glGetBooleanv(GL_TEXTURE_2D, &doTexture);

    glActiveTextureARB(GL_TEXTURE1);
    glGetBooleanv(GL_TEXTURE_2D, &doMultitexture);

    doLighting = _glIsLightingEnabled();

    glActiveTextureARB(activeTexture);

    profiler_push(__func__);


    PolyList* activeList = _glActivePolyList();

    /* Make room in the list buffer */
    GLsizei spaceNeeded = (mode == GL_POLYGON || mode == GL_TRIANGLE_FAN) ? ((count - 2) * 3) : count;
    ClipVertex* start = aligned_vector_extend(&activeList->vector, spaceNeeded + 1);

    /* Store a pointer to the header for later */
    PVRHeader* header = (PVRHeader*) start++;

    /* We store an offset to the first ClipVertex because clipping may generate more
     * vertices, which may cause a realloc and thus invalidate start and header
     * we use this startOffset to reset those pointers after clipping */
    uint32_t startOffset = start - (ClipVertex*) activeList->vector.data;

    profiler_checkpoint("allocate");

    generate(start, mode, first, count, (GLubyte*) indices, type, doTexture, doMultitexture, doLighting);

    profiler_checkpoint("generate");

    light(start, spaceNeeded);

    profiler_checkpoint("light");

    transform(start, spaceNeeded);

    profiler_checkpoint("transform");

    if(_glIsClippingEnabled()) {

        uint32_t offset = ((start - 1) - (ClipVertex*) activeList->vector.data);

#if DEBUG_CLIPPING
        uint32_t i = 0;
        fprintf(stderr, "=========\n");

        for(i = offset; i < activeList->vector.size; ++i) {
            ClipVertex* v = aligned_vector_at(&activeList->vector, i);
            if(v->flags == 0xe0000000 || v->flags == 0xf0000000) {
                fprintf(stderr, "(%f, %f, %f) -> %x\n", v->xyz[0], v->xyz[1], v->xyz[2], v->flags);
            } else {
                fprintf(stderr, "%x\n", *((uint32_t*)v));
            }
        }
#endif

        spaceNeeded = clip(&activeList->vector, offset, spaceNeeded);

        /* Clipping may have realloc'd so reset the start pointer */
        start = ((ClipVertex*) activeList->vector.data) + startOffset;
        header = (PVRHeader*) (start - 1);  /* Update the header pointer */

#if DEBUG_CLIPPING
        fprintf(stderr, "--------\n");
        for(i = offset; i < activeList->vector.size; ++i) {
            ClipVertex* v = aligned_vector_at(&activeList->vector, i);
            if(v->flags == 0xe0000000 || v->flags == 0xf0000000) {
                fprintf(stderr, "(%f, %f, %f) -> %x\n", v->xyz[0], v->xyz[1], v->xyz[2], v->flags);
            } else {
                fprintf(stderr, "%x\n", *((uint32_t*)v));
            }
        }
#endif

    }

    profiler_checkpoint("clip");

    divide(start, spaceNeeded);

    profiler_checkpoint("divide");

    push(header, start, spaceNeeded, _glActivePolyList(), 0);

    profiler_checkpoint("push");
    /*
       Now, if multitexturing is enabled, we want to send exactly the same vertices again, except:
       - We want to enable blending, and send them to the TR list
       - We want to set the depth func to GL_EQUAL
       - We want to set the second texture ID
       - We want to set the uv coordinates to the passed st ones
    */

    if(!doMultitexture) {
        /* Multitexture actively disabled */
        profiler_pop();
        return;
    }

    TextureObject* texture1 = _glGetTexture1();

    if(!texture1 || ((ENABLED_VERTEX_ATTRIBUTES & ST_ENABLED_FLAG) != ST_ENABLED_FLAG)) {
        /* Multitexture implicitly disabled */
        profiler_pop();
        return;
    }

    /* Push back a copy of the list to the transparent poly list, including the header
        (hence the - 1)
    */
    ClipVertex* vertex = aligned_vector_push_back(
        &_glTransparentPolyList()->vector, start - 1, spaceNeeded + 1
    );

    PVRHeader* mtHeader = (PVRHeader*) vertex++;
    ClipVertex* mtStart = vertex;

    /* Copy ST coordinates to UV ones */
    GLsizei i = spaceNeeded;
    while(i--) {
        vertex->uv[0] = vertex->st[0];
        vertex->uv[1] = vertex->st[1];
        ++vertex;
    }

    /* Store state, as we're about to mess around with it */
    GLint depthFunc, blendSrc, blendDst;
    glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
    glGetIntegerv(GL_BLEND_SRC, &blendSrc);
    glGetIntegerv(GL_BLEND_DST, &blendDst);

    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);

    glDepthFunc(GL_EQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Send the buffer again to the transparent list */
    push(mtHeader, mtStart, spaceNeeded, _glTransparentPolyList(), 1);

    /* Reset state */
    glDepthFunc(depthFunc);
    glBlendFunc(blendSrc, blendDst);
    (blendEnabled) ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
    (depthEnabled) ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
}

void APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices) {
    TRACE();

    if(_glCheckImmediateModeInactive(__func__)) {
        return;
    }

    submitVertices(mode, 0, count, type, indices);
}

void APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    TRACE();

    if(_glCheckImmediateModeInactive(__func__)) {
        return;
    }

    submitVertices(mode, first, count, GL_UNSIGNED_SHORT, NULL);
}

void APIENTRY glEnableClientState(GLenum cap) {
    TRACE();

    switch(cap) {
    case GL_VERTEX_ARRAY:
        ENABLED_VERTEX_ATTRIBUTES |= VERTEX_ENABLED_FLAG;
    break;
    case GL_COLOR_ARRAY:
        ENABLED_VERTEX_ATTRIBUTES |= DIFFUSE_ENABLED_FLAG;
    break;
    case GL_NORMAL_ARRAY:
        ENABLED_VERTEX_ATTRIBUTES |= NORMAL_ENABLED_FLAG;
    break;
    case GL_TEXTURE_COORD_ARRAY:
        (ACTIVE_CLIENT_TEXTURE) ?
            (ENABLED_VERTEX_ATTRIBUTES |= ST_ENABLED_FLAG):
            (ENABLED_VERTEX_ATTRIBUTES |= UV_ENABLED_FLAG);
    break;
    default:
        _glKosThrowError(GL_INVALID_ENUM, "glEnableClientState");
    }
}

void APIENTRY glDisableClientState(GLenum cap) {
    TRACE();

    switch(cap) {
    case GL_VERTEX_ARRAY:
        ENABLED_VERTEX_ATTRIBUTES &= ~VERTEX_ENABLED_FLAG;
    break;
    case GL_COLOR_ARRAY:
        ENABLED_VERTEX_ATTRIBUTES &= ~DIFFUSE_ENABLED_FLAG;
    break;
    case GL_NORMAL_ARRAY:
        ENABLED_VERTEX_ATTRIBUTES &= ~NORMAL_ENABLED_FLAG;
    break;
    case GL_TEXTURE_COORD_ARRAY:
        (ACTIVE_CLIENT_TEXTURE) ?
            (ENABLED_VERTEX_ATTRIBUTES &= ~ST_ENABLED_FLAG):
            (ENABLED_VERTEX_ATTRIBUTES &= ~UV_ENABLED_FLAG);
    break;
    default:
        _glKosThrowError(GL_INVALID_ENUM, "glDisableClientState");
    }
}

GLuint _glGetActiveClientTexture() {
    return ACTIVE_CLIENT_TEXTURE;
}

void APIENTRY glClientActiveTextureARB(GLenum texture) {
    TRACE();

    if(texture < GL_TEXTURE0_ARB || texture > GL_TEXTURE0_ARB + MAX_TEXTURE_UNITS) {
        _glKosThrowError(GL_INVALID_ENUM, "glClientActiveTextureARB");
    }

    if(_glKosHasError()) {
        _glKosPrintError();
        return;
    }

    ACTIVE_CLIENT_TEXTURE = (texture == GL_TEXTURE1_ARB) ? 1 : 0;
}

void APIENTRY glTexCoordPointer(GLint size,  GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    AttribPointer* tointer = (ACTIVE_CLIENT_TEXTURE == 0) ? &UV_POINTER : &ST_POINTER;

    tointer->ptr = pointer;
    tointer->stride = stride;
    tointer->type = type;
    tointer->size = size;
}

void APIENTRY glVertexPointer(GLint size,  GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    VERTEX_POINTER.ptr = pointer;
    VERTEX_POINTER.stride = stride;
    VERTEX_POINTER.type = type;
    VERTEX_POINTER.size = size;
}

void APIENTRY glColorPointer(GLint size,  GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    DIFFUSE_POINTER.ptr = pointer;
    DIFFUSE_POINTER.stride = stride;
    DIFFUSE_POINTER.type = type;
    DIFFUSE_POINTER.size = size;
}

void APIENTRY glNormalPointer(GLenum type,  GLsizei stride,  const GLvoid * pointer) {
    TRACE();

    NORMAL_POINTER.ptr = pointer;
    NORMAL_POINTER.stride = stride;
    NORMAL_POINTER.type = type;
    NORMAL_POINTER.size = 3;
}
