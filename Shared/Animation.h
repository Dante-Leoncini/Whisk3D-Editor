class ShapeKeyVertex { 
	public:
		int index;
		GLshort vertexX;
		GLshort vertexY;
		GLshort vertexZ;
		GLbyte normalX;
		GLbyte normalY;
		GLbyte normalZ;
};

class ShapeKey { 
	public:
		std::vector<ShapeKeyVertex> Vertex;
};

class Animation { 
	public:
		std::vector<ShapeKey> Frames;
		int MixSpeed;
};

// Variables globales
bool PlayAnimation = true;
int StartFrame = 1;
int EndFrame = 250;
int CurrentFrame = 1;

Uint32 millisecondsPerFrame = 67;  // animación (ej: 15 FPS)
int FrameRate = 60;                // render (ej: 60 FPS)

Uint32 lastAnimTime = SDL_GetTicks();
Uint32 lastRenderTime = SDL_GetTicks();

void CalculateMillisecondsPerFrame(int aFPS) {
    FrameRate = aFPS;
    millisecondsPerFrame = 1000 / aFPS;
}

enum { AnimPosition, AnimRotation, AnimScale };

class keyFrame { 
	public:
		int frame;
		GLfloat valueX;
		GLfloat valueY;
		GLfloat valueZ;
		int Interpolation;
};

// Función de intercambio
void Swap(keyFrame& a, keyFrame& b) {
    keyFrame temp = a;
    a = b;
    b = temp;
}

// Partición para quicksort
//int Partition(RArray<keyFrame>& arr, int low, int high) {
int Partition(std::vector<keyFrame>& arr, int low, int high) {
    int pivot = arr[high].frame;
    int i = low - 1;

    for (int j = low; j < high; j++) {
        if (arr[j].frame < pivot) {
            i++;
            Swap(arr[i], arr[j]);
        }
    }
    Swap(arr[i + 1], arr[high]);
    return i + 1;
}

// Implementación de quicksort
//void QuickSort(RArray<keyFrame>& arr, int low, int high) {
void QuickSort(std::vector<keyFrame>& arr, int low, int high) {
    if (low < high) {
        int pi = Partition(arr, low, high);
        QuickSort(arr, low, pi - 1);
        QuickSort(arr, pi + 1, high);
    }
}

bool compareKeyFrames(const keyFrame& a, const keyFrame& b) {
    return a.frame < b.frame;
}

class AnimProperty { 
	public:
		int Property;
        int firstFrameIndex;
        int lastFrameIndex;
		//RArray<keyFrame> keyframes;
        std::vector<keyFrame> keyframes;

        void SortKeyFrames() {
            QuickSort(keyframes, 0, keyframes.size() - 1);
        }
};

class AnimationObject { 
	public:
		Object* obj; //a que objeto le afecta. ahora es un puntero y no un indice
		int FirstKeyFrame; //id del objeto al que afecta
		int LastKeyFrame; //id del objeto al que afecta
		//RArray<AnimProperty> Propertys;
        std::vector<AnimProperty> Propertys;
        
		void UpdateFirstLastFrame(){
            FirstKeyFrame = 100000000;
            LastKeyFrame = 0;
			for(size_t pr = 0; pr < Propertys.size(); pr++) {	
			    for(size_t kf = 0; kf < Propertys[pr].keyframes.size(); kf++) {	
                    if (Propertys[pr].keyframes[kf].frame > LastKeyFrame){
                        LastKeyFrame = Propertys[pr].keyframes[kf].frame;	
                    }
                    if (Propertys[pr].keyframes[kf].frame < FirstKeyFrame){
                        FirstKeyFrame = Propertys[pr].keyframes[kf].frame;	
                    }
                }	
			}
        }
};

std::vector<AnimationObject> AnimationObjects;

int BuscarAnimacionObj(){
	int index = -1;
    /*for(size_t a = 0; a < AnimationObjects.size(); a++) {
		if (AnimationObjects[a].obj == SelectActivo){
			index = a;
			break;
		}
	}*/
	return index;
}

