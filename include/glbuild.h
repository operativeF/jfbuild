#ifdef USE_OPENGL

#if defined(_MSC_VER)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <GL/gl.h>
#elif defined(__APPLE__)
#  include <OpenGL/gl.h>
#  define APIENTRY
#else
#  include <GL/gl.h>
#endif

#ifndef GL_GLEXT_VERSION
#  include "glext.h"
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY
#  define GL_MAX_TEXTURE_MAX_ANISOTROPY GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#endif

#ifdef RENDERTYPEWIN
#  include "wglext.h"
#endif

#ifdef DYNAMIC_OPENGL

extern void (APIENTRY * bglClearColor)( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha );
extern void (APIENTRY * bglClear)( GLbitfield mask );
extern void (APIENTRY * bglColorMask)( GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha );
extern void (APIENTRY * bglAlphaFunc)( GLenum func, GLclampf ref );
extern void (APIENTRY * bglBlendFunc)( GLenum sfactor, GLenum dfactor );
extern void (APIENTRY * bglCullFace)( GLenum mode );
extern void (APIENTRY * bglFrontFace)( GLenum mode );
extern void (APIENTRY * bglPolygonOffset)( GLfloat factor, GLfloat units );
extern void (APIENTRY * bglPolygonMode)( GLenum face, GLenum mode );
extern void (APIENTRY * bglEnable)( GLenum cap );
extern void (APIENTRY * bglDisable)( GLenum cap );
extern void (APIENTRY * bglGetFloatv)( GLenum pname, GLfloat *params );
extern void (APIENTRY * bglGetIntegerv)( GLenum pname, GLint *params );
extern void (APIENTRY * bglPushAttrib)( GLbitfield mask );
extern void (APIENTRY * bglPopAttrib)( void );
extern GLenum (APIENTRY * bglGetError)( void );
extern const GLubyte* (APIENTRY * bglGetString)( GLenum name );
extern void (APIENTRY * bglHint)( GLenum target, GLenum mode );
extern void (APIENTRY * bglPixelStorei)( GLenum pname, GLint param );

// Depth
extern void (APIENTRY * bglDepthFunc)( GLenum func );
extern void (APIENTRY * bglDepthMask)( GLboolean flag );
extern void (APIENTRY * bglDepthRange)( GLclampd near_val, GLclampd far_val );

// Matrix
extern void (APIENTRY * bglMatrixMode)( GLenum mode );
extern void (APIENTRY * bglOrtho)( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val );
extern void (APIENTRY * bglFrustum)( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val );
extern void (APIENTRY * bglViewport)( GLint x, GLint y, GLsizei width, GLsizei height );
extern void (APIENTRY * bglPushMatrix)( void );
extern void (APIENTRY * bglPopMatrix)( void );
extern void (APIENTRY * bglLoadIdentity)( void );
extern void (APIENTRY * bglLoadMatrixf)( const GLfloat *m );

// Drawing
extern void (APIENTRY * bglBegin)( GLenum mode );
extern void (APIENTRY * bglEnd)( void );
extern void (APIENTRY * bglVertex2f)( GLfloat x, GLfloat y );
extern void (APIENTRY * bglVertex2i)( GLint x, GLint y );
extern void (APIENTRY * bglVertex3d)( GLdouble x, GLdouble y, GLdouble z );
extern void (APIENTRY * bglVertex3fv)( const GLfloat *v );
extern void (APIENTRY * bglColor4f)( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha );
extern void (APIENTRY * bglColor4ub)( GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha );
extern void (APIENTRY * bglTexCoord2d)( GLdouble s, GLdouble t );
extern void (APIENTRY * bglTexCoord2f)( GLfloat s, GLfloat t );

// Raster funcs
extern void (APIENTRY * bglReadPixels)( GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels );

// Texture mapping
extern void (APIENTRY * bglTexEnvf)( GLenum target, GLenum pname, GLfloat param );
extern void (APIENTRY * bglGenTextures)( GLsizei n, GLuint *textures );	// 1.1
extern void (APIENTRY * bglDeleteTextures)( GLsizei n, const GLuint *textures);	// 1.1
extern void (APIENTRY * bglBindTexture)( GLenum target, GLuint texture );	// 1.1
extern void (APIENTRY * bglTexImage1D)( GLenum target, GLint level,
								   GLint internalFormat,
								   GLsizei width, GLint border,
								   GLenum format, GLenum type,
								   const GLvoid *pixels );
