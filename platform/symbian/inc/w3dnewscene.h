/*
 * ==============================================================================
 *  w3dnewscene.h — Fase 3c: puente al MODELO NUEVO (core de PC)
 *  Funciones planas sin tipos del core (el Object viejo y el nuevo chocan de
 *  nombre si un TU ve ambos). Implementacion: w3dnewscene.cpp.
 * ==============================================================================
 */

#ifndef __W3DNEWSCENE_H__
#define __W3DNEWSCENE_H__

#include <e32std.h>

// raiz (SceneCollection) + cubo default, como ConstructUniversal de PC
void W3dNewSceneInit();

// renderiza el mundo NUEVO confinado al rect (arriba-izquierda) con la
// convencion de PC: 1 unidad = 1 metro, perspectiva 60 grados, luz GLES1
void W3dNewSceneRenderInViewport(TInt aX, TInt aY, TInt aW, TInt aH, TInt aScreenH);

// camara orbital del mundo nuevo
void W3dNewSceneOrbit(TInt aDx, TInt aDy);
void W3dNewSceneZoom(TInt aDelta);

// seleccion por color picking sobre el modelo nuevo
TBool W3dNewScenePick(TInt aX, TInt aY, TInt aVx, TInt aVy, TInt aVw, TInt aVh, TInt aScreenH);

// panel de propiedades sobre el modelo nuevo
TBool W3dNewSelInfo(char* aName, TInt aNameMax,
                    float aPos[3], float aRot[3], float aScale[3]);
void W3dNewAdjust(TInt aRow, TInt aDelta);

// textura blanca 1x1 del pipeline (todo se dibuja texturizado; lo "plano"
// usa esta: blanco x color = color). La crea el primer frame.
unsigned int W3dNewWhiteTex();

// transformaciones del modelo nuevo (tecla 1/2/3 del telefono = G/R/S de PC)
TBool W3dNewTransformStart(TInt aMode);   // 1 mover, 2 rotar, 3 escalar
TBool W3dNewTransformActive();
TInt  W3dNewTransformModo(); // 0 nada, 1 mover, 2 rotar, 3 escalar
void W3dNewTransformMove(TInt aDx, TInt aDy);
void W3dNewTransformEnd(TBool aCancel);   // izq=aceptar, der=cancelar

// orbita la vista del viewport 3D activo (flechas del keypad, sin transform)
void W3dNewOrbit(TInt aDx, TInt aDy);
void W3dNewZoom(TInt aDelta);       // 0 + flechas arriba/abajo = zoom (keypad N95)
void W3dNewPan(TInt aDx, TInt aDy);  // * + flechas = paneo
void W3dNewLook(TInt aDx, TInt aDy); // # + flechas = primera persona (girar la mirada)
// 1/2/3 durante un transform = eje X/Y/Z (cicla global/local/view/libre)
void W3dNewTransformEje(TInt aEje);
// 0 durante una rotacion: alterna trackball <-> orbital/gimbal
void W3dNewToggleOrbital();
void W3dNewToggleEdit(); // OK = Tab (toggle Edit Mode)
// actualiza la linea punteada pivot->mouse (rotar/escalar) en Symbian
void W3dNewTransformLinea(TInt aMx, TInt aMy);
// "rotar desde la vista" (trackball) con el mouse BT
TBool W3dNewEsRotarDesdeVista();
void  W3dNewRotarDesdeVista(TInt aMx, TInt aMy);

// cicla solido -> material -> render (tecla 0 del telefono)
void W3dNewSceneCycleViewMode();

// alterna material <-> render (tecla 6 del telefono)
void W3dNewSceneToggleRenderMode();

// arbol de la escena para el outliner (indices DFS, 0 = Collection raiz)
// aTipo: 0=coleccion 1=mesh 2=luz 3=otro
TInt W3dNewTreeCount();
TBool W3dNewTreeItem(TInt aIdx, char* aName, TInt aNameMax,
                     TInt& aDepth, TBool& aSel, TInt& aTipo);
void W3dNewTreeSelect(TInt aIdx);

// importa un .obj con el importador compartido de PC
TBool W3dNewImportObj(const char* aPath);

// (de ObjectMode.cpp compartido; declarado aca porque los TUs viejos de
// Symbian no pueden incluir los headers del core por el clash de Object)
void DuplicatedObject();
void W3dNewDeseleccionarTodo(); // puente al compartido
void W3dNewSeleccionarTodo();  // 'a' del teclado BT
void W3dNewInstancia();        // alt+d: duplicado linkeado
void W3dNewSetParent();        // ctrl+p
void W3dNewClearParent();      // alt+p

// agregar objetos (0=cubo 1=plano 2=circulo 3=vertice 4=luz 5=camara 6=empty)
void W3dNewAdd(TInt aTipo);

// enfocar el objeto activo (Viewport3D real)
void W3dNewEnfocar();

// tecla lapiz: cicla la seleccion - tecla C/borrar: borra el objeto activo
void W3dNewCycleSelect();
TBool W3dNewDeleteActive();

#endif
