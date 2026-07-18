#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "W3dLang.h"   // T(): los textos salen en el idioma del sistema
#include "RedoMeshPanel.h"
#include "objects/Mesh.h"          // Mesh + Regenerar + MeshType
#include "ViewPorts/LayoutInput.h" // LayoutKey
#include "ViewPorts/ViewPort3D.h"  // Viewport3DActive (rect del viewport que crea)
#include "WhiskUI/widgets/PopupMenu.h"          // MenuPantallaW / MenuPantallaH (tamano ventana)
#include "NumPad.h"                     // teclado numerico (editar el valor por texto/formula)

// la malla que el panel esta editando + un espejo float de los vertices (los
// PropFloat trabajan con float*; meshVerts es int). onChange unico para todos.
static Mesh* gRedoMesh = NULL;
static float gRedoVertsF = 8.0f;   // circulo: vertices / esfera: segments
static float gRedoVerts2F = 8.0f;  // esfera: rings
static bool  gRedoInside = false;  // Recalculate Normals: tilde "Inside"
static float gRedoCortesF = 1.0f;  // Loop Cut: Number of Cuts (espejo float)
static float gRedoFactorF = 0.0f;  // Loop Cut: Factor (-1..1)
static RedoMeshPanel* gRedoPanel = NULL;

static void RedoOnChange(){
    if (!gRedoMesh) return;
    int v = (int)(gRedoVertsF + 0.5f);   if (v < 3) v = 3;
    int v2 = (int)(gRedoVerts2F + 0.5f); if (v2 < 2) v2 = 2;
    gRedoMesh->meshVerts = v;
    gRedoMesh->meshVerts2 = v2;
    gRedoMesh->Regenerar(); // reposiciona (tamano) o recrea (vertices) en vivo
}

// Recalculate Normals: al togglear "Inside" re-corre el recalculo. Es determinista
// (orienta segun el centro), asi que togglear da vuelta limpio las caras objetivo.
static void RedoNormalesOnChange(){
    if (!gRedoMesh) return;
    gRedoMesh->RecalcularOrientacionEdit(gRedoInside);
}

// Loop Cut: cambiar cortes o factor -> re-corta desde el snapshot (en el modulo de loop cut)
static void RedoLoopCutOnChange(){
    int cortes = (int)(gRedoCortesF + 0.5f); if (cortes < 1) cortes = 1;
    LoopCutRedoAplicar(cortes, gRedoFactorF);
}