extern void (APIENTRY * bglTexImage2D)( GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels );
extern void (APIENTRY * bglTexSubImage2D)( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels );	// 1.1
extern void (APIENTRY * bglTexParameterf)( GLenum target, GLenum pname, GLfloat param );
extern void (APIENTRY * bglTexParameteri)( GLenum target, GLenum pname, GLint param );
extern void (APIENTRY * bglGetTexLevelParameteriv)( GLenum target, GLint level, GLenum pname, GLint *params );
extern void (APIENTRY * bglCompressedTexImage2DARB)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const GLvoid *);
extern void (APIENTRY * bglGetCompressedTexImageARB)(GLenum, GLint, GLvoid *);

// Buffer objects
extern void (APIENTRY * bglBindBuffer)(GLenum target, GLuint buffer);
extern void (APIENTRY * bglBufferData)(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage);
extern void (APIENTRY * bglBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data);
extern void (APIENTRY * bglDeleteBuffers)(GLsizei n, const GLuint * buffers);
extern void (APIENTRY * bglGenBuffers)(GLsizei n, GLuint * buffers);
extern void (APIENTRY * bglDrawElements)( GLenum mode, GLsizei count, GLenum type, const GLvoid *indices );
extern void (APIENTRY * bglEnableVertexAttribArray)(GLuint index);
extern void (APIENTRY * bglDisableVertexAttribArray)(GLuint index);
extern void (APIENTRY * bglVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer);

