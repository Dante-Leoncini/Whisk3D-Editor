void Aceptar(){	
	//si no hay objetos
	if (Objects.size() < 1){return;}

	if ( InteractionMode == ObjectMode ){
		if (estado != editNavegacion){
			estado = editNavegacion;
		}
	}
    ReloadViewport(true);
};

void ClickA(){
    posX+= 1.0;
	redibujar = true;  
}

void ClickD(){
    posX-= 1.0;  
	redibujar = true;  
}

void ClickE(){
    posZ-= 1.0;
	redibujar = true;  
}

void ClickQ(){
    posZ+= 1.0;  
	redibujar = true;  
}

void TeclaArriba(){
	//mueve el mouse
	if (mouseVisible){
		mouseY--;
		if (mouseY < 0){mouseY = 0;};
	}

	if (estado == editNavegacion){	
		if (navegacionMode == Orbit){
			if (ViewFromCameraActive && CameraToView){
				Object& obj = Objects[CameraActive];
				// Convertir el angulo de rotX a radianes
				GLfloat radRotX = obj.rotX * M_PI / 180.0;
				GLfloat radRotY = obj.rotY * M_PI / 180.0;
				//GLfloat radRotZ = obj.rotZ * M_PI / 180.0;

				obj.posX+= 30 * sin(radRotX);
				//obj.posY-= 30 * cos(radRotX);
				obj.posZ+= 30 * cos(radRotY);
			}
			else {
				if (ViewFromCameraActive){
					RestaurarViewport();
				}
				rotY-= 0.5;	
			}			
		}
		else if (navegacionMode == Fly){
			// Convertir el angulo de rotX a radianes
			GLfloat radRotX = rotX * M_PI / 180.0;

			PivotY+= 30 * cos(radRotX);
			PivotX-= 30 * sin(radRotX);
		}		
	}
	else if (estado == EditScale){
		SetScale(1);	
	}
	else if (estado == translacion){
		SetTranslacionObjetos(0, -30);
	}
	ReloadViewport(true);
}

void TeclaAbajo(){
	//mueve el mouse
	/*if (mouseVisible){
		mouseY++;
		if (mouseY > iScreenHeight-17){mouseY = iScreenHeight-17;};
	}*/

	if (estado == editNavegacion){ 			
		if (navegacionMode == Orbit){
			if (ViewFromCameraActive && CameraToView){
				Object& obj = Objects[CameraActive];
				// Convertir el angulo de rotX a radianes
				GLfloat radRotX = obj.rotX * M_PI / 180.0;
				GLfloat radRotY = obj.rotY * M_PI / 180.0;
				//GLfloat radRotZ = obj.rotZ * M_PI / 180.0;

				obj.posX-= 30 * sin(radRotX);
				//obj.posY-= 30 * cos(radRotX);
				obj.posZ-= 30 * cos(radRotY);
			}
			else {
				if (ViewFromCameraActive){
					RestaurarViewport();
				}
				rotY+= 0.5;	
			}		
		}
		else if (navegacionMode == Fly){
			// Convertir el angulo de rotX a radianes
			GLfloat radRotX = rotX * M_PI / 180.0;

			PivotY-= 30 * cos(radRotX);
			PivotX+= 30 * sin(radRotX);
		}
	}
	else if (estado == EditScale){
		SetScale(-1);	
	}
	else if (estado == translacion){
		SetTranslacionObjetos(0, 30);		
	}
	ReloadViewport(true);
}

