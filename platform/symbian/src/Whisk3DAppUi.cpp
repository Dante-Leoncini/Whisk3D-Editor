/*
 * ==============================================================================
 *  Name        : Whisk3DAppUi.cpp
 *  Part of     : OpenGLEx / Whisk3D
 * ==============================================================================
 */

// INCLUDE FILES
#include "Whisk3DAppUi.h"
#include "w3dnewscene.h"
void DuplicatedObject(); // ObjectMode.cpp compartido
#include "Whisk3DContainer.h"
#include <Whisk3D.rsg>
#include "whisk3D.hrh"

#include <eikmenup.h>
#include <avkon.hrh>

#include <aknconsts.h>

// ================= MEMBER FUNCTIONS =======================
//
// ----------------------------------------------------------
// CWhisk3DAppUi::ConstructL
// Symbian 2nd phase constructor can leave.
// ----------------------------------------------------------
//
void CWhisk3DAppUi::ConstructL(){
    BaseConstructL();
    iAppContainer = new (ELeave) CWhisk3DContainer;
    iAppContainer->SetMopParent(this);
    iAppContainer->ConstructL( ClientRect() );
    AddToStackL( iAppContainer );
}

// Destructor
CWhisk3DAppUi::~CWhisk3DAppUi(){
	if ( iAppContainer ){
		RemoveFromStack( iAppContainer );
		delete iAppContainer;
	}
}

// ------------------------------------------------------------------------------
//  CWhisk3DAppUi::DynInitMenuPaneL
//  This function is called by the EIKON framework just before it displays
//  a menu pane. Its default implementation is empty, and by overriding it,
//  the application can set the state of menu items dynamically according
//  to the state of application data.
// ------------------------------------------------------------------------------
//
enum{
	ObjectMode
};


void CWhisk3DAppUi::DynInitMenuPaneL(TInt /*aResourceId*/, CEikMenuPane* /*aMenuPane*/ ){
    // (sin dimming dinamico: los menus quedaron minimos tras la limpieza S60;
    //  el modelo de objetos viejo que decidia que items mostrar ya no existe)
}

