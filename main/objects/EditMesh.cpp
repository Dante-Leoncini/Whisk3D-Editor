#include "EditMesh.h"
#include <math.h>                // sqrtf (loop select geometrico)
#include "objects/Mesh.h"        // lee la geometria de la malla de render (vertex/edges/faces3d)
#include "objects/Objects.h"     // EditSelectMode + SelVertex/SelEdge/SelFace
#include "w3dGraphics.h" // abstraccion de graficos (sin GL directo)
#include "WhiskUI/theme/colores.h"  // ListaColores(Ubyte) + ColorID
#include "ui/W3dColors.h"     // W3dColoresUbyte: colores del editor (piso, normales)
#include <map>
#include <utility>
#include <string>
#include <cstring>

EditMesh::EditMesh() { src = NULL; activeIdx = -1; }

// arma la malla de edicion desde la malla de render: vertices unicos, aristas,
// CARAS (poligono + sus aristas + centro). La seleccion arranca en TODO.
void EditMesh::Construir(Mesh* m) {
    src = m;
    editVerts.clear(); pos.clear(); lineIdx.clear(); linePos.clear();
    vertSel.clear(); edgeSel.clear(); faceSel.clear();
    faces.clear(); faceSrc.clear(); faceEdges.clear(); faceCenter.clear(); faceTriDyn.clear();
    activeIdx = -1;
    if (!m || !m->vertex || m->vertexSize <= 0) { Recolorear(); return; }
    const int nV = m->vertexSize;
    const bool hayRep = ((int)m->posRep.size() == nV);

    // 1 vertice por POSICION unica (representante: posRep[i]==i)
    for (int i = 0; i < nV; i++) if (!hayRep || m->posRep[i] == i) editVerts.push_back(i);
    pos.reserve(editVerts.size() * 3);
    for (size_t k = 0; k < editVerts.size(); k++) {
        int gi = editVerts[k];
        pos.push_back(m->vertex[gi*3]); pos.push_back(m->vertex[gi*3+1]); pos.push_back(m->vertex[gi*3+2]);
    }
    // mapa: indice GPU del representante -> indice editable
    std::vector<int> repToEdit(nV, -1);
    for (size_t k = 0; k < editVerts.size(); k++) repToEdit[editVerts[k]] = (int)k;
    // de un GPU index cualquiera al editable (via su representante)
    // (lambda no en C++03; helper inline abajo con repToEdit + posRep)

    // ARISTAS unicas (m->edges son pares de representantes GPU) -> indices editables
    std::map<std::pair<int,int>, int> edgeMap; // (min,max editable) -> indice de arista
    for (size_t e = 0; e + 1 < m->edges.size(); e += 2) {
        int a = (m->edges[e]   >= 0 && m->edges[e]   < nV) ? repToEdit[m->edges[e]]   : -1;
        int b = (m->edges[e+1] >= 0 && m->edges[e+1] < nV) ? repToEdit[m->edges[e+1]] : -1;
        if (a < 0 || b < 0) continue;
        int idx = (int)(lineIdx.size() / 2);
        lineIdx.push_back((GLushort)a); lineIdx.push_back((GLushort)b);
        int lo = a < b ? a : b, hi = a < b ? b : a;
        edgeMap[std::make_pair(lo, hi)] = idx;
    }
    // lineas EXPANDIDAS (posiciones; el color lo arma Recolorear)
    linePos.reserve(lineIdx.size() * 3);
    for (size_t e = 0; e + 1 < lineIdx.size(); e += 2) {
        int a = lineIdx[e], b = lineIdx[e+1];
        linePos.push_back(pos[a*3]); linePos.push_back(pos[a*3+1]); linePos.push_back(pos[a*3+2]);
        linePos.push_back(pos[b*3]); linePos.push_back(pos[b*3+1]); linePos.push_back(pos[b*3+2]);
    }

    // CARAS: 1 por cara logica (faces3d). Cada corner GPU -> editable (via posRep).
    for (size_t f = 0; f < m->faces3d.size(); f++) {
        const std::vector<int>& ring = m->faces3d[f].idx;
        std::vector<int> poly;
        for (size_t c = 0; c < ring.size(); c++) {
            int gi = ring[c]; if (gi < 0 || gi >= nV) continue;
            int rep = hayRep ? m->posRep[gi] : gi;
            int ei = (rep >= 0 && rep < nV) ? repToEdit[rep] : -1;
            if (ei < 0) continue;
            if (!poly.empty() && poly.back() == ei) continue; // saltar duplicado consecutivo
            poly.push_back(ei);
        }
        if (poly.size() >= 2 && poly.front() == poly.back()) poly.pop_back();
        if (poly.size() < 3) continue;
        // aristas de la cara (busca el indice de arista de cada par consecutivo)
        std::vector<int> fe;
        bool ok = true;
        for (size_t c = 0; c < poly.size(); c++) {
            int a = poly[c], b = poly[(c+1) % poly.size()];
            int lo = a < b ? a : b, hi = a < b ? b : a;
            std::map<std::pair<int,int>,int>::iterator it = edgeMap.find(std::make_pair(lo, hi));
            if (it == edgeMap.end()) { ok = false; break; }
            fe.push_back(it->second);
        }
        if (!ok) continue;
        // centro de la cara
        float cx=0, cy=0, cz=0;
        for (size_t c = 0; c < poly.size(); c++) { int e=poly[c]; cx+=pos[e*3]; cy+=pos[e*3+1]; cz+=pos[e*3+2]; }
        float inv = 1.0f / (float)poly.size();
        faces.push_back(poly);
        faceSrc.push_back((int)f); // de que cara logica (faces3d) salio -> lo usa el borrado
        faceEdges.push_back(fe);
        faceCenter.push_back(cx*inv); faceCenter.push_back(cy*inv); faceCenter.push_back(cz*inv);
    }

    // seleccion default: TODO
    vertSel.assign(editVerts.size(), 1);
    edgeSel.assign(lineIdx.size() / 2, 1);
    faceSel.assign(faces.size(), 1);
    Recolorear();
}

