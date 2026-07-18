// ===================================================================================================
//  NOTIFICACIONES (toasts). Extraido de LayoutInput.cpp (Fase 2 del reorg). Ver Notificaciones.h.
//  Es una HOJA: todo el editor llama Notificar(), pero esto no llama nada del resto de LayoutInput.
// ===================================================================================================
#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "Undo.h" // Ctrl+Z: capturar modo / seleccion
#include "ViewPorts/PopUp/ConfirmarPopup.h" // AbrirConfirmarBorrado (popup de confirmar borrado)
#include "ViewPorts/LayoutInput.h"
#include "ViewPorts/PoseTransform.h" // Pose Mode transform (extraido a su propio archivo)
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
#include "ViewPorts/Notificaciones.h"

// ====================================================================
//  NOTIFICACIONES (toasts) — alineadas con el log. Tarjetas abajo a la izquierda
//  con icono verde (OK) o cruz roja (error) + texto. Las de EXITO se cierran solas
//  a los pocos segundos; las de ERROR quedan hasta la 'x' en PC (en Symbian se auto-cierran).
//  El nuevo va ARRIBA (se apilan en columna). Compartido 4 OS.
// ====================================================================
struct Notif {
    std::string msg;
    bool error;
    bool hint;            // cartel-tutorial (azul, persistente, sin 'x'): se cierra por codigo
    float ttl;            // segundos restantes; <= 0 = no se auto-cierra (error en PC, y los hints)
    int rx, ry, rw, rh;   // rect calculado al render
    int xx, xy, xw, xh;   // rect de la 'x' (cerrar), solo error
};
static std::vector<Notif> gNotifs;
static Card* gNotifCard = NULL;
static int  gNotifMx = -1, gNotifMy = -1; // mouse (para el hover de la 'x')
static bool gNotifSobreX = false;         // estaba sobre alguna 'x' el frame previo

void Notificar(const std::string& msg, bool error) {
    w3dLog(msg.c_str());  // tambien al log de diagnostico
    Notif n; n.msg = msg; n.error = error; n.hint = false;
    // TODAS las plataformas: se auto-cierran con un timer (como Symbian). El error dura mas para alcanzar
    // a leerlo. Ademas se pueden cerrar tocando el mensaje (NotificacionesClick). (Dante: exito 6s, error 10s.)
    n.ttl = error ? 10.0f : 6.0f;
    n.rx=n.ry=n.rw=n.rh=n.xx=n.xy=n.xw=n.xh=0;
    gNotifs.insert(gNotifs.begin(), n); // el NUEVO arriba de todo
    if (gNotifs.size() > 8) gNotifs.pop_back();
    g_redraw = true;
}

// cartel-tutorial (azul, persistente): reemplaza el hint anterior. Lo usa el modo guiado de
// "Pick Shortest Path" para ir pidiendo "click the first..." / "...the second...".
void NotificarHint(const std::string& msg) {
    for (size_t i=0;i<gNotifs.size();) { if (gNotifs[i].hint) gNotifs.erase(gNotifs.begin()+i); else i++; }
    Notif n; n.msg = msg; n.error = false; n.hint = true; n.ttl = 0.0f;
    n.rx=n.ry=n.rw=n.rh=n.xx=n.xy=n.xw=n.xh=0;
    gNotifs.insert(gNotifs.begin(), n);
    g_redraw = true;
}
void NotificarHintClear() {
    bool habia = false;
    for (size_t i=0;i<gNotifs.size();) { if (gNotifs[i].hint) { gNotifs.erase(gNotifs.begin()+i); habia=true; } else i++; }
    if (habia) g_redraw = true;
}

void NotificacionesTick(float dt) {
    bool hayTimer = false;
    for (size_t i = 0; i < gNotifs.size(); ) {
        // ttl > 0 = se auto-cierra (exito siempre; error solo en Symbian, donde no hay
        // como tocar la 'x'). ttl <= 0 = persistente (error en PC, y los hints).
        if (!gNotifs[i].hint && gNotifs[i].ttl > 0.0f) {
            gNotifs[i].ttl -= dt;
            if (gNotifs[i].ttl <= 0.0f) { gNotifs.erase(gNotifs.begin()+i); g_redraw = true; continue; }
            hayTimer = true;
        }
        i++;
    }
    if (hayTimer) g_redraw = true; // seguir renderizando para que corra el timer
}

// click sobre una notificacion -> la cierra. Se cierra tocando CUALQUIER parte del mensaje (no solo la 'x',
// que en tactil es dificil de embocar). Los hints (tutorial guiado) NO se cierran a mano (los maneja el codigo).
// true si lo consumio.
bool NotificacionesClick(int mx, int my) {
    for (size_t i = 0; i < gNotifs.size(); i++) {
        Notif& n = gNotifs[i];
        if (n.hint) continue; // el cartel-tutorial se cierra por codigo, no con el toque
        if (mx >= n.rx && mx < n.rx + n.rw && my >= n.ry && my < n.ry + n.rh) {
            gNotifs.erase(gNotifs.begin()+i); g_redraw = true; return true;
        }
    }
    return false;
}

