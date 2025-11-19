

typedef GLshort Edge[2];

class FaceCorners {
	public:
		int vertex;
		int uv;
		int normal;
		//seria genial agregar color despues
};

class Face { 
	public:
		std::vector<FaceCorners> corner;
};

struct VertexKey {
    int pos, normal, uv, color;
    bool operator==(const VertexKey &other) const {
        return pos == other.pos && normal == other.normal
               && uv == other.uv && color == other.color;
    }
};

namespace std {
    template <>
    struct hash<VertexKey> {
        size_t operator()(const VertexKey &k) const {
            return ((size_t)k.pos * 73856093) ^ ((size_t)k.normal * 19349663)
                ^ ((size_t)k.uv * 83492791) ^ ((size_t)k.color * 49979693);
        }
    };
}

class Wavefront {
	public:
		std::vector<GLfloat> vertex;	
		std::vector<GLubyte> vertexColor;
		std::vector<GLbyte> normals;
		std::vector<GLfloat> uv;
		std::vector<Face> faces;
		int facesSize;
		int facesCount;
		std::vector<MaterialGroup> materialsGroup;

        void Reset(){
         	vertex.clear();
         	vertexColor.clear();
         	normals.clear();
         	uv.clear();
         	faces.clear();
         	materialsGroup.clear();
			facesSize  = 0;
			facesCount = 0;
		};	

		/*void ConvertToES1(Mesh& TempMesh, int* acumuladoVertices, int* acumuladoNormales, int* acumuladoUVs){
			TempMesh.vertexSize = vertex.size();
			TempMesh.vertex = new GLshort[vertex.size()];
			TempMesh.vertexColor = new GLubyte[(vertex.size()/3)*4];
			TempMesh.normals = new GLbyte[vertex.size()];
			TempMesh.uv = new GLfloat[(vertex.size()/3)*2];
			//valores por defecto
			for (size_t i = 0; i < vertex.size()/3; i++) {
				TempMesh.vertex[i*3] = TempMesh.vertex[i*3+1] = TempMesh.vertex[i*3+2] = 0;
				TempMesh.vertexColor[i*4] = TempMesh.vertexColor[i*4+1] = TempMesh.vertexColor[i*4+2] = TempMesh.vertexColor[i*4+3] = 255;
				TempMesh.normals[i*3] = TempMesh.normals[i*3+1] = TempMesh.normals[i*3+2] = 0;
				TempMesh.uv[i*2] = TempMesh.uv[i*2+1] = 0;
			}
			
			//agrega las caras
			TempMesh.faces = new GLushort[facesSize];
			TempMesh.facesSize = facesSize;*/

			//convertir cuads y ngones a triangulos
			/*contador -= 2;
			for (int c = 0; c < contador; ++c) {
				for (int i = 0; i < 3; ++i) {
					//el primer vertice de los triangulos es el primero
					if (i == 0){
						ListCaras.push_back(tempIndices[0]);
						ListCaras.push_back(tempIndices[1]);
						ListCaras.push_back(tempIndices[2]);
					}
					else {									
						ListCaras.push_back(tempIndices[3*c+ i*3  ]);
						ListCaras.push_back(tempIndices[3*c+ i*3+1]);
						ListCaras.push_back(tempIndices[3*c+ i*3+2]);
					}
				}
			}*/

            /*int faceIndex = 0; // posición dentro de TempMesh.faces
			for (size_t i = 0; i < faces.size(); i++) {
                Face& f = faces[i];
                if (f.corner.size() < 3) continue;

                // triangulación tipo fan
                for (size_t t = 1; t < f.corner.size() - 1; t++) {
                    int idx0 = f.corner[0].vertex;
                    int idx1 = f.corner[t].vertex;
                    int idx2 = f.corner[t+1].vertex;

                    // llenar el array TempMesh.faces
                    TempMesh.faces[faceIndex++] = idx0;
                    TempMesh.faces[faceIndex++] = idx1;
                    TempMesh.faces[faceIndex++] = idx2;

                    // copiar atributos de vértices
                    for (int v = 0; v < 3; v++) {
                        TempMesh.vertex[idx0*3+v] = vertex[idx0*3+v];
                        TempMesh.vertex[idx1*3+v] = vertex[idx1*3+v];
                        TempMesh.vertex[idx2*3+v] = vertex[idx2*3+v];

                        TempMesh.normals[idx0*3+v] = normals[f.corner[0].normal*3+v];
                        TempMesh.normals[idx1*3+v] = normals[f.corner[t].normal*3+v];
                        TempMesh.normals[idx2*3+v] = normals[f.corner[t+1].normal*3+v];

                        TempMesh.vertexColor[idx0*4+v] = vertexColor[idx0*3+v];
                        TempMesh.vertexColor[idx1*4+v] = vertexColor[idx1*3+v];
                        TempMesh.vertexColor[idx2*4+v] = vertexColor[idx2*3+v];
                    }

                    for (int u = 0; u < 2; u++) {
                        TempMesh.uv[idx0*2+u] = uv[f.corner[0].uv*2+u];
                        TempMesh.uv[idx1*2+u] = uv[f.corner[t].uv*2+u];
                        TempMesh.uv[idx2*2+u] = uv[f.corner[t+1].uv*2+u];
                    }

                    // actualizar contadores
                    TempMesh.facesSize += 3;
                }
            }

			//si tiene materiales
			if (materialsGroup.size() > 0){			
				for (size_t i = 0; i < materialsGroup.size(); i++) {
					TempMesh.materialsGroup.push_back(materialsGroup[i]);	
				}
			}
			else {
				MaterialGroup tempFaceGroup;
				tempFaceGroup.start = 0; //donde esta el primer triangulo real
				tempFaceGroup.count = facesSize/3; //cuantos triangulos son reales

				tempFaceGroup.startDrawn = 0; //indice del primer triangulo para dibujar
				tempFaceGroup.indicesDrawnCount = facesSize; //cuantos vertices son
				tempFaceGroup.material = 0; //de que material

				TempMesh.materialsGroup.push_back(tempFaceGroup);
			}
			Reset();
		};	*/

