// ===================================================================================================
//  POSE MODE: transform interactivo de huesos (G/R/S). Extraido de LayoutInput.cpp (Fase 2 del reorg).
//  Todo el estado (g_pose*) es file-local; las entradas publicas estan en PoseTransform.h.
// ===================================================================================================
#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "Undo.h" // Ctrl+Z: capturar modo / seleccion
#include "ViewPorts/PopUp/ConfirmarPopup.h" // AbrirConfirmarBorrado (popup de confirmar borrado)
#include "ViewPorts/LayoutInput.h"
#include "ViewPorts/ViewPort3D.h"
#include "ViewPorts/Outliner.h"
#include "ViewPorts/Properties.h"
#include "ViewPorts/UVEditor.h"
#include "ViewPorts/Timeline.h"
#include "WhiskUI/draw/glesdraw.h"
#include "WhiskUI/draw/rectangle.h" // el velo del modo foco
#include "objects/Objects.h"
#include "objects/Mesh.h"
#include "objects/Materials.h" // Material (mat->texture) para el dropdown "Texture" del UV editor
#include "objects/Textures.h"  // Texture (path) para las etiquetas del dropdown
#include "objects/EditMesh.h"
#include "objects/Light.h"
#include "objects/Camera.h"
#include "objects/Empty.h"
#include "objects/Armature.h"
#include "animation/SkeletalAnimation.h" // InsertarKeyframeEsqueleto (Pose Mode: Insert Keyframe)
#include "objects/Instance.h"
#include "objects/Collection.h"
#include "objects/ObjectMode.h"
#include "edit/Modifier.h" // ModifierType::Mirror + target (regen de mirrors al mover objetos)
#include "objects/Primitivas.h"
#include "variables.h"
#include "render/OpcionesRender.h" // g_fpsActual
#include "ViewPorts/PopUp/PopUpBase.h"
#include "ViewPorts/PopUp/RedoMeshPanel.h"
#include "WhiskUI/widgets/card.h"        // tarjeta de las notificaciones
#include "WhiskUI/text/bitmapText.h"  // texto de las notificaciones
#include "WhiskUI/draw/icons.h"       // iconos notifOk / notifError
#include "WhiskUI/theme/colores.h"     // ColorID
#include "w3dlog.h"         // las notificaciones tambien van al log
#include "ViewPorts/PoseTransform.h"

// ===== POSE MODE: transform interactivo de huesos (G/R/S) UNIFICADO con el estado de Object Mode =====
// Los huesos se transforman 1:1 con los objetos: usa el MISMO `estado` (translacion/rotacion/EditScale) +
// axisSelect/transformOrientation (ejes X/Y/Z global/local/view) + gAnguloTransform + la entrada NUMERICA (NumInput*)
// + la barra de estado (header). Cada hueso se edita por su WORLD en espacio NODO: world' = Delta*worldSnap y el local
// se recalcula contra el padre -> multi-hueso rota/escala alrededor de un PIVOTE COMPARTIDO (por eso tambien se MUEVEN),
// igual que Object Mode. Confirm = click/Enter/tick; Cancel = Esc/click-der/cruz. La pose se guarda con Insert Keyframe.
int g_poseModo = 0; // 0=none 1=grab 2=rotate 3=scale (espejo de `estado`)
struct PoseSnap { int b, parent, prof; Vector3 T, R, S; Matrix4 world; }; // world = world NODO al arrancar; prof = profundidad
static std::vector<PoseSnap> g_poseSnap;
static Matrix4  g_poseW2N;         // mundo(engine) -> nodo del armature (al arrancar)
static Vector3  g_posePivotNode;   // pivote en espacio nodo (rotate/scale)
static int      g_poseActivo = -1; // hueso activo al arrancar (pivote Active / eje Local)
static float    g_poseAccX = 0.0f, g_poseAccY = 0.0f; // delta de mouse acumulado desde el inicio
static int      g_poseLastX = 0, g_poseLastY = 0;
static bool     g_poseTrackCap = false; static float g_poseTrackAng0 = 0.0f; // trackball: angulo del mouse al arrancar
static bool     g_poseNum = false; static float g_poseNumV = 0.0f; // valor numerico exacto activo (tipeado)
extern void NumInputReset();
extern void UndoPoseIniciar(Armature*); extern void UndoPoseConfirmar();
static Armature* PoseArmActiva(){
    return (InteractionMode == PoseMode && ObjActivo && ObjActivo->getType() == ObjectType::armature) ? (Armature*)ObjActivo : NULL;
}
static Matrix4 PMatTrans(const Vector3& t){ Matrix4 m; m.Identity(); m.m[12]=t.x; m.m[13]=t.y; m.m[14]=t.z; return m; }
static Matrix4 PMatScale(const Vector3& s){ Matrix4 m; m.Identity(); m.m[0]=s.x; m.m[5]=s.y; m.m[10]=s.z; return m; }
// mundo(engine Y-up, con el transform del objeto armature) -> espacio NODO. glTF: el nodo YA es Y-up (sin NodeToYup);
// FBX: el nodo es Z-up. (Aplicar NodeToYup a un rig glTF era una de las causas de "rotar desde vista anda mal".)
static Matrix4 PoseW2N(Armature* a){
    Matrix4 AW; a->GetWorldMatrix(AW); Matrix4 invAW = AW.Inverse();
    return a->skinGltf ? invAW : (SkelNodeToYupMat().Inverse() * invAW);
}
static Vector3 PoseDirW2N(const Matrix4& W2N, const Vector3& d){ // direccion world -> nodo (resta el origen)
    Vector3 o = W2N * Vector3(0,0,0); Vector3 r = (W2N * d) - o; float l = r.Length(); return (l>1e-8f)? r*(1.0f/l): r; }