RedoMeshPanel::RedoMeshPanel(Mesh* m, int modo)
    : PopUpBase("Add"), grupo(NULL), dragField(NULL), dragMoved(false), lastDragMx(0) {
    gRedoMesh = m;

    if (modo == 1) { // Recalculate Normals: una tarjeta con la tilde "Inside"
        grupo = new GroupPropertie(T("Recalculate Normals"));
        grupo->anchoValores = 0.5f;
        gRedoInside = false;
        PropBool* inside = new PropBool(T("Inside"));
        inside->value = &gRedoInside; inside->onChange = RedoNormalesOnChange;
        grupo->properties.push_back(inside);
        grupo->selectIndex = 0;
        vpCreador = Viewport3DActive;
        ResizeGrupo();
        Reubicar();
        return;
    }

    if (modo == 2) { // Loop Cut and Slide: Number of Cuts + Factor
        grupo = new GroupPropertie(T("Loop Cut and Slide"));
        grupo->anchoValores = 0.5f;
        gRedoCortesF = (float)LoopCutGetCortes();
        gRedoFactorF = LoopCutGetFactor();
        PropFloat* cuts = new PropFloat(T("Number of Cuts"));
        cuts->value = &gRedoCortesF;
        cuts->stepFino = 1.0f; cuts->stepGrueso = 1.0f; cuts->dragStep = 0.1f;
        cuts->SetRango(1.0f, 32.0f); cuts->onChange = RedoLoopCutOnChange;
        cuts->entero = true; cuts->centrado = true; cuts->flechas = true;
        grupo->properties.push_back(cuts);
        PropFloat* fac = new PropFloat(T("Factor"));
        fac->value = &gRedoFactorF;
        fac->stepFino = 0.05f; fac->stepGrueso = 0.05f; fac->dragStep = 0.01f;
        fac->SetRango(-1.0f, 1.0f); fac->onChange = RedoLoopCutOnChange;
        fac->centrado = true;
        grupo->properties.push_back(fac);
        grupo->selectIndex = 0;
        vpCreador = Viewport3DActive;
        ResizeGrupo();
        Reubicar();
        return;
    }

    gRedoVertsF = (float)m->meshVerts;
    gRedoVerts2F = (float)m->meshVerts2;

    const char* titulo = (m->meshTipo == (int)MeshType::cube)     ? "Add Cube"
                       : (m->meshTipo == (int)MeshType::plane)    ? "Add Plane"
                       : (m->meshTipo == (int)MeshType::UVsphere) ? "Add UV Sphere"
                       : (m->meshTipo == (int)MeshType::cone)     ? "Add Cone"
                       : (m->meshTipo == (int)MeshType::cylinder) ? "Add Cylinder"
                                                                  : "Add Circle";
    grupo = new GroupPropertie(titulo);
    grupo->anchoValores = 0.5f;

    if (m->meshTipo == (int)MeshType::circle){
        PropFloat* radio = new PropFloat(T("Radius"));
        radio->value = &m->meshSize;
        radio->stepFino = 0.05f; radio->stepGrueso = 0.05f; // flechas: +-0.05 (fino)
        radio->dragStep = 0.02f;
        radio->SetRango(0.01f, 1000.0f); radio->onChange = RedoOnChange;
        radio->centrado = true; // valor centrado (float: misma logica, se ve bien)
        grupo->properties.push_back(radio);

        PropFloat* verts = new PropFloat(T("Vertices"));
        verts->value = &gRedoVertsF;
        verts->stepFino = 1.0f; verts->stepGrueso = 1.0f; // flechas/teclas: +-1
        verts->dragStep = 0.1f; // LENTO: ~10px por vertice (que no explote)
        verts->SetRango(3.0f, 256.0f); verts->onChange = RedoOnChange;
        verts->entero = true; verts->centrado = true; verts->flechas = true;
        grupo->properties.push_back(verts);
    } else if (m->meshTipo == (int)MeshType::UVsphere){
        PropFloat* segs = new PropFloat(T("Segments"));
        segs->value = &gRedoVertsF; // segments (longitud)
        segs->stepFino = 1.0f; segs->stepGrueso = 1.0f;
        segs->dragStep = 0.1f; // LENTO (que no explote)
        segs->SetRango(3.0f, 64.0f); segs->onChange = RedoOnChange; // 64 max (flat no pasa el limite de indices)
        segs->entero = true; segs->centrado = true; segs->flechas = true;
        grupo->properties.push_back(segs);

        PropFloat* rings = new PropFloat(T("Rings"));
        rings->value = &gRedoVerts2F; // rings (latitud)
        rings->stepFino = 1.0f; rings->stepGrueso = 1.0f;
        rings->dragStep = 0.1f;
        rings->SetRango(2.0f, 64.0f); rings->onChange = RedoOnChange;
        rings->entero = true; rings->centrado = true; rings->flechas = true;
        grupo->properties.push_back(rings);

        PropFloat* radio = new PropFloat(T("Radius"));
        radio->value = &m->meshSize;
        radio->stepFino = 0.05f; radio->stepGrueso = 0.05f;
        radio->dragStep = 0.02f;
        radio->SetRango(0.01f, 1000.0f); radio->onChange = RedoOnChange;
        radio->centrado = true;
        grupo->properties.push_back(radio);

        PropBool* smooth = new PropBool(T("Shade Smooth"));
        smooth->value = &m->meshSmooth; smooth->onChange = RedoOnChange;
        grupo->properties.push_back(smooth);
    } else if (m->meshTipo == (int)MeshType::cone){
        PropFloat* r1 = new PropFloat(T("Radius 1")); // base
        r1->value = &m->meshSize;
        r1->stepFino = 0.05f; r1->stepGrueso = 0.05f; r1->dragStep = 0.02f;
        r1->SetRango(0.0f, 1000.0f); r1->onChange = RedoOnChange;
        r1->centrado = true;
        grupo->properties.push_back(r1);

        PropFloat* r2 = new PropFloat(T("Radius 2")); // punta (0 = puntiagudo)
        r2->value = &m->meshSize2;
        r2->stepFino = 0.05f; r2->stepGrueso = 0.05f; r2->dragStep = 0.02f;
        r2->SetRango(0.0f, 1000.0f); r2->onChange = RedoOnChange;
        r2->centrado = true;
        grupo->properties.push_back(r2);

        PropFloat* depth = new PropFloat(T("Depth")); // altura
        depth->value = &m->meshDepth;
        depth->stepFino = 0.05f; depth->stepGrueso = 0.05f; depth->dragStep = 0.02f;
        depth->SetRango(0.01f, 1000.0f); depth->onChange = RedoOnChange;
        depth->centrado = true;
        grupo->properties.push_back(depth);

        PropFloat* verts = new PropFloat(T("Vertices"));
        verts->value = &gRedoVertsF;
        verts->stepFino = 1.0f; verts->stepGrueso = 1.0f; verts->dragStep = 0.1f;
        verts->SetRango(3.0f, 256.0f); verts->onChange = RedoOnChange;
        verts->entero = true; verts->centrado = true; verts->flechas = true;
        grupo->properties.push_back(verts);

        PropBool* smooth = new PropBool(T("Shade Smooth"));
        smooth->value = &m->meshSmooth;
        smooth->onChange = RedoOnChange; // regenera con/sin normales compartidas
        grupo->properties.push_back(smooth);
    } else if (m->meshTipo == (int)MeshType::cylinder){
        PropFloat* radio = new PropFloat(T("Radius")); // un solo radio
        radio->value = &m->meshSize;
        radio->stepFino = 0.05f; radio->stepGrueso = 0.05f; radio->dragStep = 0.02f;
        radio->SetRango(0.01f, 1000.0f); radio->onChange = RedoOnChange;
        radio->centrado = true;
        grupo->properties.push_back(radio);

        PropFloat* depth = new PropFloat(T("Depth"));
        depth->value = &m->meshDepth;
        depth->stepFino = 0.05f; depth->stepGrueso = 0.05f; depth->dragStep = 0.02f;
        depth->SetRango(0.01f, 1000.0f); depth->onChange = RedoOnChange;
        depth->centrado = true;
        grupo->properties.push_back(depth);

        PropFloat* verts = new PropFloat(T("Vertices"));
        verts->value = &gRedoVertsF;
        verts->stepFino = 1.0f; verts->stepGrueso = 1.0f; verts->dragStep = 0.1f;
        verts->SetRango(3.0f, 256.0f); verts->onChange = RedoOnChange;
        verts->entero = true; verts->centrado = true; verts->flechas = true;
        grupo->properties.push_back(verts);

        PropBool* smooth = new PropBool(T("Shade Smooth"));
        smooth->value = &m->meshSmooth; smooth->onChange = RedoOnChange;
        grupo->properties.push_back(smooth);
    } else if (m->meshTipo == (int)MeshType::cube){
        PropFloat* size = new PropFloat(T("Size"));
        size->value = &m->meshSize;
        size->stepFino = 0.05f; size->stepGrueso = 0.05f; size->dragStep = 0.02f;
        size->SetRango(0.01f, 1000.0f); size->onChange = RedoOnChange;
        size->centrado = true;
        grupo->properties.push_back(size);

        PropBool* smooth = new PropBool(T("Shade Smooth"));
        smooth->value = &m->meshSmooth; smooth->onChange = RedoOnChange;
        grupo->properties.push_back(smooth);
    } else {
        PropFloat* size = new PropFloat(T("Size"));
        size->value = &m->meshSize;
        size->stepFino = 0.05f; size->stepGrueso = 0.05f; // flechas: +-0.05 (fino)
        size->dragStep = 0.02f;
        size->SetRango(0.01f, 1000.0f); size->onChange = RedoOnChange;
        size->centrado = true;
        grupo->properties.push_back(size);
    }

    grupo->selectIndex = 0; // abrir con el primer campo ya seleccionado

    // recordar el viewport3D que creo la forma; sus bounds se leen en cada
    // Render (en el constructor todavia valen 0, antes del primer layout)
    vpCreador = Viewport3DActive;
    ResizeGrupo();
    Reubicar();
}

