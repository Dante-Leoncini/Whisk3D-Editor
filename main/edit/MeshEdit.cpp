#include "objects/Mesh.h"
#include "edit/MeshEdit.h"      // funciones libres de edicion de malla (mesh parts, etc.)
#include "objects/EditMesh.h"   // malla de EDICION (editor): este TU SI puede verla
#include "edit/Modifier.h"      // stack de modificadores (clase del EDITOR; el core solo guarda Modifier*)
#include "animation/SkeletalAnimation.h" // SkinearMesh (Apply del modificador Armature: hornear la pose)
#include "animation/Animation.h"         // CurrentFrame
#include "Undo.h"               // Ctrl+Z: snapshot de geometria antes de los ops
#include "w3dGraphics.h"
#include "WhiskUI/theme/colores.h"
#include "render/OpcionesRender.h"
#include <iostream>
#include <math.h>
#include <set>
#include <map>
#include <utility>
#include <string>
#include <cstring>
#include <algorithm>
#include <vector>

namespace gfx = w3dEngine;

// ============================================================================
//  Logica de EDICION de la malla (Edit Mode) — movido de Mesh.cpp (Fase A del
//  re-split core/editor). Estos metodos de Mesh DEREFERENCIAN EditMesh (editor),
//  asi que viven en main/ (que SI puede ver el editor) y Mesh.cpp (core) ya no
//  incluye EditMesh.h. Siguen siendo miembros de Mesh (declarados en Mesh.h).
// ============================================================================

// EDITAR el render IN-PLACE (rapido, tiempo real, N95): empuja las posiciones del edit +
// la capa activa (uv/color) a los arrays de render SIN realloc ni re-merge ni re-triangular.
// Para mover verts / pintar. NO sirve si cambia la TOPOLOGIA (ahi va GenerarRender).
void Mesh::RefrescarRender() {
    if (edit) { edit->EmpujarPosiciones();  // posiciones (autoritativas en el edit) -> render
                edit->RefrescarOverlay(); } // lineas/puntos del overlay desde pos[]
    AplicarCapasAlRender();                 // uv/color de la capa activa -> render
    chromeUVValid = false;                  // se movieron verts -> recalcular el reflejo equirect
    if (!modificadores.empty()) GenerarMallaModificada(); // preview de modificadores en tiempo real al mover verts
}

// construye la malla de EDICION (EditMesh) si no existe. La geometria ya tiene que
// estar lista (CalcularBordes corrio: posRep + edges). Se llama al entrar a Edit.
void Mesh::EnsureEdit() {
    if (!edit) { edit = new EditMesh(); edit->Construir(this); }
}

// la geometria cambio: la malla de edicion queda invalida (se rehace on-demand)
void Mesh::InvalidarEdit() {
    delete edit; edit = NULL;
}

// delegan en EditMesh (los llaman las funciones globales de seleccion segun el modo)
void Mesh::EditSeleccionarTodo(bool sel) { EnsureEdit(); if (edit) edit->SeleccionarTodo(sel); }
void Mesh::EditInvertir()                { EnsureEdit(); if (edit) edit->Invertir(); }

// BORRA la seleccion de Edit Mode. 'deleteType' (SelVertex/SelEdge/SelFace) dice
// QUE borrar; la SELECCION se lee del MODO ACTUAL (EditSelectMode) como un set de
// vertices seleccionados (un quad con sus 4 vertices sel = cara seleccionada), asi
// elegir "Vertices" estando en modo cara no borra de mas. Compacta los arrays de
// render (preserva UV/normales), reconstruye faces3d + faces[], y deja como
// looseEdges los bordes que quedan sin cara.
void Mesh::BorrarSeleccionEdit(int deleteType) {
    EnsureEdit();
    if (!edit || !vertex) return;
    UndoCapturarMallaGeo(this); // Ctrl+Z: snapshot pre-borrado
    EditMesh* e = edit;
    const int nV = vertexSize;
    if (nV <= 0) return;
    const bool hayRep = ((int)posRep.size() == nV);

    // ---- 0) selRep: representante GPU "seleccionado" segun el MODO ACTUAL ----
    std::vector<char> selRep(nV, 0);
    if (EditSelectMode == SelVertex) {
        for (size_t k = 0; k < e->editVerts.size(); k++)
            if (k < e->vertSel.size() && e->vertSel[k] &&
                e->editVerts[k] >= 0 && e->editVerts[k] < nV)
                selRep[e->editVerts[k]] = 1;
    } else if (EditSelectMode == SelEdge) {
        for (size_t eg = 0; eg < e->edgeSel.size(); eg++) {
            if (!e->edgeSel[eg]) continue;
            int ea = e->lineIdx[eg*2], eb = e->lineIdx[eg*2+1];
            if (ea >= 0 && ea < (int)e->editVerts.size() && e->editVerts[ea] < nV) selRep[e->editVerts[ea]] = 1;
            if (eb >= 0 && eb < (int)e->editVerts.size() && e->editVerts[eb] < nV) selRep[e->editVerts[eb]] = 1;
        }
    } else { // SelFace
        for (size_t fe = 0; fe < e->faces.size(); fe++) {
            if (fe >= e->faceSel.size() || !e->faceSel[fe]) continue;
            const std::vector<int>& poly = e->faces[fe];
            for (size_t c = 0; c < poly.size(); c++) {
                int ev = poly[c];
                if (ev >= 0 && ev < (int)e->editVerts.size() && e->editVerts[ev] < nV)
                    selRep[e->editVerts[ev]] = 1;
            }
        }
    }
    #define SELG(gi) (selRep[ hayRep ? posRep[gi] : (gi) ])

    // ---- 1) delV (GPU verts a borrar) y delF (caras a borrar) segun deleteType ----
    std::vector<char> delV(nV, 0);
    std::vector<char> delF(faces3d.size(), 0);

    if (deleteType == SelVertex) {
        for (int gi = 0; gi < nV; gi++) if (SELG(gi)) delV[gi] = 1;
        for (size_t f = 0; f < faces3d.size(); f++) {
            const std::vector<int>& idx = faces3d[f].idx;
            for (size_t c = 0; c < idx.size(); c++)
                if (idx[c] >= 0 && idx[c] < nV && delV[idx[c]]) { delF[f] = 1; break; }
        }
    } else if (deleteType == SelFace) {
        // cara borrada = TODOS sus corners seleccionados
        for (size_t f = 0; f < faces3d.size(); f++) {
            const std::vector<int>& idx = faces3d[f].idx;
            if (idx.empty()) continue;
            bool todos = true;
            for (size_t c = 0; c < idx.size(); c++) {
                int gi = idx[c];
                if (gi < 0 || gi >= nV || !SELG(gi)) { todos = false; break; }
            }
            if (todos) delF[f] = 1;
        }
    } else { // SelEdge: cara con una ARISTA de 2 extremos seleccionados
        for (size_t f = 0; f < faces3d.size(); f++) {
            const std::vector<int>& idx = faces3d[f].idx;
            int m = (int)idx.size();
            for (int c = 0; c < m; c++) {
                int a = idx[c], b = idx[(c+1)%m];
                if (a<0||a>=nV||b<0||b>=nV) continue;
                if (SELG(a) && SELG(b)) { delF[f] = 1; break; }
            }
        }
    }

    // ---- 2) aristas cubiertas por una cara VIVA (no se vuelven sueltas) ----
    std::set<std::pair<int,int> > vivos;
    for (size_t f = 0; f < faces3d.size(); f++) {
        if (delF[f]) continue;
        const std::vector<int>& idx = faces3d[f].idx;
        int m = (int)idx.size();
        for (int c = 0; c < m; c++) {
            int a = idx[c], b = idx[(c+1)%m];
            if (a<0||a>=nV||b<0||b>=nV) continue;
            int ra = hayRep?posRep[a]:a, rb = hayRep?posRep[b]:b;
            if (ra==rb) continue;
            if (ra>rb){int t=ra;ra=rb;rb=t;}
            vivos.insert(std::make_pair(ra, rb));
        }
    }
    // aristas de las caras BORRADAS que no quedan en cara viva -> sueltas (mantener
    // bordes). En modo Edge la arista SELECCIONADA (2 extremos sel) NO se mantiene.
    // Corre para TODOS los modos (antes se saltaba SelVertex -> al borrar un vert desaparecian las lineas). En
    // Delete Vertices, la arista que USA un vert borrado se va con el; las demas quedan como lineas sueltas.
    std::set<std::pair<int,int> > nuevasSueltas;
    {
        for (size_t f = 0; f < faces3d.size(); f++) {
            if (!delF[f]) continue;
            const std::vector<int>& idx = faces3d[f].idx;
            int m = (int)idx.size();
            for (int c = 0; c < m; c++) {
                int a = idx[c], b = idx[(c+1)%m];
                if (a<0||a>=nV||b<0||b>=nV) continue;
                if (delV[a] || delV[b]) continue;   // arista que usa un vert BORRADO -> se va con el (Delete Vertices)
                int ra = hayRep?posRep[a]:a, rb = hayRep?posRep[b]:b;
                if (ra==rb) continue;
                if (ra>rb){int t=ra;ra=rb;rb=t;}
                std::pair<int,int> pr(ra, rb);
                if (vivos.find(pr) != vivos.end()) continue;
                if (deleteType == SelEdge && SELG(a) && SELG(b)) continue;
                nuevasSueltas.insert(pr);
            }
        }
    }
    #undef SELG
    // bordes sueltos EXISTENTES: descartar los que usen un vertice borrado
    for (size_t le = 0; le + 1 < looseEdges.size(); le += 2) {
        int a = looseEdges[le], b = looseEdges[le+1];
        if (a<0||a>=nV||b<0||b>=nV) continue;
        if (delV[a] || delV[b]) continue;
        int ra = hayRep?posRep[a]:a, rb = hayRep?posRep[b]:b;
        if (ra==rb) continue;
        if (ra>rb){int t=ra;ra=rb;rb=t;}
        if (vivos.find(std::make_pair(ra,rb)) != vivos.end()) continue;
        nuevasSueltas.insert(std::make_pair(ra, rb));
    }

    // ---- 3) POSICIONES seleccionadas que SOBREVIVEN (restaurar la seleccion por POS tras
    //         GenerarRender; borrar verts no deja nada, borrar caras/aristas deja sel los
    //         verts que quedan). posRep VIEJO, antes de tocar la geometria. ----
    std::vector<Vector3> posSel;
    for (int gi = 0; gi < nV; gi++) {
        if (delV[gi]) continue;
        int rep = hayRep ? posRep[gi] : gi;
        if (rep>=0 && rep<nV && selRep[rep]) posSel.push_back(Vector3(vertex[gi*3],vertex[gi*3+1],vertex[gi*3+2]));
    }

    // ---- 4) faces3d sobrevivientes (indices VIEJOS: GenerarRender descarta los verts no
    //         referenciados y remapea) + la lista de corners que sobreviven (para las capas) ----
    PoblarCapas();
    std::vector<MeshFace> nf3d; std::vector<int> survCorner; int Lold = 0;
    for (size_t f = 0; f < faces3d.size(); f++) {
        const std::vector<int>& idx = faces3d[f].idx; int m = (int)idx.size();
        if (!delF[f]) {
            std::vector<int> ring; bool ok = true;
            for (int c = 0; c < m; c++) { int gi = idx[c]; if (gi<0||gi>=nV||delV[gi]) { ok=false; break; } ring.push_back(gi); }
            if (ok && ring.size() >= 3) { MeshFace mf; mf.idx = ring; mf.mat = faces3d[f].mat; mf.smooth = faces3d[f].smooth; nf3d.push_back(mf); for (int c=0;c<m;c++) survCorner.push_back(Lold+c); } // conserva mesh part + shading
        }
        Lold += m;
    }
    // ---- 5) bordes sueltos nuevos (reps VIEJOS: GenerarRender los remapea) ----
    std::vector<int> nLoose;
    for (std::set<std::pair<int,int> >::iterator it = nuevasSueltas.begin(); it != nuevasSueltas.end(); ++it)
        if (it->first>=0 && it->first<nV && it->second>=0 && it->second<nV) { nLoose.push_back(it->first); nLoose.push_back(it->second); }

    // ---- 5b) VERTS SUELTOS: los que sobreviven pero quedan sin cara NI arista (aislados). Sin esto un vert que
    //          pierde toda su geometria alrededor desaparece. reps VIEJOS -> GenerarRender los remapea. ----
    std::set<int> enGeo; // reps que quedan en alguna cara viva o arista suelta
    for (size_t f=0; f<nf3d.size(); f++){ const std::vector<int>& ix=nf3d[f].idx;
        for (size_t c=0;c<ix.size();c++){ int gi=ix[c]; if(gi>=0&&gi<nV) enGeo.insert(hayRep?posRep[gi]:gi); } }
    for (std::set<std::pair<int,int> >::iterator it=nuevasSueltas.begin(); it!=nuevasSueltas.end(); ++it){ enGeo.insert(it->first); enGeo.insert(it->second); }
    std::vector<int> nLooseV; std::set<int> looseRepDone;
    for (int gi=0; gi<nV; gi++){ if (delV[gi]) continue; int rep=hayRep?posRep[gi]:gi;
        if (rep<0||rep>=nV || enGeo.count(rep) || looseRepDone.count(rep)) continue;
        looseRepDone.insert(rep); nLooseV.push_back(gi); }

    if (nf3d.empty() && nLoose.empty() && nLooseV.empty()) { // se borro TODA la geometria
        delete[] vertex; vertex=NULL; delete[] normals; normals=NULL;
        delete[] uv; uv=NULL; delete[] vertexColor; vertexColor=NULL;
        delete[] faces; faces=NULL;
        vertexSize=0; facesSize=0; faces3d.clear(); looseEdges.clear(); looseVerts.clear(); LiberarCapas();
        CalcularBordes();
        return;
    }

    // ---- 6) aplicar por la puerta de rebuild: caras + capas compactadas + loose ----
    faces3d.swap(nf3d);
    looseEdges.swap(nLoose);
    looseVerts.swap(nLooseV);
    CompactarCapas(this,survCorner); // las capas se quedan solo con los corners sobrevivientes
    GenerarRender();            // descarta verts no referenciados + remapea + re-triangula + materialsGroup + CalcularBordes
    ReconstruirEditSelPorPos(posSel);
}

// union-find: raiz de x (con la ruta corta). Lo usa el clustering del merge.
static int UFind(std::vector<int>& uf, int x){ while (uf[x]!=x) x=uf[x]; return x; }

// MERGE (soldar) los verts seleccionados para SIMPLIFICAR la malla y quitar duplicados. modo:
//   0 At Center   -> toda la seleccion a un solo vert en su centro
//   1 At Cursor   -> toda la seleccion a un solo vert en el cursor 3d (cursorLocal, en espacio local del mesh)
//   2 Collapse    -> cada GRUPO CONEXO (por aristas seleccionadas) a su propio centro
//   3 By Distance -> los verts a <= dist entre si se agrupan y cada grupo va a su centro
// Cada grupo se suelda a UN vertice; las caras que colapsan a <3 verts distintos y las aristas que quedan en un
// punto se descartan (no tiene sentido que existan). Free function (accede a los miembros publicos del Mesh).
void MergeVertsEdit(Mesh* m, int modo, float dist, const Vector3& cursorLocal) {
    if (!m) return;
    m->EnsureEdit();
    if (!m->edit || !m->vertex) return;
    EditMesh* e = m->edit;
    const int nV = m->vertexSize;
    if (nV <= 0) return;
    const bool hayRep = ((int)m->posRep.size() == nV);
    #define MREP(gi) (hayRep ? m->posRep[gi] : (gi))

    // 0) reps seleccionados segun el modo de seleccion (vertex/edge/face)
    std::vector<char> selRep(nV, 0);
    if (EditSelectMode == SelVertex) {
        for (size_t k=0;k<e->editVerts.size();k++)
            if (k<e->vertSel.size() && e->vertSel[k] && e->editVerts[k]>=0 && e->editVerts[k]<nV) selRep[MREP(e->editVerts[k])]=1;
    } else if (EditSelectMode == SelEdge) {
        for (size_t eg=0;eg<e->edgeSel.size();eg++) if (e->edgeSel[eg]){
            int ea=e->lineIdx[eg*2], eb=e->lineIdx[eg*2+1];
            if (ea>=0&&ea<(int)e->editVerts.size()&&e->editVerts[ea]<nV) selRep[MREP(e->editVerts[ea])]=1;
            if (eb>=0&&eb<(int)e->editVerts.size()&&e->editVerts[eb]<nV) selRep[MREP(e->editVerts[eb])]=1;
        }
    } else {
        for (size_t f=0;f<e->faces.size();f++) if (f<e->faceSel.size()&&e->faceSel[f])
            for (size_t c=0;c<e->faces[f].size();c++){ int ev=e->faces[f][c];
                if (ev>=0&&ev<(int)e->editVerts.size()&&e->editVerts[ev]<nV) selRep[MREP(e->editVerts[ev])]=1; }
    }
    // GPU verts seleccionados: TODOS los que comparten un rep seleccionado. Un rep agrupa los gpu verts del MISMO
    // lugar (duplicados coincidentes). Clave para el extrude-in-place: la tapa nueva queda sobre la vieja -> son gpu
    // verts distintos en el MISMO punto, hay que soldarlos (a nivel rep parecen uno solo y el merge no los veia).
    std::vector<char> selG(nV, 0);
    for (int gi=0; gi<nV; gi++){ int r=MREP(gi); if (r>=0 && r<nV && selRep[r]) selG[gi]=1; }
    std::vector<int> sg; for (int gi=0; gi<nV; gi++) if (selG[gi]) sg.push_back(gi);
    if (sg.empty()) return;

    // 1) clusters de GPU verts por union-find sobre TODOS los verts (nV). By Distance suelda lo seleccionado con
    //    los verts CERCANOS aunque NO esten seleccionados (pedido: "unir los verts mas cercanos a lo seleccionado").
    std::vector<int> uf(nV); for (int i=0;i<nV;i++) uf[i]=i;
    if (modo == 0 || modo == 1) {                  // At Center / At Cursor: todo lo SELECCIONADO en un cluster
        int first=-1; for (size_t i=0;i<sg.size();i++){ if(first<0) first=sg[i]; else { int ra=UFind(uf,first), rb=UFind(uf,sg[i]); if(ra!=rb) uf[rb]=ra; } }
    } else if (modo == 2) {                         // Collapse: coincidentes sel (mismo rep) + conexos por aristas sel
        std::map<int,int> firstSelPorRep;
        for (size_t i=0;i<sg.size();i++){ int r=MREP(sg[i]); std::map<int,int>::iterator it=firstSelPorRep.find(r);
            if (it==firstSelPorRep.end()) firstSelPorRep[r]=sg[i]; else { int ra=UFind(uf,it->second), rb=UFind(uf,sg[i]); if(ra!=rb) uf[rb]=ra; } }
        for (size_t ie=0; ie+1 < m->edges.size(); ie+=2){
            int a=m->edges[ie], b=m->edges[ie+1];
            if (a<0||b<0||a>=nV||b>=nV||!selRep[a]||!selRep[b]) continue;
            std::map<int,int>::iterator ia=firstSelPorRep.find(a), ib=firstSelPorRep.find(b);
            if (ia==firstSelPorRep.end()||ib==firstSelPorRep.end()) continue;
            int ra=UFind(uf,ia->second), rb=UFind(uf,ib->second); if(ra!=rb) uf[rb]=ra;
        }
    } else {                                        // 3 = By Distance: cada vert SELECCIONADO se une con CUALQUIER vert a <= dist
        float d2 = dist*dist;
        for (size_t i=0;i<sg.size();i++){ int a=sg[i];
            for (int b=0; b<nV; b++){ if (b==a) continue;
                float dx=m->vertex[a*3]-m->vertex[b*3], dy=m->vertex[a*3+1]-m->vertex[b*3+1], dz=m->vertex[a*3+2]-m->vertex[b*3+2];
                if (dx*dx+dy*dy+dz*dz <= d2){ int ra=UFind(uf,a), rb=UFind(uf,b); if(ra!=rb) uf[rb]=ra; } } }
    }

    // 2) clusters: root -> gpu verts. Recolecta TODOS los verts (incluye los NO seleccionados arrastrados por By
    //    Distance) cuyo root sea el de algun vert seleccionado.
    std::set<int> rootsSel; for (size_t i=0;i<sg.size();i++) rootsSel.insert(UFind(uf,sg[i]));
    std::map<int,std::vector<int> > clusters;
    for (int gi=0; gi<nV; gi++){ int root=UFind(uf,gi); if (rootsSel.count(root)) clusters[root].push_back(gi); }
    std::vector<int>  mergedRoot(nV, -1);  // gpu vert -> gpu canonico de su cluster mergeado (-1 = no mergea)
    std::vector<Vector3> targetPos(nV);    // gpu canonico -> posicion destino
    std::vector<Vector3> posSel;           // para reseleccionar tras el rebuild
    bool algo = false;
    for (std::map<int,std::vector<int> >::iterator it=clusters.begin(); it!=clusters.end(); ++it){
        std::vector<int>& mem = it->second;
        if (mem.empty()) continue;
        bool mergea = (mem.size() >= 2) || (modo == 1 && mem.size() >= 1);
        int canon = mem[0]; for (size_t k=1;k<mem.size();k++) if (mem[k]<canon) canon=mem[k];
        // destino: At Cursor -> el cursor. Si el cluster arrastro verts NO seleccionados, se snapea a ELLOS (la base
        // no se mueve): centro de los no-seleccionados. Si son todos seleccionados -> su centro.
        Vector3 centro(0,0,0); Vector3 centroUnsel(0,0,0); int nUnsel=0;
        for (size_t k=0;k<mem.size();k++){ int g=mem[k];
            centro.x+=m->vertex[g*3]; centro.y+=m->vertex[g*3+1]; centro.z+=m->vertex[g*3+2];
            if (!selG[g]){ centroUnsel.x+=m->vertex[g*3]; centroUnsel.y+=m->vertex[g*3+1]; centroUnsel.z+=m->vertex[g*3+2]; nUnsel++; } }
        centro = centro * (1.0f/(float)mem.size());
        Vector3 destino;
        if (modo == 1)        destino = cursorLocal;
        else if (nUnsel > 0)  destino = centroUnsel * (1.0f/(float)nUnsel);
        else                  destino = centro;
        if (mergea){
            algo = true;
            for (size_t k=0;k<mem.size();k++) mergedRoot[mem[k]] = canon;
            targetPos[canon] = destino;
            posSel.push_back(destino);
        } else {
            posSel.push_back(Vector3(m->vertex[canon*3], m->vertex[canon*3+1], m->vertex[canon*3+2]));
        }
    }
    if (!algo) return; // nada para soldar

    UndoCapturarMallaGeo(m); // Ctrl+Z: snapshot antes del merge

    // 3) mover los gpu verts que mergean a su destino (GenerarRender re-splitea por normal -> conserva el flat shading)
    for (int gi=0; gi<nV; gi++){ int cr=mergedRoot[gi]; if (cr>=0){ Vector3& t=targetPos[cr]; m->vertex[gi*3]=t.x; m->vertex[gi*3+1]=t.y; m->vertex[gi*3+2]=t.z; } }

    // identidad "mergeada" de un corner: el gpu canonico de su cluster si mergea, sino el gpu mismo
    #define MID(gi) ( mergedRoot[gi]>=0 ? mergedRoot[gi] : (gi) )

    // 4) reconstruir faces3d: dedup de corners por identidad mergeada (tira los que colapsan), <3 -> cara muerta.
    //    Ademas DEDUP de CARAS: si dos caras quedan con el MISMO conjunto de vertices (mismas identidades mergeadas,
    //    sin importar el orden) queda UNA sola (bug Dante: un "merge at center" fallido dejaba caras encimadas).
    m->PoblarCapas();
    std::vector<MeshFace> nf3d; std::vector<int> survCorner; int Lold = 0;
    std::set<std::vector<int> > carasVistas; // conjunto ORDENADO de identidades por cara (para dedup)
    for (size_t f=0; f<m->faces3d.size(); f++){
        const std::vector<int>& idx = m->faces3d[f].idx; int mm=(int)idx.size();
        std::vector<int> ring; std::vector<int> ringCorner; std::set<int> vistos;
        for (int c=0;c<mm;c++){ int gi=idx[c]; if (gi<0||gi>=nV) continue; int id=MID(gi);
            if (vistos.count(id)) continue; // ya esta esa identidad en el anillo -> corner colapsado
            vistos.insert(id); ring.push_back(gi); ringCorner.push_back(Lold+c);
        }
        if (ring.size() >= 3){
            std::vector<int> clave(vistos.begin(), vistos.end()); // identidades ordenadas (std::set ya viene ordenado)
            if (carasVistas.count(clave)) { Lold += mm; continue; } // cara duplicada (mismos verts) -> descartar
            carasVistas.insert(clave);
            MeshFace mf; mf.idx = ring; mf.mat = m->faces3d[f].mat; mf.smooth = m->faces3d[f].smooth;
            nf3d.push_back(mf); for (size_t c=0;c<ringCorner.size();c++) survCorner.push_back(ringCorner[c]);
        }
        Lold += mm;
    }
    // 5) loose edges: descartar los que quedan en un punto (mismos extremos mergeados)
    std::vector<int> nLoose;
    for (size_t le=0; le+1 < m->looseEdges.size(); le+=2){ int a=m->looseEdges[le], b=m->looseEdges[le+1];
        if (a<0||a>=nV||b<0||b>=nV) continue; if (MID(a)==MID(b)) continue; nLoose.push_back(a); nLoose.push_back(b); }
    // 5b) loose verts: remapear por identidad mergeada (los que se sueldan colapsan a uno)
    std::vector<int> nLooseV; std::set<int> lvDone;
    for (size_t i=0;i<m->looseVerts.size();i++){ int v=m->looseVerts[i]; if (v<0||v>=nV) continue; int id=MID(v);
        if (lvDone.count(id)) continue; lvDone.insert(id); nLooseV.push_back(id); }
    #undef MID
    #undef MREP

    // 6) aplicar por la puerta de rebuild (igual que BorrarSeleccionEdit)
    m->faces3d.swap(nf3d);
    m->looseEdges.swap(nLoose);
    m->looseVerts.swap(nLooseV);
    CompactarCapas(m, survCorner);
    m->GenerarRender();               // suelda coincidentes (pos+uv+normal+color) + re-triangula + materialsGroup + CalcularBordes
    m->ReconstruirEditSelPorPos(posSel);
}

// DELETE > EDGE LOOPS (inverso del loop cut, pedido Dante): disuelve el edge loop seleccionado uniendo los quads de
// cada lado. Por cada borde del loop fusiona sus 2 caras (union-find + splice de los rings), y despues saca los verts
// del loop (quedan valencia-2). false si la seleccion no es un loop INTERNO disolvible (no fusiono nada).
bool Mesh::BorrarEdgeLoopEdit() {
    EnsureEdit();
    if (!edit || !vertex || vertexSize <= 0) return false;
    EditMesh* e = edit;
    const int nV = vertexSize;
    const bool hayRep = ((int)posRep.size() == nV);
    #define LREP(gi) (hayRep ? posRep[gi] : (gi))

    // 1) bordes del loop (pares de reps lo<hi) + verts del loop, segun el modo de seleccion
    std::set<std::pair<int,int> > loopE;
    if (EditSelectMode == SelEdge) {
        for (size_t eg=0; eg<e->edgeSel.size(); eg++) if (e->edgeSel[eg]){
            int ra=LREP(e->editVerts[e->lineIdx[eg*2]]), rb=LREP(e->editVerts[e->lineIdx[eg*2+1]]);
            if (ra!=rb) loopE.insert(ra<rb?std::make_pair(ra,rb):std::make_pair(rb,ra)); }
    } else {
        std::vector<char> selRep(nV,0);
        if (EditSelectMode==SelFace){ for (size_t fe=0;fe<e->faces.size();fe++) if (fe<e->faceSel.size()&&e->faceSel[fe]){ const std::vector<int>&p=e->faces[fe]; for(size_t c=0;c<p.size();c++) selRep[LREP(e->editVerts[p[c]])]=1; } }
        else { for (size_t k=0;k<e->editVerts.size();k++) if (k<e->vertSel.size()&&e->vertSel[k]) selRep[LREP(e->editVerts[k])]=1; }
        for (size_t i=0;i+1<edges.size();i+=2){ int ra=edges[i],rb=edges[i+1]; if(ra<0||rb<0||ra==rb||ra>=nV||rb>=nV)continue; if(selRep[ra]&&selRep[rb]) loopE.insert(ra<rb?std::make_pair(ra,rb):std::make_pair(rb,ra)); }
    }
    if (loopE.empty()) return false;
    std::set<int> loopV; for (std::set<std::pair<int,int> >::iterator it=loopE.begin();it!=loopE.end();++it){ loopV.insert(it->first); loopV.insert(it->second); }
    // GUARD: la seleccion tiene que ser edge LOOPS limpios (cada vert tocado por EXACTAMENTE 2 aristas del loop =
    // anillos disjuntos cerrados). Si no -ej. todo el cubo seleccionado (valencia 3) o un camino abierto (valencia 1)-
    // NO disolver: devolver false para que la GUI avise ("select an edge loop first") en vez de mangler la malla.
    { std::map<int,int> val; for (std::set<std::pair<int,int> >::iterator it=loopE.begin();it!=loopE.end();++it){ val[it->first]++; val[it->second]++; }
      for (std::map<int,int>::iterator it=val.begin();it!=val.end();++it) if (it->second != 2) return false; }

    // 2) ring (gv) + corn (indice de corner VIEJO, para preservar las capas) por cara + edge->caras
    const int nF = (int)faces3d.size();
    std::vector<std::vector<int> > ring(nF), corn(nF);
    { int L=0; for (int f=0;f<nF;f++){ const std::vector<int>& idx=faces3d[f].idx; for (size_t c=0;c<idx.size();c++){ ring[f].push_back(idx[c]); corn[f].push_back(L+(int)c); } L+=(int)idx.size(); } }
    std::map<std::pair<int,int>, std::vector<int> > edgeFaces;
    for (int f=0;f<nF;f++){ int m=(int)ring[f].size(); for (int c=0;c<m;c++){ int ra=LREP(ring[f][c]), rb=LREP(ring[f][(c+1)%m]); if(ra==rb)continue; edgeFaces[ra<rb?std::make_pair(ra,rb):std::make_pair(rb,ra)].push_back(f); } }

    // 3) FUSION: por cada borde del loop, unir sus 2 caras (union-find + splice). El borde dissuelto desaparece.
    std::vector<int> uf(nF); for (int f=0;f<nF;f++) uf[f]=f;
    bool algo=false;
    for (std::set<std::pair<int,int> >::iterator it=loopE.begin();it!=loopE.end();++it){
        std::map<std::pair<int,int>, std::vector<int> >::iterator ef=edgeFaces.find(*it);
        if (ef==edgeFaces.end() || ef->second.size()!=2) continue;       // borde de malla / non-manifold -> no fusiona
        int f1=ef->second[0], f2=ef->second[1];
        while (uf[f1]!=f1) f1=uf[f1];                                     // find (rep actual)
        while (uf[f2]!=f2) f2=uf[f2];
        if (f1==f2) continue;
        int ra=it->first, rb=it->second;
        int m1=(int)ring[f1].size(), i1=-1; for (int c=0;c<m1;c++){ int p=LREP(ring[f1][c]),q=LREP(ring[f1][(c+1)%m1]); if((p==ra&&q==rb)||(p==rb&&q==ra)){i1=c;break;} }
        int m2=(int)ring[f2].size(), i2=-1; for (int c=0;c<m2;c++){ int p=LREP(ring[f2][c]),q=LREP(ring[f2][(c+1)%m2]); if((p==ra&&q==rb)||(p==rb&&q==ra)){i2=c;break;} }
        if (i1<0||i2<0) continue;
        std::vector<int> nr, nc;
        for (int k=0;k<m1;k++){ int idx=(i1+1+k)%m1; nr.push_back(ring[f1][idx]); nc.push_back(corn[f1][idx]); }    // F1 desde i1+1 (m1 verts)
        for (int k=0;k<m2-2;k++){ int idx=(i2+2+k)%m2; nr.push_back(ring[f2][idx]); nc.push_back(corn[f2][idx]); }  // F2 desde i2+2 (m2-2 verts, sin la arista compartida)
        ring[f1].swap(nr); corn[f1].swap(nc);
        uf[f2]=f1; ring[f2].clear(); corn[f2].clear();
        algo=true;
    }
    if (!algo) return false;                                             // no era un loop interno disolvible

    UndoCapturarMallaGeo(this); // Ctrl+Z

    // 4) sacar los verts del loop de los rings vivos (valencia-2 tras la fusion -> sus 2 aristas se unen). PROTECCION:
    // si sacarlos degeneraria la cara (<3 verts; pasa cuando el loop atraviesa un POLO valencia-alta, ej. la longitud
    // de una esfera) -> dejar la cara como esta. Asi nunca rompe la malla; en un loop limpio (cubo/cilindro/latitud)
    // ninguna degenera y el dissolve es completo.
    for (int f=0;f<nF;f++){ if (uf[f]!=f) continue;
        std::vector<int> nr, nc;
        for (size_t c=0;c<ring[f].size();c++){ if (loopV.count(LREP(ring[f][c]))) continue; nr.push_back(ring[f][c]); nc.push_back(corn[f][c]); }
        if (nr.size() >= 3) { ring[f].swap(nr); corn[f].swap(nc); }
    }

    // 5) rebuild (igual que BorrarSeleccionEdit): nf3d + survCorner + CompactarCapas + GenerarRender (preserva UV)
    PoblarCapas();
    std::vector<MeshFace> nf3d; std::vector<int> survCorner;
    for (int f=0;f<nF;f++){ if (uf[f]!=f || ring[f].size()<3) continue;
        MeshFace mf; mf.idx=ring[f]; mf.mat=faces3d[f].mat; nf3d.push_back(mf);
        for (size_t c=0;c<corn[f].size();c++) survCorner.push_back(corn[f][c]); }
    std::vector<int> nLoose; // bordes sueltos: descartar los que toquen un vert del loop
    for (size_t i=0;i+1<looseEdges.size();i+=2){ int a=looseEdges[i],b=looseEdges[i+1]; if (a<0||b<0||a>=nV||b>=nV) continue; if (loopV.count(LREP(a))||loopV.count(LREP(b))) continue; nLoose.push_back(a); nLoose.push_back(b); }
    #undef LREP
    faces3d.swap(nf3d);
    looseEdges.swap(nLoose);
    CompactarCapas(this,survCorner);
    GenerarRender();             // descarta los verts del loop (sin referencia) + remapea + re-triangula + CalcularBordes
    std::vector<Vector3> sinSel; ReconstruirEditSelPorPos(sinSel); // el loop ya no existe -> sin seleccion
    return true;
}

