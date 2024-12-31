/*
 * ==============================================================================
 *  Name        : Whisk3DAppUi.cpp
 *  Part of     : OpenGLEx / Whisk3D
 * ==============================================================================
 */

// INCLUDE FILES
#include "Whisk3DAppUi.h"
#include "Whisk3DContainer.h"
#include <Whisk3D.rsg>
#include "whisk3D.hrh"

#include <eikmenup.h>
#include <avkon.hrh>
#include "Mesh.h"

#include <aknconsts.h>

//para mostrar menus con listas
#include <aknlistquerydialog.h> // Para el popup


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

enum {Solid, MaterialPreview, Rendered};

void CWhisk3DAppUi::DynInitMenuPaneL(TInt aResourceId, CEikMenuPane* aMenuPane ){
    //para trabajar mas facil
    CWhisk3D& BL = *(iAppContainer->iWhisk3D); 

	//oculta el SetOrigen si no esta en modo edicion
    /* if (aResourceId == R_WHISK3D_MENU ) {
        aMenuPane->SetItemDimmed(EWhisk3DSeleccionar, ETrue);
        aMenuPane->SetItemDimmed(EViewportAdd, EFalse);

        //si hay objetos            
        if (BL.Objects.Count() > 0){   
            aMenuPane->SetItemDimmed(EViewportObject, EFalse);   
            Object& obj = BL.Objects[BL.SelectActivo];
            //si esta seleccionada
            if (obj.seleccionado){ 
                aMenuPane->SetItemDimmed(ETransformMenu, EFalse);
                aMenuPane->SetItemDimmed(ESnapMenu, EFalse);  
                //si es una malla 3d
                if (obj.type == mesh){
                    aMenuPane->SetItemDimmed(ECameraMenu, ETrue); 
                    aMenuPane->SetItemDimmed(EMaterial, EFalse);
                    aMenuPane->SetItemDimmed(EWhisk3DModificadores, EFalse);
                }
                //si es una camara
                else if (obj.type == camera){
                    aMenuPane->SetItemDimmed(ECameraMenu, EFalse); 
                    aMenuPane->SetItemDimmed(EMaterial, ETrue);
                    aMenuPane->SetItemDimmed(EWhisk3DModificadores, ETrue); 
                }
                //si es otra cosa
                else {
                    aMenuPane->SetItemDimmed(ECameraMenu, ETrue); 
                    aMenuPane->SetItemDimmed(EMaterial, ETrue);
                    aMenuPane->SetItemDimmed(EWhisk3DModificadores, ETrue); 
                }                    
            }
            //si nada esta seleccionado
            else {
                aMenuPane->SetItemDimmed(ECameraMenu, ETrue); 
                aMenuPane->SetItemDimmed(EMaterial, ETrue);
                aMenuPane->SetItemDimmed(EWhisk3DModificadores, ETrue);  
                aMenuPane->SetItemDimmed(ETransformMenu, ETrue);  
                aMenuPane->SetItemDimmed(ESnapMenu, ETrue);  
                aMenuPane->SetItemDimmed(EViewportObject, ETrue);                                         
            }
        }
        else {
            aMenuPane->SetItemDimmed(ECameraMenu, ETrue); 
            aMenuPane->SetItemDimmed(EWhisk3DModificadores, ETrue);  
            aMenuPane->SetItemDimmed(EMaterial, ETrue);
            aMenuPane->SetItemDimmed(EViewportObject, ETrue);
            aMenuPane->SetItemDimmed(ETransformMenu, ETrue);  
            aMenuPane->SetItemDimmed(ESnapMenu, ETrue);   
            aMenuPane->SetItemDimmed(EViewportObject, ETrue);                   
        }
    }
    else if ( aResourceId == R_CAMERA_MENU ){
        if (BL.CameraToView){  
            aMenuPane->SetItemTextL( ESetCameraToView, R_CAMERATOVIEW_OFF );
        }
        else {
            aMenuPane->SetItemTextL( ESetCameraToView, R_CAMERATOVIEW_ON  );
        } 
    }    
    else if (aResourceId == R_VIEWPORT_OVERLEY_MENU ) {
        // Texto para setear la pantalla ancha
        if ( iAppContainer->iWhisk3D->iWidescreenEnabled ){
            aMenuPane->SetItemTextL( EWhisk3DWidescreen, R_WIDESCREEN_OFF );
            }
        else {
            aMenuPane->SetItemTextL( EWhisk3DWidescreen, R_WIDESCREEN_ON  );
        }
        if ( iAppContainer->iWhisk3D->showOverlays ){
            aMenuPane->SetItemTextL( EViewportSetOverlay, R_HIDEOVERLAY );
        }
        else {
            aMenuPane->SetItemTextL( EViewportSetOverlay, R_SHOWOVERLAY  );
        } 
        if ( iAppContainer->iWhisk3D->show3DCursor ){
            aMenuPane->SetItemTextL( EViewportSet3DCursor, R_HIDE3DCURSOR );
        }
        else {
            aMenuPane->SetItemTextL( EViewportSet3DCursor, R_SHOW3DCURSOR  );
        } 

        if ( iAppContainer->iWhisk3D->showFloor ){
            aMenuPane->SetItemTextL( EViewportSetShowFloor, R_HIDEFLOOR );
        }
        else {
            aMenuPane->SetItemTextL( EViewportSetShowFloor, R_SHOWFLOOR  );
        } 
        if ( iAppContainer->iWhisk3D->showYaxis ){
            aMenuPane->SetItemTextL( EViewportSetShowYaxis, R_HIDEYAXIS );
        }
        else {
            aMenuPane->SetItemTextL( EViewportSetShowYaxis, R_SHOWYAXIS  );
        } 
        if ( iAppContainer->iWhisk3D->showXaxis ){
            aMenuPane->SetItemTextL( EViewportSetShowXaxis, R_HIDEXAXIS );
        }
        else {
            aMenuPane->SetItemTextL( EViewportSetShowXaxis, R_SHOWXAXIS  );
        } 
        if ( iAppContainer->iWhisk3D->showOutlineSelect ){
            aMenuPane->SetItemTextL( EViewportSetOutlineSelect, R_HIDEOUTLINESELECT );
        }
        else {
            aMenuPane->SetItemTextL( EViewportSetOutlineSelect, R_SHOWOUTLINESELECT  );
        } 
        if ( iAppContainer->iWhisk3D->showOrigins ){
            aMenuPane->SetItemTextL( EViewportSetOrigins, R_HIDEORIGIN );
        }
        else {
            aMenuPane->SetItemTextL( EViewportSetOrigins, R_SHOWORIGIN  );
        }    
    }  
    else if (aResourceId == R_TIMELINE_MENU ) {
        if ( iAppContainer->iWhisk3D->PlayAnimation ){
            aMenuPane->SetItemTextL( ETimelinePlayStop, R_STOP );
        }
        else {
            aMenuPane->SetItemTextL( ETimelinePlayStop, R_PLAY );
        }
        if ( iAppContainer->iWhisk3D->ShowTimeline ){
            aMenuPane->SetItemTextL( ETimelineShowTimeline, R_HIDE_TIMELINE );
        }
        else {
            aMenuPane->SetItemTextL( ETimelineShowTimeline, R_SHOW_TIMELINE  );
        }
    }*/
}

