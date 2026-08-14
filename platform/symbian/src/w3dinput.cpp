// ============================================================================
//  w3dinput.cpp — input PROPIO del telefono Symbian (teclado / keypad nativos).
//
//  El N95 no tiene teclas g/r/s como PC: dispara las transformaciones por su
//  keypad numerico y su menu. Estos metodos solo RUTEAN ese input del telefono
//  a las operaciones COMPARTIDAS (W3dNewTransformStart, etc.). El mouse/teclado
//  BLUETOOTH va aparte, en w3dmouse.cpp.
// ============================================================================
#include "Whisk3D.h"        // CWhisk3D (estos son metodos suyos)
#include "w3dnewscene.h"    // W3dNew* (transform/escena) + DuplicatedObject
#include "variables.h"      // ShiftCount (global compartido) + mouseX/mouseY
#include "render/OpcionesRender.h" // g_redraw (render event-driven)
#include "ViewPorts/LayoutInput.h" // LayoutDeleteEdit (menu Delete de edit mode)

void CWhisk3D::SetRotacion(){
	// puente: arranca la transformacion REAL (estado de PC / ObjectMode)
	if (W3dNewTransformActive()){ W3dNewTransformEnd(ETrue); }
	W3dNewTransformStart(2); // rotar
};

void CWhisk3D::SetEscala(){
	// puente: arranca la transformacion REAL (estado de PC / ObjectMode)
	if (W3dNewTransformActive()){ W3dNewTransformEnd(ETrue); }
	W3dNewTransformStart(3); // escalar
};

void CWhisk3D::ChangeEje(){
	// (el bloqueo de eje viejo se retiro: el estado quedaba congelado)
	SetPosicion(); // puente: arranca el move del modelo nuevo
}

void CWhisk3D::SetPosicion(){
	// puente: arranca la transformacion REAL (estado de PC / ObjectMode)
	if (W3dNewTransformActive()){ W3dNewTransformEnd(ETrue); }
	W3dNewTransformStart(1); // mover
};

void CWhisk3D::EventKeyDown(TInt scan){
	g_redraw = true; // tecla del keypad del telefono -> redibujar
	switch(scan){
		/*case(4): //ESC no anda...
			W3dNewTransformEnd(ETrue); // cancelar (modelo nuevo) */   
		case(68): //D
			if (iShiftPressed){
				if (InteractionMode == EditMode){
					LayoutDuplicarEdit(); // Edit: duplica la seleccion de malla + move
				} else {
					ShiftCount = 40;
					::DuplicatedObject(); // Object: duplica el objeto (compartido ObjectMode)
				}
			}
			else if (iAltPressed){
				// (duplicado linkeado: pendiente en el modelo nuevo)
			}
			break;
		case(69): //E
			// Edit Mode: extrude de la seleccion (vert/arista/cara) + arranca el move.
			// La funcion compartida ya chequea que haya malla en edicion.
			if (InteractionMode == EditMode) LayoutExtrudeFaces();
			break;
		case(83): //S
			SetEscala();
			break;
		case(88): //X
			// Edit Mode: menu Delete cerca del cursor; Object Mode: borra objetos
			if (!LayoutDeleteEdit(mouseX, mouseY)) W3dNewDeleteActive();
			break;
		case(77)://M
			break;
	}
};

void CWhisk3D::TecladoNumerico(TInt numero){
	switch (numero) {
			case 1:
				SetPosicion();
				break;
			case 2:
				SetRotacion();
				break;
			case 3:
				SetEscala();
				break;
			case 4:
				W3dNewSceneToggleRenderMode(); // modo de vista (viewport real)	
				break;
			case 5:
				W3dNewSceneToggleRenderMode(); // modo de vista (viewport real)	
				break;
			case 6:
				W3dNewSceneToggleRenderMode(); // modo de vista (viewport real)	
				break;
			case 0:
				W3dNewEnfocar(); // el del Viewport3D real
			default:
				break;
		}
};
