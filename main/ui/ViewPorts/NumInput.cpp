// ===================================================================================================
//  ENTRADA NUMERICA / FORMULAS durante un transform (compartida 4 OS). Extraido de LayoutInput (Fase 2).
// ===================================================================================================
#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "Undo.h" // Ctrl+Z: capturar modo / seleccion
#include "ViewPorts/PopUp/ConfirmarPopup.h" // AbrirConfirmarBorrado (popup de confirmar borrado)
#include "ViewPorts/LayoutInput.h"
#include "ViewPorts/PoseTransform.h" // Pose Mode transform (extraido a su propio archivo)
#include "ViewPorts/Notificaciones.h" // toasts (extraido a su propio archivo)
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
#include "objects/Textures.h"
#include "variables.h"
#include "render/OpcionesRender.h" // g_fpsActual
#include "ViewPorts/PopUp/PopUpBase.h"
#include "ViewPorts/PopUp/RedoMeshPanel.h"
#include "WhiskUI/widgets/card.h"        // tarjeta de las notificaciones
#include "WhiskUI/text/bitmapText.h"  // texto de las notificaciones
#include "WhiskUI/draw/icons.h"       // iconos notifOk / notifError
#include "WhiskUI/theme/colores.h"     // ColorID
#include "w3dlog.h"         // las notificaciones tambien van al log
#include "ViewPorts/NumInput.h"

// ====================================================================
//  ENTRADA NUMERICA / FORMULAS durante un transform (COMPARTIDA 4 OS)
// ====================================================================
// Mientras se mueve/rota/escala, el usuario puede TIPEAR un valor exacto en vez de
// hacerlo a ojo con el mouse: numeros, punto decimal, parentesis y * / + (formulas
// tipo "(3+10)/2"). El '-' alterna el signo (no es resta). Es PLATFORM-AGNOSTIC: el
// nucleo (buffer + evaluador + apply) vive aca; cada SO alimenta los caracteres por
// NumInputChar (PC desde SDL_TEXTINPUT; Symbian desde su teclado/keypad).
static std::string gNumBuf;       // la expresion tipeada (sin el signo)
static bool        gNumActivo = false;
static bool        gNumNegar = false; // el toggle del '-'
static int         gNumCaret = 0;     // posicion del caret en gNumBuf (para editar en el medio; teclado tactil ← →)

// --- evaluador de expresiones (+ * / parentesis, numeros con punto). Parse de
//     numero MANUAL (sin strtod) para no depender del locale (',' vs '.'). ---
struct ExprP { const char* s; bool ok; };
static float ExprExpr(ExprP& p);
static void ExprSkip(ExprP& p){ while(*p.s==' ') p.s++; }
static float ExprNum(ExprP& p){
    ExprSkip(p);
    if (*p.s=='('){ p.s++; float v=ExprExpr(p); ExprSkip(p); if(*p.s==')') p.s++; else p.ok=false; return v; }
    if (*p.s=='+'){ p.s++; return ExprNum(p); }
    if (*p.s=='-'){ p.s++; return -ExprNum(p); }
    bool any=false; float ip=0;
    while(*p.s>='0'&&*p.s<='9'){ ip=ip*10.0f+(float)(*p.s-'0'); p.s++; any=true; }
    if (*p.s=='.'){ p.s++; float sc=1.0f; while(*p.s>='0'&&*p.s<='9'){ sc*=0.1f; ip+=(float)(*p.s-'0')*sc; p.s++; any=true; } }
    if (!any){ p.ok=false; return 0.0f; }
    return ip;
}
static float ExprTerm(ExprP& p){
    float v=ExprNum(p);
    for(;;){ ExprSkip(p); char c=*p.s;
        if(c=='*'){ p.s++; v*=ExprNum(p); }
        else if(c=='/'){ p.s++; float d=ExprNum(p); v = (d!=0.0f)? v/d : 0.0f; }
        else break; }
    return v;
}
static float ExprExpr(ExprP& p){
    float v=ExprTerm(p);
    for(;;){ ExprSkip(p); char c=*p.s;
        if(c=='+'){ p.s++; v+=ExprTerm(p); }
        else if(c=='-'){ p.s++; v-=ExprTerm(p); }
        else break; }
    return v;
}
static bool EvalExpr(const std::string& str, float& out){
    if (str.empty()){ out=0.0f; return true; }
    ExprP p; p.s=str.c_str(); p.ok=true;
    float v=ExprExpr(p); ExprSkip(p);
    if (!p.ok || *p.s!='\0') return false; // expresion incompleta/invalida
    out=v; return true;
}
// mismo parser, expuesto: lo usa la entrada numerica del TIMELINE (mover/escalar keyframes con matematica)
bool W3dEvalExpr(const std::string& str, float& out){ return EvalExpr(str, out); }

