#include "WhiskUI/text/font.h"   // UTF8_Largo
#include "ViewPorts/Timeline.h"
#include "ViewPorts/TransformUI.h"     // TbVerdeBg: el verde de un boton ACTIVO (como el proportional editing)
#include "edit/BoxSelect.h"               // la caja de seleccion (verde = entero adentro, azul = rozar)
#include "PopUp/TiempoPopup.h"            // el reloj: Inicio / Fin / FPS
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "w3dGraphics.h"
#include "WhiskUI/draw/glesdraw.h"          // W3dPantallaAlto
#include "WhiskUI/theme/colores.h"           // ListaColores / ColorID
#include "WhiskUI/draw/rectangle.h"         // Rec2D
#include "WhiskUI/text/bitmapText.h"        // RenderBitmapText / textAlign
#include "WhiskUI/core/UI.h"                // GlobalScale, RenglonHeightGS, CharacterWidthGS, LetterHeightGS, ...
#include "objects/Objects.h"           // ObjActivo, g_editMesh (mascara de vertices seleccionados)
#include "objects/Mesh.h"              // invalidar el skin al editar keyframes
#include "objects/EditMesh.h"          // vertSel/editVerts: la seleccion de Edit Mode (toggle Solo verts sel)
#include "objects/Armature.h"
#include "objects/Textures.h"          // Textures[0] = atlas (RenderBar/bordes)
#include "animation/Animation.h"       // StartFrame/EndFrame/CurrentFrame/PlayAnimation/AnimPlayDir + AnimProperty
#include "animation/SkeletalAnimation.h"
#include "animation/W3dAnimSet.h"   // clips COMPARTIDOS: la curva que se edita desde el dope es de un clip PROPIO
#include "animation/VertexAnimation.h"   // fila Vertex del dope: mover/borrar/interpolar frames
#include "animation/Armature2DAnimation.h" // filas del dope de los CLIPS del armature 2D (kind 4)
#include "ViewPorts/UVEditor.h"          // UVEditorEnModoPose: el dope 2D solo se lista posando (como el 3D)
#include "objects/ObjectMode.h"          // AutoKeyOn (el boton del timeline lo prende/apaga)
#include "Undo.h"                       // Ctrl+Z al editar keyframes (borrar/mover/curvar)
#include "render/OpcionesRender.h"     // g_redraw
#include "PopUp/PopUpBase.h"           // PopUpActive
#include "WhiskUI/widgets/card.h"              // Card (boton verde redondeado del frame actual)
#include "WhiskUI/widgets/PopupMenu.h"         // dropdown de seleccion de animacion
#include "WhiskUI/draw/icons.h"             // dope sheet: iconos/flechas de las filas (igual que el Outliner)
#include "variables.h"                 // InteractionMode / PoseMode (el armature solo se lista en Pose Mode)
#include <math.h>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include "objects/Rutina.h"   // las filas de los PASOS de una rutina animados (AnimPaso)
#include "objects/Sonido.h"   // las TIRAS de los sonidos (se mueven y se recortan en segundos)

namespace gfx = w3dEngine;

extern bool leftMouseDown, middleMouseDown, ViewPortClickDown;
extern bool LCtrlPressed;  // rueda: Ctrl+rueda = zoom cuando el dope sheet esta scrolleando en vertical
extern bool LShiftPressed; // dope sheet: shift+click = agregar a la seleccion
extern bool LAltPressed;   // dope sheet: Alt+A = deseleccionar todo

// tras EDITAR keyframes (borrar/mover): re-evaluar la animacion (pose + skin de las mallas) y redibujar
// Invalida la pose del esqueleto y el skin cacheado de las mallas: hay que llamarla despues de TOCAR una curva,
// o la escena sigue mostrando lo viejo. La usa el timeline y la tarjeta "Keyframe" del panel de propiedades (por
// eso no es static: una sola implementacion, no dos que se desincronicen).
void InvalidarAnimYRedraw(){
    W3dAnimCurvasInvalidar();   // la curva cambio en el MISMO frame: que el 3D la vuelva a aplicar (no solo al mover el cabezal)
    if (ActiveAnimKind == 1 && ActiveAnimArm){ ActiveAnimArm->lastPoseFrame = -999999; ActiveAnimArm->poseDirty = true; }
    struct L { static void rec(Object* o){ if (!o) return;
        if (o->getType()==ObjectType::mesh){ Mesh* m=(Mesh*)o; if (m->skinArmature) m->lastSkinFrame = -999999; }
        for (size_t i=0;i<o->Childrens.size();i++) rec(o->Childrens[i]); } };
    L::rec(SceneCollection);
    g_redraw = true;
}

// roles de los botones de la barra (para el dispatch del click; no chocan con los BR_* del 3D)
enum { TL_ROL_T0 = 300, TL_ROL_START = 320, TL_ROL_END = 321, TL_ROL_ANIM = 322, TL_ROL_SELECT = 323, TL_ROL_PIVOT = 324,
       TL_ROL_VIEW = 325, TL_ROL_MODO = 326, TL_ROL_KEY = 327, TL_ROL_AUTOKEY = 328, TL_ROL_FPS = 329,
       TL_ROL_TIEMPO = 330,
       // la TOOLBAR de abajo: + KfConstant..KfElastic (todas las curvas) | + HFree..HPlano | + KfEaseAuto..InOut
       TL_TB_INTERP0 = 400, TL_TB_HANDLE0 = 420, TL_TB_EASE0 = 440 };

// COLOR DE EJE de una curva: X rojo, Y verde, Z azul (los mismos ejes del viewport 3D).
static const float TL_COL_EJE[3][4] = { {0.90f,0.25f,0.25f,1.0f}, {0.35f,0.85f,0.30f,1.0f}, {0.30f,0.50f,0.95f,1.0f} };
static const float* ColorDeEje(int comp){ return TL_COL_EJE[(comp==AnimX)?0:(comp==AnimY)?1:2]; }
// La fila NORMALES de una vertex anim (capa propia, borrable) es la componente 2 = el azul del eje Z, y como
// normalmente cae encima de la fila Vertices se confundia con una curva de posicion. Va en VIOLETA: es una capa,
// no un eje.
static const float TL_COL_NORMALES[4] = { 0.72f, 0.45f, 0.95f, 1.0f };

// dropdown de animacion: menu JERARQUICO (Scenes + un submenu por armadura) construido por Properties.cpp; la
// seleccion (escena o clip) se aplica con AnimSelPorId (compartida con la tarjeta Animation).
extern void ConstruirMenuAnim(PopupMenu* menu); // Properties.cpp
extern void AnimSelPorId(int id);               // Properties.cpp
static PopupMenu* g_tlMenuAnim = NULL;
// Cierra el menu 'm' SOLO si es el que esta abierto.
// OJO: no se puede comparar MenuAbierto contra un puntero que puede ser NULL. Cuando Click() elige un item
// terminal ya llama a Cerrar(), que deja MenuAbierto en NULL; si el menu contra el que se compara TAMBIEN es NULL
// (porque nunca se abrio), "NULL == NULL" da true y se llamaba Cerrar() sobre NULL -> crash. Pasaba al elegir
// Bezier con la 't': ese submenu se abre solo, sin crear nunca el menu Key contra el que se comparaba.
static void TL_CerrarMenu(PopupMenu* m){ if (m && MenuAbierto == m) m->Cerrar(); }

static void TL_menuAnimAction(int id){
    AnimSelPorId(id);
    TL_CerrarMenu(g_tlMenuAnim);
    g_redraw = true;
}
static PopupMenu* g_tlMenuSel = NULL;    // menu Select del dope sheet (All / None / Invert)
static PopupMenu* g_tlMenuView = NULL;   // menu View del dope sheet (Frame Selected)
static PopupMenu* g_tlMenuPivot = NULL;  // menu Pivot del dope sheet (Center / Current Frame)
static PopupMenu* g_tlMenuKey = NULL;    // menu Key (Transform > Move/Rotate/Scale, Duplicate, Delete)
static PopupMenu* g_tlMenuCtx = NULL;
static PopupMenu* g_tlMenuEase = NULL;
static bool g_tlCabezalHover = false;     // el mouse sobre la linea del frame actual: se pinta BLANCA
static Timeline* g_tlCaja = NULL;         // el timeline que armo la caja de seleccion (si no, es la del 3D)   // Ctrl+E / Key > Easing Type: Set Keyframe Easing Type    // click derecho en el strip: Cortar / Copiar / Pegar / Duplicar / Borrar
static PopupMenu* g_tlMenuKeyXf = NULL;  // submenu Transform del menu Key
static PopupMenu* g_tlMenuKeyIn = NULL;  // submenu Interpolation Mode del menu Key ('t')
static PopupMenu* g_tlMenuHandle = NULL; // menu Handle Type ('v')

// ROJO del AUTO KEY prendido: esta GRABANDO todo lo que toques, tiene que cantar. Mismo criterio que el verde
// del play (tinte de fondo del boton), pero en rojo.
static float TL_ROJO_BTN[4]   = { 0.62f, 0.13f, 0.13f, 1.0f };
// verdes: fondo de botones de play activos / boton del frame actual, y la linea del playhead
static float TL_VERDE_BTN[4]  = { 0.12f, 0.45f, 0.18f, 1.0f };
static float TL_VERDE_LINEA[4]= { 0.20f, 0.80f, 0.32f, 1.0f };

// ------------------------------------------------------------------ helpers de dibujo (coords LOCALES, Y abajo)
static void FillRect(int px, int py, int w, int h){
    if (w <= 0 || h <= 0) return;
    static Rec2D* r = NULL; if (!r) r = new Rec2D();
    r->SetSize((GLshort)px, (GLshort)py, (GLshort)w, (GLshort)h);
    r->RenderObject(false); // usa el Color4f actual
}
static void FillTri(float ax, float ay, float bx, float by, float cx, float cy){
    static float v[6]; v[0]=ax; v[1]=ay; v[2]=bx; v[3]=by; v[4]=cx; v[5]=cy;
    gfx::VertexPointer2f(0, v); gfx::DrawTrianglesArray(3);
}
static void TriRight(float cx, float cy, float r){ FillTri(cx-r, cy-r, cx-r, cy+r, cx+r, cy); }
static void TriLeft (float cx, float cy, float r){ FillTri(cx+r, cy-r, cx+r, cy+r, cx-r, cy); }
static void VBar(float cx, float cy, float halfH, float w){ FillRect((int)(cx-w*0.5f), (int)(cy-halfH), (int)w, (int)(halfH*2)); }
static void Diamond(float cx, float cy, float r){
    static float v[12];
    v[0]=cx; v[1]=cy-r; v[2]=cx+r; v[3]=cy; v[4]=cx; v[5]=cy+r;
    v[6]=cx; v[7]=cy-r; v[8]=cx; v[9]=cy+r; v[10]=cx-r; v[11]=cy;
    gfx::VertexPointer2f(0, v); gfx::DrawTrianglesArray(6);
}
static void SetCol(ColorID::Enum c, float a = 1.0f){
    gfx::Color4f(ListaColores[(int)c][0], ListaColores[(int)c][1], ListaColores[(int)c][2], a);
}
// Trazo de las curvas: se arma TODO en este buffer y se dibuja de una sola vez por curva (batch). Es static para
// no re-alocar en cada frame: despues del primer dibujo ya tiene la capacidad que necesita.
static std::vector<float> g_curvaBuf;
// agrega el segmento a->b al buffer, como un quad orientado (2 triangulos = 6 vertices)
static void QuadLinea(std::vector<float>& b, float ax, float ay, float bx, float by, float w){
    float dx = bx-ax, dy = by-ay, L = sqrtf(dx*dx + dy*dy);
    if (L < 0.0001f) return;
    if (w < 0.5f) w = 0.5f;
    float nx = -dy/L*w, ny = dx/L*w;
    b.push_back(ax+nx); b.push_back(ay+ny);
    b.push_back(bx+nx); b.push_back(by+ny);
    b.push_back(bx-nx); b.push_back(by-ny);
    b.push_back(ax+nx); b.push_back(ay+ny);
    b.push_back(bx-nx); b.push_back(by-ny);
    b.push_back(ax-nx); b.push_back(ay-ny);
}
// segmento a->b dibujado EN EL ACTO (sin batch), con la MISMA geometria que el trazo de las curvas: la matematica
// del quad orientado vive en un solo lado (QuadLinea). No usa GL_LINES: en ES1 el grosor de linea no es confiable
// entre drivers y el brazo del handle quedaba de un pixel o directamente invisible.
static std::vector<float> g_lineaBuf;
static void LineaFina(float ax, float ay, float bx, float by){
    g_lineaBuf.clear();
    QuadLinea(g_lineaBuf, ax, ay, bx, by, (float)GlobalScale * 0.5f);
    if (g_lineaBuf.empty()) return;
    gfx::VertexPointer2f(0, &g_lineaBuf[0]);
    gfx::DrawTrianglesArray((int)(g_lineaBuf.size()/2));
}
// texto centrado horizontalmente en cx, con su top en py (necesita textura del atlas activa)
static void TextCentrado(float cx, int py, const std::string& s){
    int tw = UTF8_Largo(s) * CharacterWidthGS;
    gfx::PushMatrix();
    gfx::Translatef((float)((int)cx - tw/2), (float)py, 0);
    RenderBitmapText(s, textAlign::left, tw + GlobalScale*2);
    gfx::PopMatrix();
}
// glyph vectorial de transporte centrado en un rect (gx,gy,gw,gh)
static void DrawGlyph(int gx, int gy, int gw, int gh, int tipo, bool pausa){
    float cx = gx + gw*0.5f, cy = gy + gh*0.5f, r = gh*0.20f; // r chico -> deja padding en el boton cuadrado
    if (pausa){ VBar(cx - r*0.55f, cy, r, GlobalScale*1.6f); VBar(cx + r*0.55f, cy, r, GlobalScale*1.6f); return; }
    switch (tipo){
        case 0: VBar(cx - r*1.1f, cy, r, GlobalScale*1.6f); TriLeft(cx + r*0.4f, cy, r); break;       // |< inicio
        case 1: TriLeft(cx - r*0.4f, cy, r*0.9f); TriLeft(cx + r*0.9f, cy, r*0.9f); break;            // << kf ant
        case 2: TriLeft(cx + r*0.4f, cy, r*0.9f); VBar(cx - r*0.7f, cy, r*0.5f, GlobalScale); break;  // <| frame ant
        case 3: TriLeft(cx, cy, r); break;                                                            // <  play rev
        case 4: TriRight(cx, cy, r); break;                                                           // >  play
        case 5: TriRight(cx - r*0.4f, cy, r*0.9f); VBar(cx + r*0.7f, cy, r*0.5f, GlobalScale); break; // |> frame sig
        case 6: TriRight(cx + r*0.4f, cy, r*0.9f); TriRight(cx - r*0.9f, cy, r*0.9f); break;          // >> kf sig
        case 7: TriRight(cx - r*0.4f, cy, r); VBar(cx + r*1.1f, cy, r, GlobalScale*1.6f); break;      // >| final
    }
}

// keyframes (frames) de la ANIMACION ACTIVA (la que estamos viendo), NO del objeto seleccionado
static void CollectKeyframes(std::vector<int>& out){
    out.clear();
    if (ActiveAnimKind == 1 && ActiveAnimArm &&
        ActiveAnimArm->animActiva >= 0 && ActiveAnimArm->animActiva < (int)ActiveAnimArm->animations.size()){
        // clip de armature ACTIVO: sus keyframes (de todos los huesos)
        SkeletalAnimation* an = ActiveAnimArm->animations[ActiveAnimArm->animActiva];
        for (size_t t=0;t<an->tracks.size();t++)
            for (size_t p=0;p<an->tracks[t].Propertys.size();p++)
                for (size_t k=0;k<an->tracks[t].Propertys[p].keyframes.size();k++)
                    out.push_back(an->tracks[t].Propertys[p].keyframes[k].frame);
    } else if (ActiveAnimKind == 3 && ActiveAnimMesh){
        // animacion PROPIA del objeto: los keyframes de SUS curvas (kind 3) Y los
        // KEYFRAMES DE VERTICES (la vertex anim ahora vive en cuadros arbitrarios)
        extern VertexAnimation* AnimObjetoActiva(Object*);
        VertexAnimation* an = AnimObjetoActiva((Object*)ActiveAnimMesh);
        if (an){
            for (size_t p=0;p<an->curvas.size();p++)
                for (size_t k=0;k<an->curvas[p].keyframes.size();k++)
                    out.push_back(an->curvas[p].keyframes[k].frame);
            for (size_t k=0;k<an->frames.size();k++)
                out.push_back(an->frames[k]->frame);
        }
    } else {
        // animacion de ESCENA activa: keyframes de TODOS los objetos de la escena (AnimationObjects = escena activa)
        for (size_t i=0;i<AnimationObjects.size();i++)
            for (size_t p=0;p<AnimationObjects[i].Propertys.size();p++)
                for (size_t k=0;k<AnimationObjects[i].Propertys[p].keyframes.size();k++)
                    out.push_back(AnimationObjects[i].Propertys[p].keyframes[k].frame);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
}

// clip de animacion ACTIVO segun la seleccion APP-WIDE (no depende del objeto seleccionado) o NULL si la animacion
// activa es una ESCENA. Asi clickear un armature NO cambia el rango/seleccion del timeline.
static SkeletalAnimation* ClipActivo(){
    if (ActiveAnimKind == 1 && ActiveAnimArm &&
        ActiveAnimArm->animActiva >= 0 && ActiveAnimArm->animActiva < (int)ActiveAnimArm->animations.size())
        return ActiveAnimArm->animations[ActiveAnimArm->animActiva];
    return NULL;
}

// clip del ARMATURE 2D ACTIVO (kind 4) o NULL. Espejo exacto de ClipActivo para el rig 2D.
static Armature2DAnimation* Clip2DActivo(){
    if (ActiveAnimKind != 4 || !ActiveAnimMesh) return NULL;
    return Arm2DClipActivo(ActiveAnimMesh);
}

// ================================================================================================
//  EL ownerKey DEL DOPE DICE DE QUE LISTA ES (fallos [A] y [B] de la ronda 7)
//
//  La seleccion del dope son STRINGS en un set: sobrevive a TODO, incluido cambiar de clip / de
//  escena / de armature 2D. Las claves decian QUE curva ("arm:<rig>/b<n>", "obj:<nombre>") pero NO
//  de QUE lista, y el resolver las ataba a lo que estuviera ACTIVO AL USARLAS (ClipActivo /
//  AnimationObjects / Clip2DActivo). O sea la regla R1 de Undo.h, pero fuera del undo: elegir un
//  keyframe en el clip K0, pasar a K1 y la tarjeta "Keyframe" editaba (valor/frame/interpolacion/
//  handles) la curva de K1, y la 'X' borraba SUS keyframes; encima se veian como seleccionados.
//  Igual con las escenas ("obj:") y con el rig 2D.
//
//  Ahora la IDENTIDAD DE LA LISTA va DENTRO de la clave, como primer segmento (asi la heredan las
//  filas hijas y DopeCubre sigue funcionando: el corte cae siempre en un '/'):
//     escena    "obj:e<ESCENA>/#<SERIAL objeto>"          (+ "/xf")
//     clip 3D   "arm:#<SERIAL rig>/k<CLIP>/b<HUESO>"
//     clip 2D   "arm2d:#<SERIAL malla>/a<ARM2D>/k<CLIP>/b<HUESO>"
//     vertex    "objanim:#<SERIAL malla>" (+ "objvtx:/objuv:/objnrm:" con el mismo id)
//  Una clave de OTRA lista ya no matchea ninguna fila (no se dibuja elegida) y el resolver la
//  rechaza -> no-op seguro, sin depender de que nadie se acuerde de soltar la seleccion.
//  El resolver ADEMAS chequea la IDENTIDAD del dueno (rig / malla / objeto), y las CUATRO ramas
//  la chequean (la de "objanim:" no lo hacia: ver abajo).
//
//  *** EL DUENO VA POR SERIAL, NO POR NOMBRE (ronda 9) ***
//  Hasta la ronda 8 el dueno iba adentro de la clave POR NOMBRE ("arm:RigA/k0/b0") y el
//  comentario que estaba aca decia que un rename posterior "degrada a perder la seleccion". Era
//  FALSO: el nombre es unico EN UN MOMENTO DADO pero se LIBERA al renombrar, y el sistema de
//  nombres unicos se lo da al siguiente que lo pida. Renombrar RigA -> "RigViejo" y despues
//  RigB -> "RigA" deja la clave vieja matcheando al rig AJENO: la tarjeta "Keyframe" edita SU
//  curva y la 'X' borra SUS keyframes. Idem borrar un objeto animado y crear otro que toma el
//  nombre liberado (el mismo escenario que cita DeleteUndo en Undo.cpp). Un NOMBRE RECICLADO es
//  el equivalente exacto del PUNTERO RECICLADO que la ronda 8 arreglo en DeleteUndo:
//  delete + new = misma direccion; rename + rename = mismo nombre.
//  Ahora el dueno va por Object::serial (Core, objects/Objects.h): contador monotono asignado al
//  nacer, que NO se reusa ni al renombrar ni al borrar. La clave queda inerte para siempre en
//  cuanto su dueno se muere, que es exactamente lo que se quiere de un estado efimero.
//  NO hace falta migrar nada: la seleccion del dope NO se serializa (es estado de sesion), asi
//  que el serial nunca sale del proceso. El criterio de "que va por serial y que sigue por
//  nombre" esta escrito en Timeline.h, arriba de DopeIdDueno.
//
//  *** EL DUENO VA PRIMERO Y EL NUMERO DESPUES (ronda 8) ***
//  El clip 3D iba al reves ("arm:k<CLIP>/<rig>/..."), o sea el NUMERO antes del DUENO, y eso
//  rompia la otra mitad: los avisos de remapeo (DopeRemapIndiceClave) piden que el numero venga
//  PEGADO al prefijo, asi que el prefijo de "borre/movi el clip i" tenia que ser "arm:k" pelado
//  -sin el rig- y corria los indices de clip de las claves de TODOS los armatures. No era
//  inofensivo: mientras el rig activo es otro la clave corrida no resuelve, pero el usuario
//  vuelve a SU rig y ahi la clave YA CORRIDA resuelve al CLIP EQUIVOCADO de su propio rig (ver
//  el test 'dopeclip3d'). Con el dueno adelante el aviso se acota a un rig, igual que el 2D
//  ("arm2d:<malla>/a<n>/k") y que los huesos.
//  REGLA para el proximo prefijo: <lista dueno>/<numero>. Si el numero queda antes del dueno,
//  el aviso no se puede acotar y el remapeo pisa listas ajenas.
//  Las filas de VERTEX ANIM ("objanim:/objvtx:/objuv:/objnrm:") SI llevan la identidad de la
//  MALLA (por serial, igual que las otras), pero NO la de la vertex anim concreta: dos anims de
//  LA MISMA malla comparten clave. Ese agujero lo cierra DopeSoltarVertexAnim, que suelta la
//  seleccion en todo camino que cambie la anim activa.
// ================================================================================================
// EL DUENO adentro de una clave: "#<serial>" (ver el bloque de arriba y Timeline.h). Sale de una
// sola funcion porque lo usan el que ARMA las claves, el que las RESUELVE y los que avisan un
// remapeo desde otros archivos (BoneEdit.cpp, UVEditor.cpp, Undo.cpp): dos formatos = el bug.
std::string DopeIdDueno(const Object* o){
    if (!o) return std::string();
    char b[24]; snprintf(b, sizeof b, "#%u", o->serial);
    return std::string(b);
}
// Los TRES prefijos salen de ACA, una sola vez: el que ARMA las claves de las filas y el que las
// RESUELVE usan la misma funcion, asi no puede haber dos formatos.
// "obj:e<ESCENA>/" = las curvas de objeto de la escena activa
static std::string DopePrefEscena(){
    InitSceneAnimations();
    char b[32]; snprintf(b, sizeof b, "obj:e%d/", SceneAnimActiva);
    return std::string(b);
}
// "arm:#<serial rig>/k<CLIP>" = vacio si no hay clip 3D activo. DUENO PRIMERO, NUMERO DESPUES (ver
// el bloque de arriba): asi el aviso de remapeo de clips se acota a UN rig ("arm:" + id + "/k").
static std::string DopePrefArm(){
    if (!ActiveAnimArm) return std::string();
    char b[32]; snprintf(b, sizeof b, "/k%d", ActiveAnimArm->animActiva);
    return "arm:" + DopeIdDueno(ActiveAnimArm) + b;
}
// "arm2d:#<serial malla>/a<ARM2D>/k<CLIP>" = vacio si no hay clip 2D activo
static std::string DopePrefArm2D(){
    if (!ActiveAnimMesh) return std::string();
    char b[48]; snprintf(b, sizeof b, "/a%d/k%d", ActiveAnimMesh->armature2dActivo, ActiveAnimMesh->Arm2DAnimActiva());
    return "arm2d:" + DopeIdDueno(ActiveAnimMesh) + b;
}
// la clave arranca con 'pref' Y sigue con un numero? (sin esto, atoi de "" da 0 y una clave
// truncada editaria el hueso/objeto 0)
static bool DopeTrasPrefijo(const std::string& k, const std::string& pref){
    if (pref.empty() || k.size() <= pref.size()) return false;
    if (k.compare(0, pref.size(), pref) != 0) return false;
    const char c = k[pref.size()];
    return c >= '0' && c <= '9';
}

// ================================================================================================
//  DOPE SHEET — panel izquierdo con lo que tiene animacion en la animacion ACTIVA, SOLO de lo SELECCIONADO.
//  Filas tipo Outliner (flecha + icono + nombre, indentadas y desplegables).
// ================================================================================================
// plegado: claves de las filas COLAPSADAS. Vacio = TODO DESPLEGADO (el arranque pedido).
static std::set<std::string> g_dopeColapsado;
static bool DopeColapsado(const std::string& k){ return !k.empty() && g_dopeColapsado.count(k) > 0; }
// OJO apagado, por fila (mismo criterio que el plegado: se recuerda por claveFila). Ocultar una curva la saca del
// dibujo y de la edicion; con 18 canales encimados es la unica forma de trabajar tranquilo sobre uno.
// AUTO FRAME (View > Auto frame, PRENDIDO por defecto): al clickear una fila del panel se elige toda su curva y
// se la ENCUADRA sola, sin tener que apretar numpad '.'. Es lo que se quiere el 99% de las veces: si vas a mirar
// una curva, la queres ver.
// Panel del dope sheet a la vista, si/no. En el telefono se come media pantalla y muchas veces solo queres el
// tiempo. Es lo MISMO que plegar el summary a mano (panelW=0), pero desde el menu y sin tener que buscar la flecha.
static bool g_dopePanelOn = true;
static bool g_dopeAutoFrame = true;
// las TIRAS de sonido: su forma de onda y la linea de su volumen (sin las dos es solo la tira, de una fila)
static bool g_tlVerOnda = true, g_tlVerVolumen = true;
static const int kTiraFilasExtra = 2;   // con alguna de las dos, la tira ocupa su fila y dos mas
// TOGGLE "Solo vertices seleccionados" (menu Vista, dope Y curvas; static = persiste la
// sesion). Con la malla en EDIT MODE:
//  - VISTA: las filas de vertex anim (Vertices/UV/Normales) muestran SOLO los keyframes
//    que le cambian algo a los vertices seleccionados (los demas cuadros no les mueven
//    ni un vertice: son ruido para lo que estas editando).
//  - EDICION: MOVER un keyframe de esas filas se resuelve por MASCARA de vertices
//    (VertexAnimMoverKeysSelectivo): sobre otro keyframe = merge selectivo (solo los
//    seleccionados se pisan), sobre un frame vacio = keyframe completo con los no
//    seleccionados EVALUADOS (no saltan). Asi se anima o DES-anima un subconjunto.
static bool g_dopeSoloSelVerts = false;
static float g_scrubAcum = 0.0f;  // resto en frames del scrub por flecha (ver ScrubFlecha)
static int g_scrubEspera = 0;      // ticks que el scrub espera despues del toque (ver W3dTimelineFlechaPulsada)
static std::set<std::string> g_dopeOjoOff;
static bool DopeOjoOff(const std::string& k){ return !k.empty() && g_dopeOjoOff.count(k) > 0; }

// ---- SELECCION ----
// Filas (click en el panel) y KEYFRAMES (click en el strip). La unidad seleccionable es UN keyframe de UNA CURVA:
// (dueño, propiedad, componente, frame). X, Y y Z son curvas independientes -> "X Location @4" es un keyframe
// DISTINTO de "Y Location @4": se selecciona, mueve y borra por separado.
static std::set<std::string> g_dopeRowSel;   // claveFila
static std::set<std::string> g_dopeKeySel;   // DopeKeyId(ownerKey, propId, compId, frame)
// EL AREA PARA AGARRAR un keyframe / un handle (px): generosa con el mouse, mas con el dedo
static float RadioAgarre() {
#ifndef W3D_SYMBIAN
    extern Uint32 g_lastFingerTicks;
    if (g_lastFingerTicks != 0) return (float)GlobalScale * 20.0f;   // el dedo tapa lo que agarra: bien grande
#endif
    return (float)GlobalScale * 8.0f;
}
// lo FINO (la linea del frame actual, el borde del panel): 3 px con el mouse, mucho mas con el dedo
static float RadioLinea() {
#ifndef W3D_SYMBIAN
    extern Uint32 g_lastFingerTicks;
    if (g_lastFingerTicks != 0) return (float)GlobalScale * 12.0f;
#endif
    return (float)GlobalScale * 3.0f;
}
// keyframe ACTIVO = el ultimo que se clickeo. Es el que edita la tarjeta "Keyframe" del panel de propiedades.
// Se guarda por IDENTIDAD (curva + frame) y no por puntero: los keyframes viven en un vector que se reordena al
// moverlos, asi que un puntero quedaria colgado en cuanto tocas algo.
static std::string g_dopeActOwner; static int g_dopeActProp = -1, g_dopeActComp = -1, g_dopeActFrame = 0;
static bool g_dopeActHay = false;
static std::string DopeKeyId(const std::string& ownerKey, int propId, int compId, int frame){
    char b[48]; snprintf(b,sizeof b,"|p%d|c%d|f%d",propId,compId,frame); return ownerKey + b;
}

// ============================================================================
//  LA SELECCION DEL DOPE TAMBIEN GUARDA INDICES (la misma clase de bug que el undo)
//
//  El ownerKey de una fila de hueso lleva el INDICE del hueso adentro: "arm:<rig>/b<IDX>" (3D) y
//  "arm2d:<malla>/a<IDX_ARM>/b<IDX>" (2D). Esas claves sobreviven a todo (son strings en un set,
//  no punteros), asi que cuando algo CORRE esos indices -BoneEditBorrar / Bone2DBorrar borran un
//  hueso y compactan; UndoArm2DBorrar saca un armature 2D entero- la clave vieja "b<n>" pasa a
//  matchear la fila NUEVA de OTRO hueso. Nadie limpiaba ni remapeaba nada, y el resultado era
//  mudo: la tarjeta "Keyframe" editaba (frame/valor/interpolacion/handles) la curva del hueso
//  EQUIVOCADO y la 'X' del dope borraba los keyframes del hueso equivocado.
//  Se remapea con la MISMA convencion del undo (ver RemapIndiceGuardado en main/undo/Undo.cpp):
//  b >= 0 = intercambio a<->b; b < 0 = se BORRO el indice 'a' (las claves de ESE hueso se van
//  de la seleccion; las de arriba bajan uno).
// ============================================================================
// el ownerKey de una clave: DopeKeyId le agrega "|pN|cM|fF" y claveFila de un canal "|pN|cM", asi
// que el ownerKey es todo lo que hay antes del ULTIMO "|p" (un nombre con '|' adentro no rompe).
static std::string DopeClaveOwner(const std::string& k){
    const size_t q = k.rfind("|p");
    return (q == std::string::npos) ? k : k.substr(0, q);
}
// "<prefijo><numero><resto>" -> numero + resto. false si la clave no es de ese prefijo.
static bool DopeClavePartir(const std::string& owner, const std::string& prefijo, int& num, std::string& resto){
    if (owner.size() <= prefijo.size() || owner.compare(0, prefijo.size(), prefijo) != 0) return false;
    size_t j = prefijo.size();
    while (j < owner.size() && owner[j] >= '0' && owner[j] <= '9') j++;
    if (j == prefijo.size()) return false;     // no hay numero pegado al prefijo
    num = atoi(owner.c_str() + prefijo.size());
    resto = owner.substr(j);
    return true;
}
// devuelve false si el indice guardado era JUSTO el borrado (misma semantica que el undo)
static bool DopeCorrerIndice(int& idx, int a, int b){
    if (b >= 0){ if (idx == a) idx = b; else if (idx == b) idx = a; return true; }
    if (idx == a) return false;
    if (idx >  a) idx--;
    return true;
}
static void DopeRemapSet(std::set<std::string>& S, const std::string& prefijo, int a, int b, bool insertar){
    std::set<std::string> nueva;
    for (std::set<std::string>::const_iterator it = S.begin(); it != S.end(); ++it){
        const std::string owner = DopeClaveOwner(*it);
        int num; std::string resto;
        if (!DopeClavePartir(owner, prefijo, num, resto)){ nueva.insert(*it); continue; } // no es de esta lista
        if (insertar){ if (num >= a) num++; }
        else if (!DopeCorrerIndice(num, a, b)) continue;                                  // era ESE: la clave se va
        char nb[24]; snprintf(nb, sizeof nb, "%d", num);
        nueva.insert(prefijo + nb + resto + it->substr(owner.size()));
    }
    S.swap(nueva);
}
static void DopeRemapTodo(const std::string& prefijo, int a, int b, bool insertar){
    if (prefijo.empty()) return;
    DopeRemapSet(g_dopeKeySel,    prefijo, a, b, insertar);   // keyframes elegidos
    DopeRemapSet(g_dopeRowSel,    prefijo, a, b, insertar);   // filas elegidas
    DopeRemapSet(g_dopeColapsado, prefijo, a, b, insertar);   // estado de plegado (misma clave)
    DopeRemapSet(g_dopeOjoOff,    prefijo, a, b, insertar);   // filas con el ojo apagado
    if (g_dopeActHay){                                        // el keyframe ACTIVO (el de la tarjeta)
        int num; std::string resto;
        if (DopeClavePartir(g_dopeActOwner, prefijo, num, resto)){
            if (insertar){ if (num >= a) num++; }
            else if (!DopeCorrerIndice(num, a, b)){ g_dopeActHay = false; g_redraw = true; return; }
            char nb[24]; snprintf(nb, sizeof nb, "%d", num); g_dopeActOwner = prefijo + nb + resto;
        }
    }
    g_redraw = true;
}
void DopeRemapIndiceClave(const std::string& prefijo, int a, int b){ DopeRemapTodo(prefijo, a, b, false); }
void DopeInsertarIndiceClave(const std::string& prefijo, int a){    DopeRemapTodo(prefijo, a, -1, true); }
// El harness de tests elige/lee el keyframe ACTIVO sin simular el click (mismo idiom que
// _BorrarVertexAnimDeFwd de Properties.cpp: se aprieta el MISMO boton que el usuario, pero la
// seleccion del dope es estado interno del archivo). Es exactamente lo que deja apuntando la
// tarjeta "Keyframe" del panel de propiedades.
void _DopeElegirKeyFwd(const std::string& ownerKey, int propId, int compId, int frame){
    g_dopeKeySel.insert(DopeKeyId(ownerKey, propId, compId, frame));
    g_dopeActOwner = ownerKey; g_dopeActProp = propId; g_dopeActComp = compId;
    g_dopeActFrame = frame; g_dopeActHay = true;
}
std::string _DopeOwnerActivoFwd(){ return g_dopeActHay ? g_dopeActOwner : std::string("(ninguno)"); }
bool _DopeKeySelHayFwd(const std::string& ownerKey, int propId, int compId, int frame){
    return g_dopeKeySel.count(DopeKeyId(ownerKey, propId, compId, frame)) > 0;
}
int  _DopeKeySelCantFwd(){ return (int)g_dopeKeySel.size(); }
// LA OTRA MITAD: las filas de una vertex anim NO dicen de QUE vertex anim son (se resuelve por la
// ACTIVA al usarlas) y su seleccion sobrevive a todo. Cambiar de vertex anim con keyframes
// elegidos aplicaba esa seleccion -mover / borrar / interpolacion, y la tarjeta Keyframe- a la
// OTRA anim. No hay indice que correr (son otra lista entera): se SUELTAN.
// SON CUATRO FILAS, no tres:
//   "objvtx:<malla>" / "objuv:<malla>" / "objnrm:<malla>" = las capas de POSES (DopeVertexAnimDe),
//   "objanim:<malla>"                                     = las curvas de TRANSFORM propias de la
//        anim (VertexAnimation::curvas), que resuelve DopeResolverProp por AnimObjetoActiva. Es
//        la fila PADRE de las otras tres y tiene canales propios (X/Y/Z Location...): quedaba
//        afuera de este soltado, asi que tras cambiar de anim la tarjeta "Keyframe" editaba -y la
//        'X' borraba- la curva homonima de una anim que el usuario nunca eligio. Mismo bug, misma
//        cura (ver el test 'dopeobjanim').
void DopeSoltarVertexAnim(){
    static const char* pref[4] = { "objvtx:", "objuv:", "objnrm:", "objanim:" };
    for (int p = 0; p < 4; p++){
        const size_t n = std::string(pref[p]).size();
        for (std::set<std::string>::iterator it = g_dopeKeySel.begin(); it != g_dopeKeySel.end(); ){
            if (it->compare(0, n, pref[p]) == 0) g_dopeKeySel.erase(it++); else ++it;
        }
        for (std::set<std::string>::iterator it = g_dopeRowSel.begin(); it != g_dopeRowSel.end(); ){
            if (it->compare(0, n, pref[p]) == 0) g_dopeRowSel.erase(it++); else ++it;
        }
        if (g_dopeActHay && g_dopeActOwner.compare(0, n, pref[p]) == 0) g_dopeActHay = false;
    }
    g_redraw = true;
}
// ---- TRANSFORM de keyframes: MOVER ('g'), ESCALAR ('s') y, SOLO EN CURVAS, ROTAR ('r') ----
// En DOPE SHEET solo existe el eje TIEMPO: mover corre frames y escalar estira la duracion (rotar no significa nada).
// En CURVAS hay dos ejes (tiempo y VALOR), asi que ahi si se rota: el giro se hace en PIXELES alrededor del pivote
// (es lo unico con sentido cuando los dos ejes tienen unidades distintas y zoom independiente) y despues se vuelve
// a datos. Escalar usa un PIVOTE: Center = el centro de los keyframes seleccionados, o el frame ACTUAL.
// Los frames son ENTEROS -> el resultado se REDONDEA. NO se clampea: se puede mover fuera del rango del clip
// e incluso a frames NEGATIVOS.
// (DOPE_MOV/DOPE_ESC/DOPE_ROT y DOPE_EJE_* viven en Timeline.h: son los valores que aceptan DopeMoveStart y
//  DopeCiclarEje, que son publicos. Tenerlos aca adentro obligaba a los llamadores a pasar numeros sueltos.)
enum { DOPE_PIV_CENTER = 0, DOPE_PIV_CURFRAME = 1 };
static int  g_dopePivot = DOPE_PIV_CENTER;
static bool g_dopeMov = false;
static int  g_dopeMovModo = 0;        // DOPE_MOV / DOPE_ESC / DOPE_ROT
static int  g_dopeMovEje = Timeline::DOPE_EJE_LIBRE;
static float g_dopeMovVal = 0.0f;     // mover: frames a correr; escalar: factor; rotar: grados
static float g_dopeMovValV = 0.0f;    // curvas + mover: cuanto se corre en VALOR (el eje vertical)
static int  g_dopeMovX0 = 0, g_dopeMovY0 = 0;
static float g_dopePivotF = 0.0f;     // pivote en FRAMES  (eje tiempo)
static float g_dopePivotV = 0.0f;     // pivote en VALOR   (eje vertical; solo curvas)
static float g_dopeRotAng0 = 0.0f;    // angulo mouse-pivote al arrancar el rotar
// CURSOR VIRTUAL del transform. El cursor REAL se ENVUELVE de un borde al otro del timeline (igual que al panear)
// para poder seguir escalando/rotando sin quedarse sin pantalla. Ese salto haria pegar un tiron si el transform
// mirara la posicion real, asi que se lleva una posicion propia que solo ACUMULA los deltas dx/dy: en el frame del
// envolvimiento CheckWarpMouseInViewport los pone en 0, con lo cual la virtual sigue derecho y no se entera.
static int  g_dopeVirtX = 0, g_dopeVirtY = 0;
static bool g_dopeVirtPrimero = false; // el 1er motion tras arrancar: dx/dy todavia son del evento ANTERIOR (igual
                                       // que g_xformPrimerMov en el 3D) -> se ignoran o el transform pega un salto
// Con UN SOLO keyframe seleccionado en curvas, rotarlo o escalarlo alrededor de si mismo no hace nada (el pivote
// cae encima). Ahi 'r' y 's' pasan a operar sobre sus HANDLES: rotarlos gira la curva, escalarlos alarga la
// distancia. Es lo unico que esas teclas pueden significar con un solo punto elegido.
static bool g_dopeHXform = false;
static std::string g_dopeHOwner; static int g_dopeHProp = -1, g_dopeHComp = -1, g_dopeHFrame = 0;
static keyFrame g_dopeHOrig;          // handles ANTES del transform (absoluto -> sin drift al arrastrar)
// entrada NUMERICA propia del timeline (acepta matematica: "2", "(3+1)/2", ...)
static bool g_dopeNumOn = false; static std::string g_dopeNumBuf; static bool g_dopeNumNeg = false;
extern bool W3dEvalExpr(const std::string& s, float& out); // LayoutInput.cpp (mismo parser que la barra 3D)
struct DopeMovProp { std::string ownerKey; int propId, compId; std::vector<keyFrame> orig; std::vector<int> selFrames;
                     std::vector<char> quieto; };   // quieto[k]: orig[k] es un PISADO (ver g_dopePisados): nunca se mueve
static std::vector<DopeMovProp> g_dopeMovSnap;
// LO QUE DUPLICAR Y PEGAR TAPAN AL NACER (pedido del duenio, 2026-11-09). La copia nace en un cuadro donde puede haber
// otro keyframe y enseguida se agarra para moverla. Antes ese keyframe se BORRABA en el acto: al correr la copia ya no
// volvia ("muevo lo duplicado por arriba del keyframe 2 y se borra"). Ahora se saca pero se anota aca, y al arrancar el
// movimiento vuelve a la foto como un keyframe QUIETO: mientras la copia esta encima no se ve, si la copia se va vuelve,
// y solo queda pisado si la copia se SUELTA encima (lo mismo que pasa al mover cualquier keyframe).
static std::vector<DopeMovProp> g_dopePisados;
static void DopePisadoAnotar(const std::string& owner, int propId, int compId, const keyFrame& kf){
    for (size_t i=0;i<g_dopePisados.size();i++){ DopeMovProp& p = g_dopePisados[i];
        if (p.ownerKey==owner && p.propId==propId && p.compId==compId){ p.orig.push_back(kf); return; } }
    DopeMovProp p; p.ownerKey=owner; p.propId=propId; p.compId=compId; p.orig.push_back(kf); g_dopePisados.push_back(p);
}
// el keyframe k de la foto, se mueve? (los elegidos; los pisados nunca: comparten el cuadro con la copia)
static bool DopeMovSel(const DopeMovProp& mp, size_t k){
    if (k < mp.quieto.size() && mp.quieto[k]) return false;
    for (size_t i=0;i<mp.selFrames.size();i++) if (mp.selFrames[i]==mp.orig[k].frame) return true;
    return false;
}
// snapshot paralelo para MOVER las filas VERTICES/UV (los frames de la vertex anim NO
// son AnimProperty). UN snapshot por ANIM: las dos filas comparten los VertexFrame (un
// frame con ambas capas es UN solo cuadro), asi que se guardan punteros ESTABLES a los
// VertexFrame + su frame original + en que fila(s) estaba seleccionado cada uno. Durante
// el arrastre solo se reasigna ->frame (nada se borra), asi los punteros siguen validos;
// las colisiones se resuelven al confirmar.
struct DopeMovVtxSnap {
    VertexAnimation* an;
    std::string ownerVtx, ownerUV, ownerNrm; // clave de cada fila presente (vacia = sin esa fila)
    std::vector<VertexFrame*> vf; std::vector<int> origFrame;
    std::vector<char> selPos, selUV, selNrm; // elegido en la fila Vertices / UV / Normales
    // MOVE SELECTIVO (toggle "Solo vertices seleccionados" + malla en Edit Mode con verts elegidos): el arrastre
    // se PREVISUALIZA igual (los frames corren), pero al CONFIRMAR no se resuelve por "el movido pisa al quieto"
    // sino por VertexAnimMoverKeysSelectivo con esta mascara (merge por vertice). Vacia = move de siempre.
    std::vector<unsigned char> mask;
    bool selectivo;
    DopeMovVtxSnap() : an(NULL), selectivo(false) {}
};
static std::vector<DopeMovVtxSnap> g_dopeMovVtx;
// Shift+D: el transform en curso viene de un DUPLICAR. Esc no solo cancela el movimiento: tambien tiene que
// deshacer la duplicacion (sino te quedan las copias tiradas donde nacieron), asi que se guarda el estado ANTERIOR.
static bool g_dopeDup = false;
static std::vector<DopeMovProp> g_dopeDupPre;
// pre-estado de la fila VERTEX para el cancel de un Shift+D: el blob de la anim ANTES de
// clonar (reusa la serializacion, como el undo). Esc restaura ese blob (borra las copias
// y devuelve lo que un clon hubiera pisado al nacer en f+off).
struct DopeDupVtxPre { std::string ownerKey; VertexAnimation* an; std::vector<unsigned char> blob; };
static std::vector<DopeDupVtxPre> g_dopeDupVtxPre;

// El ownerKey de una fila PADRE cubre al de un canal? Los ownerKey son "obj:<nombre>" / "arm:<nombre>" /
// "arm:<nombre>/b<idx>", y un padre cubre al hijo si son EL MISMO o si el hijo cuelga de el con un '/' de por medio.
// NO alcanza con comparar el prefijo crudo: "arm:rig/b3" es prefijo de "arm:rig/b30", asi que el hueso 3 se
// llevaba puestos a los huesos 30..39 (al seleccionar, al marcar y -lo grave- al BORRAR: perdia animacion ajena).
// Idem "obj:Cube" contra "obj:Cube.001". (Los ejemplos con nombre son de cuando la clave llevaba el nombre del
// dueno; hoy lleva "#<serial>" -ver el bloque de identidad arriba- pero la regla es la misma y vale para
// cualquier segmento: "#3" es prefijo de "#31".)
static bool DopeCubre(const std::string& padre, const std::string& hijo){
    if (padre.empty()) return true;                                  // Summary: cubre todo
    if (hijo.size() < padre.size()) return false;
    if (hijo.compare(0, padre.size(), padre) != 0) return false;
    return hijo.size() == padre.size() || hijo[padre.size()] == '/'; // el corte tiene que caer en un separador
}
// lo expone el test 'dopecubre' (la regla es chica pero su bug era perdida de datos: conviene tenerla fijada)
bool W3dScriptDopeCubre(const std::string& padre, const std::string& hijo){ return DopeCubre(padre, hijo); }

// LA CLAVE DICE LA MALLA DE LA ANIM ACTIVA? Las CUATRO filas de vertex anim
// ("objanim:/objvtx:/objuv:/objnrm:#<serial>") se resuelven por la ACTIVA al usarlas, asi que
// sin este chequeo una clave de OTRA malla termina editando/borrando las curvas de la malla
// activa. Sus tres hermanas ("obj:", "arm:", "arm2d:") ya validaban el dueno; la de "objanim:"
// era la unica que ni lo miraba (fallo 2 de la ronda 9: DeleteUndo::Aplicar restaura
// ActiveAnimMesh a la malla que vuelve y la clave elegida era de OTRA malla).
static bool DopeDuenoEsAnimMesh(const std::string& ownerKey, size_t nPref){
    if (!ActiveAnimMesh || ownerKey.size() < nPref) return false;
    return ownerKey.substr(nPref) == DopeIdDueno((const Object*)ActiveAnimMesh);
}
// la vertex anim ACTIVA si el ownerKey es de una fila de vertex anim de la malla activa
// ("objvtx:#<serial>" = fila Vertices/capa posiciones, "objuv:#<serial>" = fila UV/capa uv,
// "objnrm:#<serial>" = fila Normales/capa normales) o NULL. Los frames de vertices NO
// son AnimProperty: las operaciones del dope (mover/borrar/interpolar) los tocan por
// aca, no por DopeResolverProp.
static VertexAnimation* DopeVertexAnimDe(const std::string& ownerKey){
    size_t nPref = 0;
    if      (ownerKey.compare(0,7,"objvtx:")==0) nPref = 7;
    else if (ownerKey.compare(0,6,"objuv:") ==0) nPref = 6;
    else if (ownerKey.compare(0,7,"objnrm:")==0) nPref = 7;
    else return NULL;
    if (!DopeDuenoEsAnimMesh(ownerKey, nPref)) return NULL;
    extern VertexAnimation* AnimObjetoActiva(Object*);
    return AnimObjetoActiva((Object*)ActiveAnimMesh);
}
// que CAPA representa la fila: 0 = Vertices (posiciones), 1 = UV, 2 = Normales,
// -1 = no es fila de vertex anim
static int DopeVertexCapaDe(const std::string& ownerKey){
    if (ownerKey.compare(0,7,"objvtx:")==0) return 0;
    if (ownerKey.compare(0,6,"objuv:")==0)  return 1;
    if (ownerKey.compare(0,7,"objnrm:")==0) return 2;
    return -1;
}
// este VertexFrame trae la capa de esa fila? (el flag por-frame es el puntero)
static bool VertexFrameEnCapa(const VertexFrame* vf, int capa){
    if (!vf) return false;
    return capa==0 ? vf->positions!=NULL : capa==1 ? vf->uvs!=NULL : vf->normals!=NULL;
}
// cuantas capas trae el frame (para saber si borrarle una lo deja vacio)
static int VertexFrameCapas(const VertexFrame* vf){
    if (!vf) return 0;
    return (vf->positions?1:0) + (vf->uvs?1:0) + (vf->normals?1:0);
}
// MASCARA de vertices seleccionados de 'm' en EDIT MODE, sobre los verts GPU (paralela a
// mesh->vertex, que es como estan armados los frames de la vertex anim). La seleccion
// vive en el EditMesh por vertice EDITABLE (posiciones unicas); aca se expande a todas
// las copias GPU via posRep, igual que EmpujarPosiciones. false si la malla no esta en
// edicion o no hay ningun vertice seleccionado (= no hay mascara que aplicar).
static bool DopeMascaraVertsSel(Mesh* m, std::vector<unsigned char>& mask){
    if (!m || InteractionMode != EditMode || g_editMesh != (Object*)m || !m->edit) return false;
    EditMesh* e = m->edit;
    const int nV = m->vertexSize;
    if (nV <= 0 || e->editVerts.empty()) return false;
    std::vector<int> repToEdit(nV, -1);   // rep GPU -> vert editable k
    for (size_t k=0;k<e->editVerts.size();k++)
        if (e->editVerts[k]>=0 && e->editVerts[k]<nV) repToEdit[e->editVerts[k]] = (int)k;
    const bool hayRep = ((int)m->posRep.size() == nV);
    mask.assign(nV, 0);
    bool alguno = false;
    for (int gv=0; gv<nV; gv++){
        int rep = hayRep ? m->posRep[gv] : gv;
        int k = (rep>=0 && rep<nV) ? repToEdit[rep] : -1;
        if (k>=0 && k<(int)e->vertSel.size() && e->vertSel[k]){ mask[gv]=1; alguno=true; }
    }
    return alguno;
}
// El filtro de VISTA del toggle: este cuadro le ANIMA algo a los vertices de la mascara? La regla vive en el
// Core (VertexAnimKeyAportaSel) y es la MISMA con la que el move selectivo decide si el frame origen sobrevive:
// un keyframe cuyos verts seleccionados caen justo sobre la interpolacion de los vecinos no les anima nada. Que
// sean la misma regla no es un detalle: despues de mover un key con el toggle prendido, el cuadro de origen
// desaparece de la fila filtrada EXACTAMENTE cuando dejo de animar a esos vertices (se ve como que "se movio").
static bool VertexFrameAnimaSel(const VertexAnimation& an, const VertexFrame* vf, int capa,
                                const std::vector<unsigned char>& mask){
    if (!vf || mask.empty()) return true;
    return VertexAnimKeyAportaSel(an, capa, vf->frame, &mask[0]);
}
// Un vertex animation es una secuencia de POSES: no hay un "valor" escalar como en una curva
// de transform. Para poder VERLO y editarlo en el modo curvas, se arma una curva temporal
// donde el valor de cada keyframe = su INDICE (0,1,2...): queda una rampa creciente cuyos
// TRAMOS muestran la interpolacion (recta/escalon/bezier) entre pose y pose. Es SOLO para
// dibujar y hacer hit-test: mover/borrar/interpolar siguen yendo a los VertexFrame (mismo
// ownerKey "objvtx:"/"objuv:"). Devuelve la anim (o NULL) y llena 'out' con la curva temporal.
// CAPAS: cada fila (Vertices / UV) arma SU curva solo con los frames que traen su capa.
// SIN OVERSHOOT + curva = verdad: los handles de la curva temporal NO son los crudos del
// VertexFrame sino los EFECTIVOS + CLAMPEADOS que devuelve VertexAnimHandlesTramo — el
// MISMO evaluador que usa la reproduccion (EvalCurvaVertexAnim). Van como HFree (ya
// resueltos): el trazo que dibuja CurvaTrazo y el handle que dibuja/agarra HandlePos son
// EXACTAMENTE la funcion que corre al reproducir. Imposible que diverjan.
static VertexAnimation* VertexCanalTemp(const std::string& ownerKey, AnimProperty& out){
    VertexAnimation* van = DopeVertexAnimDe(ownerKey);
    if (!van) return NULL;
    const int capa = DopeVertexCapaDe(ownerKey);
    out.Property = AnimVertex; out.component = capa; out.keyframes.clear();
    const VertexFrame* prev = NULL;
    for (size_t k=0;k<van->frames.size();k++){
        const VertexFrame* vf = van->frames[k];
        if (!VertexFrameEnCapa(vf, capa)) continue;    // cada curva muestra SUS keyframes
        keyFrame kf;
        kf.frame = vf->frame;
        kf.value = (float)out.keyframes.size();  // valor = indice EN LA CAPA: rampa 0,1,2...
        kf.Interpolation = vf->interp;
        kf.handleType = HFree;                   // resueltos aca: nadie los recalcula distinto
        kf.inDF = 0.0f; kf.inDV = 0.0f; kf.outDF = 0.0f; kf.outDV = 0.0f;
        if (prev){
            float oDF, oDV, iDF, iDV;
            VertexAnimHandlesTramo(*prev, *vf, oDF, oDV, iDF, iDV);
            keyFrame& ka = out.keyframes[out.keyframes.size()-1];
            ka.outDF = oDF; ka.outDV = oDV;      // el tramo va de 'prev' (valor k) a este (k+1):
            kf.inDF  = iDF; kf.inDV  = iDV;      // mismo delta de valor (1) que el 0..1 del ease
        }
        out.keyframes.push_back(kf);
        prev = vf;
    }
    return van;
}
// resuelve ownerKey ("obj:<nombre>" / "arm:<nombre>/b<idx>") + (propId, compId) -> la CURVA viva.
// 'editar' = la curva que sale se va a ESCRIBIR (mover/borrar/escalar/interpolar/handles/tarjeta
// Keyframe/cancelar un transform). Solo importa en los clips de esqueleto: si son de un animset
// COMPARTIDO, el armature pasa antes a clips propios (copy-on-write, animation/W3dAnimSet.h). El
// dibujo, el hit-test, el encuadre y la tarjeta Keyframe cuando solo MUESTRA piden editar=false:
// mirar un clip compartido no lo copia (en el editor eso era memoria de mas por cada rig mirado).
static AnimProperty* DopeResolverProp(const std::string& ownerKey, int propId, int compId, bool editar = true){
    if (ownerKey.compare(0,8,"camact:e")==0){          // pista "Camara activa" de la escena (solo la ACTIVA)
        if (propId != AnimCamActiva || compId != AnimX) return NULL;
        if (atoi(ownerKey.c_str()+8) != SceneAnimActiva) return NULL;
        return W3dCamPista(SceneAnimActiva, false);
    }
    if (ownerKey.compare(0,8,"objanim:")==0){          // animacion PROPIA del objeto (kind 3)
        extern VertexAnimation* AnimObjetoActiva(Object*);
        // la clave tiene que ser DE LA MALLA ACTIVA (ver DopeDuenoEsAnimMesh): esta rama era la
        // unica que resolvia derecho por AnimObjetoActiva(ActiveAnimMesh) sin mirar el dueno.
        if (DopeDuenoEsAnimMesh(ownerKey, 8)){
            VertexAnimation* an = AnimObjetoActiva((Object*)ActiveAnimMesh);
            if (an) for (size_t p=0;p<an->curvas.size();p++)
                if (an->curvas[p].Property==propId && an->curvas[p].component==compId)
                    return &an->curvas[p];
        }
        return NULL;
    }
    if (ownerKey.compare(0,4,"obj:")==0){
        // "obj:e<ESCENA>/#<serial objeto>": si la clave es de OTRA escena, no resuelve (ver el
        // bloque de identidad arriba). AnimationObjects ES la lista de la escena activa.
        // El objeto va por SERIAL: por nombre, borrar el objeto animado (o renombrarlo) y que
        // otro tome el nombre libre hacia que la clave vieja cayera sobre el objeto NUEVO.
        const std::string pref = DopePrefEscena();
        if (ownerKey.size() <= pref.size() || ownerKey.compare(0, pref.size(), pref) != 0) return NULL;
        std::string n = ownerKey.substr(pref.size());
        for (size_t i=0;i<AnimationObjects.size();i++){
            if (!AnimationObjects[i].obj || DopeIdDueno(AnimationObjects[i].obj) != n) continue;
            for (size_t p=0;p<AnimationObjects[i].Propertys.size();p++)
                if (AnimationObjects[i].Propertys[p].Property==propId && AnimationObjects[i].Propertys[p].component==compId)
                    return &AnimationObjects[i].Propertys[p];
        }
    } else if (ownerKey.compare(0,6,"arm2d:")==0){
        // OJO: esta rama va ANTES que la de "arm:" - no por estilo: "arm2d:rig/a0/b3" NO empieza con
        // "arm:" (la 'm' choca con la ':'), pero dejarlo al azar del orden de los ifs es pedir el bug.
        // FORMA de la clave: "arm2d:<malla>/a<armature>/k<clip>/b<hueso>". La malla, el rig 2D y el
        // clip van ADENTRO de la clave: una clave de otro clip (o de otro rig, o de otra malla) NO
        // matchea el prefijo y devuelve NULL, en vez de editar la curva homonima del clip activo.
        Armature2DAnimation* clip = Clip2DActivo(); if (!clip) return NULL;
        const std::string pref = DopePrefArm2D() + "/b";
        if (!DopeTrasPrefijo(ownerKey, pref)) return NULL;
        int b = atoi(ownerKey.c_str()+pref.size());
        for (size_t t=0;t<clip->tracks.size();t++){
            if (clip->tracks[t].bone != b) continue;
            for (size_t p=0;p<clip->tracks[t].Propertys.size();p++)
                if (clip->tracks[t].Propertys[p].Property==propId && clip->tracks[t].Propertys[p].component==compId)
                    return &clip->tracks[t].Propertys[p];
        }
        return NULL;
    } else if (ownerKey.compare(0,4,"arm:")==0){
        // "arm:<rig>/k<CLIP>/b<HUESO>": el rig y el clip van adentro de la clave (idem 2D).
        // Si la curva que sale de aca se va a EDITAR y el clip es de un animset compartido, el
        // armature pasa antes a clips propios (ver 'editar' arriba).
        if (editar && ClipActivo()) W3dArmatureAnimsPropias(ActiveAnimArm);
        SkeletalAnimation* clip = ClipActivo(); if (!clip) return NULL;
        const std::string pref = DopePrefArm() + "/b";
        if (!DopeTrasPrefijo(ownerKey, pref)) return NULL;
        int b = atoi(ownerKey.c_str()+pref.size());
        for (size_t t=0;t<clip->tracks.size();t++){
            if (clip->tracks[t].bone != b) continue;
            for (size_t p=0;p<clip->tracks[t].Propertys.size();p++)
                if (clip->tracks[t].Propertys[p].Property==propId && clip->tracks[t].Propertys[p].component==compId)
                    return &clip->tracks[t].Propertys[p];
        }
    }
    return NULL;
}

static size_t DopeIconoObj(Object* o){
    if (!o) return (size_t)IconType::empty;
    switch (o->getType()){
        case ObjectType::mesh:     return (size_t)IconType::mesh;
        case ObjectType::light:    return (size_t)IconType::light;
        case ObjectType::camera:   return (size_t)IconType::camera;
        case ObjectType::armature: return (size_t)IconType::armature;
        case ObjectType::curve:    return (size_t)IconType::curve;
        case ObjectType::sonido:   return (size_t)IconType::sonido;
        default:                   return (size_t)IconType::empty;
    }
}
// frames con keyframe de UNA CURVA (propiedad + componente). Cada componente tiene los SUYOS.
static void DopeKeysDeCurva(const std::vector<AnimProperty>& props, int prop, int comp, std::vector<int>& out){
    out.clear();
    for (size_t p=0;p<props.size();p++) if (props[p].Property==prop && props[p].component==comp)
        for (size_t k=0;k<props[p].keyframes.size();k++) out.push_back(props[p].keyframes[k].frame);
    std::sort(out.begin(), out.end()); out.erase(std::unique(out.begin(),out.end()), out.end());
}
// frames de TODAS las curvas de una propiedad (union X+Y+Z) -> lo que marca la fila padre
static void DopeKeysDeProp(const std::vector<AnimProperty>& props, int prop, std::vector<int>& out){
    out.clear();
    for (size_t p=0;p<props.size();p++) if (props[p].Property==prop)
        for (size_t k=0;k<props[p].keyframes.size();k++) out.push_back(props[p].keyframes[k].frame);
    std::sort(out.begin(), out.end()); out.erase(std::unique(out.begin(),out.end()), out.end());
}
static void DopeUnir(std::vector<int>& dst, const std::vector<int>& src){
    for (size_t i=0;i<src.size();i++) dst.push_back(src[i]);
    std::sort(dst.begin(), dst.end()); dst.erase(std::unique(dst.begin(),dst.end()), dst.end());
}
// UNION de los frames de las 3 propiedades (lo que marca la fila PADRE, este o no desplegada)
static std::vector<int> DopeUnionProps(const std::vector<AnimProperty>& props){
    const int propId[6] = { AnimPosition, AnimRotation, AnimScale, AnimVisible, AnimRender, AnimSonido };
    std::vector<int> uni;
    for (int g=0;g<6;g++){ std::vector<int> ks; DopeKeysDeProp(props, propId[g], ks); DopeUnir(uni, ks); }
    return uni;
}
// agrega las filas de CANAL (X/Y/Z de Location / Euler Rotation / Scale). Cada canal es una CURVA propia y muestra
// SUS keyframes: X puede tener un rombo en el frame 4 y Y no. Solo se listan los canales que tienen keyframes.
static void DopeCanales(const std::vector<AnimProperty>& props, std::vector<Timeline::DopeRow>& out, int nivel, const std::string& ownerKey,
                        Object* obj = NULL){
    static const char* nP[3] = {"X Location","Y Location","Z Location"};
    static const char* nR[3] = {"X Euler Rotation","Y Euler Rotation","Z Euler Rotation"};
    static const char* nS[3] = {"X Scale","Y Scale","Z Scale"};
    const int propId[3] = { AnimPosition, AnimRotation, AnimScale };
    const char* const* noms[3] = { nP, nR, nS };
    for (int g=0; g<3; g++){
        // TODO se muestra Z-ARRIBA, como en Properties (y en Lua): la "Y" de cada canal es la z interna y la "Z" la y.
        // (Hasta el 2026-11-11 solo la posicion: la rotacion y la escala iban con los ejes del motor, y "Y Escala" era
        // la altura.)
        const int compZArriba[3] = { AnimX, AnimZ, AnimY };
        const int* compId = compZArriba;
        for (int c=0;c<3;c++){
            std::vector<int> ks; DopeKeysDeCurva(props, propId[g], compId[c], ks); // los keyframes de ESTE canal
            if (ks.empty()) continue;                                              // canal sin animacion: no se lista
            Timeline::DopeRow r; r.tipo=3; r.nivel=nivel; r.nombre=T(noms[g][c]); r.icono=-1; r.keys=ks;
            r.ownerKey = ownerKey; r.propId = propId[g]; r.compId = compId[c];
            char cb[24]; snprintf(cb,sizeof cb,"|p%d|c%d", propId[g], compId[c]);
            r.claveFila = ownerKey + cb;
            out.push_back(r);
        }
    }
    // canales de UNA sola componente (curvas 0/1): Visible y Renderizar
    const int propUno[2] = { AnimVisible, AnimRender };
    const char* nomUno[2] = { "Visible", "Renderable" };
    for (int u=0; u<2; u++){
        std::vector<int> ks; DopeKeysDeCurva(props, propUno[u], AnimX, ks);
        if (ks.empty()) continue;
        Timeline::DopeRow r; r.tipo=3; r.nivel=nivel; r.nombre=T(nomUno[u]); r.icono=-1; r.keys=ks;
        r.ownerKey = ownerKey; r.propId = propUno[u]; r.compId = AnimX;
        char cb[24]; snprintf(cb,sizeof cb,"|p%d|c0", propUno[u]);
        r.claveFila = ownerKey + cb;
        out.push_back(r);
    }
    // un SONIDO: volumen / tono / reverb (AnimSonido X / Y / Z)
    const char* nomSon[3] = { "Volume", "Pitch", "Reverb" };
    for (int c = 0; c < 3; c++){
        std::vector<int> ks; DopeKeysDeCurva(props, AnimSonido, AnimX + c, ks);
        if (ks.empty()) continue;
        Timeline::DopeRow r; r.tipo=3; r.nivel=nivel; r.nombre=T(nomSon[c]); r.icono=-1; r.keys=ks;
        r.ownerKey = ownerKey; r.propId = AnimSonido; r.compId = AnimX + c;
        char cb[24]; snprintf(cb,sizeof cb,"|p%d|c%d", AnimSonido, AnimX + c);
        r.claveFila = ownerKey + cb;
        out.push_back(r);
    }
    // los PASOS de una rutina (AnimPaso): una fila por campo animado, con el nombre del paso ("Color · R", "Textura · on")
    std::vector<int> comps;
    for (size_t p = 0; p < props.size(); p++)
        if (props[p].Property == AnimPaso && !props[p].keyframes.empty()) comps.push_back(props[p].component);
    std::sort(comps.begin(), comps.end());
    for (size_t i = 0; i < comps.size(); i++) {
        std::vector<int> ks; DopeKeysDeCurva(props, AnimPaso, comps[i], ks);
        const int campo = comps[i] % AnimPasoCampos;
        W3dPaso* paso = (obj && obj->getType() == ObjectType::rutina) ? ((Rutina*)obj)->PasoPorId((unsigned)(comps[i] / AnimPasoCampos)) : NULL;
        std::string nom = paso ? std::string(T(W3dPasoEtiqueta(paso->tipo))) : std::string("?");
        nom += std::string(" \xC2\xB7 ") + (campo == AnimPasoOn ? std::string(T("on")) : paso ? std::string(T(W3dPasoNumeroNombre(*paso, campo))) : std::string("?"));
        Timeline::DopeRow r; r.tipo=3; r.nivel=nivel; r.nombre=nom; r.icono=-1; r.keys=ks;
        r.ownerKey = ownerKey; r.propId = AnimPaso; r.compId = comps[i];
        char cb[32]; snprintf(cb,sizeof cb,"|p%d|c%d", AnimPaso, comps[i]);
        r.claveFila = ownerKey + cb;
        out.push_back(r);
    }
}

// canales del ARMATURE 2D: los mismos (prop, comp) que el 3D pero con NOMBRES 2D y sin el
// componente Z (que en 2D no existe): U/V Location, Rotation, X/Y Scale.
static void DopeCanales2D(const std::vector<AnimProperty>& props, std::vector<Timeline::DopeRow>& out,
                          int nivel, const std::string& ownerKey){
    const int propId[5] = { AnimPosition, AnimPosition, AnimRotation, AnimScale, AnimScale };
    const int compId[5] = { AnimX,        AnimY,        AnimX,        AnimX,     AnimY     };
    const char* noms[5] = { "U Location", "V Location", "Rotation", "X Scale", "Y Scale" };
    for (int c = 0; c < 5; c++){
        std::vector<int> ks; DopeKeysDeCurva(props, propId[c], compId[c], ks);
        if (ks.empty()) continue;                       // canal sin animacion: no se lista
        Timeline::DopeRow r; r.tipo=3; r.nivel=nivel; r.nombre=T(noms[c]); r.icono=-1; r.keys=ks;
        r.ownerKey = ownerKey; r.propId = propId[c]; r.compId = compId[c];
        char cb[24]; snprintf(cb,sizeof cb,"|p%d|c%d", propId[c], compId[c]);
        r.claveFila = ownerKey + cb;
        out.push_back(r);
    }
}

void Timeline::ConstruirDopeRows(){
    dopeRows.clear(); panelW = 0;
    rowH = RenglonHeightGS;
    // "JUEGO" (el estado base de un juego, ver W3dRaices.h): un juego NO tiene keyframes propios (el cabezal
    // lo lleva la simulacion); las curvas se ven eligiendo uno de sus CLIPS en el selector de animacion.
    if (ActiveAnimKind == 2) return;
    std::vector<DopeRow> filas;
    SkeletalAnimation* clip = ClipActivo();
    if (clip){
        // ---- ARMATURE: SOLO en Pose Mode y SOLO los huesos SELECCIONADOS (sino es un caos) ----
        Armature* a = ActiveAnimArm;
        if (a && InteractionMode == PoseMode){
            std::vector<int> huesos;
            for (size_t b=0;b<a->bones.size();b++){
                if (!(a->bones[b].select || (int)b == a->boneActivo)) continue;
                for (size_t t=0;t<clip->tracks.size();t++) if (clip->tracks[t].bone==(int)b){ huesos.push_back((int)b); break; }
            }
            if (!huesos.empty()){
                size_t iArm = filas.size();
                DopeRow r; r.tipo=1; r.nivel=0; r.nombre=a->name; r.icono=(int)IconType::armature;
                // la clave lleva el INDICE DEL CLIP adentro (DopePrefArm): la seleccion del dope
                // sobrevive a cambiar de clip, y sin esto la clave vieja matcheaba la fila del
                // MISMO hueso en el clip NUEVO -> se editaba la curva de un clip que el usuario
                // nunca eligio. Ver el bloque de identidad arriba.
                r.claveDespliegue = DopePrefArm(); r.claveFila = r.claveDespliegue; r.ownerKey = r.claveDespliegue; r.propId=-1; r.compId=-1;
                filas.push_back(r);
                if (!DopeColapsado(r.claveDespliegue)){
                    for (size_t h=0; h<huesos.size(); h++){
                        int b = huesos[h];
                        const BoneTrack* tr = NULL;
                        for (size_t t=0;t<clip->tracks.size();t++) if (clip->tracks[t].bone==b){ tr=&clip->tracks[t]; break; }
                        if (!tr) continue;
                        std::vector<int> u = DopeUnionProps(tr->Propertys);
                        char sb[24]; snprintf(sb,sizeof sb,"/b%d",b);
                        DopeRow rb; rb.tipo=2; rb.nivel=1; rb.nombre=a->bones[b].name; rb.icono=(int)IconType::armature;
                        rb.claveDespliegue = DopePrefArm() + sb; rb.claveFila = rb.claveDespliegue;
                        rb.ownerKey = rb.claveDespliegue; rb.propId=-1; rb.compId=-1; rb.keys = u;
                        filas.push_back(rb);
                        if (!DopeColapsado(rb.claveDespliegue)) DopeCanales(tr->Propertys, filas, 2, rb.ownerKey);
                        DopeUnir(filas[iArm].keys, u);
                    }
                } else { // colapsado: el armature igual marca la union de sus huesos seleccionados
                    for (size_t h=0; h<huesos.size(); h++){ int b=huesos[h];
                        for (size_t t=0;t<clip->tracks.size();t++) if (clip->tracks[t].bone==b){
                            std::vector<int> u = DopeUnionProps(clip->tracks[t].Propertys); DopeUnir(filas[iArm].keys, u); break; } }
                }
            }
        }
    } else if (ActiveAnimKind == 4 && ActiveAnimMesh && Clip2DActivo()) {
        // ---- CLIP DEL ARMATURE 2D (kind 4): una fila por la malla + una por HUESO 2D + sus canales.
        // Mismo criterio de filtro que el armature 3D: SOLO con el UV editor en modo POSE y SOLO los
        // huesos SELECCIONADOS (sin eso un rig de 30 huesos llena el panel de ruido).
        Armature2DAnimation* clip = Clip2DActivo();
        Mesh* mm = ActiveAnimMesh;
        if (UVEditorEnModoPose()){
            std::vector<int> huesos;
            for (size_t b=0;b<mm->Arm2DHuesos().size();b++){
                if (!(mm->Arm2DHuesos()[b].select || (int)b == mm->Arm2DBoneActivo())) continue;
                for (size_t t=0;t<clip->tracks.size();t++) if (clip->tracks[t].bone==(int)b){ huesos.push_back((int)b); break; }
            }
            if (!huesos.empty()){
                size_t iArm = filas.size();
                // la clave lleva el INDICE del armature 2D y el DEL CLIP: una malla puede tener
                // varios rigs 2D y cada uno varios clips -> sin eso compartian el estado de
                // despliegue y, peor, el ownerKey de las curvas (la seleccion del dope terminaba
                // editando el clip de al lado). Todo eso lo arma DopePrefArm2D.
                const std::string prefA2 = DopePrefArm2D();
                const Armature2D* arm2 = mm->Arm2DActivoP();
                DopeRow r; r.tipo=1; r.nivel=0;
                r.nombre = mm->name + (arm2 ? (" (" + arm2->nombre + ")") : std::string());
                r.icono=(int)IconType::armature;
                r.claveDespliegue = prefA2; r.claveFila = r.claveDespliegue;
                r.ownerKey = r.claveDespliegue; r.propId=-1; r.compId=-1;
                filas.push_back(r);
                for (size_t h=0; h<huesos.size(); h++){
                    int b = huesos[h];
                    const Bone2DTrack* tr = NULL;
                    for (size_t t=0;t<clip->tracks.size();t++) if (clip->tracks[t].bone==b){ tr=&clip->tracks[t]; break; }
                    if (!tr) continue;
                    std::vector<int> u = DopeUnionProps(tr->Propertys);
                    DopeUnir(filas[iArm].keys, u);
                    if (DopeColapsado(filas[iArm].claveDespliegue)) continue; // colapsado: solo la union
                    char sb[24]; snprintf(sb,sizeof sb,"/b%d",b);
                    DopeRow rb; rb.tipo=2; rb.nivel=1; rb.nombre=mm->Arm2DHuesos()[b].nombre; rb.icono=(int)IconType::armature;
                    rb.claveDespliegue = prefA2 + sb; rb.claveFila = rb.claveDespliegue;
                    rb.ownerKey = rb.claveDespliegue; rb.propId=-1; rb.compId=-1; rb.keys = u;
                    filas.push_back(rb);
                    if (!DopeColapsado(rb.claveDespliegue)) DopeCanales2D(tr->Propertys, filas, 2, rb.ownerKey);
                }
            }
        }
    } else if (ActiveAnimKind == 3 && ActiveAnimMesh) {
        // ---- ANIMACION PROPIA del objeto (kind 3): sus curvas viven en el clip,
        // no en la escena. El dope muestra las curvas de la animacion ACTIVA. ----
        extern VertexAnimation* AnimObjetoActiva(Object*);
        VertexAnimation* an = AnimObjetoActiva((Object*)ActiveAnimMesh);
        Object* o = (Object*)ActiveAnimMesh;
        if (an){
            // frames de VERTICES, UV y NORMALES: TRES curvas separadas (cada frame
            // aparece en la fila de cada capa que trae). Con el toggle "Solo vertices
            // seleccionados" (y la malla en Edit Mode), cada fila muestra SOLO los
            // keyframes que le cambian algo a los vertices seleccionados.
            std::vector<unsigned char> selMask;
            const bool filtrar = g_dopeSoloSelVerts && an->vcount == ((Mesh*)o)->vertexSize &&
                                 DopeMascaraVertsSel((Mesh*)o, selMask);
            std::vector<int> vtx, vuv, vnrm;
            for (size_t k=0;k<an->frames.size();k++){
                const VertexFrame* f = an->frames[k]; if (!f) continue;
                if (f->positions && (!filtrar || VertexFrameAnimaSel(*an, f, 0, selMask))) vtx.push_back(f->frame);
                if (f->uvs       && (!filtrar || VertexFrameAnimaSel(*an, f, 1, selMask))) vuv.push_back(f->frame);
                if (f->normals   && (!filtrar || VertexFrameAnimaSel(*an, f, 2, selMask))) vnrm.push_back(f->frame);
            }
            std::sort(vtx.begin(),vtx.end()); vtx.erase(std::unique(vtx.begin(),vtx.end()),vtx.end());
            std::sort(vuv.begin(),vuv.end()); vuv.erase(std::unique(vuv.begin(),vuv.end()),vuv.end());
            std::sort(vnrm.begin(),vnrm.end()); vnrm.erase(std::unique(vnrm.begin(),vnrm.end()),vnrm.end());

            std::vector<int> u = DopeUnionProps(an->curvas);
            DopeUnir(u, vtx);   // el objeto marca la union de curvas + vertices + uv + normales
            DopeUnir(u, vuv);
            DopeUnir(u, vnrm);
            if (!u.empty()){
                DopeRow r; r.tipo=1; r.nivel=0; r.nombre=o->name; r.icono=(int)DopeIconoObj(o);
                r.claveDespliegue = "objanim:" + DopeIdDueno(o); r.claveFila = r.claveDespliegue;
                r.ownerKey = r.claveDespliegue; r.propId=-1; r.compId=-1; r.keys = u;
                filas.push_back(r);
                if (!DopeColapsado(r.claveDespliegue)){
                    // filas VERTICES y UV: los keyframes de la malla, POR CAPA. Son canales
                    // seleccionables (propId=AnimVertex): click/seleccion/dibujo van por la
                    // maquinaria generica; mover/borrar/interpolar se rutean por el prefijo
                    // "objvtx:"/"objuv:" a los VertexFrame de SU capa (ver
                    // SetInterpolacionSel/DopeBorrarSeleccion/move).
                    if (!vtx.empty()){
                        DopeRow v; v.tipo=2; v.nivel=1; v.nombre=T("Vertices"); v.icono=(int)IconType::mesh;
                        v.claveDespliegue = "objvtx:" + DopeIdDueno(o); v.claveFila = v.claveDespliegue;
                        v.ownerKey = v.claveDespliegue; v.propId=AnimVertex; v.compId=0; v.keys = vtx;
                        filas.push_back(v);
                    }
                    if (!vuv.empty()){
                        DopeRow v; v.tipo=2; v.nivel=1; v.nombre=T("UV"); v.icono=(int)IconType::textura;
                        v.claveDespliegue = "objuv:" + DopeIdDueno(o); v.claveFila = v.claveDespliegue;
                        v.ownerKey = v.claveDespliegue; v.propId=AnimVertex; v.compId=1; v.keys = vuv;
                        filas.push_back(v);
                    }
                    // fila NORMALES (capa propia, borrable): normalmente coincide con
                    // Vertices (insertar keyframe captura posiciones+normales juntas)
                    if (!vnrm.empty()){
                        DopeRow v; v.tipo=2; v.nivel=1; v.nombre=T("Normals"); v.icono=(int)IconType::normalVertex;
                        v.claveDespliegue = "objnrm:" + DopeIdDueno(o); v.claveFila = v.claveDespliegue;
                        v.ownerKey = v.claveDespliegue; v.propId=AnimVertex; v.compId=2; v.keys = vnrm;
                        filas.push_back(v);
                    }
                    DopeCanales(an->curvas, filas, 1, r.ownerKey, o);
                }
            }
        }
    } else {
        // ---- ESCENA: objetos SELECCIONADOS que tengan animacion ----
        // la clave lleva el INDICE DE LA ESCENA adentro (DopePrefEscena): AnimationObjects es la
        // lista de la escena ACTIVA y cambiar de escena la swapea entera, asi que sin identidad la
        // seleccion vieja editaba las curvas homonimas de la escena nueva.
        const std::string prefEsc = DopePrefEscena();
        for (size_t i=0;i<AnimationObjects.size();i++){
            AnimationObject& ao = AnimationObjects[i];
            if (!ao.obj || !ao.obj->select || ao.Propertys.empty()) continue;
            std::vector<int> u = DopeUnionProps(ao.Propertys);
            if (u.empty()) continue;
            DopeRow r; r.tipo=1; r.nivel=0; r.nombre=ao.obj->name; r.icono=(int)DopeIconoObj(ao.obj);
            r.claveDespliegue = prefEsc + DopeIdDueno(ao.obj); r.claveFila = r.claveDespliegue;
            r.ownerKey = r.claveDespliegue; r.propId=-1; r.compId=-1; r.keys = u;
            filas.push_back(r);
            if (!DopeColapsado(r.claveDespliegue)){
                DopeRow g; g.tipo=2; g.nivel=1; g.nombre=(ao.obj->getType()==ObjectType::sonido) ? std::string(T("Sound")) : std::string(T("Object Transforms")); g.icono=-1; // SIN icono -> el texto va mas a la izquierda
                g.claveDespliegue = prefEsc + DopeIdDueno(ao.obj) + "/xf"; g.claveFila = g.claveDespliegue;
                g.ownerKey = r.ownerKey; g.propId=-1; g.compId=-1; g.keys = u;
                filas.push_back(g);
                if (!DopeColapsado(g.claveDespliegue)) DopeCanales(ao.Propertys, filas, 2, r.ownerKey, ao.obj);
            }
        }
        // ---- LOS SONIDOS seleccionados: su fila lleva la TIRA (la del objeto si esta animado; si no, una propia) ----
        for (size_t i = 0; i < ObjSelects.size(); i++){
            Object* o = ObjSelects[i];
            if (!o || o->getType() != ObjectType::sonido) continue;
            const std::string k = prefEsc + DopeIdDueno(o);
            bool hay = false;
            for (size_t f = 0; f < filas.size(); f++) if (filas[f].tipo == 1 && filas[f].ownerKey == k){ filas[f].sonido = o; hay = true; }
            if (hay) continue;
            DopeRow r; r.tipo=1; r.nivel=0; r.nombre=o->name; r.icono=(int)IconType::sonido;
            r.claveFila = k; r.ownerKey = k; r.propId=-1; r.compId=-1; r.sonido = o;
            filas.push_back(r);
        }
        // ...y con la forma de onda o el volumen a la vista, la tira es MAS ALTA: filas de continuacion debajo
        if (g_tlVerOnda || g_tlVerVolumen)
            for (size_t f = 0; f < filas.size(); f++){
                if (!filas[f].sonido || filas[f].tiraFilas != 0) continue;
                filas[f].tiraFilas = kTiraFilasExtra;
                for (int e = 0; e < kTiraFilasExtra; e++){
                    DopeRow c; c.tipo=1; c.nivel=filas[f].nivel; c.icono=-1; c.sonido=filas[f].sonido; c.tiraFilas=-1;
                    c.ownerKey = filas[f].ownerKey;   // (el ojo del Summary / del objeto tambien la oculta)
                    filas.insert(filas.begin() + (long)f + 1, c);
                }
            }
        // ---- PISTA "CAMARA ACTIVA" (los cortes de la cinematica): aparece con una CAMARA seleccionada. Es una curva
        // mas (SceneAnimation::camPista): mover / borrar / escalar y Ctrl+Z van por el camino de siempre
        // (DopeResolverProp la resuelve por el prefijo "camact:e<escena>"). ----
        bool camSel = false;
        for (size_t i = 0; i < ObjSelects.size() && !camSel; i++)
            if (ObjSelects[i] && ObjSelects[i]->getType() == ObjectType::camera) camSel = true;
        AnimProperty* cp = W3dCamPista(SceneAnimActiva, false);
        if (camSel && cp && !cp->keyframes.empty()){
            char b[32]; snprintf(b, sizeof b, "camact:e%d", SceneAnimActiva);
            DopeRow r; r.tipo=1; r.nivel=0; r.nombre=T("Active camera"); r.icono=(int)IconType::camera;
            r.claveFila = b; r.ownerKey = b; r.propId = AnimCamActiva; r.compId = AnimX;
            for (size_t k=0;k<cp->keyframes.size();k++) r.keys.push_back(cp->keyframes[k].frame);
            filas.push_back(r);
        }
    }
    if (filas.empty()) return; // nada seleccionado/animado -> panelW=0 -> timeline clasico, igual que antes

    // SUMMARY (arriba de todo): la UNION de los keyframes de TODO lo listado (= solo lo seleccionado).
    // Es DESPLEGABLE: plegado oculta TODAS las filas de abajo (queda solo el Summary).
    DopeRow s; s.tipo=0; s.nivel=0; s.nombre=T("Summary"); s.icono=-1;
    s.claveDespliegue = "summary"; s.claveFila = "summary"; s.ownerKey = ""; s.propId = -1;
    for (size_t i=0;i<filas.size();i++) if (filas[i].tipo==1) DopeUnir(s.keys, filas[i].keys);
    dopeRows.push_back(s);
    if (!DopeColapsado("summary"))
        for (size_t i=0;i<filas.size();i++) dopeRows.push_back(filas[i]);

    // OJO: cada fila esta oculta si le apagaron el suyo o el de alguno de sus PADRES (el padre de un canal es su
    // hueso/objeto, y el de todos es el Summary). Se resuelve aca, una vez: el resto del codigo solo lee 'oculto'.
    for (size_t i=0;i<dopeRows.size();i++){
        DopeRow& d = dopeRows[i];
        bool off = DopeOjoOff(d.claveFila);
        if (!off) for (size_t j=0;j<dopeRows.size() && !off;j++){
            const DopeRow& p2 = dopeRows[j];
            if (&p2 == &d || !DopeOjoOff(p2.claveFila)) continue;
            if (p2.tipo == 0) off = true;                       // el Summary cubre todo
            else if (p2.propId < 0 && DopeCubre(p2.ownerKey, d.ownerKey)) off = true;
        }
        d.oculto = off;
    }

    // ancho del panel = la fila mas larga. MISMAS distancias que el Outliner: solo se reserva el hueco de la
    // flecha si la fila es desplegable y el del icono si tiene icono (sino el texto se corre a la izquierda).
    int maxw = 0;
    for (size_t i=0;i<dopeRows.size();i++){
        const DopeRow& d = dopeRows[i];
        int w = marginGS + d.nivel*(IconSizeGS+gapGS);
        if (!d.claveDespliegue.empty()) w += IconSizeGS + gapGS;
        if (d.icono >= 0)               w += IconSizeGS + gapGS;
        w += UTF8_Largo(d.nombre)*CharacterWidthGS + marginGS*2;
        w += IconSizeGS + gapGS;   // la columna de OJOS de la derecha
        if (w > maxw) maxw = w;
    }
    panelContW = maxw - (IconSizeGS + gapGS) - marginGS;   // lo que se scrollea: el texto, sin los ojos
    // ancho: el que fijo el usuario arrastrando el borde, o el automatico (fila mas larga)
    panelW = (panelWUser > 0) ? panelWUser : maxw;
    int tope = width/2; if (tope < IconSizeGS*4) tope = IconSizeGS*4;
    if (panelW > tope) panelW = tope;
    int piso = IconSizeGS*2; if (panelW < piso) panelW = piso;
    // SUMMARY PLEGADO -> se OCULTA el panel entero (panelW=0): el timeline pasa a ocupar TODO y el texto
    // "Summary" queda FLOTANDO encima (lo dibuja RenderDopePanel sin fondo).
    if (DopeColapsado("summary") || !g_dopePanelOn) panelW = 0;
    // BARRA DE SCROLL: la del editor (Scrollable, la misma del Outliner) -> hover que la agranda, agarre verde,
    // arrastre y touch, todo por el ruteo generico. Solo en DOPE: en CURVAS el eje vertical es el VALOR, no una
    // lista de filas, asi que no hay nada que scrollear.
    if (modo == TL_MODO_DOPE) ResizeScrollbar(width, height, 0, -DopeAltoContenido(), stripY);
    else {   // CURVAS: sin barra (el eje vertical del strip es el VALOR), pero el PANEL igual se scrollea con el medio
        scrollY = scrollX = false;
        int maxY = (height - stripY) - DopeAltoContenido(); if (maxY > 0) maxY = 0;
        if (PosY < maxY) PosY = maxY; if (PosY > 0) PosY = 0;
    }
    if (panelPosX < PanelMaxX()) panelPosX = PanelMaxX(); if (panelPosX > 0) panelPosX = 0;
}

// ------------------------------------------------------------------ onChange -> timeline activo
static Timeline* g_tlActivo = NULL;
// menu Select del dope sheet: mismas acciones que A / Alt+A / Ctrl+I
static void TL_menuSelAction(int id){
    if (g_tlActivo){
        if      (id==0) g_tlActivo->DopeSelectAll();
        else if (id==1) g_tlActivo->DopeSelectNone();
        else if (id==2) g_tlActivo->DopeSelectInvert();
    }
    TL_CerrarMenu(g_tlMenuSel);
    g_redraw = true;
}
// menu View del dope sheet: encuadrar los keyframes seleccionados
enum { TL_VIEW_FRAMESEL = 0, TL_VIEW_AUTOFRAME = 1, TL_VIEW_PANEL = 2, TL_VIEW_SOLOSEL = 3, TL_VIEW_ONDA = 4, TL_VIEW_VOLUMEN = 5,
       TL_VIEW_DOPE = 6, TL_VIEW_CURVAS = 7 };
static void TL_menuViewAction(int id){
    // el CHECKBOX ya lo toggleo PopupMenu::Click, que a proposito NO cierra el menu (asi se prenden/apagan
    // varios de una). Cerrarlo aca romperia eso.
    if (id == TL_VIEW_AUTOFRAME){ g_redraw = true; return; }
    if (id == TL_VIEW_PANEL){ g_redraw = true; return; }   // el checkbox ya toco el bool; el ancho se recalcula solo
    if (id == TL_VIEW_SOLOSEL){ g_redraw = true; return; } // idem: las filas se re-arman solas con el filtro
    if (id == TL_VIEW_ONDA || id == TL_VIEW_VOLUMEN){ g_redraw = true; return; }
    if (g_tlActivo && id == TL_VIEW_FRAMESEL) g_tlActivo->DopeFrameSelected();
    if (g_tlActivo && (id == TL_VIEW_DOPE || id == TL_VIEW_CURVAS)) g_tlActivo->CambiarModo(id == TL_VIEW_CURVAS);
    TL_CerrarMenu(g_tlMenuView);
    g_redraw = true;
}
// menu Key: Transform (Move/Rotate/Scale) + Duplicate + Delete. El submenu HEREDA esta misma action (ver
// PopupMenu::Click), por eso los ids no se pisan entre el menu y su submenu.
enum { TL_KEY_DUP = 0, TL_KEY_DEL = 1, TL_KEY_SMARTEULER = 2, TL_KEY_CORTAR = 3, TL_KEY_COPIAR = 4, TL_KEY_PEGAR = 5,
       TL_KEY_MOVE = 10, TL_KEY_ROT = 11, TL_KEY_SCALE = 12,
       TL_KEY_INTERP0 = 20,     // + KfConstant ... KfElastic
       TL_KEY_HANDLE0 = 40,     // + HFree / HAligned / HVector / HAuto / HAutoClamped / HPlano
       TL_KEY_EASE0 = 60 };     // + KfEaseAuto / KfEaseIn / KfEaseOut / KfEaseInOut
static void TL_menuKeyAction(int id){
    if (g_tlActivo){
        switch (id){
            case TL_KEY_DUP:   g_tlActivo->DopeDuplicarSeleccion(); break;
            case TL_KEY_CORTAR: g_tlActivo->DopeCortarSeleccion();  break;
            case TL_KEY_COPIAR: g_tlActivo->DopeCopiarSeleccion();  break;
            case TL_KEY_PEGAR:  g_tlActivo->DopePegar();            break;
            case TL_KEY_DEL:   g_tlActivo->DopeBorrarSeleccion();   break;
            case TL_KEY_SMARTEULER: g_tlActivo->SmartEulerSel();    break;
            case TL_KEY_MOVE:  g_tlActivo->DopeMoveStart(Timeline::DOPE_MOV); break;
            case TL_KEY_ROT:   g_tlActivo->DopeMoveStart(Timeline::DOPE_ROT); break;
            case TL_KEY_SCALE: g_tlActivo->DopeMoveStart(Timeline::DOPE_ESC); break;
            default:
                if      (id >= TL_KEY_EASE0)   g_tlActivo->SetEasingSel(id - TL_KEY_EASE0);
                else if (id >= TL_KEY_HANDLE0) g_tlActivo->SetHandleTypeSel(id - TL_KEY_HANDLE0);
                else if (id >= TL_KEY_INTERP0) g_tlActivo->SetInterpolacionSel(id - TL_KEY_INTERP0);
                break;
        }
    }
    // el item pudo venir del menu Key o de cualquiera de sus submenus abierto SOLO por su atajo ('t' / 'v'):
    // se cierra el que este abierto, sea cual sea (TL_CerrarMenu no hace nada con los otros).
    TL_CerrarMenu(g_tlMenuKey); TL_CerrarMenu(g_tlMenuKeyIn); TL_CerrarMenu(g_tlMenuHandle);
    g_redraw = true;
}
// menu Pivot del dope sheet: desde donde escala 'S' (centro de los keyframes seleccionados / frame actual)
static void TL_menuPivotAction(int id){
    if (g_tlActivo) g_tlActivo->DopeSetPivot(id);
    TL_CerrarMenu(g_tlMenuPivot);
    g_redraw = true;
}
static void TL_onStart(){ if (g_tlActivo) g_tlActivo->ApplyStart(); }
static void TL_onEnd()  { if (g_tlActivo) g_tlActivo->ApplyEnd();   }
static void TL_onCur()  { if (g_tlActivo) g_tlActivo->ApplyCur();   }
static void TL_onFps()  { if (g_tlActivo) g_tlActivo->ApplyFps();   }

Timeline::Timeline(){
    pxPerFrame = (float)GlobalScale * 7.0f;
    viewStartF = -2.0f;                 // arranca mostrando un par de frames negativos (frame 0 prolijo)
    modo = TL_MODO_DOPE;
    pxPerUnit = (float)GlobalScale * 7.0f; // zoom vertical propio (curvas); INDEPENDIENTE del horizontal
    viewCenterV = 0.0f;                    // el CENTRO del strip es el CERO
    scrubbing = false; lastMx = lastMy = 0;
    stripY = numY = barH2 = 0; curBtnX = curBtnW = 0;
    panelW = 0; panelWUser = 0; panelResize = false; rowH = RenglonHeightGS; hoverRow = -1;
    panelBordeHover = false; panelPosX = 0; panelContW = 0; panelBarraH = false; panelPaneo = 0; // dope sheet (panelW=0 -> timeline clasico)
    fStart=(float)StartFrame; fEnd=(float)EndFrame; fCur=(float)CurrentFrame;

    pfStart = new PropFloat(T("Start")); pfStart->value=&fStart; pfStart->entero=true; pfStart->onChange=TL_onStart;
    pfEnd   = new PropFloat(T("End"));   pfEnd->value=&fEnd;     pfEnd->entero=true;   pfEnd->onChange=TL_onEnd;
    pfCur   = new PropFloat(T("Current Frame")); pfCur->value=&fCur;     pfCur->entero=true;   pfCur->onChange=TL_onCur;
    pfFps   = new PropFloat(T("FPS"));   pfFps->value=&fFpsTL;   pfFps->entero=true;   pfFps->onChange=TL_onFps;

    BarCrear(); // [0] = icono/menu de tipo
    // (el cartelito de AYUDA de cada uno: que hace, al dejar el mouse encima)
    static const char* const kAyudaT[8] = { "Jump to Start", "Previous Keyframe", "Previous Frame", "Play Reverse",
                                            "Play", "Next Frame", "Next Keyframe", "Jump to End" };
    for (int i=0;i<8;i++){
        Button* b = new Button("");          // vacio + cuadrado: el glyph vectorial va encima
        b->rol = TL_ROL_T0 + i;
        b->cuadrado = true;                  // ancho = alto (con el padding de UIBotonAltura)
        b->ayuda = kAyudaT[i];
        BarButtons.push_back(b); btnT[i] = b;
    }
    // INICIO / FIN / FPS: un RELOJ que los abre en un desplegable (TiempoPopup). Los tres botones de texto se comian
    // la barra (en el N95 no entraban). Siguen existiendo (ocultos): EditarCampo y la edicion por texto los usan.
    btnTiempo = new Button("", (int)IconType::reloj); btnTiempo->rol = TL_ROL_TIEMPO; btnTiempo->desplegable = true;
    btnTiempo->cuadrado = true; btnTiempo->ayuda = "Start / End / FPS";
    BarButtons.push_back(btnTiempo);
    btnStart = new Button(std::string(T("Start")) + ":1");   btnStart->rol = TL_ROL_START; btnStart->visible = false; BarButtons.push_back(btnStart);
    btnEnd   = new Button(std::string(T("End")) + ":250");   btnEnd->rol   = TL_ROL_END;   btnEnd->visible = false;   BarButtons.push_back(btnEnd);
    btnFps   = new Button("FPS:30");                          btnFps->rol   = TL_ROL_FPS;   btnFps->visible = false;   BarButtons.push_back(btnFps);
    btnAnim  = new Button(T("Scene"), (int)IconType::camera); btnAnim->rol = TL_ROL_ANIM; btnAnim->desplegable = true;
    btnAnim->caretMenu = true; // aca SI conviene la flechita (dropdown de animacion)
    // SIEMPRE visible (Scene por defecto); SyncFields actualiza texto/icono segun la animacion activa
    BarButtons.push_back(btnAnim);
    // menu Select del dope sheet (All / None / Invert): al FINAL de la barra; solo visible si hay filas
    // AUTO KEY: toggle. Prendido = rojo (esta grabando). Siempre visible: es un modo, no depende de la seleccion.
    // (un PUNTO: rojo = grabando. El cartelito dice que es)
    btnAutoKey = new Button("", (int)IconType::grabar); btnAutoKey->rol = TL_ROL_AUTOKEY; btnAutoKey->cuadrado = true;
    btnAutoKey->ayuda = "Auto Keyframe";
    BarButtons.push_back(btnAutoKey);
    // switch DOPE SHEET <-> CURVES: el texto dice a que modo se PASA (es un boton, no un dropdown)
    btnModo = new Button(T("Curves")); btnModo->rol = TL_ROL_MODO;
    btnModo->visible = false;
    BarButtons.push_back(btnModo);
    // (el MISMO icono que el Select de la vista 3D y del outliner: el cuadrado punteado)
    btnSelect = new Button("", IconType::seleccion); btnSelect->rol = TL_ROL_SELECT; btnSelect->desplegable = true; btnSelect->ayuda = "Select";
    btnSelect->visible = false;
    BarButtons.push_back(btnSelect);
    // menu Key: Transform (Move/Rotate/Scale) + Duplicate + Delete
    // (solo el ROMBO de la key: el menu abierto lleva su titulo y el cartelito dice que es)
    btnKey = new Button("", IconType::keyframe); btnKey->rol = TL_ROL_KEY; btnKey->desplegable = true; btnKey->ayuda = "Key";
    btnKey->visible = false;
    BarButtons.push_back(btnKey);
    // menu Modifiers: los modificadores de las curvas (Repetir antes / despues, con o sin offset)
    btnMods = new Button("", IconType::modificador); btnMods->desplegable = true; btnMods->ayuda = "Modifiers";
    btnMods->visible = false;
    BarButtons.push_back(btnMods);
    // menu View del dope sheet: encuadrar el timeline en los keyframes seleccionados
    btnView = new Button("", IconType::monitor); btnView->rol = TL_ROL_VIEW; btnView->desplegable = true; btnView->ayuda = "View";
    btnView->visible = false;
    BarButtons.push_back(btnView);
    // menu Pivot del dope sheet (desde donde escala 's'): mismo criterio que el Pivot Point del viewport 3D
    // (un ICONO, como el Pivot Point del 3D: punto medio = centro, cursor 3D = frame actual)
    btnPivot = new Button("", (int)IconType::pivotMedian); btnPivot->rol = TL_ROL_PIVOT; btnPivot->desplegable = true; // solo el icono
    btnPivot->visible = false;
    BarButtons.push_back(btnPivot);
    // ---- TOOLBAR de abajo (estilo Inkscape): solo iconos, con su cartelito; cada GRUPO arranca con una linea ----
    { struct B { static Button* Nuevo(std::vector<Button*>& v, int icono, int rol, const char* ayuda, bool sep){
          Button* b = new Button("", icono); b->rol = rol; b->cuadrado = true; b->centrado = true; b->ayuda = ayuda;
          b->separador = sep; b->visible = false; v.push_back(b); return b; } };
      B::Nuevo(ToolButtons, IconType::interp_constant, TL_TB_INTERP0 + KfConstant, "Constant", true);
      B::Nuevo(ToolButtons, IconType::interp_linear,   TL_TB_INTERP0 + KfLinear,   "Linear", false);
      B::Nuevo(ToolButtons, IconType::interp_bezier,   TL_TB_INTERP0 + KfBezier,   "Bezier", false);
      // las de EASING: por fuerza | con efecto (los mismos grupos que el menu de la 'E')
      B::Nuevo(ToolButtons, IconType::interp_sine,    TL_TB_INTERP0 + KfSine,    "Sinusoidal", true);
      B::Nuevo(ToolButtons, IconType::interp_quad,    TL_TB_INTERP0 + KfQuad,    "Quadratic", false);
      B::Nuevo(ToolButtons, IconType::interp_cubic,   TL_TB_INTERP0 + KfCubic,   "Cubic", false);
      B::Nuevo(ToolButtons, IconType::interp_quart,   TL_TB_INTERP0 + KfQuart,   "Quartic", false);
      B::Nuevo(ToolButtons, IconType::interp_quint,   TL_TB_INTERP0 + KfQuint,   "Quintic", false);
      B::Nuevo(ToolButtons, IconType::interp_expo,    TL_TB_INTERP0 + KfExpo,    "Exponential", false);
      B::Nuevo(ToolButtons, IconType::interp_circ,    TL_TB_INTERP0 + KfCirc,    "Circular", false);
      B::Nuevo(ToolButtons, IconType::interp_back,    TL_TB_INTERP0 + KfBack,    "Back (overshoot)", true);
      B::Nuevo(ToolButtons, IconType::interp_bounce,  TL_TB_INTERP0 + KfBounce,  "Bounce", false);
      B::Nuevo(ToolButtons, IconType::interp_elastic, TL_TB_INTERP0 + KfElastic, "Elastic", false);
      B::Nuevo(ToolButtons, IconType::handle_free,        TL_TB_HANDLE0 + HFree,        "Free", true);
      B::Nuevo(ToolButtons, IconType::handle_aligned,     TL_TB_HANDLE0 + HAligned,     "Aligned", false);
      B::Nuevo(ToolButtons, IconType::handle_vector,      TL_TB_HANDLE0 + HVector,      "Vector", false);
      B::Nuevo(ToolButtons, IconType::handle_auto,        TL_TB_HANDLE0 + HAuto,        "Automatic", false);
      B::Nuevo(ToolButtons, IconType::handle_autoclamped, TL_TB_HANDLE0 + HAutoClamped, "Auto Clamped", false);
      B::Nuevo(ToolButtons, IconType::handle_flat,        TL_TB_HANDLE0 + HPlano,       "Flat", false);
      B::Nuevo(ToolButtons, IconType::ease_auto,  TL_TB_EASE0 + KfEaseAuto,  "Automatic Easing", true);
      B::Nuevo(ToolButtons, IconType::ease_in,    TL_TB_EASE0 + KfEaseIn,    "Ease In", false);
      B::Nuevo(ToolButtons, IconType::ease_out,   TL_TB_EASE0 + KfEaseOut,   "Ease Out", false);
      B::Nuevo(ToolButtons, IconType::ease_inout, TL_TB_EASE0 + KfEaseInOut, "Ease In and Out", false); }
#ifdef W3D_SYMBIAN
    // N95 (240px): la barra completa (8 transportes + Start/End + anim) es demasiada para navegar sin mouse.
    // Dejamos SOLO lo esencial: inicio(0), play(4), final(7) + Start/End + dropdown de animacion. Los demas se
    // OCULTAN (no se borran) -> siguen existiendo para PC/Android, donde la barra entra entera.
    btnT[1]->visible = false; // kf anterior
    btnT[2]->visible = false; // frame anterior
    btnT[3]->visible = false; // play reversa
    btnT[5]->visible = false; // frame siguiente
    btnT[6]->visible = false; // kf siguiente
#endif
    g_tlActivo = this;
}
Timeline::~Timeline(){
    // el cuerpo va al FINAL del archivo: necesita el estado del transform/handle, declarado mas abajo
    DestruirEstadoTimeline();
    delete pfStart; delete pfEnd; delete pfCur; delete pfFps;
    // los Button de BarButtons los libera ~ViewportBase
}

void Timeline::Resize(int newW, int newH){ ViewportBase::Resize(newW, newH); ResizeBorder(newW, newH); }

// El PANEL del dope sheet ocupa [0..panelW) y es FIJO: no se mueve con el scroll horizontal (que solo corre los
// frames). Por eso el mapeo frame<->X arranca en panelW.
float Timeline::FrameToX(float f) const { return (float)panelW + (f - viewStartF) * pxPerFrame; }
float Timeline::XToFrame(float lx) const { return viewStartF + (lx - (float)panelW) / (pxPerFrame>0.01f?pxPerFrame:0.01f); }
// EJE VERTICAL (solo curvas): el CENTRO del strip es el CERO (viewCenterV=0). Y crece hacia ABAJO en pantalla,
// asi que el valor va con signo cambiado. El zoom vertical (pxPerUnit) es INDEPENDIENTE del horizontal.
// TOPES del zoom, en UN solo lugar. Antes ZoomBy usaba [0.3, 140] y los "Frame Selected" [0.02, GlobalScale*80]:
// encuadrar una animacion larga dejaba pxPerFrame en 0.02 y el primer clic de rueda lo saltaba a 0.3 de un tiron.
// El piso tiene que ser bajo para que encuadrar animaciones largas entre; el techo va escalado como el resto de la UI.
static float ClampZoomH(float px){
    if (px < 0.02f) return 0.02f;
    float techo = (float)GlobalScale*80.0f;
    return (px > techo) ? techo : px;
}
static float ClampZoomV(float px){
    if (px < 0.05f) return 0.05f;
    float techo = (float)GlobalScale*4000.0f;  // el valor puede ser chiquito (0.001): el techo vertical va mas alto
    return (px > techo) ? techo : px;
}
// Es un numero USABLE? (ni NaN ni infinito). Lo necesita VistaAplicar: la vista puede venir de un archivo y
// std::stof parsea "inf" y "nan" sin quejarse, asi que el try/catch del lector no los atrapa. Escrito a mano y
// no con std::isfinite: este .cpp lo compila tambien Symbian, que va en C++03.
// (3.4e38 = FLT_MAX redondeado para abajo: cualquier cosa mas grande ya es el infinito)
static bool W3dFinito(float v){ return (v == v) && (v > -3.4e38f) && (v < 3.4e38f); }
int   Timeline::CentroVertical() const { return (stripY + height) / 2; }
float Timeline::ValueToY(float v) const { return (float)CentroVertical() - (v - viewCenterV) * pxPerUnit; }
float Timeline::YToValue(float ly) const { return viewCenterV + ((float)CentroVertical() - ly) / (pxPerUnit>1e-6f?pxPerUnit:1e-6f); }
void  Timeline::PanValor(float dValor){ viewCenterV += dValor; g_redraw = true; }
void  Timeline::ZoomVBy(float factor){
    pxPerUnit = ClampZoomV(pxPerUnit * factor);       // desde el centro: viewCenterV no se toca
    g_redraw = true;
}
int Timeline::TickStep() const {
    float minPx = (float)GlobalScale * 26.0f;
    const int steps[] = {1,2,5,10,25,50,100,250,500,1000,2500,5000};
    for (int i=0;i<12;i++) if (steps[i]*pxPerFrame >= minPx) return steps[i];
    return 5000;
}

void Timeline::SyncFields(){
    // MIX: el rango del timeline es SIEMPRE el del mix (tambien al abrir un .w3d que quedo en Mix)
    if (g_animMix){ StartFrame = (int)g_mixInicio; EndFrame = (int)g_mixFin; }
    // el rango/fps de la animacion activa YA vive en los globales StartFrame/EndFrame/AnimFPS (se cargan al
    // seleccionarla con AnimCargarRangoActivo). Aca solo se refleja en los campos.
    // Start / End: si NO se editan, mostrar el valor; si se editan, el boton es el input (editField)
    if (g_propFloatEditando == pfStart){ btnStart->editField = &pfStart->field; }
    else { btnStart->editField = NULL; char b[24]; snprintf(b,sizeof b,"%s:%d",T("Start"),StartFrame); btnStart->text=b; fStart=(float)StartFrame; }
    if (g_propFloatEditando == pfEnd){ btnEnd->editField = &pfEnd->field; }
    else { btnEnd->editField = NULL; char b[24]; snprintf(b,sizeof b,"%s:%d",T("End"),EndFrame); btnEnd->text=b; fEnd=(float)EndFrame; }
    if (g_propFloatEditando == pfFps){ btnFps->editField = &pfFps->field; }
    else { btnFps->editField = NULL; char b[24]; snprintf(b,sizeof b,"FPS:%d",AnimFPS); btnFps->text=b; fFpsTL=(float)AnimFPS; }
    btnEnd->visible = false;          // (van en el reloj; MODO JUEGO: sin Fin, ver AbrirTiempoPopup)
    if (g_propFloatEditando != pfCur) fCur=(float)CurrentFrame;

    // AUTO KEY prendido -> el boton queda ROJO (esta grabando). En "Juego" no hay keyframes que grabar.
    { static const float rojo[4] = { 0.95f, 0.18f, 0.18f, 1.0f };
      btnAutoKey->tinte = NULL; btnAutoKey->colorTexto = AutoKeyOn ? rojo : NULL; }
    btnAutoKey->visible = (ActiveAnimKind != 2);
    btnAutoKey->ayuda = AutoKeyOn ? "Auto Keyframe: On" : "Auto Keyframe: Off";   // el cartelito dice el estado
    // play activo -> tinte verde en el boton correspondiente
    bool pf = (PlayAnimation && AnimPlayDir>0), pr = (PlayAnimation && AnimPlayDir<0);
    btnT[3]->tinte = pr ? TL_VERDE_BTN : NULL;
    btnT[4]->tinte = pf ? TL_VERDE_BTN : NULL;

    // dropdown de animacion: SIEMPRE visible (Scene por defecto). Muestra la animacion ACTIVA: icono camara = escena,
    // icono esqueleto = clip de armature. No depende del objeto seleccionado.
    btnAnim->visible = true;
    SkeletalAnimation* c = ClipActivo();
    if (g_animMix){ btnAnim->text = T("Mix"); btnAnim->icon = (int)IconType::igual; }   // capas mezcladas
    else if (ActiveAnimKind == 2){ btnAnim->text = T("Game"); btnAnim->icon = (int)IconType::gamepad; }
    else if (ActiveAnimKind == 3 && ActiveAnimMesh){
        // vertex anim de una malla: el nombre de la anim activa (via el controlador)
        VertexAnimationActive* va = FindTargetAnim(ActiveAnimMesh);
        int i = va ? va->currentAnim : -1;
        btnAnim->text = (i >= 0 && i < (int)ActiveAnimMesh->animations.size() && ActiveAnimMesh->animations[i])
                        ? ActiveAnimMesh->animations[i]->name : ActiveAnimMesh->name;
        btnAnim->icon = (int)IconType::mesh;
    }
    else if (ActiveAnimKind == 4 && ActiveAnimMesh){
        // clip del ARMATURE 2D ACTIVO de la malla (curvas por hueso 2D). El nombre dice ADEMAS de
        // que rig es el clip (una malla puede tener varios armatures 2D con clips homonimos),
        // igual que los submenus del selector de animacion (Properties.cpp).
        Armature2DAnimation* c2 = Arm2DClipActivo(ActiveAnimMesh);
        const Armature2D* a2 = ActiveAnimMesh->Arm2DActivoP();
        btnAnim->text = c2 ? c2->name : ActiveAnimMesh->name;
        if (a2 && ActiveAnimMesh->armatures2d.size() > 1) btnAnim->text += " (" + a2->nombre + ")";
        btnAnim->icon = (int)IconType::armature;
    }
    else if (ActiveAnimKind == 5){   // FLIPBOOK activo: su nombre
        extern Flipbook* g_flipActivo;
        btnAnim->text = g_flipActivo ? g_flipActivo->nombre : std::string(T("Flipbook"));
        btnAnim->icon = (int)IconType::textura;
    }
    else if (c){ btnAnim->text = c->name; btnAnim->icon = (int)IconType::armature; }
    else  {
        // una animacion de escena, o un CLIP DE OBJETO ("Puerta: abrir", con el icono de objeto)
        InitSceneAnimations();
        btnAnim->text = W3dAnimEscenaEtiqueta(SceneAnimActiva);
        btnAnim->icon = W3dAnimEsClip(SceneAnimActiva) ? (int)IconType::object : (int)IconType::camera;
    }
    bool hayFilas = !dopeRows.empty();
    btnModo->visible   = false;             // Dope Sheet / Curves: en el menu View (el monitor)
    btnModo->text      = T((modo == TL_MODO_CURVAS) ? "Dope Sheet" : "Curves");  // dice a DONDE va
    btnSelect->visible = hayFilas;          // menu Select: solo cuando el dope sheet tiene filas
    btnView->visible   = hayFilas;          // menu View: idem (sin keyframes no hay nada que encuadrar)
    // el menu Key esta SIEMPRE: es un menu, no un boton de accion. Sus opciones actuan sobre lo que
    // haya seleccionado; que aparezca y desaparezca hace que la barra baile y no se sepa donde estan
    // las cosas.
    btnKey->visible    = true;
    btnMods->visible   = hayFilas;          // menu Modifiers: con curvas en el timeline
    btnPivot->visible  = hayFilas;          // menu Pivot: idem (sin keyframes no hay nada que escalar)
    btnPivot->icon = (g_dopePivot == DOPE_PIV_CURFRAME) ? (int)IconType::pivotCursor : (int)IconType::pivotMedian;
}

// ------------------------------------------------------------------ Render
void Timeline::Render(){
    gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
    gfx::MatrixMode(gfx::ModelView);  gfx::LoadIdentity();

    const int glY = W3dPantallaAlto - y - height;
    gfx::Enable(gfx::ScissorTest); gfx::Scissor(x, glY, width, height);
    // fondo NEGRO (el area fuera de Start..End queda negra; el rango se pinta gris tarjeta encima)
    gfx::ClearColor(ListaColores[(int)ColorID::negro][0], ListaColores[(int)ColorID::negro][1],
                    ListaColores[(int)ColorID::negro][2], 1.0f);
    gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);
    gfx::Disable(gfx::ScissorTest);

    gfx::Viewport(x, glY, width, height);
    gfx::Ortho(0, width, height, 0, -1, 1);
    gfx::Disable(gfx::Fog); gfx::Disable(gfx::DepthTest); gfx::Disable(gfx::CullFace);
    gfx::Disable(gfx::Lighting); gfx::Disable(gfx::Texture2D); gfx::Disable(gfx::Blend);
    gfx::Enable(gfx::ColorMaterial);
    gfx::EnableArray(gfx::VertexArray); gfx::DisableArray(gfx::TexCoordArray); gfx::DisableArray(gfx::NormalArray);

    // layout vertical: [menu] [barra de numeros = alto de un input] [cuerpo]
    barH2 = LetterHeightGS + GlobalScale * 4;  // barra de numeros CHICA (tipo input), no del alto del menu
    numY  = BarTopOffset();                    // debajo del menu
    stripY = numY + barH2;                     // el cuerpo arranca debajo de la barra de numeros
    if (stripY > height) stripY = height;

    ConstruirDopeRows(); // dope sheet: arma las filas de lo SELECCIONADO (panelW=0 -> timeline clasico)
    SyncFields();        // (despues de armar las filas: la visibilidad del boton Select depende de ellas)

    int step = TickStep();
    float fRight = viewStartF + width / (pxPerFrame>0.01f?pxPerFrame:0.01f);
    int f0 = ((int)floorf(viewStartF / step)) * step;
    int fN = (int)ceilf(fRight) + step;

    // ---------------- AREA REPRODUCIBLE [Start..End]: gris tarjeta (el resto queda NEGRO) ----------------
    { float xa = FrameToX((float)StartFrame);
      // MODO JUEGO: sin Fin -> el area reproducible sigue hasta el borde derecho
      float xb = AnimEsJuego ? (float)width : FrameToX((float)EndFrame + 1);
      int ia = (int)(xa<panelW?panelW:xa), ib = (int)(xb>width?width:xb); // nunca invade el panel del dope sheet
      if (ib > ia){ SetCol(ColorID::gris, 1.0f); FillRect(ia, stripY, ib-ia, height-stripY); } }

    // ---------------- LINEAS verticales (van del piso hasta tocar la barra de numeros) ----------------
    // ALTERNAN color: las "oscuras" (mas oscuras que la tarjeta) llevan numero; las "claras" (color del texto) no.
    for (int f = f0; f <= fN; f += step){
        float fx = FrameToX((float)f);
        if (fx < panelW || fx > width) continue;
        bool oscura = ((f / step) & 1) == 0;
        if (oscura) gfx::Color4f(0.03f, 0.03f, 0.03f, 1.0f);          // linea oscura (mas oscuro que la tarjeta)
        else        SetCol(ColorID::grisLinea, 1.0f);                 // linea clara (gris medio 0x494949)
        FillRect((int)fx, stripY, GlobalScale, height - stripY);
    }

    // MODO CURVAS: en vez de los rombos por fila, el editor grafico (tiempo x valor).
    if (modo == TL_MODO_CURVAS && !dopeRows.empty()) RenderCurvas();
    // KEYFRAMES. Con dope sheet: un rombo por FILA, a la altura de su renglon (scrolleable en vertical).
    // Sin dope sheet (nada seleccionado/animado): la fila unica clasica abajo, igual que siempre.
    else if (!dopeRows.empty()){
        for (size_t r=0; r<dopeRows.size(); r++){
            int ry0 = stripY + (int)r*rowH + PosY;   // las filas arrancan DEBAJO de los numeros
            if (ry0 + rowH < stripY || ry0 > height) continue;      // fuera de vista
            float ry = (float)ry0 + rowH*0.5f;
            // fondo de la fila del SUMMARY (mas oscuro) para separarlo
            if (dopeRows[r].tipo == 0){ gfx::Color4f(0.10f,0.10f,0.10f,1.0f); FillRect(panelW, ry0, width-panelW, rowH); }
            // fila SELECCIONADA: el renglon se extiende por TODO el timeline (asi se ve que keyframes abarca)
            if (g_dopeRowSel.count(dopeRows[r].claveFila)){
                SetCol(ColorID::accentDark, 1.0f);
                int a0 = ry0 < numY ? numY : ry0, a1 = ry0 + rowH; if (a1 > height) a1 = height;
                if (a1 > a0) FillRect(panelW, a0, width - panelW, a1 - a0);
            }
            const DopeRow& dr = dopeRows[r];
            if (dr.oculto) continue;                      // ojo apagado: no se dibuja
            if (dr.sonido && dr.tiraFilas >= 0) DibujarTira(dr, ry0);   // la TIRA del sonido (debajo de sus rombos)
            const std::vector<int>& ks = dr.keys;
            for (size_t i=0;i<ks.size();i++){
                float fx = FrameToX((float)ks[i]);
                if (fx < panelW - GlobalScale*3 || fx > width + GlobalScale*3) continue;
                // SELECCIONADO: naranja (accent) y un poco mas grande. Las filas padre/summary se marcan si
                // alguno de los canales que cubren tiene ese frame seleccionado.
                bool sel = false;
                if (dr.propId >= 0) sel = g_dopeKeySel.count(DopeKeyId(dr.ownerKey, dr.propId, dr.compId, ks[i])) > 0;
                else for (size_t c=0;c<dopeRows.size() && !sel;c++){ const DopeRow& cc=dopeRows[c]; if (cc.propId<0) continue;
                        if (!DopeCubre(dr.ownerKey, cc.ownerKey)) continue;
                        sel = g_dopeKeySel.count(DopeKeyId(cc.ownerKey, cc.propId, cc.compId, ks[i])) > 0; }
                if (sel)                   SetCol(ColorID::accent, 1.0f);
                else if (dr.tipo == 3)     SetCol(ColorID::grisUI, 1.0f); // canal: rombo mas apagado
                else                       SetCol(ColorID::blanco, 1.0f); // summary / objeto / grupo
                Diamond(fx, ry, (float)GlobalScale*(sel?3.8f:3.0f));
            }
        }
    }
    // (sin filas = sin panel = SIN keyframes: antes se dibujaba una fila "summary" suelta abajo de todo aunque no
    //  hubiera nada seleccionado. Si no hay panel, no hay keyframes.)

    // ---------------- BANDA DE NUMEROS: fondo OPACO, ENCIMA de los keyframes ----------------
    // Las filas empiezan DEBAJO (stripY), asi que ningun keyframe deberia caer aca; pero al SCROLLEAR una fila
    // puede asomar por arriba. Este fondo opaco la tapa: el renglon de los numeros SIEMPRE queda legible.
    gfx::Color4f(0.06f, 0.06f, 0.06f, 1.0f);
    FillRect(0, numY, width, barH2);

    // ---------------- PLAYHEAD (linea verde): del piso hasta ARRIBA de la barra de numeros (toca el boton verde) ----------------
    float px = FrameToX((float)CurrentFrame);
    if (px >= (float)panelW - GlobalScale && px <= width+GlobalScale){
        // la linea se AGARRA: blanca con el mouse encima (se entiende que se puede arrastrar), verde arrastrandola
        if (g_tlCabezalHover && !scrubbing) SetCol(ColorID::blanco, 1.0f); else gfx::Color4fv(TL_VERDE_LINEA);
        const float ancho = (g_tlCabezalHover || scrubbing) ? 2.5f : 1.5f;
        FillRect((int)(px - GlobalScale*ancho*0.4f), numY, (int)(GlobalScale*ancho), height - numY);
    }

    // ---------------- rect del boton VERDE del frame actual ----------------
    // Va DENTRO de la banda de los numeros: NO puede pisar el menu del viewport (antes arrancaba en
    // numY-GlobalScale*2 y se dibujaba al final, encima de la barra).
    std::string curTxt; { bool ec=(g_propFloatEditando==pfCur);
        if (ec) curTxt = pfCur->field.text; else { char b[16]; snprintf(b,sizeof b,"%d",CurrentFrame); curTxt=b; } }
    int curTw = UTF8_Largo(curTxt)*CharacterWidthGS;
    curBtnW = curTw + GlobalScale*8; curBtnX = (int)px - curBtnW/2;
    int curBtnY = numY, curBtnH = barH2;

    // ---------------- FASE CON TEXTURA: numeros de frame (solo sobre lineas oscuras) ----------------
    gfx::Enable(gfx::Texture2D); gfx::Enable(gfx::Blend); gfx::BlendAlpha(); gfx::EnableArray(gfx::TexCoordArray);
    if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    int numTop = numY + (barH2 - LetterHeightGS)/2;
    SetCol(ColorID::grisUI, 1.0f);
    for (int f = f0; f <= fN; f += step){
        if (((f / step) & 1) != 0) continue;         // clara -> sin numero
        float fx = FrameToX((float)f);
        if (fx < (float)panelW || fx > width+GlobalScale*8) continue; // no invadir el panel del dope sheet
        if (fx > curBtnX - GlobalScale && fx < curBtnX + curBtnW + GlobalScale) continue; // bajo el boton verde: lo tapa
        char b[16]; snprintf(b,sizeof b,"%d",f);
        TextCentrado(fx, numTop, b);
    }

    // ---------------- boton VERDE del frame actual: ANTES del panel, asi el "Summary" le queda ENCIMA ----------------
    // (la linea verde del playhead y su recuadro van DETRAS del dope sheet; sino tapaban el Summary)
    { static Card* cbtn = NULL; if (!cbtn) cbtn = new Card(NULL, 10, 10);
      cbtn->Resize(curBtnW, curBtnH);
      gfx::PushMatrix(); gfx::Translatef((float)curBtnX, (float)curBtnY, 0);
      gfx::Color4fv(TL_VERDE_LINEA); cbtn->RenderObject(false); // relleno verde
      gfx::Color4fv(TL_VERDE_LINEA); cbtn->RenderBorder(false); // borde verde redondeado (mismo color -> sin anillo negro)
      gfx::PopMatrix(); }
    SetCol(ColorID::blanco, 1.0f);
    TextCentrado(px, numY + (barH2 - LetterHeightGS)/2, curTxt);

    // ---------------- PANEL del dope sheet: TAPA el playhead/recuadro (van detras) ----------------
    RenderDopePanel();
    // ---------------- la CAJA de seleccion (si la armo este timeline) ----------------
    if (g_tlCaja == this && BoxSelectActivo()){
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
        BoxSelectDibujar(x, y, width, height);
        gfx::Enable(gfx::Texture2D); gfx::Enable(gfx::Blend); gfx::BlendAlpha();   // la caja los apaga: los iconos de la barra los necesitan
    }
    // ROTAR / ESCALAR keyframes: la linea punteada verde del PIVOTE al mouse, como en el 3D. El pivote de los
    // handles es su keyframe; en el dope sheet no hay eje de valor -> la linea va a la altura del mouse.
    if (g_dopeMov && modo == TL_MODO_CURVAS && g_dopeMovEje != DOPE_EJE_LIBRE){
        // la LINEA DEL EJE al que esta limitado el transform (X = el tiempo, roja; Y = el valor, verde), de lado a
        // lado del strip y pasando por el pivote, como la linea del eje del 3D
        const float pf = g_dopeHXform ? (float)g_dopeHOrig.frame : g_dopePivotF;
        const float pv = g_dopeHXform ? g_dopeHOrig.value : g_dopePivotV;
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray); gfx::Enable(gfx::Blend); gfx::BlendAlpha();
        gfx::Enable(gfx::ScissorTest);
        gfx::Scissor(x + panelW, W3dPantallaAlto - (y + height), width - panelW, height - stripY);
        if (g_dopeMovEje == DOPE_EJE_X){ gfx::Color4f(TL_COL_EJE[0][0], TL_COL_EJE[0][1], TL_COL_EJE[0][2], 0.9f);
                                         const float yy = ValueToY(pv); FillRect(panelW, (int)yy, width - panelW, GlobalScale); }
        else                           { gfx::Color4f(TL_COL_EJE[1][0], TL_COL_EJE[1][1], TL_COL_EJE[1][2], 0.9f);
                                         const float xx = FrameToX(pf); FillRect((int)xx, stripY, GlobalScale, height - stripY); }
        gfx::Disable(gfx::ScissorTest);
        gfx::Enable(gfx::Texture2D); gfx::EnableArray(gfx::TexCoordArray);
        if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    }
    if (g_dopeMov && (g_dopeMovModo == DOPE_ROT || g_dopeMovModo == DOPE_ESC)){
        extern void DibujarLineaPunteada2D(float ax, float ay, float bx, float by);
        const float pf = g_dopeHXform ? (float)g_dopeHOrig.frame : g_dopePivotF;
        const float pv = g_dopeHXform ? g_dopeHOrig.value : g_dopePivotV;
        const float mxl = (float)(lastMx - x), myl = (float)(lastMy - y);
        DibujarLineaPunteada2D(FrameToX(pf), modo == TL_MODO_CURVAS ? ValueToY(pv) : myl, mxl, myl);
        gfx::Enable(gfx::Blend); gfx::BlendAlpha();
        if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    }

    // ---------------- barra del viewport ----------------
    // Durante un transform de keyframes ('g'/'s'/'r') la barra de menu se reemplaza por el readout (Move/Scale/
    // Rotate + eje) con RenderBarraInfo, IGUAL que el 3D y el 2D. Sin transform: barra normal (transporte + campos).
    gfx::EnableArray(gfx::TexCoordArray);
    if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    std::string ttBarra = DopeTextoTransform();
    if (!ttBarra.empty()) {
        RenderBarraInfo(ttBarra);
    } else {
        RenderBar();

        // glyphs vectoriales ENCIMA de los botones de transporte (sin textura, centrados en el boton cuadrado)
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
        bool pfwd = (PlayAnimation && AnimPlayDir>0), prev = (PlayAnimation && AnimPlayDir<0);
        SetCol(ColorID::blanco, 1.0f);
        for (int i=0;i<8;i++){
            Button* b = btnT[i]; if (!b->visible || b->sx < -9000) continue;
            int gx = b->sx - x, gy = b->sy - y;
            bool pausa = (i==3 && prev) || (i==4 && pfwd);
            DrawGlyph(gx, gy, b->width, b->height, i, pausa);
        }
    }

    // la TOOLBAR de abajo (interpolacion | handles | ease), encima del strip
    gfx::Enable(gfx::Texture2D); gfx::EnableArray(gfx::TexCoordArray); gfx::Enable(gfx::Blend); gfx::BlendAlpha();
    if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    RenderToolbar();
    // borde del viewport (verde si activo)
    gfx::Enable(gfx::Texture2D); gfx::EnableArray(gfx::TexCoordArray);
    if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    DibujarBordes(this);
}

// ================================================================== EDITOR DE CURVAS
// HANDLE agarrado: (fila, frame, lado). El handle APARECE al SELECCIONAR el keyframe y arrastrarlo curva el tramo.
static bool g_hOn = false;          // hay un handle agarrado
// CTRL mientras se arrastra: el handle no cambia de DIRECCION (queda alineado con como venia la curva), solo de
// largo. La direccion es la que tenia al agarrarlo (despues del bakeado: la que se estaba viendo)
static bool  g_hDirOk = false;
static float g_hDirF = 0.0f, g_hDirV = 0.0f;
static std::string g_hOwner;        // curva del keyframe dueño del handle
static int  g_hProp = -1, g_hComp = -1, g_hFrame = 0;
static bool g_hSalida = true;       // true = handle de SALIDA (a la derecha); false = de ENTRADA
// CURSOR VIRTUAL del arrastre del handle. El cursor REAL se envuelve de un borde al otro (el warp corre porque el
// boton esta apretado), asi que la posicion real PEGA UN SALTO. Si el arrastre la mirara, el handle saltaria con
// ella: arrastrando para ABAJO, al llegar al borde el cursor aparece ARRIBA y el handle se iba para arriba.
// El virtual solo ACUMULA dx/dy, que CheckWarpMouseInViewport pone en 0 justo en el frame del salto -> el
// arrastre sigue derecho y no se entera. Mismo mecanismo que el transform de keyframes (g_dopeVirt*).
// ARRASTRE DIRECTO de keyframes: clickear uno y arrastrarlo lo mueve, sin apretar 'g' (igual que los handles).
// El transform es el MISMO ('g'): recien arranca al pasar un umbral, asi un click limpio solo selecciona y no
// mueve nada por un pixel de temblor.
static bool g_dopeDragPend = false;   // se clickeo un keyframe: si se arrastra, arranca el move
static int  g_dopeDragX0 = 0, g_dopeDragY0 = 0;
static int  g_hVirtX = 0, g_hVirtY = 0;
static bool g_hVirtPrimero = false; // el 1er motion: dx/dy son de ANTES de agarrar el handle -> se ignoran

bool Timeline::HandleArrastrando() const { return g_hOn; }
bool Timeline::HandleEsSalida() const { return g_hSalida; }
AnimProperty* Timeline::CurvaDeFila(const DopeRow& d, bool editar) const {
    if (d.propId < 0) return NULL;                       // fila padre / summary: no es una curva
    return DopeResolverProp(d.ownerKey, d.propId, d.compId, editar);
}

// Posicion del handle del keyframe i, en coords LOCALES del timeline. El handle es un PUNTO (offset dF/dV desde
// el keyframe): quien decide donde cae es HandleEfectivo del core, o sea el MISMO que usa la evaluacion -> el
// handle que agarras es exactamente el que curva la animacion.
void Timeline::HandlePos(const AnimProperty* ap, size_t i, bool salida, float& hx, float& hy) const {
    float dF, dV;
    ap->HandleEfectivo(i, salida, dF, dV);
    const keyFrame& kf = ap->keyframes[i];
    hx = FrameToX((float)kf.frame + dF);
    hy = ValueToY(kf.value + dV);
    // Piso EN PIXELES para poder agarrarlo: con keyframes pegados el brazo mide unos px y los dos handles se
    // superponen. Solo corre el DIBUJO a lo largo de su propia direccion; el dato no se toca.
    float kx = FrameToX((float)kf.frame), ky = ValueToY(kf.value);
    float ex = hx - kx, ey = hy - ky, L = sqrtf(ex*ex + ey*ey);
    const float minPx = (float)(GlobalScale*14);
    if (L < 0.001f){ hx = kx + (salida ? minPx : -minPx); hy = ky; return; }
    if (L < minPx){ hx = kx + ex/L*minPx; hy = ky + ey/L*minPx; }
}


// Hay handle de este lado? SOLO si el tramo de ese lado es BEZIER: un tramo LINEAL (o constante) es una recta y no
// tiene nada que ajustar, asi que no se le dibuja handle. Para curvar se usa el menu Interpolation > Bezier.
// Cada handle pertenece a SU tramo, y el tramo lo manda el keyframe IZQUIERDO:
//   handle de SALIDA de K  -> tramo [K, K+1]   -> manda K
//   handle de ENTRADA de K -> tramo [K-1, K]   -> manda K-1
static bool HandleVisible(const AnimProperty* ap, size_t i, bool salida){
    const std::vector<keyFrame>& k = ap->keyframes;
    if (salida) return (i+1 < k.size()) && k[i].Interpolation == KfBezier;
    return (i > 0) && k[i-1].Interpolation == KfBezier;
}

// Una fila aporta una CURVA al dibujo? (es un canal Y tiene el ojo prendido). La comparten el render y el
// contador de costo: cuando cada uno filtraba por su lado, agregar el ojo a uno solo hizo que el contador midiera
// curvas que el render ya no dibujaba.
static bool FilaEsCurvaVisible(const Timeline::DopeRow& d){ return d.propId >= 0 && !d.oculto; }

// Arma el TRAZO de UNA curva en g_curvaBuf (coords locales). Devuelve la cantidad de VERTICES.
// Recorre por TRAMOS, no por pixel: un tramo lineal o constante queda definido por sus extremos y no necesita ni
// una muestra; solo el bezier se subdivide, y solo el pedazo que se ve.
int Timeline::CurvaTrazo(const AnimProperty* ap, float w){
    g_curvaBuf.clear();
    if (!ap || ap->keyframes.empty()) return 0;
    const std::vector<keyFrame>& K = ap->keyframes;
    const size_t n = K.size();
    const int x0 = panelW, x1 = width;
    const float fVisL = XToFrame((float)x0), fVisR = XToFrame((float)x1); // rango de frames VISIBLE
    const float yTop = (float)stripY, yBot = (float)height;

    // --- cola IZQUIERDA: antes del 1er keyframe la curva vale constante (Eval clampea)... salvo con el modificador
    //     REPETIR: ahi se muestrea EvalF (vuelta tras vuelta, cada ~3 px) hasta el borde de la vista ---
    const float pasoCiclo = (pxPerFrame > 0.01f) ? (3.0f * (float)GlobalScale) / pxPerFrame : 1.0f;
    if (fVisL < (float)K[0].frame && ap->cicloAntes != KfCicloNo && n > 1){
        float px_ = FrameToX((float)K[0].frame), py_ = ValueToY(K[0].value);
        for (float f = (float)K[0].frame - pasoCiclo; ; f -= pasoCiclo){
            if (f < fVisL) f = fVisL;
            const float qx = FrameToX(f), qy = ValueToY(ap->EvalF(f, 0.0f));
            QuadLinea(g_curvaBuf, qx, qy, px_, py_, w);
            px_ = qx; py_ = qy;
            if (f <= fVisL) break;
        }
    } else if (fVisL < (float)K[0].frame){
        float yv = ValueToY(K[0].value);
        if (yv >= yTop-1.0f && yv <= yBot+1.0f)      // horizontal: se ve o no, no hay caso intermedio
            QuadLinea(g_curvaBuf, (float)x0, yv, FrameToX((float)K[0].frame), yv, w);
    }
    // --- TRAMOS ---
    for (size_t i=1; i<n; i++){
        const keyFrame& a = K[i-1]; const keyFrame& b = K[i];
        if ((float)b.frame < fVisL || (float)a.frame > fVisR) continue;   // CULL en X: el tramo no toca la vista
        const int span = b.frame - a.frame;
        // CULL en Y. OJO: NO alcanza con mirar si los keyframes se ven — un tramo entre dos keyframes que estan
        // los dos fuera de pantalla (uno muy arriba y otro muy abajo) CRUZA la vista y SI se ve. Por eso el
        // recorte se hace con el rango de valores que el tramo puede TOMAR:
        //   lineal/constante -> entre los dos valores;
        //   bezier -> la CASCARA CONVEXA de sus 4 puntos de control (una bezier nunca se sale de ahi), asi el
        //             sobrepico tampoco se pierde.
        float lo, hi;
        if (a.Interpolation == KfBezier && span > 0){
            float aDF, aDV, bDF, bDV;
            ap->HandleEfectivo(i-1, true,  aDF, aDV);
            ap->HandleEfectivo(i,   false, bDF, bDV);
            lo = a.value; hi = a.value;
            const float cs[3] = { a.value + aDV, b.value + bDV, b.value };   // los otros 3 puntos de control
            for (int c=0;c<3;c++){ if (cs[c]<lo) lo=cs[c]; if (cs[c]>hi) hi=cs[c]; }
        } else { lo = (a.value<b.value)?a.value:b.value; hi = (a.value>b.value)?a.value:b.value; }
        // easing con efecto (Back / Elastic) se pasa de los dos valores: margen para no recortarlo
        if (KfEsEasing(a.Interpolation)){ float m = (hi - lo) * 0.5f; lo -= m; hi += m; }
        // el rango [lo..hi] mapea a Y invertido: hi va arriba
        if (ValueToY(hi) > yBot || ValueToY(lo) < yTop) continue;         // todo el tramo queda fuera: no se dibuja
        float ax = FrameToX((float)a.frame), bx = FrameToX((float)b.frame);
        float ay = ValueToY(a.value), by = ValueToY(b.value);
        if (a.Interpolation == KfConstant){
            QuadLinea(g_curvaBuf, ax, ay, bx, ay, w);                    // meseta: se mantiene...
            QuadLinea(g_curvaBuf, bx, ay, bx, by, w);                    // ...y PUM, cambia justo en el frame de b
        } else if (a.Interpolation != KfBezier && !KfEsEasing(a.Interpolation)){
            QuadLinea(g_curvaBuf, ax, ay, bx, by, w);                    // recta: con los extremos alcanza
        } else if (KfEsEasing(a.Interpolation)){
            // EASING: muestreado cada ~3 px (no por frame): un rebote o un resorte entre dos keyframes cercanos
            // tiene mas detalle que un frame, y la forma tiene que verse para saber que curva es
            const float pasoF = (pxPerFrame > 0.01f) ? (3.0f * (float)GlobalScale) / pxPerFrame : 1.0f;
            float px_ = ax, py_ = ay;
            for (float f = (float)a.frame + pasoF; f < (float)b.frame; f += pasoF){
                const float t = (f - (float)a.frame) / (float)span;
                float qx = FrameToX(f), qy = ValueToY(a.value + (b.value - a.value) * KfEasing(a.Interpolation, t));
                QuadLinea(g_curvaBuf, px_, py_, qx, qy, w);
                px_ = qx; py_ = qy;
            }
            QuadLinea(g_curvaBuf, px_, py_, bx, by, w);
        } else {
            // BEZIER: se muestrea FRAME A FRAME y se unen con rectas. No hace falta mas resolucion que esa: la
            // animacion solo pisa frames enteros, asi que esta linea ES literalmente lo que va a correr.
            // (Si el zoom es tan chico que un frame no llega a un pixel, se saltean frames: mas no se veria.)
            int paso = 1;
            if (pxPerFrame < 1.0f){ paso = (int)(1.0f/pxPerFrame); if (paso < 1) paso = 1; }
            float px_ = ax, py_ = ay;
            for (int f = a.frame + paso; f < b.frame; f += paso){
                float qx = FrameToX((float)f), qy = ValueToY(ap->EvalBezier(i, (float)f));
                QuadLinea(g_curvaBuf, px_, py_, qx, qy, w);
                px_ = qx; py_ = qy;
            }
            QuadLinea(g_curvaBuf, px_, py_, bx, by, w);                  // ultimo tramo hasta el keyframe
        }
    }
    // --- cola DERECHA: despues del ultimo keyframe vuelve a ser constante (salvo REPETIR, como la izquierda) ---
    if (fVisR > (float)K[n-1].frame && ap->cicloDespues != KfCicloNo && n > 1){
        float px_ = FrameToX((float)K[n-1].frame), py_ = ValueToY(K[n-1].value);
        for (float f = (float)K[n-1].frame + pasoCiclo; ; f += pasoCiclo){
            if (f > fVisR) f = fVisR;
            const float qx = FrameToX(f), qy = ValueToY(ap->EvalF(f, 0.0f));
            QuadLinea(g_curvaBuf, px_, py_, qx, qy, w);
            px_ = qx; py_ = qy;
            if (f >= fVisR) break;
        }
    } else if (fVisR > (float)K[n-1].frame){
        float yv = ValueToY(K[n-1].value);
        if (yv >= yTop-1.0f && yv <= yBot+1.0f)
            QuadLinea(g_curvaBuf, FrameToX((float)K[n-1].frame), yv, (float)x1, yv, w);
    }
    return (int)(g_curvaBuf.size()/2);
}
// suma de vertices del trazo de TODAS las curvas, sin dibujar: mide el trabajo por frame (lo usa 'curvaperf')
long long Timeline::CurvaTrazoCosto(){
    long long t = 0;
    for (size_t r=0; r<dopeRows.size(); r++){
        const DopeRow& d = dopeRows[r]; if (!FilaEsCurvaVisible(d)) continue;
        AnimProperty* ap = DopeResolverProp(d.ownerKey, d.propId, d.compId, false); if (!ap) continue;   // solo mide
        t += CurvaTrazo(ap, (float)GlobalScale*0.5f);
    }
    return t;
}

static void TramoPuntos(const Timeline* t, const AnimProperty* ap, size_t i, std::vector<float>& xy);   // (abajo)
void Timeline::RenderCurvas(){
    gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray); gfx::Disable(gfx::Blend);
    const int x0 = panelW, x1 = width;
    if (x1 <= x0) return;

    // ---- linea del CERO (el centro) + rejilla de valores ----
    { int stepPx = GlobalScale*22;                      // separacion minima entre lineas de valor
      // paso "redondo" en unidades de valor segun el zoom vertical
      const float pasos[] = {0.01f,0.02f,0.05f,0.1f,0.2f,0.5f,1.0f,2.0f,5.0f,10.0f,25.0f,50.0f,100.0f,250.0f,500.0f,1000.0f};
      float paso = 1000.0f;
      for (int i=0;i<16;i++) if (pasos[i]*pxPerUnit >= (float)stepPx){ paso = pasos[i]; break; }
      float vTop = YToValue((float)stripY), vBot = YToValue((float)height);
      float v0 = floorf(vBot/paso)*paso;
      for (float v = v0; v <= vTop + paso*0.5f; v += paso){
          float ly = ValueToY(v);
          if (ly < stripY || ly > height) continue;
          bool cero = (v > -paso*0.001f && v < paso*0.001f);
          if (cero) SetCol(ColorID::grisLinea, 1.0f);      // el CERO va marcado (es el centro)
          else      gfx::Color4f(0.16f,0.16f,0.16f,1.0f);
          FillRect(x0, (int)ly, x1-x0, cero ? GlobalScale : (GlobalScale>1?GlobalScale/2:1));
      } }

    // ---- una CURVA por canal, con el color de SU EJE (X rojo / Y verde / Z azul) ----
    // Se recorre por TRAMOS (entre keyframe y keyframe), NO pixel por pixel: un tramo lineal o constante queda
    // definido por sus extremos y no necesita ni una muestra; solo el bezier se subdivide. Todo el trazo se arma en
    // UN vertex array y se dibuja de UNA. (Antes era un FillRect y un EvalF -que escanea los keyframes- por cada
    // pixel de cada curva: miles de draw calls por frame -> el rendimiento se caia a pedazos.)
    // El recorte lo hace el SCISSOR: los tramos que sobran quedan fuera y GL no los rasteriza.
    { int lh = height - stripY; if (lh < 0) lh = 0;
      gfx::Enable(gfx::ScissorTest);
      gfx::Scissor(x + x0, W3dPantallaAlto - y - height, x1 - x0, lh); }
    const float fVisL = XToFrame((float)x0), fVisR = XToFrame((float)x1); // rango de frames VISIBLE
    const float yTop = (float)stripY, yBot = (float)height;
    static AnimProperty s_vtxTmp;   // buffer reusado para la curva temporal de vertices
    for (size_t r=0; r<dopeRows.size(); r++){
        const DopeRow& d = dopeRows[r];
        if (!FilaEsCurvaVisible(d)) continue;             // solo canales, y solo con el ojo prendido
        // canal VERTEX: su "curva" es la rampa por indice (ver VertexCanalTemp). Dibujar no edita.
        AnimProperty* ap = VertexCanalTemp(d.ownerKey, s_vtxTmp) ? &s_vtxTmp
                         : DopeResolverProp(d.ownerKey, d.propId, d.compId, false);
        if (!ap || ap->keyframes.empty()) continue;
        const std::vector<keyFrame>& K = ap->keyframes;
        const size_t n = K.size();
        // color: el del EJE (X rojo / Y verde / Z azul), salvo la fila Normales de la vertex anim (violeta)
        // (el color es el del eje que se VE: en la posicion, pos.z es la "Y" verde y pos.y la "Z" azul)
        const int ejeVisto = (d.propId == AnimPosition && d.compId == AnimY) ? AnimZ
                           : (d.propId == AnimPosition && d.compId == AnimZ) ? AnimY : d.compId;
        const float* col = (DopeVertexCapaDe(d.ownerKey)==2) ? TL_COL_NORMALES : ColorDeEje(ejeVisto);
        // Una curva SIN ningun keyframe seleccionado se dibuja al 50%: las que estas editando resaltan y el resto
        // queda de fondo (con 18 canales encimados, si van todas al 100% no se entiende nada).
        bool tieneSel = false;
        for (size_t i=0;i<n && !tieneSel;i++)
            tieneSel = g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, K[i].frame)) > 0;
        const float alfa = tieneSel ? 1.0f : 0.5f;
        float w = (tieneSel ? (float)GlobalScale : (float)GlobalScale*0.6f) * 0.5f; // media-anchura del trazo
        CurvaTrazo(ap, w);                       // arma el trazo en g_curvaBuf (con su culling)
        // UNA sola llamada de dibujo para toda la curva
        if (!g_curvaBuf.empty()){
            gfx::Enable(gfx::Blend); gfx::BlendAlpha();   // el 50% necesita alpha
            gfx::Color4f(col[0], col[1], col[2], alfa);
            gfx::VertexPointer2f(0, &g_curvaBuf[0]);
            gfx::DrawTrianglesArray((int)(g_curvaBuf.size()/2));
        }
        // ---- TRAMOS ELEGIDOS (los dos keyframes de la punta seleccionados: click en la linea, caja) en BLANCO ----
        if (tieneSel && n > 1){
            static std::vector<float> s_tramo, s_quad;
            s_quad.clear();
            const float wb = (float)GlobalScale * 0.6f;
            for (size_t i=1;i<n;i++){
                if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, K[i-1].frame)) ||
                    !g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, K[i].frame))) continue;
                const float xa = FrameToX((float)K[i-1].frame), xb = FrameToX((float)K[i].frame);
                if (xb < x0 || xa > x1) continue;                    // fuera de pantalla
                TramoPuntos(this, ap, i, s_tramo);
                for (size_t k=2; k+1<s_tramo.size(); k+=2){
                    const float ax = s_tramo[k-2], ay = s_tramo[k-1], bx = s_tramo[k], by = s_tramo[k+1];
                    float nx = -(by - ay), ny = bx - ax; const float l = sqrtf(nx*nx + ny*ny);
                    if (l < 1e-4f) continue;
                    nx = nx / l * wb; ny = ny / l * wb;
                    const float q[12] = { ax+nx,ay+ny, bx+nx,by+ny, bx-nx,by-ny,  ax+nx,ay+ny, bx-nx,by-ny, ax-nx,ay-ny };
                    s_quad.insert(s_quad.end(), q, q + 12);
                }
            }
            if (!s_quad.empty()){
                SetCol(ColorID::blanco, 1.0f);
                gfx::VertexPointer2f(0, &s_quad[0]);
                gfx::DrawTrianglesArray((int)(s_quad.size()/2));
            }
        }

        // ---- KEYFRAMES de la curva (los que se ven: culling en X y en Y) ----
        for (size_t i=0;i<n;i++){
            const keyFrame& kf = K[i];
            float kx = FrameToX((float)kf.frame), ky = ValueToY(kf.value);
            if (kx < x0-GlobalScale*4 || kx > x1+GlobalScale*4) continue;
            if (ky < yTop-GlobalScale*4 || ky > yBot+GlobalScale*4) continue;
            bool sel = g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, kf.frame)) > 0;
            if (sel) SetCol(ColorID::accent, 1.0f); else gfx::Color4f(col[0], col[1], col[2], alfa);
            Diamond(kx, ky, (float)GlobalScale*(sel?3.8f:2.8f));
            if (!sel) continue;
            // ---- HANDLES: solo del keyframe SELECCIONADO, y solo del lado cuyo tramo es BEZIER (una recta no
            //      tiene nada que ajustar). El canal VERTEX tambien: sus handles viven en el VertexFrame. ----
            for (int lado=0; lado<2; lado++){
                bool salida = (lado==1);
                if (!HandleVisible(ap, i, salida)) continue;
                float hx, hy; HandlePos(ap, i, salida, hx, hy);
                gfx::Color4f(col[0], col[1], col[2], 1.0f);
                LineaFina(kx, ky, hx, hy);                     // brazo
                SetCol(ColorID::blanco, 1.0f);
                Diamond(hx, hy, (float)GlobalScale*2.2f);      // agarradera
            }
        }
    }
    gfx::Disable(gfx::ScissorTest);
}

// ------------------------------------------------------------------ DOPE SHEET: panel (nombres/flechas/iconos)
// Rect del "Summary" FLOTANTE (cuando el panel esta oculto). Lo comparten el RENDER y el CLICK: asi no se
// pueden desincronizar (era el bug: se dibujaba en un lado y el click se testeaba en otro).
bool Timeline::DopeRectFlotante(int& rx, int& ry, int& rw, int& rh) const {
    if (dopeRows.empty() || panelW > 0) return false;
    rx = 0; ry = stripY;   // DEBAJO de los numeros (como todas las filas)
    rw = marginGS + IconSizeGS + gapGS + UTF8_Largo(dopeRows[0].nombre)*CharacterWidthGS + marginGS;
    rh = rowH;
    return true;
}

// X (local) de la columna de OJOS: pegada al borde derecho del panel, igual que en el Outliner.
int Timeline::DopeOjoX() const { return panelW - GlobalScale - IconSizeGS - marginGS; }

// SCROLL HORIZONTAL del panel: el texto visible va de 0 a la columna de ojos; lo que sobra se scrollea
int Timeline::PanelMaxX() const {
    int vis = DopeOjoX() - gapGS; if (vis < 1) vis = 1;
    int m = vis - panelContW; return m < 0 ? m : 0;
}
// la barra horizontal del panel (abajo, del ancho del texto). tx/tw = el agarre. false = no hace falta
bool Timeline::PanelBarraH(int& bx, int& by, int& bw, int& bh, int& tx, int& tw) const {
    if (panelW <= 0 || PanelMaxX() >= 0) return false;
    bh = GlobalScale*4; bx = 0; by = height - bh - GlobalScale; bw = DopeOjoX() - gapGS;
    if (bw < GlobalScale*4) return false;
    tw = bw * bw / (panelContW > 1 ? panelContW : 1); if (tw < GlobalScale*6) tw = GlobalScale*6; if (tw > bw) tw = bw;
    tx = bx + (int)((float)(bw - tw) * (float)panelPosX / (float)PanelMaxX());
    return true;
}
// banda de unos px alrededor del separador del panel (arrastrarla cambia su ancho)
bool Timeline::PanelEnBorde(int lx, int ly) const {
    return panelW > 0 && ly >= numY && ly < height && (float)lx >= (float)panelW - RadioLinea() && (float)lx <= (float)panelW + RadioLinea();
}

void Timeline::RenderDopePanel(){
    if (dopeRows.empty()) return;
    // PANEL OCULTO (Summary plegado): el timeline ocupa TODO y el texto "Summary" queda FLOTANDO encima,
    // sin fondo ni columna. Solo se dibuja la flecha + el nombre de la fila 0.
    { int rx, ry, rw, rh;
      if (DopeRectFlotante(rx, ry, rw, rh)){   // panel OCULTO: solo el "Summary" flotante
        // RECTANGULO de fondo: sin el, el texto flotante se MEZCLA con los numeros de frame.
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray); gfx::Disable(gfx::Blend);
        gfx::Color4f(0.10f,0.10f,0.10f,1.0f); FillRect(rx, ry, rw, rh);
        SetCol(ColorID::grisLinea, 1.0f); FillRect(rx+rw, ry, GlobalScale, rh); // borde derecho (lo separa del strip)
        gfx::Enable(gfx::Texture2D); gfx::Enable(gfx::Blend); gfx::BlendAlpha(); gfx::EnableArray(gfx::TexCoordArray);
        if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
        gfx::PushMatrix(); gfx::Translatef((float)(rx + marginGS), (float)(ry + (rh - IconSizeGS)/2), 0);
        SetCol(ColorID::blanco, 1.0f);
        W3dDrawStrip4(IconMesh, IconsUV[(size_t)IconType::arrowRight]->uvs); // plegado
        gfx::Translatef((float)(IconSizeGS + gapGS), 0, 0);
        RenderBitmapText(dopeRows[0].nombre);
        gfx::PopMatrix();
        return;
    } }
    // fondo del panel (mas oscuro que la tarjeta) + separador vertical. Arranca en numY: 2 COLUMNAS que empiezan
    // a la misma altura (la 1ra fila, Summary, queda en la banda de los numeros).
    gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray); gfx::Disable(gfx::Blend);
    gfx::Color4f(0.13f,0.13f,0.13f,1.0f); FillRect(0, numY, panelW, height - numY);
    // separador: BLANCO con el mouse encima, VERDE mientras se arrastra (asi se sabe que se puede agarrar)
    if (panelResize)          { SetCol(ColorID::accent, 1.0f); FillRect(panelW - GlobalScale*2, numY, GlobalScale*2, height - numY); }
    else if (panelBordeHover) { SetCol(ColorID::blanco, 1.0f); FillRect(panelW - GlobalScale*2, numY, GlobalScale*2, height - numY); }
    else { SetCol(ColorID::grisLinea, 1.0f); FillRect(panelW - GlobalScale, numY, GlobalScale, height - numY); }
    // fondo de la fila del SUMMARY dentro del panel (que quede continuo con el strip)
    { int ry0 = stripY + PosY; if (ry0 + rowH > stripY && ry0 < height){
        gfx::Color4f(0.10f,0.10f,0.10f,1.0f); FillRect(0, ry0<numY?numY:ry0, panelW - GlobalScale, rowH); } }

    // fondo de fila: SELECCIONADA (accent) / HOVER (mas clara). Fase sin textura.
    // (la parte del STRIP de la fila seleccionada la pinta el bloque de keyframes -> el renglon cruza TODO)
    for (size_t r=0; r<dopeRows.size(); r++){
        int ry = stripY + (int)r*rowH + PosY;
        if (ry + rowH < stripY || ry > height) continue;
        bool sel = g_dopeRowSel.count(dopeRows[r].claveFila) > 0;
        if (!sel && (int)r != hoverRow) continue;
        if (sel) SetCol(ColorID::accentDark, 1.0f); else gfx::Color4f(0.22f,0.22f,0.22f,1.0f); // hover
        int ry0 = ry < numY ? numY : ry, ry1 = ry + rowH; if (ry1 > height) ry1 = height;
        if (ry1 > ry0) FillRect(0, ry0, panelW - GlobalScale, ry1 - ry0);
    }

    // filas: [flecha si desplegable] [icono si tiene] nombre. MISMAS distancias que el Outliner; los huecos NO se
    // reservan si la fila no tiene flecha/icono -> "Object Transforms" y los canales quedan mas a la izquierda.
    gfx::Enable(gfx::Texture2D); gfx::Enable(gfx::Blend); gfx::BlendAlpha(); gfx::EnableArray(gfx::TexCoordArray);
    if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    // SCISSOR: el texto largo se RECORTA en el borde del panel (antes se derramaba sobre los keyframes).
    // El scissor va en coords de ventana GL (origen abajo-izquierda); el viewport ya esta en (x, glY, w, h).
    // Termina ANTES de la columna de OJOS: el texto largo no se mete debajo de ellos.
    { int lw = DopeOjoX() - gapGS, lh = height - stripY;
      if (lw < 0) lw = 0; if (lh < 0) lh = 0;
      gfx::Enable(gfx::ScissorTest);
      gfx::Scissor(x, W3dPantallaAlto - y - stripY - lh, lw, lh); }
    for (size_t r=0; r<dopeRows.size(); r++){
        int ry = stripY + (int)r*rowH + PosY;
        if (ry + rowH < stripY || ry > height) continue;  // culling vertical
        DopeRow& d = dopeRows[r];
        gfx::PushMatrix();
        gfx::Translatef((float)(panelPosX + marginGS + d.nivel*(IconSizeGS+gapGS)), (float)ry, 0);
        if (d.tipo==3) SetCol(ColorID::grisUI, 1.0f); else SetCol(ColorID::blanco, 1.0f);
        // un canal de EJE (X / Y / Z de posicion, rotacion, escala): el texto con el color de su curva (el eje que
        // se VE, como en DibujarCurvas). Seleccionado va BLANCO: sobre el fondo verde el color se perderia.
        if (d.tipo==3 && (d.propId==AnimPosition || d.propId==AnimRotation || d.propId==AnimScale) && (d.compId==AnimX || d.compId==AnimY || d.compId==AnimZ)
            && DopeVertexCapaDe(d.ownerKey) < 0){
            if (g_dopeRowSel.count(d.claveFila)) SetCol(ColorID::blanco, 1.0f);
            else { const int ejeVisto = (d.propId == AnimPosition && d.compId == AnimY) ? AnimZ
                                      : (d.propId == AnimPosition && d.compId == AnimZ) ? AnimY : d.compId;
                   gfx::Color4fv(ColorDeEje(ejeVisto)); }
        }
        if (!d.claveDespliegue.empty()){
            W3dDrawStrip4(IconMesh, IconsUV[(size_t)(DopeColapsado(d.claveDespliegue) ? IconType::arrowRight : IconType::arrow)]->uvs);
            gfx::Translatef((float)(IconSizeGS + gapGS), 0, 0);
        }
        if (d.icono >= 0){
            W3dDrawStrip4(IconMesh, IconsUV[(size_t)d.icono]->uvs);
            gfx::Translatef((float)(IconSizeGS + gapGS), 0, 0);
        }
        RenderBitmapText(d.nombre);
        gfx::PopMatrix();
    }
    gfx::Disable(gfx::ScissorTest); // fin del recorte del texto del panel

    // ---- OJOS (mismo icono y misma columna que el Outliner): apagarlo oculta la curva ----
    for (size_t r=0; r<dopeRows.size(); r++){
        int ry = stripY + (int)r*rowH + PosY;
        if (ry + rowH < stripY || ry > height) continue;   // culling vertical
        const DopeRow& d = dopeRows[r];
        if (d.tiraFilas < 0) continue;   // (continuacion de una tira: el ojo es el de su fila)
        SetCol(ColorID::grisUI, d.oculto ? 0.5f : 1.0f);
        gfx::PushMatrix();
        gfx::Translatef((float)DopeOjoX(), (float)(ry + (rowH - IconSizeGS)/2), 0);
        W3dDrawStrip4(IconMesh, IconsUV[(size_t)(d.oculto ? IconType::hidden : IconType::visible)]->uvs);
        gfx::PopMatrix();
    }

    // ---- BARRA HORIZONTAL del panel (el texto no entra): agarre gris, verde arrastrando ----
    { int bx, by, bw, bh, tx, tw;
      if (PanelBarraH(bx, by, bw, bh, tx, tw)){
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray); gfx::Disable(gfx::Blend);
        gfx::Color4f(0.08f,0.08f,0.08f,1.0f); FillRect(bx, by, bw, bh);
        if (panelBarraH) SetCol(ColorID::accent, 1.0f); else SetCol(ColorID::grisUI, 1.0f);
        FillRect(tx, by + GlobalScale, tw, bh - GlobalScale*2);
        gfx::Enable(gfx::Texture2D); gfx::Enable(gfx::Blend); gfx::BlendAlpha(); gfx::EnableArray(gfx::TexCoordArray);
        if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
      } }
    // ---- BARRA DE SCROLL: la compartida del editor (hover/agarre/arrastre/touch los da el ruteo generico) ----
    if (scrollY) DibujarScrollbar(this);
}

// click en el PANEL: sobre la FLECHA pliega/despliega; sobre el resto SELECCIONA la fila (shift = agrega).
bool Timeline::DopeClickPanel(int mx, int my){
    if (dopeRows.empty()) return false;
    int lx = mx - x, ly = my - y;
    // PANEL OCULTO (Summary plegado): el click sobre el texto FLOTANTE lo vuelve a DESPLEGAR. Usa el MISMO rect
    // que dibuja RenderDopePanel (DopeRectFlotante) -> imposible que se desincronicen.
    { int rx, ry, rw, rh;
      if (DopeRectFlotante(rx, ry, rw, rh)){
        if (lx >= rx && lx < rx+rw && ly >= ry && ly < ry+rh){ g_dopeColapsado.erase("summary"); g_redraw = true; return true; }
        return false;
      } }
    // BORDE del panel: arrastrarlo cambia su ancho (banda de unos px alrededor del separador)
    if (PanelEnBorde(lx, ly)){ panelResize = true; g_redraw = true; return true; }
    // BARRA HORIZONTAL del panel: el agarre se arrastra; afuera del agarre, salta ahi
    { int bx, by, bw, bh, tx, tw;
      if (PanelBarraH(bx, by, bw, bh, tx, tw) && lx >= bx && lx < bx+bw && ly >= by - GlobalScale && ly < by+bh+GlobalScale){
        if (lx < tx || lx >= tx + tw){
            int m = PanelMaxX(), rec = bw - tw;
            panelPosX = rec > 0 ? (int)((float)m * (float)(lx - bx - tw/2) / (float)rec) : 0;
            if (panelPosX < m) panelPosX = m; if (panelPosX > 0) panelPosX = 0;
        }
        panelBarraH = true; g_redraw = true; return true;
      } }
    if (lx < 0 || lx >= panelW || ly < stripY || ly >= height) return false;
    int r = (ly - stripY - PosY) / (rowH>0?rowH:1);
    if (r < 0 || r >= (int)dopeRows.size()) return true; // dentro del panel pero sin fila: igual consumido
    DopeRow& d = dopeRows[r];
    // OJO: primero, que esta pegado al borde derecho del panel (y ahi no hay ni flecha ni nombre)
    if (lx >= DopeOjoX() - gapGS){
        if (DopeOjoOff(d.claveFila)) g_dopeOjoOff.erase(d.claveFila); else g_dopeOjoOff.insert(d.claveFila);
        g_redraw = true; return true;
    }
    int xFlecha = panelPosX + marginGS + d.nivel*(IconSizeGS+gapGS);
    if (!d.claveDespliegue.empty() && lx >= xFlecha && lx < xFlecha + IconSizeGS + gapGS){
        if (DopeColapsado(d.claveDespliegue)) g_dopeColapsado.erase(d.claveDespliegue); else g_dopeColapsado.insert(d.claveDespliegue);
        g_redraw = true; return true;
    }
    if (!LShiftPressed) g_dopeRowSel.clear();
    if (!d.claveFila.empty()){
        if (LShiftPressed && g_dopeRowSel.count(d.claveFila)) g_dopeRowSel.erase(d.claveFila);
        else g_dopeRowSel.insert(d.claveFila);
    }
    DopeSelKeysDeFilasSel(); // clickear una fila SELECCIONA ESA CURVA: sus keyframes quedan elegidos (asi se le ven
                             // los handles y le sirven G/S/R). Una fila padre elige las curvas que cubre.
    if (g_dopeAutoFrame) DopeFrameSelected(); // ...y se encuadra sola (View > Auto frame)
    g_redraw = true; return true;
}

// sincroniza la seleccion de KEYFRAMES con la de FILAS: los keyframes de cada fila seleccionada (si es una fila
// padre, los de todos los canales que cuelgan de ella por prefijo del ownerKey).
void Timeline::DopeSelKeysDeFilasSel(){
    g_dopeKeySel.clear();
    for (size_t r=0;r<dopeRows.size();r++){
        const DopeRow& d = dopeRows[r];
        if (!g_dopeRowSel.count(d.claveFila)) continue;
        if (d.propId >= 0){                        // fila de CANAL: es una curva
            for (size_t i=0;i<d.keys.size();i++) g_dopeKeySel.insert(DopeKeyId(d.ownerKey,d.propId,d.compId,d.keys[i]));
        } else {                                   // fila PADRE (objeto / hueso / Summary): las curvas que cubre
            for (size_t k=0;k<dopeRows.size();k++){
                const DopeRow& c = dopeRows[k]; if (c.propId < 0) continue;
                if (!DopeCubre(d.ownerKey, c.ownerKey)) continue;
                for (size_t i=0;i<c.keys.size();i++) g_dopeKeySel.insert(DopeKeyId(c.ownerKey,c.propId,c.compId,c.keys[i]));
            }
        }
    }
}

// click en el STRIP sobre un ROMBO: lo selecciona (shift = agrega/saca). Si no cae en un rombo devuelve false
// para que el click siga siendo un SCRUB del frame (comportamiento de siempre).
bool Timeline::DopeClickStrip(int mx, int my){
    if (dopeRows.empty()) return false;
    int lx = mx - x, ly = my - y;
    if (lx < panelW || ly < stripY || ly >= height) return false;
    int r = (ly - stripY - PosY) / (rowH>0?rowH:1);
    if (r < 0 || r >= (int)dopeRows.size()) return false;
    const DopeRow& d = dopeRows[r];
    if (d.oculto) return false;                           // ojo apagado: no se puede clickear lo que no se ve
    // OJO: 'mejor' es un NUMERO DE FRAME (puede ser negativo): el "hubo hit" va
    // en un flag aparte, sino los keyframes en frames negativos no se podian clickear
    int mejor = 0; bool hayMejor = false; float mejorD = 1e9f;
    for (size_t i=0;i<d.keys.size();i++){ float dd = fabsf(FrameToX((float)d.keys[i]) - (float)lx); if (dd < mejorD){ mejorD=dd; mejor=d.keys[i]; hayMejor=true; } }
    if (!hayMejor || mejorD > RadioAgarre()) return false; // no cayo sobre un rombo
    if (!LShiftPressed) g_dopeKeySel.clear();
    if (d.propId >= 0){
        std::string id = DopeKeyId(d.ownerKey, d.propId, d.compId, mejor);
        if (LShiftPressed && g_dopeKeySel.count(id)) g_dopeKeySel.erase(id); else g_dopeKeySel.insert(id);
    } else {
        // fila PADRE / SUMMARY: marca los keyframes de ese frame de todos los canales que cubre (prefijo del ownerKey;
        // el summary tiene ownerKey vacio -> cubre todo)
        for (size_t k=0;k<dopeRows.size();k++){
            const DopeRow& c = dopeRows[k]; if (c.propId < 0) continue;
            if (!DopeCubre(d.ownerKey, c.ownerKey)) continue;
            for (size_t i=0;i<c.keys.size();i++) if (c.keys[i]==mejor) g_dopeKeySel.insert(DopeKeyId(c.ownerKey,c.propId,c.compId,mejor));
        }
    }
    // ...y queda listo para ARRASTRARLO (igual que en curvas): si el mouse se mueve, arranca el 'g' solo
    if (!g_dopeKeySel.empty()){ g_dopeDragPend = true; g_dopeDragX0 = mx; g_dopeDragY0 = my; }
    g_redraw = true; return true;
}

// ---- click en modo CURVAS: primero los HANDLES del keyframe seleccionado, despues los keyframes (en 2D: el
//      hit-test mira tiempo Y valor, no solo la columna del frame). ----
bool Timeline::CurvaClickStrip(int mx, int my){
    if (dopeRows.empty()) return false;
    int lx = mx - x, ly = my - y;
    if (lx < panelW || ly < stripY || ly >= height) return false;
    const float rHandle = RadioAgarre(), rKey = RadioAgarre();

    // 1) HANDLES (tienen prioridad: estan encima del keyframe y son mas chicos). Gana el MAS CERCANO, no el primero
    //    que entre en el radio: los dos handles de un keyframe estan a la misma distancia de el y con keyframes
    //    juntos se solapan -> si se cortaba en el primero, el de ENTRADA se comia siempre al de SALIDA.
    { float mejorD = 1e30f; bool hay = false;
      std::string bOwner; int bProp=-1, bComp=-1, bFrame=0; bool bSalida=true;
      static AnimProperty s_vtxTmp;
      for (size_t r=0; r<dopeRows.size(); r++){
        const DopeRow& d = dopeRows[r]; if (d.propId < 0 || d.oculto) continue;
        // canal VERTEX: sus handles se agarran sobre la curva temporal (valor = indice). Es solo el
        // hit-test: si se agarra un handle, el UndoKeyframesIniciar de abajo y HandleApply copian
        AnimProperty* ap = VertexCanalTemp(d.ownerKey, s_vtxTmp) ? &s_vtxTmp
                         : DopeResolverProp(d.ownerKey, d.propId, d.compId, false);
        if (!ap) continue;
        for (size_t i=0;i<ap->keyframes.size();i++){
            const keyFrame& kf = ap->keyframes[i];
            if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, kf.frame))) continue; // sin seleccionar no hay handle
            for (int lado=0; lado<2; lado++){
                bool salida = (lado==1);
                if (!HandleVisible(ap, i, salida)) continue;   // MISMA condicion que el dibujo: no se agarra lo que no se ve
                float hx, hy; HandlePos(ap, i, salida, hx, hy);   // y la MISMA posicion
                float ddx = hx-(float)lx, ddy = hy-(float)ly;
                if (fabsf(ddx) > rHandle || fabsf(ddy) > rHandle) continue;
                float dd = ddx*ddx + ddy*ddy;
                if (dd >= mejorD) continue;
                mejorD = dd; hay = true;
                bOwner = d.ownerKey; bProp = d.propId; bComp = d.compId; bFrame = kf.frame; bSalida = salida;
            }
        }
      }
      if (hay){
        g_hOn = true; g_hDirOk = false; g_hOwner = bOwner; g_hProp = bProp; g_hComp = bComp; g_hFrame = bFrame; g_hSalida = bSalida;
        g_hVirtX = mx; g_hVirtY = my;   // el cursor virtual arranca donde esta el real
        g_hVirtPrimero = true;
        UndoKeyframesIniciar();     // Ctrl+Z: curvar es editable/deshacible como todo lo demas
        return true;
      } }
    // 2) KEYFRAMES (hit-test 2D). Igual que los handles: gana el MAS CERCANO, no el primero que caiga en el radio.
    //    Con varias curvas encimadas, cortar en el primero hacia ganar siempre a la fila de mas arriba aunque su
    //    keyframe estuviera bastante mas lejos del mouse que el de otra.
    { float mejorD = 1e30f; bool hay = false; std::string bId;
      std::string bOwner; int bProp = -1, bComp = -1, bFrame = 0;
      static AnimProperty s_vtxTmp;
      for (size_t r=0; r<dopeRows.size(); r++){
        const DopeRow& d = dopeRows[r]; if (d.propId < 0 || d.oculto) continue;
        // canal VERTEX: hit-test sobre la curva temporal (valor = indice del keyframe). Elegir no edita.
        AnimProperty* ap = VertexCanalTemp(d.ownerKey, s_vtxTmp) ? &s_vtxTmp
                         : DopeResolverProp(d.ownerKey, d.propId, d.compId, false);
        if (!ap) continue;
        for (size_t i=0;i<ap->keyframes.size();i++){
            const keyFrame& kf = ap->keyframes[i];
            float kx = FrameToX((float)kf.frame), ky = ValueToY(kf.value);
            float ddx = kx-(float)lx, ddy = ky-(float)ly;
            if (fabsf(ddx) > rKey || fabsf(ddy) > rKey) continue;
            float dd = ddx*ddx + ddy*ddy;
            if (dd >= mejorD) continue;
            mejorD = dd; hay = true; bId = DopeKeyId(d.ownerKey, d.propId, d.compId, kf.frame);
            bOwner = d.ownerKey; bProp = d.propId; bComp = d.compId; bFrame = kf.frame;
        }
      }
      if (hay){
        if (!LShiftPressed) g_dopeKeySel.clear();
        if (LShiftPressed && g_dopeKeySel.count(bId)) g_dopeKeySel.erase(bId); else g_dopeKeySel.insert(bId);
        if (g_dopeKeySel.count(bId)){   // quedo elegido -> es el ACTIVO (el que muestra el panel de propiedades)
            g_dopeActOwner = bOwner; g_dopeActProp = bProp; g_dopeActComp = bComp; g_dopeActFrame = bFrame;
            g_dopeActHay = true;
            // ...y queda listo para ARRASTRARLO: si el mouse se mueve, arranca el 'g' solo (ver event_mouse_motion)
            g_dopeDragPend = true; g_dopeDragX0 = mx; g_dopeDragY0 = my;
        }
        g_redraw = true; return true;
      } }
    return false;
}

// arrastre del handle -> nueva PENDIENTE (valor por frame) desde la posicion del mouse. El tramo que toca pasa a
// BEZIER: agarrar un handle ES pedir una curva. OJO con de quien es el tramo: el que manda es el keyframe IZQUIERDO,
// asi que el handle de ENTRADA de K curva el tramo de K-1.
// Congela los handles CALCULADOS (Vector/Auto/AutoClamped) en los offsets guardados y pasa el keyframe a Aligned.
// Sin esto, tocar un handle automatico no haria nada: HandleEfectivo lo seguiria recalculando y el arrastre se
// perderia. Se congela lo que ya se estaba VIENDO, asi que el handle no salta al agarrarlo. Queda Aligned porque
// los automaticos ya son colineales: es el tipo que menos cambia lo que ves.
// CTRL arrastrando un handle: la DIRECCION con la que la curva viene del otro lado del keyframe. El handle de
// SALIDA sigue la tangente con la que la curva LLEGA (tramo anterior); el de ENTRADA, la opuesta a la tangente con
// la que SALE (tramo siguiente). Asi el keyframe queda liso: la curva sigue derecho, sin quiebre. Si del otro lado
// no hay tramo (la punta de la curva), se conserva la direccion que el handle ya tenia.
static void HandleDirCurva(AnimProperty* ap, size_t i, bool salida, float& dF, float& dV){
    const keyFrame& k = ap->keyframes[i];
    dF = salida ? k.outDF : k.inDF; dV = salida ? k.outDV : k.inDV;
    const size_t n = ap->keyframes.size();
    const float e = 0.01f;
    float tf = 0.0f, tv = 0.0f;
    if (salida && i > 0){
        const keyFrame& p = ap->keyframes[i-1];
        const float span = (float)(k.frame - p.frame); if (span <= 0.0f) return;
        if (p.Interpolation == KfBezier){ float a, b; ap->HandleEfectivo(i, false, a, b); tf = -a; tv = -b; }
        else if (KfEsEasing(p.Interpolation)){ tf = span * e; tv = (k.value - p.value) * (1.0f - KfEasing(p.Interpolation, 1.0f - e)); }
        else if (p.Interpolation == KfConstant){ tf = 1.0f; tv = 0.0f; }
        else { tf = span; tv = k.value - p.value; }
        if (tf > 1e-6f){ dF = tf; dV = tv; }
    } else if (!salida && i + 1 < n){
        const keyFrame& q = ap->keyframes[i+1];
        const float span = (float)(q.frame - k.frame); if (span <= 0.0f) return;
        if (k.Interpolation == KfBezier){ ap->HandleEfectivo(i, true, tf, tv); }
        else if (KfEsEasing(k.Interpolation)){ tf = span * e; tv = (q.value - k.value) * KfEasing(k.Interpolation, e); }
        else if (k.Interpolation == KfConstant){ tf = 1.0f; tv = 0.0f; }
        else { tf = span; tv = q.value - k.value; }
        if (tf > 1e-6f){ dF = -tf; dV = -tv; }
    }
}
static void HandleBakear(AnimProperty* ap, size_t i){
    keyFrame& kf = ap->keyframes[i];
    if (kf.handleType == HFree || kf.handleType == HAligned) return;
    float aDF, aDV, bDF, bDV;
    ap->HandleEfectivo(i, false, aDF, aDV);
    ap->HandleEfectivo(i, true,  bDF, bDV);
    kf.inDF = aDF; kf.inDV = aDV; kf.outDF = bDF; kf.outDV = bDV;
    kf.handleType = HAligned;
}
// ALIGNED: los dos lados quedan en la misma recta. El que NO se movio conserva su LARGO y solo se da vuelta para
// seguir la direccion del que si se movio.
static void HandleAlinear(keyFrame& kf, bool movioSalida){
    float mF = movioSalida ? kf.outDF : kf.inDF;
    float mV = movioSalida ? kf.outDV : kf.inDV;
    float L = sqrtf(mF*mF + mV*mV);
    if (L < 1e-6f) return;
    float oF = movioSalida ? kf.inDF : kf.outDF;
    float oV = movioSalida ? kf.inDV : kf.outDV;
    float oL = sqrtf(oF*oF + oV*oV);
    if (oL < 1e-6f) oL = L;                       // el otro no tenia largo: se le da el mismo
    float uF = -mF/L*oL, uV = -mV/L*oL;           // direccion OPUESTA, largo propio
    if (movioSalida){ kf.inDF = uF; kf.inDV = uV; } else { kf.outDF = uF; kf.outDV = uV; }
}

void Timeline::HandleApply(int mx, int my){
    if (!g_hOn) return;
    // canal VERTEX: la curva es la temporal (valor = indice); tras editar se copian los
    // handles de vuelta al VertexFrame (ver VertexCanalTemp / EvalVertexAnim).
    static AnimProperty s_vtxTmp;
    VertexAnimation* van = VertexCanalTemp(g_hOwner, s_vtxTmp);
    AnimProperty* ap = van ? &s_vtxTmp : DopeResolverProp(g_hOwner, g_hProp, g_hComp); if (!ap) return;
    size_t i = 0; bool hay = false;
    for (size_t k=0;k<ap->keyframes.size();k++) if (ap->keyframes[k].frame == g_hFrame){ i=k; hay=true; break; }
    if (!hay) return;
    keyFrame& kf = ap->keyframes[i];
    HandleBakear(ap, i);                     // si era de los CALCULADOS, congelar lo que se estaba viendo
    if (!g_hDirOk){ g_hDirOk = true; HandleDirCurva(ap, i, g_hSalida, g_hDirF, g_hDirV); }
    float dF = XToFrame((float)(mx - x)) - (float)kf.frame;
    float dV = YToValue((float)(my - y)) - kf.value;
    // CTRL: sobre la recta de la curva que VENIA (HandleDirCurva; proyeccion en PIXELES: los dos ejes tienen zoom
    // distinto y en pantalla es donde "alineado" se ve). No cruza al otro lado del keyframe (t > 0)
    if (LCtrlPressed){
        const float ppf = pxPerFrame > 1e-6f ? pxPerFrame : 1e-6f, ppu = pxPerUnit > 1e-6f ? pxPerUnit : 1e-6f;
        const float ux = g_hDirF * ppf, uy = g_hDirV * ppu, uu = ux*ux + uy*uy;
        if (uu > 1e-9f){
            float t = (dF * ppf * ux + dV * ppu * uy) / uu;
            if (t < 0.02f) t = 0.02f;
            dF = g_hDirF * t; dV = g_hDirV * t;
        }
    }
    // el handle no cruza al otro lado del keyframe: si lo hiciera, x dejaria de ser monotono y un frame tendria
    // dos valores (la curva se doblaria hacia atras en el tiempo)
    const float minDF = 0.05f;
    if (g_hSalida){ if (dF <  minDF) dF =  minDF; } else { if (dF > -minDF) dF = -minDF; }
    // filas VERTICES/UV: SIN overshoot — el handle queda DENTRO del rectangulo del tramo
    // (estilo "auto clamped": tiempo dentro del tramo, valor entre los dos keys). Es el
    // MISMO clamp del evaluador (VertexAnimClampHandle): lo que agarras es lo que corre,
    // y no se pueden crear zonas fuera de rango. La curva temporal va en rampa de 1 por
    // keyframe, asi que el 0..1 del ease coincide con el delta de valor del tramo.
    if (van){
        float span = 0.0f;
        if (g_hSalida){ if (i+1 < ap->keyframes.size()) span = (float)(ap->keyframes[i+1].frame - kf.frame); }
        else          { if (i > 0)                      span = (float)(kf.frame - ap->keyframes[i-1].frame); }
        VertexAnimClampHandle(g_hSalida, span, dF, dV);
    }
    if (g_hSalida){ kf.outDF = dF; kf.outDV = dV; } else { kf.inDF = dF; kf.inDV = dV; }
    if (kf.handleType == HAligned) HandleAlinear(kf, g_hSalida);  // el otro lado sigue al que moviste
    // (no hace falta poner KfBezier: el handle solo existe si el tramo YA es bezier — ver HandleVisible)
    // filas VERTICES/UV: los handles viven en el VertexFrame. Se copian los DOS lados de
    // la curva temporal: el editado (recien asignado) y el otro, que trae el valor
    // EFECTIVO que se estaba viendo (VertexAnimHandlesTramo) -> queda BAKEADO, como
    // HandleBakear: tocar un handle no hace saltar al de enfrente. HFree: ya es propio.
    if (van){ for (size_t k=0;k<van->frames.size();k++) if (van->frames[k]->frame == g_hFrame){
        VertexFrame* vf = van->frames[k];
        vf->handleType = HFree;
        vf->inDF = kf.inDF; vf->inDV = kf.inDV; vf->outDF = kf.outDF; vf->outDV = kf.outDV;
        if (van->target) van->target->skinGeomVersion++; // re-evaluar el ease en el viewport
        break; } }
    InvalidarAnimYRedraw();
}
void Timeline::HandleSoltar(){
    if (!g_hOn) return;
    g_hOn = false; UndoKeyframesConfirmar(); g_redraw = true;
}

// ============================================================================
//  SMART EULER: arregla las rotaciones que dan una vuelta de mas.
//  Un angulo en grados es el MISMO cada 360: 405 y 45 dejan al objeto rotado EXACTAMENTE igual. Pero la curva se
//  interpola por el VALOR, no por la rotacion final: pasar de 0 a 405 gira una vuelta entera de mas y despues
//  vuelve. Con 0 -> 45 -> 90 el giro sale derecho, y se ve igual en los keyframes.
//  Para cada keyframe se elige, entre todos sus equivalentes (valor + k*360), el que queda MAS CERCA del keyframe
//  ANTERIOR (que ya viene arreglado, porque se recorre en orden). Asi los saltos de vuelta entera desaparecen sin
//  cambiar ni una pose.
//  Solo toca canales de ROTACION: en position/scale un +360 no es lo mismo, seria romper la animacion.
//  Devuelve cuantos keyframes cambio.
// ============================================================================
int Timeline::SmartEulerSel(){
    UndoKeyframesIniciar();
    int n = 0;
    for (size_t r=0;r<dopeRows.size();r++){
        const DopeRow& d = dopeRows[r];
        if (d.propId != AnimRotation || d.oculto) continue;   // solo rotacion, y solo lo que se ve
        AnimProperty* ap = DopeResolverProp(d.ownerKey, d.propId, d.compId); if (!ap) continue;
        // en ORDEN: cada keyframe se compara contra el anterior YA corregido. El primero es el ancla (no tiene
        // contra que compararse) aunque este seleccionado.
        for (size_t i=1;i<ap->keyframes.size();i++){
            if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, ap->keyframes[i].frame))) continue;
            float prev = ap->keyframes[i-1].value;
            float v = ap->keyframes[i].value;
            float k = floorf((prev - v) / 360.0f + 0.5f);     // cuantas vueltas hay que sacarle/ponerle
            if (k == 0.0f) continue;
            ap->keyframes[i].value = v + k * 360.0f;
            n++;
        }
    }
    UndoKeyframesConfirmar();
    if (n) InvalidarAnimYRedraw();
    return n;
}

// menu Handle Type ('v') -> a todos los keyframes SELECCIONADOS. Al pasar a HFree/HAligned hay que CONGELAR antes
// los handles que se estaban viendo: si no, saltarian de golpe a (0,0), que es lo que hay guardado mientras el tipo
// es de los calculados.
// ---- TOOLBAR de abajo ----
bool Timeline::ToolbarVisible() const { return !g_dopeKeySel.empty() && !dopeRows.empty() && !g_dopeMov; }
void Timeline::ToolbarSincronizar(){
    // lo que tienen los keyframes elegidos: si TODOS comparten la interpolacion / el handle / el ease, ese boton
    // queda VERDE. El grupo del EASE sale con un TRAMO elegido (sus dos puntas) o con una curva de easing.
    int interp = -2, handle = -2, ease = -2; bool tramo = false;
    for (size_t r=0;r<dopeRows.size();r++){
        const DopeRow& d = dopeRows[r]; if (d.propId < 0 || DopeVertexAnimDe(d.ownerKey)) continue;
        const AnimProperty* ap = DopeResolverProp(d.ownerKey, d.propId, d.compId, false); if (!ap) continue;
        const size_t n = ap->keyframes.size();
        for (size_t i=0;i<n;i++){
            const keyFrame k = ap->keyframes.Clave(i);
            if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, k.frame))) continue;
            const int b = KfInterpBase(k.Interpolation);
            interp = (interp == -2 || interp == b) ? b : -1;
            handle = (handle == -2 || handle == k.handleType) ? k.handleType : -1;
            if (KfEsEasing(k.Interpolation)){ const int e = KfInterpEase(k.Interpolation); ease = (ease == -2 || ease == e) ? e : -1; tramo = true; }
            if (i + 1 < n && g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, ap->keyframes.Frame(i + 1)))) tramo = true;
        }
    }
    // ACTIVO: como el proportional editing de la barra del 3D (TbVerdeBg = accent atenuado de fondo, icono accent)
    const float* verde = ListaColores[static_cast<int>(ColorID::accent)];
    for (size_t i=0;i<ToolButtons.size();i++){
        Button* b = ToolButtons[i]; const int r = b->rol;
        bool on = false;
        b->visible = true;
        if (r >= TL_TB_EASE0)        { b->visible = tramo; on = (r - TL_TB_EASE0 == ease); }
        else if (r >= TL_TB_HANDLE0) { on = (r - TL_TB_HANDLE0 == handle); }
        else if (r >= TL_TB_INTERP0) { on = (r - TL_TB_INTERP0 == interp); }
        b->tinte = on ? TbVerdeBg() : NULL;
        b->colorTexto = on ? verde : NULL;
    }
}
void Timeline::ToolbarAccionRol(int rol){
    g_tlActivo = this;
    if (rol >= TL_TB_EASE0)          SetEasingSel(rol - TL_TB_EASE0);
    else if (rol >= TL_TB_HANDLE0)   SetHandleTypeSel(rol - TL_TB_HANDLE0);
    else if (rol >= TL_TB_INTERP0)   SetInterpolacionSel(rol - TL_TB_INTERP0);
    g_redraw = true;
}
void Timeline::SetHandleTypeSel(int tipo){
    UndoKeyframesIniciar();
    for (size_t r=0;r<dopeRows.size();r++){
        const DopeRow& d = dopeRows[r]; if (d.propId<0) continue;
        AnimProperty* ap = DopeResolverProp(d.ownerKey, d.propId, d.compId); if (!ap) continue;
        for (size_t i=0;i<ap->keyframes.size();i++){
            if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey,d.propId,d.compId,ap->keyframes[i].frame))) continue;
            if (tipo == HFree || tipo == HAligned){
                float aDF, aDV, bDF, bDV;
                ap->HandleEfectivo(i, false, aDF, aDV);
                ap->HandleEfectivo(i, true,  bDF, bDV);
                keyFrame& kf = ap->keyframes[i];
                kf.inDF = aDF; kf.inDV = aDV; kf.outDF = bDF; kf.outDV = bDV;
            }
            ap->keyframes[i].handleType = tipo;
            if (tipo == HAligned) HandleAlinear(ap->keyframes[i], true); // dejarlos colineales YA
        }
    }
    UndoKeyframesConfirmar();
    InvalidarAnimYRedraw();
}

// menu Interpolation -> a todos los keyframes SELECCIONADOS (define la forma del tramo que SALE de cada uno)
void Timeline::SetInterpolacionSel(int interp){
    UndoKeyframesIniciar();
    for (size_t r=0;r<dopeRows.size();r++){
        const DopeRow& d = dopeRows[r]; if (d.propId<0) continue;
        // filas VERTICES/UV: la interpolacion vive en el VertexFrame (no en un AnimProperty).
        // Solo se tocan los frames DE LA CAPA de la fila. (Un frame con ambas capas comparte
        // el ease entre las dos curvas: es UN cuadro.)
        VertexAnimation* van = DopeVertexAnimDe(d.ownerKey);
        if (van){
            int capa = DopeVertexCapaDe(d.ownerKey);
            for (size_t k=0;k<van->frames.size();k++){
                if (!VertexFrameEnCapa(van->frames[k], capa)) continue;
                if (g_dopeKeySel.count(DopeKeyId(d.ownerKey,d.propId,d.compId,van->frames[k]->frame)))
                    van->frames[k]->interp = interp;
            }
            continue;
        }
        AnimProperty* ap = DopeResolverProp(d.ownerKey, d.propId, d.compId); if (!ap) continue;
        // de ATRAS para adelante: pasar el tramo i a bezier pone los handles de i Y de i+1 (la entrada de i+1 es
        // la mitad de la forma). Si i+1 se procesara DESPUES, sus handles automaticos (la ultima clave, o una que
        // no venia de easing) pisarian esa entrada y la forma se perdia (con todo elegido, que es lo normal)
        for (size_t i=ap->keyframes.size(); i-- > 0; ){
            if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey,d.propId,d.compId,ap->keyframes[i].frame))) continue;
            // (de una curva de easing a otra se conserva su easing: Ctrl+E y E son independientes)
            const int viejo = ap->keyframes[i].Interpolation;
            ap->keyframes[i].Interpolation = KfInterpCon(interp, KfInterpEase(viejo));
            // recien pasa a bezier: si venia de una curva de easing, con los handles que conservan SU forma; si no
            // (o si era un rebote / elastico, que no entran en una bezier), handles automaticos
            if (interp == KfBezier && !KfBezierDesdeEasing(*ap, i, viejo)) ap->keyframes[i].handleType = HAuto;
        }
    }
    UndoKeyframesConfirmar();
    InvalidarAnimYRedraw();
}
// Ctrl+E: el EASING de los keyframes elegidos. Solo cambia algo en las curvas de easing (Sinusoidal...Elastic): las
// demas (Constant/Linear/Bezier) no tienen de que lado poner el efecto
void Timeline::CambiarModo(bool curvas){
    const int nuevo = curvas ? TL_MODO_CURVAS : TL_MODO_DOPE;
    if (nuevo == modo) return;
    modo = nuevo;
    // AUTO-ENCUADRE al ENTRAR al modo curvas: el zoom vertical viene de la nada (el dope sheet no tiene eje de valor)
    // y la curva podia quedar fuera de pantalla. Las filas tienen que estar armadas ANTES de medir el rango.
    if (modo == TL_MODO_CURVAS && g_dopeAutoFrame){ ConstruirDopeRows(); CurvaFrameSelected(); }
    g_redraw = true;
}
// EASE de un tramo bezier de VERTICES (k -> el siguiente de la misma capa). El tramo va de 0 a 1 (el progreso
// entre pose y pose); los handles calculados se congelan antes con los mismos valores que muestra el editor.
static void VertexEaseBezier(VertexAnimation& van, size_t k, int capa, int ease){
    size_t s = k + 1; while (s < van.frames.size() && !VertexFrameEnCapa(van.frames[s], capa)) s++;
    if (s >= van.frames.size()) return;
    VertexFrame& A = *van.frames[k]; VertexFrame& B = *van.frames[s];
    if (ease != KfEaseAuto){
        size_t p = k; bool hayP = false;
        while (p > 0){ p--; if (VertexFrameEnCapa(van.frames[p], capa)){ hayP = true; break; } }
        size_t q = s + 1; while (q < van.frames.size() && !VertexFrameEnCapa(van.frames[q], capa)) q++;
        float o1, o2, i1, i2;
        if (A.handleType != HFree && A.handleType != HAligned){
            if (hayP){ VertexAnimHandlesTramo(*van.frames[p], A, o1, o2, A.inDF, A.inDV); } else { A.inDF = -1.0f; A.inDV = 0.0f; }
            A.handleType = HFree;
        }
        if (B.handleType != HFree && B.handleType != HAligned){
            if (q < van.frames.size()){ VertexAnimHandlesTramo(B, *van.frames[q], B.outDF, B.outDV, i1, i2); } else { B.outDF = 1.0f; B.outDV = 0.0f; }
            B.handleType = HFree;
        }
    }
    KfEaseBezierTramo(A, B, 0.0f, 1.0f, ease);
}
void Timeline::SetEasingSel(int ease){
    UndoKeyframesIniciar();
    for (size_t r=0;r<dopeRows.size();r++){
        const DopeRow& d = dopeRows[r]; if (d.propId<0) continue;
        VertexAnimation* van = DopeVertexAnimDe(d.ownerKey);
        if (van){
            int capa = DopeVertexCapaDe(d.ownerKey);
            for (size_t k=0;k<van->frames.size();k++){
                if (!VertexFrameEnCapa(van->frames[k], capa)) continue;
                if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey,d.propId,d.compId,van->frames[k]->frame))) continue;
                if (KfEsEasing(van->frames[k]->interp)) van->frames[k]->interp = KfInterpCon(KfInterpBase(van->frames[k]->interp), ease);
                else if (van->frames[k]->interp == KfBezier) VertexEaseBezier(*van, k, capa, ease);
            }
            continue;
        }
        AnimProperty* ap = DopeResolverProp(d.ownerKey, d.propId, d.compId); if (!ap) continue;
        for (size_t i=0;i<ap->keyframes.size();i++){
            if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey,d.propId,d.compId,ap->keyframes[i].frame))) continue;
            keyFrame& k = ap->keyframes[i];
            if (KfEsEasing(k.Interpolation)) k.Interpolation = KfInterpCon(KfInterpBase(k.Interpolation), ease);
            else KfEaseBezier(*ap, i, ease);   // bezier comun: el ease va en los handles del tramo
        }
    }
    UndoKeyframesConfirmar();
    InvalidarAnimYRedraw();
}

void Timeline::DopeHover(int mx, int my){
    int lx = mx - x, ly = my - y, h = -1;
    if (!dopeRows.empty() && lx >= 0 && lx < panelW && ly >= stripY && ly < height){
        int r = (ly - stripY - PosY) / (rowH>0?rowH:1);
        if (r >= 0 && r < (int)dopeRows.size()) h = r;
    }
    if (h != hoverRow){ hoverRow = h; g_redraw = true; }
}

// ============================================================================
//  NAVEGACION DE KEYFRAMES POR TECLADO (Symbian: el lapiz + flechas). Sin mouse hay que poder elegir keyframes
//  igual, pero recorrer los de CADA canal seria infinito (18 curvas x 35 keyframes). Por eso se recorren los del
//  SUMMARY: un paso por FRAME que tenga algo. Para trabajar sobre una curva puntual esta el modo curva: si hay una
//  FILA seleccionada, se recorren los de ESA curva.
//  El "activo" es el mismo que marca el click (g_dopeAct*), asi teclado y mouse comparten estado.
// ============================================================================

// las filas cuyos keyframes se recorren: la del summary, o las SELECCIONADAS si hay alguna de canal
void Timeline::DopeFilasNav(std::vector<int>& out) const {
    out.clear();
    for (size_t r=0;r<dopeRows.size();r++){
        const DopeRow& d = dopeRows[r];
        if (d.propId < 0 || d.oculto) continue;
        if (g_dopeRowSel.count(d.claveFila)) out.push_back((int)r);
    }
    if (!out.empty()) return;              // hay curvas elegidas: se trabaja sobre ESAS
    // si no, TODAS las de canal (sus frames son los del summary: la union)
    for (size_t r=0;r<dopeRows.size();r++)
        if (dopeRows[r].propId >= 0 && !dopeRows[r].oculto) out.push_back((int)r);
}

// los FRAMES que se recorren (la union de los de las filas de arriba), ordenados
void Timeline::DopeFramesNav(std::vector<int>& out) const {
    out.clear();
    std::vector<int> filas; DopeFilasNav(filas);
    for (size_t i=0;i<filas.size();i++){
        const DopeRow& d = dopeRows[filas[i]];
        for (size_t k=0;k<d.keys.size();k++){
            bool ya=false; for (size_t j=0;j<out.size();j++) if (out[j]==d.keys[k]){ ya=true; break; }
            if (!ya) out.push_back(d.keys[k]);
        }
    }
    std::sort(out.begin(), out.end());
}

// selecciona (o saca) TODOS los keyframes del frame 'f' de las filas que se recorren
void Timeline::DopeSelFrame(int f, bool on){
    std::vector<int> filas; DopeFilasNav(filas);
    for (size_t i=0;i<filas.size();i++){
        const DopeRow& d = dopeRows[filas[i]];
        for (size_t k=0;k<d.keys.size();k++){
            if (d.keys[k] != f) continue;
            std::string id = DopeKeyId(d.ownerKey, d.propId, d.compId, f);
            if (on){ g_dopeKeySel.insert(id);
                     g_dopeActOwner=d.ownerKey; g_dopeActProp=d.propId; g_dopeActComp=d.compId;
                     g_dopeActFrame=f; g_dopeActHay=true; }
            else g_dopeKeySel.erase(id);
        }
    }
}

// lapiz+flechas: avanza el keyframe ACTIVO. extender = mantiene lo que ya estaba elegido.
bool Timeline::DopeSelAvanzar(int paso, bool extender){
    std::vector<int> fr; DopeFramesNav(fr);
    if (fr.empty()) return false;
    // el frame de partida: el activo, o el primero elegido, o el mas cercano al frame actual
    int act = g_dopeActHay ? g_dopeActFrame : CurrentFrame;
    int idx = -1;
    for (size_t i=0;i<fr.size();i++) if (fr[i]==act){ idx=(int)i; break; }
    if (idx < 0){ // no esta en la lista: arrancar del mas cercano
        int mejor=0; int md=0x7fffffff;
        for (size_t i=0;i<fr.size();i++){ int d=fr[i]-act; if(d<0)d=-d; if(d<md){md=d;mejor=(int)i;} }
        idx = mejor;
        if (!extender) g_dopeKeySel.clear();
        DopeSelFrame(fr[idx], true);
        g_redraw = true; return true;
    }
    if (!extender) DopeSelFrame(fr[idx], false);   // deselecciona el actual y pasa al siguiente
    int next = idx + paso;
    if (next < 0) next = (int)fr.size()-1;
    if (next >= (int)fr.size()) next = 0;
    DopeSelFrame(fr[next], true);
    g_redraw = true; return true;
}
bool Timeline::DopeSelTodoNav(){  DopeSelectAll(); return true; }
bool Timeline::DopeSelNadaNav(){  DopeSelectNone(); g_dopeActHay = false; return true; }

void Timeline::DopeSelectAll(){
    g_dopeKeySel.clear();
    // lo OCULTO no entra: ocultar una curva es justamente sacarla del laburo
    for (size_t r=0;r<dopeRows.size();r++){ const DopeRow& d=dopeRows[r]; if (d.propId<0 || d.oculto) continue;
        for (size_t i=0;i<d.keys.size();i++) g_dopeKeySel.insert(DopeKeyId(d.ownerKey,d.propId,d.compId,d.keys[i])); }
    g_redraw = true;
}
void Timeline::DopeSelectNone(){ g_dopeKeySel.clear(); g_dopeRowSel.clear(); g_redraw = true; }
void Timeline::DopeSelectInvert(){
    std::set<std::string> inv;
    for (size_t r=0;r<dopeRows.size();r++){ const DopeRow& d=dopeRows[r]; if (d.propId<0 || d.oculto) continue;
        for (size_t i=0;i<d.keys.size();i++){ std::string id = DopeKeyId(d.ownerKey,d.propId,d.compId,d.keys[i]);
            if (!g_dopeKeySel.count(id)) inv.insert(id); } }
    g_dopeKeySel.swap(inv); g_redraw = true;
}

// 'x': si hay KEYFRAMES seleccionados los borra; sino borra la animacion de las FILAS seleccionadas.
// hay keyframes o curvas elegidos en el timeline? (la C del N95 los borra a ellos y no al objeto activo)
bool TimelineHaySeleccion() { return !g_dopeKeySel.empty() || !g_dopeRowSel.empty(); }
int  TimelineCantSeleccion() { return (int)g_dopeKeySel.size(); }   // (harness)
void Timeline::DopeBorrarSeleccion(){
    if (g_dopeKeySel.empty() && g_dopeRowSel.empty()) return;
    UndoKeyframesIniciar(); // Ctrl+Z: snapshot de las curvas ANTES de borrar
    if (!g_dopeKeySel.empty()){
        for (size_t r=0;r<dopeRows.size();r++){ const DopeRow& d=dopeRows[r]; if (d.propId<0) continue;
            // filas VERTICES/UV: borrar POR CAPA — se saca SOLO la capa de la fila (borrar un
            // keyframe de la curva Vertices no toca el de UV del mismo cuadro, y al reves); si
            // el frame queda sin capas se va entero. No dejar la anim sin ningun frame (una
            // vertex anim vacia no tiene sentido): conservar al menos uno.
            VertexAnimation* van = DopeVertexAnimDe(d.ownerKey);
            if (van){
                int capa = DopeVertexCapaDe(d.ownerKey);
                for (size_t k=van->frames.size(); k-- > 0; ){
                    VertexFrame* vf = van->frames[k];
                    if (!VertexFrameEnCapa(vf, capa)) continue;
                    if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, vf->frame))) continue;
                    bool soloEsta = (VertexFrameCapas(vf) <= 1);   // sacarle esta capa lo deja sin ninguna
                    if (soloEsta && (int)van->frames.size() <= 1) break; // dejar siempre 1 frame
                    VertexAnimBorrarCapa(*van, vf->frame, capa);
                }
                if (van->target) van->target->skinGeomVersion++; // el frame actual pudo cambiar de pose
                continue;
            }
            AnimProperty* ap = DopeResolverProp(d.ownerKey, d.propId, d.compId); if (!ap) continue;
            // de atras para adelante y por FRAME: BorrarKeyframeManteniendoForma re-ajusta los handles de los
            // vecinos, asi que borrar varios de un tramo bezier deja la curva lo mas parecida posible en cada paso.
            for (size_t k=ap->keyframes.size(); k-- > 0; )
                if (g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, ap->keyframes[k].frame)))
                    BorrarKeyframeManteniendoForma(*ap, ap->keyframes[k].frame);
        }
        g_dopeKeySel.clear();
    } else if (!g_dopeRowSel.empty()){
        for (size_t r=0;r<dopeRows.size();r++){ const DopeRow& d=dopeRows[r];
            if (!g_dopeRowSel.count(d.claveFila)) continue;
            // filas VERTICES/UV: "borrar la fila" = limpiar SU CAPA de todos los frames
            // (dejando al menos un frame vivo en la anim)
            VertexAnimation* van = (d.propId >= 0) ? DopeVertexAnimDe(d.ownerKey) : NULL;
            if (van){
                int capa = DopeVertexCapaDe(d.ownerKey);
                for (size_t k=van->frames.size(); k-- > 0; ){
                    VertexFrame* vf = van->frames[k];
                    if (!VertexFrameEnCapa(vf, capa)) continue;
                    bool soloEsta = (VertexFrameCapas(vf) <= 1);   // sacarle esta capa lo deja sin ninguna
                    if (soloEsta && (int)van->frames.size() <= 1) continue; // dejar siempre 1 frame
                    VertexAnimBorrarCapa(*van, vf->frame, capa);
                }
                if (van->target) van->target->skinGeomVersion++;
            }
            else if (d.propId >= 0){ AnimProperty* ap = DopeResolverProp(d.ownerKey, d.propId, d.compId); if (ap) ap->keyframes.clear(); }
            else { // fila padre: borra las 9 CURVAS (3 propiedades x 3 componentes) de ese dueño + las de los canales
                   // que cubre por prefijo (ej: el armature borra las de todos sus huesos listados)
                const int pid[3] = { AnimPosition, AnimRotation, AnimScale };
                const int cid[3] = { AnimX, AnimY, AnimZ };
                for (int p=0;p<3;p++) for (int c=0;c<3;c++){
                    AnimProperty* ap = DopeResolverProp(d.ownerKey, pid[p], cid[c]); if (ap) ap->keyframes.clear(); }
                for (size_t k=0;k<dopeRows.size();k++){ const DopeRow& c=dopeRows[k]; if (c.propId<0) continue;
                    if (!DopeCubre(d.ownerKey, c.ownerKey)) continue;
                    AnimProperty* ap = DopeResolverProp(c.ownerKey, c.propId, c.compId); if (ap) ap->keyframes.clear(); }
            }
        }
        g_dopeRowSel.clear();
    }
    UndoKeyframesConfirmar(); // 1 comando de undo con el borrado
    InvalidarAnimYRedraw();
}

// ---- Shift+D: DUPLICAR los keyframes seleccionados y agarrarlos para moverlos (como en el resto del editor).
// Las copias no pueden nacer ENCIMA de los originales: la identidad de un keyframe aca es (curva, frame), asi que
// dos keyframes en el mismo frame serian EL MISMO y no habria forma de mover solo la copia. Nacen corridas el LARGO
// DE LA SELECCION + 1, o sea justo despues del bloque copiado: asi no pisan ni uno de los originales (con un offset
// de 1 frame y keyframes en frames consecutivos, cada copia se comia al original de al lado). Despues arranca el 'g'
// para que las ubiques, y Esc borra las copias.
// ---- PORTAPAPELES DE KEYFRAMES (Ctrl+C / Ctrl+X / Ctrl+V y el click derecho). Se guarda cada keyframe elegido con
// su curva y su frame RELATIVO al primero; pegar los pone desde el frame ACTUAL. Si la curva de origen ya no esta (se
// eligio otro objeto), va a la curva IGUAL (misma propiedad y componente) de lo que muestra el timeline: asi se copia
// la animacion de un objeto a otro. Los frames de vertices/UV no entran (no son curvas: son mallas enteras).
struct DopeClipKey { std::string ownerKey; int propId, compId, rel; keyFrame k; };
static std::vector<DopeClipKey> g_dopeClip;

bool Timeline::DopeCopiarSeleccion(){
    if (g_dopeKeySel.empty()) return false;
    std::vector<DopeClipKey> clip;
    std::set<std::string> vistas;
    int mn = 0x7fffffff;
    for (size_t r=0;r<dopeRows.size();r++){ const DopeRow& d=dopeRows[r];
        if (d.propId < 0 || DopeVertexAnimDe(d.ownerKey)) continue;
        char id[32]; snprintf(id, sizeof(id), "|%d|%d", d.propId, d.compId);
        if (!vistas.insert(d.ownerKey + id).second) continue;     // (la misma curva puede estar en 2 filas)
        AnimProperty* ap = DopeResolverProp(d.ownerKey, d.propId, d.compId, false); if (!ap) continue;
        for (size_t k=0;k<ap->keyframes.size();k++){
            if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, ap->keyframes[k].frame))) continue;
            DopeClipKey c; c.ownerKey = d.ownerKey; c.propId = d.propId; c.compId = d.compId; c.rel = 0; c.k = ap->keyframes[k];
            clip.push_back(c);
            if (c.k.frame < mn) mn = c.k.frame;
        }
    }
    if (clip.empty()) return false;
    for (size_t i=0;i<clip.size();i++) clip[i].rel = clip[i].k.frame - mn;
    g_dopeClip.swap(clip);
    return true;
}
void Timeline::DopeCortarSeleccion(){ if (DopeCopiarSeleccion()) DopeBorrarSeleccion(); }
void Timeline::DopePegar(){
    if (g_dopeClip.empty() || g_dopeMov) return;
    g_dopePisados.clear();
    UndoKeyframesIniciar();   // Ctrl+Z: el pegado es UN comando
    std::set<std::string> nuevaSel;
    for (size_t i=0;i<g_dopeClip.size();i++){
        const DopeClipKey& c = g_dopeClip[i];
        std::string owner = c.ownerKey;
        AnimProperty* ap = DopeResolverProp(owner, c.propId, c.compId);
        if (!ap){   // la curva de origen no esta: la igual de lo que se ve
            for (size_t r=0;r<dopeRows.size() && !ap;r++){ const DopeRow& d=dopeRows[r];
                if (d.propId != c.propId || d.compId != c.compId || DopeVertexAnimDe(d.ownerKey)) continue;
                ap = DopeResolverProp(d.ownerKey, d.propId, d.compId);
                if (ap) owner = d.ownerKey;
            }
        }
        if (!ap) continue;
        keyFrame k = c.k; k.frame = CurrentFrame + c.rel;
        for (size_t j=ap->keyframes.size(); j-- > 0; ) if (ap->keyframes[j].frame == k.frame){
            DopePisadoAnotar(owner, c.propId, c.compId, ap->keyframes[j]);   // (vuelve si lo pegado se corre: ver arriba)
            ap->keyframes.erase(ap->keyframes.begin()+j);
        }
        ap->keyframes.push_back(k);
        ap->SortKeyFrames();
        nuevaSel.insert(DopeKeyId(owner, c.propId, c.compId, k.frame));
    }
    UndoKeyframesConfirmar();
    if (!nuevaSel.empty()){ g_dopeKeySel.swap(nuevaSel); g_dopeRowSel.clear(); }   // lo pegado queda ELEGIDO
    InvalidarAnimYRedraw();
    // ...y AGARRADO, como el Duplicar: el mouse lo corre, click/Enter lo deja (Esc lo deja donde se pego)
    if (!g_dopeKeySel.empty()) DopeMoveStart(DOPE_MOV);
    g_dopePisados.clear();   // (sin movimiento, lo pegado se quedo encima: lo tapado queda pisado)
}
// CLICK DERECHO en el strip: el menu de los keyframes elegidos (y Pegar, aunque no haya nada elegido)
static void TL_menuCtxAction(int id){
    TL_CerrarMenu(g_tlMenuCtx);
    if (!g_tlActivo) return;
    switch (id){
        case TL_KEY_CORTAR: g_tlActivo->DopeCortarSeleccion(); break;
        case TL_KEY_COPIAR: g_tlActivo->DopeCopiarSeleccion(); break;
        case TL_KEY_PEGAR:  g_tlActivo->DopePegar(); break;
        case TL_KEY_DUP:    g_tlActivo->DopeDuplicarSeleccion(); break;
        case TL_KEY_DEL:    g_tlActivo->DopeBorrarSeleccion(); break;
    }
    g_redraw = true;
}
bool Timeline::MenuContexto(int mx, int my){
    if (dopeRows.empty() || g_dopeMov) return false;
    const int lx = mx - x, ly = my - y;
    if (lx < panelW || lx >= width || ly < stripY || ly >= height) return false;   // solo el strip
    g_tlActivo = this;
    if (!g_tlMenuCtx){ g_tlMenuCtx = new PopupMenu(); g_tlMenuCtx->action = TL_menuCtxAction; }
    g_tlMenuCtx->Limpiar();
    g_tlMenuCtx->titulo = T("Keyframes");
    static bool sHaySel, sHayClip;
    sHaySel = !g_dopeKeySel.empty(); sHayClip = !g_dopeClip.empty();
    MenuItem* it;
    it = g_tlMenuCtx->Agregar(T("Cut"), TL_KEY_CORTAR);   it->atajo = "@tl.cortar";   it->gris = &sHaySel;
    it = g_tlMenuCtx->Agregar(T("Copy"), TL_KEY_COPIAR);  it->atajo = "@tl.copiar";   it->gris = &sHaySel;
    it = g_tlMenuCtx->Agregar(T("Paste"), TL_KEY_PEGAR);  it->atajo = "@tl.pegar";    it->gris = &sHayClip;
    it = g_tlMenuCtx->Agregar(T("Duplicate"), TL_KEY_DUP); it->atajo = "@tl.duplicar"; it->gris = &sHaySel; it->lineaArriba = true;
    it = g_tlMenuCtx->Agregar(T("Delete"), TL_KEY_DEL);   it->atajo = "@tl.borrar";   it->gris = &sHaySel;
    if (MenuAbierto && MenuAbierto != g_tlMenuCtx) MenuAbierto->Cerrar();
    g_tlMenuCtx->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = g_tlMenuCtx;
    g_redraw = true;
    return true;
}

void Timeline::DopeDuplicarSeleccion(){
    if (g_dopeKeySel.empty() || g_dopeMov) return;
    g_dopePisados.clear();
    // estado ANTERIOR a duplicar (para que Esc pueda volver a el)
    g_dopeDupPre.clear();
    g_dopeDupVtxPre.clear();
    for (size_t r=0;r<dopeRows.size();r++){ const DopeRow& d=dopeRows[r]; if (d.propId<0) continue;
        bool ya=false; for (size_t s=0;s<g_dopeDupPre.size();s++)
            if (g_dopeDupPre[s].ownerKey==d.ownerKey && g_dopeDupPre[s].propId==d.propId && g_dopeDupPre[s].compId==d.compId){ ya=true; break; }
        if (ya) continue;
        AnimProperty* ap = DopeResolverProp(d.ownerKey, d.propId, d.compId); if (!ap) continue;
        DopeMovProp mp; mp.ownerKey=d.ownerKey; mp.propId=d.propId; mp.compId=d.compId; mp.orig=ap->keyframes;
        g_dopeDupPre.push_back(mp);
    }
    // OFFSET: el largo del bloque seleccionado + 1 -> las copias caen JUSTO DESPUES de lo copiado y no pisan
    // ningun original. Se mide sobre TODA la seleccion (no curva por curva) para que el bloque se copie entero y
    // parejo: si cada canal usara su propio offset, las copias quedarian desalineadas entre si.
    int selMin = 0x7fffffff, selMax = -0x7fffffff;
    for (size_t s=0;s<g_dopeDupPre.size();s++){
        DopeMovProp& mp = g_dopeDupPre[s];
        for (size_t k=0;k<mp.orig.size();k++){
            if (!g_dopeKeySel.count(DopeKeyId(mp.ownerKey, mp.propId, mp.compId, mp.orig[k].frame))) continue;
            if (mp.orig[k].frame < selMin) selMin = mp.orig[k].frame;
            if (mp.orig[k].frame > selMax) selMax = mp.orig[k].frame;
        }
    }
    // los FRAMES DE VERTICES/UV seleccionados tambien cuentan para el offset (sino una
    // seleccion solo de esas filas sale por el guard de abajo y Shift+D no hace nada)
    for (size_t r=0;r<dopeRows.size();r++){ const DopeRow& d=dopeRows[r];
        VertexAnimation* van = DopeVertexAnimDe(d.ownerKey); if (!van) continue;
        int capa = DopeVertexCapaDe(d.ownerKey);
        for (size_t k=0;k<van->frames.size();k++){
            if (!VertexFrameEnCapa(van->frames[k], capa)) continue;
            int f=van->frames[k]->frame;
            if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, f))) continue;
            if (f < selMin) selMin = f; if (f > selMax) selMax = f;
        }
    }
    if (selMin > selMax){ g_dopeDupPre.clear(); return; }
    const int off = (selMax - selMin) + 1;
    UndoKeyframesIniciar();   // Ctrl+Z: la duplicacion + el movimiento son UN solo comando
    std::set<std::string> nuevaSel;
    for (size_t s=0;s<g_dopeDupPre.size();s++){
        DopeMovProp& mp = g_dopeDupPre[s];
        AnimProperty* ap = DopeResolverProp(mp.ownerKey, mp.propId, mp.compId); if (!ap) continue;
        std::vector<keyFrame> copias;
        for (size_t k=0;k<mp.orig.size();k++){
            if (!g_dopeKeySel.count(DopeKeyId(mp.ownerKey, mp.propId, mp.compId, mp.orig[k].frame))) continue;
            keyFrame c = mp.orig[k]; c.frame = mp.orig[k].frame + off;
            copias.push_back(c);
        }
        if (copias.empty()) continue;
        // sacar lo que estuviera en los frames de destino y meter las copias
        for (size_t c=0;c<copias.size();c++){
            for (size_t k=ap->keyframes.size(); k-- > 0; ) if (ap->keyframes[k].frame == copias[c].frame){
                DopePisadoAnotar(mp.ownerKey, mp.propId, mp.compId, ap->keyframes[k]);   // (vuelve si la copia se corre)
                ap->keyframes.erase(ap->keyframes.begin()+k);
            }
        }
        for (size_t c=0;c<copias.size();c++){ ap->keyframes.push_back(copias[c]);
            nuevaSel.insert(DopeKeyId(mp.ownerKey, mp.propId, mp.compId, copias[c].frame)); }
        ap->SortKeyFrames();
    }
    // CLONAR los frames de VERTICES/UV seleccionados, POR ANIM (las filas Vertices y UV
    // comparten los VertexFrame): a cada frame fuente se le clona SOLO la capa cuya fila
    // estaba elegida (VertexAnimSetKey copia los buffers al frame f+off preservando la
    // interp; con la otra capa en NULL la copia nace solo con la elegida). Se seleccionan
    // las COPIAS -> el 'g' que arranca abajo las arrastra (via g_dopeMovVtx sobre los clones).
    {
        std::vector<VertexAnimation*> vans; std::vector<std::string> ovtx, ouv, onrm;
        for (size_t r=0;r<dopeRows.size();r++){ const DopeRow& d=dopeRows[r];
            VertexAnimation* van = DopeVertexAnimDe(d.ownerKey); if (!van) continue;
            size_t a = 0; for (; a<vans.size(); a++) if (vans[a]==van) break;
            if (a==vans.size()){ vans.push_back(van); ovtx.push_back(""); ouv.push_back(""); onrm.push_back(""); }
            int cp = DopeVertexCapaDe(d.ownerKey);
            if (cp==0) ovtx[a]=d.ownerKey; else if (cp==1) ouv[a]=d.ownerKey; else onrm[a]=d.ownerKey;
        }
        for (size_t a=0;a<vans.size();a++){
            VertexAnimation* van = vans[a];
            DopeDupVtxPre pre; pre.ownerKey = !ovtx[a].empty()?ovtx[a]:(!ouv[a].empty()?ouv[a]:onrm[a]); pre.an=van;
            VertexAnimSerializar(*van, pre.blob);
            g_dopeDupVtxPre.push_back(pre);             // pre-estado COMPLETO para el cancel
            // juntar los frames FUENTE + que capas se copian ANTES de clonar (SetKey inserta
            // y crece el vector). bit0 = fila Vertices elegida, bit1 = UV, bit2 = Normales.
            std::vector<int> src, mask;
            for (size_t k=0;k<van->frames.size();k++){
                const VertexFrame* f = van->frames[k]; if (!f) continue;
                int mk = 0;
                if (f->positions && !ovtx[a].empty() &&
                    g_dopeKeySel.count(DopeKeyId(ovtx[a], AnimVertex, 0, f->frame))) mk |= 1;
                if (f->uvs && !ouv[a].empty() &&
                    g_dopeKeySel.count(DopeKeyId(ouv[a], AnimVertex, 1, f->frame))) mk |= 2;
                if (f->normals && !onrm[a].empty() &&
                    g_dopeKeySel.count(DopeKeyId(onrm[a], AnimVertex, 2, f->frame))) mk |= 4;
                if (mk){ src.push_back(f->frame); mask.push_back(mk); }
            }
            for (size_t i=0;i<src.size();i++){
                VertexFrame* sf=NULL; for (size_t k=0;k<van->frames.size();k++) if (van->frames[k]->frame==src[i]){ sf=van->frames[k]; break; }
                if (!sf) continue;
                VertexAnimSetKey(*van, src[i]+off,
                                 (mask[i]&1) ? sf->positions : NULL,
                                 (mask[i]&4) ? sf->normals   : NULL,
                                 (mask[i]&2) ? sf->uvs       : NULL, sf->interp);
                if (mask[i]&1) nuevaSel.insert(DopeKeyId(ovtx[a], AnimVertex, 0, src[i]+off));
                if (mask[i]&2) nuevaSel.insert(DopeKeyId(ouv[a],  AnimVertex, 1, src[i]+off));
                if (mask[i]&4) nuevaSel.insert(DopeKeyId(onrm[a], AnimVertex, 2, src[i]+off));
            }
            if (van->target) van->target->skinGeomVersion++;
        }
    }
    if (nuevaSel.empty()){ g_dopeDupPre.clear(); g_dopeDupVtxPre.clear(); return; }
    g_dopeKeySel.swap(nuevaSel);       // quedan elegidas las COPIAS: el 'g' las mueve a ellas
    ConstruirDopeRows();               // las filas tienen que ver los keyframes nuevos antes de snapshotear el move
    g_dopeDup = true;
    DopeMoveStart(DOPE_MOV);
    if (!g_dopeMov){ g_dopeDup = false; g_dopeDupPre.clear(); g_dopeDupVtxPre.clear(); } // no arranco: no dejar el flag colgado
    InvalidarAnimYRedraw();
}

// ---- 'g': mover los keyframes seleccionados en el TIEMPO (izq/der). Click/Enter acepta, Esc cancela. ----
bool Timeline::DopeMoviendo() const { return g_dopeMov; }
void Timeline::DopeSetPivot(int p){ g_dopePivot = (p==DOPE_PIV_CURFRAME) ? DOPE_PIV_CURFRAME : DOPE_PIV_CENTER; }
void Timeline::DopeMoveStart(int modoT){  // DOPE_MOV ('g') / DOPE_ESC ('s') / DOPE_ROT ('r', solo curvas)
    if (g_dopeKeySel.empty()) return;
    // Si habia un handle agarrado (click sostenido) y se apreta G/S/R, hay que soltarlo ANTES: sino queda prendido
    // y, al terminar el transform, el proximo movimiento del mouse seguiria arrastrandolo. Ademas cierra su undo,
    // que si no lo pisaria el UndoKeyframesIniciar de aca abajo.
    if (g_hOn) HandleSoltar();
    g_dopeMovSnap.clear();
    // snapshot de CADA propiedad afectada (lista completa de keyframes) + que frames de esa prop estan seleccionados
    for (size_t r=0;r<dopeRows.size();r++){ const DopeRow& d=dopeRows[r]; if (d.propId<0) continue;
        bool ya=false; for (size_t s=0;s<g_dopeMovSnap.size();s++)
            if (g_dopeMovSnap[s].ownerKey==d.ownerKey && g_dopeMovSnap[s].propId==d.propId && g_dopeMovSnap[s].compId==d.compId){ ya=true; break; }
        if (ya) continue;
        AnimProperty* ap = DopeResolverProp(d.ownerKey, d.propId, d.compId); if (!ap) continue;
        DopeMovProp mp; mp.ownerKey=d.ownerKey; mp.propId=d.propId; mp.compId=d.compId; mp.orig=ap->keyframes;
        for (size_t k=0;k<ap->keyframes.size();k++)
            if (g_dopeKeySel.count(DopeKeyId(d.ownerKey,d.propId,d.compId,ap->keyframes[k].frame))) mp.selFrames.push_back(ap->keyframes[k].frame);
        mp.quieto.assign(mp.orig.size(), 0);
        for (size_t i=0;i<g_dopePisados.size();i++){ const DopeMovProp& pz = g_dopePisados[i];
            if (pz.ownerKey!=mp.ownerKey || pz.propId!=mp.propId || pz.compId!=mp.compId) continue;
            for (size_t j=0;j<pz.orig.size();j++){ mp.orig.push_back(pz.orig[j]); mp.quieto.push_back(1); } }
        if (!mp.selFrames.empty()) g_dopeMovSnap.push_back(mp);
    }
    g_dopePisados.clear();   // (ya estan en la foto)
    // snapshot PARALELO de las filas VERTICES/UV (frames de vertices, fuente de verdad
    // aparte). UNO por anim: las dos filas comparten los VertexFrame — mover un cuadro
    // que trae ambas capas mueve el cuadro entero (es UN keyframe con dos capas).
    g_dopeMovVtx.clear();
    for (size_t r=0;r<dopeRows.size();r++){ const DopeRow& d=dopeRows[r];
        VertexAnimation* van = DopeVertexAnimDe(d.ownerKey); if (!van) continue;
        int capa = DopeVertexCapaDe(d.ownerKey);
        size_t si = 0; for (; si<g_dopeMovVtx.size(); si++) if (g_dopeMovVtx[si].an==van) break;
        if (si == g_dopeMovVtx.size()){
            DopeMovVtxSnap sn; sn.an=van;
            for (size_t k=0;k<van->frames.size();k++){
                sn.vf.push_back(van->frames[k]); sn.origFrame.push_back(van->frames[k]->frame);
                sn.selPos.push_back(0); sn.selUV.push_back(0); sn.selNrm.push_back(0);
            }
            // MASCARA de la seleccion de Edit Mode: se congela ACA (al arrancar el transform), no al confirmar -
            // asi el criterio no cambia en el medio del arrastre. Solo si el toggle esta prendido y los frames
            // estan armados para este conteo de vertices (si la topologia cambio, mascara y frames no se
            // corresponden -> move de siempre).
            sn.selectivo = g_dopeSoloSelVerts && van->target && van->vcount == van->target->vertexSize &&
                           DopeMascaraVertsSel(van->target, sn.mask);
            g_dopeMovVtx.push_back(sn);
        }
        DopeMovVtxSnap& sn = g_dopeMovVtx[si];
        if (capa==0) sn.ownerVtx = d.ownerKey; else if (capa==1) sn.ownerUV = d.ownerKey; else sn.ownerNrm = d.ownerKey;
        for (size_t k=0;k<sn.vf.size();k++){
            if (!VertexFrameEnCapa(sn.vf[k], capa)) continue;
            if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, sn.origFrame[k]))) continue;
            if (capa==0) sn.selPos[k]=1; else if (capa==1) sn.selUV[k]=1; else sn.selNrm[k]=1;
        }
    }
    // tirar los snapshots sin NADA seleccionado (la fila estaba pero sin keyframes elegidos)
    for (size_t s=g_dopeMovVtx.size(); s-- > 0; ){
        bool hay=false;
        for (size_t k=0;k<g_dopeMovVtx[s].vf.size() && !hay;k++)
            hay = g_dopeMovVtx[s].selPos[k] || g_dopeMovVtx[s].selUV[k] || g_dopeMovVtx[s].selNrm[k];
        if (!hay) g_dopeMovVtx.erase(g_dopeMovVtx.begin()+s);
    }
    if (g_dopeMovSnap.empty() && g_dopeMovVtx.empty()) return;
    // UN solo keyframe + curvas + rotar/escalar -> el transform es de sus HANDLES
    g_dopeHXform = false;
    if (modo == TL_MODO_CURVAS && (modoT == DOPE_ROT || modoT == DOPE_ESC) && g_dopeKeySel.size() == 1){
        for (size_t s2=0; s2<g_dopeMovSnap.size() && !g_dopeHXform; s2++){
            DopeMovProp& mp = g_dopeMovSnap[s2];
            if (mp.selFrames.size() != 1) continue;
            AnimProperty* ap = DopeResolverProp(mp.ownerKey, mp.propId, mp.compId); if (!ap) continue;
            for (size_t k=0;k<ap->keyframes.size();k++){
                if (ap->keyframes[k].frame != mp.selFrames[0]) continue;
                if (ap->keyframes[k].Interpolation != KfBezier &&
                    !(k > 0 && ap->keyframes[k-1].Interpolation == KfBezier)) break; // sin handles: nada que tocar
                HandleBakear(ap, k);                       // congelar lo que se ve: los calculados no se pueden girar
                g_dopeHXform = true;
                g_dopeHOwner = mp.ownerKey; g_dopeHProp = mp.propId; g_dopeHComp = mp.compId;
                g_dopeHFrame = ap->keyframes[k].frame; g_dopeHOrig = ap->keyframes[k];
                break;
            }
        }
    }
    // PIVOTE. Eje TIEMPO: centro de los keyframes seleccionados, o el frame ACTUAL segun el menu Pivot.
    // Eje VALOR (solo curvas): SIEMPRE el centro de la seleccion — "Current Frame" solo habla del tiempo.
    { int mn=0x7fffffff, mx2=-0x7fffffff;
      for (size_t s=0;s<g_dopeMovSnap.size();s++) for (size_t i=0;i<g_dopeMovSnap[s].selFrames.size();i++){
          int f=g_dopeMovSnap[s].selFrames[i]; if (f<mn) mn=f; if (f>mx2) mx2=f; }
      g_dopePivotF = (g_dopePivot == DOPE_PIV_CURFRAME) ? (float)CurrentFrame
                   : ((mn<=mx2) ? (float)(mn+mx2)*0.5f : (float)CurrentFrame);
      float vmn=1e30f, vmx=-1e30f;
      for (size_t s=0;s<g_dopeMovSnap.size();s++){ DopeMovProp& mp=g_dopeMovSnap[s];
          for (size_t k=0;k<mp.orig.size();k++){
              bool sel=false; for (size_t i=0;i<mp.selFrames.size();i++) if (mp.selFrames[i]==mp.orig[k].frame){ sel=true; break; }
              if (!sel) continue;
              if (mp.orig[k].value<vmn) vmn=mp.orig[k].value; if (mp.orig[k].value>vmx) vmx=mp.orig[k].value; } }
      g_dopePivotV = (vmn<=vmx) ? (vmn+vmx)*0.5f : 0.0f; }
    // Ctrl+Z: snapshot de las curvas ANTES de transformar. Si el move viene de un Shift+D, el snapshot YA lo tomo
    // DopeDuplicarSeleccion antes de crear las copias -> no pisarlo, o el undo se comeria solo el movimiento y
    // dejaria las copias tiradas.
    if (!g_dopeDup) UndoKeyframesIniciar();
    g_dopeMov = true; g_dopeMovModo = modoT; g_dopeMovEje = DOPE_EJE_LIBRE;
    g_dopeMovVal = (modoT==DOPE_ESC) ? 1.0f : 0.0f;   // escalar arranca en x1; mover en 0; rotar en 0 grados
    g_dopeMovValV = 0.0f;
    g_dopeMovX0 = lastMx; g_dopeMovY0 = lastMy;
    g_dopeVirtX = lastMx; g_dopeVirtY = lastMy;       // el cursor virtual arranca pegado al real
    // ...y lastMouseX/Y (los GLOBALES de donde CheckWarpMouseInViewport saca el dx) tienen que apuntar al mismo
    // lado. Solo los refrescan el click y el propio CheckWarp, y ninguno corre mientras hoveras: sin esto el
    // primer dx sale (donde estas - ULTIMO CLICK), o sea toda la distancia que hoveraste, y el cursor virtual se
    // la come -> el transform arranca corrido esa distancia, para siempre. Va ACA y no en la tecla porque por aca
    // pasan TODOS los caminos (g/s/r, el menu Key y Shift+D).
    lastMouseX = lastMx; lastMouseY = lastMy;
    g_dopeVirtPrimero = true;                         // el dx de este evento todavia es de ANTES del transform
    // ROTAR: angulo inicial mouse->pivote EN PIXELES (los dos ejes tienen unidades y zoom distintos: el giro solo
    // tiene sentido en pantalla, que es donde el usuario lo ve)
    if (modoT == DOPE_ROT){
        float pxp = (float)x + FrameToX(g_dopePivotF), pyp = (float)y + ValueToY(g_dopePivotV);
        g_dopeRotAng0 = atan2f((float)lastMy - pyp, (float)lastMx - pxp);
    }
    g_dopeNumOn = false; g_dopeNumBuf.clear(); g_dopeNumNeg = false;
    g_redraw = true;
}
// re-aplica el transform DESDE EL SNAPSHOT (absoluto -> sin drift). Los frames son ENTEROS: se redondea.
// NO se clampea: los keyframes pueden ir fuera del rango del clip e incluso a frames NEGATIVOS.
// Rotar/escalar los HANDLES del unico keyframe elegido. Se hace en PIXELES (los dos ejes tienen unidades y zoom
// distintos: girar en unidades de dato daria un angulo que no es el que ves) y despues se vuelve a datos.
void Timeline::HandleXformAplicar(){
    AnimProperty* ap = DopeResolverProp(g_dopeHOwner, g_dopeHProp, g_dopeHComp); if (!ap) return;
    size_t i = 0; bool hay = false;
    for (size_t k=0;k<ap->keyframes.size();k++) if (ap->keyframes[k].frame == g_dopeHFrame){ i=k; hay=true; break; }
    if (!hay) return;
    keyFrame& kf = ap->keyframes[i];
    const float ang = (g_dopeMovModo == DOPE_ROT) ? (g_dopeMovVal * 3.14159265f / 180.0f) : 0.0f;
    const float ca = cosf(ang), sa = sinf(ang);
    const float esc = (g_dopeMovModo == DOPE_ESC) ? g_dopeMovVal : 1.0f;
    for (int lado=0; lado<2; lado++){
        float dF = lado ? g_dopeHOrig.outDF : g_dopeHOrig.inDF;   // SIEMPRE desde el original: absoluto, sin drift
        float dV = lado ? g_dopeHOrig.outDV : g_dopeHOrig.inDV;
        float px = dF * pxPerFrame, py = -dV * pxPerUnit;         // a pixeles (Y de pantalla crece hacia abajo)
        float rx = px*ca - py*sa, ry = px*sa + py*ca;             // rotar
        rx *= esc; ry *= esc;                                     // escalar (alarga la distancia)
        float nF = rx / (pxPerFrame>0.01f?pxPerFrame:0.01f);
        float nV = -ry / (pxPerUnit>1e-6f?pxPerUnit:1e-6f);
        // el handle no puede cruzar al otro lado del keyframe: x dejaria de ser monotono y un frame tendria dos
        // valores. Aca solo se frena en el eje del tiempo; el valor gira libre.
        const float minDF = 0.05f;
        if (lado){ if (nF <  minDF) nF =  minDF; } else { if (nF > -minDF) nF = -minDF; }
        if (lado){ kf.outDF = nF; kf.outDV = nV; } else { kf.inDF = nF; kf.inDV = nV; }
    }
    InvalidarAnimYRedraw();
}

void Timeline::DopeXformAplicar(){
    if (!g_dopeMov) return;
    if (g_dopeHXform){ HandleXformAplicar(); return; }   // el transform es de los HANDLES, no de los keyframes
    std::set<std::string> nuevaSel;
    for (size_t s=0;s<g_dopeMovSnap.size();s++){
        DopeMovProp& mp = g_dopeMovSnap[s];
        AnimProperty* ap = DopeResolverProp(mp.ownerKey, mp.propId, mp.compId); if (!ap) continue;
        // 1) los MOVIDOS: frame nuevo desde el ORIGINAL (absoluto -> sin drift al arrastrar)
        std::vector<keyFrame> movidos; std::set<int> ocupados;
        for (size_t k=0;k<mp.orig.size();k++){
            if (!DopeMovSel(mp, k)) continue;
            float f0 = (float)mp.orig[k].frame, v0 = mp.orig[k].value;
            float fn = f0, vn = v0;
            const bool curvas = (modo == TL_MODO_CURVAS);
            const bool ejeX = (g_dopeMovEje != DOPE_EJE_Y), ejeY = (g_dopeMovEje != DOPE_EJE_X);
            if (g_dopeMovModo == DOPE_ESC){                       // ESCALAR desde el pivote
                if (ejeX) fn = g_dopePivotF + (f0 - g_dopePivotF) * g_dopeMovVal;
                if (curvas && ejeY) vn = g_dopePivotV + (v0 - g_dopePivotV) * g_dopeMovVal;
            } else if (g_dopeMovModo == DOPE_ROT){                // ROTAR (solo curvas): en PIXELES, alrededor del pivote
                float px0 = (f0 - g_dopePivotF) * pxPerFrame;     // offset al pivote, en pantalla
                float py0 = -(v0 - g_dopePivotV) * pxPerUnit;     // (Y de pantalla crece hacia abajo)
                float a = g_dopeMovVal * 3.14159265f / 180.0f;
                float ca = cosf(a), sa = sinf(a);
                float px1 = px0*ca - py0*sa, py1 = px0*sa + py0*ca;
                fn = g_dopePivotF + px1 / (pxPerFrame>0.01f?pxPerFrame:0.01f);
                vn = g_dopePivotV - py1 / (pxPerUnit>1e-6f?pxPerUnit:1e-6f);
            } else {                                              // MOVER
                if (ejeX) fn = f0 + g_dopeMovVal;
                if (curvas && ejeY) vn = v0 + g_dopeMovValV;
            }
            keyFrame kf = mp.orig[k]; kf.frame = (int)floorf(fn + 0.5f);                    // frames ENTEROS
            if (curvas) kf.value = vn;                            // el VALOR es continuo: no se redondea
            // al escalar hacia abajo dos keyframes pueden caer en el MISMO frame: gana el ultimo
            for (size_t j=0;j<movidos.size();j++) if (movidos[j].frame == kf.frame){ movidos.erase(movidos.begin()+j); break; }
            movidos.push_back(kf); ocupados.insert(kf.frame);
        }
        // 2) los QUIETOS, salvo los que quedaron pisados por un movido
        ap->keyframes.clear();
        for (size_t k=0;k<mp.orig.size();k++){
            if (DopeMovSel(mp, k) || ocupados.count(mp.orig[k].frame)) continue;   // (un quieto tapado no se ve: se pisa si se suelta ahi)
            ap->keyframes.push_back(mp.orig[k]);
        }
        for (size_t k=0;k<movidos.size();k++){
            ap->keyframes.push_back(movidos[k]);
            nuevaSel.insert(DopeKeyId(mp.ownerKey, mp.propId, mp.compId, movidos[k].frame));
        }
        // el keyframe ACTIVO se identifica por (curva, frame): si es uno de los que se movio, hay que seguirlo o
        // la tarjeta "Keyframe" del panel de propiedades desaparece apenas lo arrastras.
        if (g_dopeActHay && g_dopeActOwner==mp.ownerKey && g_dopeActProp==mp.propId && g_dopeActComp==mp.compId){
            for (size_t k=0;k<mp.selFrames.size();k++){
                if (mp.selFrames[k] != g_dopeActFrame) continue;
                // el que salio de ESE frame original quedo en movidos[k] (mismo orden que selFrames)
                if (k < movidos.size()) g_dopeActFrame = movidos[k].frame;
                break;
            }
        }
        ap->SortKeyFrames();
    }
    // filas VERTICES/UV: solo MOVER en el tiempo (escalar/rotar el timing de vertices es
    // raro). Reasigna ->frame desde el original + delta (absoluto, sin drift) y re-ordena
    // SIN borrar (los punteros del snapshot siguen vivos). Las colisiones se saldan al
    // confirmar. Un frame se mueve si estaba elegido en CUALQUIERA de sus filas.
    if (g_dopeMovModo == DOPE_MOV){
        int df = (int)floorf(g_dopeMovVal + 0.5f);
        for (size_t s=0;s<g_dopeMovVtx.size();s++){
            DopeMovVtxSnap& sn = g_dopeMovVtx[s];
            for (size_t i=0;i<sn.vf.size();i++)
                sn.vf[i]->frame = (sn.selPos[i] || sn.selUV[i] || sn.selNrm[i]) ? sn.origFrame[i] + df : sn.origFrame[i];
            VertexAnimOrdenar(*sn.an);
            for (size_t i=0;i<sn.vf.size();i++){
                if (sn.selPos[i] && !sn.ownerVtx.empty())
                    nuevaSel.insert(DopeKeyId(sn.ownerVtx, AnimVertex, 0, sn.vf[i]->frame));
                if (sn.selUV[i] && !sn.ownerUV.empty())
                    nuevaSel.insert(DopeKeyId(sn.ownerUV, AnimVertex, 1, sn.vf[i]->frame));
                if (sn.selNrm[i] && !sn.ownerNrm.empty())
                    nuevaSel.insert(DopeKeyId(sn.ownerNrm, AnimVertex, 2, sn.vf[i]->frame));
            }
            if (sn.an->target) sn.an->target->skinGeomVersion++; // el frame actual pudo cambiar de pose
        }
    }
    g_dopeKeySel.swap(nuevaSel);
    InvalidarAnimYRedraw();
}
void Timeline::DopeMoveApply(int mx, int my){
    if (!g_dopeMov || g_dopeNumOn) return;             // tipeando un valor exacto: el mouse no interfiere
    const bool curvas = (modo == TL_MODO_CURVAS);
    if (g_dopeMovModo == DOPE_ESC){
        // ESCALAR: razon entre la distancia del mouse al PIVOTE ahora y la que habia al arrancar. En curvas se mide
        // en 2D (el pivote es un punto), en dope sheet solo en el eje del tiempo.
        float pxPiv = FrameToX(g_dopePivotF), pyPiv = ValueToY(g_dopePivotV);
        float ax = (float)(g_dopeMovX0 - x) - pxPiv, bx = (float)(mx - x) - pxPiv;
        float d0, d1;
        if (curvas){
            float ay = (float)(g_dopeMovY0 - y) - pyPiv, by = (float)(my - y) - pyPiv;
            d0 = sqrtf(ax*ax + ay*ay); d1 = sqrtf(bx*bx + by*by);
        } else { d0 = ax; d1 = bx; }
        if (fabsf(d0) < 1.0f) d0 = (d0<0?-1.0f:1.0f);
        g_dopeMovVal = d1 / d0;
    } else if (g_dopeMovModo == DOPE_ROT){
        // ROTAR: angulo barrido por el mouse alrededor del pivote, en pantalla
        float pxp = (float)x + FrameToX(g_dopePivotF), pyp = (float)y + ValueToY(g_dopePivotV);
        float a = atan2f((float)my - pyp, (float)mx - pxp);
        g_dopeMovVal = (a - g_dopeRotAng0) * 180.0f / 3.14159265f;
        while (g_dopeMovVal > 180.0f) g_dopeMovVal -= 360.0f;
        while (g_dopeMovVal < -180.0f) g_dopeMovVal += 360.0f;
    } else {
        g_dopeMovVal  = (float)(mx - g_dopeMovX0) / (pxPerFrame>0.01f?pxPerFrame:0.01f);       // frames
        g_dopeMovValV = curvas ? -(float)(my - g_dopeMovY0) / (pxPerUnit>1e-6f?pxPerUnit:1e-6f) // valor (Y invertida)
                               : 0.0f;
        // CTRL apretado: SOLO horizontal (tiempo) o SOLO vertical (valor), segun para donde se movio mas el mouse
        if (curvas && LCtrlPressed && g_dopeMovEje == DOPE_EJE_LIBRE){
            if (abs(mx - g_dopeMovX0) >= abs(my - g_dopeMovY0)) g_dopeMovValV = 0.0f; else g_dopeMovVal = 0.0f;
        }
    }
    DopeXformAplicar();
}
// X / Y durante un transform de curvas: limita a un eje (apretar de nuevo el mismo eje libera)
void Timeline::DopeCiclarEje(int eje){
    if (!g_dopeMov || modo != TL_MODO_CURVAS) return;
    g_dopeMovEje = (g_dopeMovEje == eje) ? DOPE_EJE_LIBRE : eje;
    DopeXformAplicar();
}
// valor NUMERICO exacto: mover N frames (G 2) o escalar xN (S 2 -> lo que duraba 2 frames pasa a durar 4)
void Timeline::DopeNumAplicar(){
    if (!g_dopeMov) return;
    float v; if (!W3dEvalExpr(g_dopeNumBuf, v)) return;  // expresion incompleta: esperar mas teclas
    float val = g_dopeNumNeg ? -v : v;
    // MOVER limitado al eje Y (curvas): el numero es el VALOR. En cualquier otro caso son frames / factor / grados.
    if (g_dopeMovModo == DOPE_MOV && modo == TL_MODO_CURVAS && g_dopeMovEje == DOPE_EJE_Y){
        g_dopeMovValV = val; g_dopeMovVal = 0.0f;
    } else {
        g_dopeMovVal = val;
        if (g_dopeMovModo == DOPE_MOV) g_dopeMovValV = 0.0f;
    }
    DopeXformAplicar();
}
void Timeline::DopeMoveConfirm(){
    // filas VERTICES/UV/NORMALES: los frames ya estan en su posicion final. Resolver COLISIONES: si
    // un keyframe movido cayo sobre uno QUIETO, gana el movido (el quieto se borra ENTERO,
    // con todas sus capas: dos cuadros no pueden compartir frame). Dos movidos no chocan
    // entre si (un MOVER preserva su separacion relativa).
    // ...SALVO con el toggle "Solo vertices seleccionados" (sn.selectivo): ahi el move es POR VERTICE
    // (VertexAnimMoverKeysSelectivo). Como el nucleo necesita el estado PREVIO al move para evaluar a los NO
    // seleccionados, primero se DESHACE la previsualizacion (cada frame vuelve a su cuadro original) y despues se
    // le pasa la lista origen->destino. Los VertexFrame movidos los libera el nucleo: despues de esto los
    // punteros de sn.vf ya no valen (por eso g_dopeMovVtx se limpia enseguida).
    for (size_t s=0;s<g_dopeMovVtx.size();s++){
        DopeMovVtxSnap& sn = g_dopeMovVtx[s]; if (!sn.an) continue;
        if (sn.selectivo){
            std::vector<int> orig, dest;
            for (size_t i=0;i<sn.vf.size();i++){
                if (!(sn.selPos[i] || sn.selUV[i] || sn.selNrm[i])) continue;
                if (sn.vf[i]->frame == sn.origFrame[i]) continue;      // no se movio: nada que hacer
                orig.push_back(sn.origFrame[i]); dest.push_back(sn.vf[i]->frame);
            }
            for (size_t i=0;i<sn.vf.size();i++) sn.vf[i]->frame = sn.origFrame[i];  // deshacer la preview
            VertexAnimOrdenar(*sn.an);
            if (!orig.empty())
                VertexAnimMoverKeysSelectivo(*sn.an, orig, dest, sn.mask.empty()?NULL:&sn.mask[0]);
            if (sn.an->target) sn.an->target->skinGeomVersion++;
            continue;
        }
        std::set<VertexFrame*> movidos; std::set<int> framesMovidos;
        for (size_t i=0;i<sn.vf.size();i++) if (sn.selPos[i] || sn.selUV[i] || sn.selNrm[i]){ movidos.insert(sn.vf[i]); framesMovidos.insert(sn.vf[i]->frame); }
        std::vector<VertexFrame*>& F = sn.an->frames;
        for (size_t i=F.size(); i-- > 0; ){
            if (movidos.count(F[i])) continue;                 // el movido se queda
            if (!framesMovidos.count(F[i]->frame)) continue;   // no hay colision
            VertexFrame* f=F[i];
            delete[] (GLfloat*)f->positions; delete[] (GLbyte*)f->normals;
            delete[] (GLfloat*)f->uvs;   // (antes se fugaba la capa UV del pisado)
            delete f;
            F.erase(F.begin()+i);
        }
        VertexAnimOrdenar(*sn.an); sn.an->frameCount = (int)F.size();
        if (sn.an->target) sn.an->target->skinGeomVersion++;
    }
    g_dopeMovVtx.clear();
    g_dopeMov=false; g_dopeMovModo=0; g_dopeMovEje=DOPE_EJE_LIBRE; g_dopeMovSnap.clear(); g_dopeHXform=false;
    ViewPortClickDown = false;   // el warp lo dejaba trabado para no perder el foco (ver controles.cpp)
    g_dopeDup=false; g_dopeDupPre.clear(); g_dopeDupVtxPre.clear();   // si venia de Shift+D, la duplicacion queda firme
    g_dopeNumOn=false; g_dopeNumBuf.clear();
    UndoKeyframesConfirmar(); g_redraw=true;
}
// tecla durante un transform de keyframes: arma el valor numerico (acepta matematica: 2, (3+1)/2, ...).
// '-' invierte el signo; backspace borra. Devuelve true si consumio la tecla.
bool Timeline::DopeNumChar(int c){
    if (!g_dopeMov) return false;
    if (c == 8){ if (!g_dopeNumBuf.empty()) g_dopeNumBuf.erase(g_dopeNumBuf.size()-1); else g_dopeNumNeg = false; }
    else if (c == '-'){ g_dopeNumNeg = !g_dopeNumNeg; }
    else if ((c>='0'&&c<='9') || c=='.' || c=='(' || c==')' || c=='*' || c=='/' || c=='+'){ g_dopeNumBuf += (char)c; }
    else return false;
    g_dopeNumOn = !(g_dopeNumBuf.empty() && !g_dopeNumNeg);
    if (!g_dopeNumOn){ DopeMoveApply(g_dopeVirtX, g_dopeVirtY); }  // se vacio -> vuelve el mouse (el virtual)
    else DopeNumAplicar();
    g_redraw = true; return true;
}
// ---- View > Frame Selected: encuadra el timeline en los keyframes SELECCIONADOS ----
// Varios keyframes: el primero pegado al borde IZQUIERDO y el ultimo al DERECHO (ajusta zoom + paneo).
// Uno solo: lo pone en el MEDIO del timeline y NO toca el zoom.
// primer y ultimo frame con keyframe SELECCIONADO. false = no hay ninguno.
bool Timeline::DopeRangoSeleccion(int& mn, int& mx) const {
    mn=0x7fffffff; mx=-0x7fffffff;
    for (size_t r=0;r<dopeRows.size();r++){ const DopeRow& d=dopeRows[r]; if (d.propId<0) continue;
        for (size_t k=0;k<d.keys.size();k++){
            if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, d.keys[k]))) continue;
            if (d.keys[k]<mn) mn=d.keys[k]; if (d.keys[k]>mx) mx=d.keys[k]; } }
    return mn <= mx;
}
// ---- RANGO de lo que se ve en el editor de CURVAS, en DATOS (frames y valores) ----
// Separado del zoom a proposito: es la parte que se puede testear sin ventana (script 'curvfit') y la que se
// habia quedado corta. Dos cosas que ANTES faltaban y rompian el encuadre:
//   1) las filas de VERTEX ANIM (Vertices/UV/Normales) NO son AnimProperty: DopeResolverProp devuelve NULL para
//      ellas, asi que el encuadre no encontraba ni un keyframe y se iba sin hacer nada (por eso "no funcionaba"
//      justo en las curvas de vertex anim, que son las filas nuevas);
//   2) sin NADA seleccionado no habia rango y tampoco pasaba nada.
//   3) los HANDLES bezier quedaban afuera (reporte del dueno: "el auto encuadre de las curvas enfoca bien la
//      curva, pero no enfoca bien los handlers que quedan fuera de la vista"). El rango salia de los KEYFRAMES,
//      asi que la curva entraba pero sus manijas -que es JUSTO lo que el usuario va a agarrar- se cortaban.
//      Que handle entra al rango: EL QUE SE DIBUJA, ni uno mas. Y eso son dos condiciones, las mismas dos que
//      manda RenderCurvas: el keyframe tiene que estar SELECCIONADO (solo del seleccionado se dibujan handles)
//      y el tramo de ESE lado tiene que ser BEZIER (HandleVisible: una recta no tiene nada que ajustar). Donde
//      CAE el handle lo decide AnimProperty::HandleEfectivo, el mismo del core que usan la evaluacion y el
//      dibujo -> lo que se encuadra es exactamente lo que se ve, tambien con los handles CALCULADOS
//      (HVector/HAuto/HAutoClamped), que no estan guardados en ningun lado.
//      NO se replica el piso EN PIXELES de HandlePos (el que estira a 14px un handle diminuto para poder
//      agarrarlo): eso depende del zoom y el zoom es justo lo que estamos por calcular -- seria circular. Un
//      handle asi de corto entra igual, porque su keyframe entra.
// soloSel=true mide solo los keyframes SELECCIONADOS; false, las curvas visibles ENTERAS.
bool Timeline::CurvaRangoVista(bool soloSel, float& fmn, float& fmx, float& vmn, float& vmx) const {
    fmn = 1e30f; fmx = -1e30f; vmn = 1e30f; vmx = -1e30f;
    bool hay = false;
    // rango que ocupan los HANDLES, medido APARTE: se mezcla al final y con tope (ver el paso 3)
    float hfmn = 1e30f, hfmx = -1e30f, hvmn = 1e30f, hvmx = -1e30f;
    bool hayH = false;
    AnimProperty tmp;   // curva temporal de una fila de vertex anim (LOCAL: cada fila arma la suya)
    // 1) rango de TIEMPO + los valores de los propios keyframes (y de paso, el de sus handles dibujados)
    for (size_t r=0;r<dopeRows.size();r++){
        const DopeRow& d = dopeRows[r]; if (!FilaEsCurvaVisible(d)) continue;
        const AnimProperty* ap = VertexCanalTemp(d.ownerKey, tmp) ? &tmp
                               : DopeResolverProp(d.ownerKey, d.propId, d.compId, false);   // encuadrar solo mide
        if (!ap) continue;
        for (size_t i=0;i<ap->keyframes.size();i++){
            const keyFrame& kf = ap->keyframes[i];
            bool sel = g_dopeKeySel.count(DopeKeyId(d.ownerKey,d.propId,d.compId,kf.frame)) > 0;
            if (soloSel && !sel) continue;
            if ((float)kf.frame < fmn) fmn = (float)kf.frame;
            if ((float)kf.frame > fmx) fmx = (float)kf.frame;
            if (kf.value < vmn) vmn = kf.value;
            if (kf.value > vmx) vmx = kf.value;
            hay = true;
            if (!sel) continue;                       // sin seleccionar no se le dibujan handles
            for (int lado=0; lado<2; lado++){
                bool salida = (lado==1);
                if (!HandleVisible(ap, i, salida)) continue;   // ese tramo no es bezier: no hay handle
                float dF, dV; ap->HandleEfectivo(i, salida, dF, dV);
                float hf = (float)kf.frame + dF, hv = kf.value + dV;
                if (hf < hfmn) hfmn = hf;
                if (hf > hfmx) hfmx = hf;
                if (hv < hvmn) hvmn = hv;
                if (hv > hvmx) hvmx = hv;
                hayH = true;
            }
        }
    }
    if (!hay) return false;
    // 2) el rango de VALOR NO se puede sacar solo de los keyframes: una bezier se PASA de ellos (sobrepico) y la
    //    curva saldria recortada. Se mide sobre LA CURVA, muestreada como se dibuja. El paso se acota a 512
    //    muestras: encuadrar una animacion de 10.000 frames no puede costar 10.000 EvalF por fila.
    float paso = (fmx - fmn) / 512.0f; if (paso < 1.0f) paso = 1.0f;
    for (size_t r=0;r<dopeRows.size();r++){
        const DopeRow& d = dopeRows[r]; if (!FilaEsCurvaVisible(d)) continue;
        const AnimProperty* ap = VertexCanalTemp(d.ownerKey, tmp) ? &tmp
                               : DopeResolverProp(d.ownerKey, d.propId, d.compId, false);   // encuadrar solo mide
        if (!ap || ap->keyframes.empty()) continue;
        if (soloSel){   // con seleccion, solo aportan las filas que tienen algun keyframe elegido
            bool tieneSel = false;
            for (size_t i=0;i<ap->keyframes.size() && !tieneSel;i++)
                tieneSel = g_dopeKeySel.count(DopeKeyId(d.ownerKey,d.propId,d.compId,ap->keyframes[i].frame)) > 0;
            if (!tieneSel) continue;
        }
        for (float f = fmn; f <= fmx + 1e-3f; f += paso){
            float v = ap->EvalF(f, 0.0f);
            if (v < vmn) vmn = v; if (v > vmx) vmx = v;
        }
    }
    // 3) recien AHORA entran los handles, y con TOPE. Va despues del muestreo a proposito: el rango "base"
    //    contra el que se mide el tope tiene que incluir el sobrepico de la bezier, que es parte de la curva.
    //    EL TOPE: un handle enorme (uno que el usuario arrastro lejisimos) mide mucho mas que la curva, y
    //    meterlo entero deja la animacion aplastada en una linea -- justo lo contrario de encuadrar. Se deja
    //    crecer el encuadre como mucho DOS VECES lo que ya ocupa la curva por cada lado (o sea, el rango no
    //    puede mas que quintuplicarse): asi a la curva siempre le queda como minimo un quinto de la vista, y
    //    del handle descomunal se ve por donde ARRANCA. El factor no es mas chico porque un handle NORMAL ya
    //    se pasa del rango de la curva: el sobrepico de una bezier ronda el 45% del brazo de la manija, asi
    //    que con un tope de "una vez" un handle comun quedaria cortado al pedo.
    //    RANGO BASE DEGENERADO (un solo keyframe elegido, o una curva chata: todos los valores iguales) -> NO
    //    hay tope: no queda ninguna curva por aplastar y el handle es lo unico que hay para mostrar. Es el caso
    //    tipico de "elijo UN keyframe y aprieto punto para agarrarle las manijas".
    if (hayH){
        const float kExt = 2.0f;                     // cuanto puede crecer por lado, en veces el rango base
        const float kSinTope = 1e30f;
        float topeF = (fmx - fmn > 1e-6f) ? (fmx - fmn) * kExt : kSinTope;
        float topeV = (vmx - vmn > 1e-6f) ? (vmx - vmn) * kExt : kSinTope;
        float lfmn = fmn - topeF, lfmx = fmx + topeF;   // hasta donde se puede estirar cada borde
        float lvmn = vmn - topeV, lvmx = vmx + topeV;
        if (hfmn < fmn) fmn = (hfmn > lfmn) ? hfmn : lfmn;
        if (hfmx > fmx) fmx = (hfmx < lfmx) ? hfmx : lfmx;
        if (hvmn < vmn) vmn = (hvmn > lvmn) ? hvmn : lvmn;
        if (hvmx > vmx) vmx = (hvmx < lvmx) ? hvmx : lvmx;
    }
    return true;
}
// aplica un rango (frames, valores) al zoom/paneo de los DOS ejes.
void Timeline::CurvaEncuadrar(float fmn, float fmx, float vmn, float vmx){
    // la barra de herramientas de abajo (aparece con keyframes elegidos) TAPA el fondo del strip: el encuadre
    // usa solo lo que queda visible arriba de ella
    const int tbH = (!ToolButtons.empty() && ToolbarVisible()) ? ToolbarHeight() : 0;
    float visW = (float)(width - panelW), visH = (float)(height - stripY - tbH);
    if (visW < 1.0f || visH < 1.0f) return;
    const float margen = (float)GlobalScale * 12.0f;
    // --- eje TIEMPO: un solo frame -> al MEDIO y sin tocar el zoom (no hay ancho que encuadrar) ---
    if (fmx - fmn < 0.5f) viewStartF = fmn - (visW*0.5f)/(pxPerFrame>0.01f?pxPerFrame:0.01f);
    else {
        float util = visW - margen*2.0f; if (util < 1.0f) util = 1.0f;
        pxPerFrame = ClampZoomH(util / (fmx - fmn));
        viewStartF = fmn - margen / pxPerFrame;
    }
    // --- eje VALOR: la vista se CENTRA en el medio de los valores (encuadrar es justamente mover el centro) ---
    viewCenterV = (vmn + vmx) * 0.5f;
    float spanV = vmx - vmn;
    // rango degenerado (UN solo key, o una curva chata: dos keys con el mismo valor): sin un margen MINIMO el
    // zoom se iria al techo y no se veria mas que una linea gorda. Se abre 1 unidad alrededor del valor -
    // suficiente para una curva de vertex anim (valores 0..1, indice de pose) y razonable para una de objeto.
    if (spanV < 1e-6f) spanV = 1.0f;
    float utilV = visH - margen*2.0f; if (utilV < 1.0f) utilV = 1.0f;
    pxPerUnit = ClampZoomV(utilV / spanV);
    // ValueToY centra en el medio del strip ENTERO: se corre el centro para que el medio de los valores caiga
    // en el medio de la parte visible (arriba de la barra)
    viewCenterV -= (float)(CentroVertical() - (stripY + height - tbH) / 2) / pxPerUnit;
    g_redraw = true;
}
// En CURVAS encuadra los DOS EJES: el tiempo y el VALOR (para eso el zoom es de 2 ejes).
// Con keyframes SELECCIONADOS encuadra la SELECCION; sin seleccion, la(s) curva(s) visible(s) entera(s).
void Timeline::CurvaFrameSelected(){
    float fmn, fmx, vmn, vmx;
    if (!CurvaRangoVista(true, fmn, fmx, vmn, vmx) &&
        !CurvaRangoVista(false, fmn, fmx, vmn, vmx)) return;   // ni seleccion ni curvas: nada que encuadrar
    CurvaEncuadrar(fmn, fmx, vmn, vmx);
}
// El "Frame Selected" del DOPE SHEET no tiene el problema de los handles: ahi no se dibujan (una fila del dope
// son ROMBOS en su frame, no hay eje de valor ni manijas que agarrar), asi que el unico rango que existe es el de
// los frames. El de CURVAS -sea auto-encuadre, sea "enfocar lo seleccionado"- pasa por CurvaRangoVista, que es
// donde entran los handles: los dos caminos comparten esa funcion y no pueden divergir.
void Timeline::DopeFrameSelected(){
    if (modo == TL_MODO_CURVAS){ CurvaFrameSelected(); return; }
    int mn, mx;
    if (!DopeRangoSeleccion(mn, mx)) return;      // sin keyframes seleccionados: no hay nada que encuadrar
    float visW = (float)(width - panelW);         // ancho UTIL (el panel no muestra frames)
    if (visW < 1.0f) return;
    if (mn == mx){                                // uno solo: al medio, sin tocar el zoom
        viewStartF = (float)mn - (visW*0.5f) / (pxPerFrame>0.01f?pxPerFrame:0.01f);
    } else {
        float margen = (float)GlobalScale * 12.0f;               // aire para que el rombo del borde entre entero
        float util = visW - margen*2.0f; if (util < 1.0f) util = 1.0f;
        pxPerFrame = ClampZoomH(util / (float)(mx - mn));
        viewStartF = (float)mn - margen / pxPerFrame;
    }
    g_redraw = true;
}

// Entrada numerica del transform de keyframes. Gemela de NumInputChar (LayoutInput.cpp): cada plataforma le
// pasa los caracteres tipeados (PC: SDL_TEXTINPUT -> respeta el layout del teclado, parentesis incluidos;
// Symbian: su keypad). Devuelve false si no hay transform de keyframes en curso.
bool DopeNumInputChar(int c){
    return g_tlActivo ? g_tlActivo->DopeNumChar(c) : false;
}
// Hay un transform de keyframes en curso? Lo pregunta controles.cpp para ENVOLVER el cursor dentro del timeline
// (como al panear): sin eso, al llegar al borde de la pantalla te quedas sin recorrido y no podes seguir escalando.
bool DopeXformActivo(){ return g_dopeMov; }
// barra de espacio: play/pausa desde CUALQUIER viewport (no hace falta tener el timeline enfocado)
bool TL_TogglePlay(){ if (!g_tlActivo) return false; g_tlActivo->TogglePlay(+1); return true; }
// ---- lapiz (+flechas) de Symbian sobre el TIMELINE: navega KEYFRAMES. Devuelve false si el timeline no esta
// activo, para que el llamador siga con lo suyo (ciclar objetos/huesos).
bool DopeNavTecla(int dir){
    if (!g_tlActivo || !viewPortActive || !viewPortActive->isLeaf() || viewPortActive->ViewportKind() != 5) return false;
    switch (dir){
        case 0: return g_tlActivo->DopeSelAvanzar(1, false);  // lapiz solo: saca el actual y pasa al siguiente
        case 1: return g_tlActivo->DopeSelAvanzar(1, true);   // + derecha: mantiene y suma el siguiente
        case 2: return g_tlActivo->DopeSelAvanzar(-1, true);  // + izquierda: mantiene y suma el anterior
        case 3: return g_tlActivo->DopeSelTodoNav();          // + arriba: todos
        default: return g_tlActivo->DopeSelNadaNav();         // + abajo: ninguno
    }
}

// ---- KEYFRAME ACTIVO: lo lee/escribe la tarjeta "Keyframe" del panel de propiedades ----
// Se devuelve la curva VIVA y el indice. El indice se resuelve por FRAME cada vez (no se cachea): el vector de
// keyframes se reordena en cuanto moves algo, asi que guardar un indice o un puntero seria colgarse.
AnimProperty* DopeKeyframeActivo(int* idx, bool editar){
    if (!g_dopeActHay || !g_tlActivo) return NULL;
    // tiene que seguir SELECCIONADO y existir; si no, la tarjeta se va
    if (!g_dopeKeySel.count(DopeKeyId(g_dopeActOwner, g_dopeActProp, g_dopeActComp, g_dopeActFrame))){
        g_dopeActHay = false; return NULL;
    }
    AnimProperty* ap = DopeResolverProp(g_dopeActOwner, g_dopeActProp, g_dopeActComp, editar);
    if (!ap){ g_dopeActHay = false; return NULL; }
    for (size_t k=0;k<ap->keyframes.size();k++)
        if (ap->keyframes[k].frame == g_dopeActFrame){ if (idx) *idx = (int)k; return ap; }
    g_dopeActHay = false; return NULL;
}
// nombre del canal del keyframe activo ("X Location", ...) para el titulo de la tarjeta
std::string DopeKeyframeActivoCanal(){
    if (!g_dopeActHay || !g_tlActivo) return "";
    for (size_t r=0;r<g_tlActivo->dopeRows.size();r++){
        const Timeline::DopeRow& d = g_tlActivo->dopeRows[r];
        if (d.propId==g_dopeActProp && d.compId==g_dopeActComp && d.ownerKey==g_dopeActOwner) return d.nombre;
    }
    return "";
}
// el frame del keyframe activo cambio (lo movio la tarjeta): seguirlo, y arrastrar la seleccion con el
void DopeKeyframeActivoReFrame(int nuevoFrame){
    if (!g_dopeActHay) return;
    g_dopeKeySel.erase(DopeKeyId(g_dopeActOwner, g_dopeActProp, g_dopeActComp, g_dopeActFrame));
    g_dopeActFrame = nuevoFrame;
    g_dopeKeySel.insert(DopeKeyId(g_dopeActOwner, g_dopeActProp, g_dopeActComp, g_dopeActFrame));
}
// Cancela el transform de keyframes desde AFUERA. Lo llama el Tab (que cicla de viewport por encima del ruteo a
// los viewports): irse del timeline con un transform a medio hacer dejaba ViewPortClickDown trabado en true -> el
// foco quedaba congelado y no se podia volver a cambiar de viewport, y el transform seguia vivo y huerfano.
void DopeXformCancelar(){ if (g_tlActivo && g_dopeMov) g_tlActivo->DopeMoveCancel(); if (g_tlActivo && g_tlActivo->TiraActiva()) g_tlActivo->TiraCancelar(); }
// ...y aceptarlo. Lo llaman el click IZQUIERDO (PC) y el OK/Enter de Symbian, igual que el transform del 3D.
void DopeXformAceptar(){ if (g_tlActivo && g_dopeMov) g_tlActivo->DopeMoveConfirm(); if (g_tlActivo && g_tlActivo->TiraActiva()) g_tlActivo->TiraConfirmar(); }
// readout del transform de keyframes (lo dibuja Render arriba del strip)
std::string Timeline::DopeTextoTransform() const {
    if (!g_dopeMov) return "";
    const char* op = T((g_dopeMovModo==DOPE_ESC) ? "Scale" : (g_dopeMovModo==DOPE_ROT) ? "Rotate" : "Move");
    const std::string uniS = (g_dopeMovModo==DOPE_ESC) ? std::string("x") : std::string(" ") + T((g_dopeMovModo==DOPE_ROT) ? "deg" : "frames");
    const std::string ejeS = (g_dopeMovEje==DOPE_EJE_X) ? std::string("  ") + T("[X: time]") : (g_dopeMovEje==DOPE_EJE_Y) ? std::string("  ") + T("[Y: value]") : std::string();
    const char* uni = uniS.c_str();
    const char* eje = ejeS.c_str();
    const char* piv = T((g_dopePivot==DOPE_PIV_CURFRAME) ? "current frame" : "center");
    char b[256];
    if (g_dopeNumOn){
        float v; bool ok = W3dEvalExpr(g_dopeNumBuf, v);
        std::string expr = (g_dopeNumNeg?"-[":"[") + g_dopeNumBuf + "]";
        if (ok) snprintf(b, sizeof b, "%s: %s = %.2f%s%s", op, expr.c_str(), g_dopeNumNeg?-v:v, uni, eje);
        else    snprintf(b, sizeof b, "%s: %s = ?%s", op, expr.c_str(), eje);
    }
    else if (g_dopeMovModo==DOPE_ESC) snprintf(b, sizeof b, T("Scale: %.2fx  (pivot %s)%s"), g_dopeMovVal, piv, eje);
    else if (g_dopeMovModo==DOPE_ROT) snprintf(b, sizeof b, T("Rotate: %.1f deg  (pivot %s)"), g_dopeMovVal, piv);
    else if (modo==TL_MODO_CURVAS)    snprintf(b, sizeof b, T("Move: %d frames, %.3f value%s"), (int)floorf(g_dopeMovVal+0.5f), g_dopeMovValV, eje);
    else                              snprintf(b, sizeof b, T("Move: %d frames"), (int)floorf(g_dopeMovVal+0.5f));
    return std::string(b);
}

// Lo llama ~Timeline. El estado del transform de keyframes y del handle es de ARCHIVO (no del objeto): si el
// timeline se va con algo a medio hacer hay que apagarlo, o queda prendido para el proximo (y ViewPortClickDown
// trabado dejaria el foco congelado en un viewport que ya no existe).
void Timeline::DestruirEstadoTimeline(){
    if (g_tlActivo != this) return;
    if (g_dopeMov) DopeMoveCancel();
    if (g_hOn) HandleSoltar();
    ViewPortClickDown = false;
    g_tlActivo = NULL;
}

void Timeline::DopeMoveCancel(){
    if (!g_dopeMov) return;
    std::set<std::string> vieja;
    if (g_dopeDup){
        // el move venia de un Shift+D: cancelar tiene que volver a ANTES de duplicar (borrar las copias), no solo
        // devolverlas a donde nacieron.
        for (size_t s=0;s<g_dopeDupPre.size();s++){
            DopeMovProp& mp = g_dopeDupPre[s];
            AnimProperty* ap = DopeResolverProp(mp.ownerKey, mp.propId, mp.compId); if (ap) ap->keyframes = mp.orig;
        }
        // fila VERTEX: restaurar el blob de ANTES de clonar (borra las copias y devuelve
        // lo que un clon hubiera pisado). Deserializar libera lo vivo y reconstruye.
        for (size_t s=0;s<g_dopeDupVtxPre.size();s++){
            DopeDupVtxPre& pre = g_dopeDupVtxPre[s]; if (!pre.an) continue;
            VertexAnimDeserializar(*pre.an, pre.blob.empty()?0:&pre.blob[0], pre.blob.size());
            if (pre.an->target) pre.an->target->skinGeomVersion++;
        }
        g_dopeKeySel.clear();
        g_dopeDup = false; g_dopeDupPre.clear(); g_dopeDupVtxPre.clear();
        g_dopeMov = false; g_dopeMovSnap.clear(); g_dopeMovVtx.clear(); g_dopeHXform=false;
        ViewPortClickDown = false;
        UndoKeyframesConfirmar(); // cierra el snapshot pendiente (no hubo cambio -> se descarta solo)
        InvalidarAnimYRedraw(); return;
    }
    for (size_t s=0;s<g_dopeMovSnap.size();s++){
        DopeMovProp& mp = g_dopeMovSnap[s];
        // mp.orig es de ANTES del transform: restaurarlo devuelve tambien los handles (el bakeo del arranque incluido)
        AnimProperty* ap = DopeResolverProp(mp.ownerKey, mp.propId, mp.compId);
        if (ap){
            // un PISADO (lo tapado por un Pegar) que comparte cuadro con lo elegido no vuelve: Esc deja lo pegado donde
            // se pego, pisando lo que habia (como antes de que existieran los pisados)
            ap->keyframes.clear();
            for (size_t k=0;k<mp.orig.size();k++){
                if (k < mp.quieto.size() && mp.quieto[k]){
                    bool tapado=false; for (size_t i=0;i<mp.selFrames.size();i++) if (mp.selFrames[i]==mp.orig[k].frame){ tapado=true; break; }
                    if (tapado) continue;
                }
                ap->keyframes.push_back(mp.orig[k]);
            }
        }
        for (size_t i=0;i<mp.selFrames.size();i++) vieja.insert(DopeKeyId(mp.ownerKey, mp.propId, mp.compId, mp.selFrames[i]));
    }
    // filas VERTICES/UV: devolver cada frame a su valor original y re-ordenar (nada se borro)
    for (size_t s=0;s<g_dopeMovVtx.size();s++){
        DopeMovVtxSnap& sn = g_dopeMovVtx[s]; if (!sn.an) continue;
        for (size_t i=0;i<sn.vf.size();i++){
            sn.vf[i]->frame = sn.origFrame[i];
            if (sn.selPos[i] && !sn.ownerVtx.empty())
                vieja.insert(DopeKeyId(sn.ownerVtx, AnimVertex, 0, sn.origFrame[i]));
            if (sn.selUV[i] && !sn.ownerUV.empty())
                vieja.insert(DopeKeyId(sn.ownerUV, AnimVertex, 1, sn.origFrame[i]));
            if (sn.selNrm[i] && !sn.ownerNrm.empty())
                vieja.insert(DopeKeyId(sn.ownerNrm, AnimVertex, 2, sn.origFrame[i]));
        }
        VertexAnimOrdenar(*sn.an);
        if (sn.an->target) sn.an->target->skinGeomVersion++;
    }
    g_dopeMovVtx.clear();
    g_dopeKeySel.swap(vieja);
    g_dopeMov=false; g_dopeMovModo=0; g_dopeMovEje=DOPE_EJE_LIBRE; g_dopeMovSnap.clear(); g_dopeHXform=false;
    g_dopeNumOn=false; g_dopeNumBuf.clear(); g_dopeNumNeg=false;
    ViewPortClickDown = false;
    UndoKeyframesConfirmar(); // cierra el snapshot que abrio DopeMoveStart (sin cambios -> se descarta solo)
    InvalidarAnimYRedraw();
}

// ------------------------------------------------------------------ acciones
void Timeline::TogglePlay(int dir){
    bool arranca = !(PlayAnimation && AnimPlayDir==dir);
    if (arranca && AnimEsJuego && dir > 0) {
        // NO SE JUEGA CON TEXTURAS A MEDIO CARGAR: si la cola diferida tiene algo,
        // se vacia ACA (bloqueando, con cartel) antes de arrancar. Reporte del
        // dueno: "no se puede dar play hasta que carguen todas las texturas".
        extern bool JuegoEsperarTexturas();
        if (!JuegoEsperarTexturas()) { g_redraw = true; return; }
        // dar PLAY al juego: overlays off SOLO al INICIAR (sim aun no cargada) y
        // la vista de camara vuelve a su encuadre (la inspeccion de pausa se resetea)
        extern bool SimActiva(); extern void JuegoPrepararViewports(bool);
        JuegoPrepararViewports(!SimActiva());
    }
    if (!arranca) PlayAnimation=false; else { PlayAnimation=true; AnimPlayDir=dir; }
    g_redraw = true;
}
void Timeline::GotoStart(){ CurrentFrame = StartFrame; g_redraw=true; }
void Timeline::GotoEnd(){
    // (modo juego: no hay Fin al que ir)
    if (!AnimEsJuego) CurrentFrame = EndFrame;
    g_redraw=true;
}
// el cuadro anterior / siguiente desde CUALQUIER lado (las flechas del viewport 3D): lo mismo que el <| |> del timeline
void W3dPasoFrame(int d);
void Timeline::StepFrame(int d){ W3dPasoFrame(d); }
void W3dPasoFrame(int d){
    // simulacion en PAUSA: > avanza el juego FRAME A FRAME (simula uno); < no hace nada: el juego no
    // graba su pasado
    { extern bool SimActiva(); extern bool SimStep(int);
      if (SimActiva() && !PlayAnimation && SimStep(d)) { g_redraw = true; return; } }
    CurrentFrame += d;
    // en PAUSA (animacion normal) se scrubbea LIBRE, tambien FUERA de inicio/fin y a
    // negativos: los keyframes pueden caer fuera del rango y hay que poder llegar a ellos.
    // Reproduciendo o en modo juego, si se clampea al rango.
    if (PlayAnimation || AnimEsJuego){
        if (CurrentFrame < StartFrame) CurrentFrame = StartFrame;
        if (CurrentFrame > EndFrame && !AnimEsJuego) CurrentFrame = EndFrame;
    }
    // "Juego" SIN partida: lo que hay es el estado base (su frame 1)
    { extern bool SimActiva(); if (ActiveAnimKind == 2 && !SimActiva()) CurrentFrame = StartFrame; }
    g_redraw=true;
}
void Timeline::StepKeyframe(int d){
    std::vector<int> kfs; CollectKeyframes(kfs);
    if (kfs.empty()) return;   // no hay a donde saltar: quieto (mover el frame no es lo que se pidio)
    if (d>0){ for (size_t i=0;i<kfs.size();i++) if (kfs[i]>CurrentFrame){ CurrentFrame=kfs[i]; g_redraw=true; return; } }
    else    { for (int i=(int)kfs.size()-1;i>=0;i--) if (kfs[i]<CurrentFrame){ CurrentFrame=kfs[i]; g_redraw=true; return; } }
}
// tope de paneo hacia la izquierda: ~1 pantalla hacia el lado negativo (comodo, nunca se pierde)
float Timeline::MinView() const {
    float px = (pxPerFrame>0.01f?pxPerFrame:0.01f);
    return -((float)width / px) * 0.85f;
}
void Timeline::PanFrames(float d){ viewStartF += d; if (viewStartF < MinView()) viewStartF = MinView(); g_redraw=true; }
// centro EXACTO del timeline = centro del STRIP (la zona de frames), NO del viewport: el panel del dope sheet
// ocupa la izquierda y no es parte de la linea de tiempo.
int Timeline::CentroTimeline() const { return panelW + (width - panelW)/2; }
void Timeline::ZoomBy(float factor, int cxLocal){
    float fb = XToFrame((float)cxLocal);   // frame bajo cxLocal (XToFrame ya descuenta panelW)
    pxPerFrame *= factor;
    pxPerFrame = ClampZoomH(pxPerFrame);
    // invertir FrameToX: x = panelW + (f - viewStartF)*px  ->  viewStartF = f - (x - panelW)/px.
    // (sin el -panelW el zoom se corria: era el bug que metio el panel)
    viewStartF = fb - ((float)cxLocal - (float)panelW) / pxPerFrame;
    if (viewStartF < MinView()) viewStartF = MinView();
    g_redraw = true;
}
// ---------------------------------------------------------------------------------------------------------
//  LA VISTA DEL TIMELINE VUELVE DE UN ARCHIVO (la escribe GuardarW3D en la hoja del layout; ver alla el
//  comentario grande del reparto .w3d / config.ini). Un .w3d se edita a mano y ADEMAS puede venir de una
//  version futura: NINGUN valor de adentro puede colgar el editor ni dejar el timeline inservible.
//   - lo que no es un numero FINITO (nan / inf, que std::stof parsea sin chistar) vuelve al DEFAULT;
//   - un zoom <= 0 vuelve al DEFAULT (con pxPerFrame en 0 no hay frame que dibujar, y el resto del archivo
//     divide por el); el zoom que SI es un numero usable se acota con los MISMOS topes de la rueda
//     (ClampZoomH/ClampZoomV), que es el unico lugar donde viven;
//   - un desplazamiento absurdo (|frame| > 1e6, la misma cota que el resto del lector) vuelve al DEFAULT en
//     vez de acotarse al tope: acotarlo dejaria el timeline mostrando el frame un millon, que para el usuario
//     es indistinguible de un timeline vacio. Cayendo al default abre con el encuadre de siempre.
//  El paneo NO se acota contra MinView() aca: MinView depende del ANCHO y al aplicar el layout el viewport
//  todavia no fue Resize()ado (width=0), asi que acotar ahi comeria el -2 del arranque. Ya lo acotan el paneo
//  y el zoom en cuanto el usuario toca algo.
void Timeline::VistaAplicar(float zoomF, float frameIzq, float zoomV, float centroV){
    const float kDefZoom  = (float)GlobalScale * 7.0f;   // los MISMOS del constructor
    const float kDefIzq   = -2.0f;
    const float kCotaF    = 1000000.0f;                  // cota de frames del lector (ver SesionAplicar)
    const float kCotaV    = 1e12f;                       // un valor de curva puede ser grande, pero no infinito
    pxPerFrame  = (W3dFinito(zoomF)   && zoomF > 0.0f)                 ? ClampZoomH(zoomF) : kDefZoom;
    viewStartF  = (W3dFinito(frameIzq)&& fabsf(frameIzq) <= kCotaF)    ? frameIzq          : kDefIzq;
    pxPerUnit   = (W3dFinito(zoomV)   && zoomV > 0.0f)                 ? ClampZoomV(zoomV) : kDefZoom;
    viewCenterV = (W3dFinito(centroV) && fabsf(centroV)  <= kCotaV)    ? centroV           : 0.0f;
}
void Timeline::SetFrameFromX(int lx){
    int f = (int)(XToFrame((float)lx) + 0.5f);
    // igual que StepFrame: en PAUSA (animacion normal) el playhead va LIBRE fuera del
    // rango; reproduciendo o en modo juego se clampea (el juego arranca en Start).
    if (PlayAnimation || AnimEsJuego){
        if (f < StartFrame) f = StartFrame;
        if (f > EndFrame && !AnimEsJuego) f = EndFrame;   // en modo juego no hay Fin
    }
    // "Juego" SIN partida: lo que hay es el estado base (su frame 1)
    { extern bool SimActiva(); if (ActiveAnimKind == 2 && !SimActiva()) f = StartFrame; }
    CurrentFrame = f; g_redraw = true;
}
void Timeline::ApplyStart(){ int v=(int)(fStart+0.5f); if(v<0)v=0;
    AnimSetStart(v);   // editar Start = editar el Start PROPIO de la animacion activa (escena o clip)
    if(CurrentFrame<StartFrame)CurrentFrame=StartFrame; g_redraw=true; }
void Timeline::ApplyEnd(){   int v=(int)(fEnd+0.5f);   if(v<0)v=0;
    AnimSetEnd(v);     // editar End = editar el End PROPIO de la animacion activa (escena o clip)
    if(CurrentFrame>EndFrame && EndFrame>=StartFrame)CurrentFrame=EndFrame; g_redraw=true; }
void Timeline::ApplyCur(){   int v=(int)(fCur+0.5f);   if(v<0)v=0; CurrentFrame=v; g_redraw=true; }
void Timeline::ApplyFps(){   int v=(int)(fFpsTL+0.5f); if(v<1)v=1; if(v>120)v=120; AnimSetFps(v); fFpsTL=(float)v; g_redraw=true; }
void Timeline::EditarCampo(int i){
    g_tlActivo = this;
    PropFloat* pf = (i==0)?pfStart : (i==1)?pfEnd : (i==3)?pfFps : pfCur;
    if (i==0) fStart=(float)StartFrame; else if (i==1) fEnd=(float)EndFrame; else if (i==3) fFpsTL=(float)AnimFPS; else fCur=(float)CurrentFrame;
    pf->IniciarEdicionTexto();
    if (i==0) btnStart->editField=&pfStart->field; else if (i==1) btnEnd->editField=&pfEnd->field; else if (i==3) btnFps->editField=&pfFps->field;
    g_redraw = true;
}
// El paso de las flechas segun el ZOOM. La idea: que cada pulsacion avance SIEMPRE la misma distancia EN
// PANTALLA (~10 px), con cualquier zoom. Alejado eso son muchos frames (sino no llegas nunca); con zoom son
// menos, hasta el minimo de 1 frame (los frames son enteros: mas fino no se puede, y es lo que queres cuando
// hiciste zoom para ser preciso).
// OJO: redondeo al MAS CERCANO, no para arriba. Redondeando para arriba, al zoom por defecto (7 px/frame) un
// paso "ideal" de 1.4 frames daba 2 -> el doble de rapido que antes, imposible de usar.
// Flecha MANTENIDA sobre el timeline: corre el frame actual. Se aplica CADA frame de la UI, asi que el paso se
// mide en PIXELES DE PANTALLA y no en frames: recorrer la animacion cuesta lo mismo con cualquier zoom. Los
// sobrantes se ACUMULAN, que es lo que da el control fino: con mucho zoom un frame son varios ticks (antes el paso
// se redondeaba a 1 frame minimo -> con zoom alto volaba a 1 frame por tick).
// el TOQUE de izquierda / derecha (keypad del N95): ya movio UN cuadro (la tecla, Timeline::event_key_down); el scrub
// de la flecha MANTENIDA arranca recien despues de un ratito. Sin esto un toque rapido con el timeline acercado (un
// cuadro mas ancho que lo que avanza el scrub por tick) no llegaba a moverse ni un cuadro
void W3dTimelineFlechaPulsada(){ g_scrubAcum = 0.0f; g_scrubEspera = 8; }
void Timeline::ScrubFlecha(int dir){
    if (g_scrubEspera > 0) { g_scrubEspera--; return; }
    const float pxPorTick = 3.3f;
    float px = (pxPerFrame > 0.01f) ? pxPerFrame : 0.01f;
    g_scrubAcum += (float)dir * pxPorTick / px;      // en frames (con fraccion)
    int n = (int)g_scrubAcum;                        // trunca hacia el cero: la fraccion queda para el proximo tick
    if (n == 0) return;
    g_scrubAcum -= (float)n;
    for (int k = 0, q = (n < 0 ? -n : n); k < q; k++) StepFrame(n > 0 ? 1 : -1);
}

void Timeline::TransportAction(int i){
    // con una SIMULACION cargada (scripts lua), el boton "inicio" es el STOP del
    // juego: restaura el estado inicial y descarga los scripts
    { extern bool SimActiva(); extern void SimStop();
      if (i == 0 && SimActiva()) { SimStop(); PlayAnimation = false; g_redraw = true; return; } }
    switch (i){
        case 0: GotoStart(); break;      case 1: StepKeyframe(-1); break;
        case 2: StepFrame(-1); break;    case 3: TogglePlay(-1); break;
        case 4: TogglePlay(+1); break;   case 5: StepFrame(+1); break;
        case 6: StepKeyframe(+1); break; case 7: GotoEnd(); break;
    }
}


// ---- Aperturas de menu COMPARTIDAS por el boton de la barra y el atajo de teclado: los dos llaman aca, asi no
//      pueden divergir (era facil que el menu del boton tuviera opciones que el atajo no).
void Timeline::AbrirMenuKey(int mx, int my){
    if (!g_tlMenuKey){
        g_tlMenuKey   = new PopupMenu(); g_tlMenuKey->action   = TL_menuKeyAction; 
        g_tlMenuKeyXf = new PopupMenu(); g_tlMenuKeyXf->action = TL_menuKeyAction; // los submenus HEREDAN la action
        
        g_tlMenuKeyIn = new PopupMenu(); g_tlMenuKeyIn->action = TL_menuKeyAction;
        
        g_tlMenuHandle = new PopupMenu(); g_tlMenuHandle->action = TL_menuKeyAction;
        
    }
    // se rearma en cada apertura: Rotate solo existe en CURVAS (en el dope sheet el unico eje es el tiempo, girar
    // no significa nada)
    g_tlMenuKeyXf->Limpiar();
    g_tlMenuKeyXf->Agregar(T("Move"), TL_KEY_MOVE)->atajo = "@tl.mover";
    if (modo == TL_MODO_CURVAS) g_tlMenuKeyXf->Agregar(T("Rotate"), TL_KEY_ROT)->atajo = "@tl.rotar";
    g_tlMenuKeyXf->Agregar(T("Scale"), TL_KEY_SCALE)->atajo = "@tl.escalar";
    ConstruirMenuInterp(g_tlMenuKeyIn);
    ConstruirMenuHandle(g_tlMenuHandle);
    g_tlMenuKey->Limpiar();
    g_tlMenuKey->titulo = T("Key");   // (el boton es el rombo, sin texto)
    g_tlMenuKey->Agregar(T("Transform"), -1, -1, g_tlMenuKeyXf);
    g_tlMenuKey->Agregar(T("Interpolation Mode"), -1, IconType::interp_bezier, g_tlMenuKeyIn)->atajo = "@tl.interp";
    // el EASING de las curvas de easing (Ctrl+E): otro submenu
    { static PopupMenu* sEase = NULL;
      if (!sEase){ sEase = new PopupMenu(); sEase->action = TL_menuKeyAction; }
      W3dMenuEasings(sEase, TL_KEY_EASE0, -1);
      g_tlMenuKey->Agregar(T("Easing Type"), -1, IconType::ease_auto, sEase)->atajo = "@tl.easing"; }
    // los handles solo existen en CURVAS: en el dope sheet no hay nada que ajustar
    if (modo == TL_MODO_CURVAS) g_tlMenuKey->Agregar(T("Handle Type"), -1, -1, g_tlMenuHandle)->atajo = "@tl.handle";
    g_tlMenuKey->Agregar(T("Smart Euler"), TL_KEY_SMARTEULER);
    g_tlMenuKey->Agregar(T("Cut"), TL_KEY_CORTAR)->atajo = "@tl.cortar";
    g_tlMenuKey->items.back()->lineaArriba = true;
    g_tlMenuKey->Agregar(T("Copy"), TL_KEY_COPIAR)->atajo = "@tl.copiar";
    g_tlMenuKey->Agregar(T("Paste"), TL_KEY_PEGAR)->atajo = "@tl.pegar";
    g_tlMenuKey->Agregar(T("Duplicate"), TL_KEY_DUP)->atajo = "@tl.duplicar";
    g_tlMenuKey->Agregar(T("Delete"), TL_KEY_DEL)->atajo = "@tl.borrar";
    if (MenuAbierto && MenuAbierto != g_tlMenuKey) MenuAbierto->Cerrar();
    g_tlMenuKey->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = g_tlMenuKey;
}
// ---- MODIFIERS: el modificador REPETIR de las curvas elegidas ----
static PopupMenu* g_tlMenuMods = NULL;
static void TL_menuModsAction(int id){
    if (g_tlActivo && id >= KfCicloNo && id <= KfCicloOffset) g_tlActivo->SetCicloSel(id, id);
    TL_CerrarMenu(g_tlMenuMods);
    g_redraw = true;
}
// las curvas (no las de vertices) que tienen algun keyframe elegido
static void CurvasConSeleccion(const std::vector<Timeline::DopeRow>& rows, std::vector<AnimProperty*>& out, bool escribir){
    out.clear();
    for (size_t r=0;r<rows.size();r++){
        const Timeline::DopeRow& d = rows[r]; if (d.propId < 0 || DopeVertexAnimDe(d.ownerKey)) continue;
        AnimProperty* ap = DopeResolverProp(d.ownerKey, d.propId, d.compId, escribir); if (!ap) continue;
        bool sel = false;
        for (size_t i=0;i<ap->keyframes.size() && !sel;i++)
            sel = g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, ap->keyframes.Frame(i))) > 0;
        if (sel && std::find(out.begin(), out.end(), ap) == out.end()) out.push_back(ap);
    }
}
void Timeline::AbrirMenuMods(int mx, int my){
    g_tlActivo = this;
    if (!g_tlMenuMods){ g_tlMenuMods = new PopupMenu(); g_tlMenuMods->action = TL_menuModsAction; }
    // el que tienen TODAS las curvas elegidas (antes y despues iguales) queda en verde
    std::vector<AnimProperty*> cs; CurvasConSeleccion(dopeRows, cs, false);
    int act = -2;
    for (size_t i=0;i<cs.size();i++){
        const int m = (cs[i]->cicloAntes == cs[i]->cicloDespues) ? cs[i]->cicloAntes : -1;
        act = (act == -2 || act == m) ? m : -1;
    }
    static bool sNada; sNada = cs.empty();
    g_tlMenuMods->Limpiar();
    g_tlMenuMods->titulo = T("Modifiers");
    MenuItem* it;
    it = g_tlMenuMods->Agregar(T("Repeat"), KfCicloRepite);             it->verde = (act == KfCicloRepite); it->gris = &sNada;
    it = g_tlMenuMods->Agregar(T("Repeat with Offset"), KfCicloOffset); it->verde = (act == KfCicloOffset); it->gris = &sNada;
    it = g_tlMenuMods->Agregar(T("Remove Repeat"), KfCicloNo);          it->gris = &sNada; it->lineaArriba = true;
    if (MenuAbierto && MenuAbierto != g_tlMenuMods) MenuAbierto->Cerrar();
    g_tlMenuMods->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = g_tlMenuMods;
}
void Timeline::SetCicloSel(int antes, int despues){
    std::vector<AnimProperty*> cs; CurvasConSeleccion(dopeRows, cs, true);
    if (cs.empty()) return;
    UndoKeyframesIniciar();
    for (size_t i=0;i<cs.size();i++){
        if (antes   >= KfCicloNo && antes   <= KfCicloOffset) cs[i]->cicloAntes   = antes;
        if (despues >= KfCicloNo && despues <= KfCicloOffset) cs[i]->cicloDespues = despues;
    }
    UndoKeyframesConfirmar();
    InvalidarAnimYRedraw();
}
// el panel de propiedades (tarjeta Keyframe): el mismo cambio, sobre lo elegido en el timeline activo
bool TimelineSetCiclo(int antes, int despues){
    if (!g_tlActivo || g_dopeKeySel.empty()) return false;
    g_tlActivo->SetCicloSel(antes, despues);
    return true;
}
// Interpolation Mode: la forma del tramo que SALE de cada keyframe seleccionado. Lo comparten el submenu del
// menu Key y la 't'.
static const char* const kNomInterp[KfInterpN] = { "Constant", "Linear", "Bezier",
    "Sinusoidal", "Quadratic", "Cubic", "Quartic", "Quintic", "Exponential", "Circular", "Back (overshoot)", "Bounce", "Elastic" };
static const int kIconInterp[KfInterpN] = { IconType::interp_constant, IconType::interp_linear, IconType::interp_bezier,
    IconType::interp_sine, IconType::interp_quad, IconType::interp_cubic, IconType::interp_quart, IconType::interp_quint,
    IconType::interp_expo, IconType::interp_circ, IconType::interp_back, IconType::interp_bounce, IconType::interp_elastic };
static const char* const kNomEase[4] = { "Automatic Easing", "Ease In", "Ease Out", "Ease In and Out" };
static const int kIconEase[4] = { IconType::ease_auto, IconType::ease_in, IconType::ease_out, IconType::ease_inout };
const char* W3dNombreInterpolacion(int interp){
    const int b = KfInterpBase(interp); return T((b >= 0 && b < KfInterpN) ? kNomInterp[b] : "Linear");
}
const char* W3dNombreEasing(int ease){ return T(kNomEase[ease & 3]); }
void W3dMenuInterpolaciones(PopupMenu* m, int idBase, int actual){
    m->Limpiar();
    const int act = (actual >= 0) ? KfInterpBase(actual) : -1;
    for (int i = 0; i < KfInterpN; i++){
        MenuItem* it = m->Agregar(T(kNomInterp[i]), idBase + i, kIconInterp[i]);
        it->verde = (i == act);
        // tres grupos: las de siempre | easing por fuerza (Sinusoidal..Circular) | con efecto (Back..Elastic)
        if (i == KfSine || i == KfBack) it->lineaArriba = true;
    }
}
void W3dMenuEasings(PopupMenu* m, int idBase, int actual){
    m->Limpiar();
    for (int e = 0; e < 4; e++){ MenuItem* it = m->Agregar(T(kNomEase[e]), idBase + e, kIconEase[e]); it->verde = (e == actual); it->atajo = "@tl.easing"; }
}
void Timeline::ConstruirMenuInterp(PopupMenu* m){
    W3dMenuInterpolaciones(m, TL_KEY_INTERP0, -1);
    for (size_t i = 0; i < m->items.size(); i++) m->items[i]->atajo = "@tl.interp";
}
void Timeline::AbrirMenuEasing(int mx, int my){
    if (!g_tlMenuEase){ g_tlMenuEase = new PopupMenu(); g_tlMenuEase->action = TL_menuKeyAction; }
    g_tlMenuEase->titulo = T("Set Keyframe Easing Type");
    W3dMenuEasings(g_tlMenuEase, TL_KEY_EASE0, -1);
    if (MenuAbierto && MenuAbierto != g_tlMenuEase) MenuAbierto->Cerrar();
    g_tlMenuEase->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = g_tlMenuEase;
}
// menu Select del dope sheet (All / None / Invert)
void Timeline::AbrirMenuSelect(int mx, int my){
    if (!g_tlMenuSel){
        // (el boton es un icono: el menu lleva titulo, como el View)
        g_tlMenuSel = new PopupMenu(); g_tlMenuSel->titulo = T("Select"); g_tlMenuSel->action = TL_menuSelAction;
        g_tlMenuSel->Agregar(T("All"), 0)->atajo = "@tl.seleccionarTodo";
        g_tlMenuSel->Agregar(T("None"), 1)->atajo = "@tl.deseleccionar";
        g_tlMenuSel->Agregar(T("Invert"), 2)->atajo = "@tl.invertir";
    }
    if (MenuAbierto && MenuAbierto != g_tlMenuSel) MenuAbierto->Cerrar();
    g_tlMenuSel->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = g_tlMenuSel;
}
// menu Pivot del dope sheet (Center / Current Frame): desde donde escala la 's'
void Timeline::AbrirMenuPivot(int mx, int my){
    if (!g_tlMenuPivot){
        g_tlMenuPivot = new PopupMenu(); g_tlMenuPivot->action = TL_menuPivotAction;
        g_tlMenuPivot->titulo = T("Transform Pivot Point");
    }
    // se rearma cada vez: cada opcion con su ICONO y la ACTIVA en verde (igual que el Pivot Point del 3D)
    g_tlMenuPivot->Limpiar();
    g_tlMenuPivot->Agregar(T("Center"), 0, (int)IconType::pivotMedian)->verde = (g_dopePivot != DOPE_PIV_CURFRAME);
    g_tlMenuPivot->Agregar(T("Current Frame"), 1, (int)IconType::pivotCursor)->verde = (g_dopePivot == DOPE_PIV_CURFRAME);
    if (MenuAbierto && MenuAbierto != g_tlMenuPivot) MenuAbierto->Cerrar();
    g_tlMenuPivot->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = g_tlMenuPivot;
}
// dropdown de animacion: menu jerarquico (submenu "Scenes" + un submenu por armadura con sus clips)
void Timeline::AbrirMenuAnim(int mx, int my){
    if (!g_tlMenuAnim){ g_tlMenuAnim = new PopupMenu(); g_tlMenuAnim->action = TL_menuAnimAction; }
    ConstruirMenuAnim(g_tlMenuAnim);
    if (MenuAbierto && MenuAbierto != g_tlMenuAnim) MenuAbierto->Cerrar();
    g_tlMenuAnim->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = g_tlMenuAnim;
}
// menu View: Frame Selected + Auto frame (checkbox). Se rearma en cada apertura para que el checkbox lea el
// bool VIVO.
void Timeline::AbrirMenuView(int mx, int my){
    if (!g_tlMenuView){
        // REGLA DE DISENO de los titulos: un menu que se abre desde algo SIN TEXTO (un icono, o un atajo de
        // teclado) lleva titulo -- es lo unico que te dice que estas mirando. Si lo abre un boton/item que YA
        // decia el texto, NO lleva: repetirlo es ruido. El boton View ahora es un icono -> titulo.
        g_tlMenuView = new PopupMenu(); g_tlMenuView->titulo = T("View"); g_tlMenuView->action = TL_menuViewAction; 
    }
    g_tlMenuView->Limpiar();
    // el MODO del editor: Dope Sheet (rombos por fila) o Curves (el grafico tiempo x valor). El activo en verde
    g_tlMenuView->Agregar(T("Dope Sheet"), TL_VIEW_DOPE, IconType::keyframe)->verde = (modo != TL_MODO_CURVAS);
    g_tlMenuView->Agregar(T("Curves"), TL_VIEW_CURVAS, IconType::interp_bezier)->verde = (modo == TL_MODO_CURVAS);
    { MenuItem* it = g_tlMenuView->Agregar(T("Frame Selected"), TL_VIEW_FRAMESEL); it->atajo = "@tl.encuadrar"; it->lineaArriba = true; }
    g_tlMenuView->AgregarCheck(T("Show Panel"), TL_VIEW_PANEL, &g_dopePanelOn);
    g_tlMenuView->AgregarCheck(T("Auto frame"), TL_VIEW_AUTOFRAME, &g_dopeAutoFrame);
    // toggle "Solo vertices seleccionados" (dope Y curvas: es el MISMO menu View). Con la
    // malla en Edit Mode filtra las filas de vertex anim y hace el move por mascara.
    g_tlMenuView->AgregarCheck(T("Only selected vertices"), TL_VIEW_SOLOSEL, &g_dopeSoloSelVerts);
    g_tlMenuView->AgregarCheck(T("Waveform"), TL_VIEW_ONDA, &g_tlVerOnda);         // las tiras de sonido
    g_tlMenuView->AgregarCheck(T("Volume line"), TL_VIEW_VOLUMEN, &g_tlVerVolumen);
    if (MenuAbierto && MenuAbierto != g_tlMenuView) MenuAbierto->Cerrar();
    g_tlMenuView->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = g_tlMenuView;
}
void Timeline::AbrirMenuInterp(int mx, int my){
    if (!g_tlMenuKeyIn){ g_tlMenuKeyIn = new PopupMenu(); g_tlMenuKeyIn->action = TL_menuKeyAction;
                         g_tlMenuKeyIn->titulo = T("Set Keyframe Interpolation"); }
    ConstruirMenuInterp(g_tlMenuKeyIn);
    if (MenuAbierto && MenuAbierto != g_tlMenuKeyIn) MenuAbierto->Cerrar();
    g_tlMenuKeyIn->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = g_tlMenuKeyIn;
}
// Handle Type ('v'): como se portan los dos lados del handle. Solo tiene sentido en CURVAS y sobre keyframes bezier.
// Handle Type: como se portan los dos lados del handle. Lo comparten el submenu del menu Key y la 'v'.
void Timeline::ConstruirMenuHandle(PopupMenu* m){
    m->Limpiar();
    m->Agregar(T("Free"), TL_KEY_HANDLE0 + HFree, IconType::handle_free);
    m->Agregar(T("Aligned"), TL_KEY_HANDLE0 + HAligned, IconType::handle_aligned);
    m->Agregar(T("Vector"), TL_KEY_HANDLE0 + HVector, IconType::handle_vector);
    m->Agregar(T("Automatic"), TL_KEY_HANDLE0 + HAuto, IconType::handle_auto);
    m->Agregar(T("Auto Clamped"), TL_KEY_HANDLE0 + HAutoClamped, IconType::handle_autoclamped);
    m->Agregar(T("Flat"), TL_KEY_HANDLE0 + HPlano, IconType::handle_flat);
}
void Timeline::AbrirMenuHandle(int mx, int my){
    if (!g_tlMenuHandle){
        g_tlMenuHandle = new PopupMenu(); g_tlMenuHandle->action = TL_menuKeyAction;
        g_tlMenuHandle->titulo = T("Set Keyframe Handle Type");
    }
    ConstruirMenuHandle(g_tlMenuHandle);
    if (MenuAbierto && MenuAbierto != g_tlMenuHandle) MenuAbierto->Cerrar();
    g_tlMenuHandle->Abrir(mx, my, MenuPantallaW, MenuPantallaH);
    MenuAbierto = g_tlMenuHandle;
}

// ------------------------------------------------------------------ input
// SOLO los botones que abren un MENU. Separado de las ACCIONES (transporte, Auto Key, modo, campos) porque el
// ruteo compartido lo llama al PASAR EL MOUSE sin click, para poder deslizarse de un menu a otro (lo mismo que
// hace la barra del viewport 3D). Deslizarse por encima no puede disparar una accion.
bool Timeline::AbrirMenuDeBarra(int mx, int my){
    if (btnSelect->visible && btnSelect->Contains(mx,my)){ AbrirMenuSelect(btnSelect->sx, btnSelect->sy + btnSelect->height); return true; }
    if (btnView->visible   && btnView->Contains(mx,my)){   AbrirMenuView(btnView->sx, btnView->sy + btnView->height); return true; }
    if (btnKey->visible    && btnKey->Contains(mx,my)){    AbrirMenuKey(btnKey->sx, btnKey->sy + btnKey->height); return true; }
    if (btnMods->visible   && btnMods->Contains(mx,my)){   AbrirMenuMods(btnMods->sx, btnMods->sy + btnMods->height); return true; }
    if (btnPivot->visible  && btnPivot->Contains(mx,my)){  AbrirMenuPivot(btnPivot->sx, btnPivot->sy + btnPivot->height); return true; }
    if (btnAnim->visible   && btnAnim->Contains(mx,my)){   AbrirMenuAnim(btnAnim->sx, btnAnim->sy + btnAnim->height); return true; }
    return false;
}

bool Timeline::ClickBarButton(int mx, int my){
    g_tlActivo = this;   // re-latch: si OTRO timeline se destruyo (dejo g_tlActivo NULL), este vuelve a ser el activo
    for (int i=0;i<8;i++) if (btnT[i]->visible && btnT[i]->Contains(mx,my)){ TransportAction(i); return true; }
    if (btnTiempo->visible && btnTiempo->Contains(mx,my)){
        fCur = (float)CurrentFrame; fStart = (float)StartFrame; fEnd = (float)EndFrame; fFpsTL = (float)AnimFPS;
        AbrirTiempoPopup(pfCur, pfStart, AnimEsJuego ? NULL : pfEnd, pfFps, btnTiempo->sx, btnTiempo->sy + btnTiempo->height);
        return true;
    }
    // AUTO KEY: toggle
    if (btnAutoKey->visible && btnAutoKey->Contains(mx,my)){
        AutoKeyOn = !AutoKeyOn;   // solo esta sesion: el editor SIEMPRE arranca con el auto key apagado
        g_redraw = true; return true;
    }
    // switch DOPE SHEET <-> CURVES
    if (btnModo->visible && btnModo->Contains(mx,my)){ CambiarModo(modo != TL_MODO_CURVAS); return true; }
    // ...y los MENUS: el mismo camino que usa el hover para deslizarse entre ellos (no se duplica el if de cada uno)
    if (AbrirMenuDeBarra(mx, my)) return true;
    return false;
}

void Timeline::button_left(){
    if (PopUpActive) return;
    g_tlActivo = this;   // re-latch (idem ClickBarButton): el timeline clickeado es el que manda
    int lx = lastMx - x, ly = lastMy - y;
    // moviendo keyframes ('g' tipeada): el click ACEPTA. (El arrastre DIRECTO no pasa por aca: lo confirma el
    //  soltar del boton, ver mouse_button_up.)
    if (g_dopeMov){ DopeMoveConfirm(); return; }
    // PANEL del dope sheet: flecha = plegar/desplegar; resto = seleccionar la fila (NO scrubbea)
    if (DopeClickPanel(lastMx, lastMy)) return;
    // STRIP: en CURVAS, handles y keyframes (hit-test 2D); en DOPE, el rombo de la fila. Si no cae en nada, scrubbea.
    if (TiraActiva()){ TiraConfirmar(); return; }   // ('g' de una tira: el click acepta)
    if (modo == TL_MODO_CURVAS){ if (CurvaClickStrip(lastMx, lastMy)) return; }
    else if (DopeClickStrip(lastMx, lastMy)) return;
    else if (TiraClick(lastMx, lastMy)) return;     // (los rombos tienen prioridad: se puede clickear uno sobre la tira)
    // boton VERDE del frame actual (sobresale por arriba de los numeros) -> editar por texto (teclado / pad numerico)
    if (ly >= numY - GlobalScale*2 && ly < stripY && lx >= curBtnX && lx <= curBtnX + curBtnW){ EditarCampo(2); return; }
    // EL FRAME ACTUAL se cambia SOLO en la banda de los NUMEROS (click = ir ahi, arrastre = scrub) o agarrando la
    // LINEA del frame actual. Antes cualquier click en el cuerpo movia el frame: se perdia el frame al querer
    // elegir o deseleccionar keyframes.
    if (ly >= numY - GlobalScale*2 && ly < stripY){ scrubbing = true; SetFrameFromX(lx); return; }
    if (CabezalEn(lastMx, lastMy)){ scrubbing = true; g_redraw = true; return; }
    if (ly < stripY || lx < panelW || dopeRows.empty()) return;
    // CURVAS: click sobre la LINEA de una curva = elige ese tramo (el keyframe de antes y el de despues)
    if (modo == TL_MODO_CURVAS && CurvaTramoEn(lastMx, lastMy, LShiftPressed)) return;
    // lugar vacio: arranca una CAJA de seleccion (izq -> der verde: lo que quede entero adentro; der -> izq azul:
    // alcanza con rozar). Un click sin arrastrar deselecciona (ver TimelineCajaSoltar)
    BoxSelectArmar(); BoxSelectDown(lastMx, lastMy); g_tlCaja = this;
}
// la LINEA del frame actual (el cabezal): banda de unos px a cada lado, en el cuerpo
bool Timeline::CabezalEn(int mx, int my) const {
    const int lx = mx - x, ly = my - y;
    if (ly < stripY || ly >= height || lx < panelW) return false;
    const float px = FrameToX((float)CurrentFrame);
    return fabsf((float)lx - px) <= RadioLinea();
}
// muestrea el tramo i (A = i-1, B = i) de la curva en coords LOCALES, cada ~3 px (con cualquier interpolacion:
// EvalF da lo mismo que corre la animacion)
static void TramoPuntos(const Timeline* t, const AnimProperty* ap, size_t i, std::vector<float>& xy){
    xy.clear();
    const keyFrame& a = ap->keyframes[i-1]; const keyFrame& b = ap->keyframes[i];
    if (a.Interpolation == KfConstant){   // escalon: meseta + salto
        xy.push_back(t->FrameToX((float)a.frame)); xy.push_back(t->ValueToY(a.value));
        xy.push_back(t->FrameToX((float)b.frame)); xy.push_back(t->ValueToY(a.value));
        xy.push_back(t->FrameToX((float)b.frame)); xy.push_back(t->ValueToY(b.value));
        return;
    }
    const float ppf = t->pxPerFrame > 0.01f ? t->pxPerFrame : 0.01f;
    float paso = (3.0f * (float)GlobalScale) / ppf; if (paso <= 0.0f) paso = 1.0f;
    if (a.Interpolation == KfLinear) paso = (float)(b.frame - a.frame);
    for (float f = (float)a.frame; ; f += paso){
        if (f > (float)b.frame) f = (float)b.frame;
        xy.push_back(t->FrameToX(f)); xy.push_back(t->ValueToY(ap->EvalF(f, 0.0f)));
        if (f >= (float)b.frame) break;
    }
}
static float DistSeg(float px, float py, float ax, float ay, float bx, float by){
    const float vx = bx - ax, vy = by - ay, l2 = vx*vx + vy*vy;
    float u = l2 > 1e-6f ? ((px - ax)*vx + (py - ay)*vy) / l2 : 0.0f;
    if (u < 0.0f) u = 0.0f; else if (u > 1.0f) u = 1.0f;
    const float dx_ = ax + vx*u - px, dy_ = ay + vy*u - py;
    return sqrtf(dx_*dx_ + dy_*dy_);
}
bool Timeline::CurvaTramoEn(int mx, int my, bool agregar){
    const float lx = (float)(mx - x), ly = (float)(my - y);
    float mejor = RadioAgarre(); int bRow = -1; size_t bI = 0;
    std::vector<float> xy;
    for (size_t r=0; r<dopeRows.size(); r++){
        const DopeRow& d = dopeRows[r]; if (d.propId < 0 || d.oculto || DopeVertexAnimDe(d.ownerKey)) continue;
        const AnimProperty* ap = DopeResolverProp(d.ownerKey, d.propId, d.compId, false); if (!ap) continue;
        for (size_t i=1; i<ap->keyframes.size(); i++){
            const float xa = FrameToX((float)ap->keyframes[i-1].frame), xb = FrameToX((float)ap->keyframes[i].frame);
            if (lx < xa - mejor || lx > xb + mejor) continue;   // el mouse no esta en el ancho de este tramo
            TramoPuntos(this, ap, i, xy);
            for (size_t k=2; k+1<xy.size(); k+=2){
                const float dd = DistSeg(lx, ly, xy[k-2], xy[k-1], xy[k], xy[k+1]);
                if (dd < mejor){ mejor = dd; bRow = (int)r; bI = i; }
            }
        }
    }
    if (bRow < 0) return false;
    const DopeRow& d = dopeRows[(size_t)bRow];
    const AnimProperty* ap = DopeResolverProp(d.ownerKey, d.propId, d.compId, false);
    if (!agregar) g_dopeKeySel.clear();
    g_dopeKeySel.insert(DopeKeyId(d.ownerKey, d.propId, d.compId, ap->keyframes[bI-1].frame));
    g_dopeKeySel.insert(DopeKeyId(d.ownerKey, d.propId, d.compId, ap->keyframes[bI].frame));
    // el primero del tramo queda ACTIVO: la tarjeta Keyframe del panel de propiedades muestra esa curva
    g_dopeActOwner = d.ownerKey; g_dopeActProp = d.propId; g_dopeActComp = d.compId;
    g_dopeActFrame = ap->keyframes[bI-1].frame; g_dopeActHay = true;
    g_redraw = true;
    return true;
}
// LA CAJA: VERDE (tocar = false) = solo los keyframes ADENTRO. AZUL (tocar = true) = ademas, en CURVAS, el tramo
// que la caja ROCE (aunque sea un pedazo de la linea) queda elegido entero: el keyframe de antes y el de despues.
void Timeline::CajaAplicar(int sx0, int sy0, int sx1, int sy1, bool tocar, bool agregar){
    const int x0 = sx0 - x, y0 = sy0 - y, x1 = sx1 - x, y1 = sy1 - y;   // a locales
    if (!agregar) g_dopeKeySel.clear();
    std::vector<float> xy;
    for (size_t r=0; r<dopeRows.size(); r++){
        const DopeRow& d = dopeRows[r];
        if (modo == TL_MODO_CURVAS){
            if (d.propId < 0 || d.oculto) continue;
            static AnimProperty s_vtxTmp;
            const AnimProperty* ap = VertexCanalTemp(d.ownerKey, s_vtxTmp) ? &s_vtxTmp
                                   : DopeResolverProp(d.ownerKey, d.propId, d.compId, false);
            if (!ap) continue;
            const size_t n = ap->keyframes.size();
            for (size_t i=0; i<n; i++){
                const keyFrame& k = ap->keyframes[i];
                if (BoxPunto(x0, y0, x1, y1, FrameToX((float)k.frame), ValueToY(k.value)))
                    g_dopeKeySel.insert(DopeKeyId(d.ownerKey, d.propId, d.compId, k.frame));
            }
            if (!tocar || ap == &s_vtxTmp) continue;
            for (size_t i=1; i<n; i++){
                TramoPuntos(this, ap, i, xy);
                bool toca = false;
                for (size_t k=2; k+1<xy.size() && !toca; k+=2)
                    toca = BoxSegmento(x0, y0, x1, y1, xy[k-2], xy[k-1], xy[k], xy[k+1]);
                if (!toca) continue;
                g_dopeKeySel.insert(DopeKeyId(d.ownerKey, d.propId, d.compId, ap->keyframes[i-1].frame));
                g_dopeKeySel.insert(DopeKeyId(d.ownerKey, d.propId, d.compId, ap->keyframes[i].frame));
            }
        } else {
            // DOPE: los rombos de cada fila (puntos: verde y azul dan lo mismo). El rombo de una fila PADRE elige
            // los keyframes de ese frame de todos los canales que cubre
            const float ry = (float)(stripY + (int)r*rowH + PosY) + rowH*0.5f;
            for (size_t k=0; k<d.keys.size(); k++){
                if (!BoxPunto(x0, y0, x1, y1, FrameToX((float)d.keys[k]), ry)) continue;
                if (d.propId >= 0){ g_dopeKeySel.insert(DopeKeyId(d.ownerKey, d.propId, d.compId, d.keys[k])); continue; }
                for (size_t c=0; c<dopeRows.size(); c++){ const DopeRow& cc = dopeRows[c]; if (cc.propId < 0) continue;
                    if (d.tipo != 0 && !DopeCubre(d.ownerKey, cc.ownerKey)) continue;
                    for (size_t q=0; q<cc.keys.size(); q++) if (cc.keys[q] == d.keys[k])
                        g_dopeKeySel.insert(DopeKeyId(cc.ownerKey, cc.propId, cc.compId, d.keys[k])); }
            }
        }
    }
    g_redraw = true;
}
// (controles.cpp) la caja en curso es de un timeline? -> su soltar va a el y no al 3D
bool TimelineCajaPropia(){ return g_tlCaja != NULL && BoxSelectActivo(); }
void TimelineCajaOlvidar(){ g_tlCaja = NULL; }
void TimelineCajaSoltar(bool hubo, int x0, int y0, int x1, int y1, bool tocar, bool agregar){
    Timeline* t = g_tlCaja; g_tlCaja = NULL;
    if (!t) return;
    if (hubo) t->CajaAplicar(x0, y0, x1, y1, tocar, agregar);
    else if (!agregar){ t->DopeSelectNone(); }   // un click suelto en un lugar vacio: deselecciona
    g_redraw = true;
}
void Timeline::event_mouse_motion(int mx, int my){
    if (PopUpActive){ lastMx=mx; lastMy=my; return; }
    if (ToolbarVisible()){ bool h = false;
        for (size_t i=0;i<ToolButtons.size();i++){ Button* b = ToolButtons[i];
            const bool hv = b->visible && b->Contains(mx, my); if (hv != b->hover){ b->hover = hv; h = true; } }
        if (h) g_redraw = true; }
    if (TiraActiva()){ TiraAplicar(mx); lastMx=mx; lastMy=my; return; }   // corriendo / recortando una tira
    // g/s/r: el arrastre transforma. Se usa el cursor VIRTUAL (acumula dx/dy), NO la posicion real: el cursor se
    // envuelve al llegar al borde para poder seguir escalando/rotando sin limite, y ese salto no tiene que verse.
    if (g_dopeMov){
        lastMx=mx; lastMy=my;
        if (g_dopeVirtPrimero) g_dopeVirtPrimero = false;   // dx/dy de este evento son de ANTES del transform
        else { g_dopeVirtX += dx; g_dopeVirtY += dy; }
        DopeMoveApply(g_dopeVirtX, g_dopeVirtY);
        return;
    }
    // arrastrando un handle: curva el tramo. Usa el cursor VIRTUAL (acumula dx/dy) y NO la posicion real: el
    // cursor se envuelve al llegar al borde para poder seguir arrastrando sin limite, y ese salto no tiene que verse.
    if (g_hOn && leftMouseDown){
        lastMx=mx; lastMy=my;
        if (g_hVirtPrimero) g_hVirtPrimero = false;   // dx/dy de este evento son de ANTES de agarrarlo
        else { g_hVirtX += dx; g_hVirtY += dy; }
        HandleApply(g_hVirtX, g_hVirtY);
        return;
    }
    // ARRASTRE DIRECTO: se clickeo un keyframe y el mouse se movio -> arranca el MISMO transform que la 'g'. El
    // umbral evita que un click limpio mueva el keyframe por un pixel de temblor.
    if (g_dopeDragPend && leftMouseDown && !g_dopeMov){
        int ddx = mx - g_dopeDragX0, ddy = my - g_dopeDragY0;
        if (ddx*ddx + ddy*ddy > (3*GlobalScale)*(3*GlobalScale)){
            lastMx = g_dopeDragX0; lastMy = g_dopeDragY0;   // el move arranca donde se APRETO, no donde va el mouse
            DopeMoveStart(DOPE_MOV);
            g_dopeDragPend = false;
            // lo que el mouse YA recorrio desde que se apreto (el umbral) cuenta: el keyframe queda bajo el cursor
            if (g_dopeMov){ g_dopeVirtX = mx; g_dopeVirtY = my; g_dopeVirtPrimero = false; DopeMoveApply(mx, my); }
        }
        lastMx = mx; lastMy = my;
        if (!g_dopeMov) return;
    }
    if (g_hOn && !leftMouseDown) HandleSoltar();
    // BORDE del panel: arrastrando cambia su ancho (queda FIJADO por el usuario -> panelWUser)
    if (panelResize && leftMouseDown){ panelWUser = mx - x; if (panelWUser < 1) panelWUser = 1; lastMx=mx; lastMy=my; g_redraw=true; return; }
    if (!leftMouseDown) panelResize = false;
    // la barra horizontal del panel: arrastrar el agarre corre el texto (en proporcion al recorrido)
    if (panelBarraH && leftMouseDown){
        int bx, by, bw, bh, tx, tw;
        if (PanelBarraH(bx, by, bw, bh, tx, tw) && bw > tw && dx){
            panelPosX += (int)((float)dx * (float)PanelMaxX() / (float)(bw - tw));
            if (panelPosX < PanelMaxX()) panelPosX = PanelMaxX(); if (panelPosX > 0) panelPosX = 0;
        }
        lastMx=mx; lastMy=my; g_redraw=true; return;
    }
    if (!leftMouseDown) panelBarraH = false;
    { bool h = !leftMouseDown && PanelEnBorde(mx - x, my - y);   // el borde se pinta BLANCO con el mouse encima
      if (h != panelBordeHover){ panelBordeHover = h; g_redraw = true; } }
    // BOTON DEL MEDIO sobre el PANEL: scrollea el panel (texto en horizontal, filas en vertical), NO el timeline.
    // Se decide donde arranco el arrastre y queda asi hasta soltar.
    if (!middleMouseDown) panelPaneo = 0;
    else if (panelPaneo == 0){ int lx0 = lastMx - x, ly0 = lastMy - y; panelPaneo = (panelW > 0 && lx0 >= 0 && lx0 < panelW && ly0 >= stripY) ? 1 : 2; }
    if (middleMouseDown && panelPaneo == 1){
        if (dx){ panelPosX += dx; if (panelPosX < PanelMaxX()) panelPosX = PanelMaxX(); if (panelPosX > 0) panelPosX = 0; }
        if (dy){
            if (modo == TL_MODO_DOPE) ScrollY(dy);
            else { int maxY = (height - stripY) - DopeAltoContenido(); if (maxY > 0) maxY = 0;
                   PosY += dy; if (PosY < maxY) PosY = maxY; if (PosY > 0) PosY = 0; }
        }
        lastMx = mx; lastMy = my; g_redraw = true; return;
    }
    { bool h = !leftMouseDown && CabezalEn(mx, my);   // la linea del frame actual se pinta BLANCA con el mouse encima
      if (h != g_tlCabezalHover){ g_tlCabezalHover = h; g_redraw = true; } }
    DopeHover(mx, my); // mouse-over de las filas del panel
    if (!leftMouseDown) TiraHover(mx, my);   // la punta de una tira bajo el mouse (blanca)
    if (scrubbing && leftMouseDown)  SetFrameFromX(mx - x);                                              // izquierdo: scrub (arrastra el frame actual)
    else if (middleMouseDown){       // MEDIO: PANEA. Horizontal = frames; vertical = filas del dope sheet.
        // Se usan las GLOBALES dx/dy, NO (mx-lastMx): al llegar al borde el cursor SE ENVUELVE al otro lado
        // (CheckWarpMouseInViewport) y ahi mx/my pegan un salto del ancho/alto del viewport -> el delta salia
        // enorme y el scroll se iba al tope (nunca llegabas arriba de todo). CheckWarp pone dx/dy en 0 justo en
        // el frame del envolvimiento, asi que el paneo sigue continuo.
        // SHIFT + boton del medio (solo curvas) = ESTIRAR la vista, no panearla: arrastrar en horizontal estira el
        // eje X (tiempo) y en vertical el eje Y (valor). Es el zoom de 2 ejes a mano, para dejar la curva como se
        // vea mejor. Sin shift, el boton del medio panea como siempre.
        if (modo == TL_MODO_CURVAS && LShiftPressed){
            if (dx) ZoomBy(powf(1.012f, (float)dx), CentroTimeline());   // derecha = estira el tiempo
            if (dy) ZoomVBy(powf(1.012f, -(float)dy));                   // arriba  = estira el valor
            lastMx = mx; lastMy = my; return;
        }
        PanFrames(-(float)dx / (pxPerFrame>0.01f?pxPerFrame:0.01f));
        // vertical: en DOPE scrollea las filas; en CURVAS panea el VALOR (no hay filas que scrollear)
        if (modo == TL_MODO_CURVAS){ if (dy) PanValor((float)dy / (pxPerUnit>1e-6f?pxPerUnit:1e-6f)); }
        else if (!dopeRows.empty() && dy){ ScrollY(dy); g_redraw=true; }
    }
    lastMx = mx; lastMy = my;
}
bool Timeline::event_finger_scroll(int px, int py, int dx, int dy){
    PanFrames(-(float)dx / (pxPerFrame>0.01f?pxPerFrame:0.01f));              // horizontal: frames
    if (modo == TL_MODO_DOPE && !dopeRows.empty() && dy){ ScrollByTouch(0, dy); g_redraw=true; } // vertical: filas
    return true;
}
void Timeline::event_finger_gesture(float zoomDelta, float panDx, float panDy){
    scrubbing = false; // 2 dedos: es zoom/paneo de VISTA -> NO tocar el frame (cancela cualquier scrub)
    if (zoomDelta > 1.0f) ZoomBy(1.1f, CentroTimeline()); else if (zoomDelta < -1.0f) ZoomBy(1.0f/1.1f, CentroTimeline());
    if (panDx != 0.0f) PanFrames(-panDx / (pxPerFrame>0.01f?pxPerFrame:0.01f));
}
void Timeline::event_mouse_wheel(float dy, int mx, int my){
    if (PopUpActive) return;
    { if (BarScrollHorizontal(mx,my,(int)(dy*40))) return; }
    if (OnToolbar(mx, my)){ ToolbarScrollBy((int)(dy * 40)); return; }   // la toolbar de abajo: scroll horizontal
    // sobre el PANEL de curvas la rueda scrollea SUS filas (Shift = el texto en horizontal), no hace zoom
    { const int lx = mx - x, ly = my - y;
      if (panelW > 0 && lx >= 0 && lx < panelW && ly >= stripY){
          const int paso = (int)(dy * (float)(rowH > 0 ? rowH : RenglonHeightGS) * 2.0f);
          if (LShiftPressed){ panelPosX += paso; if (panelPosX < PanelMaxX()) panelPosX = PanelMaxX(); if (panelPosX > 0) panelPosX = 0; }
          else if (modo == TL_MODO_DOPE) ScrollY(paso);
          else { int maxY = (height - stripY) - DopeAltoContenido(); if (maxY > 0) maxY = 0;
                 PosY += paso; if (PosY < maxY) PosY = maxY; if (PosY > 0) PosY = 0; }
          g_redraw = true; return;
      } }
    // Fuera del panel la RUEDA SIEMPRE hace ZOOM en el timeline (desde el CENTRO exacto), NUNCA scrollea. Para scrollear/panear
    // se usa el BOTON DEL MEDIO: horizontal = frames, vertical = filas (dope) / valor (curvas).
    // En CURVAS el zoom tiene DOS EJES independientes -> no es un zoom parejo como en el resto del editor:
    //   rueda        = los dos ejes (mantiene la proporcion actual)
    //   Ctrl+rueda   = SOLO el tiempo (X)     Shift+rueda = SOLO el valor (Y)
    // Estirar un solo eje es lo que permite ver bien una curva chata o una de valores enormes.
    float f = (dy>0) ? 1.1f : 1.0f/1.1f;
    if (modo == TL_MODO_CURVAS){
        if (LShiftPressed){ ZoomVBy(f); return; }
        if (LCtrlPressed) { ZoomBy(f, CentroTimeline()); return; }
        ZoomBy(f, CentroTimeline()); ZoomVBy(f); return;
    }
    ZoomBy(f, CentroTimeline());
}
void Timeline::mouse_button_up(int boton){
    (void)boton;
    // el frame YA se fijo en el DOWN + el arrastre (scrub); al soltar solo se limpia el estado.
    if (g_hOn) HandleSoltar();   // soltar el handle CIERRA su undo (curvar el tramo = 1 comando de Ctrl+Z)
    // ARRASTRE DIRECTO: soltar CONFIRMA (arrastrar y soltar es un gesto completo). Si nunca se movio, el
    // pendiente se descarta y el click queda como una simple seleccion.
    if (g_dopeMov && !g_dopeDragPend) DopeMoveConfirm();
    g_dopeDragPend = false;
    if (TiraActiva()) TiraSoltar();   // arrastrar una tira y soltar = listo ('g' sigue hasta el click)
    scrubbing=false; panelResize=false; panelBarraH=false; panelPaneo=0; ViewPortClickDown=false; g_redraw=true;
}
// CERO (Symbian: la tecla 0 del teclado numerico del telefono) MANTENIDO = modificador de zoom para las flechas.
// Si se suelta sin haber tocado ninguna flecha, fue un TOQUE: Frame Selected (el equivalente al numpad '.' de PC).
static bool g_tlCeroDown = false, g_tlCeroUsado = false;

void Timeline::event_key_up(int tecla){
    const int k = tecla;
    if (k==W3dK_0 || k==W3dK_KP_0){
        if (g_tlCeroDown && !g_tlCeroUsado) DopeFrameSelected(); // toque limpio: encuadra
        g_tlCeroDown = false; g_tlCeroUsado = false;
    }
}
void Timeline::event_key_down(int tecla, bool repeticion){
    if (PopUpActive || g_textFieldActivo) return;
    const int k = tecla;
    // ---- DOPE SHEET (solo si hay filas) ----
    if (TiraActiva()){ // corriendo una tira con 'g': Enter acepta, Esc cancela
        if (k==W3dK_RETURN || k==W3dK_KP_ENTER){ TiraConfirmar(); return; }
        if (k==W3dK_ESCAPE){ TiraCancelar(); return; }
        return;
    }
    if (g_dopeMov){ // transformando keyframes: Enter acepta, Esc cancela. El valor numerico lo alimenta
        if (k==W3dK_RETURN || k==W3dK_KP_ENTER){ DopeMoveConfirm(); return; }  // DopeNumInputChar (SDL_TEXTINPUT en
        if (k==W3dK_ESCAPE){ DopeMoveCancel(); return; }                       // PC, keypad en Symbian), igual que el 3D
        // X / Y: limitan el transform a un eje (solo en curvas, que es donde hay dos)
        if (k==W3dK_X){ DopeCiclarEje(DOPE_EJE_X); return; }
        if (k==W3dK_Y){ DopeCiclarEje(DOPE_EJE_Y); return; }
        return; // durante el transform no pasan los atajos de abajo
    }
    // ---- CERO MANTENIDO + FLECHAS = ZOOM (el camino de Symbian, que no tiene rueda ni boton del medio).
    //      Es el equivalente del Shift + arrastrar con el boton del medio de PC: izq/der estira el TIEMPO,
    //      arriba/abajo estira el VALOR. Un toque de cero SIN flechas = Frame Selected.
    if (g_tlCeroDown){
        if (k==W3dK_LEFT) { ZoomBy(1.0f/1.15f, CentroTimeline()); g_tlCeroUsado = true; return; }
        if (k==W3dK_RIGHT){ ZoomBy(1.15f, CentroTimeline());      g_tlCeroUsado = true; return; }
        if (modo == TL_MODO_CURVAS){
            if (k==W3dK_UP)  { ZoomVBy(1.15f);      g_tlCeroUsado = true; return; }
            if (k==W3dK_DOWN){ ZoomVBy(1.0f/1.15f); g_tlCeroUsado = true; return; }
        }
    }
    if (k==W3dK_0 || k==W3dK_KP_0){ g_tlCeroDown = true; g_tlCeroUsado = false; return; } // Symbian: se define al soltar
    // numpad '.' = Frame Selected, igual que en el viewport 3D (ahi enfoca el objeto; aca encuadra los keyframes)
    if (k==W3dK_KP_PERIOD){ DopeFrameSelected(); return; }
    if (!dopeRows.empty()){
        if (k==W3dK_B){ BoxSelectArmar(); BoxSelectMover(lastMx, lastMy); g_tlCaja = this; g_redraw = true; return; } // B: caja
        if (k==W3dK_ESCAPE && g_tlCaja == this && BoxSelectActivo()){ BoxSelectCancelar(); g_tlCaja = NULL; return; }
        if (k==W3dK_A && LAltPressed){ DopeSelectNone(); return; }    // Alt+A: deseleccionar todo
        if (k==W3dK_A)               { DopeSelectAll();  return; }    // A: seleccionar todos los keyframes
        if (k==W3dK_I && LCtrlPressed){ DopeSelectInvert(); return; } // Ctrl+I: invertir
        // Ctrl+X / Ctrl+C / Ctrl+V: cortar / copiar / pegar keyframes. Ctrl+D (y Shift+D): DUPLICAR (y agarrar)
        if (LCtrlPressed && k==W3dK_X){ DopeCortarSeleccion(); return; }
        if (LCtrlPressed && k==W3dK_C){ DopeCopiarSeleccion(); return; }
        if (LCtrlPressed && k==W3dK_V){ DopePegar(); return; }
        if (k==W3dK_D && (LCtrlPressed || LShiftPressed)){ DopeDuplicarSeleccion(); return; }
        // X / Supr / Backspace (la C del N95): borrar los keyframes / las CURVAS (filas) elegidas
        if (k==W3dK_X || k==W3dK_DELETE || k==W3dK_BACKSPACE) { DopeBorrarSeleccion(); return; }
        if (k==W3dK_G && g_dopeKeySel.empty() && TiraGrabStart()) return; // G sin keyframes elegidos: los SONIDOS
        if (k==W3dK_G)               { DopeMoveStart(DOPE_MOV); return; } // G: MOVER
        if (k==W3dK_S)               { DopeMoveStart(DOPE_ESC); return; } // S: ESCALAR desde el pivote
        // R: ROTAR. Solo en CURVAS: en el dope sheet hay un solo eje (el tiempo) y girar no significa nada.
        if (k==W3dK_R && modo==TL_MODO_CURVAS){ DopeMoveStart(DOPE_ROT); return; }
        // T: Interpolation Mode (el mismo submenu del menu Key). Se abre EN EL CURSOR, como los menus del 3D.
        if (k==W3dK_E && LCtrlPressed){ AbrirMenuEasing(lastMx, lastMy); return; }   // Ctrl+E: Set Keyframe Easing Type
        if (k==W3dK_T || k==W3dK_E){ AbrirMenuInterp(lastMx, lastMy); return; }     // E / T: Set Keyframe Interpolation
        // V: Handle Type. Solo en CURVAS: los handles solo existen ahi.
        if (k==W3dK_V && modo==TL_MODO_CURVAS){ AbrirMenuHandle(lastMx, lastMy); return; }
    }
    if (k==W3dK_SPACE) {
        // jugando (modo Juego en play) el espacio es del juego: no pausa (la pausa es ESC)
        extern bool SimActiva(); extern bool AnimEsJuego;
        if (AnimEsJuego && PlayAnimation && SimActiva()) return;
        TogglePlay(+1); return;
    }
    if (k==W3dK_LEFT)  { StepFrame(-1); return; }
    if (k==W3dK_RIGHT) { StepFrame(+1); return; }
    if (k==W3dK_UP)    { StepKeyframe(+1); return; }
    if (k==W3dK_DOWN)  { StepKeyframe(-1); return; }
    if (k==W3dK_HOME)  { GotoStart(); return; }
    if (k==W3dK_END)   { GotoEnd(); return; }
}

// ============================================================================
//  LAS TIRAS DE SONIDO (objects/Sonido.h). Todo en SEGUNDOS de la escena: la tira de un sonido arranca en
//  'inicio' y dura su tramo / su tono; con bucle se repite hasta el final del timeline (las repeticiones mas
//  apagadas). Se corren y se recortan sin engancharse a los frames (Ctrl engancha al frame, si se quiere).
// ============================================================================
namespace {
struct TiraOrig { Sonido* s; float inicio, ri, rf; };
enum { TIRA_NADA = 0, TIRA_MOVER = 1, TIRA_RECINI = 2, TIRA_RECFIN = 3 };
int  g_tiraModo = TIRA_NADA;
bool g_tiraGrab = false;      // 'g': sigue al mouse hasta el click / Enter
bool g_tiraArrancada = false; // arrastre: paso el umbral (un click limpio no corre nada)
int  g_tiraX0 = 0, g_tiraY0 = 0;
std::vector<TiraOrig> g_tiraOrig;
Sonido* g_tiraHoverS = NULL; int g_tiraHoverB = 0;   // la punta bajo el mouse (TIRA_RECINI / TIRA_RECFIN)
// el ANCHO de la zona que agarra una punta: generoso, para el dedo (y nunca menos que media fila)
int TiraZonaBorde(int rowH) { int z = GlobalScale * 10; if (z < rowH / 2) z = rowH / 2; return z; }
// el UNDO de una tira: intercambia lo vivo con lo guardado (undo y redo son el mismo toggle)
struct UndoTira { std::vector<TiraOrig> v; };
void UndoTiraAplicar(void* d) {
    UndoTira* u = (UndoTira*)d;
    for (size_t i = 0; i < u->v.size(); i++) {
        TiraOrig& o = u->v[i]; if (!o.s) continue;
        TiraOrig vivo = { o.s, o.s->inicio, o.s->recorteIni, o.s->recorteFin };
        o.s->inicio = o.inicio; o.s->recorteIni = o.ri; o.s->recorteFin = o.rf;
        o = vivo;
    }
    g_redraw = true;
}
void UndoTiraDesvincular(void* d, Object* borrado) {
    UndoTira* u = (UndoTira*)d;
    for (size_t i = 0; i < u->v.size(); i++) if ((Object*)u->v[i].s == borrado) u->v[i].s = NULL;
}
void UndoTiraLiberar(void* d) { delete (UndoTira*)d; }
// la curva del VOLUMEN del sonido en la escena activa (NULL = no esta animado)
const AnimProperty* CurvaVolumen(Object* o) {
    for (size_t i = 0; i < AnimationObjects.size(); i++) {
        if (AnimationObjects[i].obj != o) continue;
        const std::vector<AnimProperty>& P = AnimationObjects[i].Propertys;
        for (size_t p = 0; p < P.size(); p++)
            if (P[p].Property == AnimSonido && P[p].component == AnimX && !P[p].keyframes.empty()) return &P[p];
    }
    return NULL;
}
}

bool Timeline::TiraActiva() const { return g_tiraModo != TIRA_NADA && (g_tiraGrab || g_tiraArrancada || leftMouseDown); }

void Timeline::DibujarTira(const DopeRow& d, int ry0) {
    Sonido* s = (Sonido*)d.sonido;
    const float L = s->DuracionEnEscena();
    const float durArch = s->DuracionArchivo();
    const int y0 = ry0 + GlobalScale, h = rowH * (1 + (d.tiraFilas > 0 ? d.tiraFilas : 0)) - GlobalScale * 2;
    if (h <= 0) return;
    const bool sel = s->select;
    gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray); gfx::Enable(gfx::Blend); gfx::BlendAlpha();
    if (L <= 0.0f) {   // sin archivo (o no se pudo leer): una marca donde arranca
        const float x = FrameToX(W3dSonidoFrameDeSeg(s->inicio));
        if (x >= panelW && x <= width) { gfx::Color4f(0.8f, 0.3f, 0.3f, 0.9f); FillRect((int)x, y0, GlobalScale * 3, h); }
        return;
    }
    const int reps = s->bucle ? 100000 : 1;
    const AnimProperty* cv = g_tlVerVolumen ? CurvaVolumen(s) : NULL;
    for (int k = 0; k < reps; k++) {
        const float sa = s->inicio + L * (float)k, sb = sa + L;
        const float xa = FrameToX(W3dSonidoFrameDeSeg(sa)), xb = FrameToX(W3dSonidoFrameDeSeg(sb));
        if (xa > width) break;
        if (xb < panelW) continue;
        const float ca = xa < panelW ? (float)panelW : xa, cb = xb > width ? (float)width : xb;
        if (cb - ca < 1.0f) { if (xb - xa < 1.0f) break; continue; }
        // la tira (las repeticiones del bucle, mas apagadas)
        const float a = (k == 0) ? 1.0f : 0.55f;
        if (sel) gfx::Color4f(0.16f * a + 0.05f, 0.52f * a, 0.56f * a, 0.95f); else gfx::Color4f(0.12f * a, 0.34f * a, 0.38f * a, 0.9f);
        FillRect((int)ca, y0, (int)(cb - ca), h);
        // la FORMA DE ONDA: la textura del archivo entero, recortada al tramo (solo con View > Waveform)
        if (g_tlVerOnda && durArch > 0.0f) {
            const unsigned int tex = W3dSonidoOnda(s->archivo);
            if (tex) {
                const float u0 = s->recorteIni / durArch, u1 = (durArch - s->recorteFin) / durArch;
                const float ua = u0 + (u1 - u0) * (ca - xa) / (xb - xa), ub = u0 + (u1 - u0) * (cb - xa) / (xb - xa);
                gfx::Enable(gfx::Texture2D); gfx::EnableArray(gfx::TexCoordArray); gfx::BindTexture(tex);
                gfx::Color4f(0.75f, 0.95f, 1.0f, (k == 0) ? 0.85f : 0.45f);
                const GLshort v[8] = { (GLshort)ca, (GLshort)y0, (GLshort)cb, (GLshort)y0, (GLshort)ca, (GLshort)(y0 + h), (GLshort)cb, (GLshort)(y0 + h) };
                const GLfloat uv[8] = { ua, 0.0f, ub, 0.0f, ua, 1.0f, ub, 1.0f };
                W3dDrawStrip4(v, uv);
                gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
                if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
            }
        }
        // la LINEA DEL VOLUMEN (animado: su curva; si no, el valor fijo). Arriba = 1.25
        if (g_tlVerVolumen) {
            g_curvaBuf.clear();
            float px = -1.0f, py = 0.0f;
            const float paso = (float)GlobalScale * 3.0f;
            for (float x = ca; ; x += paso) {
                if (x > cb) x = cb;
                const float f = XToFrame(x);
                const float vol = cv ? cv->EvalF(f, s->volumen) : s->volumen;
                float yy = (float)(y0 + h) - (vol / 1.25f) * (float)h;
                if (yy < (float)y0) yy = (float)y0; if (yy > (float)(y0 + h)) yy = (float)(y0 + h);
                if (px >= 0.0f) QuadLinea(g_curvaBuf, px, py, x, yy, (float)GlobalScale * 0.6f);
                px = x; py = yy;
                if (x >= cb) break;
            }
            if (!g_curvaBuf.empty()) {
                gfx::Color4f(1.0f, 0.78f, 0.25f, (k == 0) ? 1.0f : 0.5f);
                gfx::VertexPointer2f(0, &g_curvaBuf[0]); gfx::DrawTrianglesArray((int)(g_curvaBuf.size() / 2));
            }
        }
        // las PUNTAS (se agarran para recortar): solo en la primera pasada. Arrastrandola = VERDE; con el mouse
        // encima = BLANCA (y mas ancha); si no, celeste apagado
        if (k == 0) {
            for (int lado = TIRA_RECINI; lado <= TIRA_RECFIN; lado++) {
                const bool arrastre = (g_tiraModo == lado && !g_tiraOrig.empty() && g_tiraOrig[0].s == s);
                const bool hover = !arrastre && g_tiraModo == TIRA_NADA && g_tiraHoverS == s && g_tiraHoverB == lado;
                const int an = GlobalScale * ((arrastre || hover) ? 3 : 2);
                if (arrastre)   gfx::Color4f(TL_VERDE_LINEA[0], TL_VERDE_LINEA[1], TL_VERDE_LINEA[2], 1.0f);
                else if (hover) gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
                else            gfx::Color4f(0.85f, 0.95f, 1.0f, sel ? 0.9f : 0.5f);
                if (lado == TIRA_RECINI) { if (xa >= panelW) FillRect((int)xa, y0, an, h); }
                else                     { if (xb <= width)  FillRect((int)xb - an, y0, an, h); }
            }
        }
        if (!s->bucle) break;
    }
    gfx::Disable(gfx::Blend);
}

int Timeline::TiraEn(int mx, int my, Sonido** out) {
    if (out) *out = NULL;
    if (dopeRows.empty() || modo != TL_MODO_DOPE) return TIRA_NADA;
    const int lx = mx - x, ly = my - y;
    if (lx < panelW || ly < stripY || ly >= height) return TIRA_NADA;
    const int r = (ly - stripY - PosY) / (rowH > 0 ? rowH : 1);
    if (r < 0 || r >= (int)dopeRows.size()) return TIRA_NADA;
    const DopeRow& d = dopeRows[r];
    if (!d.sonido || d.oculto) return TIRA_NADA;
    Sonido* s = (Sonido*)d.sonido;
    const float L = s->DuracionEnEscena();
    const float xa = FrameToX(W3dSonidoFrameDeSeg(s->inicio));
    const float xb = (L > 0.0f) ? FrameToX(W3dSonidoFrameDeSeg(s->inicio + L)) : xa + GlobalScale * 3;
    const float fin = s->bucle ? (float)width : xb;
    const float borde = (float)TiraZonaBorde(rowH);
    if ((float)lx < xa - borde || (float)lx > fin + borde) return TIRA_NADA;
    if (out) *out = s;
    if (L <= 0.0f) return TIRA_MOVER;
    // la punta MAS CERCANA (en una tira corta las dos zonas se pisan): la izquierda, la derecha o el cuerpo
    const float da = fabsf((float)lx - xa), db = fabsf((float)lx - xb);
    if (da <= borde && da <= db) return TIRA_RECINI;
    if (db <= borde) return TIRA_RECFIN;
    return TIRA_MOVER;
}
void Timeline::TiraHover(int mx, int my) {
    Sonido* s = NULL;
    const int b = TiraEn(mx, my, &s);
    const int borde = (b == TIRA_RECINI || b == TIRA_RECFIN) ? b : 0;
    Sonido* hs = borde ? s : NULL;
    if (hs != g_tiraHoverS || borde != g_tiraHoverB) { g_tiraHoverS = hs; g_tiraHoverB = borde; g_redraw = true; }
}
bool Timeline::TiraClick(int mx, int my) {
    Sonido* s = NULL;
    const int modoT = TiraEn(mx, my, &s);
    if (modoT == TIRA_NADA || !s) return false;
    // el sonido clickeado pasa a ser el ACTIVO (su tarjeta en Properties); los seleccionados se corren juntos
    if (ObjActivo != s) { ObjActivo = s; if (!s->select) s->Seleccionar(); }
    g_tiraOrig.clear();
    for (size_t i = 0; i < ObjSelects.size(); i++) {
        Object* o = ObjSelects[i];
        if (!o || o->getType() != ObjectType::sonido) continue;
        if (modoT != TIRA_MOVER && o != s) continue;   // (recortar es de UNA)
        Sonido* so = (Sonido*)o; TiraOrig t = { so, so->inicio, so->recorteIni, so->recorteFin }; g_tiraOrig.push_back(t);
    }
    if (g_tiraOrig.empty()) { TiraOrig t = { s, s->inicio, s->recorteIni, s->recorteFin }; g_tiraOrig.push_back(t); }
    g_tiraModo = modoT; g_tiraGrab = false; g_tiraArrancada = false; g_tiraX0 = mx; g_tiraY0 = my;
    g_redraw = true;
    return true;
}
bool Timeline::TiraGrabStart() {
    g_tiraOrig.clear();
    for (size_t r = 0; r < dopeRows.size(); r++) {
        Object* o = dopeRows[r].sonido;
        if (!o || dopeRows[r].oculto) continue;
        bool ya = false; for (size_t i = 0; i < g_tiraOrig.size(); i++) if (g_tiraOrig[i].s == o) ya = true;
        if (ya) continue;
        Sonido* so = (Sonido*)o; TiraOrig t = { so, so->inicio, so->recorteIni, so->recorteFin }; g_tiraOrig.push_back(t);
    }
    if (g_tiraOrig.empty()) return false;
    g_tiraModo = TIRA_MOVER; g_tiraGrab = true; g_tiraArrancada = true; g_tiraX0 = lastMx; g_tiraY0 = lastMy;
    g_redraw = true;
    return true;
}
void Timeline::TiraAplicar(int mx) {
    if (g_tiraModo == TIRA_NADA) return;
    if (!g_tiraArrancada) {   // arrastre: un umbral para que un click limpio no corra la tira
        const int ddx = mx - g_tiraX0; if (ddx * ddx <= (3 * GlobalScale) * (3 * GlobalScale)) return;
        g_tiraArrancada = true;
    }
    const float fps = (float)(AnimFPS > 0 ? AnimFPS : 30);
    float dSeg = (float)(mx - g_tiraX0) / (pxPerFrame > 0.01f ? pxPerFrame : 0.01f) / fps;
    for (size_t i = 0; i < g_tiraOrig.size(); i++) {
        const TiraOrig& o = g_tiraOrig[i]; Sonido* s = o.s; if (!s) continue;
        const float tono = s->tono > 0.05f ? s->tono : 0.05f;
        const float dur = s->DuracionArchivo();
        if (g_tiraModo == TIRA_MOVER) {
            s->inicio = o.inicio + dSeg;
            if (LCtrlPressed) s->inicio = floorf(s->inicio * fps + 0.5f) / fps;   // Ctrl: enganchado al frame
        } else if (g_tiraModo == TIRA_RECINI) {
            float ri = o.ri + dSeg * tono;
            // Ctrl: la punta IZQUIERDA cae justo en un fotograma (lo que se mueve es su momento en la escena)
            if (LCtrlPressed) { const float t = floorf((o.inicio + (ri - o.ri) / tono) * fps + 0.5f) / fps; ri = o.ri + (t - o.inicio) * tono; }
            const float tope = dur - o.rf - 0.01f;
            if (ri < 0.0f) ri = 0.0f; if (ri > tope) ri = tope > 0.0f ? tope : 0.0f;
            s->recorteIni = ri;
            s->inicio = o.inicio + (ri - o.ri) / tono;   // lo que queda sigue en su lugar (se come la punta)
        } else {
            float rf = o.rf - dSeg * tono;
            // Ctrl: la punta DERECHA cae justo en un fotograma
            if (LCtrlPressed) {
                const float finT = floorf((o.inicio + (dur - o.ri - rf) / tono) * fps + 0.5f) / fps;
                rf = dur - o.ri - (finT - o.inicio) * tono;
            }
            const float tope = dur - o.ri - 0.01f;
            if (rf < 0.0f) rf = 0.0f; if (rf > tope) rf = tope > 0.0f ? tope : 0.0f;
            s->recorteFin = rf;
        }
    }
    g_redraw = true;
}
static void TiraCerrarUndo() {
    UndoTira* u = new UndoTira();
    for (size_t i = 0; i < g_tiraOrig.size(); i++) {
        const TiraOrig& o = g_tiraOrig[i];
        if (o.s && (o.s->inicio != o.inicio || o.s->recorteIni != o.ri || o.s->recorteFin != o.rf)) u->v.push_back(o);
    }
    if (u->v.empty()) { delete u; return; }
    UndoExterno f; f.aplicar = UndoTiraAplicar; f.desvincular = UndoTiraDesvincular; f.liberar = UndoTiraLiberar;
    UndoPushExterno(f, u);
}
void Timeline::TiraConfirmar() {
    if (g_tiraModo == TIRA_NADA) return;
    TiraCerrarUndo();
    g_tiraModo = TIRA_NADA; g_tiraGrab = false; g_tiraArrancada = false; g_tiraOrig.clear();
    g_redraw = true;
}
void Timeline::TiraCancelar() {
    for (size_t i = 0; i < g_tiraOrig.size(); i++) {
        const TiraOrig& o = g_tiraOrig[i]; if (!o.s) continue;
        o.s->inicio = o.inicio; o.s->recorteIni = o.ri; o.s->recorteFin = o.rf;
    }
    g_tiraModo = TIRA_NADA; g_tiraGrab = false; g_tiraArrancada = false; g_tiraOrig.clear();
    g_redraw = true;
}
// soltar el boton: un ARRASTRE termina ahi; el 'g' sigue hasta el click
void Timeline::TiraSoltar() {
    if (g_tiraModo == TIRA_NADA || g_tiraGrab) return;
    if (g_tiraArrancada) TiraConfirmar();
    else { g_tiraModo = TIRA_NADA; g_tiraOrig.clear(); }
}

// (harness: 'sonido tira') el MISMO camino que el mouse sobre la primera tira: el hover de cada punta (y la zona
// ancha para el dedo), agarrar la punta izquierda con Ctrl (cae justo en un fotograma), correr la tira con Ctrl y
// el Ctrl+Z. false + err si algo no da
bool W3dScriptTiraPrueba(Timeline* t, std::string& err) {
    t->ConstruirDopeRows();
    int r = -1;
    for (size_t i = 0; i < t->dopeRows.size() && r < 0; i++) if (t->dopeRows[i].sonido && t->dopeRows[i].tiraFilas >= 0) r = (int)i;
    if (r < 0) { err = "tira: no hay una fila de sonido"; return false; }
    Sonido* s = (Sonido*)t->dopeRows[(size_t)r].sonido;
    const float fps = (float)(AnimFPS > 0 ? AnimFPS : 30);
    const int my = t->y + t->stripY + r * t->rowH + t->PosY + t->rowH / 2;
    const int xa = t->x + (int)t->FrameToX(W3dSonidoFrameDeSeg(s->inicio));
    const int xb = t->x + (int)t->FrameToX(W3dSonidoFrameDeSeg(s->inicio + s->DuracionEnEscena()));
    if (xb - xa < GlobalScale * 40) { err = "tira: la tira es muy corta en pantalla para probar"; return false; }
    t->TiraHover(xa + 1, my);                if (g_tiraHoverB != TIRA_RECINI) { err = "tira: el hover no marca la punta izquierda"; return false; }
    t->TiraHover(xa + GlobalScale * 8, my);  if (g_tiraHoverB != TIRA_RECINI) { err = "tira: la zona de la punta es angosta"; return false; }
    t->TiraHover(xb - 1, my);                if (g_tiraHoverB != TIRA_RECFIN) { err = "tira: el hover no marca la punta derecha"; return false; }
    t->TiraHover((xa + xb) / 2, my);         if (g_tiraHoverB != 0) { err = "tira: el medio no es una punta"; return false; }
    const float ini0 = s->inicio, ri0 = s->recorteIni;
    const bool lmd = leftMouseDown, ctrl = LCtrlPressed;
    // la punta IZQUIERDA con Ctrl
    if (!t->TiraClick(xa + 1, my) || g_tiraModo != TIRA_RECINI) { err = "tira: el click no agarro la punta"; leftMouseDown = lmd; return false; }
    leftMouseDown = true; LCtrlPressed = true;
    t->TiraAplicar(xa + 1 + GlobalScale * 23);
    const float f1 = s->inicio * fps;
    leftMouseDown = false; t->TiraSoltar();
    printf("      [tira] recorte con Ctrl: inicio %.4f s (frame %.4f) recorteIni %.4f\n", s->inicio, f1, s->recorteIni);
    if (fabsf(f1 - floorf(f1 + 0.5f)) > 1e-3f || s->recorteIni <= ri0) { err = "tira: el recorte con Ctrl no cae en un fotograma"; LCtrlPressed = ctrl; return false; }
    // la tira entera con Ctrl
    const int mid = t->x + (int)t->FrameToX(W3dSonidoFrameDeSeg(s->inicio + s->DuracionEnEscena() * 0.5f));
    if (!t->TiraClick(mid, my) || g_tiraModo != TIRA_MOVER) { err = "tira: el click no agarro la tira"; LCtrlPressed = ctrl; return false; }
    leftMouseDown = true;
    t->TiraAplicar(mid + GlobalScale * 31);
    const float f2 = s->inicio * fps;
    leftMouseDown = false; t->TiraSoltar();
    LCtrlPressed = ctrl; leftMouseDown = lmd;
    printf("      [tira] mover con Ctrl: inicio %.4f s (frame %.4f)\n", s->inicio, f2);
    if (fabsf(f2 - floorf(f2 + 0.5f)) > 1e-3f) { err = "tira: mover con Ctrl no cae en un fotograma"; return false; }
    // Ctrl+Z dos veces: como estaba
    UndoDeshacer(); UndoDeshacer();
    printf("      [tira] Ctrl+Z x2: inicio %.4f recorteIni %.4f (antes %.4f / %.4f)\n", s->inicio, s->recorteIni, ini0, ri0);
    if (fabsf(s->inicio - ini0) > 1e-5f || fabsf(s->recorteIni - ri0) > 1e-5f) { err = "tira: el Ctrl+Z no la devolvio"; return false; }
    return true;
}
