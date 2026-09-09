// Ocean OpenGL — océano animado con cámara libre
//
// Build:
//   g++ -O2 -std=c++17 ocean.cpp -o ocean $(pkg-config --cflags --libs glfw3) -lz
//
// Run windowed (X11 o Wayland):
//   ./ocean
// Run headless (verification screenshot, requiere OSMesa):
//   ./ocean --headless --screenshot ocean.png [--time T]
//
// Controls:
//   W/A/S/D  forward/back/strafe        Q/E  down/up
//   Shift    boost x3                   Mouse L: look, M: pan
//   Wheel    move speed                 R reset camera, ESC quit
//   [ / ]    fish swim speed            Y/T  school depth (shallower/deeper)
//
// Waves: 5 Gerstner waves, displacement computed on the GPU (vertex shader),
// analytic normals in the fragment shader. Grid follows the camera (integer
// XZ) so the ocean feels infinite without any per-frame CPU upload.

// Don't let GLFW pull in <GL/gl.h> — we load GL functions ourselves.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <zlib.h>

// Minimal GL base types (no <GL/gl.h> — we load functions at runtime).
typedef unsigned int  GLenum;
typedef unsigned int  GLuint;
typedef int           GLint;
typedef int           GLsizei;
typedef unsigned int  GLbitfield;
typedef float         GLfloat;
typedef int           GLboolean;
typedef long          GLsizeiptr;
typedef int           GLclampf;
typedef char          GLchar;
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// GL 3.3 core — loaded at runtime via glfwGetProcAddress (works on X11,
// Wayland and EGL alike; no GLEW needed).
// ---------------------------------------------------------------------------
#define GL_ARRAY_BUFFER          0x8892
#define GL_ELEMENT_ARRAY_BUFFER  0x8893
#define GL_STATIC_DRAW           0x88E4
#define GL_VERTEX_SHADER         0x8B31
#define GL_FRAGMENT_SHADER       0x8B30
#define GL_COMPILE_STATUS        0x8B81
#define GL_LINK_STATUS           0x8B82
#define GL_DEPTH_TEST            0x0B71
#define GL_LEQUAL                0x0203
#define GL_TRIANGLES             0x0004
#define GL_UNSIGNED_INT          0x1405
#define GL_FLOAT                 0x1406
#define GL_UNSIGNED_BYTE         0x1401
#define GL_RGBA                  0x1908
#define GL_COLOR_BUFFER_BIT      0x00004000
#define GL_DEPTH_BUFFER_BIT      0x00000100
#define GL_VENDOR                0x1F00
#define GL_RENDERER              0x1F01
#define GL_FALSE                 0
#define GL_TRUE                  1

typedef void (*PFNGLCLEARCOLORPROC)(float, float, float, float);
typedef void (*PFNGLCLEARPROC)(unsigned int);
typedef const char *(*PFNGLGETSTRINGPROC)(unsigned int);
typedef unsigned int (*PFNGLGETERRORPROC)(void);
typedef void (*PFNGLVIEWPORTPROC)(int, int, int, int);
typedef void (*PFNGLDEPTHFUNCPROC)(unsigned int);
typedef void (*PFNGLDEPTHMASKPROC)(GLboolean);
typedef void (*PFNGLUSEPROGRAMPROC)(unsigned int);
typedef unsigned int (*PFNGLCREATESHADERPROC)(unsigned int);
typedef void (*PFNGLSHADERSOURCEPROC)(unsigned int, int, const char * const *, const int *);
typedef void (*PFNGLCOMPILESHADERPROC)(unsigned int);
typedef void (*PFNGLGETSHADERIVPROC)(unsigned int, unsigned int, int *);
typedef void (*PFNGLGETSHADERINFOLOGPROC)(unsigned int, int, int *, char *);
typedef unsigned int (*PFNGLCREATEPROGRAMPROC)(void);
typedef void (*PFNGLATTACHSHADERPROC)(unsigned int, unsigned int);
typedef void (*PFNGLLINKPROGRAMPROC)(unsigned int);
typedef void (*PFNGLGETPROGRAMIVPROC)(unsigned int, unsigned int, int *);
typedef void (*PFNGLGETPROGRAMINFOLOGPROC)(unsigned int, int, int *, char *);
typedef void (*PFNGLDELETESHADERPROC)(unsigned int);
typedef int (*PFNGLGETUNIFORMLOCATIONPROC)(unsigned int, const char *);
typedef void (*PFNGLUNIFORM1FPROC)(int, float);
typedef void (*PFNGLUNIFORM3FPROC)(int, float, float, float);
typedef void (*PFNGLUNIFORMMATRIX4FVPROC)(int, int, int, const float *);
typedef void (*PFNGLUNIFORMMATRIX3FVPROC)(int, int, int, const float *);
typedef void (*PFNGLGENBUFFERSPROC)(int, unsigned int *);
typedef void (*PFNGLGETINTEGERVPROC)(unsigned int, int *);
typedef void (*PFNGLBINDBUFFERPROC)(unsigned int, unsigned int);
typedef void (*PFNGLBUFFERDATAPROC)(unsigned int, long, const void *, unsigned int);
typedef void (*PFNGLVERTEXATTRIBPOINTERPROC)(unsigned int, int, unsigned int, int, unsigned long, const void *);
typedef void (*PFNGLENABLEVERTEXARRAYATTRIBPROC)(unsigned int);
typedef void (*PFNGLDRAWELEMENTSPROC)(unsigned int, int, unsigned int, const void *);
typedef void (*PFNGLDRAWARRAYSPROC)(unsigned int, int, int);
typedef void (*PFNGLREADPIXELSPROC)(int, int, int, int, unsigned int, unsigned int, void *);
typedef void (*PFNGLENABLEPROC)(unsigned int);
typedef void (*PFNGLDISABLEPROC)(unsigned int);
typedef void (*PFNGLGENVERTEXARRAYSPROC)(int, const unsigned int *);
typedef void (*PFNGLBINDVERTEXARRAYPROC)(unsigned int);
typedef void (*PFNGLDELETEVERTEXARRAYSPROC)(int, const unsigned int *);