void RedoMeshPanel::Reubicar(){
    int vx, vy, vh;
    if (vpCreador){ vx = vpCreador->x; vy = vpCreador->y; vh = vpCreador->height; }
    else { vx = 0; vy = 0; vh = MenuPantallaH; }
    int margen = gapGS * 2;
    x = vx + margen;
    y = vy + vh - popUpWindow->height - margen; // pegado al borde inferior-IZQ
    // NUNCA salirse de la pantalla (en vertical el viewport3D es angosto y el panel sobresalia a la
    // derecha / abajo). Se reencuadra dentro de [margen, Pantalla-margen].
    if (x + popUpWindow->width  + margen > MenuPantallaW) x = MenuPantallaW - popUpWindow->width  - margen;
    if (y + popUpWindow->height + margen > MenuPantallaH) y = MenuPantallaH - popUpWindow->height - margen;
    if (x < margen) x = margen;
    if (y < margen) y = margen;
}

RedoMeshPanel::~RedoMeshPanel(){
    delete grupo;
}

void RedoMeshPanel::ResizeGrupo(){
    int w = 200 * GlobalScale;
    int maxW = MenuPantallaW - gapGS * 4; // que quepa en pantalla (en vertical el ancho es chico)
    if (w > maxW) w = maxW;               // al achicarse, la tarjeta trunca los labels sola (RenderBitmapText)
    grupo->Resize(w, 0);
    popUpWindow->Resize(w, grupo->height);
}

