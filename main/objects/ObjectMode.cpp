#include "objects/Routine.h"   // duplicar una rutina
#include "objects/Sound.h"     // duplicar un sonido + su canal animable (AnimSound)
#include "ObjectMode.h"
#include "edit/Proportional.h" // proportional editing: peso de los objetos no seleccionados
#include "w3dlog.h"    // w3dLogfW: los renames automaticos quedan registrados
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "render/RenderOptions.h" // g_transformPivot + enum TransformPivot (editor)
#include "objects/Mesh.h"     // snap del cursor al centro de la malla en Edit Mode
#include "objects/Text2D.h"   // W3dDuplicateOne: copiar elementos 2D
#include "objects/Image2D.h"
#include "objects/Rect2D.h"
#include "objects/Container2D.h"
#include "objects/Slice9.h"
#include "objects/Button2D.h"
#include "objects/Expand2D.h"
#include "objects/Video2D.h"
#include "EditMesh.h"  // SelectionCenter
#include "Undo.h"      // Ctrl+Z: capturar transform / limpiar al borrar
#include "ViewPorts/LayoutInput.h" // SNAP en modo objeto (SnapFindTarget, g_snap, enums)
#include "edit/Modifier.h" // deep-copy de modificadores al duplicar
#include "edit/MeshEdit.h" // Separate: CompactLayers (cirugia del mesh nuevo)
#include "objects/Camera.h"   // AnimFov: fov/ortografica de la camara (animables)
#include "objects/Light.h"    // AnimColor: color (diffuse) de la luz (animable)
#include "objects/Materials.h" // los canales de un MATERIAL (AnimMat*): MaterialDefault, Material::lib
#include "objects/Armature.h" // Apply Transform sobre armature: hornear en los huesos
#include "animation/SkeletalAnimation.h" // BakeTransformInBones
#include "animation/VertexAnimation.h"   // curvas propias de la animacion del objeto (kind 3)
#include "animation/Armature2DAnimation.h" // Arm2DClone: deep copy del rig 2D al duplicar la malla
#include "animation/W3dAnimSet.h"          // duplicar un armature: clips propios (Shift+D) o compartidos (Alt+D)
#include "io/ProjectMeshes.h"  // la malla como RECURSO: Alt+D vinculado, Shift+D con recurso propio, hacer unica
#include "io/W3dMeshBin.h"     // W3dMeshBinMaterializeEdit: la edicion pendiente de una malla de un recurso
#include "objects/MeshResource.h"
#include "io/PrefabsEditor.h"   // lo GENERADO por una instancia de prefab: ni se borra, ni se duplica, ni se muda

// SNAP en MODO OBJETO: se snapshotean los "puntos de snap" de la seleccion al empezar el transform (todos los
// verts de las mallas + el ORIGEN de camara/lampara/empty) y move/rotate/scale imantan la BASE al target.
static void SnapObjCapture();
static void SnapFitObjMove();
void SnapFitObjRot(); // publica: la rotacion "desde la vista" (trackball) esta en ViewPort3D.cpp
static void SnapFitObjScale();

// (LayoutInput) transform de MALLA en curso + su reset: al cambiar de eje (X/Y/Z) en Edit Mode hay que
// RESETEAR el acumulado (translate/extrude/scale) a cero y re-aplicar con el eje nuevo, como en Object Mode.
extern bool EditXformActive();
extern void EditXformRestart();

void RestoreState(bool ClearState){
	if (InteractionMode == ObjectMode){
		for(size_t o=0; o < objectState.size(); o++){
			SaveState& objState = objectState[o];
			Object& obj = *objState.obj;
			obj.pos = objState.pos;
			obj.SetRotSnapshot(objState.rot, objState.rotEuler);  // tal cual se capturo (con sus vueltas)
			obj.scale = objState.scale;
		}
		//objectState.Close();
		if (ClearState) objectState.clear();
	}
	// EDIT MODE: resetear el transform de MALLA (gEVtrans/gEVscaleAmt/...) al cambiar de eje (antes no se reseteaba
	// -> el movimiento del vertice quedaba acumulado del eje anterior). No se resetea al CONFIRMAR (ClearState).
	else if (InteractionMode == EditMode && !ClearState && EditXformActive()){
		EditXformRestart();
	}
	if (ClearState) {
		state = editNavigation;
	}
};

// ---- orientacion de la transformacion (global / local / vista) ----
// eje del mundo (engine Y-up) para el enum: X=derecha, Y=depth(engine Z),
// Z=up(engine Y). Misma convencion que usa el resto del modelo.
static Vector3 AxisWorld(int a){
	if (a == X) return Vector3(1, 0, 0);
	if (a == Y) return Vector3(0, 0, 1);
	return Vector3(0, 1, 0); // Z = arriba
}
// ---------------------------------------------------------------------------
//  EMPARENTADO: los deltas del transform interactivo se calculan en MUNDO
//  (plano de camara, ejes globales), pero pos/rot del objeto viven en el
//  espacio de SU PADRE. Con un padre rotado, sumar el delta de mundo directo a
//  obj.pos aplicaba el movimiento en ejes equivocados (reporte: "aprieto g,
//  muevo el mouse a la izquierda y se mueve a la derecha"). Estos helpers
//  hacen la conversion una sola vez y en un solo lugar; con el padre sin rotar
//  ni escalar son la identidad y todo queda como siempre.
// ---------------------------------------------------------------------------

// rotacion global del PADRE (identidad si cuelga de la raiz)
static Quaternion RotParentOf(Object& o){
	return o.Parent ? RotGlobalOf(o.Parent) : Quaternion();
}

// delta de POSICION en MUNDO -> espacio del padre de 'o'. La misma matematica
// que ReparentKeepTransform/SetInGlobal: inversa de la rotacion global del
// padre + division por su escala global, componente a componente.
static Vector3 DeltaWorldToParent(Object& o, const Vector3& d){
	if (!o.Parent) return d;
	Quaternion pr = RotGlobalOf(o.Parent);
	Vector3 ps = ScaleGlobalOf(o.Parent);
	Vector3 l = pr.Inverted() * d;
	return Vector3(ps.x != 0.0f ? l.x / ps.x : l.x,
	               ps.y != 0.0f ? l.y / ps.y : l.y,
	               ps.z != 0.0f ? l.z / ps.z : l.z);
}

// aplica una rotacion DE MUNDO sobre una rotacion LOCAL base: conjugar por el
// padre (local' = P^-1 * R * P * base). PUBLICA (ObjectMode.h): el trackball
// de la vista (ViewPort3D::RotateFromView) y el snap de rotacion rotan desde
// el SNAPSHOT y necesitan la misma cuenta con otra base.
Quaternion W3dRotateWorldOver(Object& o, const Quaternion& Rmundo, const Quaternion& localBase){
	Quaternion p = RotParentOf(o);
	Quaternion q = p.Inverted() * Rmundo * p * localBase;
	q.normalize();
	return q;
}

// idem, incremental sobre la rotacion ACTUAL del objeto
static Quaternion RotateInWorld(Object& o, const Quaternion& Rmundo){
	return W3dRotateWorldOver(o, Rmundo, o.Rot());
}

// el eje constrenido EN WORLD segun la orientacion actual. La ESCALA es un caso
// especial: siempre local (no existe escala global/desde-la-vista), sin importar
// la orientacion elegida en el menu.
Vector3 AxisOriented(Object& obj, int a){
	if (state == EditScale || transformOrientation == LocalOrient){
		// GLOBAL del objeto, no obj.Rot() pelado: el eje "local" de un objeto
		// emparentado incluye la rotacion de sus padres (si no, la guia y el
		// movimiento no coinciden con lo que se ve)
		Vector3 v = RotGlobalOf(&obj) * AxisWorld(a);
		return v.Normalized();
	}
	if (transformOrientation == ViewOrient){
		if (a == X) return camRight;
		if (a == Y) return camForward;
		return camUp; // Z
	}
	if (transformOrientation == NormalOrient) return gTransformNormal; // la normal (extrude / menu Normal)
	return AxisWorld(a); // global
}

// cicla eje/orientacion DURANTE un transform (tecla X/Y/Z en PC, 1/2/3 en
// Symbian). Primer toque de un eje: lo constriñe en GLOBAL. Re-apretar el
// MISMO eje: cicla Global -> Local -> View -> LIBRE (3 ejes). Se restaura el
// estado guardado para re-aplicar con el nuevo eje (como Blender).
void CycleAxisTransform(int axis){
	if (state == editNavigation) return;
	// si el transform venia constrenido a la NORMAL (extrude o orientacion Normal) y se aprieta
	// X/Y/Z, el usuario quiere el EJE (no la normal): se sale a GLOBAL + ese eje (= comportamiento
	// de "mover"). Unifica el extrude con el operador normal: nada de codigo aparte.
	if (gEVuseCustom){
		gEVuseCustom = false;
		transformOrientation = GlobalOrient;
		axisSelect = axis;
		RestoreState(false);
		return;
	}
	if (state == EditScale){
		// la ESCALA es siempre local: solo eje-unico (local) <-> 3 ejes. No tiene
		// global ni "desde la vista" (no existe escalar desde la vista).
		transformOrientation = LocalOrient;
		axisSelect = (axisSelect == axis) ? XYZ : axis; // re-apretar el mismo -> 3 ejes
		RestoreState(false);
		return;
	}
	// rotacion/translacion: ciclo de orientacion del eje. La ROTACION agrega un
	// paso ORBITAL/gimbal entre 'view' y el trackball (libre). El ciclo es:
	//   trackball -> eje global -> eje local -> eje view -> orbital -> trackball...
	if (axisSelect != axis){
		if (state == rotation && axisSelect == OrbitalAxis){
			// orbital -> trackball: cierra el ciclo y resetea la orientacion a global
			axisSelect = ViewAxis; transformOrientation = GlobalOrient; gTrackballCap = false;
		} else {
			axisSelect = axis; // 1er toque: mantiene la orientacion ACTUAL (la del menu)
		}
	} else {
		if (transformOrientation == GlobalOrient)     transformOrientation = LocalOrient;
		else if (transformOrientation == LocalOrient) transformOrientation = ViewOrient;
		else if (state == rotation)                  axisSelect = OrbitalAxis; // view -> orbital
		else { axisSelect = ViewAxis; transformOrientation = GlobalOrient; } // translacion: view -> libre
	}
	RestoreState(false);
}

// Shift+eje: constriñe a un PLANO (excluye 'eje', mueve en los otros dos).
// Mismo plano re-apretado: cicla Global -> Local -> View -> libre.
void CyclePlaneTransform(int axis){
	if (state == editNavigation) return;
	if (gEVuseCustom){ // salir de la normal (extrude / Normal) a un PLANO global (como mover)
		gEVuseCustom = false; transformOrientation = GlobalOrient;
		axisSelect = (axis == X) ? PlaneX : (axis == Y) ? PlaneY : PlaneZ;
		RestoreState(false); return;
	}
	int plane = (axis == X) ? PlaneX : (axis == Y) ? PlaneY : PlaneZ;
	if (state == EditScale){
		// escala: plano siempre local, sin ciclar orientacion (re-apretar -> 3 ejes)
		transformOrientation = LocalOrient;
		axisSelect = (axisSelect == plane) ? XYZ : plane;
		RestoreState(false);
		return;
	}
	if (axisSelect != plane){
		axisSelect = plane; // orientacion actual (la del menu)
	} else {
		if (transformOrientation == GlobalOrient)     transformOrientation = LocalOrient;
		else if (transformOrientation == LocalOrient) transformOrientation = ViewOrient;
		else { axisSelect = ViewAxis; transformOrientation = GlobalOrient; } // libre + reset
	}
	RestoreState(false);
}

void Cancel(){
	// POSE MODE: si hay un transform de huesos en curso, se cancela ese (restaura la pose previa) y listo.
	{ extern int g_poseMode; if (g_poseMode){ extern void PoseXformCancel(); PoseXformCancel(); return; } }
	// EDIT de ARMATURE: idem con el grab/transform de puntas (restaura head/tail del snapshot).
	{ extern bool BoneGrabActive(); extern void BoneGrabCancel();
	  if (BoneGrabActive()){ BoneGrabCancel(); return; } }
	// Mostrar el cursor
#ifndef W3D_SYMBIAN
	#if SDL_MAJOR_VERSION == 2
		SDL_ShowCursor(SDL_ENABLE);
	#elif SDL_MAJOR_VERSION == 3
		SDL_ShowCursor();
	#endif
#endif

	if (state != editNavigation){
		UndoTransformCancel(); // descarta el undo pendiente del transform cancelado
		RestoreState();
	}
	ViewPortClickDown = false;
};

// Toggles de animacion. Van FUERA del #ifdef de abajo: son estado, no implementacion, y los usan las dos
// plataformas. AutoKeyOn en particular lo lee el auto key de POSE (que vive en el Core y SI anda en Symbian).
bool AutoKeyOn = false;      // lo prende/apaga el boton del timeline
bool MotionTrailOn = false;  // menu Animation del viewport 3D: ver el camino de los objetos animados

void RemoveAnimations(Object& obj){
	for(size_t a = 0; a < AnimationObjects.size(); ) {
		if (AnimationObjects[a].obj == &obj) {
			for(size_t p = 0; p < AnimationObjects[a].Properties.size(); p++) {
				AnimationObjects[a].Properties[p].keyframes.clear();
			}
			AnimationObjects[a].Properties.clear();
			// borrar SIN avanzar: la siguiente entrada cae en el mismo indice
			// (el a++ de antes se salteaba una entrada tras cada erase)
			AnimationObjects.erase(AnimationObjects.begin() + a);
		} else {
			a++;
		}
	}
}

// ===== ANIMACION DE OBJETOS (keyframes de pos/rot/escala; menu Object > Animation) =====
// La animacion de OBJETOS (transform) se guarda en AnimationObjects (uno por objeto, con curvas Position/Rotation/
// Scale). Distinta de la de ESQUELETO (clips por armature) y de la de VERTICES. Rotacion en euler XYZ (grados).
extern bool g_redraw;
static AnimationObject& AnimObjOf(Object* o){
	for (size_t i=0;i<AnimationObjects.size();i++) if (AnimationObjects[i].obj==o) return AnimationObjects[i];
	AnimationObject ao; ao.obj=o; ao.FirstKeyFrame=0; ao.LastKeyFrame=0; AnimationObjects.push_back(ao);
	return AnimationObjects.back();
}
// la animacion PROPIA activa del objeto (kind 3, la de la tarjeta "Animacion"), o
// NULL si estamos en modo escena (kind 0) -> ahi las curvas viven en AnimObjOf
VertexAnimation* AnimActiveObject(Object* o){
	extern int ActiveAnimKind; extern Mesh* ActiveAnimMesh;
	if (ActiveAnimKind != 3 || (Object*)ActiveAnimMesh != o || !o || o->getType()!=ObjectType::mesh) return NULL;
	Mesh* m = (Mesh*)o;
	VertexAnimationActive* va = FindTargetAnim(m);
	int i = va ? va->currentAnim : -1;
	return (i>=0 && i<(int)m->animations.size()) ? m->animations[i] : NULL;
}
// las curvas donde van/de donde salen los keyframes: la animacion PROPIA activa, o
// la de ESCENA (cada cosa sus propias curvas, por diseno)
static std::vector<AnimProperty>& ActiveCurves(Object* o){
	VertexAnimation* an = AnimActiveObject(o);
	if (an) return an->curves;          // animacion propia del objeto (kind 3)
	return AnimObjOf(o).Properties;     // animacion de escena (kind 0)
}
// valor ACTUAL de un canal (prop, comp) del objeto: lo que se ve ahora (para keyar)
static float ChannelCurrentValue(Object* o, int prop, int comp){
	if (prop == AnimStep)      return o->getType() == ObjectType::routine ? W3dRoutineStepValue((Routine*)o, comp) : 0.0f;
	if (prop == AnimLightIntens) return o->getType() == ObjectType::light ? ((Light*)o)->intensity : 1.0f;
	if (prop == AnimSound){   // SONIDO: volumen / tono / reverb
		if (o->getType() != ObjectType::sound) return comp == AnimY ? 1.0f : 0.0f;
		Sound* s = (Sound*)o; return comp == AnimX ? s->volume : comp == AnimY ? s->tone : s->reverb;
	}
	if (prop == AnimVisible)   return o->visible      ? 1.0f : 0.0f;
	if (prop == AnimRender)    return o->renderable ? 1.0f : 0.0f;
	// el FONDO del render (sus curvas viven en la coleccion de la escena): color, rotacion y QUE fondo
	if (prop == AnimBackground)     return (comp >= 0 && comp < 3) ? g_renderBg[comp] : 0.0f;
	if (prop == AnimBackgroundRot)  return (comp >= 0 && comp < 3) ? g_renderBgRot[comp] : 0.0f;
	if (prop == AnimBackgroundTex)  return g_renderBgMode == 1 ? (float)W3dBackgroundTextureIndex(g_renderBgTex) : 0.0f;
	// CAMARA: fov + distancias de dibujado (near/far)
	if (prop == AnimFov)  return (o->getType()==ObjectType::camera) ? ((Camera*)o)->fov : 45.0f;
	if (prop == AnimCamFocus) return (o->getType()==ObjectType::camera) ? ((Camera*)o)->focusDist : 5.0f;
	if (prop == AnimClip){ if (o->getType()==ObjectType::camera){ Camera* c=(Camera*)o; return (comp==AnimX)? c->nearClip : c->farClip; } return 0.0f; }
	// LUZ: colores (diffuse/ambient/specular) + numericos (atenuacion/spot/GL/direccional)
	if (prop==AnimColor||prop==AnimAmbient||prop==AnimSpecular||prop==AnimAtten||prop==AnimSpot||prop==AnimLightMisc){
		if (o->getType()!=ObjectType::light) return (prop==AnimColor||prop==AnimAmbient||prop==AnimSpecular)?1.0f:0.0f;
		Light* l=(Light*)o; int i=(comp==AnimX)?0:(comp==AnimY)?1:2;
		if (prop==AnimColor)    return l->diffuse[i];
		if (prop==AnimAmbient)  return l->ambient[i];
		if (prop==AnimSpecular) return l->specular[i];
		if (prop==AnimAtten)    return (comp==AnimX)? l->attConstant : (comp==AnimY)? l->attLinear : l->attQuadratic;
		if (prop==AnimSpot)     return (comp==AnimX)? l->spotCutoff : l->spotExponent;
		return (comp==AnimX)? (float)(l->LightID - GL_LIGHT0) : (l->directional?1.0f:0.0f); // AnimLightMisc
	}
	o->UpdateDisplayRot();
	const Vector3& v = (prop == AnimPosition) ? o->pos : (prop == AnimScale) ? o->scale : o->rotEuler;
	return comp == AnimX ? v.x : comp == AnimY ? v.y : v.z;
}
// busca la curva (prop, comp) SIN crearla (para consultar el estado del keyframe)
static const AnimProperty* CurveOf(Object* o, int prop, int comp){
	const std::vector<AnimProperty>& P = ActiveCurves(o);
	for (size_t k=0;k<P.size();k++) if (P[k].Property==prop && P[k].component==comp) return &P[k];
	return NULL;
}
// "JUEGO" (el estado base de un juego, ver W3dRoots.h) no tiene keyframes: las curvas del timeline son las del
// ultimo clip elegido, que en "Juego" no se ve (el dope sheet esta vacio). Keyear o borrar ahi escribia en ese
// clip sin que se viera -en la vista de un clip de jerarquia, en el clip COMPARTIDO de la biblioteca-. Frena el
// rombo de Properties y Delete/Clear Keyframe como la I y el autokey. true = se freno (y se aviso).
static bool KeysInGame(){
	extern int ActiveAnimKind;
	if (ActiveAnimKind != 2) return false;
	Notify(T("A game has no keyframes: choose one of its clips in the timeline to animate"), true);
	return true;
}
// estado del keyframe de un canal en 'frame': 0 = sin animacion; 1 = animado pero NO
// hay key en este frame; 2 = HAY key en este frame. Lo usa el boton-rombo del panel.
// En "Juego" siempre 0: no hay keyframes que mostrar (ver KeysInGame).
int AnimChannelState(Object* o, int prop, int comp, int frame){
	if (!o) return 0;
	{ extern int ActiveAnimKind; if (ActiveAnimKind == 2) return 0; }
	const AnimProperty* c = CurveOf(o, prop, comp);
	if (!c || c->keyframes.empty()) return 0;
	for (size_t i=0;i<c->keyframes.size();i++) if (c->keyframes[i].frame==frame) return 2;
	return 1;
}
// pone (si no hay) o QUITA (si hay) un key del canal en 'frame', con el valor actual
void AnimChannelToggle(Object* o, int prop, int comp, int frame){
	if (!o || KeysInGame()) return;
	// en la vista de un CLIP DE JERARQUIA solo su raiz y sus descendientes (el clip nombra su jerarquia): el
	// rombo lo dice, como la I (un click que no hace nada y no avisa parece roto)
	if (!W3dAnimObjectAllowed(o)) {
		Notify(T("This hierarchy clip only animates its root and its children"), true);
		return;
	}
	// lo GENERADO por una instancia de prefab no se anima (sus curvas no se guardan: se anima el prefab)
	if (W3dPrefabIsGeneratedNotice(o, true)) return;
	// EL FONDO: el rombo de su fila keyea JUNTOS el color (R/G/B) y QUE fondo es (el color o una textura 360, en escalon)
	if (prop == AnimBackground){
		int est = AnimChannelState(o, AnimBackground, AnimX, frame);
		for (int cc = AnimX; cc <= AnimZ; cc++){
			AnimProperty& ap = ListProperty(ActiveCurves(o), AnimBackground, cc);
			if (est == 2) DeleteKeyframeKeepingShape(ap, frame);
			else          SetKeyCurve(ap, frame, ChannelCurrentValue(o, AnimBackground, cc), HPlane);
		}
		AnimProperty& tx = ListProperty(ActiveCurves(o), AnimBackgroundTex, AnimX);
		if (est == 2) DeleteKeyframeKeepingShape(tx, frame);
		else {
			SetKeyCurve(tx, frame, ChannelCurrentValue(o, AnimBackgroundTex, AnimX), HPlane);
			for (size_t k = 0; k < tx.keyframes.size(); k++)
				if (tx.keyframes[k].frame == frame){ tx.keyframes[k].Interpolation = KfConstant; break; }
		}
		g_redraw = true;
		return;
	}
	// COLOR (diffuse/ambient/specular): UN solo rombo para las 3 curvas R/G/B -> se keyean/borran
	// JUNTAS con el valor de cada componente. El estado se mide por la curva representante (AnimX).
	if (prop == AnimColor || prop == AnimAmbient || prop == AnimSpecular){
		int est = AnimChannelState(o, prop, AnimX, frame);
		for (int cc = AnimX; cc <= AnimZ; cc++){
			AnimProperty& ap = ListProperty(ActiveCurves(o), prop, cc);
			if (est == 2) DeleteKeyframeKeepingShape(ap, frame);
			else          SetKeyCurve(ap, frame, ChannelCurrentValue(o, prop, cc), HPlane);
		}
		g_redraw = true;
		return;
	}
	int est = AnimChannelState(o, prop, comp, frame);
	AnimProperty& ap = ListProperty(ActiveCurves(o), prop, comp);
	if (est == 2) DeleteKeyframeKeepingShape(ap, frame);          // sacar el key de este frame
	else {
		SetKeyCurve(ap, frame, ChannelCurrentValue(o, prop, comp), HPlane);      // poner con el valor actual
		// visible/render son TRUE/FALSE -> keyframe CONSTANTE (escalon); los
		// numericos (pos/rot/escala) quedan en bezier (curvas suaves)
		if ((prop == AnimVisible || prop == AnimRender || prop == AnimLightMisc ||
		     (prop == AnimStep && comp % AnimStepFields == AnimStepOn))){
			for (size_t k=0;k<ap.keyframes.size();k++)
				if (ap.keyframes[k].frame==frame){ ap.keyframes[k].Interpolation = KfConstant; break; }
		}
	}
	g_redraw = true;
}

// ---- el MENU CONTEXTUAL de una propiedad (Properties: mantener apretado / click derecho): insertar, borrar todos,
//      copiar y pegar el keyframe del canal, sin el "pone o saca" del rombo ----
// las curvas de un rombo: un COLOR (diffuse/ambient/specular) son sus 3 curvas R/G/B juntas; lo demas, la suya
static int CompsOfChannel(int prop, int comp, int* out){
	if (prop == AnimColor || prop == AnimAmbient || prop == AnimSpecular){ out[0]=AnimX; out[1]=AnimY; out[2]=AnimZ; return 3; }
	out[0] = comp; return 1;
}
// se puede keyar este objeto aca? (lo mismo que mira el rombo, con sus avisos)
static bool ChannelEditable(Object* o){
	if (!o || KeysInGame()) return false;
	if (!W3dAnimObjectAllowed(o)) { Notify(T("This hierarchy clip only animates its root and its children"), true); return false; }
	if (W3dPrefabIsGeneratedNotice(o, true)) return false;
	return true;
}
// visible/render/el on de un paso son SI/NO: su key es un ESCALON (como en el rombo)
static void KeyStepIfVa(AnimProperty& ap, int prop, int comp, int frame){
	if (!(prop == AnimVisible || prop == AnimRender || prop == AnimLightMisc ||
	      (prop == AnimStep && comp % AnimStepFields == AnimStepOn))) return;
	for (size_t k=0;k<ap.keyframes.size();k++)
		if (ap.keyframes[k].frame==frame){ ap.keyframes[k].Interpolation = KfConstant; break; }
}
bool AnimChannelInsertKey(Object* o, int prop, int comp, int frame){
	if (!ChannelEditable(o)) return false;
	int cs[3]; const int n = CompsOfChannel(prop, comp, cs);
	for (int i = 0; i < n; i++){
		AnimProperty& ap = ListProperty(ActiveCurves(o), prop, cs[i]);
		SetKeyCurve(ap, frame, ChannelCurrentValue(o, prop, cs[i]), HPlane);   // (si ya habia uno, lo actualiza)
		KeyStepIfVa(ap, prop, cs[i], frame);
	}
	g_redraw = true;
	return true;
}
bool AnimChannelDeleteAll(Object* o, int prop, int comp){
	if (!ChannelEditable(o)) return false;
	int cs[3]; const int n = CompsOfChannel(prop, comp, cs);
	std::vector<AnimProperty>& P = ActiveCurves(o);
	bool something = false;
	for (int i = 0; i < n; i++)
		for (size_t k = 0; k < P.size(); k++)
			if (P[k].Property == prop && P[k].component == cs[i] && !P[k].keyframes.empty()){ P[k].keyframes.clear(); something = true; }
	g_redraw = true;
	return something;   // (el objeto se queda con el valor que tenia: ya no hay curva que lo mueva)
}
bool AnimChannelCopyKey(Object* o, int prop, int comp, int frame, std::vector<keyFrame>& out){
	out.clear();
	if (!o) return false;
	int cs[3]; const int n = CompsOfChannel(prop, comp, cs);
	for (int i = 0; i < n; i++){
		const AnimProperty* c = CurveOf(o, prop, cs[i]);
		bool has = false;
		if (c) for (size_t k = 0; k < c->keyframes.size(); k++)
			if (c->keyframes.Frame(k) == frame){ out.push_back(c->keyframes.Key(k)); has = true; break; }
		if (!has){ out.clear(); return false; }
	}
	return true;
}
bool AnimChannelPasteKey(Object* o, int prop, int comp, int frame, const std::vector<keyFrame>& in){
	int cs[3]; const int n = CompsOfChannel(prop, comp, cs);
	if ((int)in.size() != n || !ChannelEditable(o)) return false;
	for (int i = 0; i < n; i++){
		AnimProperty& ap = ListProperty(ActiveCurves(o), prop, cs[i]);
		SetKeyCurve(ap, frame, in[(size_t)i].value, HPlane);
		for (size_t k = 0; k < ap.keyframes.size(); k++)
			if (ap.keyframes[k].frame == frame){ keyFrame kf = in[(size_t)i]; kf.frame = frame; ap.keyframes[k] = kf; break; }
	}
	// el objeto toma el valor pegado (ApplyAnimationObjects solo aplica al CAMBIAR de frame: aca no cambio)
	W3dApplyCurvesInFrame(o, ActiveCurves(o), frame);
	g_redraw = true;
	return true;
}

