#ifndef CARD_H
#define CARD_H

#ifdef _WIN32
    #include <windows.h>
#endif

#ifdef __ANDROID__
    #include <GLES/gl.h>
#else
    #include <GL/gl.h>
#endif

#include "UI/object2D.h"
#include "UI/UI.h"
#include "font.h"

// Variables UV/indices
extern GLubyte CardIndices[];
extern GLfloat CardUV[32];
extern GLfloat CardBordeUV[32];

void CalcCardUV(int texW, int texH);

class Card : public Object2D {
	public:
		GLshort mesh[32] = { 
            0,0,   6,0,   12,0,   18,0,
            0,6,   6,6,   12,6,   18,6,
            0,12,  6,12,  12,12,  18,12,
            0,18,  6,18,  12,18,  18,18
        };

		// Constructor
		Card(Object2D* parent = nullptr, int w = 10, int h = 10);

		// Métodos
		void Resize(int w, int h);
		void SetColor(GLubyte red, GLubyte green, GLubyte blue);
		void RenderObject(bool usarColorPropio = true) override;
        void RenderBorder(bool usarColorPropio);
};

#endif
