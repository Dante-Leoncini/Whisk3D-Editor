#ifndef IMAGE_H
#define IMAGE_H

#include "icons.h"

class Image {
	public:
		//int parent = -1;
    	Object2D& parent;
		int interpolacion = closest;
		int TextureID = 0;
		//colores RGB
		GLubyte color[3] = { 255, 255, 255 };
    	Sprite img;

    	// Constructor completo
		Image(Object2D& p, int texture = 0, GLshort x = 0, GLshort y = 0, GLshort w = 8, GLshort h = 8)
			: parent(p), TextureID(texture){
			img.SetVertices(x, y, w, h);
		}

		void SetColor(GLubyte red, GLubyte green, GLubyte blue){
			color[0] = red;
			color[1] = green;
			color[2] = blue;
		}

		void SetUV(GLshort x, GLshort y, GLshort w, GLshort h){
			img.SetVertices(x, y, w, h);
		}

		void Render() const {
			glColor4ub(color[0], color[1], color[2], parent.opacity);
			glBindTexture(GL_TEXTURE_2D, Textures[TextureID]->iID);

			//esto se hace despues del bind de la textura. sino no le afecta
			if (interpolacion == closest){
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			}
			else {
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}

			img.Render();
		}
};

Image* AddImage(Object2D& parent, int TextureID = 0, GLshort x = 0, GLshort y = 0, GLshort w = 8, GLshort h = 8) {
    return new Image(parent, TextureID, x, y, w, h);
}

// Crear un Text vacío
/*int AddImage(int parent, int TextureID = 0, GLshort x = 0, GLshort y = 0, GLshort w = 8, GLshort h = 8){
	Image img;
	img.parent = parent;
	img.TextureID = TextureID;
	img.SetUV(x, y, w, h);

	// Guardar en el vector de textos
	Images.push_back(img);

	// Retorna el índice
	return static_cast<int>(Images.size()) - 1;	
}*/

#endif