// ============================================================================
//  LOS CANALES DE UN MATERIAL (las pistas de material de la animacion de ESCENA activa: Animation.h)
//  El mismo modelo que los canales de un objeto o de una luz: el campo del material es lo que se ve; el rombo de la
//  fila pone o saca el keyframe con el valor actual; el menu de la fila inserta / borra todos / copia / pega. Un
//  material no tiene animacion propia ni va en un clip (de esqueleto, de objeto, de jerarquia): solo en una
//  animacion de escena, asi que con otra cosa en el timeline se avisa en vez de keyear donde no se ve.
// ============================================================================
static bool MatAnimEditable(Material* m, bool notify){
	if (!m || m == MaterialDefault || !m->lib.empty()){
		if (notify) Notify(T("This material can not be animated (default or from a library)"), true);
		return false;
	}
	if (notify ? KeysInGame() : (ActiveAnimKind == 2)) return false;
	if (ActiveAnimKind != 0 || W3dAnimIsClip(ActiveSceneAnim)){
		if (notify) Notify(T("Materials are animated in a scene animation: choose one in the timeline"), true);
		return false;
	}
	return true;
}
// la curva (prop, comp) del material en la escena activa; 'crear' = la arma si falta (la pista y la curva)
static AnimProperty* MatCurve(Material* m, int prop, int comp, bool create){
	InitSceneAnimations();
	AnimationMaterial* t = W3dMatTrack(ActiveSceneAnim, m, create);
	if (!t) return NULL;
	if (create) return &ListProperty(t->Properties, prop, comp);
	for (size_t k = 0; k < t->Properties.size(); k++)
		if (t->Properties[k].Property == prop && t->Properties[k].component == comp) return &t->Properties[k];
	return NULL;
}
// el rombo: 0 = sin animar; 1 = animado sin key en 'frame'; 2 = key en 'frame' (un color: lo dice su R)
int AnimMatChannelState(Material* m, int prop, int comp, int frame){
	if (!m || !MatAnimEditable(m, false)) return 0;
	int cs[3]; W3dMatChannelComps(prop, comp, cs);
	const AnimProperty* c = MatCurve(m, prop, cs[0], false);
	if (!c || c->keyframes.empty()) return 0;
	for (size_t i = 0; i < c->keyframes.size(); i++) if (c->keyframes.Frame(i) == frame) return 2;
	return 1;
}
void AnimMatChannelToggle(Material* m, int prop, int comp, int frame){
	if (!MatAnimEditable(m, true)) return;
	const int est = AnimMatChannelState(m, prop, comp, frame);
	int cs[3]; const int n = W3dMatChannelComps(prop, comp, cs);
	for (int i = 0; i < n; i++){
		if (est == 2){ AnimProperty* ap = MatCurve(m, prop, cs[i], false); if (ap) DeleteKeyframeKeepingShape(*ap, frame); }
		else SetKeyCurve(*MatCurve(m, prop, cs[i], true), frame, W3dMatChannelGet(m, prop, cs[i]), HPlane);
	}
	g_redraw = true;
}
bool AnimMatChannelInsertKey(Material* m, int prop, int comp, int frame){
	if (!MatAnimEditable(m, true)) return false;
	int cs[3]; const int n = W3dMatChannelComps(prop, comp, cs);
	for (int i = 0; i < n; i++)
		SetKeyCurve(*MatCurve(m, prop, cs[i], true), frame, W3dMatChannelGet(m, prop, cs[i]), HPlane);   // (si habia, lo actualiza)
	g_redraw = true;
	return true;
}
bool AnimMatChannelDeleteAll(Material* m, int prop, int comp){
	if (!MatAnimEditable(m, true)) return false;
	int cs[3]; const int n = W3dMatChannelComps(prop, comp, cs);
	bool something = false;
	for (int i = 0; i < n; i++){
		AnimProperty* ap = MatCurve(m, prop, cs[i], false);
		if (ap && !ap->keyframes.empty()){ ap->keyframes.clear(); something = true; }
	}
	g_redraw = true;
	return something;   // (el material se queda con el valor que tenia: ya no hay curva que lo mueva)
}
bool AnimMatChannelCopyKey(Material* m, int prop, int comp, int frame, std::vector<keyFrame>& out){
	out.clear();
	if (!m) return false;
	int cs[3]; const int n = W3dMatChannelComps(prop, comp, cs);
	for (int i = 0; i < n; i++){
		const AnimProperty* c = MatCurve(m, prop, cs[i], false);
		bool has = false;
		if (c) for (size_t k = 0; k < c->keyframes.size(); k++)
			if (c->keyframes.Frame(k) == frame){ out.push_back(c->keyframes.Key(k)); has = true; break; }
		if (!has){ out.clear(); return false; }
	}
	return true;
}
bool AnimMatChannelPasteKey(Material* m, int prop, int comp, int frame, const std::vector<keyFrame>& in){
	int cs[3]; const int n = W3dMatChannelComps(prop, comp, cs);
	if ((int)in.size() != n || !MatAnimEditable(m, true)) return false;
	for (int i = 0; i < n; i++){
		AnimProperty& ap = *MatCurve(m, prop, cs[i], true);
		SetKeyCurve(ap, frame, in[(size_t)i].value, HPlane);
		for (size_t k = 0; k < ap.keyframes.size(); k++)
			if (ap.keyframes[k].frame == frame){ keyFrame kf = in[(size_t)i]; kf.frame = frame; ap.keyframes[k] = kf; break; }
	}
	// el material toma el valor pegado (el cabezal no se movio: nadie mas lo aplica)
	AnimationMaterial* t = W3dMatTrack(ActiveSceneAnim, m, false);
	if (t) W3dApplyMaterialCurves(m, t->Properties, (float)frame);
	g_redraw = true;
	return true;
}

// ---- los PASOS de una RUTINA (canal AnimStep: componente = id del paso * 8 + campo) ----
// los campos ANIMABLES de un paso: su on/off y sus numeros FIJOS (uno que nombra una memoria lo escribe lua)
static void FieldsStep(const W3dStep& p, std::vector<int>& fields) {
	fields.clear();
	if (W3dStepUsesOn(p.type)) fields.push_back(AnimStepOn);
	for (int k = 0; k < W3dStepNumbers(p); k++) if (p.n[k].ref.empty()) fields.push_back(k);
}
// la I sobre la lista de pasos: keyframe de TODOS sus campos animables en 'frame' (si ya los tenia todos, los saca)
void W3dAnimKeyStep(Object* o, unsigned id, int frame) {
	if (!o || o->getType() != ObjectType::routine || KeysInGame()) return;
	W3dStep* p = ((Routine*)o)->StepById(id);
	std::vector<int> fields;
	if (p) FieldsStep(*p, fields);
	if (fields.empty()) { Notify(T("This step has nothing to animate"), false); return; }
	bool all = true;
	for (size_t i = 0; i < fields.size(); i++)
		if (AnimChannelState(o, AnimStep, (int)id * AnimStepFields + fields[i], frame) != 2) all = false;
	for (size_t i = 0; i < fields.size(); i++) {
		const int comp = (int)id * AnimStepFields + fields[i];
		AnimProperty& ap = ListProperty(ActiveCurves(o), AnimStep, comp);
		if (all) DeleteKeyframeKeepingShape(ap, frame);
		else {
			SetKeyCurve(ap, frame, ChannelCurrentValue(o, AnimStep, comp), HPlane);
			if (fields[i] == AnimStepOn)   // (prendido / apagado: un escalon)
				for (size_t k = 0; k < ap.keyframes.size(); k++)
					if (ap.keyframes[k].frame == frame) ap.keyframes[k].Interpolation = KfConstant;
		}
	}
	g_redraw = true;
}
// borraron un paso: sus curvas tambien (de todas las animaciones de escena y de la propia activa)
static void RemoveCurvesStep(std::vector<AnimProperty>& P, unsigned id) {
	for (size_t k = P.size(); k-- > 0;)
		if (P[k].Property == AnimStep && (unsigned)(P[k].component / AnimStepFields) == id) P.erase(P.begin() + (long)k);
}
void W3dAnimDeleteStep(Object* o, unsigned id) {
	if (!o || !id) return;
	for (size_t i = 0; i < AnimationObjects.size(); i++)
		if (AnimationObjects[i].obj == o) RemoveCurvesStep(AnimationObjects[i].Properties, id);
	for (size_t e = 0; e < SceneAnimations.size(); e++) {
		if (!SceneAnimations[e]) continue;
		std::vector<AnimationObject>& ob = SceneAnimations[e]->objects;
		for (size_t i = 0; i < ob.size(); i++) if (ob[i].obj == o) RemoveCurvesStep(ob[i].Properties, id);
	}
	VertexAnimation* an = AnimActiveObject(o);
	if (an) RemoveCurvesStep(an->curves, id);
}

// keyframe en las TRES curvas (X/Y/Z) de una propiedad del objeto. Cada componente es una curva independiente
// (ListProperty las crea/encuentra por (propiedad, componente)); keyar toca las 3 en el mismo frame, pero
// despues se mueven/borran/curvan por separado desde el dope sheet.
static void SetKeyObj3(std::vector<AnimProperty>& props, int prop, int frame, float x, float y, float z){
	SetKeyCurve(ListProperty(props, prop, AnimX), frame, x, HPlane);
	SetKeyCurve(ListProperty(props, prop, AnimY), frame, y, HPlane);
	SetKeyCurve(ListProperty(props, prop, AnimZ), frame, z, HPlane);
}

// ============================================================================
//  AUTO KEY: al confirmar un transform, guarda SOLO los canales que CAMBIARON.
//  El "cambio" se mide contra objectState, que es el snapshot que el propio transform toma al empezar. Por eso
//  hay que llamarla ANTES de limpiarlo (ver Viewport3D::Aceptar).
//  Es por CANAL, no por propiedad: si solo rotaste en X, se guarda X Euler Rotation y NADA mas. Eso es lo que
//  permite el modelo de curvas por componente (cada X/Y/Z es una curva propia).
// ============================================================================

// un canal cambio? Se compara con una tolerancia relativa: el transform pasa por matrices y trigonometria, asi
// que un valor que "no se toco" vuelve con basura en el ultimo bit y == daria cambios fantasma en los 9 canales.
static bool AutoKeyChange(float a, float b){
	float d = a - b; if (d < 0) d = -d;
	float m = (a < 0 ? -a : a); float mb = (b < 0 ? -b : b); if (mb > m) m = mb;
	return d > 1e-5f * (1.0f + m);
}
// guarda el canal (prop, comp) si cambio. Devuelve true si guardo.
static bool AutoKeyChannel(std::vector<AnimProperty>& props, int prop, int comp, int frame, float value, float old){
	if (!AutoKeyChange(value, old)) return false;
	SetKeyCurve(ListProperty(props, prop, comp), frame, value, HPlane);
	return true;
}
// Devuelve cuantos canales guardo (0 = no hubo cambios -> no se ensucia la animacion ni el undo).
int AutoKeyObjects(){
	if (!AutoKeyOn) return 0;
	{ extern int ActiveAnimKind; if (ActiveAnimKind == 2) return 0; }   // "Juego": sin keyframes (sus clips si)
	int n = 0, outside = 0;
	for (size_t e = 0; e < objectState.size(); e++){
		Object* o = objectState[e].obj; if (!o) continue;
		o->UpdateDisplayRot();                 // rotEuler al dia (el transform trabaja sobre el quaternion)
		// (la vista de un clip de jerarquia: solo su jerarquia. Lo que se movio afuera no se keyea y se avisa)
		if (!W3dAnimObjectAllowed(o)) {
			const Vector3& p0 = objectState[e].pos; const Vector3& s0 = objectState[e].scale;
			const Vector3& r0 = objectState[e].rotEuler;
			if (AutoKeyChange(o->pos.x, p0.x) || AutoKeyChange(o->pos.y, p0.y) || AutoKeyChange(o->pos.z, p0.z) ||
			    AutoKeyChange(o->rotEuler.x, r0.x) || AutoKeyChange(o->rotEuler.y, r0.y) || AutoKeyChange(o->rotEuler.z, r0.z) ||
			    AutoKeyChange(o->scale.x, s0.x) || AutoKeyChange(o->scale.y, s0.y) || AutoKeyChange(o->scale.z, s0.z))
				outside++;
			continue;
		}
		// el euler de ANTES sale del quaternion del snapshot, que es lo unico que se guardo
		// el euler de ANTES sale del snapshot TAL CUAL (con sus vueltas). Derivarlo del quaternion del snapshot
		// lo devolvia canonico: rotar 360 daba "de 0 a 0" y el auto key no guardaba nada.
		Vector3 oldRot = objectState[e].rotEuler;
		std::vector<AnimProperty>& props = ActiveCurves(o);   // propia (kind 3) o escena (kind 0)
		const Vector3& p0 = objectState[e].pos;
		const Vector3& s0 = objectState[e].scale;
		if (AutoKeyChannel(props, AnimPosition, AnimX, CurrentFrame, o->pos.x, p0.x)) n++;
		if (AutoKeyChannel(props, AnimPosition, AnimY, CurrentFrame, o->pos.y, p0.y)) n++;
		if (AutoKeyChannel(props, AnimPosition, AnimZ, CurrentFrame, o->pos.z, p0.z)) n++;
		if (AutoKeyChannel(props, AnimRotation, AnimX, CurrentFrame, o->rotEuler.x, oldRot.x)) n++;
		if (AutoKeyChannel(props, AnimRotation, AnimY, CurrentFrame, o->rotEuler.y, oldRot.y)) n++;
		if (AutoKeyChannel(props, AnimRotation, AnimZ, CurrentFrame, o->rotEuler.z, oldRot.z)) n++;
		if (AutoKeyChannel(props, AnimScale,    AnimX, CurrentFrame, o->scale.x, s0.x)) n++;
		if (AutoKeyChannel(props, AnimScale,    AnimY, CurrentFrame, o->scale.y, s0.y)) n++;
		if (AutoKeyChannel(props, AnimScale,    AnimZ, CurrentFrame, o->scale.z, s0.z)) n++;
	}
	if (outside) Notify(T("This hierarchy clip only animates its root and its children"), true);
	if (n) g_redraw = true;
	return n;
}

// El transform de UN objeto en el frame f: su animacion si la tiene, y si no su transform actual. Cada canal se
// mira por separado (X/Y/Z son curvas propias: puede estar animado solo X).
static void TransformInFrame(Object* o, int f, Vector3& T, Vector3& R, Vector3& S){
	o->UpdateDisplayRot();
	T = o->pos; R = o->rotEuler; S = o->scale;
	for (size_t i=0;i<AnimationObjects.size();i++){
		if (AnimationObjects[i].obj != o) continue;
		const std::vector<AnimProperty>& P = AnimationObjects[i].Properties;
		T = EvalPropVec(P, AnimPosition, f, T);
		R = EvalPropVec(P, AnimRotation, f, R);
		S = EvalPropVec(P, AnimScale,    f, S);
		break;
	}
}
// Matriz de MUNDO de 'o' EN EL FRAME f: sube por la cadena de padres evaluando la animacion de CADA UNO en ese
// frame. Sin esto el trail de un objeto emparentado sale mal apenas el padre tambien se mueve: se dibujaba con el
// world ACTUAL del padre, o sea el camino del hijo pegado a donde el padre esta AHORA, no a donde estaba en cada
// frame. NULL -> identidad (no tiene padre).
static Matrix4 WorldInFrame(Object* o, int f){   // solo la usa MotionTrailOf, aca mismo
	Matrix4 M; M.Identity();
	if (!o) return M;
	// de la raiz hacia abajo: world = padre * local
	std::vector<Object*> chain;
	for (Object* p = o; p; p = p->Parent){
		chain.push_back(p);
		if (chain.size() > 256) break;   // guarda contra un ciclo en el arbol
	}
	for (size_t i = chain.size(); i-- > 0; ){
		Vector3 T, R, S;
		TransformInFrame(chain[i], f, T, R, S);
		M = M * W3dLocalTRS(T, Quaternion::FromEulerXYZ(R.x, R.y, R.z), S);
	}
	return M;
}

// ============================================================================
//  MOTION TRAIL: por donde PASA el origen de un objeto animado. Es SOLO POSICION: la curva que traza el objeto
//  en el espacio. Se muestrea frame a frame (enteros) porque es lo unico que la animacion pisa de verdad: la
//  linea que se ve ES el camino real, no una aproximacion.
//  Devuelve false si el objeto no tiene curvas de posicion (nada que dibujar).
//  'desde'/'hasta' salen de los keyframes de la propia curva, no del rango del clip: el trail muestra lo que el
//  objeto hace, aunque el clip sea mas largo.
// ============================================================================
bool MotionTrailOf(Object* o, std::vector<Vector3>& pts, std::vector<int>& keys, int& from, int& until){
	pts.clear(); keys.clear(); from = 0; until = -1;
	if (!o) return false;
	const AnimationObject* ao = NULL;
	for (size_t i=0;i<AnimationObjects.size();i++) if (AnimationObjects[i].obj==o){ ao=&AnimationObjects[i]; break; }
	if (!ao) return false;
	// rango + frames con keyframe: la UNION de las 3 curvas de posicion (X/Y/Z son curvas independientes: un
	// keyframe puede estar solo en X)
	int mn = 0x7fffffff, mx = -0x7fffffff;
	for (size_t p2=0;p2<ao->Properties.size();p2++){
		const AnimProperty& ap = ao->Properties[p2];
		if (ap.Property != AnimPosition || ap.keyframes.empty()) continue;
		for (size_t k=0;k<ap.keyframes.size();k++){
			int f = ap.keyframes[k].frame;
			if (f < mn) mn = f; if (f > mx) mx = f;
			bool already=false; for (size_t j=0;j<keys.size();j++) if (keys[j]==f){ already=true; break; }
			if (!already) keys.push_back(f);
		}
	}
	if (mn > mx) return false;           // sin curvas de posicion
	std::sort(keys.begin(), keys.end());
	from = mn; until = mx;
	for (int f = mn; f <= mx; f++){
		Vector3 p = EvalPropVec(ao->Properties, AnimPosition, f, o->pos);
		pts.push_back(WorldInFrame(o->Parent, f) * p);   // el world del PADRE en ESE frame (puede estar animado)
	}
	return true;
}

// Insert Keyframe (i): guarda pos+rot+escala de cada objeto SELECCIONADO en el frame actual.
// 'canales' es la mascara del menu desplegable (KfChannelLoc/Rot/Scl, ver Animation.h): permite
// keyframear SOLO la localizacion, SOLO la rotacion o SOLO la escala. 0 = todos (default
// historico, y lo que siguen pasando los ~20 callers viejos del harness).
void InsertKeyframeObject(int channels){
	if (channels == 0) channels = KfChannelAll;
	int outside = 0;
	Object* generated = NULL;
	for (size_t s=0;s<ObjSelects.size();s++){ Object* o=ObjSelects[s]; if (!o) continue;
		// lo GENERADO por una instancia de prefab no se anima: sus curvas no se guardan (se anima el prefab)
		if (W3dIsGenerated(o)) { if (!generated) generated = o; continue; }
		// sin posicion, rotacion ni escala (la Rutina: una lista de llamadas de GL) no hay nada que animar
		if (o->noTransform) continue;
		// la vista de un CLIP DE JERARQUIA solo guarda su raiz y sus descendientes
		if (!W3dAnimObjectAllowed(o)) { outside++; continue; }
		o->UpdateDisplayRot(); // rotEuler al dia
		std::vector<AnimProperty>& props = ActiveCurves(o);   // propia (kind 3) o escena (kind 0)
		if (channels & KfChannelLoc) SetKeyObj3(props, AnimPosition, CurrentFrame, o->pos.x, o->pos.y, o->pos.z);
		if (channels & KfChannelRot) SetKeyObj3(props, AnimRotation, CurrentFrame, o->rotEuler.x, o->rotEuler.y, o->rotEuler.z);
		if (channels & KfChannelScl) SetKeyObj3(props, AnimScale,    CurrentFrame, o->scale.x, o->scale.y, o->scale.z);
	}
	if (outside) Notify(T("This hierarchy clip only animates its root and its children"), true);
	if (generated) W3dPrefabIsGeneratedNotice(generated, true);
	g_redraw = true;
}
// Delete Keyframe: saca los keyframes del frame actual de cada objeto seleccionado
void DeleteKeyframeObject(){
	if (KeysInGame()) return;
	for (size_t s=0;s<ObjSelects.size();s++){ Object* o=ObjSelects[s]; if (!o) continue;
		std::vector<AnimProperty>& props = ActiveCurves(o);
		for (size_t p=0;p<props.size();p++){
			W3dKeys& kf = props[p].keyframes;   // (Frame(k) no expande: solo la curva que pierde una clave)
			for (size_t k=0;k<kf.size();k++) if (kf.Frame(k)==CurrentFrame){ kf.erase(kf.begin()+k); break; } } }
	g_redraw = true;
}
// Clear Keyframe: borra TODA la animacion de los objetos seleccionados
void ClearKeyframeObject(){
	if (KeysInGame()) return;
	for (size_t s=0;s<ObjSelects.size();s++) if (ObjSelects[s]) RemoveAnimations(*ObjSelects[s]);
	g_redraw = true;
}
// PLAYBACK: aplica los keyframes al transform de cada objeto animado en el frame actual. Solo al CAMBIAR de frame
// (play o scrub) -> editar/keyframear un objeto en un frame fijo no lo "resnapea". Rotacion via euler XYZ.
// aplica las curvas de UN objeto EN EL FRAME QUE SE LE PIDA (transform + visible + render + camara + luz).
// El frame es un PARAMETRO y no el global CurrentFrame porque el guardado necesita evaluar en el CUADRO BASE
// para no hornear la pose del playhead en el .w3d (ver RestAnimObjects en io/SaveW3D.cpp). Es la UNICA
// funcion que sabe que campo mueve cada canal: el reposo tiene que deshacer exactamente lo que esta hace.
void W3dApplyCurvesInFrame(Object* o, std::vector<AnimProperty>& props, int frame){
	if (!o) return;
	bool hasP=false, hasR=false, hasS=false, hasV=false, hasRnd=false, hasFov=false, hasClip=false;
	bool hasCol=false, hasAmb=false, hasSpec=false, hasAtt=false, hasSpot=false, hasMisc=false;
	for (size_t p=0;p<props.size();p++){ if (props[p].keyframes.empty()) continue;
		int P = props[p].Property;
		if      (P==AnimPosition) hasP=true;   else if (P==AnimRotation)  hasR=true;
		else if (P==AnimScale)    hasS=true;   else if (P==AnimVisible)   hasV=true;
		else if (P==AnimRender)   hasRnd=true; else if (P==AnimFov)       hasFov=true;
		else if (P==AnimClip)     hasClip=true;else if (P==AnimColor)     hasCol=true;
		else if (P==AnimAmbient)  hasAmb=true; else if (P==AnimSpecular)  hasSpec=true;
		else if (P==AnimAtten)    hasAtt=true; else if (P==AnimSpot)      hasSpot=true;
		else if (P==AnimLightMisc)hasMisc=true;
		else if (P==AnimSound)   hasCol=true;
		else if (P==AnimLightIntens)hasCol=true;
		else if (P==AnimCamFocus)  hasFov=true; }   // (el foco de la camara: lo aplica el bloque de la camara)   // (la intensidad la aplica el bloque de la luz, abajo)   // (un sonido animado tambien lo posa el playhead: su volumen)
	// alguna curva de verdad va a escribir un canal -> el playhead POSA a este objeto.
	// Lo lee RestAnimObjects al guardar, para devolverlo al cuadro base y no hornear el
	// scrub. Si nadie lo posa, el guardado NO lo toca (lo que el usuario movio a mano manda).
	if (hasP||hasR||hasS||hasV||hasRnd||hasFov||hasClip||hasCol||hasAmb||hasSpec||hasAtt||hasSpot||hasMisc)
		o->posedByCurves = true;
	if (hasP) o->pos   = EvalPropVec(props, AnimPosition, frame, o->pos);
	if (hasS) o->scale = EvalPropVec(props, AnimScale,    frame, o->scale);
	if (hasR){ Vector3 e = EvalPropVec(props, AnimRotation, frame, o->rotEuler); o->SetRotEuler(e); }
	W3dApplyShapes(o, props, frame);   // SHAPE KEYS: los pesos de las formas (la cara que habla)
	if (hasV)   o->visible      = ListProperty(props, AnimVisible, AnimX).Eval(frame, o->visible?1.f:0.f) >= 0.5f;
	if (hasRnd) o->renderable = ListProperty(props, AnimRender,  AnimX).Eval(frame, o->renderable?1.f:0.f) >= 0.5f;
	// CAMARA: fov + distancias de dibujado (near/far). La vista al mirar por la camara sigue las curvas.
	if (o->getType()==ObjectType::camera){ Camera* c=(Camera*)o;
		if (hasFov)  c->fov      = ListProperty(props, AnimFov, AnimX).Eval(frame, c->fov);
		{ const AnimProperty& cf = ListProperty(props, AnimCamFocus, AnimX); if (!cf.keyframes.empty()) c->focusDist = cf.Eval(frame, c->focusDist); }
		if (hasClip){ c->nearClip = ListProperty(props, AnimClip, AnimX).Eval(frame, c->nearClip);
		              c->farClip  = ListProperty(props, AnimClip, AnimY).Eval(frame, c->farClip); } }
	// LUZ: colores (diffuse/ambient/specular) + numericos (atenuacion/spot/GL/direccional)
	if (o->getType()==ObjectType::light){ Light* l=(Light*)o;
		{ const AnimProperty& ci = ListProperty(props,AnimLightIntens,AnimX); if (!ci.keyframes.empty()) l->intensity = ci.Eval(frame,l->intensity); }
		if (hasCol) { Vector3 d(l->diffuse[0],l->diffuse[1],l->diffuse[2]); Vector3 v=EvalPropVec(props,AnimColor,frame,d);    l->diffuse[0]=v.x; l->diffuse[1]=v.y; l->diffuse[2]=v.z; }
		if (hasAmb) { Vector3 d(l->ambient[0],l->ambient[1],l->ambient[2]); Vector3 v=EvalPropVec(props,AnimAmbient,frame,d);  l->ambient[0]=v.x; l->ambient[1]=v.y; l->ambient[2]=v.z; }
		if (hasSpec){ Vector3 d(l->specular[0],l->specular[1],l->specular[2]); Vector3 v=EvalPropVec(props,AnimSpecular,frame,d); l->specular[0]=v.x; l->specular[1]=v.y; l->specular[2]=v.z; }
		if (hasAtt) { l->attConstant  = ListProperty(props,AnimAtten,AnimX).Eval(frame,l->attConstant);
		              l->attLinear    = ListProperty(props,AnimAtten,AnimY).Eval(frame,l->attLinear);
		              l->attQuadratic = ListProperty(props,AnimAtten,AnimZ).Eval(frame,l->attQuadratic); }
		if (hasSpot){ l->spotCutoff   = ListProperty(props,AnimSpot,AnimX).Eval(frame,l->spotCutoff);
		              l->spotExponent = ListProperty(props,AnimSpot,AnimY).Eval(frame,l->spotExponent); }
		if (hasMisc){ float gi = ListProperty(props,AnimLightMisc,AnimX).Eval(frame,(float)(l->LightID-GL_LIGHT0));
		              int idx=(int)(gi+0.5f); if(idx<0)idx=0; if(idx>7)idx=7; l->SetLightID(GL_LIGHT0+(GLenum)idx);
		              l->directional = ListProperty(props,AnimLightMisc,AnimY).Eval(frame,l->directional?1.f:0.f) >= 0.5f; } }
}
static void ApplyCurvesList(Object* o, std::vector<AnimProperty>& props){
	W3dApplyCurvesInFrame(o, props, CurrentFrame);
}
static void ApplyCurvesOf(AnimationObject& ao){ ApplyCurvesList(ao.obj, ao.Properties); }
// MIX DE ESCENAS: vive en el Core (animation/Animation.cpp, W3dMixScenesApply) para que el juego compilado
// tambien lo tenga; el editor lo aplica aca con el frame del timeline.
static bool ApplyMixScenes(){
	extern int ActiveAnimKind;
	if (!g_animMix || ActiveAnimKind == 2) return false;   // fuera del Mix / jugando (ahi lo hace el tick del juego)
	return W3dMixScenesApply(CurrentFrame, false);
}

