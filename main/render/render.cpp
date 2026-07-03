#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "render.h"
#include "objects/Light.h"
#include "WhiskUI/glesdraw.h"
#include "WhiskUI/UI.h" // GlobalScale (point sprites relativos)
#include "ui/W3dColors.h" // W3dColores: colores del editor (ejes de transformacion)

// Definiciones de funciones

void DrawnLines(int LineWidth, int cantidad, GLshort* vertexlines, GLushort* lineasIndices){
    W3dDrawLinesS(LineWidth, cantidad, vertexlines, lineasIndices);
}

void DrawnLines(int LineWidth, int cantidad, const GLshort* vertexlines, const GLushort* lineasIndices) {
    W3dDrawLinesS(LineWidth, cantidad, vertexlines, lineasIndices);
}

void RenderLinkLines(Object* obj){
    // espacio MUNDO con GetGlobalPosition (el viejo trasladaba (x,z,y)
    // sin la rotacion del padre: la linea quedaba mal al emparentar)
    static GLfloat pool[16][6]; // pool rotativo (captura tardia del driver)
    static int slot = 0;
    for (size_t c = 0; c < obj->Childrens.size(); c++) {
        Object* objChild = obj->Childrens[c];
        if (!objChild->visible || !objChild->showRelantionshipsLines ) continue;
        if (obj->getType()!= ObjectType::collection && obj->getType() != ObjectType::baseObject){
            Vector3 a = obj->GetGlobalPosition();
            Vector3 b = objChild->GetGlobalPosition();
            GLfloat* v = pool[slot];
            slot = (slot + 1) % 16;
            v[0] = a.x; v[1] = a.y; v[2] = a.z;
            v[3] = b.x; v[4] = b.y; v[5] = b.z;

            Vector3 diff = b - a;
            GLfloat distancia = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

            lineUV[3] = distancia*8;
            W3dDrawLinesF(v, LineaEdge, LineaEdgeSize);
        }
        RenderLinkLines(objChild);
    }
}

void DrawTransformAxis() {
    w3dEngine::PushMatrix();
    if (InteractionMode == ObjectMode){
        w3dEngine::Translatef(TransformPivotPoint.x/65000,
                     TransformPivotPoint.y/65000,
                     TransformPivotPoint.z/65000);
    }

    switch (axisSelect) {
        case X:
            w3dSetColor(W3dColores[W3dColor_ColorTransformX]);
            w3dEngine::DrawLinesIndexed(2, EjeRojo);
            break;
        case Y:
            w3dSetColor(W3dColores[W3dColor_ColorTransformY]);
            w3dEngine::DrawLinesIndexed(2, EjeVerde);
            break;
        case Z:
            w3dSetColor(W3dColores[W3dColor_ColorTransformZ]);
            w3dEngine::DrawLinesIndexed(2, EjeAzul);
            break;
        case XYZ:
            w3dSetColor(W3dColores[W3dColor_ColorTransformX]);
            w3dEngine::DrawLinesIndexed(2, EjeRojo);
            w3dSetColor(W3dColores[W3dColor_ColorTransformY]);
            w3dEngine::DrawLinesIndexed(2, EjeVerde);
            w3dSetColor(W3dColores[W3dColor_ColorTransformZ]);
            w3dEngine::DrawLinesIndexed(2, EjeAzul);
            break;
        // PLANOS (Shift+eje): dibujan SOLO los dos ejes del plano (el excluido NO)
        case PlaneX: // excluye X -> Y, Z
            w3dSetColor(W3dColores[W3dColor_ColorTransformY]);
            w3dEngine::DrawLinesIndexed(2, EjeVerde);
            w3dSetColor(W3dColores[W3dColor_ColorTransformZ]);
            w3dEngine::DrawLinesIndexed(2, EjeAzul);
            break;
        case PlaneY: // excluye Y -> X, Z
            w3dSetColor(W3dColores[W3dColor_ColorTransformX]);
            w3dEngine::DrawLinesIndexed(2, EjeRojo);
            w3dSetColor(W3dColores[W3dColor_ColorTransformZ]);
            w3dEngine::DrawLinesIndexed(2, EjeAzul);
            break;
        case PlaneZ: // excluye Z -> X, Y
            w3dSetColor(W3dColores[W3dColor_ColorTransformX]);
            w3dEngine::DrawLinesIndexed(2, EjeRojo);
            w3dSetColor(W3dColores[W3dColor_ColorTransformY]);
            w3dEngine::DrawLinesIndexed(2, EjeVerde);
            break;
    }
    w3dEngine::PopMatrix();
}

bool RenderAxisTransform(Object* obj) {
    bool found = false;
    w3dEngine::PushMatrix();

    obj->GetMatrix(obj->M);
    w3dEngine::MultMatrix(obj->M.m);

    if (obj == ObjActivo) {
        if (estado == rotacion || estado == EditScale){
            /*w3dEngine::Rotatef(obj->rotX, 1, 0, 0);
            w3dEngine::Rotatef(obj->rotZ, 0, 1, 0);
            w3dEngine::Rotatef(obj->rotY, 0, 0, 1);*/
        }
        if (estado == translacion || estado == rotacion || estado == EditScale){
            DrawTransformAxis();
        }
        found = true;
    }
    else if (!obj->Childrens.empty()){
        /*w3dEngine::Rotatef(obj->rotX, 1, 0, 0);
        w3dEngine::Rotatef(obj->rotZ, 0, 1, 0);
        w3dEngine::Rotatef(obj->rotY, 0, 0, 1);*/

        for (size_t c = 0; c < obj->Childrens.size(); c++) {
            if (RenderAxisTransform(obj->Childrens[c])){
                found = true;
                break;
            }
        }
    }
    w3dEngine::PopMatrix();
    return found;
}

