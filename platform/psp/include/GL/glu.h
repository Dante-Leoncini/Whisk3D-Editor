#pragma once
// GL/glu.h de la PSP: solo lo que nombra el Core (ver GL/gl.h). En la PSP las texturas van por UploadRGBA.
#include <GL/gl.h>
#ifdef __cplusplus
extern "C" {
#endif
int gluBuild2DMipmaps(GLenum target, GLint internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* data);
#ifdef __cplusplus
}
#endif
