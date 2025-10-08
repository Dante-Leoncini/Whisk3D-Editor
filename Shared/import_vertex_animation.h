/*TInt CWhisk3D::BuscarShapeKeyAnimation(TInt ID, TBool mostrarError){
	for (TInt ska = 0; ska < ShapeKeyAnimations.Count(); ska++){
		if (ShapeKeyAnimations[ska].Id == ID){
			return ska;
			break;
		}
	}
	if (mostrarError){
		_LIT(KExtensionError, "El objeto no tiene animacion");
		HBufC* noteBuf = HBufC::NewLC(180);
		noteBuf->Des().Format(KExtensionError);
		MensajeError(noteBuf);
		CleanupStack::PopAndDestroy(noteBuf);
	}
	return -1;
};*/

int BuscarShapeKeyAnimation(int ID, bool mostrarError) {
    for (size_t ska = 0; ska < ShapeKeyAnimations.size(); ++ska) {
        if (ShapeKeyAnimations[ska].Id == ID) {
            return static_cast<int>(ska);
        }
    }

    if (mostrarError) {
        std::cerr << "Error: El objeto no tiene animacion" << std::endl;
    }

    return -1;
}

bool ImportVertexAnimation(const std::string& filepath){
    // Revisar extensión
    if (filepath.size() < 4 || filepath.substr(filepath.size() - 4) != ".txt") {
        std::cerr << "Error: El archivo seleccionado no tiene la extensión .txt" << std::endl;
        return false;
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error al abrir: " << filepath << std::endl;
        return false;
    }

	//si no hay objetos
	if (Objects.size() < 1){return false;}	
	Object& obj = Objects[SelectActivo];
	if (!obj.seleccionado){return false;}
	//si no es un mesh
	bool esMesh = false;
	
	Mesh meshInstance;    // Crea un objeto v�lido
	Mesh& pMesh = meshInstance; // Asigna la referencia a ese objeto
	if (obj.type == mesh){
		pMesh = Meshes[obj.Id];
		esMesh = true;
	}	

    std::string line;
    int SAIndex = -1;
    int AnimID = 0;
    int FrameIndex = 0;

    keyFrame key{};
    key.Interpolation = 0;
    key.valueX = key.valueY = key.valueZ = 0;
    key.frame = 0;	

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        if (line.rfind("new_animation_ShapeKey", 0) == 0 && esMesh) {
            // Crear nueva animación de ShapeKey
            SAIndex = BuscarShapeKeyAnimation(obj.Id, false);
            if (SAIndex < 0) {
                SAIndex = ShapeKeyAnimations.size();
                ShapeKeyAnimation newAnim{};
                newAnim.Id = obj.Id;
                newAnim.ChangeAnimation = -1;
                newAnim.LastFrame = newAnim.NextFrame = 0;
                newAnim.LastAnimation = newAnim.NextAnimation = 0;
                newAnim.Mix = 0;
                newAnim.Normals = true;
                newAnim.Faces = false;
                newAnim.Interpolacion = false;
                ShapeKeyAnimations.push_back(newAnim);
            }

            AnimID = ShapeKeyAnimations[SAIndex].Animations.size();
            Animation newAnimation{};
            newAnimation.MixSpeed = 1;
            ShapeKeyAnimations[SAIndex].Animations.push_back(newAnimation);

            // Parsear parámetros adicionales
            std::istringstream iss(line.substr(22));
            int param, count = 0;
            while (iss >> param) {
                switch (count) {
                    case 0: ShapeKeyAnimations[SAIndex].Animations[AnimID].MixSpeed = param; break;
                    case 1: if (param == 1) ShapeKeyAnimations[SAIndex].Interpolacion = true; break;
                    case 2: if (param == 0) ShapeKeyAnimations[SAIndex].Normals = false; break;
                    case 3: if (param == 1) ShapeKeyAnimations[SAIndex].Faces = true; break;
                }
                count++;
            }
        }
        else if (line.rfind("ShapeKey", 0) == 0 && esMesh) {
            ShapeKey newShapeKey;
            ShapeKeyAnimations[SAIndex].Animations[AnimID].Frames.push_back(newShapeKey);
            FrameIndex = ShapeKeyAnimations[SAIndex].Animations[AnimID].Frames.size() - 1;
            // Reservar espacio para los vértices
            ShapeKeyAnimations[SAIndex].Animations[AnimID].Frames[FrameIndex].Vertex.reserve(pMesh.vertexSize);
        }
        else if (line.rfind("akf ", 0) == 0 && esMesh) {
            ShapeKeyVertex newVertex{};
            std::istringstream iss(line.substr(4));
            double num;
            for (int i = 0; i < 6; ++i) {
                if (!(iss >> num)) break;
                switch (i) {
                    case 0: newVertex.vertexZ = static_cast<GLshort>(-num*2000); break;
                    case 1: newVertex.vertexY = static_cast<GLshort>(num*2000); break;
                    case 2: newVertex.vertexX = static_cast<GLshort>(num*2000); break;
                    case 3: num = -(((num + 1)/2) * 255.0 - 128.0);
                            newVertex.normalZ = static_cast<GLbyte>(std::clamp(num, -128.0, 127.0)); break;
                    case 4: num = ((num + 1)/2) * 255.0 - 128.0;
                            newVertex.normalY = static_cast<GLbyte>(std::clamp(num, -128.0, 127.0)); break;
                    case 5: num = ((num + 1)/2) * 255.0 - 128.0;
                            newVertex.normalX = static_cast<GLbyte>(std::clamp(num, -128.0, 127.0)); break;
                }
            }
            ShapeKeyAnimations[SAIndex].Animations[AnimID].Frames[FrameIndex].Vertex.push_back(newVertex);
        }
    }

    file.close();
    std::cout << "Animación importada correctamente desde " << filepath << std::endl;
    return true;
}