        void ConvertToES1(Mesh* TempMesh, int* acumuladoVertices, int* acumuladoNormales, int* acumuladoUVs){
            std::vector<GLfloat> newVertices;
            std::vector<GLubyte> newColors;
            std::vector<GLbyte> newNormals;
            std::vector<GLfloat> newUVs;
            std::vector<GLushort> newFaces;

            std::unordered_map<VertexKey, GLushort> vertexMap;

            for (size_t i = 0; i < faces.size(); i++) {
                Face &f = faces[i];
                if (f.corner.size() < 3) continue;

                for (size_t t = 1; t < f.corner.size() - 1; t++) {
                    FaceCorners corners[3] = {f.corner[0], f.corner[t], f.corner[t+1]};
                    for (int c = 0; c < 3; c++) {
                        VertexKey key = {corners[c].vertex, corners[c].normal, corners[c].uv, corners[c].vertex};

                        auto it = vertexMap.find(key);
                        GLushort idx;
                        if (it != vertexMap.end()) {
                            idx = it->second;
                        } else {
                            idx = newVertices.size() / 3;
                            vertexMap[key] = idx;

                            for (int v = 0; v < 3; v++) {
                                newVertices.push_back(vertex[corners[c].vertex*3+v]);
                                newNormals.push_back(normals[corners[c].normal*3+v]);
                            }
                            for (int v = 0; v < 4; v++) {
                                newColors.push_back(vertexColor[corners[c].vertex*4+v]);
                            }
                            for (int u = 0; u < 2; u++) {
                                newUVs.push_back(uv[corners[c].uv*2+u]);
                            }
                        }
                        newFaces.push_back(idx);
                    }
                }
            }

            // asignar a TempMesh
            TempMesh->vertexSize = newVertices.size();
            TempMesh->vertex = new GLfloat[newVertices.size()];
            std::copy(newVertices.begin(), newVertices.end(), TempMesh->vertex);

            TempMesh->normals = new GLbyte[newNormals.size()];
            std::copy(newNormals.begin(), newNormals.end(), TempMesh->normals);

            TempMesh->vertexColor = new GLubyte[newColors.size()];
            std::copy(newColors.begin(), newColors.end(), TempMesh->vertexColor);

            TempMesh->uv = new GLfloat[newUVs.size()];
            std::copy(newUVs.begin(), newUVs.end(), TempMesh->uv);

            TempMesh->facesSize = newFaces.size();
            TempMesh->faces = new GLushort[newFaces.size()];
            std::copy(newFaces.begin(), newFaces.end(), TempMesh->faces);

			// MaterialGroup
			if (!materialsGroup.empty()) {
				for (const MaterialGroup& mgOrig : materialsGroup) {
					MaterialGroup mg;
					mg.start = mgOrig.start;                 // índice del primer triángulo
					mg.count = mgOrig.count;                 // cantidad de triángulos
					mg.startDrawn = mgOrig.startDrawn;       // índice de vértices en array final
					mg.indicesDrawnCount = mgOrig.indicesDrawnCount;
					mg.material = mgOrig.material;           // índice correcto del material
					TempMesh->materialsGroup.push_back(mg);
				}
			} else {
				// Si no tiene materiales, crear uno por defecto
				MaterialGroup mg;
				mg.start = 0;
				mg.count = faces.size();
				mg.startDrawn = 0;
				mg.indicesDrawnCount = TempMesh->facesSize;
				mg.material = 0;
				TempMesh->materialsGroup.push_back(mg);
			}
            Reset();
        }
};

/*void ImportOBJ(){    
    _LIT(KTitle, "Import Wavefront (.obj)");
    TFileName file(KNullDesC);
    if (AknCommonDialogs::RunSelectDlgLD(file, R_WHISK3D_SELECT_DIALOG, KTitle)){		
    	RFs fsSession;	
    	User::LeaveIfError(fsSession.Connect());
    	CleanupClosePushL(fsSession);

		// Revisar la extension del archivo
		TPtrC extension = file.Right(4);  // Obtiene las últimas 4 letras del nombre del archivo
		if (extension.CompareF(_L(".obj")) != 0) {
			_LIT(KExtensionError, "Error: El archivo seleccionado no tiene la extension .obj");
			HBufC* noteBuf = HBufC::NewLC(180);
			noteBuf->Des().Format(KExtensionError);
			MensajeError(noteBuf);
			CleanupStack::PopAndDestroy(noteBuf);
			fsSession.Close();
			return;
		}

    	RFile rFile;
    	TInt err;	

    	TRAP(err,rFile.Open(fsSession, file, EFileRead));
		if (err != KErrNone){
			_LIT(KFormatString, "Error al abrir: %S");
			HBufC* noteBuf = HBufC::NewLC(file.Length()+16);
			noteBuf->Des().Format(KFormatString, &file);
			MensajeError(noteBuf);
			CleanupStack::PopAndDestroy(noteBuf); 
			rFile.Close();
			fsSession.Close();
			return;
		}*/	
		/*if (file){
			_LIT(KFormatString, "Formato no valido");
			HBufC* noteBuf = HBufC::NewLC(100);
			noteBuf->Des().Format(KFormatString, &file);
			MensajeError(noteBuf); 
			return;
		}			*/
		/*//crea el objeto
		Cancelar();

		TInt64 startPos = 0; // Variable para mantener la posicion de lectura en el archivo
		//esto se hace para no cerrar el archivo y por cada nuevo obj encontrado simplemente volvemos a usar leerOBJ con el archivo donde quedo
		//TBool hayMasObjetos;
		TInt objetosCargados = 0;		
		TInt acumuladoVertices = 0;
		TInt acumuladoNormales = 0;
		TInt acumuladoUVs = 0;
		while (LeerOBJ(&fsSession, &rFile, &file, &startPos, &acumuladoVertices, &acumuladoNormales, &acumuladoUVs)) { // && objetosCargados < 1
			objetosCargados++;
		}

		// Cerrar el archivo
		rFile.Close();
		fsSession.Close();

		TFileName mtlFile = file;
		mtlFile.Replace(file.Length() - 4, 4, _L(".mtl"));

		RFs fs;
	    User::LeaveIfError(fs.Connect()); // Asegurarse de que fs se conecta correctamente
	    CleanupClosePushL(fs);
	    
		TEntry entry;
		err = fs.Entry(mtlFile, entry);

		//si el archivo existe. no tendria que marcar error
		if (err == KErrNone) {
			TRAP(err, LeerMTL(mtlFile, objetosCargados));
			//si ocurrio algun error al leerlo
			if (err != KErrNone) {
				_LIT(KFormatString, "Error al leer el archivo .mtl");
				HBufC* noteBuf = HBufC::NewLC(100);
				noteBuf->Des().Format(KFormatString);
				MensajeError(noteBuf);
				CleanupStack::PopAndDestroy(noteBuf);
			}
		} else {
            // El archivo no existe, manejar el error
            _LIT(KFileNotFound, "El archivo .mtl no existe");
            HBufC* noteBuf = HBufC::NewLC(100);
            noteBuf->Des().Format(KFileNotFound);
            MensajeError(noteBuf);
            CleanupStack::PopAndDestroy(noteBuf);
        }
		fs.Close();
	}	
    else {
    	_LIT(KFormatString, "Error al leer el Archivo");
		HBufC* noteBuf = HBufC::NewLC(24);
		noteBuf->Des().Format(KFormatString);
		MensajeError(noteBuf);  
		CleanupStack::PopAndDestroy(noteBuf);
    }
}*/