static PFNGLCLEARCOLORPROC glClearColor;
static PFNGLCLEARPROC glClear;
static PFNGLGETSTRINGPROC glGetString;
static PFNGLGETERRORPROC glGetError;
static PFNGLVIEWPORTPROC glViewport;
static PFNGLDEPTHFUNCPROC glDepthFunc;
static PFNGLDEPTHMASKPROC glDepthMask;
static PFNGLUSEPROGRAMPROC glUseProgram;
static PFNGLCREATESHADERPROC glCreateShader;
static PFNGLSHADERSOURCEPROC glShaderSource;
static PFNGLCOMPILESHADERPROC glCompileShader;
static PFNGLGETSHADERIVPROC glGetShaderiv;
static PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
static PFNGLCREATEPROGRAMPROC glCreateProgram;
static PFNGLATTACHSHADERPROC glAttachShader;
static PFNGLLINKPROGRAMPROC glLinkProgram;
static PFNGLGETPROGRAMIVPROC glGetProgramiv;
static PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
static PFNGLDELETESHADERPROC glDeleteShader;
static PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
static PFNGLUNIFORM1FPROC glUniform1f;
static PFNGLUNIFORM3FPROC glUniform3f;
static PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
static PFNGLUNIFORMMATRIX3FVPROC glUniformMatrix3fv;
static PFNGLGENBUFFERSPROC glGenBuffers;
static PFNGLGETINTEGERVPROC glGetInteger;
static PFNGLBINDBUFFERPROC glBindBuffer;
static PFNGLBUFFERDATAPROC glBufferData;
static PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
static PFNGLENABLEVERTEXARRAYATTRIBPROC glEnableVertexAttribArray;
static PFNGLDRAWELEMENTSPROC glDrawElements;
static PFNGLDRAWARRAYSPROC glDrawArrays;
static PFNGLREADPIXELSPROC glReadPixels;
static PFNGLENABLEPROC glEnable;
static PFNGLDISABLEPROC glDisable;
static PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
static PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
static PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;

