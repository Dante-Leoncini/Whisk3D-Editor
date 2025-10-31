void ReestablecerEstado(){
	if (InteractionMode == ObjectMode){
		for(size_t o=0; o < estadoObjetos.size(); o++){
			SaveState& estadoObj = estadoObjetos[o];
			Object& obj = *estadoObj.obj;
			obj.posX = estadoObj.posX;
			obj.posY = estadoObj.posY;
			obj.posZ = estadoObj.posZ;
			obj.rotX = estadoObj.rotX;
			obj.rotY = estadoObj.rotY;
			obj.rotZ = estadoObj.rotZ;
			obj.scaleX = estadoObj.scaleX;
			obj.scaleY = estadoObj.scaleY;
			obj.scaleZ = estadoObj.scaleZ;	
		}	
		//estadoObjetos.Close();
		estadoObjetos.clear();
	}
	estado = editNavegacion;
	ReloadViewport(true);
};

void Cancelar(){
	// Mostrar el cursor
	SDL_ShowCursor();
	if (estado != editNavegacion){
		ReestablecerEstado();
	}
};

void BorrarAnimaciones(Object& obj){
	for(size_t a = 0; a < AnimationObjects.size(); a++) {
		if (AnimationObjects[a].obj == &obj) {	
			for(size_t p = 0; p < AnimationObjects[a].Propertys.size(); p++) {
				AnimationObjects[a].Propertys[p].keyframes.clear();
			}				
			AnimationObjects[a].Propertys.clear();
			//AnimationObjects.Remove(a);
			if (a >= 0 && a < AnimationObjects.size()) {
				AnimationObjects.erase(AnimationObjects.begin() + a);
			}
		}		
	}
}

void BorrarObjeto(Object& obj){
	if (obj.type == mesh){
		//primero miramos si alguien mas esta usando esta malla 3d.
		//es posible compartir una malla 3d entre distintos objetos
		bool MeshEnUso = false;
		
		for(size_t c=0; c < Collections.size(); c++){
			for(size_t o=0; o < Collections[c]->Objects.size(); o++){
				//si el objeto esta seleccionado. significa que se va a borrar, por lo tanto no se cuenta
				if (Collections[c]->Objects[o]->type == mesh && Collections[c]->Objects[o] == &obj && !Collections[c]->Objects[o]->seleccionado){
					MeshEnUso = true;
					break;
				};				
			}
		}

		//si la malla 3d esta en uso por otro objeto que no va a ser borrado. no se puede borrar
		if (MeshEnUso){
			std::cout << "Malla 3D unica. SI se borra" << std::endl;	
			Meshes[obj.Id].LiberarMemoria();

			// borrar el elemento en posición `indice`
			//no se porque era indice >= 0 && 
			if (static_cast<size_t>(obj.Id) < Meshes.size()) {
				Meshes.erase(Meshes.begin() + obj.Id);
			}
		}
		else {
			std::cout << "Malla 3d en uso. NO se borra" << std::endl;
		}
	}

	//si existe animaciones para ese objeto. las borra		
	BorrarAnimaciones(obj);

	//si es la camara activa. borra el indice
	if (CameraActive == &obj){
		CameraActive = nullptr;	
	}

	delete &obj;

	SelectCount--;
	SelectActivo = nullptr;
}

void Borrar(){
	if (estado != editNavegacion ){
		Cancelar();
	}
	else if (InteractionMode == ObjectMode){
		//si no hay nada seleccionado. no borra
		bool algoSeleccionado = false;
		for (int c = (int)Collections.size() - 1; c >= 0; c--) {
			for (int o = (int)Collections[c]->Objects.size() - 1; o >= 0; o--) {
				if (Collections[c]->Objects[o]->seleccionado){
					algoSeleccionado = true;
					break;	
				}		
			}
		}

		if (!algoSeleccionado){
			std::cout << "nada seleccionado para borrar" << std::endl;
			return;
		}
		
		//pregunta de confirmacion
		//HBufC* noteBuf = HBufC::NewLC(100);
		//_LIT(KStaticErrorMessage, "Delete?");
		//noteBuf->Des().Format(KStaticErrorMessage);
		/*if (!CDialogs::Alert(noteBuf)){
			CleanupStack::PopAndDestroy(noteBuf);	
			return;
		}*/
		//CleanupStack::PopAndDestroy(noteBuf);	
		Cancelar();

		//libera la memoria de los punteros primero	
		// Obtener el objeto seleccionado			
		for (int c = (int)Collections.size() - 1; c >= 0; c--) {
			for (int o = (int)Collections[c]->Objects.size() - 1; o >= 0; o--) {
				if (Collections[c]->Objects[o]->seleccionado){
					std::cout << "El objeto " << (o + 1) << " esta seleccionado y se va a borrar" << std::endl;
					BorrarObjeto(*Collections[c]->Objects[o]);
				}			
			}
		}
	}
    ReloadViewport(true);	
}

