#include "./sprites.h"
#include "./object2D.h"

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
	/*glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texW);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texH);
	std::cout << "Tex size in GPU: " << texW << "x" << texH << std::endl;*/

	//esto se hace despues del bind de la textura. sino no le afecta
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

	//std::string text = "ABCDEFGHIJKLMNÑOPQRSTUVWXYZ abcdefghijklmnñopqrstuvwyz 0123456789+-= ()[]{}<>/*:#%!?.,'\"@&$";

	for (size_t i = 0; i < Objects2D.size(); i++) {
		RenderObject2D(i);
	}
}