/*TBool CWhisk3D::LeerOBJ(RFs* fsSession, RFile* rFile, TFileName* file, TInt64* startPos,
		TInt* acumuladoVertices,
		TInt* acumuladoNormales,
		TInt* acumuladoUVs
	){
	TInt err;
	Object obj;
	TBool NombreEncontrado = false;
	TBool hayMasObjetos = false;
	obj.visible = true;
	obj.posX = obj.posY = obj.posZ = 0;
	obj.rotX = obj.rotY = obj.rotZ = 0;
	obj.scaleX = obj.scaleY = obj.scaleZ = 65000;
	obj.Id = 0;		
	obj.type = mesh;

	Wavefront Wobj;
	Wobj.Reset();
	Face NewFace;
	FaceCorners NewFaceCorners;
	TBool TieneVertexColor = false;
	TInt acumuladoVerticesProximo = 0;
	TInt acumuladoNormalesProximo = 0;
	TInt acumuladoUVsProximo = 0;

	TBool continuarLeyendo = true; // Variable para controlar la lectura del archivo
	TBuf8<2048> buffer;
	TInt pos = 0;
	TInt fileSize;
	rFile->Size(fileSize);

	while (continuarLeyendo && *startPos < fileSize ) {
		// Leer una linea del archivo desde la posicion actual
		err = rFile->Read(*startPos, buffer, buffer.MaxLength());
		if (err != KErrNone) {
			//tarde o temprano va a fallar la lectura y va a parar
			// Manejar error al leer
			_LIT(KFormatString, "Error al leer linea");
			HBufC* noteBuf = HBufC::NewLC(100);
			noteBuf->Des().Format(KFormatString);
			MensajeError(noteBuf);
			continuarLeyendo = false; // Salir del bucle
			break;
		}
		// Procesar la linea hasta que no haya mas caracteres en buffer
		while (continuarLeyendo && (pos = buffer.Locate('\n')) != KErrNotFound || (pos = buffer.Locate('\r')) != KErrNotFound) {
			TPtrC8 line = buffer.Left(pos);
		
			// Contador para almacenar la cantidad de "strings" separados por espacios
			TInt contador = 0;
			if (line.Length() > 0) {
				if (!NombreEncontrado && line.Left(2) == _L8("o ")) {
					//evita el crasheo en caso de que no tenga materiales
					//se le asigna el material por defecto
					//MaterialesNuevos.Append(0);
					//MeshsGroups.Append(0);

					TLex8 lex(line.Mid(2));
					if (!lex.Eos()){
						TPtrC8 currentString = lex.NextToken();							
						obj.name = HBufC::NewL(currentString.Length());
						obj.name->Des().Copy(currentString);
					}*/
					
					/*HBufC* noteBuf3 = HBufC::NewLC(180);
					_LIT(KFormatString3, "Nombre de objeto: %S");
					noteBuf3->Des().Format(KFormatString3, obj.name);
					CDialogs::Alert(noteBuf3);
					CleanupStack::PopAndDestroy(noteBuf3);*/

					/*NombreEncontrado = true;
				}
				//si encuentra otro objeto para
				else if (NombreEncontrado && line.Left(2) == _L8("o ")) {
					continuarLeyendo = false; // Salir del bucle
					hayMasObjetos = true;
				}
				else {
					if (line.Left(2) == _L8("v ")) {
						contador = 0;
						Wobj.vertex.ReserveL(Wobj.vertex.Count()+3);
						Wobj.vertexColor.ReserveL(Wobj.vertexColor.Count()+3);
						acumuladoVerticesProximo++;

						TLex8 lex(line.Mid(2));  // Inicializa TLex con la subcadena a partir del tercer caracter
						// Iterar mientras no se llegue al final del descriptor y se haya alcanzado el limite de 8 strings
						while (!lex.Eos() && contador < 6) {		
							TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador					
							TLex8 testLex(currentString);// Crear un nuevo objeto TLex para la prueba
							
							// Convertir el string en un número TInt
							TReal number = 0.0;
							TInt err = testLex.Val(number, '.');
							if (err == KErrNone){
								if (contador < 3) {
									number = number*2000;								
									GLshort glNumber = static_cast<GLshort>(number);
									Wobj.vertex.Append(glNumber);							
								}
								//que el vertice tenga el color es una mierda. se hara un nuevo formato y se guardara en el face corner...							
								else if (contador < 6){
									number = number*255.0;	
									if (number > 255.0){number = 255.0;}	
									GLshort glNumber = static_cast<GLubyte>(number); // Conversion a GLbyte
									Wobj.vertexColor.Append(glNumber);	
									TieneVertexColor = true;*/
									
									/*HBufC* noteBuf3 = HBufC::NewLC(180);
									_LIT(KFormatString3, "se agrego color: %f, %d");
									noteBuf3->Des().Format(KFormatString3, number, glNumber);
									CDialogs::Alert(noteBuf3);
									CleanupStack::PopAndDestroy(noteBuf3);*/
								/*}
							}

							// Avanzar al siguiente "string" que no sea espacio en blanco
							lex.SkipSpace();

							// Incrementar el contador para llevar la cuenta de los strings procesados
							contador++;
						}
						//en caso de no tener color
						while (contador < 6) {		
							if (contador > 2){
								Wobj.vertexColor.Append(255);	
							}
							contador++;
						}
					}
					else if (line.Left(3) == _L8("vn ")) {
						acumuladoNormalesProximo++;
						contador = 0;
						Wobj.normals.ReserveL(Wobj.normals.Count()+3);
						TLex8 lex(line.Mid(3));  // Inicializa TLex con la subcadena a partir del tercer caracter
						// Iterar mientras no se llegue al final del descriptor y se haya alcanzado el limite de 8 strings
						while (!lex.Eos() && contador < 3) {							
							TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador
							TLex8 testLex(currentString); // Crear un nuevo objeto TLex para la prueba
							
							TReal number = 0.0;
							TInt err = testLex.Val(number, '.');
							if (err == KErrNone && contador < 3) {
								number = ((number +1)/2)* 255.0 - 128.0;
								if (number > 127.0){number = 127.0;}
								else if (number < -128.0){number = -128.0;}
								GLbyte glNumber = static_cast<GLbyte>(number); // Conversion a GLbyte
								Wobj.normals.Append(glNumber);
							}
							lex.SkipSpace();
							contador++;
						}			
					}
					else if (line.Left(3) == _L8("vt ")) {	
						acumuladoUVsProximo++;				
						contador = 0;
						Wobj.uv.ReserveL(Wobj.uv.Count()+2);

						TLex8 lex(line.Mid(3));  // Inicializa TLex con la subcadena a partir del tercer caracter
						// Iterar mientras no se llegue al final del descriptor y se haya alcanzado el limite de 8 strings
						while (!lex.Eos() && contador < 2) {							
							TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador
							TLex8 testLex(currentString); // Crear un nuevo objeto TLex para la prueba
							
							TReal number = 0.0;
							TInt err = testLex.Val(number, '.');
							if (err == KErrNone) {	
								GLfloat glNumber;
								switch (contador) {
									case 0:
										number = number * 255.0 - 128.0;
										glNumber = static_cast<GLfloat>(number); // Conversion a GLbyte
										//newVertexUV.u = glNumber;
										Wobj.uv.Append(glNumber);
										break;
									case 1:
										number = -number+1;
										number = number * 255.0 - 128.0;
										glNumber = static_cast<GLfloat>(number); // Conversion a GLbyte
										//number = number * 255.0 - 128.0;
										//newVertexUV.v = glNumber;
										Wobj.uv.Append(glNumber);
										break;
									default:
										break;
								}		
							}
							contador++;
							lex.SkipSpace();
						}	
					}
					else if (line.Left(2) == _L8("f ")) {
						contador = 0;
						TInt conTBarras = 0;

						TLex8 lex(line.Mid(2));  // Inicializa TLex con la subcadena a partir del tercer caracter

						Wobj.faces.Append(NewFace);
						Face& TempFace = Wobj.faces[Wobj.faces.Count()-1];
						MaterialGroup& UltimoMG = Wobj.materialsGroup[Wobj.materialsGroup.Count()-1];
						
						//HBufC* noteBuf4 = HBufC::NewLC(180);								
						while (!lex.Eos()) {		
							TPtrC8 currentString = lex.NextToken();

							TInt startPos2 = 0; // Posicion inicial
							TInt tokenLength = 0; // Longitud del token
							conTBarras = 0;

							// Recorre la cadena hasta encontrar el final
							while (startPos2 < currentString.Length()) {
								// Busca la posicion de la siguiente barra diagonal
								TInt nextSlashPos = currentString.Mid(startPos2).Locate('/');

								// Si no se encuentra una barra diagonal, asume que es el último token
								if (nextSlashPos == KErrNotFound) {
									tokenLength = currentString.Length() - startPos2;
								} else {
									tokenLength = nextSlashPos; // Longitud del token hasta la barra diagonal
								}

								// Extrae el token utilizando la posicion y longitud calculadas
								TPtrC8 token = currentString.Mid(startPos2, tokenLength);
								TLex8 testLex(token); // Crear un nuevo objeto TLex para la prueba

								// Convertir el string en un número TInt
								TInt number = 0;
								TInt err = testLex.Val(number);
								//v1/vt1/vn1
								if (err == KErrNone) {*/
									/*_LIT(KFormatString5, "contador: %d\nconTBarras: %d\nnumber: %d");
									noteBuf4->Des().Format(KFormatString5, contador, conTBarras, number);
									CDialogs::Alert(noteBuf4);*/
									//resetea el contador de barras si se pasa
									/*if (conTBarras > 2){
										//NewFaceGroup.FaceCorners.Append(TempFaceCorner);
										//NewFaceCorner = NewFaceGroup.FaceCorners[NewFaceGroup.FaceCorners.Count()-1];
										conTBarras = 0;
									};
									switch (conTBarras) {
										case 0:
											NewFaceCorners.vertex = number-1;
											break;
										case 1:
											NewFaceCorners.uv = number-1;
											break;
										case 2:
											//se da por hecho que aca se termino un triangulo
											NewFaceCorners.normal = number-1;
											TempFace.corner.Append(NewFaceCorners);
											break;
										default:
											break;
									}	
									//cada face corner extra es un triangulo
									if (contador > 1 && conTBarras == 2){
										Wobj.facesCount++;
										Wobj.facesSize += 3;	
										UltimoMG.count++;
										UltimoMG.indicesDrawnCount += 3;
									}*/
									/*_LIT(KFormatString6, "facesCount: %d\nfacesSize: %d");
									noteBuf4->Des().Format(KFormatString6, Wobj.facesCount, Wobj.facesSize);
									CDialogs::Alert(noteBuf4);*/
								/*}

								// Actualiza la posicion inicial para el proximo token
								startPos2 += tokenLength + 1; // Suma 1 para omitir la barra diagonal
								conTBarras++;
							}	

							lex.SkipSpace();
							contador++;				
						}*/

						// Crear triangulos de la lista temporal de indices
						//TInt numIndices = tempIndices.Count();

						/*_LIT(KFormatString4, "Num. indices: %d\nCaras: %d");
						noteBuf3->Des().Format(KFormatString4, numIndices, contador);
						CDialogs::Alert(noteBuf3);*/

						//si se resta 2 al contador. el contador es el numero de caras
						//contador -= 2;
						// Esto resuelve tanto triangulos, quads como ngones
						//ListCaras.ReserveL(ListCaras.Count() +3*contador); // Reservar espacio para los elementos
						//el contador es cuantos vertices hay. un triangulo tiene minimo 3. si es un quad o ngone tiene mas
						/*for (TInt c = 0; c < contador; ++c) {
							for (TInt i = 0; i < 3; ++i) {
								//el primer vertice de los triangulos es el primero
								if (i == 0){
									ListCaras.Append(tempIndices[0]);
									ListCaras.Append(tempIndices[1]);
									ListCaras.Append(tempIndices[2]);
								}
								else {									
									ListCaras.Append(tempIndices[3*c+ i*3  ]);
									ListCaras.Append(tempIndices[3*c+ i*3+1]);
									ListCaras.Append(tempIndices[3*c+ i*3+2]);
								}
							}
						}	*/
						/*HBufC* noteBuf3 = HBufC::NewLC(180);
						_LIT(KFormatString5, "tempIndices\ncontador: %d\nCaras: %d\nListCaras: %d");
						noteBuf3->Des().Format(KFormatString5, contador, tempIndices.Count()/3,ListCaras.Count());
						CDialogs::Alert(noteBuf3);
						CleanupStack::PopAndDestroy(noteBuf3);*/

						// Limpiar la lista temporal de indices
						//tempIndices.Close();

						// Actualizar el contador de caras en el último grupo de mallas
						//MeshsGroups[MeshsGroups.Count()-1] += (contador - 2);
						/*if (TempMaterialGroup.Count() < 1){
							MaterialGroup tempFaceGroup;
							tempFaceGroup.count = tempFaceGroup.indicesDrawnCount = tempFaceGroup.material = 0;
							tempFaceGroup.start = facesSize;
							TempMaterialGroup.Append(tempFaceGroup);
						}*/
						//TempMaterialGroup[TempMaterialGroup.Count()-1].count += contador;
						/*NewMesh.facesCount++;
						NewMesh.facesSize += 3;
						TempMaterialGroup[TempMaterialGroup.Count()-1].count ++;
						TempMaterialGroup[TempMaterialGroup.Count()-1].indicesDrawnCount += 3;*/

						//MeshsGroups[MeshsGroups.Count()-1]++;
					/*}
					else if (line.Left(7) == _L8("usemtl ")) {	
						MaterialGroup tempFaceGroup;
						tempFaceGroup.count = 0;
						tempFaceGroup.indicesDrawnCount = 0;
						tempFaceGroup.start = Wobj.facesSize/3;
						tempFaceGroup.startDrawn = Wobj.facesSize;
						tempFaceGroup.material = 0;

						Wobj.materialsGroup.Append(tempFaceGroup);
						tempFaceGroup.count = 0;
						Material mat;	
						mat.specular[0] = mat.specular[1] = mat.specular[2] = mat.specular[3] = 0.3;
						mat.diffuse[0] = mat.diffuse[1] = mat.diffuse[2] = mat.diffuse[3] = 1.0;
						mat.emission[0] = mat.emission[1] = mat.emission[2] = mat.emission[3] = 0.0;
						mat.textura = false;
						mat.vertexColor = TieneVertexColor;
						mat.repeat = true;
						mat.lighting = true;
						mat.culling = true;
						mat.transparent = false;
						mat.interpolacion = lineal;
						mat.textureID = 0;
						HBufC* materialName16 = HBufC::NewLC(180);

						TInt encontrado = -1;	
						TLex8 lex(line.Mid(7));
						if (!lex.Eos()){
							TPtrC8 currentString = lex.NextToken();							
							mat.name = HBufC::NewL(currentString.Length());
							mat.name->Des().Copy(currentString);

							//busca si existe el material
							materialName16 = CnvUtfConverter::ConvertToUnicodeFromUtf8L(currentString);
							for(int f=0; f < Materials.Count(); f++){
								if (Materials[f].name->Compare(*materialName16) == 0 && f != 0){
									encontrado = f;
									break;
								}
							}
						}
						else {	
							mat.name = HBufC::NewL(100);						
							_LIT(KMatName, "Material.%03d");
							mat.name->Des().Format(KMatName, Materials.Count()+1);
						}*/

						/*HBufC* noteBuf3 = HBufC::NewLC(180);
						_LIT(KFormatString3, "Material: %d\nNombre: %S");
						noteBuf3->Des().Format(KFormatString3, Materials.Count(), mat.name);
						CDialogs::Alert(noteBuf3);
						CleanupStack::PopAndDestroy(noteBuf3);*/

						//si se encontro no crea el nuevo material
						/*if (encontrado > -1){*/
							/*HBufC* noteBuf3 = HBufC::NewLC(180);
							_LIT(KFormatString3, "newmtl %S\nencontrado %d\nmateriales num: %d");
							noteBuf3->Des().Format(KFormatString3, materialName16, encontrado, Materials.Count());
							CDialogs::Alert(noteBuf3);*/
							/*Wobj.materialsGroup[Wobj.materialsGroup.Count()-1].material = encontrado;
						}
						else {*/
							/*HBufC* noteBuf3 = HBufC::NewLC(180);
							_LIT(KFormatString3, "no encontrado %S");
							noteBuf3->Des().Format(KFormatString3, materialName16);
							CDialogs::Alert(noteBuf3);*/
							/*Materials.Append(mat);
							Wobj.materialsGroup[Wobj.materialsGroup.Count()-1].material = Materials.Count()-1;
						}
						CleanupStack::PopAndDestroy(materialName16);
					}
				}
			}
			if (continuarLeyendo){
				// Actualizar la posicion de inicio para la proxima lectura
				*startPos += pos + 1;

				// Eliminar la parte de la linea ya procesada y el caracter de salto de linea
				buffer.Delete(0, pos + 1);
				buffer.TrimLeft(); // Eliminar espacios en blanco iniciales
			}
		}
	}
	
    Mesh NewMesh;
	Meshes.Append(NewMesh);
	obj.Id = Meshes.Count()-1;
	Mesh& TempMesh = Meshes[obj.Id];
	Wobj.ConvertToES1(TempMesh, acumuladoVertices, acumuladoNormales, acumuladoUVs);
	*acumuladoVertices += acumuladoVerticesProximo;
	*acumuladoNormales += acumuladoNormalesProximo;
	*acumuladoUVs += acumuladoUVsProximo;
	Objects.Append(obj);
	SelectActivo = Objects.Count()-1;
	Collections.Append(SelectActivo);
	
	DeseleccionarTodo();
	Objects[SelectActivo].seleccionado = true;
	SelectCount = 1;*/

	/*HBufC* noteBuf3 = HBufC::NewLC(180);
	_LIT(KFormatString4, "vertices: %d\nCaras: %d\nMateriales: %d");
	noteBuf3->Des().Format(KFormatString4, Meshes[obj.Id].vertexSize/3, Meshes[obj.Id].facesSize/3, Meshes[obj.Id].materialsGroup.Count());
	CDialogs::Alert(noteBuf3);*/

	/*HBufC* noteBuf = HBufC::NewLC(180);
	_LIT(KFormatString, "se creo la malla 3D");
	noteBuf->Des().Format(KFormatString);
	CDialogs::Alert(noteBuf);
	CleanupStack::PopAndDestroy(noteBuf);*/

	/*return hayMasObjetos;
};*/