// empuja pos[] (editable, autoritativa al editar) al render IN-PLACE (sin realloc).
void EditMesh::EmpujarPosiciones(){
    if (!src || !src->vertex) return;
    Mesh* m = src; const int nV = m->vertexSize;
    const bool hayRep = ((int)m->posRep.size() == nV);
    std::vector<int> repToEdit(nV, -1); // rep GPU -> editVert k
    for (size_t k=0;k<editVerts.size();k++) if (editVerts[k]>=0 && editVerts[k]<nV) repToEdit[editVerts[k]]=(int)k;
    for (int gv=0; gv<nV; gv++){
        int rep = hayRep ? m->posRep[gv] : gv;
        int k = (rep>=0 && rep<nV) ? repToEdit[rep] : -1;
        if (k>=0 && k*3+2 < (int)pos.size()){
            m->vertex[gv*3]=pos[k*3]; m->vertex[gv*3+1]=pos[k*3+1]; m->vertex[gv*3+2]=pos[k*3+2];
        }
    }
}


// true si la cara f esta "seleccionada" segun el MODO: face=faceSel; vertex=todos
// sus vertices sel; edge=todas sus aristas sel.
static bool CaraSel(int mode, size_t f,
                    const std::vector<std::vector<int> >& faces,
                    const std::vector<std::vector<int> >& faceEdges,
                    const std::vector<unsigned char>& vertSel,
                    const std::vector<unsigned char>& edgeSel,
                    const std::vector<unsigned char>& faceSel) {
    if (mode == SelFace) return f < faceSel.size() && faceSel[f];
    if (mode == SelVertex) {
        const std::vector<int>& p = faces[f];
        for (size_t c = 0; c < p.size(); c++) if (p[c] >= (int)vertSel.size() || !vertSel[p[c]]) return false;
        return !p.empty();
    }
    // edge
    const std::vector<int>& fe = faceEdges[f];
    for (size_t c = 0; c < fe.size(); c++) if (fe[c] >= (int)edgeSel.size() || !edgeSel[fe[c]]) return false;
    return !fe.empty();
}