// LAS PISTAS DE MATERIAL de la animacion de escena activa (Animation.h: AnimationMaterial). Como las curvas de objeto:
// solo con kind 0 y solo al CAMBIAR de frame, de escena o de curvas (g_animCurvesGen): un material editado a mano entre
// frames no se pisa. Sin pistas no cuesta nada (W3dApplyMaterialTracks mira si la lista esta vacia)
static void ApplyMaterialTracksEditor(){
	if (ActiveAnimKind != 0) return;
	static int lastFrame = -999999, lastScene = -1;
	static unsigned int lastGen = 0;
	if (CurrentFrame == lastFrame && ActiveSceneAnim == lastScene && lastGen == g_animCurvesGen) return;
	lastFrame = CurrentFrame; lastScene = ActiveSceneAnim; lastGen = g_animCurvesGen;
	W3dApplyMaterialTracks(ActiveSceneAnim, (float)CurrentFrame);
}

void ApplyAnimationObjects(){
	extern int ActiveAnimKind; extern class Mesh* ActiveAnimMesh;
	// (los materiales van aparte de los objetos: tambien con el Mix de escenas, que solo mezcla transformaciones)
	ApplyMaterialTracksEditor();
	// MIX: las capas de escena mandan (sobre cualquier kind); se recalcula siempre (las capas se editan en vivo)
	if (ApplyMixScenes()) return;
	// kind 3 (una VERTEX/OBJETO animation en el timeline): aplica SOLO las curvas
	// de ESE objeto (el resto del mundo queda quieto -> "solo se ve ese objeto")
	if (ActiveAnimKind == 3){
		// solo al CAMBIAR de frame: aplicar cada render pisaria las ediciones
		// manuales (mover/rotar) entre frames
		static int lastFrame3 = -999999; static Object* lastObj3 = 0; static unsigned int lastGen3 = 0;
		if (CurrentFrame == lastFrame3 && (Object*)ActiveAnimMesh == lastObj3 && lastGen3 == g_animCurvesGen) return;
		lastFrame3 = CurrentFrame; lastObj3 = (Object*)ActiveAnimMesh; lastGen3 = g_animCurvesGen;
		if (ActiveAnimMesh){
			VertexAnimation* an = AnimActiveObject((Object*)ActiveAnimMesh);   // sus curvas PROPIAS
			if (an) ApplyCurvesList((Object*)ActiveAnimMesh, an->curves);
		}
		return;
	}
	// las curvas de objetos SON la animacion de ESCENA: solo con kind 0.
	if (ActiveAnimKind != 0) return;
	// solo al CAMBIAR de frame (aplicar cada render pisaria lo que se mueve a mano entre frames)... o
	// cuando las curvas cambiaron por fuera del cabezal: otra raiz (escena/juego/prefab), otra animacion
	// elegida, el estado base de un juego que volvio (g_animCurvesGen, ver Animation.h). Sin eso, elegir
	// un clip en el MISMO frame que la ultima pose aplicada no posaba nada.
	static int lastFrame = -999999;
	static unsigned int lastGen = 0;
	if (CurrentFrame == lastFrame && lastGen == g_animCurvesGen) return;
	lastFrame = CurrentFrame;
	lastGen = g_animCurvesGen;
	for (size_t i=0;i<AnimationObjects.size();i++){ AnimationObject& ao=AnimationObjects[i]; if (!ao.obj) continue;
		ApplyCurvesOf(ao);
	}
	W3dActiveCamApply(ActiveSceneAnim, CurrentFrame);   // pista "Camara activa": los cortes de la cinematica
}
void Remove(bool IncludeCollections){
	if (InteractionMode == ObjectMode){
		//si no hay nada seleccionado. no borra
		if (!HasObjectsSelected(IncludeCollections)){
#ifndef W3D_SYMBIAN
			std::cout << "nada seleccionado para borrar" << std::endl;
#endif
			return;
		}
		Cancel();

		if (!SceneCollection) return;
		// lo GENERADO por una instancia de prefab no se borra (su estructura es la del prefab): se suelta de la
		// seleccion con un aviso y se borra el resto
		{
			Object* generated = NULL;
			for (size_t i = ObjSelects.size(); i-- > 0; )
				if (ObjSelects[i] && W3dIsGenerated(ObjSelects[i])) { generated = ObjSelects[i]; ObjSelects[i]->select = false; ObjSelects.erase(ObjSelects.begin() + (long)i); }
			if (generated) W3dPrefabIsGeneratedNotice(generated, true);
			if (ActiveObj && W3dIsGenerated(ActiveObj)) ActiveObj = NULL;
			if (!HasObjectsSelected(IncludeCollections)) return;
		}

		// Ctrl+Z de borrar: NO libera los objetos -> los DETACHA de la escena y los guarda el comando
		// (UndoCaptureDeleted), que los re-inserta al deshacer. Reemplaza al delete real + al UndoClear.
		UndoCaptureDeleted(IncludeCollections);
		ActiveObj = NULL;
		ObjSelects.clear();
	}
}

static int gPivotSelCount = 0; // seleccionados que SUMO la recursion (divide el promedio: antes se
                               // dividia por ObjSelects.size(), que puede diferir si hay deriva)
void CalcObjectsTransformPivotPoint(Object* obj){
	if (obj->select){
		// el pivote de TRANSFORM (Median Point) usa el ORIGEN del objeto, no su
		// centro geometrico. El FOCO ('.') si usa el centro geometrico, pero por
		// otro camino (FocusCenterSelection). Son cosas distintas.
		//
		// BASE. Este pivote NO es decoracion: ApplyPivotToTransform hace
		// nw = pivot + delta*(worldPos - pivot) y lo baja a pos con SetInGlobal, o sea que
		// TERMINA EN EL .w3d. Con la posicion EFECTIVA, "rotar 0 grados" dejaba de ser la
		// identidad -el pivote estaria en un lugar y la cadena por la que se baja en otro- y
		// el desplazamiento del constraint quedaba HORNEADO en obj->pos: apretar R y mover un
		// pixel sobre un objeto con Copy Location le reescribia la pos a la de la fuente.
		// worldPos (saveRecState) es base por el mismo motivo: las dos puntas de la resta
		// tienen que estar en el MISMO espacio.
		TransformPivotPoint += obj->GetGlobalPositionBase();
		gPivotSelCount++;
	};

	for(size_t c=0; c < obj->Childrens.size(); c++){
		CalcObjectsTransformPivotPoint(obj->Childrens[c]);
	}
}

// suma/promedia el PUNTO DE FOCO (centro geometrico para mallas, origen para el
// resto) de los seleccionados. Lo usa el FOCO ('.'), que SIEMPRE mira la geometria
// -no los origenes ni el modo de pivote-.
static void CalcFocusRec(Object* obj, Vector3& sum, int& n){
	if (obj->select){ sum += obj->PointFocus(); n++; }
	for(size_t c=0; c < obj->Childrens.size(); c++) CalcFocusRec(obj->Childrens[c], sum, n);
}
Vector3 FocusCenterSelection(){
	Vector3 sum(0.0f, 0.0f, 0.0f); int n = 0;
	if (SceneCollection)
		for(size_t c=0; c < SceneCollection->Childrens.size(); c++)
			CalcFocusRec(SceneCollection->Childrens[c], sum, n);
	return n > 0 ? sum * (1.0f / (float)n) : Vector3(0.0f, 0.0f, 0.0f);
}

void SetTransformPivotPoint(){
	if (InteractionMode == ObjectMode){
		// 3D Cursor: pivote = el cursor 3D
		if (g_transformPivot == PivotCursor3D) { TransformPivotPoint = cursor3D.pos; return; }
		// Active Element: pivote = el origen del objeto activo (BASE, ver CalcObjectsTransformPivotPoint)
		if (g_transformPivot == PivotActive && ActiveObj) {
			TransformPivotPoint = ActiveObj->GetGlobalPositionBase(); return;
		}
		// Median Point (default) + Individual (el apply lo ignora): promedio de
		// los focos de los seleccionados (la Mesh aporta su centro geometrico)
		TransformPivotPoint.x = 0.0f;
		TransformPivotPoint.y = 0.0f;
		TransformPivotPoint.z = 0.0f;
		gPivotSelCount = 0;
		for(size_t c=0; c < SceneCollection->Childrens.size(); c++){
			CalcObjectsTransformPivotPoint(SceneCollection->Childrens[c]);
		}
		// dividir por lo que la recursion realmente SUMO (por obj->select): si
		// difiere de ObjSelects el promedio quedaba corrido hacia el origen
		if (gPivotSelCount == 0) return;
		TransformPivotPoint.x /= gPivotSelCount;
		TransformPivotPoint.y /= gPivotSelCount; // altura -> Y OpenGL
		TransformPivotPoint.z /= gPivotSelCount; // profundidad -> Z OpenGL
	}
}

// Función para guardar la posición actual del mouse
void SaveMousePos() {
#ifdef W3D_SYMBIAN
	lastMouseX = mouseX; // el cursor virtual HID del N95
	lastMouseY = mouseY;
	return;
#endif
	#if SDL_MAJOR_VERSION == 2
		int mx, my;                  // SDL2 usa enteros
		SDL_GetMouseState(&mx, &my); // OK
		lastMouseX = (float)mx;      // convertimos después
		lastMouseY = (float)my;
	#elif SDL_MAJOR_VERSION == 3
		float mx, my;                       // variables temporales
		SDL_GetMouseState(&mx, &my);      // SDL devuelve int
		lastMouseX = mx;           // convertimos a float
		lastMouseY = my;
	#endif
}

// PROPORTIONAL EDITING: que objetos NO seleccionados pueden venir arrastrados. Nada 2D ni colecciones, y un hijo
// de algo seleccionado no (su padre ya lo lleva).
static bool EligibleObjProportional(Object* o){
    if (!o || o->select || !o->visible || o->noTransform) return false;
    switch (o->getType()) {
        case ObjectType::scene: case ObjectType::collection: case ObjectType::ui: case ObjectType::text2d:
        case ObjectType::image2d: case ObjectType::rect2d: case ObjectType::cont2d: case ObjectType::slice9:
        case ObjectType::button2d: case ObjectType::expand2d: case ObjectType::video2d:
            return false;
        default: break;
    }
    for (Object* p = o->Parent; p; p = p->Parent) if (p->select) return false;
    return true;
}
// mide la distancia de cada objeto proporcional a lo SELECCIONADO mas cercano y le pone su peso
static void ProportionalObjectsMeasure(){
    std::vector<Vector3> sel;
    for (size_t o = 0; o < objectState.size(); o++) if (objectState[o].distProp < 0.0f) sel.push_back(objectState[o].worldPos);
    if (sel.empty()) { objectState.clear(); return; }
    for (size_t o = 0; o < objectState.size(); o++) {
        SaveState& st = objectState[o];
        if (st.distProp < 0.0f) continue;
        float best = 1e30f;
        for (size_t s = 0; s < sel.size(); s++) { const float d = (st.worldPos - sel[s]).Length(); if (d < best) best = d; }
        st.distProp = best;
        st.weight = ProportionalWeight(best, (int)o);
    }
}
void ProportionalObjectsUpdate(){
    for (size_t o = 0; o < objectState.size(); o++) {
        SaveState& st = objectState[o];
        if (st.distProp >= 0.0f) st.weight = ProportionalWeight(st.distProp, (int)o);
    }
}

// un PADRE suyo ya entra en el transform? Entonces este objeto NO se transforma por su cuenta: ya lo lleva el padre.
// (Como en Blender. Bug del duenio, 2026-11-09: con la puerta y su manija HIJA elegidas, R giraba la puerta, la manija
// la acompaniaba Y ADEMAS giraba sola: quedaba girada dos veces y el picaporte se desarmaba.)
static bool ParentInElTransform(Object* obj){
    for (Object* p = obj->Parent; p; p = p->Parent)
        if (p->select && p->visible && !p->noTransform && !W3dIsGenerated(p)) return true;
    return false;
}

// lo GENERADO por una instancia de prefab no se mueve: su transform no se guarda (el archivo solo lleva la
// instancia y sus overrides) y se perderia al guardar o al regenerar
void saveRecState(Object* obj){
    if (!obj) return;
    if (W3dIsGenerated(obj)) return;   // (lo de abajo tambien es generado)

    // PROPORTIONAL EDITING: un NO seleccionado al alcance entra al snapshot con peso por distancia (se mide al
    // terminar el recorrido, en ProportionalObjectsMeasure)
    if (g_prop.on && InteractionMode == ObjectMode && EligibleObjProportional(obj)) {
        SaveState st;
        st.obj = obj; st.pos = obj->pos; st.rot = obj->Rot(); st.rotEuler = obj->rotEuler; st.scale = obj->scale;
        st.worldPos = obj->GetGlobalPositionBase();
        st.distProp = 0.0f; st.weight = 0.0f;
        objectState.push_back(st);
    }

    // Si está seleccionado, guardar estado (un objeto SIN TRANSFORMACION -la Rutina- no se mueve: no tiene que)
    if (obj->select && obj->visible && !obj->noTransform && !ParentInElTransform(obj)) {
        SaveState NewState;
        NewState.obj = obj;
        NewState.pos = obj->pos;
		NewState.rot = obj->Rot();
		NewState.rotEuler = obj->rotEuler;   // con sus vueltas (el quaternion solo no alcanza)
        NewState.scale = obj->scale;
        // BASE: de aca sale el offset al pivote que ApplyPivotToTransform/SnapFitObjMove
        // vuelven a bajar a obj->pos (o sea, al .w3d). Ver CalcObjectsTransformPivotPoint.
        NewState.worldPos = obj->GetGlobalPositionBase(); // para rotar/escalar desde el pivot
        objectState.push_back(NewState);
    }

    // Recursión: recorrer hijos
    for (size_t i = 0; i < obj->Childrens.size(); i++) {
        saveRecState(obj->Childrens[i]);
    }
}

bool saveState(){
    if (!SceneCollection) return false;

    SaveMousePos();
    objectState.clear();

    // Recorrer todo el árbol desde la raíz
    saveRecState(SceneCollection);
    ProportionalObjectsMeasure(); // proportional editing: pesos de los no seleccionados (sin seleccion, vacia)

	if (objectState.empty()) {
		// (lo unico elegido era generado por una instancia de prefab: se avisa por que no se mueve)
		Object* gen = NULL;
		for (size_t i = 0; i < ObjSelects.size() && !gen; i++)
			if (ObjSelects[i] && ObjSelects[i]->select && W3dIsGenerated(ObjSelects[i])) gen = ObjSelects[i];
		W3dPrefabIsGeneratedNotice(gen, true);
		return false;
	}
	//std::cout << "moviendo "<< objectState.size() << " objetos" << std::endl;

    UndoTransformStart(); // captura pos/rot/escala PREVIAS (se confirma al aceptar el transform)
    SetTransformPivotPoint();
    SnapObjCapture(); // snapshot de los puntos de snap (verts + origenes) para el imantado
	return true;
}

void BeginTranslate(){
	// la UI y sus elementos 2D NO se transforman desde el 3D (la UI es orden de dibujo;
	// los textos se mueven en el Editor 2D). Borrarlos si esta permitido.
	if (ActiveObj && (ActiveObj->getType() == ObjectType::ui || ActiveObj->getType() == ObjectType::text2d ||
	                  ActiveObj->getType() == ObjectType::image2d ||
	                  ActiveObj->getType() == ObjectType::rect2d ||
	                  ActiveObj->getType() == ObjectType::cont2d ||
	                  ActiveObj->getType() == ObjectType::slice9 ||
	                  ActiveObj->getType() == ObjectType::button2d ||
	                  ActiveObj->getType() == ObjectType::expand2d ||
	                  ActiveObj->getType() == ObjectType::video2d)) return;
	if (ActiveObj && InteractionMode == ObjectMode && ActiveObj->select && state == editNavigation){
		if (!saveState()) return;
		state = translation; g_xformFirstMov = true; // primer motion en cero (no salta)
		axisSelect = ViewAxis;
		ToolbarRegisterAction(TBMove); // historial de la barra de herramientas
	}
};

// duplica los objetos seleccionados (deep copy). Implementacion NUEVA y
// compartida: reemplaza al cuerpo viejo comentado (era del modelo viejo).
#include "Instance.h"
#include "objects/Fields.h"   // W3dFieldsCopy: duplicar los objetos con sus campos en la tabla
#include "objects/Light.h"
#include "Camera.h"
#include "Empty.h"
#include "LOD.h"        // W3dDuplicateOne: copiar los umbrales del LOD
#include "Culling.h"    // W3dDuplicateOne: copiar onlyCameraActive
#include "Fog.h"        // W3dDuplicateOne: copiar la niebla
#include "ClipRegion.h" // idem ClearZ / Recorte
#include "PostProcessObj.h" // idem Postproceso
#include "Hitbox.h"  // W3dDuplicateOne: copiar tamano/centro/etiqueta/filtro del hitbox
#include "LightProbe.h"  // W3dDuplicateOne: la caja, la grilla y la luz horneada de la sonda de luz
#include "ReflectionProbe.h"  // W3dDuplicateOne: la sonda de reflejo
#include "InstancePrefab.h" // W3dDuplicateOne: otra instancia del mismo prefab (con sus overrides)
#include "ProxyW3d.h"       // W3dDuplicateOne: otro proxy del mismo elemento de la libreria
#include "io/Prefabs.h"     // ...que genera lo suyo
#include "Particles.h"  // W3dDuplicateOne: copiar la config del emisor
#include "Collection.h" // W3dDuplicateOne: copiar sortByCamera/sortOnce
#include "script/W3dScript.h" // W3dDuplicateOne: deep copy de scriptData (scripts + refs/valores)
#include <string.h>

// copia la parte COMUN de un elemento 2D (Element2D: rect, ancla, layout, overflow...)
static void CopyBase2D(Element2D* d, Element2D* s) {
    d->width  = s->width; d->height = s->height;
    d->anchor = s->anchor; d->rot2d = s->rot2d;
    d->opacity = s->opacity; d->weight = s->weight;
    d->padLeft = s->padLeft; d->padRight = s->padRight;
    d->padArr  = s->padArr; d->padBottom = s->padBottom;
    d->layoutChildren = s->layoutChildren;
    d->layoutFit    = s->layoutFit; d->layoutAlign = s->layoutAlign;
    d->distribution = s->distribution;
    d->gap = s->gap; d->padGapPx = s->padGapPx;
    d->sizeMode = s->sizeMode;
    d->cropsX = s->cropsX; d->cropsY = s->cropsY;
    d->withScroll = s->withScroll; d->scrollX = s->scrollX; d->scrollY = s->scrollY;
    d->marginLeft = s->marginLeft; d->marginRight = s->marginRight;
    d->marginTop  = s->marginTop; d->marginBottom = s->marginBottom;
    d->marginUni  = s->marginUni; d->padUni = s->padUni;
    d->expand = s->expand;
}

