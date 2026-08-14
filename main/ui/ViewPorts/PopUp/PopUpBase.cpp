#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "PopUpBase.h"

PopUpBase* PopUpActive = NULL;

PopUpBase::PopUpBase(const std::string& Name) {
    // (eran inicializadores de clase: C++03)
    name = Name;
    x = 30;
    y = 30;
    popUpWindow = new Card(NULL, 300, 300);
}

PopUpBase::~PopUpBase() {
    delete popUpWindow;
}

bool PopUpBase::Contains(int mx, int my) const {
    return mx >= x && mx < x + popUpWindow->width &&
           my >= y && my < y + popUpWindow->height;
}

void PopUpBase::initView() {
    w3dEngine::MatrixMode(w3dEngine::Projection);
    w3dEngine::LoadIdentity();

    w3dEngine::MatrixMode(w3dEngine::ModelView);
    w3dEngine::LoadIdentity();

    // arbol arriba-izquierda -> GL abajo-izquierda (4 OS)
    const int glY = W3dPantallaAlto - y - popUpWindow->height;
    w3dEngine::Enable(w3dEngine::ScissorTest);
    w3dEngine::Scissor(x, glY, popUpWindow->width, popUpWindow->height);
    w3dEngine::Viewport(x, glY, popUpWindow->width, popUpWindow->height);
    w3dEngine::Ortho(0, popUpWindow->width, popUpWindow->height, 0, -1, 1);

    w3dEngine::Disable(w3dEngine::Fog);
    w3dEngine::Disable(w3dEngine::DepthTest);
    w3dEngine::Disable(w3dEngine::CullFace);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::BlendAlpha();
    w3dEngine::Enable(w3dEngine::ColorMaterial);

    w3dEngine::EnableArray(w3dEngine::VertexArray);
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);

    w3dEngine::PushMatrix();
}

void PopUpBase::endView() {
    w3dEngine::PopMatrix();
    w3dEngine::Disable(w3dEngine::ScissorTest);
}

void PopUpBase::Render() {}

bool PopUpBase::Click(int mx, int my) {
    return Contains(mx, my);
}

bool PopUpBase::Motion(int mx, int my) {
    return true;
}

bool PopUpBase::Tecla(int tecla) {
    return true;
}

void PopUpBase::Soltar() {}

bool PopUpBase::Arrastrando() {
    return false;
}

void PopUpBase::Cerrar() {
    if (PopUpActive == this) PopUpActive = NULL;
}