// extraer nombre base del filename (sin path ni extensión)
auto ExtractBaseName = [](const std::string& filepath) {
    // quitar ruta
    size_t pos = filepath.find_last_of("/\\");
    std::string name = (pos == std::string::npos) ? filepath : filepath.substr(pos + 1);
    // quitar extensión (la última '.')
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    return name;
};

bool LeerOBJ(std::ifstream& file,
             const std::string& filename,
             std::streampos& startPos,
             int* acumuladoVertices,
             int* acumuladoNormales,
             int* acumuladoUVs) 
{
	Mesh* mesh = new Mesh(CollectionActive, 0, 0, 0);

	// usar filename para nombrar por defecto (y hacerlo único)
	std::string fileBase = ExtractBaseName(filename);
	reinterpret_cast<Text*>(mesh->name->data)->SetValue(SetName(fileBase));

    Wavefront Wobj;
    Wobj.Reset();

    bool NombreEncontrado = false;
    bool hayMasObjetos = false;
	bool TieneVertexColor = false;

    int acumuladoVerticesProximo = 0;
    int acumuladoNormalesProximo = 0;
    int acumuladoUVsProximo = 0;

    file.clear();
    file.seekg(startPos);

    std::string line;
    while (std::getline(file, line)) {
        // Guardar posición
        startPos = file.tellg();

        if (line.rfind("o ", 0) == 0) {
            if (!NombreEncontrado) {
				//algo anda mal por aca
                //mesh->name = line.substr(2);
                NombreEncontrado = true;
            } else {
                hayMasObjetos = true;
                break;
            }
        }
        else if (line.rfind("v ", 0) == 0) {
            std::istringstream ss(line.substr(2));
			double x, y, z, r, g, b, a;

			ss >> x >> y >> z; // siempre están

			// leer hasta 4 valores extra
			if (ss >> r >> g >> b) {
				//std::cout << "Tiene vertex color!" << std::endl;
				TieneVertexColor = true;
				if (ss >> a) {
					// tiene alpha también
				} else {
					a = 1.0; // default alpha
				}
			} else {
				r = g = b = 1.0; // default color
				a = 1.0;
			}

			// vértice
			Wobj.vertex.push_back((GLfloat)(x));
			Wobj.vertex.push_back((GLfloat)(y));
			Wobj.vertex.push_back((GLfloat)(z));

			// color con saturación
			auto saturar = [](double v) { 
				double n = v * 255.0; 
				if (n < 0) n = 0; 
				if (n > 255.0) n = 255.0; 
				return (unsigned char)n; 
			};

			Wobj.vertexColor.push_back(saturar(r));
			Wobj.vertexColor.push_back(saturar(g));
			Wobj.vertexColor.push_back(saturar(b));
			Wobj.vertexColor.push_back(saturar(a));

			acumuladoVerticesProximo++;
        }
        else if (line.rfind("vn ", 0) == 0) {
            std::istringstream ss(line.substr(3));
            double nx, ny, nz;
            ss >> nx >> ny >> nz;
            auto conv = [](double v) -> signed char {
                v = ((v + 1.0) / 2.0) * 255.0 - 128.0;
                if (v > 127) v = 127;
                if (v < -128) v = -128;
                return (signed char)v;
            };
            Wobj.normals.push_back(conv(nx));
            Wobj.normals.push_back(conv(ny));
            Wobj.normals.push_back(conv(nz));
            acumuladoNormalesProximo++;
        }
        else if (line.rfind("vt ", 0) == 0) {
			std::istringstream ss(line.substr(3));
			double u, v;
			ss >> u >> v;

			// Dejar normalizado (sin escalar)
			Wobj.uv.push_back((float)u);
			Wobj.uv.push_back(1.0f - (float)v);

			acumuladoUVsProximo++;
        }
        else if (line.rfind("f ", 0) == 0) {
            std::istringstream ss(line.substr(2));
            std::string token;
            Face newFace;

            // Recorremos cada vértice de la cara
            while (ss >> token) {
                FaceCorners fc;
                size_t pos1 = token.find('/');
                size_t pos2 = token.rfind('/');

                // Vertex index
                fc.vertex = std::stoi(token.substr(0, pos1)) - 1;

                // UV index
                if (pos1 != std::string::npos && pos2 > pos1)
                    fc.uv = std::stoi(token.substr(pos1 + 1, pos2 - pos1 - 1)) - 1;
                else
                    fc.uv = -1;

                // Normal index
                if (pos2 != std::string::npos)
                    fc.normal = std::stoi(token.substr(pos2 + 1)) - 1;
                else
                    fc.normal = -1;

                newFace.corner.push_back(fc);
            }

            Wobj.faces.push_back(newFace);

            // Actualizamos el último material group
            if (!Wobj.materialsGroup.empty()) {
                MaterialGroup& mg = Wobj.materialsGroup.back();

                // Cada nueva cara incrementa el contador de caras y vertices a dibujar
                mg.count++;
                mg.indicesDrawnCount += 3; // asumimos triangulos; si es ngon, se triangula después

                // startDrawn solo se asigna la primera vez
                if (mg.count == 1) {
                    mg.startDrawn = (Wobj.faces.size() - 1) * 3;
                }
            }
        } 
        else if (line.rfind("usemtl ", 0) == 0) {
			std::string matName = line.substr(7); // toma el nombre del material
			Material* materialPuntero = BuscarMaterialPorNombre(matName);

			// si no existe, crea uno nuevo
			if (!materialPuntero) {
				materialPuntero = new Material(matName, false, TieneVertexColor);
			}

			// crea el MaterialGroup usando el índice correcto
			MaterialGroup mg;
			mg.start = Wobj.faces.size();
			mg.startDrawn = Wobj.faces.size() * 3;
			mg.count = 0;
			mg.indicesDrawnCount = 0;
			mg.material = materialPuntero;

			Wobj.materialsGroup.push_back(mg);
        }
    }

    // Convertir a Mesh final
    std::cout << "DEBUG Wobj:\n";
    std::cout << "  vertices = " << Wobj.vertex.size() / 3 << "\n";
    std::cout << "  normals  = " << Wobj.normals.size() / 3 << "\n";
    std::cout << "  uv       = " << Wobj.uv.size() / 2 << "\n";
    std::cout << "  faces    = " << Wobj.faces.size() / 3 << "\n\n";

    Wobj.ConvertToES1(mesh, acumuladoVertices, acumuladoNormales, acumuladoUVs);

    *acumuladoVertices += acumuladoVerticesProximo;
    *acumuladoNormales += acumuladoNormalesProximo;
    *acumuladoUVs += acumuladoUVsProximo;

    // Agregar a Meshes y obtener el índice
    /*Meshes.push_back(TempMesh);
    int meshIndex = (int)Meshes.size() - 1;

    // Asignar el mesh al objeto
    mesh->Id = meshIndex;   // o obj.MeshIndex = meshIndex; según tu estructura

    // Agregar el objeto a la colleccion activa
	AddToCollection(CollectionActive, obj);*/

    return hayMasObjetos;
}

