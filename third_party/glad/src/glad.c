/*
 * GLAD OpenGL 3.3 Core Loader Implementation
 */
#include "glad/glad.h"
#include <string.h>
#include <stdlib.h>

/* Function pointer definitions */
PFNGLACTIVETEXTUREPROC glad_glActiveTexture = NULL;
PFNGLATTACHSHADERPROC glad_glAttachShader = NULL;
PFNGLBINDBUFFERPROC glad_glBindBuffer = NULL;
PFNGLBINDFRAMEBUFFERPROC glad_glBindFramebuffer = NULL;
PFNGLBINDRENDERBUFFERPROC glad_glBindRenderbuffer = NULL;
PFNGLBINDTEXTUREPROC glad_glBindTexture = NULL;
PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray = NULL;
PFNGLBLENDFUNCPROC glad_glBlendFunc = NULL;
PFNGLBUFFERDATAPROC glad_glBufferData = NULL;
PFNGLBUFFERSUBDATAPROC glad_glBufferSubData = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_glCheckFramebufferStatus = NULL;
PFNGLCLEARPROC glad_glClear = NULL;
PFNGLCLEARCOLORPROC glad_glClearColor = NULL;
PFNGLCLEARDEPTHFPROC glad_glClearDepthf = NULL;
PFNGLCOMPILESHADERPROC glad_glCompileShader = NULL;
PFNGLCREATEPROGRAMPROC glad_glCreateProgram = NULL;
PFNGLCREATESHADERPROC glad_glCreateShader = NULL;
PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers = NULL;
PFNGLDELETEFRAMEBUFFERSPROC glad_glDeleteFramebuffers = NULL;
PFNGLDELETEPROGRAMPROC glad_glDeleteProgram = NULL;
PFNGLDELETERENDERBUFFERSPROC glad_glDeleteRenderbuffers = NULL;
PFNGLDELETESHADERPROC glad_glDeleteShader = NULL;
PFNGLDELETETEXTURESPROC glad_glDeleteTextures = NULL;
PFNGLDELETEVERTEXARRAYSPROC glad_glDeleteVertexArrays = NULL;
PFNGLDEPTHFUNCPROC glad_glDepthFunc = NULL;
PFNGLDEPTHMASKPROC glad_glDepthMask = NULL;
PFNGLDISABLEPROC glad_glDisable = NULL;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_glDisableVertexAttribArray = NULL;
PFNGLDRAWARRAYSPROC glad_glDrawArrays = NULL;
PFNGLDRAWELEMENTSPROC glad_glDrawElements = NULL;
PFNGLENABLEPROC glad_glEnable = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray = NULL;
PFNGLFRAMEBUFFERRENDERBUFFERPROC glad_glFramebufferRenderbuffer = NULL;
PFNGLFRAMEBUFFERTEXTURE2DPROC glad_glFramebufferTexture2D = NULL;
PFNGLGENBUFFERSPROC glad_glGenBuffers = NULL;
PFNGLGENERATEMIPMAPPROC glad_glGenerateMipmap = NULL;
PFNGLGENFRAMEBUFFERSPROC glad_glGenFramebuffers = NULL;
PFNGLGENRENDERBUFFERSPROC glad_glGenRenderbuffers = NULL;
PFNGLGENTEXTURESPROC glad_glGenTextures = NULL;
PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays = NULL;
PFNGLGETATTRIBLOCATIONPROC glad_glGetAttribLocation = NULL;
PFNGLGETERRORPROC glad_glGetError = NULL;
PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog = NULL;
PFNGLGETPROGRAMIVPROC glad_glGetProgramiv = NULL;
PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog = NULL;
PFNGLGETSHADERIVPROC glad_glGetShaderiv = NULL;
PFNGLGETSTRINGPROC glad_glGetString = NULL;
PFNGLGETSTRINGIPROC glad_glGetStringi = NULL;
PFNGLGETINTEGERVPROC glad_glGetIntegerv = NULL;
PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation = NULL;
PFNGLLINKPROGRAMPROC glad_glLinkProgram = NULL;
PFNGLPOLYGONMODEPROC glad_glPolygonMode = NULL;
PFNGLRENDERBUFFERSTORAGEPROC glad_glRenderbufferStorage = NULL;
PFNGLSCISSORPROC glad_glScissor = NULL;
PFNGLSHADERSOURCEPROC glad_glShaderSource = NULL;
PFNGLTEXIMAGE2DPROC glad_glTexImage2D = NULL;
PFNGLTEXPARAMETERIPROC glad_glTexParameteri = NULL;
PFNGLUNIFORM1IPROC glad_glUniform1i = NULL;
PFNGLUNIFORM1FPROC glad_glUniform1f = NULL;
PFNGLUNIFORM2FPROC glad_glUniform2f = NULL;
PFNGLUNIFORM3FPROC glad_glUniform3f = NULL;
PFNGLUNIFORM4FPROC glad_glUniform4f = NULL;
PFNGLUNIFORMMATRIX3FVPROC glad_glUniformMatrix3fv = NULL;
PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv = NULL;
PFNGLUSEPROGRAMPROC glad_glUseProgram = NULL;
PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer = NULL;
PFNGLVIEWPORTPROC glad_glViewport = NULL;
PFNGLCULLFACEPROC glad_glCullFace = NULL;
PFNGLFRONTFACEPROC glad_glFrontFace = NULL;

