inline float FIXED_TO_FLOAT(GLfixed x) {
    return static_cast<float>(x) / 65536.0f; // porque Q16.16
}

void RenderMesh( Object& obj, int indice ){
	Mesh& pMesh = Meshes[obj.Id];	
	glPushMatrix();
	//averiguar diferencia entre glScalex y glScalef
	//glScalex(obj.scaleX, obj.scaleZ, obj.scaleY);
	//glScalef(obj.scaleX, obj.scaleZ, obj.scaleY);
	glScalef(
		FIXED_TO_FLOAT(obj.scaleX),
		FIXED_TO_FLOAT(obj.scaleZ),
		FIXED_TO_FLOAT(obj.scaleY)
	);
			
	glColor4f(ListaColores[blanco][0],ListaColores[blanco][1],ListaColores[blanco][2],ListaColores[blanco][3]);
	glDisable(GL_COLOR_MATERIAL);
	glMaterialfv(   GL_FRONT_AND_BACK, GL_AMBIENT,  objAmbient  );
	//averiguar diferencia entre glMaterialx y glMateriali
	//glMaterialx( GL_FRONT_AND_BACK, GL_SHININESS,   12 << 16     );
	glMateriali( GL_FRONT_AND_BACK, GL_SHININESS,   12 << 16     );
	glMaterialfv(   GL_FRONT_AND_BACK, GL_EMISSION, ListaColores[negro] );

	// Set array pointers from mesh.
	glVertexPointer( 3, GL_SHORT, 0, pMesh.vertex );
	glColorPointer( 4, GL_UNSIGNED_BYTE, 0, pMesh.vertexColor );	
	glNormalPointer( GL_BYTE, 0, pMesh.normals );
	
	glShadeModel( GL_SMOOTH );
	
	//modelo con textura
	if (SimularZBuffer){
		glDisable( GL_LIGHTING );
		glDisable(GL_BLEND);
		glDisable( GL_TEXTURE_2D );
		glDrawElements(GL_TRIANGLES, pMesh.facesSize, GL_UNSIGNED_SHORT, pMesh.faces);
	}
	
	//material
	else if (view == MaterialPreview || view == Rendered){
		// Calcular coordenadas UV reflejadas
		//calculateReflectionUVs(pMesh);

		glTexCoordPointer( 2, GL_FLOAT, 0, pMesh.uv );
		
		//for(int f=0; f < pMesh.materialsGroup.Count(); f++){
		for(size_t f=0; f < pMesh.materialsGroup.size(); f++){
			Material& mat = Materials[pMesh.materialsGroup[f].material];	
			glMaterialfv(   GL_FRONT_AND_BACK, GL_DIFFUSE, mat.diffuse ); 
			glMaterialfv(   GL_FRONT_AND_BACK, GL_SPECULAR, mat.specular );

			//vertex color
			if (mat.vertexColor){
				glColor4f(ListaColores[negro][0],ListaColores[negro][1],ListaColores[negro][2],ListaColores[negro][3]);
				glEnableClientState( GL_COLOR_ARRAY );
				glEnable(GL_COLOR_MATERIAL);
			}
			else {
				glColor4f(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2], mat.diffuse[3]);
				glDisable(GL_COLOR_MATERIAL);
				glDisableClientState( GL_COLOR_ARRAY );
			}		

			//si usa culling
			if (mat.culling){glEnable( GL_CULL_FACE );}
			else {glDisable( GL_CULL_FACE );}		 

			//si tiene iluminacion	
			if (mat.lighting){glEnable( GL_LIGHTING );}
			else {glDisable( GL_LIGHTING );}
			
			//transparent
			if (mat.transparent){glEnable(GL_BLEND);}
			else {glDisable(GL_BLEND);}
			
			//si tiene texturas
			if (mat.textura){
				glEnable( GL_TEXTURE_2D );
				glBindTexture(  GL_TEXTURE_2D, mat.textureID ); //selecciona la textura							
			
				//textura pixelada o suave
				if (mat.interpolacion == lineal){
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				}
				else if (mat.interpolacion == closest){
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				}	
				//si la textura se repite
				if (mat.repeat){
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				}
				else {
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				}	
			}
			else {glDisable( GL_TEXTURE_2D );}

			glMaterialfv(   GL_FRONT_AND_BACK, GL_EMISSION, mat.emission );

			glDrawElements( GL_TRIANGLES, pMesh.materialsGroup[f].indicesDrawnCount, GL_UNSIGNED_SHORT, &pMesh.faces[pMesh.materialsGroup[f].startDrawn] );

			//for (TInt mdf = 0; mdf < pMesh.Modifiers.Count(); ++mdf) {	
			for (size_t mdf = 0; mdf < pMesh.Modifiers.size(); ++mdf) {
				Modifier& modificador = Modifiers[pMesh.Modifiers[mdf]];
				switch (modificador.type) {
					case mirror:
						if (modificador.OpcionesTBool[0]){	
							glPopMatrix();						
							glPushMatrix();
							//glScalex(-obj.scaleX, obj.scaleZ, obj.scaleY);
							glScalef(
								FIXED_TO_FLOAT(-obj.scaleX),
								FIXED_TO_FLOAT(obj.scaleZ),
								FIXED_TO_FLOAT(obj.scaleY)
							);
							glScalef(-1, 0, 0);
							glDrawElements( GL_TRIANGLES, pMesh.materialsGroup[f].indicesDrawnCount, GL_UNSIGNED_SHORT, &pMesh.faces[pMesh.materialsGroup[f].startDrawn] );
						}
						if (modificador.OpcionesTBool[1]){	
							glPopMatrix();				
							glPushMatrix();
							//glScalex(obj.scaleX, obj.scaleZ, -obj.scaleY);
							glScalef(
								FIXED_TO_FLOAT(obj.scaleX),
								FIXED_TO_FLOAT(obj.scaleZ),
								FIXED_TO_FLOAT(-obj.scaleY)
							);
							glDrawElements( GL_TRIANGLES, pMesh.materialsGroup[f].indicesDrawnCount, GL_UNSIGNED_SHORT, &pMesh.faces[pMesh.materialsGroup[f].startDrawn] );
						}
						if (modificador.OpcionesTBool[2]){	
							glPopMatrix();
							glPushMatrix();
							//glScalex(obj.scaleX, -obj.scaleZ, obj.scaleY);
							glScalef(
								FIXED_TO_FLOAT(obj.scaleX),
								FIXED_TO_FLOAT(-obj.scaleZ),
								FIXED_TO_FLOAT(obj.scaleY)
							);
							glDrawElements( GL_TRIANGLES, pMesh.materialsGroup[f].indicesDrawnCount, GL_UNSIGNED_SHORT, &pMesh.faces[pMesh.materialsGroup[f].startDrawn] );
						}
						break;
				}
			}
		}
	}	
	//modelo sin textura
	else if (view == Solid){
		glEnable( GL_LIGHTING );
		glEnable( GL_CULL_FACE );
		glDisableClientState( GL_COLOR_ARRAY );	  	
		glDisable( GL_TEXTURE_2D );
		glDisable(GL_BLEND);
		glMaterialfv(   GL_FRONT_AND_BACK, GL_DIFFUSE,  Materials[0].diffuse   ); 

		//no hace falta usar el bucle ya que es todo del mismo material.
		//esto es mucho mas rapido
		glDrawElements( GL_TRIANGLES, pMesh.facesSize, GL_UNSIGNED_SHORT, pMesh.faces );			

		//glDrawElements( GL_TRIANGLES, pMesh.facesSize*3, GL_UNSIGNED_SHORT, pMesh.faces );	
		//for(int f=0; f < pMesh.materialsGroup.Count(); f++){	
			/*Material& mat = Materials[pMesh.materialsGroup[f].material];
			//si usa culling
			if (mat.culling){glEnable( GL_CULL_FACE );}
			else {glDisable( GL_CULL_FACE );}	*/

			//glDrawElements( GL_TRIANGLES, pMesh.materialsGroupsSize[f]*3, GL_UNSIGNED_SHORT, pMesh.faces[f] );	
			//glDrawElements( GL_TRIANGLES, pMesh.materialsGroup[f].start, GL_UNSIGNED_SHORT, &pMesh.faces[pMesh.materialsGroup[f].start] );	
		//}
	} 

	glDisable(GL_POLYGON_OFFSET_FILL);
	glDepthFunc(GL_LESS);
	
	//dibuja el borde seleccionado
	/*if(SelectActivo == objId && showOverlays && showOutlineSelect){
		glDisableClientState( GL_COLOR_ARRAY );	  
		glDisable( GL_LIGHTING );
		glEnable(GL_COLOR_MATERIAL);
		glDisable( GL_TEXTURE_2D );  
		glEnable(GL_POLYGON_OFFSET_FILL);
		
		//si se esta editando
		else if (view != 2){
			glPolygonOffset(1.0, 200.0);
			glLineWidth(3);	 
			glColor4f(ListaColores[colorBordeSelect][0],ListaColores[colorBordeSelect][1],ListaColores[colorBordeSelect][2],ListaColores[colorBordeSelect][3]);
			//glDrawElements( GL_LINES, obj.edgesSize, GL_UNSIGNED_SHORT, obj.edges );	
			glLineWidth(1); //lo deja en su valor por defecto	 
		}	
	};*/
	glPopMatrix();
}