void SetTransformPivotPoint(){
	if (InteractionMode == ObjectMode){	
		TransformPivotPointFloat[0] = 0;
		TransformPivotPointFloat[1] = 0;
		TransformPivotPointFloat[2] = 0;
		for(size_t c=0; c < Collections.size(); c++){
			for(size_t o=0; o < Collections[c]->Objects.size(); o++){
				Object& obj = *Collections[c]->Objects[o];	
				if (obj.seleccionado){
					TransformPivotPointFloat[0] += obj.posX;
					TransformPivotPointFloat[1] += obj.posY;
					TransformPivotPointFloat[2] += obj.posZ;	
				};

				//esto va a dar errores si el padre y el hijo estan seleccionados
				/*TInt ParentID = obj.Parent;
				while (ParentID  > -1) {		
					Object& parentObj = Objects[ParentID];
					TransformPivotPointFloat[0] += parentObj.posX;
					TransformPivotPointFloat[1] += parentObj.posY;
					TransformPivotPointFloat[2] += parentObj.posZ;	
					ParentID = parentObj.Parent;		
				}*/
			}
		}
		TransformPivotPointFloat[0] = TransformPivotPointFloat[0]/SelectCount;
		TransformPivotPointFloat[1] = TransformPivotPointFloat[1]/SelectCount;
		TransformPivotPointFloat[2] = TransformPivotPointFloat[2]/SelectCount;
	}
}

// Función para guardar la posición actual del mouse
void GuardarMousePos() {
    //SDL_GetMouseState(&lastMouseX, &lastMouseY);
    float mx, my;                       // variables temporales
    SDL_GetMouseState(&mx, &my);      // SDL devuelve int
    lastMouseX = mx;           // convertimos a float
    lastMouseY = my;
}

void guardarEstado(){
	GuardarMousePos();
	estadoObjetos.clear();
	//estadoObjetos.ReserveL(SelectCount);
	estadoObjetos.reserve(SelectCount);
	for(size_t c=0; c < Collections.size(); c++){
		for(size_t o=0; o < Collections[c]->Objects.size(); o++){
			Object& obj = *Collections[c]->Objects[o];
			if (obj.seleccionado){
				SaveState NuevoEstado;
				NuevoEstado.obj = &obj;
				NuevoEstado.posX = obj.posX;
				NuevoEstado.posY = obj.posY;
				NuevoEstado.posZ = obj.posZ;
				NuevoEstado.rotX = obj.rotX;
				NuevoEstado.rotY = obj.rotY;
				NuevoEstado.rotZ = obj.rotZ;
				NuevoEstado.scaleX = obj.scaleX;
				NuevoEstado.scaleY = obj.scaleY;
				NuevoEstado.scaleZ = obj.scaleZ;
				estadoObjetos.push_back(NuevoEstado);
			}
		}	
}
	SetTransformPivotPoint();
};

void SetPosicion(){
	if (SelectActivo && InteractionMode == ObjectMode && SelectActivo->seleccionado && estado == editNavegacion){
		guardarEstado();
		estado = translacion;
		//if (axisSelect > 2){axisSelect = X;}
		axisSelect = ViewAxis;
	}
	/*else {
		axisSelect = X;
	}*/
	/*if (estado == rotacion){
		SetRotacion(0);
	}*/
    ReloadViewport(true);	
};

