#include "PopUpBase.h"

PopUpBase::PopUpBase(const std::string& Name): name(Name) {
    popUpWindow = new Card(nullptr, 300, 300);
}

void PopUpBase::Render(){};

void PopUpBase::initView(){
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Limpiar pantalla
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, popUpWindow->width, popUpWindow->height); // igual a tu viewport

    glViewport(x, y, popUpWindow->width, popUpWindow->height); // x, y, ancho, alto
    glOrtho(0, popUpWindow->width, popUpWindow->height, 0, -1, 1);

    glDisable(GL_FOG);
    glDisable(GL_DEPTH_TEST);
    glDisable( GL_CULL_FACE );
    glDisable( GL_LIGHTING );
    glEnable( GL_TEXTURE_2D );
    glEnable( GL_BLEND );
    glEnable(GL_COLOR_MATERIAL);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);

    glPushMatrix();   
}

PopUpBase* PopUpActive = nullptr;