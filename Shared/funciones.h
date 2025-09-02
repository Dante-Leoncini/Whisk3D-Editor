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