// ----------------------------------------------------
// CWhisk3DAppUi::HandleKeyEventL
// Key event handler
// ----------------------------------------------------
//
TKeyResponse CWhisk3DAppUi::HandleKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType ){
    if ( iAppContainer->iWhisk3D->GetState() == CWhisk3D::ERunning ){   
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
                    iAppContainer->iWhisk3D->Cancelar(); // Llama a tu función Cancelar
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
                case(5): //Space
                    iAppContainer->iWhisk3D->ToggleValue(iAppContainer->iWhisk3D->PlayAnimation);
                    return EKeyWasConsumed;
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
                    iAppContainer->iWhisk3D->Borrar();
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
                case(65): //A
                    iAppContainer->iWhisk3D->SeleccionarTodo();
                    return EKeyWasConsumed;
                case(71): //G
                    iAppContainer->iWhisk3D->SetPosicion();
                    return EKeyWasConsumed;
                case(82): //R
                    iAppContainer->iWhisk3D->SetRotacion();
                    return EKeyWasConsumed;
                case(81): //Q
                    iAppContainer->iWhisk3D->Cancelar();
                    return EKeyWasConsumed;
                case(88): //X
                    iAppContainer->iWhisk3D->EventKeyDown(scan);
                    return EKeyWasConsumed;
                case(3): //Enter
                    iAppContainer->iWhisk3D->Aceptar();
                    return EKeyWasConsumed;
                case(167): //OK
                    iAppContainer->iWhisk3D->Aceptar();
                    return EKeyWasConsumed;
                case(89): //Y
                    iAppContainer->iWhisk3D->SetEje(1);
                    return EKeyWasConsumed;
                case(90): //Z
                    iAppContainer->iWhisk3D->SetEje(2);
                    return EKeyWasConsumed;
                case(2): //Tab
                    iAppContainer->iWhisk3D->PressTab();
                    return EKeyWasNotConsumed;
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
                default:
                    return EKeyWasNotConsumed;
            }
        }
	}
    return EKeyWasNotConsumed;
}