// reconstruye la malla de edicion tras un cambio de geometria y deja SELECCIONADOS
// los vertices nuevos marcados en 'selVertNuevo' (indexado por indice GPU nuevo). El
// edge/face-sel se deriva (ambos extremos / todos los corners). Lo usan extrude/dup.
// captura las POSICIONES de los verts marcados en selPorGPU (indices del vertex[] ACTUAL).
// Sirve para restaurar la seleccion DESPUES de GenerarRender, que re-numera los GPU verts
// (un vert nuevo puede aparecer en una cara temprana y quedar con indice bajo -> el indice
// viejo deja de servir). La POSICION sí es estable.
std::vector<Vector3> Mesh::CapturarPosSel(const std::vector<char>& selPorGPU) {
    std::vector<Vector3> r;
    for (int gv=0; gv<vertexSize; gv++)
        if (gv<(int)selPorGPU.size() && selPorGPU[gv])
            r.push_back(Vector3(vertex[gv*3], vertex[gv*3+1], vertex[gv*3+2]));
    return r;
}
// reconstruye la seleccion de edit por POSICION (robusto al re-merge de GenerarRender):
// marca todo vert cuya posicion coincida con alguna de posSel y arma el edit + seleccion.
void Mesh::ReconstruirEditSelPorPos(const std::vector<Vector3>& posSel) {
    std::vector<char> sel(vertexSize, 0);
    // por DISTANCIA (no por eje): un offset chico DIAGONAL (ej. la normal de una esquina del cubo, ~9e-5 por eje)
    // caia dentro de la caja per-eje de 1e-4 y seleccionaba tambien el vertice ORIGINAL, no solo la tapa. Eso hacia
    // que el 2do extrude de un vertice viera 2 verts (un borde) y saliera un quad en vez de una linea (bug Dante).
    for (int gv=0; gv<vertexSize; gv++)
        for (size_t i=0;i<posSel.size();i++){
            float dx=vertex[gv*3]-posSel[i].x, dy=vertex[gv*3+1]-posSel[i].y, dz=vertex[gv*3+2]-posSel[i].z;
            if (dx*dx+dy*dy+dz*dz < 1e-8f) { sel[gv]=1; break; } // < 1e-4 de distancia
        }
    ReconstruirEditSel(sel);
}

void Mesh::ReconstruirEditSel(const std::vector<char>& selVertNuevo) {
    EnsureEdit();
    if (!edit) return;
    EditMesh* ne = edit;
    for (size_t k = 0; k < ne->editVerts.size(); k++) {
        int gi = ne->editVerts[k];
        ne->vertSel[k] = (gi >= 0 && gi < (int)selVertNuevo.size() && selVertNuevo[gi]) ? 1 : 0;
    }
    for (size_t eg = 0; eg < ne->edgeSel.size(); eg++) {
        int a = ne->lineIdx[eg*2], b = ne->lineIdx[eg*2+1];
        bool sa = (a>=0 && a<(int)ne->vertSel.size() && ne->vertSel[a]);
        bool sb = (b>=0 && b<(int)ne->vertSel.size() && ne->vertSel[b]);
        ne->edgeSel[eg] = (sa && sb) ? 1 : 0;
    }
    for (size_t f = 0; f < ne->faces.size(); f++) {
        const std::vector<int>& poly = ne->faces[f];
        bool all = !poly.empty();
        for (size_t c = 0; c < poly.size(); c++)
            if (poly[c]<0 || poly[c]>=(int)ne->vertSel.size() || !ne->vertSel[poly[c]]) { all=false; break; }
        ne->faceSel[f] = all ? 1 : 0;
    }
    ne->activeIdx = -1;
    ne->Recolorear();
}

// EXTRUDE en Edit Mode segun el MODO de seleccion:
//  - Caras: tapa (region) + paredes SOLO en el contorno; los verts interiores se mueven.
//  - Aristas: por cada arista sel un QUAD nuevo (arista vieja -> arista nueva). Una
//    cadena de aristas = una "pared" (los verts compartidos se duplican 1 vez).
//  - Vertices: por cada vertice sel un vertice nuevo + una ARISTA (vieja -> nueva).
// Duplica los verts de la "tapa" (offset epsilon, asi NO quedan pegados al original) y
// los deja seleccionados para que el editor los mueva. Direccion = normal promedio de
// las caras adyacentes; si no hay (suelto) outConstrain=false (mov libre) + fallback.
// (def. mas abajo, tras NormMod) true si la arista (ra,rb) esta sobre un plano de espejo con
// clipping: es una COSTURA, no contorno real -> no hay que hacerle pared (la taparia el mirror).
static bool AristaEnPlanoMirror(Mesh* m, int ra, int rb);

bool Mesh::ExtruirEdit(Vector3& outDirLocal, bool& outConstrain) {
    EnsureEdit();
    if (!edit || !vertex || vertexSize <= 0) return false;
    UndoCapturarMallaGeo(this); // Ctrl+Z: snapshot pre-extrude (cubre la topologia + el move)
    PoblarCapas(); // asegura las capas (las paredes nuevas agregan corners a TODAS)
    EditMesh* e = edit;
    const int nV = vertexSize;
    const bool hayRep = ((int)posRep.size() == nV);
    const int selModeUI = EditSelectMode;

    // ---- selRep: representantes GPU "seleccionados" segun el MODO de la UI ----
    std::vector<char> selRep(nV, 0);
    if (selModeUI == SelVertex) {
        for (size_t k = 0; k < e->editVerts.size(); k++)
            if (k < e->vertSel.size() && e->vertSel[k] && e->editVerts[k] >= 0 && e->editVerts[k] < nV)
                selRep[e->editVerts[k]] = 1;
    } else if (selModeUI == SelEdge) {
        for (size_t eg = 0; eg < e->edgeSel.size(); eg++) { if (!e->edgeSel[eg]) continue;
            int ea = e->lineIdx[eg*2], eb = e->lineIdx[eg*2+1];
            if (ea >= 0 && ea < (int)e->editVerts.size()) selRep[e->editVerts[ea]] = 1;
            if (eb >= 0 && eb < (int)e->editVerts.size()) selRep[e->editVerts[eb]] = 1;
        }
    } else {
        for (size_t fe = 0; fe < e->faces.size(); fe++) { if (fe >= e->faceSel.size() || !e->faceSel[fe]) continue;
            const std::vector<int>& poly = e->faces[fe];
            for (size_t c = 0; c < poly.size(); c++)
                if (poly[c] >= 0 && poly[c] < (int)e->editVerts.size()) selRep[e->editVerts[poly[c]]] = 1;
        }
    }
    #define XSELG(gi) (selRep[ hayRep ? posRep[gi] : (gi) ])
    #define XREP(gi)  (hayRep ? posRep[gi] : (gi))

    // ---- segun el modo: capReps (a duplicar), interiorReps (caras: a mover) + datos ----
    std::vector<char> selFace(faces3d.size(), 0);
    std::set<int> capReps, interiorReps;
    std::vector<std::pair<int,int> > selEdgesDir;        // edge mode: (ra,rb) dirigido
    std::set<std::pair<int,int> > selEdgesSet;           // edge mode: undirected
    std::map<std::pair<int,int>,int> edgeCount;          // faces mode
    std::map<std::pair<int,int>,std::pair<int,int> > edgeDir;
    std::map<std::pair<int,int>,int> edgeMat;            // material (mesh part) de la cara sel duena de cada arista -> las paredes lo heredan

    // ELEMENTO EFECTIVO = el mas ALTO que forman los verts seleccionados (NO el modo
    // de la UI): en modo vertice, seleccionar los verts de una cara extruye la CARA; 2
    // verts de una arista extruyen la ARISTA; verts sueltos extruyen VERTICE.
    int mode;
    int nSelF = 0;
    for (size_t f = 0; f < faces3d.size(); f++) {
        const std::vector<int>& idx = faces3d[f].idx;
        if (idx.size() < 3) continue;
        bool todos = true;
        for (size_t c = 0; c < idx.size(); c++) { int gi=idx[c]; if (gi<0||gi>=nV||!XSELG(gi)) { todos=false; break; } }
        if (todos) { selFace[f]=1; nSelF++; }
    }
    if (nSelF > 0) {
        mode = SelFace;
        for (size_t f = 0; f < faces3d.size(); f++) { if (!selFace[f]) continue;
            const std::vector<int>& idx = faces3d[f].idx; int m=(int)idx.size();
            for (int c = 0; c < m; c++) {
                int ra=XREP(idx[c]), rb=XREP(idx[(c+1)%m]); if (ra==rb) continue;
                int lo=ra<rb?ra:rb, hi=ra<rb?rb:ra; std::pair<int,int> key(lo,hi);
                if (edgeCount.find(key)==edgeCount.end()){ edgeCount[key]=0; edgeDir[key]=std::make_pair(ra,rb); edgeMat[key]=faces3d[f].mat; }
                edgeCount[key]++;
            }
        }
        std::set<int> regionReps;
        for (size_t f = 0; f < faces3d.size(); f++) { if (!selFace[f]) continue;
            const std::vector<int>& idx = faces3d[f].idx;
            for (size_t c = 0; c < idx.size(); c++) regionReps.insert(XREP(idx[c]));
        }
        for (std::map<std::pair<int,int>,int>::iterator it=edgeCount.begin(); it!=edgeCount.end(); ++it)
            if (it->second == 1) { capReps.insert(it->first.first); capReps.insert(it->first.second); }
        for (std::set<int>::iterator it=regionReps.begin(); it!=regionReps.end(); ++it)
            if (!capReps.count(*it)) interiorReps.insert(*it);
    } else {
        // material (mesh part) de cada arista de la malla, buscando la cara que la contiene: las paredes
        // del extrude de aristas lo heredan (asi conservan el color de la cara de la que salen).
        for (size_t f = 0; f < faces3d.size(); f++) {
            const std::vector<int>& idx = faces3d[f].idx; int m=(int)idx.size();
            for (int c = 0; c < m; c++) {
                int ra=XREP(idx[c]), rb=XREP(idx[(c+1)%m]); if (ra==rb) continue;
                int lo=ra<rb?ra:rb, hi=ra<rb?rb:ra; std::pair<int,int> key(lo,hi);
                if (edgeMat.find(key)==edgeMat.end()) edgeMat[key]=faces3d[f].mat;
            }
        }
        // aristas de la malla (edges = pares de reps) con AMBOS extremos seleccionados
        for (size_t i = 0; i+1 < edges.size(); i += 2) {
            int ra=edges[i], rb=edges[i+1];
            if (ra<0||rb<0||ra>=nV||rb>=nV||ra==rb) continue;
            if (!selRep[ra] || !selRep[rb]) continue;
            int lo=ra<rb?ra:rb, hi=ra<rb?rb:ra; std::pair<int,int> key(lo,hi);
            if (selEdgesSet.insert(key).second) { selEdgesDir.push_back(std::make_pair(ra,rb)); capReps.insert(ra); capReps.insert(rb); }
        }
        if (!selEdgesDir.empty()) {
            mode = SelEdge;
        } else {
            mode = SelVertex;
            for (int gi = 0; gi < nV; gi++) if (XSELG(gi)) capReps.insert(XREP(gi));
            if (capReps.empty()) return false;
        }
    }

    // ---- direccion (normal promedio de las caras adyacentes a la seleccion) ----
    Vector3 nrm(0, 0, 0);
    for (size_t f = 0; f < faces3d.size(); f++) {
        const std::vector<int>& idx = faces3d[f].idx; int m=(int)idx.size(); if (m<3) continue;
        bool usar = false;
        if (mode == SelFace) usar = selFace[f];
        else if (mode == SelEdge) { // cara con una arista seleccionada
            for (int c=0;c<m && !usar;c++){ int ra=XREP(idx[c]), rb=XREP(idx[(c+1)%m]);
                int lo=ra<rb?ra:rb, hi=ra<rb?rb:ra; if (selEdgesSet.count(std::make_pair(lo,hi))) usar=true; }
        } else { for (int c=0;c<m && !usar;c++) if (XSELG(idx[c])) usar=true; } // toca un vert sel
        if (!usar) continue;
        float nx=0,ny=0,nz=0;
        for (int k=0;k<m;k++){ GLfloat* a=&vertex[idx[k]*3]; GLfloat* b=&vertex[idx[(k+1)%m]*3];
            nx+=(a[1]-b[1])*(a[2]+b[2]); ny+=(a[2]-b[2])*(a[0]+b[0]); nz+=(a[0]-b[0])*(a[1]+b[1]); }
        float ln=sqrtf(nx*nx+ny*ny+nz*nz); if (ln>1e-6f){ nrm.x+=nx/ln; nrm.y+=ny/ln; nrm.z+=nz/ln; }
    }
    { float ln=sqrtf(nrm.x*nrm.x+nrm.y*nrm.y+nrm.z*nrm.z);
      if (ln>1e-6f){ nrm.x/=ln; nrm.y/=ln; nrm.z/=ln; }
      else { // sin caras: fallback a la normal guardada de los verts (o +Y), solo para el offset
          nrm=Vector3(0,0,0);
          if (normals) for (std::set<int>::iterator it=capReps.begin(); it!=capReps.end(); ++it){
              int r=*it; nrm.x+=normals[r*3]/127.0f; nrm.y+=normals[r*3+1]/127.0f; nrm.z+=normals[r*3+2]/127.0f; }
          ln=sqrtf(nrm.x*nrm.x+nrm.y*nrm.y+nrm.z*nrm.z);
          if (ln>1e-6f){ nrm.x/=ln; nrm.y/=ln; nrm.z/=ln; } else nrm=Vector3(0,1,0);
      }
    }
    // SOLO la CARA tiene normal -> el extrude se constrine a esa normal. Un VERTICE o un BORDE NO tienen
    // normal (aunque toquen caras): su extrude es LIBRE, desde la VISTA (pedido Dante). nrm queda solo para
    // el offset chico de separacion.
    outConstrain = (mode == SelFace);
    outDirLocal = nrm;

    // ---- crecer arrays: +1 vert por rep de la TAPA (offset eps), mover interiores ----
    const float eps = 1.5e-4f; // apenas > umbral posRep (1e-4) -> la tapa es un rep distinto pero casi coincidente,
                               // asi "extruir y dejar en el lugar" + Merge By Distance vuelve a la malla original
    Vector3 off = nrm * eps;
    int nuevoN = nV + (int)capReps.size();
    bool tNor=(normals!=NULL), tUV=(uv!=NULL), tCol=(vertexColor!=NULL);
    GLfloat* nVtx=new GLfloat[nuevoN*3]; GLbyte* nNor=new GLbyte[nuevoN*3];
    GLfloat* nUV=new GLfloat[nuevoN*2]; GLubyte* nCol=new GLubyte[nuevoN*4];
    std::vector<char> esCap(nuevoN, 0);
    for (int gi = 0; gi < nV; gi++) {
        nVtx[gi*3]=vertex[gi*3]; nVtx[gi*3+1]=vertex[gi*3+1]; nVtx[gi*3+2]=vertex[gi*3+2];
        if (tNor){ nNor[gi*3]=normals[gi*3]; nNor[gi*3+1]=normals[gi*3+1]; nNor[gi*3+2]=normals[gi*3+2]; } else { nNor[gi*3]=0; nNor[gi*3+1]=127; nNor[gi*3+2]=0; }
        if (tUV){ nUV[gi*2]=uv[gi*2]; nUV[gi*2+1]=uv[gi*2+1]; } else { nUV[gi*2]=0; nUV[gi*2+1]=0; }
        if (tCol){ for(int q=0;q<4;q++) nCol[gi*4+q]=vertexColor[gi*4+q]; } else { for(int q=0;q<4;q++) nCol[gi*4+q]=255; }
    }
    for (int gi = 0; gi < nV; gi++) { int r=XREP(gi); // interiores (caras): mover + tapa
        if (interiorReps.count(r)) { nVtx[gi*3]+=off.x; nVtx[gi*3+1]+=off.y; nVtx[gi*3+2]+=off.z; esCap[gi]=1; }
    }
    std::map<int,int> newOf; int wi = nV;
    for (std::set<int>::iterator it=capReps.begin(); it!=capReps.end(); ++it) { int r=*it;
        nVtx[wi*3]=vertex[r*3]+off.x; nVtx[wi*3+1]=vertex[r*3+1]+off.y; nVtx[wi*3+2]=vertex[r*3+2]+off.z;
        if (tNor){ nNor[wi*3]=normals[r*3]; nNor[wi*3+1]=normals[r*3+1]; nNor[wi*3+2]=normals[r*3+2]; } else { nNor[wi*3]=0; nNor[wi*3+1]=127; nNor[wi*3+2]=0; }
        if (tUV){ nUV[wi*2]=uv[r*2]; nUV[wi*2+1]=uv[r*2+1]; } else { nUV[wi*2]=0; nUV[wi*2+1]=0; }
        if (tCol){ for(int q=0;q<4;q++) nCol[wi*4+q]=vertexColor[r*4+q]; } else { for(int q=0;q<4;q++) nCol[wi*4+q]=255; }
        esCap[wi]=1; newOf[r]=wi; wi++;
    }
    delete[] vertex; delete[] normals; delete[] uv; delete[] vertexColor;
    vertex=nVtx; normals=nNor; uv=nUV; vertexColor=nCol; vertexSize=nuevoN;

    // ---- nuevas caras ----
    // mapa rep -> primer corner (sobre faces3d ORIGINAL) para que las PAREDES/quads nuevos
    // hereden uv/color de los verts que conectan. Las caras tapa/intactas conservan sus
    // capas: nf3d preserva el orden de corners de las caras originales, asi la capa que
    // armo PoblarCapas sigue valida para esa primera parte (solo append de lo nuevo).
    std::vector<int> vertCorner(nV, -1); { int Lc=0;
        for (size_t f=0;f<faces3d.size();f++){ const std::vector<int>& ix=faces3d[f].idx;
            for (size_t c=0;c<ix.size();c++){ int r=XREP(ix[c]); if (r>=0&&r<nV&&vertCorner[r]<0) vertCorner[r]=Lc; Lc++; } } }
    std::vector<MeshFace> nf3d;
    for (size_t f = 0; f < faces3d.size(); f++) {
        const std::vector<int>& idx = faces3d[f].idx;
        std::vector<int> ring;
        if (mode == SelFace && selFace[f]) { // repuntar: contorno->nuevo, interior->queda
            for (size_t c = 0; c < idx.size(); c++) { int gi=idx[c]; int r=XREP(gi); ring.push_back(capReps.count(r)? newOf[r] : gi); }
        } else ring = idx; // resto: sin tocar
        if (ring.size() < 3) continue;
        MeshFace mf; mf.idx = ring; mf.mat = faces3d[f].mat; mf.smooth = faces3d[f].smooth; nf3d.push_back(mf); // conserva mesh part + shading
    }
    if (mode == SelFace) { // PAREDES en el contorno: quad [b_new,a_new,a_old,b_old]
        for (std::map<std::pair<int,int>,int>::iterator it=edgeCount.begin(); it!=edgeCount.end(); ++it) {
            if (it->second != 1) continue;
            int ra=edgeDir[it->first].first, rb=edgeDir[it->first].second;
            // COSTURA del mirror: si la arista del contorno cae sobre el plano de espejo (clipping), NO se le hace
            // pared -> no genera geometria interna invisible que el mirror tapa y que arruina la topologia. La tapa
            // repuntada deja igual la arista nueva sobre el plano; el mirror la suelda con su reflejo.
            if (AristaEnPlanoMirror(this, ra, rb)) continue;
            std::map<int,int>::iterator na=newOf.find(ra), nb=newOf.find(rb);
            if (na==newOf.end()||nb==newOf.end()) continue;
            std::vector<int> w; w.push_back(nb->second); w.push_back(na->second); w.push_back(ra); w.push_back(rb);
            MeshFace mf; mf.idx=w; mf.mat=edgeMat[it->first]; nf3d.push_back(mf); // la pared hereda el mesh part de la cara del contorno
            // la pared hereda uv/color de los verts que conecta (b_new/b_old<-rb, a_new/a_old<-ra)
            AgregarCornerCapas(this,vertCorner[rb]); AgregarCornerCapas(this,vertCorner[ra]);
            AgregarCornerCapas(this,vertCorner[ra]); AgregarCornerCapas(this,vertCorner[rb]);
        }
    } else if (mode == SelEdge) { // un QUAD por arista: [ra,rb,new_rb,new_ra]
        for (size_t i = 0; i < selEdgesDir.size(); i++) {
            int ra=selEdgesDir[i].first, rb=selEdgesDir[i].second;
            std::vector<int> w; w.push_back(ra); w.push_back(rb); w.push_back(newOf[rb]); w.push_back(newOf[ra]);
            int lo=ra<rb?ra:rb, hi=ra<rb?rb:ra; std::map<std::pair<int,int>,int>::iterator em=edgeMat.find(std::make_pair(lo,hi));
            MeshFace mf; mf.idx=w; if (em!=edgeMat.end()) mf.mat=em->second; nf3d.push_back(mf); // hereda el mesh part de la cara de la arista
            AgregarCornerCapas(this,vertCorner[ra]); AgregarCornerCapas(this,vertCorner[rb]);
            AgregarCornerCapas(this,vertCorner[rb]); AgregarCornerCapas(this,vertCorner[ra]);
        }
    }
    #undef XSELG
    #undef XREP
    faces3d.swap(nf3d);
    // VERTICES: cada vert sel -> ARISTA suelta (viejo rep -> nuevo). ANTES de GenerarRender
    // para que la preserve (remapea los loose edges al GPU nuevo).
    if (mode == SelVertex)
        for (std::map<int,int>::iterator it=newOf.begin(); it!=newOf.end(); ++it) { looseEdges.push_back(it->first); looseEdges.push_back(it->second); }

    // la seleccion de la TAPA se restaura POR POSICION: GenerarRender re-numera los verts
    // (la tapa repuntada cae en una cara temprana -> su indice viejo deja de servir).
    std::vector<Vector3> posTapa = CapturarPosSel(esCap);
    GenerarRender();                 // re-merge + render (re-triangula + materialsGroup + CalcularBordes + loose)
    ReconstruirEditSelPorPos(posTapa); // deja seleccionada la TAPA
    return true;
}

// MARK SHARP / CLEAR SHARP (menu Edge o tecla W): marca/desmarca como filosos los bordes
// seleccionados. La clave es la POSICION de los 2 extremos (estable; ver Mesh::sharpEdges).
// En una malla SMOOTH, los bordes sharp quedan flat (cilindro = lados suaves + tapas planas).
// Recolecta los indices de ARISTA a marcar (sharp/seam) segun el MODO ACTIVO (EditSelectMode),
// no solo edgeSel: en FACE mode tomaba el edgeSel stale -> marcaba TODOS los bordes (bug Dante).
//  EDGE   -> las aristas seleccionadas (edgeSel)
//  FACE   -> las aristas de las caras seleccionadas (faceEdges, sin repetir)
//  VERTEX -> las aristas con AMBOS extremos seleccionados (vertSel)
static void RecolectarAristasSel(EditMesh* e, std::vector<int>& out) {
    if (EditSelectMode == SelFace) {
        std::set<int> vistas;
        for (size_t f = 0; f < e->faceSel.size() && f < e->faceEdges.size(); f++) {
            if (!e->faceSel[f]) continue;
            const std::vector<int>& fe = e->faceEdges[f];
            for (size_t j = 0; j < fe.size(); j++)
                if (vistas.insert(fe[j]).second) out.push_back(fe[j]);
        }
    } else if (EditSelectMode == SelVertex) {
        int ne = (int)(e->lineIdx.size() / 2);
        for (int eg = 0; eg < ne; eg++) {
            int ka = e->lineIdx[eg*2], kb = e->lineIdx[eg*2 + 1];
            if (ka >= 0 && ka < (int)e->vertSel.size() && kb >= 0 && kb < (int)e->vertSel.size()
                && e->vertSel[ka] && e->vertSel[kb])
                out.push_back(eg);
        }
    } else { // SelEdge
        for (size_t eg = 0; eg < e->edgeSel.size(); eg++)
            if (e->edgeSel[eg]) out.push_back((int)eg);
    }
}

// marca/desmarca la arista 'eg' (indice en lineIdx) en el set 'edges' (sharpEdges o seamEdges),
// usando la POSICION de los 2 extremos como clave (estable al re-split). Devuelve true si la toco.
static bool MarcarUnaArista(Mesh* m, EditMesh* e, int eg, std::set<std::string>& edges, bool poner) {
    if (eg < 0 || (size_t)(eg*2 + 1) >= e->lineIdx.size()) return false;
    int ka = e->lineIdx[eg*2], kb = e->lineIdx[eg*2 + 1];
    if (ka < 0 || ka >= (int)e->editVerts.size() || kb < 0 || kb >= (int)e->editVerts.size()) return false;
    int gva = e->editVerts[ka], gvb = e->editVerts[kb];
    if (gva < 0 || gva >= m->vertexSize || gvb < 0 || gvb >= m->vertexSize) return false;
    std::string key = Mesh::SharpEdgeKey(&m->vertex[gva*3], &m->vertex[gvb*3]);
    if (poner) edges.insert(key); else edges.erase(key);
    return true;
}

void Mesh::MarcarSharpEdit(bool sharp) {
    EnsureEdit();
    if (!edit || !vertex) return;
    UndoCapturarMallaGeo(this); // Ctrl+Z: snapshot pre-sharp (sharpEdges + normales del re-shade)
    EditMesh* e = edit;
    std::vector<int> aristas; RecolectarAristasSel(e, aristas);
    int marcados = 0;
    for (size_t i = 0; i < aristas.size(); i++)
        if (MarcarUnaArista(this, e, aristas[i], sharpEdges, sharp)) marcados++;
    if (marcados > 0) GenerarRender(); // recomputa las normales con los bordes nuevos
}

// SEAM (costura UV): igual que sharp pero edita seamEdges. NO toca el shading (no GenerarRender);
// solo refresca el color (los seams se ven magenta en Edit Mode, ver EditMesh::Recolorear).
void Mesh::MarcarSeamEdit(bool seam) {
    EnsureEdit();
    if (!edit || !vertex) return;
    UndoCapturarMallaGeo(this); // Ctrl+Z: snapshot pre-seam (seamEdges)
    EditMesh* e = edit;
    std::vector<int> aristas; RecolectarAristasSel(e, aristas);
    int marcados = 0;
    for (size_t i = 0; i < aristas.size(); i++)
        if (MarcarUnaArista(this, e, aristas[i], seamEdges, seam)) marcados++;
    if (marcados > 0) e->Recolorear();
}

// escribe el UV proyectado (2 floats/corner, en orden de faces3d) en la capa activa SOLO en las
// caras seleccionadas, y reconstruye (re-split por UV -> abre los seams nuevos).
void Mesh::EscribirUVProyeccion(const std::vector<float>& uvPorCorner) {
    EnsureEdit(); if (!edit) return;
    EditMesh* e = edit;
    const int nC = ContarCorners();
    if ((int)uvPorCorner.size() != nC*2) return;
    if (uvMaps.empty()) PoblarCapas();
    UVMap* um = (uvMapActivo >= 0 && uvMapActivo < (int)uvMaps.size()) ? uvMaps[uvMapActivo] : NULL;
    if (!um || (int)um->uv.size() != nC*2) return;
    // caras de faces3d seleccionadas (mapeo edit faceSel -> faces3d via faceSrc)
    std::vector<unsigned char> sel3d(faces3d.size(), 0);
    for (size_t f = 0; f < e->faceSel.size(); f++)
        if (e->faceSel[f] && f < e->faceSrc.size()) {
            int f3 = e->faceSrc[f]; if (f3 >= 0 && f3 < (int)faces3d.size()) sel3d[f3] = 1;
        }
    int L = 0;
    for (size_t f = 0; f < faces3d.size(); f++) {
        int cnt = (int)faces3d[f].idx.size();
        if (f < sel3d.size() && sel3d[f])
            for (int c = 0; c < cnt; c++) {
                um->uv[(L+c)*2]   = uvPorCorner[(L+c)*2];
                um->uv[(L+c)*2+1] = uvPorCorner[(L+c)*2+1];
            }
        L += cnt;
    }
    GenerarRender();
    // PRESERVAR la seleccion: GenerarRender invalido el edit -> re-seleccionar las MISMAS caras3d
    // (la topologia logica no cambio, solo se re-splitearon verts por UV). Asi proyectar NO pierde
    // la seleccion (pedido Dante: "al cambiar el UV no me cambies la seleccion").
    EnsureEdit();
    if (edit) {
        edit->faceSel.assign(edit->NumFaces(), 0);
        for (size_t f = 0; f < edit->faceSrc.size(); f++) {
            int f3 = edit->faceSrc[f];
            if (f3 >= 0 && f3 < (int)sel3d.size() && sel3d[f3] && f < edit->faceSel.size())
                edit->faceSel[f] = 1;
        }
        edit->activeIdx = -1;
        edit->Recolorear();
    }
}

// ====================== MESH PARTS: asignar / seleccionar caras ======================
// asigna las caras SELECCIONADAS (edit, modo face) al mesh part 'idx'. NO rehace la malla: solo
// re-agrupa el index buffer por material (ReagruparMeshParts) -> la edicion + la seleccion siguen vivas.
void Mesh::AsignarFacesAMeshPart(int idx) {
    EnsureEdit();
    if (!edit || idx < 0 || idx >= (int)materialsGroup.size()) return;
    UndoCapturarMallaGeo(this); // Ctrl+Z: snapshot pre-assign (faces3d.mat + materialsGroup)
    EditMesh* e = edit;
    int n = 0;
    for (size_t f = 0; f < e->faceSel.size(); f++)
        if (e->faceSel[f] && f < e->faceSrc.size()) {
            int f3 = e->faceSrc[f];
            if (f3 >= 0 && f3 < (int)faces3d.size()) { faces3d[f3].mat = idx; n++; }
        }
    if (n > 0) ReagruparMeshParts();
}

// (de)selecciona en el edit mesh TODAS las caras del mesh part 'idx'. Entra en modo FACE.
// sel=true: Select (agrega esas caras a la seleccion). sel=false: Deselect (las saca).
void Mesh::SeleccionarMeshPart(int idx, bool sel) {
    EnsureEdit();
    if (!edit) return;
    EditSelectMode = SelFace; // las ops de mesh part trabajan sobre caras
    EditMesh* e = edit;
    for (size_t f = 0; f < e->faceSel.size(); f++) {
        if (f >= e->faceSrc.size()) continue;
        int f3 = e->faceSrc[f];
        if (f3 >= 0 && f3 < (int)faces3d.size() && faces3d[f3].mat == idx)
            e->faceSel[f] = sel ? 1 : 0;
    }
    e->activeIdx = -1;
    e->Recolorear();
}