// recolorea puntos, lineas, puntitos-de-cara y arma el fill de caras seleccionadas.
// La seleccion ACTIVA (activeIdx, ultimo elemento) va de BLANCO.
void EditMesh::Recolorear() {
    const int mode = EditSelectMode;
    const GLubyte* csel = ListaColoresUbyte[static_cast<int>(ColorID::accent)];
    const GLubyte* cuns = W3dColoresUbyte[W3dColor_LineaPiso];
    const GLubyte* cneg = ListaColoresUbyte[static_cast<int>(ColorID::negro)];
    const GLubyte* cbla = ListaColoresUbyte[static_cast<int>(ColorID::blanco)];
    const GLubyte* cmag = W3dColoresUbyte[W3dColor_normalCustom]; // seams (magenta)
    const bool hayCosturas = (src && !src->seamEdges.empty());
    const size_t N = editVerts.size();
    const size_t E = lineIdx.size() / 2;
    const size_t F = faces.size();
    col.assign(N * 4, 255);
    lineCol.assign(E * 2 * 4, 255);
    facePtCol.assign(F * 4, 255);
    faceTriDyn.clear();

    // modo edge: un vertice "iluminado" si alguna arista adyacente esta sel
    std::vector<unsigned char> vBright;
    if (mode == SelEdge) {
        vBright.assign(N, 0);
        for (size_t e = 0; e < E; e++)
            if (e < edgeSel.size() && edgeSel[e]) { vBright[lineIdx[e*2]] = 1; vBright[lineIdx[e*2+1]] = 1; }
    }

    // PUNTOS de vertice (solo se dibujan en modo vertex, pero igual se colorean)
    for (size_t k = 0; k < N; k++) {
        const GLubyte* c;
        if (mode == SelEdge)        c = vBright[k] ? csel : cuns;
        else if (mode == SelVertex) c = (k < vertSel.size() && vertSel[k]) ? csel : cuns;
        else                        c = cuns;
        col[k*4]=c[0]; col[k*4+1]=c[1]; col[k*4+2]=c[2]; col[k*4+3]=255;
    }
    // ACTIVO (modo vertex): el ultimo vertice seleccionado va de blanco
    if (mode == SelVertex && activeIdx >= 0 && activeIdx < (int)N) {
        col[activeIdx*4]=cbla[0]; col[activeIdx*4+1]=cbla[1]; col[activeIdx*4+2]=cbla[2]; col[activeIdx*4+3]=255;
    }

    // modo face: nivel de cada arista por las caras adyacentes (0 oscuro / 1 verde
    // si pertenece a una cara seleccionada / 2 blanco si pertenece a la cara activa)
    std::vector<unsigned char> edgeLevel;
    if (mode == SelFace) {
        edgeLevel.assign(E, 0);
        for (size_t f = 0; f < F; f++) {
            unsigned char lv = (activeIdx == (int)f) ? 2
                             : ((f < faceSel.size() && faceSel[f]) ? 1 : 0);
            if (lv == 0) continue;
            const std::vector<int>& fe = faceEdges[f];
            for (size_t c = 0; c < fe.size(); c++) {
                int ei = fe[c];
                if (ei >= 0 && ei < (int)E && edgeLevel[ei] < lv) edgeLevel[ei] = lv;
            }
        }
    }

    // LINEAS (expandidas)
    for (size_t e = 0; e < E; e++) {
        int a = lineIdx[e*2], b = lineIdx[e*2+1];
        const GLubyte *ca, *cb;
        if (mode == SelVertex) {
            ca = (a < (int)vertSel.size() && vertSel[a]) ? csel : cuns;
            cb = (b < (int)vertSel.size() && vertSel[b]) ? csel : cuns;
        } else if (mode == SelEdge) {
            const GLubyte* c = (e < edgeSel.size() && edgeSel[e]) ? csel : cuns;
            ca = cb = c;
            if (activeIdx == (int)e) ca = cb = cbla; // ACTIVO (modo edge): arista blanca
        } else { // face: borde verde si la cara esta sel, blanco si es la activa
            unsigned char lv = (e < edgeLevel.size()) ? edgeLevel[e] : 0;
            const GLubyte* c = (lv == 2) ? cbla : (lv == 1) ? csel : cuns;
            ca = cb = c;
        }
        // SEAM = MAGENTA: si el borde es costura UV y NO esta seleccionado (ca==cuns), lo pinta
        // magenta para que se vea donde se abre el unwrap. La seleccion (verde/blanco) lo pisa.
        if (hayCosturas && ca == cuns) {
            int gva = (a >= 0 && a < (int)editVerts.size()) ? editVerts[a] : -1;
            int gvb = (b >= 0 && b < (int)editVerts.size()) ? editVerts[b] : -1;
            if (gva >= 0 && gvb >= 0 && gva < src->vertexSize && gvb < src->vertexSize &&
                src->seamEdges.count(Mesh::SharpEdgeKey(&src->vertex[gva*3], &src->vertex[gvb*3]))) {
                ca = cb = cmag;
            }
        }
        GLubyte* la = &lineCol[(e*2)*4];   la[0]=ca[0]; la[1]=ca[1]; la[2]=ca[2]; la[3]=255;
        GLubyte* lb = &lineCol[(e*2+1)*4]; lb[0]=cb[0]; lb[1]=cb[1]; lb[2]=cb[2]; lb[3]=255;
    }

    // PUNTITOS de cara (modo face): negro=desel, verde=sel, blanco=activo
    for (size_t f = 0; f < F; f++) {
        const GLubyte* c = (f < faceSel.size() && faceSel[f]) ? csel : cneg;
        if (mode == SelFace && activeIdx == (int)f) c = cbla;
        facePtCol[f*4]=c[0]; facePtCol[f*4+1]=c[1]; facePtCol[f*4+2]=c[2]; facePtCol[f*4+3]=255;
    }

    // FILL: triangular (abanico) las caras "seleccionadas" segun el modo
    for (size_t f = 0; f < F; f++) {
        if (!CaraSel(mode, f, faces, faceEdges, vertSel, edgeSel, faceSel)) continue;
        const std::vector<int>& p = faces[f];
        for (size_t t = 1; t + 1 < p.size(); t++) {
            faceTriDyn.push_back((MeshIndex)p[0]);
            faceTriDyn.push_back((MeshIndex)p[t]);
            faceTriDyn.push_back((MeshIndex)p[t+1]);
        }
    }
}

// dibuja PRIMERO las caras seleccionadas (verde semitransparente 50%), DESPUES las
// lineas, y arriba los vertices (modo vertex) o los puntitos de cara (modo face).
void EditMesh::Render() {
    if (pos.empty()) return;
    namespace gfx = w3dEngine;
    const int mode = EditSelectMode;
    gfx::Disable(gfx::Lighting);
    gfx::Disable(gfx::Texture2D);
    gfx::DisableArray(gfx::NormalArray);
    gfx::DisableArray(gfx::TexCoordArray);
    // X-RAY: los bordes/vertices (incluso los de ATRAS de las caras) se dibujan SIEMPRE encima (sin z-test),
    // asi se pueden ver y seleccionar los que estaban ocultos. Se restaura al final.
    if (g_xray) gfx::Disable(gfx::DepthTest);

    // 1) CARAS seleccionadas: verde 50%, blend, sin escribir z (para no tapar lo de arriba)
    if (!faceTriDyn.empty()) {
        const float* ac = ListaColores[static_cast<int>(ColorID::accent)];
        gfx::DisableArray(gfx::ColorArray);
        gfx::Enable(gfx::Blend); gfx::BlendAlpha();
        gfx::DepthMask(false);
        gfx::Color4f(ac[0], ac[1], ac[2], 0.5f);
        gfx::VertexPointer3f(0, &pos[0]);
        gfx::DrawTriangles((int)faceTriDyn.size(), &faceTriDyn[0]);
        gfx::DepthMask(true);
        gfx::Disable(gfx::Blend);
    }

    gfx::SmoothShading(true); // degradado de color a lo largo de la linea
    gfx::EnableArray(gfx::ColorArray);

    // 2) LINEAS (glDrawArrays GL_LINES, expandidas, color por vertice)
    if (!linePos.empty()) {
        gfx::VertexPointer3f(0, &linePos[0]);
        gfx::ColorPointer4ub(&lineCol[0]);
        gfx::LineWidth(1.0f);
        gfx::DrawLines((int)(linePos.size() / 3));
    }

    // 3) PUNTOS: vertices (modo vertex) o puntitos en el centro de cada cara (modo
    //    face). En modo edge no se dibujan puntos.
    if (mode == SelVertex) {
        gfx::VertexPointer3f(0, &pos[0]);
        gfx::ColorPointer4ub(&col[0]);
        gfx::PointSize(6.0f);
        gfx::DrawPoints((int)(pos.size() / 3));
        gfx::PointSize(1.0f);
    } else if (mode == SelFace && !faceCenter.empty()) {
        gfx::VertexPointer3f(0, &faceCenter[0]);
        gfx::ColorPointer4ub(&facePtCol[0]);
        gfx::PointSize(6.0f);
        gfx::DrawPoints((int)(faceCenter.size() / 3));
        gfx::PointSize(1.0f);
    }

    if (g_xray) gfx::Enable(gfx::DepthTest); // restaurar el z-test tras el overlay X-Ray
    gfx::DisableArray(gfx::ColorArray);
    if (src && src->vertex) gfx::VertexPointer3f(0, src->vertex);
    gfx::Invalidate();
}