#define GLF_LOAD(var) \
    var = (decltype(var))(GLFWglproc)glfwGetProcAddress(#var); \
    if (!var) { fprintf(stderr, "missing GL function: " #var "\n"); exit(1); }

static void loadGL() {
    GLF_LOAD(glClearColor)
    GLF_LOAD(glClear)
    GLF_LOAD(glGetString)
    GLF_LOAD(glGetError)
    GLF_LOAD(glViewport)
    GLF_LOAD(glDepthFunc)
    GLF_LOAD(glDepthMask)
    GLF_LOAD(glUseProgram)
    GLF_LOAD(glCreateShader)
    GLF_LOAD(glShaderSource)
    GLF_LOAD(glCompileShader)
    GLF_LOAD(glGetShaderiv)
    GLF_LOAD(glGetShaderInfoLog)
    GLF_LOAD(glCreateProgram)
    GLF_LOAD(glAttachShader)
    GLF_LOAD(glLinkProgram)
    GLF_LOAD(glGetProgramiv)
    GLF_LOAD(glGetProgramInfoLog)
    GLF_LOAD(glDeleteShader)
    GLF_LOAD(glGetUniformLocation)
    GLF_LOAD(glUniform1f)
    GLF_LOAD(glUniform3f)
    GLF_LOAD(glUniformMatrix4fv)
    GLF_LOAD(glUniformMatrix3fv)
    GLF_LOAD(glGenBuffers)
    GLF_LOAD(glGetInteger)
    GLF_LOAD(glBindBuffer)
    GLF_LOAD(glBufferData)
    GLF_LOAD(glVertexAttribPointer)
    GLF_LOAD(glEnableVertexAttribArray)
    GLF_LOAD(glDrawElements)
    GLF_LOAD(glDrawArrays)
    GLF_LOAD(glReadPixels)
    GLF_LOAD(glEnable)
    GLF_LOAD(glDisable)
    GLF_LOAD(glGenVertexArrays)
    GLF_LOAD(glBindVertexArray)
    GLF_LOAD(glDeleteVertexArrays)
}

// ---------------------------------------------------------------------------
// Wave model (shared by VS and FS — keep identical)
// ---------------------------------------------------------------------------
static const char *WAVE_GLSL =
    "const float WAVE_AMP[5]  = float[5](1.0, 0.6, 0.35, 0.2, 0.1);\n"
    "const float WAVE_LEN[5]  = float[5](80.0, 50.0, 30.0, 18.0, 10.0);\n"
    "const float WAVE_SPD[5]  = float[5](1.2, 1.5, 1.8, 2.2, 2.6);\n"
    "const vec2  WAVE_DIR[5]  = vec2[5](vec2(1.0, 0.2), vec2(-0.6, 1.0), vec2(0.3, -0.8), vec2(-1.0, -0.3), vec2(0.7, 0.7));\n"
    "void waveDisp(in vec2 xz, float t, out float dx, out float dz, out float dy){\n"
    "  dx = 0.0; dz = 0.0; dy = 0.0;\n"
    "  for (int i = 0; i < 5; i++){\n"
    "    float L = WAVE_LEN[i], A = WAVE_AMP[i], S = WAVE_SPD[i];\n"
    "    vec2  f = WAVE_DIR[i];\n"
    "    float k = 6.283185307179586 / L;\n"
    "    float p = k * dot(f, xz) - S * t;\n"
    "    float c = cos(p), s = sin(p);\n"
    "    float Q = A * k / dot(f, f);\n"
    "    dx += f.x * Q * c; dz += f.y * Q * c; dy += A * s;\n"
    "  }\n"
    "}\n";

static const char *SKY_VS =
    "#version 330 core\n"
    "layout(location=0) in vec2 p;\n"
    "out vec2 vNdc;\n"
    "void main(){ vNdc = p; gl_Position = vec4(p, 0.0, 1.0); }\n";

static const char *SKY_FS =
    "#version 330 core\n"
    "uniform mat4 uInvVP;\n"
    "uniform vec3 uCamPos;\n"
    "in vec2 vNdc;\n"
    "out vec4 o;\n"
    "void main(){\n"
    "  vec4 a = uInvVP * vec4(vNdc, -1.0, 1.0);\n"
    "  vec3 rd = normalize(a.xyz / a.w - uCamPos);\n"
    "  vec3 sun = normalize(vec3(0.35, 0.42, -0.84));\n"
    "  vec3 zen = vec3(0.16, 0.40, 0.76);\n"
    "  vec3 hor = vec3(0.60, 0.76, 0.88);\n"
    "  float e = clamp(rd.y * 1.5 + 0.02, 0.0, 1.0);\n"
    "  vec3 sky = mix(hor, zen, pow(e, 0.55));\n"
    "  float d = max(dot(rd, sun), 0.0);\n"
    "  sky += vec3(1.0, 0.85, 0.6) * pow(d, 6.0) * 0.30;\n"
    "  sky += vec3(1.0, 0.95, 0.85) * pow(d, 400.0) * 1.3;\n"
    "  if (d > 0.99965) sky = vec3(1.0, 0.98, 0.92);\n"
    "  o = vec4(sky, 1.0);\n"
    "}\n";

static const std::string WATER_VS =
    std::string("#version 330 core\n"
                "layout(location=0) in vec3 pos;\n"
                "uniform mat4 uModel, uView, uProj;\n"
                "uniform float uTime;\n"
                "out vec3 vWorld;\n") +
    WAVE_GLSL +
    "void main(){\n"
    "  vec4 wp = uModel * vec4(pos, 1.0);\n"
    "  float dx, dz, dy;\n"
    "  waveDisp(wp.xz, uTime, dx, dz, dy);\n"
    "  wp.xyz += vec3(dx, dy, dz);\n"
    "  vWorld = wp.xyz;\n"
    "  gl_Position = uProj * uView * wp;\n"
    "}\n";

static const std::string WATER_FS =
    std::string("#version 330 core\n"
                "in vec3 vWorld;\n"
                "uniform vec3 uCamPos;\n"
                "uniform float uTime;\n"
                "uniform float uFogNear, uFogFar;\n"
                "out vec4 o;\n") +
    WAVE_GLSL +
    "void main(){\n"
    "  float dx, dz, dy;\n"
    "  waveDisp(vWorld.xz, uTime, dx, dz, dy);\n"
    "  vec3 N = normalize(vec3(-dx, 1.0, -dz));\n"
    "  vec3 V = normalize(uCamPos - vWorld);\n"
    "  vec3 sun = normalize(vec3(0.35, 0.42, -0.84));\n"
    "  float fres = 0.02 + 0.98 * pow(1.0 - max(dot(N, V), 0.0), 5.0);\n"
    "  vec3 deep = vec3(0.02, 0.13, 0.20);\n"
    "  vec3 shal = vec3(0.10, 0.44, 0.54);\n"
    "  vec3 base = mix(deep, shal, clamp(N.y, 0.0, 1.0));\n"
    "  vec3 skyCol = mix(vec3(0.60, 0.76, 0.88), vec3(0.16, 0.40, 0.76), clamp(V.y * 0.5 + 0.5, 0.0, 1.0));\n"
    "  vec3 col = mix(base, skyCol, clamp(fres, 0.0, 1.0));\n"
    "  vec3 H = normalize(sun + V);\n"
    "  float spec = pow(max(dot(N, H), 0.0), 260.0);\n"
    "  col += vec3(1.0, 0.95, 0.85) * spec * 1.5;\n"
    "  float fog = smoothstep(uFogNear, uFogFar, length(uCamPos - vWorld));\n"
    "  col = mix(col, vec3(0.60, 0.76, 0.88), fog);\n"
    "  o = vec4(col, 1.0);\n"
    "}\n";

// ---------------------------------------------------------------------------
// Fish (low-poly, displaced + swum on the CPU — small herd, cheap)
// ---------------------------------------------------------------------------
static const char *FISH_VS =
    "#version 330 core\n"
    "layout(location=0) in vec3 pos;\n"
    "uniform mat4 uView, uProj;\n"
    "uniform vec3 uPos;\n"
    "uniform mat3 uOrient;\n"
    "uniform float uScale, uTail;\n"
    "out vec3 vWorld;\n"
    "out float vShade;\n"
    "void main(){\n"
    "  vec3 p = pos;\n"
    "  // tail wag: bend the back half (z < -0.5) sideways\n"
    "  float b = smoothstep(-0.5, -1.4, p.z);\n"
    "  p.x += b * sin(uTail) * 0.30;\n"
    "  // gentle body flex\n"
    "  p.y += b * cos(uTail * 1.7) * 0.06;\n"
    "  vec3 wp = uPos + uOrient * (p * uScale);\n"
    "  vWorld = wp;\n"
    "  gl_Position = uProj * uView * vec4(wp, 1.0);\n"
    "  // simple lambert against the sun\n"
    "  vec3 N = uOrient * normalize(vec3(0.0, 1.0, 0.0) + vec3(0.0, 0.0, -p.z * 0.35));\n"
    "  vShade = clamp(dot(N, normalize(vec3(0.35, 0.42, -0.84))) * 0.5 + 0.5, 0.25, 1.0);\n"
    "}\n";

static const char *FISH_FS =
    "#version 330 core\n"
    "uniform vec3 uCamPos;\n"
    "uniform float uFogNear, uFogFar;\n"
    "uniform vec3 uColor;\n"
    "in vec3 vWorld;\n"
    "in float vShade;\n"
    "out vec4 o;\n"
    "void main(){\n"
    "  float fog = smoothstep(uFogNear, uFogFar, length(uCamPos - vWorld));\n"
    "  vec3 col = uColor * vShade;\n"
    "  // underwater depth tint: deeper = bluer/dimmer\n"
    "  float dpt = clamp(-vWorld.y / 10.0, 0.0, 1.0);\n"
    "  col = mix(col, col * vec3(0.45, 0.70, 0.85) + vec3(0.01, 0.05, 0.09), dpt * 0.75);\n"
    "  col = mix(col, vec3(0.03, 0.15, 0.22), clamp(fog, 0.0, 1.0));\n"
    "  o = vec4(col, 1.0);\n"
    "}\n";

// Low-poly fish body: 6-sided hull along -z (head at +z), 24 verts.
// Built as a ring of cross-sections; indices connect ring i to ring i+1.
// Ring layout (z, y, x) — head (z=+1.2) ... tail (z=-1.4).
static void buildFishGeometry(std::vector<float> &verts, std::vector<unsigned int> &idx) {
    struct Ring { float z, yTop, yBot, xRad; };
    static const Ring rings[] = {
        {  1.20f,  0.00f,  0.00f, 0.000f },  // nose tip
        {  0.70f,  0.16f, -0.16f, 0.180f },
        {  0.10f,  0.26f, -0.22f, 0.300f },  // mid body (widest)
        { -0.50f,  0.18f, -0.16f, 0.200f },
        { -1.05f,  0.10f, -0.08f, 0.070f },  // peduncle
    };
    const int N = 6;  // sides of the hexagonal hull
    // emit one ring of verts per structural ring (tip = 1 vert)
    for (int r = 0; r < 5; ++r) {
        if (r == 0) {
            verts.push_back(1.20f); verts.push_back(0.f); verts.push_back(0.f);
        } else {
            for (int i = 0; i < N; ++i) {
                float a = (float(i) / float(N)) * 6.2831853f + 0.5235988f;  // 30° offset
                float c = cosf(a), s = sinf(a);
                // hexagon: x = s * rad, y = (c>0 ? c : -c*0.7) * rad-ish via abs blend
                float y = c > 0 ? c * rings[r].yTop : -c * rings[r].yBot;
                float x = s * rings[r].xRad;
                verts.push_back(rings[r].z); verts.push_back(y); verts.push_back(x);
            }
        }
    }
    // tip (0) -> ring1 (1..6)
    for (int i = 0; i < N; ++i) {
        unsigned int a = 1 + i, b = 1 + (i + 1) % N;
        idx.push_back(0); idx.push_back(b); idx.push_back(a);
    }
    // vert offsets: tip=0, ring r (1..4) starts at 1 + (r-1)*N
    // ring i -> ring i+1
    for (int r = 0; r < 4; ++r) {
        int off0 = 1 + r * N, off1 = off0 + N;
        for (int i = 0; i < N; ++i) {
            unsigned int a = off0 + i, b = off0 + (i + 1) % N;
            unsigned int c = off1 + i, d = off1 + (i + 1) % N;
            idx.push_back(a); idx.push_back(c); idx.push_back(b);
            idx.push_back(b); idx.push_back(c); idx.push_back(d);
        }
    }
    // tail fin: diamond behind the peduncle (center + top + bottom verts)
    unsigned int pc = (unsigned int)verts.size() / 3;
    verts.push_back(-1.05f); verts.push_back(0.f); verts.push_back(0.f);
    // top fin
    unsigned int ft = pc + 1;
    verts.push_back(-1.50f); verts.push_back( 0.30f); verts.push_back(0.f);
    // bottom fin
    unsigned int fb = pc + 2;
    verts.push_back(-1.50f); verts.push_back(-0.30f); verts.push_back(0.f);
    // connect peduncle ring to fin (top verts -> top fin, bottom -> bottom)
    for (int i = 0; i < N; ++i) {
        unsigned int a = 1 + 4 * N + i;
        if (verts[a * 3 + 1] >= 0.f) { idx.push_back(a); idx.push_back(ft); idx.push_back(pc); }
        else                         { idx.push_back(a); idx.push_back(pc); idx.push_back(fb); }
    }
}

// ---------------------------------------------------------------------------
// Mat4 helpers (column-major)
// ---------------------------------------------------------------------------
struct Mat4 { float m[16]; };
static Mat4 matIdentity() { Mat4 r{}; r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f; return r; }
static Mat4 matMul(const Mat4 &a, const Mat4 &b) {
    Mat4 res{};
    for (int c = 0; c < 4; ++c)
        for (int i = 0; i < 4; ++i) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[k * 4 + c] * b.m[i * 4 + k];
            res.m[i * 4 + c] = s;
        }
    return res;
}
static Mat4 matPerspective(float fovy, float aspect, float n, float f) {
    Mat4 r{};
    float t = 1.f / tanf(fovy * 0.5f);
    r.m[0] = t / aspect; r.m[5] = t;
    r.m[10] = -(f + n) / (f - n); r.m[11] = -1.f;
    r.m[14] = -2.f * f * n / (f - n);
    return r;
}
static Mat4 matLookAt(const float *eye, const float *center, const float *up) {
    float zx = eye[0] - center[0], zy = eye[1] - center[1], zz = eye[2] - center[2];
    float l = sqrtf(zx * zx + zy * zy + zz * zz); zx /= l; zy /= l; zz /= l;
    float xx = up[1] * zz - up[2] * zy, xy = up[2] * zx - up[0] * zz, xz = up[0] * zy - up[1] * zx;
    l = sqrtf(xx * xx + xy * xy + xz * xz); xx /= l; xy /= l; xz /= l;
    float yx = zy * xz - zz * xy, yy = zz * xx - zx * xz, yz = zx * xy - zy * xx;
    Mat4 r{};
    r.m[0] = xx; r.m[1] = yx; r.m[2] = zx;
    r.m[4] = xy; r.m[5] = yy; r.m[6] = zy;
    r.m[8] = xz; r.m[9] = yz; r.m[10] = zz;
    r.m[12] = -(xx * eye[0] + xy * eye[1] + xz * eye[2]);
    r.m[13] = -(yx * eye[0] + yy * eye[1] + yz * eye[2]);
    r.m[14] = -(zx * eye[0] + zy * eye[1] + zz * eye[2]);
    r.m[15] = 1.f;
    return r;
}
static Mat4 matTranslate(float x, float y, float z) { Mat4 r = matIdentity(); r.m[12] = x; r.m[13] = y; r.m[14] = z; return r; }
static Mat4 matInverse(const Mat4 &in) {
    float m[16];
    for (int i = 0; i < 16; ++i) m[i] = in.m[i];
    float inv[16] = {};
    for (int i = 0; i < 4; ++i) inv[i * 4 + i] = 1.f;
    for (int col = 0; col < 4; ++col) {
        int piv = col;
        for (int r = col + 1; r < 4; ++r)
            if (std::fabs(m[r * 4 + col]) > std::fabs(m[piv * 4 + col])) piv = r;
        for (int c = 0; c < 4; ++c) {
            float t = m[piv * 4 + c]; m[piv * 4 + c] = m[col * 4 + c]; m[col * 4 + c] = t;
            float t2 = inv[piv * 4 + c]; inv[piv * 4 + c] = inv[col * 4 + c]; inv[col * 4 + c] = t2;
        }
        float d = m[col * 4 + col];
        for (int c = 0; c < 4; ++c) { m[col * 4 + c] /= d; inv[col * 4 + c] /= d; }
        for (int r = 0; r < 4; ++r) if (r != col) {
            float f = m[r * 4 + col];
            for (int c = 0; c < 4; ++c) { m[r * 4 + c] -= f * m[col * 4 + c]; inv[r * 4 + c] -= f * inv[col * 4 + c]; }
        }
    }
    Mat4 r;
    for (int i = 0; i < 16; ++i) r.m[i] = inv[i];
    return r;
}

// ---------------------------------------------------------------------------
// PNG writer (RGBA8, zlib)
// ---------------------------------------------------------------------------
static void writePNG(const std::string &path, int w, int h, const unsigned char *rgba) {
    std::vector<unsigned char> raw;
    raw.reserve((size_t)w * h * 5);
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        const unsigned char *s = rgba + (size_t)y * w * 4;
        raw.insert(raw.end(), s, s + (size_t)w * 4);
    }
    uLongf compLen = compressBound(raw.size());
    std::vector<unsigned char> comp(compLen);
    compress2(comp.data(), &compLen, raw.data(), raw.size(), 6);
    comp.resize(compLen);

    std::vector<unsigned char> out;
    out.insert(out.end(), {(unsigned char)137, 80, 78, 71, 13, 10, 26, 10});
    auto be32 = [](std::vector<unsigned char> &v, uint32_t x) {
        v.push_back((unsigned char)(x >> 24)); v.push_back((unsigned char)(x >> 16));
        v.push_back((unsigned char)(x >> 8));  v.push_back((unsigned char)(x));
    };
    auto chunk = [&](const unsigned char *type, const unsigned char *data, size_t len) {
        std::vector<unsigned char> c;
        be32(c, (uint32_t)len);
        c.insert(c.end(), type, type + 4);
        if (len > 0) c.insert(c.end(), data, data + len);
        uLong crc = crc32(0L, c.data() + 4, (uInt)(4 + len));
        be32(c, (uint32_t)crc);
        out.insert(out.end(), c.begin(), c.end());
    };
    unsigned char ihdr[13];
    uint32_t wu = (uint32_t)w, hu = (uint32_t)h;
    ihdr[0] = (unsigned char)(wu >> 24); ihdr[1] = (unsigned char)(wu >> 16);
    ihdr[2] = (unsigned char)(wu >> 8);  ihdr[3] = (unsigned char)wu;
    ihdr[4] = (unsigned char)(hu >> 24); ihdr[5] = (unsigned char)(hu >> 16);
    ihdr[6] = (unsigned char)(hu >> 8);  ihdr[7] = (unsigned char)hu;
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    chunk((const unsigned char *)"IHDR", ihdr, 13);
    chunk((const unsigned char *)"IDAT", comp.data(), comp.size());
    chunk((const unsigned char *)"IEND", nullptr, 0);
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) return;
    fwrite(out.data(), 1, out.size(), f);
    fclose(f);
}

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------
struct Cam {
    float pos[3] = {0, 8, 30};
    float yaw = -0.3f, pitch = -0.10f;
    float speedMul = 1.f;
    float fwd[3] = {0, 0, 1}, right[3] = {1, 0, 0}, up[3] = {0, 1, 0};
    void updateVectors() {
        float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
        fwd[0] = cy * cp; fwd[1] = sp; fwd[2] = -sy * cp;
        right[0] = cy; right[1] = 0; right[2] = -sy;
        up[0] = -sy * sp; up[1] = cp; up[2] = -cy * sp;
    }
    void reset() { pos[0] = 0; pos[1] = 8; pos[2] = 30; yaw = -0.3f; pitch = -0.10f; speedMul = 1.f; }
};