// Funcion recursiva para renderizar un objeto y sus hijos
void RenderMeshAndChildren(Object& obj, int indice){
    // Guardar la matriz actual
    glPushMatrix();
    
    // Aplicar las transformaciones del objeto
    glTranslatef(obj.posX, obj.posZ, obj.posY);
    glRotatef(obj.rotX, 1, 0, 0); // angulo, X Y Z
    glRotatef(obj.rotZ, 0, 1, 0); // angulo, X Y Z
    glRotatef(obj.rotY, 0, 0, 1); // angulo, X Y Z

    // Si es visible y es un mesh, lo dibuja
    if (obj.visible && obj.type == mesh) {
        RenderMesh(obj, indice); // Ajusta el segundo parametro si es necesario
    }
    
    // Procesar cada hijo
    //for (int c = 0; c < obj.Childrens.Count(); c++) {
    for (size_t c = 0; c < obj.Childrens.size(); c++) {
        Object& objChild = Objects[obj.Childrens[c].Id];
        RenderMeshAndChildren(objChild, indice);
    }

    // Restaurar la matriz previa
    glPopMatrix();
}

void RenderObject( int objId ){
	Object& obj = Objects[objId];
	glPushMatrix();
	//glScalex(obj.scaleX, obj.scaleZ, obj.scaleY);
	glScalef(
		FIXED_TO_FLOAT(obj.scaleX),
		FIXED_TO_FLOAT(obj.scaleZ),
		FIXED_TO_FLOAT(obj.scaleY)
	);

	glDisable( GL_TEXTURE_2D );

	//color si esta seleccionado
	if (SelectActivo == objId && obj.seleccionado){
		glColor4f(ListaColores[accent][0],ListaColores[accent][1],ListaColores[accent][2],ListaColores[accent][3]);
	}
	else if (obj.seleccionado){
		glColor4f(ListaColores[accentDark][0],ListaColores[accentDark][1],ListaColores[accentDark][2],ListaColores[accentDark][3]);
	}
	else {		
		glColor4f(ListaColores[negro][0],ListaColores[negro][1],ListaColores[negro][2],ListaColores[negro][3]);		
	}
	//si es un empty
	if (obj.type == empty){		
		glDisable( GL_TEXTURE_2D );	 
		glDisable( GL_BLEND );	
		glVertexPointer( 3, GL_SHORT, 0, EmptyVertices );
		glDrawElements( GL_LINES, EmptyEdgesSize, GL_UNSIGNED_SHORT, EmptyEdges );
	}
	else if (obj.type == camera){	
		glDisable( GL_TEXTURE_2D ); 
		glDisable( GL_BLEND );
		glVertexPointer( 3, GL_SHORT, 0, CameraVertices );
		glDrawElements( GL_LINES, CameraEdgesSize, GL_UNSIGNED_SHORT, CameraEdges );
		if (CameraActive == objId){		
    		glDisable( GL_CULL_FACE  );	
			glDrawElements( GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, CameraFaceActive );	
		}
	}
	else if (obj.type == light){
		glEnable( GL_TEXTURE_2D ); 
		glEnable( GL_BLEND );
		glDepthMask(GL_FALSE); // Desactiva la escritura en el Z-buffer

		//GL_POINT_SPRITE_OES es para symbian
		//glEnable( GL_POINT_SPRITE_OES ); //activa el uso de sprites en los vertices
		glEnable(GL_POINT_SPRITE);                        // habilitar point sprites
		
		glPointSize( 32 ); //tamaño del punto
		glVertexPointer( 3, GL_SHORT, 0, pointVertex );
		glBindTexture( GL_TEXTURE_2D, Textures[4].iID ); //selecciona la textura

		//glTexEnvi( GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_TRUE );
		glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE); 

		glDrawArrays( GL_POINTS, 0, 1 );
		//glTexEnvi( GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_FALSE);
		glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_FALSE); 

		//glDisable( GL_POINT_SPRITE_OES );	
		glDisable( GL_POINT_SPRITE );			
			
		glDisable( GL_TEXTURE_2D ); 

		//glScalex( 0, 10, 0 ); //4500
		/*glScalef(
			FIXED_TO_FLOAT(0),
			FIXED_TO_FLOAT(10),
			FIXED_TO_FLOAT(0)
		);*/
		LineaLightVertex[4] = (GLshort)-obj.posZ;
		glVertexPointer( 3, GL_SHORT, 0, LineaLightVertex );
		glDrawElements( GL_LINES, LineaEdgeSize, GL_UNSIGNED_SHORT, LineaEdge );

		glDisable( GL_BLEND );
		glDepthMask(GL_TRUE); // Reactiva la escritura en el Z-buffer		
	}
	glPopMatrix();
}