Object* W3dDuplicateOne(Object* src, bool linked) {
    Object* newValue = NULL;
    if (src->getType() == ObjectType::mesh) {
        Mesh* m = (Mesh*)src;
        // la edicion de una malla de un RECURSO puede estar pendiente: la copia es una malla
        // COMPLETA (caras, capas, marcas), no solo sus arrays de render
        if (!m->pendingEdit.empty()) W3dMeshBinMaterializeEdit(m);
        Mesh* d = new Mesh(src->Parent, src->pos);
        d->vertexSize = m->vertexSize;
        // POSICIONES + ESTADO DE POSE: por la UNICA puerta (W3dPosVerts, ver Mesh.h). Copiar el
        // buffer a mano dejaba la copia con el flag posedByAnim en false y LA POSE ADENTRO:
        // con el playhead fuera del cuadro base, Shift+D daba una copia que se guardaba con la
        // pose HORNEADA y sin bloques SHARP/SEAM (mientras el original salia perfecto), y
        // cualquier edicion posterior sobre ella podaba TODAS las marcas contra la pose, callado.
        { W3dPosVerts p; p.Capture(m); p.WriteIn(d); }
        if (m->normals) {
            d->normals = new GLbyte[m->vertexSize * 3];
            memcpy(d->normals, m->normals, m->vertexSize * 3);
        }
        if (m->vertexColor) {
            d->vertexColor = new GLubyte[m->vertexSize * 4];
            memcpy(d->vertexColor, m->vertexColor, m->vertexSize * 4);
        }
        if (m->uv) {
            d->uv = new GLfloat[m->vertexSize * 2];
            // con un FLIPBOOK corriendo, uv[] tiene sumado el cuadro actual (estado de la sesion):
            // la copia arranca de la BASE, y su propio player le aplica el cuadro en el proximo
            // tick. Copiar el uv[] corrido lo volvia la base de la copia y el offset se sumaba dos veces.
            const size_t nUV = (size_t)m->vertexSize * 2;
            const bool withBase = m->flipbook && m->flipApplied >= 0 && m->flipUvBase.size() == nUV;
            memcpy(d->uv, withBase ? &m->flipUvBase[0] : m->uv, sizeof(GLfloat) * nUV);
        }
        // ANIMACION UV (flipbook): la copia anima igual que el original. El CON NOMBRE (asset de la
        // escena, SceneFlipbooks) se comparte por referencia; el PROPIO (la tira "animUV") se vuelve
        // a crear con los mismos parametros que guarda el proyecto. Cada copia tiene su player
        // (mismo desfase) y su base (se captura sola en el primer tick). Antes Shift+D / Alt+D
        // devolvian una malla con la UV del cuadro actual congelada y sin animacion.
        if (m->flipbook) {
            if (!m->flipbookOwn) d->UseFlipbook(m->flipbook, m->flipPlay.offset);
            else d->SetUVAnimStrip(m->flipbook->frames, m->flipbook->fps, m->flipbook->rows > 1 ? 1 : 0,
                                  m->flipPlay.offset, m->flipbook->stripWidth, m->flipbook->stripHeight);
        }
        d->facesSize = m->facesSize;
        if (m->faces) {
            d->faces = new MeshIndex[m->facesSize];
            memcpy(d->faces, m->faces, sizeof(MeshIndex) * m->facesSize);
        }
        d->materialsGroup = m->materialsGroup; // comparte Material* (ok)
        // caras logicas, bordes y params (sino el duplicado no tiene contorno
        // ni wireframe ni se puede editar). faces3d/edges se copian tal cual.
        d->faces3d = m->faces3d;
        d->edges = m->edges;
        // MARCAS SHARP/SEAM y GEOMETRIA SUELTA. Se copian TAL CUAL, y esta bien que sea a
        // ciegas: hasta aca el duplicado es un CLON EXACTO (mismas posiciones, mismas caras),
        // asi que cada clave de posicion cae en la misma arista que en el original. El que
        // tiene que podar es quien despues se queda con un PEDAZO (Separate: ver
        // PruneMarksNoEdge en SeparateSelectionEdit). Sin esto, Shift+D devolvia una copia
        // SIN una sola costura ni borde filoso y sin ningun aviso: las marcas no existian en
        // la copia, asi que ni el escritor tenia de que quejarse.
        d->sharpEdges = m->sharpEdges;
        d->seamEdges  = m->seamEdges;
        d->looseEdges = m->looseEdges;   // bordes/verts SUELTOS (un "Add > Vertex", lo que deja un delete)
        d->looseVerts = m->looseVerts;
        // VERTEX ANIMS (deep copy REAL): cada VertexFrame es duenio de sus buffers -> copia plana
        // = doble free. Mismo criterio que armatures2d/modificadores. Antes duplicar una malla
        // animada devolvia la copia SIN animaciones.
        for (size_t i = 0; i < m->animations.size(); i++)
            if (m->animations[i]) NewActiveVertexAnimation(d, VertexAnimClone(m->animations[i], d));
        d->meshType = m->meshType;
        d->meshSize = m->meshSize; d->meshSize2 = m->meshSize2; d->meshDepth = m->meshDepth;
        d->meshVerts = m->meshVerts; d->meshVerts2 = m->meshVerts2;
        d->meshSmooth = m->meshSmooth;
        d->lockNormals = m->lockNormals;
        // buffers PRECALCULADOS: se copian tal cual (mismas posiciones) -> el
        // duplicado queda identico (mismo contorno y MISMAS vertex-normals, sin
        // recalcular). Antes posRep no se copiaba y la vertex-normal del duplicado
        // salia distinta (no agrupaba por posicion -> 1 normal por vertice suelto).
        d->posRep = m->posRep;
        d->groupedVerts = m->groupedVerts;
        d->bordersBuf = m->bordersBuf;
        d->normFaceBuf = m->normFaceBuf;
        d->normCustomBuf = m->normCustomBuf;
        d->normVertBuf = m->normVertBuf;
        d->overlayLcache = m->overlayLcache;
        // CAPAS EDITABLES (deep copy). Sin esto el duplicado perdia sus uv maps / color layers y, sobre todo, sus
        // grupos de PESOS -> el skinning no tenia a que aplicar y la malla no deformaba. Van los DOS grupos (ver
        // el bloque VertexGroup/UVGroup en Mesh.h): vertexGroups = pesos por control-point (rig 3D) y uvGroups =
        // pesos por corner (rig 2D). Copiar solo los primeros dejaba el personaje 2D duplicado SIN pesos.
        for (size_t i = 0; i < m->uvMaps.size(); i++)       d->uvMaps.push_back(new UVMap(*m->uvMaps[i]));
        d->uvExtra = m->uvExtra;   // el render de las capas UV extra (memoria propia por malla)
        for (size_t i = 0; i < m->colorLayers.size(); i++)  d->colorLayers.push_back(new ColorLayer(*m->colorLayers[i]));
        for (size_t i = 0; i < m->vertexGroups.size(); i++) d->vertexGroups.push_back(new VertexGroup(*m->vertexGroups[i]));
        for (size_t i = 0; i < m->uvGroups.size(); i++)     d->uvGroups.push_back(new UVGroup(*m->uvGroups[i]));
        d->uvActiveMap = m->uvActiveMap; d->activeColor = m->activeColor; d->activeGroup = m->activeGroup;
        d->uvActiveGroup = m->uvActiveGroup;
        d->vertCtrlPoint = m->vertCtrlPoint; // vertice-render -> control-point (skinning / weight paint)
        // ARMATURES 2D (deep copy REAL): Armature2D es NO COPIABLE a proposito (es dueno de los punteros de
        // sus clips) -> Arm2DClone clona huesos + clips, como se clonan las anims del armature 3D. Sin esto,
        // duplicar un personaje rigueado en 2D daba una copia SIN rig.
        for (size_t i = 0; i < m->armatures2d.size(); i++)  d->armatures2d.push_back(Arm2DClone(m->armatures2d[i]));
        d->armature2dActive = m->armature2dActive;
        // EL REST del skinning 2D viaja tal cual, y el uv del duplicado se RE-DERIVA de el (uv = f(uv2dRest,
        // pose), el invariante de Mesh.h). Ojo con lo que parece un detalle: arriba se copio m->uv, que es el UV
        // POSADO; sin el rest, ese UV horneado se volvia la BASE de la copia y volver a pose identidad ya no
        // devolvia el mapeo original (la pose quedaba pegada para siempre). Con el rest + Armature2DApply la
        // copia queda visualmente IDENTICA al original y ademas despozable.
        d->uv2dRest = m->uv2dRest;
        d->Armature2DApply();
        d->last2dFrame = -999999; d->last2dAnim = -999; d->pose2dDirty = true;
        // MODIFICADORES (deep copy). Modifier no tiene recursos propios y su 'target' es un puntero COMPARTIDO
        // (el mismo esqueleto / objeto espejo) -> copia plana correcta.
        for (size_t i = 0; i < m->modifiers.size(); i++) d->modifiers.push_back(new Modifier(*m->modifiers[i]));
        d->activeModifier = m->activeModifier;
        d->skinArmature = m->skinArmature; // mismo esqueleto (los buffers skin* se regeneran en el primer render)
        newValue = d;
    }
    else if (src->getType() == ObjectType::light) {
        Light* l = Light::Create(src->Parent, 0, 0, 0);
        if (l) {
            Light* sl = (Light*)src;
            for (int i = 0; i < 4; i++) {
                l->diffuse[i] = sl->diffuse[i];
                l->ambient[i] = sl->ambient[i];
                l->specular[i] = sl->specular[i];
            }
            // el resto de las propiedades editables tambien viajan (antes el
            // duplicado volvia a los defaults: direccional/atenuacion/spot)
            l->directional = sl->directional;
            l->attConstant = sl->attConstant; l->attLinear = sl->attLinear; l->attQuadratic = sl->attQuadratic;
            l->spotCutoff = sl->spotCutoff; l->spotExponent = sl->spotExponent; l->intensity = sl->intensity; l->distance = sl->distance;
            l->area = sl->area; l->areaShape = sl->areaShape; l->areaSizeX = sl->areaSizeX; l->areaSizeY = sl->areaSizeY;   // (la de area)
        }
        newValue = l;
    }
    else if (src->getType() == ObjectType::camera) {
        Camera* sc = (Camera*)src;
        Camera* d = new Camera(src->Parent, src->pos, src->rotEuler);
        // el LENTE tambien se duplica (antes fov/orto/near/far volvian al default)
        d->fov = sc->fov; d->orthographic = sc->orthographic;
        d->nearClip = sc->nearClip; d->farClip = sc->farClip;
        d->dof = sc->dof; d->fStop = sc->fStop; d->focusDist = sc->focusDist; d->focusObject = sc->focusObject;
        newValue = d;
    }
    else if (src->getType() == ObjectType::empty) {
        Empty* d = new Empty(src->Parent, src->pos);
        d->CopyDrawing((const Empty*)src);   // la forma, el tamanio y el giro del dibujo
        newValue = d;
    }
    else if (src->getType() == ObjectType::lod) {
        LOD* d = new LOD(src->Parent, src->pos);
        d->distances        = ((LOD*)src)->distances;
        d->onlyCameraActive = ((LOD*)src)->onlyCameraActive;
        newValue = d;
    }
    else if (src->getType() == ObjectType::sound) {
        Sound* d = new Sound(src->Parent, src->pos);
        d->CopyFrom((Sound*)src);
        newValue = d;
    }
    else if (src->getType() == ObjectType::fog) {
        Fog* d = new Fog(src->Parent, src->pos);
        W3dFieldsCopy(src, d);   // (sus campos: objects/Fields.cpp)
        newValue = d;
    }
    else if (src->getType() == ObjectType::postProcess) {
        PostProcess* d = new PostProcess(src->Parent, src->pos);
        d->opt = ((PostProcess*)src)->opt;
        newValue = d;
    }
    else if (src->getType() == ObjectType::clearz) {
        ClearZ* d = new ClearZ(src->Parent, src->pos);
        W3dFieldsCopy(src, d);
        newValue = d;
    }
    else if (src->getType() == ObjectType::clip) {
        ClipRegion* s = (ClipRegion*)src;
        ClipRegion* d = new ClipRegion(src->Parent, src->pos);
        d->active = s->active; d->x = s->x; d->y = s->y; d->width = s->width; d->height = s->height;
        d->camera = s->camera; d->clearZ = s->clearZ; d->background = s->background;
        for (int k = 0; k < 4; k++) d->color[k] = s->color[k];
        newValue = d;
    }
    else if (src->getType() == ObjectType::hitbox) {
        newValue = HitboxDuplicate((Hitbox*)src);   // los campos propios (la lista vive en Hitbox.cpp)
    }
    else if (src->getType() == ObjectType::curve) {
        newValue = CurveDuplicate((Curve*)src);   // sus nodos o sus splines, su forma y su material (Curve.cpp)
    }
    else if (src->getType() == ObjectType::lightProbe) {
        newValue = LightProbeDuplicate((LightProbe*)src);   // (la caja, la grilla y la luz horneada)
    }
    else if (src->getType() == ObjectType::reflectionProbe) {
        newValue = ReflectionProbeDuplicate((ReflectionProbe*)src);   // (la caja, la captura y su imagen 360)
    }
    else if (src->getType() == ObjectType::routine) {
        // RUTINA: las listas y que lista usa cada modo (los punteros se vuelven a resolver: sucia)
        Routine* s = (Routine*)src;
        Routine* d = new Routine(src->Parent, s->name);
        for (int m = 0; m < Routine::ModeN; m++) { d->lists[m] = s->lists[m]; d->use[m] = s->use[m]; }
        d->editedList = s->editedList; d->activeStep = s->activeStep; d->dirty = true;
        d->repeat = s->repeat;
        newValue = d;
    }
    else if (W3dIsInstanceType(src->getType())) {
        // otra INSTANCIA del mismo prefab con los mismos overrides (lo que genera se genera abajo, ya con su nombre).
        // Un PROXY de una libreria, otro proxy del mismo elemento
        InstancePrefab* sp = (InstancePrefab*)src;
        // (los overrides salen de comparar lo generado con su base: lo que el usuario cambio desde el ultimo
        //  cuadro -el ojo de un hijo, un valor de su script- se anota ANTES de copiarlos)
        W3dPrefabSyncOverrides(sp);
        InstancePrefab* d = (src->getType() == ObjectType::proxy) ? new ProxyW3d(src->Parent, src->pos)
                                                                   : new InstancePrefab(src->Parent, src->pos);
        d->prefab = sp->prefab;
        d->overProps = sp->overProps;
        d->overVisible = sp->overVisible;
        // (y COMO se carga: la copia de una diferida tambien es diferida, io/Streaming.h)
        d->load = sp->load; d->distance = sp->distance; d->target = sp->target;
        newValue = d;
    }
    else if (src->getType() == ObjectType::armature) {
        // ESQUELETO: los huesos con su rest, su bind y sus matrices de skin (W3dBone es un struct
        // plano: la copia es exacta y no hay que re-preparar el skinning), los flags del rig, el
        // clip activo y las capas del mix. Antes no habia rama: Shift+D no creaba nada y Alt+D
        // daba una Instance (un dibujo del original, sin pose propia ni hijos de verdad).
        // Los CLIPS: copia propia (Shift+D) o los MISMOS del original (Alt+D). El estado de JUEGO
        // (cabezal, velocidad, transicion) es runtime: la copia arranca de cero.
        Armature* sa = (Armature*)src;
        Armature* d = new Armature(src->Parent, src->pos);
        d->bones = sa->bones;
        // los constraints de HUESO que apuntaban a un hueso del ORIGINAL apuntan al de la copia (como
        // Blender): sin esto el IK de la copia seguiria al controlador del original
        for (size_t b = 0; b < d->bones.size(); b++)
            for (size_t k = 0; k < d->bones[b].constraints.list.size(); k++) {
                W3dConstraint* c = d->bones[b].constraints.list[k];
                if (c->sourceObj == (Object*)sa) { c->sourceObj = d; c->boneCache = -1; }
                if (c->poleObj == (Object*)sa)   { c->poleObj = d; c->poleBoneCache = -1; }
            }
        d->skinUsesBind = sa->skinUsesBind; d->skinRebuildFK = sa->skinRebuildFK;
        d->skinGltf = sa->skinGltf; d->skinAuthored = sa->skinAuthored;
        d->figureScale = sa->figureScale;
        if (linked) W3dArmatureAnimsLink(d, sa);
        else           W3dArmatureAnimsCopy(d, sa);
        d->activeAnim = (sa->activeAnim < (int)d->animations.size()) ? sa->activeAnim : -1;
        d->layers = sa->layers;
        d->activeLayer = sa->activeLayer;
        newValue = d;
    }
    else if (src->getType() == ObjectType::culling) {
        Culling* s = (Culling*)src;
        Culling* d = new Culling(src->Parent, src->pos);
        // TODOS los campos (incluido metodo + los de grilla): sin esto la copia recortaba distinto.
        d->method           = s->method;
        d->active           = s->active;
        d->onlyCameraActive = s->onlyCameraActive;
        d->distanceMax      = s->distanceMax;
        d->orderAlpha       = s->orderAlpha;
        d->orderProximity   = s->orderProximity;
        d->cellSize         = s->cellSize;
        d->mode3D           = s->mode3D;
        newValue = d;   // la grilla (metodo Grid) se rearma sola al primer render (gridDirty = true en el ctor)
    }
    else if (src->getType() == ObjectType::particles) {
        Particles* sp = (Particles*)src;
        Particles* d = new Particles(src->Parent, src->pos);
        d->texture = sp->texture;       d->count = sp->count;
        d->life = sp->life;             d->size = sp->size;
        d->vel = sp->vel;               d->dispersion = sp->dispersion;
        d->gravity = sp->gravity;     d->mix = sp->mix;
        for (int i = 0; i < 4; i++) d->color[i] = sp->color[i];
        d->fade = sp->fade; d->active = sp->active;
        d->filtered = sp->filtered;
        d->variation = sp->variation;   d->turbulence = sp->turbulence;
        // las particulas VIVAS no se copian: el duplicado arranca su propia emision
        newValue = d;
    }
    else if (src->getType() == ObjectType::collection && src->Parent) {
        // OJO: la raiz Scene tambien reporta tipo collection (Scene.cpp) pero no
        // tiene Parent -> el cast de aca nunca la toca.
        Collection* d = new Collection(src->Parent, src->pos);
        W3dFieldsCopy(src, d);
        newValue = d;
    }
    else if (src->getType() == ObjectType::text2d) {
        Text2D* st = (Text2D*)src;
        Text2D* d = new Text2D(src->Parent, src->pos);
        CopyBase2D(d, st);
        d->text = st->text; d->size = st->size;
        d->alignH = st->alignH; d->alignV = st->alignV;
        d->source = st->source;
        d->type = st->type; d->decimals = st->decimals;
        d->lines = st->lines; d->autoSize = st->autoSize;
        d->palColor = st->palColor;
        for (int i = 0; i < 4; i++) d->color[i] = st->color[i];
        newValue = d;
    }
    else if (src->getType() == ObjectType::image2d) {
        Image2D* yes = (Image2D*)src;
        Image2D* d = new Image2D(src->Parent, src->pos);
        CopyBase2D(d, yes);
        d->texture = yes->texture; d->mode = yes->mode;
        d->useAlpha = yes->useAlpha; d->filtered = yes->filtered;
        d->palTint = yes->palTint;
        for (int i = 0; i < 4; i++) d->color[i] = yes->color[i];
        newValue = d;
    }
    else if (src->getType() == ObjectType::rect2d) {
        Rect2D* sr = (Rect2D*)src;
        Rect2D* d = new Rect2D(src->Parent, src->pos);
        CopyBase2D(d, sr);
        d->palColor = sr->palColor;
        for (int i = 0; i < 4; i++) d->color[i] = sr->color[i];
        newValue = d;
    }
    else if (src->getType() == ObjectType::cont2d) {
        Container2D* d = new Container2D(src->Parent, src->pos);
        CopyBase2D(d, (Container2D*)src);
        newValue = d;
    }
    else if (src->getType() == ObjectType::button2d) {
        Button2D* sb = (Button2D*)src;
        Button2D* d = new Button2D(src->Parent, src->pos);
        CopyBase2D(d, sb);
        d->text = sb->text; d->icon = sb->icon; d->source = sb->source;
        d->size = sb->size; d->pad = sb->pad;
        d->palBackground = sb->palBackground; d->palText = sb->palText; d->palBorder = sb->palBorder;
        d->palHover      = sb->palHover; // (faltaba: el duplicado volvia al hover default)
        d->textureBackground = sb->textureBackground;
        d->borderTexX = sb->borderTexX; d->borderTexY = sb->borderTexY;
        d->borderScaleTex = sb->borderScaleTex;
        for (int i = 0; i < 4; i++) {
            d->backgroundColor[i] = sb->backgroundColor[i];
            d->textColor[i]       = sb->textColor[i];
            d->borderColor[i]     = sb->borderColor[i];
            d->colorHover[i]      = sb->colorHover[i]; // idem
        }
        newValue = d;
    }
    else if (src->getType() == ObjectType::video2d) {
        Video2D* sv = (Video2D*)src;
        Video2D* d = new Video2D(src->Parent, src->pos);
        CopyBase2D(d, sv);
        d->video = sv->video; d->mode = sv->mode;
        d->loop = sv->loop; d->useAlpha = sv->useAlpha;
        d->play = sv->play; d->filtered = sv->filtered;
        newValue = d;
    }
    else if (src->getType() == ObjectType::expand2d) {
        Expand2D* d = new Expand2D(src->Parent, src->pos);
        CopyBase2D(d, (Expand2D*)src);
        newValue = d;
    }
    else if (src->getType() == ObjectType::slice9) {
        Slice9* ss = (Slice9*)src;
        Slice9* d = new Slice9(src->Parent, src->pos);
        CopyBase2D(d, ss);
        d->texture = ss->texture; d->borderScale = ss->borderScale;
        d->borderX = ss->borderX; d->borderY = ss->borderY;
        d->filtered = ss->filtered; d->palTint = ss->palTint;
        for (int i = 0; i < 4; i++) d->color[i] = ss->color[i];
        newValue = d;
    }
    if (newValue) {
        newValue->pos = src->pos;
        newValue->SetRotSnapshot(src->Rot(), src->rotEuler);   // copia exacta (con las vueltas del euler)
        newValue->scale = src->scale;
        // las banderas del objeto que no son de un tipo: estatico y las sombras en tiempo real
        newValue->isStatic = src->isStatic;
        newValue->projectsShadow = src->projectsShadow; newValue->receivesShadow = src->receivesShadow;
        newValue->bouncesLight = src->bouncesLight; newValue->catchesShadows = src->catchesShadows; newValue->inProbes = src->inProbes;
        // nombre UNICO (antes pegaba ".001" a mano y duplicar dos veces daba DOS "Cubo.001")
        newValue->SetNameObj(src->name);
        // EL STACK DE CONSTRAINTS. Va en el bloque COMUN y no en cada rama: es una
        // propiedad de Object, asi que un tipo nuevo la hereda sin que su autor se
        // acuerde de nada (mismo criterio que pos/rot/scale/nombre de estas lineas).
        // La copia tiene constraints PROPIOS -otro serial- pero mira a la MISMA fuente
        // (ver W3dCopyConstraints en Objects.h): duplicar un cartel billboard da otro
        // cartel billboard, y duplicar algo que copia la posicion de "Guia" sigue
        // copiando la de "Guia".
        W3dCopyConstraints(newValue, src);
        // LOS SCRIPTS LUA (estilo Unity), tambien en el bloque COMUN: duplicar una
        // fruta con fruta.lua da otra fruta con el MISMO .lua y los MISMOS valores
        // configurados (refs/opciones/val_): despues se le cambia el val_id y listo.
        // W3dScriptData son structs de strings planos -> el copy-ctor ya es deep
        // (la copia no comparte nada con el original). El RUNTIME (las instancias
        // lua vivas) NO se copia: si el juego esta andando, sus scripts arrancan
        // cuando SimScriptsChanged / el proximo Play los cargue.
        if (src->scriptData)
            newValue->scriptData = new W3dScriptData(*src->scriptData);
        // su BIBLIOTECA DE CLIPS DE JERARQUIA: la MISMA (Shift+D y Alt+D). Los clips son datos compartidos
        // por nombre de ruta, como un material: la copia de la puerta abre con el mismo "abrir", sin copiarlo.
        W3dHierLink(newValue, src);
        // una INSTANCIA DE PREFAB genera lo suyo (con la seleccion como estaba: el que duplica la decide)
        if (W3dIsInstanceType(newValue->getType())) W3dPrefabGenerate((InstancePrefab*)newValue);
    }
    return newValue;
}

// ============================================================================
//  LA JERARQUIA DE UNA SELECCION DUPLICADA (Shift+D y Alt+D)
//
//  Cada seleccionado se copia por su cuenta y la copia nace colgada del MISMO padre que su
//  original. Si ese padre TAMBIEN se duplico, la copia se muda a la copia del padre (misma
//  posicion local, mismo orden entre hermanos): duplicar un personaje entero (esqueleto +
//  cuerpo + hitbox elegidos) da OTRO personaje, y no un esqueleto suelto con el cuerpo nuevo
//  colgado del original. Despues, toda referencia de una copia a un objeto que TAMBIEN se
//  duplico pasa a la copia de ese objeto: el esqueleto de la malla (skinArmature y el target
//  de sus modificadores) y las refs que enumera el Core (familia Target, riel, fuente de cada
//  constraint), con su vinculo por nombre. El cuerpo copiado se deforma con el esqueleto
//  copiado y el hitbox copiado avisa a su propio dueno. Lo que apunta AFUERA de la seleccion
//  queda como estaba (la copia mira al mismo objeto, como siempre).
//  Una Instance (el Alt+D de lo que no tiene copia vinculada) no es una copia de verdad: no
//  recibe hijos ni se le toca el target (dibuja a SU original).
//  'raices' = las copias que no cuelgan de otra copia (en el orden de 'copia'): las que quedan
//  elegidas para el "mover". Los hijos las siguen; elegirlos tambien los moveria dos veces.
// ============================================================================
#include <map>
#include <set>
static void W3dAttachTo(Object* obj, Object* newParent, Object* refSibling, bool after);   // (mas abajo)

static void LinkDuplicate(const std::vector<Object*>& orig, const std::vector<Object*>& copy,
                              std::vector<Object*>& roots) {
    roots.clear();
    std::map<Object*, Object*> map;   // original -> su copia DE VERDAD
    for (size_t i = 0; i < orig.size() && i < copy.size(); i++)
        if (orig[i] && copy[i] && copy[i]->getType() != ObjectType::instance) map[orig[i]] = copy[i];
    // 1) la jerarquia, en el orden de los hijos del original (sobre una FOTO de la lista: mudar
    //    una copia la saca de la lista del original)
    for (std::map<Object*, Object*>::iterator it = map.begin(); it != map.end(); ++it) {
        const std::vector<Object*> children = it->first->Childrens;
        for (size_t k = 0; k < children.size(); k++) {
            std::map<Object*, Object*>::iterator h = map.find(children[k]);
            if (h != map.end()) W3dAttachTo(h->second, it->second, NULL, false);   // misma escena: no renumera
        }
    }
    // 2) las referencias entre objetos duplicados
    for (std::map<Object*, Object*>::iterator it = map.begin(); it != map.end(); ++it) {
        Object* c = it->second;
        if (c->getType() == ObjectType::mesh) {
            Mesh* m = (Mesh*)c;
            bool touched = false;
            std::map<Object*, Object*>::iterator a = map.find((Object*)m->skinArmature);
            if (a != map.end() && a->second->getType() == ObjectType::armature) {
                m->skinArmature = (Armature*)a->second; touched = true;
            }
            for (size_t k = 0; k < m->modifiers.size(); k++) {
                Modifier* md = m->modifiers[k];
                std::map<Object*, Object*>::iterator t = md ? map.find(md->target) : map.end();
                if (t != map.end()) { md->target = t->second; touched = true; }
            }
            if (touched) {
                m->lastSkinFrame = -999999;   // re-skinnear con el esqueleto nuevo
                if (!m->modifiers.empty()) m->GenerateMeshModified();
            }
        }
        const int n = c->RefsObject();
        for (int i = 0; i < n; i++) {
            std::map<Object*, Object*>::iterator t = map.find(c->RefObject(i));
            if (t == map.end()) continue;
            c->SetRefObject(i, t->second);
            std::string* name = c->RefObjectName(i);
            if (name) *name = t->second->name;
        }
    }
    // 3) las raices
    std::set<Object*> copies;
    for (size_t i = 0; i < copy.size(); i++) if (copy[i]) copies.insert(copy[i]);
    for (size_t i = 0; i < copy.size(); i++)
        if (copy[i] && !copies.count(copy[i]->Parent)) roots.push_back(copy[i]);
}

void DuplicatedObject(){
    if (state != editNavigation || InteractionMode != ObjectMode) return;
    if (!SceneCollection) return;

    // TODOS los seleccionados estan en ObjSelects, sin importar en que
    // coleccion esten. (Antes miraba solo los hijos DIRECTOS de SceneCollection
    // y por eso no duplicaba lo que estaba dentro de una Collection -> "no
    // andaba". NewInstance ya usaba ObjSelects, por eso ese si funcionaba.)
    // Se COPIA el vector: el ctor de cada copia llama DeselectAll y vacia
    // ObjSelects, asi que no podemos iterarlo en vivo.
    std::vector<Object*> selected = ObjSelects;
    // lo GENERADO por una instancia de prefab no se duplica (la copia quedaria adentro de la instancia, que solo
    // tiene lo que su prefab genera): se duplica la instancia
    for (size_t i = selected.size(); i-- > 0; )
        if (selected[i] && W3dPrefabIsGeneratedNotice(selected[i], true)) selected.erase(selected.begin() + (long)i);
    if (selected.empty()) return;

    // copia REAL de cada uno (malla -> deep copy; luz/camara/empty -> sus
    // propiedades; NUNCA un link, eso es NewInstance/Duplicate Linked)
    std::vector<Object*> duplicate;
    for (size_t i = 0; i < selected.size(); i++) {
        // una malla que usa un RECURSO: la copia es otra malla con SU recurso (copia del
        // original, con nombre libre: "Arbol" -> "Arbol.001"), como Shift+D en Blender
        Object* s0 = selected[i];
        Object* d = (s0->getType() == ObjectType::mesh && ((Mesh*)s0)->mesh)
                    ? (Object*)W3dMeshDuplicateWithOwnResource((Mesh*)s0) : W3dDuplicateOne(s0);
        duplicate.push_back(d);   // (NULL tambien: paralelo a 'seleccionados')
    }
    // padre + hijos elegidos juntos: la copia del hijo cuelga de la copia del padre y sus
    // referencias entre si apuntan a las copias (ver LinkDuplicate)
    std::vector<Object*> roots;
    LinkDuplicate(selected, duplicate, roots);
    if (roots.empty()) return;

    // dejar seleccionadas SOLO las copias RAIZ y entrar en modo mover (como Blender:
    // Shift+D duplica y agarra; sino la copia queda justo encima y "no anda").
    // Es lo mismo que hace NewInstance, que ya funciona.
    DeselectAll();
    for (size_t i = 0; i < roots.size(); i++) roots[i]->Select();
    UndoCaptureCreation();   // Ctrl+Z saca las copias con sus hijos (y Ctrl+Y las devuelve)
    BeginTranslate();
}

#include "W3dNotice.h"   // W3dAvisof / W3dShortName: avisos con nombres SIN desbordar (declara Notificar)

// ====================== SEPARATE (Edit Mode: P / menu Mesh > Separate) ======================
// Mueve las caras SELECCIONADAS a un mesh NUEVO (misma pos/rot/escala, materiales, vertex groups, modificadores)
// y las borra del mesh actual. Como Blender (P > Selection). Seguimos editando el original. true si separo algo.
bool SeparateSelectionEdit(Mesh* m) {
    if (!m) return false;
    m->EnsureEdit();
    if (!m->edit) return false;

    // caras3d SELECCIONADAS segun el modo ACTUAL (vertice/borde/cara) ANTES de crear el clon: su ctor llama
    // DeselectAll() y en Edit Mode eso limpia la seleccion -> hay que guardarla aca. FacesSelByMode
    // deriva las caras aunque estes en modo VERTICE (una cara cuenta si todos sus verts estan seleccionados).
    std::vector<char> sel3d;
    m->FacesSelByMode(sel3d);
    int nSel = 0; for (size_t f = 0; f < sel3d.size(); f++) if (sel3d[f]) nSel++;
    if (nSel == 0)                      { Notify(T("Separate: no faces selected"), true);      return false; }
    if (nSel == (int)m->faces3d.size()) { Notify(T("Separate: select only some faces"), true); return false; }

    // 1) clon COMPLETO (geometria + transform + materiales + vertex groups + modificadores; queda parented al
    //    mismo padre). El ctor lo deja seleccionado a nivel objeto y limpia la sub-seleccion de 'm' (ver arriba).
    //    EL STACK DE CONSTRAINTS VIENE CON EL CLON, y ESE es su unico camino: W3dDuplicateOne ya
    //    lo copia en su bloque comun. Volver a llamar a W3dCopyConstraints aca dejaria el pedazo
    //    con el stack DOS VECES (dos Copy Location identicos, uno tapando al otro, y el panel con
    //    la lista duplicada). El pedazo tiene que seguir mirando a lo mismo que la malla de la que
    //    salio, que es exactamente lo que hace la copia del clon.
    Mesh* newValue = (Mesh*)W3dDuplicateOne(m);
    if (!newValue) return false;
    newValue->SetNameObj(m->name); // nombre UNICO (la puerta comun; ver W3dNames.h)

    // 2) NUEVO: quedarse SOLO con las caras seleccionadas. NO se puede con DeleteSelectionEdit (su seleccion es
    //    por POSICION: borrar las NO-seleccionadas cubre todas las esquinas del cubo y se lleva tambien las
    //    seleccionadas). Cirugia directa igual que las ops de MeshEdit: nf3d con las caras sel3d + survCorner
    //    (corners viejos, para conservar UV/color/vertex-groups) -> CompactLayers + GenerateRender (remapea y
    //    DESCARTA los verts sin referencia).
    newValue->PopulateLayers();
    { // LA CIRUGIA DEL PEDAZO VA **EN REPOSO**. El GenerateRender de abajo poda las marcas por
      // POSICION (PruneMarksNoEdge) y el clon puede nacer POSADO (el playhead fuera del
      // cuadro base). Posado, esa poda no puede decidir nada -las claves viven en el espacio de
      // reposo- y el pedazo quedaba con las 12 marcas del cubo entero, con 8 huerfanas que el
      // escritor tira con aviso en cada guardado. Con el guard, Separate da EXACTAMENTE el mismo
      // resultado con el playhead donde este. El guard ve que la topologia cambio adentro del
      // scope y deja el pedazo en reposo (que es el modelo); el playback lo vuelve a posar.
      W3dRestVertexAnim rest(newValue);
      std::vector<MeshFace> nf3d; std::vector<int> survCorner; int L = 0;
      for (size_t f = 0; f < newValue->faces3d.size(); f++) {
          int nc = (int)newValue->faces3d[f].idx.size();
          if (f < sel3d.size() && sel3d[f]) {
              nf3d.push_back(newValue->faces3d[f]);            // idx viejos; GenerateRender los remapea
              for (int c = 0; c < nc; c++) survCorner.push_back(L + c);
          }
          L += nc;
      }
      newValue->faces3d.swap(nf3d);
      // el pedazo es SOLO las caras elegidas: las lineas y los puntos sueltos que el clon trajo de la malla entera
      // no son suyos (quedaban colgando en el pedazo nuevo)
      newValue->looseEdges.clear(); newValue->looseVerts.clear();
      CompactLayers(newValue, survCorner);
      newValue->GenerateRender();
    }
    // 2b) LAS MARCAS DE LA COPIA: el clon trajo los sets ENTEROS (es lo correcto para un clon
    //     exacto) pero esta copia se quedo con un PEDAZO; las que ya no son una arista de esta
    //     parte se van solas en el ComputeBorders del GenerateRender de arriba (ver
    //     Mesh::PruneMarksNoEdge). Antes habia que podar A MANO aca, operacion por
    //     operacion, y siempre faltaba una.

    // 3) ORIGINAL: borrar las caras SELECCIONADAS (borrar el conjunto CHICO es seguro). Re-seleccionar desde
    //    sel3d por si el ctor del clon limpio la sub-seleccion en Edit Mode.
    int prevMode = EditSelectMode; EditSelectMode = SelFace; // DeleteSelectionEdit lee la seleccion del modo actual
    m->EnsureEdit();
    if (m->edit) {
        EditMesh* e = m->edit;
        e->faceSel.assign(e->faces.size(), 0);
        for (size_t f = 0; f < e->faces.size(); f++)
            if (f < e->faceSrc.size()) { int f3 = e->faceSrc[f];
                if (f3 >= 0 && f3 < (int)m->faces3d.size() && sel3d[f3]) e->faceSel[f] = 1; }
        // noRests: el pedazo se va ENTERO. Con el borrado de caras de siempre (X > Caras) sus aristas y sus vertices
        // quedaban en el original como lineas sueltas: en el Modo Objeto se seguia viendo el contorno del pedazo
        // que ya era otra malla (bug del duenio, 2026-11-09)
        m->DeleteSelectionEdit(SelFace, true);
    }
    EditSelectMode = prevMode;
    // 3b) el ORIGINAL tambien perdio aristas, y sus marcas sobre lo que se fue tambien se
    //     podan solas (DeleteSelectionEdit termina en GenerateRender -> ComputeBorders).

    ActiveObj = m; // seguimos editando el ORIGINAL (g_editMesh sigue siendo m)
    Notify(T("Separate: new mesh created"), false);
    return true;
}