void TeclaDerecha(){
	//mueve el mouse
	/*if (mouseVisible){
		mouseX++;
		if (mouseX > iScreenWidth-11){mouseX = iScreenWidth-11;};
	}*/

	//rotX -= fixedMul( 1, aDeltaTimeSecs );
	if (estado == editNavegacion){				
		if (navegacionMode == Orbit){
			if (ViewFromCameraActive && CameraToView){
				Object& obj = Objects[CameraActive];
				// Convertir el angulo de rotX a radianes
				GLfloat radRotX = obj.rotX * M_PI / 180.0;

				obj.posX-= 30 * cos(radRotX);
				obj.posY+= 30 * sin(radRotX);
			}
			else {
				if (ViewFromCameraActive){
					RestaurarViewport();
				}
				rotX+= 0.5;	
			}		
		}
		else if (navegacionMode == Fly){
			// Convertir el angulo de rotX a radianes
			GLfloat radRotX = rotX * M_PI / 180.0;

			// Calcular el vector de direccion hacia la izquierda (90 grados a la izquierda del angulo actual)
			GLfloat leftX = cos(radRotX);
			GLfloat leftY = sin(radRotX);

			// Mover hacia la izquierda
			PivotX -= 30 * leftX;
			PivotY -= 30 * leftY;
		}
	}
	else if (estado == translacion){
		SetTranslacionObjetos(30, 0);		
	}
	else if (estado == rotacion){
		SetRotacion(-1, 0);
	}
	else if (estado == EditScale){
		SetScale(1);	
	}
	else if (estado == timelineMove){
		CurrentFrame++;
		if (!PlayAnimation){
			ReloadAnimation();
		}
	}
	ReloadViewport(true);
}

void TeclaIzquierda(){
	//mueve el mouse
	if (mouseVisible){
		mouseX--;
		if (mouseX < 0){mouseX = 0;};
	}

	//rotX += fixedMul( 0.1, aDeltaTimeSecs );
	if (estado == editNavegacion){ 
		if (navegacionMode == Orbit){
			if (ViewFromCameraActive && CameraToView){
				Object& obj = Objects[CameraActive];
				// Convertir el angulo de rotX a radianes
				GLfloat radRotX = obj.rotX * M_PI / 180.0;

				obj.posX+= 30 * cos(radRotX);
				obj.posY-= 30 * sin(radRotX);
			}
			else {
				if (ViewFromCameraActive){
					RestaurarViewport();
				}
				rotX-= 0.5;
			}
		}
		else if (navegacionMode == Fly){
			// Convertir el angulo de rotX a radianes
			GLfloat radRotX = rotX * M_PI / 180.0;

			// Calcular el vector de direccion hacia la izquierda (90 grados a la izquierda del angulo actual)
			GLfloat leftX = cos(radRotX);
			GLfloat leftY = sin(radRotX);

			// Mover hacia la izquierda
			PivotX += 30 * leftX;
			PivotY += 30 * leftY;
		}	
	}
	else if (estado == translacion){	
		SetTranslacionObjetos(-30, 0);		
	}
	else if (estado == rotacion){
		SetRotacion(1, 0);
	}
	else if (estado == EditScale){
		SetScale(-1);
	}
	else if (estado == timelineMove){
		CurrentFrame--;
		if (CurrentFrame < StartFrame){
			StartFrame = EndFrame;
		}
		if (!PlayAnimation){
			ReloadAnimation();
		}
	}
	ReloadViewport(true);
}

int dx = 0;
int dy = 0;
void CheckWarpMouseInWindow(int mx, int my){
    bool warped = false;
	dx = mx - lastMouseX;
	dy = my - lastMouseY;

    if (mx <= 0) {
        mx = winW - 2;
        SDL_WarpMouseInWindow(window, mx, my);
        warped = true;
    }
    else if (mx >= winW - 1) {
        mx = 1;
        SDL_WarpMouseInWindow(window, mx, my);
        warped = true;
    }

    // --- wrap vertical ---
    if (my <= 0) {
        my = winH - 2;
        SDL_WarpMouseInWindow(window, mx, my);
        warped = true;
    }
    else if (my >= winH - 1) {
        my = 1;
        SDL_WarpMouseInWindow(window, mx, my);
        warped = true;
    } 

    // Calcular delta solo si no hubo warp
    if (!warped) {
        dx = mx - lastMouseX;
        dy = my - lastMouseY;
    } else {
        dx = 0;
        dy = 0; // ignorar delta falso
    }
	
	// Guardar última posición
    lastMouseX = mx;
    lastMouseY = my;
	//GuardarMousePos();
}

