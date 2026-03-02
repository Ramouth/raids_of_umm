/*
 * GLAD OpenGL 3.3 Core Profile Loader
 * Minimal hand-written version for Raids of Umm'Natur
 */
#ifndef __glad_h_
#define __glad_h_

#ifdef __gl_h_
#error OpenGL header already included, remove this include
#endif
#define __gl_h_

#if defined(_WIN32) && !defined(APIENTRY) && !defined(__CYGWIN__) && !defined(__SCITECH_SNAP__)
#define APIENTRY __stdcall
#endif

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif
#ifndef GLAPI
#if defined(GLAD_GLAPI_EXPORT)
#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(GLAD_GLAPI_EXPORT_BUILD)
#if defined(__GNUC__)
#define GLAPI __attribute__ ((dllexport)) extern
#else
#define GLAPI __declspec(dllexport) extern
#endif
#else
#if defined(__GNUC__)
#define GLAPI __attribute__ ((dllimport)) extern
#else
#define GLAPI __declspec(dllimport) extern
#endif
#endif
#elif defined(__GNUC__) && defined(GLAD_GLAPI_EXPORT_BUILD)
#define GLAPI __attribute__ ((visibility ("default"))) extern
#else
#define GLAPI extern
#endif
#else
#define GLAPI extern
#endif
#endif

#include <stddef.h>
#include <stdint.h>

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef void GLvoid;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLclampx;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef int GLsizei;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void *GLeglClientBufferEXT;
typedef void *GLeglImageOES;
typedef char GLchar;
typedef char GLcharARB;
#ifdef __APPLE__
typedef void *GLhandleARB;
#else
typedef unsigned int GLhandleARB;
#endif
typedef unsigned short GLhalf;
typedef unsigned short GLhalfARB;
typedef int GLfixed;
typedef ptrdiff_t GLintptr;
typedef ptrdiff_t GLintptrARB;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLsizeiptrARB;
typedef int64_t GLint64;
typedef int64_t GLint64EXT;
typedef uint64_t GLuint64;
typedef uint64_t GLuint64EXT;
typedef struct __GLsync *GLsync;
struct _cl_context;
struct _cl_event;
typedef void (APIENTRY *GLDEBUGPROC)(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam);
typedef void (APIENTRY *GLDEBUGPROCARB)(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam);
typedef void (APIENTRY *GLDEBUGPROCKHR)(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam);
typedef void (APIENTRY *GLDEBUGPROCAMD)(GLuint id,GLenum category,GLenum severity,GLsizei length,const GLchar *message,void *userParam);
typedef unsigned short GLhalfNV;
typedef GLintptr GLvdpauSurfaceNV;
typedef void (APIENTRY *GLVULKANPROCNV)(void);