static std::vector<unsigned int> gKeys(256, 0);
static float gLastMouseX = 0, gLastMouseY = 0;
static Cam gCam;

// ---------------------------------------------------------------------------
// Fish school: each fish has a home point on a slowly-orbiting ring around
// a center 26 m ahead of the camera (the school follows you). Per frame the
// fish are integrated: spring back to home + damping + radial flee force
// from the camera when you get close (FLEE_RADIUS), so the herd scatters
// when you approach and regroups when you leave. Tail-wag speeds up while
// fleeing.
//
// Tunables: NFISH (compile-time), gFishSwim / gSchoolDepth (runtime keys).
// ---------------------------------------------------------------------------
static const int NFISH = 48;
static float gFishSwim = 2.2f;      // cruise swim speed, m/s     keys [ ]
static float gSchoolDepth = 4.0f;   // school depth, m under surf. keys Y/T

static const float SCHOOL_SPREAD  = 2.5f;  // vertical spread around the center
static const float FLEE_RADIUS    = 12.f;  // camera distance that triggers flee
static const float FLEE_FORCE     = 24.f;  // peak repulsion acceleration, m/s²
static const float FLEE_MAX_SPEED = 14.f;  // speed cap, m/s
static const float FISH_SPRING    = 2.0f;  // pull toward the home orbit, 1/s²
static const float FISH_DAMP      = 1.5f;  // velocity damping, 1/s