void InputUsuarioSDL(SDL_Event &e){
	//rueda del mouse	
    if (e.type == SDL_MOUSEWHEEL) {
		posY+= e.wheel.y*20;
		redibujar = true;  
    }
    // Botones del mouse
    else if (e.type == SDL_MOUSEBUTTONDOWN) {
		if (e.button.button == SDL_BUTTON_LEFT) {  
			if (estado == translacion){
				Aceptar();
			}
			else {
            	GuardarMousePos();
				SetCursor3D();
			}
		}
        else if (e.button.button == SDL_BUTTON_MIDDLE) {  // rueda clic
            middleMouseDown = true;
            GuardarMousePos();
        }
		else if (e.button.button == SDL_BUTTON_RIGHT) {  
			if (estado == translacion){
				Cancelar();
			}
		}
    }
    else if (e.type == SDL_MOUSEBUTTONUP) {
        if (e.button.button == SDL_BUTTON_MIDDLE) {
            middleMouseDown = false;
        }
    }
    else if (e.type == SDL_MOUSEMOTION){
		int mx = e.motion.x;
		int my = e.motion.y;
		if (middleMouseDown) {
			CheckWarpMouseInWindow(mx, my);
			// Chequear si Shift está presionado
			bool shiftHeld = (SDL_GetModState() & KMOD_SHIFT);

			if (shiftHeld) {
				float radY = rotY * M_PI / 180.0f; // Yaw
				float radX = rotX * M_PI / 180.0f; // Pitch

				float factor = 8.0f;

				float cosX = cos(radX);
				float sinX = sin(radX);
				float cosY = cos(radY);
				float sinY = sin(radY);

				PivotZ -= dy * factor * cosY;
				PivotX += dx * factor * cosX - dy * factor * sinY * sinX;
				PivotY += dx * factor * sinX + dy * factor * sinY * cosX;
				PodesCambiarElMalditoObjetoSeleccionado = false;
			} 
			else {
				// 🚀 ROTAR cámara
				rotX += dx * 0.2f;  
				rotY += dy * 0.2f;  

				// Limitar rotY para evitar giros extremos
				if(rotY > 180.0f) rotY -= 360.0f;
				if(rotY < -180.0f) rotY += 360.0f;
				if(rotX > 180.0f) rotX -= 360.0f;
				if(rotX < -180.0f) rotX += 360.0f;
			}

			redibujar = true;
		}
		else if (estado == translacion){
			// mover objetos con el mouse
			CheckWarpMouseInWindow(mx, my);
			SetTranslacionObjetos(dx, dy, 16.0f);
		}
    }
    //eventos del teclado
    else if (e.type == SDL_KEYDOWN) {
        if (e.key.repeat == 0) { 
            // Primera vez que se presiona la tecla
            switch (e.key.keysym.sym) {
				case SDLK_LSHIFT:
					PodesCambiarElMalditoObjetoSeleccionado = true;
					break;
                case SDLK_RETURN:  // Enter
                    Aceptar();
                    break;
                case SDLK_RIGHT:   // Flecha derecha
                    TeclaDerecha();
                    break;
                case SDLK_LEFT:    // Flecha izquierda
                    TeclaIzquierda();
                    break;
                case SDLK_UP:  
                    TeclaArriba();
                    break;
                case SDLK_DOWN:  
                    TeclaAbajo();
                    break;	
                case SDLK_a:  
                    ClickA();
                    break;
                case SDLK_d:   
                    ClickD();
                    break;
                case SDLK_x:   
					if (estado != editNavegacion){
						if (axisSelect != X){
							SetEje(X);
						}
						else {
							SetEje(ViewAxis);
						}
					} 
					else {
						Borrar();
					}
                    break;
                case SDLK_y:   
					if (estado != editNavegacion){
						if (axisSelect != Y){
							SetEje(Y);
						}
						else {
							SetEje(ViewAxis);
						}
					}
                    break;
                case SDLK_z:   
					if (estado != editNavegacion){
						if (axisSelect != Z){
							SetEje(Z);
						}
						else {
							SetEje(ViewAxis);
						}
					}
                    break;
                case SDLK_r:    
                    SetRotacion();
                    break;
                case SDLK_g:  
                    SetPosicion();
                    break;			
                case SDLK_s:   
                    SetEscala();
                    break;
                // Numpad
                case SDLK_KP_1: SetViewpoint(front); break;
                //case SDLK_KP_2: numpad('2'); break;
                case SDLK_KP_3: SetViewpoint(right); break;
                /*case SDLK_KP_4: numpad('4'); break;
                case SDLK_KP_5: numpad('5'); break;
                case SDLK_KP_6: numpad('6'); break;*/
                case SDLK_KP_7: SetViewpoint(top); break;
                //case SDLK_KP_8: numpad('8'); break;
                case SDLK_KP_9: abrir(); break;
                //case SDLK_KP_0: numpad('0'); break;
                case SDLK_KP_PERIOD: EnfocarObject(); break;
                // si querés, agregá más teclas aquí
                case SDLK_ESCAPE:  // Esc                    
                    Cancelar();
                    break;
            }
        }else {
            // Evento repetido por mantener apretada
            //std::cout << "apretando tecla" << std::endl;
    		SDL_Keycode key = e.key.keysym.sym;
            switch (key) {
                case SDLK_RETURN:  // Enter
					ShiftCount++;
					std::cout << "ShiftCount: " << ShiftCount << std::endl;
                    Aceptar();
                    break;
                case SDLK_RIGHT:   // Flecha derecha
                    TeclaDerecha();
                    break;
                case SDLK_LEFT:    // Flecha izquierda
                    TeclaIzquierda();
                    break;
                case SDLK_UP:   // Flecha derecha
                    TeclaArriba();
                    break;
                case SDLK_DOWN:    // Flecha izquierda
                    TeclaAbajo();
                    break;
                case SDLK_a:   // Flecha derecha
                    ClickA();
                    break;
                case SDLK_d:    // Flecha izquierda
                    ClickD();
                    break;
                case SDLK_e:   // Flecha derecha
                    ClickE();
                    break;
                case SDLK_q:    // Flecha izquierda
                    ClickQ();
                    break;
                // Numpad
                case SDLK_KP_1: SetViewpoint(front); break;
                //case SDLK_KP_2: numpad('2'); break;
                case SDLK_KP_3: SetViewpoint(right); break;
                /*case SDLK_KP_4: numpad('4'); break;
                case SDLK_KP_5: numpad('5'); break;
                case SDLK_KP_6: numpad('6'); break;*/
                case SDLK_KP_7: SetViewpoint(top); break;
                //case SDLK_KP_8: numpad('8'); break;
                case SDLK_KP_9: abrir(); break;
                //case SDLK_KP_0: numpad('0'); break;
                case SDLK_KP_PERIOD: EnfocarObject(); break;
                // si querés, agregá más teclas aquí
                case SDLK_ESCAPE:  // Esc
                    Cancelar();
                    break;
            }
        }
    } 	
	else if (e.type == SDL_KEYUP) {
		switch (e.key.keysym.sym) {
			case SDLK_LSHIFT:
				if (PodesCambiarElMalditoObjetoSeleccionado){
					changeSelect();
				}
				break;
		}
	}
}

