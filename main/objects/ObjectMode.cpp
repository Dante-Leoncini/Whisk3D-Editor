#include "ObjectMode.h"
#include "w3dlog.h"    // w3dLogfW: los renames automaticos quedan registrados
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "render/OpcionesRender.h" // g_transformPivot + enum TransformPivot (editor)
#include "objects/Mesh.h"      // snap del cursor al centro de la malla en Edit Mode
#include "objects/Texto2D.h"   // W3dDuplicarUno: copiar elementos 2D
#include "objects/Imagen2D.h"
#include "objects/Rect2D.h"
#include "objects/Contenedor2D.h"
#include "objects/Slice9.h"
#include "objects/Boton2D.h"
#include "objects/Expandir2D.h"
#include "objects/Video2D.h"
#include "EditMesh.h"  // CentroSeleccion
#include "Undo.h"      // Ctrl+Z: capturar transform / limpiar al borrar
#include "ViewPorts/LayoutInput.h" // SNAP en modo objeto (SnapBuscarTarget, g_snap, enums)
#include "edit/Modifier.h" // deep-copy de modificadores al duplicar
#include "edit/MeshEdit.h" // Separate: CompactarCapas (cirugia del mesh nuevo)
#include "objects/Camera.h"   // AnimFov: fov/ortografica de la camara (animables)
#include "objects/Light.h"    // AnimColor: color (diffuse) de la luz (animable)
#include "objects/Armature.h" // Apply Transform sobre armature: hornear en los huesos
#include "animation/SkeletalAnimation.h" // HornearTransformEnHuesos
#include "animation/VertexAnimation.h"   // curvas propias de la animacion del objeto (kind 3)
#include "animation/Armature2DAnimation.h" // Arm2DClonar: deep copy del rig 2D al duplicar la malla

// SNAP en MODO OBJETO: se snapshotean los "puntos de snap" de la seleccion al empezar el transform (todos los
// verts de las mallas + el ORIGEN de camara/lampara/empty) y move/rotate/scale imantan la BASE al target.
static void SnapObjCapturar();
static void SnapAjustarObjMove();
void SnapAjustarObjRot(); // publica: la rotacion "desde la vista" (trackball) esta en ViewPort3D.cpp
static void SnapAjustarObjScale();

// (LayoutInput) transform de MALLA en curso + su reset: al cambiar de eje (X/Y/Z) en Edit Mode hay que
// RESETEAR el acumulado (translate/extrude/scale) a cero y re-aplicar con el eje nuevo, como en Object Mode.
extern bool EditXformActivo();
extern void EditXformReiniciar();

void ReestablecerEstado(bool ClearEstado){
	if (InteractionMode == ObjectMode){
		for(size_t o=0; o < estadoObjetos.size(); o++){
			SaveState& estadoObj = estadoObjetos[o];
			Object& obj = *estadoObj.obj;
			obj.pos = estadoObj.pos;
			obj.SetRotSnapshot(estadoObj.rot, estadoObj.rotEuler);  // tal cual se capturo (con sus vueltas)
			obj.scale = estadoObj.scale;
		}
		//estadoObjetos.Close();
		if (ClearEstado) estadoObjetos.clear();
	}
	// EDIT MODE: resetear el transform de MALLA (gEVtrans/gEVscaleAmt/...) al cambiar de eje (antes no se reseteaba
	// -> el movimiento del vertice quedaba acumulado del eje anterior). No se resetea al CONFIRMAR (ClearEstado).
	else if (InteractionMode == EditMode && !ClearEstado && EditXformActivo()){
		EditXformReiniciar();
	}
	if (ClearEstado) {
		estado = editNavegacion;
	}
};

// ---- orientacion de la transformacion (global / local / vista) ----
// eje del mundo (engine Y-up) para el enum: X=derecha, Y=profundidad(engine Z),
// Z=arriba(engine Y). Misma convencion que usa el resto del modelo.
static Vector3 EjeMundo(int a){
	if (a == X) return Vector3(1, 0, 0);
	if (a == Y) return Vector3(0, 0, 1);
	return Vector3(0, 1, 0); // Z = arriba
}
// el eje constrenido EN WORLD segun la orientacion actual. La ESCALA es un caso
// especial: siempre local (no existe escala global/desde-la-vista), sin importar
// la orientacion elegida en el menu.
Vector3 EjeOrientado(Object& obj, int a){
	if (estado == EditScale || transformOrientation == LocalOrient){
		Vector3 v = obj.Rot() * EjeMundo(a);
		return v.Normalized();
	}
	if (transformOrientation == ViewOrient){
		if (a == X) return camRight;
		if (a == Y) return camForward;
		return camUp; // Z
	}
	if (transformOrientation == NormalOrient) return gTransformNormal; // la normal (extrude / menu Normal)
	return EjeMundo(a); // global
}

// cicla eje/orientacion DURANTE un transform (tecla X/Y/Z en PC, 1/2/3 en
// Symbian). Primer toque de un eje: lo constriñe en GLOBAL. Re-apretar el
// MISMO eje: cicla Global -> Local -> View -> LIBRE (3 ejes). Se restaura el
// estado guardado para re-aplicar con el nuevo eje (como Blender).
void CiclarEjeTransform(int eje){
	if (estado == editNavegacion) return;
	// si el transform venia constrenido a la NORMAL (extrude o orientacion Normal) y se aprieta
	// X/Y/Z, el usuario quiere el EJE (no la normal): se sale a GLOBAL + ese eje (= comportamiento
	// de "mover"). Unifica el extrude con el operador normal: nada de codigo aparte.
	if (gEVuseCustom){
		gEVuseCustom = false;
		transformOrientation = GlobalOrient;
		axisSelect = eje;
		ReestablecerEstado(false);
		return;
	}
	if (estado == EditScale){
		// la ESCALA es siempre local: solo eje-unico (local) <-> 3 ejes. No tiene
		// global ni "desde la vista" (no existe escalar desde la vista).
		transformOrientation = LocalOrient;
		axisSelect = (axisSelect == eje) ? XYZ : eje; // re-apretar el mismo -> 3 ejes
		ReestablecerEstado(false);
		return;
	}
	// rotacion/translacion: ciclo de orientacion del eje. La ROTACION agrega un
	// paso ORBITAL/gimbal entre 'view' y el trackball (libre). El ciclo es:
	//   trackball -> eje global -> eje local -> eje view -> orbital -> trackball...
	if (axisSelect != eje){
		if (estado == rotacion && axisSelect == OrbitalAxis){
			// orbital -> trackball: cierra el ciclo y resetea la orientacion a global
			axisSelect = ViewAxis; transformOrientation = GlobalOrient; gTrackballCap = false;
		} else {
			axisSelect = eje; // 1er toque: mantiene la orientacion ACTUAL (la del menu)
		}
	} else {
		if (transformOrientation == GlobalOrient)     transformOrientation = LocalOrient;
		else if (transformOrientation == LocalOrient) transformOrientation = ViewOrient;
		else if (estado == rotacion)                  axisSelect = OrbitalAxis; // view -> orbital
		else { axisSelect = ViewAxis; transformOrientation = GlobalOrient; } // translacion: view -> libre
	}
	ReestablecerEstado(false);
}

// Shift+eje: constriñe a un PLANO (excluye 'eje', mueve en los otros dos).
// Mismo plano re-apretado: cicla Global -> Local -> View -> libre.
void CiclarPlanoTransform(int eje){
	if (estado == editNavegacion) return;
	if (gEVuseCustom){ // salir de la normal (extrude / Normal) a un PLANO global (como mover)
		gEVuseCustom = false; transformOrientation = GlobalOrient;
		axisSelect = (eje == X) ? PlaneX : (eje == Y) ? PlaneY : PlaneZ;
		ReestablecerEstado(false); return;
	}
	int plano = (eje == X) ? PlaneX : (eje == Y) ? PlaneY : PlaneZ;
	if (estado == EditScale){
		// escala: plano siempre local, sin ciclar orientacion (re-apretar -> 3 ejes)
		transformOrientation = LocalOrient;
		axisSelect = (axisSelect == plano) ? XYZ : plano;
		ReestablecerEstado(false);
		return;
	}
	if (axisSelect != plano){
		axisSelect = plano; // orientacion actual (la del menu)
	} else {
		if (transformOrientation == GlobalOrient)     transformOrientation = LocalOrient;
		else if (transformOrientation == LocalOrient) transformOrientation = ViewOrient;
		else { axisSelect = ViewAxis; transformOrientation = GlobalOrient; } // libre + reset
	}
	ReestablecerEstado(false);
}

void Cancelar(){
	// POSE MODE: si hay un transform de huesos en curso, se cancela ese (restaura la pose previa) y listo.
	{ extern int g_poseModo; if (g_poseModo){ extern void PoseXformCancel(); PoseXformCancel(); return; } }
	// EDIT de ARMATURE: idem con el grab/transform de puntas (restaura head/tail del snapshot).
	{ extern bool BoneGrabActivo(); extern void BoneGrabCancelar();
	  if (BoneGrabActivo()){ BoneGrabCancelar(); return; } }
	// Mostrar el cursor
#ifndef W3D_SYMBIAN
	#if SDL_MAJOR_VERSION == 2
		SDL_ShowCursor(SDL_ENABLE);
	#elif SDL_MAJOR_VERSION == 3
		SDL_ShowCursor();
	#endif
#endif

	if (estado != editNavegacion){
		UndoTransformCancelar(); // descarta el undo pendiente del transform cancelado
		ReestablecerEstado();
	}
	ViewPortClickDown = false;
};

// Toggles de animacion. Van FUERA del #ifdef de abajo: son estado, no implementacion, y los usan las dos
// plataformas. AutoKeyOn en particular lo lee el auto key de POSE (que vive en el Core y SI anda en Symbian).
bool AutoKeyOn = false;      // lo prende/apaga el boton del timeline
bool MotionTrailOn = false;  // menu Animation del viewport 3D: ver el camino de los objetos animados