// ----------------------------------------------------
// CWhisk3DAppUi::HandleKeyEventL
// Key event handler
// ----------------------------------------------------
//
TKeyResponse CWhisk3DAppUi::HandleKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType ){
    if ( iAppContainer->iWhisk3D ){ // (la FSM se retiro: siempre corriendo)   
        TUint scan = aKeyEvent.iScanCode;
            
        // Imprimir el código de escaneo para depuración
        /*if (scan != 165){
            HBufC* noteBuf = HBufC::NewLC(100);
            _LIT(KFormatString, "Scan Code: %d\naType: %d");
            noteBuf->Des().Format(KFormatString, scan, aType);			
            iAppContainer->iWhisk3D->DialogAlert(noteBuf);
        }*/

        if (aType == EEventKeyDown) {
			switch(scan){    
                /*case EKeyIncVolume: // Volumen arriba
                    iAppContainer->iWhisk3D->SetPosicion();
                    return EKeyWasConsumed;
                case EKeyDecVolume: // Volumen abajo
                    iAppContainer->iWhisk3D->SetPosicion();
                    return EKeyWasConsumed;
                case(162): //volumen arriba
                    iAppContainer->iWhisk3D->SetPosicion();
                    return EKeyWasNotConsumed;
                case(163): //volumen abajo
                    iAppContainer->iWhisk3D->SetPosicion();
                    return EKeyWasNotConsumed;*/
                case EStdKeyEscape: // Código estándar para la tecla ESC
                    W3dNewTransformEnd(ETrue); // modelo nuevo // Llama a tu función Cancelar
                    return EKeyWasConsumed;                
                /*case(14): //izquierda
                    //iAppContainer->iWhisk3D->Rotar(1);
                    iAppContainer->iWhisk3D->Tab();
                    return EKeyWasConsumed;
                case(15): //derecha
                    //iAppContainer->iWhisk3D->Rotar(2);
                    iAppContainer->iWhisk3D->Tab();
                    return EKeyWasConsumed;
                case(16): //arriba
                    //iAppContainer->iWhisk3D->NextPos(0,1);
                    iAppContainer->iWhisk3D->Tab();
                    return EKeyWasConsumed;
                case(17): //abajo
                    //iAppContainer->iWhisk3D->NextPos(8,1);
                    iAppContainer->iWhisk3D->Tab();
                    return EKeyWasConsumed;*/     
                case(18): //left Shift
                case(19): //rigth Shift
                    iAppContainer->iWhisk3D->iShiftPressed = ETrue;
                    return EKeyWasConsumed;
                case(22): //left Ctrl
                    iAppContainer->iWhisk3D->iCtrlPressed = ETrue;
                    return EKeyWasConsumed;
                case(20): //left  Alt
                    iAppContainer->iWhisk3D->iAltPressed = ETrue;
                    return EKeyWasConsumed;
                case(1): //Delete
                    W3dNewDeleteActive(); // el del modelo nuevo
                    return EKeyWasConsumed;
                case(49): //1
                    iAppContainer->iWhisk3D->TecladoNumerico(1);
                    return EKeyWasConsumed;
                case(50): //2
                    iAppContainer->iWhisk3D->TecladoNumerico(2);
                    return EKeyWasConsumed;
                case(51): //3
                    iAppContainer->iWhisk3D->TecladoNumerico(3);
                    return EKeyWasConsumed;
                case(52): //4
                    iAppContainer->iWhisk3D->TecladoNumerico(4);
                    return EKeyWasConsumed;
                case(53): //5
                    iAppContainer->iWhisk3D->TecladoNumerico(5);
                    return EKeyWasConsumed;
                case(54): //6
                    iAppContainer->iWhisk3D->TecladoNumerico(6);
                    return EKeyWasConsumed;
                case(55): //7
                    iAppContainer->iWhisk3D->TecladoNumerico(7);
                    return EKeyWasConsumed;
                case(56): //8
                    iAppContainer->iWhisk3D->TecladoNumerico(8);
                    return EKeyWasConsumed;
                case(57): //9
                    iAppContainer->iWhisk3D->TecladoNumerico(9);
                    return EKeyWasConsumed;
                case(48): //0
                    iAppContainer->iWhisk3D->TecladoNumerico(0);
                    return EKeyWasConsumed;
                case(42): //*
                    iAppContainer->iWhisk3D->TecladoNumerico(10);
                    return EKeyWasConsumed;
                case(127): //#
                    iAppContainer->iWhisk3D->TecladoNumerico(11);
                    return EKeyWasConsumed;
                case(226): //camara
                    //iAppContainer->iWhisk3D->Extrude();
                    return EKeyWasConsumed;
                case(196): //llamada
                    iAppContainer->iWhisk3D->ChangeEje();
                    return EKeyWasConsumed;
                case(71): //G
                    iAppContainer->iWhisk3D->SetPosicion();
                    return EKeyWasConsumed;
                case(82): //R
                    iAppContainer->iWhisk3D->SetRotacion();
                    return EKeyWasConsumed;
                case(81): //Q
                    W3dNewTransformEnd(ETrue); // modelo nuevo
                    return EKeyWasConsumed;
                case(88): //X
                    iAppContainer->iWhisk3D->EventKeyDown(scan);
                    return EKeyWasConsumed;
                case(3): //Enter
                    W3dNewTransformEnd(EFalse); // modelo nuevo
                    return EKeyWasConsumed;
                case(167): //OK
                    if (W3dNewTransformActive()) W3dNewTransformEnd(EFalse); // confirma el transform en curso
                    else W3dNewToggleEdit(); // OK con malla seleccionada = TAB de PC (entra/sale de Edit Mode)
                    return EKeyWasConsumed;
                // (case 2 / Tab: PressTab viejo retirado)
                /*case(14): //izquierda
                    iAppContainer->iWhisk3D->Tab();
                    return EKeyWasNotConsumed;
                case(15): //derecha
                    iAppContainer->iWhisk3D->Tab();
                    return EKeyWasNotConsumed;
                case(16): //arriba
                    iAppContainer->iWhisk3D->Tab();
                    return EKeyWasNotConsumed;
                case(17): //abajo
                    iAppContainer->iWhisk3D->Tab();
                    return EKeyWasNotConsumed;*/
                case EStdKeyNo: // tecla ROJA "cortar llamada" (end): NO cerrar la app -> consumir
                    return EKeyWasConsumed;
                default:
                    iAppContainer->iWhisk3D->EventKeyDown(scan);
                    return EKeyWasNotConsumed;
			}
		}
        else if (aType == EEventKeyUp) {
            TUint scan = aKeyEvent.iScanCode;
            switch (scan) {                
                /*case(14): //izquierda
                    //if (iAppContainer->iWhisk3D->iShiftPressed) {
                    iAppContainer->iWhisk3D->Tab();
                    return EKeyWasNotConsumed;
                case(15): //derecha
                    iAppContainer->iWhisk3D->Tab();
                    return EKeyWasNotConsumed;
                case(16): //arriba
                    iAppContainer->iWhisk3D->Tab();
                    return EKeyWasNotConsumed;
                case(17): //abajo
                    iAppContainer->iWhisk3D->Tab();
                    return EKeyWasNotConsumed;*/
                case 18: // Left Shift
                case 19: // Right Shift
                    iAppContainer->iWhisk3D->iShiftPressed = EFalse;
                    return EKeyWasConsumed;                    
                case(22): //left Ctrl
                    iAppContainer->iWhisk3D->iCtrlPressed = EFalse;
                    return EKeyWasConsumed;
                case(20): //left  Alt
                    iAppContainer->iWhisk3D->iAltPressed = EFalse;
                    return EKeyWasConsumed;
                case EStdKeyNo: // tecla ROJA (end): no cerrar la app
                    return EKeyWasConsumed;
                default:
                    return EKeyWasNotConsumed;
            }
        }
	}
    return EKeyWasNotConsumed;
}

enum{
	cubo, esfera, cilindro, plane, vacio, camara,
    cad, luz, vertice, circle
};

enum{
	vertexSelect, edgeSelect, faceSelect
};

typedef enum { AnimPosition, AnimRotation, AnimScale };

// ----------------------------------------------------
// CWhisk3DAppUi::HandleCommandL
// Command handler
// ----------------------------------------------------
//
void CWhisk3DAppUi::HandleCommandL(TInt aCommand){
    switch ( aCommand )
        {
        case EAknSoftkeyBack:
            iAppContainer->iWhisk3D->SetMouse();
            break;
        case EEikCmdExit:
            {
            Exit();
            break;
            }
        //nuevos!!!      
        //case EMaterial:
        //    OpenMaterialMenuL();
        //    break;
        // (EViewportBackgroudColor: el fondo lo maneja el Viewport3D nuevo)
        // (los Add del menu S60 se borraron: estan en el desplegable
        // compartido del boton "Add"; el menu Material tambien: el
        // material se edita en el panel de propiedades compartido)
        default:
            break;
        }
    }

// End of File