/* OpenGL 3.3 Core constants */
#define GL_FALSE                          0
#define GL_TRUE                           1
#define GL_NONE                           0
#define GL_NO_ERROR                       0
#define GL_ZERO                           0
#define GL_ONE                            1
#define GL_POINTS                         0x0000
#define GL_LINES                          0x0001
#define GL_LINE_LOOP                      0x0002
#define GL_LINE_STRIP                     0x0003
#define GL_TRIANGLES                      0x0004
#define GL_TRIANGLE_STRIP                 0x0005
#define GL_TRIANGLE_FAN                   0x0006
#define GL_DEPTH_BUFFER_BIT               0x00000100
#define GL_STENCIL_BUFFER_BIT             0x00000400
#define GL_COLOR_BUFFER_BIT               0x00004000
#define GL_BYTE                           0x1400
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_SHORT                          0x1402
#define GL_UNSIGNED_SHORT                 0x1403
#define GL_INT                            0x1404
#define GL_UNSIGNED_INT                   0x1405
#define GL_FLOAT                          0x1406
#define GL_DOUBLE                         0x140A
#define GL_HALF_FLOAT                     0x140B
#define GL_FIXED                          0x140C
#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_STREAM_DRAW                    0x88E0
#define GL_STREAM_READ                    0x88E1
#define GL_STREAM_COPY                    0x88E2
#define GL_STATIC_DRAW                    0x88E4
#define GL_STATIC_READ                    0x88E5
#define GL_STATIC_COPY                    0x88E6
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_DYNAMIC_READ                   0x88E9
#define GL_DYNAMIC_COPY                   0x88EA
#define GL_VERTEX_SHADER                  0x8B31
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_GEOMETRY_SHADER                0x8DD9
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_INFO_LOG_LENGTH                0x8B84
#define GL_VALIDATE_STATUS                0x8B83
#define GL_TEXTURE_2D                     0x0DE1
#define GL_TEXTURE_CUBE_MAP               0x8513
#define GL_TEXTURE0                       0x84C0
#define GL_TEXTURE1                       0x84C1
#define GL_TEXTURE2                       0x84C2
#define GL_TEXTURE_WRAP_S                 0x2802
#define GL_TEXTURE_WRAP_T                 0x2803
#define GL_TEXTURE_MIN_FILTER             0x2801
#define GL_TEXTURE_MAG_FILTER             0x2800
#define GL_NEAREST                        0x2600
#define GL_LINEAR                         0x2601
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_REPEAT                         0x2901
#define GL_RGB                            0x1907
#define GL_RGBA                           0x1908
#define GL_RGB8                           0x8051
#define GL_RGBA8                          0x8058
#define GL_DEPTH_COMPONENT                0x1902
#define GL_DEPTH_COMPONENT16              0x81A5
#define GL_DEPTH_COMPONENT24              0x81A6
#define GL_DEPTH_STENCIL                  0x84F9
#define GL_UNSIGNED_BYTE_3_3_2            0x8032
#define GL_VENDOR                         0x1F00
#define GL_RENDERER                       0x1F01
#define GL_VERSION                        0x1F02
#define GL_SHADING_LANGUAGE_VERSION       0x8B8C
#define GL_EXTENSIONS                     0x1F03
#define GL_DEPTH_TEST                     0x0B71
#define GL_BLEND                          0x0BE2
#define GL_CULL_FACE                      0x0B44
#define GL_BACK                           0x0405
#define GL_FRONT                          0x0404
#define GL_CCW                            0x0901
#define GL_CW                             0x0900
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_LESS                           0x0201
#define GL_LEQUAL                         0x0203
#define GL_FRAMEBUFFER                    0x8D40
#define GL_RENDERBUFFER                   0x8D41
#define GL_FRAMEBUFFER_COMPLETE           0x8CD5
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_DEPTH_ATTACHMENT               0x8D00
#define GL_VERTEX_ARRAY_BINDING           0x85B5
#define GL_PROGRAM_POINT_SIZE             0x8642
#define GL_FILL                           0x1B02
#define GL_LINE                           0x1B01
#define GL_POINT                          0x1B00
#define GL_FRONT_AND_BACK                 0x0408
#define GL_RED                            0x1903
#define GL_UNSIGNED_INT_24_8              0x84FA
#define GL_DEPTH24_STENCIL8               0x88F0
#define GL_DEPTH_STENCIL_ATTACHMENT       0x821A
#define GL_NUM_EXTENSIONS                 0x821D
#define GL_INVALID_ENUM                   0x0500
#define GL_INVALID_VALUE                  0x0501
#define GL_INVALID_OPERATION              0x0502
#define GL_STACK_OVERFLOW                 0x0503
#define GL_STACK_UNDERFLOW                0x0504
#define GL_OUT_OF_MEMORY                  0x0505
#define GL_INVALID_FRAMEBUFFER_OPERATION  0x0506
#define GL_MAX_VERTEX_ATTRIBS             0x8869
#define GL_SCISSOR_TEST                   0x0C11