void DuplicatedObject(){	
	if (estado != editNavegacion || InteractionMode != ObjectMode){return;};
	for(size_t c=0; c < Collections.size(); c++){
		for(size_t o=0; o < Collections[c]->Objects.size(); o++){
			Object& obj = *Collections[c]->Objects[o];
			if (!obj.seleccionado){continue;};

			// Crear copia dinámica
			Object* NewObj = new Object(obj); // usa el constructor copia

			obj.seleccionado = false;	
			if (SelectActivo == &obj){
				SelectActivo = NewObj;
			}
			reinterpret_cast<Text*>(NewObj->name->data)->SetValue(
				SetName(reinterpret_cast<Text*>(obj.name->data)->value)
			);
			AddToCollection(CollectionActive, NewObj);

			//si es un mesh
			if (obj.type == mesh){			
				Mesh tempMesh;	
				
				Meshes.push_back(tempMesh);
				Mesh& originaMesh = Meshes[obj.Id];
				NewObj->Id = Meshes.size()-1;
				Mesh& pMesh = Meshes[NewObj->Id];
				
				pMesh.vertexSize = originaMesh.vertexSize;
				pMesh.vertex = new GLshort[pMesh.vertexSize*3];
				pMesh.vertexColor = new GLubyte[pMesh.vertexSize*4];
				pMesh.normals = new GLbyte[pMesh.vertexSize*3];
				pMesh.uv = new GLfloat[pMesh.vertexSize*2];

				for (int i = 0; i < pMesh.vertexSize*3; i++) {
					pMesh.vertex[i] = originaMesh.vertex[i];
					pMesh.normals[i] = originaMesh.normals[i];
				}
				for (int i = 0; i < pMesh.vertexSize*4; i++) {
					pMesh.vertexColor[i] = originaMesh.vertexColor[i];
				}
				for (int i = 0; i < pMesh.vertexSize*2; i++) {
					pMesh.uv[i] = originaMesh.uv[i];
				}

				pMesh.facesSize = originaMesh.facesSize;

				pMesh.faces = new GLushort[pMesh.facesSize];
				for (int i = 0; i < pMesh.facesSize; i++) {
					pMesh.faces[i] = originaMesh.faces[i];
				}		

				//creamos el objeto y le asignamos la mesh	
				MaterialGroup tempFaceGroup;
				pMesh.materialsGroup.clear();
				//pMesh.materialsGroup.ReserveL(originaMesh.materialsGroup.size());
				for (int i = 0; i < (int)originaMesh.materialsGroup.size(); i++) {
					tempFaceGroup.startDrawn = originaMesh.materialsGroup[i].startDrawn;
					tempFaceGroup.material = originaMesh.materialsGroup[i].material;
					tempFaceGroup.count = originaMesh.materialsGroup[i].count;
					tempFaceGroup.indicesDrawnCount = originaMesh.materialsGroup[i].indicesDrawnCount;
					pMesh.materialsGroup.push_back(tempFaceGroup);
				}
			}
		}
	}
	SetPosicion();
	redibujar = true;
}

void DuplicatedLinked(){
	if (estado != editNavegacion || InteractionMode != ObjectMode){return;};
	for(size_t c=0; c < Collections.size(); c++){
		for(size_t o=0; o < Collections[c]->Objects.size(); o++){
			Object& obj = *Collections[c]->Objects[o];
			if (!obj.seleccionado){continue;};

			// Crear copia dinámica
			Object* NewObj = new Object(obj); // usa el constructor copia

			obj.seleccionado = false;	
			NewObj->seleccionado = true;
			reinterpret_cast<Text*>(NewObj->name->data)->SetValue(
				SetName(reinterpret_cast<Text*>(obj.name->data)->value)
			);

			if (SelectActivo == &obj){
				SelectActivo = NewObj;
			}

			AddToCollection(CollectionActive, NewObj);
		}
	}
	SetPosicion();
	redibujar = true;
}

