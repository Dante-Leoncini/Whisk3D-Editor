#ifndef REC2D_H
#define REC2D_H

#ifdef _WIN32
    #include <windows.h>
#endif

#ifdef __ANDROID__
    #include <GLES/gl.h>
#else
    #include <GL/gl.h>
#endif

#include "UI/object2D.h"

class Rec2D : public Object2D {
	public:
		GLshort vertices[8] = { 0,0, 10,0, 0,10, 10,10 };

		// Constructor
		Rec2D(Object2D* parent = nullptr);

		// Métodos
		void SetSize(GLshort x, GLshort y, GLshort w, GLshort h);
		void SetColor(GLubyte red, GLubyte green, GLubyte blue);
		void RenderObject(bool usarColorPropio = true) override;
};

#endif