// re-lee pos[] desde src->vertex (los vertices se movieron) y rearma las lineas
// expandidas (linePos) + los centros de cara (faceCenter). NO toca color ni
// seleccion: el transform de malla en vivo lo llama tras cada apply.
// rearma los buffers del OVERLAY (lineas + centros de cara) desde pos[] -> el overlay se
// deriva de las posiciones EDITABLES (autoritativas), sin leer el render. Lo usan las dos
// puertas (RefrescarRender via EmpujarPosiciones, y SincronizarPos).
void EditMesh::RefrescarOverlay() {
    // lineas expandidas (2 vertices por arista, igual que en Construir)
    size_t li = 0;
    for (size_t e = 0; e + 1 < lineIdx.size(); e += 2) {
        int a = lineIdx[e], b = lineIdx[e+1];
        if (li + 6 > linePos.size()) break;
        linePos[li++] = pos[a*3]; linePos[li++] = pos[a*3+1]; linePos[li++] = pos[a*3+2];
        linePos[li++] = pos[b*3]; linePos[li++] = pos[b*3+1]; linePos[li++] = pos[b*3+2];
    }
    // centro de cada cara
    for (size_t f = 0; f < faces.size(); f++) {
        const std::vector<int>& p = faces[f];
        if (p.empty() || f*3+2 >= faceCenter.size()) continue;
        float cx=0, cy=0, cz=0;
        for (size_t c = 0; c < p.size(); c++) { int e=p[c]; cx+=pos[e*3]; cy+=pos[e*3+1]; cz+=pos[e*3+2]; }
        float inv = 1.0f / (float)p.size();
        faceCenter[f*3] = cx*inv; faceCenter[f*3+1] = cy*inv; faceCenter[f*3+2] = cz*inv;
    }
}

// el RENDER cambio por fuera: re-lee vertex[] -> pos[] y rearma el overlay. (Para cuando
// algo toco el render directo; la edicion normal va por pos[] -> EmpujarPosiciones.)
void EditMesh::SincronizarPos() {
    if (!src || !src->vertex) return;
    const int nV = src->vertexSize;
    for (size_t k = 0; k < editVerts.size(); k++) {
        int gi = editVerts[k]; if (gi < 0 || gi >= nV) continue;
        pos[k*3]   = src->vertex[gi*3];
        pos[k*3+1] = src->vertex[gi*3+1];
        pos[k*3+2] = src->vertex[gi*3+2];
    }
    RefrescarOverlay();
}

void EditMesh::SeleccionarTodo(bool sel) {
    unsigned char v = sel ? 1 : 0;
    if (EditSelectMode == SelFace)      faceSel.assign(faces.size(), v);
    else if (EditSelectMode == SelEdge) edgeSel.assign(lineIdx.size() / 2, v);
    else                                vertSel.assign(editVerts.size(), v);
    activeIdx = -1; // "todo"/"nada" no deja un activo
    Recolorear();
}

void EditMesh::Invertir() {
    if (EditSelectMode == SelFace) {
        for (size_t f = 0; f < faceSel.size(); f++) faceSel[f] = faceSel[f] ? 0 : 1;
    } else if (EditSelectMode == SelEdge) {
        for (size_t e = 0; e < edgeSel.size(); e++) edgeSel[e] = edgeSel[e] ? 0 : 1;
    } else {
        for (size_t k = 0; k < vertSel.size(); k++) vertSel[k] = vertSel[k] ? 0 : 1;
    }
    activeIdx = -1;
    Recolorear();
}

void EditMesh::TogglearVert(int k, bool soloEste) {
    if (soloEste) vertSel.assign(editVerts.size(), 0);
    if (k >= 0 && k < (int)vertSel.size()) vertSel[k] = soloEste ? 1 : (vertSel[k] ? 0 : 1);
    activeIdx = (k >= 0 && k < (int)vertSel.size() && vertSel[k]) ? k : -1; // activo = el clickeado si quedo sel
    Recolorear();
}

void EditMesh::TogglearEdge(int e, bool soloEste) {
    if (soloEste) edgeSel.assign(lineIdx.size() / 2, 0);
    if (e >= 0 && e < (int)edgeSel.size()) edgeSel[e] = soloEste ? 1 : (edgeSel[e] ? 0 : 1);
    activeIdx = (e >= 0 && e < (int)edgeSel.size() && edgeSel[e]) ? e : -1;
    Recolorear();
}

void EditMesh::TogglearFace(int f, bool soloEste) {
    if (soloEste) faceSel.assign(faces.size(), 0);
    if (f >= 0 && f < (int)faceSel.size()) faceSel[f] = soloEste ? 1 : (faceSel[f] ? 0 : 1);
    activeIdx = (f >= 0 && f < (int)faceSel.size() && faceSel[f]) ? f : -1;
    Recolorear();
}