/*void CWhisk3D::LeerMTL(const TFileName& aFile, TInt objetosCargados) {
	RFs fsSession2;	
	User::LeaveIfError(fsSession2.Connect());

	RFile rFile;
	TInt err;
	
	TRAP(err,rFile.Open(fsSession2, aFile, EFileRead));
	if (err != KErrNone){
		_LIT(KFormatString, "Error al abrir: %S");
        HBufC* noteBuf = HBufC::NewLC(aFile.Length() + 16);
        noteBuf->Des().Format(KFormatString, &aFile);
        MensajeError(noteBuf);
        return;
    }

	TBuf8<2048> buffer;
	TInt pos = 0;
	TInt64 startPos = 0; // Variable para mantener la posicion de lectura en el archivo
	TInt fileSize;
	rFile.Size(fileSize);

	//necesario para modificar el material correcto	
	//Object& obj = Objects[SelectActivo];
	//Mesh& pMesh = Meshes[obj.Id];

	HBufC* materialName16 = HBufC::NewLC(180);
	HBufC* noteBuf3 = HBufC::NewLC(180);*/
	
	/*RPointerArray<Object> objs; // Array de punteros a Object
	RPointerArray<Mesh> pMeshs; // Array de punteros a Meshes
	// Llena el array con punteros a los objetos
	for (TInt c = 0; c <= SelectActivo; ++c){
		Object* obj = &Objects[SelectActivo - c];
		Mesh* pMesh = &Meshes[mesh->Id];
		objs.Append(obj);
		pMeshs.Append(pMesh);*/
		
		/*_LIT(KFormatString3, "Pmesh %d\nID obj:: %d");
		noteBuf3->Des().Format(KFormatString3, pMeshs.Count(), mesh->Id);
		CDialogs::Alert(noteBuf3);*/
	//};

	/*Material* mat = NULL; 
	TBool encontrado = false;

	// Cargar la textura desde la ruta absoluta
	TBool HaytexturasQueCargar = false;
	iTextureManager = CTextureManager::NewL(iScreenWidth, iScreenHeight,
											FRUSTUM_TOP, FRUSTUM_BOTTOM, FRUSTUM_RIGHT, FRUSTUM_LEFT, FRUSTUM_NEAR,
											this); 

	while (startPos < fileSize) {
		// Leer una linea del archivo desde la posicion actual
		err = rFile.Read(startPos, buffer, buffer.MaxLength());
		if (err != KErrNone) {
			_LIT(KFormatString, "Error al leer linea");
			HBufC* noteBuf = HBufC::NewLC(100);
			noteBuf->Des().Format(KFormatString);
			MensajeError(noteBuf);
    		rFile.Close();	
    		fsSession2.Close();	
            //CleanupStack::PopAndDestroy(&fsSession);
			break;
		}      

        while ((pos = buffer.Locate('\n')) != KErrNotFound || (pos = buffer.Locate('\r')) != KErrNotFound) {
			//TInt indice = 0;
            TPtrC8 line = buffer.Left(pos);

            if (line.Length() > 0) {
                if (line.Left(7) == _L8("newmtl ")) {
                    TLex8 lex(line.Mid(7));
                    TPtrC8 materialName = lex.NextToken();

					// Convertir materialName de TPtrC8 (8 bits) a HBufC (16 bits)
					//delete materialName16;
					materialName16 = CnvUtfConverter::ConvertToUnicodeFromUtf8L(materialName);*/

					/*_LIT(KFormatString3, "newmtl %S");
					noteBuf3->Des().Format(KFormatString3, materialName16);
					CDialogs::Alert(noteBuf3);*/

					//buscar el material con el mismo nombre
					/*encontrado = false;*/

					/*for(TInt pm=0; pm < pMeshs.Count(); pm++){
					    Mesh* pMesh = pMeshs[pm]; // Obtener el puntero al Mesh actual
						for(TInt f=0; f < pMesh->materialsGroup.Count(); f++){
							//no se puede usar el material 0. ese el que es por defecto y no se toca por mas que se llame igual
							if (Materials[pMesh->materialsGroup[f].material].name->Compare(*materialName16) == 0 && pMesh->materialsGroup[f].material != 0){
								mat = &Materials[pMesh->materialsGroup[f].material];								
								_LIT(KFormatString3, "newmtl %S encontrado\nMaterial: %d");
								noteBuf3->Des().Format(KFormatString3, materialName16, pMesh->materialsGroup[f].material+1);
								CDialogs::Alert(noteBuf3);								
								encontrado = true;
								break;
							}
						}	
					}	*/
					
					/*for (TInt m = 1; m < Materials.Count(); m++) {
					    // Compara el nombre del material con el proporcionado
					    if (Materials[m].name->Compare(*materialName16) == 0) {
					        mat = &Materials[m];
					        encontrado = true;
					        break;
					    }
					}
                } 
				else if (encontrado){
					//specular
					if (line.Left(3) == _L8("Ns ")) {
						TLex8 lex(line.Mid(3));
						TReal nsValue;
						lex.Val(nsValue, '.');
						GLfloat resultado = nsValue/1000.0f;
						mat->specular[0] = resultado;
						mat->specular[1] = resultado;
						mat->specular[2] = resultado;
						mat->specular[3] = resultado;*/
						
						/*_LIT(KFormatString3, "newmtl %S encontrado\nSpecular: %f");
						noteBuf3->Des().Format(KFormatString3, materialName16, resultado);
						CDialogs::Alert(noteBuf3);*/
					/*} 
					//difusso, Aqui manejas el color ambiental Ka (kaR, kaG, kaB)			
					else if (line.Left(3) == _L8("Kd ")) {
						TLex8 lex(line.Mid(3));
						TReal kdR, kdG, kdB;
						lex.Val(kdR, '.');
						lex.SkipSpace();
						lex.Val(kdG, '.');
						lex.SkipSpace();
						lex.Val(kdB, '.');

						mat->diffuse[0] = (GLfloat)kdR;
						mat->diffuse[1] = (GLfloat)kdG;
						mat->diffuse[2] = (GLfloat)kdB;
					} */
					/*else if (line.Left(3) == _L8("Ks ")) {
						TLex8 lex(line.Mid(3));
						TReal ksR, ksG, ksB;
						lex.Val(ksR, '.');
						lex.SkipSpace();
						lex.Val(ksG, '.');
						lex.SkipSpace();
						lex.Val(ksB, '.');
						// Aqui manejas el color especular Ks (ksR, ksG, ksB)
					}*/
					// Aqui manejas el color de emision Ke (keR, keG, keB)
					/*else if (line.Left(3) == _L8("Ke ")) {
						TLex8 lex(line.Mid(3));
						TReal keR, keG, keB;
						lex.Val(keR, '.');
						lex.SkipSpace();
						lex.Val(keG, '.');
						lex.SkipSpace();
						lex.Val(keB, '.');	
						mat->emission[0] = (GLfloat)keR;
						mat->emission[1] = (GLfloat)keG;
						mat->emission[2] = (GLfloat)keB;
					} */
					/*else if (line.Left(3) == _L8("Ni ")) {
						TLex8 lex(line.Mid(3));
						TReal niValue;
						lex.Val(niValue, '.');
						// Aqui manejas el indice de refraccion Ni
					}*/
					//opacidad 
					/*else if (line.Left(2) == _L8("d ")) {
						TLex8 lex(line.Mid(2));
						TReal dValue;
						lex.Val(dValue, '.');					
						mat->diffuse[3] = (GLfloat)dValue;
						//setea la transparencia deacuerdo al alpha
						if (dValue < 1.f){mat->transparent = true;}
						else {mat->transparent = false;}
					} */
					/*else if (line.Left(6) == _L8("illum ")) {
						TLex8 lex(line.Mid(6));
						TInt illumValue;
						lex.Val(illumValue);
						// Aqui manejas el modelo de iluminacion illum
					}*/
					// Aqui manejas la textura difusa map_Kd
					/*else if (line.Left(18) == _L8("BackfaceCullingOff")){
						mat->culling = false;
					}
					else if (line.Left(7) == _L8("NoLight")){
						mat->lighting = false;
					}
					else if (line.Left(6) == _L8("map_d ")){
						mat->transparent = true;
					}
					else if (line.Left(7) == _L8("map_Kd ")) {
						HaytexturasQueCargar = true;
						TLex8 lex(line.Mid(7));
						TPtrC8 texturePath = lex.NextToken();

						// Convertir la ruta relativa a una ruta absoluta
						HBufC* texturePath16 = HBufC::NewLC(180);
						texturePath16 = CnvUtfConverter::ConvertToUnicodeFromUtf8L(texturePath);
						TParse fileParser;
						fileParser.Set(aFile, NULL, NULL);
						TFileName absolutePath = fileParser.DriveAndPath();
						absolutePath.Append(*texturePath16);

						// Comprobar si la textura existe
						RFs fs;
						fs.Connect();
						//User::LeaveIfError(fs.Connect());
						TEntry entry;
						TInt err = fs.Entry(absolutePath, entry);
						if (err == KErrNone) {						
							TTexture newTexture;
							newTexture.iTextureName = *texturePath16;
							Textures.Append(newTexture);
							mat->textura = true;
							mat->textureID = Textures.Count();

							iTextureManager->RequestToLoad(newTexture.iTextureName, fileParser.DriveAndPath(), &Textures[Textures.Count() - 1], false);
						} else {
							// El archivo no existe, manejar el error
							_LIT(KFileNotFound, "No existe la textura '%S'");
							noteBuf3->Des().Format(KFileNotFound, texturePath16);
							MensajeError(noteBuf3);
						}
						fs.Close();
						CleanupStack::PopAndDestroy(texturePath16);
					}				
				}
            }

            startPos += pos + 1;
            buffer.Delete(0, pos + 1);
            buffer.TrimLeft();
        }
    }
	CleanupStack::PopAndDestroy(materialName16);
	CleanupStack::PopAndDestroy(noteBuf3);
    rFile.Close();	
    fsSession2.Close();	

	//si hay texturas. las lee
	if (HaytexturasQueCargar){		
		iTextureManager->DoLoadL();
	}
    //CleanupStack::PopAndDestroy(&fsSession);
}*/