// proyecciones object-space sobre las caras seleccionadas. tipo: 0=Cube, 1=Cylinder, 2=Sphere.
void Mesh::ProyectarUVCaras(int tipo) {
    EnsureEdit(); if (!edit || !vertex || vertexSize <= 0) return;
    UndoCapturarMallaGeo(this); // Ctrl+Z: snapshot pre-proyeccion UV (uv[] + uvMaps)
    EditMesh* e = edit;
    // caras de faces3d seleccionadas (para trackear/normalizar solo esas)
    std::vector<unsigned char> sel3d(faces3d.size(), 0);
    for (size_t f = 0; f < e->faceSel.size(); f++)
        if (e->faceSel[f] && f < e->faceSrc.size()) { int f3 = e->faceSrc[f]; if (f3>=0 && f3<(int)faces3d.size()) sel3d[f3]=1; }
    const int nC = ContarCorners();
    std::vector<float> uvL((size_t)nC*2, 0.0f);
    // bounding box + centro (para normalizar a ~[0,1])
    float mn[3] = { 1e30f,1e30f,1e30f }, mx[3] = { -1e30f,-1e30f,-1e30f };
    for (int i = 0; i < vertexSize; i++) for (int k = 0; k < 3; k++) {
        float v = vertex[i*3+k]; if (v < mn[k]) mn[k] = v; if (v > mx[k]) mx[k] = v;
    }
    float ext[3] = { mx[0]-mn[0], mx[1]-mn[1], mx[2]-mn[2] };
    float cen[3] = { (mn[0]+mx[0])*0.5f, (mn[1]+mx[1])*0.5f, (mn[2]+mx[2])*0.5f };
    float maxext = ext[0]; if (ext[1] > maxext) maxext = ext[1]; if (ext[2] > maxext) maxext = ext[2];
    if (maxext < 1e-6f) maxext = 1.0f;
    const float TWO_PI = 2.0f * (float)M_PI;
    int L = 0;
    for (size_t f = 0; f < faces3d.size(); f++) {
        const std::vector<int>& idx = faces3d[f].idx;
        const int cnt = (int)idx.size();
        // normal de cara (Newell) para el eje dominante del cube projection
        float nx = 0, ny = 0, nz = 0;
        for (int c = 0; c < cnt; c++) {
            const float* pa = &vertex[idx[c]*3]; const float* pb = &vertex[idx[(c+1)%cnt]*3];
            nx += (pa[1]-pb[1])*(pa[2]+pb[2]); ny += (pa[2]-pb[2])*(pa[0]+pb[0]); nz += (pa[0]-pb[0])*(pa[1]+pb[1]);
        }
        for (int c = 0; c < cnt; c++) {
            const float* p = &vertex[idx[c]*3];
            float u = 0, v = 0;
            // V invertida (V=0 = ARRIBA de la imagen, convencion del engine) en todas; el angulo
            // (u) de cylinder/sphere va INVERTIDO para que de izq->der la textura no salga espejada
            // (sino la proyeccion queda dada vuelta 180 grados).
            if (tipo == 0) {                         // CUBE: plano del eje dominante
                float ax = fabsf(nx), ay = fabsf(ny), az = fabsf(nz);
                if (ax >= ay && ax >= az) { u = (p[2]-cen[2])/maxext + 0.5f; v = 0.5f - (p[1]-cen[1])/maxext; }
                else if (ay >= az)        { u = (p[0]-cen[0])/maxext + 0.5f; v = 0.5f - (p[2]-cen[2])/maxext; }
                else                      { u = (p[0]-cen[0])/maxext + 0.5f; v = 0.5f - (p[1]-cen[1])/maxext; }
            } else if (tipo == 1) {                  // CYLINDER: angulo XZ (invertido) + altura Y (de arriba)
                u = 0.5f - atan2f(p[2]-cen[2], p[0]-cen[0]) / TWO_PI;
                v = (mx[1]-p[1]) / (ext[1] > 1e-6f ? ext[1] : 1.0f);
            } else {                                 // SPHERE: angulo invertido; el acos ya va de arriba
                float dx = p[0]-cen[0], dy = p[1]-cen[1], dz = p[2]-cen[2];
                float r = sqrtf(dx*dx + dy*dy + dz*dz); if (r < 1e-6f) r = 1.0f;
                float cy = dy/r; if (cy > 1.0f) cy = 1.0f; if (cy < -1.0f) cy = -1.0f;
                u = 0.5f - atan2f(dz, dx) / TWO_PI;
                v = acosf(cy) / (float)M_PI;
            }
            uvL[(size_t)(L+c)*2] = u; uvL[(size_t)(L+c)*2+1] = v;
        }
        // CYLINDER/SPHERE: la cara QUAD del lateral que cruza la COSTURA (angulo salta de +pi a
        // -pi) tiene corners con u~0 y u~1 -> se estiraria por toda la textura. Si el rango de u
        // es > 0.5, corro los corners de u<0.5 a u+1 para que la cara quede CONTIGUA. SOLO quads:
        // las tapas (ngons que dan toda la vuelta) NO son seams, no hay que correrlas.
        if ((tipo == 1 || tipo == 2) && cnt == 4) {
            float umn = 1e30f, umx = -1e30f;
            for (int c = 0; c < cnt; c++) { float u = uvL[(size_t)(L+c)*2]; if (u < umn) umn = u; if (u > umx) umx = u; }
            if (umx - umn > 0.5f)
                for (int c = 0; c < cnt; c++) if (uvL[(size_t)(L+c)*2] < 0.5f) uvL[(size_t)(L+c)*2] += 1.0f;
        }
        L += cnt;
    }
    // "agarrar TODA la textura": normalizar los UV de las caras SELECCIONADAS a [0,1] (asi el
    // cilindro/esfera no quedan corridos por el offset del angulo ni se salen de [0,1]).
    float umn = 1e30f, vmn = 1e30f, umx = -1e30f, vmx = -1e30f;
    { int L2 = 0; for (size_t f = 0; f < faces3d.size(); f++) { int cnt = (int)faces3d[f].idx.size();
        if (f < sel3d.size() && sel3d[f]) for (int c = 0; c < cnt; c++) {
            float u = uvL[(size_t)(L2+c)*2], v = uvL[(size_t)(L2+c)*2+1];
            if (u<umn) umn=u; if (u>umx) umx=u; if (v<vmn) vmn=v; if (v>vmx) vmx=v;
        } L2 += cnt; } }
    if (umx > umn && vmx > vmn) {
        float du = umx-umn, dv = vmx-vmn; int L2 = 0;
        for (size_t f = 0; f < faces3d.size(); f++) { int cnt = (int)faces3d[f].idx.size();
            if (f < sel3d.size() && sel3d[f]) for (int c = 0; c < cnt; c++) {
                uvL[(size_t)(L2+c)*2]   = (uvL[(size_t)(L2+c)*2]   - umn) / du;
                uvL[(size_t)(L2+c)*2+1] = (uvL[(size_t)(L2+c)*2+1] - vmn) / dv;
            } L2 += cnt; }
    }
    EscribirUVProyeccion(uvL);
}

// DUPLICATE (Shift+D) en Edit Mode: copia la seleccion segun el modo (verts ->
// verts sueltos; aristas -> aristas; caras -> caras con sus verts), offset epsilon, y
// la deja seleccionada para que el editor la mueva (libre). El original NO se toca.
bool Mesh::DuplicarSeleccionEdit() {
    EnsureEdit();
    if (!edit || !vertex || vertexSize <= 0) return false;
    UndoCapturarMallaGeo(this); // Ctrl+Z: snapshot pre-duplicado
    EditMesh* e = edit;
    const int nV = vertexSize;
    const bool hayRep = ((int)posRep.size() == nV);
    const int mode = EditSelectMode;

    std::vector<char> selRep(nV, 0);
    if (mode == SelVertex) {
        for (size_t k=0;k<e->editVerts.size();k++) if (k<e->vertSel.size()&&e->vertSel[k]&&e->editVerts[k]>=0&&e->editVerts[k]<nV) selRep[e->editVerts[k]]=1;
    } else if (mode == SelEdge) {
        for (size_t eg=0;eg<e->edgeSel.size();eg++) if (e->edgeSel[eg]) { selRep[e->editVerts[e->lineIdx[eg*2]]]=1; selRep[e->editVerts[e->lineIdx[eg*2+1]]]=1; }
    } else {
        for (size_t fe=0;fe<e->faces.size();fe++) if (fe<e->faceSel.size()&&e->faceSel[fe]) { const std::vector<int>& p=e->faces[fe]; for (size_t c=0;c<p.size();c++) selRep[e->editVerts[p[c]]]=1; }
    }
    #define DREP(gi) (hayRep ? posRep[gi] : (gi))

    // que ELEMENTOS copiar + que reps duplicar. CARAS: toda cara con TODOS sus verts seleccionados (directa en modo
    // CARA, o INDIRECTA en modo VERTICE/BORDE) -> asi duplicar el cubo entero en modo vertice copia la malla COMPLETA
    // (caras + bordes), no solo verts sueltos (pedido Dante). En vertice/borde ademas: BORDES sueltos (no cubiertos por
    // una cara duplicada) + verts sueltos restantes.
    std::vector<char> selFace(faces3d.size(), 0);
    std::vector<std::pair<int,int> > selEdgesDir;
    std::set<std::pair<int,int> > seenE;
    std::set<int> dupReps;
    std::set<int> repEnCara; // reps que ya lleva una cara duplicada (no se re-duplican como borde/vert suelto)
    bool selFaceAny = false;
    for (size_t f=0;f<faces3d.size();f++){ const std::vector<int>& idx=faces3d[f].idx; if (idx.size()<3) continue;
        bool todos=true; for (size_t c=0;c<idx.size();c++){ int gi=idx[c]; if (gi<0||gi>=nV||!selRep[DREP(gi)]){todos=false;break;} }
        if (todos){ selFace[f]=1; selFaceAny=true; for (size_t c=0;c<idx.size();c++){ int r=DREP(idx[c]); dupReps.insert(r); repEnCara.insert(r);} } }
    if (mode != SelFace) {
        // BORDES sueltos: ambos verts seleccionados (modo BORDE = explicito; modo VERTICE = derivado de vertSel) y NO
        // cubiertos por una cara duplicada (si lo estan, la cara ya los lleva -> no duplicar doble).
        for (size_t eg=0; eg*2+1 < e->lineIdx.size(); eg++){
            bool sel;
            if (mode == SelEdge) sel = (eg < e->edgeSel.size() && e->edgeSel[eg]);
            else { int va=e->lineIdx[eg*2], vb=e->lineIdx[eg*2+1]; sel = (va<(int)e->vertSel.size() && vb<(int)e->vertSel.size() && e->vertSel[va] && e->vertSel[vb]); }
            if (!sel) continue;
            int ra=e->editVerts[e->lineIdx[eg*2]], rb=e->editVerts[e->lineIdx[eg*2+1]]; if (ra==rb) continue;
            if (repEnCara.count(DREP(ra)) && repEnCara.count(DREP(rb))) continue; // ya cubierta por una cara duplicada
            int lo=ra<rb?ra:rb, hi=ra<rb?rb:ra;
            if (seenE.insert(std::make_pair(lo,hi)).second){ selEdgesDir.push_back(std::make_pair(ra,rb)); dupReps.insert(ra); dupReps.insert(rb); } }
        // VERTS sueltos restantes (no quedaron en ninguna cara/borde)
        for (int gi=0;gi<nV;gi++) if (selRep[DREP(gi)]) dupReps.insert(DREP(gi));
    }
    if (dupReps.empty()) return false;
    PoblarCapas(); // asegura las capas (las caras copiadas agregan corners a TODAS)

    // direccion del offset (normal de vertice promedio, o +Y) para separar la copia
    Vector3 dir(0,0,0);
    if (normals) for (std::set<int>::iterator it=dupReps.begin();it!=dupReps.end();++it){ int r=*it; dir.x+=normals[r*3]/127.0f; dir.y+=normals[r*3+1]/127.0f; dir.z+=normals[r*3+2]/127.0f; }
    { float ln=sqrtf(dir.x*dir.x+dir.y*dir.y+dir.z*dir.z); if (ln>1e-6f){dir.x/=ln;dir.y/=ln;dir.z/=ln;} else dir=Vector3(0,1,0); }
    Vector3 off = dir * 1e-3f;

    // crecer arrays: +1 vert por dupRep
    int nuevoN = nV + (int)dupReps.size();
    bool tNor=(normals!=NULL), tUV=(uv!=NULL), tCol=(vertexColor!=NULL);
    GLfloat* nVtx=new GLfloat[nuevoN*3]; GLbyte* nNor=new GLbyte[nuevoN*3];
    GLfloat* nUV=new GLfloat[nuevoN*2]; GLubyte* nCol=new GLubyte[nuevoN*4];
    std::vector<char> esCopia(nuevoN, 0);
    for (int gi=0;gi<nV;gi++){ nVtx[gi*3]=vertex[gi*3];nVtx[gi*3+1]=vertex[gi*3+1];nVtx[gi*3+2]=vertex[gi*3+2];
        if(tNor){nNor[gi*3]=normals[gi*3];nNor[gi*3+1]=normals[gi*3+1];nNor[gi*3+2]=normals[gi*3+2];}else{nNor[gi*3]=0;nNor[gi*3+1]=127;nNor[gi*3+2]=0;}
        if(tUV){nUV[gi*2]=uv[gi*2];nUV[gi*2+1]=uv[gi*2+1];}else{nUV[gi*2]=0;nUV[gi*2+1]=0;}
        if(tCol){for(int q=0;q<4;q++)nCol[gi*4+q]=vertexColor[gi*4+q];}else{for(int q=0;q<4;q++)nCol[gi*4+q]=255;} }
    std::map<int,int> newOf; int wi=nV;
    for (std::set<int>::iterator it=dupReps.begin();it!=dupReps.end();++it){ int r=*it;
        nVtx[wi*3]=vertex[r*3]+off.x;nVtx[wi*3+1]=vertex[r*3+1]+off.y;nVtx[wi*3+2]=vertex[r*3+2]+off.z;
        if(tNor){nNor[wi*3]=normals[r*3];nNor[wi*3+1]=normals[r*3+1];nNor[wi*3+2]=normals[r*3+2];}else{nNor[wi*3]=0;nNor[wi*3+1]=127;nNor[wi*3+2]=0;}
        if(tUV){nUV[wi*2]=uv[r*2];nUV[wi*2+1]=uv[r*2+1];}else{nUV[wi*2]=0;nUV[wi*2+1]=0;}
        if(tCol){for(int q=0;q<4;q++)nCol[wi*4+q]=vertexColor[r*4+q];}else{for(int q=0;q<4;q++)nCol[wi*4+q]=255;}
        esCopia[wi]=1; newOf[r]=wi; wi++; }
    delete[] vertex; delete[] normals; delete[] uv; delete[] vertexColor;
    vertex=nVtx; normals=nNor; uv=nUV; vertexColor=nCol; vertexSize=nuevoN;

    // agregar la geometria COPIADA (las caras/aristas referencian los verts nuevos).
    // OJO: capturar el tamaño ORIGINAL antes de push_back (si no, se itera sobre las copias).
    const size_t nFacesOrig = faces3d.size();
    // CARAS copiadas (cualquier modo que haya marcado selFace): heredan las capas de su cara original.
    std::vector<int> faceOff(nFacesOrig); { int Lc=0; for (size_t f=0;f<nFacesOrig;f++){ faceOff[f]=Lc; Lc+=(int)faces3d[f].idx.size(); } }
    for (size_t f=0;f<nFacesOrig;f++){ if (!selFace[f]) continue;
        std::vector<int> ring; { const std::vector<int>& idx=faces3d[f].idx; for (size_t c=0;c<idx.size();c++) ring.push_back(newOf[DREP(idx[c])]); }
        MeshFace mf; mf.idx=ring; mf.mat=faces3d[f].mat; mf.smooth=faces3d[f].smooth; faces3d.push_back(mf); // la copia conserva mesh part + shading
        for (size_t c=0;c<ring.size();c++) AgregarCornerCapas(this,faceOff[f]+(int)c); // copia las capas del corner original
    }
    // BORDES sueltos copiados (los que no quedaron dentro de una cara duplicada)
    std::set<int> repEnBorde;
    for (size_t i=0;i<selEdgesDir.size();i++){ looseEdges.push_back(newOf[selEdgesDir[i].first]); looseEdges.push_back(newOf[selEdgesDir[i].second]);
        repEnBorde.insert(selEdgesDir[i].first); repEnBorde.insert(selEdgesDir[i].second); }
    // VERTS sueltos COPIADOS: los dupReps que no quedaron en una cara ni en un borde copiado son VERTS SUELTOS
    // -> hay que registrarlos en looseVerts (sino la copia queda como vertice HUERFANO: invisible, no seleccionable,
    // y el siguiente GenerarRender la descarta -> "duplico un vertice solito y no lo puedo extruir", bug Dante).
    for (std::set<int>::iterator it=dupReps.begin(); it!=dupReps.end(); ++it){ int r=*it;
        if (repEnCara.count(r) || repEnBorde.count(r)) continue;
        looseVerts.push_back(newOf[r]); }
    #undef DREP
    // Si se agrego TOPOLOGIA (caras y/o bordes sueltos, en cualquier modo) -> rebuild por GenerarRender
    // (re-triangula + materialsGroup + CalcularBordes + remapea loose edges + capas). Si SOLO se duplicaron
    // verts sueltos -> CalcularBordes, que no re-numera los verts (la copia sigue seleccionable y editable).
    bool huboTopo = selFaceAny || !selEdgesDir.empty();
    if (huboTopo) {
        std::vector<Vector3> posCopia = CapturarPosSel(esCopia); // restaurar por POS (re-merge)
        GenerarRender();
        ReconstruirEditSelPorPos(posCopia); // deja seleccionada la COPIA
    } else {
        CalcularBordes();             // solo verts sueltos: indices estables (no re-merge)
        ReconstruirEditSel(esCopia);  // deja seleccionada la COPIA
    }
    return true;
}

// RIP (V, pedido Dante): SEPARA la malla a lo largo de la seleccion (el "corte"). Flood-fill de las caras en
// LADOS conexos que NO cruzan el corte; por cada lado>0 duplica los verts del corte que toca (con un offset chico
// hacia el centro del lado, para que NO se re-unan al mergear por posicion y se vea la separacion) y reasigna esas
// caras a las copias. Objetivo: dividir un cubo -> rip -> con L seleccionar una mitad (ya isla aparte) -> borrar.
// Devuelve false si la seleccion no separa nada (corte vacio o que no parte la malla). Deja seleccionada la pieza nueva.
bool Mesh::RipSeleccionEdit() {
    EnsureEdit();
    if (!edit || !vertex || vertexSize <= 0) return false;
    EditMesh* e = edit;
    const int nV = vertexSize;
    const bool hayRep = ((int)posRep.size() == nV);
    #define RREP(gi) (hayRep ? posRep[gi] : (gi))
    const int mode = EditSelectMode;

    // 1) CUT EDGES (pares de reps lo<hi) segun el modo de seleccion
    std::set<std::pair<int,int> > cutEdges;
    if (mode == SelEdge) {
        for (size_t eg=0; eg<e->edgeSel.size(); eg++) if (e->edgeSel[eg]){
            int ra=RREP(e->editVerts[e->lineIdx[eg*2]]), rb=RREP(e->editVerts[e->lineIdx[eg*2+1]]);
            if (ra!=rb) cutEdges.insert(ra<rb?std::make_pair(ra,rb):std::make_pair(rb,ra)); }
    } else { // VERTICE o CARA: las aristas de la malla con AMBOS reps seleccionados
        std::vector<char> selRep(nV,0);
        if (mode==SelFace){ for (size_t fe=0;fe<e->faces.size();fe++) if (fe<e->faceSel.size()&&e->faceSel[fe]){ const std::vector<int>&p=e->faces[fe]; for (size_t c=0;c<p.size();c++) selRep[RREP(e->editVerts[p[c]])]=1; } }
        else { for (size_t k=0;k<e->editVerts.size();k++) if (k<e->vertSel.size()&&e->vertSel[k]) selRep[RREP(e->editVerts[k])]=1; }
        for (size_t i=0;i+1<edges.size();i+=2){ int ra=edges[i], rb=edges[i+1]; if (ra<0||rb<0||ra==rb||ra>=nV||rb>=nV) continue;
            if (selRep[ra] && selRep[rb]) cutEdges.insert(ra<rb?std::make_pair(ra,rb):std::make_pair(rb,ra)); }
    }
    if (cutEdges.empty()) return false;

    // 2) FLOOD FILL: caras en LADOS conexos que no cruzan el corte
    const int nF = (int)faces3d.size();
    std::map<std::pair<int,int>, std::vector<int> > edgeFaces;
    for (int f=0;f<nF;f++){ const std::vector<int>& idx=faces3d[f].idx; int m=(int)idx.size(); if (m<3) continue;
        for (int c=0;c<m;c++){ int ra=RREP(idx[c]), rb=RREP(idx[(c+1)%m]); if (ra==rb) continue;
            edgeFaces[ra<rb?std::make_pair(ra,rb):std::make_pair(rb,ra)].push_back(f); } }
    std::vector<int> side(nF, -1);
    int numSides=0;
    for (int f0=0;f0<nF;f0++){ if (side[f0]>=0 || faces3d[f0].idx.size()<3) continue;
        side[f0]=numSides; std::vector<int> st; st.push_back(f0);
        while (!st.empty()){ int f=st.back(); st.pop_back(); const std::vector<int>& idx=faces3d[f].idx; int m=(int)idx.size();
            for (int c=0;c<m;c++){ int ra=RREP(idx[c]), rb=RREP(idx[(c+1)%m]); if (ra==rb) continue;
                std::pair<int,int> key=ra<rb?std::make_pair(ra,rb):std::make_pair(rb,ra);
                if (cutEdges.count(key)) continue;                 // NO cruzar el corte
                const std::vector<int>& nb=edgeFaces[key];
                for (size_t j=0;j<nb.size();j++) if (side[nb[j]]<0){ side[nb[j]]=numSides; st.push_back(nb[j]); } } }
        numSides++; }
    if (numSides < 2) return false;                                // el corte no separa la malla

    UndoCapturarMallaGeo(this);
    PoblarCapas();

    // 3) CUT VERTS + que LADOS toca cada uno
    std::set<int> cutVerts;
    for (std::set<std::pair<int,int> >::iterator it=cutEdges.begin();it!=cutEdges.end();++it){ cutVerts.insert(it->first); cutVerts.insert(it->second); }
    std::map<int, std::set<int> > ladosDe;
    for (int f=0;f<nF;f++){ if (side[f]<0) continue; const std::vector<int>& idx=faces3d[f].idx;
        for (size_t c=0;c<idx.size();c++){ int r=RREP(idx[c]); if (cutVerts.count(r)) ladosDe[r].insert(side[f]); } }

    // 4) centroide de cada lado (solo para la DIRECCION del offset minimo de separacion)
    std::vector<Vector3> cen(numSides, Vector3(0,0,0)); std::vector<int> cenN(numSides,0);
    for (int f=0;f<nF;f++){ int s=side[f]; if (s<0) continue; const std::vector<int>& idx=faces3d[f].idx;
        for (size_t c=0;c<idx.size();c++){ int gv=idx[c]; cen[s].x+=vertex[gv*3]; cen[s].y+=vertex[gv*3+1]; cen[s].z+=vertex[gv*3+2]; cenN[s]++; } }
    for (int s=0;s<numSides;s++) if (cenN[s]>0){ cen[s].x/=cenN[s]; cen[s].y/=cenN[s]; cen[s].z/=cenN[s]; }

    // 5) (lado>0, cutVert) -> vert NUEVO
    std::map<std::pair<int,int>, int> nuevoDe;
    int extra=0;
    for (std::set<int>::iterator it=cutVerts.begin();it!=cutVerts.end();++it){ int r=*it; std::set<int>& ls=ladosDe[r];
        for (std::set<int>::iterator s=ls.begin();s!=ls.end();++s) if (*s>0) nuevoDe[std::make_pair(*s,r)] = nV+(extra++); }
    if (extra==0) return false;

    // 6) crecer los arrays con las copias (offset 4% hacia el centro del lado)
    int nuevoN=nV+extra;
    bool tNor=(normals!=NULL), tUV=(uv!=NULL), tCol=(vertexColor!=NULL);
    GLfloat* nVtx=new GLfloat[nuevoN*3]; GLbyte* nNor=new GLbyte[nuevoN*3]; GLfloat* nUV=new GLfloat[nuevoN*2]; GLubyte* nCol=new GLubyte[nuevoN*4];
    std::vector<char> esNuevo(nuevoN,0);
    for (int gi=0;gi<nV;gi++){ nVtx[gi*3]=vertex[gi*3];nVtx[gi*3+1]=vertex[gi*3+1];nVtx[gi*3+2]=vertex[gi*3+2];
        if(tNor){nNor[gi*3]=normals[gi*3];nNor[gi*3+1]=normals[gi*3+1];nNor[gi*3+2]=normals[gi*3+2];}else{nNor[gi*3]=0;nNor[gi*3+1]=127;nNor[gi*3+2]=0;}
        if(tUV){nUV[gi*2]=uv[gi*2];nUV[gi*2+1]=uv[gi*2+1];}else{nUV[gi*2]=0;nUV[gi*2+1]=0;}
        if(tCol){for(int q=0;q<4;q++)nCol[gi*4+q]=vertexColor[gi*4+q];}else{for(int q=0;q<4;q++)nCol[gi*4+q]=255;} }
    for (std::map<std::pair<int,int>,int>::iterator it=nuevoDe.begin();it!=nuevoDe.end();++it){
        int s=it->first.first, r=it->first.second, wi=it->second;
        // OFFSET MINIMO (1e-3, INVISIBLE -> ~0.05% de un cubo, sub-pixel) hacia el centro del lado. NO es para verse:
        // es para que la copia caiga en una CELDA distinta del edit mesh (cuantizado a 1e-4) y las 2 mitades queden
        // como editVerts SEPARADOS (sino L las ve unidas y no podes seleccionar/borrar una sola). El render no las
        // mergea porque ya tienen normales distintas (caras de lados distintos); el offset es solo para el edit mesh.
        float dx=cen[s].x-vertex[r*3], dy=cen[s].y-vertex[r*3+1], dz=cen[s].z-vertex[r*3+2];
        float dl=sqrtf(dx*dx+dy*dy+dz*dz), k=(dl>1e-6f)?(0.001f/dl):0.0f;
        nVtx[wi*3]=vertex[r*3]+dx*k; nVtx[wi*3+1]=vertex[r*3+1]+dy*k; nVtx[wi*3+2]=vertex[r*3+2]+dz*k;
        if(tNor){nNor[wi*3]=normals[r*3];nNor[wi*3+1]=normals[r*3+1];nNor[wi*3+2]=normals[r*3+2];}else{nNor[wi*3]=0;nNor[wi*3+1]=127;nNor[wi*3+2]=0;}
        if(tUV){nUV[wi*2]=uv[r*2];nUV[wi*2+1]=uv[r*2+1];}else{nUV[wi*2]=0;nUV[wi*2+1]=0;}
        if(tCol){for(int q=0;q<4;q++)nCol[wi*4+q]=vertexColor[r*4+q];}else{for(int q=0;q<4;q++)nCol[wi*4+q]=255;}
        esNuevo[wi]=1; }
    delete[] vertex; delete[] normals; delete[] uv; delete[] vertexColor;
    vertex=nVtx; normals=nNor; uv=nUV; vertexColor=nCol; vertexSize=nuevoN;

    // 7) reasignar las caras de los lados>0: sus cutVerts -> la copia del lado
    for (int f=0;f<nF;f++){ int s=side[f]; if (s<=0) continue; std::vector<int>& idx=faces3d[f].idx;
        for (size_t c=0;c<idx.size();c++){ int r=RREP(idx[c]); std::map<std::pair<int,int>,int>::iterator it=nuevoDe.find(std::make_pair(s,r));
            if (it!=nuevoDe.end()) idx[c]=it->second; } }
    #undef RREP

    // 8) rebuild + dejar seleccionada la pieza NUEVA (las copias) -> lista para moverla con G
    std::vector<Vector3> posNuevos = CapturarPosSel(esNuevo);
    GenerarRender();
    ReconstruirEditSelPorPos(posNuevos);
    return true;
}

// F: "New Edge/Face from Vertices". Con los VERTICES seleccionados crea: 2 -> una
// arista; 3 -> un triangulo; 4+ -> un ngon (ordenando los verts alrededor de su
// centro en el plano de mejor ajuste). NO crea verts nuevos (conecta los existentes).
bool Mesh::CrearCaraEdit() {
    EnsureEdit();
    if (!edit || !vertex || vertexSize <= 0) return false;
    UndoCapturarMallaGeo(this); // Ctrl+Z: snapshot pre-crear-cara
    PoblarCapas(); // asegura las capas (la cara nueva agrega corners a TODAS)
    EditMesh* e = edit;
    const int nV = vertexSize;
    // reps seleccionados, en CUALQUIER modo (vertex: vertSel; edge: las 2 puntas de
    // cada borde sel -> 2 bordes separados = 4 verts = una cara; face: los verts de la cara)
    std::vector<int> reps;
    std::set<int> vistos;
    if (EditSelectMode == SelEdge) {
        for (size_t eg=0; eg<e->edgeSel.size(); eg++) if (e->edgeSel[eg]) {
            int ra=e->editVerts[e->lineIdx[eg*2]], rb=e->editVerts[e->lineIdx[eg*2+1]];
            if (ra>=0&&ra<nV&&vistos.insert(ra).second) reps.push_back(ra);
            if (rb>=0&&rb<nV&&vistos.insert(rb).second) reps.push_back(rb);
        }
    } else if (EditSelectMode == SelFace) {
        for (size_t fe=0; fe<e->faces.size(); fe++) if (fe<e->faceSel.size() && e->faceSel[fe]) {
            const std::vector<int>& p=e->faces[fe];
            for (size_t c=0;c<p.size();c++){ int r=e->editVerts[p[c]]; if (r>=0&&r<nV&&vistos.insert(r).second) reps.push_back(r); }
        }
    } else {
        for (size_t k=0;k<e->editVerts.size();k++) if (k<e->vertSel.size() && e->vertSel[k]) { int r=e->editVerts[k]; if (r>=0&&r<nV&&vistos.insert(r).second) reps.push_back(r); }
    }
    if ((int)reps.size() < 2) return false;
    // posiciones seleccionadas (restaurar por POSICION tras el re-merge de GenerarRender)
    std::vector<Vector3> posSel;
    for (size_t i=0;i<reps.size();i++){ int r=reps[i]; if (r>=0&&r<vertexSize) posSel.push_back(Vector3(vertex[r*3],vertex[r*3+1],vertex[r*3+2])); }

    if (reps.size() == 2) {
        looseEdges.push_back(reps[0]); looseEdges.push_back(reps[1]);
        CalcularBordes(); RecalcularNormales(); // registra el loose edge (sin tocar faces3d)
    } else {
        std::vector<int> ring = reps;
        if (ring.size() > 3) {
            // ordenar alrededor del centro EN EL PLANO de los verts. La normal del
            // plano se saca de forma ROBUSTA = el mayor producto cruz (vi-c)x(vj-c).
            // (Newell sobre el ring SIN ordenar daba un poligono auto-intersectado ->
            // normal degenerada -> quad en moño; se notaba al DUPLICAR, que deja los
            // indices en orden "diagonal".)
            Vector3 c(0,0,0); for (size_t i=0;i<ring.size();i++) c=c+Vector3(vertex[ring[i]*3],vertex[ring[i]*3+1],vertex[ring[i]*3+2]); c=c*(1.0f/(float)ring.size());
            Vector3 n(0,0,0); float mejor=0.0f;
            for (size_t i=0;i<ring.size();i++){ Vector3 di(vertex[ring[i]*3]-c.x,vertex[ring[i]*3+1]-c.y,vertex[ring[i]*3+2]-c.z);
                for (size_t j=i+1;j<ring.size();j++){ Vector3 dj(vertex[ring[j]*3]-c.x,vertex[ring[j]*3+1]-c.y,vertex[ring[j]*3+2]-c.z);
                    Vector3 cr(di.y*dj.z-di.z*dj.y, di.z*dj.x-di.x*dj.z, di.x*dj.y-di.y*dj.x);
                    float m=cr.x*cr.x+cr.y*cr.y+cr.z*cr.z; if (m>mejor){ mejor=m; n=cr; } } }
            { float ln=sqrtf(n.x*n.x+n.y*n.y+n.z*n.z); if (ln>1e-6f) n=n*(1.0f/ln); else n=Vector3(0,1,0); }
            Vector3 u = (fabsf(n.y)<0.9f)? Vector3(0,1,0) : Vector3(1,0,0); // un vector no paralelo
            Vector3 ejeU(u.y*n.z-u.z*n.y, u.z*n.x-u.x*n.z, u.x*n.y-u.y*n.x); // u x n
            { float ln=sqrtf(ejeU.x*ejeU.x+ejeU.y*ejeU.y+ejeU.z*ejeU.z); if (ln>1e-6f) ejeU=ejeU*(1.0f/ln); }
            Vector3 ejeV(n.y*ejeU.z-n.z*ejeU.y, n.z*ejeU.x-n.x*ejeU.z, n.x*ejeU.y-n.y*ejeU.x); // n x u
            // sort por angulo (insertion sort, C++03)
            std::vector<float> ang(ring.size());
            for (size_t i=0;i<ring.size();i++){ Vector3 d(vertex[ring[i]*3]-c.x,vertex[ring[i]*3+1]-c.y,vertex[ring[i]*3+2]-c.z); ang[i]=atan2f(d.x*ejeV.x+d.y*ejeV.y+d.z*ejeV.z, d.x*ejeU.x+d.y*ejeU.y+d.z*ejeU.z); }
            for (size_t i=1;i<ring.size();i++){ int rv=ring[i]; float av=ang[i]; int j=(int)i-1; while (j>=0 && ang[j]>av){ ring[j+1]=ring[j]; ang[j+1]=ang[j]; j--; } ring[j+1]=rv; ang[j+1]=av; }
        }
        // NO DUPLICAR: si ya existe una cara con EXACTAMENTE el mismo conjunto de posiciones (sin importar el orden),
        // no crear otra. Antes, apretar "F" de nuevo sobre los mismos verts apilaba caras iguales encimadas (bug Dante:
        // el quad "no se veia" por z-fight / winding opuesto). Se compara por posRep (posicion), no por indice gpu.
        {
            const bool hayRep = ((int)posRep.size() == nV);
            std::set<int> nueva; for (size_t i=0;i<ring.size();i++){ int g=ring[i]; if(g>=0&&g<nV) nueva.insert(hayRep?posRep[g]:g); }
            for (size_t f=0; f<faces3d.size(); f++){ const std::vector<int>& ix=faces3d[f].idx;
                std::set<int> s; for (size_t c=0;c<ix.size();c++){ int g=ix[c]; if(g>=0&&g<nV) s.insert(hayRep?posRep[g]:g); }
                if (s.size()==nueva.size() && s==nueva){ ReconstruirEditSelPorPos(posSel); return false; } // ya existe -> no duplicar
            }
        }
        // vert -> primer corner (sobre faces3d ACTUAL) para que la cara nueva herede el
        // uv/color del corner existente de cada vert (asi se funde con lo que la rodea)
        std::vector<int> vertCorner(nV, -1); int Lc=0;
        for (size_t f=0;f<faces3d.size();f++){ const std::vector<int>& ix=faces3d[f].idx;
            for (size_t c=0;c<ix.size();c++){ if (ix[c]>=0&&ix[c]<nV&&vertCorner[ix[c]]<0) vertCorner[ix[c]]=Lc; Lc++; } }
        MeshFace mf; mf.idx=ring; faces3d.push_back(mf);
        for (size_t c=0;c<ring.size();c++){ int v=ring[c]; AgregarCornerCapas(this,(v>=0&&v<nV)?vertCorner[v]:-1); }
        RecalcularNormales(); // normales frescas (incluye la cara nueva)
        GenerarRender();      // re-merge + render (re-triangula + materialsGroup + CalcularBordes)
    }
    ReconstruirEditSelPorPos(posSel); // mantener la MISMA seleccion (por posicion)
    return true;
}

