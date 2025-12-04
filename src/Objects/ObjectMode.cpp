#include "Objects/ObjectMode.h"

void ReestablecerEstado(bool ClearEstado){
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
		if (ClearEstado) estadoObjetos.clear();
	}
	if (ClearEstado) {
		estado = editNavegacion;
	}
};

void Cancelar(){
	// Mostrar el cursor
	#if SDL_MAJOR_VERSION == 2
		SDL_ShowCursor(SDL_ENABLE);
	#elif SDL_MAJOR_VERSION == 3
		SDL_ShowCursor();	
	#endif

	if (estado != editNavegacion){
		ReestablecerEstado();
	}
	ViewPortClickDown = false;
};

void EliminarAnimaciones(Object& obj){
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

void Eliminar(bool IncluirCollecciones){
	if (InteractionMode == ObjectMode){
		//si no hay nada seleccionado. no borra
		if (!HayObjetosSeleccionados(IncluirCollecciones)){
			std::cout << "nada seleccionado para borrar" << std::endl;
			return;
		}
		Cancelar();

		if (!SceneCollection) return;

		SceneCollection->EliminarObjetosSeleccionados(IncluirCollecciones);
		ObjActivo = nullptr;
		ObjSelects.clear();
	}
}

void CalcObjectsTransformPivotPoint(Object* obj){
	if (obj->select){ //&& obj->getType() != ObjectType::collection
		//std::cout << "Seleccionado " << obj->name->value << std::endl;
		TransformPivotPointFloat[0] += obj->posX;
		TransformPivotPointFloat[1] += obj->posY;
		TransformPivotPointFloat[2] += obj->posZ;	
	};

	for(size_t c=0; c < obj->Childrens.size(); c++){
		CalcObjectsTransformPivotPoint(obj->Childrens[c]);
	}
}

void SetTransformPivotPoint(){
	if (InteractionMode == ObjectMode){	
		TransformPivotPointFloat[0] = 0.0f;
		TransformPivotPointFloat[1] = 0.0f;
		TransformPivotPointFloat[2] = 0.0f;
		for(size_t c=0; c < SceneCollection->Childrens.size(); c++){
			CalcObjectsTransformPivotPoint(SceneCollection->Childrens[c]);
		}
		size_t SelectCount = ObjSelects.size();
		TransformPivotPointFloat[0] = TransformPivotPointFloat[0]/SelectCount;
		TransformPivotPointFloat[1] = TransformPivotPointFloat[1]/SelectCount;
		TransformPivotPointFloat[2] = TransformPivotPointFloat[2]/SelectCount;
	}
}

// Función para guardar la posición actual del mouse
void GuardarMousePos() {
	#if SDL_MAJOR_VERSION == 2
		int mx, my;                  // SDL2 usa enteros
		SDL_GetMouseState(&mx, &my); // OK
		lastMouseX = (float)mx;      // convertimos después
		lastMouseY = (float)my;
	#elif SDL_MAJOR_VERSION == 3
		float mx, my;                       // variables temporales
		SDL_GetMouseState(&mx, &my);      // SDL devuelve int
		lastMouseX = mx;           // convertimos a float
		lastMouseY = my;
	#endif
}

void guardarEstadoRec(Object* obj){
    if (!obj) return;

    // Si está seleccionado, guardar estado
    if (obj->select && obj->visible) {
        SaveState NuevoEstado;
        NuevoEstado.obj = obj;
        NuevoEstado.posX = obj->posX;
        NuevoEstado.posY = obj->posY;
        NuevoEstado.posZ = obj->posZ;
        NuevoEstado.rotX = obj->rotX;
        NuevoEstado.rotY = obj->rotY;
        NuevoEstado.rotZ = obj->rotZ;
        NuevoEstado.scaleX = obj->scaleX;
        NuevoEstado.scaleY = obj->scaleY;
        NuevoEstado.scaleZ = obj->scaleZ;
        estadoObjetos.push_back(NuevoEstado);
    }

    // Recursión: recorrer hijos
    for (size_t i = 0; i < obj->Childrens.size(); i++) {
        guardarEstadoRec(obj->Childrens[i]);
    }
}

bool guardarEstado(){
    if (!SceneCollection) return false;

    GuardarMousePos();
    estadoObjetos.clear();

    // Recorrer todo el árbol desde la raíz
    guardarEstadoRec(SceneCollection);

	if (estadoObjetos.empty()) return false;	
	std::cout << "moviendo "<< estadoObjetos.size() << " objetos" << std::endl;

    SetTransformPivotPoint();
	return true;
}

void SetPosicion(){
	if (ObjActivo && InteractionMode == ObjectMode && ObjActivo->select && estado == editNavegacion){
		if (!guardarEstado()) return;
		estado = translacion;
		axisSelect = ViewAxis;
	}
};

void DuplicatedObject(){	
	/*if (estado != editNavegacion || InteractionMode != ObjectMode){return;};
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
	SetPosicion();*/
}

void NewInstance(){
	if (estado != editNavegacion || InteractionMode != ObjectMode){return;};
	
	// ITERAMOS AL REVÉS PARA PODER ELIMINAR SIN ROMPER EL ÍNDICE
    for (int i = (int)ObjSelects.size() - 1; i >= 0; i--) {        
        Object* obj = ObjSelects[i];     // objeto original
        if (!obj) continue;

		Instance* instance = new Instance(obj->Parent, obj);
		obj->select = false;
		instance->select = true;
		if (ObjActivo == obj) ObjActivo = instance;
	}
	SetPosicion();
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
	if (ObjActivo && ObjActivo->select && estado == editNavegacion){
		if (!guardarEstado()) return;
		estado = rotacion;	
		valorRotacion = 0;
		//axisSelect = ViewAxis;
		if (axisSelect > 2){axisSelect = X;}
	}	
};

void SetScale(int dx, int dy, float factor){
	//std::cout << "estadoObjetos size: " << estadoObjetos.size() << std::endl;
	float dxf = dx*factor;
	float dyf = dy*factor;
	for (size_t o = 0; o < estadoObjetos.size(); o++) {
		Object& obj = *estadoObjetos[o].obj;
		switch (axisSelect) {
			case X:
				obj.scaleX += dxf;
				obj.scaleX += dyf;
				break;
			case Y:
				obj.scaleY += dxf;
				obj.scaleY += dyf;
				break;
			case Z:
				obj.scaleZ += dxf;
				obj.scaleZ += dyf;
				break;
			case XYZ:
				obj.scaleX += dxf;
				obj.scaleY += dxf;
				obj.scaleZ += dxf;
				obj.scaleX += dyf;
				obj.scaleY += dyf;
				obj.scaleZ += dyf;
				break;
		}
	}
}

void SetEscala(){
	//XYZ tiene escala
	//si no hay objetos
	if (ObjActivo && ObjActivo->select && estado == editNavegacion){
		if (!guardarEstado()) return;
		estado = EditScale;
		axisSelect = XYZ;	
	}
};

void SetTranslacionObjetos(int dx, int dy, float factor){
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
				obj.posX += dx * factor;
				obj.posX += dy * factor;
				break;
			case Y:
				obj.posY -= dx * factor;
				obj.posY -= dy * factor;
				break;
			case Z:
				obj.posZ -= dx * factor;
				obj.posZ -= dy * factor;
				break;
		}
	}
}