bool LeerMTL(const std::string& filepath, int objetosCargados) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error al abrir: " << filepath << std::endl;
        return false;
    }

    std::string line;
    Material* mat = nullptr;
    bool HaytexturasQueCargar = false;

    while (std::getline(file, line)) {
        if (line.rfind("newmtl ", 0) == 0) {
			std::string matName = line.substr(7); // toma el nombre del material
            // Buscar material existente
            mat = BuscarMaterialPorNombre(matName);

			// si no existe, crea uno nuevo
			if (!mat) {
				std::cout << "LeerMTL: Material no encontrado! " << matName << std::endl;
				mat = new Material(matName);
                Materials.push_back(mat);
            }
        }
        else if (mat) {
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;

            if (prefix == "Ns") {
                float ns; iss >> ns;
                float value = ns / 1000.0f;
                mat->specular[0] = mat->specular[1] = mat->specular[2] = mat->specular[3] = value;
            }
            else if (prefix == "Kd") {
                iss >> mat->diffuse[0] >> mat->diffuse[1] >> mat->diffuse[2];
            }
            else if (prefix == "Ke") {
                iss >> mat->emission[0] >> mat->emission[1] >> mat->emission[2];
            }
            else if (prefix == "d") {
                float d; iss >> d;
                mat->diffuse[3] = d;
                mat->transparent = (d < 1.0f);
            }
            else if (prefix == "map_Kd") {
                std::string texfile;
                iss >> texfile;

                std::filesystem::path absPath = std::filesystem::path(filepath).parent_path() / texfile;

                // Normalizar separadores
                std::string texPath = absPath.string();
                std::replace(texPath.begin(), texPath.end(), '\\', '/');

                Texture newTex;
                newTex.path = texPath;

                GLuint texid;
                if (LoadTexture(texPath.c_str(), texid)) {
                    newTex.iID = texid;
                    Textures.push_back(newTex);

					// Enlazar al material actual
					mat->textura = true;
					mat->textureID = texid;
                    //std::cerr << "textura " << texPath << " ID: " << texid << "\n";
                } 
                else {
                    std::cerr << "Error cargando textura: " << texPath << "\n";
                }
            }
            else if (prefix == "BackfaceCullingOff") {
                mat->culling = false;
            }
            else if (prefix == "NoLight") {
                mat->lighting = false;
            }
            else if (prefix == "CLAMP_TO_EDGE") {
				mat->repeat = false;
            }
            else if (prefix == "map_d") {
                mat->transparent = true;
            }
        }
    }

    if (HaytexturasQueCargar) {
        // TODO: aquí deberías cargar las texturas con tu TextureManager
        std::cout << "Se encontraron texturas para cargar." << std::endl;
    }

    return true;
}