// L: Select Linked — selecciona la ISLA (componente conexa por aristas) que contiene
// al elemento k del 'modo' dado. soloEste reemplaza la seleccion; si no, la agrega.
// Ej: 2 cubos separados -> el mouse sobre uno + L selecciona ese cubo entero.
void EditMesh::SeleccionarLinked(int k, int modo, bool soloEste) {
    const int nV = (int)editVerts.size();
    const int nE = (int)(lineIdx.size() / 2);
    if (nV <= 0) return;

    // semilla: los verts del elemento clickeado
    std::vector<unsigned char> enIsla(nV, 0);
    std::vector<int> pila;
    if (modo == SelFace) {
        if (k < 0 || k >= (int)faces.size()) return;
        const std::vector<int>& p = faces[k];
        for (size_t c=0;c<p.size();c++) if (p[c]>=0 && p[c]<nV && !enIsla[p[c]]){ enIsla[p[c]]=1; pila.push_back(p[c]); }
    } else if (modo == SelEdge) {
        if (k < 0 || k >= nE) return;
        int a=lineIdx[k*2], b=lineIdx[k*2+1];
        if (a>=0&&a<nV){ enIsla[a]=1; pila.push_back(a); }
        if (b>=0&&b<nV){ enIsla[b]=1; pila.push_back(b); }
    } else {
        if (k < 0 || k >= nV) return;
        enIsla[k]=1; pila.push_back(k);
    }
    if (pila.empty()) return;

    // adyacencia vert<->vert por aristas + flood fill
    std::vector<std::vector<int> > adj(nV);
    for (int eg=0; eg<nE; eg++){ int a=lineIdx[eg*2], b=lineIdx[eg*2+1];
        if (a>=0&&a<nV&&b>=0&&b<nV){ adj[a].push_back(b); adj[b].push_back(a); } }
    while (!pila.empty()){ int v=pila.back(); pila.pop_back();
        for (size_t j=0;j<adj[v].size();j++){ int w=adj[v][j]; if (!enIsla[w]){ enIsla[w]=1; pila.push_back(w); } } }

    if (soloEste){ vertSel.assign(nV,0); edgeSel.assign(nE,0); faceSel.assign(faces.size(),0); }

    if (modo == SelFace){
        for (size_t f=0;f<faces.size();f++){ const std::vector<int>& p=faces[f]; if (p.empty()) continue;
            bool all=true; for (size_t c=0;c<p.size();c++) if (p[c]<0||p[c]>=nV||!enIsla[p[c]]){all=false;break;}
            if (all) faceSel[f]=1; }
    } else if (modo == SelEdge){
        for (int eg=0;eg<nE;eg++){ int a=lineIdx[eg*2], b=lineIdx[eg*2+1];
            if (a>=0&&a<nV&&b>=0&&b<nV && enIsla[a] && enIsla[b]) edgeSel[eg]=1; }
    } else {
        for (int v=0;v<nV;v++) if (enIsla[v]) vertSel[v]=1;
    }
    activeIdx = -1;
    Recolorear();
}

// Shift+Alt+Click (modo CARA): Loop Select de caras a partir del BORDE clickeado.
// Camina el loop cruzando, en cada quad, el borde OPUESTO al de entrada -> el loop va
// PERPENDICULAR al borde clickeado (ej cilindro: click en un borde vertical = el anillo;
// en uno horizontal = a lo largo). Solo propaga por QUADS (tri/ngones cortan el loop).
void EditMesh::SeleccionarLoopFace(int edgeK, bool soloEste) {
    const int nE = (int)(lineIdx.size() / 2);
    const int nF = (int)faces.size();
    if (edgeK < 0 || edgeK >= nE || nF == 0) return;

    // borde -> caras que lo contienen (normalmente 1 o 2)
    std::vector<std::vector<int> > edgeFaces(nE);
    for (int f=0; f<nF; f++){ const std::vector<int>& fe=faceEdges[f];
        for (size_t i=0;i<fe.size();i++){ int eg=fe[i]; if (eg>=0&&eg<nE) edgeFaces[eg].push_back(f); } }

    std::vector<unsigned char> enLoop(nF, 0);
    // arrancar de cada cara adyacente al borde clickeado (cubre las 2 direcciones)
    for (size_t s=0; s<edgeFaces[edgeK].size(); s++){
        int cur = edgeFaces[edgeK][s];
        int entering = edgeK;
        while (cur >= 0 && cur < nF && !enLoop[cur]) {
            enLoop[cur] = 1;                              // selecciono esta cara
            const std::vector<int>& fe = faceEdges[cur];
            if ((int)fe.size() != 4) break;              // no-quad: el loop corta aca
            int idx = -1; for (int i=0;i<4;i++) if (fe[i]==entering){ idx=i; break; }
            if (idx < 0) break;
            int opp = fe[(idx+2)%4];                      // borde opuesto
            int next = -1;                                // la OTRA cara del borde opuesto
            for (size_t j=0;j<edgeFaces[opp].size();j++){ int ff=edgeFaces[opp][j]; if (ff!=cur){ next=ff; break; } }
            entering = opp; cur = next;
        }
    }
    if (soloEste) faceSel.assign(nF, 0);
    for (int f=0; f<nF; f++) if (enLoop[f]) faceSel[f]=1;
    activeIdx = -1;
    Recolorear();
}