void CWhisk3DAppUi::TestTouch(){
    /*// Crear un array con las opciones
    CDesCArrayFlat* optionsArray = new (ELeave) CDesCArrayFlat(3);
    CleanupStack::PushL(optionsArray);

    optionsArray->AppendL(_L("Opcion 1"));
    optionsArray->AppendL(_L("Opcion 2"));
    optionsArray->AppendL(_L("Opcion 3"));

    // Crear un cuadro emergente con las opciones
    TInt selectedOption = -1; // Para capturar la selección del usuario
    CAknListQueryDialog* popupDialog = CAknListQueryDialog::NewL(&selectedOption);

    popupDialog->PrepareLC(R_AVKON_LIST_QUERY_DIALOG);
    popupDialog->SetItemTextArray(optionsArray);
    popupDialog->SetOwnershipType(ELbmOwnsItemArray);

    // Mostrar el cuadro de di�logo
    TInt selectedOption = popupDialog->RunLD();

    // Manejar la selecci�n del usuario
    if (selectedOption >= 0)
    {
        // El usuario seleccion� una opci�n
        switch (selectedOption)
        {
        case 0:
            // Acci�n para "Opci�n 1"
            break;
        case 1:
            // Acci�n para "Opci�n 2"
            break;
        case 2:
            // Acci�n para "Opci�n 3"
            break;
        }
    }
    else
    {
        // El usuario cancel� el di�logo
    }

    CleanupStack::PopAndDestroy(optionsArray); // Limpiar memoria*/
}


enum{
	cubo, esfera, cilindro, plane, vacio, camara,
    cad, luz, vertice, circle
};

