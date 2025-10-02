void ReloadViewport(bool hacerRedibujo){
	//Recalcula los constrains
    //for(TInt c = 0; c < Constraints.Count(); c++) {
    for(size_t c = 0; c < Constraints.size(); c++) {
		int id = Constraints[c].Id;
		int Target = Constraints[c].Target;
		switch (Constraints[c].type) {
			case trackto: {
				// Calcular vector dirección
				GLfloat dirX = Objects[Target].posX - Objects[id].posX;
				GLfloat dirY = Objects[Target].posY - Objects[id].posY;
				GLfloat dirZ = Objects[Target].posZ - Objects[id].posZ;						
				
				Objects[id].rotZ = atan2(dirX, dirY) * (180.0 / M_PI);  // Azimut

				// Calcular longitud del vector (magnitud)
				GLfloat length = sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);

				// Cálculo de la elevación (rotY)
				if (Constraints[c].opcion){	
					Objects[id].rotZ += 180; // Para invertir el eje si necesario.
					Objects[id].rotX = asin(dirZ/length) * (180.0 / M_PI);					
				}
				else {
					Objects[id].rotZ -= 90; // Para invertir el eje si necesario.
					Objects[id].rotY = asin(dirZ/length) * (180.0 / M_PI);	
				}

				break;
			}
			case copyrotation:
				Objects[id].rotX = Objects[Target].rotX;
				Objects[id].rotY = Objects[Target].rotY;
				Objects[id].rotZ = Objects[Target].rotZ;
				break;
			case copylocation:
				Objects[id].posX = Objects[Target].posX;
				Objects[id].posY = Objects[Target].posY;
				Objects[id].posZ = Objects[Target].posZ;
				break;
		}
	}
	if (hacerRedibujo){
    	redibujar = true;
	}
}

void SetTransformPivotPoint(){
	//if (Objects.Count() < 1){return;}	
	if (Objects.size() < 1){return;}	
	if (InteractionMode == ObjectMode){	
		TransformPivotPointFloat[0] = 0;
		TransformPivotPointFloat[1] = 0;
		TransformPivotPointFloat[2] = 0;
		//for(TInt i=0; i < Objects.Count(); i++){	
		for(size_t i=0; i < Objects.size(); i++){	
			Object& obj = Objects[i];	
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
		TransformPivotPointFloat[0] = TransformPivotPointFloat[0]/SelectCount;
		TransformPivotPointFloat[1] = TransformPivotPointFloat[1]/SelectCount;
		TransformPivotPointFloat[2] = TransformPivotPointFloat[2]/SelectCount;
	}
}

// Función para guardar la posición actual del mouse
void GuardarMousePos() {
    SDL_GetMouseState(&lastMouseX, &lastMouseY);
}

void guardarEstado(){
	GuardarMousePos();
	estadoObjetos.clear();
	//estadoObjetos.ReserveL(SelectCount);
	estadoObjetos.reserve(SelectCount);
	for(size_t o=0; o < Objects.size(); o++){
		Object& obj = Objects[o];
		if (obj.seleccionado){
			SaveState NuevoEstado;
			NuevoEstado.indice = o;
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
	SetTransformPivotPoint();
};

void SetPosicion(){
	//si no hay objetos
	if (Objects.size() < 1){return;}

	if (InteractionMode == ObjectMode && Objects[SelectActivo].seleccionado && estado == editNavegacion){
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
	int cantObjetosOriginal = Objects.size();
	for(int a=0; a < cantObjetosOriginal; a++){
		Object& obj = Objects[a];
		if (!obj.seleccionado){continue;};
		Objects.push_back(obj);	
		obj.seleccionado = false;	
		int nuevoindice = Objects.size()-1;
		if (SelectActivo == a){
			SelectActivo = nuevoindice;
		}
		Collection.push_back(nuevoindice);
		//si es un mesh
		if (obj.type == mesh){			
			Mesh tempMesh;	
			
			Meshes.push_back(tempMesh);
			Mesh& originaMesh = Meshes[obj.Id];
			obj.Id = Meshes.size()-1;
			Mesh& pMesh = Meshes[obj.Id];
			
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
	SetPosicion();
	redibujar = true;
}

void DuplicatedLinked(){
	if (estado != editNavegacion || InteractionMode != ObjectMode){return;};
	int cantObjetosOriginal = Objects.size();
	for(int a=0; a < cantObjetosOriginal; a++){
		Object& obj = Objects[a];
		if (!obj.seleccionado){continue;};
		Objects.push_back(obj);	
		obj.seleccionado = false;	
		int nuevoindice = Objects.size()-1;
		if (SelectActivo == a){
			SelectActivo = nuevoindice;
		}
		Collection.push_back(nuevoindice);
	}
	SetPosicion();
	redibujar = true;
}