struct Fish {
    float orbitR;    // orbit radius around the school center
    float phase;     // initial orbit angle
    float cruiseMul; // per-fish share of gFishSwim (0.75..1.25)
    float hOff;      // vertical offset from the school center
    float hAmp;      // vertical bob amplitude
    float hFreq;    // vertical bob frequency
    float scale;
    float wagFreq;  // tail-wag frequency
    float col[3];
    // state (integrated per frame)
    float ang;      // orbit angle
    float pos[3];
    float vel[3];
    float wag;      // accumulated tail phase
    float fleeW;    // current flee intensity 0..1 (drives tail panic)
};
static std::vector<Fish> gFish;
static float gSimTime = 0.f;

static void initFish() {
    gCam.updateVectors();
    float FCX = gCam.pos[0] + gCam.fwd[0] * 26.f;
    float FCZ = gCam.pos[2] + gCam.fwd[2] * 26.f;
    for (int i = 0; i < NFISH; ++i) {
        Fish f{};
        float r2 = (float)((i * 53) % 10) / 10.f;
        float r3 = (float)((i * 29) % 10) / 10.f;
        float r4 = (float)((i * 41) % 10) / 10.f;
        float r5 = (float)((i * 67) % 10) / 10.f;
        float r6 = (float)((i * 83) % 10) / 10.f;
        f.orbitR    = 5.f + 20.f * ((float)((i * 37) % 20) / 19.f); // 5..25 m
        f.phase     = (float)i * 0.7f;
        f.cruiseMul = 0.75f + 0.5f * r2;
        f.hOff      = (0.2f + 0.8f * r3) * 2.f - SCHOOL_SPREAD;      // ±SPREAD
        f.hAmp      = 0.4f + 0.4f * r4;
        f.hFreq     = 0.4f + 0.3f * r5;
        f.scale     = 0.8f + 0.9f * r6;
        f.wagFreq   = 6.f + 2.f * ((float)((i * 91) % 10) / 10.f);
        // silver-blue palette with a bit of variance
        f.col[0] = 0.35f + 0.25f * r2;
        f.col[1] = 0.45f + 0.20f * r3;
        f.col[2] = 0.55f + 0.25f * r4;
        // state at t=0: on the home orbit, swimming along the tangent
        f.ang   = f.phase;
        f.pos[0] = FCX + cosf(f.ang) * f.orbitR;
        f.pos[1] = -gSchoolDepth + f.hOff + f.hAmp * sinf(f.phase);
        f.pos[2] = FCZ + sinf(f.ang) * f.orbitR;
        f.vel[0] = -sinf(f.ang) * gFishSwim * f.cruiseMul;
        f.vel[1] = 0.f;
        f.vel[2] = cosf(f.ang) * gFishSwim * f.cruiseMul;
        f.wag   = (float)i * 1.3f;
        f.fleeW = 0.f;
        gFish.push_back(f);
    }
}

// Home point on the home orbit + tangent velocity (for the spring and the
// heading fallback).
static void fishHome(const Fish &f, float FCX, float FCZ, float outPos[3], float outTan[3]) {
    outPos[0] = FCX + cosf(f.ang) * f.orbitR;
    outPos[1] = -gSchoolDepth + f.hOff + f.hAmp * sinf(f.hFreq * gSimTime + f.phase);
    outPos[2] = FCZ + sinf(f.ang) * f.orbitR;
    float cruise = gFishSwim * f.cruiseMul;
    if (outTan) {
        outTan[0] = -sinf(f.ang) * cruise;
        outTan[1] = 0.f;
        outTan[2] = cosf(f.ang) * cruise;
    }
}