bool ImportOBJ(const std::string& filepath) {
    // Revisar extensión
    if (filepath.size() < 4 || filepath.substr(filepath.size() - 4) != ".obj") {
        std::cerr << "Error: El archivo seleccionado no tiene la extensión .obj" << std::endl;
        return false;
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error al abrir: " << filepath << std::endl;
        return false;
    }

    std::streampos startPos = 0;
    int objetosCargados = 0;
    int acumuladoVertices = 0;
    int acumuladoNormales = 0;
    int acumuladoUVs = 0;

    while (LeerOBJ(file, filepath, startPos, &acumuladoVertices, &acumuladoNormales, &acumuladoUVs)) {
        objetosCargados++;
    }

    file.close();

    // Archivo .mtl asociado
    std::string mtlFile = filepath.substr(0, filepath.size() - 4) + ".mtl";

    if (std::filesystem::exists(mtlFile)) {
        if (!LeerMTL(mtlFile, objetosCargados)) {
            std::cerr << "Error al leer el archivo .mtl" << std::endl;
        }
    } else {
        std::cerr << "El archivo .mtl no existe" << std::endl;
    }

	//DEBUG. para ver que ocurrio con las texturas despues de cargar
	/*for (size_t i = 0; i < Textures.size(); ++i) {
        const Texture& tex = Textures[i];
        std::cout << "[" << i << "] "
                  << "ID: " << tex.iID << " | "
                  << "Path: " << tex.path << std::endl;
    }*/

	//para ver los materiales
    /*std::cout << "=== DEBUG Materials ===\n";
    for (size_t i = 0; i < Materials.size(); i++) {
        const auto &m = Materials[i];
        std::cout << "[" << i << "] " << m.name 
                  << " | textura=" << (m.textura ? "true" : "false")
                  << " | textureID=" << m.textureID
                  << " | diffuse=(" << m.diffuse[0] << ", " << m.diffuse[1] << ", " << m.diffuse[2] << ", " << m.diffuse[3] << ")"
                  << " | specular=(" << m.specular[0] << ", " << m.specular[1] << ", " << m.specular[2] << ", " << m.specular[3] << ")"
                  << "\n";
    }*/

    return true;
}

// Ejemplo simple de MensajeError
void MensajeError(const std::string& msg) {
    std::cerr << msg << std::endl;
}