void InputUsuarioSymbian(GLfixed aDeltaTimeSecs){
	/*if (iInputHandler->IsInputPressed( EEscape )){
		Cancelar();
		return;
	}
	//revisa las 4 direcciones
	for(TInt f=0; f < 4; f++){
		if (iInputHandler->IsInputPressed( flechasEstados[f].cual )){
			flechasEstados[f].activo = true;
			if ( flechasEstados[f].estado == TeclaSuelta || flechasEstados[f].estado == TeclaSoltada){
				flechasEstados[f].estado = TeclaPresionada;
			}
			else {
				flechasEstados[f].estado = TeclaMantenida;
			}
		}
		else if ( flechasEstados[f].estado == TeclaMantenida || flechasEstados[f].estado == TeclaPresionada){
			flechasEstados[f].activo = false;
			flechasEstados[f].estado = TeclaSoltada;
		}
		else  {
			flechasEstados[f].activo = false;
			flechasEstados[f].estado = TeclaSuelta;
		}
	}

	//revisa si se apreto una flecha y actualiza los valores dependiendo el estado de la aplicacion
	//ya sea animando, editando una malla 3d o en modo objeto
	if (TocandoPantalla){
		TimeTouch++;
		if (TimeTouch < 10){			
			if (SoltoPantalla){
				TimeTouch = 0;
				TocandoPantalla = false;
				SoltoPantalla = false;
				if (DragTouchX < 120 && DragTouchY < 120){
					ShowMenu();
				}
				else {
				ShowObjectMenu();
				}
				return;
			}
		}
		else if (TimeTouch > 40 && SoltoPantalla){
			TimeTouch = 0;
			TocandoPantalla = false;
			SoltoPantalla = false;
			TInt MovimientoX = StartTouchX - DragTouchX + 10;
			TInt MovimientoY = StartTouchY - DragTouchY + 10;
			if (MovimientoX > -1 && MovimientoX < 21 && MovimientoY > -1 && MovimientoY < 21){
				ShowObjectMenu();
				return;
			}
		}
		else if (iShiftPressed || navegacionMode == Fly){	
			GLfloat radRotX = rotX * PI / 180.0; // Rotación en radianes (X)
			GLfloat radRotY = rotY * PI / 180.0; // Rotación en radianes (Y)

			// Direcciones en el espacio global basadas en la rotación*/
			/*GLfloat forwardX = cos(radRotY) * cos(radRotX); // Dirección hacia adelante/atrás en X
			GLfloat forwardY = sin(radRotX);               // Dirección hacia adelante/atrás en Y
			GLfloat forwardZ = sin(radRotY) * cos(radRotX); // Dirección hacia adelante/atrás en Z

			// Cálculo del vector "izquierda" basado en la rotación
			GLfloat rightX = cos(radRotY + PI / 2.0);      // Dirección hacia la derecha en X
			GLfloat rightZ = sin(radRotY + PI / 2.0);      // Dirección hacia la derecha en Z

			GLfloat upX = -cos(radRotY) * sin(radRotX);    // Dirección hacia arriba/abajo en X
			GLfloat upY = cos(radRotX);                    // Dirección hacia arriba/abajo en Y
			GLfloat upZ = -sin(radRotY) * sin(radRotX);    // Dirección hacia arriba/abajo en Z*/

			// Ajuste de sensibilidad*/
			/*GLfloat scaleFactor = cameraDistance * 0.3f;

			// Desplazamiento del toque
			GLfloat deltaX = (StartTouchX - DragTouchX) * scaleFactor; // Movimiento horizontal del táctil
			GLfloat deltaY = (StartTouchY - DragTouchY) * scaleFactor; // Movimiento vertical del táctil

			// Movimiento restringido a un solo eje (por ejemplo, eje Y del mundo)
			//GLfloat moveX = deltaX * cos(radRotX);
			//GLfloat moveY = deltaY;
			GLfloat moveZ = deltaY * sin(radRotY + PI / 2.0);

			// Aplica el movimiento al pivote de la cámara
			//PivotX = OriginalPivotX + moveX;
			//PivotY = OriginalPivotY + moveY;
			PivotZ = OriginalPivotZ + moveZ;*/

			/*GLfloat radRotX = rotX * PI / 180.0;
			OriginalLeftX = cos(radRotX);
			OriginalLeftY = sin(radRotX);

			PivotX = OriginalPivotX - ((StartTouchX - DragTouchX) * OriginalLeftX)*10;
			PivotY = OriginalPivotY - ((StartTouchY - DragTouchY) * OriginalLeftY)*10;
			PivotZ = ?????????;*/
		/*}
		else if (navegacionMode == Orbit){
			rotX = OriginalRotX - ((StartTouchX - DragTouchX)/2);
			rotY = OriginalRotY - ((StartTouchY - DragTouchY)/2);
		}
		if (SoltoPantalla){
			TimeTouch = 0;
			TocandoPantalla = false;
			SoltoPantalla = false;
		}
		return;
	}	

	if ( iShiftPressed && estado == editNavegacion){
		ShiftCount++;
		if( flechasEstados[FlechaIzquierda].estado == TeclaPresionada ){
			SeleccionarAnterior();
			ShiftCount = 40;
		}		
		else if( flechasEstados[FlechaDerecha].estado == TeclaPresionada ){
			SeleccionarProximo();
			ShiftCount = 40;
		}
		else if( flechasEstados[FlechaArriba].estado == TeclaPresionada ){
			SeleccionarTodo();
			ShiftCount = 40;
		}
		else if( flechasEstados[FlechaAbajo].estado == TeclaPresionada ){
			SeleccionarTodo();
			ShiftCount = 40;
		}*/
		/*else if (iInputHandler->IsInputPressed( EVolumenUp ) ){
			CurrentFrame++;
			if (CurrentFrame > EndFrame){
				CurrentFrame = StartFrame;
			}
			if (!PlayAnimation){
				ReloadAnimation();
			}
	    	ReloadViewport();
			ShiftCount = 40;
		}
		else if( iInputHandler->IsInputPressed( EVolumenDown ) ){
			CurrentFrame--;
			if (CurrentFrame < StartFrame){
				StartFrame = EndFrame;
			}
			if (!PlayAnimation){
				ReloadAnimation();
			}
	    	ReloadViewport();
			ShiftCount = 40;
		}*/
		/*return;
	}
	else if ( iShiftPressed && estado != editNavegacion){
		ShiftCount++;
		return;
	}
	else if (ShiftCount > 0){
		if (ShiftCount < 30){
			Tab();
		}
		ShiftCount = 0;
		return;
	}

	if( flechasEstados[FlechaIzquierda].activo ){
		//mueve el mouse
		if (mouseVisible){
			mouseX--;
			if (mouseX < 0){mouseX = 0;};
		}

		//rotX += fixedMul( 0.1, aDeltaTimeSecs );
		if (estado == editNavegacion){ 
			if (navegacionMode == Orbit){
				if (ViewFromCameraActive && CameraToView){
					Object& obj = Objects[CameraActive];
					// Convertir el angulo de rotX a radianes
					GLfloat radRotX = obj.rotX * PI / 180.0;

					obj.posX+= 30 * cos(radRotX);
					obj.posY-= 30 * sin(radRotX);
				}
				else {
					if (ViewFromCameraActive){
						RestaurarViewport();
					}
					rotX-= 0.5;
				}
			}
			else if (navegacionMode == Fly){
				// Convertir el angulo de rotX a radianes
				GLfloat radRotX = rotX * PI / 180.0;

				// Calcular el vector de direccion hacia la izquierda (90 grados a la izquierda del angulo actual)
				GLfloat leftX = cos(radRotX);
				GLfloat leftY = sin(radRotX);

				// Mover hacia la izquierda
				PivotX += 30 * leftX;
				PivotY += 30 * leftY;
			}	
		}
		else if (estado == translacion){	
			SetTranslacionObjetos(30, 0);		
		}
		else if (estado == rotacion){
			SetRotacion(1);
		}
		else if (estado == EditScale){
			SetScale(-1);
		}
		else if (estado == timelineMove){
			CurrentFrame--;
			if (CurrentFrame < StartFrame){
				StartFrame = EndFrame;
			}
			if (!PlayAnimation){
				ReloadAnimation();
			}
		}
		ReloadViewport(true);
	}
	if( iInputHandler->IsInputPressed( EJoystickRight ) ){
		//mueve el mouse
		if (mouseVisible){
			mouseX++;
			if (mouseX > iScreenWidth-11){mouseX = iScreenWidth-11;};
		}

		//rotX -= fixedMul( 1, aDeltaTimeSecs );
		if (estado == editNavegacion){				
			if (navegacionMode == Orbit){
				if (ViewFromCameraActive && CameraToView){
					Object& obj = Objects[CameraActive];
					// Convertir el angulo de rotX a radianes
					GLfloat radRotX = obj.rotX * PI / 180.0;

					obj.posX-= 30 * cos(radRotX);
					obj.posY+= 30 * sin(radRotX);
				}
				else {
					if (ViewFromCameraActive){
						RestaurarViewport();
					}
					rotX+= 0.5;	
				}		
			}
			else if (navegacionMode == Fly){
				// Convertir el angulo de rotX a radianes
				GLfloat radRotX = rotX * PI / 180.0;

				// Calcular el vector de direccion hacia la izquierda (90 grados a la izquierda del angulo actual)
				GLfloat leftX = cos(radRotX);
				GLfloat leftY = sin(radRotX);

				// Mover hacia la izquierda
				PivotX -= 30 * leftX;
				PivotY -= 30 * leftY;
			}
		}
		else if (estado == translacion){
			SetTranslacionObjetos(-30, 0);		
		}
		else if (estado == rotacion){
			SetRotacion(-1);
		}
		else if (estado == EditScale){
			SetScale(1);	
		}
		else if (estado == timelineMove){
			CurrentFrame++;
			if (!PlayAnimation){
				ReloadAnimation();
			}
		}
	    ReloadViewport(true);
	}
	if( iInputHandler->IsInputPressed( EJoystickUp ) ){
		//mueve el mouse
		if (mouseVisible){
			mouseY--;
			if (mouseY < 0){mouseY = 0;};
		}

		if (estado == editNavegacion){	
			if (navegacionMode == Orbit){
				if (ViewFromCameraActive && CameraToView){
					Object& obj = Objects[CameraActive];
					// Convertir el angulo de rotX a radianes
					GLfloat radRotX = obj.rotX * PI / 180.0;
					GLfloat radRotY = obj.rotY * PI / 180.0;
					//GLfloat radRotZ = obj.rotZ * PI / 180.0;

					obj.posX+= 30 * sin(radRotX);
					//obj.posY-= 30 * cos(radRotX);
					obj.posZ+= 30 * cos(radRotY);
				}
				else {
					if (ViewFromCameraActive){
						RestaurarViewport();
					}
					rotY-= 0.5;	
				}			
			}
			else if (navegacionMode == Fly){
				// Convertir el angulo de rotX a radianes
				GLfloat radRotX = rotX * PI / 180.0;

				PivotY+= 30 * cos(radRotX);
				PivotX-= 30 * sin(radRotX);
			}		
		}
		else if (estado == EditScale){
			SetScale(1);	
		}
		else if (estado == translacion){
			SetTranslacionObjetos(-30, 0);
		}
	    ReloadViewport(true);
	}
	if( iInputHandler->IsInputPressed( EJoystickDown ) ){
		//mueve el mouse
		if (mouseVisible){
			mouseY++;
			if (mouseY > iScreenHeight-17){mouseY = iScreenHeight-17;};
		}

		if (estado == editNavegacion){ 			
			if (navegacionMode == Orbit){
				if (ViewFromCameraActive && CameraToView){
					Object& obj = Objects[CameraActive];
					// Convertir el angulo de rotX a radianes
					GLfloat radRotX = obj.rotX * PI / 180.0;
					GLfloat radRotY = obj.rotY * PI / 180.0;
					//GLfloat radRotZ = obj.rotZ * PI / 180.0;

					obj.posX-= 30 * sin(radRotX);
					//obj.posY-= 30 * cos(radRotX);
					obj.posZ-= 30 * cos(radRotY);
				}
				else {
					if (ViewFromCameraActive){
						RestaurarViewport();
					}
					rotY+= 0.5;	
				}		
			}
			else if (navegacionMode == Fly){
				// Convertir el angulo de rotX a radianes
				GLfloat radRotX = rotX * PI / 180.0;

				PivotY-= 30 * cos(radRotX);
				PivotX+= 30 * sin(radRotX);
			}
		}
		else if (estado == EditScale){
			SetScale(-1);	
		}
		else if (estado == translacion){
			SetTranslacionObjetos(30, 0);		
		}
	    ReloadViewport(true);
	}*/
}