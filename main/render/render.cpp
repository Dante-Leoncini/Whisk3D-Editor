#include "render.h"

// Definiciones de funciones

void DrawnLines(int LineWidth, int cantidad, GLshort* vertexlines, GLushort* lineasIndices){
    glVertexPointer( 3, GL_SHORT, 0, vertexlines );
    glLineWidth(LineWidth);    
    glDrawElements( GL_LINES, cantidad*2, GL_UNSIGNED_SHORT, lineasIndices );
}

void DrawnLines(int LineWidth, int cantidad, const GLshort* vertexlines, const GLushort* lineasIndices) {
    glVertexPointer(3, GL_SHORT, 0, vertexlines);
    glLineWidth(LineWidth);
    glDrawElements(GL_LINES, cantidad * 2, GL_UNSIGNED_SHORT, lineasIndices);
}

void RenderLinkLines(Object* obj){
    glPushMatrix();
    glTranslatef(obj->pos.x, obj->pos.z, obj->pos.y);
    /*glRotatef(obj->rotX, 1, 0, 0);
    glRotatef(obj->rotZ, 0, 1, 0);
    glRotatef(obj->rotY, 0, 0, 1);*/

    for (size_t c = 0; c < obj->Childrens.size(); c++) {
        Object* objChild = obj->Childrens[c];
        if (!objChild->visible) continue;
        if (obj->getType()!= ObjectType::collection && obj->getType() != ObjectType::baseObject){
            LineaLinkChild[3] = objChild->pos.x;
            LineaLinkChild[4] = objChild->pos.z;
            LineaLinkChild[5] = objChild->pos.y;

            Vector3 diff = objChild->pos - obj->pos;
            GLfloat distancia = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

            lineUV[3] = distancia*8;
            glVertexPointer( 3, GL_FLOAT, 0, LineaLinkChild );
            glDrawElements( GL_LINES, LineaEdgeSize, GL_UNSIGNED_SHORT, LineaEdge );
        }
        RenderLinkLines(objChild);
    }

    glPopMatrix();
}

void DrawTransformAxis() {
    glPushMatrix();
    if (InteractionMode == ObjectMode){
        glTranslatef(TransformPivotPoint.x/65000, 
                     TransformPivotPoint.y/65000, 
                     TransformPivotPoint.z/65000);    
    }

    switch (axisSelect) {
        case X:
            SetColor(ListaColores[static_cast<int>(ColorID::ColorTransformX)]);
            glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, EjeRojo);
            break;
        case Y:
            SetColor(ListaColores[static_cast<int>(ColorID::ColorTransformY)]);
            glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, EjeVerde);
            break;
        case Z:
            SetColor(ListaColores[static_cast<int>(ColorID::ColorTransformZ)]);
            glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, EjeAzul);
            break;
        case XYZ:
            SetColor(ListaColores[static_cast<int>(ColorID::ColorTransformX)]);
            glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, EjeRojo);
            SetColor(ListaColores[static_cast<int>(ColorID::ColorTransformY)]);
            glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, EjeVerde);
            SetColor(ListaColores[static_cast<int>(ColorID::ColorTransformZ)]);
            glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, EjeAzul);
            break;
    }
    glPopMatrix();
}

bool RenderAxisTransform(Object* obj) {
    bool found = false;
    glPushMatrix();    

    obj->GetMatrix(obj->M);
    glMultMatrixf(obj->M);
    
    if (obj == ObjActivo) {
        if (estado == rotacion || estado == EditScale){
            /*glRotatef(obj->rotX, 1, 0, 0);
            glRotatef(obj->rotZ, 0, 1, 0);
            glRotatef(obj->rotY, 0, 0, 1);*/
        }        
        if (estado == translacion || estado == rotacion || estado == EditScale){        
            DrawTransformAxis();
        }        
        found = true;
    } 
    else if (!obj->Childrens.empty()){    
        /*glRotatef(obj->rotX, 1, 0, 0);
        glRotatef(obj->rotZ, 0, 1, 0);
        glRotatef(obj->rotY, 0, 0, 1);*/

        for (size_t c = 0; c < obj->Childrens.size(); c++) {
            if (RenderAxisTransform(obj->Childrens[c])){
                found = true;
                break;
            }
        }
    }
    glPopMatrix();
    return found;
}

void DibujarOrigen(Object* obj){    
    if (!obj->visible) return;

    glPushMatrix();   
    
    obj->GetMatrix(obj->M);
    glMultMatrixf(obj->M);

    if (obj->select || obj == ObjActivo){
        if (obj == ObjActivo){
            glColor4fv(ListaColores[static_cast<int>(ColorID::accent)]);
        }
        else {
            glColor4fv(ListaColores[static_cast<int>(ColorID::accentDark)]);
        }
        glDrawArrays(GL_POINTS, 0, 1);
    }

    if (!obj->Childrens.empty()){    
        /*glRotatef(obj->rotX, 1, 0, 0);
        glRotatef(obj->rotZ, 0, 1, 0);
        glRotatef(obj->rotY, 0, 0, 1);*/
        for (size_t c = 0; c < obj->Childrens.size(); c++) {
            DibujarOrigen(obj->Childrens[c]);
        }
    }
    glPopMatrix();
}

void RenderOrigins(){    
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, Textures[1]->iID);
    glVertexPointer(3, GL_SHORT, 0, pointVertex);
    glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
    glEnable(GL_BLEND);
    glEnable(GL_POINT_SPRITE);
    glPointSize(16);
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {
        DibujarOrigen(SceneCollection->Childrens[c]);
    }
    glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_FALSE);
    glDisable(GL_POINT_SPRITE);
}    

void DibujarIcono3D(Object* obj){    
    if (!obj->visible) return;

    glPushMatrix();    
    glTranslatef(obj->pos.x, obj->pos.z, obj->pos.y);

    if (obj->getType() == ObjectType::light){
        if (ObjActivo == obj && obj->select)
            glColor4fv(ListaColores[static_cast<int>(ColorID::accent)]);
        else if (obj->select)
            glColor4fv(ListaColores[static_cast<int>(ColorID::accentDark)]);
        else
            glColor4fv(ListaColores[static_cast<int>(ColorID::grisUI)]);
        glDrawArrays(GL_POINTS, 0, 1);
    }

    if (!obj->Childrens.empty()){    
        /*glRotatef(obj->rotX, 1, 0, 0);
        glRotatef(obj->rotZ, 0, 1, 0);
        glRotatef(obj->rotY, 0, 0, 1);*/
        for (size_t c = 0; c < obj->Childrens.size(); c++) {
            DibujarIcono3D(obj->Childrens[c]);
        }
    }
    glPopMatrix();
}

void RenderIcons3D(){
    glDepthMask(GL_FALSE);  
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, Textures[4]->iID);    
    glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);  
    glVertexPointer(3, GL_SHORT, 0, pointVertex);
    glEnable(GL_BLEND);
    glEnable(GL_POINT_SPRITE);
    glPointSize(32);

    DibujarIcono3D(SceneCollection);

    glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_FALSE);
    glDisable(GL_POINT_SPRITE);
    glDepthMask(GL_TRUE);  
}

void RenderVK(){
    // por implementar
}