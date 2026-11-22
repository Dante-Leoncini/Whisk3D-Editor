#include "WhiskUI/text/font.h"   // UTF8_Largo
#include "ViewPorts/Timeline.h"
#include "ViewPorts/TransformUI.h"     // TbGreenBg: el verde de un boton ACTIVO (como el proportional editing)
#include "edit/BoxSelect.h"             // la caja de seleccion (verde = entero adentro, azul = rozar)
#include "PopUp/TimePopup.h"            // el reloj: Inicio / Fin / FPS
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "w3dGraphics.h"
#include "WhiskUI/draw/glesdraw.h"          // W3dScreenHeight
#include "WhiskUI/theme/colors.h"           // ColorList / ColorID
#include "WhiskUI/draw/rectangle.h"         // Rec2D
#include "WhiskUI/text/bitmapText.h"        // RenderBitmapText / textAlign
#include "WhiskUI/core/UI.h"                // GlobalScale, RowHeightGS, CharacterWidthGS, LetterHeightGS, ...
#include "objects/Objects.h"           // ActiveObj, g_editMesh (mascara de vertices seleccionados)
#include "objects/Mesh.h"              // invalidar el skin al editar keyframes
#include "objects/EditMesh.h"          // vertSel/editVerts: la seleccion de Edit Mode (toggle Solo verts sel)
#include "objects/Armature.h"
#include "objects/Textures.h"          // Textures[0] = atlas (RenderBar/bordes)
#include "animation/Animation.h"       // StartFrame/EndFrame/CurrentFrame/PlayAnimation/AnimPlayDir + AnimProperty
#include "animation/SkeletalAnimation.h"
#include "animation/W3dAnimSet.h"   // clips COMPARTIDOS: la curva que se edita desde el dope es de un clip PROPIO
#include "animation/VertexAnimation.h"   // fila Vertex del dope: mover/borrar/interpolar frames
#include "animation/Armature2DAnimation.h" // filas del dope de los CLIPS del armature 2D (kind 4)
#include "ViewPorts/UVEditor.h"          // UVEditorInPoseMode: el dope 2D solo se lista posando (como el 3D)
#include "objects/ObjectMode.h"          // AutoKeyOn (el boton del timeline lo prende/apaga)
#include "Undo.h"                       // Ctrl+Z al editar keyframes (borrar/mover/curvar)
#include "render/RenderOptions.h"     // g_redraw
#include "PopUp/PopUpBase.h"          // PopUpActive
#include "WhiskUI/widgets/card.h"              // Card (boton verde redondeado del frame actual)
#include "WhiskUI/widgets/PopupMenu.h"         // dropdown de seleccion de animacion
#include "WhiskUI/draw/icons.h"             // dope sheet: iconos/flechas de las filas (igual que el Outliner)
#include "variables.h"                 // InteractionMode / PoseMode (el armature solo se lista en Pose Mode)
#include <math.h>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include "objects/Routine.h"    // las filas de los PASOS de una rutina animados (AnimStep)
#include "objects/Sound.h"      // las TIRAS de los sonidos (se mueven y se recortan en segundos)
#include "app/W3dShortcuts.h"   // las operaciones sin tecla de fabrica (Vista, interpolacion, handles)

namespace gfx = w3dEngine;

extern bool leftMouseDown, middleMouseDown, ViewPortClickDown;
extern bool LCtrlPressed;  // rueda: Ctrl+rueda = zoom cuando el dope sheet esta scrolleando en vertical
extern bool LShiftPressed; // dope sheet: shift+click = agregar a la seleccion
extern bool LAltPressed;   // dope sheet: Alt+A = deseleccionar todo

// tras EDITAR keyframes (borrar/mover): re-evaluar la animacion (pose + skin de las mallas) y redibujar
// Invalida la pose del esqueleto y el skin cacheado de las mallas: hay que llamarla despues de TOCAR una curva,
// o la escena sigue mostrando lo viejo. La usa el timeline y la tarjeta "Keyframe" del panel de propiedades (por
// eso no es static: una sola implementacion, no dos que se desincronicen).
void InvalidateAnimAndRedraw(){
    W3dAnimCurvesInvalidate();   // la curva cambio en el MISMO frame: que el 3D la vuelva a aplicar (no solo al mover el cabezal)
    if (ActiveAnimKind == 1 && ActiveAnimArm){ ActiveAnimArm->lastPoseFrame = -999999; ActiveAnimArm->poseDirty = true; }
    struct L { static void rec(Object* o){ if (!o) return;
        if (o->getType()==ObjectType::mesh){ Mesh* m=(Mesh*)o; if (m->skinArmature) m->lastSkinFrame = -999999; }
        for (size_t i=0;i<o->Childrens.size();i++) rec(o->Childrens[i]); } };
    L::rec(SceneCollection);
    g_redraw = true;
}

// roles de los botones de la barra (para el dispatch del click; no chocan con los BR_* del 3D)
enum { TL_ROLE_T0 = 300, TL_ROLE_START = 320, TL_ROLE_END = 321, TL_ROLE_ANIM = 322, TL_ROLE_SELECT = 323, TL_ROLE_PIVOT = 324,
       TL_ROLE_VIEW = 325, TL_ROLE_MODE = 326, TL_ROLE_KEY = 327, TL_ROLE_AUTOKEY = 328, TL_ROLE_FPS = 329,
       TL_ROLE_TIME = 330, TL_ROLE_SELMODE = 331,
       // la TOOLBAR de abajo: + KfConstant..KfElastic (todas las curvas) | + HFree..HPlane | + KfEaseAuto..InOut
       TL_TB_INTERP0 = 400, TL_TB_HANDLE0 = 420, TL_TB_EASE0 = 440,
       // ...y los de siempre (a la izquierda): deshacer / rehacer y, con el dedo, el Shift y el modo del arrastre
       TL_TB_UNDO = 460, TL_TB_REDO = 461, TL_TB_SHIFT = 462, TL_TB_SELEC = 463, TL_TB_VIEW = 464,
       TL_TB_TRIM = 465 };
bool g_tlViewMode = true;   // (Timeline.h) con el dedo, arrastrar mueve la vista; false = caja de seleccion
static Timeline* g_tlFingerPanel = NULL;
void TimelineFingerPanel(Timeline* t) { g_tlFingerPanel = t; }
// el alto que se come la TOOLBAR de abajo (ahora esta siempre): lo que hay detras no se ve, el scroll lo descuenta
static int TlToolbarHeight(const Timeline* t) { return (!t->ToolButtons.empty() && t->ToolbarVisible()) ? t->ToolbarHeight() : 0; }
int TimelineToolbarHeight(const Timeline* t) { return t ? TlToolbarHeight(t) : 0; }

// COLOR DE EJE de una curva: X rojo, Y verde, Z azul (los mismos ejes del viewport 3D).
static const float TL_COL_AXIS[3][4] = { {0.90f,0.25f,0.25f,1.0f}, {0.35f,0.85f,0.30f,1.0f}, {0.30f,0.50f,0.95f,1.0f} };
static const float* AxisColor(int comp){ return TL_COL_AXIS[(comp==AnimX)?0:(comp==AnimY)?1:2]; }
// La fila NORMALES de una vertex anim (capa propia, borrable) es la componente 2 = el azul del eje Z, y como
// normalmente cae encima de la fila Vertices se confundia con una curva de posicion. Va en VIOLETA: es una capa,
// no un eje.
static const float TL_COL_NORMALS[4] = { 0.72f, 0.45f, 0.95f, 1.0f };

// dropdown de animacion: menu JERARQUICO (Scenes + un submenu por armadura) construido por Properties.cpp; la
// seleccion (escena o clip) se aplica con AnimSelById (compartida con la tarjeta Animation).
extern void BuildMenuAnim(PopupMenu* menu); // Properties.cpp
extern void AnimSelById(int id);            // Properties.cpp
static PopupMenu* g_tlMenuAnim = NULL;
// Cierra el menu 'm' SOLO si es el que esta abierto.
// OJO: no se puede comparar OpenMenu contra un puntero que puede ser NULL. Cuando Click() elige un item
// terminal ya llama a Close(), que deja OpenMenu en NULL; si el menu contra el que se compara TAMBIEN es NULL
// (porque nunca se abrio), "NULL == NULL" da true y se llamaba Close() sobre NULL -> crash. Pasaba al elegir
// Bezier con la 't': ese submenu se abre solo, sin crear nunca el menu Key contra el que se comparaba.
static void TL_CloseMenu(PopupMenu* m){ if (m && OpenMenu == m) m->Close(); }

static void TL_menuAnimAction(int id){
    AnimSelById(id);
    TL_CloseMenu(g_tlMenuAnim);
    g_redraw = true;
}
static PopupMenu* g_tlMenuSel = NULL;    // menu Select del dope sheet (All / None / Invert)
static PopupMenu* g_tlMenuView = NULL;   // menu View del dope sheet (Frame Selected)
static PopupMenu* g_tlMenuPivot = NULL;  // menu Pivot del dope sheet (Center / Current Frame)
static PopupMenu* g_tlMenuKey = NULL;    // menu Key (Transform > Move/Rotate/Scale, Duplicate, Delete)
static PopupMenu* g_tlMenuCtx = NULL;
static PopupMenu* g_tlMenuEase = NULL;
static bool g_tlPlayheadHover = false;     // el mouse sobre la linea del frame actual: se pinta BLANCA
static bool g_tlButtonGreen = false;   // se apreto el boton verde del frame: click (editar) o arrastre (scrub)
static int  g_tlGreenButtonX0 = 0;
static Timeline* g_tlBox = NULL;         // el timeline que armo la caja de seleccion (si no, es la del 3D)   // Ctrl+E / Key > Easing Type: Set Keyframe Easing Type    // click derecho en el strip: Cortar / Copiar / Pegar / Duplicar / Borrar
static PopupMenu* g_tlMenuKeyXf = NULL;  // submenu Transform del menu Key
static PopupMenu* g_tlMenuKeyIn = NULL;  // submenu Interpolation Mode del menu Key ('t')
static PopupMenu* g_tlMenuHandle = NULL; // menu Handle Type ('v')

// ROJO del AUTO KEY prendido: esta GRABANDO todo lo que toques, tiene que cantar. Mismo criterio que el verde
// del play (tinte de fondo del boton), pero en rojo.
static float TL_RED_BTN[4]   = { 0.62f, 0.13f, 0.13f, 1.0f };
// verdes: fondo de botones de play activos / boton del frame actual, y la linea del playhead
static float TL_GREEN_BTN[4]  = { 0.12f, 0.45f, 0.18f, 1.0f };
static float TL_GREEN_LINE[4]= { 0.20f, 0.80f, 0.32f, 1.0f };

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
    gfx::Color4f(ColorList[(int)c][0], ColorList[(int)c][1], ColorList[(int)c][2], a);
}
// Trazo de las curvas: se arma TODO en este buffer y se dibuja de una sola vez por curva (batch). Es static para
// no re-alocar en cada frame: despues del primer dibujo ya tiene la capacidad que necesita.
static std::vector<float> g_curveBuf;
// agrega el segmento a->b al buffer, como un quad orientado (2 triangulos = 6 vertices)
static void QuadLine(std::vector<float>& b, float ax, float ay, float bx, float by, float w){
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
// del quad orientado vive en un solo lado (QuadLine). No usa GL_LINES: en ES1 el grosor de linea no es confiable
// entre drivers y el brazo del handle quedaba de un pixel o directamente invisible.
static std::vector<float> g_lineBuf;
static void ThinLine(float ax, float ay, float bx, float by){
    g_lineBuf.clear();
    QuadLine(g_lineBuf, ax, ay, bx, by, (float)GlobalScale * 0.5f);
    if (g_lineBuf.empty()) return;
    gfx::VertexPointer2f(0, &g_lineBuf[0]);
    gfx::DrawTrianglesArray((int)(g_lineBuf.size()/2));
}
// texto centrado horizontalmente en cx, con su top en py (necesita textura del atlas activa)
static void CenteredText(float cx, int py, const std::string& s){
    int tw = UTF8_Length(s) * CharacterWidthGS;
    gfx::PushMatrix();
    gfx::Translatef((float)((int)cx - tw/2), (float)py, 0);
    RenderBitmapText(s, textAlign::left, tw + GlobalScale*2);
    gfx::PopMatrix();
}
// glyph vectorial de transporte centrado en un rect (gx,gy,gw,gh)
static void DrawGlyph(int gx, int gy, int gw, int gh, int type, bool pause){
    float cx = gx + gw*0.5f, cy = gy + gh*0.5f, r = gh*0.20f; // r chico -> deja padding en el boton cuadrado
    if (pause){ VBar(cx - r*0.55f, cy, r, GlobalScale*1.6f); VBar(cx + r*0.55f, cy, r, GlobalScale*1.6f); return; }
    switch (type){
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
        ActiveAnimArm->activeAnim >= 0 && ActiveAnimArm->activeAnim < (int)ActiveAnimArm->animations.size()){
        // clip de armature ACTIVO: sus keyframes (de todos los huesos)
        SkeletalAnimation* an = ActiveAnimArm->animations[ActiveAnimArm->activeAnim];
        for (size_t t=0;t<an->tracks.size();t++)
            for (size_t p=0;p<an->tracks[t].Properties.size();p++)
                for (size_t k=0;k<an->tracks[t].Properties[p].keyframes.size();k++)
                    out.push_back(an->tracks[t].Properties[p].keyframes[k].frame);
    } else if (ActiveAnimKind == 3 && ActiveAnimMesh){
        // animacion PROPIA del objeto: los keyframes de SUS curvas (kind 3) Y los
        // KEYFRAMES DE VERTICES (la vertex anim ahora vive en cuadros arbitrarios)
        extern VertexAnimation* AnimActiveObject(Object*);
        VertexAnimation* an = AnimActiveObject((Object*)ActiveAnimMesh);
        if (an){
            for (size_t p=0;p<an->curves.size();p++)
                for (size_t k=0;k<an->curves[p].keyframes.size();k++)
                    out.push_back(an->curves[p].keyframes[k].frame);
            for (size_t k=0;k<an->frames.size();k++)
                out.push_back(an->frames[k]->frame);
        }
    } else {
        // animacion de ESCENA activa: keyframes de TODOS los objetos de la escena (AnimationObjects = escena activa)
        for (size_t i=0;i<AnimationObjects.size();i++)
            for (size_t p=0;p<AnimationObjects[i].Properties.size();p++)
                for (size_t k=0;k<AnimationObjects[i].Properties[p].keyframes.size();k++)
                    out.push_back(AnimationObjects[i].Properties[p].keyframes[k].frame);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
}

// clip de animacion ACTIVO segun la seleccion APP-WIDE (no depende del objeto seleccionado) o NULL si la animacion
// activa es una ESCENA. Asi clickear un armature NO cambia el rango/seleccion del timeline.
static SkeletalAnimation* ActiveClip(){
    if (ActiveAnimKind == 1 && ActiveAnimArm &&
        ActiveAnimArm->activeAnim >= 0 && ActiveAnimArm->activeAnim < (int)ActiveAnimArm->animations.size())
        return ActiveAnimArm->animations[ActiveAnimArm->activeAnim];
    return NULL;
}

// clip del ARMATURE 2D ACTIVO (kind 4) o NULL. Espejo exacto de ActiveClip para el rig 2D.
static Armature2DAnimation* Clip2DActive(){
    if (ActiveAnimKind != 4 || !ActiveAnimMesh) return NULL;
    return Arm2DActiveClip(ActiveAnimMesh);
}

// ================================================================================================
//  EL ownerKey DEL DOPE DICE DE QUE LISTA ES (fallos [A] y [B] de la ronda 7)
//
//  La seleccion del dope son STRINGS en un set: sobrevive a TODO, incluido cambiar de clip / de
//  escena / de armature 2D. Las claves decian QUE curva ("arm:<rig>/b<n>", "obj:<nombre>") pero NO
//  de QUE lista, y el resolver las ataba a lo que estuviera ACTIVO AL USARLAS (ActiveClip /
//  AnimationObjects / Clip2DActive). O sea la regla R1 de Undo.h, pero fuera del undo: elegir un
//  keyframe en el clip K0, pasar a K1 y la tarjeta "Keyframe" editaba (valor/frame/interpolacion/
//  handles) la curva de K1, y la 'X' borraba SUS keyframes; encima se veian como seleccionados.
//  Igual con las escenas ("obj:") y con el rig 2D.
//
//  Ahora la IDENTIDAD DE LA LISTA va DENTRO de la clave, como primer segmento (asi la heredan las
//  filas hijas y DopeCovers sigue funcionando: el corte cae siempre en un '/'):
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
//  nombre" esta escrito en Timeline.h, arriba de DopeIdOwner.
//
//  *** EL DUENO VA PRIMERO Y EL NUMERO DESPUES (ronda 8) ***
//  El clip 3D iba al reves ("arm:k<CLIP>/<rig>/..."), o sea el NUMERO antes del DUENO, y eso
//  rompia la otra mitad: los avisos de remapeo (DopeRemapIndexKey) piden que el numero venga
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
//  LA MISMA malla comparten clave. Ese agujero lo cierra DopeReleaseVertexAnim, que suelta la
//  seleccion en todo camino que cambie la anim activa.
// ================================================================================================
// EL DUENO adentro de una clave: "#<serial>" (ver el bloque de arriba y Timeline.h). Sale de una
// sola funcion porque lo usan el que ARMA las claves, el que las RESUELVE y los que avisan un
// remapeo desde otros archivos (BoneEdit.cpp, UVEditor.cpp, Undo.cpp): dos formatos = el bug.
std::string DopeIdOwner(const Object* o){
    if (!o) return std::string();
    char b[24]; snprintf(b, sizeof b, "#%u", o->serial);
    return std::string(b);
}
// Los TRES prefijos salen de ACA, una sola vez: el que ARMA las claves de las filas y el que las
// RESUELVE usan la misma funcion, asi no puede haber dos formatos.
// "obj:e<ESCENA>/" = las curvas de objeto de la escena activa
static std::string DopePrefScene(){
    InitSceneAnimations();
    char b[32]; snprintf(b, sizeof b, "obj:e%d/", ActiveSceneAnim);
    return std::string(b);
}
// "arm:#<serial rig>/k<CLIP>" = vacio si no hay clip 3D activo. DUENO PRIMERO, NUMERO DESPUES (ver
// el bloque de arriba): asi el aviso de remapeo de clips se acota a UN rig ("arm:" + id + "/k").
static std::string DopePrefArm(){
    if (!ActiveAnimArm) return std::string();
    char b[32]; snprintf(b, sizeof b, "/k%d", ActiveAnimArm->activeAnim);
    return "arm:" + DopeIdOwner(ActiveAnimArm) + b;
}
// "arm2d:#<serial malla>/a<ARM2D>/k<CLIP>" = vacio si no hay clip 2D activo
static std::string DopePrefArm2D(){
    if (!ActiveAnimMesh) return std::string();
    char b[48]; snprintf(b, sizeof b, "/a%d/k%d", ActiveAnimMesh->armature2dActive, ActiveAnimMesh->Arm2DActiveAnim());
    return "arm2d:" + DopeIdOwner(ActiveAnimMesh) + b;
}
// la clave arranca con 'pref' Y sigue con un numero? (sin esto, atoi de "" da 0 y una clave
// truncada editaria el hueso/objeto 0)
static bool DopeAfterPrefix(const std::string& k, const std::string& pref){
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
static std::set<std::string> g_dopeCollapsed;
static bool DopeCollapsed(const std::string& k){ return !k.empty() && g_dopeCollapsed.count(k) > 0; }
// OJO apagado, por fila (mismo criterio que el plegado: se recuerda por keyRow). Ocultar una curva la saca del
// dibujo y de la edicion; con 18 canales encimados es la unica forma de trabajar tranquilo sobre uno.
// AUTO FRAME (View > Auto frame, PRENDIDO por defecto): al clickear una fila del panel se elige toda su curva y
// se la ENCUADRA sola, sin tener que apretar numpad '.'. Es lo que se quiere el 99% de las veces: si vas a mirar
// una curva, la queres ver.
// Panel del dope sheet a la vista, si/no. En el telefono se come media pantalla y muchas veces solo queres el
// tiempo. Es lo MISMO que plegar el summary a mano (panelW=0), pero desde el menu y sin tener que buscar la flecha.
static bool g_dopePanelOn = true;
static bool g_dopeAutoFrame = true;
// HANDLES A LA VISTA (Vista > Mostrar handles, solo en curvas). Apagado no se dibujan, no se agarran y el encuadre no
// los cuenta: con muchas curvas encimadas molestan, y con el dedo se agarraba un handle queriendo mover la vista.
bool g_tlShowHandles = true;
// las TIRAS de sonido: su forma de onda y la linea de su volumen (sin las dos es solo la tira, de una fila)
static bool g_tlShowWave = true, g_tlShowVolume = true;
static const int kStripRowsExtra = 2;   // con alguna de las dos, la tira ocupa su fila y dos mas
// TOGGLE "Solo vertices seleccionados" (menu Vista, dope Y curvas; static = persiste la
// sesion). Con la malla en EDIT MODE:
//  - VISTA: las filas de vertex anim (Vertices/UV/Normales) muestran SOLO los keyframes
//    que le cambian algo a los vertices seleccionados (los demas cuadros no les mueven
//    ni un vertice: son ruido para lo que estas editando).
//  - EDICION: MOVER un keyframe de esas filas se resuelve por MASCARA de vertices
//    (VertexAnimMoveKeysSelective): sobre otro keyframe = merge selectivo (solo los
//    seleccionados se pisan), sobre un frame vacio = keyframe completo con los no
//    seleccionados EVALUADOS (no saltan). Asi se anima o DES-anima un subconjunto.
static bool g_dopeOnlySelVerts = false;
static float g_scrubAccum = 0.0f;  // resto en frames del scrub por flecha (ver ScrubArrow)
static int g_scrubWait    = 0;      // ticks que el scrub espera despues del toque (ver W3dTimelinePressedArrow)
static std::set<std::string> g_dopeEyeOff;
static bool DopeEyeOff(const std::string& k){ return !k.empty() && g_dopeEyeOff.count(k) > 0; }

// ---- SELECCION ----
// Filas (click en el panel) y KEYFRAMES (click en el strip). La unidad seleccionable es UN keyframe de UNA CURVA:
// (dueño, propiedad, componente, frame). X, Y y Z son curvas independientes -> "X Location @4" es un keyframe
// DISTINTO de "Y Location @4": se selecciona, mueve y borra por separado.
static std::set<std::string> g_dopeRowSel;   // keyRow
static std::set<std::string> g_dopeKeySel;   // DopeKeyId(ownerKey, propId, compId, frame)
// EL AREA PARA AGARRAR un keyframe / un handle (px): generosa con el mouse, mas con el dedo
static float GrabRadius() {
#ifndef W3D_SYMBIAN
    extern Uint32 g_lastFingerTicks;
    if (g_lastFingerTicks != 0) return (float)GlobalScale * 20.0f;   // el dedo tapa lo que agarra: bien grande
#endif
    return (float)GlobalScale * 8.0f;
}
// lo FINO (la linea del frame actual, el borde del panel): 3 px con el mouse, mucho mas con el dedo
static float LineRadius() {
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
static bool g_dopeActHas = false;
static std::string DopeKeyId(const std::string& ownerKey, int propId, int compId, int frame){
    char b[48]; snprintf(b,sizeof b,"|p%d|c%d|f%d",propId,compId,frame); return ownerKey + b;
}

// ============================================================================
//  LA SELECCION DEL DOPE TAMBIEN GUARDA INDICES (la misma clase de bug que el undo)
//
//  El ownerKey de una fila de hueso lleva el INDICE del hueso adentro: "arm:<rig>/b<IDX>" (3D) y
//  "arm2d:<malla>/a<IDX_ARM>/b<IDX>" (2D). Esas claves sobreviven a todo (son strings en un set,
//  no punteros), asi que cuando algo CORRE esos indices -BoneEditDelete / Bone2DDelete borran un
//  hueso y compactan; UndoArm2DDelete saca un armature 2D entero- la clave vieja "b<n>" pasa a
//  matchear la fila NUEVA de OTRO hueso. Nadie limpiaba ni remapeaba nada, y el resultado era
//  mudo: la tarjeta "Keyframe" editaba (frame/valor/interpolacion/handles) la curva del hueso
//  EQUIVOCADO y la 'X' del dope borraba los keyframes del hueso equivocado.
//  Se remapea con la MISMA convencion del undo (ver RemapIndexSaved en main/undo/Undo.cpp):
//  b >= 0 = intercambio a<->b; b < 0 = se BORRO el indice 'a' (las claves de ESE hueso se van
//  de la seleccion; las de arriba bajan uno).
// ============================================================================
// el ownerKey de una clave: DopeKeyId le agrega "|pN|cM|fF" y keyRow de un canal "|pN|cM", asi
// que el ownerKey es todo lo que hay antes del ULTIMO "|p" (un nombre con '|' adentro no rompe).
static std::string DopeKeyOwner(const std::string& k){
    const size_t q = k.rfind("|p");
    return (q == std::string::npos) ? k : k.substr(0, q);
}
// "<prefijo><numero><resto>" -> numero + resto. false si la clave no es de ese prefijo.
static bool DopeKeySplit(const std::string& owner, const std::string& prefix, int& num, std::string& rest){
    if (owner.size() <= prefix.size() || owner.compare(0, prefix.size(), prefix) != 0) return false;
    size_t j = prefix.size();
    while (j < owner.size() && owner[j] >= '0' && owner[j] <= '9') j++;
    if (j == prefix.size()) return false;     // no hay numero pegado al prefijo
    num = atoi(owner.c_str() + prefix.size());
    rest = owner.substr(j);
    return true;
}
// devuelve false si el indice guardado era JUSTO el borrado (misma semantica que el undo)
static bool DopeRunIndex(int& idx, int a, int b){
    if (b >= 0){ if (idx == a) idx = b; else if (idx == b) idx = a; return true; }
    if (idx == a) return false;
    if (idx >  a) idx--;
    return true;
}
static void DopeRemapSet(std::set<std::string>& S, const std::string& prefix, int a, int b, bool insert){
    std::set<std::string> newValue;
    for (std::set<std::string>::const_iterator it = S.begin(); it != S.end(); ++it){
        const std::string owner = DopeKeyOwner(*it);
        int num; std::string rest;
        if (!DopeKeySplit(owner, prefix, num, rest)){ newValue.insert(*it); continue; } // no es de esta lista
        if (insert){ if (num >= a) num++; }
        else if (!DopeRunIndex(num, a, b)) continue;                                  // era ESE: la clave se va
        char nb[24]; snprintf(nb, sizeof nb, "%d", num);
        newValue.insert(prefix + nb + rest + it->substr(owner.size()));
    }
    S.swap(newValue);
}
static void DopeRemapAll(const std::string& prefix, int a, int b, bool insert){
    if (prefix.empty()) return;
    DopeRemapSet(g_dopeKeySel,    prefix, a, b, insert);   // keyframes elegidos
    DopeRemapSet(g_dopeRowSel,    prefix, a, b, insert);   // filas elegidas
    DopeRemapSet(g_dopeCollapsed, prefix, a, b, insert);   // estado de plegado (misma clave)
    DopeRemapSet(g_dopeEyeOff,    prefix, a, b, insert);   // filas con el ojo apagado
    if (g_dopeActHas){                                     // el keyframe ACTIVO (el de la tarjeta)
        int num; std::string rest;
        if (DopeKeySplit(g_dopeActOwner, prefix, num, rest)){
            if (insert){ if (num >= a) num++; }
            else if (!DopeRunIndex(num, a, b)){ g_dopeActHas = false; g_redraw = true; return; }
            char nb[24]; snprintf(nb, sizeof nb, "%d", num); g_dopeActOwner = prefix + nb + rest;
        }
    }
    g_redraw = true;
}
void DopeRemapIndexKey(const std::string& prefix, int a, int b){ DopeRemapAll(prefix, a, b, false); }
void DopeInsertIndexKey(const std::string& prefix, int a){    DopeRemapAll(prefix, a, -1, true); }
// El harness de tests elige/lee el keyframe ACTIVO sin simular el click (mismo idiom que
// _DeleteVertexFwdAnim de Properties.cpp: se aprieta el MISMO boton que el usuario, pero la
// seleccion del dope es estado interno del archivo). Es exactamente lo que deja apuntando la
// tarjeta "Keyframe" del panel de propiedades.
void _DopePickKeyFwd(const std::string& ownerKey, int propId, int compId, int frame){
    g_dopeKeySel.insert(DopeKeyId(ownerKey, propId, compId, frame));
    g_dopeActOwner = ownerKey; g_dopeActProp = propId; g_dopeActComp = compId;
    g_dopeActFrame = frame; g_dopeActHas = true;
}
std::string _DopeActiveOwnerFwd(){ return g_dopeActHas ? g_dopeActOwner : std::string("(ninguno)"); }
bool _DopeKeySelHasFwd(const std::string& ownerKey, int propId, int compId, int frame){
    return g_dopeKeySel.count(DopeKeyId(ownerKey, propId, compId, frame)) > 0;
}
int  _DopeKeySelCountFwd(){ return (int)g_dopeKeySel.size(); }
// LA OTRA MITAD: las filas de una vertex anim NO dicen de QUE vertex anim son (se resuelve por la
// ACTIVA al usarlas) y su seleccion sobrevive a todo. Cambiar de vertex anim con keyframes
// elegidos aplicaba esa seleccion -mover / borrar / interpolacion, y la tarjeta Keyframe- a la
// OTRA anim. No hay indice que correr (son otra lista entera): se SUELTAN.
// SON CUATRO FILAS, no tres:
//   "objvtx:<malla>" / "objuv:<malla>" / "objnrm:<malla>" = las capas de POSES (DopeVertexAnimOf),
//   "objanim:<malla>"                                     = las curvas de TRANSFORM propias de la
//        anim (VertexAnimation::curvas), que resuelve DopeResolveProp por AnimActiveObject. Es
//        la fila PADRE de las otras tres y tiene canales propios (X/Y/Z Location...): quedaba
//        afuera de este soltado, asi que tras cambiar de anim la tarjeta "Keyframe" editaba -y la
//        'X' borraba- la curva homonima de una anim que el usuario nunca eligio. Mismo bug, misma
//        cura (ver el test 'dopeobjanim').
void DopeReleaseVertexAnim(){
    static const char* pref[4] = { "objvtx:", "objuv:", "objnrm:", "objanim:" };
    for (int p = 0; p < 4; p++){
        const size_t n = std::string(pref[p]).size();
        for (std::set<std::string>::iterator it = g_dopeKeySel.begin(); it != g_dopeKeySel.end(); ){
            if (it->compare(0, n, pref[p]) == 0) g_dopeKeySel.erase(it++); else ++it;
        }
        for (std::set<std::string>::iterator it = g_dopeRowSel.begin(); it != g_dopeRowSel.end(); ){
            if (it->compare(0, n, pref[p]) == 0) g_dopeRowSel.erase(it++); else ++it;
        }
        if (g_dopeActHas && g_dopeActOwner.compare(0, n, pref[p]) == 0) g_dopeActHas = false;
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
//  DopeCycleAxis, que son publicos. Tenerlos aca adentro obligaba a los llamadores a pasar numeros sueltos.)
enum { DOPE_PIV_CENTER = 0, DOPE_PIV_CURFRAME = 1, DOPE_PIV_MEDIAN = 2, DOPE_PIV_ACTIVE = 3 };
static int  g_dopePivot = DOPE_PIV_CENTER;
// QUE SE ELIGE con el click / el dedo / el lapiz (el boton del MOUSE de la barra):
//   AMBOS    el click toma el keyframe mas cercano y, si no hay, el TRAMO de curva que toco; el lapiz, de keyframe en keyframe
//   CURVA    el click toma el TRAMO (el pedazo de curva entre dos keyframes: quedan elegidos los dos) y el lapiz recorre
//            los tramos uno por uno. Asi en el telefono se elige un tramo y se le cambia la interpolacion o se lo mueve
//   KEYFRAME el click solo toma keyframes; el lapiz, de keyframe en keyframe (lo de siempre)
enum { TL_SEL_BOTH = 0, TL_SEL_CURVE = 1, TL_SEL_KEYFRAME = 2 };
static int  g_tlSelMode  = TL_SEL_BOTH;
static int  g_segmentRow = -1, g_segmentI = -1;   // el tramo que recorre el lapiz (fila de dopeRows, keyframe de la izquierda)
static bool g_dopeMov = false;
static int  g_dopeMovMode = 0;        // DOPE_MOV / DOPE_ESC / DOPE_ROT
static int  g_dopeMovAxis = Timeline::DOPE_AXIS_FREE;
static float g_dopeMovVal = 0.0f;     // mover: frames a correr; escalar: factor; rotar: grados
static float g_dopeMovValV = 0.0f;    // curvas + mover: cuanto se corre en VALOR (el eje vertical)
// ESCALAR con un factor por eje: la tarjeta "ajustar la ultima operacion" deja poner X (tiempo) e Y (valor) distintos.
// Mientras se arrastra los dos son el mismo (g_dopeMovVal); g_dopeEscXY = true usa los de la tarjeta.
static bool  g_dopeEscXY = false;
static float g_dopeEscXv = 1.0f, g_dopeEscYv = 1.0f;
static float DopeEscX(){ return g_dopeEscXY ? g_dopeEscXv : g_dopeMovVal; }
static float DopeEscY(){ return g_dopeEscXY ? g_dopeEscYv : g_dopeMovVal; }
static int  g_dopeMovX0 = 0, g_dopeMovY0 = 0;
static float g_dopePivotF = 0.0f;     // pivote en FRAMES  (eje tiempo)
static float g_dopePivotV = 0.0f;     // pivote en VALOR   (eje vertical; solo curvas)
static float g_dopeRotAng0 = 0.0f;    // angulo mouse-pivote al arrancar el rotar
// CURSOR VIRTUAL del transform. El cursor REAL se ENVUELVE de un borde al otro del timeline (igual que al panear)
// para poder seguir escalando/rotando sin quedarse sin pantalla. Ese salto haria pegar un tiron si el transform
// mirara la posicion real, asi que se lleva una posicion propia que solo ACUMULA los deltas dx/dy: en el frame del
// envolvimiento CheckWarpMouseInViewport los pone en 0, con lo cual la virtual sigue derecho y no se entera.
static int  g_dopeVirtX = 0, g_dopeVirtY = 0;
static bool g_dopeVirtFirst = false; // el 1er motion tras arrancar: dx/dy todavia son del evento ANTERIOR (igual
                                       // que g_xformFirstMov en el 3D) -> se ignoran o el transform pega un salto
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
                     std::vector<char> still; };   // quieto[k]: orig[k] es un PISADO (ver g_dopeOverwritten): nunca se mueve
static std::vector<DopeMovProp> g_dopeMovSnap;
// LO QUE DUPLICAR Y PEGAR TAPAN AL NACER (pedido del duenio, 2026-11-09). La copia nace en un cuadro donde puede haber
// otro keyframe y enseguida se agarra para moverla. Antes ese keyframe se BORRABA en el acto: al correr la copia ya no
// volvia ("muevo lo duplicado por arriba del keyframe 2 y se borra"). Ahora se saca pero se anota aca, y al arrancar el
// movimiento vuelve a la foto como un keyframe QUIETO: mientras la copia esta encima no se ve, si la copia se va vuelve,
// y solo queda pisado si la copia se SUELTA encima (lo mismo que pasa al mover cualquier keyframe).
static std::vector<DopeMovProp> g_dopeOverwritten;
static void DopeOverwrittenAnnotate(const std::string& owner, int propId, int compId, const keyFrame& kf){
    for (size_t i=0;i<g_dopeOverwritten.size();i++){ DopeMovProp& p = g_dopeOverwritten[i];
        if (p.ownerKey==owner && p.propId==propId && p.compId==compId){ p.orig.push_back(kf); return; } }
    DopeMovProp p; p.ownerKey=owner; p.propId=propId; p.compId=compId; p.orig.push_back(kf); g_dopeOverwritten.push_back(p);
}
// el keyframe k de la foto, se mueve? (los elegidos; los pisados nunca: comparten el cuadro con la copia)
static bool DopeMovSel(const DopeMovProp& mp, size_t k){
    if (k < mp.still.size() && mp.still[k]) return false;
    for (size_t i=0;i<mp.selFrames.size();i++) if (mp.selFrames[i]==mp.orig[k].frame) return true;
    return false;
}
// snapshot paralelo para MOVER las filas VERTICES/UV (los frames de la vertex anim NO
// son AnimProperty). UN snapshot por ANIM: las dos filas comparten los VertexFrame (un
// frame con ambas capas es UN solo cuadro), asi que se guardan punteros ESTABLES a los
// VertexFrame + su frame original + en que row(s) estaba seleccionado cada uno. Durante
// el arrastre solo se reasigna ->frame (nada se borra), asi los punteros siguen validos;
// las colisiones se resuelven al confirmar.
struct DopeMovVtxSnap {
    VertexAnimation* an;
    std::string ownerVtx, ownerUV, ownerNrm; // clave de cada fila presente (vacia = sin esa fila)
    std::vector<VertexFrame*> vf; std::vector<int> origFrame;
    std::vector<char> selPos, selUV, selNrm; // elegido en la fila Vertices / UV / Normales
    // MOVE SELECTIVO (toggle "Solo vertices seleccionados" + malla en Edit Mode con verts elegidos): el arrastre
    // se PREVISUALIZA igual (los frames corren), pero al CONFIRMAR no se resuelve por "el movido pisa al quieto"
    // sino por VertexAnimMoveKeysSelective con esta mascara (merge por vertice). Vacia = move de siempre.
    std::vector<unsigned char> mask;
    bool selective;
    DopeMovVtxSnap() : an(NULL), selective(false) {}
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
static bool DopeCovers(const std::string& parent, const std::string& child){
    if (parent.empty()) return true;                                  // Summary: cubre todo
    if (child.size() < parent.size()) return false;
    if (child.compare(0, parent.size(), parent) != 0) return false;
    return child.size() == parent.size() || child[parent.size()] == '/'; // el corte tiene que caer en un separador
}
// lo expone el test 'dopecubre' (la regla es chica pero su bug era perdida de datos: conviene tenerla fijada)
bool W3dScriptDopeCovers(const std::string& parent, const std::string& child){ return DopeCovers(parent, child); }

// LA CLAVE DICE LA MALLA DE LA ANIM ACTIVA? Las CUATRO filas de vertex anim
// ("objanim:/objvtx:/objuv:/objnrm:#<serial>") se resuelven por la ACTIVA al usarlas, asi que
// sin este chequeo una clave de OTRA malla termina editando/borrando las curvas de la malla
// activa. Sus tres hermanas ("obj:", "arm:", "arm2d:") ya validaban el dueno; la de "objanim:"
// era la unica que ni lo miraba (fallo 2 de la ronda 9: DeleteUndo::Aplicar restaura
// ActiveAnimMesh a la malla que vuelve y la clave elegida era de OTRA malla).
static bool DopeOwnerIsAnimMesh(const std::string& ownerKey, size_t nPref){
    if (!ActiveAnimMesh || ownerKey.size() < nPref) return false;
    return ownerKey.substr(nPref) == DopeIdOwner((const Object*)ActiveAnimMesh);
}
// la vertex anim ACTIVA si el ownerKey es de una fila de vertex anim de la malla activa
// ("objvtx:#<serial>" = fila Vertices/capa posiciones, "objuv:#<serial>" = fila UV/capa uv,
// "objnrm:#<serial>" = fila Normales/capa normales) o NULL. Los frames de vertices NO
// son AnimProperty: las operaciones del dope (mover/borrar/interpolar) los tocan por
// aca, no por DopeResolveProp.
static VertexAnimation* DopeVertexAnimOf(const std::string& ownerKey){
    size_t nPref = 0;
    if      (ownerKey.compare(0,7,"objvtx:")==0) nPref = 7;
    else if (ownerKey.compare(0,6,"objuv:") ==0) nPref = 6;
    else if (ownerKey.compare(0,7,"objnrm:")==0) nPref = 7;
    else return NULL;
    if (!DopeOwnerIsAnimMesh(ownerKey, nPref)) return NULL;
    extern VertexAnimation* AnimActiveObject(Object*);
    return AnimActiveObject((Object*)ActiveAnimMesh);
}
// que CAPA representa la fila: 0 = Vertices (posiciones), 1 = UV, 2 = Normales,
// -1 = no es fila de vertex anim
static int DopeVertexLayerOf(const std::string& ownerKey){
    if (ownerKey.compare(0,7,"objvtx:")==0) return 0;
    if (ownerKey.compare(0,6,"objuv:")==0)  return 1;
    if (ownerKey.compare(0,7,"objnrm:")==0) return 2;
    return -1;
}
// este VertexFrame trae la capa de esa fila? (el flag por-frame es el puntero)
static bool VertexFrameInLayer(const VertexFrame* vf, int layer){
    if (!vf) return false;
    return layer==0 ? vf->positions!=NULL : layer==1 ? vf->uvs!=NULL : vf->normals!=NULL;
}
// cuantas capas trae el frame (para saber si borrarle una lo deja vacio)
static int VertexFrameLayers(const VertexFrame* vf){
    if (!vf) return 0;
    return (vf->positions?1:0) + (vf->uvs?1:0) + (vf->normals?1:0);
}
// MASCARA de vertices seleccionados de 'm' en EDIT MODE, sobre los verts GPU (paralela a
// mesh->vertex, que es como estan armados los frames de la vertex anim). La seleccion
// vive en el EditMesh por vertice EDITABLE (posiciones unicas); aca se expande a todas
// las copias GPU via posRep, igual que PushPositions. false si la malla no esta en
// edicion o no hay ningun vertice seleccionado (= no hay mascara que aplicar).
static bool DopeMaskVertsSel(Mesh* m, std::vector<unsigned char>& mask){
    if (!m || InteractionMode != EditMode || g_editMesh != (Object*)m || !m->edit) return false;
    EditMesh* e = m->edit;
    const int nV = m->vertexSize;
    if (nV <= 0 || e->editVerts.empty()) return false;
    std::vector<int> repToEdit(nV, -1);   // rep GPU -> vert editable k
    for (size_t k=0;k<e->editVerts.size();k++)
        if (e->editVerts[k]>=0 && e->editVerts[k]<nV) repToEdit[e->editVerts[k]] = (int)k;
    const bool hasRep = ((int)m->posRep.size() == nV);
    mask.assign(nV, 0);
    bool any = false;
    for (int gv=0; gv<nV; gv++){
        int rep = hasRep ? m->posRep[gv] : gv;
        int k = (rep>=0 && rep<nV) ? repToEdit[rep] : -1;
        if (k>=0 && k<(int)e->vertSel.size() && e->vertSel[k]){ mask[gv]=1; any=true; }
    }
    return any;
}
// El filtro de VISTA del toggle: este cuadro le ANIMA algo a los vertices de la mascara? La regla vive en el
// Core (VertexAnimKeyContributesSel) y es la MISMA con la que el move selectivo decide si el frame origen sobrevive:
// un keyframe cuyos verts seleccionados caen justo sobre la interpolacion de los vecinos no les anima nada. Que
// sean la misma regla no es un detalle: despues de mover un key con el toggle prendido, el cuadro de origen
// desaparece de la fila filtrada EXACTAMENTE cuando dejo de animar a esos vertices (se ve como que "se movio").
static bool VertexFrameAnimatesSel(const VertexAnimation& an, const VertexFrame* vf, int layer,
                                const std::vector<unsigned char>& mask){
    if (!vf || mask.empty()) return true;
    return VertexAnimKeyContributesSel(an, layer, vf->frame, &mask[0]);
}
// Un vertex animation es una secuencia de POSES: no hay un "valor" escalar como en una curva
// de transform. Para poder VERLO y editarlo en el modo curvas, se arma una curva temporal
// donde el valor de cada keyframe = su INDICE (0,1,2...): queda una rampa creciente cuyos
// TRAMOS muestran la interpolacion (recta/escalon/bezier) entre pose y pose. Es SOLO para
// dibujar y hacer hit-test: mover/borrar/interpolar siguen yendo a los VertexFrame (mismo
// ownerKey "objvtx:"/"objuv:"). Devuelve la anim (o NULL) y llena 'out' con la curva temporal.
// CAPAS: cada fila (Vertices / UV) arma SU curva solo con los frames que traen su capa.
// SIN OVERSHOOT + curva = verdad: los handles de la curva temporal NO son los crudos del
// VertexFrame sino los EFECTIVOS + CLAMPEADOS que devuelve VertexAnimHandlesSegment — el
// MISMO evaluador que usa la reproduccion (EvalCurveVertexAnim). Van como HFree (ya
// resueltos): el trazo que dibuja CurveStroke y el handle que dibuja/agarra HandlePos son
// EXACTAMENTE la funcion que corre al reproducir. Imposible que diverjan.
static VertexAnimation* VertexChannelTemp(const std::string& ownerKey, AnimProperty& out){
    VertexAnimation* van = DopeVertexAnimOf(ownerKey);
    if (!van) return NULL;
    const int layer = DopeVertexLayerOf(ownerKey);
    out.Property = AnimVertex; out.component = layer; out.keyframes.clear();
    const VertexFrame* prev = NULL;
    for (size_t k=0;k<van->frames.size();k++){
        const VertexFrame* vf = van->frames[k];
        if (!VertexFrameInLayer(vf, layer)) continue;    // cada curva muestra SUS keyframes
        keyFrame kf;
        kf.frame = vf->frame;
        kf.value = (float)out.keyframes.size();  // valor = indice EN LA CAPA: rampa 0,1,2...
        kf.Interpolation = vf->interp;
        kf.handleType = HFree;                   // resueltos aca: nadie los recalcula distinto
        kf.inDF = 0.0f; kf.inDV = 0.0f; kf.outDF = 0.0f; kf.outDV = 0.0f;
        if (prev){
            float oDF, oDV, iDF, iDV;
            VertexAnimHandlesSegment(*prev, *vf, oDF, oDV, iDF, iDV);
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
static AnimProperty* DopeResolveProp(const std::string& ownerKey, int propId, int compId, bool edit = true){
    if (ownerKey.compare(0,8,"camact:e")==0){          // pista "Camara activa" de la escena (solo la ACTIVA)
        if (propId != AnimCamActive || compId != AnimX) return NULL;
        if (atoi(ownerKey.c_str()+8) != ActiveSceneAnim) return NULL;
        return W3dCamTrack(ActiveSceneAnim, false);
    }
    if (ownerKey.compare(0,8,"objanim:")==0){          // animacion PROPIA del objeto (kind 3)
        extern VertexAnimation* AnimActiveObject(Object*);
        // la clave tiene que ser DE LA MALLA ACTIVA (ver DopeOwnerIsAnimMesh): esta rama era la
        // unica que resolvia derecho por AnimActiveObject(ActiveAnimMesh) sin mirar el dueno.
        if (DopeOwnerIsAnimMesh(ownerKey, 8)){
            VertexAnimation* an = AnimActiveObject((Object*)ActiveAnimMesh);
            if (an) for (size_t p=0;p<an->curves.size();p++)
                if (an->curves[p].Property==propId && an->curves[p].component==compId)
                    return &an->curves[p];
        }
        return NULL;
    }
    if (ownerKey.compare(0,4,"obj:")==0){
        // "obj:e<ESCENA>/#<serial objeto>": si la clave es de OTRA escena, no resuelve (ver el
        // bloque de identidad arriba). AnimationObjects ES la lista de la escena activa.
        // El objeto va por SERIAL: por nombre, borrar el objeto animado (o renombrarlo) y que
        // otro tome el nombre libre hacia que la clave vieja cayera sobre el objeto NUEVO.
        const std::string pref = DopePrefScene();
        if (ownerKey.size() <= pref.size() || ownerKey.compare(0, pref.size(), pref) != 0) return NULL;
        std::string n = ownerKey.substr(pref.size());
        for (size_t i=0;i<AnimationObjects.size();i++){
            if (!AnimationObjects[i].obj || DopeIdOwner(AnimationObjects[i].obj) != n) continue;
            for (size_t p=0;p<AnimationObjects[i].Properties.size();p++)
                if (AnimationObjects[i].Properties[p].Property==propId && AnimationObjects[i].Properties[p].component==compId)
                    return &AnimationObjects[i].Properties[p];
        }
    } else if (ownerKey.compare(0,6,"arm2d:")==0){
        // OJO: esta rama va ANTES que la de "arm:" - no por estilo: "arm2d:rig/a0/b3" NO empieza con
        // "arm:" (la 'm' choca con la ':'), pero dejarlo al azar del orden de los ifs es pedir el bug.
        // FORMA de la clave: "arm2d:<malla>/a<armature>/k<clip>/b<hueso>". La malla, el rig 2D y el
        // clip van ADENTRO de la clave: una clave de otro clip (o de otro rig, o de otra malla) NO
        // matchea el prefijo y devuelve NULL, en vez de editar la curva homonima del clip activo.
        Armature2DAnimation* clip = Clip2DActive(); if (!clip) return NULL;
        const std::string pref = DopePrefArm2D() + "/b";
        if (!DopeAfterPrefix(ownerKey, pref)) return NULL;
        int b = atoi(ownerKey.c_str()+pref.size());
        for (size_t t=0;t<clip->tracks.size();t++){
            if (clip->tracks[t].bone != b) continue;
            for (size_t p=0;p<clip->tracks[t].Properties.size();p++)
                if (clip->tracks[t].Properties[p].Property==propId && clip->tracks[t].Properties[p].component==compId)
                    return &clip->tracks[t].Properties[p];
        }
        return NULL;
    } else if (ownerKey.compare(0,4,"arm:")==0){
        // "arm:<rig>/k<CLIP>/b<HUESO>": el rig y el clip van adentro de la clave (idem 2D).
        // Si la curva que sale de aca se va a EDITAR y el clip es de un animset compartido, el
        // armature pasa antes a clips propios (ver 'editar' arriba).
        if (edit && ActiveClip()) W3dOwnArmatureAnims(ActiveAnimArm);
        SkeletalAnimation* clip = ActiveClip(); if (!clip) return NULL;
        const std::string pref = DopePrefArm() + "/b";
        if (!DopeAfterPrefix(ownerKey, pref)) return NULL;
        int b = atoi(ownerKey.c_str()+pref.size());
        for (size_t t=0;t<clip->tracks.size();t++){
            if (clip->tracks[t].bone != b) continue;
            for (size_t p=0;p<clip->tracks[t].Properties.size();p++)
                if (clip->tracks[t].Properties[p].Property==propId && clip->tracks[t].Properties[p].component==compId)
                    return &clip->tracks[t].Properties[p];
        }
    }
    return NULL;
}

static size_t DopeIconObj(Object* o){
    if (!o) return (size_t)IconType::empty;
    switch (o->getType()){
        case ObjectType::mesh:     return (size_t)IconType::mesh;
        case ObjectType::light:    return (size_t)IconType::light;
        case ObjectType::camera:   return (size_t)IconType::camera;
        case ObjectType::armature: return (size_t)IconType::armature;
        case ObjectType::curve:    return (size_t)IconType::curve;
        case ObjectType::sound:   return (size_t)IconType::sound;
        default:                   return (size_t)IconType::empty;
    }
}
// frames con keyframe de UNA CURVA (propiedad + componente). Cada componente tiene los SUYOS.
static void DopeCurveKeys(const std::vector<AnimProperty>& props, int prop, int comp, std::vector<int>& out){
    out.clear();
    for (size_t p=0;p<props.size();p++) if (props[p].Property==prop && props[p].component==comp)
        for (size_t k=0;k<props[p].keyframes.size();k++) out.push_back(props[p].keyframes[k].frame);
    std::sort(out.begin(), out.end()); out.erase(std::unique(out.begin(),out.end()), out.end());
}
// frames de TODAS las curvas de una propiedad (union X+Y+Z) -> lo que marca la fila padre
static void DopeKeysOfProp(const std::vector<AnimProperty>& props, int prop, std::vector<int>& out){
    out.clear();
    for (size_t p=0;p<props.size();p++) if (props[p].Property==prop)
        for (size_t k=0;k<props[p].keyframes.size();k++) out.push_back(props[p].keyframes[k].frame);
    std::sort(out.begin(), out.end()); out.erase(std::unique(out.begin(),out.end()), out.end());
}
static void DopeJoin(std::vector<int>& dst, const std::vector<int>& src){
    for (size_t i=0;i<src.size();i++) dst.push_back(src[i]);
    std::sort(dst.begin(), dst.end()); dst.erase(std::unique(dst.begin(),dst.end()), dst.end());
}
// UNION de los frames de las 3 propiedades (lo que marca la fila PADRE, este o no desplegada)
static std::vector<int> DopeUnionProps(const std::vector<AnimProperty>& props){
    const int propId[6] = { AnimPosition, AnimRotation, AnimScale, AnimVisible, AnimRender, AnimSound };
    std::vector<int> uni;
    for (int g=0;g<6;g++){ std::vector<int> ks; DopeKeysOfProp(props, propId[g], ks); DopeJoin(uni, ks); }
    return uni;
}
// agrega las filas de CANAL (X/Y/Z de Location / Euler Rotation / Scale). Cada canal es una CURVA propia y muestra
// SUS keyframes: X puede tener un rombo en el frame 4 y Y no. Solo se listan los canales que tienen keyframes.
static void DopeChannels(const std::vector<AnimProperty>& props, std::vector<Timeline::DopeRow>& out, int level, const std::string& ownerKey,
                        Object* obj = NULL){
    static const char* nP[3] = {"X Location","Y Location","Z Location"};
    static const char* nR[3] = {"X Euler Rotation","Y Euler Rotation","Z Euler Rotation"};
    static const char* nS[3] = {"X Scale","Y Scale","Z Scale"};
    const int propId[3] = { AnimPosition, AnimRotation, AnimScale };
    const char* const* names[3] = { nP, nR, nS };
    for (int g=0; g<3; g++){
        // TODO se muestra Z-ARRIBA, como en Properties (y en Lua): la "Y" de cada canal es la z interna y la "Z" la y.
        // (Hasta el 2026-11-11 solo la posicion: la rotacion y la escala iban con los ejes del motor, y "Y Escala" era
        // la altura.)
        const int compZUp[3] = { AnimX, AnimZ, AnimY };
        const int* compId = compZUp;
        for (int c=0;c<3;c++){
            std::vector<int> ks; DopeCurveKeys(props, propId[g], compId[c], ks); // los keyframes de ESTE canal
            if (ks.empty()) continue;                                            // canal sin animacion: no se lista
            Timeline::DopeRow r; r.type=3; r.level=level; r.name=T(names[g][c]); r.icon=-1; r.keys=ks;
            r.ownerKey = ownerKey; r.propId = propId[g]; r.compId = compId[c];
            char cb[24]; snprintf(cb,sizeof cb,"|p%d|c%d", propId[g], compId[c]);
            r.keyRow = ownerKey + cb;
            out.push_back(r);
        }
    }
    // canales de UNA sola componente (curvas 0/1): Visible y Renderizar
    const int propOne[2] = { AnimVisible, AnimRender };
    const char* nameOne[2] = { "Visible", "Renderable" };
    for (int u=0; u<2; u++){
        std::vector<int> ks; DopeCurveKeys(props, propOne[u], AnimX, ks);
        if (ks.empty()) continue;
        Timeline::DopeRow r; r.type=3; r.level=level; r.name=T(nameOne[u]); r.icon=-1; r.keys=ks;
        r.ownerKey = ownerKey; r.propId = propOne[u]; r.compId = AnimX;
        char cb[24]; snprintf(cb,sizeof cb,"|p%d|c0", propOne[u]);
        r.keyRow = ownerKey + cb;
        out.push_back(r);
    }
    // un SONIDO: volumen / tono / reverb (AnimSound X / Y / Z)
    const char* nameSound[3] = { "Volume", "Pitch", "Reverb" };
    for (int c = 0; c < 3; c++){
        std::vector<int> ks; DopeCurveKeys(props, AnimSound, AnimX + c, ks);
        if (ks.empty()) continue;
        Timeline::DopeRow r; r.type=3; r.level=level; r.name=T(nameSound[c]); r.icon=-1; r.keys=ks;
        r.ownerKey = ownerKey; r.propId = AnimSound; r.compId = AnimX + c;
        char cb[24]; snprintf(cb,sizeof cb,"|p%d|c%d", AnimSound, AnimX + c);
        r.keyRow = ownerKey + cb;
        out.push_back(r);
    }
    // los PASOS de una rutina (AnimStep): una fila por campo animado, con el nombre del paso ("Color · R", "Textura · on")
    std::vector<int> comps;
    for (size_t p = 0; p < props.size(); p++)
        if (props[p].Property == AnimStep && !props[p].keyframes.empty()) comps.push_back(props[p].component);
    std::sort(comps.begin(), comps.end());
    for (size_t i = 0; i < comps.size(); i++) {
        std::vector<int> ks; DopeCurveKeys(props, AnimStep, comps[i], ks);
        const int field = comps[i] % AnimStepFields;
        W3dStep* step = (obj && obj->getType() == ObjectType::routine) ? ((Routine*)obj)->StepById((unsigned)(comps[i] / AnimStepFields)) : NULL;
        std::string name = step ? std::string(T(W3dStepLabel(step->type))) : std::string("?");
        name += std::string(" \xC2\xB7 ") + (field == AnimStepOn ? std::string(T("on")) : step ? std::string(T(W3dStepNumberName(*step, field))) : std::string("?"));
        Timeline::DopeRow r; r.type=3; r.level=level; r.name=name; r.icon=-1; r.keys=ks;
        r.ownerKey = ownerKey; r.propId = AnimStep; r.compId = comps[i];
        char cb[32]; snprintf(cb,sizeof cb,"|p%d|c%d", AnimStep, comps[i]);
        r.keyRow = ownerKey + cb;
        out.push_back(r);
    }
}

// canales del ARMATURE 2D: los mismos (prop, comp) que el 3D pero con NOMBRES 2D y sin el
// componente Z (que en 2D no existe): U/V Location, Rotation, X/Y Scale.
static void DopeChannels2D(const std::vector<AnimProperty>& props, std::vector<Timeline::DopeRow>& out,
                          int level, const std::string& ownerKey){
    const int propId[5]  = { AnimPosition, AnimPosition, AnimRotation, AnimScale, AnimScale };
    const int compId[5]  = { AnimX,        AnimY,        AnimX,        AnimX,     AnimY     };
    const char* names[5] = { "U Location", "V Location", "Rotation", "X Scale", "Y Scale" };
    for (int c = 0; c < 5; c++){
        std::vector<int> ks; DopeCurveKeys(props, propId[c], compId[c], ks);
        if (ks.empty()) continue;                       // canal sin animacion: no se lista
        Timeline::DopeRow r; r.type=3; r.level=level; r.name=T(names[c]); r.icon=-1; r.keys=ks;
        r.ownerKey = ownerKey; r.propId = propId[c]; r.compId = compId[c];
        char cb[24]; snprintf(cb,sizeof cb,"|p%d|c%d", propId[c], compId[c]);
        r.keyRow = ownerKey + cb;
        out.push_back(r);
    }
}

// el nombre de un canal como lo lista DopeChannels ("X Location", con la Y y la Z como se ven: Z arriba), o "" si es
// uno que no tiene nombre ahi
static std::string DopeChannelName(int prop, int comp){
    // (por la componente INTERNA: la X es la X, la Y interna se ve como Z y la Z interna como Y)
    static const char* nP[3] = {"X Location","Z Location","Y Location"};
    static const char* nR[3] = {"X Euler Rotation","Z Euler Rotation","Y Euler Rotation"};
    static const char* nS[3] = {"X Scale","Z Scale","Y Scale"};
    if (comp < 0 || comp > 2) return std::string();
    if (prop == AnimPosition) return T(nP[comp]);
    if (prop == AnimRotation) return T(nR[comp]);
    if (prop == AnimScale)    return T(nS[comp]);
    if (prop == AnimVisible && comp == AnimX) return T("Visible");
    if (prop == AnimRender && comp == AnimX)  return T("Renderable");
    return std::string();
}

void Timeline::BuildDopeRows(){
    dopeRows.clear(); dopeVisible = 0; panelW = 0;
    rowH = RowHeightGS;
    // "JUEGO" (el estado base de un juego, ver W3dRoots.h): un juego NO tiene keyframes propios (el cabezal
    // lo lleva la simulacion); las curvas se ven eligiendo uno de sus CLIPS en el selector de animacion.
    if (ActiveAnimKind == 2) return;
    std::vector<DopeRow> rows;
    std::vector<DopeRow> ghosts;   // (los canales que no se listan: ver DopeRow::fantasma)
    SkeletalAnimation* clip = ActiveClip();
    if (clip){
        // ---- ARMATURE: SOLO en Pose Mode y SOLO los huesos SELECCIONADOS (sino es un caos) ----
        Armature* a = ActiveAnimArm;
        if (a && InteractionMode != PoseMode && a->select && mode == TL_MODE_DOPE){
            // ...y FUERA de Pose Mode, con el armature SELECCIONADO: UNA fila con los keyframes de TODOS sus huesos
            // (sin el detalle de cada uno). Son keyframes como cualquiera: se eligen, se arrastran, se borran y se
            // escalan, y lo que se hace en un frame lo hacen todos los huesos que tienen un keyframe ahi. Sus
            // canales van como filas FANTASMA (no se listan). En CURVAS no: ahi cada curva es de un hueso (Pose Mode)
            const std::string pref = DopePrefArm();
            DopeRow r; r.type=1; r.level=0; r.name=a->name; r.icon=(int)IconType::armature;
            r.keyRow = pref; r.ownerKey = pref; r.propId=-1; r.compId=-1;
            for (size_t t=0;t<clip->tracks.size();t++){
                const BoneTrack& tr = clip->tracks[t];
                char sb[24]; snprintf(sb,sizeof sb,"/b%d",tr.bone);
                for (size_t p=0;p<tr.Properties.size();p++){
                    const AnimProperty& ap = tr.Properties[p];
                    if (ap.keyframes.empty()) continue;
                    DopeRow c; c.type=3; c.level=1; c.icon=-1; c.ghost=true;
                    c.ownerKey = pref + sb; c.propId = ap.Property; c.compId = ap.component;
                    // (el nombre no se ve en el panel, pero lo usan la tarjeta Keyframe y la lista de curvas elegidas)
                    c.name = (tr.bone >= 0 && tr.bone < (int)a->bones.size()) ? a->bones[(size_t)tr.bone].name : std::string("?");
                    { const std::string cn = DopeChannelName(ap.Property, ap.component); if (!cn.empty()) c.name += " \xC2\xB7 " + cn; }
                    char cb[32]; snprintf(cb,sizeof cb,"|p%d|c%d", ap.Property, ap.component);
                    c.keyRow = c.ownerKey + cb;
                    for (size_t k=0;k<ap.keyframes.size();k++) c.keys.push_back(ap.keyframes.Frame(k));
                    std::sort(c.keys.begin(), c.keys.end()); c.keys.erase(std::unique(c.keys.begin(), c.keys.end()), c.keys.end());
                    DopeJoin(r.keys, c.keys);
                    ghosts.push_back(c);
                }
            }
            if (!r.keys.empty()) rows.push_back(r); else ghosts.clear();
        }
        if (a && InteractionMode == PoseMode){
            std::vector<int> bones;
            for (size_t b=0;b<a->bones.size();b++){
                if (!(a->bones[b].select || (int)b == a->activeBone)) continue;
                for (size_t t=0;t<clip->tracks.size();t++) if (clip->tracks[t].bone==(int)b){ bones.push_back((int)b); break; }
            }
            if (!bones.empty()){
                size_t iArm = rows.size();
                DopeRow r; r.type=1; r.level=0; r.name=a->name; r.icon=(int)IconType::armature;
                // la clave lleva el INDICE DEL CLIP adentro (DopePrefArm): la seleccion del dope
                // sobrevive a cambiar de clip, y sin esto la clave vieja matcheaba la fila del
                // MISMO hueso en el clip NUEVO -> se editaba la curva de un clip que el usuario
                // nunca eligio. Ver el bloque de identidad arriba.
                r.keyExpansion = DopePrefArm(); r.keyRow = r.keyExpansion; r.ownerKey = r.keyExpansion; r.propId=-1; r.compId=-1;
                rows.push_back(r);
                if (!DopeCollapsed(r.keyExpansion)){
                    for (size_t h=0; h<bones.size(); h++){
                        int b = bones[h];
                        const BoneTrack* tr = NULL;
                        for (size_t t=0;t<clip->tracks.size();t++) if (clip->tracks[t].bone==b){ tr=&clip->tracks[t]; break; }
                        if (!tr) continue;
                        std::vector<int> u = DopeUnionProps(tr->Properties);
                        char sb[24]; snprintf(sb,sizeof sb,"/b%d",b);
                        DopeRow rb; rb.type=2; rb.level=1; rb.name=a->bones[b].name; rb.icon=(int)IconType::armature;
                        rb.keyExpansion = DopePrefArm() + sb; rb.keyRow = rb.keyExpansion;
                        rb.ownerKey = rb.keyExpansion; rb.propId=-1; rb.compId=-1; rb.keys = u;
                        rows.push_back(rb);
                        if (!DopeCollapsed(rb.keyExpansion)) DopeChannels(tr->Properties, rows, 2, rb.ownerKey);
                        DopeJoin(rows[iArm].keys, u);
                    }
                } else { // colapsado: el armature igual marca la union de sus huesos seleccionados
                    for (size_t h=0; h<bones.size(); h++){ int b=bones[h];
                        for (size_t t=0;t<clip->tracks.size();t++) if (clip->tracks[t].bone==b){
                            std::vector<int> u = DopeUnionProps(clip->tracks[t].Properties); DopeJoin(rows[iArm].keys, u); break; } }
                }
            }
        }
    } else if (ActiveAnimKind == 4 && ActiveAnimMesh && Clip2DActive()) {
        // ---- CLIP DEL ARMATURE 2D (kind 4): una fila por la malla + una por HUESO 2D + sus canales.
        // Mismo criterio de filtro que el armature 3D: SOLO con el UV editor en modo POSE y SOLO los
        // huesos SELECCIONADOS (sin eso un rig de 30 huesos llena el panel de ruido).
        Armature2DAnimation* clip = Clip2DActive();
        Mesh* mm = ActiveAnimMesh;
        if (UVEditorInPoseMode()){
            std::vector<int> bones;
            for (size_t b=0;b<mm->Arm2DBones().size();b++){
                if (!(mm->Arm2DBones()[b].select || (int)b == mm->Arm2DActiveBone())) continue;
                for (size_t t=0;t<clip->tracks.size();t++) if (clip->tracks[t].bone==(int)b){ bones.push_back((int)b); break; }
            }
            if (!bones.empty()){
                size_t iArm = rows.size();
                // la clave lleva el INDICE del armature 2D y el DEL CLIP: una malla puede tener
                // varios rigs 2D y cada uno varios clips -> sin eso compartian el estado de
                // despliegue y, peor, el ownerKey de las curvas (la seleccion del dope terminaba
                // editando el clip de al lado). Todo eso lo arma DopePrefArm2D.
                const std::string prefA2 = DopePrefArm2D();
                const Armature2D* arm2 = mm->Arm2DActiveP();
                DopeRow r; r.type=1; r.level=0;
                r.name = mm->name + (arm2 ? (" (" + arm2->name + ")") : std::string());
                r.icon=(int)IconType::armature;
                r.keyExpansion = prefA2; r.keyRow = r.keyExpansion;
                r.ownerKey = r.keyExpansion; r.propId=-1; r.compId=-1;
                rows.push_back(r);
                for (size_t h=0; h<bones.size(); h++){
                    int b = bones[h];
                    const Bone2DTrack* tr = NULL;
                    for (size_t t=0;t<clip->tracks.size();t++) if (clip->tracks[t].bone==b){ tr=&clip->tracks[t]; break; }
                    if (!tr) continue;
                    std::vector<int> u = DopeUnionProps(tr->Properties);
                    DopeJoin(rows[iArm].keys, u);
                    if (DopeCollapsed(rows[iArm].keyExpansion)) continue; // colapsado: solo la union
                    char sb[24]; snprintf(sb,sizeof sb,"/b%d",b);
                    DopeRow rb; rb.type=2; rb.level=1; rb.name=mm->Arm2DBones()[b].name; rb.icon=(int)IconType::armature;
                    rb.keyExpansion = prefA2 + sb; rb.keyRow = rb.keyExpansion;
                    rb.ownerKey = rb.keyExpansion; rb.propId=-1; rb.compId=-1; rb.keys = u;
                    rows.push_back(rb);
                    if (!DopeCollapsed(rb.keyExpansion)) DopeChannels2D(tr->Properties, rows, 2, rb.ownerKey);
                }
            }
        }
    } else if (ActiveAnimKind == 3 && ActiveAnimMesh) {
        // ---- ANIMACION PROPIA del objeto (kind 3): sus curvas viven en el clip,
        // no en la escena. El dope muestra las curvas de la animacion ACTIVA. ----
        extern VertexAnimation* AnimActiveObject(Object*);
        VertexAnimation* an = AnimActiveObject((Object*)ActiveAnimMesh);
        Object* o = (Object*)ActiveAnimMesh;
        if (an){
            // frames de VERTICES, UV y NORMALES: TRES curvas separadas (cada frame
            // aparece en la fila de cada capa que trae). Con el toggle "Solo vertices
            // seleccionados" (y la malla en Edit Mode), cada fila muestra SOLO los
            // keyframes que le cambian algo a los vertices seleccionados.
            std::vector<unsigned char> selMask;
            const bool filter = g_dopeOnlySelVerts && an->vcount == ((Mesh*)o)->vertexSize &&
                                 DopeMaskVertsSel((Mesh*)o, selMask);
            std::vector<int> vtx, vuv, vnrm;
            for (size_t k=0;k<an->frames.size();k++){
                const VertexFrame* f = an->frames[k]; if (!f) continue;
                if (f->positions && (!filter || VertexFrameAnimatesSel(*an, f, 0, selMask))) vtx.push_back(f->frame);
                if (f->uvs       && (!filter || VertexFrameAnimatesSel(*an, f, 1, selMask))) vuv.push_back(f->frame);
                if (f->normals   && (!filter || VertexFrameAnimatesSel(*an, f, 2, selMask))) vnrm.push_back(f->frame);
            }
            std::sort(vtx.begin(),vtx.end()); vtx.erase(std::unique(vtx.begin(),vtx.end()),vtx.end());
            std::sort(vuv.begin(),vuv.end()); vuv.erase(std::unique(vuv.begin(),vuv.end()),vuv.end());
            std::sort(vnrm.begin(),vnrm.end()); vnrm.erase(std::unique(vnrm.begin(),vnrm.end()),vnrm.end());

            std::vector<int> u = DopeUnionProps(an->curves);
            DopeJoin(u, vtx);   // el objeto marca la union de curvas + vertices + uv + normales
            DopeJoin(u, vuv);
            DopeJoin(u, vnrm);
            if (!u.empty()){
                DopeRow r; r.type=1; r.level=0; r.name=o->name; r.icon=(int)DopeIconObj(o);
                r.keyExpansion = "objanim:" + DopeIdOwner(o); r.keyRow = r.keyExpansion;
                r.ownerKey = r.keyExpansion; r.propId=-1; r.compId=-1; r.keys = u;
                rows.push_back(r);
                if (!DopeCollapsed(r.keyExpansion)){
                    // filas VERTICES y UV: los keyframes de la malla, POR CAPA. Son canales
                    // seleccionables (propId=AnimVertex): click/seleccion/dibujo van por la
                    // maquinaria generica; mover/borrar/interpolar se rutean por el prefijo
                    // "objvtx:"/"objuv:" a los VertexFrame de SU capa (ver
                    // SetInterpolationSel/DopeDeleteSelection/move).
                    if (!vtx.empty()){
                        DopeRow v; v.type=2; v.level=1; v.name=T("Vertices"); v.icon=(int)IconType::mesh;
                        v.keyExpansion = "objvtx:" + DopeIdOwner(o); v.keyRow = v.keyExpansion;
                        v.ownerKey = v.keyExpansion; v.propId=AnimVertex; v.compId=0; v.keys = vtx;
                        rows.push_back(v);
                    }
                    if (!vuv.empty()){
                        DopeRow v; v.type=2; v.level=1; v.name=T("UV"); v.icon=(int)IconType::texture;
                        v.keyExpansion = "objuv:" + DopeIdOwner(o); v.keyRow = v.keyExpansion;
                        v.ownerKey = v.keyExpansion; v.propId=AnimVertex; v.compId=1; v.keys = vuv;
                        rows.push_back(v);
                    }
                    // fila NORMALES (capa propia, borrable): normalmente coincide con
                    // Vertices (insertar keyframe captura posiciones+normales juntas)
                    if (!vnrm.empty()){
                        DopeRow v; v.type=2; v.level=1; v.name=T("Normals"); v.icon=(int)IconType::normalVertex;
                        v.keyExpansion = "objnrm:" + DopeIdOwner(o); v.keyRow = v.keyExpansion;
                        v.ownerKey = v.keyExpansion; v.propId=AnimVertex; v.compId=2; v.keys = vnrm;
                        rows.push_back(v);
                    }
                    DopeChannels(an->curves, rows, 1, r.ownerKey, o);
                }
            }
        }
    } else {
        // ---- ESCENA: objetos SELECCIONADOS que tengan animacion ----
        // la clave lleva el INDICE DE LA ESCENA adentro (DopePrefScene): AnimationObjects es la
        // lista de la escena ACTIVA y cambiar de escena la swapea entera, asi que sin identidad la
        // seleccion vieja editaba las curvas homonimas de la escena nueva.
        const std::string prefEsc = DopePrefScene();
        for (size_t i=0;i<AnimationObjects.size();i++){
            AnimationObject& ao = AnimationObjects[i];
            if (!ao.obj || !ao.obj->select || ao.Properties.empty()) continue;
            std::vector<int> u = DopeUnionProps(ao.Properties);
            if (u.empty()) continue;
            DopeRow r; r.type=1; r.level=0; r.name=ao.obj->name; r.icon=(int)DopeIconObj(ao.obj);
            r.keyExpansion = prefEsc + DopeIdOwner(ao.obj); r.keyRow = r.keyExpansion;
            r.ownerKey = r.keyExpansion; r.propId=-1; r.compId=-1; r.keys = u;
            rows.push_back(r);
            if (!DopeCollapsed(r.keyExpansion)){
                DopeRow g; g.type=2; g.level=1; g.name=(ao.obj->getType()==ObjectType::sound) ? std::string(T("Sound")) : std::string(T("Object Transforms")); g.icon=-1; // SIN icono -> el texto va mas a la izquierda
                g.keyExpansion = prefEsc + DopeIdOwner(ao.obj) + "/xf"; g.keyRow = g.keyExpansion;
                g.ownerKey = r.ownerKey; g.propId=-1; g.compId=-1; g.keys = u;
                rows.push_back(g);
                if (!DopeCollapsed(g.keyExpansion)) DopeChannels(ao.Properties, rows, 2, r.ownerKey, ao.obj);
            }
        }
        // ---- LOS SONIDOS seleccionados: su fila lleva la TIRA (la del objeto si esta animado; si no, una propia) ----
        for (size_t i = 0; i < ObjSelects.size(); i++){
            Object* o = ObjSelects[i];
            if (!o || o->getType() != ObjectType::sound) continue;
            const std::string k = prefEsc + DopeIdOwner(o);
            bool has = false;
            for (size_t f = 0; f < rows.size(); f++) if (rows[f].type == 1 && rows[f].ownerKey == k){ rows[f].sound = o; has = true; }
            if (has) continue;
            DopeRow r; r.type=1; r.level=0; r.name=o->name; r.icon=(int)IconType::sound;
            r.keyRow = k; r.ownerKey = k; r.propId=-1; r.compId=-1; r.sound = o;
            rows.push_back(r);
        }
        // ...y con la forma de onda o el volumen a la vista, la tira es MAS ALTA: filas de continuacion debajo
        if (g_tlShowWave || g_tlShowVolume)
            for (size_t f = 0; f < rows.size(); f++){
                if (!rows[f].sound || rows[f].stripRows != 0) continue;
                rows[f].stripRows = kStripRowsExtra;
                for (int e = 0; e < kStripRowsExtra; e++){
                    DopeRow c; c.type=1; c.level=rows[f].level; c.icon=-1; c.sound=rows[f].sound; c.stripRows=-1;
                    c.ownerKey = rows[f].ownerKey;   // (el ojo del Summary / del objeto tambien la oculta)
                    rows.insert(rows.begin() + (long)f + 1, c);
                }
            }
        // ---- PISTA "CAMARA ACTIVA" (los cortes de la cinematica): aparece con una CAMARA seleccionada. Es una curva
        // mas (SceneAnimation::camTrack): mover / borrar / escalar y Ctrl+Z van por el camino de siempre
        // (DopeResolveProp la resuelve por el prefijo "camact:e<escena>"). ----
        bool camSel = false;
        for (size_t i = 0; i < ObjSelects.size() && !camSel; i++)
            if (ObjSelects[i] && ObjSelects[i]->getType() == ObjectType::camera) camSel = true;
        AnimProperty* cp = W3dCamTrack(ActiveSceneAnim, false);
        if (camSel && cp && !cp->keyframes.empty()){
            char b[32]; snprintf(b, sizeof b, "camact:e%d", ActiveSceneAnim);
            DopeRow r; r.type=1; r.level=0; r.name=T("Active camera"); r.icon=(int)IconType::camera;
            r.keyRow = b; r.ownerKey = b; r.propId = AnimCamActive; r.compId = AnimX;
            for (size_t k=0;k<cp->keyframes.size();k++) r.keys.push_back(cp->keyframes[k].frame);
            rows.push_back(r);
        }
    }
    if (rows.empty()) return; // nada seleccionado/animado -> panelW=0 -> timeline clasico, igual que antes

    // SUMMARY (arriba de todo): la UNION de los keyframes de TODO lo listado (= solo lo seleccionado).
    // Es DESPLEGABLE: plegado oculta TODAS las filas de abajo (queda solo el Summary).
    DopeRow s; s.type=0; s.level=0; s.name=T("Summary"); s.icon=-1;
    s.keyExpansion = "summary"; s.keyRow = "summary"; s.ownerKey = ""; s.propId = -1;
    for (size_t i=0;i<rows.size();i++) if (rows[i].type==1) DopeJoin(s.keys, rows[i].keys);
    dopeRows.push_back(s);
    if (!DopeCollapsed("summary"))
        for (size_t i=0;i<rows.size();i++) dopeRows.push_back(rows[i]);
    dopeVisible = dopeRows.size();
    for (size_t i=0;i<ghosts.size();i++) dopeRows.push_back(ghosts[i]);   // (al final: no corren a nadie)

    // OJO: cada fila esta oculta si le apagaron el suyo o el de alguno de sus PADRES (el padre de un canal es su
    // hueso/objeto, y el de todos es el Summary). Se resuelve aca, una vez: el resto del codigo solo lee 'oculto'.
    // (primero las que tienen el ojo apagado -casi nunca hay- y cada fila se compara solo con esas: comparar todas
    //  contra todas, con los cientos de canales de un armature, era mucho para cada cuadro)
    std::vector<size_t> offRows;
    for (size_t i=0;i<dopeRows.size();i++) if (DopeEyeOff(dopeRows[i].keyRow)) offRows.push_back(i);
    for (size_t i=0;i<dopeRows.size();i++){
        DopeRow& d = dopeRows[i];
        bool off = false;
        for (size_t j=0;j<offRows.size() && !off;j++){
            const DopeRow& p2 = dopeRows[offRows[j]];
            if (offRows[j] == i) off = true;
            else if (p2.type == 0) off = true;                  // el Summary cubre todo
            else if (p2.propId < 0 && DopeCovers(p2.ownerKey, d.ownerKey)) off = true;
        }
        d.hidden = off;
    }

    // ancho del panel = la fila mas larga. MISMAS distancias que el Outliner: solo se reserva el hueco de la
    // flecha si la fila es desplegable y el del icono si tiene icono (sino el texto se corre a la izquierda).
    int maxw = 0;
    for (size_t i=0;i<dopeVisible;i++){
        const DopeRow& d = dopeRows[i];
        int w = marginGS + d.level*(IconSizeGS+gapGS);
        if (!d.keyExpansion.empty()) w += IconSizeGS + gapGS;
        if (d.icon >= 0)               w += IconSizeGS + gapGS;
        w += UTF8_Length(d.name)*CharacterWidthGS + marginGS*2;
        w += IconSizeGS + gapGS;   // la columna de OJOS de la derecha
        if (w > maxw) maxw = w;
    }
    panelContW = maxw - (IconSizeGS + gapGS) - marginGS;   // lo que se scrollea: el texto, sin los ojos
    // ancho: el que fijo el usuario arrastrando el borde, o el automatico (fila mas larga)
    panelW = (panelWUser > 0) ? panelWUser : maxw;
    int limit = width/2; if (limit < IconSizeGS*4) limit = IconSizeGS*4;
    if (panelW > limit) panelW = limit;
    int floor = IconSizeGS*2; if (panelW < floor) panelW = floor;
    // SUMMARY PLEGADO -> se OCULTA el panel entero (panelW=0): el timeline pasa a ocupar TODO y el texto
    // "Summary" queda FLOTANDO encima (lo dibuja RenderDopePanel sin fondo).
    if (DopeCollapsed("summary") || !g_dopePanelOn) panelW = 0;
    // BARRA DE SCROLL: la del editor (Scrollable, la misma del Outliner) -> hover que la agranda, agarre verde,
    // arrastre y touch, todo por el ruteo generico. Solo en DOPE: en CURVAS el eje vertical es el VALOR, no una
    // lista de filas, asi que no hay nada que scrollear.
    // (la TOOLBAR de abajo tapa lo suyo: las ultimas filas tienen que poder subir por encima de ella)
    if (mode == TL_MODE_DOPE) ResizeScrollbar(width, height - TlToolbarHeight(this), 0, -DopeContentHeight(), stripY);
    else {   // CURVAS: sin barra (el eje vertical del strip es el VALOR), pero el PANEL igual se scrollea con el medio
        scrollY = scrollX = false;
        int maxY = (height - stripY - TlToolbarHeight(this)) - DopeContentHeight(); if (maxY > 0) maxY = 0;
        if (PosY < maxY) PosY = maxY; if (PosY > 0) PosY = 0;
    }
    if (panelPosX < PanelMaxX()) panelPosX = PanelMaxX(); if (panelPosX > 0) panelPosX = 0;
}

// ------------------------------------------------------------------ onChange -> timeline activo
static Timeline* g_tlActive = NULL;
// menu Select del dope sheet: mismas acciones que A / Alt+A / Ctrl+I
static void TL_menuSelAction(int id){
    if (g_tlActive){
        if      (id==0) g_tlActive->DopeSelectAll();
        else if (id==1) g_tlActive->DopeSelectNone();
        else if (id==2) g_tlActive->DopeSelectInvert();
    }
    TL_CloseMenu(g_tlMenuSel);
    g_redraw = true;
}
// menu View del dope sheet: encuadrar los keyframes seleccionados
enum { TL_VIEW_FRAMESEL = 0, TL_VIEW_AUTOFRAME = 1, TL_VIEW_PANEL = 2, TL_VIEW_SOLOSEL = 3, TL_VIEW_WAVE = 4, TL_VIEW_VOLUME = 5,
       TL_VIEW_DOPE = 6, TL_VIEW_CURVES = 7, TL_VIEW_HANDLES = 8, TL_VIEW_GIZMO = 9, TL_VIEW_SCRUB = 10 };
static void TL_menuViewAction(int id){
    // el CHECKBOX ya lo toggleo PopupMenu::Click, que a proposito NO cierra el menu (asi se prenden/apagan
    // varios de una). Cerrarlo aca romperia eso.
    if (id == TL_VIEW_AUTOFRAME){ g_redraw = true; return; }
    if (id == TL_VIEW_PANEL){ g_redraw = true; return; }   // el checkbox ya toco el bool; el ancho se recalcula solo
    if (id == TL_VIEW_SOLOSEL){ g_redraw = true; return; } // idem: las filas se re-arman solas con el filtro
    if (id == TL_VIEW_WAVE || id == TL_VIEW_VOLUME){ g_redraw = true; return; }
    if (id == TL_VIEW_HANDLES){ g_redraw = true; return; }   // (el checkbox ya toco g_tlShowHandles)
    if (id == TL_VIEW_GIZMO){ g_redraw = true; return; }     // (el checkbox ya toco g_gizmoOn)
    if (id == TL_VIEW_SCRUB){ g_redraw = true; return; }     // (el checkbox ya toco g_soundScrub)
    if (g_tlActive && id == TL_VIEW_FRAMESEL) g_tlActive->DopeFrameSelected();
    if (g_tlActive && (id == TL_VIEW_DOPE || id == TL_VIEW_CURVES)) g_tlActive->ChangeMode(id == TL_VIEW_CURVES);
    TL_CloseMenu(g_tlMenuView);
    g_redraw = true;
}
// menu Key: Transform (Move/Rotate/Scale) + Duplicate + Delete. El submenu HEREDA esta misma action (ver
// PopupMenu::Click), por eso los ids no se pisan entre el menu y su submenu.
enum { TL_KEY_DUP = 0, TL_KEY_OF = 1, TL_KEY_SMARTEULER = 2, TL_KEY_CUT = 3, TL_KEY_COPY = 4, TL_KEY_PASTE = 5,
       TL_KEY_UNDO = 90, TL_KEY_REDO = 91,
       TL_KEY_MOVE = 10, TL_KEY_ROT = 11, TL_KEY_SCALE = 12,
       TL_KEY_INTERP0 = 20,     // + KfConstant ... KfElastic
       TL_KEY_HANDLE0 = 40,     // + HFree / HAligned / HVector / HAuto / HAutoClamped / HPlane
       TL_KEY_EASE0 = 60 };     // + KfEaseAuto / KfEaseIn / KfEaseOut / KfEaseInOut
static void TL_menuKeyAction(int id){
    if (g_tlActive){
        switch (id){
            case TL_KEY_DUP:   g_tlActive->DopeDuplicateSelection(); break;
            case TL_KEY_CUT: g_tlActive->DopeCutSelection();  break;
            case TL_KEY_COPY: g_tlActive->DopeCopySelection();  break;
            case TL_KEY_PASTE:  g_tlActive->DopePaste();            break;
            case TL_KEY_OF:   g_tlActive->DopeDeleteSelection();   break;
            case TL_KEY_SMARTEULER: g_tlActive->SmartEulerSel();    break;
            case TL_KEY_MOVE:  g_tlActive->DopeMoveStart(Timeline::DOPE_MOV); break;
            case TL_KEY_ROT:   g_tlActive->DopeMoveStart(Timeline::DOPE_ROT); break;
            case TL_KEY_SCALE: g_tlActive->DopeMoveStart(Timeline::DOPE_ESC); break;
            default:
                if      (id >= TL_KEY_EASE0)   g_tlActive->SetEasingSel(id - TL_KEY_EASE0);
                else if (id >= TL_KEY_HANDLE0) g_tlActive->SetHandleTypeSel(id - TL_KEY_HANDLE0);
                else if (id >= TL_KEY_INTERP0) g_tlActive->SetInterpolationSel(id - TL_KEY_INTERP0);
                break;
        }
    }
    // el item pudo venir del menu Key o de cualquiera de sus submenus abierto SOLO por su atajo ('t' / 'v'):
    // se cierra el que este abierto, sea cual sea (TL_CloseMenu no hace nada con los otros).
    TL_CloseMenu(g_tlMenuKey); TL_CloseMenu(g_tlMenuKeyIn); TL_CloseMenu(g_tlMenuHandle);
    g_redraw = true;
}
// ATAJOS DE LAS OPERACIONES SIN TECLA DE FABRICA (app/W3dShortcuts.h): la misma accion que la opcion del menu, sobre el
// timeline que tiene el foco. "tlkey:<id>" = el menu Key; "tlvista:<id>" = el menu Vista (100/101: prender o apagar
// el panel / los handles, que en el menu son casillas)
static Timeline* TL_ShortcutTimeline(){
    return (viewPortActive && viewPortActive->isLeaf() && viewPortActive->ViewportKind() == 5) ? (Timeline*)viewPortActive : NULL;
}
static void TL_ShortcutKey(int id){ Timeline* t = TL_ShortcutTimeline(); if (!t) return; g_tlActive = t; TL_menuKeyAction(id); }
static void TL_ShortcutView(int id){
    Timeline* t = TL_ShortcutTimeline(); if (!t) return;
    g_tlActive = t;
    if (id == 100){ g_dopePanelOn = !g_dopePanelOn; g_redraw = true; return; }
    if (id == 101){ g_tlShowHandles = !g_tlShowHandles; g_redraw = true; return; }
    TL_menuViewAction(id);
}
static struct RegistryShortcutsTimeline {
    RegistryShortcutsTimeline(){ W3dShortcutRegisterDispatch("tlkey", TL_ShortcutKey); W3dShortcutRegisterDispatch("tlvista", TL_ShortcutView); }
} sRegistryShortcutsTimeline;
// menu Pivot del dope sheet: desde donde escala 'S' (centro de los keyframes seleccionados / frame actual)
static void TL_menuPivotAction(int id){
    if (g_tlActive) g_tlActive->DopeSetPivot(id);
    TL_CloseMenu(g_tlMenuPivot);
    g_redraw = true;
}
static void TL_onStart(){ if (g_tlActive) g_tlActive->ApplyStart(); }
static void TL_onEnd()  { if (g_tlActive) g_tlActive->ApplyEnd();   }
static void TL_onCur()  { if (g_tlActive) g_tlActive->ApplyCur();   }
static void TL_onFps()  { if (g_tlActive) g_tlActive->ApplyFps();   }

Timeline::Timeline(){
    pxPerFrame = (float)GlobalScale * 7.0f;
    viewStartF = -2.0f;                 // arranca mostrando un par de frames negativos (frame 0 prolijo)
    mode = TL_MODE_DOPE;
    pxPerUnit = (float)GlobalScale * 7.0f; // zoom vertical propio (curvas); INDEPENDIENTE del horizontal
    viewCenterV = 0.0f;                    // el CENTRO del strip es el CERO
    scrubbing = false; lastMx = lastMy = 0;
    stripY = numY = barH2 = 0; curBtnX = curBtnW = 0;
    panelW = 0; panelWUser = 0; panelResize = false; rowH = RowHeightGS; hoverRow = -1; dopeVisible = 0;
    panelFocus = -1;
    panelBorderHover = false; panelPosX = 0; panelContW = 0; panelBarH = false; panelPan = 0; // dope sheet (panelW=0 -> timeline clasico)
    fStart=(float)StartFrame; fEnd=(float)EndFrame; fCur=(float)CurrentFrame;

    pfStart = new PropFloat(T("Start")); pfStart->value=&fStart; pfStart->integer=true; pfStart->onChange=TL_onStart;
    pfEnd   = new PropFloat(T("End"));   pfEnd->value=&fEnd;     pfEnd->integer=true;   pfEnd->onChange=TL_onEnd;
    pfCur   = new PropFloat(T("Current Frame")); pfCur->value=&fCur;     pfCur->integer=true;   pfCur->onChange=TL_onCur;
    pfFps   = new PropFloat(T("FPS"));   pfFps->value=&fFpsTL;   pfFps->integer=true;   pfFps->onChange=TL_onFps;

    BarCreate(); // [0] = icono/menu de tipo
    // (el cartelito de AYUDA de cada uno: que hace, al dejar el mouse encima)
    static const char* const kHelpT[8] = { "Jump to Start", "Previous Keyframe", "Previous Frame", "Play Reverse",
                                            "Play", "Next Frame", "Next Keyframe", "Jump to End" };
    for (int i=0;i<8;i++){
        Button* b = new Button("");          // vacio + cuadrado: el glyph vectorial va encima
        b->role = TL_ROLE_T0 + i;
        b->square = true;                  // ancho = alto (con el padding de UIButtonHeight)
        b->help = kHelpT[i];
        BarButtons.push_back(b); btnT[i] = b;
    }
    // INICIO / FIN / FPS: un RELOJ que los abre en un desplegable (TimePopup). Los tres botones de texto se comian
    // la barra (en el N95 no entraban). Siguen existiendo (ocultos): EditField y la edicion por texto los usan.
    btnTime = new Button("", (int)IconType::clock); btnTime->role = TL_ROLE_TIME; btnTime->dropdown = true;
    btnTime->square = true; btnTime->help = "Start / End / FPS";
    BarButtons.push_back(btnTime);
    btnStart = new Button(std::string(T("Start")) + ":1");   btnStart->role = TL_ROLE_START; btnStart->visible = false; BarButtons.push_back(btnStart);
    btnEnd   = new Button(std::string(T("End")) + ":250");   btnEnd->role   = TL_ROLE_END;   btnEnd->visible = false;   BarButtons.push_back(btnEnd);
    btnFps   = new Button("FPS:30");                          btnFps->role   = TL_ROLE_FPS;   btnFps->visible = false;   BarButtons.push_back(btnFps);
    btnAnim  = new Button(T("Scene"), (int)IconType::camera); btnAnim->role = TL_ROLE_ANIM; btnAnim->dropdown = true;
    btnAnim->caretMenu = true; // aca SI conviene la flechita (dropdown de animacion)
    // SIEMPRE visible (Scene por defecto); SyncFields actualiza texto/icono segun la animacion activa
    BarButtons.push_back(btnAnim);
    // menu Select del dope sheet (All / None / Invert): al FINAL de la barra; solo visible si hay filas
    // AUTO KEY: toggle. Prendido = rojo (esta grabando). Siempre visible: es un modo, no depende de la seleccion.
    // (un PUNTO: rojo = grabando. El cartelito dice que es)
    btnAutoKey = new Button("", (int)IconType::record); btnAutoKey->role = TL_ROLE_AUTOKEY; btnAutoKey->square = true;
    btnAutoKey->help = "Auto Keyframe";
    BarButtons.push_back(btnAutoKey);
    // switch DOPE SHEET <-> CURVES: el texto dice a que modo se PASA (es un boton, no un dropdown)
    btnMode = new Button(T("Curves")); btnMode->role = TL_ROLE_MODE;
    btnMode->visible = false;
    BarButtons.push_back(btnMode);
    // (el MISMO icono que el Select de la vista 3D y del outliner: el cuadrado punteado)
    btnSelect = new Button("", IconType::selection); btnSelect->role = TL_ROLE_SELECT; btnSelect->dropdown = true; btnSelect->help = "Select";
    btnSelect->visible = false;
    BarButtons.push_back(btnSelect);
    // menu Key: Transform (Move/Rotate/Scale) + Duplicate + Delete
    // (solo el ROMBO de la key: el menu abierto lleva su titulo y el cartelito dice que es)
    btnKey = new Button("", IconType::keyframe); btnKey->role = TL_ROLE_KEY; btnKey->dropdown = true; btnKey->help = "Key";
    btnKey->visible = false;
    BarButtons.push_back(btnKey);
    // menu Modifiers: los modificadores de las curvas (Repetir antes / despues, con o sin offset)
    // menu View del dope sheet: encuadrar el timeline en los keyframes seleccionados
    btnView = new Button("", IconType::monitor); btnView->role = TL_ROLE_VIEW; btnView->dropdown = true; btnView->help = "View";
    btnView->visible = false;
    BarButtons.push_back(btnView);
    // menu Pivot del dope sheet (desde donde escala 's'): mismo criterio que el Pivot Point del viewport 3D
    // (un ICONO, como el Pivot Point del 3D: punto medio = centro, cursor 3D = frame actual)
    btnPivot = new Button("", (int)IconType::pivotMedian); btnPivot->role = TL_ROLE_PIVOT; btnPivot->dropdown = true; // solo el icono
    btnPivot->visible = false;
    BarButtons.push_back(btnPivot);
    btnSelMode = new Button("", (int)IconType::mouse); btnSelMode->role = TL_ROLE_SELMODE; btnSelMode->dropdown = true;
    btnSelMode->help = "Select with click and pencil"; btnSelMode->visible = false;
    BarButtons.push_back(btnSelMode);
    // ---- TOOLBAR de abajo (estilo Inkscape): solo iconos, con su cartelito; cada GRUPO arranca con una linea ----
    { struct B { static Button* New(std::vector<Button*>& v, int icon, int role, const char* help, bool sep){
          Button* b = new Button("", icon); b->role = role; b->square = true; b->centered = true; b->help = help;
          b->separator = sep; b->visible = false; v.push_back(b); return b; } };
      // SIEMPRE: deshacer / rehacer (las flechas curvas de siempre). Con el DEDO: el Shift (un toque SUMA a la
      // seleccion en vez de reemplazarla) y que hace el arrastre de un dedo: la flecha de SELECCION (la caja, agarrar
      // keyframes) o el MONITOR (mover la vista, el de fabrica). Solo uno de los dos esta prendido
      B::New(ToolButtons, IconType::undo, TL_TB_UNDO, "Undo", false);
      B::New(ToolButtons, IconType::redo,  TL_TB_REDO, "Redo", false);
      { Button* sh = B::New(ToolButtons, -1, TL_TB_SHIFT, "Shift", true); sh->text = "Shift"; sh->square = false; }
      B::New(ToolButtons, IconType::selection, TL_TB_SELEC, "Select", false);
      B::New(ToolButtons, IconType::monitor,   TL_TB_VIEW, "Move the view", false);
      // la TIJERA: corta el sonido elegido en el frame actual (queda en dos: el original y su copia)
      B::New(ToolButtons, IconType::scissors,    TL_TB_TRIM,  "Cut the sound at the current frame (trim)", true);
      B::New(ToolButtons, IconType::interp_constant, TL_TB_INTERP0 + KfConstant, "Constant", true);
      B::New(ToolButtons, IconType::interp_linear,   TL_TB_INTERP0 + KfLinear,   "Linear", false);
      B::New(ToolButtons, IconType::interp_bezier,   TL_TB_INTERP0 + KfBezier,   "Bezier", false);
      // las de EASING: por fuerza | con efecto (los mismos grupos que el menu de la 'E')
      B::New(ToolButtons, IconType::interp_sine,    TL_TB_INTERP0 + KfSine,    "Sinusoidal", true);
      B::New(ToolButtons, IconType::interp_quad,    TL_TB_INTERP0 + KfQuad,    "Quadratic", false);
      B::New(ToolButtons, IconType::interp_cubic,   TL_TB_INTERP0 + KfCubic,   "Cubic", false);
      B::New(ToolButtons, IconType::interp_quart,   TL_TB_INTERP0 + KfQuart,   "Quartic", false);
      B::New(ToolButtons, IconType::interp_quint,   TL_TB_INTERP0 + KfQuint,   "Quintic", false);
      B::New(ToolButtons, IconType::interp_expo,    TL_TB_INTERP0 + KfExpo,    "Exponential", false);
      B::New(ToolButtons, IconType::interp_circ,    TL_TB_INTERP0 + KfCirc,    "Circular", false);
      B::New(ToolButtons, IconType::interp_back,    TL_TB_INTERP0 + KfBack,    "Back (overshoot)", true);
      B::New(ToolButtons, IconType::interp_bounce,  TL_TB_INTERP0 + KfBounce,  "Bounce", false);
      B::New(ToolButtons, IconType::interp_elastic, TL_TB_INTERP0 + KfElastic, "Elastic", false);
      B::New(ToolButtons, IconType::handle_free,        TL_TB_HANDLE0 + HFree,        "Free", true);
      B::New(ToolButtons, IconType::handle_aligned,     TL_TB_HANDLE0 + HAligned,     "Aligned", false);
      B::New(ToolButtons, IconType::handle_vector,      TL_TB_HANDLE0 + HVector,      "Vector", false);
      B::New(ToolButtons, IconType::handle_auto,        TL_TB_HANDLE0 + HAuto,        "Automatic", false);
      B::New(ToolButtons, IconType::handle_autoclamped, TL_TB_HANDLE0 + HAutoClamped, "Auto Clamped", false);
      B::New(ToolButtons, IconType::handle_flat,        TL_TB_HANDLE0 + HPlane,       "Flat", false);
      B::New(ToolButtons, IconType::ease_auto,  TL_TB_EASE0 + KfEaseAuto,  "Automatic Easing", true);
      B::New(ToolButtons, IconType::ease_in,    TL_TB_EASE0 + KfEaseIn,    "Ease In", false);
      B::New(ToolButtons, IconType::ease_out,   TL_TB_EASE0 + KfEaseOut,   "Ease Out", false);
      B::New(ToolButtons, IconType::ease_inout, TL_TB_EASE0 + KfEaseInOut, "Ease In and Out", false); }
#ifdef W3D_SYMBIAN
    // N95 (240px): la barra completa (8 transportes + Start/End + anim) es demasiada para navegar sin mouse.
    // Dejamos SOLO lo esencial: start(0), play(4), final(7) + Start/End + dropdown de animacion. Los demas se
    // OCULTAN (no se borran) -> siguen existiendo para PC/Android, donde la barra entra entera.
    btnT[1]->visible = false; // kf anterior
    btnT[2]->visible = false; // frame anterior
    btnT[3]->visible = false; // play reversa
    btnT[5]->visible = false; // frame siguiente
    btnT[6]->visible = false; // kf siguiente
#endif
    g_tlActive = this;
}
Timeline::~Timeline(){
    // el cuerpo va al FINAL del archivo: necesita el estado del transform/handle, declarado mas abajo
    DestroyTimelineState();
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
    float ceiling = (float)GlobalScale*80.0f;
    return (px > ceiling) ? ceiling : px;
}
static float ClampZoomV(float px){
    if (px < 0.05f) return 0.05f;
    float ceiling = (float)GlobalScale*4000.0f;  // el valor puede ser chiquito (0.001): el techo vertical va mas alto
    return (px > ceiling) ? ceiling : px;
}
// Es un numero USABLE? (ni NaN ni infinito). Lo necesita ViewApply: la vista puede venir de un archivo y
// std::stof parsea "inf" y "nan" sin quejarse, asi que el try/catch del lector no los atrapa. Escrito a mano y
// no con std::isfinite: este .cpp lo compila tambien Symbian, que va en C++03.
// (3.4e38 = FLT_MAX redondeado para abajo: cualquier cosa mas grande ya es el infinito)
static bool W3dFinite(float v){ return (v == v) && (v > -3.4e38f) && (v < 3.4e38f); }
int   Timeline::CenterVertical() const { return (stripY + height) / 2; }
float Timeline::ValueToY(float v) const { return (float)CenterVertical() - (v - viewCenterV) * pxPerUnit; }
float Timeline::YToValue(float ly) const { return viewCenterV + ((float)CenterVertical() - ly) / (pxPerUnit>1e-6f?pxPerUnit:1e-6f); }
void  Timeline::PanValue(float dValue){ viewCenterV += dValue; g_redraw = true; }
void  Timeline::ZoomVBy(float factor){
    pxPerUnit = ClampZoomV(pxPerUnit * factor);       // desde el centro: viewCenterV no se toca
    g_redraw = true;
}
void  Timeline::ZoomVIn(float factor, float ly){
    const float v = YToValue(ly);                     // el valor que esta bajo los dedos...
    pxPerUnit = ClampZoomV(pxPerUnit * factor);
    viewCenterV = v - ((float)CenterVertical() - ly) / pxPerUnit;   // ...sigue ahi (invertir ValueToY)
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
    if (g_animMix){ StartFrame = (int)g_mixStart; EndFrame = (int)g_mixEnd; }
    // el rango/fps de la animacion activa YA vive en los globales StartFrame/EndFrame/AnimFPS (se cargan al
    // seleccionarla con AnimLoadActiveRange). Aca solo se refleja en los campos.
    // Start / End: si NO se editan, mostrar el valor; si se editan, el boton es el input (editField)
    if (g_propFloatEditing == pfStart){ btnStart->editField = &pfStart->field; }
    else { btnStart->editField = NULL; char b[24]; snprintf(b,sizeof b,"%s:%d",T("Start"),StartFrame); btnStart->text=b; fStart=(float)StartFrame; }
    if (g_propFloatEditing == pfEnd){ btnEnd->editField = &pfEnd->field; }
    else { btnEnd->editField = NULL; char b[24]; snprintf(b,sizeof b,"%s:%d",T("End"),EndFrame); btnEnd->text=b; fEnd=(float)EndFrame; }
    if (g_propFloatEditing == pfFps){ btnFps->editField = &pfFps->field; }
    else { btnFps->editField = NULL; char b[24]; snprintf(b,sizeof b,"FPS:%d",AnimFPS); btnFps->text=b; fFpsTL=(float)AnimFPS; }
    btnEnd->visible = false;          // (van en el reloj; MODO JUEGO: sin Fin, ver OpenTimePopup)
    if (g_propFloatEditing != pfCur) fCur=(float)CurrentFrame;

    // AUTO KEY prendido -> el boton queda ROJO (esta grabando). En "Juego" no hay keyframes que grabar.
    { static const float red[4] = { 0.95f, 0.18f, 0.18f, 1.0f };
      btnAutoKey->tint  = NULL; btnAutoKey->textColor = AutoKeyOn ? red : NULL; }
    btnAutoKey->visible = (ActiveAnimKind != 2);
    btnAutoKey->help = AutoKeyOn ? "Auto Keyframe: On" : "Auto Keyframe: Off";   // el cartelito dice el estado
    // play activo -> tinte verde en el boton correspondiente
    bool pf = (PlayAnimation && AnimPlayDir>0), pr = (PlayAnimation && AnimPlayDir<0);
    btnT[3]->tint = pr ? TL_GREEN_BTN : NULL;
    btnT[4]->tint = pf ? TL_GREEN_BTN : NULL;

    // dropdown de animacion: SIEMPRE visible (Scene por defecto). Muestra la animacion ACTIVA: icono camara = escena,
    // icono esqueleto = clip de armature. No depende del objeto seleccionado.
    btnAnim->visible = true;
    SkeletalAnimation* c = ActiveClip();
    if (g_animMix){ btnAnim->text = T("Mix"); btnAnim->icon = (int)IconType::equal; }   // capas mezcladas
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
        Armature2DAnimation* c2 = Arm2DActiveClip(ActiveAnimMesh);
        const Armature2D* a2 = ActiveAnimMesh->Arm2DActiveP();
        btnAnim->text = c2 ? c2->name : ActiveAnimMesh->name;
        if (a2 && ActiveAnimMesh->armatures2d.size() > 1) btnAnim->text += " (" + a2->name + ")";
        btnAnim->icon = (int)IconType::armature;
    }
    else if (ActiveAnimKind == 5){   // FLIPBOOK activo: su nombre
        extern Flipbook* g_flipActive;
        btnAnim->text = g_flipActive ? g_flipActive->name : std::string(T("Flipbook"));
        btnAnim->icon = (int)IconType::texture;
    }
    else if (c){ btnAnim->text = c->name; btnAnim->icon = (int)IconType::armature; }
    else  {
        // una animacion de escena, o un CLIP DE OBJETO ("Puerta: abrir", con el icono de objeto)
        InitSceneAnimations();
        btnAnim->text = W3dAnimSceneLabel(ActiveSceneAnim);
        btnAnim->icon = W3dAnimIsClip(ActiveSceneAnim) ? (int)IconType::object : (int)IconType::camera;
    }
    bool hasRows = !dopeRows.empty();
    btnMode->visible   = false;             // Dope Sheet / Curves: en el menu View (el monitor)
    btnMode->text      = T((mode == TL_MODE_CURVES) ? "Dope Sheet" : "Curves");  // dice a DONDE va
    btnSelect->visible = hasRows;          // menu Select: solo cuando el dope sheet tiene filas
    btnView->visible   = hasRows;          // menu View: idem (sin keyframes no hay nada que encuadrar)
    // el menu Key esta SIEMPRE: es un menu, no un boton de accion. Sus opciones actuan sobre lo que
    // haya seleccionado; que aparezca y desaparezca hace que la barra baile y no se sepa donde estan
    // las cosas.
    btnKey->visible    = true;
    btnPivot->visible  = hasRows;          // menu Pivot: idem (sin keyframes no hay nada que escalar)
    btnSelMode->visible = hasRows;         // el MOUSE: que se elige (sin keyframes no hay nada)
    btnPivot->icon = (g_dopePivot == DOPE_PIV_CURFRAME) ? (int)IconType::pivotCursor :
                     (g_dopePivot == DOPE_PIV_ACTIVE)   ? (int)IconType::pivotActive : (int)IconType::pivotMedian;
}

// ------------------------------------------------------------------ Render
void Timeline::Render(){
    gfx::MatrixMode(gfx::Projection); gfx::LoadIdentity();
    gfx::MatrixMode(gfx::ModelView);  gfx::LoadIdentity();

    const int glY = W3dScreenHeight - y - height;
    gfx::Enable(gfx::ScissorTest); gfx::Scissor(x, glY, width, height);
    // fondo NEGRO (el area fuera de Start..End queda negra; el rango se pinta gris tarjeta encima)
    gfx::ClearColor(ColorList[(int)ColorID::black][0], ColorList[(int)ColorID::black][1],
                    ColorList[(int)ColorID::black][2], 1.0f);
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

    BuildDopeRows(); // dope sheet: arma las filas de lo SELECCIONADO (panelW=0 -> timeline clasico)
    SyncFields();    // (despues de armar las filas: la visibilidad del boton Select depende de ellas)

    int step = TickStep();
    float fRight = viewStartF + width / (pxPerFrame>0.01f?pxPerFrame:0.01f);
    int f0 = ((int)floorf(viewStartF / step)) * step;
    int fN = (int)ceilf(fRight) + step;

    // ---------------- AREA REPRODUCIBLE [Start..End]: gris tarjeta (el resto queda NEGRO) ----------------
    { float xa = FrameToX((float)StartFrame);
      // MODO JUEGO: sin Fin -> el area reproducible sigue hasta el borde derecho
      float xb = AnimIsGame ? (float)width : FrameToX((float)EndFrame + 1);
      int ia = (int)(xa<panelW?panelW:xa), ib = (int)(xb>width?width:xb); // nunca invade el panel del dope sheet
      if (ib > ia){ SetCol(ColorID::gray, 1.0f); FillRect(ia, stripY, ib-ia, height-stripY); } }

    // ---------------- LINEAS verticales (van del piso hasta tocar la barra de numeros) ----------------
    // ALTERNAN color: las "oscuras" (mas oscuras que la tarjeta) llevan numero; las "claras" (color del texto) no.
    for (int f = f0; f <= fN; f += step){
        float fx = FrameToX((float)f);
        if (fx < panelW || fx > width) continue;
        bool dark = ((f / step) & 1) == 0;
        if (dark) gfx::Color4f(0.03f, 0.03f, 0.03f, 1.0f);          // linea oscura (mas oscuro que la tarjeta)
        else        SetCol(ColorID::grayLine, 1.0f);                // linea clara (gris medio 0x494949)
        FillRect((int)fx, stripY, GlobalScale, height - stripY);
    }

    // MODO CURVAS: en vez de los rombos por fila, el editor grafico (tiempo x valor).
    if (mode == TL_MODE_CURVES && !dopeRows.empty()) RenderCurves();
    // KEYFRAMES. Con dope sheet: un rombo por FILA, a la altura de su renglon (scrolleable en vertical).
    // Sin dope sheet (nada seleccionado/animado): la fila unica clasica abajo, igual que siempre.
    else if (!dopeRows.empty()){
        for (size_t r=0; r<dopeVisible; r++){
            int ry0 = stripY + (int)r*rowH + PosY;   // las filas arrancan DEBAJO de los numeros
            if (ry0 + rowH < stripY || ry0 > height) continue;      // fuera de vista
            float ry = (float)ry0 + rowH*0.5f;
            // fondo de la fila del SUMMARY (mas oscuro) para separarlo
            if (dopeRows[r].type == 0){ gfx::Color4f(0.10f,0.10f,0.10f,1.0f); FillRect(panelW, ry0, width-panelW, rowH); }
            // fila SELECCIONADA: el renglon se extiende por TODO el timeline (asi se ve que keyframes abarca)
            if (g_dopeRowSel.count(dopeRows[r].keyRow)){
                SetCol(ColorID::accentDark, 1.0f);
                int a0 = ry0 < numY ? numY : ry0, a1 = ry0 + rowH; if (a1 > height) a1 = height;
                if (a1 > a0) FillRect(panelW, a0, width - panelW, a1 - a0);
            }
            const DopeRow& dr = dopeRows[r];
            if (dr.hidden) continue;                      // ojo apagado: no se dibuja
            if (dr.sound && dr.stripRows >= 0) DrawStrip(dr, ry0);   // la TIRA del sonido (debajo de sus rombos)
            const std::vector<int>& ks = dr.keys;
            for (size_t i=0;i<ks.size();i++){
                float fx = FrameToX((float)ks[i]);
                if (fx < panelW - GlobalScale*3 || fx > width + GlobalScale*3) continue;
                // SELECCIONADO: naranja (accent) y un poco mas grande. Las filas padre/summary se marcan si
                // alguno de los canales que cubren tiene ese frame seleccionado.
                bool sel = false;
                if (dr.propId >= 0) sel = g_dopeKeySel.count(DopeKeyId(dr.ownerKey, dr.propId, dr.compId, ks[i])) > 0;
                else if (!g_dopeKeySel.empty())   // (sin nada elegido no hace falta recorrer los canales que cubre)
                    for (size_t c=0;c<dopeRows.size() && !sel;c++){ const DopeRow& cc=dopeRows[c]; if (cc.propId<0) continue;
                        if (!DopeCovers(dr.ownerKey, cc.ownerKey)) continue;
                        sel = g_dopeKeySel.count(DopeKeyId(cc.ownerKey, cc.propId, cc.compId, ks[i])) > 0; }
                if (sel)                   SetCol(ColorID::accent, 1.0f);
                else if (dr.type == 3)     SetCol(ColorID::grayUI, 1.0f); // canal: rombo mas apagado
                else                       SetCol(ColorID::white, 1.0f);  // summary / objeto / grupo
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
        if (g_tlPlayheadHover && !scrubbing) SetCol(ColorID::white, 1.0f); else gfx::Color4fv(TL_GREEN_LINE);
        const float areaWidth = (g_tlPlayheadHover || scrubbing) ? 2.5f : 1.5f;
        FillRect((int)(px - GlobalScale*areaWidth*0.4f), numY, (int)(GlobalScale*areaWidth), height - numY);
    }

    // ---------------- rect del boton VERDE del frame actual ----------------
    // Va DENTRO de la banda de los numeros: NO puede pisar el menu del viewport (antes arrancaba en
    // numY-GlobalScale*2 y se dibujaba al final, encima de la barra).
    std::string curTxt; { bool ec=(g_propFloatEditing==pfCur);
        if (ec) curTxt = pfCur->field.text; else { char b[16]; snprintf(b,sizeof b,"%d",CurrentFrame); curTxt=b; } }
    int curTw = UTF8_Length(curTxt)*CharacterWidthGS;
    curBtnW = curTw + GlobalScale*8; curBtnX = (int)px - curBtnW/2;
    int curBtnY = numY, curBtnH = barH2;

    // ---------------- FASE CON TEXTURA: numeros de frame (solo sobre lineas oscuras) ----------------
    gfx::Enable(gfx::Texture2D); gfx::Enable(gfx::Blend); gfx::BlendAlpha(); gfx::EnableArray(gfx::TexCoordArray);
    if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    int numTop = numY + (barH2 - LetterHeightGS)/2;
    SetCol(ColorID::grayUI, 1.0f);
    for (int f = f0; f <= fN; f += step){
        if (((f / step) & 1) != 0) continue;         // clara -> sin numero
        float fx = FrameToX((float)f);
        if (fx < (float)panelW || fx > width+GlobalScale*8) continue; // no invadir el panel del dope sheet
        if (fx > curBtnX - GlobalScale && fx < curBtnX + curBtnW + GlobalScale) continue; // bajo el boton verde: lo tapa
        char b[16]; snprintf(b,sizeof b,"%d",f);
        CenteredText(fx, numTop, b);
    }

    // ---------------- boton VERDE del frame actual: ANTES del panel, asi el "Summary" le queda ENCIMA ----------------
    // (la linea verde del playhead y su recuadro van DETRAS del dope sheet; sino tapaban el Summary)
    { static Card* cbtn = NULL; if (!cbtn) cbtn = new Card(NULL, 10, 10);
      cbtn->Resize(curBtnW, curBtnH);
      gfx::PushMatrix(); gfx::Translatef((float)curBtnX, (float)curBtnY, 0);
      gfx::Color4fv(TL_GREEN_LINE); cbtn->RenderObject(false); // relleno verde
      gfx::Color4fv(TL_GREEN_LINE); cbtn->RenderBorder(false); // borde verde redondeado (mismo color -> sin anillo negro)
      gfx::PopMatrix(); }
    SetCol(ColorID::white, 1.0f);
    CenteredText(px, numY + (barH2 - LetterHeightGS)/2, curTxt);

    // ---------------- PANEL del dope sheet: TAPA el playhead/recuadro (van detras) ----------------
    RenderDopePanel();
    // ---------------- la CAJA de seleccion (si la armo este timeline) ----------------
    if (g_tlBox == this && BoxSelectActive()){
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
        BoxSelectDraw(x, y, width, height);
        gfx::Enable(gfx::Texture2D); gfx::Enable(gfx::Blend); gfx::BlendAlpha();   // la caja los apaga: los iconos de la barra los necesitan
    }
    // ROTAR / ESCALAR keyframes: la linea punteada verde del PIVOTE al mouse, como en el 3D. El pivote de los
    // handles es su keyframe; en el dope sheet no hay eje de valor -> la linea va a la altura del mouse.
    if (g_dopeMov && mode == TL_MODE_CURVES && g_dopeMovAxis != DOPE_AXIS_FREE){
        // la LINEA DEL EJE al que esta limitado el transform (X = el tiempo, roja; Y = el valor, verde), de lado a
        // lado del strip y pasando por el pivote, como la linea del eje del 3D
        const float pf = g_dopeHXform ? (float)g_dopeHOrig.frame : g_dopePivotF;
        const float pv = g_dopeHXform ? g_dopeHOrig.value : g_dopePivotV;
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray); gfx::Enable(gfx::Blend); gfx::BlendAlpha();
        gfx::Enable(gfx::ScissorTest);
        gfx::Scissor(x + panelW, W3dScreenHeight - (y + height), width - panelW, height - stripY);
        if (g_dopeMovAxis == DOPE_AXIS_X){ gfx::Color4f(TL_COL_AXIS[0][0], TL_COL_AXIS[0][1], TL_COL_AXIS[0][2], 0.9f);
                                         const float yy = ValueToY(pv); FillRect(panelW, (int)yy, width - panelW, GlobalScale); }
        else                           { gfx::Color4f(TL_COL_AXIS[1][0], TL_COL_AXIS[1][1], TL_COL_AXIS[1][2], 0.9f);
                                         const float xx = FrameToX(pf); FillRect((int)xx, stripY, GlobalScale, height - stripY); }
        gfx::Disable(gfx::ScissorTest);
        gfx::Enable(gfx::Texture2D); gfx::EnableArray(gfx::TexCoordArray);
        if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    }
    if (g_dopeMov && (g_dopeMovMode == DOPE_ROT || g_dopeMovMode == DOPE_ESC)){
        extern void DrawDashedLine2D(float ax, float ay, float bx, float by);
        const float pf = g_dopeHXform ? (float)g_dopeHOrig.frame : g_dopePivotF;
        const float pv = g_dopeHXform ? g_dopeHOrig.value : g_dopePivotV;
        const float mxl = (float)(lastMx - x), myl = (float)(lastMy - y);
        DrawDashedLine2D(FrameToX(pf), mode == TL_MODE_CURVES ? ValueToY(pv) : myl, mxl, myl);
        gfx::Enable(gfx::Blend); gfx::BlendAlpha();
        if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    }

    // ---------------- el GIZMO 2D sobre los keyframes elegidos (Gizmo.h), recortado al strip ----------------
    gfx::Enable(gfx::ScissorTest);
    gfx::Scissor(x + panelW, W3dScreenHeight - (y + height), width - panelW, height - stripY);
    Gizmo2DRender(this);
    gfx::Disable(gfx::ScissorTest);
    gfx::Enable(gfx::Texture2D); gfx::Enable(gfx::Blend); gfx::BlendAlpha();

    // ---------------- barra del viewport ----------------
    // Durante un transform de keyframes ('g'/'s'/'r') la barra de menu se reemplaza por el readout (Move/Scale/
    // Rotate + eje) con RenderBarInfo, IGUAL que el 3D y el 2D. Sin transform: barra normal (transporte + campos).
    gfx::EnableArray(gfx::TexCoordArray);
    if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    std::string ttBar = DopeTextTransform();
    if (!ttBar.empty()) {
        RenderBarInfo(ttBar);
    } else {
        RenderBar();

        // glyphs vectoriales ENCIMA de los botones de transporte (sin textura, centrados en el boton cuadrado)
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray);
        bool pfwd = (PlayAnimation && AnimPlayDir>0), prev = (PlayAnimation && AnimPlayDir<0);
        SetCol(ColorID::white, 1.0f);
        for (int i=0;i<8;i++){
            Button* b = btnT[i]; if (!b->visible || b->sx < -9000) continue;
            int gx = b->sx - x, gy = b->sy - y;
            bool pause = (i==3 && prev) || (i==4 && pfwd);
            DrawGlyph(gx, gy, b->width, b->height, i, pause);
        }
    }

    // la TOOLBAR de abajo (interpolacion | handles | ease), encima del strip
    gfx::Enable(gfx::Texture2D); gfx::EnableArray(gfx::TexCoordArray); gfx::Enable(gfx::Blend); gfx::BlendAlpha();
    if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    RenderToolbar();
    // borde del viewport (verde si activo)
    gfx::Enable(gfx::Texture2D); gfx::EnableArray(gfx::TexCoordArray);
    if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
    DrawBorders(this);
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
static bool g_hOutput = true;       // true = handle de SALIDA (a la derecha); false = de ENTRADA
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
static bool g_hVirtFirst = false; // el 1er motion: dx/dy son de ANTES de agarrar el handle -> se ignoran

bool Timeline::HandleDragging() const { return g_hOn; }
bool Timeline::HandleIsOutput() const { return g_hOutput; }
AnimProperty* Timeline::RowCurve(const DopeRow& d, bool edit) const {
    if (d.propId < 0) return NULL;                       // fila padre / summary: no es una curva
    return DopeResolveProp(d.ownerKey, d.propId, d.compId, edit);
}

// Posicion del handle del keyframe i, en coords LOCALES del timeline. El handle es un PUNTO (offset dF/dV desde
// el keyframe): quien decide donde cae es EffectiveHandle del core, o sea el MISMO que usa la evaluacion -> el
// handle que agarras es exactamente el que curva la animacion.
void Timeline::HandlePos(const AnimProperty* ap, size_t i, bool output, float& hx, float& hy) const {
    float dF, dV;
    ap->EffectiveHandle(i, output, dF, dV);
    const keyFrame& kf = ap->keyframes[i];
    hx = FrameToX((float)kf.frame + dF);
    hy = ValueToY(kf.value + dV);
    // Piso EN PIXELES para poder agarrarlo: con keyframes pegados el brazo mide unos px y los dos handles se
    // superponen. Solo corre el DIBUJO a lo largo de su propia direccion; el dato no se toca.
    float kx = FrameToX((float)kf.frame), ky = ValueToY(kf.value);
    float ex = hx - kx, ey = hy - ky, L = sqrtf(ex*ex + ey*ey);
    const float minPx = (float)(GlobalScale*14);
    if (L < 0.001f){ hx = kx + (output ? minPx : -minPx); hy = ky; return; }
    if (L < minPx){ hx = kx + ex/L*minPx; hy = ky + ey/L*minPx; }
}


// Hay handle de este lado? SOLO si el tramo de ese lado es BEZIER: un tramo LINEAL (o constante) es una recta y no
// tiene nada que ajustar, asi que no se le dibuja handle. Para curvar se usa el menu Interpolation > Bezier.
// Cada handle pertenece a SU tramo, y el tramo lo manda el keyframe IZQUIERDO:
//   handle de SALIDA de K  -> tramo [K, K+1]   -> manda K
//   handle de ENTRADA de K -> tramo [K-1, K]   -> manda K-1
static bool HandleVisible(const AnimProperty* ap, size_t i, bool output){
    if (!g_tlShowHandles) return false;   // Vista > Mostrar handles apagado
    const std::vector<keyFrame>& k = ap->keyframes;
    if (output) return (i+1 < k.size()) && k[i].Interpolation == KfBezier;
    return (i > 0) && k[i-1].Interpolation == KfBezier;
}

// Una fila aporta una CURVA al dibujo? (es un canal Y tiene el ojo prendido). La comparten el render y el
// contador de costo: cuando cada uno filtraba por su lado, agregar el ojo a uno solo hizo que el contador midiera
// curvas que el render ya no dibujaba.
static bool RowIsCurveVisible(const Timeline::DopeRow& d){ return d.propId >= 0 && !d.hidden && !d.ghost; }

// Arma el TRAZO de UNA curva en g_curveBuf (coords locales). Devuelve la cantidad de VERTICES.
// Recorre por TRAMOS, no por pixel: un tramo lineal o constante queda definido por sus extremos y no necesita ni
// una muestra; solo el bezier se subdivide, y solo el pedazo que se ve.
int Timeline::CurveStroke(const AnimProperty* ap, float w){
    g_curveBuf.clear();
    if (!ap || ap->keyframes.empty()) return 0;
    const std::vector<keyFrame>& K = ap->keyframes;
    const size_t n = K.size();
    const int x0 = panelW, x1 = width;
    const float fVisL = XToFrame((float)x0), fVisR = XToFrame((float)x1); // rango de frames VISIBLE
    const float yTop = (float)stripY, yBot = (float)height;

    // --- con MODIFICADORES DE CURVA prendidos (Animation.h): lo que corre ya no son los tramos entre keyframes sino
    //     la curva modificada (ruido, escalones, ciclos...). Se muestrea EvalF cada ~2 px en toda la vista: es
    //     exactamente lo que va a correr (los keyframes quedan donde estan, como en Blender) ---
    { bool hasMods = false;
      for (size_t m = 0; m < ap->mods.size() && !hasMods; m++) hasMods = ap->mods[m].active;
      if (hasMods){
          const float step = (pxPerFrame > 0.01f) ? (2.0f * (float)GlobalScale) / pxPerFrame : 1.0f;
          float px_ = (float)x0, py_ = ValueToY(ap->EvalF(fVisL, 0.0f));
          for (float f = fVisL + step; ; f += step){
              if (f > fVisR) f = fVisR;
              const float qx = FrameToX(f), qy = ValueToY(ap->EvalF(f, 0.0f));
              if (!((py_ < yTop && qy < yTop) || (py_ > yBot && qy > yBot))) QuadLine(g_curveBuf, px_, py_, qx, qy, w);
              px_ = qx; py_ = qy;
              if (f >= fVisR) break;
          }
          return (int)(g_curveBuf.size()/2);
      } }

    // --- cola IZQUIERDA: antes del 1er keyframe la curva vale constante (Eval clampea)... salvo con el modificador
    //     REPETIR: ahi se muestrea EvalF (vuelta tras vuelta, cada ~3 px) hasta el borde de la vista ---
    const float stepCycle = (pxPerFrame > 0.01f) ? (3.0f * (float)GlobalScale) / pxPerFrame : 1.0f;
    if (fVisL < (float)K[0].frame && ap->cyclePrev != KfCycleNo && n > 1){
        float px_ = FrameToX((float)K[0].frame), py_ = ValueToY(K[0].value);
        for (float f = (float)K[0].frame - stepCycle; ; f -= stepCycle){
            if (f < fVisL) f = fVisL;
            const float qx = FrameToX(f), qy = ValueToY(ap->EvalF(f, 0.0f));
            QuadLine(g_curveBuf, qx, qy, px_, py_, w);
            px_ = qx; py_ = qy;
            if (f <= fVisL) break;
        }
    } else if (fVisL < (float)K[0].frame){
        float yv = ValueToY(K[0].value);
        if (yv >= yTop-1.0f && yv <= yBot+1.0f)      // horizontal: se ve o no, no hay caso intermedio
            QuadLine(g_curveBuf, (float)x0, yv, FrameToX((float)K[0].frame), yv, w);
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
            ap->EffectiveHandle(i-1, true,  aDF, aDV);
            ap->EffectiveHandle(i,   false, bDF, bDV);
            lo = a.value; hi = a.value;
            const float cs[3] = { a.value + aDV, b.value + bDV, b.value };   // los otros 3 puntos de control
            for (int c=0;c<3;c++){ if (cs[c]<lo) lo=cs[c]; if (cs[c]>hi) hi=cs[c]; }
        } else { lo = (a.value<b.value)?a.value:b.value; hi = (a.value>b.value)?a.value:b.value; }
        // easing con efecto (Back / Elastic) se pasa de los dos valores: margen para no recortarlo
        if (KfIsEasing(a.Interpolation)){ float m = (hi - lo) * 0.5f; lo -= m; hi += m; }
        // el rango [lo..hi] mapea a Y invertido: hi va arriba
        if (ValueToY(hi) > yBot || ValueToY(lo) < yTop) continue;         // todo el tramo queda fuera: no se dibuja
        float ax = FrameToX((float)a.frame), bx = FrameToX((float)b.frame);
        float ay = ValueToY(a.value), by = ValueToY(b.value);
        if (a.Interpolation == KfConstant){
            QuadLine(g_curveBuf, ax, ay, bx, ay, w);                    // meseta: se mantiene...
            QuadLine(g_curveBuf, bx, ay, bx, by, w);                    // ...y PUM, cambia justo en el frame de b
        } else if (a.Interpolation != KfBezier && !KfIsEasing(a.Interpolation)){
            QuadLine(g_curveBuf, ax, ay, bx, by, w);                    // recta: con los extremos alcanza
        } else if (KfIsEasing(a.Interpolation)){
            // EASING: muestreado cada ~3 px (no por frame): un rebote o un resorte entre dos keyframes cercanos
            // tiene mas detalle que un frame, y la forma tiene que verse para saber que curva es
            const float stepF = (pxPerFrame > 0.01f) ? (3.0f * (float)GlobalScale) / pxPerFrame : 1.0f;
            float px_ = ax, py_ = ay;
            for (float f = (float)a.frame + stepF; f < (float)b.frame; f += stepF){
                const float t = (f - (float)a.frame) / (float)span;
                float qx = FrameToX(f), qy = ValueToY(a.value + (b.value - a.value) * KfEasing(a.Interpolation, t));
                QuadLine(g_curveBuf, px_, py_, qx, qy, w);
                px_ = qx; py_ = qy;
            }
            QuadLine(g_curveBuf, px_, py_, bx, by, w);
        } else {
            // BEZIER: se muestrea FRAME A FRAME y se unen con rectas. No hace falta mas resolucion que esa: la
            // animacion solo pisa frames enteros, asi que esta linea ES literalmente lo que va a correr.
            // (Si el zoom es tan chico que un frame no llega a un pixel, se saltean frames: mas no se veria.)
            int step = 1;
            if (pxPerFrame < 1.0f){ step = (int)(1.0f/pxPerFrame); if (step < 1) step = 1; }
            float px_ = ax, py_ = ay;
            for (int f = a.frame + step; f < b.frame; f += step){
                float qx = FrameToX((float)f), qy = ValueToY(ap->EvalBezier(i, (float)f));
                QuadLine(g_curveBuf, px_, py_, qx, qy, w);
                px_ = qx; py_ = qy;
            }
            QuadLine(g_curveBuf, px_, py_, bx, by, w);                  // ultimo tramo hasta el keyframe
        }
    }
    // --- cola DERECHA: despues del ultimo keyframe vuelve a ser constante (salvo REPETIR, como la izquierda) ---
    if (fVisR > (float)K[n-1].frame && ap->cycleAfter != KfCycleNo && n > 1){
        float px_ = FrameToX((float)K[n-1].frame), py_ = ValueToY(K[n-1].value);
        for (float f = (float)K[n-1].frame + stepCycle; ; f += stepCycle){
            if (f > fVisR) f = fVisR;
            const float qx = FrameToX(f), qy = ValueToY(ap->EvalF(f, 0.0f));
            QuadLine(g_curveBuf, px_, py_, qx, qy, w);
            px_ = qx; py_ = qy;
            if (f >= fVisR) break;
        }
    } else if (fVisR > (float)K[n-1].frame){
        float yv = ValueToY(K[n-1].value);
        if (yv >= yTop-1.0f && yv <= yBot+1.0f)
            QuadLine(g_curveBuf, FrameToX((float)K[n-1].frame), yv, (float)x1, yv, w);
    }
    return (int)(g_curveBuf.size()/2);
}
// suma de vertices del trazo de TODAS las curvas, sin dibujar: mide el trabajo por frame (lo usa 'curvaperf')
long long Timeline::CurveStrokeCost(){
    long long t = 0;
    for (size_t r=0; r<dopeRows.size(); r++){
        const DopeRow& d = dopeRows[r]; if (!RowIsCurveVisible(d)) continue;
        AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId, false); if (!ap) continue;   // solo mide
        t += CurveStroke(ap, (float)GlobalScale*0.5f);
    }
    return t;
}

static void SegmentPoints(const Timeline* t, const AnimProperty* ap, size_t i, std::vector<float>& xy);   // (abajo)
void Timeline::RenderCurves(){
    gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray); gfx::Disable(gfx::Blend);
    const int x0 = panelW, x1 = width;
    if (x1 <= x0) return;

    // ---- linea del CERO (el centro) + rejilla de valores ----
    { int stepPx = GlobalScale*22;                      // separacion minima entre lineas de valor
      // paso "redondo" en unidades de valor segun el zoom vertical
      const float steps[] = {0.01f,0.02f,0.05f,0.1f,0.2f,0.5f,1.0f,2.0f,5.0f,10.0f,25.0f,50.0f,100.0f,250.0f,500.0f,1000.0f};
      float step = 1000.0f;
      for (int i=0;i<16;i++) if (steps[i]*pxPerUnit >= (float)stepPx){ step = steps[i]; break; }
      float vTop = YToValue((float)stripY), vBot = YToValue((float)height);
      float v0 = floorf(vBot/step)*step;
      for (float v = v0; v <= vTop + step*0.5f; v += step){
          float ly = ValueToY(v);
          if (ly < stripY || ly > height) continue;
          bool zero = (v > -step*0.001f && v < step*0.001f);
          if (zero) SetCol(ColorID::grayLine, 1.0f);      // el CERO va marcado (es el centro)
          else      gfx::Color4f(0.16f,0.16f,0.16f,1.0f);
          FillRect(x0, (int)ly, x1-x0, zero ? GlobalScale : (GlobalScale>1?GlobalScale/2:1));
      } }

    // ---- una CURVA por canal, con el color de SU EJE (X rojo / Y verde / Z azul) ----
    // Se recorre por TRAMOS (entre keyframe y keyframe), NO pixel por pixel: un tramo lineal o constante queda
    // definido por sus extremos y no necesita ni una muestra; solo el bezier se subdivide. Todo el trazo se arma en
    // UN vertex array y se dibuja de UNA. (Antes era un FillRect y un EvalF -que escanea los keyframes- por cada
    // pixel de cada curva: miles de draw calls por frame -> el rendimiento se caia a pedazos.)
    // El recorte lo hace el SCISSOR: los tramos que sobran quedan fuera y GL no los rasteriza.
    { int lh = height - stripY; if (lh < 0) lh = 0;
      gfx::Enable(gfx::ScissorTest);
      gfx::Scissor(x + x0, W3dScreenHeight - y - height, x1 - x0, lh); }
    const float fVisL = XToFrame((float)x0), fVisR = XToFrame((float)x1); // rango de frames VISIBLE
    const float yTop = (float)stripY, yBot = (float)height;
    static AnimProperty s_vtxTmp;   // buffer reusado para la curva temporal de vertices
    for (size_t r=0; r<dopeRows.size(); r++){
        const DopeRow& d = dopeRows[r];
        if (!RowIsCurveVisible(d)) continue;             // solo canales, y solo con el ojo prendido
        // canal VERTEX: su "curva" es la rampa por indice (ver VertexChannelTemp). Dibujar no edita.
        AnimProperty* ap = VertexChannelTemp(d.ownerKey, s_vtxTmp) ? &s_vtxTmp
                         : DopeResolveProp(d.ownerKey, d.propId, d.compId, false);
        if (!ap || ap->keyframes.empty()) continue;
        const std::vector<keyFrame>& K = ap->keyframes;
        const size_t n = K.size();
        // color: el del EJE (X rojo / Y verde / Z azul), salvo la fila Normales de la vertex anim (violeta)
        // (el color es el del eje que se VE: en la posicion, pos.z es la "Y" verde y pos.y la "Z" azul)
        const int seenAxis = (d.propId == AnimPosition && d.compId == AnimY) ? AnimZ
                           : (d.propId == AnimPosition && d.compId == AnimZ) ? AnimY : d.compId;
        const float* col = (DopeVertexLayerOf(d.ownerKey)==2) ? TL_COL_NORMALS : AxisColor(seenAxis);
        // Una curva SIN ningun keyframe seleccionado se dibuja al 50%: las que estas editando resaltan y el resto
        // queda de fondo (con 18 canales encimados, si van todas al 100% no se entiende nada).
        bool hasSel = false;
        for (size_t i=0;i<n && !hasSel;i++)
            hasSel = g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, K[i].frame)) > 0;
        const float alpha = hasSel ? 1.0f : 0.5f;
        float w = (hasSel ? (float)GlobalScale : (float)GlobalScale*0.6f) * 0.5f; // media-anchura del trazo
        CurveStroke(ap, w);                       // arma el trazo en g_curveBuf (con su culling)
        // UNA sola llamada de dibujo para toda la curva
        if (!g_curveBuf.empty()){
            gfx::Enable(gfx::Blend); gfx::BlendAlpha();   // el 50% necesita alpha
            gfx::Color4f(col[0], col[1], col[2], alpha);
            gfx::VertexPointer2f(0, &g_curveBuf[0]);
            gfx::DrawTrianglesArray((int)(g_curveBuf.size()/2));
        }
        // ---- TRAMOS ELEGIDOS (los dos keyframes de la punta seleccionados: click en la linea, caja) en BLANCO ----
        if (hasSel && n > 1){
            static std::vector<float> s_segment, s_quad;
            s_quad.clear();
            const float wb = (float)GlobalScale * 0.6f;
            for (size_t i=1;i<n;i++){
                if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, K[i-1].frame)) ||
                    !g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, K[i].frame))) continue;
                const float xa = FrameToX((float)K[i-1].frame), xb = FrameToX((float)K[i].frame);
                if (xb < x0 || xa > x1) continue;                    // fuera de pantalla
                SegmentPoints(this, ap, i, s_segment);
                for (size_t k=2; k+1<s_segment.size(); k+=2){
                    const float ax = s_segment[k-2], ay = s_segment[k-1], bx = s_segment[k], by = s_segment[k+1];
                    float nx = -(by - ay), ny = bx - ax; const float l = sqrtf(nx*nx + ny*ny);
                    if (l < 1e-4f) continue;
                    nx = nx / l * wb; ny = ny / l * wb;
                    const float q[12] = { ax+nx,ay+ny, bx+nx,by+ny, bx-nx,by-ny,  ax+nx,ay+ny, bx-nx,by-ny, ax-nx,ay-ny };
                    s_quad.insert(s_quad.end(), q, q + 12);
                }
            }
            if (!s_quad.empty()){
                SetCol(ColorID::white, 1.0f);
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
            if (sel) SetCol(ColorID::accent, 1.0f); else gfx::Color4f(col[0], col[1], col[2], alpha);
            Diamond(kx, ky, (float)GlobalScale*(sel?3.8f:2.8f));
            if (!sel) continue;
            // ---- HANDLES: solo del keyframe SELECCIONADO, y solo del lado cuyo tramo es BEZIER (una recta no
            //      tiene nada que ajustar). El canal VERTEX tambien: sus handles viven en el VertexFrame. ----
            for (int side=0; side<2; side++){
                bool output = (side==1);
                if (!HandleVisible(ap, i, output)) continue;
                float hx, hy; HandlePos(ap, i, output, hx, hy);
                gfx::Color4f(col[0], col[1], col[2], 1.0f);
                ThinLine(kx, ky, hx, hy);                     // brazo
                SetCol(ColorID::white, 1.0f);
                Diamond(hx, hy, (float)GlobalScale*2.2f);      // agarradera
            }
        }
    }
    gfx::Disable(gfx::ScissorTest);
}

// ------------------------------------------------------------------ DOPE SHEET: panel (nombres/flechas/iconos)
// Rect del "Summary" FLOTANTE (cuando el panel esta oculto). Lo comparten el RENDER y el CLICK: asi no se
// pueden desincronizar (era el bug: se dibujaba en un lado y el click se testeaba en otro).
bool Timeline::DopeRectFloating(int& rx, int& ry, int& rw, int& rh) const {
    if (dopeRows.empty() || panelW > 0) return false;
    rx = 0; ry = stripY;   // DEBAJO de los numeros (como todas las filas)
    rw = marginGS + IconSizeGS + gapGS + UTF8_Length(dopeRows[0].name)*CharacterWidthGS + marginGS;
    rh = rowH;
    return true;
}

// X (local) de la columna de OJOS: pegada al borde derecho del panel, igual que en el Outliner.
int Timeline::DopeEyeX() const { return panelW - GlobalScale - IconSizeGS - marginGS; }

// SCROLL HORIZONTAL del panel: el texto visible va de 0 a la columna de ojos; lo que sobra se scrollea
int Timeline::PanelMaxX() const {
    int vis = DopeEyeX() - gapGS; if (vis < 1) vis = 1;
    int m = vis - panelContW; return m < 0 ? m : 0;
}
// la barra horizontal del panel (abajo, del ancho del texto). tx/tw = el agarre. false = no hace falta
bool Timeline::PanelBarH(int& bx, int& by, int& bw, int& bh, int& tx, int& tw) const {
    if (panelW <= 0 || PanelMaxX() >= 0) return false;
    bh = GlobalScale*4; bx = 0; by = height - bh - GlobalScale; bw = DopeEyeX() - gapGS;
    if (bw < GlobalScale*4) return false;
    tw = bw * bw / (panelContW > 1 ? panelContW : 1); if (tw < GlobalScale*6) tw = GlobalScale*6; if (tw > bw) tw = bw;
    tx = bx + (int)((float)(bw - tw) * (float)panelPosX / (float)PanelMaxX());
    return true;
}
// banda de unos px alrededor del separador del panel (arrastrarla cambia su ancho)
bool Timeline::PanelInBorder(int lx, int ly) const {
    return panelW > 0 && ly >= numY && ly < height && (float)lx >= (float)panelW - LineRadius() && (float)lx <= (float)panelW + LineRadius();
}

void Timeline::RenderDopePanel(){
    if (dopeRows.empty()) return;
    // PANEL OCULTO (Summary plegado): el timeline ocupa TODO y el texto "Summary" queda FLOTANDO encima,
    // sin fondo ni columna. Solo se dibuja la flecha + el nombre de la fila 0.
    { int rx, ry, rw, rh;
      if (DopeRectFloating(rx, ry, rw, rh)){   // panel OCULTO: solo el "Summary" flotante
        // RECTANGULO de fondo: sin el, el texto flotante se MEZCLA con los numeros de frame.
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray); gfx::Disable(gfx::Blend);
        gfx::Color4f(0.10f,0.10f,0.10f,1.0f); FillRect(rx, ry, rw, rh);
        SetCol(ColorID::grayLine, 1.0f); FillRect(rx+rw, ry, GlobalScale, rh); // borde derecho (lo separa del strip)
        gfx::Enable(gfx::Texture2D); gfx::Enable(gfx::Blend); gfx::BlendAlpha(); gfx::EnableArray(gfx::TexCoordArray);
        if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
        gfx::PushMatrix(); gfx::Translatef((float)(rx + marginGS), (float)(ry + (rh - IconSizeGS)/2), 0);
        SetCol(ColorID::white, 1.0f);
        W3dDrawStrip4(IconMesh, IconsUV[(size_t)IconType::arrowRight]->uvs); // plegado
        gfx::Translatef((float)(IconSizeGS + gapGS), 0, 0);
        RenderBitmapText(dopeRows[0].name);
        gfx::PopMatrix();
        return;
    } }
    // fondo del panel (mas oscuro que la tarjeta) + separador vertical. Arranca en numY: 2 COLUMNAS que empiezan
    // a la misma altura (la 1ra fila, Summary, queda en la banda de los numeros).
    gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray); gfx::Disable(gfx::Blend);
    gfx::Color4f(0.13f,0.13f,0.13f,1.0f); FillRect(0, numY, panelW, height - numY);
    // separador: BLANCO con el mouse encima, VERDE mientras se arrastra (asi se sabe que se puede agarrar)
    if (panelResize)          { SetCol(ColorID::accent, 1.0f); FillRect(panelW - GlobalScale*2, numY, GlobalScale*2, height - numY); }
    else if (panelBorderHover) { SetCol(ColorID::white, 1.0f); FillRect(panelW - GlobalScale*2, numY, GlobalScale*2, height - numY); }
    else { SetCol(ColorID::grayLine, 1.0f); FillRect(panelW - GlobalScale, numY, GlobalScale, height - numY); }
    // fondo de la fila del SUMMARY dentro del panel (que quede continuo con el strip)
    { int ry0 = stripY + PosY; if (ry0 + rowH > stripY && ry0 < height){
        gfx::Color4f(0.10f,0.10f,0.10f,1.0f); FillRect(0, ry0<numY?numY:ry0, panelW - GlobalScale, rowH); } }

    // fondo de fila: SELECCIONADA (accent) / HOVER (mas clara). Fase sin textura.
    // (la parte del STRIP de la fila seleccionada la pinta el bloque de keyframes -> el renglon cruza TODO)
    for (size_t r=0; r<dopeVisible; r++){
        int ry = stripY + (int)r*rowH + PosY;
        if (ry + rowH < stripY || ry > height) continue;
        bool sel = g_dopeRowSel.count(dopeRows[r].keyRow) > 0;
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
    { int lw = DopeEyeX() - gapGS, lh = height - stripY;
      if (lw < 0) lw = 0; if (lh < 0) lh = 0;
      gfx::Enable(gfx::ScissorTest);
      gfx::Scissor(x, W3dScreenHeight - y - stripY - lh, lw, lh); }
    for (size_t r=0; r<dopeVisible; r++){
        int ry = stripY + (int)r*rowH + PosY;
        if (ry + rowH < stripY || ry > height) continue;  // culling vertical
        DopeRow& d = dopeRows[r];
        gfx::PushMatrix();
        // LAS LINEAS DEL ARBOL (como en el Outliner): debajo de lo desplegado (el objeto, la transformacion...), una
        // raya en la columna de cada nivel de arriba, de fila en fila: se ve que filas cuelgan de cual
        if (d.level > 0){
            SetCol(ColorID::grayUI, 1.0f);
            gfx::PushMatrix();
            gfx::Translatef((float)(panelPosX + marginGS), (float)ry, 0);
            for (int k = 0; k < d.level; k++){
                W3dDrawStrip4(IconLineMesh, IconsUV[(size_t)IconType::line]->uvs);
                gfx::Translatef((float)(IconSizeGS + gapGS), 0, 0);
            }
            gfx::PopMatrix();
        }
        gfx::Translatef((float)(panelPosX + marginGS + d.level*(IconSizeGS+gapGS)), (float)ry, 0);
        if (d.type==3) SetCol(ColorID::grayUI, 1.0f); else SetCol(ColorID::white, 1.0f);
        // un canal de EJE (X / Y / Z de posicion, rotacion, escala): el texto con el color de su curva (el eje que
        // se VE, como en DibujarCurvas). Seleccionado va BLANCO: sobre el fondo verde el color se perderia.
        if (d.type==3 && (d.propId==AnimPosition || d.propId==AnimRotation || d.propId==AnimScale) && (d.compId==AnimX || d.compId==AnimY || d.compId==AnimZ)
            && DopeVertexLayerOf(d.ownerKey) < 0){
            if (g_dopeRowSel.count(d.keyRow)) SetCol(ColorID::white, 1.0f);
            else { const int seenAxis = (d.propId == AnimPosition && d.compId == AnimY) ? AnimZ
                                      : (d.propId == AnimPosition && d.compId == AnimZ) ? AnimY : d.compId;
                   gfx::Color4fv(AxisColor(seenAxis)); }
        }
        if (!d.keyExpansion.empty()){
            W3dDrawStrip4(IconMesh, IconsUV[(size_t)(DopeCollapsed(d.keyExpansion) ? IconType::arrowRight : IconType::arrow)]->uvs);
            gfx::Translatef((float)(IconSizeGS + gapGS), 0, 0);
        }
        if (d.icon >= 0){
            W3dDrawStrip4(IconMesh, IconsUV[(size_t)d.icon]->uvs);
            gfx::Translatef((float)(IconSizeGS + gapGS), 0, 0);
        }
        RenderBitmapText(d.name);
        gfx::PopMatrix();
    }
    gfx::Disable(gfx::ScissorTest); // fin del recorte del texto del panel

    // ---- OJOS (mismo icono y misma columna que el Outliner): apagarlo oculta la curva ----
    for (size_t r=0; r<dopeVisible; r++){
        int ry = stripY + (int)r*rowH + PosY;
        if (ry + rowH < stripY || ry > height) continue;   // culling vertical
        const DopeRow& d = dopeRows[r];
        if (d.stripRows < 0) continue;   // (continuacion de una tira: el ojo es el de su fila)
        SetCol(ColorID::grayUI, d.hidden ? 0.5f : 1.0f);
        gfx::PushMatrix();
        gfx::Translatef((float)DopeEyeX(), (float)(ry + (rowH - IconSizeGS)/2), 0);
        W3dDrawStrip4(IconMesh, IconsUV[(size_t)(d.hidden ? IconType::hidden : IconType::visible)]->uvs);
        gfx::PopMatrix();
    }

    // ---- BARRA HORIZONTAL del panel (el texto no entra): agarre gris, verde arrastrando ----
    { int bx, by, bw, bh, tx, tw;
      if (PanelBarH(bx, by, bw, bh, tx, tw)){
        gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray); gfx::Disable(gfx::Blend);
        gfx::Color4f(0.08f,0.08f,0.08f,1.0f); FillRect(bx, by, bw, bh);
        if (panelBarH) SetCol(ColorID::accent, 1.0f); else SetCol(ColorID::grayUI, 1.0f);
        FillRect(tx, by + GlobalScale, tw, bh - GlobalScale*2);
        gfx::Enable(gfx::Texture2D); gfx::Enable(gfx::Blend); gfx::BlendAlpha(); gfx::EnableArray(gfx::TexCoordArray);
        if (!Textures.empty() && Textures[0]) gfx::BindTexture(Textures[0]->iID);
      } }
    // ---- BARRA DE SCROLL: la compartida del editor (hover/agarre/arrastre/touch los da el ruteo generico) ----
    if (scrollY) DrawScrollbar(this);
}

// click en el PANEL: sobre la FLECHA pliega/despliega; sobre el resto SELECCIONA la fila (shift = agrega).
bool Timeline::DopeClickPanel(int mx, int my){
    if (dopeRows.empty()) return false;
    int lx = mx - x, ly = my - y;
    // PANEL OCULTO (Summary plegado): el click sobre el texto FLOTANTE lo vuelve a DESPLEGAR. Usa el MISMO rect
    // que dibuja RenderDopePanel (DopeRectFloating) -> imposible que se desincronicen.
    { int rx, ry, rw, rh;
      if (DopeRectFloating(rx, ry, rw, rh)){
        if (lx >= rx && lx < rx+rw && ly >= ry && ly < ry+rh){ g_dopeCollapsed.erase("summary"); g_redraw = true; return true; }
        return false;
      } }
    // BORDE del panel: arrastrarlo cambia su ancho (banda de unos px alrededor del separador)
    if (PanelInBorder(lx, ly)){ panelResize = true; g_redraw = true; return true; }
    // BARRA HORIZONTAL del panel: el agarre se arrastra; afuera del agarre, salta ahi
    { int bx, by, bw, bh, tx, tw;
      if (PanelBarH(bx, by, bw, bh, tx, tw) && lx >= bx && lx < bx+bw && ly >= by - GlobalScale && ly < by+bh+GlobalScale){
        if (lx < tx || lx >= tx + tw){
            int m = PanelMaxX(), rec = bw - tw;
            panelPosX = rec > 0 ? (int)((float)m * (float)(lx - bx - tw/2) / (float)rec) : 0;
            if (panelPosX < m) panelPosX = m; if (panelPosX > 0) panelPosX = 0;
        }
        panelBarH = true; g_redraw = true; return true;
      } }
    if (lx < 0 || lx >= panelW || ly < stripY || ly >= height) return false;
    int r = (ly - stripY - PosY) / (rowH>0?rowH:1);
    if (r < 0 || r >= (int)dopeVisible) return true; // dentro del panel pero sin fila: igual consumido
    DopeRow& d = dopeRows[r];
    // OJO: primero, que esta pegado al borde derecho del panel (y ahi no hay ni flecha ni nombre)
    if (lx >= DopeEyeX() - gapGS){
        if (DopeEyeOff(d.keyRow)) g_dopeEyeOff.erase(d.keyRow); else g_dopeEyeOff.insert(d.keyRow);
        g_redraw = true; return true;
    }
    int xArrow = panelPosX + marginGS + d.level*(IconSizeGS+gapGS);
    if (!d.keyExpansion.empty() && lx >= xArrow && lx < xArrow + IconSizeGS + gapGS){
        if (DopeCollapsed(d.keyExpansion)) g_dopeCollapsed.erase(d.keyExpansion); else g_dopeCollapsed.insert(d.keyExpansion);
        g_redraw = true; return true;
    }
    if (!LShiftPressed) g_dopeRowSel.clear();
    if (!d.keyRow.empty()){
        if (LShiftPressed && g_dopeRowSel.count(d.keyRow)) g_dopeRowSel.erase(d.keyRow);
        else g_dopeRowSel.insert(d.keyRow);
    }
    DopeSelRowSelKeys(); // clickear una fila SELECCIONA ESA CURVA: sus keyframes quedan elegidos (asi se le ven
                             // los handles y le sirven G/S/R). Una fila padre elige las curvas que cubre.
    if (g_dopeAutoFrame) DopeFrameSelected(); // ...y se encuadra sola (View > Auto frame)
    g_redraw = true; return true;
}

// sincroniza la seleccion de KEYFRAMES con la de FILAS: los keyframes de cada fila seleccionada (si es una fila
// padre, los de todos los canales que cuelgan de ella por prefijo del ownerKey).
void Timeline::DopeSelRowSelKeys(){
    g_dopeKeySel.clear();
    for (size_t r=0;r<dopeRows.size();r++){
        const DopeRow& d = dopeRows[r];
        if (d.ghost || !g_dopeRowSel.count(d.keyRow)) continue;
        if (d.propId >= 0){                        // fila de CANAL: es una curva
            for (size_t i=0;i<d.keys.size();i++) g_dopeKeySel.insert(DopeKeyId(d.ownerKey,d.propId,d.compId,d.keys[i]));
        } else {                                   // fila PADRE (objeto / hueso / Summary): las curvas que cubre
            for (size_t k=0;k<dopeRows.size();k++){
                const DopeRow& c = dopeRows[k]; if (c.propId < 0) continue;
                if (!DopeCovers(d.ownerKey, c.ownerKey)) continue;
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
    if (r < 0 || r >= (int)dopeVisible) return false;
    const DopeRow& d = dopeRows[r];
    if (d.hidden) return false;                           // ojo apagado: no se puede clickear lo que no se ve
    // OJO: 'mejor' es un NUMERO DE FRAME (puede ser negativo): el "hubo hit" va
    // en un flag aparte, sino los keyframes en frames negativos no se podian clickear
    int best = 0; bool hasBest = false; float bestD = 1e9f;
    for (size_t i=0;i<d.keys.size();i++){ float dd = fabsf(FrameToX((float)d.keys[i]) - (float)lx); if (dd < bestD){ bestD=dd; best=d.keys[i]; hasBest=true; } }
    if (!hasBest || bestD > GrabRadius()) return false; // no cayo sobre un rombo
    if (!LShiftPressed) g_dopeKeySel.clear();
    if (d.propId >= 0){
        std::string id = DopeKeyId(d.ownerKey, d.propId, d.compId, best);
        if (LShiftPressed && g_dopeKeySel.count(id)) g_dopeKeySel.erase(id); else g_dopeKeySel.insert(id);
    } else {
        // fila PADRE / SUMMARY: marca los keyframes de ese frame de todos los canales que cubre (prefijo del ownerKey;
        // el summary tiene ownerKey vacio -> cubre todo)
        for (size_t k=0;k<dopeRows.size();k++){
            const DopeRow& c = dopeRows[k]; if (c.propId < 0) continue;
            if (!DopeCovers(d.ownerKey, c.ownerKey)) continue;
            for (size_t i=0;i<c.keys.size();i++) if (c.keys[i]==best) g_dopeKeySel.insert(DopeKeyId(c.ownerKey,c.propId,c.compId,best));
        }
    }
    // ...y queda listo para ARRASTRARLO (igual que en curvas): si el mouse se mueve, arranca el 'g' solo
    if (!g_dopeKeySel.empty()){ g_dopeDragPend = true; g_dopeDragX0 = mx; g_dopeDragY0 = my; }
    g_redraw = true; return true;
}

// ---- click en modo CURVAS: primero los HANDLES del keyframe seleccionado, despues los keyframes (en 2D: el
//      hit-test mira tiempo Y valor, no solo la columna del frame). ----
// el HANDLE (de un keyframe elegido, y a la vista) mas cercano al punto. agarrar = ademas lo toma para arrastrarlo.
// Lo usan el click (antes que el gizmo y que los keyframes: es lo mas chico y lo que esta encima) y el dedo (sobre un
// handle, arrastrar lo edita en vez de mover la vista).
bool Timeline::HandleUnder(int mx, int my, bool grab){
    if (mode != TL_MODE_CURVES || dopeRows.empty() || !g_tlShowHandles) return false;
    int lx = mx - x, ly = my - y;
    if (lx < panelW || ly < stripY || ly >= height) return false;
    if (OnToolbar(mx, my)) return false;
    const float rHandle = GrabRadius();
    { float bestD = 1e30f; bool has = false;
      std::string bOwner; int bProp=-1, bComp=-1, bFrame=0; bool bOutput=true;
      static AnimProperty s_vtxTmp;
      for (size_t r=0; r<dopeRows.size(); r++){
        const DopeRow& d = dopeRows[r]; if (d.propId < 0 || d.hidden) continue;
        AnimProperty* ap = VertexChannelTemp(d.ownerKey, s_vtxTmp) ? &s_vtxTmp
                         : DopeResolveProp(d.ownerKey, d.propId, d.compId, false);
        if (!ap) continue;
        for (size_t i=0;i<ap->keyframes.size();i++){
            const keyFrame& kf = ap->keyframes[i];
            if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, kf.frame))) continue;
            for (int side=0; side<2; side++){
                bool output = (side==1);
                if (!HandleVisible(ap, i, output)) continue;
                float hx, hy; HandlePos(ap, i, output, hx, hy);
                float ddx = hx-(float)lx, ddy = hy-(float)ly;
                if (fabsf(ddx) > rHandle || fabsf(ddy) > rHandle) continue;
                float dd = ddx*ddx + ddy*ddy;
                if (dd >= bestD) continue;
                bestD  = dd; has = true;
                bOwner = d.ownerKey; bProp = d.propId; bComp = d.compId; bFrame = kf.frame; bOutput = output;
            }
        }
      }
      if (!has) return false;
      if (!grab) return true;
      g_hOn = true; g_hDirOk = false; g_hOwner = bOwner; g_hProp = bProp; g_hComp = bComp; g_hFrame = bFrame; g_hOutput = bOutput;
      g_hVirtX = mx; g_hVirtY = my;
      g_hVirtFirst = true;
      UndoKeyframesStart();
      g_redraw = true;
      return true; }
}

bool Timeline::CurveClickStrip(int mx, int my){
    if (dopeRows.empty()) return false;
    int lx = mx - x, ly = my - y;
    if (lx < panelW || ly < stripY || ly >= height) return false;
    const float rHandle = GrabRadius(), rKey = GrabRadius();

    // 1) HANDLES (tienen prioridad: estan encima del keyframe y son mas chicos). Gana el MAS CERCANO, no el primero
    //    que entre en el radio: los dos handles de un keyframe estan a la misma distancia de el y con keyframes
    //    juntos se solapan -> si se cortaba en el primero, el de ENTRADA se comia siempre al de SALIDA.
    { float bestD = 1e30f; bool has = false;
      std::string bOwner; int bProp=-1, bComp=-1, bFrame=0; bool bOutput=true;
      static AnimProperty s_vtxTmp;
      for (size_t r=0; r<dopeRows.size(); r++){
        const DopeRow& d = dopeRows[r]; if (d.propId < 0 || d.hidden) continue;
        // canal VERTEX: sus handles se agarran sobre la curva temporal (valor = indice). Es solo el
        // hit-test: si se agarra un handle, el UndoKeyframesStart de abajo y HandleApply copian
        AnimProperty* ap = VertexChannelTemp(d.ownerKey, s_vtxTmp) ? &s_vtxTmp
                         : DopeResolveProp(d.ownerKey, d.propId, d.compId, false);
        if (!ap) continue;
        for (size_t i=0;i<ap->keyframes.size();i++){
            const keyFrame& kf = ap->keyframes[i];
            if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, kf.frame))) continue; // sin seleccionar no hay handle
            for (int side=0; side<2; side++){
                bool output = (side==1);
                if (!HandleVisible(ap, i, output)) continue;   // MISMA condicion que el dibujo: no se agarra lo que no se ve
                float hx, hy; HandlePos(ap, i, output, hx, hy);   // y la MISMA posicion
                float ddx = hx-(float)lx, ddy = hy-(float)ly;
                if (fabsf(ddx) > rHandle || fabsf(ddy) > rHandle) continue;
                float dd = ddx*ddx + ddy*ddy;
                if (dd >= bestD) continue;
                bestD  = dd; has = true;
                bOwner = d.ownerKey; bProp = d.propId; bComp = d.compId; bFrame = kf.frame; bOutput = output;
            }
        }
      }
      if (has){
        g_hOn = true; g_hDirOk = false; g_hOwner = bOwner; g_hProp = bProp; g_hComp = bComp; g_hFrame = bFrame; g_hOutput = bOutput;
        g_hVirtX = mx; g_hVirtY = my;   // el cursor virtual arranca donde esta el real
        g_hVirtFirst = true;
        UndoKeyframesStart();     // Ctrl+Z: curvar es editable/deshacible como todo lo demas
        return true;
      } }
    // 2) KEYFRAMES (hit-test 2D). Igual que los handles: gana el MAS CERCANO, no el primero que caiga en el radio.
    //    (en el modo CURVA el click no toma keyframes: toma el tramo, mas abajo)
    if (g_tlSelMode != TL_SEL_CURVE)
    //    Con varias curvas encimadas, cortar en el primero hacia ganar siempre a la fila de mas arriba aunque su
    //    keyframe estuviera bastante mas lejos del mouse que el de otra.
    { float bestD = 1e30f; bool has = false; std::string bId;
      std::string bOwner; int bProp = -1, bComp = -1, bFrame = 0;
      static AnimProperty s_vtxTmp;
      for (size_t r=0; r<dopeRows.size(); r++){
        const DopeRow& d = dopeRows[r]; if (d.propId < 0 || d.hidden) continue;
        // canal VERTEX: hit-test sobre la curva temporal (valor = indice del keyframe). Elegir no edita.
        AnimProperty* ap = VertexChannelTemp(d.ownerKey, s_vtxTmp) ? &s_vtxTmp
                         : DopeResolveProp(d.ownerKey, d.propId, d.compId, false);
        if (!ap) continue;
        for (size_t i=0;i<ap->keyframes.size();i++){
            const keyFrame& kf = ap->keyframes[i];
            float kx = FrameToX((float)kf.frame), ky = ValueToY(kf.value);
            float ddx = kx-(float)lx, ddy = ky-(float)ly;
            if (fabsf(ddx) > rKey || fabsf(ddy) > rKey) continue;
            float dd = ddx*ddx + ddy*ddy;
            if (dd >= bestD) continue;
            bestD  = dd; has = true; bId = DopeKeyId(d.ownerKey, d.propId, d.compId, kf.frame);
            bOwner = d.ownerKey; bProp = d.propId; bComp = d.compId; bFrame = kf.frame;
        }
      }
      if (has){
        if (!LShiftPressed) g_dopeKeySel.clear();
        if (LShiftPressed && g_dopeKeySel.count(bId)) g_dopeKeySel.erase(bId); else g_dopeKeySel.insert(bId);
        if (g_dopeKeySel.count(bId)){   // quedo elegido -> es el ACTIVO (el que muestra el panel de propiedades)
            g_dopeActOwner = bOwner; g_dopeActProp = bProp; g_dopeActComp = bComp; g_dopeActFrame = bFrame;
            g_dopeActHas = true;
            // ...y queda listo para ARRASTRARLO: si el mouse se mueve, arranca el 'g' solo (ver event_mouse_motion)
            g_dopeDragPend = true; g_dopeDragX0 = mx; g_dopeDragY0 = my;
        }
        g_redraw = true; return true;
      } }
    // 3) EL TRAMO de curva bajo el click (modo CURVA, o AMBOS sin keyframe cerca): sus dos keyframes quedan elegidos
    if (g_tlSelMode != TL_SEL_KEYFRAME){
      const float fClick = XToFrame((float)lx);
      float bestD = rKey * 1.5f; int bRow = -1; size_t bI = 0;
      for (size_t r=0; r<dopeRows.size(); r++){
        const DopeRow& d = dopeRows[r]; if (d.propId < 0 || d.hidden || d.ghost || DopeVertexAnimOf(d.ownerKey)) continue;
        AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId, false);
        if (!ap || ap->keyframes.size() < 2) continue;
        for (size_t i=0; i+1<ap->keyframes.size(); i++){
            if (fClick < (float)ap->keyframes[i].frame || fClick > (float)ap->keyframes[i+1].frame) continue;
            const float dy = fabsf(ValueToY(ap->EvalF(fClick, 0.0f)) - (float)ly);
            if (dy < bestD){ bestD = dy; bRow = (int)r; bI = i; }
            break;
        }
      }
      if (bRow >= 0){ DopeSegmentPick(bRow, (int)bI, LShiftPressed); g_dopeDragPend = true; g_dopeDragX0 = mx; g_dopeDragY0 = my; return true; }
    }
    return false;
}

// arrastre del handle -> nueva PENDIENTE (valor por frame) desde la posicion del mouse. El tramo que toca pasa a
// BEZIER: agarrar un handle ES pedir una curva. OJO con de quien es el tramo: el que manda es el keyframe IZQUIERDO,
// asi que el handle de ENTRADA de K curva el tramo de K-1.
// Congela los handles CALCULADOS (Vector/Auto/AutoClamped) en los offsets guardados y pasa el keyframe a Aligned.
// Sin esto, tocar un handle automatico no haria nada: EffectiveHandle lo seguiria recalculando y el arrastre se
// perderia. Se congela lo que ya se estaba VIENDO, asi que el handle no salta al agarrarlo. Queda Aligned porque
// los automaticos ya son colineales: es el tipo que menos cambia lo que ves.
// CTRL arrastrando un handle: la DIRECCION con la que la curva viene del otro lado del keyframe. El handle de
// SALIDA sigue la tangente con la que la curva LLEGA (tramo anterior); el de ENTRADA, la opuesta a la tangente con
// la que SALE (tramo siguiente). Asi el keyframe queda liso: la curva sigue derecho, sin quiebre. Si del otro lado
// no hay tramo (la punta de la curva), se conserva la direccion que el handle ya tenia.
static void HandleDirCurve(AnimProperty* ap, size_t i, bool output, float& dF, float& dV){
    const keyFrame& k = ap->keyframes[i];
    dF = output ? k.outDF : k.inDF; dV = output ? k.outDV : k.inDV;
    const size_t n = ap->keyframes.size();
    const float e = 0.01f;
    float tf = 0.0f, tv = 0.0f;
    if (output && i > 0){
        const keyFrame& p = ap->keyframes[i-1];
        const float span = (float)(k.frame - p.frame); if (span <= 0.0f) return;
        if (p.Interpolation == KfBezier){ float a, b; ap->EffectiveHandle(i, false, a, b); tf = -a; tv = -b; }
        else if (KfIsEasing(p.Interpolation)){ tf = span * e; tv = (k.value - p.value) * (1.0f - KfEasing(p.Interpolation, 1.0f - e)); }
        else if (p.Interpolation == KfConstant){ tf = 1.0f; tv = 0.0f; }
        else { tf = span; tv = k.value - p.value; }
        if (tf > 1e-6f){ dF = tf; dV = tv; }
    } else if (!output && i + 1 < n){
        const keyFrame& q = ap->keyframes[i+1];
        const float span = (float)(q.frame - k.frame); if (span <= 0.0f) return;
        if (k.Interpolation == KfBezier){ ap->EffectiveHandle(i, true, tf, tv); }
        else if (KfIsEasing(k.Interpolation)){ tf = span * e; tv = (q.value - k.value) * KfEasing(k.Interpolation, e); }
        else if (k.Interpolation == KfConstant){ tf = 1.0f; tv = 0.0f; }
        else { tf = span; tv = q.value - k.value; }
        if (tf > 1e-6f){ dF = -tf; dV = -tv; }
    }
}
static void HandleBake(AnimProperty* ap, size_t i){
    keyFrame& kf = ap->keyframes[i];
    if (kf.handleType == HFree || kf.handleType == HAligned) return;
    float aDF, aDV, bDF, bDV;
    ap->EffectiveHandle(i, false, aDF, aDV);
    ap->EffectiveHandle(i, true,  bDF, bDV);
    kf.inDF = aDF; kf.inDV = aDV; kf.outDF = bDF; kf.outDV = bDV;
    kf.handleType = HAligned;
}
// ALIGNED: los dos lados quedan en la misma recta. El que NO se movio conserva su LARGO y solo se da vuelta para
// seguir la direccion del que si se movio.
static void HandleAlign(keyFrame& kf, bool movedOutput){
    float mF = movedOutput ? kf.outDF : kf.inDF;
    float mV = movedOutput ? kf.outDV : kf.inDV;
    float L = sqrtf(mF*mF + mV*mV);
    if (L < 1e-6f) return;
    float oF = movedOutput ? kf.inDF : kf.outDF;
    float oV = movedOutput ? kf.inDV : kf.outDV;
    float oL = sqrtf(oF*oF + oV*oV);
    if (oL < 1e-6f) oL = L;                       // el otro no tenia largo: se le da el mismo
    float uF = -mF/L*oL, uV = -mV/L*oL;           // direccion OPUESTA, largo propio
    if (movedOutput){ kf.inDF = uF; kf.inDV = uV; } else { kf.outDF = uF; kf.outDV = uV; }
}

void Timeline::HandleApply(int mx, int my){
    if (!g_hOn) return;
    // canal VERTEX: la curva es la temporal (valor = indice); tras editar se copian los
    // handles de vuelta al VertexFrame (ver VertexChannelTemp / EvalVertexAnim).
    static AnimProperty s_vtxTmp;
    VertexAnimation* van = VertexChannelTemp(g_hOwner, s_vtxTmp);
    AnimProperty* ap = van ? &s_vtxTmp : DopeResolveProp(g_hOwner, g_hProp, g_hComp); if (!ap) return;
    size_t i = 0; bool has = false;
    for (size_t k=0;k<ap->keyframes.size();k++) if (ap->keyframes[k].frame == g_hFrame){ i=k; has=true; break; }
    if (!has) return;
    keyFrame& kf = ap->keyframes[i];
    HandleBake(ap, i);                     // si era de los CALCULADOS, congelar lo que se estaba viendo
    if (!g_hDirOk){ g_hDirOk = true; HandleDirCurve(ap, i, g_hOutput, g_hDirF, g_hDirV); }
    float dF = XToFrame((float)(mx - x)) - (float)kf.frame;
    float dV = YToValue((float)(my - y)) - kf.value;
    // CTRL: sobre la recta de la curva que VENIA (HandleDirCurve; proyeccion en PIXELES: los dos ejes tienen zoom
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
    if (g_hOutput){ if (dF <  minDF) dF =  minDF; } else { if (dF > -minDF) dF = -minDF; }
    // filas VERTICES/UV: SIN overshoot — el handle queda DENTRO del rectangulo del tramo
    // (estilo "auto clamped": tiempo dentro del tramo, valor entre los dos keys). Es el
    // MISMO clamp del evaluador (VertexAnimClampHandle): lo que agarras es lo que corre,
    // y no se pueden crear zonas fuera de rango. La curva temporal va en rampa de 1 por
    // keyframe, asi que el 0..1 del ease coincide con el delta de valor del tramo.
    if (van){
        float span = 0.0f;
        if (g_hOutput){ if (i+1 < ap->keyframes.size()) span = (float)(ap->keyframes[i+1].frame - kf.frame); }
        else          { if (i > 0)                      span = (float)(kf.frame - ap->keyframes[i-1].frame); }
        VertexAnimClampHandle(g_hOutput, span, dF, dV);
    }
    if (g_hOutput){ kf.outDF = dF; kf.outDV = dV; } else { kf.inDF = dF; kf.inDV = dV; }
    if (kf.handleType == HAligned) HandleAlign(kf, g_hOutput);  // el otro lado sigue al que moviste
    // (no hace falta poner KfBezier: el handle solo existe si el tramo YA es bezier — ver HandleVisible)
    // filas VERTICES/UV: los handles viven en el VertexFrame. Se copian los DOS lados de
    // la curva temporal: el editado (recien asignado) y el otro, que trae el valor
    // EFECTIVO que se estaba viendo (VertexAnimHandlesSegment) -> queda BAKEADO, como
    // HandleBake: tocar un handle no hace saltar al de enfrente. HFree: ya es propio.
    if (van){ for (size_t k=0;k<van->frames.size();k++) if (van->frames[k]->frame == g_hFrame){
        VertexFrame* vf = van->frames[k];
        vf->handleType = HFree;
        vf->inDF = kf.inDF; vf->inDV = kf.inDV; vf->outDF = kf.outDF; vf->outDV = kf.outDV;
        if (van->target) van->target->skinGeomVersion++; // re-evaluar el ease en el viewport
        break; } }
    InvalidateAnimAndRedraw();
}
void Timeline::HandleRelease(){
    if (!g_hOn) return;
    g_hOn = false; UndoKeyframesConfirm(); g_redraw = true;
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
    UndoKeyframesStart();
    int n = 0;
    for (size_t r=0;r<dopeRows.size();r++){
        const DopeRow& d = dopeRows[r];
        if (d.propId != AnimRotation || d.hidden) continue;   // solo rotacion, y solo lo que se ve
        AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId); if (!ap) continue;
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
    UndoKeyframesConfirm();
    if (n) InvalidateAnimAndRedraw();
    return n;
}

// menu Handle Type ('v') -> a todos los keyframes SELECCIONADOS. Al pasar a HFree/HAligned hay que CONGELAR antes
// los handles que se estaban viendo: si no, saltarian de golpe a (0,0), que es lo que hay guardado mientras el tipo
// es de los calculados.
// ---- TOOLBAR de abajo ----
// Con el DEDO, SIEMPRE (deshacer/rehacer, el Shift y el modo del arrastre); con el mouse, solo con keyframes elegidos
// (el timeline de la PC es bajito y la barra le comeria casi todo: ahi estan Ctrl+Z y Shift). Nunca moviendo keyframes.
// Los grupos de la interpolacion / los handles / el ease, solo con keyframes elegidos (ver ToolbarSync)
bool Timeline::ToolbarVisible() const {
    if (g_dopeMov) return false;
    // (tambien con un SONIDO elegido: ahi esta la tijera)
    return ToolbarUsesTouch() || (!g_dopeKeySel.empty() && !dopeRows.empty()) || PickedStrip() != NULL;
}
void Timeline::ToolbarSync(){
    // lo que tienen los keyframes elegidos: si TODOS comparten la interpolacion / el handle / el ease, ese boton
    // queda VERDE. El grupo del EASE sale con un TRAMO elegido (sus dos puntas) o con una curva de easing.
    int interp = -2, handle = -2, ease = -2; bool segment = false;
    for (size_t r=0;r<dopeRows.size();r++){
        const DopeRow& d = dopeRows[r]; if (d.propId < 0 || DopeVertexAnimOf(d.ownerKey)) continue;
        const AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId, false); if (!ap) continue;
        const size_t n = ap->keyframes.size();
        for (size_t i=0;i<n;i++){
            const keyFrame k = ap->keyframes.Key(i);
            if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, k.frame))) continue;
            const int b = KfInterpBase(k.Interpolation);
            interp = (interp == -2 || interp == b) ? b : -1;
            handle = (handle == -2 || handle == k.handleType) ? k.handleType : -1;
            if (KfIsEasing(k.Interpolation)){ const int e = KfInterpEase(k.Interpolation); ease = (ease == -2 || ease == e) ? e : -1; segment = true; }
            if (i + 1 < n && g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, ap->keyframes.Frame(i + 1)))) segment = true;
        }
    }
    // ACTIVO: como el proportional editing de la barra del 3D (TbGreenBg = accent atenuado de fondo, icono accent)
    const float* green = ColorList[static_cast<int>(ColorID::accent)];
    const bool hasKeys = !g_dopeKeySel.empty() && !dopeRows.empty();
    const bool touch = ToolbarUsesTouch();
    for (size_t i=0;i<ToolButtons.size();i++){
        Button* b = ToolButtons[i]; const int r = b->role;
        bool on = false;
        b->visible = hasKeys;
        if (r == TL_TB_UNDO || r == TL_TB_REDO){
            b->visible = true;
            const bool has = (r == TL_TB_UNDO) ? UndoHasSomething() : UndoHasRedo();   // (gris = no hay nada que des/rehacer)
            b->tint = NULL; b->textColor = has ? ColorList[(int)ColorID::white] : ColorList[(int)ColorID::grayUI];
            continue;
        }
        if (r == TL_TB_TRIM){ b->visible = (PickedStrip() != NULL); b->tint = NULL; b->textColor = NULL; continue; }
        if (r == TL_TB_SHIFT || r == TL_TB_SELEC || r == TL_TB_VIEW){
            b->visible = touch;
            on = (r == TL_TB_SHIFT) ? LShiftPressed : (r == TL_TB_VIEW) ? g_tlViewMode : !g_tlViewMode;
            b->tint = on ? TbGreenBg() : NULL;
            b->textColor = on ? green : NULL;
            continue;
        }
        if (r >= TL_TB_EASE0)        { b->visible = hasKeys && segment; on = (r - TL_TB_EASE0 == ease); }
        else if (r >= TL_TB_HANDLE0) { on = (r - TL_TB_HANDLE0 == handle); }
        else if (r >= TL_TB_INTERP0) { on = (r - TL_TB_INTERP0 == interp); }
        b->tint = on ? TbGreenBg() : NULL;
        b->textColor = on ? green : NULL;
    }
}
void Timeline::ToolbarActionRole(int role){
    g_tlActive = this;
    extern void RebindMaterialMeshPart();
    if (role == TL_TB_UNDO){ UndoUndo(); RebindMaterialMeshPart(); g_redraw = true; return; }
    if (role == TL_TB_REDO){ UndoRedo(); RebindMaterialMeshPart(); g_redraw = true; return; }
    if (role == TL_TB_SHIFT){ LShiftPressed = !LShiftPressed; g_redraw = true; return; }   // (el mismo Shift del 3D)
    if (role == TL_TB_SELEC){ g_tlViewMode = false; g_redraw = true; return; }
    if (role == TL_TB_VIEW){ g_tlViewMode = true; g_redraw = true; return; }
    if (role == TL_TB_TRIM){ StripCut(); g_redraw = true; return; }
    if (role >= TL_TB_EASE0)          SetEasingSel(role - TL_TB_EASE0);
    else if (role >= TL_TB_HANDLE0)   SetHandleTypeSel(role - TL_TB_HANDLE0);
    else if (role >= TL_TB_INTERP0)   SetInterpolationSel(role - TL_TB_INTERP0);
    g_redraw = true;
}
void Timeline::SetHandleTypeSel(int type){
    UndoKeyframesStart();
    for (size_t r=0;r<dopeRows.size();r++){
        const DopeRow& d = dopeRows[r]; if (d.propId<0) continue;
        AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId); if (!ap) continue;
        for (size_t i=0;i<ap->keyframes.size();i++){
            if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey,d.propId,d.compId,ap->keyframes[i].frame))) continue;
            if (type == HFree || type == HAligned){
                float aDF, aDV, bDF, bDV;
                ap->EffectiveHandle(i, false, aDF, aDV);
                ap->EffectiveHandle(i, true,  bDF, bDV);
                keyFrame& kf = ap->keyframes[i];
                kf.inDF = aDF; kf.inDV = aDV; kf.outDF = bDF; kf.outDV = bDV;
            }
            ap->keyframes[i].handleType = type;
            if (type == HAligned) HandleAlign(ap->keyframes[i], true); // dejarlos colineales YA
        }
    }
    UndoKeyframesConfirm();
    InvalidateAnimAndRedraw();
}

// menu Interpolation -> a todos los keyframes SELECCIONADOS (define la forma del tramo que SALE de cada uno)
void Timeline::SetInterpolationSel(int interp){
    UndoKeyframesStart();
    for (size_t r=0;r<dopeRows.size();r++){
        const DopeRow& d = dopeRows[r]; if (d.propId<0) continue;
        // filas VERTICES/UV: la interpolacion vive en el VertexFrame (no en un AnimProperty).
        // Solo se tocan los frames DE LA CAPA de la fila. (Un frame con ambas capas comparte
        // el ease entre las dos curvas: es UN cuadro.)
        VertexAnimation* van = DopeVertexAnimOf(d.ownerKey);
        if (van){
            int layer = DopeVertexLayerOf(d.ownerKey);
            for (size_t k=0;k<van->frames.size();k++){
                if (!VertexFrameInLayer(van->frames[k], layer)) continue;
                if (g_dopeKeySel.count(DopeKeyId(d.ownerKey,d.propId,d.compId,van->frames[k]->frame)))
                    van->frames[k]->interp = interp;
            }
            continue;
        }
        AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId); if (!ap) continue;
        // de ATRAS para adelante: pasar el tramo i a bezier pone los handles de i Y de i+1 (la entrada de i+1 es
        // la mitad de la forma). Si i+1 se procesara DESPUES, sus handles automaticos (la ultima clave, o una que
        // no venia de easing) pisarian esa entrada y la forma se perdia (con todo elegido, que es lo normal)
        for (size_t i=ap->keyframes.size(); i-- > 0; ){
            if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey,d.propId,d.compId,ap->keyframes[i].frame))) continue;
            // (de una curva de easing a otra se conserva su easing: Ctrl+E y E son independientes)
            const int old = ap->keyframes[i].Interpolation;
            ap->keyframes[i].Interpolation = KfInterpWith(interp, KfInterpEase(old));
            // recien pasa a bezier: si venia de una curva de easing, con los handles que conservan SU forma; si no
            // (o si era un rebote / elastico, que no entran en una bezier), handles automaticos
            if (interp == KfBezier && !KfBezierFromEasing(*ap, i, old)) ap->keyframes[i].handleType = HAuto;
        }
    }
    UndoKeyframesConfirm();
    InvalidateAnimAndRedraw();
}
// Ctrl+E: el EASING de los keyframes elegidos. Solo cambia algo en las curvas de easing (Sinusoidal...Elastic): las
// demas (Constant/Linear/Bezier) no tienen de que lado poner el efecto
void Timeline::ChangeMode(bool curves){
    const int newValue = curves ? TL_MODE_CURVES : TL_MODE_DOPE;
    if (newValue == mode) return;
    mode = newValue;
    // AUTO-ENCUADRE al ENTRAR al modo curvas: el zoom vertical viene de la nada (el dope sheet no tiene eje de valor)
    // y la curva podia quedar fuera de pantalla. Las filas tienen que estar armadas ANTES de medir el rango.
    if (mode == TL_MODE_CURVES && g_dopeAutoFrame){ BuildDopeRows(); CurveFrameSelected(); }
    g_redraw = true;
}
// EASE de un tramo bezier de VERTICES (k -> el siguiente de la misma capa). El tramo va de 0 a 1 (el progreso
// entre pose y pose); los handles calculados se congelan antes con los mismos valores que muestra el editor.
static void VertexEaseBezier(VertexAnimation& van, size_t k, int layer, int ease){
    size_t s = k + 1; while (s < van.frames.size() && !VertexFrameInLayer(van.frames[s], layer)) s++;
    if (s >= van.frames.size()) return;
    VertexFrame& A = *van.frames[k]; VertexFrame& B = *van.frames[s];
    if (ease != KfEaseAuto){
        size_t p = k; bool hasP = false;
        while (p > 0){ p--; if (VertexFrameInLayer(van.frames[p], layer)){ hasP = true; break; } }
        size_t q = s + 1; while (q < van.frames.size() && !VertexFrameInLayer(van.frames[q], layer)) q++;
        float o1, o2, i1, i2;
        if (A.handleType != HFree && A.handleType != HAligned){
            if (hasP){ VertexAnimHandlesSegment(*van.frames[p], A, o1, o2, A.inDF, A.inDV); } else { A.inDF = -1.0f; A.inDV = 0.0f; }
            A.handleType = HFree;
        }
        if (B.handleType != HFree && B.handleType != HAligned){
            if (q < van.frames.size()){ VertexAnimHandlesSegment(B, *van.frames[q], B.outDF, B.outDV, i1, i2); } else { B.outDF = 1.0f; B.outDV = 0.0f; }
            B.handleType = HFree;
        }
    }
    KfEaseBezierSegment(A, B, 0.0f, 1.0f, ease);
}
void Timeline::SetEasingSel(int ease){
    UndoKeyframesStart();
    for (size_t r=0;r<dopeRows.size();r++){
        const DopeRow& d = dopeRows[r]; if (d.propId<0) continue;
        VertexAnimation* van = DopeVertexAnimOf(d.ownerKey);
        if (van){
            int layer = DopeVertexLayerOf(d.ownerKey);
            for (size_t k=0;k<van->frames.size();k++){
                if (!VertexFrameInLayer(van->frames[k], layer)) continue;
                if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey,d.propId,d.compId,van->frames[k]->frame))) continue;
                if (KfIsEasing(van->frames[k]->interp)) van->frames[k]->interp = KfInterpWith(KfInterpBase(van->frames[k]->interp), ease);
                else if (van->frames[k]->interp == KfBezier) VertexEaseBezier(*van, k, layer, ease);
            }
            continue;
        }
        AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId); if (!ap) continue;
        for (size_t i=0;i<ap->keyframes.size();i++){
            if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey,d.propId,d.compId,ap->keyframes[i].frame))) continue;
            keyFrame& k = ap->keyframes[i];
            if (KfIsEasing(k.Interpolation)) k.Interpolation = KfInterpWith(KfInterpBase(k.Interpolation), ease);
            else KfEaseBezier(*ap, i, ease);   // bezier comun: el ease va en los handles del tramo
        }
    }
    UndoKeyframesConfirm();
    InvalidateAnimAndRedraw();
}

void Timeline::DopeHover(int mx, int my){
    int lx = mx - x, ly = my - y, h = -1;
    if (!dopeRows.empty() && lx >= 0 && lx < panelW && ly >= stripY && ly < height){
        int r = (ly - stripY - PosY) / (rowH>0?rowH:1);
        if (r >= 0 && r < (int)dopeVisible) h = r;
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
void Timeline::DopeRowsNav(std::vector<int>& out) const {
    out.clear();
    for (size_t r=0;r<dopeRows.size();r++){
        const DopeRow& d = dopeRows[r];
        if (d.propId < 0 || d.hidden) continue;
        if (!d.ghost && g_dopeRowSel.count(d.keyRow)) { out.push_back((int)r); continue; }
        // una FILA PADRE elegida (el objeto, un hueso, "Object Transforms"): las curvas que cubre. El Summary: todas
        for (size_t p=0;p<dopeRows.size();p++){
            const DopeRow& q = dopeRows[p];
            if (q.propId >= 0 || q.ghost || !g_dopeRowSel.count(q.keyRow)) continue;
            if (q.type == 0 || DopeCovers(q.ownerKey, d.ownerKey)) { out.push_back((int)r); break; }
        }
    }
    if (!out.empty()) return;              // hay curvas elegidas: se trabaja sobre ESAS
    // si no, TODAS las de canal (sus frames son los del summary: la union)
    for (size_t r=0;r<dopeRows.size();r++)
        if (dopeRows[r].propId >= 0 && !dopeRows[r].hidden) out.push_back((int)r);
}

// los FRAMES que se recorren (la union de los de las filas de arriba), ordenados
void Timeline::DopeFramesNav(std::vector<int>& out) const {
    out.clear();
    std::vector<int> rows; DopeRowsNav(rows);
    for (size_t i=0;i<rows.size();i++){
        const DopeRow& d = dopeRows[rows[i]];
        for (size_t k=0;k<d.keys.size();k++){
            bool already=false; for (size_t j=0;j<out.size();j++) if (out[j]==d.keys[k]){ already=true; break; }
            if (!already) out.push_back(d.keys[k]);
        }
    }
    std::sort(out.begin(), out.end());
}

// selecciona (o saca) TODOS los keyframes del frame 'f' de las filas que se recorren
void Timeline::DopeSelFrame(int f, bool on){
    std::vector<int> rows; DopeRowsNav(rows);
    for (size_t i=0;i<rows.size();i++){
        const DopeRow& d = dopeRows[rows[i]];
        for (size_t k=0;k<d.keys.size();k++){
            if (d.keys[k] != f) continue;
            std::string id = DopeKeyId(d.ownerKey, d.propId, d.compId, f);
            if (on){ g_dopeKeySel.insert(id);
                     g_dopeActOwner=d.ownerKey; g_dopeActProp=d.propId; g_dopeActComp=d.compId;
                     g_dopeActFrame=f; g_dopeActHas=true; }
            else g_dopeKeySel.erase(id);
        }
    }
}

// lapiz+flechas: avanza el keyframe ACTIVO. extender = mantiene lo que ya estaba elegido.
bool Timeline::DopeSelAdvance(int step, bool extend){
    if (g_tlSelMode == TL_SEL_CURVE) return DopeSegmentAdvance(step, extend);
    std::vector<int> fr; DopeFramesNav(fr);
    if (fr.empty()) return false;
    // el frame de partida: el activo, o el primero elegido, o el mas cercano al frame actual
    int act = g_dopeActHas ? g_dopeActFrame : CurrentFrame;
    int idx = -1;
    for (size_t i=0;i<fr.size();i++) if (fr[i]==act){ idx=(int)i; break; }
    if (idx < 0){ // no esta en la lista: arrancar del mas cercano
        int best=0; int md=0x7fffffff;
        for (size_t i=0;i<fr.size();i++){ int d=fr[i]-act; if(d<0)d=-d; if(d<md){md=d;best=(int)i;} }
        idx = best;
        if (!extend) g_dopeKeySel.clear();
        DopeSelFrame(fr[idx], true);
        g_redraw = true; return true;
    }
    if (!extend) DopeSelFrame(fr[idx], false);   // deselecciona el actual y pasa al siguiente
    int next = idx + step;
    if (next < 0) next = (int)fr.size()-1;
    if (next >= (int)fr.size()) next = 0;
    DopeSelFrame(fr[next], true);
    g_redraw = true; return true;
}
bool Timeline::DopeSelAllNav(){  DopeSelectAll(); return true; }
bool Timeline::DopeSelNothingNav(){  DopeSelectNone(); g_dopeActHas = false; return true; }

void Timeline::DopeSelectAll(){
    g_dopeKeySel.clear();
    // lo OCULTO no entra: ocultar una curva es justamente sacarla del laburo
    for (size_t r=0;r<dopeRows.size();r++){ const DopeRow& d=dopeRows[r]; if (d.propId<0 || d.hidden) continue;
        for (size_t i=0;i<d.keys.size();i++) g_dopeKeySel.insert(DopeKeyId(d.ownerKey,d.propId,d.compId,d.keys[i])); }
    g_redraw = true;
}
void Timeline::DopeSelectNone(){ g_dopeKeySel.clear(); g_dopeRowSel.clear(); g_redraw = true; }
void Timeline::DopeSelectInvert(){
    std::set<std::string> inv;
    for (size_t r=0;r<dopeRows.size();r++){ const DopeRow& d=dopeRows[r]; if (d.propId<0 || d.hidden) continue;
        for (size_t i=0;i<d.keys.size();i++){ std::string id = DopeKeyId(d.ownerKey,d.propId,d.compId,d.keys[i]);
            if (!g_dopeKeySel.count(id)) inv.insert(id); } }
    g_dopeKeySel.swap(inv); g_redraw = true;
}

// 'x': si hay KEYFRAMES seleccionados los borra; sino borra la animacion de las FILAS seleccionadas.
// hay keyframes o curvas elegidos en el timeline? (la C del N95 los borra a ellos y no al objeto activo)
bool TimelineHasSelection() { return !g_dopeKeySel.empty() || !g_dopeRowSel.empty(); }
int  TimelineSelectionCount() { return (int)g_dopeKeySel.size(); }   // (harness)
void Timeline::DopeDeleteSelection(){
    if (g_dopeKeySel.empty() && g_dopeRowSel.empty()) return;
    UndoKeyframesStart(); // Ctrl+Z: snapshot de las curvas ANTES de borrar
    if (!g_dopeKeySel.empty()){
        for (size_t r=0;r<dopeRows.size();r++){ const DopeRow& d=dopeRows[r]; if (d.propId<0) continue;
            // filas VERTICES/UV: borrar POR CAPA — se saca SOLO la capa de la fila (borrar un
            // keyframe de la curva Vertices no toca el de UV del mismo cuadro, y al reves); si
            // el frame queda sin capas se va entero. No dejar la anim sin ningun frame (una
            // vertex anim vacia no tiene sentido): conservar al menos uno.
            VertexAnimation* van = DopeVertexAnimOf(d.ownerKey);
            if (van){
                int layer = DopeVertexLayerOf(d.ownerKey);
                for (size_t k=van->frames.size(); k-- > 0; ){
                    VertexFrame* vf = van->frames[k];
                    if (!VertexFrameInLayer(vf, layer)) continue;
                    if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, vf->frame))) continue;
                    bool onlyIs = (VertexFrameLayers(vf) <= 1);   // sacarle esta capa lo deja sin ninguna
                    if (onlyIs && (int)van->frames.size() <= 1) break; // dejar siempre 1 frame
                    VertexAnimDeleteLayer(*van, vf->frame, layer);
                }
                if (van->target) van->target->skinGeomVersion++; // el frame actual pudo cambiar de pose
                continue;
            }
            AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId); if (!ap) continue;
            // de atras para adelante y por FRAME: DeleteKeyframeKeepingShape re-ajusta los handles de los
            // vecinos, asi que borrar varios de un tramo bezier deja la curva lo mas parecida posible en cada paso.
            for (size_t k=ap->keyframes.size(); k-- > 0; )
                if (g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, ap->keyframes[k].frame)))
                    DeleteKeyframeKeepingShape(*ap, ap->keyframes[k].frame);
        }
        g_dopeKeySel.clear();
    } else if (!g_dopeRowSel.empty()){
        for (size_t r=0;r<dopeRows.size();r++){ const DopeRow& d=dopeRows[r];
            if (!g_dopeRowSel.count(d.keyRow)) continue;
            // filas VERTICES/UV: "borrar la fila" = limpiar SU CAPA de todos los frames
            // (dejando al menos un frame vivo en la anim)
            VertexAnimation* van = (d.propId >= 0) ? DopeVertexAnimOf(d.ownerKey) : NULL;
            if (van){
                int layer = DopeVertexLayerOf(d.ownerKey);
                for (size_t k=van->frames.size(); k-- > 0; ){
                    VertexFrame* vf = van->frames[k];
                    if (!VertexFrameInLayer(vf, layer)) continue;
                    bool onlyIs = (VertexFrameLayers(vf) <= 1);   // sacarle esta capa lo deja sin ninguna
                    if (onlyIs && (int)van->frames.size() <= 1) continue; // dejar siempre 1 frame
                    VertexAnimDeleteLayer(*van, vf->frame, layer);
                }
                if (van->target) van->target->skinGeomVersion++;
            }
            else if (d.propId >= 0){ AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId); if (ap) ap->keyframes.clear(); }
            else { // fila padre: borra las 9 CURVAS (3 propiedades x 3 componentes) de ese dueño + las de los canales
                   // que cubre por prefijo (ej: el armature borra las de todos sus huesos listados)
                const int pid[3] = { AnimPosition, AnimRotation, AnimScale };
                const int cid[3] = { AnimX, AnimY, AnimZ };
                for (int p=0;p<3;p++) for (int c=0;c<3;c++){
                    AnimProperty* ap = DopeResolveProp(d.ownerKey, pid[p], cid[c]); if (ap) ap->keyframes.clear(); }
                for (size_t k=0;k<dopeRows.size();k++){ const DopeRow& c=dopeRows[k]; if (c.propId<0) continue;
                    if (!DopeCovers(d.ownerKey, c.ownerKey)) continue;
                    AnimProperty* ap = DopeResolveProp(c.ownerKey, c.propId, c.compId); if (ap) ap->keyframes.clear(); }
            }
        }
        g_dopeRowSel.clear();
    }
    UndoKeyframesConfirm(); // 1 comando de undo con el borrado
    InvalidateAnimAndRedraw();
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

bool Timeline::DopeCopySelection(){
    if (g_dopeKeySel.empty()) return false;
    std::vector<DopeClipKey> clip;
    std::set<std::string> views;
    int mn = 0x7fffffff;
    for (size_t r=0;r<dopeRows.size();r++){ const DopeRow& d=dopeRows[r];
        if (d.propId < 0 || DopeVertexAnimOf(d.ownerKey)) continue;
        char id[32]; sprintf(id, "|%d|%d", d.propId, d.compId);
        if (!views.insert(d.ownerKey + id).second) continue;     // (la misma curva puede estar en 2 filas)
        AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId, false); if (!ap) continue;
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
void Timeline::DopeCutSelection(){ if (DopeCopySelection()) DopeDeleteSelection(); }
void Timeline::DopePaste(){
    if (g_dopeClip.empty() || g_dopeMov) return;
    g_dopeOverwritten.clear();
    UndoKeyframesStart();   // Ctrl+Z: el pegado es UN comando
    std::set<std::string> newSel;
    for (size_t i=0;i<g_dopeClip.size();i++){
        const DopeClipKey& c = g_dopeClip[i];
        std::string owner = c.ownerKey;
        AnimProperty* ap = DopeResolveProp(owner, c.propId, c.compId);
        if (!ap){   // la curva de origen no esta: la igual de lo que se ve
            for (size_t r=0;r<dopeRows.size() && !ap;r++){ const DopeRow& d=dopeRows[r];
                if (d.propId != c.propId || d.compId != c.compId || DopeVertexAnimOf(d.ownerKey)) continue;
                ap = DopeResolveProp(d.ownerKey, d.propId, d.compId);
                if (ap) owner = d.ownerKey;
            }
        }
        if (!ap) continue;
        keyFrame k = c.k; k.frame = CurrentFrame + c.rel;
        for (size_t j=ap->keyframes.size(); j-- > 0; ) if (ap->keyframes[j].frame == k.frame){
            DopeOverwrittenAnnotate(owner, c.propId, c.compId, ap->keyframes[j]);   // (vuelve si lo pegado se corre: ver arriba)
            ap->keyframes.erase(ap->keyframes.begin()+j);
        }
        ap->keyframes.push_back(k);
        ap->SortKeyFrames();
        newSel.insert(DopeKeyId(owner, c.propId, c.compId, k.frame));
    }
    UndoKeyframesConfirm();
    if (!newSel.empty()){ g_dopeKeySel.swap(newSel); g_dopeRowSel.clear(); }   // lo pegado queda ELEGIDO
    InvalidateAnimAndRedraw();
    // ...y AGARRADO, como el Duplicar: el mouse lo corre, click/Enter lo deja (Esc lo deja donde se pego)
    if (!g_dopeKeySel.empty()) DopeMoveStart(DOPE_MOV);
    g_dopeOverwritten.clear();   // (sin movimiento, lo pegado se quedo encima: lo tapado queda pisado)
}
// CLICK DERECHO en el strip: el menu de los keyframes elegidos (y Pegar, aunque no haya nada elegido)
static void TL_menuCtxAction(int id){
    TL_CloseMenu(g_tlMenuCtx);
    if (!g_tlActive) return;
    switch (id){
        case TL_KEY_CUT: g_tlActive->DopeCutSelection(); break;
        case TL_KEY_COPY: g_tlActive->DopeCopySelection(); break;
        case TL_KEY_PASTE:  g_tlActive->DopePaste(); break;
        case TL_KEY_DUP:    g_tlActive->DopeDuplicateSelection(); break;
        case TL_KEY_OF:    g_tlActive->DopeDeleteSelection(); break;
        case TL_KEY_UNDO: { extern void RebindMaterialMeshPart(); UndoUndo(); RebindMaterialMeshPart(); } break;
        case TL_KEY_REDO:  { extern void RebindMaterialMeshPart(); UndoRedo();  RebindMaterialMeshPart(); } break;
    }
    g_redraw = true;
}
bool Timeline::MenuContextKeyboard(){
    // (un punto del strip: el menu obra sobre los keyframes ELEGIDOS, no sobre el punto)
    const int mx = x + panelW + (width - panelW) / 2, my = y + stripY + (height - stripY) / 2;
    return MenuContext(mx, my, mx, my);
}
bool Timeline::MenuContext(int mx, int my, int menuX, int menuY){
    if (dopeRows.empty() || g_dopeMov) return false;
    const int lx = mx - x, ly = my - y;
    if (lx < panelW || lx >= width || ly < stripY || ly >= height) return false;   // solo el strip
    g_tlActive = this;
    if (!g_tlMenuCtx){ g_tlMenuCtx = new PopupMenu(); g_tlMenuCtx->action = TL_menuCtxAction; }
    g_tlMenuCtx->Clear();
    g_tlMenuCtx->title = T("Keyframes");
    static bool sHasSel, sHasClip;
    sHasSel = !g_dopeKeySel.empty(); sHasClip = !g_dopeClip.empty();
    MenuItem* it;
    static bool sHasUndo, sHasRedo;
    sHasUndo = UndoHasSomething(); sHasRedo = UndoHasRedo();
    it = g_tlMenuCtx->Add(T("Undo"), TL_KEY_UNDO, (int)IconType::undo); it->shortcut = "@global.deshacer"; it->gray = &sHasUndo;
    it = g_tlMenuCtx->Add(T("Redo"), TL_KEY_REDO, (int)IconType::redo);  it->shortcut = "@global.rehacer";  it->gray = &sHasRedo;
    it = g_tlMenuCtx->Add(T("Cut"), TL_KEY_CUT, (int)IconType::scissors);   it->shortcut = "@tl.cortar";   it->gray = &sHasSel;
    it->lineUp = true;
    it = g_tlMenuCtx->Add(T("Copy"), TL_KEY_COPY, (int)IconType::copy);  it->shortcut = "@tl.copiar";   it->gray = &sHasSel;
    it = g_tlMenuCtx->Add(T("Paste"), TL_KEY_PASTE, (int)IconType::paste);   it->shortcut = "@tl.pegar";    it->gray = &sHasClip;
    it = g_tlMenuCtx->Add(T("Duplicate"), TL_KEY_DUP); it->shortcut = "@tl.duplicar"; it->gray = &sHasSel; it->lineUp = true;
    it = g_tlMenuCtx->Add(T("Delete"), TL_KEY_OF);   it->shortcut = "@tl.borrar";   it->gray = &sHasSel;
    if (OpenMenu && OpenMenu != g_tlMenuCtx) OpenMenu->Close();
    g_tlMenuCtx->Open(menuX >= 0 ? menuX : mx, menuY >= 0 ? menuY : my, MenuScreenW, MenuScreenH);
    OpenMenu = g_tlMenuCtx;
    g_redraw = true;
    return true;
}

void Timeline::DopeDuplicateSelection(){
    if (g_dopeKeySel.empty() || g_dopeMov) return;
    g_dopeOverwritten.clear();
    // estado ANTERIOR a duplicar (para que Esc pueda volver a el)
    g_dopeDupPre.clear();
    g_dopeDupVtxPre.clear();
    for (size_t r=0;r<dopeRows.size();r++){ const DopeRow& d=dopeRows[r]; if (d.propId<0) continue;
        bool already=false; for (size_t s=0;s<g_dopeDupPre.size();s++)
            if (g_dopeDupPre[s].ownerKey==d.ownerKey && g_dopeDupPre[s].propId==d.propId && g_dopeDupPre[s].compId==d.compId){ already=true; break; }
        if (already) continue;
        AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId); if (!ap) continue;
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
        VertexAnimation* van = DopeVertexAnimOf(d.ownerKey); if (!van) continue;
        int layer = DopeVertexLayerOf(d.ownerKey);
        for (size_t k=0;k<van->frames.size();k++){
            if (!VertexFrameInLayer(van->frames[k], layer)) continue;
            int f=van->frames[k]->frame;
            if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, f))) continue;
            if (f < selMin) selMin = f; if (f > selMax) selMax = f;
        }
    }
    if (selMin > selMax){ g_dopeDupPre.clear(); return; }
    const int off = (selMax - selMin) + 1;
    UndoKeyframesStart();   // Ctrl+Z: la duplicacion + el movimiento son UN solo comando
    std::set<std::string> newSel;
    for (size_t s=0;s<g_dopeDupPre.size();s++){
        DopeMovProp& mp = g_dopeDupPre[s];
        AnimProperty* ap = DopeResolveProp(mp.ownerKey, mp.propId, mp.compId); if (!ap) continue;
        std::vector<keyFrame> copies;
        for (size_t k=0;k<mp.orig.size();k++){
            if (!g_dopeKeySel.count(DopeKeyId(mp.ownerKey, mp.propId, mp.compId, mp.orig[k].frame))) continue;
            keyFrame c = mp.orig[k]; c.frame = mp.orig[k].frame + off;
            copies.push_back(c);
        }
        if (copies.empty()) continue;
        // sacar lo que estuviera en los frames de destino y meter las copias
        for (size_t c=0;c<copies.size();c++){
            for (size_t k=ap->keyframes.size(); k-- > 0; ) if (ap->keyframes[k].frame == copies[c].frame){
                DopeOverwrittenAnnotate(mp.ownerKey, mp.propId, mp.compId, ap->keyframes[k]);   // (vuelve si la copia se corre)
                ap->keyframes.erase(ap->keyframes.begin()+k);
            }
        }
        for (size_t c=0;c<copies.size();c++){ ap->keyframes.push_back(copies[c]);
            newSel.insert(DopeKeyId(mp.ownerKey, mp.propId, mp.compId, copies[c].frame)); }
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
            VertexAnimation* van = DopeVertexAnimOf(d.ownerKey); if (!van) continue;
            size_t a = 0; for (; a<vans.size(); a++) if (vans[a]==van) break;
            if (a==vans.size()){ vans.push_back(van); ovtx.push_back(""); ouv.push_back(""); onrm.push_back(""); }
            int cp = DopeVertexLayerOf(d.ownerKey);
            if (cp==0) ovtx[a]=d.ownerKey; else if (cp==1) ouv[a]=d.ownerKey; else onrm[a]=d.ownerKey;
        }
        for (size_t a=0;a<vans.size();a++){
            VertexAnimation* van = vans[a];
            DopeDupVtxPre pre; pre.ownerKey = !ovtx[a].empty()?ovtx[a]:(!ouv[a].empty()?ouv[a]:onrm[a]); pre.an=van;
            VertexAnimSerialize(*van, pre.blob);
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
                if (mask[i]&1) newSel.insert(DopeKeyId(ovtx[a], AnimVertex, 0, src[i]+off));
                if (mask[i]&2) newSel.insert(DopeKeyId(ouv[a],  AnimVertex, 1, src[i]+off));
                if (mask[i]&4) newSel.insert(DopeKeyId(onrm[a], AnimVertex, 2, src[i]+off));
            }
            if (van->target) van->target->skinGeomVersion++;
        }
    }
    if (newSel.empty()){ g_dopeDupPre.clear(); g_dopeDupVtxPre.clear(); return; }
    g_dopeKeySel.swap(newSel);       // quedan elegidas las COPIAS: el 'g' las mueve a ellas
    BuildDopeRows();                 // las filas tienen que ver los keyframes nuevos antes de snapshotear el move
    g_dopeDup = true;
    DopeMoveStart(DOPE_MOV);
    if (!g_dopeMov){ g_dopeDup = false; g_dopeDupPre.clear(); g_dopeDupVtxPre.clear(); } // no arranco: no dejar el flag colgado
    InvalidateAnimAndRedraw();
}

// ---- 'g': mover los keyframes seleccionados en el TIEMPO (izq/der). Click/Enter acepta, Esc cancela. ----
bool Timeline::DopeMoving() const { return g_dopeMov; }
void Timeline::DopeSetPivot(int p){ g_dopePivot = (p>=DOPE_PIV_CENTER && p<=DOPE_PIV_ACTIVE) ? p : DOPE_PIV_CENTER; }
// ---- EL PIVOTE de los keyframes elegidos segun el menu Pivot (lo comparten el G/R/S y el gizmo 2D) ----
//   Center          = el centro de la caja de los elegidos (en tiempo y en valor)
//   Median Point    = el promedio de los elegidos
//   Current Frame   = el frame actual (el valor: el centro de la caja; "frame actual" solo habla del tiempo)
//   Active Keyframe = el keyframe activo (el de la tarjeta), si esta entre los elegidos; si no, el centro
struct DopePivAccum {
    int mn, mx; float vmn, vmx; double sf, sv; int n; bool hasAct; float actF, actV;
    DopePivAccum() : mn(0x7fffffff), mx(-0x7fffffff), vmn(1e30f), vmx(-1e30f), sf(0.0), sv(0.0), n(0),
                    hasAct(false), actF(0.0f), actV(0.0f) {}
    void Add(int f, float v){ if (f<mn) mn=f; if (f>mx) mx=f; if (v<vmn) vmn=v; if (v>vmx) vmx=v; sf+=f; sv+=v; n++; }
    void Active(const std::string& owner, int prop, int comp, int f, float v){
        if (g_dopeActHas && g_dopeActOwner==owner && g_dopeActProp==prop && g_dopeActComp==comp && g_dopeActFrame==f){
            hasAct = true; actF = (float)f; actV = v; } }
};
static void DopePivResolve(const DopePivAccum& a, float& pf, float& pv){
    pf = (a.mn<=a.mx) ? (float)(a.mn+a.mx)*0.5f : (float)CurrentFrame;
    pv = (a.vmn<=a.vmx) ? (a.vmn+a.vmx)*0.5f : 0.0f;
    if (g_dopePivot == DOPE_PIV_CURFRAME) pf = (float)CurrentFrame;
    else if (g_dopePivot == DOPE_PIV_MEDIAN && a.n > 0){ pf = (float)(a.sf / a.n); pv = (float)(a.sv / a.n); }
    else if (g_dopePivot == DOPE_PIV_ACTIVE && a.hasAct){ pf = a.actF; pv = a.actV; }
}
// el pivote de la seleccion de AHORA (el gizmo lo dibuja ahi antes de arrancar nada). false = no hay keyframes elegidos
static bool DopePivotLive(const Timeline* t, float& pf, float& pv){
    if (g_dopeKeySel.empty()) return false;
    DopePivAccum a;
    std::set<std::string> views;
    for (size_t r=0;r<t->dopeRows.size();r++){ const Timeline::DopeRow& d = t->dopeRows[r];
        if (d.propId<0) continue;
        char key[32]; snprintf(key, sizeof key, "|%d|%d", d.propId, d.compId);
        if (!views.insert(d.ownerKey + key).second) continue;
        AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId, false); if (!ap) continue;
        for (size_t k=0;k<ap->keyframes.size();k++){
            const keyFrame& kf = ap->keyframes[k];
            if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, kf.frame))) continue;
            a.Add(kf.frame, kf.value); a.Active(d.ownerKey, d.propId, d.compId, kf.frame, kf.value);
        }
    }
    DopePivResolve(a, pf, pv);
    return true;
}
void Timeline::DopeMoveStart(int modeT){  // DOPE_MOV ('g') / DOPE_ESC ('s') / DOPE_ROT ('r', solo curvas)
    if (g_dopeKeySel.empty()) return;
    // Si habia un handle agarrado (click sostenido) y se apreta G/S/R, hay que soltarlo ANTES: sino queda prendido
    // y, al terminar el transform, el proximo movimiento del mouse seguiria arrastrandolo. Ademas cierra su undo,
    // que si no lo pisaria el UndoKeyframesStart de aca abajo.
    if (g_hOn) HandleRelease();
    g_dopeMovSnap.clear();
    // snapshot de CADA propiedad afectada (lista completa de keyframes) + que frames de esa prop estan seleccionados
    for (size_t r=0;r<dopeRows.size();r++){ const DopeRow& d=dopeRows[r]; if (d.propId<0) continue;
        bool already=false; for (size_t s=0;s<g_dopeMovSnap.size();s++)
            if (g_dopeMovSnap[s].ownerKey==d.ownerKey && g_dopeMovSnap[s].propId==d.propId && g_dopeMovSnap[s].compId==d.compId){ already=true; break; }
        if (already) continue;
        AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId); if (!ap) continue;
        DopeMovProp mp; mp.ownerKey=d.ownerKey; mp.propId=d.propId; mp.compId=d.compId; mp.orig=ap->keyframes;
        for (size_t k=0;k<ap->keyframes.size();k++)
            if (g_dopeKeySel.count(DopeKeyId(d.ownerKey,d.propId,d.compId,ap->keyframes[k].frame))) mp.selFrames.push_back(ap->keyframes[k].frame);
        mp.still.assign(mp.orig.size(), 0);
        for (size_t i=0;i<g_dopeOverwritten.size();i++){ const DopeMovProp& pz = g_dopeOverwritten[i];
            if (pz.ownerKey!=mp.ownerKey || pz.propId!=mp.propId || pz.compId!=mp.compId) continue;
            for (size_t j=0;j<pz.orig.size();j++){ mp.orig.push_back(pz.orig[j]); mp.still.push_back(1); } }
        if (!mp.selFrames.empty()) g_dopeMovSnap.push_back(mp);
    }
    g_dopeOverwritten.clear();   // (ya estan en la foto)
    // snapshot PARALELO de las filas VERTICES/UV (frames de vertices, fuente de verdad
    // aparte). UNO por anim: las dos filas comparten los VertexFrame — mover un cuadro
    // que trae ambas capas mueve el cuadro entero (es UN keyframe con dos capas).
    g_dopeMovVtx.clear();
    for (size_t r=0;r<dopeRows.size();r++){ const DopeRow& d=dopeRows[r];
        VertexAnimation* van = DopeVertexAnimOf(d.ownerKey); if (!van) continue;
        int layer = DopeVertexLayerOf(d.ownerKey);
        size_t yes = 0; for (; yes<g_dopeMovVtx.size(); yes++) if (g_dopeMovVtx[yes].an==van) break;
        if (yes == g_dopeMovVtx.size()){
            DopeMovVtxSnap sn; sn.an=van;
            for (size_t k=0;k<van->frames.size();k++){
                sn.vf.push_back(van->frames[k]); sn.origFrame.push_back(van->frames[k]->frame);
                sn.selPos.push_back(0); sn.selUV.push_back(0); sn.selNrm.push_back(0);
            }
            // MASCARA de la seleccion de Edit Mode: se congela ACA (al arrancar el transform), no al confirmar -
            // asi el criterio no cambia en el medio del arrastre. Solo si el toggle esta prendido y los frames
            // estan armados para este conteo de vertices (si la topologia cambio, mascara y frames no se
            // corresponden -> move de siempre).
            sn.selective = g_dopeOnlySelVerts && van->target && van->vcount == van->target->vertexSize &&
                           DopeMaskVertsSel(van->target, sn.mask);
            g_dopeMovVtx.push_back(sn);
        }
        DopeMovVtxSnap& sn = g_dopeMovVtx[yes];
        if (layer==0) sn.ownerVtx = d.ownerKey; else if (layer==1) sn.ownerUV = d.ownerKey; else sn.ownerNrm = d.ownerKey;
        for (size_t k=0;k<sn.vf.size();k++){
            if (!VertexFrameInLayer(sn.vf[k], layer)) continue;
            if (!g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, sn.origFrame[k]))) continue;
            if (layer==0) sn.selPos[k]=1; else if (layer==1) sn.selUV[k]=1; else sn.selNrm[k]=1;
        }
    }
    // tirar los snapshots sin NADA seleccionado (la fila estaba pero sin keyframes elegidos)
    for (size_t s=g_dopeMovVtx.size(); s-- > 0; ){
        bool has=false;
        for (size_t k=0;k<g_dopeMovVtx[s].vf.size() && !has;k++)
            has = g_dopeMovVtx[s].selPos[k] || g_dopeMovVtx[s].selUV[k] || g_dopeMovVtx[s].selNrm[k];
        if (!has) g_dopeMovVtx.erase(g_dopeMovVtx.begin()+s);
    }
    if (g_dopeMovSnap.empty() && g_dopeMovVtx.empty()) return;
    // UN solo keyframe + curvas + rotar/escalar -> el transform es de sus HANDLES
    g_dopeHXform = false;
    if (mode == TL_MODE_CURVES && (modeT == DOPE_ROT || modeT == DOPE_ESC) && g_dopeKeySel.size() == 1){
        for (size_t s2=0; s2<g_dopeMovSnap.size() && !g_dopeHXform; s2++){
            DopeMovProp& mp = g_dopeMovSnap[s2];
            if (mp.selFrames.size() != 1) continue;
            AnimProperty* ap = DopeResolveProp(mp.ownerKey, mp.propId, mp.compId); if (!ap) continue;
            for (size_t k=0;k<ap->keyframes.size();k++){
                if (ap->keyframes[k].frame != mp.selFrames[0]) continue;
                if (ap->keyframes[k].Interpolation != KfBezier &&
                    !(k > 0 && ap->keyframes[k-1].Interpolation == KfBezier)) break; // sin handles: nada que tocar
                HandleBake(ap, k);                       // congelar lo que se ve: los calculados no se pueden girar
                g_dopeHXform = true;
                g_dopeHOwner = mp.ownerKey; g_dopeHProp = mp.propId; g_dopeHComp = mp.compId;
                g_dopeHFrame = ap->keyframes[k].frame; g_dopeHOrig = ap->keyframes[k];
                break;
            }
        }
    }
    // PIVOTE segun el menu Pivot (Center / Current Frame / Median Point / Active Keyframe; ver DopePivResolve),
    // con los keyframes elegidos de la foto. (Los PISADOS que se agregaron a la foto no cuentan: estan tapados)
    { DopePivAccum a;
      for (size_t s=0;s<g_dopeMovSnap.size();s++){ DopeMovProp& mp=g_dopeMovSnap[s];
          for (size_t k=0;k<mp.orig.size();k++){
              if (k < mp.still.size() && mp.still[k]) continue;
              bool sel=false; for (size_t i=0;i<mp.selFrames.size();i++) if (mp.selFrames[i]==mp.orig[k].frame){ sel=true; break; }
              if (!sel) continue;
              a.Add(mp.orig[k].frame, mp.orig[k].value);
              a.Active(mp.ownerKey, mp.propId, mp.compId, mp.orig[k].frame, mp.orig[k].value); } }
      DopePivResolve(a, g_dopePivotF, g_dopePivotV); }
    // Ctrl+Z: snapshot de las curvas ANTES de transformar. Si el move viene de un Shift+D, el snapshot YA lo tomo
    // DopeDuplicateSelection antes de crear las copias -> no pisarlo, o el undo se comeria solo el movimiento y
    // dejaria las copias tiradas.
    if (!g_dopeDup) UndoKeyframesStart();
    g_dopeMov = true; g_dopeMovMode = modeT; g_dopeMovAxis = DOPE_AXIS_FREE;
    g_dopeEscXY = false;
    g_dopeMovVal = (modeT==DOPE_ESC) ? 1.0f : 0.0f;   // escalar arranca en x1; mover en 0; rotar en 0 grados
    g_dopeMovValV = 0.0f;
    g_dopeMovX0 = lastMx; g_dopeMovY0 = lastMy;
    g_dopeVirtX = lastMx; g_dopeVirtY = lastMy;       // el cursor virtual arranca pegado al real
    // ...y lastMouseX/Y (los GLOBALES de donde CheckWarpMouseInViewport saca el dx) tienen que apuntar al mismo
    // lado. Solo los refrescan el click y el propio CheckWarp, y ninguno corre mientras hoveras: sin esto el
    // primer dx sale (donde estas - ULTIMO CLICK), o sea toda la distancia que hoveraste, y el cursor virtual se
    // la come -> el transform arranca corrido esa distancia, para siempre. Va ACA y no en la tecla porque por aca
    // pasan TODOS los caminos (g/s/r, el menu Key y Shift+D).
    lastMouseX = lastMx; lastMouseY = lastMy;
    g_dopeVirtFirst = true;                         // el dx de este evento todavia es de ANTES del transform
    // ROTAR: angulo inicial mouse->pivot EN PIXELES (los dos ejes tienen unidades y zoom distintos: el giro solo
    // tiene sentido en pantalla, que es donde el usuario lo ve)
    if (modeT == DOPE_ROT){
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
void Timeline::HandleXformApply(){
    AnimProperty* ap = DopeResolveProp(g_dopeHOwner, g_dopeHProp, g_dopeHComp); if (!ap) return;
    size_t i = 0; bool has = false;
    for (size_t k=0;k<ap->keyframes.size();k++) if (ap->keyframes[k].frame == g_dopeHFrame){ i=k; has=true; break; }
    if (!has) return;
    keyFrame& kf = ap->keyframes[i];
    const float ang = (g_dopeMovMode == DOPE_ROT) ? (g_dopeMovVal * 3.14159265f / 180.0f) : 0.0f;
    const float ca = cosf(ang), sa = sinf(ang);
    // escalar: por eje (con el eje Y elegido -la manija Y del gizmo, 'S Y'- solo se aplastan en valor)
    const bool axisX = (g_dopeMovAxis != DOPE_AXIS_Y), axisY = (g_dopeMovAxis != DOPE_AXIS_X);
    const float escX = (g_dopeMovMode == DOPE_ESC && axisX) ? DopeEscX() : 1.0f;
    const float escY = (g_dopeMovMode == DOPE_ESC && axisY) ? DopeEscY() : 1.0f;
    for (int side=0; side<2; side++){
        float dF = side ? g_dopeHOrig.outDF : g_dopeHOrig.inDF;   // SIEMPRE desde el original: absoluto, sin drift
        float dV = side ? g_dopeHOrig.outDV : g_dopeHOrig.inDV;
        float px = dF * pxPerFrame, py = -dV * pxPerUnit;         // a pixeles (Y de pantalla crece hacia abajo)
        float rx = px*ca - py*sa, ry = px*sa + py*ca;             // rotar
        rx *= escX; ry *= escY;                                   // escalar (alarga la distancia; por eje)
        float nF = rx / (pxPerFrame>0.01f?pxPerFrame:0.01f);
        float nV = -ry / (pxPerUnit>1e-6f?pxPerUnit:1e-6f);
        // el handle no puede cruzar al otro lado del keyframe: x dejaria de ser monotono y un frame tendria dos
        // valores. Aca solo se frena en el eje del tiempo; el valor gira libre.
        const float minDF = 0.05f;
        if (side){ if (nF <  minDF) nF =  minDF; } else { if (nF > -minDF) nF = -minDF; }
        if (side){ kf.outDF = nF; kf.outDV = nV; } else { kf.inDF = nF; kf.inDV = nV; }
    }
    InvalidateAnimAndRedraw();
}

void Timeline::DopeXformApply(){
    if (!g_dopeMov) return;
    if (g_dopeHXform){ HandleXformApply(); return; }   // el transform es de los HANDLES, no de los keyframes
    std::set<std::string> newSel;
    for (size_t s=0;s<g_dopeMovSnap.size();s++){
        DopeMovProp& mp = g_dopeMovSnap[s];
        AnimProperty* ap = DopeResolveProp(mp.ownerKey, mp.propId, mp.compId); if (!ap) continue;
        // 1) los MOVIDOS: frame nuevo desde el ORIGINAL (absoluto -> sin drift al arrastrar)
        std::vector<keyFrame> moved; std::set<int> busy;
        for (size_t k=0;k<mp.orig.size();k++){
            if (!DopeMovSel(mp, k)) continue;
            float f0 = (float)mp.orig[k].frame, v0 = mp.orig[k].value;
            float fn = f0, vn = v0;
            const bool curves = (mode == TL_MODE_CURVES);
            const bool axisX = (g_dopeMovAxis != DOPE_AXIS_Y), axisY = (g_dopeMovAxis != DOPE_AXIS_X);
            if (g_dopeMovMode == DOPE_ESC){                       // ESCALAR desde el pivote
                if (axisX) fn = g_dopePivotF + (f0 - g_dopePivotF) * DopeEscX();
                if (curves && axisY) vn = g_dopePivotV + (v0 - g_dopePivotV) * DopeEscY();
            } else if (g_dopeMovMode == DOPE_ROT){                // ROTAR (solo curvas): en PIXELES, alrededor del pivote
                float px0 = (f0 - g_dopePivotF) * pxPerFrame;     // offset al pivote, en pantalla
                float py0 = -(v0 - g_dopePivotV) * pxPerUnit;     // (Y de pantalla crece hacia abajo)
                float a = g_dopeMovVal * 3.14159265f / 180.0f;
                float ca = cosf(a), sa = sinf(a);
                float px1 = px0*ca - py0*sa, py1 = px0*sa + py0*ca;
                fn = g_dopePivotF + px1 / (pxPerFrame>0.01f?pxPerFrame:0.01f);
                vn = g_dopePivotV - py1 / (pxPerUnit>1e-6f?pxPerUnit:1e-6f);
            } else {                                              // MOVER
                if (axisX) fn = f0 + g_dopeMovVal;
                if (curves && axisY) vn = v0 + g_dopeMovValV;
            }
            keyFrame kf = mp.orig[k]; kf.frame = (int)floorf(fn + 0.5f);                    // frames ENTEROS
            if (curves) kf.value = vn;                            // el VALOR es continuo: no se redondea
            // ESCALAR tambien escala los HANDLES (los que se guardan: los calculados se recalculan solos): escalar en
            // Y a cero aplana la curva entera, handles incluidos. El brazo no cruza al otro lado del keyframe.
            if (g_dopeMovMode == DOPE_ESC){
                const float sF = axisX ? DopeEscX() : 1.0f, sV = (curves && axisY) ? DopeEscY() : 1.0f;
                const float minDF = 0.05f;
                kf.inDF  = -fabsf(kf.inDF * sF);  if (kf.inDF  > -minDF) kf.inDF  = -minDF;
                kf.outDF =  fabsf(kf.outDF * sF); if (kf.outDF <  minDF) kf.outDF =  minDF;
                kf.inDV *= sV; kf.outDV *= sV;
            }
            // al escalar hacia abajo dos keyframes pueden caer en el MISMO frame: gana el ultimo
            for (size_t j=0;j<moved.size();j++) if (moved[j].frame == kf.frame){ moved.erase(moved.begin()+j); break; }
            moved.push_back(kf); busy.insert(kf.frame);
        }
        // 2) los QUIETOS, salvo los que quedaron pisados por un movido
        ap->keyframes.clear();
        for (size_t k=0;k<mp.orig.size();k++){
            if (DopeMovSel(mp, k) || busy.count(mp.orig[k].frame)) continue;   // (un quieto tapado no se ve: se pisa si se suelta ahi)
            ap->keyframes.push_back(mp.orig[k]);
        }
        for (size_t k=0;k<moved.size();k++){
            ap->keyframes.push_back(moved[k]);
            newSel.insert(DopeKeyId(mp.ownerKey, mp.propId, mp.compId, moved[k].frame));
        }
        // el keyframe ACTIVO se identifica por (curva, frame): si es uno de los que se movio, hay que seguirlo o
        // la tarjeta "Keyframe" del panel de propiedades desaparece apenas lo arrastras.
        if (g_dopeActHas && g_dopeActOwner==mp.ownerKey && g_dopeActProp==mp.propId && g_dopeActComp==mp.compId){
            for (size_t k=0;k<mp.selFrames.size();k++){
                if (mp.selFrames[k] != g_dopeActFrame) continue;
                // el que salio de ESE frame original quedo en movidos[k] (mismo orden que selFrames)
                if (k < moved.size()) g_dopeActFrame = moved[k].frame;
                break;
            }
        }
        ap->SortKeyFrames();
    }
    // filas VERTICES/UV: solo MOVER en el tiempo (escalar/rotar el timing de vertices es
    // raro). Reasigna ->frame desde el original + delta (absoluto, sin drift) y re-ordena
    // SIN borrar (los punteros del snapshot siguen vivos). Las colisiones se saldan al
    // confirmar. Un frame se mueve si estaba elegido en CUALQUIERA de sus filas.
    if (g_dopeMovMode == DOPE_MOV){
        int df = (int)floorf(g_dopeMovVal + 0.5f);
        for (size_t s=0;s<g_dopeMovVtx.size();s++){
            DopeMovVtxSnap& sn = g_dopeMovVtx[s];
            for (size_t i=0;i<sn.vf.size();i++)
                sn.vf[i]->frame = (sn.selPos[i] || sn.selUV[i] || sn.selNrm[i]) ? sn.origFrame[i] + df : sn.origFrame[i];
            VertexAnimSort(*sn.an);
            for (size_t i=0;i<sn.vf.size();i++){
                if (sn.selPos[i] && !sn.ownerVtx.empty())
                    newSel.insert(DopeKeyId(sn.ownerVtx, AnimVertex, 0, sn.vf[i]->frame));
                if (sn.selUV[i] && !sn.ownerUV.empty())
                    newSel.insert(DopeKeyId(sn.ownerUV, AnimVertex, 1, sn.vf[i]->frame));
                if (sn.selNrm[i] && !sn.ownerNrm.empty())
                    newSel.insert(DopeKeyId(sn.ownerNrm, AnimVertex, 2, sn.vf[i]->frame));
            }
            if (sn.an->target) sn.an->target->skinGeomVersion++; // el frame actual pudo cambiar de pose
        }
    }
    g_dopeKeySel.swap(newSel);
    InvalidateAnimAndRedraw();
}
void Timeline::DopeMoveApply(int mx, int my){
    if (!g_dopeMov || g_dopeNumOn) return;             // tipeando un valor exacto: el mouse no interfiere
    const bool curves = (mode == TL_MODE_CURVES);
    if (g_dopeMovMode == DOPE_ESC){
        // ESCALAR: razon entre la distancia del mouse al PIVOTE ahora y la que habia al arrancar. En curvas se mide
        // en 2D (el pivote es un punto), en dope sheet solo en el eje del tiempo.
        float pxPiv = FrameToX(g_dopePivotF), pyPiv = ValueToY(g_dopePivotV);
        float ax = (float)(g_dopeMovX0 - x) - pxPiv, bx = (float)(mx - x) - pxPiv;
        float d0, d1;
        if (curves){
            float ay = (float)(g_dopeMovY0 - y) - pyPiv, by = (float)(my - y) - pyPiv;
            d0 = sqrtf(ax*ax + ay*ay); d1 = sqrtf(bx*bx + by*by);
        } else { d0 = ax; d1 = bx; }
        if (fabsf(d0) < 1.0f) d0 = (d0<0?-1.0f:1.0f);
        g_dopeMovVal = d1 / d0;
    } else if (g_dopeMovMode == DOPE_ROT){
        // ROTAR: angulo barrido por el mouse alrededor del pivote, en pantalla
        float pxp = (float)x + FrameToX(g_dopePivotF), pyp = (float)y + ValueToY(g_dopePivotV);
        float a = atan2f((float)my - pyp, (float)mx - pxp);
        g_dopeMovVal = (a - g_dopeRotAng0) * 180.0f / 3.14159265f;
        while (g_dopeMovVal > 180.0f) g_dopeMovVal -= 360.0f;
        while (g_dopeMovVal < -180.0f) g_dopeMovVal += 360.0f;
    } else {
        g_dopeMovVal  = (float)(mx - g_dopeMovX0) / (pxPerFrame>0.01f?pxPerFrame:0.01f);       // frames
        g_dopeMovValV = curves ? -(float)(my - g_dopeMovY0) / (pxPerUnit>1e-6f?pxPerUnit:1e-6f) // valor (Y invertida)
                               : 0.0f;
        // CTRL apretado: SOLO horizontal (tiempo) o SOLO vertical (valor), segun para donde se movio mas el mouse
        if (curves && LCtrlPressed && g_dopeMovAxis == DOPE_AXIS_FREE){
            if (abs(mx - g_dopeMovX0) >= abs(my - g_dopeMovY0)) g_dopeMovValV = 0.0f; else g_dopeMovVal = 0.0f;
        }
    }
    DopeXformApply();
}
// X / Y durante un transform de curvas: limita a un eje (apretar de nuevo el mismo eje libera)
void Timeline::DopeCycleAxis(int axis){
    if (!g_dopeMov || mode != TL_MODE_CURVES) return;
    g_dopeMovAxis = (g_dopeMovAxis == axis) ? DOPE_AXIS_FREE : axis;
    DopeXformApply();
}
// valor NUMERICO exacto: mover N frames (G 2) o escalar xN (S 2 -> lo que duraba 2 frames pasa a durar 4)
void Timeline::DopeNumApply(){
    if (!g_dopeMov) return;
    float v; if (!W3dEvalExpr(g_dopeNumBuf, v)) return;  // expresion incompleta: esperar mas teclas
    float val = g_dopeNumNeg ? -v : v;
    // MOVER limitado al eje Y (curvas): el numero es el VALOR. En cualquier otro caso son frames / factor / grados.
    if (g_dopeMovMode == DOPE_MOV && mode == TL_MODE_CURVES && g_dopeMovAxis == DOPE_AXIS_Y){
        g_dopeMovValV = val; g_dopeMovVal = 0.0f;
    } else {
        g_dopeMovVal = val;
        if (g_dopeMovMode == DOPE_MOV) g_dopeMovValV = 0.0f;
    }
    DopeXformApply();
}
// ============================================================================
// AJUSTAR LA ULTIMA OPERACION (PopUp/TimelineRedoPanel.cpp): al confirmar un mover / rotar / escalar de keyframes se
// guarda la FOTO del transform (las curvas antes, el pivote, el eje). Cambiar un valor en la tarjeta vuelve a aplicar
// el transform DESDE ESA FOTO (es absoluto: igual que mientras se arrastra) sobre el MISMO paso de undo, que guarda el
// antes y hace swap: queda un solo Ctrl+Z. Cancelar deshace ese paso. Las filas de vertices no se re-hacen (su
// movimiento se salda con colisiones al confirmar): con vertices no hay tarjeta.
// ============================================================================
static struct TLRedo {
    bool valid; Timeline* tl; const void* undo; const void* prev;
    std::vector<DopeMovProp> snap;
    int mode, axis, pivot; float val, valV, pivF, pivV;
    bool hx; std::string hOwner; int hProp, hComp, hFrame; keyFrame hOrig;
    std::set<std::string> sel; bool actHas; int actFrame;
    TLRedo() : valid(false), tl(NULL), undo(NULL), prev(NULL), mode(0), axis(0), pivot(0), val(0), valV(0), pivF(0), pivV(0),
               hx(false), hProp(-1), hComp(-1), hFrame(0), actHas(false), actFrame(0) {}
} g_tlRedo;
extern bool g_transformRedoAuto;   // (TransformRedoPanel.cpp) false = la tarjeta no se abre sola (las pruebas viejas)
extern void TimelineRedoOpen();    // (PopUp/TimelineRedoPanel.cpp)
extern void TimelineRedoClosePanel();
static bool g_tlRedoReapplying = false;

bool TimelineRedoRead(TimelineRedoOp& op){
    if (!g_tlRedo.valid || !g_tlRedo.tl) return false;
    op.mode = (g_tlRedo.mode == Timeline::DOPE_ROT) ? 1 : (g_tlRedo.mode == Timeline::DOPE_ESC) ? 2 : 0;
    op.curves = (g_tlRedo.tl->mode == Timeline::TL_MODE_CURVES);
    op.pivot  = g_tlRedo.pivot;
    const bool axisX = (g_tlRedo.axis != Timeline::DOPE_AXIS_Y), axisY = (g_tlRedo.axis != Timeline::DOPE_AXIS_X);
    op.frames = (op.mode == 0 && axisX) ? g_tlRedo.val : 0.0f;
    op.value  = (op.mode == 0 && axisY && op.curves) ? g_tlRedo.valV : 0.0f;
    op.angle  = (op.mode == 1) ? g_tlRedo.val : 0.0f;
    op.escX = (op.mode == 2 && axisX) ? g_tlRedo.val : 1.0f;
    op.escY = (op.mode == 2 && axisY && op.curves) ? g_tlRedo.val : 1.0f;
    return true;
}
Timeline* TimelineRedoView(){ return g_tlRedo.valid ? g_tlRedo.tl : NULL; }
void TimelineRedoForget(){ g_tlRedo.valid = false; g_tlRedo.snap.clear(); }
bool TimelineRedoReapply(const TimelineRedoOp& op){
    if (!g_tlRedo.valid || !g_tlRedo.tl) return false;
    if (UndoLast() != g_tlRedo.undo) { TimelineRedoClosePanel(); return false; }   // el historial cambio por otro lado
    Timeline* t = g_tlRedo.tl;
    g_tlRedoReapplying = true;
    g_dopeMovSnap = g_tlRedo.snap; g_dopeMovVtx.clear();
    g_dopeMov = true; g_dopeMovMode = g_tlRedo.mode; g_dopeMovAxis = Timeline::DOPE_AXIS_FREE;
    g_dopePivotF = g_tlRedo.pivF; g_dopePivotV = g_tlRedo.pivV;
    g_dopeHXform = g_tlRedo.hx; g_dopeHOwner = g_tlRedo.hOwner; g_dopeHProp = g_tlRedo.hProp; g_dopeHComp = g_tlRedo.hComp;
    g_dopeHFrame = g_tlRedo.hFrame; g_dopeHOrig = g_tlRedo.hOrig;
    g_dopeEscXY = false; g_dopeMovValV = 0.0f;
    if (g_tlRedo.mode == Timeline::DOPE_ESC){ g_dopeEscXY = true; g_dopeEscXv = op.escX; g_dopeEscYv = op.escY; g_dopeMovVal = op.escX; }
    else if (g_tlRedo.mode == Timeline::DOPE_ROT) g_dopeMovVal = op.angle;
    else { g_dopeMovVal = op.frames; g_dopeMovValV = op.value; }
    // un handle de UN keyframe: la foto del handle se restaura ANTES de re-aplicar (HandleXformApply parte de ella)
    g_dopeKeySel = g_tlRedo.sel;
    t->DopeXformApply();
    g_dopeMov = false; g_dopeMovMode = 0; g_dopeMovAxis = Timeline::DOPE_AXIS_FREE; g_dopeMovSnap.clear();
    g_dopeHXform = false; g_dopeEscXY = false;
    g_tlRedoReapplying = false;
    g_redraw = true;
    return true;
}
void TimelineRedoCancel(){
    if (g_tlRedo.valid && g_tlRedo.undo && UndoLast() == g_tlRedo.undo){ g_tlRedoReapplying = true; UndoUndo(); g_tlRedoReapplying = false; }
    g_tlRedo.valid = false; g_tlRedo.snap.clear();
    g_redraw = true;
}
bool TimelineRedoReapplying(){ return g_tlRedoReapplying; }

void Timeline::DopeMoveConfirm(){
    // LA FOTO para la tarjeta de ajustar (antes de que se borre); con filas de vertices no se puede re-hacer
    const bool redoOk = g_transformRedoAuto && !g_tlRedoReapplying && g_dopeMovVtx.empty() && !g_dopeMovSnap.empty() &&
                        !(g_dopeMovMode == DOPE_MOV && fabsf(g_dopeMovVal) < 1e-6f && fabsf(g_dopeMovValV) < 1e-6f);
    if (redoOk){
        g_tlRedo.valid = true; g_tlRedo.tl = this; g_tlRedo.snap = g_dopeMovSnap;
        g_tlRedo.mode = g_dopeMovMode; g_tlRedo.axis = g_dopeMovAxis; g_tlRedo.pivot = g_dopePivot;
        g_tlRedo.val = g_dopeMovVal; g_tlRedo.valV = g_dopeMovValV; g_tlRedo.pivF = g_dopePivotF; g_tlRedo.pivV = g_dopePivotV;
        g_tlRedo.hx = g_dopeHXform; g_tlRedo.hOwner = g_dopeHOwner; g_tlRedo.hProp = g_dopeHProp; g_tlRedo.hComp = g_dopeHComp;
        g_tlRedo.hFrame = g_dopeHFrame; g_tlRedo.hOrig = g_dopeHOrig;
        // la seleccion de ANTES del transform: los keyframes elegidos de la foto (re-aplicar la arma de nuevo)
        g_tlRedo.sel.clear();
        for (size_t s=0;s<g_dopeMovSnap.size();s++){ const DopeMovProp& mp = g_dopeMovSnap[s];
            for (size_t k=0;k<mp.selFrames.size();k++) g_tlRedo.sel.insert(DopeKeyId(mp.ownerKey, mp.propId, mp.compId, mp.selFrames[k])); }
        g_tlRedo.prev = UndoLast(); g_tlRedo.undo = NULL;
    } else if (!g_tlRedoReapplying) g_tlRedo.valid = false;
    // filas VERTICES/UV/NORMALES: los frames ya estan en su posicion final. Resolver COLISIONES: si
    // un keyframe movido cayo sobre uno QUIETO, gana el movido (el quieto se borra ENTERO,
    // con todas sus capas: dos cuadros no pueden compartir frame). Dos movidos no chocan
    // entre si (un MOVER preserva su separacion relativa).
    // ...SALVO con el toggle "Solo vertices seleccionados" (sn.selective): ahi el move es POR VERTICE
    // (VertexAnimMoveKeysSelective). Como el nucleo necesita el estado PREVIO al move para evaluar a los NO
    // seleccionados, primero se DESHACE la previsualizacion (cada frame vuelve a su cuadro original) y despues se
    // le pasa la lista origin->destination. Los VertexFrame movidos los libera el nucleo: despues de esto los
    // punteros de sn.vf ya no valen (por eso g_dopeMovVtx se limpia enseguida).
    for (size_t s=0;s<g_dopeMovVtx.size();s++){
        DopeMovVtxSnap& sn = g_dopeMovVtx[s]; if (!sn.an) continue;
        if (sn.selective){
            std::vector<int> orig, dest;
            for (size_t i=0;i<sn.vf.size();i++){
                if (!(sn.selPos[i] || sn.selUV[i] || sn.selNrm[i])) continue;
                if (sn.vf[i]->frame == sn.origFrame[i]) continue;      // no se movio: nada que hacer
                orig.push_back(sn.origFrame[i]); dest.push_back(sn.vf[i]->frame);
            }
            for (size_t i=0;i<sn.vf.size();i++) sn.vf[i]->frame = sn.origFrame[i];  // deshacer la preview
            VertexAnimSort(*sn.an);
            if (!orig.empty())
                VertexAnimMoveKeysSelective(*sn.an, orig, dest, sn.mask.empty()?NULL:&sn.mask[0]);
            if (sn.an->target) sn.an->target->skinGeomVersion++;
            continue;
        }
        std::set<VertexFrame*> moved; std::set<int> movedFrames;
        for (size_t i=0;i<sn.vf.size();i++) if (sn.selPos[i] || sn.selUV[i] || sn.selNrm[i]){ moved.insert(sn.vf[i]); movedFrames.insert(sn.vf[i]->frame); }
        std::vector<VertexFrame*>& F = sn.an->frames;
        for (size_t i=F.size(); i-- > 0; ){
            if (moved.count(F[i])) continue;                 // el movido se queda
            if (!movedFrames.count(F[i]->frame)) continue;   // no hay colision
            VertexFrame* f=F[i];
            delete[] (GLfloat*)f->positions; delete[] (GLbyte*)f->normals;
            delete[] (GLfloat*)f->uvs;   // (antes se fugaba la capa UV del pisado)
            delete f;
            F.erase(F.begin()+i);
        }
        VertexAnimSort(*sn.an); sn.an->frameCount = (int)F.size();
        if (sn.an->target) sn.an->target->skinGeomVersion++;
    }
    g_dopeMovVtx.clear();
    g_dopeMov=false; g_dopeMovMode=0; g_dopeMovAxis=DOPE_AXIS_FREE; g_dopeMovSnap.clear(); g_dopeHXform=false;
    ViewPortClickDown = false;   // el warp lo dejaba trabado para no perder el foco (ver controls.cpp)
    g_dopeDup=false; g_dopeDupPre.clear(); g_dopeDupVtxPre.clear();   // si venia de Shift+D, la duplicacion queda firme
    g_dopeNumOn=false; g_dopeNumBuf.clear(); g_dopeEscXY=false;
    UndoKeyframesConfirm(); g_redraw=true;
    // dejo un paso de undo: la tarjeta para ajustarlo (si no cambio nada, no hay nada que ajustar)
    if (redoOk){
        const void* u = UndoLast();
        if (u && u != g_tlRedo.prev){ g_tlRedo.undo = u; TimelineRedoOpen(); }
        else g_tlRedo.valid = false;
    }
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
    else DopeNumApply();
    g_redraw = true; return true;
}
// ---- View > Frame Selected: encuadra el timeline en los keyframes SELECCIONADOS ----
// Varios keyframes: el primero pegado al borde IZQUIERDO y el ultimo al DERECHO (ajusta zoom + paneo).
// Uno solo: lo pone en el MEDIO del timeline y NO toca el zoom.
// primer y ultimo frame con keyframe SELECCIONADO. false = no hay ninguno.
bool Timeline::DopeSelectionRange(int& mn, int& mx) const {
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
//   1) las filas de VERTEX ANIM (Vertices/UV/Normales) NO son AnimProperty: DopeResolveProp devuelve NULL para
//      ellas, asi que el encuadre no encontraba ni un keyframe y se iba sin hacer nada (por eso "no funcionaba"
//      justo en las curvas de vertex anim, que son las filas nuevas);
//   2) sin NADA seleccionado no habia rango y tampoco pasaba nada.
//   3) los HANDLES bezier quedaban afuera (reporte del dueno: "el auto encuadre de las curvas enfoca bien la
//      curva, pero no enfoca bien los handlers que quedan fuera de la vista"). El rango salia de los KEYFRAMES,
//      asi que la curva entraba pero sus manijas -que es JUSTO lo que el usuario va a agarrar- se cortaban.
//      Que handle entra al rango: EL QUE SE DIBUJA, ni uno mas. Y eso son dos condiciones, las mismas dos que
//      manda RenderCurves: el keyframe tiene que estar SELECCIONADO (solo del seleccionado se dibujan handles)
//      y el tramo de ESE lado tiene que ser BEZIER (HandleVisible: una recta no tiene nada que ajustar). Donde
//      CAE el handle lo decide AnimProperty::EffectiveHandle, el mismo del core que usan la evaluacion y el
//      dibujo -> lo que se encuadra es exactamente lo que se ve, tambien con los handles CALCULADOS
//      (HVector/HAuto/HAutoClamped), que no estan guardados en ningun lado.
//      NO se replica el piso EN PIXELES de HandlePos (el que estira a 14px un handle diminuto para poder
//      agarrarlo): eso depende del zoom y el zoom es justo lo que estamos por calcular -- seria circular. Un
//      handle asi de corto entra igual, porque su keyframe entra.
// onlySel=true mide solo los keyframes SELECCIONADOS; false, las curvas visibles ENTERAS.
bool Timeline::CurveViewRange(bool onlySel, float& fmn, float& fmx, float& vmn, float& vmx) const {
    fmn = 1e30f; fmx = -1e30f; vmn = 1e30f; vmx = -1e30f;
    bool has = false;
    // rango que ocupan los HANDLES, medido APARTE: se mezcla al final y con tope (ver el paso 3)
    float hfmn = 1e30f, hfmx = -1e30f, hvmn = 1e30f, hvmx = -1e30f;
    bool hasH = false;
    AnimProperty tmp;   // curva temporal de una fila de vertex anim (LOCAL: cada fila arma la suya)
    // 1) rango de TIEMPO + los valores de los propios keyframes (y de paso, el de sus handles dibujados)
    for (size_t r=0;r<dopeRows.size();r++){
        const DopeRow& d = dopeRows[r]; if (!RowIsCurveVisible(d)) continue;
        const AnimProperty* ap = VertexChannelTemp(d.ownerKey, tmp) ? &tmp
                               : DopeResolveProp(d.ownerKey, d.propId, d.compId, false);   // encuadrar solo mide
        if (!ap) continue;
        for (size_t i=0;i<ap->keyframes.size();i++){
            const keyFrame& kf = ap->keyframes[i];
            bool sel = g_dopeKeySel.count(DopeKeyId(d.ownerKey,d.propId,d.compId,kf.frame)) > 0;
            if (onlySel && !sel) continue;
            if ((float)kf.frame < fmn) fmn = (float)kf.frame;
            if ((float)kf.frame > fmx) fmx = (float)kf.frame;
            if (kf.value < vmn) vmn = kf.value;
            if (kf.value > vmx) vmx = kf.value;
            has = true;
            if (!sel) continue;                       // sin seleccionar no se le dibujan handles
            for (int side=0; side<2; side++){
                bool output = (side==1);
                if (!HandleVisible(ap, i, output)) continue;   // ese tramo no es bezier: no hay handle
                float dF, dV; ap->EffectiveHandle(i, output, dF, dV);
                float hf = (float)kf.frame + dF, hv = kf.value + dV;
                if (hf < hfmn) hfmn = hf;
                if (hf > hfmx) hfmx = hf;
                if (hv < hvmn) hvmn = hv;
                if (hv > hvmx) hvmx = hv;
                hasH = true;
            }
        }
    }
    if (!has) return false;
    // 2) el rango de VALOR NO se puede sacar solo de los keyframes: una bezier se PASA de ellos (sobrepico) y la
    //    curva saldria recortada. Se mide sobre LA CURVA, muestreada como se dibuja. El paso se acota a 512
    //    muestras: encuadrar una animacion de 10.000 frames no puede costar 10.000 EvalF por fila.
    float step = (fmx - fmn) / 512.0f; if (step < 1.0f) step = 1.0f;
    for (size_t r=0;r<dopeRows.size();r++){
        const DopeRow& d = dopeRows[r]; if (!RowIsCurveVisible(d)) continue;
        const AnimProperty* ap = VertexChannelTemp(d.ownerKey, tmp) ? &tmp
                               : DopeResolveProp(d.ownerKey, d.propId, d.compId, false);   // encuadrar solo mide
        if (!ap || ap->keyframes.empty()) continue;
        if (onlySel){   // con seleccion, solo aportan las filas que tienen algun keyframe elegido
            bool hasSel = false;
            for (size_t i=0;i<ap->keyframes.size() && !hasSel;i++)
                hasSel = g_dopeKeySel.count(DopeKeyId(d.ownerKey,d.propId,d.compId,ap->keyframes[i].frame)) > 0;
            if (!hasSel) continue;
        }
        for (float f = fmn; f <= fmx + 1e-3f; f += step){
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
    if (hasH){
        const float kExt = 2.0f;                     // cuanto puede crecer por lado, en veces el rango base
        const float kNoLimit = 1e30f;
        float limitF = (fmx - fmn > 1e-6f) ? (fmx - fmn) * kExt : kNoLimit;
        float limitV = (vmx - vmn > 1e-6f) ? (vmx - vmn) * kExt : kNoLimit;
        float lfmn = fmn - limitF, lfmx = fmx + limitF;   // hasta donde se puede estirar cada borde
        float lvmn = vmn - limitV, lvmx = vmx + limitV;
        if (hfmn < fmn) fmn = (hfmn > lfmn) ? hfmn : lfmn;
        if (hfmx > fmx) fmx = (hfmx < lfmx) ? hfmx : lfmx;
        if (hvmn < vmn) vmn = (hvmn > lvmn) ? hvmn : lvmn;
        if (hvmx > vmx) vmx = (hvmx < lvmx) ? hvmx : lvmx;
    }
    return true;
}
// aplica un rango (frames, valores) al zoom/paneo de los DOS ejes.
void Timeline::CurveFrame(float fmn, float fmx, float vmn, float vmx){
    // la barra de herramientas de abajo (aparece con keyframes elegidos) TAPA el fondo del strip: el encuadre
    // usa solo lo que queda visible arriba de ella
    const int tbH = (!ToolButtons.empty() && ToolbarVisible()) ? ToolbarHeight() : 0;
    float visW = (float)(width - panelW), visH = (float)(height - stripY - tbH);
    if (visW < 1.0f || visH < 1.0f) return;
    const float margin = (float)GlobalScale * 12.0f;
    // --- eje TIEMPO: un solo frame -> al MEDIO y sin tocar el zoom (no hay ancho que encuadrar) ---
    if (fmx - fmn < 0.5f) viewStartF = fmn - (visW*0.5f)/(pxPerFrame>0.01f?pxPerFrame:0.01f);
    else {
        float util = visW - margin*2.0f; if (util < 1.0f) util = 1.0f;
        pxPerFrame = ClampZoomH(util / (fmx - fmn));
        viewStartF = fmn - margin / pxPerFrame;
    }
    // --- eje VALOR: la vista se CENTRA en el medio de los valores (encuadrar es justamente mover el centro) ---
    viewCenterV = (vmn + vmx) * 0.5f;
    float spanV = vmx - vmn;
    // rango degenerado (UN solo key, o una curva chata: dos keys con el mismo valor): sin un margen MINIMO el
    // zoom se iria al techo y no se veria mas que una linea gorda. Se abre 1 unidad alrededor del valor -
    // suficiente para una curva de vertex anim (valores 0..1, indice de pose) y razonable para una de objeto.
    if (spanV < 1e-6f) spanV = 1.0f;
    float usefulV = visH - margin*2.0f; if (usefulV < 1.0f) usefulV = 1.0f;
    pxPerUnit = ClampZoomV(usefulV / spanV);
    // ValueToY centra en el medio del strip ENTERO: se corre el centro para que el medio de los valores caiga
    // en el medio de la parte visible (arriba de la barra)
    viewCenterV -= (float)(CenterVertical() - (stripY + height - tbH) / 2) / pxPerUnit;
    g_redraw = true;
}
// En CURVAS encuadra los DOS EJES: el tiempo y el VALOR (para eso el zoom es de 2 ejes).
// Con keyframes SELECCIONADOS encuadra la SELECCION; sin seleccion, la(s) curve(s) visible(s) entera(s).
void Timeline::CurveFrameSelected(){
    float fmn, fmx, vmn, vmx;
    if (!CurveViewRange(true, fmn, fmx, vmn, vmx) &&
        !CurveViewRange(false, fmn, fmx, vmn, vmx)) return;   // ni seleccion ni curvas: nada que encuadrar
    CurveFrame(fmn, fmx, vmn, vmx);
}
// El "Frame Selected" del DOPE SHEET no tiene el problema de los handles: ahi no se dibujan (una fila del dope
// son ROMBOS en su frame, no hay eje de valor ni manijas que agarrar), asi que el unico rango que existe es el de
// los frames. El de CURVAS -sea auto-encuadre, sea "enfocar lo seleccionado"- pasa por CurveViewRange, que es
// donde entran los handles: los dos caminos comparten esa funcion y no pueden divergir.
void Timeline::DopeFrameSelected(){
    if (mode == TL_MODE_CURVES){ CurveFrameSelected(); return; }
    int mn, mx;
    if (!DopeSelectionRange(mn, mx)) return;      // sin keyframes seleccionados: no hay nada que encuadrar
    float visW = (float)(width - panelW);         // ancho UTIL (el panel no muestra frames)
    if (visW < 1.0f) return;
    if (mn == mx){                                // uno solo: al medio, sin tocar el zoom
        viewStartF = (float)mn - (visW*0.5f) / (pxPerFrame>0.01f?pxPerFrame:0.01f);
    } else {
        float margin = (float)GlobalScale * 12.0f;               // aire para que el rombo del borde entre entero
        float util = visW - margin*2.0f; if (util < 1.0f) util = 1.0f;
        pxPerFrame = ClampZoomH(util / (float)(mx - mn));
        viewStartF = (float)mn - margin / pxPerFrame;
    }
    g_redraw = true;
}

// Entrada numerica del transform de keyframes. Gemela de NumInputChar (LayoutInput.cpp): cada plataforma le
// pasa los caracteres tipeados (PC: SDL_TEXTINPUT -> respeta el layout del teclado, parentesis incluidos;
// Symbian: su keypad). Devuelve false si no hay transform de keyframes en curso.
bool DopeNumInputChar(int c){
    return g_tlActive ? g_tlActive->DopeNumChar(c) : false;
}
// Hay un transform de keyframes en curso? Lo pregunta controls.cpp para ENVOLVER el cursor dentro del timeline
// (como al panear): sin eso, al llegar al borde de la pantalla te quedas sin recorrido y no podes seguir escalando.
bool DopeXformActive(){ return g_dopeMov; }
// ---- EL TRANSFORM DE KEYFRAMES CON EL TECLADO DEL TELEFONO (flechas / inclinar), sin el cursor virtual. Con el cursor
// el escalar es la razon de distancias al pivote (cruzarlo daba la vuelta por cero: -inf..0..+inf) y arriba/abajo no
// llegaban. Aca: MOVER = izq/der el tiempo, arriba/abajo el valor (respeta el eje del 1 / 2); ESCALAR = multiplicar
// (derecha / arriba agranda, nunca pasa por cero); ROTAR = grados. dx/dy en PIXELES de pantalla (con la aceleracion
// del que llama)
int DopeXformMode(){ return !g_dopeMov ? 0 : (g_dopeMovMode == Timeline::DOPE_ESC ? 3 : g_dopeMovMode == Timeline::DOPE_ROT ? 2 : 1); }
void DopeXformKey(float dx, float dy){
    if (!g_dopeMov || !g_tlActive || g_dopeNumOn) return;
    Timeline* tl = g_tlActive;
    const bool curves = (tl->mode == Timeline::TL_MODE_CURVES);
    if (g_dopeMovMode == Timeline::DOPE_ESC){
        // LINEAL: pasa por 0 y sigue a negativo (invertir la curva: escalar a -1). Con el cursor era la razon de
        // distancias y no se podia cruzar; multiplicando, nunca llegaba a 0
        g_dopeMovVal += (dx - dy) * 0.005f;
    } else if (g_dopeMovMode == Timeline::DOPE_ROT){
        g_dopeMovVal += (dx - dy) * 0.5f;
    } else {
        if (g_dopeMovAxis != Timeline::DOPE_AXIS_Y) g_dopeMovVal += dx / (tl->pxPerFrame > 0.01f ? tl->pxPerFrame : 0.01f);
        if (curves && g_dopeMovAxis != Timeline::DOPE_AXIS_X) g_dopeMovValV -= dy / (tl->pxPerUnit > 1e-6f ? tl->pxPerUnit : 1e-6f);
    }
    tl->DopeXformApply();
    g_redraw = true;
}
// el angulo de una (rotar inclinando el telefono)
void DopeXformSetDegrees(float g){
    if (!g_dopeMov || !g_tlActive || g_dopeMovMode != Timeline::DOPE_ROT || g_dopeNumOn) return;
    g_dopeMovVal = g; g_tlActive->DopeXformApply(); g_redraw = true;
}
// 1 / 2 durante el transform: el eje X (tiempo) / Y (valor), como en la Vista 3D (de nuevo = libre)
void DopeXformAxisKey(int n){
    if (!g_dopeMov || !g_tlActive) return;
    if (n == 1) g_tlActive->DopeCycleAxis(Timeline::DOPE_AXIS_X);
    else if (n == 2) g_tlActive->DopeCycleAxis(Timeline::DOPE_AXIS_Y);
    g_redraw = true;
}
// barra de espacio: play/pausa desde CUALQUIER viewport (no hace falta tener el timeline enfocado)
bool TL_TogglePlay(){ if (!g_tlActive) return false; g_tlActive->TogglePlay(+1); return true; }
// ---- lapiz (+flechas) de Symbian sobre el TIMELINE: navega KEYFRAMES. Devuelve false si el timeline no esta
// activo, para que el llamador siga con lo suyo (ciclar objetos/huesos).
bool DopeNavKey(int dir){
    if (!g_tlActive || !viewPortActive || !viewPortActive->isLeaf() || viewPortActive->ViewportKind() != 5) return false;
    // LAS TIRAS DE SONIDO: el lapiz recorre punta izquierda / el sonido entero / punta derecha (y el que sigue). Antes,
    // sin keyframes que recorrer, el lapiz caia al ciclado de OBJETOS y cambiaba el objeto activo
    if (dir == 0 && g_tlActive->StripNavPrefers()) return g_tlActive->StripSelAdvance(+1);
    if (dir == 0 && g_tlActive->StripHas() && g_tlActive->DopeNoKeysNav()) return true;   // (nada que recorrer: no cambia el objeto)
    switch (dir){
        case 0: return g_tlActive->DopeSelAdvance(1, false);  // lapiz solo: saca el actual y pasa al siguiente
        case 1: return g_tlActive->DopeSelAdvance(1, true);   // + derecha: mantiene y suma el siguiente
        case 2: return g_tlActive->DopeSelAdvance(-1, true);  // + izquierda: mantiene y suma el anterior
        case 3: return g_tlActive->DopeSelAllNav();           // + arriba: todos
        default: return g_tlActive->DopeSelNothingNav();      // + abajo: ninguno
    }
}

// ---- KEYFRAME ACTIVO: lo lee/escribe la tarjeta "Keyframe" del panel de propiedades ----
// Se devuelve la curva VIVA y el indice. El indice se resuelve por FRAME cada vez (no se cachea): el vector de
// keyframes se reordena en cuanto moves algo, asi que guardar un indice o un puntero seria colgarse.
AnimProperty* DopeActiveKeyframe(int* idx, bool edit){
    if (!g_dopeActHas || !g_tlActive) return NULL;
    // tiene que seguir SELECCIONADO y existir; si no, la tarjeta se va
    if (!g_dopeKeySel.count(DopeKeyId(g_dopeActOwner, g_dopeActProp, g_dopeActComp, g_dopeActFrame))){
        g_dopeActHas = false; return NULL;
    }
    AnimProperty* ap = DopeResolveProp(g_dopeActOwner, g_dopeActProp, g_dopeActComp, edit);
    if (!ap){ g_dopeActHas = false; return NULL; }
    for (size_t k=0;k<ap->keyframes.size();k++)
        if (ap->keyframes[k].frame == g_dopeActFrame){ if (idx) *idx = (int)k; return ap; }
    g_dopeActHas = false; return NULL;
}
// ---- SELECCION MULTIPLE para el panel de propiedades (tarjetas Curva y Keyframes) ----
// las curvas con algun keyframe elegido (las filas de vertices no: no son curvas). El orden es el del panel.
int DopeCurvesPicked(std::vector<AnimProperty*>& curves, std::vector<std::string>* names, bool edit){
    curves.clear(); if (names) names->clear();
    if (!g_tlActive || g_dopeKeySel.empty()) return 0;
    const std::vector<Timeline::DopeRow>& rows = g_tlActive->dopeRows;
    for (size_t r=0;r<rows.size();r++){
        const Timeline::DopeRow& d = rows[r]; if (d.propId < 0 || DopeVertexAnimOf(d.ownerKey)) continue;
        AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId, edit); if (!ap) continue;
        bool sel = false;
        for (size_t i=0;i<ap->keyframes.size() && !sel;i++)
            sel = g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, ap->keyframes.Frame(i))) > 0;
        if (!sel || std::find(curves.begin(), curves.end(), ap) != curves.end()) continue;
        curves.push_back(ap); if (names) names->push_back(d.name);
    }
    return (int)curves.size();
}
// los keyframes elegidos: (curva, indice) de cada uno
int DopeKeysPicked(std::vector<AnimProperty*>& curves, std::vector<int>& idx, bool edit){
    curves.clear(); idx.clear();
    std::vector<AnimProperty*> cs; DopeCurvesPicked(cs, NULL, edit);
    if (cs.empty()) return 0;
    const std::vector<Timeline::DopeRow>& rows = g_tlActive->dopeRows;
    for (size_t r=0;r<rows.size();r++){
        const Timeline::DopeRow& d = rows[r]; if (d.propId < 0 || DopeVertexAnimOf(d.ownerKey)) continue;
        AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId, edit); if (!ap) continue;
        if (std::find(cs.begin(), cs.end(), ap) == cs.end()) continue;
        bool already = false; for (size_t q=0;q<curves.size() && !already;q++) already = (curves[q] == ap);
        if (already) continue;   // (la misma curva en dos filas: una vez)
        for (size_t i=0;i<ap->keyframes.size();i++)
            if (g_dopeKeySel.count(DopeKeyId(d.ownerKey, d.propId, d.compId, ap->keyframes.Frame(i)))){ curves.push_back(ap); idx.push_back((int)i); }
    }
    return (int)idx.size();
}
// ---- EDITAR LA SELECCION desde el panel de propiedades (un solo paso de undo cada uno) ----
// el valor tipeado en la tarjeta Keyframes va a TODOS los keyframes elegidos
void DopeKeysSetValue(float v){
    if (!g_tlActive) return;
    std::vector<AnimProperty*> cs; std::vector<int> ix;
    if (!DopeKeysPicked(cs, ix, true)) return;
    UndoKeyframesStart();
    for (size_t q=0;q<ix.size();q++) cs[q]->keyframes[(size_t)ix[q]].value = v;
    UndoKeyframesConfirm(); InvalidateAnimAndRedraw();
}
// el FRAME tipeado: cada keyframe elegido va a ese frame (en su curva gana el que llega, como al arrastrarlo; dos
// elegidos de la misma curva quedan en uno). La seleccion y el activo los siguen.
void DopeKeysSetFrame(int nf){
    if (!g_tlActive) return;
    UndoKeyframesStart();
    std::set<AnimProperty*> done;
    const std::vector<Timeline::DopeRow>& rows = g_tlActive->dopeRows;
    for (size_t r=0;r<rows.size();r++){
        const Timeline::DopeRow& d = rows[r]; if (d.propId < 0 || DopeVertexAnimOf(d.ownerKey)) continue;
        AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId, true); if (!ap || done.count(ap)) continue;
        std::vector<keyFrame> picked, rest;
        for (size_t i=0;i<ap->keyframes.size();i++){
            const std::string id = DopeKeyId(d.ownerKey, d.propId, d.compId, ap->keyframes[i].frame);
            if (g_dopeKeySel.count(id)){ picked.push_back(ap->keyframes[i]); g_dopeKeySel.erase(id); }
            else rest.push_back(ap->keyframes[i]);
        }
        if (picked.empty()) continue;
        done.insert(ap);
        keyFrame k = picked[0]; k.frame = nf;
        std::vector<keyFrame> newValue;
        for (size_t i=0;i<rest.size();i++) if (rest[i].frame != nf) newValue.push_back(rest[i]);
        newValue.push_back(k);
        ap->keyframes = newValue;
        ap->SortKeyFrames();
        g_dopeKeySel.insert(DopeKeyId(d.ownerKey, d.propId, d.compId, nf));
        if (g_dopeActHas && g_dopeActOwner==d.ownerKey && g_dopeActProp==d.propId && g_dopeActComp==d.compId) g_dopeActFrame = nf;
    }
    UndoKeyframesConfirm(); InvalidateAnimAndRedraw();
}
// un HANDLE tipeado (lado 0 = izquierdo / 1 = derecho; eje 0 = X tiempo / 1 = Y valor) en los elegidos que lo guardan
void DopeKeysSetHandle(int side, int axis, float v){
    if (!g_tlActive) return;
    std::vector<AnimProperty*> cs; std::vector<int> ix;
    if (!DopeKeysPicked(cs, ix, true)) return;
    UndoKeyframesStart();
    for (size_t q=0;q<ix.size();q++){
        keyFrame& k = cs[q]->keyframes[(size_t)ix[q]];
        if (k.handleType != HFree && k.handleType != HAligned) continue;   // los calculados no guardan nada
        float& f = side ? (axis ? k.outDV : k.outDF) : (axis ? k.inDV : k.inDF);
        f = v;
    }
    UndoKeyframesConfirm(); InvalidateAnimAndRedraw();
}
// interpolacion / easing (100+) / tipo de handle de los keyframes elegidos (los del timeline: tambien las filas de
// vertices)
void DopeKeysSetInterp(int id){
    if (!g_tlActive) return;
    if (id >= 100) g_tlActive->SetEasingSel(id - 100); else g_tlActive->SetInterpolationSel(id);
}
void DopeKeysSetHandleType(int type){ if (g_tlActive) g_tlActive->SetHandleTypeSel(type); }
// ...y lo mismo para TODOS los keyframes de las curvas elegidas (la tarjeta Curva): la seleccion se amplia a las
// curvas enteras mientras dura y despues vuelve a ser la de antes
static void DopeWithWholeCurves(int that, int arg){
    if (!g_tlActive) return;
    const std::set<std::string> prev = g_dopeKeySel;
    const std::vector<Timeline::DopeRow>& rows = g_tlActive->dopeRows;
    std::vector<AnimProperty*> cs; DopeCurvesPicked(cs, NULL, true);
    for (size_t r=0;r<rows.size();r++){
        const Timeline::DopeRow& d = rows[r]; if (d.propId < 0 || DopeVertexAnimOf(d.ownerKey)) continue;
        AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId, true);
        if (!ap || std::find(cs.begin(), cs.end(), ap) == cs.end()) continue;
        for (size_t i=0;i<ap->keyframes.size();i++) g_dopeKeySel.insert(DopeKeyId(d.ownerKey, d.propId, d.compId, ap->keyframes.Frame(i)));
    }
    if (that == 0) DopeKeysSetInterp(arg); else DopeKeysSetHandleType(arg);
    g_dopeKeySel = prev;
}
void DopeCurvesSetInterp(int id){ DopeWithWholeCurves(0, id); }
void DopeCurvesSetHandleType(int type){ DopeWithWholeCurves(1, type); }

// LA curva activa (la de los modificadores de curva): la del keyframe activo; si no hay, la unica elegida
AnimProperty* DopeActiveCurve(std::string* name, bool edit){
    int ki; AnimProperty* ap = DopeActiveKeyframe(&ki, edit);
    if (ap){ if (name) *name = DopeActiveKeyframeChannel(); return ap; }
    std::vector<AnimProperty*> cs; std::vector<std::string> ns;
    if (DopeCurvesPicked(cs, &ns, edit) == 1){ if (name) *name = ns[0]; return cs[0]; }
    return NULL;
}
// nombre del canal del keyframe activo ("X Location", ...) para el titulo de la tarjeta
std::string DopeActiveKeyframeChannel(){
    if (!g_dopeActHas || !g_tlActive) return "";
    for (size_t r=0;r<g_tlActive->dopeRows.size();r++){
        const Timeline::DopeRow& d = g_tlActive->dopeRows[r];
        if (d.propId==g_dopeActProp && d.compId==g_dopeActComp && d.ownerKey==g_dopeActOwner) return d.name;
    }
    return "";
}
// el frame del keyframe activo cambio (lo movio la tarjeta): seguirlo, y arrastrar la seleccion con el
void DopeActiveKeyframeReFrame(int newFrame){
    if (!g_dopeActHas) return;
    g_dopeKeySel.erase(DopeKeyId(g_dopeActOwner, g_dopeActProp, g_dopeActComp, g_dopeActFrame));
    g_dopeActFrame = newFrame;
    g_dopeKeySel.insert(DopeKeyId(g_dopeActOwner, g_dopeActProp, g_dopeActComp, g_dopeActFrame));
}
// Cancela el transform de keyframes desde AFUERA. Lo llama el Tab (que cicla de viewport por encima del ruteo a
// los viewports): irse del timeline con un transform a medio hacer dejaba ViewPortClickDown trabado en true -> el
// foco quedaba congelado y no se podia volver a cambiar de viewport, y el transform seguia vivo y huerfano.
void DopeXformCancel(){ if (g_tlActive && g_dopeMov) g_tlActive->DopeMoveCancel(); if (g_tlActive && g_tlActive->StripActive()) g_tlActive->StripCancel(); }
// ...y aceptarlo. Lo llaman el click IZQUIERDO (PC) y el OK/Enter de Symbian, igual que el transform del 3D.
void DopeXformAccept(){ if (g_tlActive && g_dopeMov) g_tlActive->DopeMoveConfirm(); if (g_tlActive && g_tlActive->StripActive()) g_tlActive->StripConfirm(); }
// readout del transform de keyframes (lo dibuja Render arriba del strip)
std::string Timeline::DopeTextTransform() const {
    if (!g_dopeMov) return "";
    const char* op = T((g_dopeMovMode==DOPE_ESC) ? "Scale" : (g_dopeMovMode==DOPE_ROT) ? "Rotate" : "Move");
    const std::string uniS  = (g_dopeMovMode==DOPE_ESC) ? std::string("x") : std::string(" ") + T((g_dopeMovMode==DOPE_ROT) ? "deg" : "frames");
    const std::string axisS = (g_dopeMovAxis==DOPE_AXIS_X) ? std::string("  ") + T("[X: time]") : (g_dopeMovAxis==DOPE_AXIS_Y) ? std::string("  ") + T("[Y: value]") : std::string();
    const char* uni  = uniS.c_str();
    const char* axis = axisS.c_str();
    const char* piv  = T((g_dopePivot==DOPE_PIV_CURFRAME) ? "current frame" : (g_dopePivot==DOPE_PIV_MEDIAN) ? "median point" :
                        (g_dopePivot==DOPE_PIV_ACTIVE) ? "active keyframe" : "center");
    char b[256];
    if (g_dopeNumOn){
        float v; bool ok = W3dEvalExpr(g_dopeNumBuf, v);
        std::string expr = (g_dopeNumNeg?"-[":"[") + g_dopeNumBuf + "]";
        if (ok) snprintf(b, sizeof b, "%s: %s = %.2f%s%s", op, expr.c_str(), g_dopeNumNeg?-v:v, uni, axis);
        else    snprintf(b, sizeof b, "%s: %s = ?%s", op, expr.c_str(), axis);
    }
    else if (g_dopeMovMode==DOPE_ESC) snprintf(b, sizeof b, T("Scale: %.2fx  (pivot %s)%s"), g_dopeMovVal, piv, axis);
    else if (g_dopeMovMode==DOPE_ROT) snprintf(b, sizeof b, T("Rotate: %.1f deg  (pivot %s)"), g_dopeMovVal, piv);
    else if (mode==TL_MODE_CURVES)    snprintf(b, sizeof b, T("Move: %d frames, %.3f value%s"), (int)floorf(g_dopeMovVal+0.5f), g_dopeMovValV, axis);
    else                              snprintf(b, sizeof b, T("Move: %d frames"), (int)floorf(g_dopeMovVal+0.5f));
    return std::string(b);
}

// Lo llama ~Timeline. El estado del transform de keyframes y del handle es de ARCHIVO (no del objeto): si el
// timeline se va con algo a medio hacer hay que apagarlo, o queda prendido para el proximo (y ViewPortClickDown
// trabado dejaria el foco congelado en un viewport que ya no existe).
void Timeline::DestroyTimelineState(){
    if (g_tlActive != this) return;
    if (g_dopeMov) DopeMoveCancel();
    if (g_hOn) HandleRelease();
    ViewPortClickDown = false;
    g_tlActive = NULL;
}

void Timeline::DopeMoveCancel(){
    if (!g_dopeMov) return;
    std::set<std::string> old;
    if (g_dopeDup){
        // el move venia de un Shift+D: cancelar tiene que volver a ANTES de duplicar (borrar las copias), no solo
        // devolverlas a donde nacieron.
        for (size_t s=0;s<g_dopeDupPre.size();s++){
            DopeMovProp& mp = g_dopeDupPre[s];
            AnimProperty* ap = DopeResolveProp(mp.ownerKey, mp.propId, mp.compId); if (ap) ap->keyframes = mp.orig;
        }
        // fila VERTEX: restaurar el blob de ANTES de clonar (borra las copias y devuelve
        // lo que un clon hubiera pisado). Deserializar libera lo vivo y reconstruye.
        for (size_t s=0;s<g_dopeDupVtxPre.size();s++){
            DopeDupVtxPre& pre = g_dopeDupVtxPre[s]; if (!pre.an) continue;
            VertexAnimDeserialize(*pre.an, pre.blob.empty()?0:&pre.blob[0], pre.blob.size());
            if (pre.an->target) pre.an->target->skinGeomVersion++;
        }
        g_dopeKeySel.clear();
        g_dopeDup = false; g_dopeDupPre.clear(); g_dopeDupVtxPre.clear();
        g_dopeMov = false; g_dopeMovSnap.clear(); g_dopeMovVtx.clear(); g_dopeHXform=false;
        ViewPortClickDown = false;
        UndoKeyframesConfirm(); // cierra el snapshot pendiente (no hubo cambio -> se descarta solo)
        InvalidateAnimAndRedraw(); return;
    }
    for (size_t s=0;s<g_dopeMovSnap.size();s++){
        DopeMovProp& mp = g_dopeMovSnap[s];
        // mp.orig es de ANTES del transform: restaurarlo devuelve tambien los handles (el bakeo del arranque incluido)
        AnimProperty* ap = DopeResolveProp(mp.ownerKey, mp.propId, mp.compId);
        if (ap){
            // un PISADO (lo tapado por un Pegar) que comparte cuadro con lo elegido no vuelve: Esc deja lo pegado donde
            // se pego, pisando lo que habia (como antes de que existieran los pisados)
            ap->keyframes.clear();
            for (size_t k=0;k<mp.orig.size();k++){
                if (k < mp.still.size() && mp.still[k]){
                    bool covered=false; for (size_t i=0;i<mp.selFrames.size();i++) if (mp.selFrames[i]==mp.orig[k].frame){ covered=true; break; }
                    if (covered) continue;
                }
                ap->keyframes.push_back(mp.orig[k]);
            }
        }
        for (size_t i=0;i<mp.selFrames.size();i++) old.insert(DopeKeyId(mp.ownerKey, mp.propId, mp.compId, mp.selFrames[i]));
    }
    // filas VERTICES/UV: devolver cada frame a su valor original y re-ordenar (nada se borro)
    for (size_t s=0;s<g_dopeMovVtx.size();s++){
        DopeMovVtxSnap& sn = g_dopeMovVtx[s]; if (!sn.an) continue;
        for (size_t i=0;i<sn.vf.size();i++){
            sn.vf[i]->frame = sn.origFrame[i];
            if (sn.selPos[i] && !sn.ownerVtx.empty())
                old.insert(DopeKeyId(sn.ownerVtx, AnimVertex, 0, sn.origFrame[i]));
            if (sn.selUV[i] && !sn.ownerUV.empty())
                old.insert(DopeKeyId(sn.ownerUV, AnimVertex, 1, sn.origFrame[i]));
            if (sn.selNrm[i] && !sn.ownerNrm.empty())
                old.insert(DopeKeyId(sn.ownerNrm, AnimVertex, 2, sn.origFrame[i]));
        }
        VertexAnimSort(*sn.an);
        if (sn.an->target) sn.an->target->skinGeomVersion++;
    }
    g_dopeMovVtx.clear();
    g_dopeKeySel.swap(old);
    g_dopeMov=false; g_dopeMovMode=0; g_dopeMovAxis=DOPE_AXIS_FREE; g_dopeMovSnap.clear(); g_dopeHXform=false;
    g_dopeNumOn=false; g_dopeNumBuf.clear(); g_dopeNumNeg=false;
    ViewPortClickDown = false;
    UndoKeyframesConfirm(); // cierra el snapshot que abrio DopeMoveStart (sin cambios -> se descarta solo)
    InvalidateAnimAndRedraw();
}

// ------------------------------------------------------------------ acciones
void Timeline::TogglePlay(int dir){
    bool starts = !(PlayAnimation && AnimPlayDir==dir);
    if (starts && AnimIsGame && dir > 0) {
        // NO SE JUEGA CON TEXTURAS A MEDIO CARGAR: si la cola diferida tiene algo,
        // se vacia ACA (bloqueando, con cartel) antes de arrancar. Reporte del
        // dueno: "no se puede dar play hasta que carguen todas las texturas".
        extern bool GameWaitTextures();
        if (!GameWaitTextures()) { g_redraw = true; return; }
        // dar PLAY al juego: overlays off SOLO al INICIAR (sim aun no cargada) y
        // la vista de camara vuelve a su encuadre (la inspeccion de pausa se resetea)
        extern bool SimActive(); extern void GamePrepareViewports(bool);
        GamePrepareViewports(!SimActive());
    }
    if (!starts) PlayAnimation=false; else { PlayAnimation=true; AnimPlayDir=dir; }
    g_redraw = true;
}
void Timeline::GotoStart(){ CurrentFrame = StartFrame; g_redraw=true; }
void Timeline::GotoEnd(){
    // (modo juego: no hay Fin al que ir)
    if (!AnimIsGame) CurrentFrame = EndFrame;
    g_redraw=true;
}
// el cuadro anterior / siguiente desde CUALQUIER lado (las flechas del viewport 3D): lo mismo que el <| |> del timeline
void W3dStepFrame(int d);
void Timeline::StepFrame(int d){ W3dStepFrame(d); }
void W3dStepFrame(int d){
    // simulacion en PAUSA: > avanza el juego FRAME A FRAME (simula uno); < no hace nada: el juego no
    // graba su pasado
    { extern bool SimActive(); extern bool SimStep(int);
      if (SimActive() && !PlayAnimation && SimStep(d)) { g_redraw = true; return; } }
    CurrentFrame += d;
    // en PAUSA (animacion normal) se scrubbea LIBRE, tambien FUERA de inicio/fin y a
    // negativos: los keyframes pueden caer fuera del rango y hay que poder llegar a ellos.
    // Reproduciendo o en modo juego, si se clampea al rango.
    if (PlayAnimation || AnimIsGame){
        if (CurrentFrame < StartFrame) CurrentFrame = StartFrame;
        if (CurrentFrame > EndFrame && !AnimIsGame) CurrentFrame = EndFrame;
    }
    // "Juego" SIN partida: lo que hay es el estado base (su frame 1)
    { extern bool SimActive(); if (ActiveAnimKind == 2 && !SimActive()) CurrentFrame = StartFrame; }
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
int Timeline::CenterTimeline() const { return panelW + (width - panelW)/2; }
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
//  LA VISTA DEL TIMELINE VUELVE DE UN ARCHIVO (la escribe SaveW3D en la hoja del layout; ver alla el
//  comentario grande del reparto .w3d / config.start). Un .w3d se edita a mano y ADEMAS puede venir de una
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
void Timeline::ViewApply(float zoomF, float frameLeft, float zoomV, float centerV){
    const float kDefZoom  = (float)GlobalScale * 7.0f;   // los MISMOS del constructor
    const float kDefLeft  = -2.0f;
    const float kBoundF   = 1000000.0f;                  // cota de frames del lector (ver SessionApply)
    const float kBoundV   = 1e12f;                       // un valor de curva puede ser grande, pero no infinito
    pxPerFrame  = (W3dFinite(zoomF)   && zoomF > 0.0f)                 ? ClampZoomH(zoomF) : kDefZoom;
    viewStartF  = (W3dFinite(frameLeft)&& fabsf(frameLeft) <= kBoundF)    ? frameLeft          : kDefLeft;
    pxPerUnit   = (W3dFinite(zoomV)   && zoomV > 0.0f)                 ? ClampZoomV(zoomV) : kDefZoom;
    viewCenterV = (W3dFinite(centerV) && fabsf(centerV)  <= kBoundV)    ? centerV           : 0.0f;
}
void Timeline::SetFrameFromX(int lx){
    int f = (int)(XToFrame((float)lx) + 0.5f);
    // igual que StepFrame: en PAUSA (animacion normal) el playhead va LIBRE fuera del
    // rango; reproduciendo o en modo juego se clampea (el juego arranca en Start).
    if (PlayAnimation || AnimIsGame){
        if (f < StartFrame) f = StartFrame;
        if (f > EndFrame && !AnimIsGame) f = EndFrame;   // en modo juego no hay Fin
    }
    // "Juego" SIN partida: lo que hay es el estado base (su frame 1)
    { extern bool SimActive(); if (ActiveAnimKind == 2 && !SimActive()) f = StartFrame; }
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
void Timeline::EditField(int i){
    g_tlActive = this;
    PropFloat* pf = (i==0)?pfStart : (i==1)?pfEnd : (i==3)?pfFps : pfCur;
    if (i==0) fStart=(float)StartFrame; else if (i==1) fEnd=(float)EndFrame; else if (i==3) fFpsTL=(float)AnimFPS; else fCur=(float)CurrentFrame;
    pf->StartEditText();
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
void W3dTimelinePressedArrow(){ g_scrubAccum = 0.0f; g_scrubWait = 8; }
void Timeline::ScrubArrow(int dir){
    if (g_scrubWait > 0) { g_scrubWait--; return; }
    const float pxByTick = 3.3f;
    float px = (pxPerFrame > 0.01f) ? pxPerFrame : 0.01f;
    g_scrubAccum += (float)dir * pxByTick / px;      // en frames (con fraccion)
    int n = (int)g_scrubAccum;                       // trunca hacia el cero: la fraccion queda para el proximo tick
    if (n == 0) return;
    g_scrubAccum -= (float)n;
    for (int k = 0, q = (n < 0 ? -n : n); k < q; k++) StepFrame(n > 0 ? 1 : -1);
}

void Timeline::TransportAction(int i){
    // con una SIMULACION cargada (scripts lua), el boton "inicio" es el STOP del
    // juego: restaura el estado inicial y descarga los scripts
    { extern bool SimActive(); extern void SimStop();
      if (i == 0 && SimActive()) { SimStop(); PlayAnimation = false; g_redraw = true; return; } }
    switch (i){
        case 0: GotoStart(); break;      case 1: StepKeyframe(-1); break;
        case 2: StepFrame(-1); break;    case 3: TogglePlay(-1); break;
        case 4: TogglePlay(+1); break;   case 5: StepFrame(+1); break;
        case 6: StepKeyframe(+1); break; case 7: GotoEnd(); break;
    }
}


// ---- Aperturas de menu COMPARTIDAS por el boton de la barra y el atajo de teclado: los dos llaman aca, asi no
//      pueden divergir (era facil que el menu del boton tuviera opciones que el atajo no).
void Timeline::OpenMenuKey(int mx, int my){
    if (!g_tlMenuKey){
        g_tlMenuKey   = new PopupMenu(); g_tlMenuKey->action   = TL_menuKeyAction; 
        g_tlMenuKeyXf = new PopupMenu(); g_tlMenuKeyXf->action = TL_menuKeyAction; // los submenus HEREDAN la action
        
        g_tlMenuKeyIn = new PopupMenu(); g_tlMenuKeyIn->action = TL_menuKeyAction;
        
        g_tlMenuHandle = new PopupMenu(); g_tlMenuHandle->action = TL_menuKeyAction;
        
    }
    // se rearma en cada apertura: Rotate solo existe en CURVAS (en el dope sheet el unico eje es el tiempo, girar
    // no significa nada)
    g_tlMenuKeyXf->Clear();
    g_tlMenuKeyXf->Add(T("Move"), TL_KEY_MOVE)->shortcut = "@tl.mover";
    if (mode == TL_MODE_CURVES) g_tlMenuKeyXf->Add(T("Rotate"), TL_KEY_ROT)->shortcut = "@tl.rotar";
    g_tlMenuKeyXf->Add(T("Scale"), TL_KEY_SCALE)->shortcut = "@tl.escalar";
    BuildMenuInterp(g_tlMenuKeyIn);
    BuildMenuHandle(g_tlMenuHandle);
    g_tlMenuKey->Clear();
    g_tlMenuKey->title = T("Key");   // (el boton es el rombo, sin texto)
    g_tlMenuKey->Add(T("Transform"), -1, -1, g_tlMenuKeyXf);
    g_tlMenuKey->Add(T("Interpolation Mode"), -1, IconType::interp_bezier, g_tlMenuKeyIn)->shortcut = "@tl.interp";
    // el EASING de las curvas de easing (Ctrl+E): otro submenu
    { static PopupMenu* sEase = NULL;
      if (!sEase){ sEase = new PopupMenu(); sEase->action = TL_menuKeyAction; }
      W3dMenuEasings(sEase, TL_KEY_EASE0, -1);
      g_tlMenuKey->Add(T("Easing Type"), -1, IconType::ease_auto, sEase)->shortcut = "@tl.easing"; }
    // los handles solo existen en CURVAS: en el dope sheet no hay nada que ajustar
    if (mode == TL_MODE_CURVES) g_tlMenuKey->Add(T("Handle Type"), -1, -1, g_tlMenuHandle)->shortcut = "@tl.handle";
    g_tlMenuKey->Add(T("Smart Euler"), TL_KEY_SMARTEULER);
    g_tlMenuKey->Add(T("Cut"), TL_KEY_CUT, (int)IconType::scissors)->shortcut = "@tl.cortar";
    g_tlMenuKey->items.back()->lineUp = true;
    g_tlMenuKey->Add(T("Copy"), TL_KEY_COPY, (int)IconType::copy)->shortcut = "@tl.copiar";
    g_tlMenuKey->Add(T("Paste"), TL_KEY_PASTE, (int)IconType::paste)->shortcut = "@tl.pegar";
    g_tlMenuKey->Add(T("Duplicate"), TL_KEY_DUP)->shortcut = "@tl.duplicar";
    g_tlMenuKey->Add(T("Delete"), TL_KEY_OF)->shortcut = "@tl.borrar";
    if (OpenMenu && OpenMenu != g_tlMenuKey) OpenMenu->Close();
    g_tlMenuKey->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = g_tlMenuKey;
}
// (el menu Modifiers -la llave inglesa- con el Repetir de las curvas ya no esta: el Repetir es el modificador Ciclos,
//  en Propiedades > Modificadores. Tenerlo aca era la misma funcion dos veces)
// Interpolation Mode: la forma del tramo que SALE de cada keyframe seleccionado. Lo comparten el submenu del
// menu Key y la 't'.
static const char* const kNameInterp[KfInterpN] = { "Constant", "Linear", "Bezier",
    "Sinusoidal", "Quadratic", "Cubic", "Quartic", "Quintic", "Exponential", "Circular", "Back (overshoot)", "Bounce", "Elastic" };
static const int kIconInterp[KfInterpN] = { IconType::interp_constant, IconType::interp_linear, IconType::interp_bezier,
    IconType::interp_sine, IconType::interp_quad, IconType::interp_cubic, IconType::interp_quart, IconType::interp_quint,
    IconType::interp_expo, IconType::interp_circ, IconType::interp_back, IconType::interp_bounce, IconType::interp_elastic };
static const char* const kNameEase[4] = { "Automatic Easing", "Ease In", "Ease Out", "Ease In and Out" };
static const int kIconEase[4] = { IconType::ease_auto, IconType::ease_in, IconType::ease_out, IconType::ease_inout };
const char* W3dInterpolationName(int interp){
    const int b = KfInterpBase(interp); return T((b >= 0 && b < KfInterpN) ? kNameInterp[b] : "Linear");
}
const char* W3dNameEasing(int ease){ return T(kNameEase[ease & 3]); }
void W3dMenuInterpolations(PopupMenu* m, int idBase, int current){
    m->Clear();
    const int act = (current >= 0) ? KfInterpBase(current) : -1;
    for (int i = 0; i < KfInterpN; i++){
        MenuItem* it = m->Add(T(kNameInterp[i]), idBase + i, kIconInterp[i]);
        it->green = (i == act);
        // tres grupos: las de siempre | easing por fuerza (Sinusoidal..Circular) | con efecto (Back..Elastic)
        if (i == KfSine || i == KfBack) it->lineUp = true;
    }
}
void W3dMenuEasings(PopupMenu* m, int idBase, int current){
    m->Clear();
    for (int e = 0; e < 4; e++){ MenuItem* it = m->Add(T(kNameEase[e]), idBase + e, kIconEase[e]); it->green = (e == current); it->shortcut = "@tl.easing"; }
}
void Timeline::BuildMenuInterp(PopupMenu* m){
    W3dMenuInterpolations(m, TL_KEY_INTERP0, -1);
    for (size_t i = 0; i < m->items.size(); i++) m->items[i]->shortcut = "@tl.interp";
}
void Timeline::OpenMenuEasing(int mx, int my){
    if (!g_tlMenuEase){ g_tlMenuEase = new PopupMenu(); g_tlMenuEase->action = TL_menuKeyAction; }
    g_tlMenuEase->title = T("Set Keyframe Easing Type");
    W3dMenuEasings(g_tlMenuEase, TL_KEY_EASE0, -1);
    if (OpenMenu && OpenMenu != g_tlMenuEase) OpenMenu->Close();
    g_tlMenuEase->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = g_tlMenuEase;
}
// menu Select del dope sheet (All / None / Invert)
void Timeline::OpenMenuSelect(int mx, int my){
    if (!g_tlMenuSel){
        // (el boton es un icono: el menu lleva titulo, como el View)
        g_tlMenuSel = new PopupMenu(); g_tlMenuSel->title = T("Select"); g_tlMenuSel->action = TL_menuSelAction;
        g_tlMenuSel->Add(T("All"), 0)->shortcut = "@tl.seleccionarTodo";
        g_tlMenuSel->Add(T("None"), 1)->shortcut = "@tl.deseleccionar";
        g_tlMenuSel->Add(T("Invert"), 2)->shortcut = "@tl.invertir";
    }
    if (OpenMenu && OpenMenu != g_tlMenuSel) OpenMenu->Close();
    g_tlMenuSel->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = g_tlMenuSel;
}
// menu Pivot del dope sheet (Center / Current Frame): desde donde escala la 's'
void Timeline::OpenMenuPivot(int mx, int my){
    if (!g_tlMenuPivot){
        g_tlMenuPivot = new PopupMenu(); g_tlMenuPivot->action = TL_menuPivotAction;
        g_tlMenuPivot->title = T("Transform Pivot Point");
    }
    // se rearma cada vez: cada opcion con su ICONO y la ACTIVA en verde (igual que el Pivot Point del 3D)
    g_tlMenuPivot->Clear();
    g_tlMenuPivot->Add(T("Center"), DOPE_PIV_CENTER, (int)IconType::pivotMedian)->green = (g_dopePivot == DOPE_PIV_CENTER);
    g_tlMenuPivot->Add(T("Median Point"), DOPE_PIV_MEDIAN, (int)IconType::pivotIndividual)->green = (g_dopePivot == DOPE_PIV_MEDIAN);
    g_tlMenuPivot->Add(T("Current Frame"), DOPE_PIV_CURFRAME, (int)IconType::pivotCursor)->green = (g_dopePivot == DOPE_PIV_CURFRAME);
    g_tlMenuPivot->Add(T("Active Keyframe"), DOPE_PIV_ACTIVE, (int)IconType::pivotActive)->green = (g_dopePivot == DOPE_PIV_ACTIVE);
    if (OpenMenu && OpenMenu != g_tlMenuPivot) OpenMenu->Close();
    g_tlMenuPivot->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = g_tlMenuPivot;
}
// dropdown de animacion: menu jerarquico (submenu "Scenes" + un submenu por armadura con sus clips)
void Timeline::OpenMenuAnim(int mx, int my){
    if (!g_tlMenuAnim){ g_tlMenuAnim = new PopupMenu(); g_tlMenuAnim->action = TL_menuAnimAction; }
    BuildMenuAnim(g_tlMenuAnim);
    if (OpenMenu && OpenMenu != g_tlMenuAnim) OpenMenu->Close();
    g_tlMenuAnim->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = g_tlMenuAnim;
}
// menu View: Frame Selected + Auto frame (checkbox). Se rearma en cada apertura para que el checkbox lea el
// bool VIVO.
void Timeline::OpenMenuView(int mx, int my){
    if (!g_tlMenuView){
        // REGLA DE DISENO de los titulos: un menu que se abre desde algo SIN TEXTO (un icono, o un atajo de
        // teclado) lleva titulo -- es lo unico que te dice que estas mirando. Si lo abre un boton/item que YA
        // decia el texto, NO lleva: repetirlo es ruido. El boton View ahora es un icono -> titulo.
        g_tlMenuView = new PopupMenu(); g_tlMenuView->title = T("View"); g_tlMenuView->action = TL_menuViewAction; 
    }
    g_tlMenuView->Clear();
    // el MODO del editor: Dope Sheet (rombos por fila) o Curves (el grafico tiempo x valor). El activo en verde
    g_tlMenuView->Add(T("Dope Sheet"), TL_VIEW_DOPE, IconType::keyframe)->green = (mode != TL_MODE_CURVES);
    g_tlMenuView->Add(T("Curves"), TL_VIEW_CURVES, IconType::interp_bezier)->green = (mode == TL_MODE_CURVES);
    { MenuItem* it = g_tlMenuView->Add(T("Frame Selected"), TL_VIEW_FRAMESEL); it->shortcut = "@tl.encuadrar"; it->lineUp = true; }
    g_tlMenuView->AddCheck(T("Show Panel"), TL_VIEW_PANEL, &g_dopePanelOn);
    if (mode == TL_MODE_CURVES) g_tlMenuView->AddCheck(T("Show Handles"), TL_VIEW_HANDLES, &g_tlShowHandles);
    g_tlMenuView->AddCheck(T("Auto frame"), TL_VIEW_AUTOFRAME, &g_dopeAutoFrame);
    // el GIZMO 2D sobre los keyframes elegidos: el MISMO interruptor que el de la Vista 3D (Overlays > Gizmo). Con el
    // teclado del telefono no suma nada y tapa los keyframes: se apaga desde aca tambien
    { extern bool g_gizmoOn; g_tlMenuView->AddCheck(T("Gizmo"), TL_VIEW_GIZMO, &g_gizmoOn); }
    // toggle "Solo vertices seleccionados" (dope Y curvas: es el MISMO menu View). Con la
    // malla en Edit Mode filtra las filas de vertex anim y hace el move por mascara.
    g_tlMenuView->AddCheck(T("Only selected vertices"), TL_VIEW_SOLOSEL, &g_dopeOnlySelVerts);
    // LO DEL SONIDO, juntos y separados con una linea, con el PARLANTE: la forma de onda y la linea del volumen de las
    // tiras, y el AUDIO SCRUBBING (con la animacion quieta, mover el frame hace sonar ese pedacito)
    { MenuItem* it = g_tlMenuView->AddCheck(T("Waveform"), TL_VIEW_WAVE, &g_tlShowWave, IconType::sound); if (it) it->lineUp = true; }
    g_tlMenuView->AddCheck(T("Volume line"), TL_VIEW_VOLUME, &g_tlShowVolume, IconType::sound);
    { extern bool g_soundScrub; g_tlMenuView->AddCheck(T("Audio scrubbing"), TL_VIEW_SCRUB, &g_soundScrub, IconType::sound); }
    if (OpenMenu && OpenMenu != g_tlMenuView) OpenMenu->Close();
    g_tlMenuView->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = g_tlMenuView;
}
void Timeline::OpenMenuInterp(int mx, int my){
    if (!g_tlMenuKeyIn){ g_tlMenuKeyIn = new PopupMenu(); g_tlMenuKeyIn->action = TL_menuKeyAction;
                         g_tlMenuKeyIn->title = T("Set Keyframe Interpolation"); }
    BuildMenuInterp(g_tlMenuKeyIn);
    if (OpenMenu && OpenMenu != g_tlMenuKeyIn) OpenMenu->Close();
    g_tlMenuKeyIn->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = g_tlMenuKeyIn;
}
// Handle Type ('v'): como se portan los dos lados del handle. Solo tiene sentido en CURVAS y sobre keyframes bezier.
// Handle Type: como se portan los dos lados del handle. Lo comparten el submenu del menu Key y la 'v'.
void Timeline::BuildMenuHandle(PopupMenu* m){
    m->Clear();
    m->Add(T("Free"), TL_KEY_HANDLE0 + HFree, IconType::handle_free);
    m->Add(T("Aligned"), TL_KEY_HANDLE0 + HAligned, IconType::handle_aligned);
    m->Add(T("Vector"), TL_KEY_HANDLE0 + HVector, IconType::handle_vector);
    m->Add(T("Automatic"), TL_KEY_HANDLE0 + HAuto, IconType::handle_auto);
    m->Add(T("Auto Clamped"), TL_KEY_HANDLE0 + HAutoClamped, IconType::handle_autoclamped);
    m->Add(T("Flat"), TL_KEY_HANDLE0 + HPlane, IconType::handle_flat);
}
void Timeline::OpenMenuHandle(int mx, int my){
    if (!g_tlMenuHandle){
        g_tlMenuHandle = new PopupMenu(); g_tlMenuHandle->action = TL_menuKeyAction;
        g_tlMenuHandle->title = T("Set Keyframe Handle Type");
    }
    BuildMenuHandle(g_tlMenuHandle);
    if (OpenMenu && OpenMenu != g_tlMenuHandle) OpenMenu->Close();
    g_tlMenuHandle->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = g_tlMenuHandle;
}

// ------------------------------------------------------------------ input
// SOLO los botones que abren un MENU. Separado de las ACCIONES (transporte, Auto Key, modo, campos) porque el
// ruteo compartido lo llama al PASAR EL MOUSE sin click, para poder deslizarse de un menu a otro (lo mismo que
// hace la barra del viewport 3D). Deslizarse por encima no puede disparar una accion.
bool Timeline::OpenBarMenu(int mx, int my){
    if (btnSelect->visible && btnSelect->Contains(mx,my)){ OpenMenuSelect(btnSelect->sx, btnSelect->sy + btnSelect->height); return true; }
    if (btnView->visible   && btnView->Contains(mx,my)){   OpenMenuView(btnView->sx, btnView->sy + btnView->height); return true; }
    if (btnKey->visible    && btnKey->Contains(mx,my)){    OpenMenuKey(btnKey->sx, btnKey->sy + btnKey->height); return true; }
    if (btnPivot->visible  && btnPivot->Contains(mx,my)){  OpenMenuPivot(btnPivot->sx, btnPivot->sy + btnPivot->height); return true; }
    if (btnSelMode->visible && btnSelMode->Contains(mx,my)){ OpenMenuSelMode(btnSelMode->sx, btnSelMode->sy + btnSelMode->height); return true; }
    if (btnAnim->visible   && btnAnim->Contains(mx,my)){   OpenMenuAnim(btnAnim->sx, btnAnim->sy + btnAnim->height); return true; }
    return false;
}

bool Timeline::ClickBarButton(int mx, int my){
    g_tlActive = this;   // re-latch: si OTRO timeline se destruyo (dejo g_tlActive NULL), este vuelve a ser el activo
    for (int i=0;i<8;i++) if (btnT[i]->visible && btnT[i]->Contains(mx,my)){ TransportAction(i); return true; }
    if (btnTime->visible && btnTime->Contains(mx,my)){
        fCur = (float)CurrentFrame; fStart = (float)StartFrame; fEnd = (float)EndFrame; fFpsTL = (float)AnimFPS;
        OpenTimePopup(pfCur, pfStart, AnimIsGame ? NULL : pfEnd, pfFps, btnTime->sx, btnTime->sy + btnTime->height);
        return true;
    }
    // AUTO KEY: toggle
    if (btnAutoKey->visible && btnAutoKey->Contains(mx,my)){
        AutoKeyOn = !AutoKeyOn;   // solo esta sesion: el editor SIEMPRE arranca con el auto key apagado
        g_redraw = true; return true;
    }
    // switch DOPE SHEET <-> CURVES
    if (btnMode->visible && btnMode->Contains(mx,my)){ ChangeMode(mode != TL_MODE_CURVES); return true; }
    // ...y los MENUS: el mismo camino que usa el hover para deslizarse entre ellos (no se duplica el if de cada uno)
    if (OpenBarMenu(mx, my)) return true;
    return false;
}

void Timeline::button_left(){
    if (PopUpActive) return;
    g_tlActive = this;   // re-latch (idem ClickBarButton): el timeline clickeado es el que manda
    int lx = lastMx - x, ly = lastMy - y;
    // moviendo keyframes ('g' tipeada): el click ACEPTA. (El arrastre DIRECTO no pasa por aca: lo confirma el
    //  soltar del boton, ver mouse_button_up.)
    if (g_dopeMov){ DopeMoveConfirm(); return; }
    // un HANDLE de la curva le gana a todo (al gizmo tambien): es lo mas chico y suele quedar debajo de sus flechas
    if (mode == TL_MODE_CURVES && !StripActive() && HandleUnder(lastMx, lastMy, true)) return;
    // el GIZMO 2D: apretar una manija la agarra (soltar sin arrastrar = el modo siguiente)
    if (Gizmo2DDown(this, lastMx, lastMy)){ ViewPortClickDown = true; return; }
    // PANEL del dope sheet: flecha = plegar/desplegar; resto = seleccionar la fila (NO scrubbea)
    if (DopeClickPanel(lastMx, lastMy)) return;
    // STRIP: en CURVAS, handles y keyframes (hit-test 2D); en DOPE, el rombo de la fila. Si no cae en nada, scrubbea.
    if (StripActive()){ StripConfirm(); return; }   // ('g' de una tira: el click acepta)
    if (mode == TL_MODE_CURVES){ if (CurveClickStrip(lastMx, lastMy)) return; }
    else if (DopeClickStrip(lastMx, lastMy)) return;
    else if (StripClick(lastMx, lastMy)) return;     // (los rombos tienen prioridad: se puede clickear uno sobre la tira)
    // boton VERDE del frame actual (sobresale por arriba de los numeros) -> editar por texto (teclado / pad numerico)
    // el boton VERDE del frame actual: apretarlo y ARRASTRAR = scrub (como agarrar la linea del frame actual); soltarlo
    // sin arrastrar = editar el numero por texto. Antes el apretar ya abria la edicion y arrastrar desde ahi no hacia nada
    if (ly >= numY - GlobalScale*2 && ly < stripY && lx >= curBtnX && lx <= curBtnX + curBtnW){
        scrubbing = true; g_tlButtonGreen = true; g_tlGreenButtonX0 = lastMx; g_redraw = true; return; }
    // EL FRAME ACTUAL se cambia SOLO en la banda de los NUMEROS (click = ir ahi, arrastre = scrub) o agarrando la
    // LINEA del frame actual. Antes cualquier click en el cuerpo movia el frame: se perdia el frame al querer
    // elegir o deseleccionar keyframes.
    if (ly >= numY - GlobalScale*2 && ly < stripY){ scrubbing = true; SetFrameFromX(lx); return; }
    if (PlayheadIn(lastMx, lastMy)){ scrubbing = true; g_redraw = true; return; }
    if (ly < stripY || lx < panelW || dopeRows.empty()) return;
    // CURVAS: click sobre la LINEA de una curva = elige ese tramo (el keyframe de antes y el de despues)
    if (mode == TL_MODE_CURVES && CurveSegmentIn(lastMx, lastMy, LShiftPressed)) return;
    // lugar vacio: arranca una CAJA de seleccion (izq -> der verde: lo que quede entero adentro; der -> izq azul:
    // alcanza con rozar). Un click sin arrastrar deselecciona (ver TimelineBoxRelease)
    BoxSelectBuild(); BoxSelectDown(lastMx, lastMy); g_tlBox = this;
}
// la LINEA del frame actual (el cabezal): banda de unos px a cada lado, en el cuerpo
bool Timeline::PlayheadIn(int mx, int my) const {
    const int lx = mx - x, ly = my - y;
    if (ly < stripY || ly >= height || lx < panelW) return false;
    const float px = FrameToX((float)CurrentFrame);
    return fabsf((float)lx - px) <= LineRadius();
}
// muestrea el tramo i (A = i-1, B = i) de la curva en coords LOCALES, cada ~3 px (con cualquier interpolacion:
// EvalF da lo mismo que corre la animacion)
static void SegmentPoints(const Timeline* t, const AnimProperty* ap, size_t i, std::vector<float>& xy){
    xy.clear();
    const keyFrame& a = ap->keyframes[i-1]; const keyFrame& b = ap->keyframes[i];
    if (a.Interpolation == KfConstant){   // escalon: meseta + salto
        xy.push_back(t->FrameToX((float)a.frame)); xy.push_back(t->ValueToY(a.value));
        xy.push_back(t->FrameToX((float)b.frame)); xy.push_back(t->ValueToY(a.value));
        xy.push_back(t->FrameToX((float)b.frame)); xy.push_back(t->ValueToY(b.value));
        return;
    }
    const float ppf = t->pxPerFrame > 0.01f ? t->pxPerFrame : 0.01f;
    float step = (3.0f * (float)GlobalScale) / ppf; if (step <= 0.0f) step = 1.0f;
    if (a.Interpolation == KfLinear) step = (float)(b.frame - a.frame);
    for (float f = (float)a.frame; ; f += step){
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
bool Timeline::CurveSegmentIn(int mx, int my, bool add){
    const float lx = (float)(mx - x), ly = (float)(my - y);
    float best = GrabRadius(); int bRow = -1; size_t bI = 0;
    std::vector<float> xy;
    for (size_t r=0; r<dopeRows.size(); r++){
        const DopeRow& d = dopeRows[r]; if (d.propId < 0 || d.hidden || DopeVertexAnimOf(d.ownerKey)) continue;
        const AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId, false); if (!ap) continue;
        for (size_t i=1; i<ap->keyframes.size(); i++){
            const float xa = FrameToX((float)ap->keyframes[i-1].frame), xb = FrameToX((float)ap->keyframes[i].frame);
            if (lx < xa - best || lx > xb + best) continue;   // el mouse no esta en el ancho de este tramo
            SegmentPoints(this, ap, i, xy);
            for (size_t k=2; k+1<xy.size(); k+=2){
                const float dd = DistSeg(lx, ly, xy[k-2], xy[k-1], xy[k], xy[k+1]);
                if (dd < best){ best = dd; bRow = (int)r; bI = i; }
            }
        }
    }
    if (bRow < 0) return false;
    const DopeRow& d = dopeRows[(size_t)bRow];
    const AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId, false);
    if (!add) g_dopeKeySel.clear();
    g_dopeKeySel.insert(DopeKeyId(d.ownerKey, d.propId, d.compId, ap->keyframes[bI-1].frame));
    g_dopeKeySel.insert(DopeKeyId(d.ownerKey, d.propId, d.compId, ap->keyframes[bI].frame));
    // el primero del tramo queda ACTIVO: la tarjeta Keyframe del panel de propiedades muestra esa curva
    g_dopeActOwner = d.ownerKey; g_dopeActProp = d.propId; g_dopeActComp = d.compId;
    g_dopeActFrame = ap->keyframes[bI-1].frame; g_dopeActHas = true;
    g_redraw = true;
    return true;
}
// LA CAJA: VERDE (tocar = false) = solo los keyframes ADENTRO. AZUL (tocar = true) = ademas, en CURVAS, el tramo
// que la caja ROCE (aunque sea un pedazo de la linea) queda elegido entero: el keyframe de antes y el de despues.
void Timeline::BoxApply(int sx0, int sy0, int sx1, int sy1, bool touch, bool add){
    const int x0 = sx0 - x, y0 = sy0 - y, x1 = sx1 - x, y1 = sy1 - y;   // a locales
    if (!add) g_dopeKeySel.clear();
    std::vector<float> xy;
    for (size_t r=0; r<dopeRows.size(); r++){
        const DopeRow& d = dopeRows[r]; if (d.ghost && mode != TL_MODE_CURVES) continue;
        if (mode == TL_MODE_CURVES){
            if (d.propId < 0 || d.hidden) continue;
            static AnimProperty s_vtxTmp;
            const AnimProperty* ap = VertexChannelTemp(d.ownerKey, s_vtxTmp) ? &s_vtxTmp
                                   : DopeResolveProp(d.ownerKey, d.propId, d.compId, false);
            if (!ap) continue;
            const size_t n = ap->keyframes.size();
            for (size_t i=0; i<n; i++){
                const keyFrame& k = ap->keyframes[i];
                if (BoxPoint(x0, y0, x1, y1, FrameToX((float)k.frame), ValueToY(k.value)))
                    g_dopeKeySel.insert(DopeKeyId(d.ownerKey, d.propId, d.compId, k.frame));
            }
            if (!touch || ap == &s_vtxTmp) continue;
            for (size_t i=1; i<n; i++){
                SegmentPoints(this, ap, i, xy);
                bool touches = false;
                for (size_t k=2; k+1<xy.size() && !touches; k+=2)
                    touches = BoxSegment(x0, y0, x1, y1, xy[k-2], xy[k-1], xy[k], xy[k+1]);
                if (!touches) continue;
                g_dopeKeySel.insert(DopeKeyId(d.ownerKey, d.propId, d.compId, ap->keyframes[i-1].frame));
                g_dopeKeySel.insert(DopeKeyId(d.ownerKey, d.propId, d.compId, ap->keyframes[i].frame));
            }
        } else {
            // DOPE: los rombos de cada fila (puntos: verde y azul dan lo mismo). El rombo de una fila PADRE elige
            // los keyframes de ese frame de todos los canales que cubre
            const float ry = (float)(stripY + (int)r*rowH + PosY) + rowH*0.5f;
            for (size_t k=0; k<d.keys.size(); k++){
                if (!BoxPoint(x0, y0, x1, y1, FrameToX((float)d.keys[k]), ry)) continue;
                if (d.propId >= 0){ g_dopeKeySel.insert(DopeKeyId(d.ownerKey, d.propId, d.compId, d.keys[k])); continue; }
                for (size_t c=0; c<dopeRows.size(); c++){ const DopeRow& cc = dopeRows[c]; if (cc.propId < 0) continue;
                    if (d.type != 0 && !DopeCovers(d.ownerKey, cc.ownerKey)) continue;
                    for (size_t q=0; q<cc.keys.size(); q++) if (cc.keys[q] == d.keys[k])
                        g_dopeKeySel.insert(DopeKeyId(cc.ownerKey, cc.propId, cc.compId, d.keys[k])); }
            }
        }
    }
    g_redraw = true;
}
// (controls.cpp) la caja en curso es de un timeline? -> su soltar va a el y no al 3D
bool TimelineBoxOwn(){ return g_tlBox != NULL && BoxSelectActive(); }
void TimelineBoxForget(){ g_tlBox = NULL; }
void TimelineBoxRelease(bool had, int x0, int y0, int x1, int y1, bool touch, bool add){
    Timeline* t = g_tlBox; g_tlBox = NULL;
    if (!t) return;
    if (had) t->BoxApply(x0, y0, x1, y1, touch, add);
    else if (!add){ t->DopeSelectNone(); }   // un click suelto en un lugar vacio: deselecciona
    g_redraw = true;
}
void Timeline::event_mouse_motion(int mx, int my){
    if (PopUpActive){ lastMx=mx; lastMy=my; return; }
    if (Gizmo2DDragging()){ Gizmo2DMotion(this, mx, my); lastMx=mx; lastMy=my; return; }   // el gizmo 2D agarrado
    if (ToolbarVisible()){ bool h = false;
        for (size_t i=0;i<ToolButtons.size();i++){ Button* b = ToolButtons[i];
            const bool hv = b->visible && b->Contains(mx, my); if (hv != b->hover){ b->hover = hv; h = true; } }
        if (h) g_redraw = true; }
    if (StripActive()){ StripApply(mx); lastMx=mx; lastMy=my; return; }   // corriendo / recortando una tira
    // g/s/r: el arrastre transforma. Se usa el cursor VIRTUAL (acumula dx/dy), NO la posicion real: el cursor se
    // envuelve al llegar al borde para poder seguir escalando/rotando sin limite, y ese salto no tiene que verse.
    if (g_dopeMov){
        lastMx=mx; lastMy=my;
        if (g_dopeVirtFirst) g_dopeVirtFirst = false;   // dx/dy de este evento son de ANTES del transform
        else { g_dopeVirtX += dx; g_dopeVirtY += dy; }
        DopeMoveApply(g_dopeVirtX, g_dopeVirtY);
        return;
    }
    // arrastrando un handle: curva el tramo. Usa el cursor VIRTUAL (acumula dx/dy) y NO la posicion real: el
    // cursor se envuelve al llegar al borde para poder seguir arrastrando sin limite, y ese salto no tiene que verse.
    if (g_hOn && leftMouseDown){
        lastMx=mx; lastMy=my;
        if (g_hVirtFirst) g_hVirtFirst = false;   // dx/dy de este evento son de ANTES de agarrarlo
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
            if (g_dopeMov){ g_dopeVirtX = mx; g_dopeVirtY = my; g_dopeVirtFirst = false; DopeMoveApply(mx, my); }
        }
        lastMx = mx; lastMy = my;
        if (!g_dopeMov) return;
    }
    if (g_hOn && !leftMouseDown) HandleRelease();
    // BORDE del panel: arrastrando cambia su ancho (queda FIJADO por el usuario -> panelWUser)
    if (panelResize && leftMouseDown){ panelWUser = mx - x; if (panelWUser < 1) panelWUser = 1; lastMx=mx; lastMy=my; g_redraw=true; return; }
    if (!leftMouseDown) panelResize = false;
    // la barra horizontal del panel: arrastrar el agarre corre el texto (en proporcion al recorrido)
    if (panelBarH && leftMouseDown){
        int bx, by, bw, bh, tx, tw;
        if (PanelBarH(bx, by, bw, bh, tx, tw) && bw > tw && dx){
            panelPosX += (int)((float)dx * (float)PanelMaxX() / (float)(bw - tw));
            if (panelPosX < PanelMaxX()) panelPosX = PanelMaxX(); if (panelPosX > 0) panelPosX = 0;
        }
        lastMx=mx; lastMy=my; g_redraw=true; return;
    }
    if (!leftMouseDown) panelBarH = false;
    { bool h = !leftMouseDown && PanelInBorder(mx - x, my - y);   // el borde se pinta BLANCO con el mouse encima
      if (h != panelBorderHover){ panelBorderHover = h; g_redraw = true; } }
    // BOTON DEL MEDIO sobre el PANEL: scrollea el panel (texto en horizontal, filas en vertical), NO el timeline.
    // Se decide donde arranco el arrastre y queda asi hasta soltar.
    if (!middleMouseDown) panelPan = 0;
    else if (panelPan == 0){ int lx0 = lastMx - x, ly0 = lastMy - y; panelPan = (panelW > 0 && lx0 >= 0 && lx0 < panelW && ly0 >= stripY) ? 1 : 2; }
    if (middleMouseDown && panelPan == 1){
        if (dx){ panelPosX += dx; if (panelPosX < PanelMaxX()) panelPosX = PanelMaxX(); if (panelPosX > 0) panelPosX = 0; }
        if (dy){
            if (mode == TL_MODE_DOPE) ScrollY(dy);
            else { int maxY = (height - stripY - TlToolbarHeight(this)) - DopeContentHeight(); if (maxY > 0) maxY = 0;
                   PosY += dy; if (PosY < maxY) PosY = maxY; if (PosY > 0) PosY = 0; }
        }
        lastMx = mx; lastMy = my; g_redraw = true; return;
    }
    { bool h = !leftMouseDown && PlayheadIn(mx, my);   // la linea del frame actual se pinta BLANCA con el mouse encima
      if (h != g_tlPlayheadHover){ g_tlPlayheadHover = h; g_redraw = true; } }
    DopeHover(mx, my); // mouse-over de las filas del panel
    if (!leftMouseDown) StripHover(mx, my);   // la punta de una tira bajo el mouse (blanca)
    // (el boton verde: hasta no correrse unos pixeles sigue siendo un click, que al soltar edita el numero)
    if (scrubbing && leftMouseDown && g_tlButtonGreen && abs(mx - g_tlGreenButtonX0) > 4 * GlobalScale) g_tlButtonGreen = false;
    if (scrubbing && leftMouseDown && !g_tlButtonGreen)  SetFrameFromX(mx - x);                            // izquierdo: scrub (arrastra el frame actual)
    else if (middleMouseDown){       // MEDIO: PANEA. Horizontal = frames; vertical = filas del dope sheet.
        // Se usan las GLOBALES dx/dy, NO (mx-lastMx): al llegar al borde el cursor SE ENVUELVE al otro lado
        // (CheckWarpMouseInViewport) y ahi mx/my pegan un salto del ancho/alto del viewport -> el delta salia
        // enorme y el scroll se iba al tope (nunca llegabas arriba de todo). CheckWarp pone dx/dy en 0 justo en
        // el frame del envolvimiento, asi que el paneo sigue continuo.
        // SHIFT + boton del medio (solo curvas) = ESTIRAR la vista, no panearla: arrastrar en horizontal estira el
        // eje X (tiempo) y en vertical el eje Y (valor). Es el zoom de 2 ejes a mano, para dejar la curva como se
        // vea mejor. Sin shift, el boton del medio panea como siempre.
        if (mode == TL_MODE_CURVES && LShiftPressed){
            if (dx) ZoomBy(powf(1.012f, (float)dx), CenterTimeline());   // derecha = estira el tiempo
            if (dy) ZoomVBy(powf(1.012f, -(float)dy));                   // arriba  = estira el valor
            lastMx = mx; lastMy = my; return;
        }
        PanFrames(-(float)dx / (pxPerFrame>0.01f?pxPerFrame:0.01f));
        // vertical: en DOPE scrollea las filas; en CURVAS panea el VALOR (no hay filas que scrollear)
        if (mode == TL_MODE_CURVES){ if (dy) PanValue((float)dy / (pxPerUnit>1e-6f?pxPerUnit:1e-6f)); }
        else if (!dopeRows.empty() && dy){ ScrollY(dy); g_redraw=true; }
    }
    lastMx = mx; lastMy = my;
}
bool Timeline::event_finger_scroll(int px, int py, int dx, int dy){
    if (g_tlFingerPanel == this){ PanelScrollBy(dx, dy); return true; }       // el dedo arrastra el PANEL de canales
    PanFrames(-(float)dx / (pxPerFrame>0.01f?pxPerFrame:0.01f));              // horizontal: frames
    if (mode == TL_MODE_DOPE && !dopeRows.empty() && dy){ ScrollByTouch(0, dy); g_redraw=true; } // vertical: filas
    else if (mode == TL_MODE_CURVES && dy) PanValue((float)dy / (pxPerUnit>1e-6f?pxPerUnit:1e-6f));  // vertical: el valor
    return true;
}
bool Timeline::PanelIn(int mx, int my) const {
    const int lx = mx - x, ly = my - y;
    // (el BORDE no: arrastrarlo achica o agranda el panel, tambien con el dedo)
    return panelW > 0 && lx >= 0 && lx < panelW && ly >= stripY && ly < height && !PanelInBorder(lx, ly);
}
// el PANEL con el dedo: lo mismo que la rueda sobre el (filas en vertical, el texto en horizontal)
void Timeline::PanelScrollBy(int dx, int dy){
    if (dx){ panelPosX += dx; if (panelPosX < PanelMaxX()) panelPosX = PanelMaxX(); if (panelPosX > 0) panelPosX = 0; }
    if (dy){
        if (mode == TL_MODE_DOPE) ScrollByTouch(0, dy);
        else { int maxY = (height - stripY - TlToolbarHeight(this)) - DopeContentHeight(); if (maxY > 0) maxY = 0;
               PosY += dy; if (PosY < maxY) PosY = maxY; if (PosY > 0) PosY = 0; }
    }
    g_redraw = true;
}
bool Timeline::DragMovesView(int mx, int my){
    const int lx = mx - x, ly = my - y;
    if (lx < panelW || lx >= width || ly < stripY || ly >= height) return false;   // fuera del strip (numeros, panel)
    if (OnToolbar(mx, my) || PlayheadIn(mx, my) || Gizmo2DOver(this, mx, my)) return false;
    if (StripActive() || g_dopeMov) return false;
    if (HandleUnder(mx, my, false)) return false;   // sobre un HANDLE a la vista: el dedo lo edita
    if (PanelInBorder(lx, ly)) return false;        // el BORDE del panel: el dedo lo arrastra (ancho del panel)
    return true;
}
void Timeline::event_finger_gesture(float zoomDelta, float panDx, float panDy){
    scrubbing = false; // 2 dedos: es zoom/paneo de VISTA -> NO tocar el frame (cancela cualquier scrub)
    if (zoomDelta > 1.0f) ZoomBy(1.1f, CenterTimeline()); else if (zoomDelta < -1.0f) ZoomBy(1.0f/1.1f, CenterTimeline());
    if (panDx != 0.0f) PanFrames(-panDx / (pxPerFrame>0.01f?pxPerFrame:0.01f));
}
// ================================================================== GIZMO 2D (Gizmo.h)
bool Timeline::Gizmo2DPivot(float& lx, float& ly){
    if (dopeRows.empty() || StripActive() || g_hOn || BoxSelectActive()) return false;
    float pf = 0.0f, pv = 0.0f;
    if (!DopePivotLive(this, pf, pv)) return false;
    lx = FrameToX(pf);
    // en el dope sheet no hay eje de valor: el gizmo va a media altura del strip
    ly = (mode == TL_MODE_CURVES) ? ValueToY(pv) : (float)(stripY + (height - stripY) / 2);
    return lx >= (float)panelW && lx <= (float)width && ly >= (float)stripY && ly <= (float)height;
}
int Timeline::Gizmo2DModes() const {
    return (mode == TL_MODE_CURVES) ? (Gizmo2DCanMove | Gizmo2DCanRotate | Gizmo2DCanScale)
                                    : (Gizmo2DCanMove | Gizmo2DCanScale);   // dope sheet: rotar no significa nada
}
bool Timeline::Gizmo2DInZone(float lx, float ly) const {
    return lx >= (float)panelW && ly >= (float)stripY && !const_cast<Timeline*>(this)->OnToolbar(x + (int)lx, y + (int)ly);
}
bool Timeline::Gizmo2DStart(int modeG, int axis){
    const bool curves = (mode == TL_MODE_CURVES);
    const int mt = (modeG == 1) ? DOPE_ROT : (modeG == 2) ? DOPE_ESC : DOPE_MOV;
    if (mt == DOPE_ROT && !curves) return false;
    g_tlActive = this;
    g_dopeDragPend = false;
    DopeMoveStart(mt);
    if (!g_dopeMov) return false;
    g_dopeMovAxis = !curves ? DOPE_AXIS_FREE : (axis == 1) ? DOPE_AXIS_X : (axis == 2) ? DOPE_AXIS_Y : DOPE_AXIS_FREE;
    return true;
}
void Timeline::Gizmo2DApply(float dx, float dy, float ang, float f){
    if (!g_dopeMov) return;
    const bool curves = (mode == TL_MODE_CURVES);
    if (g_dopeMovMode == DOPE_MOV){
        g_dopeMovVal  = dx / (pxPerFrame>0.01f?pxPerFrame:0.01f);                    // frames
        g_dopeMovValV = curves ? -dy / (pxPerUnit>1e-6f?pxPerUnit:1e-6f) : 0.0f;    // valor (Y de pantalla abajo)
    } else if (g_dopeMovMode == DOPE_ROT) g_dopeMovVal = ang * 180.0f / 3.14159265f;  // (en pantalla, como el 'r')
    else g_dopeMovVal = f;
    DopeXformApply();
}
void Timeline::Gizmo2DConfirm(){ if (g_dopeMov) DopeMoveConfirm(); }
void Timeline::Gizmo2DCancel(){ if (g_dopeMov) DopeMoveCancel(); }

// 2 DEDOS con la geometria completa (controls.cpp). Antes el zoom iba a saltos fijos de 10% desde el CENTRO y con un
// umbral que el propio paneo de dos dedos disparaba (la separacion tiembla): moverse con dos dedos hacia zoom solo, y
// en las curvas no habia paneo vertical ni zoom del valor. Ahora:
//  - PANEO: lo que esta bajo los dedos los sigue. Horizontal = tiempo; vertical = el VALOR en curvas, las filas en el dope.
//  - ZOOM: anclado al punto medio de los dedos y PROPORCIONAL (abrir los dedos al doble = el doble de zoom). En CURVAS
//    cada eje va por su lado: abrir en horizontal estira el tiempo, en vertical el valor, en diagonal los dos. En el eje
//    en el que los dedos estan casi alineados no hay zoom (su separacion es chica y la razon seria puro temblor).
static float g_tlPinchRestY = 0.0f;   // fraccion de pixel del scroll vertical de las filas (dope)
bool Timeline::event_finger_pinch(float cx, float cy, float panDx, float panDy, float sx0, float sy0, float sx1, float sy1){
    scrubbing = false;   // 2 dedos: es la VISTA, no el frame
    const bool curves = (mode == TL_MODE_CURVES);
    if (panDx != 0.0f) PanFrames(-panDx / (pxPerFrame>0.01f?pxPerFrame:0.01f));
    if (panDy != 0.0f) {
        if (curves) PanValue(panDy / (pxPerUnit>1e-6f?pxPerUnit:1e-6f));
        else if (!dopeRows.empty()) {
            g_tlPinchRestY += panDy;
            const int iy = (int)g_tlPinchRestY; g_tlPinchRestY -= (float)iy;
            if (iy) ScrollByTouch(0, iy);
        }
    }
    const float lx = cx - (float)x, ly = cy - (float)y;
    const float minSep = 24.0f * (float)GlobalScale;
    if (curves) {
        if (fabsf(sx0) >= minSep && fabsf(sx1) >= minSep) ZoomBy(fabsf(sx1) / fabsf(sx0), (int)(lx + 0.5f));
        if (fabsf(sy0) >= minSep && fabsf(sy1) >= minSep) ZoomVIn(fabsf(sy1) / fabsf(sy0), ly);
    } else {
        const float d0 = sqrtf(sx0*sx0 + sy0*sy0), d1 = sqrtf(sx1*sx1 + sy1*sy1);
        if (d0 >= minSep && d1 >= minSep) ZoomBy(d1 / d0, (int)(lx + 0.5f));
    }
    g_redraw = true;
    return true;
}
void Timeline::event_mouse_wheel(float dy, int mx, int my){
    if (PopUpActive) return;
    { if (BarScrollHorizontal(mx,my,(int)(dy*40))) return; }
    if (OnToolbar(mx, my)){ ToolbarScrollBy((int)(dy * 40)); return; }   // la toolbar de abajo: scroll horizontal
    // sobre el PANEL de curvas la rueda scrollea SUS filas (Shift = el texto en horizontal), no hace zoom
    { const int lx = mx - x, ly = my - y;
      if (panelW > 0 && lx >= 0 && lx < panelW && ly >= stripY){
          const int step = (int)(dy * (float)(rowH > 0 ? rowH : RowHeightGS) * 2.0f);
          if (LShiftPressed){ panelPosX += step; if (panelPosX < PanelMaxX()) panelPosX = PanelMaxX(); if (panelPosX > 0) panelPosX = 0; }
          else if (mode == TL_MODE_DOPE) ScrollY(step);
          else { int maxY = (height - stripY - TlToolbarHeight(this)) - DopeContentHeight(); if (maxY > 0) maxY = 0;
                 PosY += step; if (PosY < maxY) PosY = maxY; if (PosY > 0) PosY = 0; }
          g_redraw = true; return;
      } }
    // Fuera del panel la RUEDA SIEMPRE hace ZOOM en el timeline (desde el CENTRO exacto), NUNCA scrollea. Para scrollear/panear
    // se usa el BOTON DEL MEDIO: horizontal = frames, vertical = filas (dope) / valor (curvas).
    // En CURVAS el zoom tiene DOS EJES independientes -> no es un zoom parejo como en el resto del editor:
    //   rueda        = los dos ejes (mantiene la proporcion actual)
    //   Ctrl+rueda   = SOLO el tiempo (X)     Shift+rueda = SOLO el valor (Y)
    // Estirar un solo eje es lo que permite ver bien una curva chata o una de valores enormes.
    float f = (dy>0) ? 1.1f : 1.0f/1.1f;
    if (mode == TL_MODE_CURVES){
        if (LShiftPressed){ ZoomVBy(f); return; }
        if (LCtrlPressed) { ZoomBy(f, CenterTimeline()); return; }
        ZoomBy(f, CenterTimeline()); ZoomVBy(f); return;
    }
    ZoomBy(f, CenterTimeline());
}
void Timeline::mouse_button_up(int button){
    (void)button;
    if (Gizmo2DRelease(this)){ scrubbing=false; ViewPortClickDown=false; g_redraw=true; return; }   // (confirma / modo siguiente)
    if (g_tlButtonGreen){   // el boton verde, soltado sin arrastrar: editar el frame por texto
        g_tlButtonGreen = false; scrubbing = false; ViewPortClickDown = false; EditField(2); g_redraw = true; return; }
    // el frame YA se fijo en el DOWN + el arrastre (scrub); al soltar solo se limpia el estado.
    if (g_hOn) HandleRelease();   // soltar el handle CIERRA su undo (curvar el tramo = 1 comando de Ctrl+Z)
    // ARRASTRE DIRECTO: soltar CONFIRMA (arrastrar y soltar es un gesto completo). Si nunca se movio, el
    // pendiente se descarta y el click queda como una simple seleccion.
    if (g_dopeMov && !g_dopeDragPend) DopeMoveConfirm();
    g_dopeDragPend = false;
    if (StripActive()) StripRelease();   // arrastrar una tira y soltar = listo ('g' sigue hasta el click)
    scrubbing=false; panelResize=false; panelBarH=false; panelPan=0; ViewPortClickDown=false; g_redraw=true;
}
// CERO (Symbian: la tecla 0 del teclado numerico del telefono) MANTENIDO = modificador de zoom para las flechas.
// Si se suelta sin haber tocado ninguna flecha, fue un TOQUE: Frame Selected (el equivalente al numpad '.' de PC).
static bool g_tlZeroDown = false, g_tlZeroUsed = false;

void Timeline::event_key_up(int key){
    const int k = key;
    if (k==W3dK_0 || k==W3dK_KP_0){
        if (g_tlZeroDown && !g_tlZeroUsed) DopeFrameSelected(); // toque limpio: encuadra
        g_tlZeroDown = false; g_tlZeroUsed = false;
    }
}
void Timeline::event_key_down(int keyCode, bool repeat){
    if (PopUpActive || g_activeTextField) return;
    const int k = keyCode;
    // ---- DOPE SHEET (solo si hay filas) ----
    if (StripActive()){ // corriendo una tira con 'g': Enter acepta, Esc cancela
        if (k==W3dK_RETURN || k==W3dK_KP_ENTER){ StripConfirm(); return; }
        if (k==W3dK_ESCAPE){ StripCancel(); return; }
        return;
    }
    if (g_dopeMov){ // transformando keyframes: Enter acepta, Esc cancela. El valor numerico lo alimenta
        if (k==W3dK_RETURN || k==W3dK_KP_ENTER){ DopeMoveConfirm(); return; }  // DopeNumInputChar (SDL_TEXTINPUT en
        if (k==W3dK_ESCAPE){ DopeMoveCancel(); return; }                       // PC, keypad en Symbian), igual que el 3D
        // X / Y: limitan el transform a un eje (solo en curvas, que es donde hay dos)
        if (k==W3dK_X){ DopeCycleAxis(DOPE_AXIS_X); return; }
        if (k==W3dK_Y){ DopeCycleAxis(DOPE_AXIS_Y); return; }
        return; // durante el transform no pasan los atajos de abajo
    }
    // ---- CERO MANTENIDO + FLECHAS = ZOOM (el camino de Symbian, que no tiene rueda ni boton del medio).
    //      Es el equivalente del Shift + arrastrar con el boton del medio de PC: izq/der estira el TIEMPO,
    //      arriba/abajo estira el VALOR. Un toque de cero SIN flechas = Frame Selected.
    if (g_tlZeroDown){
        if (k==W3dK_LEFT) { ZoomBy(1.0f/1.15f, CenterTimeline()); g_tlZeroUsed = true; return; }
        if (k==W3dK_RIGHT){ ZoomBy(1.15f, CenterTimeline());      g_tlZeroUsed = true; return; }
        if (mode == TL_MODE_CURVES){
            if (k==W3dK_UP)  { ZoomVBy(1.15f);      g_tlZeroUsed = true; return; }
            if (k==W3dK_DOWN){ ZoomVBy(1.0f/1.15f); g_tlZeroUsed = true; return; }
        }
    }
    if (k==W3dK_0 || k==W3dK_KP_0){ g_tlZeroDown = true; g_tlZeroUsed = false; return; } // Symbian: se define al soltar
    // numpad '.' = Frame Selected, igual que en el viewport 3D (ahi enfoca el objeto; aca encuadra los keyframes)
    if (k==W3dK_KP_PERIOD){ DopeFrameSelected(); return; }
    if (!dopeRows.empty()){
        if (k==W3dK_B){ BoxSelectBuild(); BoxSelectMove(lastMx, lastMy); g_tlBox = this; g_redraw = true; return; } // B: caja
        if (k==W3dK_ESCAPE && g_tlBox == this && BoxSelectActive()){ BoxSelectCancel(); g_tlBox = NULL; return; }
        if (k==W3dK_A && LAltPressed){ DopeSelectNone(); return; }    // Alt+A: deseleccionar todo
        if (k==W3dK_A)               { DopeSelectAll();  return; }    // A: seleccionar todos los keyframes
        if (k==W3dK_I && LCtrlPressed){ DopeSelectInvert(); return; } // Ctrl+I: invertir
        // Ctrl+X / Ctrl+C / Ctrl+V: cortar / copiar / pegar keyframes. Ctrl+D (y Shift+D): DUPLICAR (y agarrar)
        if (LCtrlPressed && k==W3dK_X){ DopeCutSelection(); return; }
        if (LCtrlPressed && k==W3dK_C){ DopeCopySelection(); return; }
        if (LCtrlPressed && k==W3dK_V){ DopePaste(); return; }
        if (k==W3dK_D && (LCtrlPressed || LShiftPressed)){ DopeDuplicateSelection(); return; }
        // X / Supr / Backspace (la C del N95): borrar los keyframes / las CURVAS (filas) elegidas
        if (k==W3dK_X || k==W3dK_DELETE || k==W3dK_BACKSPACE) { DopeDeleteSelection(); return; }
        if (k==W3dK_G && g_dopeKeySel.empty() && StripGrabStart()) return; // G sin keyframes elegidos: los SONIDOS
        if (k==W3dK_G)               { DopeMoveStart(DOPE_MOV); return; }  // G: MOVER
        if (k==W3dK_S)               { DopeMoveStart(DOPE_ESC); return; }  // S: ESCALAR desde el pivote
        // R: ROTAR. Solo en CURVAS: en el dope sheet hay un solo eje (el tiempo) y girar no significa nada.
        if (k==W3dK_R && mode==TL_MODE_CURVES){ DopeMoveStart(DOPE_ROT); return; }
        // T: Interpolation Mode (el mismo submenu del menu Key). Se abre EN EL CURSOR, como los menus del 3D.
        if (k==W3dK_E && LCtrlPressed){ OpenMenuEasing(lastMx, lastMy); return; }   // Ctrl+E: Set Keyframe Easing Type
        if (k==W3dK_T || k==W3dK_E){ OpenMenuInterp(lastMx, lastMy); return; }     // E / T: Set Keyframe Interpolation
        // V: Handle Type. Solo en CURVAS: los handles solo existen ahi.
        if (k==W3dK_V && mode==TL_MODE_CURVES){ OpenMenuHandle(lastMx, lastMy); return; }
    }
    if (k==W3dK_SPACE) {
        // jugando (modo Juego en play) el espacio es del juego: no pausa (la pausa es ESC)
        extern bool SimActive(); extern bool AnimIsGame;
        if (AnimIsGame && PlayAnimation && SimActive()) return;
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
//  LAS TIRAS DE SONIDO (objects/Sound.h). Todo en SEGUNDOS de la escena: la tira de un sonido arranca en
//  'inicio' y dura su tramo / su tono; con bucle se repite hasta el final del timeline (las repeticiones mas
//  apagadas). Se corren y se recortan sin engancharse a los frames (Ctrl engancha al frame, si se quiere).
// ============================================================================
namespace {
struct StripOrig { Sound* s; float start, ri, rf; };
enum { STRIP_NOTHING = 0, STRIP_MOVE = 1, STRIP_TRIMSTART = 2, STRIP_TRIMEND = 3 };
int  g_stripMode = STRIP_NOTHING;
bool g_stripGrab = false;    // 'g': sigue al mouse hasta el click / Enter
bool g_stripStarted = false; // arrastre: paso el umbral (un click limpio no corre nada)
int  g_stripX0 = 0, g_stripY0 = 0;
std::vector<StripOrig> g_stripOrig;
Sound* g_stripHoverS = NULL; int g_stripHoverB = 0;   // la punta bajo el mouse (STRIP_TRIMSTART / STRIP_TRIMEND)
// LO ELEGIDO CON EL LAPIZ (el teclado del telefono): el sonido y la parte (STRIP_TRIMSTART / STRIP_MOVE / STRIP_TRIMEND).
// El 1 (la 'g') agarra esa parte y las flechas la corren (g_stripKeyAccum: los pixeles que se llevan corridos)
Sound* g_stripSelS = NULL; int g_stripSelPart = STRIP_MOVE;
float g_stripKeyAccum = 0.0f;
// el ANCHO de la zona que agarra una punta: generoso, para el dedo (y nunca menos que media fila)
int StripZoneBorder(int rowH) { int z = GlobalScale * 10; if (z < rowH / 2) z = rowH / 2; return z; }
// el UNDO de una tira: intercambia lo vivo con lo guardado (undo y redo son el mismo toggle)
struct UndoStrip { std::vector<StripOrig> v; };
void UndoStripApply(void* d) {
    UndoStrip* u = (UndoStrip*)d;
    for (size_t i = 0; i < u->v.size(); i++) {
        StripOrig& o = u->v[i]; if (!o.s) continue;
        StripOrig live = { o.s, o.s->start, o.s->clipStart, o.s->clipEnd };
        o.s->start = o.start; o.s->clipStart = o.ri; o.s->clipEnd = o.rf;
        o = live;
    }
    g_redraw = true;
}
void UndoStripUnlink(void* d, Object* deleted) {
    UndoStrip* u = (UndoStrip*)d;
    for (size_t i = 0; i < u->v.size(); i++) if ((Object*)u->v[i].s == deleted) u->v[i].s = NULL;
}
void UndoStripFree(void* d) { delete (UndoStrip*)d; }
// la curva del VOLUMEN del sonido en la escena activa (NULL = no esta animado)
const AnimProperty* CurveVolume(Object* o) {
    for (size_t i = 0; i < AnimationObjects.size(); i++) {
        if (AnimationObjects[i].obj != o) continue;
        const std::vector<AnimProperty>& P = AnimationObjects[i].Properties;
        for (size_t p = 0; p < P.size(); p++)
            if (P[p].Property == AnimSound && P[p].component == AnimX && !P[p].keyframes.empty()) return &P[p];
    }
    return NULL;
}
}

bool Timeline::StripActive() const { return g_stripMode != STRIP_NOTHING && (g_stripGrab || g_stripStarted || leftMouseDown); }

void Timeline::DrawStrip(const DopeRow& d, int ry0) {
    Sound* s = (Sound*)d.sound;
    const float L = s->DurationInScene();
    const float durArch = s->DurationFile();
    const int y0 = ry0 + GlobalScale, h = rowH * (1 + (d.stripRows > 0 ? d.stripRows : 0)) - GlobalScale * 2;
    if (h <= 0) return;
    const bool sel = s->select;
    gfx::Disable(gfx::Texture2D); gfx::DisableArray(gfx::TexCoordArray); gfx::Enable(gfx::Blend); gfx::BlendAlpha();
    if (L <= 0.0f) {   // sin archivo (o no se pudo leer): una marca donde arranca
        const float x = FrameToX(W3dSoundFrameFromSec(s->start));
        if (x >= panelW && x <= width) { gfx::Color4f(0.8f, 0.3f, 0.3f, 0.9f); FillRect((int)x, y0, GlobalScale * 3, h); }
        return;
    }
    const int reps = s->loop ? 100000 : 1;
    const AnimProperty* cv = g_tlShowVolume ? CurveVolume(s) : NULL;
    for (int k = 0; k < reps; k++) {
        const float sa = s->start + L * (float)k, sb = sa + L;
        const float xa = FrameToX(W3dSoundFrameFromSec(sa)), xb = FrameToX(W3dSoundFrameFromSec(sb));
        if (xa > width) break;
        if (xb < panelW) continue;
        const float ca = xa < panelW ? (float)panelW : xa, cb = xb > width ? (float)width : xb;
        if (cb - ca < 1.0f) { if (xb - xa < 1.0f) break; continue; }
        // la tira (las repeticiones del bucle, mas apagadas)
        const float a = (k == 0) ? 1.0f : 0.55f;
        if (sel) gfx::Color4f(0.16f * a + 0.05f, 0.52f * a, 0.56f * a, 0.95f); else gfx::Color4f(0.12f * a, 0.34f * a, 0.38f * a, 0.9f);
        FillRect((int)ca, y0, (int)(cb - ca), h);
        // la FORMA DE ONDA: la textura del archivo entero, recortada al tramo (solo con View > Waveform)
        if (g_tlShowWave && durArch > 0.0f) {
            const unsigned int tex = W3dSoundWave(s->file);
            if (tex) {
                const float u0 = s->clipStart / durArch, u1 = (durArch - s->clipEnd) / durArch;
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
        if (g_tlShowVolume) {
            g_curveBuf.clear();
            float px = -1.0f, py = 0.0f;
            const float step = (float)GlobalScale * 3.0f;
            for (float x = ca; ; x += step) {
                if (x > cb) x = cb;
                const float f = XToFrame(x);
                const float vol = cv ? cv->EvalF(f, s->volume) : s->volume;
                float yy = (float)(y0 + h) - (vol / 1.25f) * (float)h;
                if (yy < (float)y0) yy = (float)y0; if (yy > (float)(y0 + h)) yy = (float)(y0 + h);
                if (px >= 0.0f) QuadLine(g_curveBuf, px, py, x, yy, (float)GlobalScale * 0.6f);
                px = x; py = yy;
                if (x >= cb) break;
            }
            if (!g_curveBuf.empty()) {
                gfx::Color4f(1.0f, 0.78f, 0.25f, (k == 0) ? 1.0f : 0.5f);
                gfx::VertexPointer2f(0, &g_curveBuf[0]); gfx::DrawTrianglesArray((int)(g_curveBuf.size() / 2));
            }
        }
        // EL SONIDO ENTERO elegido con el lapiz: un borde blanco arriba y abajo
        if (k == 0 && g_stripSelS == s && g_stripSelPart == STRIP_MOVE && !(g_stripMode == STRIP_MOVE && g_stripGrab)) {
            gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
            FillRect((int)ca, y0, (int)(cb - ca), GlobalScale);
            FillRect((int)ca, y0 + h - GlobalScale, (int)(cb - ca), GlobalScale);
        }
        // las PUNTAS (se agarran para recortar): solo en la primera pasada. Arrastrandola = VERDE; con el mouse
        // encima = BLANCA (y mas ancha); si no, celeste apagado
        if (k == 0) {
            for (int side = STRIP_TRIMSTART; side <= STRIP_TRIMEND; side++) {
                const bool drag = (g_stripMode == side && !g_stripOrig.empty() && g_stripOrig[0].s == s);
                const bool hover = !drag && ((g_stripMode == STRIP_NOTHING && g_stripHoverS == s && g_stripHoverB == side) ||
                                                 (g_stripSelS == s && g_stripSelPart == side));   // (o la elegida con el lapiz)
                const int an = GlobalScale * ((drag || hover) ? 3 : 2);
                if (drag)   gfx::Color4f(TL_GREEN_LINE[0], TL_GREEN_LINE[1], TL_GREEN_LINE[2], 1.0f);
                else if (hover) gfx::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
                else            gfx::Color4f(0.85f, 0.95f, 1.0f, sel ? 0.9f : 0.5f);
                if (side == STRIP_TRIMSTART) { if (xa >= panelW) FillRect((int)xa, y0, an, h); }
                else                     { if (xb <= width)  FillRect((int)xb - an, y0, an, h); }
            }
        }
        if (!s->loop) break;
    }
    gfx::Disable(gfx::Blend);
}

int Timeline::StripIn(int mx, int my, Sound** out) {
    if (out) *out = NULL;
    if (dopeRows.empty() || mode != TL_MODE_DOPE) return STRIP_NOTHING;
    const int lx = mx - x, ly = my - y;
    if (lx < panelW || ly < stripY || ly >= height) return STRIP_NOTHING;
    const int r = (ly - stripY - PosY) / (rowH > 0 ? rowH : 1);
    if (r < 0 || r >= (int)dopeVisible) return STRIP_NOTHING;
    const DopeRow& d = dopeRows[r];
    if (!d.sound || d.hidden) return STRIP_NOTHING;
    Sound* s = (Sound*)d.sound;
    const float L = s->DurationInScene();
    const float xa = FrameToX(W3dSoundFrameFromSec(s->start));
    const float xb = (L > 0.0f) ? FrameToX(W3dSoundFrameFromSec(s->start + L)) : xa + GlobalScale * 3;
    const float end = s->loop ? (float)width : xb;
    const float border = (float)StripZoneBorder(rowH);
    if ((float)lx < xa - border || (float)lx > end + border) return STRIP_NOTHING;
    if (out) *out = s;
    if (L <= 0.0f) return STRIP_MOVE;
    // la punta MAS CERCANA (en una tira corta las dos zonas se pisan): la izquierda, la derecha o el cuerpo
    const float da = fabsf((float)lx - xa), db = fabsf((float)lx - xb);
    if (da <= border && da <= db) return STRIP_TRIMSTART;
    if (db <= border) return STRIP_TRIMEND;
    return STRIP_MOVE;
}
void Timeline::StripHover(int mx, int my) {
    Sound* s = NULL;
    const int b = StripIn(mx, my, &s);
    const int border = (b == STRIP_TRIMSTART || b == STRIP_TRIMEND) ? b : 0;
    Sound* hs = border ? s : NULL;
    if (hs != g_stripHoverS || border != g_stripHoverB) { g_stripHoverS = hs; g_stripHoverB = border; g_redraw = true; }
}
bool Timeline::StripClick(int mx, int my) {
    Sound* s = NULL;
    const int modeT = StripIn(mx, my, &s);
    if (modeT == STRIP_NOTHING || !s) return false;
    // el sonido clickeado pasa a ser el ACTIVO (su tarjeta en Properties); los seleccionados se corren juntos
    if (ActiveObj != s) { ActiveObj = s; if (!s->select) s->Select(); }
    g_stripOrig.clear();
    for (size_t i = 0; i < ObjSelects.size(); i++) {
        Object* o = ObjSelects[i];
        if (!o || o->getType() != ObjectType::sound) continue;
        if (modeT != STRIP_MOVE && o != s) continue;   // (recortar es de UNA)
        Sound* so = (Sound*)o; StripOrig t = { so, so->start, so->clipStart, so->clipEnd }; g_stripOrig.push_back(t);
    }
    if (g_stripOrig.empty()) { StripOrig t = { s, s->start, s->clipStart, s->clipEnd }; g_stripOrig.push_back(t); }
    g_stripMode = modeT; g_stripGrab = false; g_stripStarted = false; g_stripX0 = mx; g_stripY0 = my;
    g_redraw = true;
    return true;
}
bool Timeline::StripGrabStart() {
    g_stripOrig.clear();
    g_stripKeyAccum = 0.0f;
    // con una parte elegida con el lapiz: SOLO ese sonido, y lo que se mueve es esa parte (recortar o correr)
    if (g_stripSelS && StripRowOf(g_stripSelS) >= 0) {
        Sound* so   = g_stripSelS; StripOrig t = { so, so->start, so->clipStart, so->clipEnd }; g_stripOrig.push_back(t);
        g_stripMode = g_stripSelPart; g_stripGrab = true; g_stripStarted = true; g_stripX0 = lastMx; g_stripY0 = lastMy;
        g_redraw = true;
        return true;
    }
    for (size_t r = 0; r < dopeRows.size(); r++) {
        Object* o = dopeRows[r].sound;
        if (!o || dopeRows[r].hidden) continue;
        bool already = false; for (size_t i = 0; i < g_stripOrig.size(); i++) if (g_stripOrig[i].s == o) already = true;
        if (already) continue;
        Sound* so = (Sound*)o; StripOrig t = { so, so->start, so->clipStart, so->clipEnd }; g_stripOrig.push_back(t);
    }
    if (g_stripOrig.empty()) return false;
    g_stripMode = STRIP_MOVE; g_stripGrab = true; g_stripStarted = true; g_stripX0 = lastMx; g_stripY0 = lastMy;
    g_redraw = true;
    return true;
}
void Timeline::StripApply(int mx) {
    if (g_stripMode == STRIP_NOTHING) return;
    if (!g_stripStarted) {   // arrastre: un umbral para que un click limpio no corra la tira
        const int ddx = mx - g_stripX0; if (ddx * ddx <= (3 * GlobalScale) * (3 * GlobalScale)) return;
        g_stripStarted = true;
    }
    const float fps = (float)(AnimFPS > 0 ? AnimFPS : 30);
    float dSeg = (float)(mx - g_stripX0) / (pxPerFrame > 0.01f ? pxPerFrame : 0.01f) / fps;
    for (size_t i = 0; i < g_stripOrig.size(); i++) {
        const StripOrig& o = g_stripOrig[i]; Sound* s = o.s; if (!s) continue;
        const float tone = s->tone > 0.05f ? s->tone : 0.05f;
        const float dur = s->DurationFile();
        if (g_stripMode == STRIP_MOVE) {
            s->start = o.start + dSeg;
            if (LCtrlPressed) s->start = floorf(s->start * fps + 0.5f) / fps;   // Ctrl: enganchado al frame
        } else if (g_stripMode == STRIP_TRIMSTART) {
            float ri = o.ri + dSeg * tone;
            // Ctrl: la punta IZQUIERDA cae justo en un fotograma (lo que se mueve es su momento en la escena)
            if (LCtrlPressed) { const float t = floorf((o.start + (ri - o.ri) / tone) * fps + 0.5f) / fps; ri = o.ri + (t - o.start) * tone; }
            const float limit = dur - o.rf - 0.01f;
            if (ri < 0.0f) ri = 0.0f; if (ri > limit) ri = limit > 0.0f ? limit : 0.0f;
            s->clipStart = ri;
            s->start = o.start + (ri - o.ri) / tone;   // lo que queda sigue en su lugar (se come la punta)
        } else {
            float rf = o.rf - dSeg * tone;
            // Ctrl: la punta DERECHA cae justo en un fotograma
            if (LCtrlPressed) {
                const float endT = floorf((o.start + (dur - o.ri - rf) / tone) * fps + 0.5f) / fps;
                rf = dur - o.ri - (endT - o.start) * tone;
            }
            const float limit = dur - o.ri - 0.01f;
            if (rf < 0.0f) rf = 0.0f; if (rf > limit) rf = limit > 0.0f ? limit : 0.0f;
            s->clipEnd = rf;
        }
    }
    g_redraw = true;
}
static void StripCloseUndo() {
    UndoStrip* u = new UndoStrip();
    for (size_t i = 0; i < g_stripOrig.size(); i++) {
        const StripOrig& o = g_stripOrig[i];
        if (o.s && (o.s->start != o.start || o.s->clipStart != o.ri || o.s->clipEnd != o.rf)) u->v.push_back(o);
    }
    if (u->v.empty()) { delete u; return; }
    UndoExternal f; f.apply = UndoStripApply; f.unlink = UndoStripUnlink; f.free = UndoStripFree;
    UndoPushExternal(f, u);
}
void Timeline::StripConfirm() {
    if (g_stripMode == STRIP_NOTHING) return;
    StripCloseUndo();
    g_stripMode = STRIP_NOTHING; g_stripGrab = false; g_stripStarted = false; g_stripOrig.clear();
    g_redraw = true;
}
void Timeline::StripCancel() {
    for (size_t i = 0; i < g_stripOrig.size(); i++) {
        const StripOrig& o = g_stripOrig[i]; if (!o.s) continue;
        o.s->start = o.start; o.s->clipStart = o.ri; o.s->clipEnd = o.rf;
    }
    g_stripMode = STRIP_NOTHING; g_stripGrab = false; g_stripStarted = false; g_stripOrig.clear();
    g_redraw = true;
}
// soltar el boton: un ARRASTRE termina ahi; el 'g' sigue hasta el click
void Timeline::StripRelease() {
    if (g_stripMode == STRIP_NOTHING || g_stripGrab) return;
    if (g_stripStarted) StripConfirm();
    else { g_stripMode = STRIP_NOTHING; g_stripOrig.clear(); }
}

// (harness: 'sonido tira') el MISMO camino que el mouse sobre la primera tira: el hover de cada punta (y la zona
// ancha para el dedo), agarrar la punta izquierda con Ctrl (cae justo en un fotograma), correr la tira con Ctrl y
// el Ctrl+Z. false + err si algo no da
bool W3dScriptStripTest(Timeline* t, std::string& err) {
    t->BuildDopeRows();
    int r = -1;
    for (size_t i = 0; i < t->dopeRows.size() && r < 0; i++) if (t->dopeRows[i].sound && t->dopeRows[i].stripRows >= 0) r = (int)i;
    if (r < 0) { err = "tira: no hay una fila de sonido"; return false; }
    Sound* s = (Sound*)t->dopeRows[(size_t)r].sound;
    const float fps = (float)(AnimFPS > 0 ? AnimFPS : 30);
    const int my = t->y + t->stripY + r * t->rowH + t->PosY + t->rowH / 2;
    const int xa = t->x + (int)t->FrameToX(W3dSoundFrameFromSec(s->start));
    const int xb = t->x + (int)t->FrameToX(W3dSoundFrameFromSec(s->start + s->DurationInScene()));
    if (xb - xa < GlobalScale * 40) { err = "tira: la tira es muy corta en pantalla para probar"; return false; }
    t->StripHover(xa + 1, my);                if (g_stripHoverB != STRIP_TRIMSTART) { err = "tira: el hover no marca la punta izquierda"; return false; }
    t->StripHover(xa + GlobalScale * 8, my);  if (g_stripHoverB != STRIP_TRIMSTART) { err = "tira: la zona de la punta es angosta"; return false; }
    t->StripHover(xb - 1, my);                if (g_stripHoverB != STRIP_TRIMEND) { err = "tira: el hover no marca la punta derecha"; return false; }
    t->StripHover((xa + xb) / 2, my);         if (g_stripHoverB != 0) { err = "tira: el medio no es una punta"; return false; }
    const float start0 = s->start, ri0 = s->clipStart;
    const bool lmd = leftMouseDown, ctrl = LCtrlPressed;
    // la punta IZQUIERDA con Ctrl
    if (!t->StripClick(xa + 1, my) || g_stripMode != STRIP_TRIMSTART) { err = "tira: el click no agarro la punta"; leftMouseDown = lmd; return false; }
    leftMouseDown = true; LCtrlPressed = true;
    t->StripApply(xa + 1 + GlobalScale * 23);
    const float f1 = s->start * fps;
    leftMouseDown = false; t->StripRelease();
    printf("      [tira] recorte con Ctrl: inicio %.4f s (frame %.4f) recorteIni %.4f\n", s->start, f1, s->clipStart);
    if (fabsf(f1 - floorf(f1 + 0.5f)) > 1e-3f || s->clipStart <= ri0) { err = "tira: el recorte con Ctrl no cae en un fotograma"; LCtrlPressed = ctrl; return false; }
    // la tira entera con Ctrl
    const int mid = t->x + (int)t->FrameToX(W3dSoundFrameFromSec(s->start + s->DurationInScene() * 0.5f));
    if (!t->StripClick(mid, my) || g_stripMode != STRIP_MOVE) { err = "tira: el click no agarro la tira"; LCtrlPressed = ctrl; return false; }
    leftMouseDown = true;
    t->StripApply(mid + GlobalScale * 31);
    const float f2 = s->start * fps;
    leftMouseDown = false; t->StripRelease();
    LCtrlPressed = ctrl; leftMouseDown = lmd;
    printf("      [tira] mover con Ctrl: inicio %.4f s (frame %.4f)\n", s->start, f2);
    if (fabsf(f2 - floorf(f2 + 0.5f)) > 1e-3f) { err = "tira: mover con Ctrl no cae en un fotograma"; return false; }
    // Ctrl+Z dos veces: como estaba
    UndoUndo(); UndoUndo();
    printf("      [tira] Ctrl+Z x2: inicio %.4f recorteIni %.4f (antes %.4f / %.4f)\n", s->start, s->clipStart, start0, ri0);
    if (fabsf(s->start - start0) > 1e-5f || fabsf(s->clipStart - ri0) > 1e-5f) { err = "tira: el Ctrl+Z no la devolvio"; return false; }
    return true;
}

// ---- EL PANEL DE CURVAS CON EL TECLADO (el N95): como el outliner. Ver Timeline.h (panelFocus) ----
// una fila que se puede enfocar: las que se ven, menos las de continuacion de la tira de un sonido
static bool PanelRowFocusable(const Timeline* tl, int r){
    return r >= 0 && r < (int)tl->dopeVisible && tl->dopeRows[(size_t)r].stripRows >= 0;
}
// la fila enfocada pasa a ser LA elegida (sus keyframes quedan elegidos) y se ve
static void PanelFocusPick(Timeline* tl){
    if (!PanelRowFocusable(tl, tl->panelFocus)) return;
    const Timeline::DopeRow& d = tl->dopeRows[(size_t)tl->panelFocus];
    g_dopeRowSel.clear();
    if (!d.keyRow.empty()) g_dopeRowSel.insert(d.keyRow);
    tl->DopeSelRowSelKeys();
    g_dopeActHas = false;
    // que se vea: el scroll vertical del panel
    const int rh = tl->rowH > 0 ? tl->rowH : 1;
    const int ry = tl->panelFocus * rh + tl->PosY;           // (relativo al comienzo de las filas)
    const int height = tl->height - tl->stripY;
    if (ry < 0) tl->PosY -= ry;
    else if (ry + rh > height) tl->PosY -= (ry + rh - height);
    if (tl->PosY > 0) tl->PosY = 0;
    g_redraw = true;
}
bool Timeline::PanelFocusEnter(int dir){
    BuildDopeRows();
    if (dopeVisible == 0) { panelFocus = -1; return false; }
    // como una lista que DA LA VUELTA pasando por la barra: bajando se entra por la primera (el Summary), subiendo por
    // la ultima
    int r = -1;
    if (dir >= 0) { for (size_t i=0;i<dopeVisible && r<0;i++) if (PanelRowFocusable(this, (int)i)) r = (int)i; }
    else          { for (int i=(int)dopeVisible-1;i>=0 && r<0;i--) if (PanelRowFocusable(this, i)) r = i; }
    if (r < 0) { panelFocus = -1; return false; }
    panelFocus = r;
    barFocusIndex = -1;
    PanelFocusPick(this);
    return true;
}
bool Timeline::PanelFocusMove(int dir){
    if (dopeVisible == 0) { panelFocus = -1; return false; }
    int r = panelFocus;
    for (;;) {
        r += dir;
        if (r < 0 || r >= (int)dopeVisible) return false;        // arriba de la primera / abajo de la ultima: a la barra
        if (PanelRowFocusable(this, r)) break;
    }
    panelFocus = r;
    PanelFocusPick(this);
    return true;
}
void Timeline::PanelFocusCollapse(bool collapse){
    if (!PanelRowFocusable(this, panelFocus)) return;
    const std::string k = dopeRows[(size_t)panelFocus].keyExpansion;
    if (k.empty()) return;
    if (collapse) g_dopeCollapsed.insert(k); else g_dopeCollapsed.erase(k);
    const std::string key = dopeRows[(size_t)panelFocus].keyRow;
    BuildDopeRows();
    // (las filas cambiaron: el foco sigue a la misma)
    for (size_t i=0;i<dopeVisible;i++) if (dopeRows[i].keyRow == key) { panelFocus = (int)i; break; }
    if (panelFocus >= (int)dopeVisible) panelFocus = (int)dopeVisible - 1;
    g_redraw = true;
}
void Timeline::PanelFocusEye(){
    if (!PanelRowFocusable(this, panelFocus)) return;
    const std::string& k = dopeRows[(size_t)panelFocus].keyRow;
    if (k.empty()) return;
    if (DopeEyeOff(k)) g_dopeEyeOff.erase(k); else g_dopeEyeOff.insert(k);
    g_redraw = true;
}
// (el ruteo del keypad, LayoutInput.cpp / el N95)
bool TimelinePanelFocusActive(){
    return g_tlActive && viewPortActive == (ViewportBase*)g_tlActive && g_tlActive->panelFocus >= 0;
}
// el 2 del N95 (como ocultar un objeto en el outliner): el OJO de la fila enfocada
void TimelinePanelEye(){ if (TimelinePanelFocusActive()) g_tlActive->PanelFocusEye(); }
// VERDE + izquierda/derecha con el foco en el panel (el N95): el ANCHO del panel, como arrastrar su borde. En el
// telefono ocupaba mucho y sin mouse no habia como achicarlo
void TimelinePanelWidth(int dx){
    if (!TimelinePanelFocusActive() || dx == 0) return;
    Timeline* tl = g_tlActive;
    int w = (tl->panelWUser > 0 ? tl->panelWUser : tl->panelW) + dx * GlobalScale * 3;
    const int min = IconSizeGS * 2, max = tl->width - IconSizeGS * 4;
    if (w < min) w = min; if (max > min && w > max) w = max;
    tl->panelWUser = w;
    g_redraw = true;
}

// ---- EL MODO DE SELECCION (el boton del MOUSE) y los TRAMOS de curva ----
static PopupMenu* g_tlMenuSelMode = NULL;
static void TL_menuSelActionMode(int id){
    if (id >= TL_SEL_BOTH && id <= TL_SEL_KEYFRAME) { g_tlSelMode = id; g_segmentRow = -1; g_segmentI = -1; }
    g_redraw = true;
}
void Timeline::OpenMenuSelMode(int mx, int my){
    if (!g_tlMenuSelMode){ g_tlMenuSelMode = new PopupMenu(); g_tlMenuSelMode->title = T("Select"); g_tlMenuSelMode->action = TL_menuSelActionMode; }
    g_tlMenuSelMode->Clear();
    g_tlMenuSelMode->Add(T("Both"), TL_SEL_BOTH, IconType::mouse)->green = (g_tlSelMode == TL_SEL_BOTH);
    g_tlMenuSelMode->Add(T("Curve segment"), TL_SEL_CURVE, IconType::interp_bezier)->green = (g_tlSelMode == TL_SEL_CURVE);
    g_tlMenuSelMode->Add(T("Keyframe"), TL_SEL_KEYFRAME, IconType::keyframe)->green = (g_tlSelMode == TL_SEL_KEYFRAME);
    if (OpenMenu && OpenMenu != g_tlMenuSelMode) OpenMenu->Close();
    g_tlMenuSelMode->Open(mx, my, MenuScreenW, MenuScreenH);
    OpenMenu = g_tlMenuSelMode;
}
// elegir el TRAMO i (entre el keyframe i y el i+1) de la fila: sus dos keyframes; el de la izquierda queda ACTIVO
// (la tarjeta Keyframe y la interpolacion del tramo son de el). sumar = Shift: se agrega a lo que habia
void Timeline::DopeSegmentPick(int row, int i, bool add){
    if (row < 0 || row >= (int)dopeRows.size()) return;
    const DopeRow& d = dopeRows[(size_t)row];
    AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId, false);
    if (!ap || i < 0 || i + 1 >= (int)ap->keyframes.size()) return;
    if (!add) g_dopeKeySel.clear();
    const int f0 = ap->keyframes[(size_t)i].frame, f1 = ap->keyframes[(size_t)i + 1].frame;
    g_dopeKeySel.insert(DopeKeyId(d.ownerKey, d.propId, d.compId, f0));
    g_dopeKeySel.insert(DopeKeyId(d.ownerKey, d.propId, d.compId, f1));
    g_dopeActOwner = d.ownerKey; g_dopeActProp = d.propId; g_dopeActComp = d.compId; g_dopeActFrame = f0; g_dopeActHas = true;
    g_segmentRow = row; g_segmentI = i;
    g_redraw = true;
}
// el lapiz en modo CURVA: el tramo siguiente / anterior de las curvas que se recorren (las elegidas en el panel; sin
// ninguna, todas): curva por curva, de izquierda a derecha
bool Timeline::DopeSegmentAdvance(int step, bool extend){
    std::vector<int> rows; DopeRowsNav(rows);
    std::vector<std::pair<int,int> > segments;
    for (size_t k=0;k<rows.size();k++){
        const DopeRow& d = dopeRows[(size_t)rows[k]];
        if (d.ghost || DopeVertexAnimOf(d.ownerKey)) continue;
        AnimProperty* ap = DopeResolveProp(d.ownerKey, d.propId, d.compId, false);
        if (!ap) continue;
        for (size_t i=0; i+1<ap->keyframes.size(); i++) segments.push_back(std::make_pair(rows[k], (int)i));
    }
    if (segments.empty()) return false;
    int act = -1;
    for (size_t t=0;t<segments.size();t++) if (segments[t].first == g_segmentRow && segments[t].second == g_segmentI){ act = (int)t; break; }
    int next = (act < 0) ? 0 : act + step;
    if (next < 0) next = (int)segments.size() - 1;
    if (next >= (int)segments.size()) next = 0;
    DopeSegmentPick(segments[(size_t)next].first, segments[(size_t)next].second, extend);
    return true;
}

// ---- LAS TIRAS DE SONIDO CON EL TECLADO (el lapiz, el 1 y las flechas del telefono) y la TIJERA ----
int Timeline::StripRowOf(const Sound* s) const {
    if (!s) return -1;
    for (size_t r = 0; r < dopeVisible && r < dopeRows.size(); r++)
        if (dopeRows[r].sound == (const Object*)s && dopeRows[r].stripRows >= 0 && !dopeRows[r].hidden) return (int)r;
    return -1;
}
bool Timeline::StripHas() const {
    for (size_t r = 0; r < dopeVisible && r < dopeRows.size(); r++)
        if (dopeRows[r].sound && dopeRows[r].stripRows >= 0 && !dopeRows[r].hidden) return true;
    return false;
}
bool Timeline::DopeNoKeysNav() const { std::vector<int> fr; DopeFramesNav(fr); return fr.empty(); }
// el lapiz va a las tiras: hay tiras y (ya hay una parte elegida, o la fila elegida en el panel es de un sonido, o no
// hay keyframes que recorrer)
bool Timeline::StripNavPrefers() const {
    if (!StripHas()) return false;
    if (g_stripSelS && StripRowOf(g_stripSelS) >= 0) return true;
    for (size_t r = 0; r < dopeVisible && r < dopeRows.size(); r++)
        if (dopeRows[r].sound && g_dopeRowSel.count(dopeRows[r].keyRow)) return true;
    return DopeNoKeysNav();
}
// punta izquierda -> el sonido entero -> punta derecha -> (el sonido que sigue) ...
bool Timeline::StripSelAdvance(int step) {
    std::vector<Sound*> ss;
    // UN SOLO SONIDO si su fila esta elegida en el panel (como una curva); con el Summary o nada elegido, todos
    Object* onlyThis = NULL;
    for (size_t r = 0; r < dopeVisible && r < dopeRows.size() && !onlyThis; r++)
        if (dopeRows[r].sound && g_dopeRowSel.count(dopeRows[r].keyRow)) onlyThis = dopeRows[r].sound;
    for (size_t r = 0; r < dopeVisible && r < dopeRows.size(); r++) {
        const DopeRow& d = dopeRows[r];
        if (!d.sound || d.stripRows < 0 || d.hidden) continue;
        if (onlyThis && d.sound != onlyThis) continue;
        bool already = false; for (size_t i = 0; i < ss.size(); i++) already = already || (ss[i] == (Sound*)d.sound);
        if (!already) ss.push_back((Sound*)d.sound);
    }
    if (ss.empty()) return false;
    static const int kParts[3] = { STRIP_TRIMSTART, STRIP_MOVE, STRIP_TRIMEND };
    int act = -1;
    for (size_t i = 0; i < ss.size() && act < 0; i++)
        for (int k = 0; k < 3; k++) if (ss[i] == g_stripSelS && kParts[k] == g_stripSelPart) act = (int)i * 3 + k;
    const int n = (int)ss.size() * 3;
    int next = (act < 0) ? 0 : act + step;
    if (next < 0) next = n - 1; if (next >= n) next = 0;
    g_stripSelS = ss[(size_t)(next / 3)]; g_stripSelPart = kParts[next % 3];
    g_dopeKeySel.clear(); g_dopeActHas = false;   // (el 1 agarra la tira, no keyframes)
    g_redraw = true;
    return true;
}
// las flechas con la tira agarrada: la corren / recortan como si se arrastrara (dx en pixeles de pantalla)
void TimelineStripKey(float dx) {
    if (!g_tlActive || !g_tlActive->StripActive()) return;
    g_stripKeyAccum += dx;
    g_tlActive->StripApply(g_stripX0 + (int)g_stripKeyAccum);
}
bool TimelineStripActive() { return g_tlActive && g_tlActive->StripActive() && g_stripGrab; }
// el sonido que corta la TIJERA: el elegido con el lapiz, o el unico sonido seleccionado; y que el frame actual caiga
// adentro de su tira (si no, no hay nada que cortar)
// el SONIDO elegido: el de la parte elegida con el lapiz, o el objeto activo si es un sonido con su tira a la vista
Sound* Timeline::PickedStrip() const {
    if (g_stripSelS && StripRowOf(g_stripSelS) >= 0) return g_stripSelS;
    if (ActiveObj && ActiveObj->getType() == ObjectType::sound && StripRowOf((Sound*)ActiveObj) >= 0) return (Sound*)ActiveObj;
    return NULL;
}
Sound* Timeline::StripToCut() const {
    Sound* s = PickedStrip();
    if (!s) return NULL;
    const float t = W3dSoundSecFromFrame((float)CurrentFrame);
    const float inEsc = s->DurationInScene();
    if (inEsc <= 0.0f || t <= s->start + 0.01f || t >= s->start + inEsc - 0.01f) return NULL;
    return s;
}
// LA TIJERA (trim): el sonido queda en DOS en el frame actual. El original termina ahi; su COPIA (otro objeto Sonido,
// con todo lo mismo) arranca ahi y sigue con el resto. Un solo Ctrl+Z deshace los dos pasos
bool Timeline::StripCut() {
    Sound* s = StripToCut();
    if (!s) {
        if (PickedStrip()) { extern void Notify(const std::string&, bool); Notify(T("Move the current frame inside the sound to cut it"), true); }
        return false;
    }
    const float t = W3dSoundSecFromFrame((float)CurrentFrame);
    const float tone = s->tone > 0.05f ? s->tone : 0.05f;
    const float dur = s->DurationFile();
    const float cut = s->clipStart + (t - s->start) * tone;   // (en segundos del ARCHIVO)
    if (dur <= 0.0f || cut <= s->clipStart || cut >= dur - s->clipEnd) return false;
    const float rfOrig = s->clipEnd;   // (la copia termina donde terminaba el original)
    UndoGroupStart();
    // el ORIGINAL: termina en el corte (su undo: el de las tiras)
    { UndoStrip* u = new UndoStrip(); StripOrig o = { s, s->start, s->clipStart, s->clipEnd }; u->v.push_back(o);
      UndoExternal f; f.apply = UndoStripApply; f.unlink = UndoStripUnlink; f.free = UndoStripFree;
      s->clipEnd = dur - cut;
      UndoPushExternal(f, u); }
    // la COPIA: arranca en el corte
    extern Object* W3dDuplicateOne(Object* src, bool linked);
    Sound* c = (Sound*)W3dDuplicateOne(s, false);
    if (c) {
        c->clipStart = cut;
        c->clipEnd   = rfOrig;
        c->start = t;
        DeselectAll();
        c->Select();
        UndoCaptureCreation();   // (solo la copia es "creada": el Ctrl+Z la saca)
        // LOS DOS quedan elegidos (el timeline muestra lo elegido: con solo la copia, el original desaparecia y parecia
        // borrado). La copia queda ACTIVA
        s->Select(); c->Select();
        g_stripSelS = c; g_stripSelPart = STRIP_MOVE;
    }
    UndoGroupEnd();
    BuildDopeRows();
    g_redraw = true;
    return true;
}

