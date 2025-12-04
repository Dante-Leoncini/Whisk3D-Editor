#ifndef SPRITES_H
#define SPRITES_H

class Sprite {
	public:
	    GLfloat uvs[8] = {
			0.0f, 0.0f,
			1.0f, 0.0f,
			0.0f, 1.0f,
			1.0f, 1.0f
		};
		GLshort vertices[8] = { 0,0, 1,0, 0,1, 1,1 };

		void SetUV(int texW, int texH, int x, int y, int w, int h) {
			GLfloat u1 = (GLfloat)x / texW;
			GLfloat u2 = (GLfloat)(x + w) / texW;

			GLfloat v1 = (GLfloat)y / texH;
			GLfloat v2 = (GLfloat)(y + h) / texH;

			// top-left
			uvs[0] = u1; uvs[1] = v1;
			// top-right
			uvs[2] = u2; uvs[3] = v1;
			// bottom-left
			uvs[4] = u1; uvs[5] = v2;
			// bottom-right
			uvs[6] = u2; uvs[7] = v2;
		}

		// Calcular los vértices una sola vez
		void SetVertices(GLshort x, GLshort y, GLshort w, GLshort h) {
			vertices[0] = (GLshort)x;
			vertices[1] = (GLshort)y;

			vertices[2] = (GLshort)(x + w);
			vertices[3] = (GLshort)y;

			vertices[4] = (GLshort)x;
			vertices[5] = (GLshort)(y + h);

			vertices[6] = (GLshort)(x + w);
			vertices[7] = (GLshort)(y + h);
		}

		// Calcular los vértices una sola vez
		void SetX(int x, int w) {
			vertices[0] = vertices[4] = (GLfloat)x;
			vertices[2] = vertices[6] = (GLfloat)(x + w);
		}

		void SetY(int y, int h) {
			vertices[1] = vertices[3] = (GLfloat)y;       // parte superior
			vertices[5] = vertices[7] = (GLfloat)(y + h); // parte inferior
		}

		void Render() const {
			// Dibujar con TRIANGLE_STRIP (dos triángulos)
			glTexCoordPointer(2, GL_FLOAT, 0, uvs);
    		glVertexPointer(2, GL_SHORT, 0, vertices);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
};

//Sprite sprite;

#endif