int BuscarAnimProperty(int indice, int propertySelect){
	int index = -1;
	/*for(size_t pr = 0; pr < AnimationObjects[indice].Propertys.size(); pr++) {		
		if (AnimationObjects[indice].Propertys[pr].Property == propertySelect){	
			index = pr;
			break;
		}
	}*/
	return index;
}

int BuscarShapeKeyAnimation(Object* obj, bool mostrarError) {
    /*for (size_t ska = 0; ska < ShapeKeyAnimations.size(); ++ska) {
        if (ShapeKeyAnimations[ska].obj == obj) {
            return static_cast<int>(ska);
        }
    }

    if (mostrarError) {
        std::cerr << "Error: El objeto no tiene animacion" << std::endl;
    }*/

    return -1;
}

void ReloadAnimation() {		
    // ShapeKeyAnimation
    /*for (size_t ska = 0; ska < ShapeKeyAnimations.size(); ska++) {
        Mesh& pMesh = Meshes[ShapeKeyAnimations[ska].Id];
        ShapeKeyAnimation& animState = ShapeKeyAnimations[ska];

        animState.Mix++;
        if (animState.Mix >= animState.Animations[animState.LastAnimation].MixSpeed) {
            animState.Mix = 0;
            animState.LastAnimation = animState.NextAnimation;
            animState.LastFrame = animState.NextFrame;

            if (animState.ChangeAnimation < 0) {
                animState.NextFrame++;
                if (animState.NextFrame >= (int)animState.Animations[animState.NextAnimation].Frames.size()) {
                    animState.NextFrame = 0;
                }
            } else {
                animState.NextAnimation = animState.ChangeAnimation;
                animState.ChangeAnimation = -1;
                animState.NextFrame = 0;
            }
        }

        Animation& LasAnim = animState.Animations[animState.LastAnimation];
        Animation& NexAnim = animState.Animations[animState.NextAnimation];
        ShapeKey& LastFrame = LasAnim.Frames[animState.LastFrame];
        ShapeKey& NextFrame = NexAnim.Frames[animState.NextFrame];

        // Calcular el porcentaje de mezcla
        GLfloat mixFactor = static_cast<GLfloat>(animState.Mix) / LasAnim.MixSpeed;

        if (!animState.Interpolacion) {
            for (size_t v = 0; v < LastFrame.Vertex.size(); v++) {
                pMesh.vertex[v*3]     = LastFrame.Vertex[v].vertexX;
                pMesh.vertex[v*3 + 1] = LastFrame.Vertex[v].vertexY;
                pMesh.vertex[v*3 + 2] = LastFrame.Vertex[v].vertexZ;
                if (animState.Normals) {
                    pMesh.normals[v*3]     = LastFrame.Vertex[v].normalX;
                    pMesh.normals[v*3 + 1] = LastFrame.Vertex[v].normalY;
                    pMesh.normals[v*3 + 2] = LastFrame.Vertex[v].normalZ;
                }
            }
        } else {
            for (size_t v = 0; v < NextFrame.Vertex.size(); v++) {
                // Interpolación lineal de los vértices
                pMesh.vertex[v*3] = static_cast<GLshort>(
                    LastFrame.Vertex[v].vertexX +
                    mixFactor * (NextFrame.Vertex[v].vertexX - LastFrame.Vertex[v].vertexX)
                );
                pMesh.vertex[v*3 + 1] = static_cast<GLshort>(
                    LastFrame.Vertex[v].vertexY +
                    mixFactor * (NextFrame.Vertex[v].vertexY - LastFrame.Vertex[v].vertexY)
                );
                pMesh.vertex[v*3 + 2] = static_cast<GLshort>(
                    LastFrame.Vertex[v].vertexZ +
                    mixFactor * (NextFrame.Vertex[v].vertexZ - LastFrame.Vertex[v].vertexZ)
                );

                // Interpolación lineal de las normales (si aplica)
                if (animState.Normals) {
                    pMesh.normals[v*3] = static_cast<GLshort>(
                        LastFrame.Vertex[v].normalX +
                        mixFactor * (NextFrame.Vertex[v].normalX - LastFrame.Vertex[v].normalX)
                    );
                    pMesh.normals[v*3 + 1] = static_cast<GLshort>(
                        LastFrame.Vertex[v].normalY +
                        mixFactor * (NextFrame.Vertex[v].normalY - LastFrame.Vertex[v].normalY)
                    );
                    pMesh.normals[v*3 + 2] = static_cast<GLshort>(
                        LastFrame.Vertex[v].normalZ +
                        mixFactor * (NextFrame.Vertex[v].normalZ - LastFrame.Vertex[v].normalZ)
                    );
                }
            }
        }
    }

    // Animación de objetos (posición, rotación, escala, etc.)
    for (size_t a = 0; a < AnimationObjects.size(); a++) {
        for (size_t p = 0; p < AnimationObjects[a].Propertys.size(); p++) {
            AnimProperty& anim = AnimationObjects[a].Propertys[p];
            if (anim.keyframes.size() > 0) {
                GLfloat valueX = 0, valueY = 0, valueZ = 0;
                int firstFrameIndex = 0, lastFrameIndex = 0;

                // Encontrar el primer y último frame relevante
                for (size_t f = 0; f < anim.keyframes.size(); f++) {
                    if (anim.keyframes[f].frame <= CurrentFrame) {
                        firstFrameIndex = (int)f;
                    }
                    if (anim.keyframes[f].frame >= CurrentFrame) {
                        lastFrameIndex = (int)f;
                        break;
                    }
                }

                // Si CurrentFrame está fuera de los límites de los keyframes
                if (CurrentFrame <= anim.keyframes[firstFrameIndex].frame) {
                    valueX = anim.keyframes[firstFrameIndex].valueX;
                    valueY = anim.keyframes[firstFrameIndex].valueY;
                    valueZ = anim.keyframes[firstFrameIndex].valueZ;
                } else if (CurrentFrame >= anim.keyframes[lastFrameIndex].frame) {
                    valueX = anim.keyframes.back().valueX;
                    valueY = anim.keyframes.back().valueY;
                    valueZ = anim.keyframes.back().valueZ;
                } else {
                    // Interpolación entre keyframes
                    int frame1 = anim.keyframes[firstFrameIndex].frame;
                    int frame2 = anim.keyframes[lastFrameIndex].frame;

                    GLfloat valueX1 = anim.keyframes[firstFrameIndex].valueX;
                    GLfloat valueY1 = anim.keyframes[firstFrameIndex].valueY;
                    GLfloat valueZ1 = anim.keyframes[firstFrameIndex].valueZ;

                    GLfloat valueX2 = anim.keyframes[lastFrameIndex].valueX;
                    GLfloat valueY2 = anim.keyframes[lastFrameIndex].valueY;
                    GLfloat valueZ2 = anim.keyframes[lastFrameIndex].valueZ;

                    GLfloat t  = (CurrentFrame - frame1) / (GLfloat)(frame2 - frame1);
                    GLfloat t2 = 0;

                    switch (anim.keyframes[firstFrameIndex].Interpolation) {
                        case Constant:
                            valueX = valueX1;
                            valueY = valueY1;
                            valueZ = valueZ1;
                            break;
                        case Linear:
                            valueX = valueX1 + (valueX2 - valueX1) * t;
                            valueY = valueY1 + (valueY2 - valueY1) * t;
                            valueZ = valueZ1 + (valueZ2 - valueZ1) * t;
                            break;
                        case EaseIn:
                            t2 = t * t;
                            valueX = valueX1 + (valueX2 - valueX1) * t2;
                            valueY = valueY1 + (valueY2 - valueY1) * t2;
                            valueZ = valueZ1 + (valueZ2 - valueZ1) * t2;
                            break;
                        case EaseOut:
                            t2 = 1 - (1 - t) * (1 - t);
                            valueX = valueX1 + (valueX2 - valueX1) * t2;
                            valueY = valueY1 + (valueY2 - valueY1) * t2;
                            valueZ = valueZ1 + (valueZ2 - valueZ1) * t2;
                            break;
                        case EaseInOut:
                            if (t < 0.5f)
                                t2 = 2 * t * t;
                            else
                                t2 = 1 - 2 * (1 - t) * (1 - t);
                            valueX = valueX1 + (valueX2 - valueX1) * t2;
                            valueY = valueY1 + (valueY2 - valueY1) * t2;
                            valueZ = valueZ1 + (valueZ2 - valueZ1) * t2;
                            break;
                        default:
                            valueX = valueX1;
                            valueY = valueY1;
                            valueZ = valueZ1;
                            break;
                    }
                }

                // Asignar valor a la propiedad
                switch (anim.Property) {
                    case AnimPosition:
                        AnimationObjects[a].obj->posX = valueX;
                        AnimationObjects[a].obj->posY = valueY;
                        AnimationObjects[a].obj->posZ = valueZ;
                        break;
                    case AnimRotation:
                        AnimationObjects[a].obj->rotX = valueX;
                        AnimationObjects[a].obj->rotY = valueY;
                        AnimationObjects[a].obj->rotZ = valueZ;
                        break;
                    case AnimScale:
                        AnimationObjects[a].obj->scaleX = valueX;
                        AnimationObjects[a].obj->scaleY = valueY;
                        AnimationObjects[a].obj->scaleZ = valueZ;
                        break;
                    default:
                        break;
                }
            }
        }
    }

    ReloadViewport();*/
}