// CAJA DE TEXTO EDITABLE: el campo enfocado + el ruteo de caracteres (compartido).
TextField* g_textFieldActivo = NULL;
// edicion numerica por texto de un PropFloat (WhiskUI/PropFloat): Enter aplica, Cancel descarta.
extern bool NumEditActivo();
extern void NumEditCommit();
extern void NumEditCancel();
bool TextFieldInputChar(int c){
    if (!g_textFieldActivo) return false;
    if (c == 8)       g_textFieldActivo->Backspace();
    else if (c == 127)g_textFieldActivo->DelForward();
    else if (c >= 32 && c < 127) g_textFieldActivo->InsertChar(c); // ASCII imprimible
    else return false;
    g_redraw = true;
    return true;
}

bool NumInputActivo(){ return gNumActivo; }
void NumInputReset(){ gNumBuf.clear(); gNumActivo=false; gNumNegar=false; gNumCaret=0; }
const std::string& NumInputBuffer(){ return gNumBuf; }
int  NumInputCaret(){ return gNumCaret; }                 // posicion del caret (para dibujarlo en la barra)
void NumInputLeft(){  if (gNumCaret>0) { gNumCaret--; g_redraw=true; } }
void NumInputRight(){ if (gNumCaret<(int)gNumBuf.size()) { gNumCaret++; g_redraw=true; } }
bool NumInputNegado(){ return gNumNegar; }
// valor actual (false si la expresion esta incompleta -> no aplicar todavia)
bool NumInputValor(float& out){
    float v; if (!EvalExpr(gNumBuf, v)) return false;
    out = gNumNegar ? -v : v; return true;
}

// aplica el valor exacto al transform en curso (malla o objeto)
static void NumInputAplicar(){
    if (!gNumActivo) return;
    float v; if (!NumInputValor(v)) return; // incompleta: espero mas caracteres
    if (InteractionMode == EditMode && EditXformActivo()) EditXformNumValor(v);
    else if (InteractionMode == PoseMode){ extern void PoseXformNumValor(float); PoseXformNumValor(v); }
    else                                                  SetTransformNumerico(v);
}

// COMPARTIDA: cada plataforma alimenta los caracteres tipeados aca. Devuelve true si
// lo consumio (hay un transform activo y el caracter es relevante). c==8 = backspace.
bool NumInputChar(int c){
    if (estado == editNavegacion) return false; // solo durante un transform
    if (c == 8){ // backspace: borra el char ANTES del caret (o el signo si esta vacio)
        if (gNumCaret > 0 && !gNumBuf.empty()) { gNumBuf.erase(gNumCaret-1, 1); gNumCaret--; }
        else if (gNumBuf.empty()) gNumNegar = false;
    } else if (c == '-'){
        gNumNegar = !gNumNegar; gNumActivo = true;
    } else if ((c>='0'&&c<='9') || c=='.' || c=='(' || c==')' || c=='*' || c=='/' || c=='+'){
        if (gNumCaret < 0) gNumCaret = 0; if (gNumCaret > (int)gNumBuf.size()) gNumCaret = (int)gNumBuf.size();
        gNumBuf.insert(gNumBuf.begin()+gNumCaret, (char)c); gNumCaret++; gNumActivo = true; // inserta EN el caret
    } else {
        return false; // no es un caracter numerico
    }
    if (gNumBuf.empty() && !gNumNegar) gNumActivo = false; // se vacio: vuelve el mouse
    NumInputAplicar();
    g_redraw = true;
    return true;
}

// El teclado tactil (NumPad en modo transform) llama a estas para editar el transform
// que se muestra ARRIBA (sin popup/textinput propio), igual que un teclado fisico en PC.
void NumInputBegin(){ // marca la entrada activa asi la barra muestra "[|]" apenas se abre el teclado
    if (!gNumActivo){ gNumBuf.clear(); gNumCaret=0; gNumNegar=false; gNumActivo=true; }
    g_redraw = true;
}
void NumInputConfirmar(){ // OK del teclado: confirma el transform (Aceptar ya hace NumInputReset)
    if (Viewport3DActive) Viewport3DActive->Aceptar();
    g_redraw = true;
}
void NumInputCancelar(){ // X del teclado: descarta el transform (mismo camino que la cruz de la barra)
    if (InteractionMode == EditMode && EditXformActivo()) EditXformCancelar();
    else Cancelar(); // objeto: descarta el transform (free function de ObjectMode)
    NumInputReset();
    g_redraw = true;
}
// hay un transform en curso? (mismo criterio que la barra de estado que lo muestra). El teclado
// tactil en modo transform se cierra solo si esto deja de ser cierto (se confirmo/cancelo).
bool NumInputTransformEnCurso(){
    if (!Viewport3DActive) return false;
    if (!(estado == translacion || estado == rotacion || estado == EditScale)) return false;
    if (InteractionMode == ObjectMode) return true;
    if (InteractionMode == EditMode && EditXformActivo()) return true;
    if (InteractionMode == PoseMode && PoseHeaderModo()) return true;
    return false;
}