static Vector3 PoseCam(int which){ // ejes de camara: 0=right 1=up 2=forward
    Quaternion vr = Viewport3DActive ? Viewport3DActive->viewRot : Quaternion(1,0,0,0);
    return vr * ((which==0)?Vector3(1,0,0):(which==1)?Vector3(0,1,0):Vector3(0,0,-1));
}
// eje constrenido (X/Y/Z) en espacio NODO segun axisSelect/transformOrientation (misma convencion que EjeMundo: Y=profundidad, Z=arriba)
static Vector3 PoseAxisNode(Armature* a, int ax){
    if (transformOrientation == LocalOrient && g_poseActivo >= 0){
        // LOCAL = el eje del HUESO ACTIVO, con la MISMA convencion que EjeOrientado(obj) de Object Mode:
        // "eje local" = rotacion_del_hueso * EjeMundo(ax). EjeMundo mapea user X->(1,0,0), Y->(0,0,1) (profundidad),
        // Z->(0,1,0) (arriba) -> en el world del hueso EN ESPACIO ENGINE eso es col0 / col2 / col1.
        // (Antes se tomaban las columnas del world en espacio NODO y en el orden 0/1/2: Y y Z quedaban CAMBIADOS
        //  respecto de Global, y en rigs FBX -nodo Z-up- ademas no era el espacio correcto.)
        Matrix4 N2W = g_poseW2N.Inverse();
        Matrix4 Wb  = N2W * SkelBoneWorldNode(a, g_poseActivo); // world del hueso en espacio ENGINE
        int c = (ax==0) ? 0 : (ax==1) ? 2 : 1;
        Vector3 w(Wb.m[c*4], Wb.m[c*4+1], Wb.m[c*4+2]);
        float l = w.Length(); if (l>1e-8f) w = w*(1.0f/l);
        return PoseDirW2N(g_poseW2N, w);                       // ...y de vuelta a NODO para armar el Delta
    }
    Vector3 world;
    if (transformOrientation == ViewOrient) world = (ax==0)?PoseCam(0):(ax==1)?PoseCam(2):PoseCam(1); // X=right Y=fwd Z=up
    else world = (ax==0)?Vector3(1,0,0):(ax==1)?Vector3(0,0,1):Vector3(0,1,0);                        // GLOBAL (engine Y-up)
    return PoseDirW2N(g_poseW2N, world);
}
// pivote en espacio NODO segun g_transformPivot (median de las cabezas seleccionadas / hueso activo / individual=median-fallback)
static Vector3 PosePivotNode(Armature* a, const std::vector<int>& sel){
    if (g_transformPivot == PivotActive && g_poseActivo >= 0){ Matrix4 W = SkelBoneWorldNode(a, g_poseActivo); return W * Vector3(0,0,0); }
    Vector3 acc(0,0,0); int n=0;
    for (size_t k=0;k<sel.size();k++){ Matrix4 W = SkelBoneWorldNode(a, sel[k]); acc = acc + (W * Vector3(0,0,0)); n++; }
    return (n>0)? acc*(1.0f/(float)n) : acc;
}
// descompone un local de hueso (T*PreRot*R(order)*S) de vuelta a poseT/R/S
static void PoseDecomp(W3dBone& b, const Matrix4& L){
    b.poseT = Vector3(L.m[12], L.m[13], L.m[14]);
    Matrix4 noT = L; noT.m[12]=noT.m[13]=noT.m[14]=0.0f;
    Matrix4 RS = SkelMatRotEuler(b.preRot, 0).Inverse() * noT; // = R(order)*S
    Vector3 cx(RS.m[0],RS.m[1],RS.m[2]), cy(RS.m[4],RS.m[5],RS.m[6]), cz(RS.m[8],RS.m[9],RS.m[10]);
    float sx=cx.Length(), sy=cy.Length(), sz=cz.Length();
    Matrix4 Rm; Rm.Identity();
    if(sx>1e-8f){Rm.m[0]=cx.x/sx;Rm.m[1]=cx.y/sx;Rm.m[2]=cx.z/sx;}
    if(sy>1e-8f){Rm.m[4]=cy.x/sy;Rm.m[5]=cy.y/sy;Rm.m[6]=cy.z/sy;}
    if(sz>1e-8f){Rm.m[8]=cz.x/sz;Rm.m[9]=cz.y/sz;Rm.m[10]=cz.z/sz;}
    b.poseR = SkelMatrizAEulerFBX(Rm, b.rotOrder);
    b.poseS = Vector3(sx, sy, sz);
}
static void PoseAplicar(Armature* a); // fwd (definida abajo)
extern bool NumInputActivo();
void PoseXformStart(int modo){
    Armature* a = PoseArmActiva(); if (!a) return;
    if (estado != editNavegacion) return; // ya hay un transform en curso
    std::vector<int> sel;
    for (size_t i = 0; i < a->bones.size(); i++){ W3dBone& b = a->bones[i]; if (b.select || (int)i == a->boneActivo) sel.push_back((int)i); }
    if (sel.empty()) return;
    std::vector<char> isSel(a->bones.size(), 0); for (size_t k=0;k<sel.size();k++) isSel[sel[k]]=1;
    g_poseSnap.clear();
    for (size_t k=0;k<sel.size();k++){ int i=sel[k]; W3dBone& b=a->bones[i];
        PoseSnap s; s.b=i; s.parent=b.parent; s.T=b.poseT; s.R=b.poseR; s.S=b.poseS;
        s.world = SkelBoneWorldNode(a, i);
        int d=0, p=b.parent, guard=0; while(p>=0 && p<(int)a->bones.size() && guard++<(int)a->bones.size()){ d++; p=a->bones[p].parent; } s.prof=d; // profundidad (para orden topologico)
        g_poseSnap.push_back(s); }
    g_poseActivo = (a->boneActivo>=0 && a->boneActivo<(int)a->bones.size() && isSel[a->boneActivo]) ? a->boneActivo : sel[0];
    g_poseW2N = PoseW2N(a);
    g_posePivotNode = PosePivotNode(a, sel);
    // El GIZMO, los EJES X/Y/Z y la LINEA PUNTEADA se dibujan en TransformPivotPoint (via GizmoPivot()).
    // Hay que ponerlo en el pivote de la POSE, en MUNDO: antes quedaba donde lo habia dejado Object Mode
    // (el origen del armature) y los ejes salian en el lugar equivocado.
    { Matrix4 N2W = g_poseW2N.Inverse(); TransformPivotPoint = N2W * g_posePivotNode; }
    g_poseTrackCap = false; // trackball: se recaptura el angulo inicial del mouse
    UndoPoseIniciar(a); // Ctrl+Z: snapshot de la pose de TODOS los huesos antes del transform
    // estado COMPARTIDO con Object Mode -> header + numerico + touch keyboard + confirm/cancel se activan solos
    g_poseModo = modo;
    estado = (modo==1)?translacion:(modo==2)?rotacion:EditScale;
    axisSelect = (modo==3)?XYZ:ViewAxis;                     // scale libre = 3 ejes; move/rotate libre = vista
    transformOrientation = (modo==3)?LocalOrient:GlobalOrient;
    gAnguloTransform = 0.0f;
    g_poseAccX = g_poseAccY = 0.0f; g_poseLastX = lastMouseX; g_poseLastY = lastMouseY;
    g_poseNum = false; g_poseNumV = 0.0f; NumInputReset();
}
// llamada desde event_mouse_motion: acumula el delta de mouse y re-aplica desde el snapshot (absoluto -> sin drift)
void PoseXformMotion(int mx, int my){
    Armature* a = PoseArmActiva(); if (!a || !g_poseModo) return;
    if (NumInputActivo()){ g_poseLastX = mx; g_poseLastY = my; return; } // tipeando un valor exacto: el mouse no interfiere
    g_poseNum = false;
    g_poseAccX += (float)(mx - g_poseLastX); g_poseAccY += (float)(my - g_poseLastY);
    g_poseLastX = mx; g_poseLastY = my;
    // LINEA PUNTEADA pivote->mouse (la dibuja RenderBarraTransform): misma que Object Mode. Necesita
    // TransformPivotPoint ya en el pivote de la pose (lo setea PoseXformStart).
    if (Viewport3DActive) Viewport3DActive->ActualizarLineaTransform(mx, my);
    PoseAplicar(a);
}
// valor NUMERICO exacto (tipeado): lo llama NumInputAplicar cuando InteractionMode==PoseMode
void PoseXformNumValor(float v){ Armature* a = PoseArmActiva(); if (!a || !g_poseModo) return; g_poseNum = true; g_poseNumV = v; PoseAplicar(a); }
static void PoseInvalidar(Armature* a){
    if (!a) return; a->poseDirty = true;
    struct L { static void rec(Object* o, Armature* arm){ if (!o) return;
        if (o->getType()==ObjectType::mesh){ Mesh* m=(Mesh*)o; if (m->skinArmature==arm) m->lastSkinFrame=-999999; }
        for (size_t i=0;i<o->Childrens.size();i++) rec(o->Childrens[i], arm); } };
    L::rec(SceneCollection, a);
    g_redraw = true;
}
static void PoseAplicar(Armature* a){
    if (!a || !g_poseModo || g_poseSnap.empty()) return;
    bool axFree  = (axisSelect==ViewAxis || axisSelect==XYZ);      // libre (sin eje unico)
    bool esPlano = (axisSelect==PlaneX || axisSelect==PlaneY || axisSelect==PlaneZ);
    int  ax = (axisSelect==X||axisSelect==PlaneX)?0 : (axisSelect==Y||axisSelect==PlaneY)?1 : (axisSelect==Z||axisSelect==PlaneZ)?2 : 0;
    Matrix4 Delta; Delta.Identity();
    if (g_poseModo == 1){        // ---- TRANSLATE ----
        float wpp = Viewport3DActive ? Viewport3DActive->VelocidadArrastreMundo() : 0.01f;
        Vector3 tNode(0,0,0);
        if (axFree){             // plano de la vista (o valor numerico a lo largo de camRight)
            if (g_poseNum) tNode = PoseDirW2N(g_poseW2N, PoseCam(0)) * g_poseNumV;
            else tNode = PoseDirW2N(g_poseW2N, PoseCam(0)) * (g_poseAccX*wpp) + PoseDirW2N(g_poseW2N, PoseCam(1)) * (-g_poseAccY*wpp);
        } else if (esPlano){     // los OTROS dos ejes
            int a1=(ax+1)%3, a2=(ax+2)%3;
            float m1 = g_poseNum ? g_poseNumV : (g_poseAccX*wpp);
            float m2 = g_poseNum ? 0.0f       : (-g_poseAccY*wpp);
            tNode = PoseAxisNode(a, a1)*m1 + PoseAxisNode(a, a2)*m2;
        } else {                 // eje unico
            float mag = g_poseNum ? g_poseNumV : ((g_poseAccX - g_poseAccY)*wpp);
            tNode = PoseAxisNode(a, ax) * mag;
        }
        Delta = PMatTrans(tNode);
    } else if (g_poseModo == 2){  // ---- ROTATE alrededor del pivote compartido ----
        // TRACKBALL (libre / desde la vista): IGUAL que Object Mode -> el angulo lo da la posicion del MOUSE
        // respecto del PIVOTE proyectado a pantalla (360 grados reales), NO el desplazamiento horizontal.
        // Con un eje constrenido (X/Y/Z) se usa el arrastre horizontal, como Object Mode.
        float ang;
        if (g_poseNum) ang = g_poseNumV;
        else if (axFree && Viewport3DActive){
            float px, py;
            if (Viewport3DActive->ProyectarPunto(TransformPivotPoint, px, py)){
                // OJO: se usa g_poseLastX/Y (la pos VIVA del mouse que guarda PoseXformMotion), NO lastMouseX/Y:
                // esas globales las actualiza CheckWarpMouseInViewport, que en Pose esta DESACTIVADO a proposito
                // (para que el cursor no se envuelva y pegue un tiron) -> quedaban congeladas y el angulo daba 0.
                float lmx = (float)g_poseLastX - (float)Viewport3DActive->x;
                float lmy = (float)g_poseLastY - (float)Viewport3DActive->y;
                float a = 180.0f - atan2f(lmy - py, lmx - px) * 180.0f / 3.14159265f;
                if (!g_poseTrackCap){ g_poseTrackCap = true; g_poseTrackAng0 = a; }
                // NEGADO (igual que Object Mode, que rota con -delta): el hueso tiene que seguir al mouse.
                // Sin el signo la rotacion sale al reves. Solo aplica al trackball: los ejes X/Y/Z y el valor
                // numerico ya van en el sentido correcto.
                ang = g_poseTrackAng0 - a;
            } else ang = g_poseAccX * 0.5f; // el pivote no se pudo proyectar (detras de camara): fallback
        } else ang = g_poseAccX * 0.5f;
        gAnguloTransform = ang;
        Vector3 axisNode = axFree ? PoseDirW2N(g_poseW2N, PoseCam(2)) : PoseAxisNode(a, ax); // libre = forward de camara (trackball)
        Matrix4 R = Quaternion::FromAxisAngle(axisNode, ang).ToMatrix();
        Delta = PMatTrans(g_posePivotNode) * R * PMatTrans(g_posePivotNode * -1.0f);
    } else {                      // ---- SCALE alrededor del pivote compartido ----
        float f = g_poseNum ? g_poseNumV : (1.0f + g_poseAccX * 0.01f);
        if (!g_poseNum && f < 0.01f) f = 0.01f;
        Matrix4 Sc; Sc.Identity();
        if (axFree){ Sc = PMatScale(Vector3(f,f,f)); }
        else {       // escala f a lo largo del eje n:  I + (f-1)*n n^T   (m[col*4+row], column-major)
            Vector3 n = PoseAxisNode(a, ax); float g = f - 1.0f;
            Sc.m[0]=1+g*n.x*n.x; Sc.m[1]=g*n.x*n.y; Sc.m[2]=g*n.x*n.z;
            Sc.m[4]=g*n.y*n.x;   Sc.m[5]=1+g*n.y*n.y; Sc.m[6]=g*n.y*n.z;
            Sc.m[8]=g*n.z*n.x;   Sc.m[9]=g*n.z*n.y;   Sc.m[10]=1+g*n.z*n.z;
        }
        Delta = PMatTrans(g_posePivotNode) * Sc * PMatTrans(g_posePivotNode * -1.0f);
    }
    // aplicar el Delta (espacio NODO) resolviendo la JERARQUIA: se procesa en ORDEN TOPOLOGICO (padres antes que hijos) y
    // el world del padre se recalcula EN VIVO (ya con los ancestros actualizados). Cada hueso queda en world'=Delta*worldSnap;
    // asi un hijo seleccionado cuyo ANCESTRO (no el padre directo) tambien se selecciono NO se transforma dos veces.
    static std::vector<int> orden; orden.clear();
    for (size_t k=0;k<g_poseSnap.size();k++) orden.push_back((int)k);
    for (size_t i=1;i<orden.size();i++){ int ki=orden[i]; int pk=g_poseSnap[ki].prof; int j=(int)i-1;   // insertion sort por profundidad
        while(j>=0 && g_poseSnap[orden[j]].prof>pk){ orden[j+1]=orden[j]; j--; } orden[j+1]=ki; }
    for (size_t oi=0; oi<orden.size(); oi++){
        PoseSnap& s = g_poseSnap[orden[oi]]; if (s.b < 0 || s.b >= (int)a->bones.size()) continue;
        Matrix4 targetWorld = Delta * s.world;
        Matrix4 parentWorld = (s.parent>=0 && s.parent<(int)a->bones.size()) ? SkelBoneWorldNode(a, s.parent) : Matrix4(); // EN VIVO
        Matrix4 newLocal = parentWorld.Inverse() * targetWorld;
        PoseDecomp(a->bones[s.b], newLocal);
    }
    PoseInvalidar(a);
}
void PoseXformConfirm(){
    if (!g_poseModo) return;
    // AUTO KEY: va ANTES de limpiar g_poseSnap, que es contra lo que se mide QUE canal cambio. Solo los huesos
    // que se movieron de verdad dejan keyframe, y solo en sus canales.
    if (AutoKeyOn){
        Armature* a = PoseArmActiva();
        if (a && AutoKeyEsqueletoPrep(a)){
            int n = 0;
            for (size_t k=0;k<g_poseSnap.size();k++){
                PoseSnap& sn = g_poseSnap[k];
                n += AutoKeyHueso(a, sn.b, sn.T, sn.R, sn.S);
            }
            if (n) AutoKeyEsqueletoFin(a);
        }
    }
    g_poseModo = 0; g_poseSnap.clear(); estado = editNavegacion; NumInputReset(); UndoPoseConfirmar();
} // deja la pose (Insert Keyframe la guarda a mano; con Auto Key se guarda sola)
void PoseXformCancel(){
    Armature* a = PoseArmActiva();
    if (a) for (size_t k=0;k<g_poseSnap.size();k++){ PoseSnap& s=g_poseSnap[k]; if (s.b>=0 && s.b<(int)a->bones.size()){
        a->bones[s.b].poseT=s.T; a->bones[s.b].poseR=s.R; a->bones[s.b].poseS=s.S; } }
    if (a) PoseInvalidar(a);
    g_poseModo = 0; g_poseSnap.clear(); estado = editNavegacion; NumInputReset();
}
// X/Y/Z durante un transform de pose: constriñe el eje (1er toque) y cicla Global->Local->View (mismo eje re-apretado),
// luego RE-APLICA desde el snapshot (absoluto). No usa CiclarEjeTransform de Object Mode (esa llama ReestablecerEstado,
// que restaura los OBJETOS, no la pose).
void PoseCiclarEje(int eje){
    if (!g_poseModo) return;
    if (g_poseModo == 3){ transformOrientation = LocalOrient; axisSelect = (axisSelect==eje)?XYZ:eje; } // scale: local, eje<->3ejes
    else if (axisSelect != eje){ axisSelect = eje; }                                                    // 1er toque: eje (orientacion actual)
    else if (transformOrientation == GlobalOrient){ transformOrientation = LocalOrient; }               // mismo eje: cicla orientacion
    else if (transformOrientation == LocalOrient){ transformOrientation = ViewOrient; }
    else { axisSelect = ViewAxis; transformOrientation = GlobalOrient; }                                 // vuelve a libre
    g_poseAccX = 0.0f; g_poseAccY = 0.0f; g_poseLastX = lastMouseX; g_poseLastY = lastMouseY; g_poseNum = false; NumInputReset(); g_poseTrackCap = false;
    Armature* a = PoseArmActiva(); if (a) PoseAplicar(a);
}
void PoseCiclarPlano(int eje){ // Shift+X/Y/Z: constriñe al PLANO (los otros dos ejes). Rotate no tiene plano.
    if (!g_poseModo || g_poseModo==2) return;
    int pl = (eje==X)?PlaneX:(eje==Y)?PlaneY:PlaneZ;
    axisSelect = (axisSelect==pl)?ViewAxis:pl; transformOrientation = GlobalOrient;
    g_poseAccX = 0.0f; g_poseAccY = 0.0f; g_poseLastX = lastMouseX; g_poseLastY = lastMouseY; g_poseNum = false; NumInputReset(); g_poseTrackCap = false;
    Armature* a = PoseArmActiva(); if (a) PoseAplicar(a);
}
void PoseCiclarOrient(){ // "R de nuevo": cicla la orientacion del eje actual (Global->Local->View)
    if (!g_poseModo) return;
    transformOrientation = (transformOrientation==GlobalOrient)?LocalOrient:(transformOrientation==LocalOrient)?ViewOrient:GlobalOrient;
    g_poseAccX = 0.0f; g_poseAccY = 0.0f; g_poseLastX = lastMouseX; g_poseLastY = lastMouseY; g_poseTrackCap = false;
    Armature* a = PoseArmActiva(); if (a) PoseAplicar(a);
}
// EJES de la pose en MUNDO, para que la GUIA (DibujarLineaEjeMundo) dibuje EXACTAMENTE el mismo eje con el que se
// rota. Sin esto la guia usaba EjeOrientado(*ObjActivo,...) = los ejes del OBJETO ARMATURE, que en "Local" NO son
// los del HUESO -> la linea no coincidia con la rotacion real (bug Dante).
// Devuelve false si no hay transform de pose (el caller usa los ejes del objeto, como siempre).
bool PoseEjesMundo(Vector3& ex, Vector3& ey, Vector3& ez){
    Armature* a = PoseArmActiva(); if (!a || !g_poseModo) return false;
    Matrix4 N2W = g_poseW2N.Inverse();
    Vector3 o = N2W * Vector3(0,0,0);
    Vector3 e[3];
    for (int i=0;i<3;i++){
        Vector3 n = PoseAxisNode(a, i);                 // el MISMO eje (en nodo) que usa PoseAplicar
        Vector3 w = (N2W * n) - o;                      // nodo -> mundo (como DIRECCION)
        float l = w.Length(); e[i] = (l>1e-8f) ? w*(1.0f/l) : w;
    }
    ex = e[0]; ey = e[1]; ez = e[2];
    return true;
}