// Funcion recursiva para renderizar un objeto y sus hijos
void RenderObjectAndChildrens(int objId){
	Object& obj = Objects[objId];
    // Guardar la matriz actual
    glPushMatrix();
    
    // Aplicar las transformaciones del objeto
    glTranslatef(obj.posX, obj.posZ, obj.posY);
    glRotatef(obj.rotX, 1, 0, 0); // angulo, X Y Z
    glRotatef(obj.rotZ, 0, 1, 0); // angulo, X Y Z
    glRotatef(obj.rotY, 0, 0, 1); // angulo, X Y Z

    // Si es visible y no es un mesh, lo dibuja
    if (obj.visible && obj.type != mesh) {
        RenderObject(objId); // Ajusta el segundo parametro si es necesario
    }
    
    // Procesar cada hijo
    //for (int c = 0; c < obj.Childrens.Count(); c++) {
    for (size_t c = 0; c < obj.Childrens.size(); c++) {
        RenderObjectAndChildrens(obj.Childrens[c].Id);
    }

    // Restaurar la matriz previa
    glPopMatrix();
}

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
void RenderLinkLines(int objId){
	Object& obj = Objects[objId];
    // Guardar la matriz actual
    glPushMatrix();
    
    // Aplicar las transformaciones del objeto
    glTranslatef(obj.posX, obj.posZ, obj.posY);
    glRotatef(obj.rotX, 1, 0, 0); // angulo, X Y Z
    glRotatef(obj.rotZ, 0, 1, 0); // angulo, X Y Z
    glRotatef(obj.rotY, 0, 0, 1); // angulo, X Y Z
    
    // Procesar cada hijo
    //for (TInt c = 0; c < obj.Childrens.Count(); c++) {
    for (size_t c = 0; c < obj.Childrens.size(); c++) {
		Object& objChild = Objects[obj.Childrens[c].Id];
		LineaLinkChild[3] = (GLshort)objChild.posX;
		LineaLinkChild[4] = (GLshort)objChild.posZ;
		LineaLinkChild[5] = (GLshort)objChild.posY;

		// Calcular la distancia en 3D entre obj y objChild
		GLfloat diffX = objChild.posX - obj.posX;
        GLfloat diffY = objChild.posY - obj.posY;
        GLfloat diffZ = objChild.posZ - obj.posZ;
        //GLfloat distancia = sqrtu(diffX * diffX + diffY * diffY + diffZ * diffZ);
		GLfloat distancia = std::sqrt(diffX * diffX + diffY * diffY + diffZ * diffZ);
        
        // Usar la distancia escalada para modificar las coordenadas UV
        lineUV[3] = distancia/8;
		glVertexPointer( 3, GL_SHORT, 0, LineaLinkChild );
		glDrawElements( GL_LINES, LineaEdgeSize, GL_UNSIGNED_SHORT, LineaEdge );
				
		DrawnLines(1, 1, LineaTimeline, LineaEdge);
		//if (obj.Childrens.Count() > 0){
		if (obj.Childrens.size() > 0){
			RenderLinkLines(obj.Childrens[c].Id);
		}
    }

    // Restaurar la matriz previa
    glPopMatrix();
}

