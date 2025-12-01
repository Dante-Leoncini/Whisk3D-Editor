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

//Relantionshipslines
void RenderLinkLines(Object* obj){
    // Guardar la matriz actual
    glPushMatrix();
    
    // Aplicar las transformaciones del objeto
    glTranslatef(obj->posX, obj->posZ, obj->posY);
    glRotatef(obj->rotX, 1, 0, 0); // angulo, X Y Z
    glRotatef(obj->rotZ, 0, 1, 0); // angulo, X Y Z
    glRotatef(obj->rotY, 0, 0, 1); // angulo, X Y Z
    
    // Procesar cada hijo
    for (size_t c = 0; c < obj->Childrens.size(); c++) {
		Object* objChild = obj->Childrens[c];
		if (!objChild->visible) continue;
		if (obj->getType()!= ObjectType::collection && obj->getType() != ObjectType::baseObject){
			LineaLinkChild[3] = objChild->posX;
			LineaLinkChild[4] = objChild->posZ;
			LineaLinkChild[5] = objChild->posY;

			// Calcular la distancia en 3D entre obj y objChild
			GLfloat diffX = objChild->posX - obj->posX;
			GLfloat diffY = objChild->posY - obj->posY;
			GLfloat diffZ = objChild->posZ - obj->posZ;
			//GLfloat distancia = sqrtu(diffX * diffX + diffY * diffY + diffZ * diffZ);
			GLfloat distancia = std::sqrt(diffX * diffX + diffY * diffY + diffZ * diffZ);
			
			// Usar la distancia escalada para modificar las coordenadas UV
			lineUV[3] = distancia*8;
			glVertexPointer( 3, GL_FLOAT, 0, LineaLinkChild );
			glDrawElements( GL_LINES, LineaEdgeSize, GL_UNSIGNED_SHORT, LineaEdge );
		}

		RenderLinkLines(objChild);
    }

    // Restaurar la matriz previa
    glPopMatrix();
}

void DrawTransformAxis() {
	glPushMatrix();    
		
	if (InteractionMode == ObjectMode){
		glTranslatef(TransformPivotPointFloat[0]/65000, 
					 TransformPivotPointFloat[1]/65000, 
					 TransformPivotPointFloat[2]/65000
		);	
	}
	switch (axisSelect) {
		case X:
			glColor4fv(ListaColores[ColorTransformX]);
			glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, EjeRojo);
			break;

		case Y:
			glColor4fv(ListaColores[ColorTransformY]);
			glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, EjeVerde);
			break;

		case Z:
			glColor4fv(ListaColores[ColorTransformZ]);
			glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, EjeAzul);
			break;

		case XYZ:
			glColor4fv(ListaColores[ColorTransformX]);
			glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, EjeRojo);

			glColor4fv(ListaColores[ColorTransformY]);
			glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, EjeVerde);

			glColor4fv(ListaColores[ColorTransformZ]);
			glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, EjeAzul);
			break;
	}

	glPopMatrix();
}