// JOIN (Ctrl+J, menu Object): une las MALLAS seleccionadas DENTRO del objeto ACTIVO, que conserva su transform
// EXACTO (pos/rot/escala, aunque sea hijo-de-hijo-de-hijo). Cada objeto mergeado se lleva al espacio local del
// activo con inv(worldActivo)*worldOtro -> queda visualmente donde estaba al mergear (aunque el activo o el otro
// esten movidos/rotados/escalados/anidados). Requiere: activo = Mesh + al menos otra Mesh seleccionada; los
// objetos NO-Mesh se ignoran; sin activo no hay join. Undo ATOMICO (Ctrl+Z restaura geo + objetos en 1 paso).
void JoinObjects(){
    if (state != editNavigation || InteractionMode != ObjectMode) return;
    // el TARGET tiene que ser una malla SELECCIONADA. Si el activo no lo es (ej. quedo en el Cube por
    // defecto tras importar, fuera del armature) se hornearia todo en ese objeto extrano y se borrarian
    // las mallas del armature. Fallback: primera malla seleccionada.
    Object* active = ActiveObj;
    if (!active || active->getType() != ObjectType::mesh || !active->select) {
        active = NULL;
        for (size_t i = 0; i < ObjSelects.size(); i++) {
            Object* o = ObjSelects[i];
            if (o && o->select && o->getType() == ObjectType::mesh) { active = o; break; }
        }
    }
    if (!active) { Notify(T("Join: no active mesh object"), true); return; }
    Mesh* am = (Mesh*)active;

    std::vector<Object*> merged; // mallas seleccionadas != activo (no-Mesh se ignoran; SIN duplicados)
    for (size_t i = 0; i < ObjSelects.size(); i++) {
        Object* o = ObjSelects[i];
        if (!o || o == active || !o->select || o->getType() != ObjectType::mesh) continue; // !select: pudo quedar en ObjSelects deseleccionado
        bool dup = false; for (size_t k = 0; k < merged.size(); k++) if (merged[k] == o) { dup = true; break; } // ObjSelects puede traer
        if (!dup) merged.push_back(o); // la misma malla repetida -> sin este dedup se anexaba 2 veces (caras dobladas -> malla "invisible"/rota)
    }
    if (merged.empty()) { Notify(T("Join: select 2+ mesh objects (active is the target)"), true); return; }
    // lo GENERADO por una instancia de prefab no se une (ni como destino ni como mergeado): el borrado de los
    // mergeados no pasa por la puerta protegida de Delete y el horneado le haria unica la malla a un hijo
    // generado; nada de eso se guarda (se edita el prefab, o se desempaqueta la instancia)
    if (W3dPrefabIsGeneratedNotice(active, true)) return;
    for (size_t i = 0; i < merged.size(); i++) if (W3dPrefabIsGeneratedNotice(merged[i], true)) return;

    // BASE las dos puntas: el Join HORNEA los vertices del otro en la malla activa, asi que el
    // resultado no puede depender de la vista que se dibujo ultimo. Con la efectiva, juntar dos
    // mallas daba una geometria distinta segun donde estuviera la camara (ver Objects.h).
    Matrix4 Wa; active->GetWorldMatrixBase(Wa);   // world->local del activo (cadena de padres completa)
    Matrix4 invWa; Matrix4::InvertAffine(Wa, invWa);

    // el Join hornea geometria en la malla del OBJETO activo: si su malla es de un recurso que
    // usan otros objetos, antes se hace UNICA (los demas no cambian). Todo en un Ctrl+Z.
    UndoGroupStart();
    if (W3dMeshShared(am)) W3dMeshMakeUnique(am);
    UndoJoinStart(am); // snapshot de la geo del activo ANTES de anexar (undo atomico)

    for (size_t i = 0; i < merged.size(); i++) {
        Matrix4 Wo; merged[i]->GetWorldMatrixBase(Wo);
        Matrix4 M = invWa * Wo;               // local(otro) -> mundo -> local(activo)
        am->AppendMeshTransformed((Mesh*)merged[i], M);
    }
    // rebuild: rearma las capas UV/color desde el render concatenado y re-mergea los verts.
    // FreeLayers(false) PRESERVA los vertex groups (mergeados en AppendMeshTransformed) -> el skinning sobrevive al join.
    // GenerateRender(false) PRESERVA las normales: el join no cambia la forma de ninguna de las mallas, asi que sus
    // normales siguen valiendo. Recalcularlas promediaba y REDONDEABA los bordes filosos que traia el archivo
    // (glTF/FBX guardan splits de normal); ahora quedan como estaban.
    am->FreeLayers(false);
    am->PopulateLayers();
    am->GenerateRender(false);
    am->lastSkinFrame = -999999; // si el join era de mallas skinneadas: forzar el re-skin (el frame-gate sino lo saltaba)

    // borrar los mergeados (undo atomico): dejar select=true SOLO en ellos. Se limpia la seleccion de
    // TODA la escena (no solo ObjSelects) porque el DeleteUndo borra por el flag select recorriendo la
    // escena: una malla con select=true fuera de ObjSelects se borraria SIN haberse anexado.
    if (SceneCollection) SceneCollection->DeselectComplete(true);
    for (size_t i = 0; i < merged.size(); i++) merged[i]->select = true;
    UndoJoinConfirm(); // DeleteUndo(los select=true) empaquetado con la geo -> 1 comando
    UndoGroupSequentialEnd();   // [hacer unica, join]: Ctrl+Z al reves, Ctrl+Y al derecho

    DeselectAll(); // seleccion final: solo el activo (resultado del join)
    active->Select();
    ActiveObj = active;
    Notify(std::string(T("Joined into")) + " " + active->name, false);
}

// APPLY (Alt+A, menu Object > Apply): hornea el transform del objeto en la MALLA y resetea ese componente, dejando
// la malla VISUALMENTE en su lugar. what: 0=Location, 1=Rotation, 2=Scale, 3=All Transforms.
//   Location -> pos queda (0,0,0) (el origen va al 0; la malla queda donde estaba)
//   Rotation -> rot queda 0° (la malla queda rotada donde estaba)
//   Scale    -> scale queda 1 (una escala 1.34 pasa a 1; la malla mantiene su tamaño)
// Matematica: v_new = inv(M_reset) * M_actual * v, con M = T*R*S LOCAL del objeto -> M_reset*v_new = M_actual*v
// (la malla no se mueve en el espacio del padre). Aplica a TODAS las mallas seleccionadas. Undo atomico.
// fuerza el re-skin de TODAS las mallas de la escena que deforman con 'a': tras hornear el transform del objeto en los
// huesos, el skin de cada malla quedo calculado con la rest VIEJA. (Lo usa tambien el undo.)
void InvalidateArmatureSkin(Armature* a){
    if (!a || !SceneCollection) return;
    struct L { static void rec(Object* o, Armature* a){ if(!o) return;
        if (o->getType()==ObjectType::mesh){ Mesh* m=(Mesh*)o; if (m->skinArmature==a){ m->lastSkinFrame=-999999; } }
        for (size_t i=0;i<o->Childrens.size();i++) rec(o->Childrens[i], a); } };
    L::rec(SceneCollection, a);
    a->lastPoseFrame=-999999; a->lastPoseAnim=-999; a->poseDirty=false;
}

void ApplyTransform(int what){
    if (state != editNavigation || InteractionMode != ObjectMode) return;
    std::vector<Object*> meshes; std::vector<Armature*> arms;
    for (size_t i=0;i<ObjSelects.size();i++){ Object* o=ObjSelects[i]; if(!o) continue;
        if (o->getType()==ObjectType::mesh) meshes.push_back(o);
        else if (o->getType()==ObjectType::armature) arms.push_back((Armature*)o); }
    if (meshes.empty() && arms.empty()) { Notify(T("Apply: select mesh or armature object(s)"), true); return; }

    Quaternion idRot = Quaternion::FromEulerXYZ(0.0f,0.0f,0.0f); // identidad
    // el transform es del OBJETO: hornearlo en una malla que usan OTROS objetos (un recurso
    // compartido) los deformaria a todos. Esas se hacen UNICAS antes (como Blender, que no deja
    // aplicar sobre datos multi-usuario). Todo en un solo Ctrl+Z.
    UndoGroupStart();
    for (size_t i=0;i<meshes.size();i++) if (W3dMeshShared((Mesh*)meshes[i])) W3dMeshMakeUnique((Mesh*)meshes[i]);
    UndoApplyStart(); // snapshot de transforms + geo de mallas + rest de huesos de armatures (undo atomico)

    // MALLAS: hornear el transform en los VERTICES (v_new = inv(M_reset)*M_actual*v)
    for (size_t i=0;i<meshes.size();i++){
        Object* o = meshes[i];
        Vector3 pos2 = o->pos; Quaternion rot2 = o->Rot(); Vector3 scl2 = o->scale; // valores reseteados
        if (what==0 || what==3) pos2 = Vector3(0,0,0);
        if (what==1 || what==3) rot2 = idRot;
        if (what==2 || what==3) scl2 = Vector3(1,1,1);
        Matrix4 M  = o->BuildMatrix(o->pos, o->Rot(), o->scale); // actual (T*R*S)
        Matrix4 Mp = o->BuildMatrix(pos2,  rot2,  scl2);       // reseteado
        Matrix4 invMp; Matrix4::InvertAffine(Mp, invMp);
        Matrix4 B = invMp * M;                                 // v_new = inv(M_reset)*M_actual*v
        ((Mesh*)o)->ApplyMatrix(B);
        o->pos = pos2; o->scale = scl2; o->SetRot(rot2);       // resetear el componente en el objeto
    }
    // ARMATURES: hornear el transform del objeto en los HUESOS (rest). Como skinA no cambia -> skinMatrix'=B*skinMatrix
    // y las mallas skinneadas HIJAS quedan identicas al resetear el transform del armature (ej: normalizar un rig 100x).
    for (size_t i=0;i<arms.size();i++){
        Armature* a = arms[i];
        Vector3 pos2 = a->pos; Quaternion rot2 = a->Rot(); Vector3 scl2 = a->scale;
        if (what==0 || what==3) pos2 = Vector3(0,0,0);
        if (what==1 || what==3) rot2 = idRot;
        if (what==2 || what==3) scl2 = Vector3(1,1,1);
        Matrix4 M  = a->BuildMatrix(a->pos, a->Rot(), a->scale); // M_arm actual
        Matrix4 Mp = a->BuildMatrix(pos2,  rot2,  scl2);       // M_arm reseteado
        Matrix4 invMp; Matrix4::InvertAffine(Mp, invMp);
        Matrix4 B = invMp * M;                                 // se hornea en espacio nodo (world_FK'=B*world_FK)
        BakeTransformInBones(a, B);
        a->pos = pos2; a->scale = scl2; a->SetRot(rot2);
        InvalidateArmatureSkin(a); // re-skinnea las mallas hijas a la nueva rest
    }
    UndoApplyConfirm();
    UndoGroupSequentialEnd();   // [hacer unica, hornear]: Ctrl+Z al reves, Ctrl+Y al derecho
    const char* name = (what==0)?"Location":(what==1)?"Rotation":(what==2)?"Scale":"All Transforms";
    Notify(std::string(T("Apply")) + " " + T(name) + T(": baked"), false);
}

// Set Active Object as Camera (Ctrl+Numpad 0 / menu View > Cameras): hace que el objeto ACTIVO sea la CAMARA
// activa de la escena. Solo si el activo ES una camara (si no es camara NO se puede -> no hace nada, false).
bool SetActiveObjectAsCamera(){
    if (!ActiveObj || ActiveObj->getType() != ObjectType::camera) { Notify(T("Set Active Camera: the active object is not a camera"), true); return false; }
    CameraActive = (Camera*)ActiveObj; // Camera : public Object (1ra base) -> cast offset 0
    return true;
}

// ============================================================================
//  EMBUDO UNICO DEL REPARENT (W3dAttachTo). TODA mudanza de un objeto en el arbol
//  pasa por aca: W3dReparent (Ctrl+P / Alt+P), ReparentSimple, ReparentKeepTransform
//  y MoveNextTo -y por ellas el drag&drop y el modo mover del outliner, la tarjeta
//  Parent y el editor 2D-. Hace DOS cosas:
//   1) la cirugia de punteros: sacar del padre viejo e insertar en el nuevo (al final,
//      o junto a un hermano de referencia).
//   2) NOMBRES: el espacio de nombres de los OBJETOS es POR ESCENA (la raiz UI, ver
//      W3dScopeNameOf), asi que mudar un objeto de una escena a otra FABRICA
//      homonimos. Era el fallo F3: abrir whiskpaddle y hacer "parent BtnSonido Pelota"
//      dejaba TRES nombres repetidos en el scope 'Juego', sin un solo aviso. Se
//      re-uniquifica el SUBARBOL ENTERO que se mudo, con la puerta de siempre
//      (Object::SetNameObj -> FreeName, que se excluye a si mismo: si el nombre
//      sigue libre no cambia nada, y los sub-arboles que caen dentro de una UI anidada
//      no cambiaron de scope, asi que tampoco).
//  NO arrastra los vinculos por nombre, por el mismo motivo que la reparacion de carga:
//  el objeto se FUE de la escena donde vivian esas refs (ahi ya no resuelve a nada), y
//  en la escena nueva todavia no hay ninguna ref suya. Reescribirlas solo dejaria las
//  refs de la escena vieja apuntando a un nombre que ahi no existe.
//
//  *** UNDO: DESDE LA RONDA 14 REPARENTAR ES DESHACIBLE (Ctrl+Z deshace un Ctrl+P) ***
//  Antes NO lo era, y eso no era una comodidad que faltaba: era el HABILITADOR del fallo A
//  (ver ReparentUndo en Undo.cpp). Un comando parado en el stack de REDO guarda posiciones
//  del arbol y las replaya; mientras mover un objeto no empujara ningun paso, el redo NO se
//  vaciaba y el replay caia sobre un arbol distinto -> el mismo Object* en dos listas de
//  hijos, .w3d corrupto y SEGV al desalojarse el historial.
//  LA CAPTURA NO VA ACA, VA EN LAS CUATRO PUERTAS PUBLICAS (W3dReparent, ReparentSimple,
//  ReparentKeepTransform y MoveNextTo). Motivo: este embudo hace SOLO la cirugia de
//  punteros y el TRANSFORM lo reescriben los llamadores DESPUES (los caminos Keep
//  Transform), asi que un snapshot tomado aca guardaria coordenadas que la operacion todavia
//  va a pisar. El paso de RENUMERACION que se empuja mas abajo queda FUNDIDO con el de la
//  mudanza (grupo de undo abierto por la puerta): un solo Ctrl+Z devuelve arbol Y nombres.
// ============================================================================
static void RemoveFromParent(Object* obj);

// ============================================================================
//  UNA MUDANZA = UN PASO DE UNDO (y N mudanzas de un Ctrl+P = un paso tambien)
//
//  RAII con CONTADOR DE REENTRADA porque las puertas se llaman entre ellas: MoveNextTo
//  llama a ReparentKeepTransform y despues vuelve a W3dAttachTo. Sin el contador, la
//  llamada de adentro capturaba un "estado previo" que ya era el de la mitad de la
//  operacion (el objeto ya mudado) y el Ctrl+Z devolvia el arbol a medio camino.
//  El grupo (UndoGroupStart/Fin) junta el paso del ARBOL con el de los NOMBRES que
//  empuja el embudo al cruzar de escena.
// ============================================================================
static int g_repLevel = 0;
struct StepReparent {
    bool root, captured;
    StepReparent(Object* obj) : root(g_repLevel == 0), captured(false) {
        g_repLevel++;
        if (!root) return;                 // reentrada: la puerta de afuera ya capturo
        // W3dNamesLoading = "el que llama es dueno del paso de undo" (los loaders, que no
        // dejan historial, y el modal 'mover' del outliner, que captura al entrar y empuja UN
        // paso al confirmar). Aca no se toca NADA en ese caso: ni se captura ni se confirma,
        // porque confirmar cerraria el pendiente del modal a mitad de camino.
        if (W3dNamesLoading) return;
        captured = true;
        UndoGroupStart();
        UndoReparentStart(obj);
    }
    ~StepReparent() {
        g_repLevel--;
        if (!captured) return;
        UndoReparentConfirm();           // no pushea si el objeto quedo donde estaba
        UndoGroupEnd();
    }
};
// N mudanzas en UN SOLO Ctrl+Z (los bucles de Set/ClearParentSelection y los menus de la
// tarjeta Parent / del Editor 2D, que iteran la seleccion por su cuenta).
void ReparentGroupStart() { UndoGroupStart(); }
void ReparentGroupEnd()     { UndoGroupEnd(); }

// ============================================================================
//  EL CRUCE ENTRE LOS DOS MUNDOS (3D <-> elementos 2D de una escena UI) - FALLO B
//
//  El proyecto se guarda en DOS archivos: el .w3d (la escena) y un .w3dui POR ESCENA UI
//  (los elementos 2D, que es lo que ademas consume el runtime de los juegos). El .w3d corta
//  la recursion al llegar a un elemento 2D (ChildVaInOtherFile, SaveW3D.cpp) y el
//  .w3dui escribe SOLO tipos 2D (una lista blanca, UI2DFormat.cpp). O sea que un objeto
//  colgado del lado equivocado no lo escribe NINGUNO de los dos: desaparece al guardar, sin
//  un solo aviso. Los dos sentidos se arreglan distinto, y a proposito:
//
//   (1) OBJETO 3D bajo un ELEMENTO 2D -> SE PERMITE y viaja en el .w3d, que es el archivo
//       "maestro" (tiene el arbol entero y es el unico de los dos que sabe serializar una
//       malla/camara/luz). Se escribe bajo el nodo UI con el campo "padre2d" y al abrir se
//       re-cuelga del elemento exacto (SaveW3D.cpp / import_w3d.cpp). El .w3dui no lo
//       lleva y esta bien: el runtime de la UI no instancia objetos 3D.
//   (2) ELEMENTO 2D bajo un objeto 3D -> SE BLOQUEA, con aviso. No hay archivo que lo pueda
//       guardar: el .w3dui solo alcanza lo que cuelga de SU UI, y el .w3d no sabe escribir
//       un elemento 2D (caia en la rama generica y volvia convertido en un Empty, o sea
//       corrupcion silenciosa: el widget perdia tipo, tamano, anclas, paleta y su lugar en
//       el layout). Ademas un elemento 2D FUERA de su escena no tiene sentido en el modelo:
//       el layout, el lienzo y el espacio de nombres son POR ESCENA (W3dScopeNameOf).
//       Bloquear no es perder nada: la mudanza simplemente no ocurre y el usuario se entera.
// ============================================================================
bool UI2D_IsElement2D(Object* o);   // render/UIOverlay.h (no se incluye: es header de render)
static bool CrossingUIValid(Object* obj, Object* newParent) {
    if (!obj || !newParent) return false;
    if (!UI2D_IsElement2D(obj)) return true;   // (1) el 3D bajo un 2D si se permite
    if (newParent->getType() == ObjectType::ui || UI2D_IsElement2D(newParent)) return true;
    W3dAvisof(true, T("'%s' is a 2D element: it can only hang from its UI scene or from another "
                      "2D element (it was not moved)."), W3dShortName(obj->name).c_str());
    w3dLogfW("[ui2d] reparent bloqueado: '%s' (elemento 2D) -> '%s' (fuera de una escena UI)",
             obj->name.c_str(), newParent->name.c_str());
    return false;
}

// junta el subarbol de 'o' (incluido) en PRE-ORDEN, el mismo del recorrido por nombre
static void CollectSubtree(Object* o, std::vector<Object*>& out) {
    if (!o) return;
    out.push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) CollectSubtree(o->Childrens[i], out);
}

static void W3dAttachTo(Object* obj, Object* newParent, Object* refSibling, bool after) {
    if (!obj || !newParent) return;
    Object* oldScope = W3dScopeNameOf(obj);
    RemoveFromParent(obj);
    size_t idx = newParent->Childrens.size();
    if (refSibling)
        for (size_t i = 0; i < newParent->Childrens.size(); i++)
            if (newParent->Childrens[i] == refSibling) { idx = i + (after ? 1 : 0); break; }
    if (idx > newParent->Childrens.size()) idx = newParent->Childrens.size();
    newParent->Childrens.insert(newParent->Childrens.begin() + idx, obj);
    obj->Parent = newParent;

    // mismo espacio de nombres (reordenar entre hermanos, o mover dentro de la misma
    // escena): no hay nada que renumerar. Es el caso comun -> se sale barato.
    if (W3dScopeNameOf(obj) == oldScope) return;
    if (W3dNamesLoading) return;   // los loaders arman el arbol y reparan al final

    std::vector<Object*> sub;
    CollectSubtree(obj, sub);
    bool hasChange = false;
    for (size_t i = 0; i < sub.size() && !hasChange; i++)
        if (sub[i]->FreeName(sub[i]->name) != sub[i]->name) hasChange = true;
    if (!hasChange) return;

    // UN solo paso de undo con TODAS las puntas (aunque alguna no cambie: un
    // RenameUndo sobre un nombre igual es un no-op)
    // Object::name SI es un destino 'Directo' legitimo (el objeto no se recrea; se valida
    // que siga en la escena, ver W3dRenameDest en Undo.h)
    std::vector<W3dRenameDest> tips;
    for (size_t i = 0; i < sub.size(); i++) tips.push_back(W3dDestName(&sub[i]->name));
    UndoCaptureRenames(tips);

    int n = 0; std::string first;
    for (size_t i = 0; i < sub.size(); i++) {
        const std::string old = sub[i]->name;
        sub[i]->SetNameObj(old);          // uniquifica en el scope NUEVO
        if (sub[i]->name == old) continue;
        n++;
        // los nombres van RECORTADOS al aviso (W3dNotice.h): son texto del usuario/archivo, sin cota
        if (n <= 3) { if (!first.empty()) first += ", ";
                      first += W3dShortName(old) + " -> " + W3dShortName(sub[i]->name); }
        w3dLogfW("[nombres] reparent: '%s' -> '%s' (el nombre ya estaba tomado en la escena destino)",
                 old.c_str(), sub[i]->name.c_str());
    }
    if (n > 0)
        W3dAvisof(true, T("%d name(s) renumbered when changing scene (%s%s). Check your Lua scripts."),
                  n, first.c_str(), n > 3 ? ", ..." : "");
}

static void W3dReparent(Object* obj, Object* newParent) {
    if (!obj || !newParent || obj == newParent) return;
    Object* oldParent = obj->Parent ? obj->Parent : SceneCollection;
    if (oldParent == newParent) return;
    if (!CrossingUIValid(obj, newParent)) return;
    if (!W3dPrefabCrossingValid(obj, newParent)) return;   // (lo generado por una instancia no se muda)
    StepReparent step(obj);   // Ctrl+Z deshace esta mudanza (ver el bloque de arriba)
    // conservar la posicion GLOBAL (v1: solo traslacion). BASE: el pos que sale de aca se
    // ESCRIBE en el objeto y va al .w3d; con la efectiva, reparentar algo con un constraint
    // (o colgado de algo con constraint) le horneaba la camara adentro del pos (ver Objects.h).
    Vector3 g = obj->GetGlobalPositionBase();
    Vector3 gp = newParent->GetGlobalPositionBase();
    W3dAttachTo(obj, newParent, NULL, false);
    obj->pos = g - gp;
}

// rotacion GLOBAL por la cadena de padres (R = R_padre * R_local). NO-static:
// el transform de sub-elementos de malla (editor) la usa para world<->local.
// (RotGlobalOf se MUDO a libs/Whisk3DCore/objects/Objects.cpp: es una consulta
//  pura del grafo de escena -- la rotacion acumulada por la cadena de padres --
//  que necesitan tambien los builds SIN editor, como el runtime de un juego.)

// escala GLOBAL acumulada (componente a componente, sin shear)
Vector3 ScaleGlobalOf(Object* o) {
    Vector3 s(1.0f, 1.0f, 1.0f);
    while (o) {
        s.x *= o->scale.x;
        s.y *= o->scale.y;
        s.z *= o->scale.z;
        o = o->Parent;
    }
    return s;
}

static void RemoveFromParent(Object* obj) {
    Object* p = obj->Parent ? obj->Parent : SceneCollection;
    if (!p) return;
    for (size_t i = 0; i < p->Childrens.size(); i++) {
        if (p->Childrens[i] == obj) {
            p->Childrens.erase(p->Childrens.begin() + i);
            break;
        }
    }
}