void DrawTransformAxis(Object& obj) {
	glPushMatrix();    
	glVertexPointer( 3, GL_SHORT, 0, objVertexdataFloor );
	glLineWidth(2);
		
	if (InteractionMode == ObjectMode){
		glTranslatef(TransformPivotPointFloat[0]/65000, 
					 TransformPivotPointFloat[1]/65000, 
					 TransformPivotPointFloat[2]/65000
		);	
	}
	if (axisSelect == X){
		glColor4f(ColorTransformX[0],ColorTransformX[1],ColorTransformX[2],ColorTransformX[3]);
		glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeRojo );				
	}
	else if (axisSelect == Y){
		glColor4f(ColorTransformY[0],ColorTransformY[1],ColorTransformY[2],ColorTransformY[3]);
		glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeVerde ); 				
	}
	else if (axisSelect == Z){
		glColor4f(ColorTransformZ[0],ColorTransformZ[1],ColorTransformZ[2],ColorTransformZ[3]);
		glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeAzul ); 				
	}	
	else if (axisSelect == XYZ){
		glColor4f(ColorTransformX[0],ColorTransformX[1],ColorTransformX[2],ColorTransformX[3]);
		glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeRojo );	
		glColor4f(ColorTransformY[0],ColorTransformY[1],ColorTransformY[2],ColorTransformY[3]);
		glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeVerde ); 
		glColor4f(ColorTransformZ[0],ColorTransformZ[1],ColorTransformZ[2],ColorTransformZ[3]);
		glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeAzul ); 				
	}
	glLineWidth(1);	//la deja como es por defecto	
	glPopMatrix();
}

