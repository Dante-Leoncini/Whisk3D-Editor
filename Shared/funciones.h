/*void NewMaterial(bool reemplazar){	
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
		mat.name = HBufC::NewL(15);
		_LIT(KMatNameDefault, "DefaultMaterial");
		mat.name->Des().Format(KMatNameDefault, Materials.Count()+1);
	}
	else {
		mat.name = HBufC::NewL(12);
		_LIT(KMatName, "Material.%03d");
		mat.name->Des().Format(KMatName, Materials.Count()+1);
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
		HBufC* noteBuf = HBufC::NewLC(100);
		_LIT(KFormatString, "Old Material 1 to %d");
		noteBuf->Des().Format(KFormatString, pMesh.materialsGroup.Count());
		int OldMaterialID = 1;
		//if (pMesh.materialsGroup.Count() > 1){
		if (pMesh.materialsGroup.size() > 1){
			//OldMaterialID = DialogNumber(1, 1, pMesh.materialsGroup.Count(), noteBuf);
			//OldMaterialID = DialogNumber(1, 1, pMesh.materialsGroup.size(), noteBuf);
		}
		//pMesh.materialsGroup[OldMaterialID-1].material = Materials.Count()-1;
		pMesh.materialsGroup[OldMaterialID-1].material = Materials.size()-1;
	}
}*/

void EliminarMesh(int indice){
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