// EDGE LOOP: el anillo que SIGUE la linea del borde (ej. todo el contorno/rim de un cilindro).
// Metodo GEOMETRICO: en cada vertice continua al borde mas "derecho" (el que mejor mantiene la
// direccion de avance, max alineacion). Asi funciona con cualquier valencia: el grid (cruza al
// opuesto), el rim del cilindro (sigue la curva tangente, aunque el cap sea un ngon de valencia 3).
// Corta si el mejor giro es > ~60 grados (polo). Camina en las 2 direcciones desde edgeK.
void EditMesh::SeleccionarLoopEdge(int edgeK, bool soloEste) {
    const int nE = (int)(lineIdx.size() / 2);
    if (edgeK < 0 || edgeK >= nE) return;
    const int nV = (int)(pos.size() / 3);
    std::vector<std::vector<int> > vertEdges(nV);                    // vertice -> bordes
    for (int e=0;e<nE;e++){ int a=lineIdx[e*2], b=lineIdx[e*2+1];
        if (a>=0&&a<nV) vertEdges[a].push_back(e); if (b>=0&&b<nV) vertEdges[b].push_back(e); }
    // borde -> caras (desde faceEdges): para el loop TOPOLOGICO. En un vertice VALENCIA-4 (cuadricula, ej. el cubo
    // dividido o el lateral de un cilindro) el loop sigue por el borde OPUESTO = el unico que NO comparte NINGUNA
    // cara con el borde de entrada (el "del medio" del quad). Esto es lo correcto a la Blender; el geometrico solo
    // es fallback cuando eso es ambiguo (rim de cilindro = valencia 3, polos, bordes de malla abierta).
    std::vector<std::vector<int> > edgeFaces(nE);
    for (size_t f=0; f<faceEdges.size(); f++)
        for (size_t c=0; c<faceEdges[f].size(); c++){ int e=faceEdges[f][c]; if (e>=0&&e<nE) edgeFaces[e].push_back((int)f); }

    std::vector<unsigned char> enLoop(nE, 0);
    enLoop[edgeK] = 1;
    for (int dir=0; dir<2; dir++){
        int cur = edgeK;
        int V = (dir==0) ? lineIdx[edgeK*2] : lineIdx[edgeK*2+1];    // avanzamos hacia V
        while (true){
            if (V<0 || V>=nV) break;
            // 1) TOPOLOGICO: el borde en V que NO comparte cara con 'cur'. Si hay EXACTAMENTE 1 -> es el opuesto.
            int next = -1, nOpuestos = 0;
            for (size_t i=0;i<vertEdges[V].size();i++){
                int e2=vertEdges[V][i]; if (e2==cur) continue;
                bool comparte=false;
                for (size_t a=0; a<edgeFaces[cur].size() && !comparte; a++)
                    for (size_t b=0; b<edgeFaces[e2].size(); b++) if (edgeFaces[cur][a]==edgeFaces[e2][b]){ comparte=true; break; }
                if (!comparte){ next=e2; nOpuestos++; }
            }
            // 2) FALLBACK GEOMETRICO (topologico ambiguo: 0 opuestos = rim/borde abierto, o >1 = polo/non-manifold):
            //    seguir el borde mas DERECHO (max alineacion con la entrada). Umbral permisivo (~120 grados) para que
            //    NO corte en el rim de un cilindro low-poly (donde cada borde gira 90 grados o mas).
            if (nOpuestos != 1) {
                int ca=lineIdx[cur*2], cb=lineIdx[cur*2+1];
                int A = (ca==V) ? cb : ca;
                float ix=pos[V*3]-pos[A*3], iy=pos[V*3+1]-pos[A*3+1], iz=pos[V*3+2]-pos[A*3+2];
                float il=sqrtf(ix*ix+iy*iy+iz*iz); if (il<1e-9f) break; ix/=il; iy/=il; iz/=il;
                // umbral ~72 grados: el rim de un cilindro (giro 45 a 60 grados) SIGUE, pero un giro de 90 grados
                // (ej. el loop VERTICAL que quiere doblar hacia el rim) NO -> ahi corta, como debe.
                next=-1; float best=0.3f;
                for (size_t i=0;i<vertEdges[V].size();i++){
                    int e2=vertEdges[V][i]; if (e2==cur) continue;
                    int ea=lineIdx[e2*2], eb=lineIdx[e2*2+1];
                    int B = (ea==V) ? eb : ea;                       // extremo de e2 opuesto a V
                    float ox=pos[B*3]-pos[V*3], oy=pos[B*3+1]-pos[V*3+1], oz=pos[B*3+2]-pos[V*3+2];
                    float ol=sqrtf(ox*ox+oy*oy+oz*oz); if (ol<1e-9f) continue; ox/=ol; oy/=ol; oz/=ol;
                    float d = ix*ox+iy*oy+iz*oz;
                    if (d>best){ best=d; next=e2; }
                }
            }
            if (next<0 || enLoop[next]) break;                       // sin continuacion / cerro el anillo
            enLoop[next] = 1;
            int na=lineIdx[next*2], nb=lineIdx[next*2+1];
            V = (na==V) ? nb : na;                                    // el otro vertice de 'next'
            cur = next;
        }
    }
    if (soloEste) edgeSel.assign(nE, 0);
    for (int e=0;e<nE;e++) if (enLoop[e]) edgeSel[e]=1;
    activeIdx = -1;
    Recolorear();
}