void SearchSelectObj(Object& obj, int objIndex, bool& found) {
    glPushMatrix();    
    glTranslatef(obj.posX, obj.posZ, obj.posY);
    
    if (objIndex == SelectActivo) {
		if (estado == rotacion || estado == EditScale){
			glRotatef(obj.rotX, 1, 0, 0); //angulo, X Y Z
			glRotatef(obj.rotZ, 0, 1, 0); //angulo, X Y Z
			glRotatef(obj.rotY, 0, 0, 1); //angulo, X Y Z
		}		
		//dibuja los ejes de transformacion
		if (estado == translacion || estado == rotacion || estado == EditScale){		
        	DrawTransformAxis(obj);
		}		
        found = true;
    } 
	//else if (obj.Childrens.Count() > 0){	
	else if (obj.Childrens.size() > 0){	
		glRotatef(obj.rotX, 1, 0, 0); //angulo, X Y Z
		glRotatef(obj.rotZ, 0, 1, 0); //angulo, X Y Z
		glRotatef(obj.rotY, 0, 0, 1); //angulo, X Y Z
        //for (int c = 0; c < obj.Childrens.Count(); c++) {
        for (size_t c = 0; c < obj.Childrens.size(); c++) {
            if (found) break;  // Si ya lo encontro, salir del bucle
            Object& objChild = Objects[obj.Childrens[c].Id];
            SearchSelectObj(objChild, obj.Childrens[c].Id, found);
        }
    }
    glPopMatrix();
}