// el nuevo padre no puede ser el mismo objeto ni un descendiente
static bool ReparentValid(Object* obj, Object* newParent) {
    if (!obj || !newParent || obj == newParent) return false;
    for (Object* q = newParent; q; q = q->Parent) {
        if (q == obj) return false;
    }
    return true;
}

// reparenta conservando la transformacion LOCAL (el objeto puede saltar)
void ReparentSimple(Object* obj, Object* newParent) {
    if (!ReparentValid(obj, newParent)) return;
    if ((obj->Parent ? obj->Parent : SceneCollection) == newParent) return;
    if (!CrossingUIValid(obj, newParent)) return;
    if (!W3dPrefabCrossingValid(obj, newParent)) return;
    StepReparent step(obj);   // Ctrl+Z deshace esta mudanza
    W3dAttachTo(obj, newParent, NULL, false);
}

// reparenta MANTENIENDO la transformacion GLOBAL: recalcula pos, rot y
// escala locales para que el objeto quede exactamente donde estaba
void ReparentKeepTransform(Object* obj, Object* newParent) {
    if (!ReparentValid(obj, newParent)) return;
    if ((obj->Parent ? obj->Parent : SceneCollection) == newParent) return;
    if (!CrossingUIValid(obj, newParent)) return;
    if (!W3dPrefabCrossingValid(obj, newParent)) return;
    // el paso de undo se abre ANTES de tocar nada: este camino reescribe pos/rot/escala
    // DESPUES de la cirugia de punteros y el snapshot tiene que ser el de antes de todo
    StepReparent step(obj);
    // SIN TRANSFORMACION (la Rutina): no hay nada que conservar, se muda y sigue en identidad
    if (obj->noTransform) { W3dAttachTo(obj, newParent, NULL, false); return; }

    // las tres componentes por el MISMO camino: RotGlobalOf/ScaleGlobalOf leen los CAMPOS (o
    // sea la base), asi que la posicion tambien tiene que ser BASE o la ida y vuelta
    // mundo<->local queda descuadrada. Y este camino ESCRIBE pos/rot/escala (van al .w3d).
    Quaternion rg = RotGlobalOf(obj);
    Vector3 sg = ScaleGlobalOf(obj);
    Vector3 pg = obj->GetGlobalPositionBase();

    Quaternion prg = RotGlobalOf(newParent);
    Vector3 psg = ScaleGlobalOf(newParent);
    Vector3 ppg = newParent->GetGlobalPositionBase();

    W3dAttachTo(obj, newParent, NULL, false);

    Quaternion ipr = prg.Inverted();
    Vector3 d = ipr * (pg - ppg); // el delta en el espacio del padre
    obj->pos = Vector3(psg.x != 0.0f ? d.x / psg.x : d.x,
                       psg.y != 0.0f ? d.y / psg.y : d.y,
                       psg.z != 0.0f ? d.z / psg.z : d.z);
    obj->SetRot(ipr * rg);
    obj->scale = Vector3(psg.x != 0.0f ? sg.x / psg.x : sg.x,
                         psg.y != 0.0f ? sg.y / psg.y : sg.y,
                         psg.z != 0.0f ? sg.z / psg.z : sg.z);
}

// reordena: pone a obj justo antes/despues de ref (mismo padre que ref),
// manteniendo la transformacion global si cambia de padre
void MoveNextTo(Object* obj, Object* ref, bool after) {
    if (!obj || !ref || obj == ref) return;
    Object* parent = ref->Parent ? ref->Parent : SceneCollection;
    if (!ReparentValid(obj, parent) && parent != (obj->Parent ? obj->Parent : SceneCollection)) return;
    if (!CrossingUIValid(obj, parent)) return;
    if (!W3dPrefabCrossingValid(obj, parent)) return;   // (ni reordenar lo generado: es la estructura del prefab)
    // REENTRADA: abajo se llama a ReparentKeepTransform (que tambien es una puerta) y despues
    // se vuelve al embudo. El contador de StepReparent hace que la captura sea UNA sola, con el
    // estado de ANTES de todo, y que el grupo se cierre recien aca.
    StepReparent step(obj);
    if ((obj->Parent ? obj->Parent : SceneCollection) != parent) {
        ReparentKeepTransform(obj, parent);
        if ((obj->Parent ? obj->Parent : SceneCollection) != parent) return;
    }
    // reubicar dentro del vector del padre (mismo padre -> mismo scope: el embudo
    // no toca nombres, solo mueve el puntero)
    W3dAttachTo(obj, parent, ref, after);
}

void SetParentSelection() {
    if (!ActiveObj || !SceneCollection) return;
    // todos los seleccionados (menos el activo) pasan a ser hijos del activo.
    // Por ObjSelects, NO solo los hijos DIRECTOS de la raiz: lo seleccionado
    // dentro de una coleccion tambien se emparenta (mismo criterio que
    // ClearParentSelection, que ya recorre en profundidad).
    std::vector<Object*> sel;
    for (size_t i = 0; i < ObjSelects.size(); i++) {
        Object* o = ObjSelects[i];
        if (!o || !o->select || o == ActiveObj) continue;
        // no emparentar a un ANCESTRO del activo (crearia un ciclo en el arbol)
        bool ancestor = false;
        for (Object* p = ActiveObj->Parent; p; p = p->Parent) if (p == o) { ancestor = true; break; }
        if (!ancestor) sel.push_back(o);
    }
    // emparentar N objetos es UNA accion del usuario -> UN solo Ctrl+Z
    ReparentGroupStart();
    for (size_t i = 0; i < sel.size(); i++) {
        W3dReparent(sel[i], ActiveObj);
    }
    ReparentGroupEnd();
}

void ClearParentSelection() {
    if (!SceneCollection) return;
    // los seleccionados vuelven a colgar de la raiz (recorrer copia DFS)
    std::vector<Object*> sel;
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {
        // buscar seleccionados anidados
        std::vector<Object*> stack;
        stack.push_back(SceneCollection->Childrens[c]);
        while (!stack.empty()) {
            Object* o = stack.back();
            stack.pop_back();
            if (o->select && o->Parent && o->Parent != SceneCollection) sel.push_back(o);
            for (size_t h = 0; h < o->Childrens.size(); h++) stack.push_back(o->Childrens[h]);
        }
    }
    ReparentGroupStart();   // desemparentar N objetos = UN solo Ctrl+Z
    for (size_t i = 0; i < sel.size(); i++) {
        W3dReparent(sel[i], SceneCollection);
    }
    ReparentGroupEnd();
}

void NewInstance(){
	if (state != editNavigation || InteractionMode != ObjectMode){return;};

	// COPIA de la seleccion ANTES de iterar: el ctor de Object pisa la seleccion
	// global (DeselectAll) -> iterar ObjSelects en vivo leia un vector ya
	// vaciado con 2+ seleccionados (mismo fix que DuplicatedObject)
    std::vector<Object*> sel = ObjSelects;
    for (size_t i = sel.size(); i-- > 0; )   // (lo generado por una instancia de prefab: ver DuplicatedObject)
        if (sel[i] && W3dPrefabIsGeneratedNotice(sel[i], true)) sel.erase(sel.begin() + (long)i);
    std::vector<Object*> origs, newValue;   // paralelos: el original y su copia (NULL = no se pudo)
    // UN Ctrl+Z: una malla SUELTA primero pasa a tener su recurso (su propio paso) y despues
    // nacen las copias; deshacer va al reves (saca las copias y la vuelve suelta)
    UndoGroupStart();
    for (int i = (int)sel.size() - 1; i >= 0; i--) {
        Object* obj = sel[i];            // objeto original
        if (!obj) continue;
        origs.push_back(obj);
        // UNA MALLA: el duplicado VINCULADO es otra malla que usa el MISMO recurso (misma
        // geometria en memoria y en la GPU; editar una edita las dos, como Alt+D en Blender)
        if (obj->getType() == ObjectType::mesh) {
            newValue.push_back(W3dMeshDuplicateLinked((Mesh*)obj));
            continue;
        }
        // UN ESQUELETO: otro armature de verdad (su pose, su cabezal, sus hijos) con los MISMOS
        // clips en memoria. UN HITBOX: no tiene nada pesado que compartir, y una Instance es solo
        // un dibujo (sin eventos): la copia es otro hitbox. Con el cuerpo y el sensor elegidos
        // junto al esqueleto, Alt+D da otro PERSONAJE que comparte malla y animaciones.
        if (obj->getType() == ObjectType::armature || obj->getType() == ObjectType::hitbox) {
            newValue.push_back(W3dDuplicateOne(obj, true));
            continue;
        }
		Instance* instance = new Instance(obj->Parent, obj);
		obj->select = false;
		instance->select = true;
		if (ActiveObj == obj) ActiveObj = instance;
		newValue.push_back(instance);
	}
    // padre + hijos elegidos juntos: la copia del hijo cuelga de la copia del padre y sus
    // referencias entre si apuntan a las copias (ver LinkDuplicate)
    std::vector<Object*> roots;
    LinkDuplicate(origs, newValue, roots);
    // quedan seleccionadas SOLO las copias RAIZ (el constructor de cada una ya pisa la seleccion)
    DeselectAll();
    for (size_t i = 0; i < roots.size(); i++) roots[i]->Select();
    if (!roots.empty()) UndoCaptureCreation();   // Ctrl+Z las saca con sus hijos (y Ctrl+Y las devuelve)
    UndoGroupSequentialEnd();
	BeginTranslate();
}

// reconstruye el quaternion REAL del objeto activo desde los valores de DISPLAY
// que se editaron, segun el modo de rotacion. La llaman los campos del editor.
void SyncActiveRotation(){
	if (!ActiveObj) return;
	if (ActiveObj->rotMode == RotQuaternion){
		ActiveObj->SetRot(ActiveObj->rotQuat);  // se editaron w/x/y/z directo en la copia de display
	} else if (ActiveObj->rotMode == RotAxisAngle){
		ActiveObj->SetRotAxisAngle(ActiveObj->rotAxis, ActiveObj->rotAngle);   // la puerta normaliza el eje
	} else { // RotEulerXYZ: el euler que se tipeo ES el dato (puede ser 405) -> el quaternion se deriva de el
		ActiveObj->SetRotEuler(ActiveObj->rotEuler);
	}
}

void SetRotation(int dx, int dy){
	float ang = (dx + dy) * 0.1f;   // sensibilidad (ajustable)
	gAngleTransform += ang;         // total acumulado (para la barra de estado)

	for (size_t o = 0; o < objectState.size(); o++) {
		Object& obj = *objectState[o].obj;
		// rotar alrededor del eje constrenido EN WORLD. El pre-multiply pelado
		// solo vale con padre identidad: para un hijo de un padre rotado hay
		// que conjugar por el padre (RotateInWorld), o el eje visible y el eje
		// aplicado no coinciden.
		Vector3 axis;
		if (axisSelect == ViewAxis || axisSelect == XYZ) axis = camForward; // libre = eje de vista
		else axis = AxisOriented(obj, axisSelect);
		obj.SetRot(RotateInWorld(obj, Quaternion::FromAxisAngle(axis, ang * objectState[o].weight))); // peso: proportional
	}
	ApplyPivotToTransform(); // gira las posiciones alrededor del pivote
	SnapFitObjRot(); // imanta: el activo apunta al target (si snap ON)
	{ extern bool g_objectsMoved; g_objectsMoved = true; } // Mirror con target depende de la rotacion/posicion
}

// rotacion ORBITAL/gimbal: gira alrededor de los ejes de la VISTA (en mundo).
// izq/der (dx) -> yaw sobre el eje vertical de la vista (camUp); arr/ab (dy) ->
// pitch sobre el horizontal (camRight). Incremental sobre obj.rot (como
// BeginRotate), con quaterniones (sin gimbal-lock). Tecla V (PC) / 0 (Symbian).
void RotateOrbital(int dx, int dy){
	float yaw   = dx * 0.1f;
	float pitch = dy * 0.1f;
	gAngleTransform += (dx + dy) * 0.1f; // total para la barra de estado
	Quaternion q = Quaternion::FromAxisAngle(camUp, yaw)      // izq/der: era -yaw (invertido)
	             * Quaternion::FromAxisAngle(camRight, pitch);
	for (size_t o = 0; o < objectState.size(); o++) {
		Object& obj = *objectState[o].obj;
		const float weight = objectState[o].weight; // proportional editing: parte del giro
		obj.SetRot(RotateInWorld(obj, (weight >= 1.0f) ? q : Quaternion::Slerp(Quaternion(), q, weight))); // gira en los ejes de la vista (conjugado por el padre)
	}
	ApplyPivotToTransform(); // gira las posiciones alrededor del pivote
	{ extern bool g_objectsMoved; g_objectsMoved = true; } // Mirror con target depende de la rotacion/posicion
}

// alterna el modo de rotacion LIBRE entre trackball (eje de vista) y ORBITAL.
void ToggleRotationOrbital(){
	if (state != rotation) return;
	axisSelect = (axisSelect == OrbitalAxis) ? ViewAxis : OrbitalAxis;
	gTrackballCap = false; // re-captura el angulo si vuelve al trackball
	RestoreState(false);
}

void BeginRotate(){
	// la UI y sus elementos 2D NO se transforman desde el 3D (la UI es orden de dibujo;
	// los textos se mueven en el Editor 2D). Borrarlos si esta permitido.
	if (ActiveObj && (ActiveObj->getType() == ObjectType::ui || ActiveObj->getType() == ObjectType::text2d ||
	                  ActiveObj->getType() == ObjectType::image2d ||
	                  ActiveObj->getType() == ObjectType::rect2d ||
	                  ActiveObj->getType() == ObjectType::cont2d ||
	                  ActiveObj->getType() == ObjectType::slice9 ||
	                  ActiveObj->getType() == ObjectType::button2d ||
	                  ActiveObj->getType() == ObjectType::expand2d ||
	                  ActiveObj->getType() == ObjectType::video2d)) return;
	//si no hay objetos. En Edit Mode NO se transforma el objeto (es para editar la malla)
	if (ActiveObj && InteractionMode == ObjectMode && ActiveObj->select && state == editNavigation){
		if (!saveState()) return;
		state = rotation; g_xformFirstMov = true; // primer motion en cero (no salta)
		valueRotation = 0;
		gAngleTransform = 0.0f; // arranca el conteo del angulo
		gTrackballCap = false;  // re-captura el angulo inicial del trackball
		// R arranca LIBRE = "rotar desde la vista" (trackball alrededor del eje
		// de camara, angulo segun el mouse al pivot). X/Y/Z constriñen a un eje.
		axisSelect = ViewAxis;
		ToolbarRegisterAction(TBRotate); // historial de la barra de herramientas
	}
};

void SetScale(int dx, int dy, float factor){
	//std::cout << "objectState size: " << objectState.size() << std::endl;
	float d0 = (dx + dy) * factor;
	for (size_t o = 0; o < objectState.size(); o++) {
		Object& obj = *objectState[o].obj;
		const float d = d0 * objectState[o].weight; // proportional editing: parte de la escala
		// enum: X->scale.x, Y->scale.z, Z->scale.y (mismo swap Y/Z del modelo)
		switch (axisSelect) {
			case X:      obj.scale.x += d; break;
			case Y:      obj.scale.z += d; break;
			case Z:      obj.scale.y += d; break;
			case PlaneX: obj.scale.z += d; obj.scale.y += d; break; // excluye X
			case PlaneY: obj.scale.x += d; obj.scale.y += d; break; // excluye Y
			case PlaneZ: obj.scale.x += d; obj.scale.z += d; break; // excluye Z
			default:     obj.scale.x += d; obj.scale.y += d; obj.scale.z += d; break; // libre
		}
	}
	ApplyPivotToTransform(); // aleja/acerca las posiciones del pivote segun el factor
	SnapFitObjScale(); // imanta: la base toca el target radialmente (si snap ON)
	{ extern bool g_objectsMoved; g_objectsMoved = true; } // Mirror con target depende de la posicion/escala
}

void SetScaleFactor(float f){
	for (size_t o = 0; o < objectState.size(); o++) {
		Object& obj = *objectState[o].obj;
		const Vector3 s0 = objectState[o].scale;
		const float k = 1.0f + (f - 1.0f) * objectState[o].weight; // proportional editing: parte del factor
		bool ex = true, ey = true, ez = true;   // (los ejes del MODELO: X->scale.x, Y->scale.z, Z->scale.y)
		switch (axisSelect) {
			case X:      ey = ez = false; break;
			case Y:      ex = ey = false; break;
			case Z:      ex = ez = false; break;
			case PlaneX: ex = false; break;
			case PlaneY: ez = false; break;
			case PlaneZ: ey = false; break;
			default: break;
		}
		obj.scale.x = ex ? s0.x * k : s0.x;
		obj.scale.y = ey ? s0.y * k : s0.y;
		obj.scale.z = ez ? s0.z * k : s0.z;
	}
	ApplyPivotToTransform();
	SnapFitObjScale();
	{ extern bool g_objectsMoved; g_objectsMoved = true; }
}

void BeginScale(){
	// la UI y sus elementos 2D NO se transforman desde el 3D (ver BeginTranslate)
	if (ActiveObj && (ActiveObj->getType() == ObjectType::ui || ActiveObj->getType() == ObjectType::text2d ||
	                  ActiveObj->getType() == ObjectType::image2d ||
	                  ActiveObj->getType() == ObjectType::rect2d ||
	                  ActiveObj->getType() == ObjectType::cont2d ||
	                  ActiveObj->getType() == ObjectType::slice9 ||
	                  ActiveObj->getType() == ObjectType::button2d ||
	                  ActiveObj->getType() == ObjectType::expand2d ||
	                  ActiveObj->getType() == ObjectType::video2d)) return;
	//XYZ tiene escala. En Edit Mode NO se transforma el objeto (es para editar la malla)
	if (ActiveObj && InteractionMode == ObjectMode && ActiveObj->select && state == editNavigation){
		if (!saveState()) return;
		state = EditScale; g_xformFirstMov = true; // primer motion en cero (no salta)
		axisSelect = XYZ;
		ToolbarRegisterAction(TBScale); // historial de la barra de herramientas
	}
};

// suma de los ejes ACTIVOS (en MUNDO, segun orientacion) para el valor numerico
static Vector3 ActiveAxesObj(Object& o){
	if (axisSelect==X||axisSelect==Y||axisSelect==Z) return AxisOriented(o, axisSelect);
	if (axisSelect==PlaneX) return AxisOriented(o,Y)+AxisOriented(o,Z);
	if (axisSelect==PlaneY) return AxisOriented(o,X)+AxisOriented(o,Z);
	if (axisSelect==PlaneZ) return AxisOriented(o,X)+AxisOriented(o,Y);
	return AxisOriented(o,X)+AxisOriented(o,Y)+AxisOriented(o,Z); // libre
}

// ENTRADA NUMERICA: aplica un valor EXACTO al transform de OBJETOS en curso, desde el
// estado guardado (snapshot). translate=distancia, rotacion=grados, escala=factor.
void SetNumericTransform(float v){
	for (size_t o = 0; o < objectState.size(); o++) {
		SaveState& st = objectState[o];
		Object& obj = *st.obj;
		if (state == translation){
			// ActiveAxesObj es MUNDO; pos vive en el espacio del padre
			obj.pos = st.pos + DeltaWorldToParent(obj, ActiveAxesObj(obj) * v);
		} else if (state == rotation){
			Vector3 ax;
			if (axisSelect==ViewAxis||axisSelect==XYZ||axisSelect==OrbitalAxis) ax = camForward;
			else ax = AxisOriented(obj, axisSelect);
			// rotacion DE MUNDO sobre el snapshot: conjugada por el padre
			// (con padre identidad es el pre-multiply de siempre)
			Quaternion pq = RotParentOf(obj);
			Quaternion q = pq.Inverted() * Quaternion::FromAxisAngle(ax, v) * pq * st.rot;
			q.normalize();
			// El euler NO se puede re-derivar acá: este camino aplica el angulo de UNA sola vez desde el snapshot
			// (tipeaste "360"), y conservar vueltas eligiendo "la forma mas parecida a la anterior" solo funciona
			// con pasos chicos: para un salto de 360 la mas parecida a 0 es 0, y la vuelta se perdia.
			// El angulo pedido (v) ES el dato: se le suma al euler del snapshot sobre el eje en el que se roto.
			// Con un eje X/Y/Z global el euler es exactamente el del snapshot + v en ese componente; en cualquier
			// otro eje (vista, local, plano) el euler no se descompone asi -> ahi se re-deriva como siempre.
			// ...y solo si el PADRE no esta rotado: con padre rotado el euler
			// local no se descompone eje a eje (el conjugado de arriba manda)
			bool axisSimple = (transformOrientation == GlobalOrient &&
			                  (axisSelect == X || axisSelect == Y || axisSelect == Z) &&
			                  fabsf(pq.w) > 0.999999f);
			if (axisSimple){
				Vector3 e = st.rotEuler;
				// AxisWorld: user X->x, Y->z (profundidad), Z->y (arriba)
				if (axisSelect == X)      e.x += v;
				else if (axisSelect == Y) e.z += v;
				else                      e.y += v;
				obj.SetRotSnapshot(q, e);     // el euler ES el dato (no se re-deriva: se comeria la vuelta)
			} else {
				obj.rotEuler = st.rotEuler;   // referencia: las vueltas que ya traia
				obj.SetRot(q);                // el quaternion manda; el euler se re-deriva cerca de esa referencia
			}
		} else { // EditScale: factor v en los ejes activos (swap Y/Z del modelo)
			Vector3 s = st.scale;
			switch (axisSelect){
				case X:      s.x*=v; break;
				case Y:      s.z*=v; break;
				case Z:      s.y*=v; break;
				case PlaneX: s.z*=v; s.y*=v; break;
				case PlaneY: s.x*=v; s.y*=v; break;
				case PlaneZ: s.x*=v; s.z*=v; break;
				default:     s.x*=v; s.y*=v; s.z*=v; break;
			}
			obj.scale = s;
		}
	}
	gAngleTransform = v;
	ApplyPivotToTransform();
}

void SetTranslationObjects(int dx, int dy, float speed){
	for (size_t o = 0; o < objectState.size(); o++) {
		Object& obj = *objectState[o].obj;
		Vector3 free = camRight * (dx * speed) + camUp * (-dy * speed); // plano camara
		const float weight = objectState[o].weight; // proportional editing: 1 = seleccionado
		// el delta se ARMA en mundo y se APLICA en el espacio del padre
		// (DeltaWorldToParent): sin eso, mover un hijo de un padre rotado iba
		// para cualquier lado (hasta al reves del mouse)
		if (axisSelect == X || axisSelect == Y || axisSelect == Z) {
			// un eje: proyectar el movimiento de PANTALLA sobre la direccion en
			// que se ve el eje (relativo a la vista). Asi arrastrar "hacia donde
			// apunta el eje en pantalla" mueve en +eje (no se invierte).
			Vector3 axis = AxisOriented(obj, axisSelect);
			float amount = (dx * axis.Dot(camRight) - dy * axis.Dot(camUp)) * speed;
			obj.pos += DeltaWorldToParent(obj, axis * (amount * weight));
		} else if (axisSelect == PlaneX || axisSelect == PlaneY || axisSelect == PlaneZ) {
			// plano: movimiento libre MENOS la componente del eje excluido
			int ex = (axisSelect == PlaneX) ? X : (axisSelect == PlaneY) ? Y : Z;
			Vector3 axis = AxisOriented(obj, ex);
			obj.pos += DeltaWorldToParent(obj, (free - axis * free.Dot(axis)) * weight);
		} else {
			obj.pos += DeltaWorldToParent(obj, free * weight); // libre (3 ejes)
		}
	}
	SnapFitObjMove(); // imanta la base de la seleccion al target (si snap ON)
	{ extern bool g_objectsMoved; g_objectsMoved = true; } // Mirror con target: su plano depende de la posicion
}

// traslacion ABSOLUTA de mundo desde el snapshot del transform (gizmo: el punto agarrado sigue al puntero, no se
// acumulan deltas por pixel). Cada objeto vuelve a su pos capturada + el delta llevado al espacio de su padre; el
// snap se re-aplica sobre eso.
void SetTranslationObjectsWorld(const Vector3& d){
	for (size_t o = 0; o < objectState.size(); o++) {
		Object& obj = *objectState[o].obj;
		obj.pos = objectState[o].pos + DeltaWorldToParent(obj, d * objectState[o].weight); // peso: proportional editing
	}
	SnapFitObjMove();
	{ extern bool g_objectsMoved; g_objectsMoved = true; }
}

// ====================================================================
// SNAP (menu shift+s, ): mueve la seleccion o el cursor 3D
// ====================================================================

// pone el origen de o en la posicion GLOBAL g (convierte a local segun el
// padre, misma matematica que ReparentKeepTransform)
static void SetInGlobal(Object* o, const Vector3& g) {
    { extern bool g_objectsMoved; g_objectsMoved = true; } // snap mueve el objeto -> Mirror con target puede cambiar
    Object* p = o->Parent;
    if (!p) { o->pos = g; return; }
    Quaternion prg = RotGlobalOf(p);
    Vector3 psg = ScaleGlobalOf(p);
    // BASE, y por dos motivos. Uno: esto ESCRIBE o->pos, que va al .w3d, asi que un padre
    // con constraint no puede quedar horneado adentro (ver Objects.h). Dos: prg/psg salen de
    // RotGlobalOf/ScaleGlobalOf, que leen los CAMPOS del padre -o sea que YA son base-, y
    // mezclarlas con una posicion efectiva deja la cuenta descuadrada consigo misma.
    // Es la misma correccion que ya tenian ReparentKeepTransform y la rama de Edit Mode de
    // Snap Selection to Cursor; esta -la rama de Object Mode del MISMO snap- se habia salteado.
    Vector3 ppg = p->GetGlobalPositionBase();
    Vector3 d = prg.Inverted() * (g - ppg);
    o->pos = Vector3(psg.x != 0.0f ? d.x / psg.x : d.x,
                     psg.y != 0.0f ? d.y / psg.y : d.y,
                     psg.z != 0.0f ? d.z / psg.z : d.z);
}

// Reposiciona los objetos seleccionados ALREDEDOR del pivote (TransformPivotPoint),
// segun cuanto rotaron/escalaron desde el estado guardado. Se llama tras cada apply
// de rotar/escalar. En modo "Individual Origins" NO hace nada (cada uno en su origen).
void ApplyPivotToTransform(){
    if (g_transformPivot == PivotIndividual) return; // cada uno rota/escala en su lugar
    if (state != rotation && state != EditScale) return; // translate no usa pivot
    Vector3 pivot = TransformPivotPoint;
    for (size_t o = 0; o < objectState.size(); o++){
        SaveState& st = objectState[o];
        Object& obj = *st.obj;
        Vector3 off = st.worldPos - pivot; // offset inicial respecto al pivote
        Vector3 nw;
        if (state == rotation){
            // rotacion acumulada desde el inicio, EN MUNDO: el delta local se
            // conjuga por el padre (antes solo era mundo para objetos top-level
            // y la orbita alrededor del pivote salia torcida en los emparentados)
            Quaternion pq = RotParentOf(obj);
            Quaternion delta = pq * (obj.Rot() * st.rot.Inverted()) * pq.Inverted();
            nw = pivot + delta * off;
        } else { // EditScale: la distancia al pivote escala con el factor (por eje)
            float fx = st.scale.x != 0.0f ? obj.scale.x / st.scale.x : 1.0f;
            float fy = st.scale.y != 0.0f ? obj.scale.y / st.scale.y : 1.0f;
            float fz = st.scale.z != 0.0f ? obj.scale.z / st.scale.z : 1.0f;
            nw = pivot + Vector3(off.x*fx, off.y*fy, off.z*fz);
        }
        SetInGlobal(&obj, nw);
    }
}