// Integrate the whole school one step: spring to home + damping + flee from
// the camera. Fish inside FLEE_RADIUS get a radial repulsion that grows as
// you close in; the spring brings them back to the orbit afterwards.
static void stepFish(float dt) {
    if (dt <= 0.f) return;
    gCam.updateVectors();
    float FCX = gCam.pos[0] + gCam.fwd[0] * 26.f;
    float FCZ = gCam.pos[2] + gCam.fwd[2] * 26.f;
    const float *cp = gCam.pos;
    for (Fish &f : gFish) {
        float home[3];
        fishHome(f, FCX, FCZ, home, nullptr);
        // spring to home + damping
        float fx = (home[0] - f.pos[0]) * FISH_SPRING - f.vel[0] * FISH_DAMP;
        float fy = (home[1] - f.pos[1]) * FISH_SPRING - f.vel[1] * FISH_DAMP;
        float fz = (home[2] - f.pos[2]) * FISH_SPRING - f.vel[2] * FISH_DAMP;
        // flee: radial repulsion from the camera
        float dx = f.pos[0] - cp[0], dy = f.pos[1] - cp[1], dz = f.pos[2] - cp[2];
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        float w = 0.f;
        if (dist < FLEE_RADIUS && dist > 1e-4f) {
            w = 1.f - dist / FLEE_RADIUS;
            float s = FLEE_FORCE * w * w / dist;
            fx += dx * s; fy += dy * s; fz += dz * s;
        }
        f.fleeW += (w - f.fleeW) * std::min(1.f, 8.f * dt);
        // integrate
        f.vel[0] += fx * dt; f.vel[1] += fy * dt; f.vel[2] += fz * dt;
        float sp2 = f.vel[0] * f.vel[0] + f.vel[1] * f.vel[1] + f.vel[2] * f.vel[2];
        if (sp2 > FLEE_MAX_SPEED * FLEE_MAX_SPEED) {
            float k = FLEE_MAX_SPEED / sqrtf(sp2);
            f.vel[0] *= k; f.vel[1] *= k; f.vel[2] *= k;
        }
        f.pos[0] += f.vel[0] * dt;
        f.pos[1] += f.vel[1] * dt;
        f.pos[2] += f.vel[2] * dt;
        // stay underwater
        if (f.pos[1] > -0.3f) { f.pos[1] = -0.3f; if (f.vel[1] > 0.f) f.vel[1] = 0.f; }
        if (f.pos[1] < -14.f) { f.pos[1] = -14.f; if (f.vel[1] < 0.f) f.vel[1] = 0.f; }
        // orbit advance + tail (faster while fleeing)
        f.ang += (gFishSwim * f.cruiseMul / f.orbitR) * dt;
        f.wag += f.wagFreq * (1.f + 3.f * f.fleeW) * dt;
    }
    gSimTime += dt;
}