/* Function pointer typedefs */
typedef void (APIENTRYP PFNGLACTIVETEXTUREPROC)(GLenum texture);
typedef void (APIENTRYP PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (APIENTRYP PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void (APIENTRYP PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void (APIENTRYP PFNGLBINDRENDERBUFFERPROC)(GLenum target, GLuint renderbuffer);
typedef void (APIENTRYP PFNGLBINDTEXTUREPROC)(GLenum target, GLuint texture);
typedef void (APIENTRYP PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void (APIENTRYP PFNGLBLENDFUNCPROC)(GLenum sfactor, GLenum dfactor);
typedef void (APIENTRYP PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void (APIENTRYP PFNGLBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
typedef GLenum (APIENTRYP PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum target);
typedef void (APIENTRYP PFNGLCLEARPROC)(GLbitfield mask);
typedef void (APIENTRYP PFNGLCLEARCOLORPROC)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (APIENTRYP PFNGLCLEARDEPTHFPROC)(GLfloat depth);
typedef void (APIENTRYP PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef GLuint (APIENTRYP PFNGLCREATEPROGRAMPROC)(void);
typedef GLuint (APIENTRYP PFNGLCREATESHADERPROC)(GLenum type);
typedef void (APIENTRYP PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint *buffers);
typedef void (APIENTRYP PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei n, const GLuint *framebuffers);
typedef void (APIENTRYP PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef void (APIENTRYP PFNGLDELETERENDERBUFFERSPROC)(GLsizei n, const GLuint *renderbuffers);
typedef void (APIENTRYP PFNGLDELETESHADERPROC)(GLuint shader);
typedef void (APIENTRYP PFNGLDELETETEXTURESPROC)(GLsizei n, const GLuint *textures);
typedef void (APIENTRYP PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint *arrays);
typedef void (APIENTRYP PFNGLDEPTHFUNCPROC)(GLenum func);
typedef void (APIENTRYP PFNGLDEPTHMASKPROC)(GLboolean flag);
typedef void (APIENTRYP PFNGLDISABLEPROC)(GLenum cap);
typedef void (APIENTRYP PFNGLDISABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (APIENTRYP PFNGLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);
typedef void (APIENTRYP PFNGLDRAWELEMENTSPROC)(GLenum mode, GLsizei count, GLenum type, const void *indices);
typedef void (APIENTRYP PFNGLENABLEPROC)(GLenum cap);
typedef void (APIENTRYP PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (APIENTRYP PFNGLFRAMEBUFFERRENDERBUFFERPROC)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (APIENTRYP PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRYP PFNGLGENBUFFERSPROC)(GLsizei n, GLuint *buffers);
typedef void (APIENTRYP PFNGLGENERATEMIPMAPPROC)(GLenum target);
typedef void (APIENTRYP PFNGLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint *framebuffers);
typedef void (APIENTRYP PFNGLGENRENDERBUFFERSPROC)(GLsizei n, GLuint *renderbuffers);
typedef void (APIENTRYP PFNGLGENTEXTURESPROC)(GLsizei n, GLuint *textures);
typedef void (APIENTRYP PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint *arrays);
typedef GLint (APIENTRYP PFNGLGETATTRIBLOCATIONPROC)(GLuint program, const GLchar *name);
typedef GLenum (APIENTRYP PFNGLGETERRORPROC)(void);
typedef void (APIENTRYP PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRYP PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRYP PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint *params);
typedef const GLubyte *(APIENTRYP PFNGLGETSTRINGPROC)(GLenum name);
typedef const GLubyte *(APIENTRYP PFNGLGETSTRINGIPROC)(GLenum name, GLuint index);
typedef void (APIENTRYP PFNGLGETINTEGERVPROC)(GLenum pname, GLint *data);
typedef GLint (APIENTRYP PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar *name);
typedef void (APIENTRYP PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void (APIENTRYP PFNGLPOLYGONMODEPROC)(GLenum face, GLenum mode);
typedef void (APIENTRYP PFNGLRENDERBUFFERSTORAGEPROC)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP PFNGLSCISSORPROC)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void (APIENTRYP PFNGLTEXIMAGE2DPROC)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP PFNGLTEXPARAMETERIPROC)(GLenum target, GLenum pname, GLint param);
typedef void (APIENTRYP PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void (APIENTRYP PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void (APIENTRYP PFNGLUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRYP PFNGLUNIFORM3FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (APIENTRYP PFNGLUNIFORM4FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (APIENTRYP PFNGLUNIFORMMATRIX3FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORMMATRIX4FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP PFNGLUSEPROGRAMPROC)(GLuint program);
typedef void (APIENTRYP PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef void (APIENTRYP PFNGLVIEWPORTPROC)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP PFNGLCULLFACEPROC)(GLenum mode);
typedef void (APIENTRYP PFNGLFRONTFACEPROC)(GLenum mode);
typedef void (APIENTRYP PFNGLDEBUGMESSAGECALLBACKPROC)(GLDEBUGPROC callback, const void *userParam);

/* Function declarations */
GLAPI PFNGLACTIVETEXTUREPROC glad_glActiveTexture;
GLAPI PFNGLATTACHSHADERPROC glad_glAttachShader;
GLAPI PFNGLBINDBUFFERPROC glad_glBindBuffer;
GLAPI PFNGLBINDFRAMEBUFFERPROC glad_glBindFramebuffer;
GLAPI PFNGLBINDRENDERBUFFERPROC glad_glBindRenderbuffer;
GLAPI PFNGLBINDTEXTUREPROC glad_glBindTexture;
GLAPI PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray;
GLAPI PFNGLBLENDFUNCPROC glad_glBlendFunc;
GLAPI PFNGLBUFFERDATAPROC glad_glBufferData;
GLAPI PFNGLBUFFERSUBDATAPROC glad_glBufferSubData;
GLAPI PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_glCheckFramebufferStatus;
GLAPI PFNGLCLEARPROC glad_glClear;
GLAPI PFNGLCLEARCOLORPROC glad_glClearColor;
GLAPI PFNGLCLEARDEPTHFPROC glad_glClearDepthf;
GLAPI PFNGLCOMPILESHADERPROC glad_glCompileShader;
GLAPI PFNGLCREATEPROGRAMPROC glad_glCreateProgram;
GLAPI PFNGLCREATESHADERPROC glad_glCreateShader;
GLAPI PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers;
GLAPI PFNGLDELETEFRAMEBUFFERSPROC glad_glDeleteFramebuffers;
GLAPI PFNGLDELETEPROGRAMPROC glad_glDeleteProgram;
GLAPI PFNGLDELETERENDERBUFFERSPROC glad_glDeleteRenderbuffers;
GLAPI PFNGLDELETESHADERPROC glad_glDeleteShader;
GLAPI PFNGLDELETETEXTURESPROC glad_glDeleteTextures;
GLAPI PFNGLDELETEVERTEXARRAYSPROC glad_glDeleteVertexArrays;
GLAPI PFNGLDEPTHFUNCPROC glad_glDepthFunc;
GLAPI PFNGLDEPTHMASKPROC glad_glDepthMask;
GLAPI PFNGLDISABLEPROC glad_glDisable;
GLAPI PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_glDisableVertexAttribArray;
GLAPI PFNGLDRAWARRAYSPROC glad_glDrawArrays;
GLAPI PFNGLDRAWELEMENTSPROC glad_glDrawElements;
GLAPI PFNGLENABLEPROC glad_glEnable;
GLAPI PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray;
GLAPI PFNGLFRAMEBUFFERRENDERBUFFERPROC glad_glFramebufferRenderbuffer;
GLAPI PFNGLFRAMEBUFFERTEXTURE2DPROC glad_glFramebufferTexture2D;
GLAPI PFNGLGENBUFFERSPROC glad_glGenBuffers;
GLAPI PFNGLGENERATEMIPMAPPROC glad_glGenerateMipmap;
GLAPI PFNGLGENFRAMEBUFFERSPROC glad_glGenFramebuffers;
GLAPI PFNGLGENRENDERBUFFERSPROC glad_glGenRenderbuffers;
GLAPI PFNGLGENTEXTURESPROC glad_glGenTextures;
GLAPI PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays;
GLAPI PFNGLGETATTRIBLOCATIONPROC glad_glGetAttribLocation;
GLAPI PFNGLGETERRORPROC glad_glGetError;
GLAPI PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog;
GLAPI PFNGLGETPROGRAMIVPROC glad_glGetProgramiv;
GLAPI PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog;
GLAPI PFNGLGETSHADERIVPROC glad_glGetShaderiv;
GLAPI PFNGLGETSTRINGPROC glad_glGetString;
GLAPI PFNGLGETSTRINGIPROC glad_glGetStringi;
GLAPI PFNGLGETINTEGERVPROC glad_glGetIntegerv;
GLAPI PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation;
GLAPI PFNGLLINKPROGRAMPROC glad_glLinkProgram;
GLAPI PFNGLPOLYGONMODEPROC glad_glPolygonMode;
GLAPI PFNGLRENDERBUFFERSTORAGEPROC glad_glRenderbufferStorage;
GLAPI PFNGLSCISSORPROC glad_glScissor;
GLAPI PFNGLSHADERSOURCEPROC glad_glShaderSource;
GLAPI PFNGLTEXIMAGE2DPROC glad_glTexImage2D;
GLAPI PFNGLTEXPARAMETERIPROC glad_glTexParameteri;
GLAPI PFNGLUNIFORM1IPROC glad_glUniform1i;
GLAPI PFNGLUNIFORM1FPROC glad_glUniform1f;
GLAPI PFNGLUNIFORM2FPROC glad_glUniform2f;
GLAPI PFNGLUNIFORM3FPROC glad_glUniform3f;
GLAPI PFNGLUNIFORM4FPROC glad_glUniform4f;
GLAPI PFNGLUNIFORMMATRIX3FVPROC glad_glUniformMatrix3fv;
GLAPI PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv;
GLAPI PFNGLUSEPROGRAMPROC glad_glUseProgram;
GLAPI PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer;
GLAPI PFNGLVIEWPORTPROC glad_glViewport;
GLAPI PFNGLCULLFACEPROC glad_glCullFace;
GLAPI PFNGLFRONTFACEPROC glad_glFrontFace;

/* Macro aliases */
#define glActiveTexture glad_glActiveTexture
#define glAttachShader glad_glAttachShader
#define glBindBuffer glad_glBindBuffer
#define glBindFramebuffer glad_glBindFramebuffer
#define glBindRenderbuffer glad_glBindRenderbuffer
#define glBindTexture glad_glBindTexture
#define glBindVertexArray glad_glBindVertexArray
#define glBlendFunc glad_glBlendFunc
#define glBufferData glad_glBufferData
#define glBufferSubData glad_glBufferSubData
#define glCheckFramebufferStatus glad_glCheckFramebufferStatus
#define glClear glad_glClear
#define glClearColor glad_glClearColor
#define glClearDepthf glad_glClearDepthf
#define glCompileShader glad_glCompileShader
#define glCreateProgram glad_glCreateProgram
#define glCreateShader glad_glCreateShader
#define glDeleteBuffers glad_glDeleteBuffers
#define glDeleteFramebuffers glad_glDeleteFramebuffers
#define glDeleteProgram glad_glDeleteProgram
#define glDeleteRenderbuffers glad_glDeleteRenderbuffers
#define glDeleteShader glad_glDeleteShader
#define glDeleteTextures glad_glDeleteTextures
#define glDeleteVertexArrays glad_glDeleteVertexArrays
#define glDepthFunc glad_glDepthFunc
#define glDepthMask glad_glDepthMask
#define glDisable glad_glDisable
#define glDisableVertexAttribArray glad_glDisableVertexAttribArray
#define glDrawArrays glad_glDrawArrays
#define glDrawElements glad_glDrawElements
#define glEnable glad_glEnable
#define glEnableVertexAttribArray glad_glEnableVertexAttribArray
#define glFramebufferRenderbuffer glad_glFramebufferRenderbuffer
#define glFramebufferTexture2D glad_glFramebufferTexture2D
#define glGenBuffers glad_glGenBuffers
#define glGenerateMipmap glad_glGenerateMipmap
#define glGenFramebuffers glad_glGenFramebuffers
#define glGenRenderbuffers glad_glGenRenderbuffers
#define glGenTextures glad_glGenTextures
#define glGenVertexArrays glad_glGenVertexArrays
#define glGetAttribLocation glad_glGetAttribLocation
#define glGetError glad_glGetError
#define glGetProgramInfoLog glad_glGetProgramInfoLog
#define glGetProgramiv glad_glGetProgramiv
#define glGetShaderInfoLog glad_glGetShaderInfoLog
#define glGetShaderiv glad_glGetShaderiv
#define glGetString glad_glGetString
#define glGetStringi glad_glGetStringi
#define glGetIntegerv glad_glGetIntegerv
#define glGetUniformLocation glad_glGetUniformLocation
#define glLinkProgram glad_glLinkProgram
#define glPolygonMode glad_glPolygonMode
#define glRenderbufferStorage glad_glRenderbufferStorage
#define glScissor glad_glScissor
#define glShaderSource glad_glShaderSource
#define glTexImage2D glad_glTexImage2D
#define glTexParameteri glad_glTexParameteri
#define glUniform1i glad_glUniform1i
#define glUniform1f glad_glUniform1f
#define glUniform2f glad_glUniform2f
#define glUniform3f glad_glUniform3f
#define glUniform4f glad_glUniform4f
#define glUniformMatrix3fv glad_glUniformMatrix3fv
#define glUniformMatrix4fv glad_glUniformMatrix4fv
#define glUseProgram glad_glUseProgram
#define glVertexAttribPointer glad_glVertexAttribPointer
#define glViewport glad_glViewport
#define glCullFace glad_glCullFace
#define glFrontFace glad_glFrontFace

typedef void* (*GLADloadproc)(const char *name);

/* Loader function — call after creating the OpenGL context */
#ifdef __cplusplus
extern "C" {
#endif

int gladLoadGL(void);
int gladLoadGLLoader(GLADloadproc load);

#ifdef __cplusplus
}
#endif

#endif /* __glad_h_ */