/* esto es para symbian y por eso lo dejo aca
void ImportAnimation(){
	//si no hay objetos
	if (Objects.size() < 1){return;}	
	Object& obj = Objects[SelectActivo];
	if (!obj.seleccionado){return;}
	//si no es un mesh
	bool esMesh = false;
	
	Mesh meshInstance;    // Crea un objeto v�lido
	Mesh& pMesh = meshInstance; // Asigna la referencia a ese objeto
	if (obj.type == mesh){
		pMesh = Meshes[obj.Id];
		esMesh = true;
	}	

    _LIT(KTitle, "Import Animation (.txt)");
    TFileName file(KNullDesC);
    if (AknCommonDialogs::RunSelectDlgLD(file, R_WHISK3D_SELECT_DIALOG, KTitle)){		
    	RFs fsSession;	
    	User::LeaveIfError(fsSession.Connect());
    	CleanupClosePushL(fsSession);

		// Revisar la extension del archivo
		TPtrC extension = file.Right(4);  // Obtiene las ultimas 4 letras del nombre del archivo
		if (extension.CompareF(_L(".txt")) != 0) {
			_LIT(KExtensionError, "Error: El archivo seleccionado no tiene la extension .txt");
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
		}	

		Cancelar();

		TInt64 startPos = 0;
		bool continuarLeyendo = true; // Variable para controlar la lectura del archivo
		TBuf8<2048> buffer;
		TInt pos = 0;
		TInt fileSize;
		rFile.Size(fileSize);

		bool animacion_keyframe = false;
		TInt SAIndex = 0;
		TInt AnimID = 0;
		TInt FrameIndex = 0;

		TInt animIndex = 0;
		TInt propIndex = 0;
		
		keyFrame key;
		key.Interpolation = 0;					
		key.valueX = 0;
		key.valueY =  0;
		key.valueZ =  0;				
		key.frame = 0;

		while (continuarLeyendo && startPos < fileSize ) {
			// Leer una linea del archivo desde la posicion actual
			err = rFile.Read(startPos, buffer, buffer.MaxLength());
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
					if (line.Left(22) == _L8("new_animation_ShapeKey")) {			
						if (esMesh){
							animacion_keyframe = true;
							SAIndex = BuscarShapeKeyAnimation(obj.Id, false);
							if (SAIndex < 0){					
								SAIndex = ShapeKeyAnimations.Count();			
								ShapeKeyAnimation NewShapeKeyAnimations;
								ShapeKeyAnimations.Append(NewShapeKeyAnimations);
								ShapeKeyAnimations[SAIndex].Id = obj.Id;
								ShapeKeyAnimations[SAIndex].ChangeAnimation = -1;
								ShapeKeyAnimations[SAIndex].LastFrame = 0;
								ShapeKeyAnimations[SAIndex].NextFrame = 0;
								ShapeKeyAnimations[SAIndex].LastAnimation = 0;
								ShapeKeyAnimations[SAIndex].NextAnimation = 0;
								ShapeKeyAnimations[SAIndex].Mix = 0;
								ShapeKeyAnimations[SAIndex].Normals = true;
								ShapeKeyAnimations[SAIndex].Faces = false;
								ShapeKeyAnimations[SAIndex].Interpolacion = false;
							}
							AnimID = ShapeKeyAnimations[SAIndex].Animations.Count();	
							Animation NewAnimation;
							NewAnimation.MixSpeed = 1;
							ShapeKeyAnimations[SAIndex].Animations.Append(NewAnimation);


							TLex8 lex(line.Mid(22));  // Inicializa TLex con la subcadena a partir del tercer caracter
							while (!lex.Eos()) {	
								TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador
								TLex8 testLex(currentString); // Crear un nuevo objeto TLex para la prueba

								TInt number = 0;
								TInt err = testLex.Val(number);
								if (err == KErrNone){
									switch (contador) {
										case 0:
											ShapeKeyAnimations[SAIndex].Animations[AnimID].MixSpeed = number;	
											break;
										case 1:
											if (number == 1){
												ShapeKeyAnimations[SAIndex].Interpolacion = true;
											}
											break;
										case 2:
											if (number == 0){
												ShapeKeyAnimations[SAIndex].Normals = false;
											}
											break;
										case 3:
											if (number == 1){
												ShapeKeyAnimations[SAIndex].Faces = true;
											}
											break;
									}									
								}			
								contador++;					
								lex.SkipSpace();
							}
						}		
						else {		
							TLex8 lex(line.Mid(22));  // Inicializa TLex con la subcadena a partir del tercer caracter
							while (!lex.Eos()) {						
								lex.SkipSpace();
							}
						}	
					}												
					else if (line.Left(8) == _L8("ShapeKey")){	
						if (esMesh){
							ShapeKey NewShapeKey;
							ShapeKeyAnimations[SAIndex].Animations[AnimID].Frames.Append(NewShapeKey);
							FrameIndex = ShapeKeyAnimations[SAIndex].Animations[AnimID].Frames.Count()-1;
							ShapeKeyAnimations[SAIndex].Animations[AnimID].Frames[FrameIndex].Vertex.ReserveL(pMesh.vertexSize);	
							if (ShapeKeyAnimations[SAIndex].Animations[AnimID].Frames.Count() > 1 && ShapeKeyAnimations[SAIndex].NextFrame == 0){
								ShapeKeyAnimations[SAIndex].NextFrame = 1;
							}					
						}			
						TLex8 lex(line.Mid(8));
						while (!lex.Eos()) {								
							lex.SkipSpace();
						}
					}							
					else if (line.Left(4) == _L8("akf ")){	
						TLex8 lex(line.Mid(4));
						// Iterar mientras no se llegue al final del descriptor y se haya alcanzado el limite de 8 strings
						contador = 0;
						ShapeKeyVertex NewShapeKeyVertex;
						while (!lex.Eos() && contador < 6) {							
							TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador
							TLex8 testLex(currentString); // Crear un nuevo objeto TLex para la prueba

							TReal number = 0.0;
							TInt err = testLex.Val(number, '.');
							if (err == KErrNone){
								switch (contador) {
									case 0:
										number = number*2000;
										NewShapeKeyVertex.vertexZ = static_cast<GLshort>(-number);
										break;
									case 1:
										number = number*2000;
										NewShapeKeyVertex.vertexY = static_cast<GLshort>(number);
										break;
									case 2:
										number = number*2000;
										NewShapeKeyVertex.vertexX = static_cast<GLshort>(number);
										break;
									case 3:	
										number = -(((number +1)/2)* 255.0 - 128.0);
										if (number > 127.0){number = 127.0;}
										else if (number < -128.0){number = -128.0;}
										NewShapeKeyVertex.normalZ = static_cast<GLbyte>(number); // Conversion a GLbyte
										break;
									case 4:	
										number = ((number +1)/2)* 255.0 - 128.0;
										if (number > 127.0){number = 127.0;}
										else if (number < -128.0){number = -128.0;}
										NewShapeKeyVertex.normalY = static_cast<GLbyte>(number);	
										break;
									case 5:	
										number = ((number +1)/2)* 255.0 - 128.0;
										if (number > 127.0){number = 127.0;}
										else if (number < -128.0){number = -128.0;}
										NewShapeKeyVertex.normalX = static_cast<GLbyte>(number);	
										break;
								}
							}
							
							contador++;
							lex.SkipSpace();				
						}
						if (esMesh){
							ShapeKeyAnimations[SAIndex].Animations[AnimID].Frames[FrameIndex].Vertex.Append(NewShapeKeyVertex);
						}
					}
					else if (line.Left(9) == _L8("rotacion ")) {						
						animIndex = BuscarAnimacionObj();
						//si no existe la animacion. la crea
						if (animIndex < 0){							
							AnimationObject NewAnim;	
							AnimationObjects.Append(NewAnim);
							animIndex = AnimationObjects.Count()-1;	
						}
						AnimationObject& anim = AnimationObjects[animIndex];
						anim.Id = SelectActivo;
							
						propIndex = BuscarAnimProperty(animIndex, AnimRotation);
						//si no existe el animProp lo crea
						if (propIndex < 0){		
							AnimProperty propNew;
							anim.Propertys.Append(propNew);
							propIndex = anim.Propertys.Count()-1;
						}
						AnimProperty& prop = anim.Propertys[propIndex];
						prop.Property = AnimRotation;
						
						TLex8 lex(line.Mid(9));  // Inicializa TLex con la subcadena a partir del tercer caracter
						// Iterar mientras no se llegue al final del descriptor y se haya alcanzado el limite de 8 strings
						contador = 0;
						while (!lex.Eos() && contador < 2) {							
							TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador
							TLex8 testLex(currentString); // Crear un nuevo objeto TLex para la prueba
							
							TInt number = 0;
							TInt err = testLex.Val(number);
							if (err == KErrNone && number > 0) {	
								prop.keyframes.ReserveL(number); // Reservar espacio para los keyframes
							}
							contador++;
							lex.SkipSpace();
						}
					}
					else if (line.Left(2) == _L8("r ")){	
						AnimationObject& anim = AnimationObjects[animIndex];
						AnimProperty& prop = anim.Propertys[propIndex];					
						TInt indiceKey = prop.keyframes.Count();
						prop.keyframes.Append(key);

						TLex8 lex(line.Mid(2));
						// Iterar mientras no se llegue al final del descriptor y se haya alcanzado el limite de 8 strings
						contador = 0;
						while (!lex.Eos() && contador < 4) {							
							TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador
							TLex8 testLex(currentString); // Crear un nuevo objeto TLex para la prueba
							
							TInt number = 0;
							TInt err = testLex.Val(number);
							if (err == KErrNone){
								switch (contador) {
									case 0:
										prop.keyframes[indiceKey].frame = number;
										break;
									case 1:
										prop.keyframes[indiceKey].valueX = (GLfloat)number;
										break;
									case 2:
										prop.keyframes[indiceKey].valueZ = (GLfloat)number;	
										break;
									case 3:	
										prop.keyframes[indiceKey].valueY = (GLfloat)number;	
										break;
								}
							}
							
							contador++;
							lex.SkipSpace();				
						}
					}
					else if (line.Left(9) == _L8("locacion ")) {
						animIndex = BuscarAnimacionObj();
						//si no existe la animacion. la crea
						if (animIndex < 0){							
							AnimationObject NewAnim;	
							AnimationObjects.Append(NewAnim);
							animIndex = AnimationObjects.Count()-1;	
						}
						AnimationObject& anim = AnimationObjects[animIndex];
						anim.Id = SelectActivo;
							
						propIndex = BuscarAnimProperty(animIndex, AnimPosition);
						//si no existe el animProp lo crea
						if (propIndex < 0){		
							AnimProperty propNew;
							anim.Propertys.Append(propNew);
							propIndex = anim.Propertys.Count()-1;
						}
						AnimProperty& prop = anim.Propertys[propIndex];
						prop.Property = AnimPosition;	
						
						TLex8 lex(line.Mid(9));  // Inicializa TLex con la subcadena a partir del tercer caracter
						// Iterar mientras no se llegue al final del descriptor y se haya alcanzado el limite de 8 strings
						contador = 0;
						while (!lex.Eos() && contador < 2) {							
							TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador
							TLex8 testLex(currentString); // Crear un nuevo objeto TLex para la prueba
							
							TInt number = 0;
							TInt err = testLex.Val(number);
							if (err == KErrNone && number > 0) {	
								prop.keyframes.ReserveL(number); // Reservar espacio para los keyframes
							}
							contador++;
							lex.SkipSpace();
						}
					}
					else if (line.Left(2) == _L8("l ")){	
						AnimationObject& anim = AnimationObjects[animIndex];
						AnimProperty& prop = anim.Propertys[propIndex];					
						TInt indiceKey = prop.keyframes.Count();
						prop.keyframes.Append(key);

						TLex8 lex(line.Mid(2));
						// Iterar mientras no se llegue al final del descriptor y se haya alcanzado el limite de 8 strings
						contador = 0;
						while (!lex.Eos() && contador < 4) {							
							TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador
							TLex8 testLex(currentString); // Crear un nuevo objeto TLex para la prueba
							
							TInt number = 0;
							TInt err = testLex.Val(number);
							if (err == KErrNone){
								switch (contador) {
									case 0:
										prop.keyframes[indiceKey].frame = number;
										break;
									case 1:
										prop.keyframes[indiceKey].valueX = (GLfloat)number;
										break;
									case 2:
										prop.keyframes[indiceKey].valueY = (GLfloat)number;	
										break;
									case 3:
										prop.keyframes[indiceKey].valueZ = (GLfloat)number;		
										break;
								}
							}
							
							contador++;
							lex.SkipSpace();
						}
					}
					else if (line.Left(7) == _L8("escala ")) {
						animIndex = BuscarAnimacionObj();
						//si no existe la animacion. la crea
						if (animIndex < 0){							
							AnimationObject NewAnim;	
							AnimationObjects.Append(NewAnim);
							animIndex = AnimationObjects.Count()-1;	
						}
						AnimationObject& anim = AnimationObjects[animIndex];
						anim.Id = SelectActivo;
							
						propIndex = BuscarAnimProperty(animIndex, AnimScale);
						//si no existe el animProp lo crea
						if (propIndex < 0){		
							AnimProperty propNew;
							anim.Propertys.Append(propNew);
							propIndex = anim.Propertys.Count()-1;
						}
						AnimProperty& prop = anim.Propertys[propIndex];
						prop.Property = AnimScale;	
						
						TLex8 lex(line.Mid(7));  // Inicializa TLex con la subcadena a partir del tercer caracter
						// Iterar mientras no se llegue al final del descriptor y se haya alcanzado el limite de 8 strings
						contador = 0;
						while (!lex.Eos() && contador < 2) {							
							TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador
							TLex8 testLex(currentString); // Crear un nuevo objeto TLex para la prueba
							
							TInt number = 0;
							TInt err = testLex.Val(number);
							if (err == KErrNone && number > 0) {	
								prop.keyframes.ReserveL(number); // Reservar espacio para los keyframes
							}
							contador++;
							lex.SkipSpace();
						}
					}
					else if (line.Left(2) == _L8("s ")){	
						AnimationObject& anim = AnimationObjects[animIndex];
						AnimProperty& prop = anim.Propertys[propIndex];					
						TInt indiceKey = prop.keyframes.Count();
						prop.keyframes.Append(key);

						TLex8 lex(line.Mid(2));
						// Iterar mientras no se llegue al final del descriptor y se haya alcanzado el limite de 8 strings
						contador = 0;
						while (!lex.Eos() && contador < 4) {							
							TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador
							TLex8 testLex(currentString); // Crear un nuevo objeto TLex para la prueba
							
							TInt number = 0;
							TInt err = testLex.Val(number);
							if (err == KErrNone){
								switch (contador) {
									case 0:
										prop.keyframes[indiceKey].frame = number;
										break;
									case 1:
										prop.keyframes[indiceKey].valueX = (GLfloat)number;
										break;
									case 2:
										prop.keyframes[indiceKey].valueY = (GLfloat)number;	
										break;
									case 3:
										prop.keyframes[indiceKey].valueZ = (GLfloat)number;		
										break;
								}
							}
							
							contador++;
							lex.SkipSpace();
						}
					}
				}
				if (continuarLeyendo){
					// Actualizar la posicion de inicio para la proxima lectura
					startPos += pos + 1;

					// Eliminar la parte de la linea ya procesada y el caracter de salto de linea
					buffer.Delete(0, pos + 1);
					buffer.TrimLeft(); // Eliminar espacios en blanco iniciales
				}
			}
		}

		// Cerrar el archivo
		rFile.Close();
		fsSession.Close();

		redibujar = true;
	}
    else {
    	_LIT(KFormatString, "Error al leer el Archivo");
		HBufC* noteBuf = HBufC::NewLC(24);
		noteBuf->Des().Format(KFormatString);
		MensajeError(noteBuf);  
		CleanupStack::PopAndDestroy(noteBuf);
    }
}*/