// MODO VERTICE: selecciona los VERTS del edge loop que pasa por edgeK. Usa edgeSel como intermedio pero lo arma con
// SOLO este loop (reemplaza) y lo LIMPIA al final. CLAVE (bug Dante): en modo vertice el edgeSel no se muestra ni lo
// limpia el deseleccionar verts -> si quedaba seteado, los Shift+Alt+Click iban ACUMULANDO loops viejos. Aca cada
// llamada arranca con el edgeSel limpio. soloEste=true reemplaza la seleccion de verts; false la agrega.
void EditMesh::SeleccionarLoopEdgeVerts(int edgeK, bool soloEste) {
    SeleccionarLoopEdge(edgeK, true);                              // edgeSel = SOLO este loop (no acumula)
    if (soloEste) for (size_t k=0;k<vertSel.size();k++) vertSel[k]=0;
    for (size_t i=0; i*2+1<lineIdx.size() && i<edgeSel.size(); i++) if (edgeSel[i]) {
        int a=lineIdx[i*2], b=lineIdx[i*2+1];
        if (a>=0&&a<(int)vertSel.size()) vertSel[a]=1;
        if (b>=0&&b<(int)vertSel.size()) vertSel[b]=1; }
    for (size_t i=0;i<edgeSel.size();i++) edgeSel[i]=0;            // limpia el intermedio (no debe persistir en modo vertice)
    activeIdx = -1;
    Recolorear();
}

// EDGE RING: los travesanos PERPENDICULARES. Camina por quads igual que el loop de CARAS,
// pero en vez de seleccionar las caras junta los bordes OPUESTOS que va cruzando.
void EditMesh::SeleccionarRingEdge(int edgeK, bool soloEste) {
    const int nE = (int)(lineIdx.size() / 2);
    const int nF = (int)faces.size();
    if (edgeK < 0 || edgeK >= nE || nF == 0) return;
    std::vector<std::vector<int> > edgeFaces(nE);
    for (int f=0; f<nF; f++){ const std::vector<int>& fe=faceEdges[f];
        for (size_t i=0;i<fe.size();i++){ int eg=fe[i]; if (eg>=0&&eg<nE) edgeFaces[eg].push_back(f); } }

    std::vector<unsigned char> enRing(nE, 0);
    enRing[edgeK] = 1;
    for (size_t s=0; s<edgeFaces[edgeK].size(); s++){               // las 2 direcciones
        int cur = edgeFaces[edgeK][s];
        int entering = edgeK;
        while (cur >= 0 && cur < nF){
            const std::vector<int>& fe = faceEdges[cur];
            if ((int)fe.size() != 4) break;                         // no-quad: corta
            int idx=-1; for (int i=0;i<4;i++) if (fe[i]==entering){ idx=i; break; }
            if (idx < 0) break;
            int opp = fe[(idx+2)%4];                                // borde opuesto (el travesano)
            if (opp<0 || opp>=nE || enRing[opp]) break;
            enRing[opp] = 1;
            int next = -1;
            for (size_t j=0;j<edgeFaces[opp].size();j++){ int ff=edgeFaces[opp][j]; if (ff!=cur){ next=ff; break; } }
            entering = opp; cur = next;
        }
    }
    if (soloEste) edgeSel.assign(nE, 0);
    for (int e=0;e<nE;e++) if (enRing[e]) edgeSel[e]=1;
    activeIdx = -1;
    Recolorear();
}

// BFS sin pesos (distancia TOPOLOGICA en saltos) desde src sobre un grafo de adyacencia.
static void BFSDist(const std::vector<std::vector<int> >& adj, int src, std::vector<int>& dist){
    int N = (int)adj.size();
    dist.assign(N, -1);
    if (src < 0 || src >= N) return;
    std::vector<int> cola; cola.push_back(src); dist[src] = 0;
    size_t head = 0;
    while (head < cola.size()){
        int u = cola[head++];
        for (size_t i=0;i<adj[u].size();i++){ int v = adj[u][i];
            if (v>=0 && v<N && dist[v]<0){ dist[v] = dist[u]+1; cola.push_back(v); } }
    }
}

