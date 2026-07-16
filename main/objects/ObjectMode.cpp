#include "ObjectMode.h"
#include "render/OpcionesRender.h" // g_transformPivot + enum TransformPivot (editor)
#include "objects/Mesh.h"      // snap del cursor al centro de la malla en Edit Mode
#include "EditMesh.h"  // CentroSeleccion
#include "Undo.h"      // Ctrl+Z: capturar transform / limpiar al borrar
#include "ViewPorts/LayoutInput.h" // SNAP en modo objeto (SnapBuscarTarget, g_snap, enums)
#include "edit/Modifier.h" // deep-copy de modificadores al duplicar
#include "objects/Armature.h" // Apply Transform sobre armature: hornear en los huesos
#include "animation/SkeletalAnimation.h" // HornearTransformEnHuesos

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
			obj.rot = estadoObj.rot;
        	obj.ActualizarDisplayRot();
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
		Vector3 v = obj.rot * EjeMundo(a);
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

#ifdef W3D_SYMBIAN
void EliminarAnimaciones(Object&) {} // animacion: pendiente en Symbian
void InsertarKeyframeObjeto() {}
void BorrarKeyframeObjeto() {}
void LimpiarKeyframeObjeto() {}
void AplicarAnimacionObjetos() {}
#else
void EliminarAnimaciones(Object& obj){
	for(size_t a = 0; a < AnimationObjects.size(); a++) {
		if (AnimationObjects[a].obj == &obj) {
			for(size_t p = 0; p < AnimationObjects[a].Propertys.size(); p++) {
				AnimationObjects[a].Propertys[p].keyframes.clear();
			}
			AnimationObjects[a].Propertys.clear();
			//AnimationObjects.Remove(a);
			if (a >= 0 && a < AnimationObjects.size()) {
				AnimationObjects.erase(AnimationObjects.begin() + a);
			}
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
// keyframe en las TRES curvas (X/Y/Z) de una propiedad del objeto. Cada componente es una curva independiente
// (PropertyDeLista las crea/encuentra por (propiedad, componente)); keyar toca las 3 en el mismo frame, pero
// despues se mueven/borran/curvan por separado desde el dope sheet.
static void SetKeyObj3(AnimationObject& ao, int prop, int frame, float x, float y, float z){
	SetKeyCurva(PropertyDeLista(ao.Propertys, prop, AnimX), frame, x);
	SetKeyCurva(PropertyDeLista(ao.Propertys, prop, AnimY), frame, y);
	SetKeyCurva(PropertyDeLista(ao.Propertys, prop, AnimZ), frame, z);
}

// ============================================================================
//  AUTO KEY: al confirmar un transform, guarda SOLO los canales que CAMBIARON.
//  El "cambio" se mide contra estadoObjetos, que es el snapshot que el propio transform toma al empezar. Por eso
//  hay que llamarla ANTES de limpiarlo (ver Viewport3D::Aceptar).
//  Es por CANAL, no por propiedad: si solo rotaste en X, se guarda X Euler Rotation y NADA mas. Eso es lo que
//  permite el modelo de curvas por componente (cada X/Y/Z es una curva propia).
// ============================================================================
bool AutoKeyOn = false;      // lo prende/apaga el boton del timeline
bool MotionTrailOn = false;  // menu Animation del viewport 3D: ver el camino de los objetos animados

// un canal cambio? Se compara con una tolerancia relativa: el transform pasa por matrices y trigonometria, asi
// que un valor que "no se toco" vuelve con basura en el ultimo bit y == daria cambios fantasma en los 9 canales.
static bool AutoKeyCambio(float a, float b){
	float d = a - b; if (d < 0) d = -d;
	float m = (a < 0 ? -a : a); float mb = (b < 0 ? -b : b); if (mb > m) m = mb;
	return d > 1e-5f * (1.0f + m);
}
// guarda el canal (prop, comp) si cambio. Devuelve true si guardo.
static bool AutoKeyCanal(AnimationObject& ao, int prop, int comp, int frame, float valor, float viejo){
	if (!AutoKeyCambio(valor, viejo)) return false;
	SetKeyCurva(PropertyDeLista(ao.Propertys, prop, comp), frame, valor);
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
		Vector3 rotVieja = estadoObjetos[e].rot.ToEulerXYZ();   // el MISMO camino que usa ActualizarDisplayRot
		AnimationObject& ao = AnimObjDe(o);
		const Vector3& p0 = estadoObjetos[e].pos;
		const Vector3& s0 = estadoObjetos[e].scale;
		if (AutoKeyCanal(ao, AnimPosition, AnimX, CurrentFrame, o->pos.x, p0.x)) n++;
		if (AutoKeyCanal(ao, AnimPosition, AnimY, CurrentFrame, o->pos.y, p0.y)) n++;
		if (AutoKeyCanal(ao, AnimPosition, AnimZ, CurrentFrame, o->pos.z, p0.z)) n++;
		if (AutoKeyCanal(ao, AnimRotation, AnimX, CurrentFrame, o->rotEuler.x, rotVieja.x)) n++;
		if (AutoKeyCanal(ao, AnimRotation, AnimY, CurrentFrame, o->rotEuler.y, rotVieja.y)) n++;
		if (AutoKeyCanal(ao, AnimRotation, AnimZ, CurrentFrame, o->rotEuler.z, rotVieja.z)) n++;
		if (AutoKeyCanal(ao, AnimScale,    AnimX, CurrentFrame, o->scale.x, s0.x)) n++;
		if (AutoKeyCanal(ao, AnimScale,    AnimY, CurrentFrame, o->scale.y, s0.y)) n++;
		if (AutoKeyCanal(ao, AnimScale,    AnimZ, CurrentFrame, o->scale.z, s0.z)) n++;
		if (n) ao.UpdateFirstLastFrame();
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
Matrix4 WorldEnFrame(Object* o, int f){
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

// Insert Keyframe (i): guarda pos+rot+escala de cada objeto SELECCIONADO en el frame actual
void InsertarKeyframeObjeto(){
	for (size_t s=0;s<ObjSelects.size();s++){ Object* o=ObjSelects[s]; if (!o) continue;
		o->ActualizarDisplayRot(); // rotEuler al dia
		AnimationObject& ao = AnimObjDe(o);
		SetKeyObj3(ao, AnimPosition, CurrentFrame, o->pos.x, o->pos.y, o->pos.z);
		SetKeyObj3(ao, AnimRotation, CurrentFrame, o->rotEuler.x, o->rotEuler.y, o->rotEuler.z);
		SetKeyObj3(ao, AnimScale,    CurrentFrame, o->scale.x, o->scale.y, o->scale.z);
		ao.UpdateFirstLastFrame();
	}
	g_redraw = true;
}
// Delete Keyframe: saca los keyframes del frame actual de cada objeto seleccionado
void BorrarKeyframeObjeto(){
	for (size_t s=0;s<ObjSelects.size();s++){ Object* o=ObjSelects[s]; if (!o) continue;
		for (size_t i=0;i<AnimationObjects.size();i++) if (AnimationObjects[i].obj==o)
			for (size_t p=0;p<AnimationObjects[i].Propertys.size();p++){
				std::vector<keyFrame>& kf = AnimationObjects[i].Propertys[p].keyframes;
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
void AplicarAnimacionObjetos(){
	// las curvas de objetos SON la animacion de ESCENA: solo reproducen cuando la animacion activa es una escena
	// (kind 0). Con un clip de armature activo (kind 1) la escena no reproduce -> los objetos quedan quietos.
	extern int ActiveAnimKind;
	if (ActiveAnimKind != 0) return;
	static int ultimoFrame = -999999;
	if (CurrentFrame == ultimoFrame) return;
	ultimoFrame = CurrentFrame;
	for (size_t i=0;i<AnimationObjects.size();i++){ AnimationObject& ao=AnimationObjects[i]; if (!ao.obj) continue;
		// que propiedades tienen ALGUNA curva con keyframes? (cada componente X/Y/Z es una curva propia)
		bool hayP=false, hayR=false, hayS=false;
		for (size_t p=0;p<ao.Propertys.size();p++){ if (ao.Propertys[p].keyframes.empty()) continue;
			if      (ao.Propertys[p].Property==AnimPosition) hayP=true;
			else if (ao.Propertys[p].Property==AnimRotation) hayR=true;
			else if (ao.Propertys[p].Property==AnimScale)    hayS=true; }
		// EvalPropVec evalua X/Y/Z por separado; el componente sin curva propia queda en el valor actual del objeto
		if (hayP) ao.obj->pos   = EvalPropVec(ao.Propertys, AnimPosition, CurrentFrame, ao.obj->pos);
		if (hayS) ao.obj->scale = EvalPropVec(ao.Propertys, AnimScale,    CurrentFrame, ao.obj->scale);
		if (hayR){ Vector3 e = EvalPropVec(ao.Propertys, AnimRotation, CurrentFrame, ao.obj->rotEuler);
			ao.obj->rot = Quaternion::FromEulerXYZ(e.x,e.y,e.z); ao.obj->ActualizarDisplayRot(); }
	}
}
#endif

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

void CalcObjectsTransformPivotPoint(Object* obj){
	if (obj->select){
		// el pivote de TRANSFORM (Median Point) usa el ORIGEN del objeto, no su
		// centro geometrico. El FOCO ('.') si usa el centro geometrico, pero por
		// otro camino (CentroFocoSeleccion). Son cosas distintas.
		TransformPivotPoint += obj->GetGlobalPosition();
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
		// Active Element: pivote = el origen del objeto activo
		if (g_transformPivot == PivotActive && ObjActivo) {
			TransformPivotPoint = ObjActivo->GetGlobalPosition(); return;
		}
		// Median Point (default) + Individual (el apply lo ignora): promedio de
		// los focos de los seleccionados (la Mesh aporta su centro geometrico)
		TransformPivotPoint.x = 0.0f;
		TransformPivotPoint.y = 0.0f;
		TransformPivotPoint.z = 0.0f;
		for(size_t c=0; c < SceneCollection->Childrens.size(); c++){
			CalcObjectsTransformPivotPoint(SceneCollection->Childrens[c]);
		}
		size_t SelectCount = ObjSelects.size();
		if (SelectCount == 0) return;
		TransformPivotPoint.x /= SelectCount;
		TransformPivotPoint.y /= SelectCount; // altura → Y OpenGL
		TransformPivotPoint.z /= SelectCount; // profundidad → Z OpenGL
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
		NuevoEstado.rot = obj->rot;
        NuevoEstado.scale = obj->scale;
        NuevoEstado.worldPos = obj->GetGlobalPosition(); // para rotar/escalar desde el pivot
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
	if (ObjActivo && InteractionMode == ObjectMode && ObjActivo->select && estado == editNavegacion){
		if (!guardarEstado()) return;
		estado = translacion; g_xformPrimerMov = true; // primer motion en cero (no salta)
		axisSelect = ViewAxis;
		ToolbarRegistrarAccion(TBMove); // historial de la barra de herramientas
	}
};

// duplica los objetos seleccionados (deep copy). Implementacion NUEVA y
// compartida: reemplaza al cuerpo viejo comentado (era del modelo viejo).
#include "objects/Mesh.h"
#include "Instance.h"
#include "objects/Light.h"
#include "Camera.h"
#include "Empty.h"
#include <string.h>

static Object* W3dDuplicarUno(Object* src) {
    Object* nuevo = NULL;
    if (src->getType() == ObjectType::mesh) {
        Mesh* m = (Mesh*)src;
        Mesh* d = new Mesh(src->Parent, src->pos);
        d->vertexSize = m->vertexSize;
        if (m->vertex) {
            d->vertex = new GLfloat[m->vertexSize * 3];
            memcpy(d->vertex, m->vertex, sizeof(GLfloat) * m->vertexSize * 3);
        }
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
        // VERTEX GROUPS (pesos de skinning) -> el modificador Armature no tenia a que aplicar y la malla no deformaba.
        for (size_t i = 0; i < m->uvMaps.size(); i++)       d->uvMaps.push_back(new UVMap(*m->uvMaps[i]));
        for (size_t i = 0; i < m->colorLayers.size(); i++)  d->colorLayers.push_back(new ColorLayer(*m->colorLayers[i]));
        for (size_t i = 0; i < m->vertexGroups.size(); i++) d->vertexGroups.push_back(new VertexGroup(*m->vertexGroups[i]));
        d->uvMapActivo = m->uvMapActivo; d->colorActivo = m->colorActivo; d->grupoActivo = m->grupoActivo;
        d->vertCtrlPoint = m->vertCtrlPoint; // vertice-render -> control-point (skinning / weight paint)
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
        }
        nuevo = l;
    }
    else if (src->getType() == ObjectType::camera) {
        nuevo = new Camera(src->Parent, src->pos, src->rotEuler);
    }
    else if (src->getType() == ObjectType::empty) {
        nuevo = new Empty(src->Parent, src->pos);
    }
    if (nuevo) {
        nuevo->pos = src->pos;
        nuevo->rotEuler = src->rotEuler;
        nuevo->rot = src->rot;
        nuevo->scale = src->scale;
        nuevo->name = src->name + ".001";
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

void Notificar(const std::string& msg, bool error); // LayoutInput.cpp (toasts); forward-decl (no incluir el header)

// invierte una matriz AFIN 4x4 (columna-major: m[col*4+fila]; m[12..14]=traslacion; fila inferior [0,0,0,1]).
// Invierte la parte lineal 3x3 (adjugada/det) y la traslacion (-Ainv*t). false si es singular (escala 0).
static bool InvertAffine(const Matrix4& in, Matrix4& out) {
    const float* m = in.m;
    float a=m[0], b=m[4], c=m[8];   // A[fila][col] = m[col*4+fila]
    float d=m[1], e=m[5], f=m[9];
    float g=m[2], h=m[6], i=m[10];
    float det = a*(e*i - f*h) + b*(f*g - d*i) + c*(d*h - e*g);
    if (det > -1e-12f && det < 1e-12f) { out = in; return false; }
    float invDet = 1.0f/det;
    float i00=(e*i-f*h)*invDet, i01=(c*h-b*i)*invDet, i02=(b*f-c*e)*invDet;
    float i10=(f*g-d*i)*invDet, i11=(a*i-c*g)*invDet, i12=(c*d-a*f)*invDet;
    float i20=(d*h-e*g)*invDet, i21=(b*g-a*h)*invDet, i22=(a*e-b*d)*invDet;
    float tx=m[12], ty=m[13], tz=m[14];
    out.m[0]=i00; out.m[1]=i10; out.m[2]=i20; out.m[3]=0;   // out.m[col*4+fila] = Ainv[fila][col]
    out.m[4]=i01; out.m[5]=i11; out.m[6]=i21; out.m[7]=0;
    out.m[8]=i02; out.m[9]=i12; out.m[10]=i22; out.m[11]=0;
    out.m[12]=-(i00*tx + i01*ty + i02*tz);
    out.m[13]=-(i10*tx + i11*ty + i12*tz);
    out.m[14]=-(i20*tx + i21*ty + i22*tz);
    out.m[15]=1;
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
    if (!active) { Notificar("Join: no active mesh object", true); return; }
    Mesh* am = (Mesh*)active;

    std::vector<Object*> merged; // mallas seleccionadas != activo (no-Mesh se ignoran; SIN duplicados)
    for (size_t i = 0; i < ObjSelects.size(); i++) {
        Object* o = ObjSelects[i];
        if (!o || o == active || !o->select || o->getType() != ObjectType::mesh) continue; // !select: pudo quedar en ObjSelects deseleccionado
        bool dup = false; for (size_t k = 0; k < merged.size(); k++) if (merged[k] == o) { dup = true; break; } // ObjSelects puede traer
        if (!dup) merged.push_back(o); // la misma malla repetida -> sin este dedup se anexaba 2 veces (caras dobladas -> malla "invisible"/rota)
    }
    if (merged.empty()) { Notificar("Join: select 2+ mesh objects (active is the target)", true); return; }

    Matrix4 Wa; active->GetWorldMatrix(Wa);   // mundo->local del activo (cadena de padres completa)
    Matrix4 invWa; InvertAffine(Wa, invWa);

    UndoJoinIniciar(am); // snapshot de la geo del activo ANTES de anexar (undo atomico)

    for (size_t i = 0; i < merged.size(); i++) {
        Matrix4 Wo; merged[i]->GetWorldMatrix(Wo);
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
    if (mallas.empty() && arms.empty()) { Notificar("Apply: select mesh or armature object(s)", true); return; }

    Quaternion idRot = Quaternion::FromEulerXYZ(0.0f,0.0f,0.0f); // identidad
    UndoApplyIniciar(); // snapshot de transforms + geo de mallas + rest de huesos de armatures (undo atomico)

    // MALLAS: hornear el transform en los VERTICES (v_new = inv(M_reset)*M_actual*v)
    for (size_t i=0;i<mallas.size();i++){
        Object* o = mallas[i];
        Vector3 pos2 = o->pos; Quaternion rot2 = o->rot; Vector3 scl2 = o->scale; // valores reseteados
        if (what==0 || what==3) pos2 = Vector3(0,0,0);
        if (what==1 || what==3) rot2 = idRot;
        if (what==2 || what==3) scl2 = Vector3(1,1,1);
        Matrix4 M  = o->BuildMatrix(o->pos, o->rot, o->scale); // actual (T*R*S)
        Matrix4 Mp = o->BuildMatrix(pos2,  rot2,  scl2);       // reseteado
        Matrix4 invMp; InvertAffine(Mp, invMp);
        Matrix4 B = invMp * M;                                 // v_new = inv(M_reset)*M_actual*v
        ((Mesh*)o)->AplicarMatriz(B);
        o->pos = pos2; o->rot = rot2; o->scale = scl2;         // resetear el componente en el objeto
        o->ActualizarDisplayRot();
    }
    // ARMATURES: hornear el transform del objeto en los HUESOS (rest). Como skinA no cambia -> skinMatrix'=B*skinMatrix
    // y las mallas skinneadas HIJAS quedan identicas al resetear el transform del armature (ej: normalizar un rig 100x).
    for (size_t i=0;i<arms.size();i++){
        Armature* a = arms[i];
        Vector3 pos2 = a->pos; Quaternion rot2 = a->rot; Vector3 scl2 = a->scale;
        if (what==0 || what==3) pos2 = Vector3(0,0,0);
        if (what==1 || what==3) rot2 = idRot;
        if (what==2 || what==3) scl2 = Vector3(1,1,1);
        Matrix4 M  = a->BuildMatrix(a->pos, a->rot, a->scale); // M_arm actual
        Matrix4 Mp = a->BuildMatrix(pos2,  rot2,  scl2);       // M_arm reseteado
        Matrix4 invMp; InvertAffine(Mp, invMp);
        Matrix4 B = invMp * M;                                 // se hornea en espacio nodo (world_FK'=B*world_FK)
        HornearTransformEnHuesos(a, B);
        a->pos = pos2; a->rot = rot2; a->scale = scl2; a->ActualizarDisplayRot();
        InvalidarSkinDeArmature(a); // libera el cache + re-skinnea las mallas hijas a la nueva rest
    }
    UndoApplyConfirmar();
    const char* nom = (what==0)?"Location":(what==1)?"Rotation":(what==2)?"Scale":"All Transforms";
    Notificar(std::string("Apply ") + nom + ": baked", false);
}

// Set Active Object as Camera (Ctrl+Numpad 0 / menu View > Cameras): hace que el objeto ACTIVO sea la CAMARA
// activa de la escena. Solo si el activo ES una camara (si no es camara NO se puede -> no hace nada, false).
bool SetActiveObjectAsCamera(){
    if (!ObjActivo || ObjActivo->getType() != ObjectType::camera) { Notificar("Set Active Camera: the active object is not a camera", true); return false; }
    CameraActive = (Camera*)ObjActivo; // Camera : public Object (1ra base) -> cast offset 0
    return true;
}

static void W3dReparent(Object* obj, Object* nuevoPadre) {
    if (!obj || !nuevoPadre || obj == nuevoPadre) return;
    Object* viejoPadre = obj->Parent ? obj->Parent : SceneCollection;
    if (viejoPadre == nuevoPadre) return;
    // conservar la posicion GLOBAL (v1: solo traslacion)
    Vector3 g = obj->GetGlobalPosition();
    Vector3 gp = nuevoPadre->GetGlobalPosition();
    for (size_t i = 0; i < viejoPadre->Childrens.size(); i++) {
        if (viejoPadre->Childrens[i] == obj) {
            viejoPadre->Childrens.erase(viejoPadre->Childrens.begin() + i);
            break;
        }
    }
    nuevoPadre->Childrens.push_back(obj);
    obj->Parent = nuevoPadre;
    obj->pos = g - gp;
}

// rotacion GLOBAL por la cadena de padres (R = R_padre * R_local). NO-static:
// el transform de sub-elementos de malla (editor) la usa para world<->local.
Quaternion RotGlobalDe(Object* o) {
    if (!o) return Quaternion(1, 0, 0, 0);
    if (!o->Parent) return o->rot;
    return RotGlobalDe(o->Parent) * o->rot;
}

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
    QuitarDePadre(obj);
    nuevoPadre->Childrens.push_back(obj);
    obj->Parent = nuevoPadre;
}

// reparenta MANTENIENDO la transformacion GLOBAL: recalcula pos, rot y
// escala locales para que el objeto quede exactamente donde estaba
void ReparentKeepTransform(Object* obj, Object* nuevoPadre) {
    if (!ReparentValido(obj, nuevoPadre)) return;
    if ((obj->Parent ? obj->Parent : SceneCollection) == nuevoPadre) return;

    Quaternion rg = RotGlobalDe(obj);
    Vector3 sg = ScaleGlobalDe(obj);
    Vector3 pg = obj->GetGlobalPosition();

    Quaternion prg = RotGlobalDe(nuevoPadre);
    Vector3 psg = ScaleGlobalDe(nuevoPadre);
    Vector3 ppg = nuevoPadre->GetGlobalPosition();

    QuitarDePadre(obj);
    nuevoPadre->Childrens.push_back(obj);
    obj->Parent = nuevoPadre;

    Quaternion ipr = prg.Inverted();
    Vector3 d = ipr * (pg - ppg); // el delta en el espacio del padre
    obj->pos = Vector3(psg.x != 0.0f ? d.x / psg.x : d.x,
                       psg.y != 0.0f ? d.y / psg.y : d.y,
                       psg.z != 0.0f ? d.z / psg.z : d.z);
    obj->rot = ipr * rg;
    obj->rot.normalize();
    obj->ActualizarDisplayRot();
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
    if ((obj->Parent ? obj->Parent : SceneCollection) != padre) {
        ReparentKeepTransform(obj, padre);
        if ((obj->Parent ? obj->Parent : SceneCollection) != padre) return;
    }
    // reubicar dentro del vector del padre
    QuitarDePadre(obj);
    size_t idx = padre->Childrens.size();
    for (size_t i = 0; i < padre->Childrens.size(); i++) {
        if (padre->Childrens[i] == ref) {
            idx = i + (despues ? 1 : 0);
            break;
        }
    }
    padre->Childrens.insert(padre->Childrens.begin() + idx, obj);
    obj->Parent = padre;
}

void SetParentSeleccion() {
    if (!ObjActivo || !SceneCollection) return;
    // todos los seleccionados (menos el activo) pasan a ser hijos del activo
    std::vector<Object*> sel;
    for (size_t c = 0; c < SceneCollection->Childrens.size(); c++) {
        Object* o = SceneCollection->Childrens[c];
        if (o->select && o != ObjActivo) sel.push_back(o);
    }
    for (size_t i = 0; i < sel.size(); i++) {
        W3dReparent(sel[i], ObjActivo);
    }
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
    for (size_t i = 0; i < sel.size(); i++) {
        W3dReparent(sel[i], SceneCollection);
    }
}

void NewInstance(){
	if (estado != editNavegacion || InteractionMode != ObjectMode){return;};

	// ITERAMOS AL REVÉS PARA PODER ELIMINAR SIN ROMPER EL ÍNDICE
    for (int i = (int)ObjSelects.size() - 1; i >= 0; i--) {
        Object* obj = ObjSelects[i];     // objeto original
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
		ObjActivo->rot.normalize();             // se editaron w/x/y/z directo
	} else if (ObjActivo->rotMode == RotAxisAngle){
		// el eje tiene que ser unitario para que el angulo sea correcto
		ObjActivo->rot = Quaternion::FromAxisAngle(ObjActivo->rotAxis.Normalized(),
		                                           ObjActivo->rotAngle);
	} else { // RotEulerXYZ
		ObjActivo->rot = Quaternion::FromEulerXYZ(ObjActivo->rotEuler.x,
		                                          ObjActivo->rotEuler.y,
		                                          ObjActivo->rotEuler.z);
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
		obj.rot = Quaternion::FromAxisAngle(axis, ang) * obj.rot;
		obj.rot.normalize();
		obj.ActualizarDisplayRot();
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
		obj.rot = q * obj.rot; // pre-multiplica: gira en los ejes de la vista
		obj.rot.normalize();
		obj.ActualizarDisplayRot();
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
			obj.rot = Quaternion::FromAxisAngle(ax, v) * st.rot;
			obj.rot.normalize(); obj.ActualizarDisplayRot();
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
    Vector3 ppg = p->GetGlobalPosition();
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
            Quaternion delta = obj.rot * st.rot.Inverted();
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

static void SnapObjCapturar(){
    g_objSnapPts.clear(); g_objSnapHayAct = false;
    for (size_t o=0;o<estadoObjetos.size();o++){
        Object* ob = estadoObjetos[o].obj; if (!ob) continue;
        bool verts=false;
        if (ob->getType()==ObjectType::mesh){
            Mesh* m=(Mesh*)ob;
            if (m->vertex && m->vertexSize>0){
                Matrix4 W; m->GetWorldMatrix(W);
                for (int v=0; v<m->vertexSize; v++)
                    g_objSnapPts.push_back(W * Vector3(m->vertex[v*3], m->vertex[v*3+1], m->vertex[v*3+2]));
                verts=true;
            }
        }
        if (!verts) g_objSnapPts.push_back(ob->GetGlobalPosition()); // sin verts (camara/lampara/empty) -> el origen
        if (ob==ObjActivo){ g_objSnapActivo = ob->GetGlobalPosition(); g_objSnapHayAct = true; }
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
        ob.rot = Quaternion::FromAxisAngle(axis, angDeg) * st.rot; ob.rot.normalize(); ob.ActualizarDisplayRot(); }
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
        // cursor 3d (MUNDO) -> LOCAL del mesh (inversa de LocalAMundo): quita origen, rotacion y escala globales.
        Quaternion rg = RotGlobalDe(m);
        Vector3    sg = ScaleGlobalDe(m);
        Vector3    d  = rg.Inverted() * (cursor3D.pos - m->GetGlobalPosition());
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
        Vector3 delta = cursor3D.pos - ObjActivo->GetGlobalPosition();
        std::vector<Vector3> g(sel.size());
        for (size_t i = 0; i < sel.size(); i++) g[i] = sel[i]->GetGlobalPosition();
        for (size_t i = 0; i < sel.size(); i++) PonerEnGlobal(sel[i], g[i] + delta);
    } else {
        for (size_t i = 0; i < sel.size(); i++)
            PonerEnGlobal(sel[i], cursor3D.pos);
    }
}

void SnapSeleccionAlActivo() {
    if (InteractionMode == EditMode) return; // edit: TODO sub-elementos (Fase 2a)
    if (!ObjActivo) return;
    Vector3 destino = ObjActivo->GetGlobalPosition();
    std::vector<Object*> sel;
    RecolectarSnap(SceneCollection, sel);
    for (size_t i = 0; i < sel.size(); i++)
        if (sel[i] != ObjActivo) PonerEnGlobal(sel[i], destino);
}

void SnapSeleccionAlGrid() {
    if (InteractionMode == EditMode) return; // edit: TODO sub-elementos (Fase 2a)
    std::vector<Object*> sel;
    RecolectarSnap(SceneCollection, sel);
    // snapshot de globales antes de mover (padre+hijo seleccionados)
    std::vector<Vector3> g(sel.size());
    for (size_t i = 0; i < sel.size(); i++) g[i] = sel[i]->GetGlobalPosition();
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
static void DesplazarVertices(Mesh* m, const Vector3& d) {
    if (!m->vertex) return;
    for (int i = 0; i < m->vertexSize; i++) {
        m->vertex[i*3]   += d.x;
        m->vertex[i*3+1] += d.y;
        m->vertex[i*3+2] += d.z;
    }
    m->CalcularBordes(); // recalcula centroGeom + edges + bordesBuf (+ invalida edit)
}

// (R*S)^-1 * d : pasa un vector de PARENT-local a MESH-local (deshace rot+escala
// del objeto). Sirve para que los vertices compensen un movimiento del origen.
static Vector3 ParentLocalAMeshLocal(Mesh* m, const Vector3& d) {
    Vector3 rd = m->rot.Inverted() * d; // R^-1
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
    for (size_t i = 0; i < ms.size(); i++)
        DesplazarVertices(ms[i], ms[i]->centroGeom * -1.0f); // verts -= baricentro
}

// 2) Origin to Geometry: el ORIGEN se mueve al baricentro y la geometria queda EN SU
//    LUGAR. Los vertices se mueven igual que (1), pero el objeto se mueve al reves
//    (pos += R*S*baricentro) para compensar.
void SetOriginOriginToGeometry() {
    std::vector<Mesh*> ms; RecolectarMallasSel(ms);
    for (size_t i = 0; i < ms.size(); i++) {
        Mesh* m = ms[i];
        Vector3 c = m->centroGeom;
        // baricentro local -> offset en parent-local (aplica escala y rotacion del obj)
        Vector3 sc(m->scale.x*c.x, m->scale.y*c.y, m->scale.z*c.z);
        m->pos += m->rot * sc;       // el origen va a donde estaba el baricentro
        DesplazarVertices(m, c * -1.0f); // verts -= baricentro (la geometria no se mueve)
    }
}

// 3) Origin to 3D Cursor: el ORIGEN se mueve al cursor 3D y la geometria queda en su
//    lugar. Lo que se movio el objeto, los vertices lo compensan al reves.
void SetOriginToCursor() {
    std::vector<Mesh*> ms; RecolectarMallasSel(ms);
    for (size_t i = 0; i < ms.size(); i++) {
        Mesh* m = ms[i];
        Vector3 oldPos = m->pos;
        PonerEnGlobal(m, cursor3D.pos);  // origen -> cursor (world->local, respeta padre)
        // los vertices compensan (oldPos - newPos) llevado a mesh-local
        DesplazarVertices(m, ParentLocalAMeshLocal(m, oldPos - m->pos));
    }
}

void SnapCursorALoSeleccionado() {
    // Edit Mode: el cursor al CENTRO de los sub-elementos seleccionados de la malla
    // (mismo calculo que el foco; en mundo via LocalAMundo). Da igual vertex/edge/face.
    if (InteractionMode == EditMode && g_editMesh) {
        Mesh* m = (Mesh*)g_editMesh;
        m->EnsureEdit();
        float cx, cy, cz;
        if (m->edit && m->edit->CentroSeleccion(cx, cy, cz))
            cursor3D.pos = m->LocalAMundo(Vector3(cx, cy, cz));
        return;
    }
    std::vector<Object*> sel;
    RecolectarSnap(SceneCollection, sel);
    if (sel.empty()) return;
    Vector3 suma(0.0f, 0.0f, 0.0f);
    for (size_t i = 0; i < sel.size(); i++) suma += sel[i]->GetGlobalPosition();
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
            cursor3D.pos = m->LocalAMundo(Vector3(cx, cy, cz));
        return;
    }
    if (!ObjActivo) return;
    cursor3D.pos = ObjActivo->GetGlobalPosition();
}