// mouse sobre las notifs: guarda la pos para el hover de la 'x' (gris->blanca).
// Solo re-renderiza al entrar/salir de una 'x' (no en cada pixel de movimiento).
void NotificacionesMotion(int mx, int my) {
    gNotifMx = mx; gNotifMy = my;
    bool sobre = false;
    for (size_t i = 0; i < gNotifs.size(); i++) {
        Notif& n = gNotifs[i];
        if (n.error && mx >= n.xx && mx < n.xx + n.xw && my >= n.xy && my < n.xy + n.xh) { sobre = true; break; }
    }
    if (sobre != gNotifSobreX) { gNotifSobreX = sobre; g_redraw = true; }
}

// envuelve un texto a <= maxLin lineas de a lo sumo 'cpl' caracteres (fuente monoespaciada). Corta por
// palabras; una palabra mas larga que la linea se parte. Si no entra todo, la ultima linea termina en "..."
static std::vector<std::string> NotifWrap(const std::string& s, int cpl, int maxLin){
    std::vector<std::string> out;
    if (cpl < 1) cpl = 1;
    if (maxLin < 1) maxLin = 1;
    std::vector<std::string> words; std::string w;
    for (size_t i = 0; i < s.size(); i++){
        char c = s[i];
        if (c == ' ' || c == '\n' || c == '\t'){ if (!w.empty()){ words.push_back(w); w.clear(); } }
        else w += c;
    }
    if (!w.empty()) words.push_back(w);
    std::string cur; bool corte = false;
    for (size_t wi = 0; wi < words.size() && !corte; wi++){
        std::string word = words[wi];
        while ((int)word.size() > cpl){                 // palabra mas larga que la linea: partirla
            if (!cur.empty()){ out.push_back(cur); cur.clear(); }
            if ((int)out.size() >= maxLin){ corte = true; break; }
            out.push_back(word.substr(0, cpl));
            word = word.substr(cpl);
        }
        if (corte) break;
        if (cur.empty()) cur = word;
        else if ((int)(cur.size() + 1 + word.size()) <= cpl) cur += " " + word;
        else {
            out.push_back(cur); cur.clear();
            if ((int)out.size() >= maxLin){ corte = true; break; }
            cur = word;
        }
    }
    if (!corte && !cur.empty() && (int)out.size() < maxLin) out.push_back(cur);
    else if (!cur.empty()) corte = true; // quedo texto sin colocar -> se trunco
    if (corte && !out.empty()){          // marcar el recorte con "..."
        std::string& last = out.back();
        int room = cpl - 3; if (room < 0) room = 0;
        if ((int)last.size() > room) last = last.substr(0, room);
        last += "...";
    }
    if (out.empty()) out.push_back("");
    return out;
}