void DibujarOrigen(Object& obj, int objIndex){
    glPushMatrix();    
    glTranslatef(obj.posX, obj.posZ, obj.posY);
    
    if (obj.visible && (obj.seleccionado || objIndex == SelectActivo)){	
		if (objIndex == SelectActivo){
			glColor4f(ListaColores[accent][0],ListaColores[accent][1],ListaColores[accent][2],ListaColores[accent][3]);
		}
		else {
			glColor4f(ListaColores[accentDark][0],ListaColores[accentDark][1],ListaColores[accentDark][2],ListaColores[accentDark][3]);
		}
		glVertexPointer( 3, GL_SHORT, 0, pointVertex );
		glBindTexture( GL_TEXTURE_2D, Textures[1].iID ); //selecciona la textura
		//glTexEnvi( GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_TRUE );    	
		glTexEnvi( GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE );  
		glPushMatrix();    		
		//glScalex(obj.scaleX, obj.scaleZ, obj.scaleY);	
		glScalef(
			FIXED_TO_FLOAT(obj.scaleX),
			FIXED_TO_FLOAT(obj.scaleZ),
			FIXED_TO_FLOAT(obj.scaleY)
		);
		glDrawArrays( GL_POINTS, 0, 1 );	
    	glPopMatrix();
    } 
	//else if (obj.Childrens.Count() > 0){
	else if (obj.Childrens.size() > 0){	
		glRotatef(obj.rotX, 1, 0, 0); //angulo, X Y Z
		glRotatef(obj.rotZ, 0, 1, 0); //angulo, X Y Z
		glRotatef(obj.rotY, 0, 0, 1); //angulo, X Y Z
        //for (int c = 0; c < obj.Childrens.Count(); c++) {
        for (size_t c = 0; c < obj.Childrens.size(); c++) {
            Object& objChild = Objects[obj.Childrens[c].Id];
            DibujarOrigen(objChild, obj.Childrens[c].Id);
        }
    }
    glPopMatrix();
}

