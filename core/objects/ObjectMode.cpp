#include "ObjectMode.h"

void ReestablecerEstado(bool ClearEstado){
	if (InteractionMode == ObjectMode){
		for(size_t o=0; o < estadoObjetos.size(); o++){
			SaveState& estadoObj = estadoObjetos[o];
			Object& obj = *estadoObj.obj;
			obj.pos = estadoObj.pos;
			obj.rot = estadoObj.rot;
			obj.scale = estadoObj.scale;
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
	if (obj->select){
		TransformPivotPoint += obj->pos;
	};

	for(size_t c=0; c < obj->Childrens.size(); c++){
		CalcObjectsTransformPivotPoint(obj->Childrens[c]);
	}
}

void SetTransformPivotPoint(){
	if (InteractionMode == ObjectMode){	
		TransformPivotPoint.x = 0.0f;
		TransformPivotPoint.y = 0.0f;
		TransformPivotPoint.z = 0.0f;
		for(size_t c=0; c < SceneCollection->Childrens.size(); c++){
			CalcObjectsTransformPivotPoint(SceneCollection->Childrens[c]);
		}
		size_t SelectCount = ObjSelects.size();
		TransformPivotPoint.x = TransformPivotPoint.x/SelectCount;
		TransformPivotPoint.y = TransformPivotPoint.y/SelectCount;
		TransformPivotPoint.z = TransformPivotPoint.z/SelectCount;
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
        NuevoEstado.pos = obj->pos;
		NuevoEstado.rot = obj->rot;
        NuevoEstado.scale = obj->scale;
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
	//std::cout << "moviendo "<< estadoObjetos.size() << " objetos" << std::endl;

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
	float ang = (dx + dy) * 0.1f;   // sensibilidad (ajustable)

	for (size_t o = 0; o < estadoObjetos.size(); o++) {
		Object& obj = *estadoObjetos[o].obj;
		switch (axisSelect) {
			case ViewAxis:
				break;
			case X:
                obj.RotateLocal(ang, 0, 0);
				break;
			case Y:
                obj.RotateLocal(0, ang, 0);
				break;
			case Z:
                obj.RotateLocal(0, 0, ang);
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
				obj.scale.x += dxf;
				obj.scale.x += dyf;
				break;
			case Y:
				obj.scale.y += dxf;
				obj.scale.y += dyf;
				break;
			case Z:
				obj.scale.z += dxf;
				obj.scale.z += dyf;
				break;
			case XYZ:
				obj.scale.x += dxf + dyf;
				obj.scale.y += dxf + dyf;
				obj.scale.z += dxf + dyf;
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

void SetTranslacionObjetos(int dx, int dy, float speed){
	for (size_t o = 0; o < estadoObjetos.size(); o++) {
		Object& obj = *estadoObjetos[o].obj;
		switch (axisSelect) {
			case ViewAxis: {
				Vector3 disp = camRight * (dx * speed) + camUp * (-dy * speed);
				obj.pos += disp;
                break;
            }
			case X:
				obj.pos.x += dx * speed;
				obj.pos.x += dy * speed;
				break;
			case Y:
				obj.pos.y -= dx * speed;
				obj.pos.y -= dy * speed;
				break;
			case Z:
				obj.pos.z -= dx * speed;
				obj.pos.z -= dy * speed;
				break;
		}
	}
}