// Cruza un QUAD de la malla de edicion: dada la cara editF y la arista de ENTRADA
// egIn con orientacion (Ain,Bin) [edit verts], devuelve la arista OPUESTA egOut con su
// orientacion (Aout,Bout) propagada por los lados (Aout = vecino de Ain por un lado).
// false si la cara no es un quad o egIn no le pertenece. Lo usa el loop cut.
static bool CruzarQuadEdit(EditMesh* e, int editF, int egIn, int Ain, int Bin,
                           int& egOut, int& Aout, int& Bout) {
    if (editF < 0 || editF >= (int)e->faces.size()) return false;
    const std::vector<int>& poly = e->faces[editF];
    const std::vector<int>& fe   = e->faceEdges[editF];
    if (poly.size() != 4 || fe.size() != 4) return false;
    int li = -1; for (int c=0;c<4;c++) if (fe[c]==egIn){ li=c; break; }
    if (li < 0) return false;
    egOut = fe[(li+2)%4];
    int p0 = poly[li], p1 = poly[(li+1)%4], p2 = poly[(li+2)%4], p3 = poly[(li+3)%4];
    if      (Ain == p0 && Bin == p1) { Aout = p3; Bout = p2; }
    else if (Ain == p1 && Bin == p0) { Aout = p2; Bout = p3; }
    else return false;
    return true;
}

// recorre el loop de quads desde startEditEdge (arista de la malla de edicion) cruzando
// el borde opuesto de cada quad. Llena rungEg/A/B (aristas cruzadas DIRIGIDAS, edit verts)
// + loopFaces (caras de edicion, una entre rung[i] y rung[i+1]) + cerrado. Lo usan
// LoopCutEdit (el corte) y LoopCutPreview (dibujar la vista previa).
bool Mesh::LoopCutRecorrido(int startEditEdge, std::vector<int>& rungEg, std::vector<int>& rungA,
                            std::vector<int>& rungB, std::vector<int>& loopFaces, bool& cerrado) {
    EnsureEdit(); if (!edit) return false;
    EditMesh* e = edit;
    const int nE = e->NumEdges(), nEF = (int)e->faces.size();
    if (startEditEdge < 0 || startEditEdge >= nE) return false;
    rungEg.clear(); rungA.clear(); rungB.clear(); loopFaces.clear(); cerrado=false;

    std::vector<std::vector<int> > edgeFaces(nE);
    for (int f=0; f<nEF; f++){ const std::vector<int>& fe=e->faceEdges[f];
        for (size_t i=0;i<fe.size();i++){ int eg=fe[i]; if (eg>=0&&eg<nE) edgeFaces[eg].push_back(f); } }
    int ea = e->lineIdx[startEditEdge*2], eb = e->lineIdx[startEditEdge*2+1];
    std::vector<char> faceUsada(nEF, 0);
    rungEg.push_back(startEditEdge); rungA.push_back(ea); rungB.push_back(eb);
    { // hacia adelante
        int curEg = startEditEdge, A = ea, B = eb;
        while (true) {
            int nf = -1;
            for (size_t s=0;s<edgeFaces[curEg].size();s++){ int ff=edgeFaces[curEg][s]; if (!faceUsada[ff]){ nf=ff; break; } }
            if (nf < 0) break;
            int egOut, Aout, Bout;
            if (!CruzarQuadEdit(e, nf, curEg, A, B, egOut, Aout, Bout)) break;
            faceUsada[nf] = 1; loopFaces.push_back(nf);
            if (egOut == startEditEdge) { cerrado = true; break; }
            rungEg.push_back(egOut); rungA.push_back(Aout); rungB.push_back(Bout);
            curEg = egOut; A = Aout; B = Bout;
        }
    }
    if (!cerrado) { // hacia atras desde la OTRA cara del start edge (prepende)
        std::vector<int> bEg, bA, bB, bFaces;
        int curEg = startEditEdge, A = ea, B = eb;
        while (true) {
            int nf = -1;
            for (size_t s=0;s<edgeFaces[curEg].size();s++){ int ff=edgeFaces[curEg][s]; if (!faceUsada[ff]){ nf=ff; break; } }
            if (nf < 0) break;
            int egOut, Aout, Bout;
            if (!CruzarQuadEdit(e, nf, curEg, A, B, egOut, Aout, Bout)) break;
            faceUsada[nf] = 1; bFaces.push_back(nf);
            bEg.push_back(egOut); bA.push_back(Aout); bB.push_back(Bout);
            curEg = egOut; A = Aout; B = Bout;
        }
        std::vector<int> nEg, nA, nB, nF;
        for (int i=(int)bEg.size()-1;i>=0;i--){ nEg.push_back(bEg[i]); nA.push_back(bA[i]); nB.push_back(bB[i]); }
        for (size_t i=0;i<rungEg.size();i++){ nEg.push_back(rungEg[i]); nA.push_back(rungA[i]); nB.push_back(rungB[i]); }
        for (int i=(int)bFaces.size()-1;i>=0;i--) nF.push_back(bFaces[i]);
        for (size_t i=0;i<loopFaces.size();i++) nF.push_back(loopFaces[i]);
        rungEg.swap(nEg); rungA.swap(nA); rungB.swap(nB); loopFaces.swap(nF);
    }
    return !loopFaces.empty();
}

extern bool gLoopCutEsPunto; // (LayoutInput.cpp) preview de PUNTO (borde suelto) vs LINEA (anillo de quads)

// la arista de edicion 'startEditEdge' es una arista SUELTA (esta en looseEdges, sin caras)? Lo usan el loop cut y
// su preview para decidir el fallback de "subdividir un solo borde" (agrega un vert) en vez del anillo de quads.
static bool BordeEsSuelto(Mesh* m, int startEditEdge){
    EditMesh* e = m->edit; if (!e) return false;
    if (startEditEdge<0 || startEditEdge>=e->NumEdges()) return false;
    const int nV=m->vertexSize; const bool hayRep=((int)m->posRep.size()==nV);
    int ea=e->lineIdx[startEditEdge*2], eb=e->lineIdx[startEditEdge*2+1];
    if (ea<0||eb<0||ea>=(int)e->editVerts.size()||eb>=(int)e->editVerts.size()) return false;
    int ga=e->editVerts[ea], gb=e->editVerts[eb]; if(ga<0||gb<0||ga>=nV||gb>=nV) return false;
    int repA=hayRep?m->posRep[ga]:ga, repB=hayRep?m->posRep[gb]:gb; if(repA==repB) return false;
    for (size_t i=0;i+1<m->looseEdges.size();i+=2){ int p=m->looseEdges[i],q=m->looseEdges[i+1];
        int rp=hayRep?m->posRep[p]:p, rq=hayRep?m->posRep[q]:q;
        if ((rp==repA&&rq==repB)||(rp==repB&&rq==repA)) return true; }
    return false;
}

// SUBDIVIDIR una ARISTA SUELTA (sin caras): agrega numCuts verts sobre ella (factor desliza) y la parte en
// numCuts+1 aristas sueltas. Es el fallback del loop cut sobre el perfil del Screw (aristas sueltas). El UV no
// importa (el Screw lo recalcula). outSel = posiciones LOCALES de los verts nuevos (para dejarlos seleccionados).
// Devuelve false si la arista NO es suelta (tiene caras: ese caso lo maneja el loop cut de anillo).
static bool SubdivBordeSuelto(Mesh* m, int startEditEdge, int numCuts, float factor, std::vector<Vector3>* outSel){
    m->EnsureEdit(); EditMesh* e = m->edit; if (!e || !m->vertex) return false;
    if (!BordeEsSuelto(m, startEditEdge)) return false;
    if (numCuts < 1) numCuts = 1;
    const int nV = m->vertexSize;
    const bool hayRep = ((int)m->posRep.size()==nV);
    int ea=e->lineIdx[startEditEdge*2], eb=e->lineIdx[startEditEdge*2+1];
    int ga=e->editVerts[ea], gb=e->editVerts[eb];
    int repA=hayRep?m->posRep[ga]:ga, repB=hayRep?m->posRep[gb]:gb;
    // verts nuevos (pos interpolada A->B; normal Y+, uv/color default: el Screw los recalcula)
    std::vector<GLfloat> vp(m->vertex, m->vertex+nV*3);
    std::vector<GLbyte>  vn; if(m->normals) vn.assign(m->normals,m->normals+nV*3); else vn.assign((size_t)nV*3,(GLbyte)0);
    std::vector<GLfloat> vu; if(m->uv) vu.assign(m->uv,m->uv+nV*2); else vu.assign((size_t)nV*2,0.0f);
    std::vector<GLubyte> vc; if(m->vertexColor) vc.assign(m->vertexColor,m->vertexColor+nV*4); else vc.assign((size_t)nV*4,(GLubyte)255);
    std::vector<int> nuevo(numCuts);
    if (outSel) outSel->clear();
    for (int j=0;j<numCuts;j++){ float s=(float)(j+1+factor)/(float)(numCuts+1); if(s<0)s=0; if(s>1)s=1;
        int gi=(int)(vp.size()/3);
        float px=m->vertex[ga*3]*(1-s)+m->vertex[gb*3]*s, py=m->vertex[ga*3+1]*(1-s)+m->vertex[gb*3+1]*s, pz=m->vertex[ga*3+2]*(1-s)+m->vertex[gb*3+2]*s;
        vp.push_back(px);vp.push_back(py);vp.push_back(pz);
        vn.push_back(0);vn.push_back(127);vn.push_back(0); vu.push_back(0);vu.push_back(0);
        vc.push_back(255);vc.push_back(255);vc.push_back(255);vc.push_back(255);
        nuevo[j]=gi; if (outSel) outSel->push_back(Vector3(px,py,pz));
    }
    int nN=(int)(vp.size()/3);
    delete[] m->vertex;      m->vertex=new GLfloat[nN*3];      for(int i=0;i<nN*3;i++) m->vertex[i]=vp[i];
    delete[] m->normals;     m->normals=new GLbyte[nN*3];      for(int i=0;i<nN*3;i++) m->normals[i]=vn[i];
    delete[] m->uv;          m->uv=new GLfloat[nN*2];          for(int i=0;i<nN*2;i++) m->uv[i]=vu[i];
    delete[] m->vertexColor; m->vertexColor=new GLubyte[nN*4]; for(int i=0;i<nN*4;i++) m->vertexColor[i]=vc[i];
    m->vertexSize=nN;
    // partir las aristas sueltas (a,b) -> cadena a -> cuts -> b (orden segun la direccion de la arista)
    std::vector<int> nLoose;
    for (size_t i=0;i+1<m->looseEdges.size();i+=2){ int p=m->looseEdges[i],q=m->looseEdges[i+1];
        int rp=hayRep?m->posRep[p]:p, rq=hayRep?m->posRep[q]:q;
        if ((rp==repA&&rq==repB)||(rp==repB&&rq==repA)){
            std::vector<int> chain; chain.push_back(p);
            if (rp==repA) { for(int k=0;k<numCuts;k++) chain.push_back(nuevo[k]); }    // p==A: A->B directo
            else          { for(int k=numCuts-1;k>=0;k--) chain.push_back(nuevo[k]); } // p==B: B->A invertido
            chain.push_back(q);
            for(size_t k=0;k+1<chain.size();k++){ nLoose.push_back(chain[k]); nLoose.push_back(chain[k+1]); }
        } else { nLoose.push_back(p); nLoose.push_back(q); }
    }
    m->looseEdges.swap(nLoose);
    m->GenerarRender();
    return true;
}

// vista previa del loop cut: devuelve los SEGMENTOS de linea (pares de puntos LOCALES)
// del corte que se generaria, sin tocar la geometria. Para que el viewport lo dibuje.
bool Mesh::LoopCutPreview(int startEditEdge, int numCuts, float factor, std::vector<float>& outSegs) {
    outSegs.clear();
    EnsureEdit(); if (!edit || !vertex) return false;
    if (numCuts < 1) numCuts = 1;
    EditMesh* e = edit;
    std::vector<int> rungEg, rungA, rungB, loopFaces; bool cerrado=false;
    if (!LoopCutRecorrido(startEditEdge, rungEg, rungA, rungB, loopFaces, cerrado)) {
        // sin anillo de quads: si es un BORDE SUELTO, el preview son PUNTO(s) sobre la arista (no una linea)
        if (!BordeEsSuelto(this, startEditEdge)) { gLoopCutEsPunto=false; return false; }
        int a=e->lineIdx[startEditEdge*2], b=e->lineIdx[startEditEdge*2+1];
        if (a<0||b<0||a*3+2>=(int)e->pos.size()||b*3+2>=(int)e->pos.size()){ gLoopCutEsPunto=false; return false; }
        for (int j=0;j<numCuts;j++){ float s=(float)(j+1+factor)/(float)(numCuts+1); if(s<0)s=0; if(s>1)s=1;
            for (int q=0;q<3;q++) outSegs.push_back(e->pos[a*3+q]*(1-s)+e->pos[b*3+q]*s); } // 1 PUNTO por corte
        gLoopCutEsPunto = true;
        return !outSegs.empty();
    }
    gLoopCutEsPunto = false; // hay anillo -> preview de LINEAS
    const int L = (int)loopFaces.size();
    if (L == 0) return false;
    std::vector<float> sj(numCuts);
    for (int j=0;j<numCuts;j++){ float s=(float)(j+1+factor)/(float)(numCuts+1);
        if (s<0.0f) s=0.0f; if (s>1.0f) s=1.0f; sj[j]=s; }
    // por cada cara del loop, una linea por corte entre el punto de su rung de entrada y el de salida
    for (int i=0;i<L;i++){
        int ri=i, ro = cerrado ? ((i+1)%(int)rungEg.size()) : (i+1);
        if (ro >= (int)rungEg.size()) continue;
        // posiciones LOCALES de las puntas de los rungs (edit pos)
        float* eAi=&e->pos[rungA[ri]*3]; float* eBi=&e->pos[rungB[ri]*3];
        float* eAo=&e->pos[rungA[ro]*3]; float* eBo=&e->pos[rungB[ro]*3];
        for (int j=0;j<numCuts;j++){ float s=sj[j];
            for (int q=0;q<3;q++) outSegs.push_back(eAi[q]*(1-s)+eBi[q]*s);
            for (int q=0;q<3;q++) outSegs.push_back(eAo[q]*(1-s)+eBo[q]*s);
        }
    }
    return !outSegs.empty();
}

// LOOP CUT (Ctrl+R): corta un loop de quads con numCuts aristas nuevas. startEditEdge =
// la arista (en la malla de edicion) bajo el mouse; el loop se camina cruzando el borde
// OPUESTO de cada quad (igual que el loop-select). factor in [-1,1] desliza los cortes
// (0=centrado/parejo, +1/-1 = pegados a un borde del quad). Crea verts nuevos en las
// aristas cruzadas (interpolando normal/uv/color) y parte cada cara del loop en numCuts+1.
// Solo quads (tri/ngones cortan el loop). Devuelve false si no se puede.
bool Mesh::LoopCutEdit(int startEditEdge, int numCuts, float factor) {
    EnsureEdit();
    if (!edit || !vertex || vertexSize <= 0) return false;
    // NO captura undo aca: el loop cut RE-CORTA en cada frame del slide / panel redo -> inundaria el stack.
    // La captura (1 sola vez, pre-corte) la hace el caller del editor cuando arranca el corte (ver LCGuardar).
    if (numCuts < 1) numCuts = 1;
    EditMesh* e = edit;
    const int nV = vertexSize;
    const bool hayRep = ((int)posRep.size() == nV);

    std::vector<int> rungEg, rungA, rungB, loopFaces; bool cerrado = false;
    if (!LoopCutRecorrido(startEditEdge, rungEg, rungA, rungB, loopFaces, cerrado)) {
        // sin anillo de quads (perfil del Screw / arista suelta) -> subdividir SOLO ese borde: agrega numCuts verts.
        std::vector<Vector3> selPos;
        bool ok = SubdivBordeSuelto(this, startEditEdge, numCuts, factor, &selPos);
        if (ok) ReconstruirEditSelPorPos(selPos); // deja los verts nuevos seleccionados
        return ok;
    }
    const int L = (int)loopFaces.size();
    if (L == 0) return false;

    // === reconstruir geometria: caras NO-loop intactas; partir las del loop ===
    // El render (uv/color/NORMAL) lo DERIVA GenerarRender de las CAPAS; aca solo armamos las
    // posiciones (vp) y, EN PARALELO, la FUENTE de capa de cada corner nuevo (src): copiar del
    // corner viejo (verts existentes) o LERP entre dos (verts del corte). La normal de los verts
    // del corte sale INTERPOLADA de cornerNormal (via src) -> queda bien SIN recalcular, asi loop
    // cut PRESERVA el shading existente (no recomputa todo).
    PoblarCapas();
    std::vector<int> faceOff(faces3d.size()); { int Lc=0; for (size_t f=0;f<faces3d.size();f++){ faceOff[f]=Lc; Lc+=(int)faces3d[f].idx.size(); } }
    std::vector<GLfloat> vp(vertex, vertex+nV*3);
    // parametros de corte s_j: los cortes mantienen su separacion pareja y se DESLIZAN
    // rigido con el factor (no se estiran). factor 0 = parejo; +1/-1 = el grupo se corre
    // hasta que el corte del extremo toca el borde (Blender). s_j = (j+1+factor)/(n+1).
    std::vector<float> sj(numCuts);
    for (int j=0;j<numCuts;j++){ float s=(float)(j+1+factor)/(float)(numCuts+1);
        if (s<0.0f) s=0.0f; if (s>1.0f) s=1.0f; sj[j]=s; }

    std::vector<MeshFace> nf3d;
    std::vector<CornerSrc> src; // fuente de capa por corner (paralelo a los corners de nf3d)
    std::vector<int> cutVerts;  // los verts nuevos del corte (para dejarlos SELECCIONADOS)
    std::vector<char> esLoop(faces3d.size(), 0);
    for (int i=0;i<L;i++){ int fs = e->faceSrc[loopFaces[i]]; if (fs>=0 && fs<(int)faces3d.size()) esLoop[fs]=1; }
    for (size_t f=0; f<faces3d.size(); f++) if (!esLoop[f]) { // caras intactas: copiar tal cual + sus capas
        nf3d.push_back(faces3d[f]);
        for (size_t c=0;c<faces3d[f].idx.size();c++) src.push_back(CornerSrc(faceOff[f]+(int)c)); }

    for (int i=0;i<L;i++){
        int editF = loopFaces[i];
        int fs = e->faceSrc[editF];
        if (fs<0 || fs>=(int)faces3d.size()) continue;
        const std::vector<int>& ring = faces3d[fs].idx;
        if (ring.size() != 4) { nf3d.push_back(faces3d[fs]); for (size_t c=0;c<ring.size();c++) src.push_back(CornerSrc(faceOff[fs]+(int)c)); continue; }
        int ri = i, ro = cerrado ? ((i+1)%L) : (i+1); // rung de entrada / salida de esta cara
        int evA0=rungA[ri], evB0=rungB[ri], evA1=rungA[ro], evB1=rungB[ro];
        // mapear edit vert -> GPU vert + su POSICION en el ring (para la capa del corner)
        int gA0=-1,gB0=-1,gA1=-1,gB1=-1, cA0=-1,cB0=-1,cA1=-1,cB1=-1;
        for (size_t c=0;c<ring.size();c++){ int gi=ring[c]; int rep=hayRep?posRep[gi]:gi;
            if (rep==e->editVerts[evA0]){ gA0=gi; cA0=(int)c; } if (rep==e->editVerts[evB0]){ gB0=gi; cB0=(int)c; }
            if (rep==e->editVerts[evA1]){ gA1=gi; cA1=(int)c; } if (rep==e->editVerts[evB1]){ gB1=gi; cB1=(int)c; } }
        if (gA0<0||gB0<0||gA1<0||gB1<0){ nf3d.push_back(faces3d[fs]); for (size_t c=0;c<ring.size();c++) src.push_back(CornerSrc(faceOff[fs]+(int)c)); continue; }
        int crA0=faceOff[fs]+cA0, crB0=faceOff[fs]+cB0, crA1=faceOff[fs]+cA1, crB1=faceOff[fs]+cB1; // corners viejos
        // crear verts de corte (SOLO posiciones; uv/color salen de las capas via src)
        std::vector<int> cvIn(numCuts), cvOut(numCuts);
        for (int j=0;j<numCuts;j++){
            float s=sj[j];
            int ni=(int)(vp.size()/3);
            for (int q=0;q<3;q++) vp.push_back(vertex[gA0*3+q]*(1-s)+vertex[gB0*3+q]*s);
            cvIn[j]=ni; cutVerts.push_back(ni);
            int no=(int)(vp.size()/3);
            for (int q=0;q<3;q++) vp.push_back(vertex[gA1*3+q]*(1-s)+vertex[gB1*3+q]*s);
            cvOut[j]=no; cutVerts.push_back(no);
        }
        // normal de la cara ORIGINAL (Newell) -> para que los sub-quads queden con el
        // MISMO sentido (sino, segun la orientacion del rung, algunas caras se invertian)
        float onx=0,ony=0,onz=0;
        for (size_t c=0;c<ring.size();c++){ const float* a=&vp[ring[c]*3]; const float* b=&vp[ring[(c+1)%ring.size()]*3];
            onx+=(a[1]-b[1])*(a[2]+b[2]); ony+=(a[2]-b[2])*(a[0]+b[0]); onz+=(a[0]-b[0])*(a[1]+b[1]); }
        // sub-quads: [gA0, cvIn0, cvOut0, gA1] ... [cvInLast, gB0, gB1, cvOutLast]
        for (int j=0;j<=numCuts;j++){
            int a = (j==0)        ? gA0 : cvIn[j-1];
            int b = (j==numCuts)  ? gB0 : cvIn[j];
            int c = (j==numCuts)  ? gB1 : cvOut[j];
            int d = (j==0)        ? gA1 : cvOut[j-1];
            int qi[4]={a,b,c,d};
            // fuente de capa de cada corner del sub-quad (paralela a qi): los originales
            // COPIAN su corner viejo; los del corte LERPean entre los 2 corners de su arista
            CornerSrc qs[4];
            qs[0] = (j==0)       ? CornerSrc(crA0) : CornerSrc(crA0,crB0,sj[j-1]);
            qs[1] = (j==numCuts) ? CornerSrc(crB0) : CornerSrc(crA0,crB0,sj[j]);
            qs[2] = (j==numCuts) ? CornerSrc(crB1) : CornerSrc(crA1,crB1,sj[j]);
            qs[3] = (j==0)       ? CornerSrc(crA1) : CornerSrc(crA1,crB1,sj[j-1]);
            // normal del sub-quad; si va al reves de la cara original, lo doy vuelta (+ la capa)
            float qnx=0,qny=0,qnz=0;
            for (int c2=0;c2<4;c2++){ const float* pa=&vp[qi[c2]*3]; const float* pb=&vp[qi[(c2+1)%4]*3];
                qnx+=(pa[1]-pb[1])*(pa[2]+pb[2]); qny+=(pa[2]-pb[2])*(pa[0]+pb[0]); qnz+=(pa[0]-pb[0])*(pa[1]+pb[1]); }
            MeshFace q;
            if (qnx*onx+qny*ony+qnz*onz < 0.0f){ q.idx.push_back(d); q.idx.push_back(c); q.idx.push_back(b); q.idx.push_back(a);
                src.push_back(qs[3]); src.push_back(qs[2]); src.push_back(qs[1]); src.push_back(qs[0]); }
            else                               { q.idx.push_back(a); q.idx.push_back(b); q.idx.push_back(c); q.idx.push_back(d);
                src.push_back(qs[0]); src.push_back(qs[1]); src.push_back(qs[2]); src.push_back(qs[3]); }
            nf3d.push_back(q);
        }
    }

    // volcar SOLO posiciones (uv/color/normal los deriva GenerarRender de las capas). normals[]
    // queda viejo un instante; GenerarRender lo rehace desde cornerNormal -> no hay que tocarlo.
    int nuevoN=(int)(vp.size()/3);
    delete[] vertex; vertex=new GLfloat[nuevoN*3]; for (int i=0;i<nuevoN*3;i++) vertex[i]=vp[i];
    vertexSize=nuevoN;
    faces3d.swap(nf3d);
    ReconstruirCapasDesde(this,src); // capas nuevas (copiar / lerp por corner, incluida la normal)
    // restaurar la seleccion del CORTE por POSICION (GenerarRender re-numera los verts)
    std::vector<Vector3> posSel; posSel.reserve(cutVerts.size());
    for (size_t i=0;i<cutVerts.size();i++) if (cutVerts[i]>=0 && cutVerts[i]<vertexSize)
        posSel.push_back(Vector3(vertex[cutVerts[i]*3], vertex[cutVerts[i]*3+1], vertex[cutVerts[i]*3+2]));
    GenerarRender(); // re-merge + render (re-triangula + materialsGroup + CalcularBordes + loose)
    ReconstruirEditSelPorPos(posSel);
    return true;
}

// RECALCULATE NORMALS (menu Face): re-orienta el WINDING de las caras para que las
// normales miren coherentemente para AFUERA, usando como referencia el centro
// geometrico de la malla (heuristica: la normal de una cara deberia apuntar desde el
// centro hacia la cara). 'inside'=true las deja para ADENTRO (la tilde del panel redo).
// Opera sobre las caras SELECCIONADAS; si no hay ninguna, sobre TODAS. Arregla las
// caras invisibles (normal al reves) sin tocar la geometria (solo el orden de los verts).
bool Mesh::RecalcularOrientacionEdit(bool inside) {
    EnsureEdit();
    if (!edit || !vertex || vertexSize <= 0 || faces3d.empty()) return false;
    UndoCapturarMallaGeo(this); // Ctrl+Z: snapshot pre-recalcular-normales (winding de faces3d + normales)
    EditMesh* e = edit;
    const int nV = vertexSize;
    const bool hayRep = ((int)posRep.size() == nV);
    #define RREP(gi) (hayRep ? posRep[gi] : (gi))

    // verts seleccionados (segun el modo) -> elegir caras objetivo + preservar la seleccion
    std::vector<char> selRep(nV, 0);
    if (EditSelectMode == SelVertex) {
        for (size_t k=0;k<e->editVerts.size();k++) if (k<e->vertSel.size()&&e->vertSel[k]&&e->editVerts[k]>=0&&e->editVerts[k]<nV) selRep[e->editVerts[k]]=1;
    } else if (EditSelectMode == SelEdge) {
        for (size_t eg=0;eg<e->edgeSel.size();eg++) if (e->edgeSel[eg]){ selRep[e->editVerts[e->lineIdx[eg*2]]]=1; selRep[e->editVerts[e->lineIdx[eg*2+1]]]=1; }
    } else {
        for (size_t fe=0;fe<e->faces.size();fe++) if (fe<e->faceSel.size()&&e->faceSel[fe]){ const std::vector<int>& p=e->faces[fe]; for (size_t c=0;c<p.size();c++) selRep[e->editVerts[p[c]]]=1; }
    }

    // caras objetivo = las que tienen TODAS sus puntas seleccionadas; si no hay, TODAS
    std::vector<char> objetivo(faces3d.size(), 0); int nObj=0;
    for (size_t f=0;f<faces3d.size();f++){ const std::vector<int>& idx=faces3d[f].idx; if (idx.size()<3) continue;
        bool todos=true; for (size_t c=0;c<idx.size();c++){ int gi=idx[c]; if (gi<0||gi>=nV||!selRep[RREP(gi)]){todos=false;break;} }
        if (todos){ objetivo[f]=1; nObj++; } }
    if (nObj==0) { for (size_t f=0;f<faces3d.size();f++) if (faces3d[f].idx.size()>=3){ objetivo[f]=1; nObj++; } }
    if (nObj==0) return false;

    PoblarCapas(); // asegura las capas (la activa = el render actual) para reversearlas bien
    Vector3 mc = centroGeom; // centro de la malla (CalcularBordes lo deja al dia)
    int Loff = 0;            // offset de corner de la cara actual (sobre TODAS las caras)
    for (size_t f=0;f<faces3d.size();f++){
        std::vector<int>& idx=faces3d[f].idx; int mc2=(int)idx.size();
        if (objetivo[f] && mc2>=3){
            float nx=0,ny=0,nz=0; Vector3 fc(0,0,0); // normal Newell + centro de la cara
            for (int k=0;k<mc2;k++){ GLfloat* a=&vertex[idx[k]*3]; GLfloat* b=&vertex[idx[(k+1)%mc2]*3];
                nx+=(a[1]-b[1])*(a[2]+b[2]); ny+=(a[2]-b[2])*(a[0]+b[0]); nz+=(a[0]-b[0])*(a[1]+b[1]);
                fc.x+=a[0]; fc.y+=a[1]; fc.z+=a[2]; }
            fc=fc*(1.0f/(float)mc2);
            Vector3 dir(fc.x-mc.x, fc.y-mc.y, fc.z-mc.z); // afuera = del centro hacia la cara
            float dl=sqrtf(dir.x*dir.x+dir.y*dir.y+dir.z*dir.z);
            if (dl >= 1e-5f){
                if (inside){ dir.x=-dir.x; dir.y=-dir.y; dir.z=-dir.z; }
                if (nx*dir.x + ny*dir.y + nz*dir.z < 0.0f){ // la normal va al reves -> dar vuelta
                    for (int a=0,b=mc2-1;a<b;a++,b--){ int t=idx[a]; idx[a]=idx[b]; idx[b]=t; }
                    ReverseCapasDeCorner(this,Loff, mc2); // las capas siguen el flip del winding
                }
            }
        }
        Loff += mc2;
    }
    #undef RREP

    std::vector<Vector3> posSel = CapturarPosSel(selRep); // restaurar por POSICION (re-merge)
    RecalcularNormales(); // normales del nuevo winding (posiciones sin cambiar -> posRep OK)
    GenerarRender();      // re-merge + render (re-triangula + materialsGroup + CalcularBordes)
    ReconstruirEditSelPorPos(posSel);
    return true;
}

// FLIP NORMALS (menu Mesh > Normals > Flip): invierte el winding de las caras objetivo (seleccionadas, o
// TODAS si no hay seleccion) SIN condicion -> simplemente da vuelta las normales. Mas simple que Recalculate:
// si miran para un lado, quedan para el otro. No cambia la geometria (solo el orden de los verts + sus capas).
bool Mesh::FlipNormalesEdit() {
    EnsureEdit();
    if (!edit || !vertex || vertexSize <= 0 || faces3d.empty()) return false;
    UndoCapturarMallaGeo(this); // Ctrl+Z
    EditMesh* e = edit;
    const int nV = vertexSize;
    const bool hayRep = ((int)posRep.size() == nV);
    #define FREP(gi) (hayRep ? posRep[gi] : (gi))
    std::vector<char> selRep(nV, 0);
    if (EditSelectMode == SelVertex) {
        for (size_t k=0;k<e->editVerts.size();k++) if (k<e->vertSel.size()&&e->vertSel[k]&&e->editVerts[k]>=0&&e->editVerts[k]<nV) selRep[e->editVerts[k]]=1;
    } else if (EditSelectMode == SelEdge) {
        for (size_t eg=0;eg<e->edgeSel.size();eg++) if (e->edgeSel[eg]){ selRep[e->editVerts[e->lineIdx[eg*2]]]=1; selRep[e->editVerts[e->lineIdx[eg*2+1]]]=1; }
    } else {
        for (size_t fe=0;fe<e->faces.size();fe++) if (fe<e->faceSel.size()&&e->faceSel[fe]){ const std::vector<int>& p=e->faces[fe]; for (size_t c=0;c<p.size();c++) selRep[e->editVerts[p[c]]]=1; }
    }
    std::vector<char> objetivo(faces3d.size(), 0); int nObj=0;
    for (size_t f=0;f<faces3d.size();f++){ const std::vector<int>& idx=faces3d[f].idx; if (idx.size()<3) continue;
        bool todos=true; for (size_t c=0;c<idx.size();c++){ int gi=idx[c]; if (gi<0||gi>=nV||!selRep[FREP(gi)]){todos=false;break;} }
        if (todos){ objetivo[f]=1; nObj++; } }
    if (nObj==0) { for (size_t f=0;f<faces3d.size();f++) if (faces3d[f].idx.size()>=3){ objetivo[f]=1; nObj++; } } // sin sel: TODAS
    if (nObj==0) return false;
    PoblarCapas();
    int Loff=0;
    for (size_t f=0;f<faces3d.size();f++){
        std::vector<int>& idx=faces3d[f].idx; int mc2=(int)idx.size();
        if (objetivo[f] && mc2>=3){
            for (int a=0,b=mc2-1;a<b;a++,b--){ int t=idx[a]; idx[a]=idx[b]; idx[b]=t; } // revertir winding
            ReverseCapasDeCorner(this,Loff,mc2); // las capas siguen el flip
        }
        Loff += mc2;
    }
    #undef FREP
    std::vector<Vector3> posSel = CapturarPosSel(selRep);
    RecalcularNormales();
    GenerarRender();
    ReconstruirEditSelPorPos(posSel);
    return true;
}