// PICK SHORTEST PATH (Ctrl+Click). Arma el grafo de adyacencia del modo activo, hace BFS desde el
// activo (A) y desde toIdx (B), y selecciona: un solo caminito (backtrack por dist decreciente) o,
// con fillRegion, TODA la region (todo e con distA[e]+distB[e]==total -> en un grid, el rectangulo).
void EditMesh::SeleccionarShortestPath(int toIdx, bool fillRegion) {
    const int mode = EditSelectMode;
    const int nV = (int)(pos.size() / 3);
    const int nE = (int)(lineIdx.size() / 2);
    const int nF = (int)faces.size();
    int N = 0; unsigned char* sel = NULL;
    std::vector<std::vector<int> > adj;

    if (mode == SelVertex){
        N = nV; if (toIdx<0 || toIdx>=N) return;
        adj.assign(nV, std::vector<int>());                       // vertices unidos por aristas
        for (int e=0;e<nE;e++){ int a=lineIdx[e*2], b=lineIdx[e*2+1];
            if (a>=0&&a<nV&&b>=0&&b<nV){ adj[a].push_back(b); adj[b].push_back(a); } }
        if (!vertSel.empty()) sel = &vertSel[0];
    } else if (mode == SelEdge){
        N = nE; if (toIdx<0 || toIdx>=N) return;
        adj.assign(nE, std::vector<int>());                       // aristas adyacentes si comparten vertice
        std::vector<std::vector<int> > vertEdges(nV);
        for (int e=0;e<nE;e++){ int a=lineIdx[e*2], b=lineIdx[e*2+1];
            if (a>=0&&a<nV) vertEdges[a].push_back(e); if (b>=0&&b<nV) vertEdges[b].push_back(e); }
        for (int v=0;v<nV;v++){ const std::vector<int>& ve=vertEdges[v];
            for (size_t i=0;i<ve.size();i++) for (size_t j=i+1;j<ve.size();j++){ adj[ve[i]].push_back(ve[j]); adj[ve[j]].push_back(ve[i]); } }
        if (!edgeSel.empty()) sel = &edgeSel[0];
    } else { // SelFace
        N = nF; if (toIdx<0 || toIdx>=N) return;
        adj.assign(nF, std::vector<int>());                       // caras adyacentes si comparten arista
        std::vector<std::vector<int> > edgeFaces(nE);
        for (int f=0;f<nF;f++){ const std::vector<int>& fe=faceEdges[f];
            for (size_t i=0;i<fe.size();i++){ int eg=fe[i]; if (eg>=0&&eg<nE) edgeFaces[eg].push_back(f); } }
        for (int e=0;e<nE;e++){ const std::vector<int>& ef=edgeFaces[e];
            for (size_t i=0;i<ef.size();i++) for (size_t j=i+1;j<ef.size();j++){ adj[ef[i]].push_back(ef[j]); adj[ef[j]].push_back(ef[i]); } }
        if (!faceSel.empty()) sel = &faceSel[0];
    }
    if (!sel || N<=0 || toIdx>=N) return;

    int from = activeIdx;
    if (from<0 || from>=N){ sel[toIdx]=1; activeIdx=toIdx; Recolorear(); return; } // sin activo: solo el clickeado
    std::vector<int> dA, dB;
    BFSDist(adj, from, dA);
    BFSDist(adj, toIdx, dB);
    if (dA[toIdx] < 0){ sel[toIdx]=1; activeIdx=toIdx; Recolorear(); return; }     // no conexos: solo el clickeado
    const int total = dA[toIdx];
    if (fillRegion){
        for (int e=0;e<N;e++) if (dA[e]>=0 && dB[e]>=0 && dA[e]+dB[e]==total) sel[e]=1;
    } else {
        int cur = toIdx;
        while (cur != from){ sel[cur]=1; int nx=-1;
            for (size_t i=0;i<adj[cur].size();i++){ int w=adj[cur][i]; if (w>=0&&w<N && dA[w]==dA[cur]-1){ nx=w; break; } }
            if (nx<0) break; cur=nx; }
        sel[from]=1;
    }
    activeIdx = toIdx;
    Recolorear();
}

// centro LOCAL de los vertices INVOLUCRADOS en la seleccion del modo activo (un
// vertice compartido cuenta una sola vez). false si no hay nada seleccionado.
bool EditMesh::CentroSeleccion(float& cx, float& cy, float& cz) const {
    const int mode = EditSelectMode;
    const size_t N = editVerts.size();
    if (N == 0) return false;
    std::vector<unsigned char> inv(N, 0);
    if (mode == SelVertex) {
        for (size_t k = 0; k < N; k++) if (k < vertSel.size() && vertSel[k]) inv[k] = 1;
    } else if (mode == SelEdge) {
        const size_t E = lineIdx.size() / 2;
        for (size_t e = 0; e < E; e++) if (e < edgeSel.size() && edgeSel[e]) {
            inv[lineIdx[e*2]] = 1; inv[lineIdx[e*2+1]] = 1;
        }
    } else { // face
        for (size_t f = 0; f < faces.size(); f++) if (f < faceSel.size() && faceSel[f]) {
            const std::vector<int>& p = faces[f];
            for (size_t c = 0; c < p.size(); c++) if (p[c] >= 0 && p[c] < (int)N) inv[p[c]] = 1;
        }
    }
    int cnt = 0; float sx = 0, sy = 0, sz = 0;
    for (size_t k = 0; k < N; k++) if (inv[k]) { sx += pos[k*3]; sy += pos[k*3+1]; sz += pos[k*3+2]; cnt++; }
    if (cnt == 0) return false;
    cx = sx / cnt; cy = sy / cnt; cz = sz / cnt;
    return true;
}

bool EditMesh::CentroRadioSeleccion(float& cx, float& cy, float& cz, float& rLocal) const {
    if (!CentroSeleccion(cx, cy, cz)) return false; // mismo centro (centroide de lo seleccionado)
    // 2da pasada: la distancia mas lejana al centro entre los MISMOS vertices seleccionados
    const int mode = EditSelectMode;
    const size_t N = editVerts.size();
    std::vector<unsigned char> inv(N, 0);
    if (mode == SelVertex) {
        for (size_t k = 0; k < N; k++) if (k < vertSel.size() && vertSel[k]) inv[k] = 1;
    } else if (mode == SelEdge) {
        const size_t E = lineIdx.size() / 2;
        for (size_t e = 0; e < E; e++) if (e < edgeSel.size() && edgeSel[e]) { inv[lineIdx[e*2]] = 1; inv[lineIdx[e*2+1]] = 1; }
    } else {
        for (size_t f = 0; f < faces.size(); f++) if (f < faceSel.size() && faceSel[f]) {
            const std::vector<int>& p = faces[f];
            for (size_t c = 0; c < p.size(); c++) if (p[c] >= 0 && p[c] < (int)N) inv[p[c]] = 1;
        }
    }
    float r2 = 0.0f;
    for (size_t k = 0; k < N; k++) if (inv[k]) {
        float dx = pos[k*3] - cx, dy = pos[k*3+1] - cy, dz = pos[k*3+2] - cz;
        float d2 = dx*dx + dy*dy + dz*dz; if (d2 > r2) r2 = d2;
    }
    rLocal = sqrtf(r2);
    return true;
}