static GLuint compileShader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[4096]; glGetShaderInfoLog(s, sizeof log, nullptr, log);
               fprintf(stderr, "Shader error:\n%s\n", log); exit(1); }
    return s;
}
static GLuint makeProgram(const char *vs, const char *fs) {
    GLuint v = compileShader(GL_VERTEX_SHADER, vs), f = compileShader(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char log[4096]; glGetProgramInfoLog(p, sizeof log, nullptr, log);
               fprintf(stderr, "Link error:\n%s\n", log); exit(1); }
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

int main(int argc, char **argv) {
    bool doShot = false, headless = false;
    float shotTime = 0.f;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--screenshot") doShot = true;
        if (a == "--headless")   headless = true;
        if (a == "--time" && i + 1 < argc) shotTime = atof(argv[++i]);
    }

    if (headless) glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_NULL);
    if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    const int SW = 1280, SH = 720;
    GLFWwindow *win = glfwCreateWindow(SW, SH, "Ocean OpenGL", nullptr, nullptr);
    if (!win) { fprintf(stderr, "window creation failed\n"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    if (!headless) glfwSwapInterval(1);
    loadGL();
    fprintf(stderr, "GL: %s | %s\n", glGetString(GL_VENDOR), glGetString(GL_RENDERER));

    GLuint skyProg = makeProgram(SKY_VS, SKY_FS);
    GLuint waterProg = makeProgram(WATER_VS.c_str(), WATER_FS.c_str());

    GLint skyLocInvVP = glGetUniformLocation(skyProg, "uInvVP");
    GLint skyLocCam = glGetUniformLocation(skyProg, "uCamPos");
    GLint wLocT    = glGetUniformLocation(waterProg, "uTime");
    GLint wLocModel = glGetUniformLocation(waterProg, "uModel");
    GLint wLocView = glGetUniformLocation(waterProg, "uView");
    GLint wLocProj = glGetUniformLocation(waterProg, "uProj");
    GLint wLocCam  = glGetUniformLocation(waterProg, "uCamPos");
    GLint wLocFogN = glGetUniformLocation(waterProg, "uFogNear");
    GLint wLocFogF = glGetUniformLocation(waterProg, "uFogFar");

    // ---- sky fullscreen triangle ----
    GLuint skyVAO; glGenVertexArrays(1, &skyVAO);
    glBindVertexArray(skyVAO);
    GLuint skyVBO; glGenBuffers(1, &skyVBO);
    static const float skyVerts[] = {-1.f, -1.f,  3.f, -1.f,  -1.f, 3.f};
    glBindBuffer(GL_ARRAY_BUFFER, skyVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof skyVerts, skyVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    // ---- water grid (static; displaced on GPU) ----
    const int SEG = 256, SPAN = 800;
    const float FOG_NEAR = 180.f, FOG_FAR = 398.f;
    std::vector<float> verts((SEG + 1) * (SEG + 1) * 3);
    for (int y = 0; y <= SEG; ++y)
        for (int x = 0; x <= SEG; ++x) {
            float u = float(x) / SEG, v = float(y) / SEG;
            verts[(y * (SEG + 1) + x) * 3 + 0] = (u - 0.5f) * SPAN;
            verts[(y * (SEG + 1) + x) * 3 + 1] = 0.f;
            verts[(y * (SEG + 1) + x) * 3 + 2] = (v - 0.5f) * SPAN;
        }
    std::vector<unsigned int> idx;
    for (int y = 0; y < SEG; ++y)
        for (int x = 0; x < SEG; ++x) {
            unsigned int a = y * (SEG + 1) + x, b = a + 1, c = a + (SEG + 1), d = c + 1;
            idx.push_back(a); idx.push_back(c); idx.push_back(b);
            idx.push_back(b); idx.push_back(c); idx.push_back(d);
        }
    // ---- water: explicit VAO (default-VAO vertex attribs are unreliable on Mesa) ----
    GLuint waterVAO; glGenVertexArrays(1, &waterVAO);
    glBindVertexArray(waterVAO);
    GLuint waterVBO, waterIBO;
    glGenBuffers(1, &waterVBO); glGenBuffers(1, &waterIBO);
    glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, waterIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);

    // ---- fish: low-poly school, opaque, drawn before the translucent water ----
    initFish();
    GLuint fishProg = makeProgram(FISH_VS, FISH_FS);
    GLint fLocView = glGetUniformLocation(fishProg, "uView");
    GLint fLocProj = glGetUniformLocation(fishProg, "uProj");
    GLint fLocPos  = glGetUniformLocation(fishProg, "uPos");
    GLint fLocOri  = glGetUniformLocation(fishProg, "uOrient");
    GLint fLocScl  = glGetUniformLocation(fishProg, "uScale");
    GLint fLocTail = glGetUniformLocation(fishProg, "uTail");
    GLint fLocCam  = glGetUniformLocation(fishProg, "uCamPos");
    GLint fLocFogN = glGetUniformLocation(fishProg, "uFogNear");
    GLint fLocFogF = glGetUniformLocation(fishProg, "uFogFar");
    GLint fLocCol  = glGetUniformLocation(fishProg, "uColor");
    std::vector<float> fishVerts; std::vector<unsigned int> fishIdx;
    buildFishGeometry(fishVerts, fishIdx);
    GLuint fishVAO; glGenVertexArrays(1, &fishVAO);
    glBindVertexArray(fishVAO);
    GLuint fishVBO; glGenBuffers(1, &fishVBO);
    glBindBuffer(GL_ARRAY_BUFFER, fishVBO);
    glBufferData(GL_ARRAY_BUFFER, fishVerts.size() * sizeof(float), fishVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
    GLuint fishIBO; glGenBuffers(1, &fishIBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fishIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, fishIdx.size() * sizeof(unsigned int), fishIdx.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);

    auto render = [&](float t, int w, int h) {
        glViewport(0, 0, w, h);
        glClearColor(0.60f, 0.76f, 0.88f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        { unsigned err = glGetError(); if (err) fprintf(stderr, "GL clear err 0x%x\n", err); }

        gCam.updateVectors();
        float aspect = float(w) / float(h);
        Mat4 P = matPerspective(60.f * 3.14159265358979323846f / 180.f, aspect, 0.1f, 2000.f);
        float center[3] = { gCam.pos[0] + gCam.fwd[0], gCam.pos[1] + gCam.fwd[1], gCam.pos[2] + gCam.fwd[2] };
        float worldUp[3] = {0, 1, 0};
        Mat4 V = matLookAt(gCam.pos, center, worldUp);
        Mat4 VP = matMul(P, V);

        // sky first, no depth test
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glUseProgram(skyProg);
        glUniformMatrix4fv(skyLocInvVP, 1, GL_FALSE, matInverse(VP).m);
        glUniform3f(skyLocCam, gCam.pos[0], gCam.pos[1], gCam.pos[2]);
        glBindVertexArray(skyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        { unsigned err = glGetError(); if (err) fprintf(stderr, "GL sky draw error 0x%x\n", err); }
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        // water: opaque, grid centered on camera integer XZ
        float cx = float((int)gCam.pos[0]);
        float cz = float((int)gCam.pos[2]);
        Mat4 T = matTranslate(cx, 0.f, cz);
        glUseProgram(waterProg);
        glUniform1f(wLocT, t);
        glUniformMatrix4fv(wLocModel, 1, GL_FALSE, T.m);
        glUniformMatrix4fv(wLocView, 1, GL_FALSE, V.m);
        glUniformMatrix4fv(wLocProj, 1, GL_FALSE, P.m);
        glUniform3f(wLocCam, gCam.pos[0], gCam.pos[1], gCam.pos[2]);
        glUniform1f(wLocFogN, FOG_NEAR);
        glUniform1f(wLocFogF, FOG_FAR);
        glBindVertexArray(waterVAO);
        glDrawElements(GL_TRIANGLES, idx.size(), GL_UNSIGNED_INT, 0);
        { unsigned err = glGetError();
          if (err) fprintf(stderr, "GL draw error 0x%x (water)\n", err); }

        // fish school (underwater): drawn after the opaque water with depth
        // test off, so they always show through the surface. Depth cueing is
        // faked in the shader via a bluer/dimmer tint with depth. Fish are
        // sorted far->near: with no depth test, draw order is their only
        // depth order. (Mesa 26.2.2 on this box rejects glEnable(GL_BLEND)
        // with INVALID_OPERATION, so no real alpha blending is used.)
        {
            glDisable(GL_DEPTH_TEST);
            glUseProgram(fishProg);
            glUniformMatrix4fv(fLocView, 1, GL_FALSE, V.m);
            glUniformMatrix4fv(fLocProj, 1, GL_FALSE, P.m);
            glUniform3f(fLocCam, gCam.pos[0], gCam.pos[1], gCam.pos[2]);
            glUniform1f(fLocFogN, FOG_NEAR);
            glUniform1f(fLocFogF, FOG_FAR);
            glBindVertexArray(fishVAO);
            std::vector<size_t> order(gFish.size());
            for (size_t i = 0; i < order.size(); ++i) order[i] = i;
            const float cx0 = gCam.pos[0], cy0 = gCam.pos[1], cz0 = gCam.pos[2];
            auto d2 = [&](size_t i) {
                const float *p = gFish[i].pos;
                float ddx = p[0] - cx0, ddy = p[1] - cy0, ddz = p[2] - cz0;
                return ddx * ddx + ddy * ddy + ddz * ddz;
            };
            std::sort(order.begin(), order.end(),
                      [&](size_t a, size_t b) { return d2(a) > d2(b); });
            for (size_t oi = 0; oi < order.size(); ++oi) {
                const Fish &f = gFish[order[oi]];
                // heading = velocity; fall back to the orbit tangent if ~0
                float vx = f.vel[0], vy = f.vel[1], vz = f.vel[2];
                float vl = sqrtf(vx * vx + vy * vy + vz * vz);
                if (vl < 0.05f) { vx = -sinf(f.ang); vy = 0.f; vz = cosf(f.ang); vl = 1.f; }
                else { vx /= vl; vy /= vl; vz /= vl; }
                float ax = vz, ay = 0.f, az = -vx;  // up x fwd
                float al = sqrtf(ax * ax + az * az);
                if (al < 1e-3f) {  // degenerate (fwd ~ vertical): use tangent
                    vx = -sinf(f.ang); vy = 0.f; vz = cosf(f.ang);
                    ax = vz; az = -vx; al = 1.f;
                }
                ax /= al; az /= al;
                float bx = vy * az - vz * ay, by = vz * ax - vx * az, bz = vx * ay - vy * ax;  // fwd x right
                float ori[9] = { ax, ay, az, bx, by, bz, vx, vy, vz };  // columns: right, up, fwd
                glUniform3f(fLocPos, f.pos[0], f.pos[1], f.pos[2]);
                glUniformMatrix3fv(fLocOri, 1, GL_FALSE, ori);
                glUniform1f(fLocScl, f.scale);
                glUniform1f(fLocTail, f.wag);
                glUniform3f(fLocCol, f.col[0], f.col[1], f.col[2]);
                glDrawElements(GL_TRIANGLES, (GLsizei)fishIdx.size(), GL_UNSIGNED_INT, 0);
            }
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
        }
        { unsigned err = glGetError(); if (err) fprintf(stderr, "GL fish draw error 0x%x\n", err); }
    };

    // ---- input ----
    if (!headless) {
        glfwSetKeyCallback(win, [](GLFWwindow *, int key, int, int action, int) {
            if (key >= 0 && key < 256) gKeys[key] = (unsigned int)action;
        });
        glfwSetCursorPosCallback(win, [](GLFWwindow *, double x, double y) {
            if (gKeys[GLFW_MOUSE_BUTTON_LEFT] || gKeys[GLFW_MOUSE_BUTTON_MIDDLE]) {
                float dx = float(x) - gLastMouseX, dy = float(y) - gLastMouseY;
                if (gKeys[GLFW_MOUSE_BUTTON_LEFT]) {
                    gCam.yaw += dx * 0.0025f;
                    gCam.pitch -= dy * 0.0025f;
                    gCam.pitch = std::max(-1.55f, std::min(1.55f, gCam.pitch));
                } else {
                    float sp = 0.05f * gCam.speedMul;
                    gCam.pos[0] -= gCam.right[0] * dx * sp + gCam.fwd[0] * dy * sp;
                    gCam.pos[2] -= gCam.right[2] * dx * sp + gCam.fwd[2] * dy * sp;
                }
                gLastMouseX = float(x); gLastMouseY = float(y);
            }
        });
        glfwSetMouseButtonCallback(win, [](GLFWwindow *win, int button, int action, int) {
            if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_MIDDLE) {
                gKeys[button] = (unsigned int)action;
                if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT)
                    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                else if (button == GLFW_MOUSE_BUTTON_LEFT)
                    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        });
        glfwSetScrollCallback(win, [](GLFWwindow *, double, double yoff) {
            gCam.speedMul *= (yoff > 0) ? 1.15f : 1.f / 1.15f;
            gCam.speedMul = std::max(0.05f, std::min(400.f, gCam.speedMul));
        });
    }

    // ---- main loop ----
    double last = glfwGetTime();
    // screenshot: fast-forward the school to the requested time so the
    // fish are at t (windowed screenshot mode also simulates from 0, since
    // the loop's clock is the wall clock). Static camera: fish near the
    // close side of the orbit already show the flee offset at equilibrium.
    if (shotTime > 0.f) {
        float ft = 0.f;
        while (ft < shotTime - 1e-4f) {
            float s = std::min(1.f / 60.f, shotTime - ft);
            stepFish(s);
            ft += s;
        }
    }
    bool shotDone = false;
    float fpsAcc = 0.f; int fpsFrames = 0;
    while (true) {
        double now = glfwGetTime();
        float dt = float(now - last); last = now;
        if (dt > 0.1f) dt = 0.1f;

        float t = headless ? shotTime : float(now);
        stepFish(dt);

        if (!headless) {
            float sp = 18.f * gCam.speedMul * (gKeys[GLFW_KEY_LEFT_SHIFT] ? 3.f : 1.f) * dt;
            if (gKeys[GLFW_KEY_W]) { gCam.pos[0] += gCam.fwd[0] * sp; gCam.pos[1] += gCam.fwd[1] * sp; gCam.pos[2] += gCam.fwd[2] * sp; }
            if (gKeys[GLFW_KEY_S]) { gCam.pos[0] -= gCam.fwd[0] * sp; gCam.pos[1] -= gCam.fwd[1] * sp; gCam.pos[2] -= gCam.fwd[2] * sp; }
            if (gKeys[GLFW_KEY_D]) { gCam.pos[0] += gCam.right[0] * sp; gCam.pos[2] += gCam.right[2] * sp; }
            if (gKeys[GLFW_KEY_A]) { gCam.pos[0] -= gCam.right[0] * sp; gCam.pos[2] -= gCam.right[2] * sp; }
            if (gKeys[GLFW_KEY_E]) gCam.pos[1] += sp;
            if (gKeys[GLFW_KEY_Q]) gCam.pos[1] -= sp;
            if (gKeys[GLFW_KEY_R]) gCam.reset();
            if (gKeys[GLFW_KEY_RIGHT_BRACKET]) gFishSwim = std::min(8.f, gFishSwim * 1.15f);
            if (gKeys[GLFW_KEY_LEFT_BRACKET])  gFishSwim = std::max(0.4f, gFishSwim / 1.15f);
            if (gKeys[GLFW_KEY_T]) gSchoolDepth = std::min(10.f, gSchoolDepth + 2.f * dt);
            if (gKeys[GLFW_KEY_Y]) gSchoolDepth = std::max(1.f, gSchoolDepth - 2.f * dt);
            if (gKeys[GLFW_KEY_ESCAPE]) break;
            fpsAcc += dt; ++fpsFrames;
            if (fpsAcc > 0.5f) {
                char title[200];
                snprintf(title, sizeof title,
                         "Ocean OpenGL — %.1f fps | swim %.1f m/s [ ] | school %.1f m Y/T | WASD+QE | Shift | L look / M pan | wheel | R",
                         fpsFrames / fpsAcc, gFishSwim, gSchoolDepth);
                glfwSetWindowTitle(win, title);
                fpsAcc = 0.f; fpsFrames = 0;
            }
        }

        int w = SW, h = SH;
        if (!headless) { int fw, fh; glfwGetFramebufferSize(win, &fw, &fh); w = fw; h = fh; }
        render(t, w, h);

        if (doShot && !shotDone) {
            shotDone = true;
            std::vector<unsigned char> px(w * h * 4);
            glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
            for (int y = 0; y < h / 2; ++y)
                for (int x = 0; x < w; ++x) {
                    int a = (y * w + x) * 4, b = ((h - 1 - y) * w + x) * 4;
                    for (int c = 0; c < 4; ++c) std::swap(px[a + c], px[b + c]);
                }
            writePNG("ocean.png", w, h, px.data());
            fprintf(stderr, "Screenshot saved: ocean.png\n");
        }
        if (headless) break;
        glfwPollEvents();
        glfwSwapBuffers(win);
    }
    glfwTerminate();
    return 0;
}