// SHADE SMOOTH / FLAT sobre las caras SELECCIONADAS (menu Face). Ahora es POR CARA (flag faces3d[f].smooth):
// solo afecta a la seleccion, NO a toda la malla. GenerarRender recalcula las normales respetando el flag por cara
// (CornerNormalConSharp: una cara flat corta el grupo de suavizado en sus bordes -> flat; una smooth se funde con
// sus vecinas smooth). El merge de GenerarRender splittea/une los verts segun la normal resultante. Sobrevive los
// cambios de topologia (el flag viaja en faces3d). NO toca el meshSmooth GLOBAL (ese es solo el default -1).
bool Mesh::ShadeEdit(bool smooth) {
    EnsureEdit();
    if (!edit || !vertex || vertexSize <= 0) return false;
    EditMesh* e = edit;
    const int nV = vertexSize;
    const bool hayRep = ((int)posRep.size() == nV);

    std::vector<char> selRep(nV, 0);
    if (EditSelectMode == SelVertex) {
        for (size_t k=0;k<e->editVerts.size();k++) if (k<e->vertSel.size()&&e->vertSel[k]&&e->editVerts[k]>=0&&e->editVerts[k]<nV) selRep[e->editVerts[k]]=1;
    } else if (EditSelectMode == SelEdge) {
        for (size_t eg=0;eg<e->edgeSel.size();eg++) if (e->edgeSel[eg]){ selRep[e->editVerts[e->lineIdx[eg*2]]]=1; selRep[e->editVerts[e->lineIdx[eg*2+1]]]=1; }
    } else {
        for (size_t fe=0;fe<e->faces.size();fe++) if (fe<e->faceSel.size()&&e->faceSel[fe]){ const std::vector<int>& p=e->faces[fe]; for (size_t c=0;c<p.size();c++) selRep[e->editVerts[p[c]]]=1; }
    }
    #define SREP(gi)  (hayRep ? posRep[gi] : (gi))
    #define SSELG(gi) (selRep[ hayRep ? posRep[gi] : (gi) ])

    // caras COMPLETAMENTE seleccionadas
    std::vector<char> esSel(nV, 0);
    for (int gi=0; gi<nV; gi++) esSel[gi] = SSELG(gi) ? 1 : 0;
    std::vector<char> selFace(faces3d.size(), 0);
    int nSel = 0;
    for (size_t f=0;f<faces3d.size();f++){ const std::vector<int>& idx=faces3d[f].idx; int m=(int)idx.size(); if (m<3) continue;
        bool todos=true; for (int c=0;c<m;c++){ int gi=idx[c]; if (gi<0||gi>=nV||!SSELG(gi)){todos=false;break;} }
        if (todos){ selFace[f]=1; nSel++; }
    }
    #undef SREP
    #undef SSELG
    if (nSel == 0) return false;

    UndoCapturarMallaGeo(this); // Ctrl+Z: snapshot ANTES de tocar los flags (el swap revierte faces3d entero)
    for (size_t f=0;f<faces3d.size();f++) if (selFace[f]) faces3d[f].smooth = smooth ? 1 : 0; // shading POR CARA
    std::vector<Vector3> posSel = CapturarPosSel(esSel);
    GenerarRender();                  // recalcula normales por-cara (ConSharp) + re-mergea/splittea segun el flag
    ReconstruirEditSelPorPos(posSel);
    return true;
}

// TRIANGULATE FACES (Ctrl+T, menu Face): parte en triangulos las caras SELECCIONADAS de mas de 3 lados (abanico
// desde el 1er vertice: quad -> 2 tris, ngon -> n-2 tris). NO crea verts (usa los de la cara). Preserva UV/color/
// normal por corner (ReconstruirCapasDesde copiando del corner original). Deja la misma seleccion.
bool Mesh::TriangularSeleccionEdit() {
    EnsureEdit();
    if (!edit || !vertex || vertexSize <= 0) return false;
    EditMesh* e = edit;
    const int nV = vertexSize;
    const bool hayRep = ((int)posRep.size() == nV);

    // verts seleccionados (por REP de posicion), segun el modo de sub-elemento
    std::vector<char> selRep(nV, 0);
    if (EditSelectMode == SelVertex) {
        for (size_t k=0;k<e->editVerts.size();k++) if (k<e->vertSel.size()&&e->vertSel[k]&&e->editVerts[k]>=0&&e->editVerts[k]<nV) selRep[e->editVerts[k]]=1;
    } else if (EditSelectMode == SelEdge) {
        for (size_t eg=0;eg<e->edgeSel.size();eg++) if (e->edgeSel[eg]){ selRep[e->editVerts[e->lineIdx[eg*2]]]=1; selRep[e->editVerts[e->lineIdx[eg*2+1]]]=1; }
    } else {
        for (size_t fe=0;fe<e->faces.size();fe++) if (fe<e->faceSel.size()&&e->faceSel[fe]){ const std::vector<int>& p=e->faces[fe]; for (size_t c=0;c<p.size();c++) selRep[e->editVerts[p[c]]]=1; }
    }
    #define GSELG(gi) (selRep[ hayRep ? posRep[gi] : (gi) ])

    // caras COMPLETAMENTE seleccionadas y con > 3 lados (las unicas que hay que triangular)
    std::vector<char> selFace(faces3d.size(), 0);
    int nTri = 0;
    for (size_t f=0;f<faces3d.size();f++){ const std::vector<int>& idx=faces3d[f].idx; int m=(int)idx.size(); if (m<=3) continue;
        bool todos=true; for (int c=0;c<m;c++){ int gi=idx[c]; if (gi<0||gi>=nV||!GSELG(gi)){todos=false;break;} }
        if (todos){ selFace[f]=1; nTri++; }
    }
    if (nTri == 0) return false;

    UndoCapturarMallaGeo(this); // Ctrl+Z: snapshot completo de la geometria pre-triangulate
    PoblarCapas();              // asegura las capas por corner (para ReconstruirCapasDesde)

    // guardar que verts estan seleccionados (por GPU) para restaurar la seleccion tras GenerarRender
    std::vector<char> esSel(nV, 0);
    for (int gi=0; gi<nV; gi++) esSel[gi] = GSELG(gi) ? 1 : 0;
    #undef GSELG

    // nueva faces3d (abanico donde selFace) + fuente de capa por corner nuevo (copia del corner original)
    std::vector<MeshFace> nf; nf.reserve(faces3d.size()+nTri*2);
    std::vector<CornerSrc> src;
    int base = 0; // indice del 1er corner de la cara actual en las capas viejas
    for (size_t f=0;f<faces3d.size();f++){
        const std::vector<int>& idx = faces3d[f].idx; int m=(int)idx.size();
        if (!selFace[f] || m<=3) { // cara intacta: se copia igual (cada corner copia el suyo)
            nf.push_back(faces3d[f]);
            for (int c=0;c<m;c++) src.push_back(CornerSrc(base+c));
        } else { // abanico (0, i, i+1) para i=1..m-2
            for (int i=1;i<=m-2;i++){
                MeshFace t; t.mat=faces3d[f].mat; t.smooth=faces3d[f].smooth; // los tris del abanico heredan el shading
                t.idx.push_back(idx[0]); t.idx.push_back(idx[i]); t.idx.push_back(idx[i+1]);
                nf.push_back(t);
                src.push_back(CornerSrc(base+0)); src.push_back(CornerSrc(base+i)); src.push_back(CornerSrc(base+i+1));
            }
        }
        base += m;
    }
    ReconstruirCapasDesde(this,src); // rehace TODAS las capas al layout nuevo (lee las viejas por corner) ANTES del swap
    faces3d.swap(nf);

    std::vector<Vector3> posSel = CapturarPosSel(esSel);
    GenerarRender();                    // rebuild (re-mergea + recalcula normales); triangular no movio verts
    ReconstruirEditSelPorPos(posSel);   // vuelve a seleccionar los mismos verts
    return true;
}

// (definidas mas abajo, junto a MirrorPlano; forward-decl para usarlas aca)
static bool InvAffineMod(const Matrix4& in, Matrix4& out);
static Vector3 NormMod(Vector3 v);

// verts que YA se pegaron al plano en ESTE transform (clave = editK<<3 | (mi<<2) | eje). Una vez pegado, queda
// pegado a esa pared por el resto del arrastre: aunque la muevas para el lado libre, solo desliza por el plano.
// Se limpia en cada arranque de transform (EditXformIniciar -> ClipMirrorReset).
static std::set<long long> gClipStuck;
void ClipMirrorReset(){ gClipStuck.clear(); }

// CLIPPING del modificador Mirror (edit-time): impide que los verts CRUCEN el plano del mirror mientras se mueven
// (half-space, estilo Blender) y, una vez que un vert se PEGA al plano, lo deja pegado a esa pared por el resto del
// transform (solo desliza por el plano; el eje del espejo queda clavado en 0). Para cada vert mira el lado ACTUAL y
// el lado al EMPEZAR (startLocal): se pega si entra a la banda, si cruza el plano, o si YA se habia pegado. Local space.
void Mesh::ClipMirrorVerts(const std::vector<int>& editKs, const std::vector<Vector3>& startLocal) {
    if (!edit || modificadores.empty()) return;
    for (size_t mi=0; mi<modificadores.size(); mi++){
        Modifier* mod = modificadores[mi];
        if (mod->tipo != ModifierType::Mirror || !mod->clipping) continue;
        Vector3 c(0,0,0), ax0(1,0,0), ax1(0,1,0), ax2(0,0,1);
        if (mod->target){ // plano del target llevado al espacio LOCAL del objeto
            Matrix4 Wo, Wt; GetWorldMatrix(Wo); mod->target->GetWorldMatrix(Wt);
            Matrix4 iWo; InvAffineMod(Wo, iWo); Matrix4 M = iWo * Wt;
            c   = Vector3(M.m[12],M.m[13],M.m[14]);
            ax0 = NormMod(Vector3(M.m[0],M.m[1],M.m[2]));
            ax1 = NormMod(Vector3(M.m[4],M.m[5],M.m[6]));
            ax2 = NormMod(Vector3(M.m[8],M.m[9],M.m[10]));
        }
        Vector3 ax[3] = { ax0, ax1, ax2 }; bool en[3] = { mod->ejeX, mod->ejeY, mod->ejeZ };
        float md = mod->mergeDist;
        for (size_t j=0;j<editKs.size();j++){ int k=editKs[j];
            if (k<0 || k*3+2 >= (int)edit->pos.size()) continue;
            if (j >= startLocal.size()) continue;
            float px=edit->pos[k*3], py=edit->pos[k*3+1], pz=edit->pos[k*3+2];
            const Vector3& s0 = startLocal[j];
            for (int a=0;a<3;a++){ if(!en[a]) continue; const Vector3& n=ax[a];
                float d  = (px-c.x)*n.x + (py-c.y)*n.y + (pz-c.z)*n.z;      // lado ACTUAL
                float d0 = (s0.x-c.x)*n.x + (s0.y-c.y)*n.y + (s0.z-c.z)*n.z; // lado al EMPEZAR
                long long key = ((long long)k << 8) | ((long long)mi << 2) | a; // pegado por-vert(>>8)-por-modif(2..7)-por-eje(0..1)
                bool yaPegado    = gClipStuck.count(key) != 0;                   // ya se habia pegado antes en este arrastre
                bool dentroBanda = (d > -md && d < md);                          // esta dentro del margen -> snap a la costura
                bool cruza = (d0 >= 0.0f && d < 0.0f) || (d0 < 0.0f && d > 0.0f); // cruzo el plano (saltandose la banda)
                if (yaPegado || dentroBanda || cruza){
                    px-=d*n.x; py-=d*n.y; pz-=d*n.z;   // clampea AL plano (d=0)
                    gClipStuck.insert(key);            // y queda pegado a esta pared por el resto del transform
                }
            }
            edit->pos[k*3]=px; edit->pos[k*3+1]=py; edit->pos[k*3+2]=pz;
        }
    }
}

// hook: RenderObject (core) dibuja el overlay de edit por aca, sin derefenciar edit
void Mesh::RenderEditOverlay() {
    EnsureEdit(); if (edit) edit->Render();
}

// JOIN (Ctrl+J): anexa la geometria de 'otra' a ESTA malla. Cada vertice de 'otra' se transforma por M (que
// lleva el espacio LOCAL de 'otra' al LOCAL de esta -> el objeto activo conserva su transform y los otros quedan
// visualmente donde estaban). Concatena los buffers de render (verts transformados, uv, color, normales crudas),
// remapea faces3d (+base a los indices, +offset al mesh part) y trae los materiales de 'otra' como mesh parts
// nuevos. NO rebuildea: el caller hace LiberarCapas + PoblarCapas + GenerarRender al final (recompone normales
// limpias de la geo mergeada y preserva UV/color desde las capas rearmadas del render).
void Mesh::AnexarMallaTransformada(Mesh* otra, const Matrix4& M) {
    if (!otra || otra->vertexSize <= 0 || !otra->vertex) return;
    const int base = vertexSize;          // primer indice de los verts nuevos
    const int add  = otra->vertexSize;
    const int nN   = base + add;

    // --- posiciones (transformadas por M; operator* asume w=1 = punto) ---
    GLfloat* nv = new GLfloat[nN*3];
    if (vertex) for (int i=0;i<base*3;i++) nv[i]=vertex[i];
    for (int i=0;i<add;i++){
        Vector3 w = M * Vector3(otra->vertex[i*3], otra->vertex[i*3+1], otra->vertex[i*3+2]);
        nv[(base+i)*3]=w.x; nv[(base+i)*3+1]=w.y; nv[(base+i)*3+2]=w.z;
    }
    delete[] vertex; vertex=nv;

    // --- normales: las de ESTA quedan como estan; las de 'otra' se ROTAN por la MATRIZ NORMAL de M (transpuesta de
    //     la inversa de su parte lineal) para que sigan apuntando bien en el espacio de esta malla. El join las
    //     PRESERVA (el caller usa GenerarRender(false)): recalcularlas promediaria y REDONDEARIA los bordes filosos
    //     que el archivo trae como splits de normal. ---
    Matrix4 NM = M; NM.m[12]=NM.m[13]=NM.m[14]=0.0f; NM = NM.Inverse(); // n' = transpuesta(inv(lineal)) * n
    GLbyte* nn = new GLbyte[nN*3];
    for (int i=0;i<base*3;i++) nn[i] = normals ? normals[i] : (GLbyte)((i%3==1)?127:0);
    for (int i=0;i<add;i++){
        float nx = otra->normals ? otra->normals[i*3]/127.0f   : 0.0f;
        float ny = otra->normals ? otra->normals[i*3+1]/127.0f : 1.0f;
        float nz = otra->normals ? otra->normals[i*3+2]/127.0f : 0.0f;
        float tx = NM.m[0]*nx + NM.m[1]*ny + NM.m[2]*nz;   // fila r de la transpuesta = NM.m[r*4+c]
        float ty = NM.m[4]*nx + NM.m[5]*ny + NM.m[6]*nz;
        float tz = NM.m[8]*nx + NM.m[9]*ny + NM.m[10]*nz;
        float l = sqrtf(tx*tx+ty*ty+tz*tz); if (l>1e-8f){ tx/=l; ty/=l; tz/=l; } else { tx=0.0f; ty=1.0f; tz=0.0f; }
        nn[(base+i)*3]=(GLbyte)(tx*127.0f); nn[(base+i)*3+1]=(GLbyte)(ty*127.0f); nn[(base+i)*3+2]=(GLbyte)(tz*127.0f);
    }
    delete[] normals; normals=nn;

    // --- uv ---
    GLfloat* nu = new GLfloat[nN*2];
    for (int i=0;i<base*2;i++) nu[i] = uv ? uv[i] : 0.0f;
    for (int i=0;i<add*2;i++)  nu[base*2+i] = otra->uv ? otra->uv[i] : 0.0f;
    delete[] uv; uv=nu;

    // --- vertex color ---
    GLubyte* nc = new GLubyte[nN*4];
    for (int i=0;i<base*4;i++) nc[i] = vertexColor ? vertexColor[i] : (GLubyte)255;
    for (int i=0;i<add*4;i++)  nc[base*4+i] = otra->vertexColor ? otra->vertexColor[i] : (GLubyte)255;
    delete[] vertexColor; vertexColor=nc;

    vertexSize = nN;

    // --- faces3d (remapear indices +base; el mesh part de 'otra' pasa a ser uno nuevo aca) ---
    const int matBase = (int)materialsGroup.size();
    for (size_t f=0; f<otra->faces3d.size(); f++){
        MeshFace nf = otra->faces3d[f];
        for (size_t c=0;c<nf.idx.size();c++) nf.idx[c] += base;
        int m = nf.mat; if (m<0) m=0;
        nf.mat = matBase + m;
        faces3d.push_back(nf);
    }
    // --- mesh parts: los materiales de 'otra' se agregan como partes nuevas (comparten el Material*) ---
    for (size_t g=0; g<otra->materialsGroup.size(); g++) materialsGroup.push_back(otra->materialsGroup[g]);

    // --- bordes sueltos ---
    for (size_t i=0;i+1<otra->looseEdges.size();i+=2){
        looseEdges.push_back(otra->looseEdges[i]+base);
        looseEdges.push_back(otra->looseEdges[i+1]+base);
    }

    // --- SKINNING: mergear vertCtrlPoint (render->control-point) + vertex groups. Sin esto el join rompia las mallas
    // skinneadas (perdian los pesos -> colapsaban). Los control-points de 'otra' se DESPLAZAN despues de los de esta;
    // los vertex groups se mergean POR NOMBRE (mismo hueso = mismo grupo). GenerarRender (del caller) despues remapea
    // vertCtrlPoint al render dedupeado (ver el fix en GenerarRender). ---
    if (!vertCtrlPoint.empty() || !otra->vertCtrlPoint.empty()){
        int cpOffset = 0; for (size_t i=0;i<vertCtrlPoint.size();i++) if (vertCtrlPoint[i]+1 > cpOffset) cpOffset = vertCtrlPoint[i]+1; // #control-points de ESTA
        if ((int)vertCtrlPoint.size() < base) vertCtrlPoint.resize(base, -1); // esta malla sin skin -> sus verts van sin CP
        for (int i=0;i<add;i++){ int cp = (i < (int)otra->vertCtrlPoint.size()) ? otra->vertCtrlPoint[i] : -1;
            vertCtrlPoint.push_back(cp >= 0 ? cp + cpOffset : -1); }
        for (size_t g=0; g<otra->vertexGroups.size(); g++){ VertexGroup* bg = otra->vertexGroups[g]; if (!bg) continue;
            VertexGroup* ag = NULL; for (size_t k=0;k<vertexGroups.size();k++) if (vertexGroups[k] && vertexGroups[k]->nombre==bg->nombre){ ag=vertexGroups[k]; break; }
            if (!ag){ ag = new VertexGroup(bg->nombre); vertexGroups.push_back(ag); }
            for (size_t v=0; v<bg->verts.size() && v<bg->pesos.size(); v++){ ag->verts.push_back(bg->verts[v] + cpOffset); ag->pesos.push_back(bg->pesos[v]); }
        }
        if (!skinArmature && otra->skinArmature) skinArmature = otra->skinArmature; // heredar el rig si esta no estaba skinneada
    }
}

// APPLY (Alt+A): hornea la matriz B en la geometria. Transforma cada vertice (v -> B*v) y regenera el render
// (GenerarRender recompone las normales de la geo horneada + preserva UV/color desde las capas). El caller
// (AplicarTransform) usa B = inv(M_reset)*M_actual para que la malla quede VISUALMENTE en el mismo lugar tras
// resetear el location/rotation/scale del objeto. No cambia la topologia (B es biyectiva salvo escala 0).
void Mesh::AplicarMatriz(const Matrix4& B) {
    if (!vertex || vertexSize <= 0) return;
    for (int i=0;i<vertexSize;i++){
        Vector3 w = B * Vector3(vertex[i*3], vertex[i*3+1], vertex[i*3+2]);
        vertex[i*3]=w.x; vertex[i*3+1]=w.y; vertex[i*3+2]=w.z;
    }
    PoblarCapas();   // asegura las capas UV/color desde el render (por si no estaban) antes de regenerar
    GenerarRender(); // recompone normales de la geo horneada + preserva UV/color + CalcularBordes
}

// ============================================================================
//  STACK DE MODIFICADORES (del EDITOR). La clase Modifier vive en el editor (edit/Modifier.h); el core solo
//  guarda los punteros (Mesh::modificadores) y llama a LiberarModificadores() en su destructor. NADA de la
//  generacion/procesamiento vive aca todavia (Dante: "por ahora solo el stack y su orden").
// ============================================================================
void Mesh::AgregarModificador(int tipo) {
    modificadores.push_back(new Modifier(tipo, NombreTipoModificador(tipo)));
    modificadorActivo = (int)modificadores.size() - 1; // el nuevo queda seleccionado
}
void Mesh::QuitarModificadorActivo() {
    if (modificadorActivo < 0 || modificadorActivo >= (int)modificadores.size()) return;
    delete modificadores[modificadorActivo];
    modificadores.erase(modificadores.begin() + modificadorActivo);
    if (modificadorActivo >= (int)modificadores.size()) modificadorActivo = (int)modificadores.size() - 1; // -1 si quedo vacio
}
void Mesh::MoverModificador(int dir) { // -1 = sube (hacia el principio), +1 = baja. El orden importa.
    int i = modificadorActivo, j = i + dir;
    if (i < 0 || i >= (int)modificadores.size() || j < 0 || j >= (int)modificadores.size()) return;
    Modifier* tmp = modificadores[i]; modificadores[i] = modificadores[j]; modificadores[j] = tmp;
    modificadorActivo = j; // el activo sigue al modificador movido
}
void Mesh::LiberarModificadores() {
    for (size_t i = 0; i < modificadores.size(); i++) delete modificadores[i];
    modificadores.clear();
    modificadorActivo = -1;
}
std::string Mesh::NombreModificador(int i) const {
    if (i < 0 || i >= (int)modificadores.size()) return std::string();
    return modificadores[i]->nombre;
}

// ============================================================================
//  GENERACION de la malla de los modificadores (editor). Pipeline en vectores: arranca de la malla de RENDER
//  (editable expandida: vertex[]/normals/uv/color/faces[]) y aplica cada modificador en orden -> gen buffers.
//  La editable (vertex/faces3d) queda INTACTA (se edita esa). Por ahora solo el MIRROR. Clipping = pendiente.
// ============================================================================
void Mesh::LiberarMallaModificada() {
    delete[] genVertex; delete[] genNormals; delete[] genUV; delete[] genColor; delete[] genFaces;
    genVertex=NULL; genNormals=NULL; genUV=NULL; genColor=NULL; genFaces=NULL;
    genVertexSize=0; genFacesSize=0; genMaterialsGroup.clear(); genBordesBuf.clear(); genValido=false;
    delete[] genChromeExpPos; delete[] genChromeExpUV; genChromeExpPos=NULL; genChromeExpUV=NULL; genChromeCount=0; genChromeValid=false; // el reflejo gen se recalcula
}

// invierte una afin 4x4 columna-major (misma que Join, duplicada porque aquella es static en ObjectMode.cpp).
static bool InvAffineMod(const Matrix4& in, Matrix4& out){
    const float* m=in.m;
    float a=m[0],b=m[4],c=m[8], d=m[1],e=m[5],f=m[9], g=m[2],h=m[6],i=m[10];
    float det=a*(e*i-f*h)+b*(f*g-d*i)+c*(d*h-e*g);
    if (det>-1e-12f && det<1e-12f){ out=in; return false; }
    float id=1.0f/det;
    float i00=(e*i-f*h)*id,i01=(c*h-b*i)*id,i02=(b*f-c*e)*id;
    float i10=(f*g-d*i)*id,i11=(a*i-c*g)*id,i12=(c*d-a*f)*id;
    float i20=(d*h-e*g)*id,i21=(b*g-a*h)*id,i22=(a*e-b*d)*id;
    float tx=m[12],ty=m[13],tz=m[14];
    out.m[0]=i00;out.m[1]=i10;out.m[2]=i20;out.m[3]=0;
    out.m[4]=i01;out.m[5]=i11;out.m[6]=i21;out.m[7]=0;
    out.m[8]=i02;out.m[9]=i12;out.m[10]=i22;out.m[11]=0;
    out.m[12]=-(i00*tx+i01*ty+i02*tz); out.m[13]=-(i10*tx+i11*ty+i12*tz); out.m[14]=-(i20*tx+i21*ty+i22*tz); out.m[15]=1;
    return true;
}
static Vector3 NormMod(Vector3 v){ float l=sqrtf(v.x*v.x+v.y*v.y+v.z*v.z); if(l>1e-6f){v.x/=l;v.y/=l;v.z/=l;} return v; }

// true si la arista (ra,rb) (reps GPU de la malla) esta apoyada sobre ALGUN plano de un Mirror con clipping:
// AMBOS extremos a <= tolerancia del mismo plano. En ese caso la arista es una COSTURA del espejo, no un contorno
// real, y el extrude no debe hacerle pared (seria geometria interna que el mirror tapa). Todo en espacio LOCAL.
static bool AristaEnPlanoMirror(Mesh* m, int ra, int rb){
    if (!m || !m->vertex || ra<0 || rb<0 || ra>=m->vertexSize || rb>=m->vertexSize) return false;
    Vector3 A(m->vertex[ra*3], m->vertex[ra*3+1], m->vertex[ra*3+2]);
    Vector3 B(m->vertex[rb*3], m->vertex[rb*3+1], m->vertex[rb*3+2]);
    for (size_t mi=0; mi<m->modificadores.size(); mi++){
        Modifier* mod = m->modificadores[mi];
        if (mod->tipo != ModifierType::Mirror || !mod->clipping) continue;
        Vector3 c(0,0,0), ax0(1,0,0), ax1(0,1,0), ax2(0,0,1);
        if (mod->target){ // plano del target -> espacio LOCAL del objeto (igual que ClipMirrorVerts)
            Matrix4 Wo, Wt; m->GetWorldMatrix(Wo); mod->target->GetWorldMatrix(Wt);
            Matrix4 iWo; InvAffineMod(Wo, iWo); Matrix4 M = iWo * Wt;
            c   = Vector3(M.m[12],M.m[13],M.m[14]);
            ax0 = NormMod(Vector3(M.m[0],M.m[1],M.m[2]));
            ax1 = NormMod(Vector3(M.m[4],M.m[5],M.m[6]));
            ax2 = NormMod(Vector3(M.m[8],M.m[9],M.m[10]));
        }
        Vector3 ax[3] = { ax0, ax1, ax2 }; bool en[3] = { mod->ejeX, mod->ejeY, mod->ejeZ };
        float md = mod->mergeDist; if (md < 1e-3f) md = 1e-3f; // tolerancia (los verts de costura estan en d~0)
        for (int a=0;a<3;a++){ if(!en[a]) continue; const Vector3& n=ax[a];
            float da = (A.x-c.x)*n.x + (A.y-c.y)*n.y + (A.z-c.z)*n.z;
            float db = (B.x-c.x)*n.x + (B.y-c.y)*n.y + (B.z-c.z)*n.z;
            if (da>-md && da<md && db>-md && db<md) return true; // ambos sobre ESTE plano
        }
    }
    return false;
}

// espeja la geometria a traves del plano (c, n): duplica verts REFLEJADOS + tris con winding REVERTIDO (normal
// afuera). merge: los verts a <mergeDist del plano se SNAPEAN al plano + su normal se ALINEA (quita la componente
// en n) y NO se duplican (compartidos) -> la union se ve "soldada" (media esfera -> esfera). El merge es VISUAL.
// ============================================================================
//  Pipeline de modificadores por POLIGONOS (NO triangulos): Catmull-Clark necesita las caras + adyacencia y verts
//  TOPOLOGICOS (deduplicados por posicion). Se lleva la malla como poligonos con uv/color POR CORNER (preserva
//  costuras) y material POR CARA; se triangula al final. Mirror se aplica tambien sobre poligonos.
// ============================================================================
struct PolyMesh {
    std::vector<Vector3>                    P;    // posiciones topologicas (unicas por lugar)
    std::vector<std::vector<int> >          F;    // caras: indices a P
    std::vector<int>                        Fmat; // material (mesh part) por cara
    std::vector<std::vector<float> >        Fuv;  // uv POR CORNER (2 por corner)
    std::vector<std::vector<unsigned char> >Fcol; // color POR CORNER (4 por corner)
    std::vector<std::pair<int,int> >        E;    // aristas topologicas (perfil) -> las usa el Screw para barrer
    int                                     Emat; // material para las caras que genera el Screw desde las aristas
    PolyMesh() : Emat(0) {}
};

// arma la PolyMesh desde faces3d: deduplica los verts de render por POSICION (posRep) -> verts topologicos.
static void ConstruirPolyMesh(Mesh* m, PolyMesh& W) {
    const int nV = m->vertexSize;
    const bool hayRep = ((int)m->posRep.size() == nV);
    std::map<int,int> repToTopo; std::vector<int> gpuToTopo(nV, -1);
    for (int gi=0; gi<nV; gi++){ int r = hayRep ? m->posRep[gi] : gi; if (r<0||r>=nV) r=gi;
        std::map<int,int>::iterator it = repToTopo.find(r);
        if (it==repToTopo.end()){ int t=(int)W.P.size(); repToTopo[r]=t; gpuToTopo[gi]=t;
            W.P.push_back(Vector3(m->vertex[r*3], m->vertex[r*3+1], m->vertex[r*3+2])); }
        else gpuToTopo[gi]=it->second;
    }
    for (size_t f=0; f<m->faces3d.size(); f++){ const std::vector<int>& idx=m->faces3d[f].idx; if (idx.size()<3) continue;
        std::vector<int> face; std::vector<float> uv; std::vector<unsigned char> col;
        for (size_t c=0;c<idx.size();c++){ int gi=idx[c]; if (gi<0||gi>=nV){ face.clear(); break; }
            face.push_back(gpuToTopo[gi]);
            uv.push_back(m->uv?m->uv[gi*2]:0.0f); uv.push_back(m->uv?m->uv[gi*2+1]:0.0f);
            for (int q=0;q<4;q++) col.push_back(m->vertexColor?m->vertexColor[gi*4+q]:(unsigned char)255);
        }
        if (face.size()<3) continue;
        W.F.push_back(face); W.Fmat.push_back(m->faces3d[f].mat); W.Fuv.push_back(uv); W.Fcol.push_back(col);
    }
    // aristas topologicas (perfil del Screw): de las caras + los bordes SUELTOS (un perfil de botella suele ser
    // una cadena de aristas sueltas sin caras). Deduplicadas por vert topologico.
    std::set<std::pair<int,int> > eset;
    for (size_t f=0; f<W.F.size(); f++){ int mm=(int)W.F[f].size(); for (int i=0;i<mm;i++){ int a=W.F[f][i], b=W.F[f][(i+1)%mm];
        if (a!=b) eset.insert(a<b?std::make_pair(a,b):std::make_pair(b,a)); } }
    for (size_t le=0; le+1<m->looseEdges.size(); le+=2){ int a=m->looseEdges[le], b=m->looseEdges[le+1];
        if (a<0||a>=nV||b<0||b>=nV) continue; int ta=gpuToTopo[a], tb=gpuToTopo[b]; if (ta<0||tb<0||ta==tb) continue;
        eset.insert(ta<tb?std::make_pair(ta,tb):std::make_pair(tb,ta)); }
    for (std::set<std::pair<int,int> >::iterator it=eset.begin(); it!=eset.end(); ++it) W.E.push_back(*it);
    W.Emat = 0; // las caras que genera el Screw van al primer mesh part
}

