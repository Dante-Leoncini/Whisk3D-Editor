#include "./Primitivas.h"

enum class MeshType {cube, UVsphere, IcoSphere, plane, vertice, circle};

class VertexGroup { 
	public:
		std::vector<int> indices;
};

class MaterialGroup { 
	public:
        int start = 0; //donde esta el primer triangulo real
        int count = 0; //cuantos triangulos son reales

        int startDrawn = 0; //indice del primer triangulo para dibujar
		int indicesDrawnCount = 0; //cuantos vertices son
		Material* material = nullptr; //de que material
};

class Mesh : public Object { 
	public:
		int vertexSize = 0;
		GLfloat* vertex = nullptr;

		std::vector<VertexGroup> vertexGroups;
		GLubyte* vertexColor = nullptr;
		GLbyte* normals = nullptr;
		GLfloat* uv = nullptr;

		//caras
   		int facesSize = 0;
   		GLushort* faces = nullptr;
		std::vector<MaterialGroup> materialsGroup;
		std::vector<int> Modifiers;

        ObjectType getType() override{
            return ObjectType::mesh;
        }

        Mesh(Object* parent = nullptr, GLfloat px = 0.0f, GLfloat py = 0.0f, GLfloat pz = 0.0f)
            : Object(parent, "Mesh") {
            posX = px;
            posY = py;
            posZ = pz;	
            IconType = static_cast<size_t>(IconType::mesh);
        }

		//libera la memoria
		void LiberarMemoria(){
			delete[] vertex;
			delete[] vertexColor;
			delete[] normals;
			delete[] uv;
			for(size_t i=0; i < vertexGroups.size(); i++){
				vertexGroups[i].indices.clear();
			}
			vertexGroups.clear();

			delete[] faces;
			materialsGroup.clear();
			Modifiers.clear();
		}