void NotificacionesRender(int screenW, int screenH) {
    if (gNotifs.empty()) return;
    if (!gNotifCard) gNotifCard = new Card(NULL, 10, 10);

    w3dEngine::Viewport(0, 0, screenW, screenH);
    w3dEngine::Disable(w3dEngine::ScissorTest);
    w3dEngine::MatrixMode(w3dEngine::Projection); w3dEngine::LoadIdentity();
    w3dEngine::Ortho(0, screenW, screenH, 0, -1, 1);
    w3dEngine::MatrixMode(w3dEngine::ModelView); w3dEngine::LoadIdentity();
    w3dEngine::Disable(w3dEngine::DepthTest); w3dEngine::Disable(w3dEngine::Lighting); w3dEngine::Disable(w3dEngine::Fog);
    w3dEngine::Enable(w3dEngine::Blend); w3dEngine::BlendAlpha();
    w3dEngine::EnableArray(w3dEngine::VertexArray); w3dEngine::EnableArray(w3dEngine::TexCoordArray);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    if (!Textures.empty()) w3dEngine::BindTexture(Textures[0]->iID);

    const float* gris   = ListaColores[static_cast<int>(ColorID::gris)];
    const float* grisUI = ListaColores[static_cast<int>(ColorID::grisUI)];
    const float* accent = ListaColores[static_cast<int>(ColorID::accent)];
    const float* blanco = ListaColores[static_cast<int>(ColorID::blanco)];
    const float rojo[3] = { 0.92f, 0.28f, 0.24f };
    const float azul[3] = { 0.30f, 0.60f, 0.95f }; // hint (tutorial)

    const int margin = gapGS * 2;
    const int pad    = gapGS;
    const int lineH  = RenglonHeightGS;   // alto de cada linea de texto
    const int xBtnW  = RenglonHeightGS;
    int yBottom = screenH - margin;       // borde inferior de la pila (el mas viejo va abajo); sube hacia arriba

    for (int i = (int)gNotifs.size() - 1; i >= 0; i--) { // viejo abajo -> nuevo arriba
        Notif& n = gNotifs[i];
        const float* col = n.error ? rojo : (n.hint ? azul : accent);
        int textW  = (int)n.msg.size() * CharacterWidthGS;
        int maxCardW = screenW - margin*2;
        int cardW  = pad + IconSizeGS + gapGS + textW + pad + (n.error ? xBtnW : 0);
        bool wrap = false;
        if (cardW > maxCardW){ cardW = maxCardW; wrap = true; } // no cabe en 1 linea -> envolver
#ifdef W3D_SYMBIAN
        cardW = maxCardW; wrap = true;                          // N95: ancho COMPLETO (poca resolucion)
#endif
        int txtMax = cardW - pad - IconSizeGS - gapGS - pad - (n.error ? xBtnW : 0);
        // hasta 3 lineas (pedido Dante): un mensaje largo NUNCA se recorta a media palabra sin avisar
        std::vector<std::string> lineas;
        if (wrap && CharacterWidthGS > 0) lineas = NotifWrap(n.msg, txtMax / CharacterWidthGS, 3);
        else lineas.push_back(n.msg);
        int nlin  = (int)lineas.size(); if (nlin < 1) nlin = 1;
        int cardH = lineH * nlin + pad * 2;

        int x = margin;
        int y = yBottom - cardH;                 // top de esta card
        n.rx=x; n.ry=y; n.rw=cardW; n.rh=cardH;

        // fondo + borde de color
        w3dEngine::Enable(w3dEngine::Texture2D);
        w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)x, (GLfloat)y, 0);
        gNotifCard->Resize(cardW, cardH);
        w3dEngine::Color4f(gris[0], gris[1], gris[2], 0.96f);  gNotifCard->RenderObject(false);
        w3dEngine::Color4f(col[0], col[1], col[2], 1.0f);      gNotifCard->RenderBorder(false);
        w3dEngine::PopMatrix();

        // icono (tinte rojo/verde) a la izquierda, alineado con la PRIMERA linea
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)(x + pad), (GLfloat)(y + pad + (lineH - IconSizeGS)/2), 0);
        w3dEngine::Color4f(col[0], col[1], col[2], 1.0f);
        int icon = n.error ? (int)IconType::notifError : (int)IconType::notifOk;
        W3dDrawStrip4(IconMesh, IconsUV[icon]->uvs);
        w3dEngine::PopMatrix();

        // texto (1 a 3 lineas)
        w3dEngine::Color4f(blanco[0], blanco[1], blanco[2], 1.0f);
        for (int L = 0; L < nlin; L++){
            w3dEngine::PushMatrix();
            w3dEngine::Translatef((GLfloat)(x + pad + IconSizeGS + gapGS), (GLfloat)(y + pad + L*lineH), 0);
            RenderBitmapText(lineas[L], textAlign::left, txtMax);
            w3dEngine::PopMatrix();
        }

        // 'x' de cerrar (solo error): chica, en la ESQUINA superior derecha.
        // gris por defecto; blanca al pasar el mouse. El glyph 'x' del font tiene
        // su tinta (5px) ABAJO del cell (11px) con pixeles vacios arriba, asi que
        // lo subo esa cantidad para que quede pegado a la esquina superior.
        if (n.error) {
            int bw = CharacterWidthGS + gapGS;          // ancho clickeable
            int bx = x + cardW - bw - gapGS / 2 + 1;    // pegada a la derecha (+1px: no tocar el borde)
            int inset = GlobalScale * 2;                // separacion chica del borde
            int vacioArriba = LetterHeightGS - 5 * GlobalScale; // px vacios sobre la 'x'
            int byGlyph = y + inset - vacioArriba + 7;  // baja la 'x' de la esquina (+7px: no tocar el borde superior)
            n.xx = bx; n.xy = y; n.xw = bw; n.xh = RenglonHeightGS; // hit-rect del corner
            bool hover = (gNotifMx >= n.xx && gNotifMx < n.xx + n.xw &&
                          gNotifMy >= n.xy && gNotifMy < n.xy + n.xh);
            const float* cx = hover ? blanco : grisUI;
            w3dEngine::PushMatrix(); w3dEngine::Translatef((GLfloat)bx, (GLfloat)byGlyph, 0);
            w3dEngine::Color4f(cx[0], cx[1], cx[2], 1.0f);
            RenderBitmapText("x", textAlign::center, bw);
            w3dEngine::PopMatrix();
        }
        yBottom = y - gapGS; // la proxima (mas nueva) va ARRIBA de esta
    }
}