// UN nivel de subdivision. simple=true -> Simple (subdivide sin mover verts); false -> Catmull-Clark (suaviza).
static void SubdividirUnNivel(PolyMesh& W, bool simple) {
    const int nP=(int)W.P.size(), nF=(int)W.F.size(); if (nP==0||nF==0) return;
    // 1) punto de CARA (centroide) + uv/color promedio
    std::vector<Vector3> FP(nF); std::vector<float> FPu(nF*2,0.0f), FPc(nF*4,0.0f);
    for (int f=0; f<nF; f++){ int m=(int)W.F[f].size(); Vector3 c(0,0,0); float u=0,v=0,cc[4]={0,0,0,0};
        for (int i=0;i<m;i++){ int vi=W.F[f][i]; c=c+W.P[vi]; u+=W.Fuv[f][i*2]; v+=W.Fuv[f][i*2+1];
            for(int q=0;q<4;q++) cc[q]+=W.Fcol[f][i*4+q]; }
        float im=1.0f/(float)m; FP[f]=c*im; FPu[f*2]=u*im; FPu[f*2+1]=v*im; for(int q=0;q<4;q++) FPc[f*4+q]=cc[q]*im; }
    // 2) aristas -> id + caras adyacentes
    std::map<std::pair<int,int>,int> eid; std::vector<std::pair<int,int> > ev; std::vector<std::vector<int> > eface;
    for (int f=0; f<nF; f++){ int m=(int)W.F[f].size(); for (int i=0;i<m;i++){ int a=W.F[f][i], b=W.F[f][(i+1)%m];
        std::pair<int,int> k(a<b?a:b, a<b?b:a); std::map<std::pair<int,int>,int>::iterator it=eid.find(k); int e;
        if (it==eid.end()){ e=(int)ev.size(); eid[k]=e; ev.push_back(k); eface.push_back(std::vector<int>()); } else e=it->second;
        eface[e].push_back(f); } }
    const int nE=(int)ev.size();
    // 3) punto de ARISTA
    std::vector<Vector3> EP(nE);
    for (int e=0;e<nE;e++){ int a=ev[e].first, b=ev[e].second;
        if (simple || eface[e].size()<2) EP[e]=(W.P[a]+W.P[b])*0.5f;
        else EP[e]=(W.P[a]+W.P[b]+FP[eface[e][0]]+FP[eface[e][1]])*0.25f; }
    // 4) punto de VERTICE (Catmull-Clark mueve; Simple lo deja)
    std::vector<Vector3> VP(W.P);
    if (!simple){
        std::vector<Vector3> Favg(nP,Vector3(0,0,0)); std::vector<int> Fcnt(nP,0);
        std::vector<Vector3> Ravg(nP,Vector3(0,0,0)); std::vector<int> Rcnt(nP,0);
        std::vector<bool> bord(nP,false); std::vector<Vector3> bAcc(nP,Vector3(0,0,0)); std::vector<int> bCnt(nP,0);
        for (int f=0; f<nF; f++){ int m=(int)W.F[f].size(); for (int i=0;i<m;i++){ int vi=W.F[f][i]; Favg[vi]=Favg[vi]+FP[f]; Fcnt[vi]++; } }
        for (int e=0;e<nE;e++){ int a=ev[e].first, b=ev[e].second; Vector3 mid=(W.P[a]+W.P[b])*0.5f;
            Ravg[a]=Ravg[a]+mid; Rcnt[a]++; Ravg[b]=Ravg[b]+mid; Rcnt[b]++;
            if (eface[e].size()<2){ bord[a]=bord[b]=true; bAcc[a]=bAcc[a]+W.P[b]; bCnt[a]++; bAcc[b]=bAcc[b]+W.P[a]; bCnt[b]++; } }
        for (int v=0; v<nP; v++){
            if (bord[v]){ if (bCnt[v]>0){ Vector3 nb=bAcc[v]*(1.0f/(float)bCnt[v]); VP[v]=W.P[v]*0.75f+nb*0.25f; } }
            else if (Fcnt[v]>0 && Rcnt[v]>0){ int nn=Fcnt[v]; if (nn<3) nn=3;
                Vector3 F_=Favg[v]*(1.0f/(float)Fcnt[v]); Vector3 R_=Ravg[v]*(1.0f/(float)Rcnt[v]);
                VP[v]=(F_ + R_*2.0f + W.P[v]*(float)(nn-3))*(1.0f/(float)nn); } }
    }
    // 5) nueva malla: verts VP(0..) + FP(baseF..) + EP(baseE..); por corner de cada cara -> un quad
    PolyMesh out; out.P.reserve(nP+nF+nE);
    for (int v=0;v<nP;v++) out.P.push_back(VP[v]);
    int baseF=(int)out.P.size(); for (int f=0;f<nF;f++) out.P.push_back(FP[f]);
    int baseE=(int)out.P.size(); for (int e=0;e<nE;e++) out.P.push_back(EP[e]);
    for (int f=0; f<nF; f++){ int m=(int)W.F[f].size();
        for (int i=0;i<m;i++){ int pi=(i+m-1)%m, ni=(i+1)%m; int vi=W.F[f][i], vprev=W.F[f][pi], vnext=W.F[f][ni];
            int eN=eid[std::make_pair(vi<vnext?vi:vnext, vi<vnext?vnext:vi)];
            int eP=eid[std::make_pair(vprev<vi?vprev:vi, vprev<vi?vi:vprev)];
            std::vector<int> q; q.push_back(vi); q.push_back(baseE+eN); q.push_back(baseF+f); q.push_back(baseE+eP);
            out.F.push_back(q); out.Fmat.push_back(W.Fmat[f]);
            std::vector<float> qu; std::vector<unsigned char> qc;
            // V (corner i)
            qu.push_back(W.Fuv[f][i*2]); qu.push_back(W.Fuv[f][i*2+1]); for(int z=0;z<4;z++) qc.push_back(W.Fcol[f][i*4+z]);
            // E(v,next) = promedio corner i y next
            qu.push_back((W.Fuv[f][i*2]+W.Fuv[f][ni*2])*0.5f); qu.push_back((W.Fuv[f][i*2+1]+W.Fuv[f][ni*2+1])*0.5f);
            for(int z=0;z<4;z++) qc.push_back((unsigned char)(((int)W.Fcol[f][i*4+z]+(int)W.Fcol[f][ni*4+z])/2));
            // F (cara)
            qu.push_back(FPu[f*2]); qu.push_back(FPu[f*2+1]); for(int z=0;z<4;z++) qc.push_back((unsigned char)FPc[f*4+z]);
            // E(prev,v) = promedio corner prev e i
            qu.push_back((W.Fuv[f][pi*2]+W.Fuv[f][i*2])*0.5f); qu.push_back((W.Fuv[f][pi*2+1]+W.Fuv[f][i*2+1])*0.5f);
            for(int z=0;z<4;z++) qc.push_back((unsigned char)(((int)W.Fcol[f][pi*4+z]+(int)W.Fcol[f][i*4+z])/2));
            out.Fuv.push_back(qu); out.Fcol.push_back(qc);
        }
    }
    W.P.swap(out.P); W.F.swap(out.F); W.Fmat.swap(out.Fmat); W.Fuv.swap(out.Fuv); W.Fcol.swap(out.Fcol);
}

// MIRROR sobre poligonos: refleja los verts a traves del plano (c,n) y agrega las caras reflejadas con winding
// REVERTIDO. merge: los verts a <mergeDist del plano se SNAPEAN y se comparten (la costura queda soldada; la
// normal continua sale sola al recalcular smooth por simetria).
static void MirrorPoly(PolyMesh& W, const Vector3& c, const Vector3& n, bool merge, float mergeDist) {
    int nP=(int)W.P.size(); std::vector<int> mir(nP);
    for (int i=0;i<nP;i++){ float d=(W.P[i].x-c.x)*n.x+(W.P[i].y-c.y)*n.y+(W.P[i].z-c.z)*n.z;
        if (merge && d>-mergeDist && d<mergeDist){ W.P[i]=W.P[i]-n*d; mir[i]=i; }
        else { mir[i]=(int)W.P.size(); W.P.push_back(W.P[i]-n*(2*d)); } }
    int nF=(int)W.F.size();
    for (int f=0; f<nF; f++){ int m=(int)W.F[f].size();
        std::vector<int> nf; std::vector<float> nu; std::vector<unsigned char> nc;
        for (int i=m-1;i>=0;i--){ nf.push_back(mir[W.F[f][i]]); nu.push_back(W.Fuv[f][i*2]); nu.push_back(W.Fuv[f][i*2+1]);
            for(int z=0;z<4;z++) nc.push_back(W.Fcol[f][i*4+z]); }
        W.F.push_back(nf); W.Fmat.push_back(W.Fmat[f]); W.Fuv.push_back(nu); W.Fcol.push_back(nc);
    }
}

// rota v alrededor de un eje del mundo (0=X,1=Y,2=Z) por 'ang' radianes.
static Vector3 RotarEje(const Vector3& v, int axis, float ang){
    float c=cosf(ang), s=sinf(ang);
    if (axis==0) return Vector3(v.x, v.y*c - v.z*s, v.y*s + v.z*c);      // eje X
    if (axis==1) return Vector3(v.z*s + v.x*c, v.y, v.z*c - v.x*s);      // eje Y
    return Vector3(v.x*c - v.y*s, v.x*s + v.y*c, v.z);                   // eje Z
}

// suelda verts coincidentes (misma posicion cuantizada a 1e-4) del PolyMesh: remapea las caras, tira corners
// consecutivos duplicados y caras degeneradas (<3). Lo usa el Screw (merge) para colapsar los polos (perfil que
// toca el eje) y la costura del torno 360 -> topologia limpia. Preserva UV/color por corner.
static void SoldarPolyPorPos(PolyMesh& W){
    const int nP=(int)W.P.size(); if(nP==0) return;
    std::map<std::string,int> mp; std::vector<int> remap(nP); std::vector<Vector3> np;
    for (int i=0;i<nP;i++){ int q[3]={ (int)floorf(W.P[i].x*10000.0f+0.5f),(int)floorf(W.P[i].y*10000.0f+0.5f),(int)floorf(W.P[i].z*10000.0f+0.5f) };
        std::string k((const char*)q,sizeof(q)); std::map<std::string,int>::iterator it=mp.find(k);
        if (it!=mp.end()) remap[i]=it->second; else { remap[i]=(int)np.size(); mp[k]=remap[i]; np.push_back(W.P[i]); } }
    std::vector<std::vector<int> > nf; std::vector<int> nfm; std::vector<std::vector<float> > nfu; std::vector<std::vector<unsigned char> > nfc;
    for (size_t f=0;f<W.F.size();f++){ int m=(int)W.F[f].size();
        std::vector<int> r; std::vector<float> ru; std::vector<unsigned char> rc;
        for (int c=0;c<m;c++){ int v=remap[W.F[f][c]]; if(!r.empty() && r.back()==v) continue; // corner colapsado
            r.push_back(v); ru.push_back(W.Fuv[f][c*2]); ru.push_back(W.Fuv[f][c*2+1]);
            rc.push_back(W.Fcol[f][c*4]);rc.push_back(W.Fcol[f][c*4+1]);rc.push_back(W.Fcol[f][c*4+2]);rc.push_back(W.Fcol[f][c*4+3]); }
        if (r.size()>=2 && r.front()==r.back()){ r.pop_back(); ru.pop_back();ru.pop_back(); for(int k=0;k<4;k++) rc.pop_back(); }
        if (r.size()<3) continue;
        nf.push_back(r); nfm.push_back(W.Fmat[f]); nfu.push_back(ru); nfc.push_back(rc); }
    W.P.swap(np); W.F.swap(nf); W.Fmat.swap(nfm); W.Fuv.swap(nfu); W.Fcol.swap(nfc);
}

// SCREW: barre el perfil (aristas W.E) alrededor del eje. Copia el perfil 'steps' veces girando 'angleDeg' del
// primero al ultimo y subiendo 'height' por el eje; conecta cada arista entre copias consecutivas -> quad. Si es
// vuelta completa (360) sin subida, cierra el anillo (torno). stretchU/V generan UV cilindrica (U=giro, V=perfil).
// flip = invierte el winding (normales al otro lado). merge = suelda verts coincidentes (polos + costura) al final.
static void ScrewPoly(PolyMesh& W, int axis, float angleDeg, float height, int steps, bool stretchU, bool stretchV, bool flip, bool merge) {
    if (steps < 2) steps = 2;
    const int nP=(int)W.P.size(); if (nP==0) return;
    if (axis<0||axis>2) axis=2;
    const float angleRad = angleDeg * 3.14159265358979f/180.0f;
    const bool closed = (fabsf(fabsf(angleDeg)-360.0f) < 0.5f) && (fabsf(height) < 1e-5f); // torno cerrado
    const int copies = steps;                       // copias del perfil
    const int seams  = closed ? steps : (steps-1);  // conexiones (cerrado: incluye la que vuelve al inicio)
    Vector3 ax(0,0,0); if(axis==0)ax.x=1; else if(axis==1)ax.y=1; else ax.z=1;
    // V por vert del perfil = posicion normalizada A LO LARGO del eje (para textura cilindrica)
    std::vector<float> vertV(nP, 0.0f);
    if (stretchV){ float mn=1e30f, mx=-1e30f;
        for (int p=0;p<nP;p++){ float a=(axis==0?W.P[p].x:axis==1?W.P[p].y:W.P[p].z); if(a<mn)mn=a; if(a>mx)mx=a; }
        float rng=(mx-mn); if (rng<1e-6f) rng=1.0f;
        // V INVERTIDO (1 arriba del eje, 0 abajo): asi la textura no queda dada vuelta en Y (convencion de imagen:
        // fila 0 = arriba). U (giro) queda bien; V escala bien, solo habia que voltearlo.
        for (int p=0;p<nP;p++){ float a=(axis==0?W.P[p].x:axis==1?W.P[p].y:W.P[p].z); vertV[p]=1.0f-(a-mn)/rng; } }
    PolyMesh out;
    // verts: copia s -> perfil rotado + subido
    for (int s=0; s<copies; s++){
        float t = closed ? ((float)s/(float)steps) : (steps>1 ? (float)s/(float)(steps-1) : 0.0f);
        float ang = angleRad*t, rise = height*t;
        for (int p=0;p<nP;p++) out.P.push_back(RotarEje(W.P[p], axis, ang) + ax*rise);
    }
    // superficie barrida: por cada arista, un quad entre copia s y s+1
    for (int s=0; s<seams; s++){ int s0=s, s1= closed ? ((s+1)%steps) : (s+1);
        float u0 = closed ? ((float)s/(float)steps) : (float)s/(float)(steps-1);
        float u1 = closed ? ((float)(s+1)/(float)steps) : (float)(s+1)/(float)(steps-1);
        if (!stretchU){ u0=0; u1=0; }
        for (size_t e=0;e<W.E.size();e++){ int a=W.E[e].first, b=W.E[e].second;
            float va=stretchV?vertV[a]:0.0f, vb=stretchV?vertV[b]:0.0f;
            // corners del quad + su UV, en un orden base; 'flip=false' lo invierte -> normal hacia AFUERA por
            // defecto (una botella/lathe cerrado se ve solida). flip=true = winding base (por si quedo al reves).
            int   qi[4] = { s0*nP+a, s0*nP+b, s1*nP+b, s1*nP+a };
            float qU[4] = { u0, u0, u1, u1 };
            float qV[4] = { va, vb, vb, va };
            std::vector<int> q; std::vector<float> qu;
            if (flip) { for (int i=0;i<4;i++){ q.push_back(qi[i]); qu.push_back(qU[i]); qu.push_back(qV[i]); } }
            else      { for (int i=3;i>=0;i--){ q.push_back(qi[i]); qu.push_back(qU[i]); qu.push_back(qV[i]); } }
            out.F.push_back(q); out.Fmat.push_back(W.Emat); out.Fuv.push_back(qu);
            std::vector<unsigned char> qc(16,255); out.Fcol.push_back(qc);
        }
    }
    // caras del perfil duplicadas en cada copia (si el perfil tiene caras)
    for (int s=0; s<copies; s++){ int base=s*nP;
        for (size_t f=0;f<W.F.size();f++){ std::vector<int> nf; for(size_t c=0;c<W.F[f].size();c++) nf.push_back(base+W.F[f][c]);
            out.F.push_back(nf); out.Fmat.push_back(W.Fmat[f]); out.Fuv.push_back(W.Fuv[f]); out.Fcol.push_back(W.Fcol[f]); } }
    W.P.swap(out.P); W.F.swap(out.F); W.Fmat.swap(out.Fmat); W.Fuv.swap(out.Fuv); W.Fcol.swap(out.Fcol); W.E.clear();
    if (merge) SoldarPolyPorPos(W); // suelda polos + costura del torno 360
}

// aplica el STACK de modificadores sobre poligonos. render=true usa los niveles/steps de RENDER (sino los de
// viewport). Devuelve false si nada corrio. outSmooth -> normales smooth. (free function: miembros publicos del Mesh)
static bool Mesh_AplicarStack(Mesh* m, bool render, PolyMesh& W, bool& outSmooth) {
    outSmooth = false;
    const bool enEdit = ((Object*)m == g_editMesh); // en Edit Mode se saltean los mods con mostrarEdit=false
    { bool alguno=false; for (size_t i=0;i<m->modificadores.size();i++){ Modifier* md=m->modificadores[i];
        if (md->tipo == ModifierType::Armature) continue; // el Armature NO genera malla (deform por-frame en el render)
        if (!md->mostrarViewport) continue; if (enEdit && !md->mostrarEdit) continue; alguno=true; break; }
      if (!alguno) return false; }
    ConstruirPolyMesh(m, W);
    if (W.F.empty() && W.E.empty()) return false; // sin caras ni aristas -> nada que modificar
    int aplicados = 0;
    for (size_t i=0; i<m->modificadores.size(); i++){ Modifier* mod = m->modificadores[i];
        if (mod->tipo == ModifierType::Armature) continue; // skinning: deform por-frame en el render, no gen
        if (!mod->mostrarViewport) continue;        // OFF -> NUNCA se calcula
        if (enEdit && !mod->mostrarEdit) continue;  // OFF -> se saltea SOLO en Edit Mode
        aplicados++;
        if (mod->tipo == ModifierType::SubdivisionSurface){
            int lvl = (int)((render ? mod->subRenderLevel : mod->subLevel) + 0.5f);
            if (lvl < 0) lvl = 0; if (lvl > 6) lvl = 6; // tope (cada nivel x4 caras)
            for (int L=0; L<lvl; L++) SubdividirUnNivel(W, mod->subSimple);
            if (!mod->subSimple && lvl>0) outSmooth = true;
        } else if (mod->tipo == ModifierType::Screw){
            int st = (int)((render ? mod->screwRenderSteps : mod->screwSteps) + 0.5f);
            if (st < 2) st = 2; if (st > 512) st = 512;
            ScrewPoly(W, mod->screwAxis, mod->screwAngle, mod->screwHeight, st, mod->screwStretchU, mod->screwStretchV, mod->screwFlip, mod->screwMerge);
            if (mod->screwSmooth) outSmooth = true; // normales suaves -> lathe redondito
        } else if (mod->tipo == ModifierType::Mirror){
            Vector3 c(0,0,0), axX(1,0,0), axY(0,1,0), axZ(0,0,1);
            if (mod->target){ Matrix4 Wo, Wt; m->GetWorldMatrix(Wo); mod->target->GetWorldMatrix(Wt);
                Matrix4 iWo; InvAffineMod(Wo, iWo); Matrix4 M = iWo * Wt;
                c   = Vector3(M.m[12],M.m[13],M.m[14]);
                axX = NormMod(Vector3(M.m[0],M.m[1],M.m[2])); axY = NormMod(Vector3(M.m[4],M.m[5],M.m[6])); axZ = NormMod(Vector3(M.m[8],M.m[9],M.m[10])); }
            if (mod->ejeX) MirrorPoly(W, c, axX, mod->merge, mod->mergeDist);
            if (mod->ejeY) MirrorPoly(W, c, axY, mod->merge, mod->mergeDist);
            if (mod->ejeZ) MirrorPoly(W, c, axZ, mod->merge, mod->mergeDist);
        }
    }
    return (aplicados>0 && !W.F.empty());
}

// del PolyMesh saca verts de RENDER deduplicados (pos+uv+normal+color) + los POLIGONOS (indices a esos verts, se
// PRESERVAN los quads) + material por cara. smooth -> normal promediada por vert; sino plana por cara.
static void PolyARenderVerts(const PolyMesh& W, bool smooth,
        std::vector<GLfloat>& gvp, std::vector<GLbyte>& gvn, std::vector<GLfloat>& gvu, std::vector<GLubyte>& gvc,
        std::vector<std::vector<int> >& poly, std::vector<int>& polyMat) {
    std::vector<Vector3> faceN(W.F.size());
    for (size_t f=0; f<W.F.size(); f++){ Vector3 nrm(0,0,0); int m=(int)W.F[f].size(); // Newell
        for (int i=0;i<m;i++){ const Vector3& a=W.P[W.F[f][i]]; const Vector3& b=W.P[W.F[f][(i+1)%m]];
            nrm.x+=(a.y-b.y)*(a.z+b.z); nrm.y+=(a.z-b.z)*(a.x+b.x); nrm.z+=(a.x-b.x)*(a.y+b.y); }
        float l=sqrtf(nrm.x*nrm.x+nrm.y*nrm.y+nrm.z*nrm.z); faceN[f] = (l>1e-6f) ? nrm*(1.0f/l) : Vector3(0,1,0); }
    std::vector<Vector3> vertN;
    if (smooth){ vertN.assign(W.P.size(), Vector3(0,0,0));
        for (size_t f=0; f<W.F.size(); f++) for (size_t c=0;c<W.F[f].size();c++) vertN[W.F[f][c]] = vertN[W.F[f][c]] + faceN[f];
        for (size_t v=0; v<vertN.size(); v++){ float l=sqrtf(vertN[v].x*vertN[v].x+vertN[v].y*vertN[v].y+vertN[v].z*vertN[v].z);
            vertN[v] = (l>1e-6f) ? vertN[v]*(1.0f/l) : Vector3(0,1,0); } }
    std::map<std::string,int> mp;
    for (size_t f=0; f<W.F.size(); f++){ int m=(int)W.F[f].size(); if (m<3) continue;
        std::vector<int> ci(m);
        for (int c=0;c<m;c++){ int v=W.F[f][c]; Vector3 nn = smooth ? vertN[v] : faceN[f];
            float px=W.P[v].x, py=W.P[v].y, pz=W.P[v].z, u0=W.Fuv[f][c*2], u1=W.Fuv[f][c*2+1];
            GLbyte nx=(GLbyte)(nn.x*127), ny=(GLbyte)(nn.y*127), nz=(GLbyte)(nn.z*127);
            GLubyte r=W.Fcol[f][c*4], g=W.Fcol[f][c*4+1], b=W.Fcol[f][c*4+2], a=W.Fcol[f][c*4+3];
            char buf[40]; int p=0;
            memcpy(buf+p,&px,4);p+=4; memcpy(buf+p,&py,4);p+=4; memcpy(buf+p,&pz,4);p+=4;
            memcpy(buf+p,&u0,4);p+=4; memcpy(buf+p,&u1,4);p+=4;
            buf[p++]=(char)nx;buf[p++]=(char)ny;buf[p++]=(char)nz; buf[p++]=(char)r;buf[p++]=(char)g;buf[p++]=(char)b;buf[p++]=(char)a;
            std::string key(buf,p); std::map<std::string,int>::iterator it=mp.find(key); int gi;
            if (it!=mp.end()) gi=it->second;
            else { gi=(int)(gvp.size()/3); gvp.push_back(px);gvp.push_back(py);gvp.push_back(pz);
                gvn.push_back(nx);gvn.push_back(ny);gvn.push_back(nz); gvu.push_back(u0);gvu.push_back(u1);
                gvc.push_back(r);gvc.push_back(g);gvc.push_back(b);gvc.push_back(a); mp[key]=gi; }
            ci[c]=gi;
        }
        poly.push_back(ci); polyMat.push_back(W.Fmat[f]);
    }
}

void Mesh::GenerarMallaModificada() {
    { extern long g_genMallaCount; g_genMallaCount++; } // DIAGNOSTICO (Statistics): contar regeneraciones. Al ROTAR no debe subir.
    LiberarMallaModificada();
    // NO exige caras: el Screw barre un PERFIL que puede ser solo aristas sueltas (una botella sin caras).
    if (modificadores.empty() || !vertex || vertexSize<=0) return;
    extern bool g_modRenderMode; // (OpcionesRender): true en el render final -> usa los niveles/steps de RENDER

    PolyMesh W; bool outSmooth=false;
    if (!Mesh_AplicarStack(this, g_modRenderMode, W, outSmooth)){ genValido=false; return; }
    const bool smooth = meshSmooth || outSmooth;

    std::vector<GLfloat> gvp; std::vector<GLbyte> gvn; std::vector<GLfloat> gvu; std::vector<GLubyte> gvc;
    std::vector<std::vector<int> > poly; std::vector<int> polyMat;
    PolyARenderVerts(W, smooth, gvp,gvn,gvu,gvc, poly, polyMat);
    // triangular los poligonos (fan) para el render GL
    std::vector<MeshIndex> tri; std::vector<int> triMat;
    for (size_t f=0; f<poly.size(); f++){ int m=(int)poly[f].size();
        for (int t=1; t+1<m; t++){ tri.push_back((MeshIndex)poly[f][0]); tri.push_back((MeshIndex)poly[f][t]); tri.push_back((MeshIndex)poly[f][t+1]); triMat.push_back(polyMat[f]); } }
    genVertexSize=(int)(gvp.size()/3);
    if (genVertexSize<=0 || tri.empty()){ genValido=false; return; }
    genVertex=new GLfloat[gvp.size()]; for(size_t k=0;k<gvp.size();k++) genVertex[k]=gvp[k];
    genNormals=new GLbyte[gvn.size()]; for(size_t k=0;k<gvn.size();k++) genNormals[k]=gvn[k];
    genUV=new GLfloat[gvu.size()];     for(size_t k=0;k<gvu.size();k++) genUV[k]=gvu[k];
    genColor=new GLubyte[gvc.size()];  for(size_t k=0;k<gvc.size();k++) genColor[k]=gvc[k];
    // agrupar tris por material (contiguos), mismo formato que faces[] (startDrawn/indicesDrawnCount)
    std::vector<MeshIndex> vf2; vf2.reserve(tri.size());
    genMaterialsGroup = materialsGroup;
    for (size_t g=0; g<genMaterialsGroup.size(); g++){ genMaterialsGroup[g].startDrawn=(int)vf2.size();
        for (size_t t=0;t<triMat.size();t++) if (triMat[t]==(int)g){ vf2.push_back(tri[t*3]); vf2.push_back(tri[t*3+1]); vf2.push_back(tri[t*3+2]); }
        genMaterialsGroup[g].indicesDrawnCount=(int)vf2.size()-genMaterialsGroup[g].startDrawn; }
    genFacesSize=(int)vf2.size();
    genFaces=new MeshIndex[vf2.size()]; for(size_t k=0;k<vf2.size();k++) genFaces[k]=vf2[k];
    // CONTORNO de seleccion: aristas de POLIGONO (no las diagonales de la triangulacion), dedup. Con esto
    // el objeto con modificadores (subdiv/screw) SI muestra que esta seleccionado (antes quedaba sin borde).
    {
        std::set<std::pair<int,int> > eset;
        genBordesBuf.clear();
        for (size_t f=0; f<poly.size(); f++){
            int mm=(int)poly[f].size();
            for (int i=0;i<mm;i++){
                int a=poly[f][i], b=poly[f][(i+1)%mm];
                if (a==b || a<0 || b<0 || a>=genVertexSize || b>=genVertexSize) continue;
                std::pair<int,int> key = (a<b) ? std::make_pair(a,b) : std::make_pair(b,a);
                if (eset.insert(key).second){
                    genBordesBuf.push_back(gvp[a*3]); genBordesBuf.push_back(gvp[a*3+1]); genBordesBuf.push_back(gvp[a*3+2]);
                    genBordesBuf.push_back(gvp[b*3]); genBordesBuf.push_back(gvp[b*3+1]); genBordesBuf.push_back(gvp[b*3+2]);
                }
            }
        }
    }
    genValido=true;
}

// Aplica UN eje del MIRROR directamente sobre la malla EDITABLE (vertex[]/faces3d + uv/color/normal por VERT):
// los QUADS siguen siendo quads (duplica la cara con winding REVERTIDO, NO triangula). merge: los verts a
// <mergeDist del plano se SNAPEAN al plano y se COMPARTEN (mir[i]=i) -> al re-mergear GenerarRender la costura
// queda soldada y el shading fluye continuo (por simetria la normal de la costura sale alineada al plano sola).
// Las capas se rehacen despues con LiberarCapas+PoblarCapas (leen uv[]/color[] por corner, ya con los espejados).
static void MirrorEditableAxis(Mesh* m, const Vector3& c, const Vector3& n, bool merge, float mergeDist) {
    const int nV = m->vertexSize;
    if (nV <= 0 || !m->vertex) return;
    std::vector<GLfloat> vp(m->vertex, m->vertex + nV*3);
    std::vector<GLbyte>  vn; if (m->normals) vn.assign(m->normals, m->normals+nV*3); else vn.assign((size_t)nV*3, (GLbyte)0);
    std::vector<GLfloat> vu; if (m->uv) vu.assign(m->uv, m->uv+nV*2); else vu.assign((size_t)nV*2, 0.0f);
    std::vector<GLubyte> vc; if (m->vertexColor) vc.assign(m->vertexColor, m->vertexColor+nV*4); else vc.assign((size_t)nV*4, (GLubyte)255);
    std::vector<int> mir(nV);
    for (int i=0;i<nV;i++){
        float px=vp[i*3], py=vp[i*3+1], pz=vp[i*3+2];
        float d = (px-c.x)*n.x + (py-c.y)*n.y + (pz-c.z)*n.z; // distancia FIRMADA al plano
        if (merge && d>-mergeDist && d<mergeDist){
            vp[i*3]=px-d*n.x; vp[i*3+1]=py-d*n.y; vp[i*3+2]=pz-d*n.z; // snap al plano
            mir[i]=i; // compartido (costura soldada) -> shading continuo
        } else {
            float nx=vn[i*3]/127.0f, ny=vn[i*3+1]/127.0f, nz=vn[i*3+2]/127.0f;
            float dn=nx*n.x+ny*n.y+nz*n.z; float rnx=nx-2*dn*n.x, rny=ny-2*dn*n.y, rnz=nz-2*dn*n.z; // normal reflejada
            float uu=vu[i*2], vv=vu[i*2+1];                       // (a locales: push_back puede realocar)
            GLubyte q0=vc[i*4],q1=vc[i*4+1],q2=vc[i*4+2],q3=vc[i*4+3];
            vp.push_back(px-2*d*n.x); vp.push_back(py-2*d*n.y); vp.push_back(pz-2*d*n.z); // v' = v - 2*d*n
            vn.push_back((GLbyte)(rnx*127)); vn.push_back((GLbyte)(rny*127)); vn.push_back((GLbyte)(rnz*127));
            vu.push_back(uu); vu.push_back(vv);
            vc.push_back(q0); vc.push_back(q1); vc.push_back(q2); vc.push_back(q3);
            mir[i]=(int)(vp.size()/3)-1;
        }
    }
    int nN=(int)(vp.size()/3);
    delete[] m->vertex;      m->vertex=new GLfloat[nN*3];      for(int i=0;i<nN*3;i++) m->vertex[i]=vp[i];
    delete[] m->normals;     m->normals=new GLbyte[nN*3];      for(int i=0;i<nN*3;i++) m->normals[i]=vn[i];
    delete[] m->uv;          m->uv=new GLfloat[nN*2];          for(int i=0;i<nN*2;i++) m->uv[i]=vu[i];
    delete[] m->vertexColor; m->vertexColor=new GLubyte[nN*4]; for(int i=0;i<nN*4;i++) m->vertexColor[i]=vc[i];
    m->vertexSize=nN;
    // duplicar cada cara con winding REVERTIDO (quad->quad, ngon->ngon: NO triangula)
    int nF=(int)m->faces3d.size();
    for (int f=0;f<nF;f++){
        MeshFace nf; nf.mat=m->faces3d[f].mat; nf.smooth=m->faces3d[f].smooth; // hereda mesh part + shading por cara
        const std::vector<int>& idx=m->faces3d[f].idx;
        for (int cc=(int)idx.size()-1; cc>=0; cc--) nf.idx.push_back(mir[idx[cc]]);
        m->faces3d.push_back(nf);
    }
}

// "Apply Modifier": hornea el modificador activo en la malla EDITABLE + lo saca del stack.
// MIRROR: se aplica sobre faces3d (los QUADS siguen siendo quads, NADA de triangular) y hace el MERGE si estaba
// activado (suelda la costura). GenerarRender re-mergea y recalcula las normales -> quedan continuas por la
// costura compartida (arregla "se perdieron las normales" del bake viejo, que triangulada + recalculaba flat).
void Mesh::AplicarModificadorActivo() {
    if (modificadorActivo < 0 || modificadorActivo >= (int)modificadores.size()) return;
    Modifier* mod = modificadores[modificadorActivo];
    if (mod->tipo == ModifierType::Armature) {
        // APPLY ARMATURE: hornea la POSE deformada del frame actual en la malla editable (mueve vertices + normales a
        // la posicion actual, como pidio Dante) y saca el modificador. El skinning deja de deformar (queda congelado).
        // NO toca el esqueleto ni el export (el export exporta el rig sin hornear; esto es la accion manual opuesta).
        if (!skinArmature || !vertex || vertexSize <= 0) { QuitarModificadorActivo(); return; }
        UndoCapturarMallaGeo(this);       // Ctrl+Z: snapshot pre-apply
        lastSkinFrame = -999999;          // forzar el re-skin a la pose ACTUAL (evita el cache por frame)
        SkinearMesh(this);                // deforma a CurrentFrame -> skinVertex (mismo espacio local que vertex)
        if (skinVertex) for (int i = 0; i < vertexSize * 3; i++) vertex[i] = skinVertex[i]; // verts -> pose actual
        skinArmature = NULL;              // corta la deformacion: la malla queda congelada en la pose horneada
        QuitarModificadorActivo();        // el Armature ya esta horneado -> fuera del stack
        PoblarCapas();                    // asegura las capas UV/color desde el render antes de regenerar
        GenerarRender();                  // recompone las normales de la geo horneada + preserva UV/color + bordes
        return;
    }
    if (mod->tipo == ModifierType::Mirror) {
        UndoCapturarMallaGeo(this); // Ctrl+Z: snapshot completo de la geometria pre-apply
        Vector3 c(0,0,0), axX(1,0,0), axY(0,1,0), axZ(0,0,1);
        if (mod->target){ // el plano sale del TARGET (posicion + rotacion en mundo, al local del objeto)
            Matrix4 Wo, Wt; GetWorldMatrix(Wo); mod->target->GetWorldMatrix(Wt);
            Matrix4 iWo; InvAffineMod(Wo, iWo); Matrix4 M = iWo * Wt;
            c   = Vector3(M.m[12],M.m[13],M.m[14]);
            axX = NormMod(Vector3(M.m[0],M.m[1],M.m[2]));
            axY = NormMod(Vector3(M.m[4],M.m[5],M.m[6]));
            axZ = NormMod(Vector3(M.m[8],M.m[9],M.m[10]));
        }
        if (mod->ejeX) MirrorEditableAxis(this, c, axX, mod->merge, mod->mergeDist); // secuencial X->Y->Z
        if (mod->ejeY) MirrorEditableAxis(this, c, axY, mod->merge, mod->mergeDist);
        if (mod->ejeZ) MirrorEditableAxis(this, c, axZ, mod->merge, mod->mergeDist);
        QuitarModificadorActivo(); // ya esta horneado -> fuera del stack
        LiberarCapas();            // las capas viejas quedaron de otro tamano (cambio el conteo de corners)
        PoblarCapas();             // rehace UVMap/Col por corner desde uv[]/color[] (incluye los verts espejados)
        GenerarRender();           // re-mergea (suelda la costura) + recalcula normales continuas; sin stack -> genValido=false
        return;
    }
    // OTROS tipos (Subdivision, Screw): hornear MANTENIENDO LOS QUADS (no triangulado). Usa el mismo pipeline de
    // poligonos y baja los POLIGONOS a faces3d -> quedas con topologia de quads limpia para seguir editando.
    UndoCapturarMallaGeo(this); // Ctrl+Z: snapshot pre-apply
    PolyMesh W; bool outSmooth=false;
    if (!Mesh_AplicarStack(this, false, W, outSmooth)){ QuitarModificadorActivo(); return; } // nivel de VIEWPORT
    const bool smooth = meshSmooth || outSmooth;
    std::vector<GLfloat> gvp; std::vector<GLbyte> gvn; std::vector<GLfloat> gvu; std::vector<GLubyte> gvc;
    std::vector<std::vector<int> > poly; std::vector<int> polyMat;
    PolyARenderVerts(W, smooth, gvp,gvn,gvu,gvc, poly, polyMat);
    if (gvp.empty() || poly.empty()){ QuitarModificadorActivo(); return; }
    int nv = (int)(gvp.size()/3);
    delete[] vertex;      vertex      = new GLfloat[nv*3]; for(int i=0;i<nv*3;i++) vertex[i]=gvp[i];
    delete[] normals;     normals     = new GLbyte [nv*3]; for(int i=0;i<nv*3;i++) normals[i]=gvn[i];
    delete[] uv;          uv          = new GLfloat[nv*2]; for(int i=0;i<nv*2;i++) uv[i]=gvu[i];
    delete[] vertexColor; vertexColor = new GLubyte[nv*4]; for(int i=0;i<nv*4;i++) vertexColor[i]=gvc[i];
    vertexSize = nv;
    faces3d.clear(); looseEdges.clear();
    for (size_t f=0; f<poly.size(); f++){ MeshFace mf; mf.mat=polyMat[f]; // <- POLIGONO (quad), no triangulos
        for (size_t c=0;c<poly[f].size();c++) mf.idx.push_back(poly[f][c]); faces3d.push_back(mf); }
    LiberarCapas();            // las capas se rehacen desde el render nuevo (PoblarCapas en GenerarRender)
    // horneamos TODO el stack -> vaciarlo entero (materialsGroup se conserva; GenerarRender rearma los rangos)
    for (size_t i=0;i<modificadores.size();i++) delete modificadores[i];
    modificadores.clear(); modificadorActivo = -1;
    GenerarRender();           // rebuild desde faces3d (mergea + capas + normales); sin stack -> genValido queda false
}