		void RenderObject() override {
			glPushMatrix();
			//glScalex(obj.scaleX, obj.scaleZ, obj.scaleY);
			//glScalef(obj.scaleX, obj.scaleZ, obj.scaleY);
			/*glScalef(
				FIXED_TO_FLOAT(scaleX),
				FIXED_TO_FLOAT(scaleZ),
				FIXED_TO_FLOAT(scaleY)
			);*/

			glScalef(scaleX, scaleZ, scaleY);
					
			glColor4f(ListaColores[blanco][0],ListaColores[blanco][1],ListaColores[blanco][2],ListaColores[blanco][3]);
			glDisable(GL_COLOR_MATERIAL);
			glMaterialfv(   GL_FRONT_AND_BACK, GL_AMBIENT,  MaterialPreviewAmbient  );
			//averiguar diferencia entre glMaterialx y glMateriali
			//glMaterialx( GL_FRONT_AND_BACK, GL_SHININESS,   12 << 16     );
			glMateriali( GL_FRONT_AND_BACK, GL_SHININESS,   12     );
			glMaterialfv(   GL_FRONT_AND_BACK, GL_EMISSION, ListaColores[negro] );

			// Set array pointers from mesh.
			glVertexPointer( 3, GL_FLOAT, 0, vertex );
			glColorPointer( 4, GL_UNSIGNED_BYTE, 0, vertexColor );	
			glNormalPointer( GL_BYTE, 0, normals );
			
			glShadeModel( GL_SMOOTH );
			
			//modelo con textura
			/*if (SimularZBuffer){
				glDisable( GL_LIGHTING );
				glDisable(GL_BLEND);
				glDisable( GL_TEXTURE_2D );
				glDrawElements(GL_TRIANGLES, pMesh.facesSize, GL_UNSIGNED_SHORT, pMesh.faces);
			}*/
			
			//material
			if (view == RenderType::MaterialPreview || view == RenderType::Rendered){
				// Calcular coordenadas UV reflejadas
				//calculateReflectionUVs(pMesh);

				glTexCoordPointer( 2, GL_FLOAT, 0, uv );
				
				//for(int f=0; f < pMesh.materialsGroup.Count(); f++){
				for(size_t f=0; f < materialsGroup.size(); f++){
                	//std::cout << "materialsGroup: " << (f+1) << "/" << materialsGroup.size() << std::endl;
                	//std::cout << "valor: " << materialsGroup[f].material << std::endl;		

					//si el material es nullo... usa el que esta por defecto para evitar unc rash		
					Material* mat = MaterialDefecto;
					if (materialsGroup[f].material){
						mat = materialsGroup[f].material;
					}

					glMaterialfv(   GL_FRONT_AND_BACK, GL_DIFFUSE, mat->diffuse ); 
					glMaterialfv(   GL_FRONT_AND_BACK, GL_SPECULAR, mat->specular );

					//vertex color
					if (mat->vertexColor){
						glColor4f(ListaColores[negro][0],ListaColores[negro][1],ListaColores[negro][2],ListaColores[negro][3]);
						glEnableClientState( GL_COLOR_ARRAY );
						glEnable(GL_COLOR_MATERIAL);
					}
					else {
						glColor4f(mat->diffuse[0], mat->diffuse[1], mat->diffuse[2], mat->diffuse[3]);
						glDisable(GL_COLOR_MATERIAL);
						glDisableClientState( GL_COLOR_ARRAY );
					}		

					//si usa culling
					if (mat->culling){glEnable( GL_CULL_FACE );}
					else {glDisable( GL_CULL_FACE );}		 

					//si tiene iluminacion	
					if (mat->lighting){
            			glEnableClientState(GL_NORMAL_ARRAY);
						glEnable( GL_LIGHTING );
					}
					else {
            			glDisableClientState(GL_NORMAL_ARRAY);
						glDisable( GL_LIGHTING );
					}
					
					//transparent
					if (mat->transparent){glEnable(GL_BLEND);}
					else {glDisable(GL_BLEND);}
					
					//si tiene texturas
					if (mat->textura){
						glEnable( GL_TEXTURE_2D );
						glEnableClientState(GL_TEXTURE_COORD_ARRAY);
						glBindTexture(  GL_TEXTURE_2D, mat->textureID ); //selecciona la textura							
					
						//textura pixelada o suave
						if (mat->interpolacion == lineal){
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
						}
						else if (mat->interpolacion == closest){
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
							glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
						}	
						//si la textura se repite
						if (mat->repeat){
							glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
							glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
						}
						else {
							glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
							glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
						}	
					}
					else {
						glDisable( GL_TEXTURE_2D );
						glDisableClientState(GL_TEXTURE_COORD_ARRAY);
					}

					glMaterialfv(   GL_FRONT_AND_BACK, GL_EMISSION, mat->emission );

					glDrawElements( GL_TRIANGLES, materialsGroup[f].indicesDrawnCount, GL_UNSIGNED_SHORT, &faces[materialsGroup[f].startDrawn] );

					//for (TInt mdf = 0; mdf < pMesh.Modifiers.Count(); ++mdf) {	
					/*for (size_t mdf = 0; mdf < pMesh.Modifiers.size(); ++mdf) {
						Modifier& modificador = Modifiers[pMesh.Modifiers[mdf]];
						switch (modificador.type) {
							case mirror:
								if (modificador.OpcionesTBool[0]){	
									glPopMatrix();						
									glPushMatrix();
									//glScalex(-obj.scaleX, obj.scaleZ, obj.scaleY);
									glScalef(
										FIXED_TO_FLOAT(-scaleX),
										FIXED_TO_FLOAT(scaleZ),
										FIXED_TO_FLOAT(scaleY)
									);
									glScalef(-1, 0, 0);
									glDrawElements( GL_TRIANGLES, pMesh.materialsGroup[f].indicesDrawnCount, GL_UNSIGNED_SHORT, &pMesh.faces[pMesh.materialsGroup[f].startDrawn] );
								}
								if (modificador.OpcionesTBool[1]){	
									glPopMatrix();				
									glPushMatrix();
									//glScalex(obj.scaleX, obj.scaleZ, -obj.scaleY);
									glScalef(
										FIXED_TO_FLOAT(scaleX),
										FIXED_TO_FLOAT(scaleZ),
										FIXED_TO_FLOAT(-scaleY)
									);
									glDrawElements( GL_TRIANGLES, pMesh.materialsGroup[f].indicesDrawnCount, GL_UNSIGNED_SHORT, &pMesh.faces[pMesh.materialsGroup[f].startDrawn] );
								}
								if (modificador.OpcionesTBool[2]){	
									glPopMatrix();
									glPushMatrix();
									//glScalex(obj.scaleX, -obj.scaleZ, obj.scaleY);
									glScalef(
										FIXED_TO_FLOAT(scaleX),
										FIXED_TO_FLOAT(-scaleZ),
										FIXED_TO_FLOAT(scaleY)
									);
									glDrawElements( GL_TRIANGLES, pMesh.materialsGroup[f].indicesDrawnCount, GL_UNSIGNED_SHORT, &pMesh.faces[pMesh.materialsGroup[f].startDrawn] );
								}
								break;
						}
					}*/
				}
			}	
			//modelo sin textura
			else if (view == RenderType::Solid){
				glEnable( GL_LIGHTING );
				glEnable( GL_CULL_FACE );
				glDisableClientState( GL_COLOR_ARRAY );	  	
				glDisable( GL_TEXTURE_2D );
				glDisable(GL_BLEND);
				glMaterialfv(   GL_FRONT_AND_BACK, GL_DIFFUSE,  MaterialDefecto->diffuse ); 

				//no hace falta usar el bucle ya que es todo del mismo material.
				//esto es mucho mas rapido
				glDrawElements( GL_TRIANGLES, facesSize, GL_UNSIGNED_SHORT, faces );			

				//glDrawElements( GL_TRIANGLES, pMesh.facesSize*3, GL_UNSIGNED_SHORT, pMesh.faces );	
				//for(int f=0; f < pMesh.materialsGroup.Count(); f++){	
					/*Material& mat = Materials[pMesh.materialsGroup[f].material];
					//si usa culling
					if (mat->culling){glEnable( GL_CULL_FACE );}
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
		/*void RenderMeshAndChildren(Object& obj){
		//void RenderMeshAndChildren(Object& obj, int indice){
			// Guardar la matriz actual
			glPushMatrix();
			
			// Aplicar las transformaciones del objeto
			glTranslatef(obj.posX, obj.posZ, obj.posY);
			glRotatef(obj.rotX, 1, 0, 0); // angulo, X Y Z
			glRotatef(obj.rotZ, 0, 1, 0); // angulo, X Y Z
			glRotatef(obj.rotY, 0, 0, 1); // angulo, X Y Z

			// Si es visible y es un mesh, lo dibuja
			if (obj.visible && obj.type == mesh) {
				RenderMesh(obj);
				//RenderMesh(obj, indice); // Ajusta el segundo parametro si es necesario
			}
			
			// Procesar cada hijo
			for (size_t c = 0; c < obj.Childrens.size(); c++) {
				Object& objChild = *obj.Childrens[c];
				//RenderMeshAndChildren(objChild, indice);
				RenderMeshAndChildren(objChild);
			}

			// Restaurar la matriz previa
			glPopMatrix();
		}*/

		~Mesh() {
            //LiberarMemoria();
			delete name;
		}
};

Object* NewMesh(MeshType type = MeshType::cube, Object* parent = nullptr, bool query = false){
    Mesh* mesh = new Mesh(parent, Cursor3DposX, Cursor3DposY, Cursor3DposZ);

    //SetActive(NewMesh);
	
	MaterialGroup tempFaceGroup;
	tempFaceGroup.startDrawn = 0;
	tempFaceGroup.material = MaterialDefecto;
	
	VertexGroup TempVertexGroups;	
	
	if (type == MeshType::plane){ 
		mesh->vertexSize = 4;
		mesh->vertex = new GLfloat[mesh->vertexSize*3];
		mesh->vertexColor = new GLubyte[mesh->vertexSize*4];
		mesh->normals = new GLbyte[mesh->vertexSize*3];
		mesh->uv = new GLfloat[mesh->vertexSize*2];
		int InputSize = 2;
		//CleanupStack::PopAndDestroy(noteBuf);
		InputSize = InputSize*1000;	

		for (int i = 0; i < mesh->vertexSize*3; i++) {
			mesh->vertex[i] = PlaneVertices[i]*InputSize;
		}
		for (int i = 0; i < mesh->vertexSize; i++) {
			mesh->normals[i*3] = 0;
			mesh->normals[i*3+1] = 127;
			mesh->normals[i*3+2] = 0;
		}
		for (int i = 0; i < mesh->vertexSize*4; i++) {
			mesh->vertexColor[i] = 255;
		}
		for (int i = 0; i < mesh->vertexSize*2; i++) {
			mesh->uv[i] = (GLfloat)PlaneUV[i];
		}

		//mesh->facesCount = tempFaceGroup.count = 2;
		mesh->facesSize = tempFaceGroup.indicesDrawnCount = 6;

		mesh->faces = new GLushort[tempFaceGroup.indicesDrawnCount];
		for (int i = 0; i < tempFaceGroup.indicesDrawnCount; i++) {
			mesh->faces[i] = PlaneTriangles[i];
		}
	}	
	else if (type == MeshType::circle){ 
		//HBufC* noteBuf = HBufC::NewLC(100);
		//noteBuf->Des().Copy(_L("Add Circle vertices"));
		//TInt originalVertexSize  = DialogNumber(8, 3, 512, noteBuf);	
		int originalVertexSize = 8;
		mesh->vertexSize = originalVertexSize + 1;  // Agregar un vértice adicional
		//noteBuf->Des().Copy(_L("Add Circle radius cm"));
		//TInt Radius = DialogNumber(100, 1, 10000, noteBuf);	
		int Radius = 100;	
		//CleanupStack::PopAndDestroy(noteBuf);
		Radius = Radius*20;	

		mesh->vertex = new GLfloat[mesh->vertexSize*3];
		mesh->vertexColor = new GLubyte[mesh->vertexSize*4];
		mesh->normals = new GLbyte[mesh->vertexSize*3];
		mesh->uv = new GLfloat[mesh->vertexSize*2];

		for (int i = 0; i < mesh->vertexSize*3; i++) {
			mesh->vertex[i] = Radius;
		}

		for (int i = 0; i < mesh->vertexSize; i++) {
			GLfloat angle = 2.0f * M_PI * (i % originalVertexSize) / originalVertexSize;  // angulo para los vértices

			mesh->vertex[i*3] = -Radius * cos(angle);
			mesh->vertex[i*3+1] = 0;
			mesh->vertex[i*3+2] = -Radius * sin(angle);
			mesh->normals[i*3] = 0;
			mesh->normals[i*3+1] = 127;
			mesh->normals[i*3+2] = 0;
			mesh->uv[i * 2] = static_cast<GLfloat>(cos(angle) * 127);
			mesh->uv[i * 2 + 1] = static_cast<GLfloat>(sin(angle) * 127);
		}

		for (int i = 0; i < mesh->vertexSize*4; i++) {
			mesh->vertexColor[i] = 255;
		}

		mesh->facesSize = tempFaceGroup.indicesDrawnCount = (mesh->vertexSize - 2) * 3;

		mesh->faces = new GLushort[tempFaceGroup.indicesDrawnCount];
		for (int i = 0; i < originalVertexSize - 1; i++) {
			mesh->faces[i * 3] = 0;          // Primer vértice
			mesh->faces[i * 3 + 1] = i + 2;  // Segundo vértice
			mesh->faces[i * 3 + 2] = i + 1;  // Tercer vértice
		}
	}
	else if (type == MeshType::cube){ 
    	mesh->vertexSize = 24;
		mesh->vertex = new GLfloat[mesh->vertexSize*3];
		mesh->vertexColor = new GLubyte[mesh->vertexSize*4];
		mesh->normals = new GLbyte[mesh->vertexSize*3];
		mesh->uv = new GLfloat[mesh->vertexSize*2];

		for (int i = 0; i < mesh->vertexSize*3; i++) {
			mesh->vertex[i] = CuboVertices[i];
			mesh->normals[i] = CuboNormals[i];
		}
		for (int i = 0; i < mesh->vertexSize*4; i++) {
			mesh->vertexColor[i] = 255;
		}
		for (int i = 0; i < mesh->vertexSize*2; i++) {
			//mesh->uv[i] = (GLfloat)((CuboUV[i]+128)/255)*1280;
			mesh->uv[i] = (GLfloat)CuboUV[i];
		}

		mesh->facesSize = tempFaceGroup.indicesDrawnCount = 36;

		mesh->faces = new GLushort[tempFaceGroup.indicesDrawnCount];
		for (int i = 0; i < tempFaceGroup.indicesDrawnCount; i++) {
			mesh->faces[i] = CuboTriangles[i];
		}
		reinterpret_cast<Text*>(mesh->name->data)->SetValue(SetName("Cube"));
	}	

	//le asignamos la mesh	
	mesh->materialsGroup.push_back(tempFaceGroup);
	return mesh;
};

int DuplicateMesh(int meshIndex) {
	return -1;
    /*if (meshIndex < 0 || meshIndex >= (int)Meshes.size()) {
        std::cerr << "DuplicateMesh: índice inválido\n";
        return -1;
    }

    Mesh& src = Meshes[meshIndex];
    Mesh copy;

    // Copiar tamaños
    copy.vertexSize = src.vertexSize;
    copy.facesSize = src.facesSize;

    // Copiar arrays dinámicos
    if (src.vertex && src.vertexSize > 0) {
        copy.vertex = new GLshort[src.vertexSize * 3];
        std::memcpy(copy.vertex, src.vertex, sizeof(GLshort) * src.vertexSize * 3);
    }

    if (src.vertexColor && src.vertexSize > 0) {
        copy.vertexColor = new GLubyte[src.vertexSize * 3];
        std::memcpy(copy.vertexColor, src.vertexColor, sizeof(GLubyte) * src.vertexSize * 3);
    }

    if (src.normals && src.vertexSize > 0) {
        copy.normals = new GLbyte[src.vertexSize * 3];
        std::memcpy(copy.normals, src.normals, sizeof(GLbyte) * src.vertexSize * 3);
    }

    if (src.uv && src.vertexSize > 0) {
        copy.uv = new GLfloat[src.vertexSize * 2];
        std::memcpy(copy.uv, src.uv, sizeof(GLfloat) * src.vertexSize * 2);
    }

    if (src.faces && src.facesSize > 0) {
        copy.faces = new GLushort[src.facesSize * 3];
        std::memcpy(copy.faces, src.faces, sizeof(GLushort) * src.facesSize * 3);
    }

    // Copiar std::vectors
    copy.vertexGroups = src.vertexGroups;
    copy.materialsGroup = src.materialsGroup;
    copy.Modifiers = src.Modifiers;

    // Insertar en Meshes y devolver índice
    Meshes.push_back(std::move(copy));
    return (int)Meshes.size() - 1;*/
}