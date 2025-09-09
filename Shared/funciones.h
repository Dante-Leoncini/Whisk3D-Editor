void RecalcViewPos(){		
	Object& obj = Objects[CameraActive];
	rotX = -obj.rotZ+90;
	rotY = -obj.rotY;	
	PivotX = -obj.posX;
	PivotY = -obj.posY;
	PivotZ = -obj.posZ;

	//en caso de que este emparentado
	/*TInt ParentID = obj.Parent;
	while (ParentID  > -1) {		
		Object& parentObj = Objects[ParentID];		
		rotX = -parentObj.rotZ;
		rotY = -parentObj.rotY;	
		//PivotX = -parentObj.posX;
		//PivotY = -parentObj.posY;
		//PivotZ = -parentObj.posZ;	
		ParentID = parentObj.Parent;		
	}*/
}

void DeseleccionarTodo(){
	if (InteractionMode == ObjectMode){
		//for(int o=0; o < Objects.Count(); o++){
		for(size_t o=0; o < Objects.size(); o++){
			Objects[o].seleccionado = false;				
		}
		SelectCount = 0;
	}
}

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

void ReestablecerEstado(){
	if (InteractionMode == ObjectMode){
		//for(int o=0; o < estadoObjetos.Count(); o++){
		for(size_t o=0; o < estadoObjetos.size(); o++){
			SaveState& estadoObj = estadoObjetos[o];
			Object& obj = Objects[estadoObj.indice];
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
	if (estado != editNavegacion){
		ReestablecerEstado();
	}
};

void AddMesh( int modelo ){
	Cancelar();
	DeseleccionarTodo();
	//Collection.Append(Objects.Count());
	Collection.push_back(Objects.size());
	//SelectActivo = Objects.Count();
	SelectActivo = Objects.size();
	SelectCount = 1;
	
	Object TempObj;	
	//Objects.Append(TempObj);
	Objects.push_back(TempObj);
	//Object& obj = Objects[Objects.Count()-1];
	Object& obj = Objects[Objects.size()-1];
	Objects[SelectActivo].seleccionado = true;
	
	//obj.Id = Meshes.Count();
	obj.Id = Meshes.size();
	obj.type = mesh;
	obj.visible = true;
	obj.posX = Cursor3DposX;
	obj.posY = Cursor3DposY;
	obj.posZ = Cursor3DposZ;
	obj.rotX = obj.rotY = obj.rotZ = 0;
	obj.scaleX = obj.scaleY = obj.scaleZ = 45000;
	obj.Parent = -1;
	
	Mesh tempMesh;
	//Meshes.Append(tempMesh);
	Meshes.push_back(tempMesh);
	Mesh& pMesh = Meshes[obj.Id];	
	
	MaterialGroup tempFaceGroup;
	tempFaceGroup.startDrawn = 0;
	tempFaceGroup.material = 0;
	
	VertexGroup TempVertexGroups;	
	
	if (modelo == plane){ pMesh.vertexSize = 4;
		pMesh.vertex = new GLshort[pMesh.vertexSize*3];
		pMesh.vertexColor = new GLubyte[pMesh.vertexSize*4];
		pMesh.normals = new GLbyte[pMesh.vertexSize*3];
		pMesh.uv = new GLfloat[pMesh.vertexSize*2];
		/*HBufC* noteBuf = HBufC::NewLC(100);
		noteBuf->Des().Copy(_L("Add Plane Size"));
		TInt InputSize = DialogNumber(2, 0, 20, noteBuf);	*/	
		int InputSize = 2;
		//CleanupStack::PopAndDestroy(noteBuf);
		InputSize = InputSize*1000;	

		for (int i = 0; i < pMesh.vertexSize*3; i++) {
			pMesh.vertex[i] = PlaneVertices[i]*InputSize;
		}
		for (int i = 0; i < pMesh.vertexSize; i++) {
			pMesh.normals[i*3] = 0;
			pMesh.normals[i*3+1] = 127;
			pMesh.normals[i*3+2] = 0;
		}
		for (int i = 0; i < pMesh.vertexSize*4; i++) {
			pMesh.vertexColor[i] = 255;
		}
		for (int i = 0; i < pMesh.vertexSize*2; i++) {
			pMesh.uv[i] = (GLfloat)PlaneUV[i];
		}

		//pMesh.facesCount = tempFaceGroup.count = 2;
		pMesh.facesSize = tempFaceGroup.indicesDrawnCount = 6;

		pMesh.faces = new GLushort[tempFaceGroup.indicesDrawnCount];
		for (int i = 0; i < tempFaceGroup.indicesDrawnCount; i++) {
			pMesh.faces[i] = PlaneTriangles[i];
		}
	}	
	else if (modelo == circle){ 
		//HBufC* noteBuf = HBufC::NewLC(100);
		//noteBuf->Des().Copy(_L("Add Circle vertices"));
		//TInt originalVertexSize  = DialogNumber(8, 3, 512, noteBuf);	
		int originalVertexSize = 8;
		pMesh.vertexSize = originalVertexSize + 1;  // Agregar un vértice adicional
		//noteBuf->Des().Copy(_L("Add Circle radius cm"));
		//TInt Radius = DialogNumber(100, 1, 10000, noteBuf);	
		int Radius = 100;	
		//CleanupStack::PopAndDestroy(noteBuf);
		Radius = Radius*20;	

		pMesh.vertex = new GLshort[pMesh.vertexSize*3];
		pMesh.vertexColor = new GLubyte[pMesh.vertexSize*4];
		pMesh.normals = new GLbyte[pMesh.vertexSize*3];
		pMesh.uv = new GLfloat[pMesh.vertexSize*2];

		for (int i = 0; i < pMesh.vertexSize*3; i++) {
			pMesh.vertex[i] = Radius;
		}

		for (int i = 0; i < pMesh.vertexSize; i++) {
			GLfloat angle = 2.0f * M_PI * (i % originalVertexSize) / originalVertexSize;  // angulo para los vértices

			pMesh.vertex[i*3] = -Radius * cos(angle);
			pMesh.vertex[i*3+1] = 0;
			pMesh.vertex[i*3+2] = -Radius * sin(angle);
			pMesh.normals[i*3] = 0;
			pMesh.normals[i*3+1] = 127;
			pMesh.normals[i*3+2] = 0;
			pMesh.uv[i * 2] = static_cast<GLfloat>(cos(angle) * 127);
			pMesh.uv[i * 2 + 1] = static_cast<GLfloat>(sin(angle) * 127);
		}

		for (int i = 0; i < pMesh.vertexSize*4; i++) {
			pMesh.vertexColor[i] = 255;
		}

		pMesh.facesSize = tempFaceGroup.indicesDrawnCount = (pMesh.vertexSize - 2) * 3;

		pMesh.faces = new GLushort[tempFaceGroup.indicesDrawnCount];
		for (int i = 0; i < originalVertexSize - 1; i++) {
			pMesh.faces[i * 3] = 0;          // Primer vértice
			pMesh.faces[i * 3 + 1] = i + 2;  // Segundo vértice
			pMesh.faces[i * 3 + 2] = i + 1;  // Tercer vértice
		}
	}
	else if (modelo == cubo){ 
    	pMesh.vertexSize = 24;
		pMesh.vertex = new GLshort[pMesh.vertexSize*3];
		pMesh.vertexColor = new GLubyte[pMesh.vertexSize*4];
		pMesh.normals = new GLbyte[pMesh.vertexSize*3];
		pMesh.uv = new GLfloat[pMesh.vertexSize*2];

		for (int i = 0; i < pMesh.vertexSize*3; i++) {
			pMesh.vertex[i] = CuboVertices[i];
			pMesh.normals[i] = CuboNormals[i];
		}
		for (int i = 0; i < pMesh.vertexSize*4; i++) {
			pMesh.vertexColor[i] = 255;
		}
		for (int i = 0; i < pMesh.vertexSize*2; i++) {
			//pMesh.uv[i] = (GLfloat)((CuboUV[i]+128)/255)*1280;
			pMesh.uv[i] = (GLfloat)CuboUV[i];
		}

		pMesh.facesSize = tempFaceGroup.indicesDrawnCount = 36;

		pMesh.faces = new GLushort[tempFaceGroup.indicesDrawnCount];
		for (int i = 0; i < tempFaceGroup.indicesDrawnCount; i++) {
			pMesh.faces[i] = CuboTriangles[i];
		}
	}	

	//creamos el objeto y le asignamos la mesh	
	//Meshes[obj.Id].materialsGroup.Append(tempFaceGroup);
	Meshes[obj.Id].materialsGroup.push_back(tempFaceGroup);
    redibujar = true;
}

void AddObject( int tipo ){
	//Cancelar();
	Object obj;
	obj.type = tipo;
	obj.visible = true;
	obj.seleccionado = false;
	obj.posX = Cursor3DposX;
	obj.posY = Cursor3DposY;
	obj.posZ = Cursor3DposZ;
	obj.rotX = obj.rotY = obj.rotZ = 0;
	obj.scaleX = obj.scaleY = obj.scaleZ = 45000;
	obj.Parent = -1;	
	obj.Id = -0;
	//Objects.Append(obj);
	Objects.push_back(obj);
	//Collection.Append(Objects.Count()-1);
	Collection.push_back(Objects.size()-1);
	//SelectActivo = Objects.Count()-1;
	SelectActivo = Objects.size()-1;
	if (tipo == light){
		Light tempLight;
		tempLight.type = pointLight;
		tempLight.lightId = nextLightId;
		tempLight.color[0] = 1.0;
		tempLight.color[1] = 1.0;
		tempLight.color[2] = 1.0;
		tempLight.color[3] = 1.0;
		//tempLight.color  = { MATERIALCOLOR(1.0, 1.0, 1.0, 1.0) };

		glEnable( nextLightId );		
		GLfloat lightDiffuseSpot[4]   = { 1.0, 1.0, 1.0, 1.0 };
		GLfloat lightSpecularSpot[4]  = { 1.0, 1.0, 1.0, 1.0 };
		glLightfv(  nextLightId, GL_DIFFUSE,  lightDiffuseSpot  );
		glLightfv(  nextLightId, GL_AMBIENT,  lightAmbient  );
		glLightfv(  nextLightId, GL_SPECULAR, lightSpecularSpot );
		//glLightfv(  nextLightId, GL_POSITION, lightPositionSpot );
		glLightfv(  nextLightId, GL_POSITION, positionPuntualLight );
		

		glLightf(   nextLightId, GL_CONSTANT_ATTENUATION,  1.5  );
		glLightf(   nextLightId, GL_LINEAR_ATTENUATION,    0.5  );
		glLightf(   nextLightId, GL_QUADRATIC_ATTENUATION, 0.5  );

		/*glLightf(   nextLightId, GL_SPOT_CUTOFF,   170.0                );
		glLightf(   nextLightId, GL_SPOT_EXPONENT,  20.0                );
		glLightfv(  nextLightId, GL_SPOT_DIRECTION, lightDirectionSpot );*/
		nextLightId++;

		//Lights.Append(tempLight);
		Lights.push_back(tempLight);
		//obj.Id = Lights.Count()-1;
		obj.Id = Lights.size()-1;
	}
	//tipo camara
	else if (tipo == camera){
		if (CameraActive < 0){
			CameraActive = SelectActivo;		
		}		
	}
	DeseleccionarTodo();
	Objects[SelectActivo].seleccionado = true;
	SelectCount = 1;
    redibujar = true;
}

void NewMaterial(bool reemplazar){
	/*HBufC* inicialBuf = HBufC::NewLC(100);
	_LIT(Kinicial, "Material.%03d");
	inicialBuf->Des().Format(Kinicial, Materials.Count()+1);
	
	HBufC* tituloBuf = HBufC::NewLC(100);
	_LIT(Ktitulo, "Ingrese el nombre del Material");
	tituloBuf->Des().Copy(Ktitulo);
	TPtr nombre = DialogText(inicialBuf, tituloBuf);
	
	HBufC* noteBuf = HBufC::NewLC(100);//textoingresado.AllocLC();
	noteBuf->Des().Copy(textoingresado);
	CDialogs::Alert(noteBuf);
	CleanupStack::PopAndDestroy(noteBuf);	
	CleanupStack::PopAndDestroy(tituloBuf);	
	CleanupStack::PopAndDestroy(inicialBuf);	*/
	
	Material mat;	
	mat.specular[0] = mat.specular[1] = mat.specular[2] = mat.specular[3] = 0.3;
	mat.diffuse[0] = mat.diffuse[1] = mat.diffuse[2] = mat.diffuse[3] = 1.0;
	mat.emission[0] = mat.emission[1] = mat.emission[2] = mat.emission[3] = 0.0;
	mat.textura = false;
	mat.vertexColor = false;
	mat.repeat = true;
	mat.lighting = true;
	mat.culling = true;
	mat.transparent = false;
	mat.interpolacion = lineal;
	mat.textureID = 0;
	//if (Materials.Count() < 1){
	if (Materials.size() < 1){
		/*mat.name = HBufC::NewL(15);
		_LIT(KMatNameDefault, "DefaultMaterial");
		mat.name->Des().Format(KMatNameDefault, Materials.Count()+1);*/
	}
	else {
		/*mat.name = HBufC::NewL(12);
		_LIT(KMatName, "Material.%03d");
		mat.name->Des().Format(KMatName, Materials.Count()+1);*/
	}
	//Materials.Append(mat);
	Materials.push_back(mat);
	
	//si no hay objetos
	//if (Objects.Count() < 1){return;}	
	if (Objects.size() < 1){return;}	
	Object& obj = Objects[SelectActivo];
	//si no es un mesh
	if (obj.type != mesh || !obj.seleccionado){return;}		
	Mesh& pMesh = Meshes[obj.Id];

	if (reemplazar){
		/*HBufC* noteBuf = HBufC::NewLC(100);
		_LIT(KFormatString, "Old Material 1 to %d");
		noteBuf->Des().Format(KFormatString, pMesh.materialsGroup.Count());*/
		int OldMaterialID = 1;
		//if (pMesh.materialsGroup.Count() > 1){
		if (pMesh.materialsGroup.size() > 1){
			//OldMaterialID = DialogNumber(1, 1, pMesh.materialsGroup.Count(), noteBuf);
			//OldMaterialID = DialogNumber(1, 1, pMesh.materialsGroup.size(), noteBuf);
		}
		//pMesh.materialsGroup[OldMaterialID-1].material = Materials.Count()-1;
		pMesh.materialsGroup[OldMaterialID-1].material = Materials.size()-1;
	}
	redibujar = true;
}

//mira si no hay camara activa
//si no hay una camara activa. busca una camara para asignarla
//si no hay camaras... quedara en -1
void CheckCameraState(){
	if (CameraActive < 0){
		//for(TInt i=0; i < Objects.Count(); i++){
		for(size_t i=0; i < Objects.size(); i++){
			if (Objects[i].type == camera){
				CameraActive = i;
				return;
			}		
		}
	}	
}

void SetViewpoint(int opcion){
	switch (opcion) {
		case top:
			rotX = -180.0;
			rotY = 90.0;
			ViewFromCameraActive = false;	
			CameraToView = false;
			break;
		case front:
			rotX = -180.0;
			rotY = 0.0;	
			ViewFromCameraActive = false;	
			CameraToView = false;
			break;
		case right:
			rotX = 90.0;
			rotY = 0.0;		
			ViewFromCameraActive = false;	
			CameraToView = false;
			break;
		case cameraView:
			CheckCameraState();
			if (CameraActive < 0){
				//_LIT(KFormatString, "There are no cameras!");
				//HBufC* noteBuf = HBufC::NewLC(50);
				//noteBuf->Des().Format(KFormatString);
				//MensajeError(noteBuf);  
				//CleanupStack::PopAndDestroy(noteBuf);
			}
			//else if (Objects.Count() > CameraActive && !ViewFromCameraActive){	
			else if (Objects.size() > static_cast<size_t>(CameraActive) && !ViewFromCameraActive){	
				LastRotX = rotX;
				LastRotY = rotY;	
				LastPivotX = PivotX;
				LastPivotY = PivotY;
				LastPivotZ = PivotZ;
				RecalcViewPos();
				ViewFromCameraActive = true;
			}
			break;
	}
	redibujar = true;
	std::cout << "rotX: " << rotX << " | rotY: " << rotY << std::endl;
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

void EnfocarObject(){
	//si no hay objetos
	//if (Objects.Count() < 1){return;}
	if (Objects.size() < 1){return;}
	SetTransformPivotPoint();	
	PivotX = 0.0f; 
	PivotY = 0.0f;
	PivotZ = 0.0f;
	PivotX = PivotX-TransformPivotPointFloat[0]; 
	PivotY = PivotY-TransformPivotPointFloat[1];
	PivotZ = PivotZ-TransformPivotPointFloat[2];
    redibujar = true;
}

void changeSelect(){
	if (InteractionMode == ObjectMode){
		//si no hay objetos
		//o si esta moviendo, rotando o haciendo algo... no deja que continue
		//if (1 > Objects.Count() || estado != editNavegacion){
		if (1 > Objects.size() || estado != editNavegacion){
			return;
		}
		//DeseleccionarTodo();
		//deselecciona el objeto actual si es que estaba seleccionado
		if (Objects[SelectActivo].seleccionado){
			Objects[SelectActivo].seleccionado = false;
			SelectCount--;
		}

		//pasa al siguiente
		SelectActivo++;
		if (static_cast<size_t>(SelectActivo) >= Objects.size()){
			SelectActivo = 0;
		}
		//selecciona el proximo objeto
		if (!Objects[SelectActivo].seleccionado){
			Objects[SelectActivo].seleccionado = true;
			SelectCount++;
		}
	}
    redibujar = true;	
}

//coloca el cursor 3d desde la vista 3d
void SetCursor3D(){// 1) Calcular base de la cámara (forward/right/up)
    float pitch = rotX * DEG2RAD;
    float yaw = rotY * DEG2RAD;

    Vec3 forward(cosf(pitch) * sinf(yaw), sinf(pitch), cosf(pitch) * cosf(yaw));
    forward = Normalize(forward);

    Vec3 worldUp(0, 1, 0);
    Vec3 right = Cross(forward, worldUp);
    float rlen = Len(right);
    if (rlen < 1e-8f) {
        right = Vec3(1, 0, 0); // Evitar degeneración en pitch ±90°
    } else {
        right = right * (1.0f / rlen);
    }

    Vec3 up = Cross(right, forward); // Unitario por construcción

    // 2) Posición de la cámara
    Vec3 pivotPos(PivotX + posX, PivotY + posY, PivotZ + posZ);
    Vec3 camPos = pivotPos - forward * cameraDistance;

    // 3) Mouse a NDC
    float ndcX = (2.0f * (float)lastMouseX / (float)winW) - 1.0f;
    float ndcY = 1.0f - (2.0f * (float)lastMouseY / (float)winH);

    // 4) Calcular dirección del rayo en el espacio de la cámara
    float halfFovRad = fovDeg * DEG2RAD * 0.5f;
    float halfH = tanf(halfFovRad);
    float halfW = aspect * halfH;

    // Rayo en el espacio de la cámara (sin normalizar)
    Vec3 rayDir = forward + right * (ndcX * halfW) + up * (ndcY * halfH);

    // 5) Intersección con un plano perpendicular al forward, pasando por el pivot
    // Plano: punto = pivotPos, normal = forward
    // Rayo: origen = camPos, dirección = rayDir
    // Ecuación: dot((camPos + t * rayDir - pivotPos), forward) = 0
    float denom = Dot(rayDir, forward);
    if (fabs(denom) < 1e-8f) {
        // Rayo paralelo al plano, usar posición por defecto
        Cursor3DposX = pivotPos.x;
        Cursor3DposY = pivotPos.y;
        Cursor3DposZ = pivotPos.z;
        return;
    }

    float t = Dot(pivotPos - camPos, forward) / denom;
    if (t < 0) {
        // Intersección detrás de la cámara, usar posición por defecto
        Cursor3DposX = pivotPos.x;
        Cursor3DposY = pivotPos.y;
        Cursor3DposZ = pivotPos.z;
        return;
    }

    Vec3 cursorPos = camPos + rayDir * t;

    Cursor3DposX = cursorPos.x;
    Cursor3DposY = cursorPos.y;
    Cursor3DposZ = cursorPos.z;
}

// Función para guardar la posición actual del mouse
void GuardarMousePos() {
    SDL_GetMouseState(&lastMouseX, &lastMouseY);
	/*std::cout << "Mouse guardado en: X=" << lastMouseX 
              << " Y=" << lastMouseY << std::endl;*/
}

void guardarEstado(){
	GuardarMousePos();
	//estadoObjetos.Close();
	estadoObjetos.clear();
	//estadoObjetos.ReserveL(SelectCount);
	estadoObjetos.reserve(SelectCount);
	//for(int o=0; o < Objects.Count(); o++){
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
			//estadoObjetos.Append(NuevoEstado);
			estadoObjetos.push_back(NuevoEstado);
		}
	}	
	SetTransformPivotPoint();
};

void BorrarMesh(int indice){
	int links = 0;
	
	for(size_t o=0; o < Objects.size(); o++){
		if (Objects[o].type == mesh && Objects[o].Id == indice){links++;};				
	}

	if (links < 2){	
		for(size_t o=0; o < Objects.size(); o++){
			if (Objects[o].type == mesh && Objects[o].Id > indice){
				Objects[o].Id--;
			};				
		}
		Meshes[indice].LiberarMemoria();
		//Meshes.Remove(indice);

		// borrar el elemento en posición `indice`
		if (indice >= 0 && static_cast<size_t>(indice) < Meshes.size()) {
			Meshes.erase(Meshes.begin() + indice);
		}
	}
}

void BorrarAnimaciones(int indice){
	for(size_t a = 0; a < AnimationObjects.size(); a++) {
		if (AnimationObjects[a].Id == indice) {	
			for(size_t p = 0; p < AnimationObjects[a].Propertys.size(); p++) {
				AnimationObjects[a].Propertys[p].keyframes.clear();
			}				
			AnimationObjects[a].Propertys.clear();
			//AnimationObjects.Remove(a);
			if (a >= 0 && a < AnimationObjects.size()) {
				AnimationObjects.erase(AnimationObjects.begin() + a);
			}
		}
		// Hace falta cambiar los indices
		else if (AnimationObjects[a].Id > indice) {
			AnimationObjects[a].Id--;
		}			
	}
}

void BorrarObjeto(int indice){
	Object& obj = Objects[indice];
	// Liberar memoria de los punteros del objeto seleccionado
	if (obj.type == mesh){
		BorrarMesh(obj.Id);
		std::cout << "era un mesh" << std::endl;
	}

	//si existe animaciones para ese objeto. las borra		
	BorrarAnimaciones(indice);
	std::cout << "borrar animaciones" << std::endl;

	// Borrar de la coleccion
	for (int c = static_cast<int>(Collection.size()) - 1; c >= 0; c--) {
		if (Collection[c] == indice) {
			//Collection.Remove(c);
			// borrar el elemento en posición `indice`
			if (c >= 0 && static_cast<size_t>(c) < Collection.size()) {
				Collection.erase(Collection.begin() + c);
			}
		}
		// Hace falta cambiar los indices
		else if (Collection[c] > indice) {
			Collection[c]--;
		}
	}

	//si es la camara activa. borra el indice
	if (CameraActive == indice){
		CameraActive = -1;	
		ViewFromCameraActive = false;	
	}
	//si era mas grande. resta uno para que el indice apunte a la camara correcta
	else if (CameraActive > indice){
		CameraActive--;
	}

	//Objects.Remove(indice);
	if (indice >= 0 && static_cast<size_t>(indice) < Objects.size()) {
		Objects.erase(Objects.begin() + indice);
	}

	SelectCount--;
	SelectActivo = 0;
	/*if (Objects.Count() > 0){
		SelectCount = 1;
		SelectActivo = Objects.Count()-1;
		Objects[SelectActivo].seleccionado = true;
	}
	else {
		SelectCount = 0;
		SelectActivo = 0;
	}*/
	
	// Actualizar indices en los objetos
	std::cout << "ya casi" << std::endl;
	for (int o = 0; o < static_cast<int>(Objects.size()); o++) {
		for (int c = static_cast<int>(Objects[o].Childrens.size()) - 1; c >= 0; c--) {
			if (Objects[o].Childrens[c].Id == indice) {
				//Objects[o].Childrens.Remove(c);
				if (c >= 0 && static_cast<size_t>(c) < Objects.size()) {
					Objects.erase(Objects.begin() + c);
				}
			} 
			else if (Objects[o].Childrens[c].Id > indice) {
				Objects[o].Childrens[c].Id--;
			}
		}
		//borra y actualiza los padres
		if (Objects[o].Parent == indice){				
			Objects[o].Parent = -1;
			//Collection.Append(o);
			Collection.push_back(o);
		} 
		else if (Objects[o].Parent > indice) {
			Objects[o].Parent--;
		}
	}
}

void Borrar(){
	if (estado != editNavegacion ){
		Cancelar();
	}
	else if (InteractionMode == ObjectMode){
		if (Objects.size() < 1){return;}

		//si no hay nada seleccionado. no borra
		bool algoSeleccionado = false;
		for (size_t o = Objects.size() - 1; o >= 0; o--) {
			if (Objects[o].seleccionado){
				algoSeleccionado = true;
				break;	
			}		
		}
		if (!algoSeleccionado){return;}
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
		for (size_t o = Objects.size() - 1; o >= 0; o--) {
			if (Objects[o].seleccionado){
				BorrarObjeto(o);
			}			
		}
	}
    ReloadViewport(true);	
}

void SetEje(int eje){
	if (estado != editNavegacion){
		axisSelect = eje;
	}	
    redibujar = true;	
};

void SetRotacion(int dx, int dy){
	for (size_t o = 0; o < estadoObjetos.size(); o++) {
		switch (axisSelect) {
			case ViewAxis:
				/*Objects[estadoObjetos[o].indice].rotX -= valor;

				Vec3 objPos = Objects[estadoObjetos[o].indice].pos;
				Vec3 screenPos = ProjectToScreen(objPos); // convierte mundo -> pantalla

				int dxScreen = mouseX - screenPos.x;
				int dyScreen = mouseY - screenPos.y;*/
				break;
			case X:
				Objects[estadoObjetos[o].indice].rotX -= dx;
				Objects[estadoObjetos[o].indice].rotX -= dy;
				break;
			case Y:
				Objects[estadoObjetos[o].indice].rotY -= dx;
				Objects[estadoObjetos[o].indice].rotY -= dy;
				break;
			case Z:
				Objects[estadoObjetos[o].indice].rotZ -= dx;
				Objects[estadoObjetos[o].indice].rotZ -= dy;
				break;
		}
	}
}

void SetRotacion(){
	//si no hay objetos
	if (Objects.size() < 1){return;}
	else if (Objects[SelectActivo].seleccionado && estado == editNavegacion){
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

void SetScale(int valor){
	valor = valor*1000;
	for (size_t o = 0; o < estadoObjetos.size(); o++) {
		switch (axisSelect) {
			case X:
				Objects[estadoObjetos[o].indice].scaleX += valor;
				break;
			case Y:
				Objects[estadoObjetos[o].indice].scaleY += valor;
				break;
			case Z:
				Objects[estadoObjetos[o].indice].scaleZ += valor;
				break;
			case XYZ:
				Objects[estadoObjetos[o].indice].scaleX += valor;
				Objects[estadoObjetos[o].indice].scaleY += valor;
				Objects[estadoObjetos[o].indice].scaleZ += valor;
				break;
		}
	}
	redibujar = true;
}

void SetEscala(){
	//XYZ tiene escala
	//si no hay objetos
	if (Objects.size() < 1){return;}
	else if (Objects[SelectActivo].seleccionado && estado == editNavegacion){
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

void RestaurarViewport(){
	ViewFromCameraActive = false;
	rotX = LastRotX;
	rotY = LastRotY;	
	PivotX = LastPivotX;
	PivotY = LastPivotY;
	PivotZ = LastPivotZ;
}

void ReloadAnimation(){

}

void SetTranslacionObjetos(int dx, int dy, float factor = 1.0f){
	for (size_t o = 0; o < estadoObjetos.size(); o++) {
		switch (axisSelect) {
			case ViewAxis: {
                float radY = rotY * M_PI / 180.0f;
                float radX = rotX * M_PI / 180.0f;

                //float factor = 1.0f;

                float cosX = cos(radX);
                float sinX = sin(radX);
                float cosY = cos(radY);
                float sinY = sin(radY);

                auto& obj = Objects[estadoObjetos[o].indice];
                obj.posZ -= dy * factor * cosY;
                obj.posX += dx * factor * cosX - dy * factor * sinY * sinX;
                obj.posY += dx * factor * sinX + dy * factor * sinY * cosX;

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
				Objects[estadoObjetos[o].indice].posX += dx;
				Objects[estadoObjetos[o].indice].posX += dy;
				break;
			case Y:
				Objects[estadoObjetos[o].indice].posY -= dx;
				Objects[estadoObjetos[o].indice].posY -= dy;
				break;
			case Z:
				Objects[estadoObjetos[o].indice].posZ -= dx;
				Objects[estadoObjetos[o].indice].posZ -= dy;
				break;
		}
	}
}

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