// ===================================================
// MESH PARTS (materialsGroup): crear / borrar / reordenar. Antes eran metodos de
// Mesh (core); son edicion (editor), asi que viven aca como funciones libres.
// ===================================================
int NuevoMeshPart(Mesh* m) {
    MaterialGroup g; // name "Mesh", material NULL = usa el material por defecto
    m->materialsGroup.push_back(g);
    m->ReagruparMeshParts();
    return (int)m->materialsGroup.size() - 1;
}

// Borra el mesh part 'idx'. Las caras huerfanas pasan al ANTERIOR (idx-1, o al que quede en 0 si era
// el primero). SIEMPRE queda >=1 mesh part (no borra el ultimo). Remapea los indices de arriba.
void BorrarMeshPart(Mesh* m, int idx) {
    int n = (int)m->materialsGroup.size();
    if (n <= 1) return;                 // siempre tiene que haber al menos 1
    if (idx < 0 || idx >= n) return;
    int destino = (idx > 0) ? idx - 1 : 0; // huerfanas -> anterior (o el nuevo 0)
    for (size_t f=0; f<m->faces3d.size(); f++) {
        int mm = m->faces3d[f].mat;
        if (mm == idx) mm = destino;    // huerfana -> anterior
        if (mm > idx)  mm -= 1;         // los de arriba bajan 1 (se removio idx)
        m->faces3d[f].mat = mm;
    }
    m->materialsGroup.erase(m->materialsGroup.begin() + idx);
    m->ReagruparMeshParts();
}

// mueve el mesh part 'idx' una posicion (dir: -1 sube / +1 baja) intercambiandolo con el vecino. Cambia el ORDEN
// del materialsGroup = ORDEN DE DIBUJADO. Remapea faces3d.mat de los 2 intercambiados.
void MoverMeshPart(Mesh* m, int idx, int dir) {
    int n = (int)m->materialsGroup.size();
    int j = idx + dir;
    if (idx < 0 || idx >= n || j < 0 || j >= n) return;
    MaterialGroup tmp = m->materialsGroup[idx]; m->materialsGroup[idx] = m->materialsGroup[j]; m->materialsGroup[j] = tmp;
    for (size_t f=0; f<m->faces3d.size(); f++) {
        if      (m->faces3d[f].mat == idx) m->faces3d[f].mat = j;
        else if (m->faces3d[f].mat == j)   m->faces3d[f].mat = idx;
    }
    m->ReagruparMeshParts(); // rehace el index buffer en el nuevo orden -> nuevo orden de dibujado
}

// ===================================================
// CAPAS por-corner (UV maps / color layers). Antes eran metodos de Mesh (core);
// son edicion (editor), asi que viven aca como funciones libres sobre Mesh*.
// ===================================================
void DuplicarUVMapActivo(Mesh* m) {
    m->PoblarCapas();
    if (m->uvMapActivo < 0 || m->uvMapActivo >= (int)m->uvMaps.size()) return;
    UVMap* src = m->uvMaps[m->uvMapActivo];
    UVMap* nw = new UVMap(src->nombre + ".001");
    nw->uv = src->uv;
    m->uvMaps.push_back(nw); m->uvMapActivo = (int)m->uvMaps.size() - 1;
}

void DuplicarColorLayerActivo(Mesh* m) {
    m->PoblarCapas();
    if (m->colorActivo < 0 || m->colorActivo >= (int)m->colorLayers.size()) return;
    ColorLayer* src = m->colorLayers[m->colorActivo];
    ColorLayer* nw = new ColorLayer(src->nombre + ".001");
    nw->color = src->color; nw->porVertice = src->porVertice;
    m->colorLayers.push_back(nw); m->colorActivo = (int)m->colorLayers.size() - 1;
}

// BORRAR / MOVER la UV map o la capa de color ACTIVA (menu Properties). Siempre queda al menos 1 (los botones
// Delete/Move se ocultan con 1 solo elemento). El caller re-hornea con AplicarCapasAlRender.
void BorrarUVMapActivo(Mesh* m) {
    if ((int)m->uvMaps.size() <= 1) return;
    int i = m->uvMapActivo; if (i < 0 || i >= (int)m->uvMaps.size()) return;
    delete m->uvMaps[i]; m->uvMaps.erase(m->uvMaps.begin() + i);
    if (m->uvMapActivo >= (int)m->uvMaps.size()) m->uvMapActivo = (int)m->uvMaps.size() - 1;
}
void MoverUVMapActivo(Mesh* m, int dir) {
    int i = m->uvMapActivo, j = i + dir;
    if (i < 0 || i >= (int)m->uvMaps.size() || j < 0 || j >= (int)m->uvMaps.size()) return;
    UVMap* t = m->uvMaps[i]; m->uvMaps[i] = m->uvMaps[j]; m->uvMaps[j] = t; m->uvMapActivo = j;
}
void BorrarColorLayerActivo(Mesh* m) {
    if ((int)m->colorLayers.size() <= 1) return;
    int i = m->colorActivo; if (i < 0 || i >= (int)m->colorLayers.size()) return;
    delete m->colorLayers[i]; m->colorLayers.erase(m->colorLayers.begin() + i);
    if (m->colorActivo >= (int)m->colorLayers.size()) m->colorActivo = (int)m->colorLayers.size() - 1;
}
void MoverColorLayerActivo(Mesh* m, int dir) {
    int i = m->colorActivo, j = i + dir;
    if (i < 0 || i >= (int)m->colorLayers.size() || j < 0 || j >= (int)m->colorLayers.size()) return;
    ColorLayer* t = m->colorLayers[i]; m->colorLayers[i] = m->colorLayers[j]; m->colorLayers[j] = t; m->colorActivo = j;
}

// GRUPOS DE VERTICES (huesos del rig / pesos). A diferencia de UV/color pueden ser 0 (no hay que poblar por defecto).
void CrearVertexGroup(Mesh* m) {
    if (!m) return;
    // nombre unico "Group.NNN"
    std::string base = "Group", nombre = base; int suf = 0;
    for (;;){ bool choca = false;
        for (size_t i=0;i<m->vertexGroups.size();i++) if (m->vertexGroups[i]->nombre == nombre){ choca=true; break; }
        if (!choca) break;
        ++suf; char b[16]; sprintf(b, ".%03d", suf); nombre = base + b; }
    m->vertexGroups.push_back(new VertexGroup(nombre));
    m->grupoActivo = (int)m->vertexGroups.size() - 1;
}
void BorrarVertexGroupActivo(Mesh* m) {
    if (!m) return;
    int i = m->grupoActivo; if (i < 0 || i >= (int)m->vertexGroups.size()) return;
    delete m->vertexGroups[i]; m->vertexGroups.erase(m->vertexGroups.begin() + i);
    if (m->grupoActivo >= (int)m->vertexGroups.size()) m->grupoActivo = (int)m->vertexGroups.size() - 1;
}
void MoverVertexGroupActivo(Mesh* m, int dir) {
    if (!m) return;
    int i = m->grupoActivo, j = i + dir;
    if (i < 0 || i >= (int)m->vertexGroups.size() || j < 0 || j >= (int)m->vertexGroups.size()) return;
    VertexGroup* t = m->vertexGroups[i]; m->vertexGroups[i] = m->vertexGroups[j]; m->vertexGroups[j] = t; m->grupoActivo = j;
}

// reversa los datos de TODAS las capas de los corners [L .. L+count). Lo usa el flip de
// winding para que cada corner siga llevando el dato del vert al que ahora apunta.
void ReverseCapasDeCorner(Mesh* m, int L, int count) {
    for (size_t k=0;k<m->uvMaps.size();k++){ std::vector<GLfloat>& u=m->uvMaps[k]->uv;
        for (int a=0,b=count-1;a<b;a++,b--){ int ia=(L+a)*2, ib=(L+b)*2; if (ib+1>=(int)u.size()) break;
            std::swap(u[ia],u[ib]); std::swap(u[ia+1],u[ib+1]); } }
    for (size_t k=0;k<m->colorLayers.size();k++){ if (m->colorLayers[k]->porVertice) continue;
        std::vector<GLubyte>& c=m->colorLayers[k]->color;
        for (int a=0,b=count-1;a<b;a++,b--){ int ia=(L+a)*4, ib=(L+b)*4; if (ib+3>=(int)c.size()) break;
            for(int q=0;q<4;q++) std::swap(c[ia+q],c[ib+q]); } }
    if (!m->cornerNormal.empty())
        for (int a=0,b=count-1;a<b;a++,b--){ int ia=(L+a)*3, ib=(L+b)*3; if (ib+2>=(int)m->cornerNormal.size()) break;
            for(int q=0;q<3;q++) std::swap(m->cornerNormal[ia+q],m->cornerNormal[ib+q]); }
}

// agrega UN corner al FINAL de todas las capas por-corner, copiando del corner srcL (o
// default uv=0 / color blanco si srcL<0). Lo usan las ops que agregan caras al final de faces3d.
void AgregarCornerCapas(Mesh* m, int srcL) {
    for (size_t k=0;k<m->uvMaps.size();k++){ std::vector<GLfloat>& u=m->uvMaps[k]->uv;
        GLfloat a=0,b=0; if (srcL>=0 && srcL*2+1<(int)u.size()){ a=u[srcL*2]; b=u[srcL*2+1]; }
        u.push_back(a); u.push_back(b); }
    for (size_t k=0;k<m->colorLayers.size();k++){ if (m->colorLayers[k]->porVertice) continue;
        std::vector<GLubyte>& c=m->colorLayers[k]->color;
        GLubyte q[4]={255,255,255,255}; if (srcL>=0 && srcL*4+3<(int)c.size()){ for(int i=0;i<4;i++) q[i]=c[srcL*4+i]; }
        for(int i=0;i<4;i++) c.push_back(q[i]); }
    if (!m->cornerNormal.empty()){ // normal autoritativa: el corner nuevo hereda la de srcL
        GLbyte n[3]={0,127,0}; if (srcL>=0 && srcL*3+2<(int)m->cornerNormal.size()){ for(int i=0;i<3;i++) n[i]=m->cornerNormal[srcL*3+i]; }
        for(int i=0;i<3;i++) m->cornerNormal.push_back(n[i]); }
}

// reconstruye TODAS las capas por-corner quedandose SOLO con los corners de survCorner.
// Para las ops que BORRAN corners (delete): lee las capas viejas -> arma nuevas -> swap.
void CompactarCapas(Mesh* m, const std::vector<int>& survCorner) {
    for (size_t k=0;k<m->uvMaps.size();k++){ std::vector<GLfloat>& u=m->uvMaps[k]->uv; std::vector<GLfloat> nu; nu.reserve(survCorner.size()*2);
        for (size_t i=0;i<survCorner.size();i++){ int L=survCorner[i];
            if (L>=0 && L*2+1<(int)u.size()){ nu.push_back(u[L*2]); nu.push_back(u[L*2+1]); } else { nu.push_back(0); nu.push_back(0); } }
        u.swap(nu); }
    for (size_t k=0;k<m->colorLayers.size();k++){ if (m->colorLayers[k]->porVertice) continue; std::vector<GLubyte>& c=m->colorLayers[k]->color; std::vector<GLubyte> nc; nc.reserve(survCorner.size()*4);
        for (size_t i=0;i<survCorner.size();i++){ int L=survCorner[i];
            if (L>=0 && L*4+3<(int)c.size()){ for(int q=0;q<4;q++) nc.push_back(c[L*4+q]); } else { for(int q=0;q<4;q++) nc.push_back(255); } }
        c.swap(nc); }
    if (!m->cornerNormal.empty()){ std::vector<GLbyte> nn; nn.reserve(survCorner.size()*3);
        for (size_t i=0;i<survCorner.size();i++){ int L=survCorner[i];
            if (L>=0 && L*3+2<(int)m->cornerNormal.size()){ for(int q=0;q<3;q++) nn.push_back(m->cornerNormal[L*3+q]); } else { nn.push_back(0); nn.push_back(127); nn.push_back(0); } }
        m->cornerNormal.swap(nn); }
}

// rehace TODAS las capas por-corner desde una lista de fuentes (copiar del corner a, o lerp
// entre a y b en s). Para ops que restructuran con verts interpolados (loop cut).
void ReconstruirCapasDesde(Mesh* m, const std::vector<CornerSrc>& src) {
    for (size_t k=0;k<m->uvMaps.size();k++){ const std::vector<GLfloat>& u=m->uvMaps[k]->uv; std::vector<GLfloat> nu; nu.reserve(src.size()*2);
        for (size_t i=0;i<src.size();i++){ int a=src[i].a, b=src[i].b; float u0=0,v0=0;
            if (a>=0 && a*2+1<(int)u.size()){ u0=u[a*2]; v0=u[a*2+1]; }
            if (b>=0 && b*2+1<(int)u.size()){ float s=src[i].s; u0=u0*(1-s)+u[b*2]*s; v0=v0*(1-s)+u[b*2+1]*s; }
            nu.push_back(u0); nu.push_back(v0); }
        m->uvMaps[k]->uv.swap(nu); }
    for (size_t k=0;k<m->colorLayers.size();k++){ if (m->colorLayers[k]->porVertice) continue; const std::vector<GLubyte>& c=m->colorLayers[k]->color; std::vector<GLubyte> nc; nc.reserve(src.size()*4);
        for (size_t i=0;i<src.size();i++){ int a=src[i].a, b=src[i].b; float cc[4]={255,255,255,255};
            if (a>=0 && a*4+3<(int)c.size()){ for(int q=0;q<4;q++) cc[q]=c[a*4+q]; }
            if (b>=0 && b*4+3<(int)c.size()){ float s=src[i].s; for(int q=0;q<4;q++) cc[q]=cc[q]*(1-s)+c[b*4+q]*s; }
            for(int q=0;q<4;q++) nc.push_back((GLubyte)cc[q]); }
        m->colorLayers[k]->color.swap(nc); }
    if (!m->cornerNormal.empty()){ const std::vector<GLbyte>& cn=m->cornerNormal; std::vector<GLbyte> nn; nn.reserve(src.size()*3);
        for (size_t i=0;i<src.size();i++){ int a=src[i].a, b=src[i].b; float n[3]={0,127,0};
            if (a>=0 && a*3+2<(int)cn.size()){ for(int q=0;q<3;q++) n[q]=cn[a*3+q]; }
            if (b>=0 && b*3+2<(int)cn.size()){ float s=src[i].s; for(int q=0;q<3;q++) n[q]=n[q]*(1-s)+cn[b*3+q]*s; }
            for(int q=0;q<3;q++) nn.push_back((GLbyte)n[q]); }
        m->cornerNormal.swap(nn); }
}

// ===================================================
// FOCO de camara (menu '.'): centro/radio del bounding de la malla en MUNDO. Es ayuda del
// editor/viewport; antes vivia en el Core. Se dejan como Mesh:: (override de Object::Foco).
// ===================================================
// Si la malla esta SKINNEADA, el foco tiene que encuadrar la pose DEFORMADA del frame actual (skinVertex),
// no el bind: centroGeom se calcula del bind (vertex, control points sin deformar) -> un personaje animado
// enfocaba una nube colapsada (LISA parada enfocaba los pies). Bounding LOCAL de skinVertex. false si no hay
// skinning o todavia no se skinneo (se skinnea en el render; si aun no se dibujo, cae al bind).
static bool CentroRadioSkinLocal(const Mesh* m, Vector3& c, float& r) {
    if (!m->skinArmature || !m->skinVertex || m->vertexSize <= 0) return false;
    const GLfloat* sv = m->skinVertex;
    Vector3 mn(sv[0], sv[1], sv[2]), mx = mn;
    for (int i = 1; i < m->vertexSize; i++) {
        float x = sv[i*3], y = sv[i*3+1], z = sv[i*3+2];
        if (x<mn.x)mn.x=x; if (y<mn.y)mn.y=y; if (z<mn.z)mn.z=z;
        if (x>mx.x)mx.x=x; if (y>mx.y)mx.y=y; if (z>mx.z)mx.z=z;
    }
    c = (mn + mx) * 0.5f;
    r = (mx - c).Length();
    return true;
}
Vector3 Mesh::PuntoFoco() const {
    Vector3 c; float r;
    if (CentroRadioSkinLocal(this, c, r)) return LocalAMundo(c);
    return LocalAMundo(centroGeom);
}

// escala un radio LOCAL a MUNDO transformando 3 puntos sobre los ejes y tomando el mayor
// (asi una escala no uniforme no subestima el bounding).
float Mesh::EscalarRadioLocal(const Vector3& cLocal, float rLocal) const {
    Vector3 c = LocalAMundo(cLocal);
    Vector3 ax[3] = { Vector3(rLocal,0,0), Vector3(0,rLocal,0), Vector3(0,0,rLocal) };
    float r = 0.0f;
    for (int i = 0; i < 3; i++) { float d = (LocalAMundo(cLocal + ax[i]) - c).Length(); if (d > r) r = d; }
    return r;
}
float Mesh::RadioFoco() const {
    Vector3 c; float r;
    if (CentroRadioSkinLocal(this, c, r)) return EscalarRadioLocal(c, r);
    return EscalarRadioLocal(centroGeom, radioGeom);
}

// ===================================================
// GenerarRender (rebuild editable->render) + CalcularBordes (bordes/centro geometrico).
// Antes en el Core (Mesh.cpp); son del pipeline de edicion. Siguen como Mesh:: (decl en Mesh.h).
// ===================================================
// REBUILD COMPLETO del render: DESTRUYE los arrays y los rehace desde los CORNERS (faces3d)
// + las CAPAS ACTIVAS. Expande cada corner a un vertice GPU MERGEANDO los identicos (pos +
// uv + normal + color) para que las mallas smooth NO se inflen. Es la UNICA fuente de verdad
// del render ante cambios de TOPOLOGIA: las edit-ops construyen faces3d + las capas y llaman
// aca (asi TODAS las capas sobreviven, sin meter interpolacion en cada op). LENTO -> usar
// solo cuando cambia la topologia; para mover/pintar usar RefrescarRender (in-place).
void Mesh::GenerarRender(bool recomputarNormales) {
    int nC = ContarCorners();
    // Sin caras (nC=0) igual seguimos si hay geometria SUELTA (loose edges/verts): para preservarla y, sobre todo,
    // para reconstruir faces[] a vacio (sino quedan triangulos fantasma del estado anterior). Solo cortamos si no
    // queda NADA que rebuildear o no hay vertex[] de donde leer posiciones.
    if (!vertex || (nC <= 0 && looseEdges.empty() && looseVerts.empty())) return;
    chromeUVValid = false; // la geometria cambia -> recalcular el reflejo equirect
    tangentsValid = false; // ... y las tangentes del normal map (dependen de pos+UV)
    // FLAT (cubo/plano): la normal autoritativa por corner = la de su CARA. Asi el merge de
    // abajo (la clave incluye la normal) NO une verts de caras distintas -> shading plano tras
    // extrude/loop cut. SMOOTH: normal por GRUPO de suavizado (promedia las caras de alrededor;
    // un borde sharp corta el grupo). Sin bordes sharp = todo suave; con sharp = cilindro.
    // Path rapido PorCara SOLO si es flat global y NINGUNA cara tiene override per-cara (Face>Shade sobre seleccion);
    // si hay override o es smooth -> ConSharp (respeta el flag smooth POR CARA de cada faces3d).
    if (!recomputarNormales){
        // CONSERVAR las normales actuales (JOIN): cornerNormal se llena DESDE normals[] -> el merge de abajo usa la
        // normal real de cada vertice (la del archivo, con sus splits) y NO se redondean los bordes filosos.
        SincronizarCornerNormal();
    } else {
    bool hayOverride=false; for (size_t f=0;f<faces3d.size();f++) if (faces3d[f].smooth>=0){ hayOverride=true; break; }
    if (!meshSmooth && !hayOverride) CornerNormalPorCara();
    else                            CornerNormalConSharp();
    }
    UVMap* um = (uvMapActivo>=0 && uvMapActivo<(int)uvMaps.size()) ? uvMaps[uvMapActivo] : NULL;
    ColorLayer* cl = (colorActivo>=0 && colorActivo<(int)colorLayers.size()) ? colorLayers[colorActivo] : NULL;
    bool tUV  = um && (int)um->uv.size()==nC*2;
    bool tCol = cl && !cl->porVertice && (int)cl->color.size()==nC*4;
    bool tNor = (normals != NULL);
    bool tCN  = ((int)cornerNormal.size() == nC*3); // normal AUTORITATIVA por corner

    std::map<std::string,int> mapa; // clave (pos+uv+normal+color) -> indice GPU nuevo (merge)
    std::vector<GLfloat> vp; std::vector<GLbyte> vn; std::vector<GLfloat> vu; std::vector<GLubyte> vc;
    std::vector<MeshFace> nf3d; nf3d.reserve(faces3d.size());
    std::vector<int> oldToNew(vertexSize, -1); // GPU viejo -> nuevo (1er corner) para remapear loose edges
    int L = 0;
    for (size_t f=0;f<faces3d.size();f++) { MeshFace mf; mf.mat = faces3d[f].mat; mf.smooth = faces3d[f].smooth; // conserva mesh part + shading por cara
        for (size_t c=0;c<faces3d[f].idx.size();c++) {
            int gv = faces3d[f].idx[c];
            float px=vertex[gv*3], py=vertex[gv*3+1], pz=vertex[gv*3+2];
            GLbyte nbx=0,nby=127,nbz=0; // normal del corner: autoritativa si esta, sino el render viejo
            if (tCN){ nbx=cornerNormal[L*3]; nby=cornerNormal[L*3+1]; nbz=cornerNormal[L*3+2]; }
            else if (tNor){ nbx=normals[gv*3]; nby=normals[gv*3+1]; nbz=normals[gv*3+2]; }
            float u0 = tUV ? um->uv[L*2] : 0.0f, v0 = tUV ? um->uv[L*2+1] : 0.0f;
            GLubyte r=255,g=255,b=255,a=255; if (tCol){ r=cl->color[L*4];g=cl->color[L*4+1];b=cl->color[L*4+2];a=cl->color[L*4+3]; }
            char buf[40]; int p=0;
            memcpy(buf+p,&px,4);p+=4; memcpy(buf+p,&py,4);p+=4; memcpy(buf+p,&pz,4);p+=4;
            memcpy(buf+p,&u0,4);p+=4; memcpy(buf+p,&v0,4);p+=4;
            buf[p++]=(char)nbx;buf[p++]=(char)nby;buf[p++]=(char)nbz;
            buf[p++]=(char)r;buf[p++]=(char)g;buf[p++]=(char)b;buf[p++]=(char)a;
            std::string key(buf,p);
            std::map<std::string,int>::iterator it=mapa.find(key);
            int gi;
            if (it!=mapa.end()) gi=it->second;
            else { gi=(int)(vp.size()/3);
                vp.push_back(px);vp.push_back(py);vp.push_back(pz);
                vn.push_back(nbx);vn.push_back(nby);vn.push_back(nbz);
                vu.push_back(u0);vu.push_back(v0);
                vc.push_back(r);vc.push_back(g);vc.push_back(b);vc.push_back(a);
                mapa[key]=gi; }
            mf.idx.push_back(gi); if (gv>=0 && gv<(int)oldToNew.size() && oldToNew[gv]<0) oldToNew[gv]=gi; L++;
        }
        nf3d.push_back(mf);
    }
    // LOOSE EDGES: remapear sus extremos al GPU nuevo (preservarlos en el rebuild). Un
    // extremo que no aparece en ninguna cara se agrega como vert nuevo (pos del vert viejo).
    std::vector<int> nLoose; nLoose.reserve(looseEdges.size());
    for (size_t i=0;i+1<looseEdges.size();i+=2){
        int ab[2]={looseEdges[i],looseEdges[i+1]}, nn[2];
        for (int s=0;s<2;s++){ int o=ab[s];
            if (o>=0 && o<(int)oldToNew.size() && oldToNew[o]>=0) nn[s]=oldToNew[o];
            else if (o>=0 && o<vertexSize){ int gi=(int)(vp.size()/3);
                vp.push_back(vertex[o*3]);vp.push_back(vertex[o*3+1]);vp.push_back(vertex[o*3+2]);
                vn.push_back(0);vn.push_back(127);vn.push_back(0);
                vu.push_back(0);vu.push_back(0);
                vc.push_back(255);vc.push_back(255);vc.push_back(255);vc.push_back(255);
                oldToNew[o]=gi; nn[s]=gi;
            } else nn[s]=-1; }
        if (nn[0]>=0 && nn[1]>=0){ nLoose.push_back(nn[0]); nLoose.push_back(nn[1]); }
    }
    // LOOSE VERTS: verts sueltos (sin cara NI arista). Los que ya aparecen en una cara/arista NO son sueltos de
    // verdad (oldToNew ya seteado) -> se saltean; el resto se agrega como vert nuevo (pos del viejo).
    std::vector<int> nLooseV; nLooseV.reserve(looseVerts.size());
    for (size_t i=0;i<looseVerts.size();i++){ int o=looseVerts[i];
        if (o<0 || o>=vertexSize) continue;
        if (oldToNew[o]>=0) continue; // ya lo referencia una cara/arista (o un looseVert previo) -> NO es suelto, se descarta
        int gi=(int)(vp.size()/3);
        vp.push_back(vertex[o*3]);vp.push_back(vertex[o*3+1]);vp.push_back(vertex[o*3+2]);
        vn.push_back(0);vn.push_back(127);vn.push_back(0); vu.push_back(0);vu.push_back(0);
        vc.push_back(255);vc.push_back(255);vc.push_back(255);vc.push_back(255);
        oldToNew[o]=gi; nLooseV.push_back(gi);
    }
    int nuevoN=(int)(vp.size()/3); if (nuevoN<=0) return;
    delete[] vertex;      vertex=new GLfloat[nuevoN*3];  for(int i=0;i<nuevoN*3;i++) vertex[i]=vp[i];
    delete[] normals;     normals=new GLbyte[nuevoN*3];  for(int i=0;i<nuevoN*3;i++) normals[i]=vn[i];
    delete[] uv;          uv=new GLfloat[nuevoN*2];      for(int i=0;i<nuevoN*2;i++) uv[i]=vu[i];
    delete[] vertexColor; vertexColor=new GLubyte[nuevoN*4]; for(int i=0;i<nuevoN*4;i++) vertexColor[i]=vc[i];
    vertexSize=nuevoN;
    // SKINNING: remapear vertCtrlPoint (render-vert -> control-point) al render NUEVO. El merge de arriba re-dedupea
    // los render verts, asi que el mapeo viejo queda invalido: sin esto, editar/joinear una malla skinneada rompia el
    // skin (los verts sin CP -> peso 0 -> la malla colapsaba). oldToNew mapea viejo->nuevo; verts nuevos (loose) = -1.
    if (!vertCtrlPoint.empty()){
        std::vector<int> nCP(nuevoN, -1);
        for (int i=0;i<(int)oldToNew.size() && i<(int)vertCtrlPoint.size();i++)
            if (oldToNew[i]>=0 && oldToNew[i]<nuevoN) nCP[oldToNew[i]] = vertCtrlPoint[i];
        vertCtrlPoint.swap(nCP);
    }
    faces3d.swap(nf3d);   // misma cantidad/orden de corners -> las capas siguen validas
    looseEdges.swap(nLoose); // bordes sueltos remapeados a los GPU nuevos
    looseVerts.swap(nLooseV); // verts sueltos remapeados
    ReagruparMeshParts(); // arma el index buffer + los rangos por mesh part (mf.mat)
    // ORDEN IMPORTANTE: CalcularBordes PRIMERO (recomputa posRep = conectividad topologica) y RECIEN
    // despues GenerarMallaModificada. El stack de modificadores (subdivision/screw) arma la PolyMesh
    // deduplicando los verts de render por posRep; si posRep esta viejo (topologia cambiada por un
    // extrude/loop cut) suelda mal y la subdivision sale deforme hasta mover un vertice. Con posRep
    // fresco antes de aplicar el stack, queda bien al instante (bug reportado por Dante).
    CalcularBordes(); // recomputa posRep/edges/centroGeom + invalida el edit (geometria nueva)
    GenerarMallaModificada(); // EDITOR: re-aplica el stack de modificadores -> malla generada (render). No-op sin stack
}

// ============================================================================
//  OPTIMIZACION DE CACHE DE VERTICES — algoritmo de Tom Forsyth ("Linear-Speed
//  Vertex Cache Optimisation", 2006), que es PUBLICO y de uso libre. Esto es una
//  IMPLEMENTACION PROPIA de Whisk3D escrita desde la descripcion del algoritmo
//  (NO copia codigo del PowerVR SDK ni de ningun tercero) -> mantiene Whisk3D MIT.
//  Reordena los triangulos del index buffer para maximizar los aciertos del cache
//  post-transform del GPU (menos transformaciones de vertice repetidas). Ganancia
//  grande en GPUs tile-based como el PowerVR MBX del N95. NO toca el vertex buffer:
//  solo el ORDEN de los indices -> la malla se ve identica.
// ============================================================================
static const int   kVC_Size       = 32;    // tamano del cache que asumimos
static const float kVC_DecayPower  = 1.5f;
static const float kVC_LastTri     = 0.75f;
static const float kVC_ValScale    = 2.0f;
static const float kVC_ValPower    = 0.5f;

// score de un vertice: "que tan arriba esta en el cache" + bonus por "pocos triangulos
// pendientes lo usan" (conviene gastarlo ya). -1 = ningun triangulo pendiente lo usa.
static float VCacheScore(int trisPend, int posCache) {
    if (trisPend <= 0) return -1.0f;
    float score = 0.0f;
    if (posCache >= 0) {
        if (posCache < 3) score = kVC_LastTri;                 // los 3 verts del triangulo recien emitido
        else { float s = 1.0f - (float)(posCache - 3) / (float)(kVC_Size - 3); score = powf(s, kVC_DecayPower); }
    }
    score += kVC_ValScale * powf((float)trisPend, -kVC_ValPower);
    return score;
}

// reordena IN-PLACE los 3*numTris indices de 'idx' (una mesh part) para el cache de vertices.
static void OptimizarCacheVertices(MeshIndex* idx, int numTris, int numVerts) {
    if (!idx || numVerts <= 0 || numTris < 64) return; // mallas chicas entran enteras al cache: no hace falta

    // adyacencia vertice -> triangulos que lo usan (formato CSR)
    std::vector<int> cuenta(numVerts, 0);
    for (int i = 0; i < numTris*3; i++) { int v=(int)idx[i]; if (v>=0&&v<numVerts) cuenta[v]++; }
    std::vector<int> off(numVerts+1, 0);
    for (int v = 0; v < numVerts; v++) off[v+1] = off[v] + cuenta[v];
    std::vector<int> trisDe(off[numVerts]);
    { std::vector<int> cur(off.begin(), off.end()-1);
      for (int t=0;t<numTris;t++) for(int k=0;k<3;k++){ int v=(int)idx[t*3+k]; if (v>=0&&v<numVerts) trisDe[cur[v]++]=t; } }

    std::vector<int>   pend(cuenta);             // triangulos pendientes por vertice
    std::vector<int>   posCache(numVerts, -1);
    std::vector<float> scoreV(numVerts);
    for (int v=0;v<numVerts;v++) scoreV[v] = VCacheScore(pend[v], -1);
    std::vector<char>  usado(numTris, 0);
    std::vector<float> scoreT(numTris);
    for (int t=0;t<numTris;t++) scoreT[t] = scoreV[(int)idx[t*3]] + scoreV[(int)idx[t*3+1]] + scoreV[(int)idx[t*3+2]];

    std::vector<MeshIndex> salida; salida.reserve(numTris*3);
    std::vector<int> cache; cache.reserve(kVC_Size + 4); // LRU, MRU al frente

    int mejor=-1; float mejorS=-1.0f;
    for (int t=0;t<numTris;t++) if (scoreT[t] > mejorS){ mejorS=scoreT[t]; mejor=t; }

    for (int e=0; e<numTris; e++) {
        if (mejor < 0) { // fallback: el mejor entre TODOS los pendientes (arranca una componente nueva)
            mejorS=-1.0f; for (int t=0;t<numTris;t++) if (!usado[t] && scoreT[t]>mejorS){ mejorS=scoreT[t]; mejor=t; }
            if (mejor < 0) break;
        }
        int t = mejor; usado[t]=1;
        int v[3] = { (int)idx[t*3], (int)idx[t*3+1], (int)idx[t*3+2] };
        for (int k=0;k<3;k++) salida.push_back((MeshIndex)v[k]);
        for (int k=0;k<3;k++) pend[v[k]]--;
        // mover v[0..2] al frente del cache (sin duplicar)
        for (int k=0;k<3;k++){ int vk=v[k];
            for (size_t i=0;i<cache.size();i++) if (cache[i]==vk){ cache.erase(cache.begin()+i); break; }
            cache.insert(cache.begin(), vk); }
        // los que pasan kVC_Size salen del cache; junto los AFECTADOS (en cache + salidos) para re-scorear
        std::vector<int> afect;
        while ((int)cache.size() > kVC_Size){ int vo=cache.back(); cache.pop_back(); posCache[vo]=-1; afect.push_back(vo); }
        for (size_t i=0;i<cache.size();i++){ posCache[cache[i]]=(int)i; afect.push_back(cache[i]); }
        for (size_t a=0;a<afect.size();a++){ int vv=afect[a]; scoreV[vv]=VCacheScore(pend[vv], posCache[vv]); }
        // re-score de los triangulos de los afectados + trackeo el mejor entre los tocados
        mejor=-1; mejorS=-1.0f;
        for (size_t a=0;a<afect.size();a++){ int vv=afect[a];
            for (int j=off[vv]; j<off[vv+1]; j++){ int tt=trisDe[j]; if (usado[tt]) continue;
                scoreT[tt] = scoreV[(int)idx[tt*3]] + scoreV[(int)idx[tt*3+1]] + scoreV[(int)idx[tt*3+2]];
                if (scoreT[tt] > mejorS){ mejorS=scoreT[tt]; mejor=tt; } } }
    }
    for (int i=0;i<numTris*3 && i<(int)salida.size();i++) idx[i] = salida[i];
}