static GLADloadproc s_loader = NULL;

static void *get_proc(const char *name) {
    if (s_loader) return s_loader(name);
    return NULL;
}

int gladLoadGLLoader(GLADloadproc load) {
    s_loader = load;

    glad_glActiveTexture = (PFNGLACTIVETEXTUREPROC)get_proc("glActiveTexture");
    glad_glAttachShader = (PFNGLATTACHSHADERPROC)get_proc("glAttachShader");
    glad_glBindBuffer = (PFNGLBINDBUFFERPROC)get_proc("glBindBuffer");
    glad_glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)get_proc("glBindFramebuffer");
    glad_glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC)get_proc("glBindRenderbuffer");
    glad_glBindTexture = (PFNGLBINDTEXTUREPROC)get_proc("glBindTexture");
    glad_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)get_proc("glBindVertexArray");
    glad_glBlendFunc = (PFNGLBLENDFUNCPROC)get_proc("glBlendFunc");
    glad_glBufferData = (PFNGLBUFFERDATAPROC)get_proc("glBufferData");
    glad_glBufferSubData = (PFNGLBUFFERSUBDATAPROC)get_proc("glBufferSubData");
    glad_glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)get_proc("glCheckFramebufferStatus");
    glad_glClear = (PFNGLCLEARPROC)get_proc("glClear");
    glad_glClearColor = (PFNGLCLEARCOLORPROC)get_proc("glClearColor");
    glad_glClearDepthf = (PFNGLCLEARDEPTHFPROC)get_proc("glClearDepthf");
    glad_glCompileShader = (PFNGLCOMPILESHADERPROC)get_proc("glCompileShader");
    glad_glCreateProgram = (PFNGLCREATEPROGRAMPROC)get_proc("glCreateProgram");
    glad_glCreateShader = (PFNGLCREATESHADERPROC)get_proc("glCreateShader");
    glad_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)get_proc("glDeleteBuffers");
    glad_glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)get_proc("glDeleteFramebuffers");
    glad_glDeleteProgram = (PFNGLDELETEPROGRAMPROC)get_proc("glDeleteProgram");
    glad_glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC)get_proc("glDeleteRenderbuffers");
    glad_glDeleteShader = (PFNGLDELETESHADERPROC)get_proc("glDeleteShader");
    glad_glDeleteTextures = (PFNGLDELETETEXTURESPROC)get_proc("glDeleteTextures");
    glad_glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)get_proc("glDeleteVertexArrays");
    glad_glDepthFunc = (PFNGLDEPTHFUNCPROC)get_proc("glDepthFunc");
    glad_glDepthMask = (PFNGLDEPTHMASKPROC)get_proc("glDepthMask");
    glad_glDisable = (PFNGLDISABLEPROC)get_proc("glDisable");
    glad_glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)get_proc("glDisableVertexAttribArray");
    glad_glDrawArrays = (PFNGLDRAWARRAYSPROC)get_proc("glDrawArrays");
    glad_glDrawElements = (PFNGLDRAWELEMENTSPROC)get_proc("glDrawElements");
    glad_glEnable = (PFNGLENABLEPROC)get_proc("glEnable");
    glad_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)get_proc("glEnableVertexAttribArray");
    glad_glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)get_proc("glFramebufferRenderbuffer");
    glad_glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)get_proc("glFramebufferTexture2D");
    glad_glGenBuffers = (PFNGLGENBUFFERSPROC)get_proc("glGenBuffers");
    glad_glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC)get_proc("glGenerateMipmap");
    glad_glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)get_proc("glGenFramebuffers");
    glad_glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC)get_proc("glGenRenderbuffers");
    glad_glGenTextures = (PFNGLGENTEXTURESPROC)get_proc("glGenTextures");
    glad_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)get_proc("glGenVertexArrays");
    glad_glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)get_proc("glGetAttribLocation");
    glad_glGetError = (PFNGLGETERRORPROC)get_proc("glGetError");
    glad_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)get_proc("glGetProgramInfoLog");
    glad_glGetProgramiv = (PFNGLGETPROGRAMIVPROC)get_proc("glGetProgramiv");
    glad_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)get_proc("glGetShaderInfoLog");
    glad_glGetShaderiv = (PFNGLGETSHADERIVPROC)get_proc("glGetShaderiv");
    glad_glGetString = (PFNGLGETSTRINGPROC)get_proc("glGetString");
    glad_glGetStringi = (PFNGLGETSTRINGIPROC)get_proc("glGetStringi");
    glad_glGetIntegerv = (PFNGLGETINTEGERVPROC)get_proc("glGetIntegerv");
    glad_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)get_proc("glGetUniformLocation");
    glad_glLinkProgram = (PFNGLLINKPROGRAMPROC)get_proc("glLinkProgram");
    glad_glPolygonMode = (PFNGLPOLYGONMODEPROC)get_proc("glPolygonMode");
    glad_glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)get_proc("glRenderbufferStorage");
    glad_glScissor = (PFNGLSCISSORPROC)get_proc("glScissor");
    glad_glShaderSource = (PFNGLSHADERSOURCEPROC)get_proc("glShaderSource");
    glad_glTexImage2D = (PFNGLTEXIMAGE2DPROC)get_proc("glTexImage2D");
    glad_glTexParameteri = (PFNGLTEXPARAMETERIPROC)get_proc("glTexParameteri");
    glad_glUniform1i = (PFNGLUNIFORM1IPROC)get_proc("glUniform1i");
    glad_glUniform1f = (PFNGLUNIFORM1FPROC)get_proc("glUniform1f");
    glad_glUniform2f = (PFNGLUNIFORM2FPROC)get_proc("glUniform2f");
    glad_glUniform3f = (PFNGLUNIFORM3FPROC)get_proc("glUniform3f");
    glad_glUniform4f = (PFNGLUNIFORM4FPROC)get_proc("glUniform4f");
    glad_glUniformMatrix3fv = (PFNGLUNIFORMMATRIX3FVPROC)get_proc("glUniformMatrix3fv");
    glad_glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)get_proc("glUniformMatrix4fv");
    glad_glUseProgram = (PFNGLUSEPROGRAMPROC)get_proc("glUseProgram");
    glad_glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)get_proc("glVertexAttribPointer");
    glad_glViewport = (PFNGLVIEWPORTPROC)get_proc("glViewport");
    glad_glCullFace = (PFNGLCULLFACEPROC)get_proc("glCullFace");
    glad_glFrontFace = (PFNGLFRONTFACEPROC)get_proc("glFrontFace");

    /* Verify a core function loaded successfully */
    return glad_glCreateProgram != NULL ? 1 : 0;
}

int gladLoadGL(void) {
    /* Can't load without a proc provider */
    return 0;
}