void Render(){
	if (SimularZBuffer){
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Color negro
		glEnable(GL_FOG);
		glFogf(GL_FOG_MODE, GL_LINEAR); // Tipo de niebla lineal
		glFogf(GL_FOG_START, FRUSTUM_NEAR);  // Distancia inicial de la niebla
		glFogf(GL_FOG_END, FRUSTUM_FAR);     // Distancia final de la niebla
		GLfloat fogColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
		glFogfv(GL_FOG_COLOR, fogColor); // Color de la niebla
	}
	else {
		glDisable(GL_FOG);
		glClearColor(ListaColores[background][0],ListaColores[background][1],ListaColores[background][2],ListaColores[background][3]);
	}

    // Limpiar pantalla
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glLoadIdentity();
    glEnable( GL_DEPTH_TEST );	

	if (ViewFromCameraActive){
		RecalcViewPos();
		//glTranslatef( posX, posZ, posY);
	}
	glTranslatef( posX, posZ, -cameraDistance+posY );

	//original
	glRotatef(rotY, 1, 0, 0); //angulo, X Y Z
	glRotatef(rotX, 0, 1, 0); //angulo, X Y Z
	// Use some magic numbers to scale the head model to fit the screen.
	//glScalex( 1 << 10, 1 << 10, 1 << 10 );
	glScalef(1024.0f / 65536.0f, 1024.0f / 65536.0f, 1024.0f / 65536.0f);
	glTranslatef( PivotX, PivotZ, PivotY);

	//primero hay que colocar las luces en caso de estar en modo render!
	if ((!SimularZBuffer && view == MaterialPreview) || view == Rendered){
		//for(int o=0; o < Objects.Count(); o++){
		//for(int o=0; o < Objects.size(); o++){
		for (size_t o = 0; o < Objects.size(); o++) {
			Object& obj = Objects[o];
			if(!obj.visible || obj.type != light ) {continue;}
			Light& light = Lights[obj.Id];

			glPushMatrix(); //guarda la matrix
			glTranslatef( obj.posX, obj.posZ, obj.posY);
			GLfloat lightPos[] = {0.0f, 0.0f, 0.0f, 1.0f}; // Luz puntual en la posici�n transformada
			glLightfv(light.lightId, GL_POSITION, lightPos);
			//glLightfv(  light.lightId, GL_POSITION, positionPuntualLight );
			glPopMatrix(); //reinicia la matrix a donde se guardo  
		}	
	}

	//por defecto la linea es de 1	
	glLineWidth(1);	
	
	//bucle que dibuja cada objeto en orden
	//if(Meshes.Count() > 0){
	if(Meshes.size() > 0){
		// Funcion principal para iterar sobre la coleccion
		//for (int o = 0; o < Collection.Count(); o++) {
		for (size_t o = 0; o < Collection.size(); o++) {
			Object& obj = Objects[Collection[o]];
			RenderMeshAndChildren(obj, Collection[o]);
		}
	}

	//el valor que tiene que tener para dibujar el resto correctamente
	//glDisable(GL_COLOR_MATERIAL);
	glDisableClientState( GL_COLOR_ARRAY );

	//fin del dibujado de objetos
	//si estaba simulando el zbuffer. el resto no hace falta
	if (SimularZBuffer){		
		redibujar = false;
		return;
	};

	//el resto de objetos no usan materiales ni luces
	glMaterialfv(   GL_FRONT_AND_BACK, GL_DIFFUSE,  ListaColores[negro] );
	glMaterialfv(   GL_FRONT_AND_BACK, GL_AMBIENT,  ListaColores[negro] );
	glMaterialfv(   GL_FRONT_AND_BACK, GL_SPECULAR, ListaColores[negro] );
	glDisable( GL_CULL_FACE ); // Enable back face culling.
	glDisable( GL_LIGHTING );
	glEnable(GL_COLOR_MATERIAL);
	glDisable( GL_TEXTURE_2D );
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	
	//se encarga de dibujar el layout 
	if (showOverlays){
		//dibujar las lineas del piso y el piso
		if (showFloor || showXaxis || showYaxis){
			glEnable(GL_FOG);
			glFogf(GL_FOG_MODE, GL_LINEAR); // Tipo de niebla lineal
			glFogf(GL_FOG_START, FRUSTUM_NEAR);  // Distancia inicial de la niebla
			glFogf(GL_FOG_END, FRUSTUM_FAR);     // Distancia final de la niebla
			//GLfloat fogColor[] = {0.23f, 0.23f, 0.23f, 1.f};
			//glClearColor( ClearColor[0], ClearColor[1], ClearColor[2], 1.f );
			glFogfv(GL_FOG_COLOR, ListaColores[background]); // Color de la niebla
			glLineWidth(1);	 

			glVertexPointer( 3, GL_SHORT, 0, objVertexdataFloor );
			//glNormalPointer( GL_BYTE, 0, objNormaldataFloor );

			//dibuja el piso	
			if (showFloor){
				glColor4f(LineaPiso[0],LineaPiso[1],LineaPiso[2],LineaPiso[3]);
				glDrawElements( GL_LINES, objFacesFloor, GL_UNSIGNED_SHORT, objFacedataFloor );			
			}		
			//linea Verde
			if (showXaxis){				
				glLineWidth(2);
				glColor4f(LineaPisoRoja[0],LineaPisoRoja[1],LineaPisoRoja[2],LineaPisoRoja[3]);
				glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeRojo );
				glLineWidth(1);	//la deja como es por defecto
			}
			else if (showFloor){
				glColor4f(LineaPiso[0],LineaPiso[1],LineaPiso[2],LineaPiso[3]);
				glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeRojo );
			}
			//linea Roja	
			if (showYaxis){
				glLineWidth(2);
				glColor4f(LineaPisoVerde[0],LineaPisoVerde[1],LineaPisoVerde[2],LineaPisoVerde[3]);
				glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeVerde );
				glLineWidth(1);	//la deja como es por defecto
			}
			else if (showFloor){
				glColor4f(LineaPiso[0],LineaPiso[1],LineaPiso[2],LineaPiso[3]);
				glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeVerde );
			}	
			glDisable(GL_FOG);
		}

		//esto solo se hace si hay objetos
		//if (Objects.Count() > 0){
		if (Objects.size() > 0){
			//dibujo de objetos nuevo!
			glLineWidth(1);	 
			//for (int o = 0; o < Collection.Count(); o++) {
			for (size_t o = 0; o < Collection.size(); o++) {
				RenderObjectAndChildrens(Collection[o]);
			}	 

			//dibujar lineas parent		
			if (ShowRelantionshipsLines){
				glEnable( GL_TEXTURE_2D );
				glEnable( GL_BLEND );
				glDepthMask(GL_FALSE); // Desactiva la escritura en el Z-buffer				
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				glTexCoordPointer( 2, GL_FLOAT, 0, lineUV ); //SpriteUvSize
				glColor4f(ListaColores[negro][0],ListaColores[negro][1],ListaColores[negro][2],ListaColores[negro][3]);	
				glBindTexture( GL_TEXTURE_2D, Textures[3].iID ); //selecciona la de linea punteada	
				//for (int o = 0; o < Collection.Count(); o++) {
				for (size_t o = 0; o < Collection.size(); o++) {
					Object& obj = Objects[Collection[o]];
					//if (obj.Childrens.Count() > 0){
					if (obj.Childrens.size() > 0){
						RenderLinkLines(Collection[o]);
					}
				}
				glDepthMask(GL_TRUE); // Reactiva la escritura en el Z-buffer		
			}

			glDisable( GL_DEPTH_TEST );	
			glDisable( GL_BLEND );
			glDisable( GL_TEXTURE_2D );
			//dibuja los ejes de transformacion
			if (estado == translacion || estado == rotacion || estado == EditScale) {
				//for (TInt o = 0; o < Collection.Count(); o++) {
				for (size_t o = 0; o < Collection.size(); o++) {
					bool found = false;
					Object& obj = Objects[Collection[o]];
					SearchSelectObj(obj, Collection[o], found);
					if (found) break;  // Si ya encontro el objeto, salir del bucle
				}
			}

			//Dibuja el origen de los objetos seleccionados		
			if (showOrigins){	
				//std::cout << "origen!" << std::endl;
				glEnable( GL_TEXTURE_2D );
				glEnable( GL_BLEND );
				// Enable point sprites.
				//glEnable( GL_POINT_SPRITE_OES );
				glEnable( GL_POINT_SPRITE );
				// Make the points bigger.
				glPointSize( 8 );
				//for (TInt o = 0; o < Collection.Count(); o++) {
				for (size_t o = 0; o < Collection.size(); o++) {
					Object& obj = Objects[o];
					DibujarOrigen(obj, o);
				}
				//glTexEnvi( GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_FALSE);
				//glDisable( GL_POINT_SPRITE_OES );
				glTexEnvi( GL_POINT_SPRITE, GL_COORD_REPLACE, GL_FALSE);
				glDisable( GL_POINT_SPRITE );
			}		
		}

		//dibuja el cursor 3D	
		if (show3DCursor){
			glDisable( GL_DEPTH_TEST );
			glPushMatrix(); //guarda la matrix
			glTranslatef( Cursor3DposX, Cursor3DposZ, Cursor3DposY);
			
			glEnable( GL_TEXTURE_2D );
			glEnable( GL_BLEND );
			//glEnable( GL_POINT_SPRITE_OES ); // Enable point sprites.	
			glEnable( GL_POINT_SPRITE ); // Enable point sprites.	
			glPointSize( 32 ); // Make the points bigger.
			glColor4f(ListaColores[blanco][0],ListaColores[blanco][1],ListaColores[blanco][2],ListaColores[blanco][3]);
			glVertexPointer( 3, GL_SHORT, 0, pointVertex );
			glBindTexture( GL_TEXTURE_2D, Textures[2].iID);//iCursor3dTextura.iID ); //selecciona la textura

			//glTexEnvi( GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_TRUE );
			glTexEnvi( GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE );
			glDrawArrays( GL_POINTS, 0, 1 );
			//glTexEnvi( GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_FALSE);
			glTexEnvi( GL_POINT_SPRITE, GL_COORD_REPLACE, GL_FALSE);

			//dibuja lineas		
			glDisable( GL_TEXTURE_2D );
			//glDisable( GL_POINT_SPRITE_OES );
			glDisable( GL_POINT_SPRITE );
			glDisable( GL_BLEND );
		
			glColor4f(ListaColores[negro][0],ListaColores[negro][1],ListaColores[negro][2],ListaColores[negro][3]);
			glVertexPointer( 3, GL_SHORT, 0, Cursor3DVertices );
			glDrawElements( GL_LINES, Cursor3DEdgesSize, GL_UNSIGNED_SHORT, Cursor3DEdges );	

			glPopMatrix(); //reinicia la matrix a donde se guardo	
		}
	}

	if (ShowUi){
		//dibujarUI();
	}

    //termino de dibujar
    redibujar = false;

    //glPopMatrix();             // Restaurar matriz    
}