void DibujarOrigen(Object* obj){
    if (!obj->visible) return;

    w3dEngine::PushMatrix();

    obj->GetMatrix(obj->M);
    w3dEngine::MultMatrix(obj->M.m);

    if (obj->select || obj == ObjActivo){
        if (obj == ObjActivo){
            w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accent)]);
        }
        else {
            w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accentDark)]);
        }
        w3dEngine::DrawPoints(1);
    }

    if (!obj->Childrens.empty()){
        /*w3dEngine::Rotatef(obj->rotX, 1, 0, 0);
        w3dEngine::Rotatef(obj->rotZ, 0, 1, 0);
        w3dEngine::Rotatef(obj->rotY, 0, 0, 1);*/
        for (size_t c = 0; c < obj->Childrens.size(); c++) {
            DibujarOrigen(obj->Childrens[c]);
        }
    }
    w3dEngine::PopMatrix();
}

void RenderOrigins(){
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::BindTexture(Textures[1]->iID);
#ifndef W3D_SYMBIAN
    // pixel perfect: sin filtrado (se veia borroso al escalar)
    w3dEngine::TexFilter(false);
    w3dEngine::TexFilter(false);
#endif
    w3dEngine::VertexPointer3s(0, pointVertex);
    w3dEngine::PointSpriteCoordReplace(true);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::Enable(w3dEngine::PointSprite);
    w3dEngine::PointSize(8 * GlobalScale); // origen: 8 relativo a la UI
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {
        DibujarOrigen(SceneCollection->Childrens[c]);
    }
    w3dEngine::PointSpriteCoordReplace(false);
    w3dEngine::Disable(w3dEngine::PointSprite);
}

// visible considerando TODOS los ancestros (ocultar la coleccion
// tambien oculta la linea de la lampara al piso)
static bool VisibleEnArbol(Object* o) {
    while (o) {
        if (!o->visible) return false;
        o = o->Parent;
    }
    return true;
}

void RenderLightLines(){
    // pool rotativo: nada de arrays de stack (captura tardia del driver)
    static GLfloat pool[16][6];
    static int slot = 0;
    w3dEngine::Disable(w3dEngine::Texture2D);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::DisableArray(w3dEngine::TexCoordArray);
    w3dEngine::LineWidth(1);
    for (size_t l = 0; l < Lights.size(); l++) {
        if (!VisibleEnArbol(Lights[l])) continue;
        // mismo esquema de color que el icono de la luz
        if (Lights[l] == (Light*)ObjActivo && Lights[l]->select)
            w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accent)]);
        else if (Lights[l]->select)
            w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accentDark)]);
        else
            w3dEngine::Color4f(ListaColores[static_cast<int>(ColorID::grisUI)][0],
                      ListaColores[static_cast<int>(ColorID::grisUI)][1],
                      ListaColores[static_cast<int>(ColorID::grisUI)][2], 0.7f);
        Vector3 p = Lights[l]->GetGlobalPosition();
        GLfloat* v = pool[slot];
        slot = (slot + 1) % 16;
        v[0] = p.x; v[1] = p.y; v[2] = p.z;
        v[3] = p.x; v[4] = 0.0f; v[5] = p.z;
        w3dEngine::VertexPointer3f(0, v);
        w3dEngine::DrawLines(2);
    }
    w3dEngine::EnableArray(w3dEngine::TexCoordArray);
}

void DibujarIcono3D(Object* obj){
    if (!obj->visible) return;

    w3dEngine::PushMatrix();
    obj->GetMatrix(obj->M);
    w3dEngine::MultMatrix(obj->M.m);

    if (obj->getType() == ObjectType::light){
        if (ObjActivo == obj && obj->select)
            w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accent)]);
        else if (obj->select)
            w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::accentDark)]);
        else
            w3dEngine::Color4fv(ListaColores[static_cast<int>(ColorID::grisUI)]);
        w3dEngine::DrawPoints(1);
    }

    if (!obj->Childrens.empty()){
        /*w3dEngine::Rotatef(obj->rotX, 1, 0, 0);
        w3dEngine::Rotatef(obj->rotZ, 0, 1, 0);
        w3dEngine::Rotatef(obj->rotY, 0, 0, 1);*/
        for (size_t c = 0; c < obj->Childrens.size(); c++) {
            DibujarIcono3D(obj->Childrens[c]);
        }
    }
    w3dEngine::PopMatrix();
}

void RenderIcons3D(){
    w3dEngine::DepthMask(false);
    w3dEngine::Enable(w3dEngine::Texture2D);
    w3dEngine::BindTexture(Textures[4]->iID);
#ifndef W3D_SYMBIAN
    w3dEngine::TexFilter(false);
    w3dEngine::TexFilter(false);
#endif
    w3dEngine::PointSpriteCoordReplace(true);
    w3dEngine::VertexPointer3s(0, pointVertex);
    w3dEngine::Enable(w3dEngine::Blend);
    w3dEngine::Enable(w3dEngine::PointSprite);
    w3dEngine::PointSize(16 * GlobalScale); // iconos de 16 relativos a la UI

    DibujarIcono3D(SceneCollection);

    w3dEngine::PointSpriteCoordReplace(false);
    w3dEngine::Disable(w3dEngine::PointSprite);
    w3dEngine::DepthMask(true);
}

void RenderVK(){
    // por implementar
}