/*void CWhisk3D::ReloadAnimation(){		
	//ShapeKeyAnimation
    for(TInt ska = 0; ska < ShapeKeyAnimations.Count(); ska++){
		Mesh& pMesh = Meshes[ShapeKeyAnimations[ska].Id];
		ShapeKeyAnimation& animState = ShapeKeyAnimations[ska];
		animState.Mix++;
		if (animState.Mix >= animState.Animations[animState.LastAnimation].MixSpeed){
			animState.Mix = 0;
			animState.LastAnimation = animState.NextAnimation;
			animState.LastFrame = animState.NextFrame;
			if (animState.ChangeAnimation < 0){
				animState.NextFrame++;
				if (animState.NextFrame >= animState.Animations[animState.NextAnimation].Frames.Count()){
					animState.NextFrame = 0;
				}
			}
			else {
				animState.NextAnimation = animState.ChangeAnimation;
				animState.ChangeAnimation = -1;
				animState.NextFrame = 0;
			}
		}
		Animation& LasAnim = animState.Animations[animState.LastAnimation];
		Animation& NexAnim = animState.Animations[animState.NextAnimation];
		ShapeKey& LastFrame = LasAnim.Frames[animState.LastFrame];
		ShapeKey& NextFrame = NexAnim.Frames[animState.NextFrame];

		// Calcular el porcentaje de mezcla
		GLfloat mixFactor = static_cast<GLfloat>(animState.Mix) / LasAnim.MixSpeed;

		if (!animState.Interpolacion){
			for(TInt v = 0; v < LastFrame.Vertex.Count(); v++) {
				pMesh.vertex[v*3] = LastFrame.Vertex[v].vertexX;
				pMesh.vertex[v*3+1] = LastFrame.Vertex[v].vertexY;
				pMesh.vertex[v*3+2] = LastFrame.Vertex[v].vertexZ;
				if (animState.Normals){
					pMesh.normals[v*3] = LastFrame.Vertex[v].normalX;
					pMesh.normals[v*3+1] = LastFrame.Vertex[v].normalY;
					pMesh.normals[v*3+2] = LastFrame.Vertex[v].normalZ;
				}
			}
		}
		else {
			for (TInt v = 0; v < NextFrame.Vertex.Count(); v++) {
				// Interpolación lineal de los vértices
				pMesh.vertex[v * 3] = static_cast<GLshort>(
					LastFrame.Vertex[v].vertexX +
					mixFactor * (NextFrame.Vertex[v].vertexX - LastFrame.Vertex[v].vertexX)
				);
				pMesh.vertex[v * 3 + 1] = static_cast<GLshort>(
					LastFrame.Vertex[v].vertexY +
					mixFactor * (NextFrame.Vertex[v].vertexY - LastFrame.Vertex[v].vertexY)
				);
				pMesh.vertex[v * 3 + 2] = static_cast<GLshort>(
					LastFrame.Vertex[v].vertexZ +
					mixFactor * (NextFrame.Vertex[v].vertexZ - LastFrame.Vertex[v].vertexZ)
				);

				// Interpolación lineal de las normales (si aplica)
				if (animState.Normals) {
					pMesh.normals[v * 3] = static_cast<GLshort>(
						LastFrame.Vertex[v].normalX +
						mixFactor * (NextFrame.Vertex[v].normalX - LastFrame.Vertex[v].normalX)
					);
					pMesh.normals[v * 3 + 1] = static_cast<GLshort>(
						LastFrame.Vertex[v].normalY +
						mixFactor * (NextFrame.Vertex[v].normalY - LastFrame.Vertex[v].normalY)
					);
					pMesh.normals[v * 3 + 2] = static_cast<GLshort>(
						LastFrame.Vertex[v].normalZ +
						mixFactor * (NextFrame.Vertex[v].normalZ - LastFrame.Vertex[v].normalZ)
					);
				}
			}
		}
	}
    for(TInt a = 0; a < AnimationObjects.Count(); a++) {
		for(TInt p = 0; p < AnimationObjects[a].Propertys.Count(); p++){
			AnimProperty& anim = AnimationObjects[a].Propertys[p];
			if (anim.keyframes.Count() > 0){
				GLfloat valueX = 0;
				GLfloat valueY = 0;
				GLfloat valueZ = 0;
				TInt firstFrameIndex = 0;
				TInt lastFrameIndex = 0;
				// Encontrar el primer frame
				for(TInt f = 0; f < anim.keyframes.Count(); f++) {
					if (anim.keyframes[f].frame <= CurrentFrame) {
						firstFrameIndex = f;
					}
					if (anim.keyframes[f].frame >= CurrentFrame) {
						lastFrameIndex = f;
						break;
					}
				}

				// Si el CurrentFrame esta fuera de los limites de los keyframes, usar el valor del primer o último frame
				if (CurrentFrame <= anim.keyframes[firstFrameIndex].frame) {
					valueX = anim.keyframes[firstFrameIndex].valueX;
					valueY = anim.keyframes[firstFrameIndex].valueY;
					valueZ = anim.keyframes[firstFrameIndex].valueZ;
				} 
				else if (CurrentFrame >= anim.keyframes[lastFrameIndex].frame) {
					valueX = anim.keyframes[anim.keyframes.Count() - 1].valueX;
					valueY = anim.keyframes[anim.keyframes.Count() - 1].valueY;
					valueZ = anim.keyframes[anim.keyframes.Count() - 1].valueZ;
				} 
				// Interpolacion entre keyframes
				else {		
					// Interpolacion entre keyframes
					TInt frame1 = anim.keyframes[firstFrameIndex].frame;
					GLfloat valueX1 = anim.keyframes[firstFrameIndex].valueX;
					GLfloat valueY1 = anim.keyframes[firstFrameIndex].valueY;
					GLfloat valueZ1 = anim.keyframes[firstFrameIndex].valueZ;
					TInt frame2 = anim.keyframes[lastFrameIndex].frame;
					GLfloat valueX2 = anim.keyframes[lastFrameIndex].valueX;
					GLfloat valueY2 = anim.keyframes[lastFrameIndex].valueY;
					GLfloat valueZ2 = anim.keyframes[lastFrameIndex].valueZ;

					GLfloat t;
					GLfloat t2;

					switch (anim.keyframes[firstFrameIndex].Interpolation) {
						case Constant:
							valueX = valueX1;
							valueY = valueY1;
							valueZ = valueZ1;
							break;
						case Linear:
							valueX = valueX1 + ((valueX2 - valueX1) * (CurrentFrame - frame1)) / (frame2 - frame1);
							valueY = valueY1 + ((valueY2 - valueY1) * (CurrentFrame - frame1)) / (frame2 - frame1);
							valueZ = valueZ1 + ((valueZ2 - valueZ1) * (CurrentFrame - frame1)) / (frame2 - frame1);
							break;
						case EaseIn:
							t = (CurrentFrame - frame1) / (GLfloat)(frame2 - frame1); // Normalizar el tiempo entre 0 y 1
							t2 = t * t; // EaseIn (usando una interpolacion cuadratica)
							
							valueX = valueX1 + (valueX2 - valueX1) * t2;
							valueY = valueY1 + (valueY2 - valueY1) * t2;
							valueZ = valueZ1 + (valueZ2 - valueZ1) * t2;
							break;
						case EaseOut:
							t = (CurrentFrame - frame1) / (GLfloat)(frame2 - frame1); // Normalizar el tiempo entre 0 y 1
							
							t2 = 1 - (1 - t) * (1 - t); // EaseOut (inversa de EaseIn)

							valueX = valueX1 + (valueX2 - valueX1) * t2;
							valueY = valueY1 + (valueY2 - valueY1) * t2;
							valueZ = valueZ1 + (valueZ2 - valueZ1) * t2;
							break;						
						case EaseInOut:
							t = (CurrentFrame - frame1) / (GLfloat)(frame2 - frame1); // Normalizar el tiempo entre 0 y 1

							// EaseIn (aceleracion)
							if (t < 0.5) {
								t2 = 2 * t * t;  // t^2
							} 
							// EaseOut (desaceleracion)
							else {
								t2 = 1 - 2 * (1 - t) * (1 - t);  // 1 - (1 - t)^2
							}

							valueX = valueX1 + (valueX2 - valueX1) * t2;
							valueY = valueY1 + (valueY2 - valueY1) * t2;
							valueZ = valueZ1 + (valueZ2 - valueZ1) * t2;
							break;
						default:
							valueX = valueX1;
							valueY = valueY1;
							valueZ = valueZ1;
							break;							
					}
				}

				// Asignar el valor calculado a la propiedad correspondiente del objeto
				switch (anim.Property) {
					case AnimPosition:
						SceneCollection->Childrens[AnimationObjects[a].Id].posX = valueX;
						SceneCollection->Childrens[AnimationObjects[a].Id].posY = valueY;
						SceneCollection->Childrens[AnimationObjects[a].Id].posZ = valueZ;
						break;
					case AnimRotation:
						SceneCollection->Childrens[AnimationObjects[a].Id].rotX = valueX;
						SceneCollection->Childrens[AnimationObjects[a].Id].rotY = valueY;
						SceneCollection->Childrens[AnimationObjects[a].Id].rotZ = valueZ;
						break;
					case AnimScale:
						SceneCollection->Childrens[AnimationObjects[a].Id].scaleX = valueX;
						SceneCollection->Childrens[AnimationObjects[a].Id].scaleY = valueY;
						SceneCollection->Childrens[AnimationObjects[a].Id].scaleZ = valueZ;
						break;
					// Otros casos según las propiedades
					default:
						// Manejar cualquier otro caso aqui si es necesario
						break;
				}
			}
		}
    }
	ReloadViewport();
}*/