// ============================================================================
//  SNAP en MODO OBJETO (move/rotate/scale). La seleccion se trata como si TODOS sus verts estuvieran seleccionados
//  (camara/lampara/empty aportan su ORIGEN). Se snapshotean al empezar y la BASE se imanta al target bajo el cursor.
// ============================================================================
static std::vector<Vector3> g_objSnapPts; // puntos de snap (MUNDO) de la seleccion, capturados al empezar
static Vector3 g_objSnapActive;           // origen del objeto ACTIVO (aguja de la rotacion / base Active)
static bool    g_objSnapHasAct = false;

// BASE en las tres lecturas, y NO por como se mide sino por donde termina el dato: estos puntos son
// el lado IZQUIERDO de 'off = T - B', y ese off se le suma a st.worldPos (base) para bajarlo a pos
// con SetInGlobal (base). Si la seleccion que se mueve se midiera EFECTIVA, el offset del
// constraint entraria en la resta y saldria horneado en el .w3d. El target T si es efectivo: es un
// punto de MUNDO que el usuario apunto con el mouse sobre lo que se DIBUJA (ver SnapFindTarget).
static void SnapObjCapture(){
    g_objSnapPts.clear(); g_objSnapHasAct = false;
    for (size_t o=0;o<objectState.size();o++){
        Object* ob = objectState[o].obj; if (!ob) continue;
        if (objectState[o].distProp >= 0.0f) continue; // proportional: no es de la seleccion, no imanta
        bool verts=false;
        if (ob->getType()==ObjectType::mesh){
            Mesh* m=(Mesh*)ob;
            if (m->vertex && m->vertexSize>0){
                Matrix4 W; m->GetWorldMatrixBase(W);
                for (int v=0; v<m->vertexSize; v++)
                    g_objSnapPts.push_back(W * Vector3(m->vertex[v*3], m->vertex[v*3+1], m->vertex[v*3+2]));
                verts=true;
            }
        }
        if (!verts) g_objSnapPts.push_back(ob->GetGlobalPositionBase()); // sin verts (camara/lampara/empty) -> el origen
        if (ob==ActiveObj){ g_objSnapActive = ob->GetGlobalPositionBase(); g_objSnapHasAct = true; }
    }
}

// base del snap (Closest/Center/Median/Active) entre los puntos snapshot, respecto al target T.
static bool SnapObjBase(const Vector3& T, Vector3& out){
    if (g_objSnapPts.empty()) return false;
    if (g_snap.base==SNAP_ACTIVE && g_objSnapHasAct){ out=g_objSnapActive; return true; }
    if (g_snap.base==SNAP_CENTER){
        Vector3 mn=g_objSnapPts[0], mx=g_objSnapPts[0];
        for (size_t i=1;i<g_objSnapPts.size();i++){ const Vector3&w=g_objSnapPts[i];
            if(w.x<mn.x)mn.x=w.x; if(w.y<mn.y)mn.y=w.y; if(w.z<mn.z)mn.z=w.z;
            if(w.x>mx.x)mx.x=w.x; if(w.y>mx.y)mx.y=w.y; if(w.z>mx.z)mx.z=w.z; }
        out=(mn+mx)*0.5f; return true;
    }
    if (g_snap.base==SNAP_MEDIAN){
        Vector3 c(0,0,0); for (size_t i=0;i<g_objSnapPts.size();i++) c+=g_objSnapPts[i];
        out=c*(1.0f/(float)g_objSnapPts.size()); return true;
    }
    // CLOSEST (y fallback de Active sin activo): el punto mas cercano al target
    float bd=1e30f; bool any=false;
    for (size_t i=0;i<g_objSnapPts.size();i++){ Vector3 d=g_objSnapPts[i]-T; float dd=d.Dot(d); if(dd<bd){bd=dd;out=g_objSnapPts[i];any=true;} }
    return any;
}

// MOVE: desplaza toda la seleccion (desde el snapshot) para que la BASE quede en el target (o su componente de eje).
static void SnapFitObjMove(){
    { extern bool g_snapNoCursor; if (g_snapNoCursor) return; }   // (un salto del iman con las flechas)
    g_snapHit=false;
    if (!g_snap.enabled || !g_snap.afMove || !Viewport3DActive || InteractionMode!=ObjectMode || objectState.empty()) return;
    Vector3 T; float sx=0,sy=0;
    if (!SnapFindTarget(g_snapCurX,g_snapCurY,Viewport3DActive,T,sx,sy)) return;
    Vector3 B; if (!SnapObjBase(T,B)) return;
    Vector3 off = T - B;
    Object* ref = ActiveObj ? ActiveObj : objectState[0].obj;
    if (axisSelect==X||axisSelect==Y||axisSelect==Z){ Vector3 a=AxisOriented(*ref,axisSelect); off=a*off.Dot(a); }
    else if (axisSelect==PlaneX||axisSelect==PlaneY||axisSelect==PlaneZ){ int ex=(axisSelect==PlaneX)?X:(axisSelect==PlaneY)?Y:Z; Vector3 a=AxisOriented(*ref,ex); off=off-a*off.Dot(a); }
    for (size_t o=0;o<objectState.size();o++){ SaveState& st=objectState[o]; SetInGlobal(st.obj, st.worldPos + off); }
    g_snapHit=true; g_snapSx=sx; g_snapSy=sy;
}

// ROTATE: gira toda la seleccion alrededor del pivote (TransformPivotPoint) para que el objeto ACTIVO apunte al target.
void SnapFitObjRot(){
    g_snapHit=false;
    if (!g_snap.enabled || !g_snap.afRot || !Viewport3DActive || InteractionMode!=ObjectMode || objectState.empty()) return;
    if (axisSelect==OrbitalAxis || !g_objSnapHasAct) return; // orbital no tiene plano; sin activo no hay aguja
    Object* ref = ActiveObj ? ActiveObj : objectState[0].obj;
    Vector3 axis = (axisSelect==ViewAxis||axisSelect==XYZ) ? camForward : AxisOriented(*ref, axisSelect);
    { float to=sqrtf(axis.Dot(axis)); if(to<1e-6f) return; axis=axis*(1.0f/to); }
    Vector3 T; float sx=0,sy=0;
    if (!SnapFindTarget(g_snapCurX,g_snapCurY,Viewport3DActive,T,sx,sy)) return;
    const Vector3 P = TransformPivotPoint;
    Vector3 vN = g_objSnapActive - P; vN = vN - axis*vN.Dot(axis); // aguja = origen del activo (snapshot)
    Vector3 vT = T - P;               vT = vT - axis*vT.Dot(axis);
    float lN=sqrtf(vN.Dot(vN)), lT=sqrtf(vT.Dot(vT));
    if (lN<1e-5f || lT<1e-5f) return; // activo sobre el eje/pivote -> sin angulo
    vN=vN*(1.0f/lN); vT=vT*(1.0f/lT);
    float cosA=vN.Dot(vT); if(cosA>1)cosA=1; if(cosA<-1)cosA=-1;
    Vector3 cross(vN.y*vT.z-vN.z*vT.y, vN.z*vT.x-vN.x*vT.z, vN.x*vT.y-vN.y*vT.x);
    float angDeg = atan2f(cross.Dot(axis), cosA)*180.0f/3.14159265f; // absoluto -> obj.rot desde el snapshot
    for (size_t o=0;o<objectState.size();o++){ SaveState& st=objectState[o]; Object& ob=*st.obj;
        // rotacion DE MUNDO sobre el snapshot: conjugada por el padre (emparentados)
        ob.SetRot(W3dRotateWorldOver(ob, Quaternion::FromAxisAngle(axis, angDeg), st.rot)); }
    ApplyPivotToTransform();
    gAngleTransform = angDeg;
    g_snapHit=true; g_snapSx=sx; g_snapSy=sy;
}

// SCALE (uniforme): la BASE se mueve radialmente desde el pivote hasta el punto mas cercano al target. Bajo escala
// uniforme de objeto, un punto del mundo se mueve radial al pivote (el reposicionar el origen + escalar los verts
// se combinan en un puro escalado desde el pivote), asi que un factor k uniforme desde el snapshot lo resuelve.
static void SnapFitObjScale(){
    g_snapHit=false;
    if (!g_snap.enabled || !g_snap.afScale || !Viewport3DActive || InteractionMode!=ObjectMode || objectState.empty()) return;
    if (axisSelect!=XYZ && axisSelect!=ViewAxis) return; // solo escala UNIFORME (con eje bloqueado el radial no vale)
    Vector3 T; float sx=0,sy=0;
    if (!SnapFindTarget(g_snapCurX,g_snapCurY,Viewport3DActive,T,sx,sy)) return;
    Vector3 B; if (!SnapObjBase(T,B)) return;
    const Vector3 P = TransformPivotPoint;
    Vector3 d = B - P; float dd=d.Dot(d); if (dd<1e-12f) return; // base en el pivote -> la escala no la mueve
    float k = (T-P).Dot(d)/dd; if (k < 1e-4f) k = 1e-4f; // no colapsar/invertir
    for (size_t o=0;o<objectState.size();o++){ SaveState& st=objectState[o]; st.obj->scale = st.scale * k; }
    ApplyPivotToTransform();
    g_snapHit=true; g_snapSx=sx; g_snapSy=sy;
}

// redondea a la unidad de grilla mas cercana (1.0, como Blender por defecto)
static float RoundGrid(float v) {
    return (float)((int)(v + (v >= 0.0f ? 0.5f : -0.5f)));
}
static Vector3 RoundGrid(const Vector3& v) {
    return Vector3(RoundGrid(v.x), RoundGrid(v.y), RoundGrid(v.z));
}

// junta todos los seleccionados (DFS desde la raiz)
static void CollectSnap(Object* node, std::vector<Object*>& out) {
    if (!node) return;
    for (size_t i = 0; i < node->Childrens.size(); i++) {
        Object* o = node->Childrens[i];
        if (o->select) out.push_back(o);
        CollectSnap(o, out);
    }
}

// traslada RIGIDO los verts seleccionados (segun el modo vertex/edge/face) por 'delta' en coords LOCALES y
// persiste todo (render + overlay + bordes + normales + preview de mods + undo). Extraido de SnapSelectionToCursor.
void MoveSelectionEditLocal(Mesh* m, const Vector3& delta) {
    if (!m) return;
    m->EnsureEdit();
    EditMesh* e = m->edit;
    if (!e) return;
    if (delta.x == 0.0f && delta.y == 0.0f && delta.z == 0.0f) return;
    UndoCaptureMeshGeo(m); // Ctrl+Z: snapshot antes de mover
    // que VERTICES editables toca la seleccion segun el modo (vertex/edge/face)
    std::vector<char> selV(e->editVerts.size(), 0);
    if (EditSelectMode == SelVertex) {
        for (size_t k = 0; k < e->vertSel.size(); k++) if (e->vertSel[k]) selV[k] = 1;
    } else if (EditSelectMode == SelEdge) {
        for (size_t eg = 0; eg < e->edgeSel.size(); eg++) if (e->edgeSel[eg]) {
            selV[e->lineIdx[eg*2]] = 1; selV[e->lineIdx[eg*2+1]] = 1;
        }
    } else {
        for (size_t f = 0; f < e->faces.size(); f++) if (f < e->faceSel.size() && e->faceSel[f])
            for (size_t c = 0; c < e->faces[f].size(); c++) selV[e->faces[f][c]] = 1;
    }
    for (size_t k = 0; k < selV.size(); k++) { if (!selV[k]) continue;
        if (k*3+2 < e->pos.size()) { e->pos[k*3] += delta.x; e->pos[k*3+1] += delta.y; e->pos[k*3+2] += delta.z; }
    }
    // persistir: empuja al render + rearma overlay + bordes/normales + preview de modificadores (igual que el confirm de un move)
    e->PushPositions(); e->RefreshOverlay();
    m->ComputeBorders(false);
    if (!m->lockNormals) m->RecalculateNormals();
    if (!m->modifiers.empty()) m->GenerateMeshModified();
    g_redraw = true;
}

void SnapSelectionToCursor(bool keepOffset) {
    // Edit Mode: traslada RIGIDO los sub-elementos seleccionados para que su CENTRO caiga en el cursor 3d
    // (el inverso exacto de "Cursor to Selected"). Conservan su forma; solo se desplaza el grupo. En edicion
    // de malla no hay "activo" con offset propio, asi que ambas variantes hacen lo mismo (keepOffset se ignora).
    if (InteractionMode == EditMode) {
        if (!g_editMesh) return;
        Mesh* m = (Mesh*)g_editMesh;
        m->EnsureEdit();
        EditMesh* e = m->edit;
        if (!e) return;
        float cx, cy, cz;
        if (!e->SelectionCenter(cx, cy, cz)) return; // nada seleccionado
        // cursor 3d (MUNDO) -> LOCAL del mesh (inversa de LocalToWorldBase): quita origen, rotacion y escala globales.
        // BASE: esto MUEVE VERTICES de la malla, y el cursor 3D es un punto de mundo que no depende
        // de ninguna camara. Con la posicion efectiva y la rotacion base (RotGlobalOf lee los campos)
        // la cuenta ni siquiera cerraba entre si.
        Quaternion rg = RotGlobalOf(m);
        Vector3    sg = ScaleGlobalOf(m);
        Vector3    d  = rg.Inverted() * (cursor3D.pos - m->GetGlobalPositionBase());
        Vector3 cursorLocal(sg.x!=0.0f?d.x/sg.x:d.x, sg.y!=0.0f?d.y/sg.y:d.y, sg.z!=0.0f?d.z/sg.z:d.z);
        Vector3 delta(cursorLocal.x - cx, cursorLocal.y - cy, cursorLocal.z - cz);
        MoveSelectionEditLocal(m, delta); // traslada rigido la seleccion + persiste (no-op si delta=0)
        return;
    }
    std::vector<Object*> sel;
    CollectSnap(SceneCollection, sel);
    if (sel.empty()) return;
    if (keepOffset && ActiveObj) {
        // toda la seleccion se mueve para que el ACTIVO caiga en el cursor,
        // conservando los offsets relativos entre los seleccionados.
        // Capturo las globales ANTES de mover nada (si un padre y su hijo
        // estan ambos seleccionados, mover el padre cambiaria la del hijo).
        // BASE las dos: alimentan a SetInGlobal, que ESCRIBE pos. Con la efectiva, un
        // acompaniante con Copy Location a 10 y pos.x=2 terminaba en 15 en vez de en 7,
        // con el salto del constraint horneado adentro. Ver SetInGlobal y Objects.h.
        Vector3 delta = cursor3D.pos - ActiveObj->GetGlobalPositionBase();
        std::vector<Vector3> g(sel.size());
        for (size_t i = 0; i < sel.size(); i++) g[i] = sel[i]->GetGlobalPositionBase();
        for (size_t i = 0; i < sel.size(); i++) SetInGlobal(sel[i], g[i] + delta);
    } else {
        for (size_t i = 0; i < sel.size(); i++)
            SetInGlobal(sel[i], cursor3D.pos);
    }
}

void SnapSelectionToActive() {
    if (InteractionMode == EditMode) return; // edit: TODO sub-elementos (Fase 2a)
    if (!ActiveObj) return;
    // BASE: el destino se le baja a pos a cada seleccionado (SetInGlobal). Si el activo tiene
    // Copy Location a 20 con pos.x=1, los otros tienen que ir a 1 -que es lo que el activo
    // realmente ES en el archivo- y no a 20, que es donde lo esta parando su constraint.
    Vector3 destination = ActiveObj->GetGlobalPositionBase();
    std::vector<Object*> sel;
    CollectSnap(SceneCollection, sel);
    for (size_t i = 0; i < sel.size(); i++)
        if (sel[i] != ActiveObj) SetInGlobal(sel[i], destination);
}

void SnapSelectionToGrid() {
    if (InteractionMode == EditMode) return; // edit: TODO sub-elementos (Fase 2a)
    std::vector<Object*> sel;
    CollectSnap(SceneCollection, sel);
    // snapshot de globales antes de mover (padre+hijo seleccionados). BASE: lo que se redondea
    // es lo que se va a ESCRIBIR. Con la efectiva, un cubo en pos.x=0.3 con Copy Location a una
    // fuente en 10.4 terminaba con pos=(10,0,0) -el snap a grilla lo mandaba a la fuente- en vez
    // de con el (0,0,0) que le corresponde a SU 0.3.
    std::vector<Vector3> g(sel.size());
    for (size_t i = 0; i < sel.size(); i++) g[i] = sel[i]->GetGlobalPositionBase();
    for (size_t i = 0; i < sel.size(); i++)
        SetInGlobal(sel[i], RoundGrid(g[i]));
}

void SnapCursorToGrid() {
    cursor3D.pos = RoundGrid(cursor3D.pos);
}

void SnapCursorToOrigin() {
    cursor3D.pos = Vector3(0.0f, 0.0f, 0.0f);
}

// ====================================================================
// SET ORIGIN (submenu del menu Object): mueve el ORIGEN del objeto y/o la
// GEOMETRIA. Respeta pos/rot/escala/padre. Opera sobre las MALLAS seleccionadas.
// ====================================================================

// resta un offset (local) a TODOS los vertices de la malla (translacion: no toca
// normales) y recalcula bordes/centro.
//
//  DOS COSAS QUE ESTO NO HACIA Y TIENE QUE HACER:
//  (a) MOVER vertex[] a mano es exactamente el pecado que el guard W3dMoveVerts existe para
//      impedir: las claves de sharpEdges/seamEdges son los BYTES de la posicion, asi que sin
//      re-anclar las marcas quedaban vivas en memoria (se seguian dibujando) y sin punto donde
//      escribirse -> el .w3dm salia sin SHARP ni SEAM. Es el mismo bug del Apply, por la puerta
//      del menu Object > Set Origin.
//  (b) UNDO: no habia ninguno. Las tres entradas del submenu movian la geometria de forma
//      IRREVERSIBLE (Ctrl+Z no devolvia nada). El snapshot va ANTES de tocar nada; el caller
//      agrupa el paso de geometria con el del transform del objeto (ver UndoGrupo* abajo).
static void ScrollVertices(Mesh* m, const Vector3& d) {
    if (!m->vertex) return;
    // el origen es del OBJETO: una malla de un recurso que usan otros se hace UNICA antes
    // (el caller abrio el grupo de undo: todo vuelve con un Ctrl+Z)
    if (W3dMeshShared(m)) W3dMeshMakeUnique(m);
    UndoCaptureMeshGeo(m);   // Ctrl+Z: snapshot ANTES de mover (antes esto no se deshacia)
    {
        W3dMoveVerts mv(m);   // <- re-ancla sharp/seam al cerrar el scope
        for (int i = 0; i < m->vertexSize; i++) {
            m->vertex[i*3]   += d.x;
            m->vertex[i*3+1] += d.y;
            m->vertex[i*3+2] += d.z;
        }
    }
    m->ComputeBorders(); // recalcula centerGeom + edges + bordersBuf (+ invalida edit)
    W3dMeshMarkEdited(m);   // su recurso (ya unico) toma la geometria nueva
}

// (R*S)^-1 * d : pasa un vector de PARENT-local a MESH-local (deshace rot+escala
// del objeto). Sirve para que los vertices compensen un movimiento del origen.
static Vector3 ParentLocalAMeshLocal(Mesh* m, const Vector3& d) {
    Vector3 rd = m->Rot().Inverted() * d; // R^-1
    return Vector3(m->scale.x != 0.0f ? rd.x / m->scale.x : rd.x,  // S^-1
                   m->scale.y != 0.0f ? rd.y / m->scale.y : rd.y,
                   m->scale.z != 0.0f ? rd.z / m->scale.z : rd.z);
}

static void CollectMeshesSel(std::vector<Mesh*>& out) {
    std::vector<Object*> sel;
    CollectSnap(SceneCollection, sel);
    for (size_t i = 0; i < sel.size(); i++)
        if (sel[i]->getType() == ObjectType::mesh) out.push_back((Mesh*)sel[i]);
}

// 1) Geometry to Origin: la geometria se mueve para que su BARICENTRO caiga en el
//    origen del objeto. El objeto (pos/rot/escala) NO se mueve.
void SetOriginGeometryToOrigin() {
    std::vector<Mesh*> ms; CollectMeshesSel(ms);
    if (ms.empty()) return;
    UndoGroupStart();   // N mallas seleccionadas = UN SOLO Ctrl+Z (el objeto no se mueve aca)
    for (size_t i = 0; i < ms.size(); i++)
        ScrollVertices(ms[i], ms[i]->centerGeom * -1.0f); // verts -= baricentro
    UndoGroupSequentialEnd();   // (ScrollVertices puede hacer unica la malla antes de moverla)
}

// 2) Origin to Geometry: el ORIGEN se mueve al baricentro y la geometria queda EN SU
//    LUGAR. Los vertices se mueven igual que (1), pero el objeto se mueve al reves
//    (pos += R*S*baricentro) para compensar.
void SetOriginOriginToGeometry() {
    std::vector<Mesh*> ms; CollectMeshesSel(ms);
    if (ms.empty()) return;
    // ESTA entrada mueve DOS cosas (el origen del objeto Y la geometria): las dos tienen que
    // volver con UN Ctrl+Z o el paso intermedio deja la malla corrida respecto de su origen.
    UndoGroupStart();
    UndoTransformStart();   // pos/rot/escala de los seleccionados, ANTES de tocarlos
    for (size_t i = 0; i < ms.size(); i++) {
        Mesh* m = ms[i];
        Vector3 c = m->centerGeom;
        // baricentro local -> offset en parent-local (aplica escala y rotacion del obj)
        Vector3 sc(m->scale.x*c.x, m->scale.y*c.y, m->scale.z*c.z);
        m->pos += m->Rot() * sc;     // el origen va a donde estaba el baricentro
        ScrollVertices(m, c * -1.0f); // verts -= baricentro (la geometria no se mueve)
    }
    UndoTransformConfirm();
    UndoGroupSequentialEnd();   // (idem: hacer unica + mover, y despues el transform)
}

// 3) Origin to 3D Cursor: el ORIGEN se mueve al cursor 3D y la geometria queda en su
//    lugar. Lo que se movio el objeto, los vertices lo compensan al reves.
void SetOriginToCursor() {
    std::vector<Mesh*> ms; CollectMeshesSel(ms);
    if (ms.empty()) return;
    UndoGroupStart();       // origen + geometria en UN SOLO Ctrl+Z (ver SetOriginOriginToGeometry)
    UndoTransformStart();
    for (size_t i = 0; i < ms.size(); i++) {
        Mesh* m = ms[i];
        Vector3 oldPos = m->pos;
        SetInGlobal(m, cursor3D.pos);  // origen -> cursor (world->local, respeta padre)
        // los vertices compensan (oldPos - newPos) llevado a mesh-local
        ScrollVertices(m, ParentLocalAMeshLocal(m, oldPos - m->pos));
    }
    UndoTransformConfirm();
    UndoGroupSequentialEnd();   // (idem)
}

// ====================================================================
//  OLVIDAR EL ARCHIVO DE ORIGEN (menu Object > Clear Original File)
//  Ver el porque completo en ObjectMode.h. Solo borra la PROCEDENCIA
//  (Mesh::origen); la geometria vive horneada en el .w3dm y no se toca.
// ====================================================================
int ForgetOriginSelected() {
    std::vector<Mesh*> ms; CollectMeshesSel(ms);
    int n = 0; std::string last;
    for (size_t i = 0; i < ms.size(); i++) {
        if (ms[i]->origin.empty()) continue;
        last = ms[i]->origin;
        w3dLogfW("[W3D] '%s': olvido su archivo de origen '%s' (la geometria ya viaja adentro del .w3d)",
                 ms[i]->name.c_str(), last.c_str());
        ms[i]->origin.clear();
        n++;
    }
    if (n == 0) Notify(T("No original file to forget"), false);
    else if (n == 1) Notify(T("Original file forgotten") + std::string(": ") + W3dShortName(last), false);
    else Notify(T("Original file forgotten"), false);
    return n;
}

// ----------------------------------------------------------------------------
//  EL CURSOR 3D Y LOS CONSTRAINTS: el caso GENUINAMENTE AMBIGUO, y por que se
//  resuelve en BASE.
//
//  El cursor no es un campo de ningun objeto: no se guarda en el .w3d y no lo lee
//  nadie mas que el editor, asi que "sigue el dato hasta el final" no termina en el
//  cursor. Termina UN PASO DESPUES: TODOS sus consumidores ESCRIBEN
//  (Snap Selection to Cursor, Set Origin to 3D Cursor, el pivote PivotCursor3D),
//  y esos ya bajan por la cadena BASE.
//  Entonces el criterio que decide es la IDENTIDAD: "Cursor to Selected" seguido de
//  "Selection to Cursor" tiene que dejar la escena como estaba. Midiendo el cursor
//  EFECTIVO y escribiendo en BASE, ese par mueve el objeto -le mete el salto del
//  constraint en pos- que es exactamente el bug de esta tanda con otra puerta.
//  El precedente ya estaba: la rama de Edit Mode de SnapSelectionToCursor usa
//  GetGlobalPositionBase desde hace rato; medir con LocalToWorld (efectiva) y escribir
//  con LocalToWorldBase rompia la ida y vuelta ADENTRO de la misma pareja de funciones.
//  LO QUE SE ACEPTA A CAMBIO: sobre un objeto con Copy Location, "Cursor to Selected"
//  deja el cursor en el origen REAL del objeto y no donde se lo ve. Es visible pero es
//  honesto (ahi es donde el cursor puede devolverlo), y no toca ningun archivo.
// ----------------------------------------------------------------------------
void SnapCursorToSelected() {
    // Edit Mode: el cursor al CENTRO de los sub-elementos seleccionados de la malla
    // (mismo calculo que el foco; en mundo via LocalToWorldBase). Da igual vertex/edge/face.
    if (InteractionMode == EditMode && g_editMesh) {
        Mesh* m = (Mesh*)g_editMesh;
        m->EnsureEdit();
        float cx, cy, cz;
        if (m->edit && m->edit->SelectionCenter(cx, cy, cz))
            cursor3D.pos = m->LocalToWorldBase(Vector3(cx, cy, cz));
        return;
    }
    std::vector<Object*> sel;
    CollectSnap(SceneCollection, sel);
    if (sel.empty()) return;
    Vector3 sum(0.0f, 0.0f, 0.0f);
    for (size_t i = 0; i < sel.size(); i++) sum += sel[i]->GetGlobalPositionBase();
    cursor3D.pos = sum * (1.0f / (float)sel.size()); // mediana ~ promedio
}