void SetRotacion(int dx, int dy){
	for (size_t o = 0; o < estadoObjetos.size(); o++) {
		Object& obj = *estadoObjetos[o].obj;
		switch (axisSelect) {
			case ViewAxis:
				/*Objects[estadoObjetos[o].indice].rotX -= valor;

				Vec3 objPos = Objects[estadoObjetos[o].indice].pos;
				Vec3 screenPos = ProjectToScreen(objPos); // convierte mundo -> pantalla

				int dxScreen = mouseX - screenPos.x;
				int dyScreen = mouseY - screenPos.y;*/
				break;
			case X:
				obj.rotX -= dx;
				obj.rotX -= dy;
				break;
			case Y:
				obj.rotY -= dx;
				obj.rotY -= dy;
				break;
			case Z:
				obj.rotZ -= dx;
				obj.rotZ -= dy;
				break;
		}
	}
}

void SetRotacion(){
	//si no hay objetos
	if (SelectActivo && SelectActivo->seleccionado && estado == editNavegacion){
		guardarEstado();
		estado = rotacion;	
		valorRotacion = 0;
		//axisSelect = ViewAxis;
		if (axisSelect > 2){axisSelect = X;}
	}	
	//esto es para symbian. la tecla 2 es para rotar. pero tambien para seleccionar el eje Y
	/*else {
		axisSelect = Y;
	}*/
	/*if (estado == rotacion){
		SetRotacion(0, 0);
	}*/
    ReloadViewport(true);	
};

void SetScale(int dx, int dy){
	dx = dx*500;
	dy = dy*500;
	for (size_t o = 0; o < estadoObjetos.size(); o++) {
		Object& obj = *estadoObjetos[o].obj;
		switch (axisSelect) {
			case X:
				obj.scaleX += dx;
				obj.scaleX += dy;
				break;
			case Y:
				obj.scaleY += dx;
				obj.scaleY += dy;
				break;
			case Z:
				obj.scaleZ += dx;
				obj.scaleZ += dy;
				break;
			case XYZ:
				obj.scaleX += dx;
				obj.scaleY += dx;
				obj.scaleZ += dx;
				obj.scaleX += dy;
				obj.scaleY += dy;
				obj.scaleZ += dy;
				break;
		}
	}
	redibujar = true;
}

void SetEscala(){
	//XYZ tiene escala
	//si no hay objetos
	if (SelectActivo && SelectActivo->seleccionado && estado == editNavegacion){
		estado = EditScale;
		guardarEstado();
		axisSelect = XYZ;	
	}
	//esto era para symbian- porque "escala" y "eje Z" es el numero 3
	/*else {
		axisSelect = Z;
	}*/
	/*if (estado == rotacion){
		SetRotacion(0, 0);
	}*/
    ReloadViewport(true);
};

void SetTranslacionObjetos(int dx, int dy, float factor = 1.0f){
	for (size_t o = 0; o < estadoObjetos.size(); o++) {
		Object& obj = *estadoObjetos[o].obj;
		switch (axisSelect) {
			case ViewAxis: {
                //float radY = Viewports3D[ViewportId].rotY * M_PI / 180.0f;
                //float radX = Viewports3D[ViewportId].rotX * M_PI / 180.0f;

                //float cosX = cos(radX);
                //float sinX = sin(radX);
                //float cosY = cos(radY);
                //float sinY = sin(radY);

                Object& obj = *estadoObjetos[o].obj;
                obj.posZ -= dy * factor * precalculado.cosY;
                obj.posX += dx * factor * precalculado.cosX - dy * factor * precalculado.sinY * precalculado.sinX;
                obj.posY += dx * factor * precalculado.sinX + dy * factor * precalculado.sinY * precalculado.cosX;

				/*std::cout << "cameraDistance " << cameraDistance 
				        << " | PivotY " << PivotY						
						<< " | posY: " << posY << std::endl;

				// factor relativo a distancia
				float factorDist = (cameraDistance - posY ) * 0.0055f;

				obj.posZ -= dy * factor * cosY * factorDist;
				obj.posX += dx * factor * cosX * factorDist - dy * factor * sinY * sinX * factorDist;
				obj.posY += dx * factor * sinX * factorDist + dy * factor * sinY * cosX * factorDist;*/
                break;
            }
			case X:
				obj.posX += dx;
				obj.posX += dy;
				break;
			case Y:
				obj.posY -= dx;
				obj.posY -= dy;
				break;
			case Z:
				obj.posZ -= dx;
				obj.posZ -= dy;
				break;
		}
	}
}