// Shaders
extern void (APIENTRY * bglActiveTexture)( GLenum texture );
extern void (APIENTRY * bglAttachShader)(GLuint program, GLuint shader);
extern void (APIENTRY * bglCompileShader)(GLuint shader);
extern GLuint (APIENTRY * bglCreateProgram)(void);
extern GLuint (APIENTRY * bglCreateShader)(GLenum type);
extern void (APIENTRY * bglDeleteProgram)(GLuint program);
extern void (APIENTRY * bglDeleteShader)(GLuint shader);
extern void (APIENTRY * bglDetachShader)(GLuint program, GLuint shader);
extern GLint (APIENTRY * bglGetAttribLocation)(GLuint program, const GLchar *name);
extern void (APIENTRY * bglGetProgramiv)(GLuint program, GLenum pname, GLint *params);
extern void (APIENTRY * bglGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
extern void (APIENTRY * bglGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
extern void (APIENTRY * bglGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
extern GLint (APIENTRY * bglGetUniformLocation)(GLuint program, const GLchar *name);
extern void (APIENTRY * bglLinkProgram)(GLuint program);
extern void (APIENTRY * bglShaderSource)(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
extern void (APIENTRY * bglUniform1i)(GLint location, GLint v0);
extern void (APIENTRY * bglUniform1f)(GLint location, GLfloat v0);
extern void (APIENTRY * bglUniform2f)(GLint location, GLfloat v0, GLfloat v1);
extern void (APIENTRY * bglUniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
extern void (APIENTRY * bglUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
extern void (APIENTRY * bglUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern void (APIENTRY * bglUseProgram)(GLuint program);

#ifdef RENDERTYPEWIN
// Windows
extern HGLRC (WINAPI * bwglCreateContext)(HDC);
extern BOOL (WINAPI * bwglDeleteContext)(HGLRC);
extern PROC (WINAPI * bwglGetProcAddress)(LPCSTR);
extern BOOL (WINAPI * bwglMakeCurrent)(HDC,HGLRC);
extern BOOL (WINAPI * bwglSwapBuffers)(HDC);

extern const char * (WINAPI * bwglGetExtensionsStringARB)(HDC hdc);
extern BOOL (WINAPI * bwglChoosePixelFormatARB)(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
extern HGLRC (WINAPI * bwglCreateContextAttribsARB)(HDC hDC, HGLRC hShareContext, const int *attribList);
#endif

#else	// DYNAMIC_OPENGL

#define bglClearColor		glClearColor
#define bglClear		glClear
#define bglColorMask		glColorMask
#define bglAlphaFunc		glAlphaFunc
#define bglBlendFunc		glBlendFunc
#define bglCullFace		glCullFace
#define bglFrontFace		glFrontFace
#define bglPolygonOffset	glPolygonOffset
#define bglPolygonMode    glPolygonMode
#define bglEnable		glEnable
#define bglDisable		glDisable
#define bglGetFloatv		glGetFloatv
#define bglGetIntegerv		glGetIntegerv
#define bglPushAttrib		glPushAttrib
#define bglPopAttrib		glPopAttrib
#define bglGetError		glGetError
#define bglGetString		glGetString
#define bglHint			glHint
#define bglPixelStorei	glPixelStorei

// Depth
#define bglDepthFunc		glDepthFunc
#define bglDepthMask		glDepthMask
#define bglDepthRange		glDepthRange

// Matrix
#define bglMatrixMode		glMatrixMode
#define bglOrtho		glOrtho
#define bglFrustum		glFrustum
#define bglViewport		glViewport
#define bglPushMatrix		glPushMatrix
#define bglPopMatrix		glPopMatrix
#define bglLoadIdentity		glLoadIdentity
#define bglLoadMatrixf		glLoadMatrixf

// Drawing
#define bglBegin		glBegin
#define bglEnd			glEnd
#define bglVertex2f		glVertex2f
#define bglVertex2i		glVertex2i
#define bglVertex3d		glVertex3d
#define bglVertex3fv		glVertex3fv
#define bglColor4f		glColor4f
#define bglColor4ub		glColor4ub
#define bglTexCoord2d		glTexCoord2d
#define bglTexCoord2f       glTexCoord2f

// Raster funcs
#define bglReadPixels		glReadPixels

// Texture mapping
#define bglTexEnvf		glTexEnvf
#define bglGenTextures		glGenTextures
#define bglDeleteTextures	glDeleteTextures
#define bglBindTexture		glBindTexture
#define bglTexImage1D		glTexImage1D
#define bglTexImage2D		glTexImage2D
#define bglTexSubImage2D	glTexSubImage2D
#define bglTexParameterf	glTexParameterf
#define bglTexParameteri	glTexParameteri
#define bglGetTexLevelParameteriv glGetTexLevelParameteriv
#define bglCompressedTexImage2DARB glCompressedTexImage2DARB
#define bglGetCompressedTexImageARB glGetCompressedTexImageARB

// Buffer objects
#define bglBindBuffer    glBindBuffer
#define bglBufferData    glBufferData
#define bglBufferSubData    glBufferSubData
#define bglDeleteBuffers glDeleteBuffers
#define bglGenBuffers    glGenBuffers
#define bglDrawElements  glDrawElements
#define bglEnableVertexAttribArray glEnableVertexAttribArray
#define bglDisableVertexAttribArray glDisableVertexAttribArray
#define bglVertexAttribPointer glVertexAttribPointer

// Shaders
#define bglActiveTexture glActiveTexture
#define bglAttachShader  glAttachShader
#define bglCompileShader glCompileShader
#define bglCreateProgram glCreateProgram
#define bglCreateShader  glCreateShader
#define bglDeleteProgram glDeleteProgram
#define bglDeleteShader  glDeleteShader
#define bglDetachShader  glDetachShader
#define bglGetAttribLocation glGetAttribLocation
#define bglGetProgramiv  glGetProgramiv
#define bglGetProgramInfoLog glGetProgramInfoLog
#define bglGetShaderiv   glGetShaderiv
#define bglGetShaderInfoLog glGetShaderInfoLog
#define bglGetUniformLocation glGetUniformLocation
#define bglLinkProgram   glLinkProgram
#define bglShaderSource  glShaderSource
#define bglUseProgram    glUseProgram
#define bglUniform1i     glUniform1i
#define bglUniform1f     glUniform1f
#define bglUniform2f     glUniform2f
#define bglUniform3f     glUniform3f
#define bglUniform4f     glUniform4f
#define bglUniformMatrix4fv glUniformMatrix4fv

#ifdef RENDERTYPEWIN
#define bwglCreateContext	wglCreateContext
#define bwglDeleteContext	wglDeleteContext
#define bwglGetProcAddress	wglGetProcAddress
#define bwglMakeCurrent		wglMakeCurrent
#define bwglSwapBuffers     wglSwapBuffers

#define bwglGetExtensionsStringARB  wglGetExtensionsStringARB
#define bwglChoosePixelFormatARB    wglChoosePixelFormatARB
#define bwglCreateContextAttribsARB wglCreateContentAttribsARB
#endif

#endif

int loadglfunctions(int all);   // all==0: the basic ones needed to bootstrap
void unloadglfunctions(void);

GLuint glbuild_compile_shader(GLuint type, const GLchar *source);
GLuint glbuild_link_program(int shadercount, GLuint *shaders);
int glbuild_prepare_8bit_shader(GLuint *paltex, GLuint *frametex, GLuint *program, int resx, int resy);		// <0 = error

#endif //USE_OPENGL