// para el header de Pose (ViewPort3D::W3dTextoTransform): modo activo (1=grab 2=rotate 3=scale) y el valor a mostrar.
int PoseHeaderModo(){ return g_poseModo; }
float PoseHeaderValor(){
    if (g_poseModo==1){ if (g_poseNum) return g_poseNumV; float wpp = Viewport3DActive?Viewport3DActive->VelocidadArrastreMundo():0.01f;
        bool axFree=(axisSelect==ViewAxis||axisSelect==XYZ);
        return axFree ? (Vector3(g_poseAccX, g_poseAccY, 0.0f).Length()*wpp) : ((g_poseAccX-g_poseAccY)*wpp); }
    if (g_poseModo==3){ return g_poseNum ? g_poseNumV : (1.0f + g_poseAccX*0.01f); }
    return g_poseNum ? g_poseNumV : (g_poseAccX*0.5f); // rotate: grados (el header lo toma de gAnguloTransform igual)
}
void PoseInsertKeyframe(){ Armature* a = PoseArmActiva(); if (a){ InsertarKeyframeEsqueleto(a); PoseInvalidar(a); } }
// Pose Mode > Clear Transform: resetea la pose de los huesos SELECCIONADOS a su rest (si no hay seleccion, TODOS).
// what: 0=All (T+R+S), 1=Translation (Alt+G), 2=Rotation (Alt+R), 3=Scale (Alt+S). No inserta keyframe.
void PoseClearTransform(int what){
    Armature* a = PoseArmActiva(); if (!a) return;
    bool haySel = false; for (size_t b = 0; b < a->bones.size(); b++) if (a->bones[b].select){ haySel = true; break; }
    for (size_t b = 0; b < a->bones.size(); b++){ W3dBone& bo = a->bones[b];
        if (haySel && !bo.select) continue; // solo los seleccionados; si no hay ninguno seleccionado -> todos
        if (what == 0 || what == 1) bo.poseT = bo.restT;
        if (what == 0 || what == 2) bo.poseR = bo.restR;
        if (what == 0 || what == 3) bo.poseS = bo.restS;
    }
    PoseInvalidar(a);
}
void PoseClearTransformAll(){ PoseClearTransform(0); } // compat