void EliminarAnimaciones(Object& obj){
	for(size_t a = 0; a < AnimationObjects.size(); ) {
		if (AnimationObjects[a].obj == &obj) {
			for(size_t p = 0; p < AnimationObjects[a].Propertys.size(); p++) {
				AnimationObjects[a].Propertys[p].keyframes.clear();
			}
			AnimationObjects[a].Propertys.clear();
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
static AnimationObject& AnimObjDe(Object* o){
	for (size_t i=0;i<AnimationObjects.size();i++) if (AnimationObjects[i].obj==o) return AnimationObjects[i];
	AnimationObject ao; ao.obj=o; ao.FirstKeyFrame=0; ao.LastKeyFrame=0; AnimationObjects.push_back(ao);
	return AnimationObjects.back();
}
// la animacion PROPIA activa del objeto (kind 3, la de la tarjeta "Animacion"), o
// NULL si estamos en modo escena (kind 0) -> ahi las curvas viven en AnimObjDe
VertexAnimation* AnimObjetoActiva(Object* o){
	extern int ActiveAnimKind; extern Mesh* ActiveAnimMesh;
	if (ActiveAnimKind != 3 || (Object*)ActiveAnimMesh != o || !o || o->getType()!=ObjectType::mesh) return NULL;
	Mesh* m = (Mesh*)o;
	VertexAnimationActive* va = FindTargetAnim(m);
	int i = va ? va->currentAnim : -1;
	return (i>=0 && i<(int)m->animations.size()) ? m->animations[i] : NULL;
}
// las curvas donde van/de donde salen los keyframes: la animacion PROPIA activa, o
// la de ESCENA (cada cosa sus propias curvas, por diseno)
static std::vector<AnimProperty>& CurvasActivas(Object* o){
	VertexAnimation* an = AnimObjetoActiva(o);
	if (an) return an->curvas;         // animacion propia del objeto (kind 3)
	return AnimObjDe(o).Propertys;     // animacion de escena (kind 0)
}
// valor ACTUAL de un canal (prop, comp) del objeto: lo que se ve ahora (para keyar)
static float CanalValorActual(Object* o, int prop, int comp){
	if (prop == AnimVisible)   return o->visible      ? 1.0f : 0.0f;
	if (prop == AnimRender)    return o->renderizable ? 1.0f : 0.0f;
	// CAMARA: fov + distancias de dibujado (near/far)
	if (prop == AnimFov)  return (o->getType()==ObjectType::camera) ? ((Camera*)o)->fov : 45.0f;
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
		return (comp==AnimX)? (float)(l->LightID - GL_LIGHT0) : (l->direccional?1.0f:0.0f); // AnimLightMisc
	}
	o->ActualizarDisplayRot();
	const Vector3& v = (prop == AnimPosition) ? o->pos : (prop == AnimScale) ? o->scale : o->rotEuler;
	return comp == AnimX ? v.x : comp == AnimY ? v.y : v.z;
}
// busca la curva (prop, comp) SIN crearla (para consultar el estado del keyframe)
static const AnimProperty* CurvaDe(Object* o, int prop, int comp){
	const std::vector<AnimProperty>& P = CurvasActivas(o);
	for (size_t k=0;k<P.size();k++) if (P[k].Property==prop && P[k].component==comp) return &P[k];
	return NULL;
}
// estado del keyframe de un canal en 'frame': 0 = sin animacion; 1 = animado pero NO
// hay key en este frame; 2 = HAY key en este frame. Lo usa el boton-rombo del panel.
int AnimCanalEstado(Object* o, int prop, int comp, int frame){
	if (!o) return 0;
	const AnimProperty* c = CurvaDe(o, prop, comp);
	if (!c || c->keyframes.empty()) return 0;
	for (size_t i=0;i<c->keyframes.size();i++) if (c->keyframes[i].frame==frame) return 2;
	return 1;
}
// pone (si no hay) o QUITA (si hay) un key del canal en 'frame', con el valor actual
void AnimCanalToggle(Object* o, int prop, int comp, int frame){
	if (!o) return;
	// COLOR (diffuse/ambient/specular): UN solo rombo para las 3 curvas R/G/B -> se keyean/borran
	// JUNTAS con el valor de cada componente. El estado se mide por la curva representante (AnimX).
	if (prop == AnimColor || prop == AnimAmbient || prop == AnimSpecular){
		int est = AnimCanalEstado(o, prop, AnimX, frame);
		for (int cc = AnimX; cc <= AnimZ; cc++){
			AnimProperty& ap = PropertyDeLista(CurvasActivas(o), prop, cc);
			if (est == 2) BorrarKeyframeManteniendoForma(ap, frame);
			else          SetKeyCurva(ap, frame, CanalValorActual(o, prop, cc));
		}
		g_redraw = true;
		return;
	}
	int est = AnimCanalEstado(o, prop, comp, frame);
	AnimProperty& ap = PropertyDeLista(CurvasActivas(o), prop, comp);
	if (est == 2) BorrarKeyframeManteniendoForma(ap, frame);          // sacar el key de este frame
	else {
		SetKeyCurva(ap, frame, CanalValorActual(o, prop, comp));      // poner con el valor actual
		// visible/render son TRUE/FALSE -> keyframe CONSTANTE (escalon); los
		// numericos (pos/rot/escala) quedan en bezier (curvas suaves)
		if ((prop == AnimVisible || prop == AnimRender || prop == AnimLightMisc)){
			for (size_t k=0;k<ap.keyframes.size();k++)
				if (ap.keyframes[k].frame==frame){ ap.keyframes[k].Interpolation = KfConstant; break; }
		}
	}
	g_redraw = true;
}

// keyframe en las TRES curvas (X/Y/Z) de una propiedad del objeto. Cada componente es una curva independiente
// (PropertyDeLista las crea/encuentra por (propiedad, componente)); keyar toca las 3 en el mismo frame, pero
// despues se mueven/borran/curvan por separado desde el dope sheet.
static void SetKeyObj3(std::vector<AnimProperty>& props, int prop, int frame, float x, float y, float z){
	SetKeyCurva(PropertyDeLista(props, prop, AnimX), frame, x);
	SetKeyCurva(PropertyDeLista(props, prop, AnimY), frame, y);
	SetKeyCurva(PropertyDeLista(props, prop, AnimZ), frame, z);
}

// ============================================================================
//  AUTO KEY: al confirmar un transform, guarda SOLO los canales que CAMBIARON.
//  El "cambio" se mide contra estadoObjetos, que es el snapshot que el propio transform toma al empezar. Por eso
//  hay que llamarla ANTES de limpiarlo (ver Viewport3D::Aceptar).
//  Es por CANAL, no por propiedad: si solo rotaste en X, se guarda X Euler Rotation y NADA mas. Eso es lo que
//  permite el modelo de curvas por componente (cada X/Y/Z es una curva propia).
// ============================================================================

// un canal cambio? Se compara con una tolerancia relativa: el transform pasa por matrices y trigonometria, asi
// que un valor que "no se toco" vuelve con basura en el ultimo bit y == daria cambios fantasma en los 9 canales.
static bool AutoKeyCambio(float a, float b){
	float d = a - b; if (d < 0) d = -d;
	float m = (a < 0 ? -a : a); float mb = (b < 0 ? -b : b); if (mb > m) m = mb;
	return d > 1e-5f * (1.0f + m);
}
// guarda el canal (prop, comp) si cambio. Devuelve true si guardo.
static bool AutoKeyCanal(std::vector<AnimProperty>& props, int prop, int comp, int frame, float valor, float viejo){
	if (!AutoKeyCambio(valor, viejo)) return false;
	SetKeyCurva(PropertyDeLista(props, prop, comp), frame, valor);
	return true;
}
// Devuelve cuantos canales guardo (0 = no hubo cambios -> no se ensucia la animacion ni el undo).
int AutoKeyObjetos(){
	if (!AutoKeyOn) return 0;
	int n = 0;
	for (size_t e = 0; e < estadoObjetos.size(); e++){
		Object* o = estadoObjetos[e].obj; if (!o) continue;
		o->ActualizarDisplayRot();                 // rotEuler al dia (el transform trabaja sobre el quaternion)
		// el euler de ANTES sale del quaternion del snapshot, que es lo unico que se guardo
		// el euler de ANTES sale del snapshot TAL CUAL (con sus vueltas). Derivarlo del quaternion del snapshot
		// lo devolvia canonico: rotar 360 daba "de 0 a 0" y el auto key no guardaba nada.
		Vector3 rotVieja = estadoObjetos[e].rotEuler;
		std::vector<AnimProperty>& props = CurvasActivas(o);   // propia (kind 3) o escena (kind 0)
		const Vector3& p0 = estadoObjetos[e].pos;
		const Vector3& s0 = estadoObjetos[e].scale;
		if (AutoKeyCanal(props, AnimPosition, AnimX, CurrentFrame, o->pos.x, p0.x)) n++;
		if (AutoKeyCanal(props, AnimPosition, AnimY, CurrentFrame, o->pos.y, p0.y)) n++;
		if (AutoKeyCanal(props, AnimPosition, AnimZ, CurrentFrame, o->pos.z, p0.z)) n++;
		if (AutoKeyCanal(props, AnimRotation, AnimX, CurrentFrame, o->rotEuler.x, rotVieja.x)) n++;
		if (AutoKeyCanal(props, AnimRotation, AnimY, CurrentFrame, o->rotEuler.y, rotVieja.y)) n++;
		if (AutoKeyCanal(props, AnimRotation, AnimZ, CurrentFrame, o->rotEuler.z, rotVieja.z)) n++;
		if (AutoKeyCanal(props, AnimScale,    AnimX, CurrentFrame, o->scale.x, s0.x)) n++;
		if (AutoKeyCanal(props, AnimScale,    AnimY, CurrentFrame, o->scale.y, s0.y)) n++;
		if (AutoKeyCanal(props, AnimScale,    AnimZ, CurrentFrame, o->scale.z, s0.z)) n++;
	}
	if (n) g_redraw = true;
	return n;
}

// El transform de UN objeto en el frame f: su animacion si la tiene, y si no su transform actual. Cada canal se
// mira por separado (X/Y/Z son curvas propias: puede estar animado solo X).
static void TransformEnFrame(Object* o, int f, Vector3& T, Vector3& R, Vector3& S){
	o->ActualizarDisplayRot();
	T = o->pos; R = o->rotEuler; S = o->scale;
	for (size_t i=0;i<AnimationObjects.size();i++){
		if (AnimationObjects[i].obj != o) continue;
		const std::vector<AnimProperty>& P = AnimationObjects[i].Propertys;
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
static Matrix4 WorldEnFrame(Object* o, int f){   // solo la usa MotionTrailDe, aca mismo
	Matrix4 M; M.Identity();
	if (!o) return M;
	// de la raiz hacia abajo: world = padre * local
	std::vector<Object*> cadena;
	for (Object* p = o; p; p = p->Parent){
		cadena.push_back(p);
		if (cadena.size() > 256) break;   // guarda contra un ciclo en el arbol
	}
	for (size_t i = cadena.size(); i-- > 0; ){
		Vector3 T, R, S;
		TransformEnFrame(cadena[i], f, T, R, S);
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
bool MotionTrailDe(Object* o, std::vector<Vector3>& pts, std::vector<int>& keys, int& desde, int& hasta){
	pts.clear(); keys.clear(); desde = 0; hasta = -1;
	if (!o) return false;
	const AnimationObject* ao = NULL;
	for (size_t i=0;i<AnimationObjects.size();i++) if (AnimationObjects[i].obj==o){ ao=&AnimationObjects[i]; break; }
	if (!ao) return false;
	// rango + frames con keyframe: la UNION de las 3 curvas de posicion (X/Y/Z son curvas independientes: un
	// keyframe puede estar solo en X)
	int mn = 0x7fffffff, mx = -0x7fffffff;
	for (size_t p2=0;p2<ao->Propertys.size();p2++){
		const AnimProperty& ap = ao->Propertys[p2];
		if (ap.Property != AnimPosition || ap.keyframes.empty()) continue;
		for (size_t k=0;k<ap.keyframes.size();k++){
			int f = ap.keyframes[k].frame;
			if (f < mn) mn = f; if (f > mx) mx = f;
			bool ya=false; for (size_t j=0;j<keys.size();j++) if (keys[j]==f){ ya=true; break; }
			if (!ya) keys.push_back(f);
		}
	}
	if (mn > mx) return false;           // sin curvas de posicion
	std::sort(keys.begin(), keys.end());
	desde = mn; hasta = mx;
	for (int f = mn; f <= mx; f++){
		Vector3 p = EvalPropVec(ao->Propertys, AnimPosition, f, o->pos);
		pts.push_back(WorldEnFrame(o->Parent, f) * p);   // el world del PADRE en ESE frame (puede estar animado)
	}
	return true;
}

// Insert Keyframe (i): guarda pos+rot+escala de cada objeto SELECCIONADO en el frame actual.
// 'canales' es la mascara del menu desplegable (KfCanalLoc/Rot/Scl, ver Animation.h): permite
// keyframear SOLO la localizacion, SOLO la rotacion o SOLO la escala. 0 = todos (default
// historico, y lo que siguen pasando los ~20 callers viejos del harness).
void InsertarKeyframeObjeto(int canales){
	if (canales == 0) canales = KfCanalTodos;
	for (size_t s=0;s<ObjSelects.size();s++){ Object* o=ObjSelects[s]; if (!o) continue;
		o->ActualizarDisplayRot(); // rotEuler al dia
		std::vector<AnimProperty>& props = CurvasActivas(o);   // propia (kind 3) o escena (kind 0)
		if (canales & KfCanalLoc) SetKeyObj3(props, AnimPosition, CurrentFrame, o->pos.x, o->pos.y, o->pos.z);
		if (canales & KfCanalRot) SetKeyObj3(props, AnimRotation, CurrentFrame, o->rotEuler.x, o->rotEuler.y, o->rotEuler.z);
		if (canales & KfCanalScl) SetKeyObj3(props, AnimScale,    CurrentFrame, o->scale.x, o->scale.y, o->scale.z);
	}
	g_redraw = true;
}
// Delete Keyframe: saca los keyframes del frame actual de cada objeto seleccionado
void BorrarKeyframeObjeto(){
	for (size_t s=0;s<ObjSelects.size();s++){ Object* o=ObjSelects[s]; if (!o) continue;
		std::vector<AnimProperty>& props = CurvasActivas(o);
		for (size_t p=0;p<props.size();p++){
			std::vector<keyFrame>& kf = props[p].keyframes;
			for (size_t k=0;k<kf.size();k++) if (kf[k].frame==CurrentFrame){ kf.erase(kf.begin()+k); break; } } }
	g_redraw = true;
}
// Clear Keyframe: borra TODA la animacion de los objetos seleccionados
void LimpiarKeyframeObjeto(){
	for (size_t s=0;s<ObjSelects.size();s++) if (ObjSelects[s]) EliminarAnimaciones(*ObjSelects[s]);
	g_redraw = true;
}
// PLAYBACK: aplica los keyframes al transform de cada objeto animado en el frame actual. Solo al CAMBIAR de frame
// (play o scrub) -> editar/keyframear un objeto en un frame fijo no lo "resnapea". Rotacion via euler XYZ.
// aplica las curvas de UN objeto EN EL FRAME QUE SE LE PIDA (transform + visible + render + camara + luz).
// El frame es un PARAMETRO y no el global CurrentFrame porque el guardado necesita evaluar en el CUADRO BASE
// para no hornear la pose del playhead en el .w3d (ver ReposoAnimObjetos en io/GuardarW3D.cpp). Es la UNICA
// funcion que sabe que campo mueve cada canal: el reposo tiene que deshacer exactamente lo que esta hace.
void W3dAplicarCurvasEnFrame(Object* o, std::vector<AnimProperty>& props, int frame){
	if (!o) return;
	bool hayP=false, hayR=false, hayS=false, hayV=false, hayRnd=false, hayFov=false, hayClip=false;
	bool hayCol=false, hayAmb=false, haySpec=false, hayAtt=false, haySpot=false, hayMisc=false;
	for (size_t p=0;p<props.size();p++){ if (props[p].keyframes.empty()) continue;
		int P = props[p].Property;
		if      (P==AnimPosition) hayP=true;   else if (P==AnimRotation)  hayR=true;
		else if (P==AnimScale)    hayS=true;   else if (P==AnimVisible)   hayV=true;
		else if (P==AnimRender)   hayRnd=true; else if (P==AnimFov)       hayFov=true;
		else if (P==AnimClip)     hayClip=true;else if (P==AnimColor)     hayCol=true;
		else if (P==AnimAmbient)  hayAmb=true; else if (P==AnimSpecular)  haySpec=true;
		else if (P==AnimAtten)    hayAtt=true; else if (P==AnimSpot)      haySpot=true;
		else if (P==AnimLightMisc)hayMisc=true; }
	// alguna curva de verdad va a escribir un canal -> el playhead POSA a este objeto.
	// Lo lee ReposoAnimObjetos al guardar, para devolverlo al cuadro base y no hornear el
	// scrub. Si nadie lo posa, el guardado NO lo toca (lo que el usuario movio a mano manda).
	if (hayP||hayR||hayS||hayV||hayRnd||hayFov||hayClip||hayCol||hayAmb||haySpec||hayAtt||haySpot||hayMisc)
		o->posadoPorCurvas = true;
	if (hayP) o->pos   = EvalPropVec(props, AnimPosition, frame, o->pos);
	if (hayS) o->scale = EvalPropVec(props, AnimScale,    frame, o->scale);
	if (hayR){ Vector3 e = EvalPropVec(props, AnimRotation, frame, o->rotEuler); o->SetRotEuler(e); }
	if (hayV)   o->visible      = PropertyDeLista(props, AnimVisible, AnimX).Eval(frame, o->visible?1.f:0.f) >= 0.5f;
	if (hayRnd) o->renderizable = PropertyDeLista(props, AnimRender,  AnimX).Eval(frame, o->renderizable?1.f:0.f) >= 0.5f;
	// CAMARA: fov + distancias de dibujado (near/far). La vista al mirar por la camara sigue las curvas.
	if (o->getType()==ObjectType::camera){ Camera* c=(Camera*)o;
		if (hayFov)  c->fov      = PropertyDeLista(props, AnimFov, AnimX).Eval(frame, c->fov);
		if (hayClip){ c->nearClip = PropertyDeLista(props, AnimClip, AnimX).Eval(frame, c->nearClip);
		              c->farClip  = PropertyDeLista(props, AnimClip, AnimY).Eval(frame, c->farClip); } }
	// LUZ: colores (diffuse/ambient/specular) + numericos (atenuacion/spot/GL/direccional)
	if (o->getType()==ObjectType::light){ Light* l=(Light*)o;
		if (hayCol) { Vector3 d(l->diffuse[0],l->diffuse[1],l->diffuse[2]); Vector3 v=EvalPropVec(props,AnimColor,frame,d);    l->diffuse[0]=v.x; l->diffuse[1]=v.y; l->diffuse[2]=v.z; }
		if (hayAmb) { Vector3 d(l->ambient[0],l->ambient[1],l->ambient[2]); Vector3 v=EvalPropVec(props,AnimAmbient,frame,d);  l->ambient[0]=v.x; l->ambient[1]=v.y; l->ambient[2]=v.z; }
		if (haySpec){ Vector3 d(l->specular[0],l->specular[1],l->specular[2]); Vector3 v=EvalPropVec(props,AnimSpecular,frame,d); l->specular[0]=v.x; l->specular[1]=v.y; l->specular[2]=v.z; }
		if (hayAtt) { l->attConstant  = PropertyDeLista(props,AnimAtten,AnimX).Eval(frame,l->attConstant);
		              l->attLinear    = PropertyDeLista(props,AnimAtten,AnimY).Eval(frame,l->attLinear);
		              l->attQuadratic = PropertyDeLista(props,AnimAtten,AnimZ).Eval(frame,l->attQuadratic); }
		if (haySpot){ l->spotCutoff   = PropertyDeLista(props,AnimSpot,AnimX).Eval(frame,l->spotCutoff);
		              l->spotExponent = PropertyDeLista(props,AnimSpot,AnimY).Eval(frame,l->spotExponent); }
		if (hayMisc){ float gi = PropertyDeLista(props,AnimLightMisc,AnimX).Eval(frame,(float)(l->LightID-GL_LIGHT0));
		              int idx=(int)(gi+0.5f); if(idx<0)idx=0; if(idx>7)idx=7; l->SetLightID(GL_LIGHT0+(GLenum)idx);
		              l->direccional = PropertyDeLista(props,AnimLightMisc,AnimY).Eval(frame,l->direccional?1.f:0.f) >= 0.5f; } }
}
static void AplicarCurvasLista(Object* o, std::vector<AnimProperty>& props){
	W3dAplicarCurvasEnFrame(o, props, CurrentFrame);
}
static void AplicarCurvasDe(AnimationObject& ao){ AplicarCurvasLista(ao.obj, ao.Propertys); }
void AplicarAnimacionObjetos(){
	extern int ActiveAnimKind; extern class Mesh* ActiveAnimMesh;
	// kind 3 (una VERTEX/OBJETO animation en el timeline): aplica SOLO las curvas
	// de ESE objeto (el resto del mundo queda quieto -> "solo se ve ese objeto")
	if (ActiveAnimKind == 3){
		// solo al CAMBIAR de frame: aplicar cada render pisaria las ediciones
		// manuales (mover/rotar) entre frames
		static int ultFrame3 = -999999; static Object* ultObj3 = 0;
		if (CurrentFrame == ultFrame3 && (Object*)ActiveAnimMesh == ultObj3) return;
		ultFrame3 = CurrentFrame; ultObj3 = (Object*)ActiveAnimMesh;
		if (ActiveAnimMesh){
			VertexAnimation* an = AnimObjetoActiva((Object*)ActiveAnimMesh);   // sus curvas PROPIAS
			if (an) AplicarCurvasLista((Object*)ActiveAnimMesh, an->curvas);
		}
		return;
	}
	// las curvas de objetos SON la animacion de ESCENA: solo con kind 0.
	if (ActiveAnimKind != 0) return;
	static int ultimoFrame = -999999;
	if (CurrentFrame == ultimoFrame) return;
	ultimoFrame = CurrentFrame;
	for (size_t i=0;i<AnimationObjects.size();i++){ AnimationObject& ao=AnimationObjects[i]; if (!ao.obj) continue;
		AplicarCurvasDe(ao);
	}
}
void Eliminar(bool IncluirCollecciones){
	if (InteractionMode == ObjectMode){
		//si no hay nada seleccionado. no borra
		if (!HayObjetosSeleccionados(IncluirCollecciones)){
#ifndef W3D_SYMBIAN
			std::cout << "nada seleccionado para borrar" << std::endl;
#endif
			return;
		}
		Cancelar();

		if (!SceneCollection) return;

		// Ctrl+Z de borrar: NO libera los objetos -> los DETACHA de la escena y los guarda el comando
		// (UndoCapturarBorrado), que los re-inserta al deshacer. Reemplaza al delete real + al UndoLimpiar.
		UndoCapturarBorrado(IncluirCollecciones);
		ObjActivo = NULL;
		ObjSelects.clear();
	}
}

static int gPivotSelCount = 0; // seleccionados que SUMO la recursion (divide el promedio: antes se
                               // dividia por ObjSelects.size(), que puede diferir si hay deriva)
void CalcObjectsTransformPivotPoint(Object* obj){
	if (obj->select){
		// el pivote de TRANSFORM (Median Point) usa el ORIGEN del objeto, no su
		// centro geometrico. El FOCO ('.') si usa el centro geometrico, pero por
		// otro camino (CentroFocoSeleccion). Son cosas distintas.
		//
		// BASE. Este pivote NO es decoracion: AplicarPivotATransform hace
		// nw = pivot + delta*(worldPos - pivot) y lo baja a pos con PonerEnGlobal, o sea que
		// TERMINA EN EL .w3d. Con la posicion EFECTIVA, "rotar 0 grados" dejaba de ser la
		// identidad -el pivote estaria en un lugar y la cadena por la que se baja en otro- y
		// el desplazamiento del constraint quedaba HORNEADO en obj->pos: apretar R y mover un
		// pixel sobre un objeto con Copy Location le reescribia la pos a la de la fuente.
		// worldPos (guardarEstadoRec) es base por el mismo motivo: las dos puntas de la resta
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
static void CalcFocoRec(Object* obj, Vector3& suma, int& n){
	if (obj->select){ suma += obj->PuntoFoco(); n++; }
	for(size_t c=0; c < obj->Childrens.size(); c++) CalcFocoRec(obj->Childrens[c], suma, n);
}
Vector3 CentroFocoSeleccion(){
	Vector3 suma(0.0f, 0.0f, 0.0f); int n = 0;
	if (SceneCollection)
		for(size_t c=0; c < SceneCollection->Childrens.size(); c++)
			CalcFocoRec(SceneCollection->Childrens[c], suma, n);
	return n > 0 ? suma * (1.0f / (float)n) : Vector3(0.0f, 0.0f, 0.0f);
}

void SetTransformPivotPoint(){
	if (InteractionMode == ObjectMode){
		// 3D Cursor: pivote = el cursor 3D
		if (g_transformPivot == PivotCursor3D) { TransformPivotPoint = cursor3D.pos; return; }
		// Active Element: pivote = el origen del objeto activo (BASE, ver CalcObjectsTransformPivotPoint)
		if (g_transformPivot == PivotActive && ObjActivo) {
			TransformPivotPoint = ObjActivo->GetGlobalPositionBase(); return;
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
void GuardarMousePos() {
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

void guardarEstadoRec(Object* obj){
    if (!obj) return;

    // Si está seleccionado, guardar estado
    if (obj->select && obj->visible) {
        SaveState NuevoEstado;
        NuevoEstado.obj = obj;
        NuevoEstado.pos = obj->pos;
		NuevoEstado.rot = obj->Rot();
		NuevoEstado.rotEuler = obj->rotEuler;   // con sus vueltas (el quaternion solo no alcanza)
        NuevoEstado.scale = obj->scale;
        // BASE: de aca sale el offset al pivote que AplicarPivotATransform/SnapAjustarObjMove
        // vuelven a bajar a obj->pos (o sea, al .w3d). Ver CalcObjectsTransformPivotPoint.
        NuevoEstado.worldPos = obj->GetGlobalPositionBase(); // para rotar/escalar desde el pivot
        estadoObjetos.push_back(NuevoEstado);
    }

    // Recursión: recorrer hijos
    for (size_t i = 0; i < obj->Childrens.size(); i++) {
        guardarEstadoRec(obj->Childrens[i]);
    }
}

bool guardarEstado(){
    if (!SceneCollection) return false;

    GuardarMousePos();
    estadoObjetos.clear();

    // Recorrer todo el árbol desde la raíz
    guardarEstadoRec(SceneCollection);

	if (estadoObjetos.empty()) return false;
	//std::cout << "moviendo "<< estadoObjetos.size() << " objetos" << std::endl;

    UndoTransformIniciar(); // captura pos/rot/escala PREVIAS (se confirma al aceptar el transform)
    SetTransformPivotPoint();
    SnapObjCapturar(); // snapshot de los puntos de snap (verts + origenes) para el imantado
	return true;
}

void SetPosicion(){
	// la UI y sus elementos 2D NO se transforman desde el 3D (la UI es orden de dibujo;
	// los textos se mueven en el Editor 2D). Borrarlos si esta permitido.
	if (ObjActivo && (ObjActivo->getType() == ObjectType::ui || ObjActivo->getType() == ObjectType::texto2d ||
	                  ObjActivo->getType() == ObjectType::imagen2d ||
	                  ObjActivo->getType() == ObjectType::rect2d ||
	                  ObjActivo->getType() == ObjectType::cont2d ||
	                  ObjActivo->getType() == ObjectType::slice9 ||
	                  ObjActivo->getType() == ObjectType::boton2d ||
	                  ObjActivo->getType() == ObjectType::expandir2d ||
	                  ObjActivo->getType() == ObjectType::video2d)) return;
	if (ObjActivo && InteractionMode == ObjectMode && ObjActivo->select && estado == editNavegacion){
		if (!guardarEstado()) return;
		estado = translacion; g_xformPrimerMov = true; // primer motion en cero (no salta)
		axisSelect = ViewAxis;
		ToolbarRegistrarAccion(TBMove); // historial de la barra de herramientas
	}
};

// duplica los objetos seleccionados (deep copy). Implementacion NUEVA y
// compartida: reemplaza al cuerpo viejo comentado (era del modelo viejo).
#include "Instance.h"
#include "objects/Light.h"
#include "Camera.h"
#include "Empty.h"
#include "LOD.h"     // W3dDuplicarUno: copiar los umbrales del LOD
#include "Culling.h" // W3dDuplicarUno: copiar soloCamaraActiva
#include "Particulas.h" // W3dDuplicarUno: copiar la config del emisor
#include "Collection.h" // W3dDuplicarUno: copiar ordenarPorCamara/ordenarUnaVez
#include "script/W3dScript.h" // W3dDuplicarUno: deep copy de scriptDatos (scripts + refs/valores)
#include <string.h>

// copia la parte COMUN de un elemento 2D (Elemento2D: rect, ancla, layout, overflow...)
static void CopiarBase2D(Elemento2D* d, Elemento2D* s) {
    d->ancho = s->ancho; d->alto = s->alto;
    d->ancla = s->ancla; d->rot2d = s->rot2d;
    d->opacidad = s->opacidad; d->peso = s->peso;
    d->padIzq = s->padIzq; d->padDer = s->padDer;
    d->padArr = s->padArr; d->padAba = s->padAba;
    d->layoutHijos = s->layoutHijos;
    d->layoutAjuste = s->layoutAjuste; d->layoutAlign = s->layoutAlign;
    d->distribucion = s->distribucion;
    d->gap = s->gap; d->padGapPx = s->padGapPx;
    d->tamModo = s->tamModo;
    d->recortaX = s->recortaX; d->recortaY = s->recortaY;
    d->conScroll = s->conScroll; d->scrollX = s->scrollX; d->scrollY = s->scrollY;
    d->margIzq = s->margIzq; d->margDer = s->margDer;
    d->margArr = s->margArr; d->margAba = s->margAba;
    d->margUni = s->margUni; d->padUni = s->padUni;
    d->expandir = s->expandir;
}

Object* W3dDuplicarUno(Object* src) {
    Object* nuevo = NULL;
    if (src->getType() == ObjectType::mesh) {
        Mesh* m = (Mesh*)src;
        Mesh* d = new Mesh(src->Parent, src->pos);
        d->vertexSize = m->vertexSize;
        // POSICIONES + ESTADO DE POSE: por la UNICA puerta (W3dPosVerts, ver Mesh.h). Copiar el
        // buffer a mano dejaba la copia con el flag posadaPorAnim en false y LA POSE ADENTRO:
        // con el playhead fuera del cuadro base, Shift+D daba una copia que se guardaba con la
        // pose HORNEADA y sin bloques SHARP/SEAM (mientras el original salia perfecto), y
        // cualquier edicion posterior sobre ella podaba TODAS las marcas contra la pose, callado.
        { W3dPosVerts p; p.Capturar(m); p.EscribirEn(d); }
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
            memcpy(d->uv, m->uv, sizeof(GLfloat) * m->vertexSize * 2);
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
        // PodarMarcasSinArista en SepararSeleccionEdit). Sin esto, Shift+D devolvia una copia
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
            if (m->animations[i]) NewActiveVertexAnimation(d, VertexAnimClonar(m->animations[i], d));
        d->meshTipo = m->meshTipo;
        d->meshSize = m->meshSize; d->meshSize2 = m->meshSize2; d->meshDepth = m->meshDepth;
        d->meshVerts = m->meshVerts; d->meshVerts2 = m->meshVerts2;
        d->meshSmooth = m->meshSmooth;
        // buffers PRECALCULADOS: se copian tal cual (mismas posiciones) -> el
        // duplicado queda identico (mismo contorno y MISMAS vertex-normals, sin
        // recalcular). Antes posRep no se copiaba y la vertex-normal del duplicado
        // salia distinta (no agrupaba por posicion -> 1 normal por vertice suelto).
        d->posRep = m->posRep;
        d->vertsAgrupados = m->vertsAgrupados;
        d->bordesBuf = m->bordesBuf;
        d->normFaceBuf = m->normFaceBuf;
        d->normCustomBuf = m->normCustomBuf;
        d->normVertBuf = m->normVertBuf;
        d->overlayLcache = m->overlayLcache;
        // CAPAS EDITABLES (deep copy). Sin esto el duplicado perdia sus uv maps / color layers y, sobre todo, sus
        // grupos de PESOS -> el skinning no tenia a que aplicar y la malla no deformaba. Van los DOS grupos (ver
        // el bloque VertexGroup/UVGroup en Mesh.h): vertexGroups = pesos por control-point (rig 3D) y uvGroups =
        // pesos por corner (rig 2D). Copiar solo los primeros dejaba el personaje 2D duplicado SIN pesos.
        for (size_t i = 0; i < m->uvMaps.size(); i++)       d->uvMaps.push_back(new UVMap(*m->uvMaps[i]));
        for (size_t i = 0; i < m->colorLayers.size(); i++)  d->colorLayers.push_back(new ColorLayer(*m->colorLayers[i]));
        for (size_t i = 0; i < m->vertexGroups.size(); i++) d->vertexGroups.push_back(new VertexGroup(*m->vertexGroups[i]));
        for (size_t i = 0; i < m->uvGroups.size(); i++)     d->uvGroups.push_back(new UVGroup(*m->uvGroups[i]));
        d->uvMapActivo = m->uvMapActivo; d->colorActivo = m->colorActivo; d->grupoActivo = m->grupoActivo;
        d->uvGrupoActivo = m->uvGrupoActivo;
        d->vertCtrlPoint = m->vertCtrlPoint; // vertice-render -> control-point (skinning / weight paint)
        // ARMATURES 2D (deep copy REAL): Armature2D es NO COPIABLE a proposito (es dueno de los punteros de
        // sus clips) -> Arm2DClonar clona huesos + clips, como se clonan las anims del armature 3D. Sin esto,
        // duplicar un personaje rigueado en 2D daba una copia SIN rig.
        for (size_t i = 0; i < m->armatures2d.size(); i++)  d->armatures2d.push_back(Arm2DClonar(m->armatures2d[i]));
        d->armature2dActivo = m->armature2dActivo;
        // EL REST del skinning 2D viaja tal cual, y el uv del duplicado se RE-DERIVA de el (uv = f(uv2dRest,
        // pose), el invariante de Mesh.h). Ojo con lo que parece un detalle: arriba se copio m->uv, que es el UV
        // POSADO; sin el rest, ese UV horneado se volvia la BASE de la copia y volver a pose identidad ya no
        // devolvia el mapeo original (la pose quedaba pegada para siempre). Con el rest + Armature2DAplicar la
        // copia queda visualmente IDENTICA al original y ademas despozable.
        d->uv2dRest = m->uv2dRest;
        d->Armature2DAplicar();
        d->last2dFrame = -999999; d->last2dAnim = -999; d->pose2dDirty = true;
        // MODIFICADORES (deep copy). Modifier no tiene recursos propios y su 'target' es un puntero COMPARTIDO
        // (el mismo esqueleto / objeto espejo) -> copia plana correcta.
        for (size_t i = 0; i < m->modificadores.size(); i++) d->modificadores.push_back(new Modifier(*m->modificadores[i]));
        d->modificadorActivo = m->modificadorActivo;
        d->skinArmature = m->skinArmature; // mismo esqueleto (los buffers skin* se regeneran en el primer render)
        nuevo = d;
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
            l->direccional = sl->direccional;
            l->attConstant = sl->attConstant; l->attLinear = sl->attLinear; l->attQuadratic = sl->attQuadratic;
            l->spotCutoff = sl->spotCutoff; l->spotExponent = sl->spotExponent;
        }
        nuevo = l;
    }
    else if (src->getType() == ObjectType::camera) {
        Camera* sc = (Camera*)src;
        Camera* d = new Camera(src->Parent, src->pos, src->rotEuler);
        // el LENTE tambien se duplica (antes fov/orto/near/far volvian al default)
        d->fov = sc->fov; d->orthographic = sc->orthographic;
        d->nearClip = sc->nearClip; d->farClip = sc->farClip;
        nuevo = d;
    }
    else if (src->getType() == ObjectType::empty) {
        nuevo = new Empty(src->Parent, src->pos);
    }
    else if (src->getType() == ObjectType::lod) {
        LOD* d = new LOD(src->Parent, src->pos);
        d->distancias       = ((LOD*)src)->distancias;
        d->soloCamaraActiva = ((LOD*)src)->soloCamaraActiva;
        nuevo = d;
    }
    else if (src->getType() == ObjectType::culling) {
        Culling* d = new Culling(src->Parent, src->pos);
        // TODOS los campos: 'activo' (el checkbox nuevo) y distanciaMax se olvidaban
        // -> la copia recortaba distinto que el original.
        d->activo           = ((Culling*)src)->activo;
        d->soloCamaraActiva = ((Culling*)src)->soloCamaraActiva;
        d->distanciaMax     = ((Culling*)src)->distanciaMax;
        nuevo = d;
    }
    else if (src->getType() == ObjectType::particulas) {
        Particulas* sp = (Particulas*)src;
        Particulas* d = new Particulas(src->Parent, src->pos);
        d->textura = sp->textura;       d->cantidad = sp->cantidad;
        d->vida = sp->vida;             d->tam = sp->tam;
        d->vel = sp->vel;               d->dispersion = sp->dispersion;
        d->gravedad = sp->gravedad;     d->aditivo = sp->aditivo;
        d->sustractivo = sp->sustractivo;
        for (int i = 0; i < 4; i++) d->color[i] = sp->color[i];
        d->desvanecer = sp->desvanecer; d->activo = sp->activo;
        d->variacion = sp->variacion;   d->turbulencia = sp->turbulencia;
        // las particulas VIVAS no se copian: el duplicado arranca su propia emision
        nuevo = d;
    }
    else if (src->getType() == ObjectType::collection && src->Parent) {
        // OJO: la raiz Scene tambien reporta tipo collection (Scene.cpp) pero no
        // tiene Parent -> el cast de aca nunca la toca.
        Collection* d = new Collection(src->Parent, src->pos);
        d->ordenarPorCamara = ((Collection*)src)->ordenarPorCamara;
        d->ordenarUnaVez    = ((Collection*)src)->ordenarUnaVez;
        nuevo = d;
    }
    else if (src->getType() == ObjectType::texto2d) {
        Texto2D* st = (Texto2D*)src;
        Texto2D* d = new Texto2D(src->Parent, src->pos);
        CopiarBase2D(d, st);
        d->texto = st->texto; d->tam = st->tam;
        d->alignH = st->alignH; d->alignV = st->alignV;
        d->fuente = st->fuente;
        d->tipo = st->tipo; d->decimales = st->decimales;
        d->lineas = st->lineas; d->autoTam = st->autoTam;
        d->palColor = st->palColor;
        for (int i = 0; i < 4; i++) d->color[i] = st->color[i];
        nuevo = d;
    }
    else if (src->getType() == ObjectType::imagen2d) {
        Imagen2D* si = (Imagen2D*)src;
        Imagen2D* d = new Imagen2D(src->Parent, src->pos);
        CopiarBase2D(d, si);
        d->textura = si->textura; d->modo = si->modo;
        d->usarAlpha = si->usarAlpha; d->filtrado = si->filtrado;
        d->palTinte = si->palTinte;
        for (int i = 0; i < 4; i++) d->color[i] = si->color[i];
        nuevo = d;
    }
    else if (src->getType() == ObjectType::rect2d) {
        Rect2D* sr = (Rect2D*)src;
        Rect2D* d = new Rect2D(src->Parent, src->pos);
        CopiarBase2D(d, sr);
        d->palColor = sr->palColor;
        for (int i = 0; i < 4; i++) d->color[i] = sr->color[i];
        nuevo = d;
    }
    else if (src->getType() == ObjectType::cont2d) {
        Contenedor2D* d = new Contenedor2D(src->Parent, src->pos);
        CopiarBase2D(d, (Contenedor2D*)src);
        nuevo = d;
    }
    else if (src->getType() == ObjectType::boton2d) {
        Boton2D* sb = (Boton2D*)src;
        Boton2D* d = new Boton2D(src->Parent, src->pos);
        CopiarBase2D(d, sb);
        d->texto = sb->texto; d->icono = sb->icono; d->fuente = sb->fuente;
        d->tam = sb->tam; d->pad = sb->pad;
        d->palFondo = sb->palFondo; d->palTexto = sb->palTexto; d->palBorde = sb->palBorde;
        d->palHover = sb->palHover; // (faltaba: el duplicado volvia al hover default)
        d->texturaFondo = sb->texturaFondo;
        d->bordeTexX = sb->bordeTexX; d->bordeTexY = sb->bordeTexY;
        d->escalaBordeTex = sb->escalaBordeTex;
        for (int i = 0; i < 4; i++) {
            d->colorFondo[i] = sb->colorFondo[i];
            d->colorTexto[i] = sb->colorTexto[i];
            d->colorBorde[i] = sb->colorBorde[i];
            d->colorHover[i] = sb->colorHover[i]; // idem
        }
        nuevo = d;
    }
    else if (src->getType() == ObjectType::video2d) {
        Video2D* sv = (Video2D*)src;
        Video2D* d = new Video2D(src->Parent, src->pos);
        CopiarBase2D(d, sv);
        d->video = sv->video; d->modo = sv->modo;
        d->loop = sv->loop; d->usarAlpha = sv->usarAlpha;
        d->reproducir = sv->reproducir; d->filtrado = sv->filtrado;
        nuevo = d;
    }
    else if (src->getType() == ObjectType::expandir2d) {
        Expandir2D* d = new Expandir2D(src->Parent, src->pos);
        CopiarBase2D(d, (Expandir2D*)src);
        nuevo = d;
    }
    else if (src->getType() == ObjectType::slice9) {
        Slice9* ss = (Slice9*)src;
        Slice9* d = new Slice9(src->Parent, src->pos);
        CopiarBase2D(d, ss);
        d->textura = ss->textura; d->escalaBorde = ss->escalaBorde;
        d->bordeX = ss->bordeX; d->bordeY = ss->bordeY;
        d->filtrado = ss->filtrado; d->palTinte = ss->palTinte;
        for (int i = 0; i < 4; i++) d->color[i] = ss->color[i];
        nuevo = d;
    }
    if (nuevo) {
        nuevo->pos = src->pos;
        nuevo->SetRotSnapshot(src->Rot(), src->rotEuler);   // copia exacta (con las vueltas del euler)
        nuevo->scale = src->scale;
        // nombre UNICO (antes pegaba ".001" a mano y duplicar dos veces daba DOS "Cubo.001")
        nuevo->SetNameObj(src->name);
        // EL STACK DE CONSTRAINTS. Va en el bloque COMUN y no en cada rama: es una
        // propiedad de Object, asi que un tipo nuevo la hereda sin que su autor se
        // acuerde de nada (mismo criterio que pos/rot/scale/nombre de estas lineas).
        // La copia tiene constraints PROPIOS -otro serial- pero mira a la MISMA fuente
        // (ver W3dCopiarConstraints en Objects.h): duplicar un cartel billboard da otro
        // cartel billboard, y duplicar algo que copia la posicion de "Guia" sigue
        // copiando la de "Guia".
        W3dCopiarConstraints(nuevo, src);
        // LOS SCRIPTS LUA (estilo Unity), tambien en el bloque COMUN: duplicar una
        // fruta con fruta.lua da otra fruta con el MISMO .lua y los MISMOS valores
        // configurados (refs/opciones/val_): despues se le cambia el val_id y listo.
        // W3dScriptDatos son structs de strings planos -> el copy-ctor ya es deep
        // (la copia no comparte nada con el original). El RUNTIME (las instancias
        // lua vivas) NO se copia: si el juego esta andando, sus scripts arrancan
        // cuando SimScriptsCambiados / el proximo Play los cargue.
        if (src->scriptDatos)
            nuevo->scriptDatos = new W3dScriptDatos(*src->scriptDatos);
    }
    return nuevo;
}

void DuplicatedObject(){
    if (estado != editNavegacion || InteractionMode != ObjectMode) return;
    if (!SceneCollection) return;

    // TODOS los seleccionados estan en ObjSelects, sin importar en que
    // coleccion esten. (Antes miraba solo los hijos DIRECTOS de SceneCollection
    // y por eso no duplicaba lo que estaba dentro de una Collection -> "no
    // andaba". NewInstance ya usaba ObjSelects, por eso ese si funcionaba.)
    // Se COPIA el vector: el ctor de cada copia llama DeseleccionarTodo y vacia
    // ObjSelects, asi que no podemos iterarlo en vivo.
    std::vector<Object*> seleccionados = ObjSelects;
    if (seleccionados.empty()) return;

    // copia REAL de cada uno (malla -> deep copy; luz/camara/empty -> sus
    // propiedades; NUNCA un link, eso es NewInstance/Duplicate Linked)
    std::vector<Object*> duplicados;
    for (size_t i = 0; i < seleccionados.size(); i++) {
        Object* d = W3dDuplicarUno(seleccionados[i]);
        if (d) duplicados.push_back(d);
    }
    if (duplicados.empty()) return;

    // dejar seleccionadas SOLO las copias y entrar en modo mover (como Blender:
    // Shift+D duplica y agarra; sino la copia queda justo encima y "no anda").
    // Es lo mismo que hace NewInstance, que ya funciona.
    DeseleccionarTodo();
    for (size_t i = 0; i < duplicados.size(); i++) duplicados[i]->Seleccionar();
    SetPosicion();
}

#include "W3dAviso.h"   // W3dAvisof / W3dNombreCorto: avisos con nombres SIN desbordar (declara Notificar)

// ====================== SEPARATE (Edit Mode: P / menu Mesh > Separate) ======================
// Mueve las caras SELECCIONADAS a un mesh NUEVO (misma pos/rot/escala, materiales, vertex groups, modificadores)
// y las borra del mesh actual. Como Blender (P > Selection). Seguimos editando el original. true si separo algo.
bool SepararSeleccionEdit(Mesh* m) {
    if (!m) return false;
    m->EnsureEdit();
    if (!m->edit) return false;

    // caras3d SELECCIONADAS segun el modo ACTUAL (vertice/borde/cara) ANTES de crear el clon: su ctor llama
    // DeseleccionarTodo() y en Edit Mode eso limpia la seleccion -> hay que guardarla aca. CarasSelPorModo
    // deriva las caras aunque estes en modo VERTICE (una cara cuenta si todos sus verts estan seleccionados).
    std::vector<char> sel3d;
    m->CarasSelPorModo(sel3d);
    int nSel = 0; for (size_t f = 0; f < sel3d.size(); f++) if (sel3d[f]) nSel++;
    if (nSel == 0)                      { Notificar(T("Separate: no faces selected"), true);      return false; }
    if (nSel == (int)m->faces3d.size()) { Notificar(T("Separate: select only some faces"), true); return false; }

    // 1) clon COMPLETO (geometria + transform + materiales + vertex groups + modificadores; queda parented al
    //    mismo padre). El ctor lo deja seleccionado a nivel objeto y limpia la sub-seleccion de 'm' (ver arriba).
    //    EL STACK DE CONSTRAINTS VIENE CON EL CLON, y ESE es su unico camino: W3dDuplicarUno ya
    //    lo copia en su bloque comun. Volver a llamar a W3dCopiarConstraints aca dejaria el pedazo
    //    con el stack DOS VECES (dos Copy Location identicos, uno tapando al otro, y el panel con
    //    la lista duplicada). El pedazo tiene que seguir mirando a lo mismo que la malla de la que
    //    salio, que es exactamente lo que hace la copia del clon.
    Mesh* nuevo = (Mesh*)W3dDuplicarUno(m);
    if (!nuevo) return false;
    nuevo->SetNameObj(m->name); // nombre UNICO (la puerta comun; ver W3dNombres.h)

    // 2) NUEVO: quedarse SOLO con las caras seleccionadas. NO se puede con BorrarSeleccionEdit (su seleccion es
    //    por POSICION: borrar las NO-seleccionadas cubre todas las esquinas del cubo y se lleva tambien las
    //    seleccionadas). Cirugia directa igual que las ops de MeshEdit: nf3d con las caras sel3d + survCorner
    //    (corners viejos, para conservar UV/color/vertex-groups) -> CompactarCapas + GenerarRender (remapea y
    //    DESCARTA los verts sin referencia).
    nuevo->PoblarCapas();
    { // LA CIRUGIA DEL PEDAZO VA **EN REPOSO**. El GenerarRender de abajo poda las marcas por
      // POSICION (PodarMarcasSinArista) y el clon puede nacer POSADO (el playhead fuera del
      // cuadro base). Posado, esa poda no puede decidir nada -las claves viven en el espacio de
      // reposo- y el pedazo quedaba con las 12 marcas del cubo entero, con 8 huerfanas que el
      // escritor tira con aviso en cada guardado. Con el guard, Separate da EXACTAMENTE el mismo
      // resultado con el playhead donde este. El guard ve que la topologia cambio adentro del
      // scope y deja el pedazo en reposo (que es el modelo); el playback lo vuelve a posar.
      W3dReposoVertexAnim reposo(nuevo);
      std::vector<MeshFace> nf3d; std::vector<int> survCorner; int L = 0;
      for (size_t f = 0; f < nuevo->faces3d.size(); f++) {
          int nc = (int)nuevo->faces3d[f].idx.size();
          if (f < sel3d.size() && sel3d[f]) {
              nf3d.push_back(nuevo->faces3d[f]);            // idx viejos; GenerarRender los remapea
              for (int c = 0; c < nc; c++) survCorner.push_back(L + c);
          }
          L += nc;
      }
      nuevo->faces3d.swap(nf3d);
      CompactarCapas(nuevo, survCorner);
      nuevo->GenerarRender();
    }
    // 2b) LAS MARCAS DE LA COPIA: el clon trajo los sets ENTEROS (es lo correcto para un clon
    //     exacto) pero esta copia se quedo con un PEDAZO; las que ya no son una arista de esta
    //     parte se van solas en el CalcularBordes del GenerarRender de arriba (ver
    //     Mesh::PodarMarcasSinArista). Antes habia que podar A MANO aca, operacion por
    //     operacion, y siempre faltaba una.

    // 3) ORIGINAL: borrar las caras SELECCIONADAS (borrar el conjunto CHICO es seguro). Re-seleccionar desde
    //    sel3d por si el ctor del clon limpio la sub-seleccion en Edit Mode.
    int modoPrev = EditSelectMode; EditSelectMode = SelFace; // BorrarSeleccionEdit lee la seleccion del modo actual
    m->EnsureEdit();
    if (m->edit) {
        EditMesh* e = m->edit;
        e->faceSel.assign(e->faces.size(), 0);
        for (size_t f = 0; f < e->faces.size(); f++)
            if (f < e->faceSrc.size()) { int f3 = e->faceSrc[f];
                if (f3 >= 0 && f3 < (int)m->faces3d.size() && sel3d[f3]) e->faceSel[f] = 1; }
        m->BorrarSeleccionEdit(SelFace);
    }
    EditSelectMode = modoPrev;
    // 3b) el ORIGINAL tambien perdio aristas, y sus marcas sobre lo que se fue tambien se
    //     podan solas (BorrarSeleccionEdit termina en GenerarRender -> CalcularBordes).

    ObjActivo = m; // seguimos editando el ORIGINAL (g_editMesh sigue siendo m)
    Notificar(T("Separate: new mesh created"), false);
    return true;
}

// JOIN (Ctrl+J, menu Object): une las MALLAS seleccionadas DENTRO del objeto ACTIVO, que conserva su transform
// EXACTO (pos/rot/escala, aunque sea hijo-de-hijo-de-hijo). Cada objeto mergeado se lleva al espacio local del
// activo con inv(worldActivo)*worldOtro -> queda visualmente donde estaba al mergear (aunque el activo o el otro
// esten movidos/rotados/escalados/anidados). Requiere: activo = Mesh + al menos otra Mesh seleccionada; los
// objetos NO-Mesh se ignoran; sin activo no hay join. Undo ATOMICO (Ctrl+Z restaura geo + objetos en 1 paso).
void JoinObjetos(){
    if (estado != editNavegacion || InteractionMode != ObjectMode) return;
    // el TARGET tiene que ser una malla SELECCIONADA. Si el activo no lo es (ej. quedo en el Cube por
    // defecto tras importar, fuera del armature) se hornearia todo en ese objeto extrano y se borrarian
    // las mallas del armature. Fallback: primera malla seleccionada.
    Object* active = ObjActivo;
    if (!active || active->getType() != ObjectType::mesh || !active->select) {
        active = NULL;
        for (size_t i = 0; i < ObjSelects.size(); i++) {
            Object* o = ObjSelects[i];
            if (o && o->select && o->getType() == ObjectType::mesh) { active = o; break; }
        }
    }
    if (!active) { Notificar(T("Join: no active mesh object"), true); return; }
    Mesh* am = (Mesh*)active;

    std::vector<Object*> merged; // mallas seleccionadas != activo (no-Mesh se ignoran; SIN duplicados)
    for (size_t i = 0; i < ObjSelects.size(); i++) {
        Object* o = ObjSelects[i];
        if (!o || o == active || !o->select || o->getType() != ObjectType::mesh) continue; // !select: pudo quedar en ObjSelects deseleccionado
        bool dup = false; for (size_t k = 0; k < merged.size(); k++) if (merged[k] == o) { dup = true; break; } // ObjSelects puede traer
        if (!dup) merged.push_back(o); // la misma malla repetida -> sin este dedup se anexaba 2 veces (caras dobladas -> malla "invisible"/rota)
    }
    if (merged.empty()) { Notificar(T("Join: select 2+ mesh objects (active is the target)"), true); return; }

    // BASE las dos puntas: el Join HORNEA los vertices del otro en la malla activa, asi que el
    // resultado no puede depender de la vista que se dibujo ultimo. Con la efectiva, juntar dos
    // mallas daba una geometria distinta segun donde estuviera la camara (ver Objects.h).
    Matrix4 Wa; active->GetWorldMatrixBase(Wa);   // mundo->local del activo (cadena de padres completa)
    Matrix4 invWa; Matrix4::InvertirAfin(Wa, invWa);

    UndoJoinIniciar(am); // snapshot de la geo del activo ANTES de anexar (undo atomico)

    for (size_t i = 0; i < merged.size(); i++) {
        Matrix4 Wo; merged[i]->GetWorldMatrixBase(Wo);
        Matrix4 M = invWa * Wo;               // local(otro) -> mundo -> local(activo)
        am->AnexarMallaTransformada((Mesh*)merged[i], M);
    }
    // rebuild: rearma las capas UV/color desde el render concatenado y re-mergea los verts.
    // LiberarCapas(false) PRESERVA los vertex groups (mergeados en AnexarMallaTransformada) -> el skinning sobrevive al join.
    // GenerarRender(false) PRESERVA las normales: el join no cambia la forma de ninguna de las mallas, asi que sus
    // normales siguen valiendo. Recalcularlas promediaba y REDONDEABA los bordes filosos que traia el archivo
    // (glTF/FBX guardan splits de normal); ahora quedan como estaban.
    am->LiberarCapas(false);
    am->PoblarCapas();
    am->GenerarRender(false);
    am->lastSkinFrame = -999999; // si el join era de mallas skinneadas: forzar el re-skin (el frame-gate sino lo saltaba)

    // borrar los mergeados (undo atomico): dejar select=true SOLO en ellos. Se limpia la seleccion de
    // TODA la escena (no solo ObjSelects) porque el DeleteUndo borra por el flag select recorriendo la
    // escena: una malla con select=true fuera de ObjSelects se borraria SIN haberse anexado.
    if (SceneCollection) SceneCollection->DeseleccionarCompleto(true);
    for (size_t i = 0; i < merged.size(); i++) merged[i]->select = true;
    UndoJoinConfirmar(); // DeleteUndo(los select=true) empaquetado con la geo -> 1 comando

    DeseleccionarTodo(); // seleccion final: solo el activo (resultado del join)
    active->Seleccionar();
    ObjActivo = active;
    Notificar(std::string("Joined into ") + active->name, false);
}

// APPLY (Alt+A, menu Object > Apply): hornea el transform del objeto en la MALLA y resetea ese componente, dejando
// la malla VISUALMENTE en su lugar. what: 0=Location, 1=Rotation, 2=Scale, 3=All Transforms.
//   Location -> pos queda (0,0,0) (el origen va al 0; la malla queda donde estaba)
//   Rotation -> rot queda 0° (la malla queda rotada donde estaba)
//   Scale    -> scale queda 1 (una escala 1.34 pasa a 1; la malla mantiene su tamaño)
// Matematica: v_new = inv(M_reset) * M_actual * v, con M = T*R*S LOCAL del objeto -> M_reset*v_new = M_actual*v
// (la malla no se mueve en el espacio del padre). Aplica a TODAS las mallas seleccionadas. Undo atomico.
// libera el cache de vertex-animation + fuerza el re-skin de TODAS las mallas de la escena que deforman con 'a'. Tras
// hornear el transform del objeto en los huesos, el cache tiene deformaciones VIEJAS y su firma NO incluye la rest ->
// hay que liberarlo a mano, sino SkinearMesh reproduce el frame stale en vez de re-skinnear. (Lo usa tambien el undo.)
void InvalidarSkinDeArmature(Armature* a){
    if (!a || !SceneCollection) return;
    struct L { static void rec(Object* o, Armature* a){ if(!o) return;
        if (o->getType()==ObjectType::mesh){ Mesh* m=(Mesh*)o; if (m->skinArmature==a){ m->LiberarSkinCache(); m->lastSkinFrame=-999999; } }
        for (size_t i=0;i<o->Childrens.size();i++) rec(o->Childrens[i], a); } };
    L::rec(SceneCollection, a);
    a->lastPoseFrame=-999999; a->lastPoseAnim=-999; a->poseDirty=false;
}

void AplicarTransform(int what){
    if (estado != editNavegacion || InteractionMode != ObjectMode) return;
    std::vector<Object*> mallas; std::vector<Armature*> arms;
    for (size_t i=0;i<ObjSelects.size();i++){ Object* o=ObjSelects[i]; if(!o) continue;
        if (o->getType()==ObjectType::mesh) mallas.push_back(o);
        else if (o->getType()==ObjectType::armature) arms.push_back((Armature*)o); }
    if (mallas.empty() && arms.empty()) { Notificar(T("Apply: select mesh or armature object(s)"), true); return; }

    Quaternion idRot = Quaternion::FromEulerXYZ(0.0f,0.0f,0.0f); // identidad
    UndoApplyIniciar(); // snapshot de transforms + geo de mallas + rest de huesos de armatures (undo atomico)

    // MALLAS: hornear el transform en los VERTICES (v_new = inv(M_reset)*M_actual*v)
    for (size_t i=0;i<mallas.size();i++){
        Object* o = mallas[i];
        Vector3 pos2 = o->pos; Quaternion rot2 = o->Rot(); Vector3 scl2 = o->scale; // valores reseteados
        if (what==0 || what==3) pos2 = Vector3(0,0,0);
        if (what==1 || what==3) rot2 = idRot;
        if (what==2 || what==3) scl2 = Vector3(1,1,1);
        Matrix4 M  = o->BuildMatrix(o->pos, o->Rot(), o->scale); // actual (T*R*S)
        Matrix4 Mp = o->BuildMatrix(pos2,  rot2,  scl2);       // reseteado
        Matrix4 invMp; Matrix4::InvertirAfin(Mp, invMp);
        Matrix4 B = invMp * M;                                 // v_new = inv(M_reset)*M_actual*v
        ((Mesh*)o)->AplicarMatriz(B);
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
        Matrix4 invMp; Matrix4::InvertirAfin(Mp, invMp);
        Matrix4 B = invMp * M;                                 // se hornea en espacio nodo (world_FK'=B*world_FK)
        HornearTransformEnHuesos(a, B);
        a->pos = pos2; a->scale = scl2; a->SetRot(rot2);
        InvalidarSkinDeArmature(a); // libera el cache + re-skinnea las mallas hijas a la nueva rest
    }
    UndoApplyConfirmar();
    const char* nom = (what==0)?"Location":(what==1)?"Rotation":(what==2)?"Scale":"All Transforms";
    Notificar(std::string("Apply ") + nom + ": baked", false);
}

// Set Active Object as Camera (Ctrl+Numpad 0 / menu View > Cameras): hace que el objeto ACTIVO sea la CAMARA
// activa de la escena. Solo si el activo ES una camara (si no es camara NO se puede -> no hace nada, false).
bool SetActiveObjectAsCamera(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::camera) { Notificar(T("Set Active Camera: the active object is not a camera"), true); return false; }
    CameraActive = (Camera*)ObjActivo; // Camera : public Object (1ra base) -> cast offset 0
    return true;
}

// ============================================================================
//  EMBUDO UNICO DEL REPARENT (W3dAdjuntarA). TODA mudanza de un objeto en el arbol
//  pasa por aca: W3dReparent (Ctrl+P / Alt+P), ReparentSimple, ReparentKeepTransform
//  y MoverJuntoA -y por ellas el drag&drop y el modo mover del outliner, la tarjeta
//  Parent y el editor 2D-. Hace DOS cosas:
//   1) la cirugia de punteros: sacar del padre viejo e insertar en el nuevo (al final,
//      o junto a un hermano de referencia).
//   2) NOMBRES: el espacio de nombres de los OBJETOS es POR ESCENA (la raiz UI, ver
//      W3dNombreScopeDe), asi que mudar un objeto de una escena a otra FABRICA
//      homonimos. Era el fallo F3: abrir whiskpaddle y hacer "parent BtnSonido Pelota"
//      dejaba TRES nombres repetidos en el scope 'Juego', sin un solo aviso. Se
//      re-uniquifica el SUBARBOL ENTERO que se mudo, con la puerta de siempre
//      (Object::SetNameObj -> NombreLibre, que se excluye a si mismo: si el nombre
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
//  ReparentKeepTransform y MoverJuntoA). Motivo: este embudo hace SOLO la cirugia de
//  punteros y el TRANSFORM lo reescriben los llamadores DESPUES (los caminos Keep
//  Transform), asi que un snapshot tomado aca guardaria coordenadas que la operacion todavia
//  va a pisar. El paso de RENUMERACION que se empuja mas abajo queda FUNDIDO con el de la
//  mudanza (grupo de undo abierto por la puerta): un solo Ctrl+Z devuelve arbol Y nombres.
// ============================================================================
static void QuitarDePadre(Object* obj);

// ============================================================================
//  UNA MUDANZA = UN PASO DE UNDO (y N mudanzas de un Ctrl+P = un paso tambien)
//
//  RAII con CONTADOR DE REENTRADA porque las puertas se llaman entre ellas: MoverJuntoA
//  llama a ReparentKeepTransform y despues vuelve a W3dAdjuntarA. Sin el contador, la
//  llamada de adentro capturaba un "estado previo" que ya era el de la mitad de la
//  operacion (el objeto ya mudado) y el Ctrl+Z devolvia el arbol a medio camino.
//  El grupo (UndoGrupoIniciar/Fin) junta el paso del ARBOL con el de los NOMBRES que
//  empuja el embudo al cruzar de escena.
// ============================================================================
static int g_repNivel = 0;
struct PasoReparent {
    bool raiz, capturo;
    PasoReparent(Object* obj) : raiz(g_repNivel == 0), capturo(false) {
        g_repNivel++;
        if (!raiz) return;                 // reentrada: la puerta de afuera ya capturo
        // W3dNombresCargando = "el que llama es dueno del paso de undo" (los loaders, que no
        // dejan historial, y el modal 'mover' del outliner, que captura al entrar y empuja UN
        // paso al confirmar). Aca no se toca NADA en ese caso: ni se captura ni se confirma,
        // porque confirmar cerraria el pendiente del modal a mitad de camino.
        if (W3dNombresCargando) return;
        capturo = true;
        UndoGrupoIniciar();
        UndoReparentIniciar(obj);
    }
    ~PasoReparent() {
        g_repNivel--;
        if (!capturo) return;
        UndoReparentConfirmar();           // no pushea si el objeto quedo donde estaba
        UndoGrupoFin();
    }
};
// N mudanzas en UN SOLO Ctrl+Z (los bucles de Set/ClearParentSeleccion y los menus de la
// tarjeta Parent / del Editor 2D, que iteran la seleccion por su cuenta).
void ReparentGrupoIniciar() { UndoGrupoIniciar(); }
void ReparentGrupoFin()     { UndoGrupoFin(); }

// ============================================================================
//  EL CRUCE ENTRE LOS DOS MUNDOS (3D <-> elementos 2D de una escena UI) - FALLO B
//
//  El proyecto se guarda en DOS archivos: el .w3d (la escena) y un .w3dui POR ESCENA UI
//  (los elementos 2D, que es lo que ademas consume el runtime de los juegos). El .w3d corta
//  la recursion al llegar a un elemento 2D (HijoVaEnOtroArchivo, GuardarW3D.cpp) y el
//  .w3dui escribe SOLO tipos 2D (una lista blanca, UI2DFormato.cpp). O sea que un objeto
//  colgado del lado equivocado no lo escribe NINGUNO de los dos: desaparece al guardar, sin
//  un solo aviso. Los dos sentidos se arreglan distinto, y a proposito:
//
//   (1) OBJETO 3D bajo un ELEMENTO 2D -> SE PERMITE y viaja en el .w3d, que es el archivo
//       "maestro" (tiene el arbol entero y es el unico de los dos que sabe serializar una
//       malla/camara/luz). Se escribe bajo el nodo UI con el campo "padre2d" y al abrir se
//       re-cuelga del elemento exacto (GuardarW3D.cpp / import_w3d.cpp). El .w3dui no lo
//       lleva y esta bien: el runtime de la UI no instancia objetos 3D.
//   (2) ELEMENTO 2D bajo un objeto 3D -> SE BLOQUEA, con aviso. No hay archivo que lo pueda
//       guardar: el .w3dui solo alcanza lo que cuelga de SU UI, y el .w3d no sabe escribir
//       un elemento 2D (caia en la rama generica y volvia convertido en un Empty, o sea
//       corrupcion silenciosa: el widget perdia tipo, tamano, anclas, paleta y su lugar en
//       el layout). Ademas un elemento 2D FUERA de su escena no tiene sentido en el modelo:
//       el layout, el lienzo y el espacio de nombres son POR ESCENA (W3dNombreScopeDe).
//       Bloquear no es perder nada: la mudanza simplemente no ocurre y el usuario se entera.
// ============================================================================
bool UI2D_EsElemento2D(Object* o);   // render/UIOverlay.h (no se incluye: es header de render)
static bool CruceUIValido(Object* obj, Object* nuevoPadre) {
    if (!obj || !nuevoPadre) return false;
    if (!UI2D_EsElemento2D(obj)) return true;   // (1) el 3D bajo un 2D si se permite
    if (nuevoPadre->getType() == ObjectType::ui || UI2D_EsElemento2D(nuevoPadre)) return true;
    W3dAvisof(true, "'%s' es un elemento 2D: solo puede colgar de su escena UI o de otro "
                    "elemento 2D (no se movio).", W3dNombreCorto(obj->name).c_str());
    w3dLogfW("[ui2d] reparent bloqueado: '%s' (elemento 2D) -> '%s' (fuera de una escena UI)",
             obj->name.c_str(), nuevoPadre->name.c_str());
    return false;
}

// junta el subarbol de 'o' (incluido) en PRE-ORDEN, el mismo del recorrido por nombre
static void JuntarSubarbol(Object* o, std::vector<Object*>& out) {
    if (!o) return;
    out.push_back(o);
    for (size_t i = 0; i < o->Childrens.size(); i++) JuntarSubarbol(o->Childrens[i], out);
}

static void W3dAdjuntarA(Object* obj, Object* nuevoPadre, Object* refHermano, bool despues) {
    if (!obj || !nuevoPadre) return;
    Object* scopeViejo = W3dNombreScopeDe(obj);
    QuitarDePadre(obj);
    size_t idx = nuevoPadre->Childrens.size();
    if (refHermano)
        for (size_t i = 0; i < nuevoPadre->Childrens.size(); i++)
            if (nuevoPadre->Childrens[i] == refHermano) { idx = i + (despues ? 1 : 0); break; }
    if (idx > nuevoPadre->Childrens.size()) idx = nuevoPadre->Childrens.size();
    nuevoPadre->Childrens.insert(nuevoPadre->Childrens.begin() + idx, obj);
    obj->Parent = nuevoPadre;

    // mismo espacio de nombres (reordenar entre hermanos, o mover dentro de la misma
    // escena): no hay nada que renumerar. Es el caso comun -> se sale barato.
    if (W3dNombreScopeDe(obj) == scopeViejo) return;
    if (W3dNombresCargando) return;   // los loaders arman el arbol y reparan al final

    std::vector<Object*> sub;
    JuntarSubarbol(obj, sub);
    bool hayCambio = false;
    for (size_t i = 0; i < sub.size() && !hayCambio; i++)
        if (sub[i]->NombreLibre(sub[i]->name) != sub[i]->name) hayCambio = true;
    if (!hayCambio) return;

    // UN solo paso de undo con TODAS las puntas (aunque alguna no cambie: un
    // RenameUndo sobre un nombre igual es un no-op)
    // Object::name SI es un destino 'Directo' legitimo (el objeto no se recrea; se valida
    // que siga en la escena, ver W3dRenameDest en Undo.h)
    std::vector<W3dRenameDest> puntas;
    for (size_t i = 0; i < sub.size(); i++) puntas.push_back(W3dDestNombre(&sub[i]->name));
    UndoCapturarRenames(puntas);

    int n = 0; std::string primeros;
    for (size_t i = 0; i < sub.size(); i++) {
        const std::string viejo = sub[i]->name;
        sub[i]->SetNameObj(viejo);          // uniquifica en el scope NUEVO
        if (sub[i]->name == viejo) continue;
        n++;
        // los nombres van RECORTADOS al aviso (W3dAviso.h): son texto del usuario/archivo, sin cota
        if (n <= 3) { if (!primeros.empty()) primeros += ", ";
                      primeros += W3dNombreCorto(viejo) + " -> " + W3dNombreCorto(sub[i]->name); }
        w3dLogfW("[nombres] reparent: '%s' -> '%s' (el nombre ya estaba tomado en la escena destino)",
                 viejo.c_str(), sub[i]->name.c_str());
    }
    if (n > 0)
        W3dAvisof(true, "%d nombre(s) renumerados al cambiar de escena (%s%s). Revisa tus scripts lua.",
                  n, primeros.c_str(), n > 3 ? ", ..." : "");
}

static void W3dReparent(Object* obj, Object* nuevoPadre) {
    if (!obj || !nuevoPadre || obj == nuevoPadre) return;
    Object* viejoPadre = obj->Parent ? obj->Parent : SceneCollection;
    if (viejoPadre == nuevoPadre) return;
    if (!CruceUIValido(obj, nuevoPadre)) return;
    PasoReparent paso(obj);   // Ctrl+Z deshace esta mudanza (ver el bloque de arriba)
    // conservar la posicion GLOBAL (v1: solo traslacion). BASE: el pos que sale de aca se
    // ESCRIBE en el objeto y va al .w3d; con la efectiva, reparentar algo con un constraint
    // (o colgado de algo con constraint) le horneaba la camara adentro del pos (ver Objects.h).
    Vector3 g = obj->GetGlobalPositionBase();
    Vector3 gp = nuevoPadre->GetGlobalPositionBase();
    W3dAdjuntarA(obj, nuevoPadre, NULL, false);
    obj->pos = g - gp;
}

// rotacion GLOBAL por la cadena de padres (R = R_padre * R_local). NO-static:
// el transform de sub-elementos de malla (editor) la usa para world<->local.
// (RotGlobalDe se MUDO a libs/Whisk3DCore/objects/Objects.cpp: es una consulta
//  pura del grafo de escena -- la rotacion acumulada por la cadena de padres --
//  que necesitan tambien los builds SIN editor, como el runtime de un juego.)

// escala GLOBAL acumulada (componente a componente, sin shear)
Vector3 ScaleGlobalDe(Object* o) {
    Vector3 s(1.0f, 1.0f, 1.0f);
    while (o) {
        s.x *= o->scale.x;
        s.y *= o->scale.y;
        s.z *= o->scale.z;
        o = o->Parent;
    }
    return s;
}

static void QuitarDePadre(Object* obj) {
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
static bool ReparentValido(Object* obj, Object* nuevoPadre) {
    if (!obj || !nuevoPadre || obj == nuevoPadre) return false;
    for (Object* q = nuevoPadre; q; q = q->Parent) {
        if (q == obj) return false;
    }
    return true;
}

// reparenta conservando la transformacion LOCAL (el objeto puede saltar)
void ReparentSimple(Object* obj, Object* nuevoPadre) {
    if (!ReparentValido(obj, nuevoPadre)) return;
    if ((obj->Parent ? obj->Parent : SceneCollection) == nuevoPadre) return;
    if (!CruceUIValido(obj, nuevoPadre)) return;
    PasoReparent paso(obj);   // Ctrl+Z deshace esta mudanza
    W3dAdjuntarA(obj, nuevoPadre, NULL, false);
}

// reparenta MANTENIENDO la transformacion GLOBAL: recalcula pos, rot y
// escala locales para que el objeto quede exactamente donde estaba
void ReparentKeepTransform(Object* obj, Object* nuevoPadre) {
    if (!ReparentValido(obj, nuevoPadre)) return;
    if ((obj->Parent ? obj->Parent : SceneCollection) == nuevoPadre) return;
    if (!CruceUIValido(obj, nuevoPadre)) return;
    // el paso de undo se abre ANTES de tocar nada: este camino reescribe pos/rot/escala
    // DESPUES de la cirugia de punteros y el snapshot tiene que ser el de antes de todo
    PasoReparent paso(obj);

    // las tres componentes por el MISMO camino: RotGlobalDe/ScaleGlobalDe leen los CAMPOS (o
    // sea la base), asi que la posicion tambien tiene que ser BASE o la ida y vuelta
    // mundo<->local queda descuadrada. Y este camino ESCRIBE pos/rot/escala (van al .w3d).
    Quaternion rg = RotGlobalDe(obj);
    Vector3 sg = ScaleGlobalDe(obj);
    Vector3 pg = obj->GetGlobalPositionBase();

    Quaternion prg = RotGlobalDe(nuevoPadre);
    Vector3 psg = ScaleGlobalDe(nuevoPadre);
    Vector3 ppg = nuevoPadre->GetGlobalPositionBase();

    W3dAdjuntarA(obj, nuevoPadre, NULL, false);

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
void MoverJuntoA(Object* obj, Object* ref, bool despues) {
    if (!obj || !ref || obj == ref) return;
    Object* padre = ref->Parent ? ref->Parent : SceneCollection;
    if (!ReparentValido(obj, padre) && padre != (obj->Parent ? obj->Parent : SceneCollection)) return;
    if (!CruceUIValido(obj, padre)) return;
    // REENTRADA: abajo se llama a ReparentKeepTransform (que tambien es una puerta) y despues
    // se vuelve al embudo. El contador de PasoReparent hace que la captura sea UNA sola, con el
    // estado de ANTES de todo, y que el grupo se cierre recien aca.
    PasoReparent paso(obj);
    if ((obj->Parent ? obj->Parent : SceneCollection) != padre) {
        ReparentKeepTransform(obj, padre);
        if ((obj->Parent ? obj->Parent : SceneCollection) != padre) return;
    }
    // reubicar dentro del vector del padre (mismo padre -> mismo scope: el embudo
    // no toca nombres, solo mueve el puntero)
    W3dAdjuntarA(obj, padre, ref, despues);
}

void SetParentSeleccion() {
    if (!ObjActivo || !SceneCollection) return;
    // todos los seleccionados (menos el activo) pasan a ser hijos del activo.
    // Por ObjSelects, NO solo los hijos DIRECTOS de la raiz: lo seleccionado
    // dentro de una coleccion tambien se emparenta (mismo criterio que
    // ClearParentSeleccion, que ya recorre en profundidad).
    std::vector<Object*> sel;
    for (size_t i = 0; i < ObjSelects.size(); i++) {
        Object* o = ObjSelects[i];
        if (!o || !o->select || o == ObjActivo) continue;
        // no emparentar a un ANCESTRO del activo (crearia un ciclo en el arbol)
        bool ancestro = false;
        for (Object* p = ObjActivo->Parent; p; p = p->Parent) if (p == o) { ancestro = true; break; }
        if (!ancestro) sel.push_back(o);
    }
    // emparentar N objetos es UNA accion del usuario -> UN solo Ctrl+Z
    ReparentGrupoIniciar();
    for (size_t i = 0; i < sel.size(); i++) {
        W3dReparent(sel[i], ObjActivo);
    }
    ReparentGrupoFin();
}

void ClearParentSeleccion() {
    if (!SceneCollection) return;
    // los seleccionados vuelven a colgar de la raiz (recorrer copia DFS)
    std::vector<Object*> sel;
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {
        // buscar seleccionados anidados
        std::vector<Object*> pila;
        pila.push_back(SceneCollection->Childrens[c]);
        while (!pila.empty()) {
            Object* o = pila.back();
            pila.pop_back();
            if (o->select && o->Parent && o->Parent != SceneCollection) sel.push_back(o);
            for (size_t h = 0; h < o->Childrens.size(); h++) pila.push_back(o->Childrens[h]);
        }
    }
    ReparentGrupoIniciar();   // desemparentar N objetos = UN solo Ctrl+Z
    for (size_t i = 0; i < sel.size(); i++) {
        W3dReparent(sel[i], SceneCollection);
    }
    ReparentGrupoFin();
}

void NewInstance(){
	if (estado != editNavegacion || InteractionMode != ObjectMode){return;};

	// COPIA de la seleccion ANTES de iterar: el ctor de Object pisa la seleccion
	// global (DeseleccionarTodo) -> iterar ObjSelects en vivo leia un vector ya
	// vaciado con 2+ seleccionados (mismo fix que DuplicatedObject)
    std::vector<Object*> sel = ObjSelects;
    for (int i = (int)sel.size() - 1; i >= 0; i--) {
        Object* obj = sel[i];            // objeto original
        if (!obj) continue;

		Instance* instance = new Instance(obj->Parent, obj);
		obj->select = false;
		instance->select = true;
		if (ObjActivo == obj) ObjActivo = instance;
	}
	SetPosicion();
}

// reconstruye el quaternion REAL del objeto activo desde los valores de DISPLAY
// que se editaron, segun el modo de rotacion. La llaman los campos del editor.
void SincronizarRotacionActiva(){
	if (!ObjActivo) return;
	if (ObjActivo->rotMode == RotQuaternion){
		ObjActivo->SetRot(ObjActivo->rotQuat);  // se editaron w/x/y/z directo en la copia de display
	} else if (ObjActivo->rotMode == RotAxisAngle){
		ObjActivo->SetRotAxisAngle(ObjActivo->rotAxis, ObjActivo->rotAngle);   // la puerta normaliza el eje
	} else { // RotEulerXYZ: el euler que se tipeo ES el dato (puede ser 405) -> el quaternion se deriva de el
		ObjActivo->SetRotEuler(ObjActivo->rotEuler);
	}
}

void SetRotacion(int dx, int dy){
	float ang = (dx + dy) * 0.1f;   // sensibilidad (ajustable)
	gAnguloTransform += ang;        // total acumulado (para la barra de estado)

	for (size_t o = 0; o < estadoObjetos.size(); o++) {
		Object& obj = *estadoObjetos[o].obj;
		// rotar alrededor del eje constrenido EN WORLD (la identidad de
		// conjugacion hace que pre-multiplicar funcione para global/local/view:
		// local = el propio eje rotado por obj.rot).
		Vector3 axis;
		if (axisSelect == ViewAxis || axisSelect == XYZ) axis = camForward; // libre = eje de vista
		else axis = EjeOrientado(obj, axisSelect);
		obj.SetRot(Quaternion::FromAxisAngle(axis, ang) * obj.Rot());
	}
	AplicarPivotATransform(); // gira las posiciones alrededor del pivote
	SnapAjustarObjRot(); // imanta: el activo apunta al target (si snap ON)
	{ extern bool g_objetosMovidos; g_objetosMovidos = true; } // Mirror con target depende de la rotacion/posicion
}

// rotacion ORBITAL/gimbal: gira alrededor de los ejes de la VISTA (en mundo).
// izq/der (dx) -> yaw sobre el eje vertical de la vista (camUp); arr/ab (dy) ->
// pitch sobre el horizontal (camRight). Incremental sobre obj.rot (como
// SetRotacion), con quaterniones (sin gimbal-lock). Tecla V (PC) / 0 (Symbian).
void RotarOrbital(int dx, int dy){
	float yaw   = dx * 0.1f;
	float pitch = dy * 0.1f;
	gAnguloTransform += (dx + dy) * 0.1f; // total para la barra de estado
	Quaternion q = Quaternion::FromAxisAngle(camUp, yaw)      // izq/der: era -yaw (invertido)
	             * Quaternion::FromAxisAngle(camRight, pitch);
	for (size_t o = 0; o < estadoObjetos.size(); o++) {
		Object& obj = *estadoObjetos[o].obj;
		obj.SetRot(q * obj.Rot()); // pre-multiplica: gira en los ejes de la vista
	}
	AplicarPivotATransform(); // gira las posiciones alrededor del pivote
	{ extern bool g_objetosMovidos; g_objetosMovidos = true; } // Mirror con target depende de la rotacion/posicion
}

// alterna el modo de rotacion LIBRE entre trackball (eje de vista) y ORBITAL.
void ToggleRotacionOrbital(){
	if (estado != rotacion) return;
	axisSelect = (axisSelect == OrbitalAxis) ? ViewAxis : OrbitalAxis;
	gTrackballCap = false; // re-captura el angulo si vuelve al trackball
	ReestablecerEstado(false);
}

void SetRotacion(){
	// la UI y sus elementos 2D NO se transforman desde el 3D (la UI es orden de dibujo;
	// los textos se mueven en el Editor 2D). Borrarlos si esta permitido.
	if (ObjActivo && (ObjActivo->getType() == ObjectType::ui || ObjActivo->getType() == ObjectType::texto2d ||
	                  ObjActivo->getType() == ObjectType::imagen2d ||
	                  ObjActivo->getType() == ObjectType::rect2d ||
	                  ObjActivo->getType() == ObjectType::cont2d ||
	                  ObjActivo->getType() == ObjectType::slice9 ||
	                  ObjActivo->getType() == ObjectType::boton2d ||
	                  ObjActivo->getType() == ObjectType::expandir2d ||
	                  ObjActivo->getType() == ObjectType::video2d)) return;
	//si no hay objetos. En Edit Mode NO se transforma el objeto (es para editar la malla)
	if (ObjActivo && InteractionMode == ObjectMode && ObjActivo->select && estado == editNavegacion){
		if (!guardarEstado()) return;
		estado = rotacion; g_xformPrimerMov = true; // primer motion en cero (no salta)
		valorRotacion = 0;
		gAnguloTransform = 0.0f; // arranca el conteo del angulo
		gTrackballCap = false;   // re-captura el angulo inicial del trackball
		// R arranca LIBRE = "rotar desde la vista" (trackball alrededor del eje
		// de camara, angulo segun el mouse al pivot). X/Y/Z constriñen a un eje.
		axisSelect = ViewAxis;
		ToolbarRegistrarAccion(TBRotate); // historial de la barra de herramientas
	}
};

void SetScale(int dx, int dy, float factor){
	//std::cout << "estadoObjetos size: " << estadoObjetos.size() << std::endl;
	float d = (dx + dy) * factor;
	for (size_t o = 0; o < estadoObjetos.size(); o++) {
		Object& obj = *estadoObjetos[o].obj;
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
	AplicarPivotATransform(); // aleja/acerca las posiciones del pivote segun el factor
	SnapAjustarObjScale(); // imanta: la base toca el target radialmente (si snap ON)
	{ extern bool g_objetosMovidos; g_objetosMovidos = true; } // Mirror con target depende de la posicion/escala
}

void SetEscala(){
	// la UI y sus elementos 2D NO se transforman desde el 3D (ver SetPosicion)
	if (ObjActivo && (ObjActivo->getType() == ObjectType::ui || ObjActivo->getType() == ObjectType::texto2d ||
	                  ObjActivo->getType() == ObjectType::imagen2d ||
	                  ObjActivo->getType() == ObjectType::rect2d ||
	                  ObjActivo->getType() == ObjectType::cont2d ||
	                  ObjActivo->getType() == ObjectType::slice9 ||
	                  ObjActivo->getType() == ObjectType::boton2d ||
	                  ObjActivo->getType() == ObjectType::expandir2d ||
	                  ObjActivo->getType() == ObjectType::video2d)) return;
	//XYZ tiene escala. En Edit Mode NO se transforma el objeto (es para editar la malla)
	if (ObjActivo && InteractionMode == ObjectMode && ObjActivo->select && estado == editNavegacion){
		if (!guardarEstado()) return;
		estado = EditScale; g_xformPrimerMov = true; // primer motion en cero (no salta)
		axisSelect = XYZ;
		ToolbarRegistrarAccion(TBScale); // historial de la barra de herramientas
	}
};

// suma de los ejes ACTIVOS (en MUNDO, segun orientacion) para el valor numerico
static Vector3 EjesActivosObj(Object& o){
	if (axisSelect==X||axisSelect==Y||axisSelect==Z) return EjeOrientado(o, axisSelect);
	if (axisSelect==PlaneX) return EjeOrientado(o,Y)+EjeOrientado(o,Z);
	if (axisSelect==PlaneY) return EjeOrientado(o,X)+EjeOrientado(o,Z);
	if (axisSelect==PlaneZ) return EjeOrientado(o,X)+EjeOrientado(o,Y);
	return EjeOrientado(o,X)+EjeOrientado(o,Y)+EjeOrientado(o,Z); // libre
}

// ENTRADA NUMERICA: aplica un valor EXACTO al transform de OBJETOS en curso, desde el
// estado guardado (snapshot). translate=distancia, rotacion=grados, escala=factor.
void SetTransformNumerico(float v){
	for (size_t o = 0; o < estadoObjetos.size(); o++) {
		SaveState& st = estadoObjetos[o];
		Object& obj = *st.obj;
		if (estado == translacion){
			obj.pos = st.pos + EjesActivosObj(obj) * v;
		} else if (estado == rotacion){
			Vector3 ax;
			if (axisSelect==ViewAxis||axisSelect==XYZ||axisSelect==OrbitalAxis) ax = camForward;
			else ax = EjeOrientado(obj, axisSelect);
			Quaternion q = Quaternion::FromAxisAngle(ax, v) * st.rot;
			q.normalize();
			// El euler NO se puede re-derivar acá: este camino aplica el angulo de UNA sola vez desde el snapshot
			// (tipeaste "360"), y conservar vueltas eligiendo "la forma mas parecida a la anterior" solo funciona
			// con pasos chicos: para un salto de 360 la mas parecida a 0 es 0, y la vuelta se perdia.
			// El angulo pedido (v) ES el dato: se le suma al euler del snapshot sobre el eje en el que se roto.
			// Con un eje X/Y/Z global el euler es exactamente el del snapshot + v en ese componente; en cualquier
			// otro eje (vista, local, plano) el euler no se descompone asi -> ahi se re-deriva como siempre.
			bool ejeSimple = (transformOrientation == GlobalOrient &&
			                  (axisSelect == X || axisSelect == Y || axisSelect == Z));
			if (ejeSimple){
				Vector3 e = st.rotEuler;
				// EjeMundo: user X->x, Y->z (profundidad), Z->y (arriba)
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
	gAnguloTransform = v;
	AplicarPivotATransform();
}

void SetTranslacionObjetos(int dx, int dy, float speed){
	for (size_t o = 0; o < estadoObjetos.size(); o++) {
		Object& obj = *estadoObjetos[o].obj;
		Vector3 libre = camRight * (dx * speed) + camUp * (-dy * speed); // plano camara
		if (axisSelect == X || axisSelect == Y || axisSelect == Z) {
			// un eje: proyectar el movimiento de PANTALLA sobre la direccion en
			// que se ve el eje (relativo a la vista). Asi arrastrar "hacia donde
			// apunta el eje en pantalla" mueve en +eje (no se invierte).
			Vector3 axis = EjeOrientado(obj, axisSelect);
			float amount = (dx * axis.Dot(camRight) - dy * axis.Dot(camUp)) * speed;
			obj.pos += axis * amount;
		} else if (axisSelect == PlaneX || axisSelect == PlaneY || axisSelect == PlaneZ) {
			// plano: movimiento libre MENOS la componente del eje excluido
			int ex = (axisSelect == PlaneX) ? X : (axisSelect == PlaneY) ? Y : Z;
			Vector3 axis = EjeOrientado(obj, ex);
			obj.pos += libre - axis * libre.Dot(axis);
		} else {
			obj.pos += libre; // libre (3 ejes)
		}
	}
	SnapAjustarObjMove(); // imanta la base de la seleccion al target (si snap ON)
	{ extern bool g_objetosMovidos; g_objetosMovidos = true; } // Mirror con target: su plano depende de la posicion
}

// ====================================================================
// SNAP (menu shift+s, estilo Blender): mueve la seleccion o el cursor 3D
// ====================================================================

// pone el origen de o en la posicion GLOBAL g (convierte a local segun el
// padre, misma matematica que ReparentKeepTransform)
static void PonerEnGlobal(Object* o, const Vector3& g) {
    { extern bool g_objetosMovidos; g_objetosMovidos = true; } // snap mueve el objeto -> Mirror con target puede cambiar
    Object* p = o->Parent;
    if (!p) { o->pos = g; return; }
    Quaternion prg = RotGlobalDe(p);
    Vector3 psg = ScaleGlobalDe(p);
    // BASE, y por dos motivos. Uno: esto ESCRIBE o->pos, que va al .w3d, asi que un padre
    // con constraint no puede quedar horneado adentro (ver Objects.h). Dos: prg/psg salen de
    // RotGlobalDe/ScaleGlobalDe, que leen los CAMPOS del padre -o sea que YA son base-, y
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
void AplicarPivotATransform(){
    if (g_transformPivot == PivotIndividual) return; // cada uno rota/escala en su lugar
    if (estado != rotacion && estado != EditScale) return; // translate no usa pivot
    Vector3 pivot = TransformPivotPoint;
    for (size_t o = 0; o < estadoObjetos.size(); o++){
        SaveState& st = estadoObjetos[o];
        Object& obj = *st.obj;
        Vector3 off = st.worldPos - pivot; // offset inicial respecto al pivote
        Vector3 nw;
        if (estado == rotacion){
            // rotacion acumulada desde el inicio (en mundo p/ objetos top-level)
            Quaternion delta = obj.Rot() * st.rot.Inverted();
            nw = pivot + delta * off;
        } else { // EditScale: la distancia al pivote escala con el factor (por eje)
            float fx = st.scale.x != 0.0f ? obj.scale.x / st.scale.x : 1.0f;
            float fy = st.scale.y != 0.0f ? obj.scale.y / st.scale.y : 1.0f;
            float fz = st.scale.z != 0.0f ? obj.scale.z / st.scale.z : 1.0f;
            nw = pivot + Vector3(off.x*fx, off.y*fy, off.z*fz);
        }
        PonerEnGlobal(&obj, nw);
    }
}

// ============================================================================
//  SNAP en MODO OBJETO (move/rotate/scale). La seleccion se trata como si TODOS sus verts estuvieran seleccionados
//  (camara/lampara/empty aportan su ORIGEN). Se snapshotean al empezar y la BASE se imanta al target bajo el cursor.
// ============================================================================
static std::vector<Vector3> g_objSnapPts; // puntos de snap (MUNDO) de la seleccion, capturados al empezar
static Vector3 g_objSnapActivo;           // origen del objeto ACTIVO (aguja de la rotacion / base Active)
static bool    g_objSnapHayAct = false;

// BASE en las tres lecturas, y NO por como se mide sino por donde termina el dato: estos puntos son
// el lado IZQUIERDO de 'off = T - B', y ese off se le suma a st.worldPos (base) para bajarlo a pos
// con PonerEnGlobal (base). Si la seleccion que se mueve se midiera EFECTIVA, el offset del
// constraint entraria en la resta y saldria horneado en el .w3d. El target T si es efectivo: es un
// punto de MUNDO que el usuario apunto con el mouse sobre lo que se DIBUJA (ver SnapBuscarTarget).
static void SnapObjCapturar(){
    g_objSnapPts.clear(); g_objSnapHayAct = false;
    for (size_t o=0;o<estadoObjetos.size();o++){
        Object* ob = estadoObjetos[o].obj; if (!ob) continue;
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
        if (ob==ObjActivo){ g_objSnapActivo = ob->GetGlobalPositionBase(); g_objSnapHayAct = true; }
    }
}

// base del snap (Closest/Center/Median/Active) entre los puntos snapshot, respecto al target T.
static bool SnapObjBase(const Vector3& T, Vector3& out){
    if (g_objSnapPts.empty()) return false;
    if (g_snap.base==SNAP_ACTIVE && g_objSnapHayAct){ out=g_objSnapActivo; return true; }
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
static void SnapAjustarObjMove(){
    g_snapHit=false;
    if (!g_snap.enabled || !g_snap.afMove || !Viewport3DActive || InteractionMode!=ObjectMode || estadoObjetos.empty()) return;
    Vector3 T; float sx=0,sy=0;
    if (!SnapBuscarTarget(g_snapCurX,g_snapCurY,Viewport3DActive,T,sx,sy)) return;
    Vector3 B; if (!SnapObjBase(T,B)) return;
    Vector3 off = T - B;
    Object* ref = ObjActivo ? ObjActivo : estadoObjetos[0].obj;
    if (axisSelect==X||axisSelect==Y||axisSelect==Z){ Vector3 a=EjeOrientado(*ref,axisSelect); off=a*off.Dot(a); }
    else if (axisSelect==PlaneX||axisSelect==PlaneY||axisSelect==PlaneZ){ int ex=(axisSelect==PlaneX)?X:(axisSelect==PlaneY)?Y:Z; Vector3 a=EjeOrientado(*ref,ex); off=off-a*off.Dot(a); }
    for (size_t o=0;o<estadoObjetos.size();o++){ SaveState& st=estadoObjetos[o]; PonerEnGlobal(st.obj, st.worldPos + off); }
    g_snapHit=true; g_snapSx=sx; g_snapSy=sy;
}

// ROTATE: gira toda la seleccion alrededor del pivote (TransformPivotPoint) para que el objeto ACTIVO apunte al target.
void SnapAjustarObjRot(){
    g_snapHit=false;
    if (!g_snap.enabled || !g_snap.afRot || !Viewport3DActive || InteractionMode!=ObjectMode || estadoObjetos.empty()) return;
    if (axisSelect==OrbitalAxis || !g_objSnapHayAct) return; // orbital no tiene plano; sin activo no hay aguja
    Object* ref = ObjActivo ? ObjActivo : estadoObjetos[0].obj;
    Vector3 axis = (axisSelect==ViewAxis||axisSelect==XYZ) ? camForward : EjeOrientado(*ref, axisSelect);
    { float al=sqrtf(axis.Dot(axis)); if(al<1e-6f) return; axis=axis*(1.0f/al); }
    Vector3 T; float sx=0,sy=0;
    if (!SnapBuscarTarget(g_snapCurX,g_snapCurY,Viewport3DActive,T,sx,sy)) return;
    const Vector3 P = TransformPivotPoint;
    Vector3 vN = g_objSnapActivo - P; vN = vN - axis*vN.Dot(axis); // aguja = origen del activo (snapshot)
    Vector3 vT = T - P;               vT = vT - axis*vT.Dot(axis);
    float lN=sqrtf(vN.Dot(vN)), lT=sqrtf(vT.Dot(vT));
    if (lN<1e-5f || lT<1e-5f) return; // activo sobre el eje/pivote -> sin angulo
    vN=vN*(1.0f/lN); vT=vT*(1.0f/lT);
    float cosA=vN.Dot(vT); if(cosA>1)cosA=1; if(cosA<-1)cosA=-1;
    Vector3 cross(vN.y*vT.z-vN.z*vT.y, vN.z*vT.x-vN.x*vT.z, vN.x*vT.y-vN.y*vT.x);
    float angDeg = atan2f(cross.Dot(axis), cosA)*180.0f/3.14159265f; // absoluto -> obj.rot desde el snapshot
    for (size_t o=0;o<estadoObjetos.size();o++){ SaveState& st=estadoObjetos[o]; Object& ob=*st.obj;
        ob.SetRot(Quaternion::FromAxisAngle(axis, angDeg) * st.rot); }
    AplicarPivotATransform();
    gAnguloTransform = angDeg;
    g_snapHit=true; g_snapSx=sx; g_snapSy=sy;
}

// SCALE (uniforme): la BASE se mueve radialmente desde el pivote hasta el punto mas cercano al target. Bajo escala
// uniforme de objeto, un punto del mundo se mueve radial al pivote (el reposicionar el origen + escalar los verts
// se combinan en un puro escalado desde el pivote), asi que un factor k uniforme desde el snapshot lo resuelve.
static void SnapAjustarObjScale(){
    g_snapHit=false;
    if (!g_snap.enabled || !g_snap.afScale || !Viewport3DActive || InteractionMode!=ObjectMode || estadoObjetos.empty()) return;
    if (axisSelect!=XYZ && axisSelect!=ViewAxis) return; // solo escala UNIFORME (con eje bloqueado el radial no vale)
    Vector3 T; float sx=0,sy=0;
    if (!SnapBuscarTarget(g_snapCurX,g_snapCurY,Viewport3DActive,T,sx,sy)) return;
    Vector3 B; if (!SnapObjBase(T,B)) return;
    const Vector3 P = TransformPivotPoint;
    Vector3 d = B - P; float dd=d.Dot(d); if (dd<1e-12f) return; // base en el pivote -> la escala no la mueve
    float k = (T-P).Dot(d)/dd; if (k < 1e-4f) k = 1e-4f; // no colapsar/invertir
    for (size_t o=0;o<estadoObjetos.size();o++){ SaveState& st=estadoObjetos[o]; st.obj->scale = st.scale * k; }
    AplicarPivotATransform();
    g_snapHit=true; g_snapSx=sx; g_snapSy=sy;
}

// redondea a la unidad de grilla mas cercana (1.0, como Blender por defecto)
static float RedondearGrid(float v) {
    return (float)((int)(v + (v >= 0.0f ? 0.5f : -0.5f)));
}
static Vector3 RedondearGrid(const Vector3& v) {
    return Vector3(RedondearGrid(v.x), RedondearGrid(v.y), RedondearGrid(v.z));
}

// junta todos los seleccionados (DFS desde la raiz)
static void RecolectarSnap(Object* nodo, std::vector<Object*>& out) {
    if (!nodo) return;
    for (size_t i = 0; i < nodo->Childrens.size(); i++) {
        Object* o = nodo->Childrens[i];
        if (o->select) out.push_back(o);
        RecolectarSnap(o, out);
    }
}

// traslada RIGIDO los verts seleccionados (segun el modo vertex/edge/face) por 'delta' en coords LOCALES y
// persiste todo (render + overlay + bordes + normales + preview de mods + undo). Extraido de SnapSeleccionAlCursor.
void MoverSeleccionEditLocal(Mesh* m, const Vector3& delta) {
    if (!m) return;
    m->EnsureEdit();
    EditMesh* e = m->edit;
    if (!e) return;
    if (delta.x == 0.0f && delta.y == 0.0f && delta.z == 0.0f) return;
    UndoCapturarMallaGeo(m); // Ctrl+Z: snapshot antes de mover
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
    e->EmpujarPosiciones(); e->RefrescarOverlay();
    m->CalcularBordes(false);
    if (!g_editLockNormales) m->RecalcularNormales();
    if (!m->modificadores.empty()) m->GenerarMallaModificada();
    g_redraw = true;
}

void SnapSeleccionAlCursor(bool mantenerOffset) {
    // Edit Mode: traslada RIGIDO los sub-elementos seleccionados para que su CENTRO caiga en el cursor 3d
    // (el inverso exacto de "Cursor to Selected"). Conservan su forma; solo se desplaza el grupo. En edicion
    // de malla no hay "activo" con offset propio, asi que ambas variantes hacen lo mismo (mantenerOffset se ignora).
    if (InteractionMode == EditMode) {
        if (!g_editMesh) return;
        Mesh* m = (Mesh*)g_editMesh;
        m->EnsureEdit();
        EditMesh* e = m->edit;
        if (!e) return;
        float cx, cy, cz;
        if (!e->CentroSeleccion(cx, cy, cz)) return; // nada seleccionado
        // cursor 3d (MUNDO) -> LOCAL del mesh (inversa de LocalAMundoBase): quita origen, rotacion y escala globales.
        // BASE: esto MUEVE VERTICES de la malla, y el cursor 3D es un punto de mundo que no depende
        // de ninguna camara. Con la posicion efectiva y la rotacion base (RotGlobalDe lee los campos)
        // la cuenta ni siquiera cerraba entre si.
        Quaternion rg = RotGlobalDe(m);
        Vector3    sg = ScaleGlobalDe(m);
        Vector3    d  = rg.Inverted() * (cursor3D.pos - m->GetGlobalPositionBase());
        Vector3 cursorLocal(sg.x!=0.0f?d.x/sg.x:d.x, sg.y!=0.0f?d.y/sg.y:d.y, sg.z!=0.0f?d.z/sg.z:d.z);
        Vector3 delta(cursorLocal.x - cx, cursorLocal.y - cy, cursorLocal.z - cz);
        MoverSeleccionEditLocal(m, delta); // traslada rigido la seleccion + persiste (no-op si delta=0)
        return;
    }
    std::vector<Object*> sel;
    RecolectarSnap(SceneCollection, sel);
    if (sel.empty()) return;
    if (mantenerOffset && ObjActivo) {
        // toda la seleccion se mueve para que el ACTIVO caiga en el cursor,
        // conservando los offsets relativos entre los seleccionados.
        // Capturo las globales ANTES de mover nada (si un padre y su hijo
        // estan ambos seleccionados, mover el padre cambiaria la del hijo).
        // BASE las dos: alimentan a PonerEnGlobal, que ESCRIBE pos. Con la efectiva, un
        // acompaniante con Copy Location a 10 y pos.x=2 terminaba en 15 en vez de en 7,
        // con el salto del constraint horneado adentro. Ver PonerEnGlobal y Objects.h.
        Vector3 delta = cursor3D.pos - ObjActivo->GetGlobalPositionBase();
        std::vector<Vector3> g(sel.size());
        for (size_t i = 0; i < sel.size(); i++) g[i] = sel[i]->GetGlobalPositionBase();
        for (size_t i = 0; i < sel.size(); i++) PonerEnGlobal(sel[i], g[i] + delta);
    } else {
        for (size_t i = 0; i < sel.size(); i++)
            PonerEnGlobal(sel[i], cursor3D.pos);
    }
}

void SnapSeleccionAlActivo() {
    if (InteractionMode == EditMode) return; // edit: TODO sub-elementos (Fase 2a)
    if (!ObjActivo) return;
    // BASE: el destino se le baja a pos a cada seleccionado (PonerEnGlobal). Si el activo tiene
    // Copy Location a 20 con pos.x=1, los otros tienen que ir a 1 -que es lo que el activo
    // realmente ES en el archivo- y no a 20, que es donde lo esta parando su constraint.
    Vector3 destino = ObjActivo->GetGlobalPositionBase();
    std::vector<Object*> sel;
    RecolectarSnap(SceneCollection, sel);
    for (size_t i = 0; i < sel.size(); i++)
        if (sel[i] != ObjActivo) PonerEnGlobal(sel[i], destino);
}

void SnapSeleccionAlGrid() {
    if (InteractionMode == EditMode) return; // edit: TODO sub-elementos (Fase 2a)
    std::vector<Object*> sel;
    RecolectarSnap(SceneCollection, sel);
    // snapshot de globales antes de mover (padre+hijo seleccionados). BASE: lo que se redondea
    // es lo que se va a ESCRIBIR. Con la efectiva, un cubo en pos.x=0.3 con Copy Location a una
    // fuente en 10.4 terminaba con pos=(10,0,0) -el snap a grilla lo mandaba a la fuente- en vez
    // de con el (0,0,0) que le corresponde a SU 0.3.
    std::vector<Vector3> g(sel.size());
    for (size_t i = 0; i < sel.size(); i++) g[i] = sel[i]->GetGlobalPositionBase();
    for (size_t i = 0; i < sel.size(); i++)
        PonerEnGlobal(sel[i], RedondearGrid(g[i]));
}

void SnapCursorAlGrid() {
    cursor3D.pos = RedondearGrid(cursor3D.pos);
}

void SnapCursorAlOrigen() {
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
//  (a) MOVER vertex[] a mano es exactamente el pecado que el guard W3dMoverVerts existe para
//      impedir: las claves de sharpEdges/seamEdges son los BYTES de la posicion, asi que sin
//      re-anclar las marcas quedaban vivas en memoria (se seguian dibujando) y sin punto donde
//      escribirse -> el .w3dm salia sin SHARP ni SEAM. Es el mismo bug del Apply, por la puerta
//      del menu Object > Set Origin.
//  (b) UNDO: no habia ninguno. Las tres entradas del submenu movian la geometria de forma
//      IRREVERSIBLE (Ctrl+Z no devolvia nada). El snapshot va ANTES de tocar nada; el caller
//      agrupa el paso de geometria con el del transform del objeto (ver UndoGrupo* abajo).
static void DesplazarVertices(Mesh* m, const Vector3& d) {
    if (!m->vertex) return;
    UndoCapturarMallaGeo(m);   // Ctrl+Z: snapshot ANTES de mover (antes esto no se deshacia)
    {
        W3dMoverVerts mv(m);   // <- re-ancla sharp/seam al cerrar el scope
        for (int i = 0; i < m->vertexSize; i++) {
            m->vertex[i*3]   += d.x;
            m->vertex[i*3+1] += d.y;
            m->vertex[i*3+2] += d.z;
        }
    }
    m->CalcularBordes(); // recalcula centroGeom + edges + bordesBuf (+ invalida edit)
}

// (R*S)^-1 * d : pasa un vector de PARENT-local a MESH-local (deshace rot+escala
// del objeto). Sirve para que los vertices compensen un movimiento del origen.
static Vector3 ParentLocalAMeshLocal(Mesh* m, const Vector3& d) {
    Vector3 rd = m->Rot().Inverted() * d; // R^-1
    return Vector3(m->scale.x != 0.0f ? rd.x / m->scale.x : rd.x,  // S^-1
                   m->scale.y != 0.0f ? rd.y / m->scale.y : rd.y,
                   m->scale.z != 0.0f ? rd.z / m->scale.z : rd.z);
}

static void RecolectarMallasSel(std::vector<Mesh*>& out) {
    std::vector<Object*> sel;
    RecolectarSnap(SceneCollection, sel);
    for (size_t i = 0; i < sel.size(); i++)
        if (sel[i]->getType() == ObjectType::mesh) out.push_back((Mesh*)sel[i]);
}

// 1) Geometry to Origin: la geometria se mueve para que su BARICENTRO caiga en el
//    origen del objeto. El objeto (pos/rot/escala) NO se mueve.
void SetOriginGeometryToOrigin() {
    std::vector<Mesh*> ms; RecolectarMallasSel(ms);
    if (ms.empty()) return;
    UndoGrupoIniciar();   // N mallas seleccionadas = UN SOLO Ctrl+Z (el objeto no se mueve aca)
    for (size_t i = 0; i < ms.size(); i++)
        DesplazarVertices(ms[i], ms[i]->centroGeom * -1.0f); // verts -= baricentro
    UndoGrupoFin();
}

// 2) Origin to Geometry: el ORIGEN se mueve al baricentro y la geometria queda EN SU
//    LUGAR. Los vertices se mueven igual que (1), pero el objeto se mueve al reves
//    (pos += R*S*baricentro) para compensar.
void SetOriginOriginToGeometry() {
    std::vector<Mesh*> ms; RecolectarMallasSel(ms);
    if (ms.empty()) return;
    // ESTA entrada mueve DOS cosas (el origen del objeto Y la geometria): las dos tienen que
    // volver con UN Ctrl+Z o el paso intermedio deja la malla corrida respecto de su origen.
    UndoGrupoIniciar();
    UndoTransformIniciar();   // pos/rot/escala de los seleccionados, ANTES de tocarlos
    for (size_t i = 0; i < ms.size(); i++) {
        Mesh* m = ms[i];
        Vector3 c = m->centroGeom;
        // baricentro local -> offset en parent-local (aplica escala y rotacion del obj)
        Vector3 sc(m->scale.x*c.x, m->scale.y*c.y, m->scale.z*c.z);
        m->pos += m->Rot() * sc;     // el origen va a donde estaba el baricentro
        DesplazarVertices(m, c * -1.0f); // verts -= baricentro (la geometria no se mueve)
    }
    UndoTransformConfirmar();
    UndoGrupoFin();
}

// 3) Origin to 3D Cursor: el ORIGEN se mueve al cursor 3D y la geometria queda en su
//    lugar. Lo que se movio el objeto, los vertices lo compensan al reves.
void SetOriginToCursor() {
    std::vector<Mesh*> ms; RecolectarMallasSel(ms);
    if (ms.empty()) return;
    UndoGrupoIniciar();       // origen + geometria en UN SOLO Ctrl+Z (ver SetOriginOriginToGeometry)
    UndoTransformIniciar();
    for (size_t i = 0; i < ms.size(); i++) {
        Mesh* m = ms[i];
        Vector3 oldPos = m->pos;
        PonerEnGlobal(m, cursor3D.pos);  // origen -> cursor (world->local, respeta padre)
        // los vertices compensan (oldPos - newPos) llevado a mesh-local
        DesplazarVertices(m, ParentLocalAMeshLocal(m, oldPos - m->pos));
    }
    UndoTransformConfirmar();
    UndoGrupoFin();
}

// ====================================================================
//  OLVIDAR EL ARCHIVO DE ORIGEN (menu Object > Clear Original File)
//  Ver el porque completo en ObjectMode.h. Solo borra la PROCEDENCIA
//  (Mesh::origen); la geometria vive horneada en el .w3dm y no se toca.
// ====================================================================
int OlvidarOrigenSeleccionadas() {
    std::vector<Mesh*> ms; RecolectarMallasSel(ms);
    int n = 0; std::string ultimo;
    for (size_t i = 0; i < ms.size(); i++) {
        if (ms[i]->origen.empty()) continue;
        ultimo = ms[i]->origen;
        w3dLogfW("[W3D] '%s': olvido su archivo de origen '%s' (la geometria ya viaja adentro del .w3d)",
                 ms[i]->name.c_str(), ultimo.c_str());
        ms[i]->origen.clear();
        n++;
    }
    if (n == 0) Notificar(T("No original file to forget"), false);
    else if (n == 1) Notificar(T("Original file forgotten") + std::string(": ") + W3dNombreCorto(ultimo), false);
    else Notificar(T("Original file forgotten"), false);
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
//  El precedente ya estaba: la rama de Edit Mode de SnapSeleccionAlCursor usa
//  GetGlobalPositionBase desde hace rato; medir con LocalAMundo (efectiva) y escribir
//  con LocalAMundoBase rompia la ida y vuelta ADENTRO de la misma pareja de funciones.
//  LO QUE SE ACEPTA A CAMBIO: sobre un objeto con Copy Location, "Cursor to Selected"
//  deja el cursor en el origen REAL del objeto y no donde se lo ve. Es visible pero es
//  honesto (ahi es donde el cursor puede devolverlo), y no toca ningun archivo.
// ----------------------------------------------------------------------------
void SnapCursorALoSeleccionado() {
    // Edit Mode: el cursor al CENTRO de los sub-elementos seleccionados de la malla
    // (mismo calculo que el foco; en mundo via LocalAMundoBase). Da igual vertex/edge/face.
    if (InteractionMode == EditMode && g_editMesh) {
        Mesh* m = (Mesh*)g_editMesh;
        m->EnsureEdit();
        float cx, cy, cz;
        if (m->edit && m->edit->CentroSeleccion(cx, cy, cz))
            cursor3D.pos = m->LocalAMundoBase(Vector3(cx, cy, cz));
        return;
    }
    std::vector<Object*> sel;
    RecolectarSnap(SceneCollection, sel);
    if (sel.empty()) return;
    Vector3 suma(0.0f, 0.0f, 0.0f);
    for (size_t i = 0; i < sel.size(); i++) suma += sel[i]->GetGlobalPositionBase();
    cursor3D.pos = suma * (1.0f / (float)sel.size()); // mediana ~ promedio
}

void SnapCursorAlActivo() {
    // Edit Mode: por ahora el centro de la seleccion de la malla (igual que Selected).
    // TODO: el sub-elemento ACTIVO exacto (vertice/arista/cara activa) cuando este.
    if (InteractionMode == EditMode && g_editMesh) {
        Mesh* m = (Mesh*)g_editMesh;
        m->EnsureEdit();
        float cx, cy, cz;
        if (m->edit && m->edit->CentroSeleccion(cx, cy, cz))
            cursor3D.pos = m->LocalAMundoBase(Vector3(cx, cy, cz));   // BASE: ver SnapCursorALoSeleccionado
        return;
    }
    if (!ObjActivo) return;
    cursor3D.pos = ObjActivo->GetGlobalPositionBase();   // BASE: ver SnapCursorALoSeleccionado
}
// ============================================================================
//  NOMBRES UNICOS de OBJETO: rename con arrastre de VINCULOS + reparacion de la
//  carga. Vive aca (editor) y no en W3dEscena.cpp porque ese archivo lo compila
//  TAMBIEN el runtime del juego y esto usa Undo/Notificar (solo editor).
//
//  Los objetos se vinculan POR NOMBRE en cinco lugares y ninguno se actualizaba
//  al renombrar (bug VIVO hoy, independiente de los duplicados):
//    (a) refs de los scripts lua  -> W3dScriptEntrada::refs (texto en el .w3dui)
//    (b) targetName de Mirror/Instance/ObjetoScript/Camara + la fuente de cada constraint
//    (c) RielName de la camara
//    (d) escenaInicial del proyecto (si el renombrado es una raiz UI)
//    (e) Object::paleta (eso lo maneja W3dPaletaRenombrar, no esto)
//  Todo se arrastra en UN SOLO paso de undo: con pasos separados, el Ctrl+Z
//  intermedio deja el vinculo roto (misma leccion que Bone2DRenameUndo).
// ============================================================================
#include "script/W3dScript.h"   // W3dScriptDatos / W3dScriptEntrada::refs
#include "W3dEscena.h"          // W3dEscenaRaizDe / escena inicial
#include "objects/Target.h"     // targetName cacheado
#include "objects/Mirror.h"
#include "objects/Gamepad.h"
#include "w3dlog.h"
#include "objects/UI.h"
#include "W3dNombres.h"
#include "W3dPaletas.h"            // paletas + colores de paleta (globales de proyecto)
#include "objects/Materials.h"     // Materials (global de proyecto)
#include "animation/Animation.h"   // SceneAnimations (global de proyecto)
#include <map>
#include <set>

// junta los std::string* que apuntan al nombre 'viejo' y que hay que arrastrar.
// 'escena' = la raiz del scope (las refs de script se resuelven POR ESCENA, ver
// SimJuego.cpp; arrastrar mas alla renombraria refs de otra escena que apuntan
// legitimamente a OTRO objeto homonimo).
// Las refs viven POR VALOR en un vector<pair> editable desde el IDE: se guardan como
// (objeto, script, ref) y NO como std::string*, que se cuelga en cuanto se agrega o
// borra una ref/script (ver W3dRenameDest en Undo.h).
static void JuntarRefsDeScripts(Object* nodo, const std::string& viejo, std::vector<W3dRenameDest>& out) {
    if (!nodo) return;
    if (nodo->scriptDatos)
        for (size_t s = 0; s < nodo->scriptDatos->scripts.size(); s++) {
            W3dScriptEntrada& e = nodo->scriptDatos->scripts[s];
            for (size_t r = 0; r < e.refs.size(); r++)
                if (e.refs[r].second == viejo) out.push_back(W3dDestRefLua(nodo, (int)s, (int)r));
        }
    for (size_t i = 0; i < nodo->Childrens.size(); i++)
        JuntarRefsDeScripts(nodo->Childrens[i], viejo, out);
}
// ---------------------------------------------------------------------------
//  TODA punta de vinculo POR NOMBRE que nombra a 'viejo'. Se recorre el arbol
//  ENTERO: un objeto puede vivir en otra escena y apuntar igual.
//
//  ANTES ERA UN switch POR TIPO (mirror/instance/gamepad/camara + el riel
//  aparte). Ese switch es una LISTA DE CLASES mantenida a mano, y la clase
//  que se olvide de agregar ahi queda con el vinculo roto en silencio al
//  renombrar la punta. Ahora entra por la puerta UNICA de refs (Objects.h):
//  RefsObjeto enumera las refs propias de la clase MAS una por constraint, y
//  RefObjetoNombre da el std::string* que las acompania. Con eso:
//   - las cinco de siempre siguen cubiertas (cada una las declara con
//     RefsPropias/RefPropiaNombre, incluido el riel de la camara, que es su ref 1),
//   - los CONSTRAINTS quedan cubiertos GRATIS: renombrar la fuente re-liga el
//     'fuenteNombre' de todos los constraints que la nombraban, y
//   - una clase nueva que guarde un Object* lo hereda sin tocar este archivo.
// ---------------------------------------------------------------------------
static void JuntarTargetsPorNombre(Object* nodo, const std::string& viejo, std::vector<W3dRenameDest>& out) {
    if (!nodo) return;
    const int n = nodo->RefsObjeto();
    for (int i = 0; i < n; i++) {
        // los nombres de las refs son MIEMBROS del objeto (targetName / RielName) o del
        // constraint, que el objeto tiene vivo: puntero directo, como antes
        std::string* p = nodo->RefObjetoNombre(i);
        if (p && *p == viejo) out.push_back(W3dDestNombre(p));
    }
    for (size_t i = 0; i < nodo->Childrens.size(); i++)
        JuntarTargetsPorNombre(nodo->Childrens[i], viejo, out);
}

int W3dNombresContarRefs(Object* o, const std::string& viejo) {
    std::vector<W3dRenameDest> v;
    Object* esc = W3dEscenaRaizDe(o);
    JuntarRefsDeScripts(esc ? esc : SceneCollection, viejo, v);
    return (int)v.size();
}

std::string W3dRenombrarObjeto(Object* o, const std::string& pedido, bool avisar) {
    if (!o) return std::string();
    const std::string viejo = o->name;
    const std::string nuevo = o->NombreLibre(pedido);
    if (nuevo == viejo) return viejo;

    // TODAS las puntas del vinculo, en UN solo paso de undo (con IDENTIDAD ESTABLE: las refs
    // de lua viven dentro de un vector por valor -> ver W3dRenameDest en Undo.h)
    std::vector<W3dRenameDest> aRenombrar;
    aRenombrar.push_back(W3dDestNombre(&o->name));
    Object* esc = W3dEscenaRaizDe(o);
    int nrefs0 = (int)aRenombrar.size();
    JuntarRefsDeScripts(esc ? esc : SceneCollection, viejo, aRenombrar);
    const int nrefs = (int)aRenombrar.size() - nrefs0;
    int ntgt0 = (int)aRenombrar.size();
    JuntarTargetsPorNombre(SceneCollection, viejo, aRenombrar);
    const int ntgt = (int)aRenombrar.size() - ntgt0;
    // escena INICIAL del proyecto (el nombre de la escena es la clave)
    const bool esEscenaInicial = (o->getType() == ObjectType::ui && W3dEscenaInicial() == viejo);

    UndoCapturarRenames(aRenombrar);
    for (size_t k = 0; k < aRenombrar.size(); k++)
        if (std::string* p = W3dDestResolver(aRenombrar[k])) *p = nuevo;
    if (esEscenaInicial) W3dEscenaSetInicial(nuevo);

    if (avisar) {
        const std::string norm = W3dNombreNormalizar(pedido, "Objeto");
        if (nuevo != norm) W3dAvisoYaExiste(norm, nuevo);
        if (nrefs > 0 || ntgt > 0)
            W3dAvisof(false, "%d referencia(s) de script y %d target(s) actualizados", nrefs, ntgt);
        if (o->getType() == ObjectType::ui)
            W3dAvisof(true, "escena '%s' -> '%s': revisa los cambiarEscena(\"%s\") de tus .lua",
                      W3dNombreCorto(viejo).c_str(), W3dNombreCorto(nuevo).c_str(),
                      W3dNombreCorto(viejo).c_str());
    }
    return nuevo;
}

// ---------------------------------------------------------------------------
//  REPARACION al ABRIR un proyecto (o al MERGEAR un .w3dui): los archivos ya
//  guardados TIENEN duplicados (el bug de modelos/Cubo_0.glb lo prueba). Se
//  recorre el arbol en el mismo orden que FindObjectByName (depth-first) y el
//  PRIMERO se queda con el nombre pelado, para que las refs y los targets que
//  hoy resuelven al primero sigan resolviendo al MISMO objeto.
//
//  *** NO SE ARRASTRA NINGUN VINCULO ***  (esto es lo contrario del rename
//  MANUAL, que si los arrastra). Motivo, y es la clave de todo el bloque:
//  FindObjectByName / BuscarPorNombre devuelven el PRIMERO del recorrido, o sea
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
//  W3dNombresJuntarEspacios mas abajo.
//
//  NO usa undo (abrir un proyecto limpia el historial) y avisa SIEMPRE de forma
//  visible: los nombres que el usuario ve en el outliner cambiaron.
// ---------------------------------------------------------------------------
// el orden es PRE-ORDEN (el mismo de FindObjectByName), y se lleva un set de nombres
// YA TOMADOS por scope: asi el PRIMERO del recorrido conserva el nombre pelado y solo se
// numeran los que vienen despues. (Mirar el arbol vivo en vez de un set daba el resultado
// al reves: en hola.w3d la ESCENA "Hola" perdia contra su propio hijo "Hola".)
typedef std::map<Object*, std::set<std::string> > TomadosPorScope;
static bool NombreTomadoEnSet(const std::string& n, void* ctx) {
    std::set<std::string>* S = (std::set<std::string>*)ctx;
    return S->find(n) != S->end();
}
static void RepararRec(Object* nodo, TomadosPorScope& tomados, std::vector<std::pair<Object*, std::string> >& cambios) {
    if (!nodo) return;
    for (size_t i = 0; i < nodo->Childrens.size(); i++) {
        Object* h = nodo->Childrens[i];
        Object* raiz = W3dNombreScopeDe(h);
        TomadosPorScope::iterator it = tomados.find(raiz);
        if (it == tomados.end()) {
            // scope nuevo: sembrarlo con el nombre de SU RAIZ (la busqueda por nombre
            // arranca EN la raiz, asi que un hijo homonimo de la escena es ambiguo)
            std::set<std::string> s0;
            if (raiz) s0.insert(raiz->name);
            it = tomados.insert(std::make_pair(raiz, s0)).first;
        }
        const std::string norm = W3dNombreNormalizar(h->name, "Objeto");
        std::string nuevo = norm;
        if (NombreTomadoEnSet(norm, &it->second))
            nuevo = W3dNombreUnico(norm, "Objeto", NombreTomadoEnSet, &it->second);
        it->second.insert(nuevo);
        if (nuevo != h->name) cambios.push_back(std::make_pair(h, nuevo));
        // si es una raiz UI, su scope propio arranca con SU nombre definitivo
        if (h->getType() == ObjectType::ui) {
            std::set<std::string> s0; s0.insert(nuevo);
            tomados[h] = s0;
        }
        RepararRec(h, tomados, cambios);
    }
}

// ---------------------------------------------------------------------------
//  LOS OTROS ESPACIOS DE NOMBRES (todo lo que NO es Object::name). Una sola
//  enumeracion, en el ORDEN DE BUSQUEDA de cada espacio, usada por DOS clientes:
//    1) la reparacion de carga (aca abajo): renumera los duplicados del archivo.
//    2) el UNDO (W3dNombreLibrePara): al RESTAURAR un nombre hay que re-chequear
//       que no se lo hayan tomado mientras tanto (bug F2).
//  Si aparece un espacio nuevo, se agrega ACA y los dos clientes lo cubren.
//
//  PALETAS: los COLORES son un caso especial. El invariante del proyecto dice
//  que todas las paletas tienen los mismos colores CON EL MISMO NOMBRE en el
//  mismo indice, asi que el espacio es la lista de la paleta 0 (igual que
//  W3dPaletaColorNombreLibre) y despues de tocar un nombre hay que propagarlo
//  por indice a las demas (PaletasPropagarColores).
// ---------------------------------------------------------------------------
// abre un espacio nuevo al final de la lista y devuelve la referencia para llenarlo
static W3dEspacioNombres& NuevoEspacio(std::vector<W3dEspacioNombres>& out,
                                       const char* etq, const char* defo, void* dueno) {
    out.push_back(W3dEspacioNombres());
    W3dEspacioNombres& E = out.back();
    E.etiqueta = etq; E.porDefecto = defo; E.dueno = dueno;
    return E;
}
// casi todos los espacios son un 'vector<T*>' cuyo campo de nombre se llama 'nombre' o
// 'name': dos macros chicas que abren el espacio y lo llenan de una (C++03, sin lambdas).
#define W3D_ESPACIO_NOMBRE(out, etq, defo, dueno, lista) \
    { W3dEspacioNombres& E_ = NuevoEspacio(out, etq, defo, dueno); \
      for (size_t iP = 0; iP < (lista).size(); iP++) if ((lista)[iP]) E_.nombres.push_back(&(lista)[iP]->nombre); }
#define W3D_ESPACIO_NAME(out, etq, defo, dueno, lista) \
    { W3dEspacioNombres& E_ = NuevoEspacio(out, etq, defo, dueno); \
      for (size_t iP = 0; iP < (lista).size(); iP++) if ((lista)[iP]) E_.nombres.push_back(&(lista)[iP]->name); }

static void JuntarEspaciosDeMalla(Mesh* m, std::vector<W3dEspacioNombres>& out) {
    W3D_ESPACIO_NOMBRE(out, "VERTEX GROUP", "Group",       m, m->vertexGroups)
    W3D_ESPACIO_NOMBRE(out, "UV GROUP",     "Group",       m, m->uvGroups)
    W3D_ESPACIO_NOMBRE(out, "UV MAP",       "UVMap",       m, m->uvMaps)
    W3D_ESPACIO_NOMBRE(out, "COLOR LAYER",  "Color",       m, m->colorLayers)
    W3D_ESPACIO_NAME  (out, "VERTEX ANIM",  "Anim",        m, m->animations)
    W3D_ESPACIO_NOMBRE(out, "MODIFICADOR",  "Mod",         m, m->modificadores)
    W3D_ESPACIO_NOMBRE(out, "ARMATURE 2D",  "Armature 2D", m, m->armatures2d)
    // MESH PARTS: viven POR VALOR dentro de la malla
    { W3dEspacioNombres& E = NuevoEspacio(out, "MESH PART", "Parte", m);
      for (size_t i = 0; i < m->materialsGroup.size(); i++) E.nombres.push_back(&m->materialsGroup[i].name); }
    // HUESOS 2D: el espacio es la MALLA ENTERA (todos sus rigs 2D a la vez; el skinning 2D
    // aplica TODOS los armatures y cada hueso pesa por su UV group HOMONIMO)
    { W3dEspacioNombres& E = NuevoEspacio(out, "HUESO 2D", "Hueso", m);
      for (size_t a = 0; a < m->armatures2d.size(); a++) if (m->armatures2d[a])
          for (size_t b = 0; b < m->armatures2d[a]->huesos.size(); b++)
              E.nombres.push_back(&m->armatures2d[a]->huesos[b].nombre); }
    // CLIPS 2D: uno por ARMATURE 2D
    for (size_t a = 0; a < m->armatures2d.size(); a++) if (m->armatures2d[a])
        W3D_ESPACIO_NAME(out, "CLIP 2D", "Anim", m, m->armatures2d[a]->anims)
}

void W3dNombresJuntarEspacios(std::vector<W3dEspacioNombres>& out) {
    out.clear();
    // ---- globales de PROYECTO ----
    W3D_ESPACIO_NAME(out, "MATERIAL", "Material", NULL, Materials)
    W3D_ESPACIO_NAME(out, "ANIMACION DE ESCENA", "Scene", NULL, SceneAnimations)
    { std::vector<Paleta>& ps = W3dPaletas();
      W3dEspacioNombres& P = NuevoEspacio(out, "PALETA", "Paleta", NULL);
      for (size_t i = 0; i < ps.size(); i++) P.nombres.push_back(&ps[i].nombre);
      if (!ps.empty()) {   // los colores van por la paleta 0 (ver el invariante de arriba)
          W3dEspacioNombres& C = NuevoEspacio(out, "COLOR DE PALETA", "Color", NULL);
          for (size_t i = 0; i < ps[0].colores.size(); i++) C.nombres.push_back(&ps[0].colores[i].nombre);
      } }

    // ---- por MALLA y por ARMATURE (recorrido del arbol) ----
    std::vector<Object*> pila; if (SceneCollection) pila.push_back(SceneCollection);
    while (!pila.empty()) {
        Object* n = pila.back(); pila.pop_back();
        for (size_t i = 0; i < n->Childrens.size(); i++) pila.push_back(n->Childrens[i]);
        // CONSTRAINTS: el scope es POR OBJETO (como los modificadores de una malla, y no
        // como los objetos, que son por escena). Dos billboards en el MISMO objeto tienen
        // que salir "Billboard" y "Billboard.001"; dos objetos distintos pueden tener cada
        // uno su "Billboard" y no se pisan. Lo tiene CUALQUIER tipo de objeto, no solo la
        // malla, asi que el espacio se abre aca arriba del if/else de tipos.
        // Solo si tiene: abrir un espacio VACIO por cada objeto de la escena le costaria a
        // los dos clientes (la reparacion de carga y el undo de renames) un recorrido por
        // objeto para nada.
        if (!n->constraints.empty())
            W3D_ESPACIO_NOMBRE(out, "CONSTRAINT", "Constraint", n, n->constraints)
        if (n->getType() == ObjectType::mesh) JuntarEspaciosDeMalla((Mesh*)n, out);
        else if (n->getType() == ObjectType::armature) {
            Armature* a = (Armature*)n;
            { W3dEspacioNombres& E = NuevoEspacio(out, "HUESO", "Hueso", a);   // viven POR VALOR
              for (size_t i = 0; i < a->bones.size(); i++) E.nombres.push_back(&a->bones[i].name); }
            W3D_ESPACIO_NAME(out, "CLIP", "Anim", a, a->animations)
        }
    }
}
#undef W3D_ESPACIO_NOMBRE
#undef W3D_ESPACIO_NAME

// invariante de paletas: el nombre del color 'i' es el MISMO en todas las paletas
static void PaletasPropagarColores() {
    std::vector<Paleta>& ps = W3dPaletas();
    if (ps.empty()) return;
    for (size_t i = 0; i < ps[0].colores.size(); i++)
        for (size_t j = 1; j < ps.size(); j++)
            if (i < ps[j].colores.size()) ps[j].colores[i].nombre = ps[0].colores[i].nombre;
}

std::string W3dNombreLibrePara(std::string* destino, const std::string& pedido, const char** etiquetaOut) {
    if (etiquetaOut) *etiquetaOut = NULL;
    if (!destino) return pedido;
    // 1) es el nombre de un OBJETO? Se resuelve con LA misma puerta que el editor
    //    (Object::NombreLibre: scope por escena, se excluye a si mismo).
    {
        std::vector<Object*> pila; if (SceneCollection) pila.push_back(SceneCollection);
        while (!pila.empty()) {
            Object* n = pila.back(); pila.pop_back();
            for (size_t i = 0; i < n->Childrens.size(); i++) pila.push_back(n->Childrens[i]);
            if (&n->name == destino) { if (etiquetaOut) *etiquetaOut = "OBJETO"; return n->NombreLibre(pedido); }
        }
    }
    // 2) alguno de los otros espacios (por IDENTIDAD DE PUNTERO). Si no aparece en
    //    ninguno NO es un nombre: es una ref de lua / un targetName / un RielName, y
    //    esos se restauran TAL CUAL (son el otro extremo de un vinculo, no un nombre).
    std::vector<W3dEspacioNombres> esp;
    W3dNombresJuntarEspacios(esp);
    for (size_t e = 0; e < esp.size(); e++) {
        bool mio = false;
        for (size_t k = 0; k < esp[e].nombres.size(); k++) if (esp[e].nombres[k] == destino) { mio = true; break; }
        if (!mio) continue;
        if (etiquetaOut) *etiquetaOut = esp[e].etiqueta;
        return W3dNombreUnicoEnLista(pedido, esp[e].porDefecto, esp[e].nombres, destino);
    }
    return pedido;
}

// ---------------------------------------------------------------------------
//  EL PUNTERO SIGUE VIVO? (validacion del destino 'Directo' del undo de renames,
//  ver W3dRenameDest en Undo.h). Directo se usa SOLO para nombres que son MIEMBRO
//  de un Object: Object::name y las puntas de vinculo cacheadas targetName /
//  RielName. Esos no se recrean, pero el OBJETO se puede borrar (y liberarse al
//  caerse del stack de undo), y entonces el Ctrl+Z escribia en memoria liberada.
//  Aca se busca el puntero POR IDENTIDAD entre los objetos vivos de la escena; si
//  no aparece, el paso de undo es un NO-OP seguro.
//
//  POR QUE SOLO EL ARBOL DE OBJETOS y no todos los espacios de nombres: para los
//  elementos de una lista (vertex groups, UV maps, clips, materiales...) buscar
//  "el puntero sigue siendo UN nombre vivo" NO alcanza y ademas ENGANA. Esos
//  elementos se borran y se re-crean en bloque (Mesh::LiberarCapas + los new de
//  MeshGeoUndo::AplicarA) y el allocator RECICLA las direcciones: el puntero
//  viejo de un vertex group aparece como el nombre vivo de OTRA cosa (medido: el
//  undo escribia el nombre del vertex group encima de un UV group). Por eso esos
//  destinos van por (dueno + indice) y no por puntero. Ver W3dDestCapaMalla.
//
//  OJO (mismo criterio que ObjetoEnEscena, en Undo.cpp): un objeto BORRADO esta
//  DETACHADO del arbol y por lo tanto "no vivo" para esta puerta. Es correcto: el
//  undo del borrado corre ANTES (LIFO) y lo devuelve al arbol, asi que la
//  secuencia normal rename->borrar->Ctrl+Z->Ctrl+Z sigue funcionando igual.
// ---------------------------------------------------------------------------
// puntas de vinculo POR NOMBRE que cuelgan de un objeto vivo. Por la MISMA puerta
// generica que JuntarTargetsPorNombre (ver el bloque de arriba): tienen que ser el
// mismo conjunto, o el undo de un rename que SI se arrastro dejaria de restaurarse
// por no reconocer el destino (W3dDestNombre lo avisa, pero recien en el log).
// Con el switch a mano eran dos listas que mantener sincronizadas a ojo.
//
// LIMITE CONOCIDO, el mismo que ya tenia targetName: esto contesta "esa direccion es
// HOY una punta de vinculo", no "es LA punta que se capturo". Un constraint se borra
// con el boton "-" y el proximo puede caer en la misma direccion. Lo que protege de
// verdad es que el rename y su undo son un solo paso atomico; para la identidad
// estable de una entrada del stack esta el SERIAL (ver ConstraintPorSerial).
static bool PunteroEsPuntaDeVinculo(Object* nodo, std::string* p) {
    if (!nodo) return false;
    const int n = nodo->RefsObjeto();
    for (int i = 0; i < n; i++) if (nodo->RefObjetoNombre(i) == p) return true;
    return false;
}
bool W3dNombrePunteroVivo(std::string* p) {
    if (!p) return false;
    std::vector<Object*> pila; if (SceneCollection) pila.push_back(SceneCollection);
    while (!pila.empty()) {
        Object* n = pila.back(); pila.pop_back();
        for (size_t i = 0; i < n->Childrens.size(); i++) pila.push_back(n->Childrens[i]);
        if (&n->name == p) return true;
        if (PunteroEsPuntaDeVinculo(n, p)) return true;
    }
    return false;
}

int W3dNombresRepararEscena(bool avisar) {
    if (!SceneCollection) return 0;
    int n = 0;
    std::string primeros;

    // ---- ESPACIO 1: los OBJETOS (por escena) ----
    TomadosPorScope tomados;
    std::vector<std::pair<Object*, std::string> > cambios;
    RepararRec(SceneCollection, tomados, cambios);
    for (size_t i = 0; i < cambios.size(); i++) {
        Object* o = cambios[i].first;
        const std::string viejo = o->name;
        const std::string nuevo = cambios[i].second;
        if (nuevo == viejo) continue;
        // SIN arrastrar vinculos (ver el bloque de arriba): el que conserva el nombre
        // es el PRIMERO, que es al que ya resolvian todos los targets y refs.
        o->name = nuevo;
        n++;
        if (n <= 3) { if (!primeros.empty()) primeros += ", ";
                      primeros += W3dNombreCorto(viejo) + " -> " + W3dNombreCorto(nuevo); }
        w3dLogfW("[nombres] OBJETO duplicado renumerado: '%s' -> '%s' (los vinculos por nombre "
                 "siguen apuntando al primer '%s')", viejo.c_str(), nuevo.c_str(), viejo.c_str());
    }

    // ---- LOS OTROS ESPACIOS (materiales, grupos, huesos, clips, paletas, ...) ----
    std::vector<W3dEspacioNombres> esp;
    W3dNombresJuntarEspacios(esp);
    bool tocoColores = false;
    for (size_t e = 0; e < esp.size(); e++) {
        W3dEspacioNombres& E = esp[e];
        std::set<std::string> tom;
        for (size_t k = 0; k < E.nombres.size(); k++) {
            std::string* p = E.nombres[k];
            const std::string viejo = *p;
            const std::string norm = W3dNombreNormalizar(viejo, E.porDefecto);
            std::string nuevo = norm;
            if (NombreTomadoEnSet(norm, &tom))
                nuevo = W3dNombreUnico(norm, E.porDefecto, NombreTomadoEnSet, &tom);
            tom.insert(nuevo);
            if (nuevo == viejo) continue;
            *p = nuevo;
            if (E.etiqueta && std::string(E.etiqueta) == "COLOR DE PALETA") tocoColores = true;
            n++;
            if (n <= 3) { if (!primeros.empty()) primeros += ", ";
                          primeros += W3dNombreCorto(viejo) + " -> " + W3dNombreCorto(nuevo); }
            w3dLogfW("[nombres] %s duplicado renumerado: '%s' -> '%s' (el primer '%s' conserva "
                     "el nombre, asi que los bindings no se mueven)",
                     E.etiqueta, viejo.c_str(), nuevo.c_str(), viejo.c_str());
        }
    }
    if (tocoColores) PaletasPropagarColores();

    if (n > 0 && avisar)        // ROJO: los nombres que se ven en el outliner cambiaron
        W3dAvisof(true, "%d nombre(s) repetidos renumerados (%s%s). El original NO cambio: lo que "
                        "apuntaba por nombre sigue apuntando ahi.",
                  n, primeros.c_str(), n > 3 ? ", ..." : "");
    return n;
}