void RedoMeshPanel::Render(){
    if (!grupo) return;
    ResizeGrupo();
    Reubicar(); // por si el viewport cambio de tamano
    // cada popup arma su propio ortho/viewport/scissor local (patron FileBrowser).
    // grupo->Render() dibuja en coords locales: (0,0) = esquina sup-izq del panel.
    initView();
    w3dEngine::PushMatrix();
    grupo->Render();
    w3dEngine::PopMatrix();
    // borde VERDE = panel activo (mismo accent que el viewport activo)
    const float* verde = ListaColores[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(verde[0], verde[1], verde[2], 1.0f);
    grupo->card->RenderBorder(false);
    endView();
}

bool RedoMeshPanel::Click(int mx, int my){
    if (!Contains(mx, my)) return false; // afuera -> el caller cierra el panel
    int myL = my - y; // a coordenadas locales del panel
    int lx  = mx - x;
    // rect de la columna de valores (igual que el render: titulo + box)
    int boxStart = grupo->colEtiqueta + 2 * borderGS;
    int boxW = grupo->propertiBox->width - bordersGS;
    int aw = RenglonHeightGS; // ancho de cada flecha < >
    // mismo recorrido que ClickEn: titulo + cada fila mide lo que da su Resize
    int yRow = borderGS + RenglonHeightGS + gapGS;
    for (size_t j = 0; j < grupo->properties.size(); j++){
        int h = grupo->properties[j]->Resize(grupo->width);
        if (grupo->properties[j]->Seleccionable() && myL >= yRow && myL < yRow + h){
            grupo->selectIndex = (int)j;
            if (grupo->properties[j]->GetType() == PropertyType::Float){
                PropFloat* pf = static_cast<PropFloat*>(grupo->properties[j]);
                if (pf->flechas){
                    // click en la flecha izquierda/derecha: -1 / +1 (sin arrastre)
                    if (lx >= boxStart && lx < boxStart + aw){ pf->button_left();  return true; }
                    if (lx >= boxStart + boxW - aw && lx < boxStart + boxW){ pf->button_right(); return true; }
                }
                dragField = pf; dragMoved = false; lastDragMx = mx; // resto del box: ARRASTRAR (mueve) o TAP (abre teclado al soltar)
            } else if (grupo->properties[j]->GetType() == PropertyType::Bool){
                grupo->properties[j]->EditPropertie(); // togglea + onChange (Regenerar)
            }
            return true;
        }
        yRow += h;
    }
    return true; // dentro del panel pero no en un campo: consumir (no cerrar)
}

bool RedoMeshPanel::Motion(int mx, int my){
    // el popup es modal: el viewport NO warpea el mouse mientras esta abierto,
    // asi que el delta crudo (mx - lastDragMx) es seguro.
    if (dragField && dragField->value){
        int dmx = mx - lastDragMx;
        lastDragMx = mx;
        if (dmx != 0) dragMoved = true; // hubo arrastre real -> al soltar NO abrir el teclado
        dragField->Set(*dragField->value + dmx * dragField->dragStep);
    }
    return true;
}

bool RedoMeshPanel::Tecla(int tecla){
    if (!grupo || grupo->properties.empty()) return true;
    int n = (int)grupo->properties.size();
    switch (tecla){
        // arriba/abajo: navegar entre campos con WRAP (sin los estados -1/-2 de
        // NextSelect/PrevSelect, que son para navegar entre VARIOS grupos)
        case LayoutKey::Up:
        case LayoutKey::Down: {
            int dir = (tecla == LayoutKey::Down) ? +1 : -1;
            int s = grupo->selectIndex;
            if (s < 0) s = (dir > 0) ? -1 : 0; // arrancar antes/en el 0
            for (int k = 0; k < n; k++){
                s += dir;
                if (s < 0)  s = n - 1;
                if (s >= n) s = 0;
                if (grupo->properties[s]->Seleccionable() &&
                    grupo->properties[s]->GetType() != PropertyType::Gap) break;
            }
            grupo->selectIndex = s;
            break;
        }
        case LayoutKey::Left:
        case LayoutKey::Right: {
            if (grupo->selectIndex < 0) grupo->selectIndex = 0;
            PropertieBase* sel = (grupo->selectIndex >= 0 && grupo->selectIndex < n)
                                 ? grupo->properties[grupo->selectIndex] : NULL;
            if (sel && sel->GetType() == PropertyType::Bool) sel->EditPropertie(); // togglea
            else if (tecla == LayoutKey::Left) grupo->button_left();  // -paso
            else grupo->button_right();                               // +paso
            break;
        }
        case LayoutKey::Cancel: Cerrar(); break; // Escape / soft-izq: cerrar
        case LayoutKey::Enter:  Cerrar(); break; // Enter / OK: confirmar y cerrar
    }
    return true;
}

void RedoMeshPanel::Soltar(){
    // TAP sobre el value box (sin arrastrar) -> abrir el TECLADO NUMERICO para editar el valor por
    // texto/formula. El numpad restaura ESTE panel al cerrar (prevPopup), y su onChange re-corta en vivo.
    if (dragField && !dragMoved && dragField->value){
        dragField->IniciarEdicionTexto();
        NumPadAbrir();
    }
    dragField = NULL;
}

void AbrirRedoMeshPanel(Mesh* m){
    if (!m) return;
    if (gRedoPanel){ delete gRedoPanel; gRedoPanel = NULL; } // reemplaza el anterior
    gRedoPanel = new RedoMeshPanel(m);
    PopUpActive = gRedoPanel;
}

void AbrirRedoNormalesPanel(Mesh* m){
    if (!m) return;
    if (gRedoPanel){ delete gRedoPanel; gRedoPanel = NULL; }
    gRedoPanel = new RedoMeshPanel(m, 1); // modo 1 = Recalculate Normals
    PopUpActive = gRedoPanel;
}

void AbrirRedoLoopCutPanel(Mesh* m){
    if (!m) return;
    if (gRedoPanel){ delete gRedoPanel; gRedoPanel = NULL; }
    gRedoPanel = new RedoMeshPanel(m, 2); // modo 2 = Loop Cut and Slide
    PopUpActive = gRedoPanel;
}
