void AddMesh( int modelo ){
	Cancelar();
	DeseleccionarTodo();
	SelectCount = 1;
	
	ObjectsCount++;
	Object* obj = new Object();
	SelectActivo = obj;
	
	obj->Id = Meshes.size();
	obj->type = mesh;
	obj->seleccionado = true;
	obj->visible = true;
	obj->posX = Cursor3DposX;
	obj->posY = Cursor3DposY;
	obj->posZ = Cursor3DposZ;
	obj->rotX = obj->rotY = obj->rotZ = 0;
	obj->scaleX = obj->scaleY = obj->scaleZ = 45000;
	obj->Parent = -1;
	obj->IconType = GetIconType(mesh);
	
	Mesh tempMesh;
	Meshes.push_back(tempMesh);
	Mesh& pMesh = Meshes[obj->Id];	
	
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
		reinterpret_cast<Text*>(obj->name->data)->SetValue(SetName("Cube"));

	}	

	//creamos el objeto y le asignamos la mesh	
	//Meshes[obj.Id].materialsGroup.Append(tempFaceGroup);
	Meshes[obj->Id].materialsGroup.push_back(tempFaceGroup);
	AddToCollection(CollectionActive, obj);
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
	if (!SelectActivo){return;}	
	Object& obj = *SelectActivo;
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
}

void BorrarMesh(int indice){
	/*int links = 0;
	
	for(size_t o=0; o < Objects.size(); o++){
		if (Objects[o]->type == mesh && Objects[o]->Id == indice){links++;};				
	}

	if (links < 2){	
		for(size_t o=0; o < Objects.size(); o++){
			if (Objects[o]->type == mesh && Objects[o]->Id > indice){
				Objects[o]->Id--;
			};				
		}
		Meshes[indice].LiberarMemoria();
		//Meshes.Remove(indice);

		// borrar el elemento en posición `indice`
		if (indice >= 0 && static_cast<size_t>(indice) < Meshes.size()) {
			Meshes.erase(Meshes.begin() + indice);
		}
	}*/
}