// Reconstruye el index buffer (faces[]) AGRUPANDO los triangulos por material POR-CARA (mf.mat), y

// recalcula los BORDES unicos desde faces3d, dedup por POSICION (no por indice:
// asi un borde compartido por 2 caras con vertices distintos -mismo lugar- no se
// repite). Cada par (edges[2i], edges[2i+1]) es una arista.

// "Optimizar Vertex Groups" (1 hueso por vertice): por cada control-point deja SOLO el hueso de MAYOR peso, con
// peso 1, y lo saca del resto de los grupos. DESTRUCTIVO (se pierde la mezcla de pesos). Acelera el skinning en el
// N95: el blend por vertice pasa de N influencias a 1 (un solo M*v, sin division ni acumular). Invalida el CSR de
// skinning para que se reconstruya con 1 influencia por vertex.
void OptimizarVertexGroups1Hueso(Mesh* m){
    if (!m || m->vertexGroups.empty()) return;
    std::map<int, std::pair<int,float> > mejor; // control-point -> (indice de grupo/hueso, peso maximo visto)
    for (size_t g = 0; g < m->vertexGroups.size(); g++){
        VertexGroup* vg = m->vertexGroups[g];
        for (size_t j = 0; j < vg->verts.size() && j < vg->pesos.size(); j++){
            int cp = vg->verts[j]; float w = vg->pesos[j];
            std::map<int,std::pair<int,float> >::iterator it = mejor.find(cp);
            if (it == mejor.end() || w > it->second.second) mejor[cp] = std::make_pair((int)g, w);
        }
    }
    // invertir el mapa: por grupo, la lista de control-points que le quedan (todos con peso 1)
    std::vector<std::vector<int> > cpsPorGrupo(m->vertexGroups.size());
    for (std::map<int,std::pair<int,float> >::iterator it = mejor.begin(); it != mejor.end(); ++it)
        cpsPorGrupo[it->second.first].push_back(it->first);
    for (size_t g = 0; g < m->vertexGroups.size(); g++){
        VertexGroup* vg = m->vertexGroups[g];
        vg->verts = cpsPorGrupo[g];
        vg->pesos.assign(cpsPorGrupo[g].size(), 1.0f);
    }
    m->skinGeomVersion++;       // invalida el CSR de skinning -> se reconstruye con 1 influencia por vertex
    m->lastSkinFrame = -999999; // forzar re-skin en el proximo frame
}

// key de POSICION cuantizada (3 ints) para el dedup de CalcularBordes: NUMERICA en vez de std::string -> sin una
// alloc de string por vertice (mortal en el N95 al importar mallas grandes; mismo resultado). C++03: el struct va a
// nivel de archivo (los tipos LOCALES no se pueden usar como argumento de template).
namespace { struct PosKeyBordes {
    int a, b, c;
    bool operator<(const PosKeyBordes& o) const {
        if (a != o.a) return a < o.a;
        if (b != o.b) return b < o.b;
        return c < o.c;
    }
}; }

void Mesh::CalcularBordes(bool invalidarEdit, bool reagruparPosRep) {
    skinGeomVersion++; // la geometria de render se regenero -> invalida el cache CSR de skinning (aunque nV no cambie)
    edges.clear();
    bordesBuf.clear();
    vertsAgrupados = 0;
    overlayLcache = -1.0f; // la geometria cambio -> rehacer los overlays de normales
    if (!vertex || vertexSize <= 0) { posRep.clear(); return; }
    const int nV = vertexSize;
    // representante de cada vertice por posicion: el menor indice entre los verts coincidentes. HASH por posicion
    // CUANTIZADA a 1e-4 -> O(n log n). (Antes era un doble loop O(n^2) que con una esfera de 45+ segmentos TRABABA
    // la app al regenerar -en el redo panel el slider regenera muchas veces-: Mismo resultado, mucho mas
    // rapido: a 64x64 son ~4k verts -> 4k*log4k en vez de 16M comparaciones.) Se cachea en posRep (overlay O(n)/frame).
    // reagruparPosRep=false: CONSERVA el posRep existente (mismo nV) -> NO re-suelda por posicion. Lo usa el confirm
    // de un move para que el snap pueda dejar verts encimados SIN fundirlos (el Auto Merge, opt-in, hace la soldadura).
    if (reagruparPosRep || (int)posRep.size() != nV) {
        posRep.assign(nV, 0);
        std::map<PosKeyBordes,int> posMap;
        for (int i = 0; i < nV; i++) {
            PosKeyBordes k;
            k.a = (int)floorf(vertex[i*3]   * 10000.0f + 0.5f); // cuantiza a 1e-4 (coincidentes -> misma celda)
            k.b = (int)floorf(vertex[i*3+1] * 10000.0f + 0.5f);
            k.c = (int)floorf(vertex[i*3+2] * 10000.0f + 0.5f);
            std::map<PosKeyBordes,int>::iterator it = posMap.find(k);
            if (it != posMap.end()) posRep[i] = it->second; // ya vimos esta posicion -> su representante (menor indice)
            else { posRep[i] = i; posMap[k] = i; }           // 1ra vez -> este vert es el representante
        }
    }
    // cantidad de posiciones unicas + CENTRO GEOMETRICO (promedio de las posiciones
    // unicas = los grupos de vertice). El foco/pivot lo usan en vez del origen.
    float cgx = 0, cgy = 0, cgz = 0;
    for (int i = 0; i < nV; i++) if (posRep[i] == i) {
        vertsAgrupados++;
        cgx += vertex[i*3]; cgy += vertex[i*3+1]; cgz += vertex[i*3+2];
    }
    if (vertsAgrupados > 0)
        centroGeom = Vector3(cgx / vertsAgrupados, cgy / vertsAgrupados, cgz / vertsAgrupados);
    else
        centroGeom = Vector3(0, 0, 0);

    // RADIO del bounding LOCAL alrededor de centroGeom (la distancia mas lejana). Lo usa el foco/encuadre
    // (tecla '.') para ajustar el zoom a lo que se ve. Se recalcula solo cuando cambia la geometria.
    float rg2 = 0.0f;
    for (int i = 0; i < nV; i++) {
        float dx = vertex[i*3] - centroGeom.x, dy = vertex[i*3+1] - centroGeom.y, dz = vertex[i*3+2] - centroGeom.z;
        float d2 = dx*dx + dy*dy + dz*dz; if (d2 > rg2) rg2 = d2;
    }
    radioGeom = sqrtf(rg2);

    // la geometria cambio -> la malla de EDICION (si existia) queda invalida.
    // (salvo el confirm del transform de malla: solo movio vertices, conserva el
    // edit y su seleccion -> invalidarEdit=false)
    if (invalidarEdit) InvalidarEdit();
    if (faces3d.empty() && looseEdges.empty()) return; // sin caras ni bordes sueltos
    std::vector<int>& rep = posRep;
    std::set<std::pair<int,int> > unicos;
    for (size_t f = 0; f < faces3d.size(); f++) {
        const std::vector<int>& idx = faces3d[f].idx;
        int m = (int)idx.size();
        for (int k = 0; k < m; k++) {
            int a = idx[k], b = idx[(k+1)%m];
            if (a<0||a>=nV||b<0||b>=nV) continue;
            int ra = rep[a], rb = rep[b];
            if (ra == rb) continue;
            if (ra > rb) { int t=ra; ra=rb; rb=t; }
            unicos.insert(std::make_pair(ra, rb));
        }
    }
    // BORDES SUELTOS (del borrado en edit): dedup por POSICION igual que los de cara
    for (size_t e = 0; e + 1 < looseEdges.size(); e += 2) {
        int a = looseEdges[e], b = looseEdges[e+1];
        if (a<0||a>=nV||b<0||b>=nV) continue;
        int ra = rep[a], rb = rep[b];
        if (ra == rb) continue;
        if (ra > rb) { int t=ra; ra=rb; rb=t; }
        unicos.insert(std::make_pair(ra, rb));
    }
    edges.reserve(unicos.size()*2);
    for (std::set<std::pair<int,int> >::iterator it = unicos.begin(); it != unicos.end(); ++it) {
        edges.push_back(it->first);
        edges.push_back(it->second);
    }
    // precalcular el buffer de lineas del contorno (asi NO se rehace por frame)
    bordesBuf.reserve(edges.size()*3);
    for (size_t e = 0; e + 1 < edges.size(); e += 2) {
        int a = edges[e], b = edges[e+1];
        bordesBuf.push_back(vertex[a*3]); bordesBuf.push_back(vertex[a*3+1]); bordesBuf.push_back(vertex[a*3+2]);
        bordesBuf.push_back(vertex[b*3]); bordesBuf.push_back(vertex[b*3+1]); bordesBuf.push_back(vertex[b*3+2]);
    }
}


// optimiza el cache de vertices de CADA mesh part del render (Forsyth). Antes en el Core; se
// movio aca con su helper OptimizarCacheVertices. Lo llaman ReagruparMeshParts + el importador.
void Mesh::OptimizarCacheRender() {
    if (!faces || facesSize < 192) return; // <64 triangulos: entran enteros al cache, no hace falta
    for (size_t gi=0; gi<materialsGroup.size(); gi++) {
        int start = materialsGroup[gi].startDrawn, cnt = materialsGroup[gi].indicesDrawnCount;
        int gt = cnt/3;
        if (gt < 64 || start < 0 || start+cnt > facesSize) continue;
        float acmr; { std::vector<int> f; int m=0; for(int i=0;i<cnt;i++){ int v=(int)faces[start+i]; bool h=false; for(size_t j=0;j<f.size();j++) if(f[j]==v){h=true;break;} if(!h){ m++; f.insert(f.begin(),v); if((int)f.size()>16) f.pop_back(); } } acmr=(float)m/gt; }
        if (acmr > 1.0f) OptimizarCacheVertices(&faces[start], gt, vertexSize);
    }
}

// ===================================================
// NORMALES / SHADING (recalculo de normales, corner-normals, sharp edges). Antes en el
// Core (Mesh.cpp); es modelado del editor. Siguen como Mesh:: (decl en Mesh.h).
// ===================================================
// recalcula el array 'normals' (GLbyte por vertice GPU) desde la geometria actual:
// normal Newell de cada cara logica (faces3d) acumulada en sus corners; si la malla
// es SMOOTH se promedia agrupando por POSICION (posRep). Lo llama el transform de
// malla al CONFIRMAR (mover vertices invalida las normales viejas).
void Mesh::RecalcularNormales() {
    if (!vertex || !normals || vertexSize <= 0) return;
    const int nV = vertexSize;
    std::vector<float> acc(nV*3, 0.0f);
    if (!faces3d.empty()) {
        for (size_t fi = 0; fi < faces3d.size(); fi++) {
            const std::vector<int>& ring = faces3d[fi].idx;
            int m = (int)ring.size(); if (m < 3) continue;
            float nx=0,ny=0,nz=0;
            for (int k=0;k<m;k++){ // Newell (robusto para ngones)
                GLfloat* a=&vertex[ring[k]*3]; GLfloat* b=&vertex[ring[(k+1)%m]*3];
                nx+=(a[1]-b[1])*(a[2]+b[2]); ny+=(a[2]-b[2])*(a[0]+b[0]); nz+=(a[0]-b[0])*(a[1]+b[1]);
            }
            float ln=sqrtf(nx*nx+ny*ny+nz*nz); if (ln<1e-6f) continue; nx/=ln; ny/=ln; nz/=ln;
            for (int k=0;k<m;k++){ int gi=ring[k]; if (gi<0||gi>=nV) continue;
                acc[gi*3]+=nx; acc[gi*3+1]+=ny; acc[gi*3+2]+=nz; }
        }
    } else {
        int nTri = facesSize/3;
        for (int t=0;t<nTri;t++){
            int i0=faces[t*3], i1=faces[t*3+1], i2=faces[t*3+2];
            GLfloat* p0=&vertex[i0*3]; GLfloat* p1=&vertex[i1*3]; GLfloat* p2=&vertex[i2*3];
            float ax=p1[0]-p0[0], ay=p1[1]-p0[1], az=p1[2]-p0[2];
            float bx=p2[0]-p0[0], by=p2[1]-p0[1], bz=p2[2]-p0[2];
            float nx=ay*bz-az*by, ny=az*bx-ax*bz, nz=ax*by-ay*bx;
            float ln=sqrtf(nx*nx+ny*ny+nz*nz); if (ln<1e-6f) continue; nx/=ln; ny/=ln; nz/=ln;
            acc[i0*3]+=nx; acc[i0*3+1]+=ny; acc[i0*3+2]+=nz;
            acc[i1*3]+=nx; acc[i1*3+1]+=ny; acc[i1*3+2]+=nz;
            acc[i2*3]+=nx; acc[i2*3+1]+=ny; acc[i2*3+2]+=nz;
        }
    }
    // SMOOTH: promedio por POSICION (los vertices del mismo lugar comparten normal)
    if (meshSmooth && (int)posRep.size() == nV) {
        std::vector<float> sum(nV*3, 0.0f);
        for (int i=0;i<nV;i++){ int r=posRep[i]; sum[r*3]+=acc[i*3]; sum[r*3+1]+=acc[i*3+1]; sum[r*3+2]+=acc[i*3+2]; }
        for (int i=0;i<nV;i++){ int r=posRep[i]; acc[i*3]=sum[r*3]; acc[i*3+1]=sum[r*3+1]; acc[i*3+2]=sum[r*3+2]; }
    }
    for (int i=0;i<nV;i++){
        float nx=acc[i*3], ny=acc[i*3+1], nz=acc[i*3+2];
        float ln=sqrtf(nx*nx+ny*ny+nz*nz);
        if (ln<1e-6f){ normals[i*3]=0; normals[i*3+1]=127; normals[i*3+2]=0; continue; }
        nx/=ln; ny/=ln; nz/=ln;
        normals[i*3]=(GLbyte)(nx*127.0f); normals[i*3+1]=(GLbyte)(ny*127.0f); normals[i*3+2]=(GLbyte)(nz*127.0f);
    }
    SincronizarCornerNormal(); // -> capa autoritativa por corner (la que lee GenerarRender)
}

// copia normals[] (render) -> cornerNormal (autoritativa POR CORNER). La llaman las ops
// que ESCRIBEN normales (RecalcularNormales, shade) para que GenerarRender las vea.
void Mesh::SincronizarCornerNormal() {
    if (!normals || faces3d.empty()) return;
    int nC = ContarCorners(); cornerNormal.resize((size_t)nC*3);
    int L=0; for (size_t f=0;f<faces3d.size();f++) for (size_t c=0;c<faces3d[f].idx.size();c++){
        int gv=faces3d[f].idx[c]; if (gv<0||gv>=vertexSize){L++;continue;}
        cornerNormal[L*3]=normals[gv*3]; cornerNormal[L*3+1]=normals[gv*3+1]; cornerNormal[L*3+2]=normals[gv*3+2]; L++; }
}

// FLAT: cada corner toma la normal Newell de SU cara (no se acarrea/promedia). Asi en
// GenerarRender dos corners en la misma posicion pero de caras distintas tienen normales
// distintas -> NO se mergean -> cada cara queda con su propia normal (shading plano). Lo
// que hacia smooth al extruir un cubo: las paredes heredaban la normal de la cara original.
void Mesh::CornerNormalPorCara() {
    if (!vertex || faces3d.empty()) return;
    int nC = ContarCorners();
    cornerNormal.resize((size_t)nC*3);
    int L = 0;
    for (size_t f=0; f<faces3d.size(); f++) {
        const std::vector<int>& ring = faces3d[f].idx;
        int m = (int)ring.size();
        float nx=0,ny=0,nz=0;
        for (int k=0;k<m;k++){ // Newell (robusto para ngones / quads no planos)
            int ia=ring[k], ib=ring[(k+1)%m];
            if (ia<0||ia>=vertexSize||ib<0||ib>=vertexSize) continue;
            GLfloat* a=&vertex[ia*3]; GLfloat* b=&vertex[ib*3];
            nx+=(a[1]-b[1])*(a[2]+b[2]); ny+=(a[2]-b[2])*(a[0]+b[0]); nz+=(a[0]-b[0])*(a[1]+b[1]);
        }
        float ln=sqrtf(nx*nx+ny*ny+nz*nz);
        GLbyte bx=0,by=127,bz=0; // degenerada -> +Y de fallback
        if (ln>=1e-6f){ nx/=ln;ny/=ln;nz/=ln; bx=(GLbyte)(nx*127.0f); by=(GLbyte)(ny*127.0f); bz=(GLbyte)(nz*127.0f); }
        for (int k=0;k<m;k++){ if (L*3+2 < (int)cornerNormal.size()){ cornerNormal[L*3]=bx; cornerNormal[L*3+1]=by; cornerNormal[L*3+2]=bz; } L++; }
    }
}

// clave de 12 bytes de una posicion (los 3 floats crudos). Dos corners en el MISMO
// lugar tienen exactamente los mismos bytes -> mismo grupo de posicion.
static std::string PosKey12(const float* p){ char b[12]; memcpy(b, p, 12); return std::string(b, 12); }

std::string Mesh::SharpEdgeKey(const float* a, const float* b){
    std::string ka = PosKey12(a), kb = PosKey12(b);
    return (ka < kb) ? (ka + kb) : (kb + ka); // ordenado: el borde no tiene direccion
}

// union-find (raices de los grupos de suavizado, indexado por corner)
static int UFFind(std::vector<int>& uf, int x){ while (uf[x] != x){ uf[x] = uf[uf[x]]; x = uf[x]; } return x; }
static void UFUnion(std::vector<int>& uf, int a, int b){ int ra=UFFind(uf,a), rb=UFFind(uf,b); if (ra!=rb) uf[ra]=rb; }

// SMOOTH con bordes SHARP: la normal de cada corner = promedio de las caras de su GRUPO
// DE SUAVIZADO. El grupo se arma uniendo, alrededor de cada vertice, las caras que
// comparten un borde NO sharp; un borde sharp (o toda la malla flat) CORTA el grupo ->
// esa cara queda con su propia normal (plana). Asi un cilindro shade-smooth con el aro
// de las tapas marcado sharp: lados suaves + tapas planas + arista filosa.
void Mesh::CornerNormalConSharp(){
    if (!vertex || faces3d.empty()) return;
    int nC = ContarCorners();
    if (nC <= 0) return;
    cornerNormal.resize((size_t)nC * 3);

    // 1) normal Newell por cara + de que cara es cada corner + si la cara es FLAT (per-cara: -1 hereda meshSmooth)
    std::vector<float> fn(faces3d.size() * 3, 0.0f);
    std::vector<int> cornerFace(nC, -1);
    std::vector<char> faceFlat(faces3d.size(), 0);
    for (size_t f=0; f<faces3d.size(); f++){ int sm=faces3d[f].smooth; faceFlat[f] = (sm==0) || (sm<0 && !meshSmooth) ? 1 : 0; }
    std::vector<int> uf(nC); for (int i = 0; i < nC; i++) uf[i] = i;
    int L = 0;
    for (size_t f = 0; f < faces3d.size(); f++) {
        const std::vector<int>& ring = faces3d[f].idx;
        int m = (int)ring.size();
        float nx=0, ny=0, nz=0;
        for (int k = 0; k < m; k++) {
            int ia = ring[k], ib = ring[(k+1)%m];
            if (ia<0||ia>=vertexSize||ib<0||ib>=vertexSize) continue;
            GLfloat* a=&vertex[ia*3]; GLfloat* b=&vertex[ib*3];
            nx += (a[1]-b[1])*(a[2]+b[2]); ny += (a[2]-b[2])*(a[0]+b[0]); nz += (a[0]-b[0])*(a[1]+b[1]);
        }
        float ln = sqrtf(nx*nx+ny*ny+nz*nz); if (ln>1e-6f){ nx/=ln; ny/=ln; nz/=ln; } else { nx=0; ny=1; nz=0; }
        fn[f*3]=nx; fn[f*3+1]=ny; fn[f*3+2]=nz;
        for (int k = 0; k < m; k++){ if (L < nC) cornerFace[L]=(int)f; L++; }
    }

    // 2) por cada borde compartido (key por posicion) unir los corners de las 2 caras en
    //    cada extremo, SALVO que sea sharp (o la malla flat = todo sharp -> nunca une)
    std::map<std::string, std::pair<int,int> > visto; // key -> (cornerEnBajo, cornerEnAlto) de la 1er cara
    L = 0;
    for (size_t f = 0; f < faces3d.size(); f++) {
        const std::vector<int>& ring = faces3d[f].idx;
        int m = (int)ring.size();
        for (int k = 0; k < m; k++) {
            int ia = ring[k], ib = ring[(k+1)%m];
            int ca = L + k, cb = L + ((k+1)%m);     // corners en ia, ib
            if (ia<0||ia>=vertexSize||ib<0||ib>=vertexSize) continue;
            std::string pkA = PosKey12(&vertex[ia*3]);
            std::string pkB = PosKey12(&vertex[ib*3]);
            if (pkA == pkB) continue;                // arista degenerada
            int loC, hiC; std::string ek;
            if (pkA < pkB){ loC=ca; hiC=cb; ek = pkA + pkB; }
            else          { loC=cb; hiC=ca; ek = pkB + pkA; }
            std::map<std::string, std::pair<int,int> >::iterator it = visto.find(ek);
            if (it == visto.end()) { visto[ek] = std::make_pair(loC, hiC); }
            else {
                // borde sharp si esta marcado, o si CUALQUIERA de las 2 caras del borde es flat (per-cara) -> corta el grupo
                int fOtra = cornerFace[it->second.first];
                bool sharp = (sharpEdges.find(ek) != sharpEdges.end()) || faceFlat[f] || (fOtra>=0 && faceFlat[fOtra]);
                if (!sharp) { UFUnion(uf, it->second.first, loC); UFUnion(uf, it->second.second, hiC); }
            }
        }
        L += m;
    }

    // 3) acumular la normal de cada grupo (raiz) y escribirla en sus corners
    std::vector<float> acc(nC * 3, 0.0f);
    for (int c = 0; c < nC; c++){ int r = UFFind(uf, c); int f = cornerFace[c]; if (f < 0) continue;
        acc[r*3]+=fn[f*3]; acc[r*3+1]+=fn[f*3+1]; acc[r*3+2]+=fn[f*3+2]; }
    for (int c = 0; c < nC; c++){ int r = UFFind(uf, c);
        float nx=acc[r*3], ny=acc[r*3+1], nz=acc[r*3+2];
        float ln=sqrtf(nx*nx+ny*ny+nz*nz);
        GLbyte bx=0, by=127, bz=0;
        if (ln>1e-6f){ nx/=ln; ny/=ln; nz/=ln; bx=(GLbyte)(nx*127.0f); by=(GLbyte)(ny*127.0f); bz=(GLbyte)(nz*127.0f); }
        cornerNormal[c*3]=bx; cornerNormal[c*3+1]=by; cornerNormal[c*3+2]=bz; }
}

// ============================================================================
//  CAPAS (UV / vertex color / vertex group) + MESH PARTS — movido de Mesh.cpp
//  (re-split core/editor). Concepto del EDITOR; siguen siendo miembros de Mesh
//  (declarados en Mesh.h). El Core ya no las define.
// ============================================================================
// libera las capas persistentes (uv/color/groups). Lo llaman el destructor y Regenerar.
void Mesh::LiberarCapas(bool incluirGrupos) {
    for (size_t i=0;i<uvMaps.size();i++)       delete uvMaps[i];
    for (size_t i=0;i<colorLayers.size();i++)  delete colorLayers[i];
    uvMaps.clear(); colorLayers.clear();
    uvMapActivo = -1; colorActivo = -1;
    // vertex groups: NO son una "capa" derivable del render (PoblarCapas NO los rehace). Solo se borran cuando se
    // pide (reset total / geometria nueva / destructor). En un JOIN/APPLY hay que PRESERVARLOS (incluirGrupos=false),
    // sino se perdia el skinning y la malla colapsaba (bug del Ctrl+J sobre mallas skinneadas).
    if (incluirGrupos) {
        for (size_t i=0;i<vertexGroups.size();i++) delete vertexGroups[i];
        vertexGroups.clear(); grupoActivo = -1;
    }
    cornerNormal.clear(); // se rehace en PoblarCapas desde normals[]
}

// cantidad de CORNERS (esquinas de cara): a esto se indexan las capas por-corner.
int Mesh::ContarCorners() const {
    int n=0; for (size_t f=0;f<faces3d.size();f++) n += (int)faces3d[f].idx.size(); return n;
}

// crea las capas iniciales desde los arrays de render (uv[]/vertexColor[]) si no hay
// ninguna. Por CORNER (orden de faces3d): corner L=(cara f, esquina c) -> vert GPU
// faces3d[f].idx[c]. AUTO-HEAL: si la capa activa quedo de otro tamano (la geometria
// cambio en una edit-op), rehace las capas desde el render (la capa activa = lo que las
// ops preservaron en uv[]/vertexColor[], asi sus datos sobreviven). Idempotente si no.
void Mesh::PoblarCapas() {
    int nC = ContarCorners();
    if (nC <= 0) return;
    bool stale =
        (!uvMaps.empty() && uvMapActivo>=0 && uvMapActivo<(int)uvMaps.size() &&
         (int)uvMaps[uvMapActivo]->uv.size() != nC*2) ||
        (!colorLayers.empty() && colorActivo>=0 && colorActivo<(int)colorLayers.size() &&
         !colorLayers[colorActivo]->porVertice && (int)colorLayers[colorActivo]->color.size() != nC*4);
    if (stale) LiberarCapas(); // la geometria cambio -> rehacer (FASE 2b: remapear las NO-activas)
    if (uvMaps.empty() && uv) {
        UVMap* mp = new UVMap("UVMap"); mp->uv.resize((size_t)nC*2);
        int L=0; for (size_t f=0;f<faces3d.size();f++) for (size_t c=0;c<faces3d[f].idx.size();c++){
            int gv=faces3d[f].idx[c]; mp->uv[L*2]=uv[gv*2]; mp->uv[L*2+1]=uv[gv*2+1]; L++; }
        uvMaps.push_back(mp); uvMapActivo = 0;
    }
    if (colorLayers.empty() && vertexColor) {
        ColorLayer* cl = new ColorLayer("Col"); cl->color.resize((size_t)nC*4);
        int L=0; for (size_t f=0;f<faces3d.size();f++) for (size_t c=0;c<faces3d[f].idx.size();c++){
            int gv=faces3d[f].idx[c]; for(int q=0;q<4;q++) cl->color[L*4+q]=vertexColor[gv*4+q]; L++; }
        colorLayers.push_back(cl); colorActivo = 0;
    }
    // NORMAL autoritativa por corner: si no esta o quedo de otro tamaño (op que no la
    // acarreo) la rehago desde normals[]. Las ops que SI la acarrean dejan el size OK.
    if ((int)cornerNormal.size() != nC*3 && normals) {
        cornerNormal.resize((size_t)nC*3);
        int L=0; for (size_t f=0;f<faces3d.size();f++) for (size_t c=0;c<faces3d[f].idx.size();c++){
            int gv=faces3d[f].idx[c]; cornerNormal[L*3]=normals[gv*3]; cornerNormal[L*3+1]=normals[gv*3+1]; cornerNormal[L*3+2]=normals[gv*3+2]; L++; }
    }
}

// EL RENDER SE DERIVA DE LA CAPA ACTIVA: copia la UVMap activa + la ColorLayer activa
// (por corner) a uv[]/vertexColor[] (por GPU vert). Lo llama el editor al cambiar de capa
// activa o al editar una capa. (Sin re-split de verts: las capas de PoblarCapas/duplicadas
// comparten el seam del render; cambiar seams es FASE 4 con GenerarRender.)
void Mesh::AplicarCapasAlRender() {
    int nC = ContarCorners();
    if (nC <= 0) return;
    UVMap* um = (uvMapActivo>=0 && uvMapActivo<(int)uvMaps.size()) ? uvMaps[uvMapActivo] : NULL;
    ColorLayer* cl = (colorActivo>=0 && colorActivo<(int)colorLayers.size()) ? colorLayers[colorActivo] : NULL;
    if (um && (int)um->uv.size() != nC*2) um = NULL;       // guard de tamano
    if (cl && (int)cl->color.size() != nC*4) cl = NULL;    // la capa SIEMPRE guarda por-corner (nC*4)
    bool tCN = (normals && (int)cornerNormal.size() == nC*3); // normal autoritativa -> render
    if (!um && !cl && !tCN) return;
    int L = 0;
    for (size_t f=0;f<faces3d.size();f++) for (size_t c=0;c<faces3d[f].idx.size();c++) {
        int gv = faces3d[f].idx[c];
        if (um && uv)          { uv[gv*2]=um->uv[L*2]; uv[gv*2+1]=um->uv[L*2+1]; }
        if (cl && vertexColor) { for (int q=0;q<4;q++) vertexColor[gv*4+q]=cl->color[L*4+q]; }
        if (tCN)               { normals[gv*3]=cornerNormal[L*3]; normals[gv*3+1]=cornerNormal[L*3+1]; normals[gv*3+2]=cornerNormal[L*3+2]; }
        L++;
    }
    // capa por-VERTICE: el color se COLAPSA por grupo de posicion (todos los verts coincidentes
    // toman el color del primero) -> 1 color por vertice. (La capa sigue guardando por-corner: el
    // toggle es no-destructivo, volver a por-corner re-bakea.) El export auto-detecta per-vertice.
    if (cl && cl->porVertice && vertexColor) {
        std::map<std::string,int> rep;
        for (int i = 0; i < vertexSize; i++) {
            std::string k((const char*)&vertex[i*3], 12);
            std::map<std::string,int>::iterator it = rep.find(k);
            if (it == rep.end()) rep[k] = i;
            else { int r = it->second; for (int q = 0; q < 4; q++) vertexColor[i*4+q] = vertexColor[r*4+q]; }
        }
    }
}

// duplican la capa ACTIVA y dejan la copia como activa (boton "+" de la pestaña Vertices).
// ===== Las DOS unicas puertas al render (abstraccion: las ops NO tocan vertex[]/faces3d a
//       mano -> integridad). RefrescarRender = edicion IN-PLACE rapida (no cambia topologia);
//       GenerarRender = REBUILD completo (cambio de topologia). =====


// los rangos de cada mesh part (materialsGroup[g].startDrawn/indicesDrawnCount). Antes GenerarRender
// colapsaba TODO a un grupo (perdia los mesh parts al editar). NO toca vertices/uv/normales/color ni
// el edit mesh: por eso Assign/Delete pueden usarla SIN un GenerarRender completo (la edicion sigue
// viva). Se preservan las entradas de materialsGroup (nombre+material); las vacias quedan con count 0.
void Mesh::ReagruparMeshParts() {
    int nGrupos = (int)materialsGroup.size();
    { int mx = 0; for (size_t f=0;f<faces3d.size();f++){ int m=faces3d[f].mat; if (m<0){ faces3d[f].mat=0; m=0; } if (m>mx) mx=m; }
      if (mx+1 > nGrupos) nGrupos = mx+1; }
    if (nGrupos < 1) nGrupos = 1;
    while ((int)materialsGroup.size() < nGrupos){ MaterialGroup g; materialsGroup.push_back(g); } // pad (nombre default)
    std::vector<MeshIndex> tris; // MeshIndex: en PC los indices pueden pasar 65535 (no truncar a 16 bits)
    for (int gi=0; gi<(int)materialsGroup.size(); gi++){
        materialsGroup[gi].startDrawn = (int)tris.size();
        for (size_t f=0;f<faces3d.size();f++){ if (faces3d[f].mat != gi) continue;
            const std::vector<int>& idx=faces3d[f].idx;
            for (size_t k=1;k+1<idx.size();k++){ tris.push_back((MeshIndex)idx[0]);tris.push_back((MeshIndex)idx[k]);tris.push_back((MeshIndex)idx[k+1]); } }
        materialsGroup[gi].indicesDrawnCount = (int)tris.size() - materialsGroup[gi].startDrawn;
    }
    facesSize=(int)tris.size(); delete[] faces; faces=new MeshIndex[facesSize>0?facesSize:1];
    for (int i=0;i<facesSize;i++) faces[i]=tris[i];
    OptimizarCacheRender(); // reordena los triangulos de cada mesh part para el cache de vertices (no cambia la geometria)
    // el index buffer (faces[]) cambio de agrupamiento -> invalidar el VBO de indices: sino en OBJECT MODE (que
    // dibuja del vboIdx) queda el agrupamiento VIEJO y la reasignacion de mesh part "no se veia" (se veia en Edit,
    // que no usa VBO). Bumpear skinGeomVersion fuerza el re-SubirVBO en el proximo draw.
    skinGeomVersion++;
}
