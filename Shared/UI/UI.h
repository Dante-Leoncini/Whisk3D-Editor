#include "./font.h"
#include "./sprites.h"

void DibujarSprite(int x, int y, int w, int h) {
	// Rellenar array global
    sprite.vertices[0] = (GLfloat)x;
    sprite.vertices[1] = (GLfloat)y;

    sprite.vertices[2] = (GLfloat)(x + w);
    sprite.vertices[3] = (GLfloat)y;

    sprite.vertices[4] = (GLfloat)x;
    sprite.vertices[5] = (GLfloat)(y + h);

    sprite.vertices[6] = (GLfloat)(x + w);
    sprite.vertices[7] = (GLfloat)(y + h);

    // Dibujar con TRIANGLE_STRIP (dos triángulos)
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void DibujarUI() {
    // Guardar matrices
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, winW, winH, 0, -1, 1);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
	glEnable( GL_BLEND );

	glColor4f(ListaColores[blanco][0],ListaColores[blanco][1],ListaColores[blanco][2],ListaColores[blanco][3]);

    //int texW, texH;
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glBindTexture(GL_TEXTURE_2D, Textures[0].iID);
	/*glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texW);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texH);
	std::cout << "Tex size in GPU: " << texW << "x" << texH << std::endl;*/

	//esto se hace despues del bind de la textura. sino no le afecta
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glVertexPointer(2, GL_FLOAT, 0, sprite.vertices);

	//std::string text = "ABCDEFGHIJKLMNÑOPQRSTUVWXYZ abcdefghijklmnñopqrstuvwyz 0123456789+-= ()[]{}<>/*:#%!?.,'\"@&$";
	std::string text = "Whisk3D pre-alpha 25-10-04";

	//SetScaleFont(1);

	int glyphIndex = 0;
	for (size_t i = 0; i < text.size();) {
		unsigned char c = text[i];

		std::string letra;

		if (c < 128) {
			// ASCII normal
			letra = std::string(1, c);
			i++;
		} else {
			// UTF-8 multibyte (ej: Ñ ocupa 2 bytes: 0xC3 0x91)
			letra = text.substr(i, 2);
			i += 2;
		}

		const GLfloat* uv = font.getUV(letra);
		glTexCoordPointer(2, GL_FLOAT, 0, uv);
    	DibujarSprite(5+(glyphIndex*24), 0, 20, 44);
		glyphIndex++;
	}
}