bool RenderAxisTransform(Object* obj) {
	bool found = false;
    glPushMatrix();    
    glTranslatef(obj->posX, obj->posZ, obj->posY);
    
    if (obj == ObjActivo) {
		if (estado == rotacion || estado == EditScale){
			glRotatef(obj->rotX, 1, 0, 0); //angulo, X Y Z
			glRotatef(obj->rotZ, 0, 1, 0); //angulo, X Y Z
			glRotatef(obj->rotY, 0, 0, 1); //angulo, X Y Z
		}		
		//dibuja los ejes de transformacion
		if (estado == translacion || estado == rotacion || estado == EditScale){		
        	DrawTransformAxis();
		}		
		found = true;
    } 
	else if (obj->Childrens.size() > 0){	
		glRotatef(obj->rotX, 1, 0, 0); //angulo, X Y Z
		glRotatef(obj->rotZ, 0, 1, 0); //angulo, X Y Z
		glRotatef(obj->rotY, 0, 0, 1); //angulo, X Y Z

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
    if (obj->visible){
		glPushMatrix();    
		glTranslatef(obj->posX, obj->posZ, obj->posY);
		if (obj->select || obj == ObjActivo){
			if (obj == ObjActivo){
				glColor4f(ListaColores[accent][0],ListaColores[accent][1],ListaColores[accent][2],ListaColores[accent][3]);
			}
			else {
				glColor4f(ListaColores[accentDark][0],ListaColores[accentDark][1],ListaColores[accentDark][2],ListaColores[accentDark][3]);
			}	

			glDrawArrays( GL_POINTS, 0, 1 );
		}

		if (!obj->Childrens.empty()){	
			glRotatef(obj->rotX, 1, 0, 0); //angulo, X Y Z
			glRotatef(obj->rotZ, 0, 1, 0); //angulo, X Y Z
			glRotatef(obj->rotY, 0, 0, 1); //angulo, X Y Z
			for (size_t c = 0; c < obj->Childrens.size(); c++) {
				DibujarOrigen(obj->Childrens[c]);
			}
		}
    	glPopMatrix();
    } 
}

void RenderOrigins(){	
	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, Textures[1].iID ); //selecciona la textura
	glVertexPointer( 3, GL_SHORT, 0, pointVertex );
	glTexEnvi( GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE );  
	glEnable( GL_BLEND );
	// Enable point sprites.
	glEnable( GL_POINT_SPRITE );
	// Make the points bigger.
	glPointSize( 16 );
	for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {
		DibujarOrigen(SceneCollection->Childrens[c]);
	}
	glTexEnvi( GL_POINT_SPRITE, GL_COORD_REPLACE, GL_FALSE);
	glDisable( GL_POINT_SPRITE );
}	

void DibujarIcono3D(Object* obj){    
    if (obj->visible){
		glPushMatrix();    
		glTranslatef(obj->posX, obj->posZ, obj->posY);

		if (obj->getType() == ObjectType::light){
			if (ObjActivo == obj && obj->select){
                glColor4f(ListaColores[accent][0],ListaColores[accent][1],ListaColores[accent][2],ListaColores[accent][3]);
            }
            else if (obj->select){
                glColor4f(ListaColores[accentDark][0],ListaColores[accentDark][1],ListaColores[accentDark][2],ListaColores[accentDark][3]);
            }
            else {		
                glColor4f(ListaColores[grisUI][0], ListaColores[grisUI][1], ListaColores[grisUI][2], ListaColores[grisUI][3]);		
            }
			glDrawArrays( GL_POINTS, 0, 1 );
		}

		if (!obj->Childrens.empty()){	
			glRotatef(obj->rotX, 1, 0, 0); //angulo, X Y Z
			glRotatef(obj->rotZ, 0, 1, 0); //angulo, X Y Z
			glRotatef(obj->rotY, 0, 0, 1); //angulo, X Y Z
			for (size_t c = 0; c < obj->Childrens.size(); c++) {
				DibujarIcono3D(obj->Childrens[c]);
			}
		}
    	glPopMatrix();
    } 
}

void RenderIcons3D(){
	glDepthMask(GL_FALSE); // Reactiva la escritura en el Z-buffer   
	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, Textures[4].iID ); //selecciona la textura 	
	glTexEnvi( GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE );  
	glVertexPointer( 3, GL_SHORT, 0, pointVertex );
	glEnable( GL_BLEND );
	glEnable( GL_POINT_SPRITE );
	glPointSize( 32 );

	DibujarIcono3D(SceneCollection);

	glTexEnvi( GL_POINT_SPRITE, GL_COORD_REPLACE, GL_FALSE);
	glDisable( GL_POINT_SPRITE );
	glDepthMask(GL_TRUE); // Reactiva la escritura en el Z-buffer     
}

void RenderVK(){

}