void SnapCursorToActive() {
    // Edit Mode: por ahora el centro de la seleccion de la malla (igual que Selected).
    // TODO: el sub-elemento ACTIVO exacto (vertice/arista/cara activa) cuando este.
    if (InteractionMode == EditMode && g_editMesh) {
        Mesh* m = (Mesh*)g_editMesh;
        m->EnsureEdit();
        float cx, cy, cz;
        if (m->edit && m->edit->SelectionCenter(cx, cy, cz))
            cursor3D.pos = m->LocalToWorldBase(Vector3(cx, cy, cz));   // BASE: ver SnapCursorToSelected
        return;
    }
    if (!ActiveObj) return;
    cursor3D.pos = ActiveObj->GetGlobalPositionBase();   // BASE: ver SnapCursorToSelected
}
// ============================================================================
//  NOMBRES UNICOS de OBJETO: rename con arrastre de VINCULOS + reparacion de la
//  carga. Vive aca (editor) y no en W3dScene.cpp porque ese archivo lo compila
//  TAMBIEN el runtime del juego y esto usa Undo/Notificar (solo editor).
//
//  Los objetos se vinculan POR NOMBRE en cinco lugares y ninguno se actualizaba
//  al renombrar (bug VIVO hoy, independiente de los duplicados):
//    (a) refs de los scripts lua  -> W3dScriptEntry::refs (texto en el .w3dui)
//    (b) targetName de Mirror/Instance/ObjectScript/Camara + la fuente de cada constraint
//    (c) RailName de la camara
//    (d) escenaInicial del proyecto (si el renombrado es una raiz UI)
//    (e) Object::paleta (eso lo maneja W3dPaletteRename, no esto)
//  Todo se arrastra en UN SOLO paso de undo: con pasos separados, el Ctrl+Z
//  intermedio deja el vinculo roto (misma leccion que Bone2DRenameUndo).
// ============================================================================
#include "script/W3dScript.h"   // W3dScriptData / W3dScriptEntry::refs
#include "W3dScene.h"           // W3dSceneRootOf / escena inicial
#include "objects/Target.h"     // targetName cacheado
#include "objects/Mirror.h"
#include "objects/Gamepad.h"
#include "w3dlog.h"
#include "objects/UI.h"
#include "W3dNames.h"
#include "W3dPalettes.h"           // paletas + colores de paleta (globales de proyecto)
#include "objects/Materials.h"     // Materials (global de proyecto)
#include "animation/Animation.h"   // SceneAnimations (global de proyecto)
#include <map>
#include <set>

// junta los std::string* que apuntan al nombre 'viejo' y que hay que arrastrar.
// 'escena' = la raiz del scope (las refs de script se resuelven POR ESCENA, ver
// SimGame.cpp; arrastrar mas alla renombraria refs de otra escena que apuntan
// legitimamente a OTRO objeto homonimo).
// Las refs viven POR VALOR en un vector<pair> editable desde el IDE: se guardan como
// (objeto, script, ref) y NO como std::string*, que se cuelga en cuanto se agrega o
// borra una ref/script (ver W3dRenameDest en Undo.h).
//
// SOLO las que RESUELVEN a 'objetivo' (el que se renombra) desde donde estan, como las resuelve el Play
// (W3dSceneFindRef: adentro de una instancia de prefab, primero lo de SU instancia). Los nombres son unicos
// POR SCOPE: un objeto de la escena puede llamarse igual que uno generado por una instancia ("Cuerpo"), y
// arrastrar por nombre pelado reescribia la ref del script generado que apuntaba a SU propio "Cuerpo" (y eso
// quedaba como override de la instancia, guardado). NULL = todas las que dicen 'viejo' (sin objetivo).
static void CollectScriptRefs(Object* node, const std::string& old, Object* target, std::vector<W3dRenameDest>& out) {
    if (!node) return;
    if (node->scriptData)
        for (size_t s = 0; s < node->scriptData->scripts.size(); s++) {
            W3dScriptEntry& e = node->scriptData->scripts[s];
            for (size_t r = 0; r < e.refs.size(); r++)
                if (e.refs[r].second == old && (!target || W3dSceneFindRef(node, old) == target))
                    out.push_back(W3dDestRefLua(node, (int)s, (int)r));
        }
    for (size_t i = 0; i < node->Childrens.size(); i++)
        CollectScriptRefs(node->Childrens[i], old, target, out);
}
// ---------------------------------------------------------------------------
//  TODA punta de vinculo POR NOMBRE que nombra a 'viejo'. Se recorre el arbol
//  ENTERO: un objeto puede vivir en otra escena y apuntar igual.
//
//  ANTES ERA UN switch POR TIPO (mirror/instance/gamepad/camara + el riel
//  aparte). Ese switch es una LISTA DE CLASES mantenida a mano, y la clase
//  que se olvide de agregar ahi queda con el vinculo roto en silencio al
//  renombrar la punta. Ahora entra por la puerta UNICA de refs (Objects.h):
//  RefsObject enumera las refs propias de la clase MAS una por constraint, y
//  RefObjectName da el std::string* que las acompania. Con eso:
//   - las cinco de siempre siguen cubiertas (cada una las declara con
//     OwnRefs/RefOwnName, incluido el riel de la camara, que es su ref 1),
//   - los CONSTRAINTS quedan cubiertos GRATIS: renombrar la fuente re-liga el
//     'sourceName' de todos los constraints que la nombraban, y
//   - una clase nueva que guarde un Object* lo hereda sin tocar este archivo.
// ---------------------------------------------------------------------------
// Igual que las refs de script: SOLO las puntas que llevan a 'objetivo' (su puntero, o -sin puntero- lo que
// encuentra su nombre desde ese objeto, por scope). Una punta que apunta a un HOMONIMO de otro scope (lo generado
// por una instancia) no se toca.
static void CollectTargetsByName(Object* node, const std::string& old, Object* target, std::vector<W3dRenameDest>& out) {
    if (!node) return;
    const int n = node->RefsObject();
    for (int i = 0; i < n; i++) {
        // los nombres de las refs son MIEMBROS del objeto (targetName / RailName) o del
        // constraint, que el objeto tiene vivo: puntero directo, como antes
        std::string* p = node->RefObjectName(i);
        if (!p || *p != old) continue;
        if (target) {
            Object* t = node->RefObject(i);
            if (t ? (t != target) : (W3dFindNameFrom(node, old) != target)) continue;
        }
        out.push_back(W3dDestName(p));
    }
    for (size_t i = 0; i < node->Childrens.size(); i++)
        CollectTargetsByName(node->Childrens[i], old, target, out);
}

int W3dNamesCountRefs(Object* o, const std::string& old) {
    std::vector<W3dRenameDest> v;
    Object* esc = W3dSceneRootOf(o);
    CollectScriptRefs(esc ? esc : SceneCollection, old, NULL, v);
    return (int)v.size();
}

std::string W3dRenameObject(Object* o, const std::string& request, bool notify) {
    if (!o) return std::string();
    // lo GENERADO por una instancia de prefab se llama como en su prefab (las rutas de sus clips, sus refs y sus
    // overrides van por esos nombres): se renombra en el prefab
    if (W3dIsGenerated(o)) { W3dPrefabIsGeneratedNotice(o, notify); return o->name; }
    const std::string old      = o->name;
    const std::string newValue = o->FreeName(request);
    if (newValue == old) return old;

    // TODAS las puntas del vinculo, en UN solo paso de undo (con IDENTIDAD ESTABLE: las refs
    // de lua viven dentro de un vector por valor -> ver W3dRenameDest en Undo.h)
    std::vector<W3dRenameDest> aRename;
    aRename.push_back(W3dDestName(&o->name));
    Object* esc = W3dSceneRootOf(o);
    int nrefs0 = (int)aRename.size();
    CollectScriptRefs(esc ? esc : SceneCollection, old, o, aRename);
    const int nrefs = (int)aRename.size() - nrefs0;
    int ntgt0 = (int)aRename.size();
    CollectTargetsByName(SceneCollection, old, o, aRename);
    const int ntgt = (int)aRename.size() - ntgt0;
    // escena INICIAL del proyecto (el nombre de la escena es la clave)
    const bool isSceneInitial = (o->getType() == ObjectType::ui && W3dInitialScene() == old);

    UndoCaptureRenames(aRename);
    for (size_t k = 0; k < aRename.size(); k++)
        if (std::string* p = W3dDestResolve(aRename[k])) *p = newValue;
    if (isSceneInitial) W3dSceneSetInitial(newValue);

    if (notify) {
        const std::string norm = W3dNameNormalize(request, "Object");
        if (newValue != norm) W3dNoticeAlreadyExists(norm, newValue);
        if (nrefs > 0 || ntgt > 0)
            W3dAvisof(false, T("%d script reference(s) and %d target(s) updated"), nrefs, ntgt);
        if (o->getType() == ObjectType::ui)
            W3dAvisof(true, T("scene '%s' -> '%s': check the cambiarEscena(\"%s\") calls in your .lua files"),
                      W3dShortName(old).c_str(), W3dShortName(newValue).c_str(),
                      W3dShortName(old).c_str());
    }
    return newValue;
}

// ---------------------------------------------------------------------------
//  REPARACION al ABRIR un proyecto (o al MERGEAR un .w3dui): los archivos ya
//  guardados TIENEN duplicados (el bug de models/Cubo_0.glb lo prueba). Se
//  recorre el arbol en el mismo orden que FindObjectByName (depth-first) y el
//  PRIMERO se queda con el nombre pelado, para que las refs y los targets que
//  hoy resuelven al primero sigan resolviendo al MISMO objeto.
//
//  *** NO SE ARRASTRA NINGUN VINCULO ***  (esto es lo contrario del rename
//  MANUAL, que si los arrastra). Motivo, y es la clave de todo el bloque:
//  FindObjectByName / FindByName devuelven el PRIMERO del recorrido, o sea
//  justo el que CONSERVA el nombre. Toda camara, constraint, mirror, instance,
//  riel y ref de lua que decia "Pieza" ya resolvia al primer "Pieza" y va a
//  seguir resolviendo EXACTAMENTE al mismo objeto. Reescribir esos vinculos al
//  nombre nuevo los mudaba en silencio al SEGUNDO homonimo (bug F1: un .w3d con
//  camara target "Pieza" + constraint target "Pieza" + dos luces "Pieza" salia
//  con los DOS targets apuntando a "Pieza.001").
//  El unico renombrado que se ve es el del objeto duplicado, que hasta ahora NO
//  era alcanzable por nombre de todas formas (el primero se lo comia).
//
//  MISMO CRITERIO para el resto de los espacios (materiales, vertex groups, uv
//  groups/maps, color layers, mesh parts, modificadores, huesos 3D/2D, clips,
//  armatures 2D, paletas y colores de paleta, animaciones de escena): se recorre
//  cada espacio en SU ORDEN DE BUSQUEDA y el primero se queda con el nombre, asi
//  que los bindings POR NOMBRE (hueso 3D <-> vertex group, hueso 2D <-> UV group,
//  Object::paleta -> paleta) siguen resolviendo a la misma punta. Ver
//  W3dNamesCollectSpaces mas abajo.
//
//  NO usa undo (abrir un proyecto limpia el historial) y avisa SIEMPRE de forma
//  visible: los nombres que el usuario ve en el outliner cambiaron.
// ---------------------------------------------------------------------------
// el orden es PRE-ORDEN (el mismo de FindObjectByName), y se lleva un set de nombres
// YA TOMADOS por scope: asi el PRIMERO del recorrido conserva el nombre pelado y solo se
// numeran los que vienen despues. (Mirar el arbol vivo en vez de un set daba el resultado
// al reves: en hola.w3d la ESCENA "Hola" perdia contra su propio hijo "Hola".)
typedef std::map<Object*, std::set<std::string> > TakenByScope;
static bool TakenNameInSet(const std::string& n, void* ctx) {
    std::set<std::string>* S = (std::set<std::string>*)ctx;
    return S->find(n) != S->end();
}
static void RepairRec(Object* node, TakenByScope& taken, std::vector<std::pair<Object*, std::string> >& changes) {
    if (!node) return;
    for (size_t i = 0; i < node->Childrens.size(); i++) {
        Object* h = node->Childrens[i];
        Object* root = W3dScopeNameOf(h);
        TakenByScope::iterator it = taken.find(root);
        if (it == taken.end()) {
            // scope nuevo: sembrarlo con el nombre de SU RAIZ (la busqueda por nombre
            // arranca EN la raiz, asi que un hijo homonimo de la escena es ambiguo). Una
            // INSTANCIA DE PREFAB no se siembra: su nombre es del scope de afuera y lo que
            // genera se busca desde sus hijos (ver W3dIsBoundaryScope en Objects.h)
            std::set<std::string> s0;
            if (root && !W3dIsInstanceType(root->getType())) s0.insert(root->name);
            it = taken.insert(std::make_pair(root, s0)).first;
        }
        const std::string norm = W3dNameNormalize(h->name, "Object");
        std::string newValue = norm;
        if (TakenNameInSet(norm, &it->second))
            newValue = W3dUniqueName(norm, "Object", TakenNameInSet, &it->second);
        it->second.insert(newValue);
        if (newValue != h->name) changes.push_back(std::make_pair(h, newValue));
        // si es una raiz UI, su scope propio arranca con SU nombre definitivo; una instancia de
        // prefab, vacio (la otra frontera de scope)
        if (h->getType() == ObjectType::ui) {
            std::set<std::string> s0; s0.insert(newValue);
            taken[h] = s0;
        } else if (W3dIsInstanceType(h->getType())) {
            taken[h] = std::set<std::string>();
        }
        RepairRec(h, taken, changes);
    }
}

// ---------------------------------------------------------------------------
//  LOS OTROS ESPACIOS DE NOMBRES (todo lo que NO es Object::name). Una sola
//  enumeracion, en el ORDEN DE BUSQUEDA de cada espacio, usada por DOS clientes:
//    1) la reparacion de carga (aca abajo): renumera los duplicados del archivo.
//    2) el UNDO (W3dFreeNameFor): al RESTAURAR un nombre hay que re-chequear
//       que no se lo hayan tomado mientras tanto (bug F2).
//  Si aparece un espacio nuevo, se agrega ACA y los dos clientes lo cubren.
//
//  PALETAS: los COLORES son un caso especial. El invariante del proyecto dice
//  que todas las paletas tienen los mismos colores CON EL MISMO NOMBRE en el
//  mismo indice, asi que el espacio es la lista de la paleta 0 (igual que
//  W3dPaletteFreeNameColor) y despues de tocar un nombre hay que propagarlo
//  por indice a las demas (PalettesPropagateColors).
// ---------------------------------------------------------------------------
// abre un espacio nuevo al final de la lista y devuelve la referencia para llenarlo
static W3dNamespace& NewSpace(std::vector<W3dNamespace>& out,
                                       const char* label, const char* defo, void* owner) {
    out.push_back(W3dNamespace());
    W3dNamespace& E = out.back();
    E.label = label; E.defaultValue = defo; E.owner = owner;
    return E;
}
// casi todos los espacios son un 'vector<T*>' cuyo campo de nombre se llama 'nombre' o
// 'name': dos macros chicas que abren el espacio y lo llenan de una (C++03, sin lambdas).
#define W3D_SPACE_LABEL(out, label, defo, owner, list) \
    { W3dNamespace& E_ = NewSpace(out, label, defo, owner); \
      for (size_t iP = 0; iP < (list).size(); iP++) if ((list)[iP]) E_.names.push_back(&(list)[iP]->name); }
#define W3D_SPACE_NAME(out, label, defo, owner, list) \
    { W3dNamespace& E_ = NewSpace(out, label, defo, owner); \
      for (size_t iP = 0; iP < (list).size(); iP++) if ((list)[iP]) E_.names.push_back(&(list)[iP]->name); }

static void CollectMeshSpaces(Mesh* m, std::vector<W3dNamespace>& out) {
    W3D_SPACE_LABEL(out, "VERTEX GROUP", "Group",       m, m->vertexGroups)
    W3D_SPACE_LABEL(out, "UV GROUP",     "Group",       m, m->uvGroups)
    W3D_SPACE_LABEL(out, "UV MAP",       "UVMap",       m, m->uvMaps)
    W3D_SPACE_LABEL(out, "COLOR LAYER",  "Color",       m, m->colorLayers)
    W3D_SPACE_NAME  (out, "VERTEX ANIM",  "Anim",        m, m->animations)
    W3D_SPACE_LABEL(out, "MODIFICADOR",  "Mod",         m, m->modifiers)
    W3D_SPACE_LABEL(out, "ARMATURE 2D",  "Armature 2D", m, m->armatures2d)
    // MESH PARTS: viven POR VALOR dentro de la malla
    { W3dNamespace& E = NewSpace(out, "MESH PART", "Part", m);
      for (size_t i = 0; i < m->materialsGroup.size(); i++) E.names.push_back(&m->materialsGroup[i].name); }
    // HUESOS 2D: el espacio es la MALLA ENTERA (todos sus rigs 2D a la vez; el skinning 2D
    // aplica TODOS los armatures y cada hueso pesa por su UV group HOMONIMO)
    { W3dNamespace& E = NewSpace(out, "HUESO 2D", "Bone", m);
      for (size_t a = 0; a < m->armatures2d.size(); a++) if (m->armatures2d[a])
          for (size_t b = 0; b < m->armatures2d[a]->bones.size(); b++)
              E.names.push_back(&m->armatures2d[a]->bones[b].name); }
    // CLIPS 2D: uno por ARMATURE 2D
    for (size_t a = 0; a < m->armatures2d.size(); a++) if (m->armatures2d[a])
        W3D_SPACE_NAME(out, "CLIP 2D", "Anim", m, m->armatures2d[a]->anims)
}

void W3dNamesCollectSpaces(std::vector<W3dNamespace>& out) {
    out.clear();
    // ---- globales de PROYECTO ----
    W3D_SPACE_NAME(out, "MATERIAL", "Material", NULL, Materials)
    W3D_SPACE_NAME(out, "ANIMACION DE ESCENA", "Scene", NULL, SceneAnimations)
    // las MALLAS 3D del registro (objects/MeshResource.h): el undo de un rename re-chequea aca
    W3D_SPACE_LABEL(out, "MALLA 3D", "Mesh", NULL, W3dMeshesRegistry())
    { std::vector<Palette>& ps = W3dPalettes();
      W3dNamespace& P = NewSpace(out, "PALETA", "Palette", NULL);
      for (size_t i = 0; i < ps.size(); i++) P.names.push_back(&ps[i].name);
      if (!ps.empty()) {   // los colores van por la paleta 0 (ver el invariante de arriba)
          W3dNamespace& C = NewSpace(out, "COLOR DE PALETA", "Color", NULL);
          for (size_t i = 0; i < ps[0].colors.size(); i++) C.names.push_back(&ps[0].colors[i].name);
      } }

    // ---- por MALLA y por ARMATURE (recorrido del arbol) ----
    std::vector<Object*> stack; if (SceneCollection) stack.push_back(SceneCollection);
    while (!stack.empty()) {
        Object* n = stack.back(); stack.pop_back();
        for (size_t i = 0; i < n->Childrens.size(); i++) stack.push_back(n->Childrens[i]);
        // CONSTRAINTS: el scope es POR OBJETO (como los modificadores de una malla, y no
        // como los objetos, que son por escena). Dos billboards en el MISMO objeto tienen
        // que salir "Billboard" y "Billboard.001"; dos objetos distintos pueden tener cada
        // uno su "Billboard" y no se pisan. Lo tiene CUALQUIER tipo de objeto, no solo la
        // malla, asi que el espacio se abre aca arriba del if/else de tipos.
        // Solo si tiene: abrir un espacio VACIO por cada objeto de la escena le costaria a
        // los dos clientes (la reparacion de carga y el undo de renames) un recorrido por
        // objeto para nada.
        if (!n->constraints.empty())
            W3D_SPACE_LABEL(out, "CONSTRAINT", "Constraint", n, n->constraints)
        if (n->getType() == ObjectType::mesh) CollectMeshSpaces((Mesh*)n, out);
        else if (n->getType() == ObjectType::armature) {
            Armature* a = (Armature*)n;
            { W3dNamespace& E = NewSpace(out, "HUESO", "Bone", a);   // viven POR VALOR
              for (size_t i = 0; i < a->bones.size(); i++) E.names.push_back(&a->bones[i].name); }
            W3D_SPACE_NAME(out, "CLIP", "Anim", a, a->animations)
        }
    }
}
#undef W3D_SPACE_LABEL
#undef W3D_SPACE_NAME

// invariante de paletas: el nombre del color 'i' es el MISMO en todas las paletas
static void PalettesPropagateColors() {
    std::vector<Palette>& ps = W3dPalettes();
    if (ps.empty()) return;
    for (size_t i = 0; i < ps[0].colors.size(); i++)
        for (size_t j = 1; j < ps.size(); j++)
            if (i < ps[j].colors.size()) ps[j].colors[i].name = ps[0].colors[i].name;
}

std::string W3dFreeNameFor(std::string* destination, const std::string& request, const char** labelOut) {
    if (labelOut) *labelOut = NULL;
    if (!destination) return request;
    // 1) es el nombre de un OBJETO? Se resuelve con LA misma puerta que el editor
    //    (Object::FreeName: scope por escena, se excluye a si mismo).
    {
        std::vector<Object*> stack; if (SceneCollection) stack.push_back(SceneCollection);
        while (!stack.empty()) {
            Object* n = stack.back(); stack.pop_back();
            for (size_t i = 0; i < n->Childrens.size(); i++) stack.push_back(n->Childrens[i]);
            if (&n->name == destination) { if (labelOut) *labelOut = "OBJETO"; return n->FreeName(request); }
        }
    }
    // 2) alguno de los otros espacios (por IDENTIDAD DE PUNTERO). Si no aparece en
    //    ninguno NO es un nombre: es una ref de lua / un targetName / un RailName, y
    //    esos se restauran TAL CUAL (son el otro extremo de un vinculo, no un nombre).
    std::vector<W3dNamespace> esp;
    W3dNamesCollectSpaces(esp);
    for (size_t e = 0; e < esp.size(); e++) {
        bool mine = false;
        for (size_t k = 0; k < esp[e].names.size(); k++) if (esp[e].names[k] == destination) { mine = true; break; }
        if (!mine) continue;
        if (labelOut) *labelOut = esp[e].label;
        return W3dUniqueNameInList(request, esp[e].defaultValue, esp[e].names, destination);
    }
    return request;
}

// ---------------------------------------------------------------------------
//  EL PUNTERO SIGUE VIVO? (validacion del destino 'Directo' del undo de renames,
//  ver W3dRenameDest en Undo.h). Directo se usa SOLO para nombres que son MIEMBRO
//  de un Object: Object::name y las puntas de vinculo cacheadas targetName /
//  RailName. Esos no se recrean, pero el OBJETO se puede borrar (y liberarse al
//  caerse del stack de undo), y entonces el Ctrl+Z escribia en memoria liberada.
//  Aca se busca el puntero POR IDENTIDAD entre los objetos vivos de la escena; si
//  no aparece, el paso de undo es un NO-OP seguro.
//
//  POR QUE SOLO EL ARBOL DE OBJETOS y no todos los espacios de nombres: para los
//  elementos de una lista (vertex groups, UV maps, clips, materiales...) buscar
//  "el puntero sigue siendo UN nombre vivo" NO alcanza y ademas ENGANA. Esos
//  elementos se borran y se re-crean en bloque (Mesh::FreeLayers + los new de
//  MeshGeoUndo::ApplyTo) y el allocator RECICLA las direcciones: el puntero
//  viejo de un vertex group aparece como el nombre vivo de OTRA cosa (medido: el
//  undo escribia el nombre del vertex group encima de un UV group). Por eso esos
//  destinos van por (dueno + indice) y no por puntero. Ver W3dDestLayerMesh.
//
//  OJO (mismo criterio que ObjectInScene, en Undo.cpp): un objeto BORRADO esta
//  DETACHADO del arbol y por lo tanto "no vivo" para esta puerta. Es correcto: el
//  undo del borrado corre ANTES (LIFO) y lo devuelve al arbol, asi que la
//  secuencia normal rename->deleteValue->Ctrl+Z->Ctrl+Z sigue funcionando igual.
// ---------------------------------------------------------------------------
// puntas de vinculo POR NOMBRE que cuelgan de un objeto vivo. Por la MISMA puerta
// generica que CollectTargetsByName (ver el bloque de arriba): tienen que ser el
// mismo conjunto, o el undo de un rename que SI se arrastro dejaria de restaurarse
// por no reconocer el destino (W3dDestName lo avisa, pero recien en el log).
// Con el switch a mano eran dos listas que mantener sincronizadas a ojo.
//
// LIMITE CONOCIDO, el mismo que ya tenia targetName: esto contesta "esa direccion es
// HOY una punta de vinculo", no "es LA punta que se capturo". Un constraint se borra
// con el boton "-" y el proximo puede caer en la misma direccion. Lo que protege de
// verdad es que el rename y su undo son un solo paso atomico; para la identidad
// estable de una entrada del stack esta el SERIAL (ver ConstraintBySerial).
static bool PointerIsLinkTip(Object* node, std::string* p) {
    if (!node) return false;
    const int n = node->RefsObject();
    for (int i = 0; i < n; i++) if (node->RefObjectName(i) == p) return true;
    return false;
}
bool W3dPointerNameLive(std::string* p) {
    if (!p) return false;
    std::vector<Object*> stack; if (SceneCollection) stack.push_back(SceneCollection);
    while (!stack.empty()) {
        Object* n = stack.back(); stack.pop_back();
        for (size_t i = 0; i < n->Childrens.size(); i++) stack.push_back(n->Childrens[i]);
        if (&n->name == p) return true;
        if (PointerIsLinkTip(n, p)) return true;
    }
    return false;
}

int W3dNamesRepairScene(bool notify) {
    if (!SceneCollection) return 0;
    int n = 0;
    std::string first;

    // ---- ESPACIO 1: los OBJETOS (por escena) ----
    TakenByScope taken;
    std::vector<std::pair<Object*, std::string> > changes;
    RepairRec(SceneCollection, taken, changes);
    for (size_t i = 0; i < changes.size(); i++) {
        Object* o = changes[i].first;
        const std::string old      = o->name;
        const std::string newValue = changes[i].second;
        if (newValue == old) continue;
        // SIN arrastrar vinculos (ver el bloque de arriba): el que conserva el nombre
        // es el PRIMERO, que es al que ya resolvian todos los targets y refs.
        o->name = newValue;
        n++;
        if (n <= 3) { if (!first.empty()) first += ", ";
                      first += W3dShortName(old) + " -> " + W3dShortName(newValue); }
        w3dLogfW("[nombres] OBJETO duplicado renumerado: '%s' -> '%s' (los vinculos por nombre "
                 "siguen apuntando al primer '%s')", old.c_str(), newValue.c_str(), old.c_str());
    }

    // ---- LOS OTROS ESPACIOS (materiales, grupos, huesos, clips, paletas, ...) ----
    std::vector<W3dNamespace> esp;
    W3dNamesCollectSpaces(esp);
    bool touchedColors = false;
    for (size_t e = 0; e < esp.size(); e++) {
        W3dNamespace& E = esp[e];
        std::set<std::string> tom;
        for (size_t k = 0; k < E.names.size(); k++) {
            std::string* p = E.names[k];
            const std::string old = *p;
            const std::string norm = W3dNameNormalize(old, E.defaultValue);
            std::string newValue = norm;
            if (TakenNameInSet(norm, &tom))
                newValue = W3dUniqueName(norm, E.defaultValue, TakenNameInSet, &tom);
            tom.insert(newValue);
            if (newValue == old) continue;
            *p = newValue;
            if (E.label && std::string(E.label) == "COLOR DE PALETA") touchedColors = true;
            n++;
            if (n <= 3) { if (!first.empty()) first += ", ";
                          first += W3dShortName(old) + " -> " + W3dShortName(newValue); }
            w3dLogfW("[nombres] %s duplicado renumerado: '%s' -> '%s' (el primer '%s' conserva "
                     "el nombre, asi que los bindings no se mueven)",
                     E.label, old.c_str(), newValue.c_str(), old.c_str());
        }
    }
    if (touchedColors) PalettesPropagateColors();

    if (n > 0 && notify)        // ROJO: los nombres que se ven en el outliner cambiaron
        W3dAvisof(true, T("%d repeated name(s) renumbered (%s%s). The original did NOT change: whatever "
                          "pointed to it by name still points there."),
                  n, first.c_str(), n > 3 ? ", ..." : "");
    return n;
}