enum{
	top,
	front,
	right,
    cameraView
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
        case EViewportSetShowFloor:
            iAppContainer->iWhisk3D->ToggleValue(iAppContainer->iWhisk3D->showFloor);
            break;       
        case EViewportSetShowYaxis:
            iAppContainer->iWhisk3D->ToggleValue(iAppContainer->iWhisk3D->showYaxis);
            break;       
        case EViewportSetShowXaxis:
            iAppContainer->iWhisk3D->ToggleValue(iAppContainer->iWhisk3D->showXaxis);
            break;       
        case EViewportSetOutlineSelect:
            iAppContainer->iWhisk3D->ToggleValue(iAppContainer->iWhisk3D->showOutlineSelect);
            break;       
        case EViewportSetOrigins:
            iAppContainer->iWhisk3D->ToggleValue(iAppContainer->iWhisk3D->showOrigins);
            break;          
        case EViewportSetOverlay:
            iAppContainer->iWhisk3D->ToggleValue(iAppContainer->iWhisk3D->showOverlays);
            break;       
        case EViewportSet3DCursor:
            iAppContainer->iWhisk3D->ToggleValue(iAppContainer->iWhisk3D->show3DCursor);
            break;    
        case ETimelinePlayStop:
            iAppContainer->iWhisk3D->ToggleValue(iAppContainer->iWhisk3D->PlayAnimation);
            break;  
        case ETimelineShowTimeline:
            iAppContainer->iWhisk3D->ToggleValue(iAppContainer->iWhisk3D->ShowTimeline);
            break;
        case ETimelineSetCurrentFrame:
            iAppContainer->iWhisk3D->SetCurrentFrame();
            break;
        case ETimelineSetEndFrame:
            iAppContainer->iWhisk3D->SetEndFrame();
            break;
        case ETimelineSetStartFrame:
            iAppContainer->iWhisk3D->SetStartFrame();
            break;
        case EImportOBJ:
            iAppContainer->iWhisk3D->ImportOBJ();
            break; 
        case EWhisk3DWidescreen:
            iAppContainer->SetWidescreen();
            break;    
        case EViewportBackgroudColor:
            iAppContainer->iWhisk3D->SetViewportBackgroudColor();
            break;    
        case ECursorToSelect:
            iAppContainer->iWhisk3D->CursorToSelect();
            break;
        case ESelectToCursor:
            iAppContainer->iWhisk3D->SelectToCursor();
            break;     
        case ECursorToWorldOrigin:    
            iAppContainer->iWhisk3D->CursorToWorldOrigin();
            break;        
        case ECamera:            
            iAppContainer->iWhisk3D->AddObject(camera);
            break;   
        case ELight:            
            iAppContainer->iWhisk3D->AddObject(light);
            break;       
        case EEmpty:            
            iAppContainer->iWhisk3D->AddObject(empty);
            break;
        case ECube:            
            iAppContainer->iWhisk3D->AddMesh(cubo);
            break;  
        case ECircle:            
            iAppContainer->iWhisk3D->AddMesh(circle);
            break;  
        case EPlane:            
            iAppContainer->iWhisk3D->AddMesh(plane);
            break;    
        case EVertex:
            iAppContainer->iWhisk3D->AddMesh(vertice);
            break; 
        case ESetMaterial:
            iAppContainer->iWhisk3D->SetMaterial();
            break;  
        case ESetSpecular:
            iAppContainer->iWhisk3D->SetSpecular();
            break;     
        case EWhisk3DSetEscala:
            iAppContainer->iWhisk3D->SetEscala();
            break;   
        case EWhisk3DSetPosicion:
            iAppContainer->iWhisk3D->SetPosicion();
            break;   
        case EWhisk3DSetRotacion:
            iAppContainer->iWhisk3D->SetRotacion();
            break;     
        case EDuplicatedObject:
            iAppContainer->iWhisk3D->DuplicatedObject();
            break;       
        case EDuplicatedLinked:
            iAppContainer->iWhisk3D->DuplicatedLinked();
            break;     
        case ESetParent:
            iAppContainer->iWhisk3D->SetParent();
            break;     
        case EClearParent:
            iAppContainer->iWhisk3D->ClearParent();
            break;  
        case ESetActiveObjectAsCamera:
            iAppContainer->iWhisk3D->SetActiveObjectAsCamera();
            break;  
        case ERenderUI:
            iAppContainer->iWhisk3D->SaveCanvasToImage(false, true);
            break;  
        case ERenderUIanimation:
            iAppContainer->iWhisk3D->SaveCanvasToImage(false, true);
            break;  
        case ERenderImage:
            iAppContainer->iWhisk3D->SaveCanvasToImage(false, false);
            break; 
        case ERenderAnimation:
            iAppContainer->iWhisk3D->SaveCanvasToImage(true, false);
            break; 
        case ESetAmbientLight:
            iAppContainer->iWhisk3D->SetAmbientLight();
            break;   
        case ESetDiffuse:
            iAppContainer->iWhisk3D->SetDiffuse();
            break;   
        case ESetEmission:
            iAppContainer->iWhisk3D->SetEmission();
            break;  
        case ESetTexture:
            iAppContainer->iWhisk3D->SetTexture();
            break;  
        case ESetActiveTexture:
            iAppContainer->iWhisk3D->ActivarTextura();
            break; 
        case ESetInterpolation:
            iAppContainer->iWhisk3D->SetInterpolation();
            break; 
        case ESetTransparent:
            iAppContainer->iWhisk3D->SetTransparencia();
            break; 
        case ESetTextureRepeat:
            iAppContainer->iWhisk3D->SetTextureRepeat();
            break;     
        case ESetCulling:
            iAppContainer->iWhisk3D->SetCulling();
            break;  
        case ESetLighting:
            iAppContainer->iWhisk3D->SetLighting();
            break;  
        case ESetVertexColor:
            iAppContainer->iWhisk3D->SetVertexColor();
            break;    
        case EWhisk3DInfoObject:
            iAppContainer->iWhisk3D->InfoObject(1);
            break; 
        case EWhisk3DBorrar:
            iAppContainer->iWhisk3D->Borrar();
            break;  
        case EWhisk3DPerspectiva:
            iAppContainer->iWhisk3D->SetPerspectiva();
            break;
        case ECameraView:
            iAppContainer->iWhisk3D->SetViewpoint(cameraView);
            break;
        case ESetCameraToView:
            iAppContainer->iWhisk3D->SetCameraToView();
            break;
        case ETopView:
            iAppContainer->iWhisk3D->SetViewpoint(top);
            break;
        case EWhisk3DFrontView:
            iAppContainer->iWhisk3D->SetViewpoint(front);
            break;
        case EWhisk3DRightView:
            iAppContainer->iWhisk3D->SetViewpoint(right);
            break;
        case EWhisk3DObjFocus:
            iAppContainer->iWhisk3D->EnfocarObject();
            break; 
        case EWhisk3DViewMaterial:
            iAppContainer->iWhisk3D->SetShading(MaterialPreview);
            break; 
        case EWhisk3DViewSolid:
            iAppContainer->iWhisk3D->SetShading(Solid);
            break; 
        case EViewRendered:
            iAppContainer->iWhisk3D->SetShading(Rendered);
            break;
        case EWhisk3DCambiarObject:
            iAppContainer->iWhisk3D->changeSelect();
            break; 
        case ESelectInvert:
            iAppContainer->iWhisk3D->InvertirSeleccion();
            break; 
        case EWhisk3DModificadorArray:
            iAppContainer->iWhisk3D->AddModificador(array);
            break; 
        case EWhisk3DModificadorMirror:
            iAppContainer->iWhisk3D->AddModificador(mirror);
            break; 
        case EWhisk3DModificadorScrew:
            iAppContainer->iWhisk3D->AddModificador(screw);
            break;             
        case ENewMaterial:
            iAppContainer->iWhisk3D->NewMaterial(true);
            break;
        case ENewTexture:
            iAppContainer->iWhisk3D->NewTexture();
            break; 
        case ESetFrameRate:
            iAppContainer->iWhisk3D->SetFrameRate();
            break; 
        case ERemoveMaterial:
            iAppContainer->iWhisk3D->RemoveMaterial();
            break;  
        case ERemoveTexture:
            iAppContainer->iWhisk3D->RemoveTexture();
            break; 
        case EInsertKeyframe:
            iAppContainer->iWhisk3D->InsertKeyframe(AnimPosition);
            break; 
        case EInsertKeyframeLocation:
            iAppContainer->iWhisk3D->InsertKeyframe(AnimPosition);
            break; 
        case EImportAnimation:
            iAppContainer->iWhisk3D->ImportAnimation();
            break; 
        case ESetMixNormals:
            iAppContainer->iWhisk3D->SetMixNormals();
            break; 
        case ESetMixFaces:
            iAppContainer->iWhisk3D->SetMixFaces();
            break; 
        case ESetSpeedMix:
            iAppContainer->iWhisk3D->SetSpeedMix();
            break; 
        case ESetShapekeysInterpolation:
            iAppContainer->iWhisk3D->SetShapekeysInterpolation();
            break;
        case ESetAnimation:
            iAppContainer->iWhisk3D->SetAnimation(-1);
            break;  
        case ETrackTo:
            iAppContainer->iWhisk3D->SetTrackTo();
            break;   
        case ECopyRotation:
            iAppContainer->iWhisk3D->SetCopyRotation();
            break;   
        case ECopyLocation:
            iAppContainer->iWhisk3D->SetCopyLocation();
            break;               
        case EFollowPath:
            iAppContainer->iWhisk3D->SetFollowPath();
            break;         
        case EClearConstraints:
            iAppContainer->iWhisk3D->ClearConstraints();
            break;   
        case EInsertKeyframeRotation:
            iAppContainer->iWhisk3D->InsertKeyframe(AnimRotation);
            break; 
        case EInsertKeyframeScale:
            iAppContainer->iWhisk3D->InsertKeyframe(AnimScale);
            break; 
        case ERemoveKeyframes:
            iAppContainer->iWhisk3D->RemoveKeyframes();
            break; 
        case EClearKeyframes:
            iAppContainer->iWhisk3D->ClearKeyframes();
            break; 
        case EWhisk3DNavegacion:
            iAppContainer->iWhisk3D->SetNavigation();
            break;            
        default:
            break;
        }
    }

// End of File
