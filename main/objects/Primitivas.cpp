#include "Primitivas.h"
#include "W3dLang.h"   // los nombres por defecto nacen en el idioma del usuario
#include "objects/Mesh.h" // Mesh, MeshType, MaterialGroup, MaterialDefecto
#include "variables.h"     // cursor3D (posicion del cursor 3D del editor)
#include <math.h>          // sqrtf/sinf/cosf (generacion de esferas/conos en Regenerar)

// Definiciones reales
const GLushort PlaneTriangles[PlaneFacesSize] = {
    1,0,3,
    1,3,2
};

const GLshort PlaneVertices[PlaneVertexSize] = {
    -1, 0,  1, 
    -1, 0, -1,
    1,  0, -1,
    1,  0,  1
};

const GLbyte PlaneUV[PlaneUvSize] = {
    127,127,
    -128,127,
    -128,-128,
    127,-128
};

const GLushort PlaneBordes[PlaneEdgesSize] = {
    0, 1,
    1, 2,
    2, 3,
    3, 0
};

const GLbyte CuboNormals[CuboVertexSize] = {
    0,0,127, 0,0,127, 0,0,127, 0,0,127,
    127,0,0, 127,0,0, 127,0,0, 127,0,0,
    0,127,0, 0,127,0, 0,127,0, 0,127,0,
    0,-128,0, 0,-128,0, 0,-128,0, 0,-128,0,
    -128,0,0, -128,0,0, -128,0,0, -128,0,0,
    0,0,-128, 0,0,-128, 0,0,-128, 0,0,-128
};

const GLushort CuboTriangles[CuboFacesSize] = {
     1,0,3, 1,3,2,
     5,4,7, 5,7,6,
     9,8,11, 9,11,10,
     13,12,15, 13,15,14,
     17,16,19, 17,19,18,
     21,22,23, 21,23,20
};

const GLfloat CuboVertices[CuboVertexSize] = {
    -1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f,  1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,
    1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,
    1.0f,  1.0f, -1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f, -1.0f,  1.0f,
    1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,
    1.0f,  1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f
};

const GLushort CuboBordes[CuboEdgesSize] = {
    0, 1, 1, 2, 2, 3, 3, 0,
    1, 4, 2, 5, 4, 5, 4, 6,
    6, 7, 7, 5, 0, 6, 3, 7
};

const GLbyte CuboUV[CuboUvSize] = {
    -128,-128, 127,-128, 127,127, -128,127,
    -128,-128, 127,-128, 127,127, -128,127,
    127,127, -128,127, -128,-128, 127,-128,
    127,127, -128,127, -128,-128, 127,-128,
    127,127, -128,127, -128,-128, 127,-128,
    127,-128, -128,-128, -128,127, 127,127
};

// crea un objeto Mesh de la primitiva 'type' en la posicion del cursor 3D, setea sus parametros
// (meshTipo/meshSize/...) y regenera la geometria. Antes vivia en el Core (Mesh.cpp).
Object* NewMesh(MeshType type, Object* parent, bool query){
    Mesh* mesh = new Mesh(parent, cursor3D.pos);
    if (type == MeshType::plane || type == MeshType::cube ||
        type == MeshType::circle || type == MeshType::UVsphere ||
        type == MeshType::cone || type == MeshType::cylinder){
        mesh->meshTipo = (int)type;
        if (type == MeshType::UVsphere){
            mesh->meshSize = 1.0f;   // radio
            mesh->meshVerts = 16;    // segments
            mesh->meshVerts2 = 8;    // rings
            mesh->meshSmooth = true; // las esferas arrancan suaves
        } else if (type == MeshType::cone){
            mesh->meshSize = 1.0f;   // radio1 (base)
            mesh->meshSize2 = 0.0f;  // radio2 (punta) -> puntiagudo
            mesh->meshDepth = 2.0f;  // altura
            mesh->meshVerts = 8;     // vertices
        } else if (type == MeshType::cylinder){
            mesh->meshSize = 1.0f;   // radio (unico)
            mesh->meshDepth = 2.0f;  // altura
            mesh->meshVerts = 8;     // vertices
        } else {
            mesh->meshSize = (type == MeshType::circle) ? 1.0f : 2.0f; // radio 1 / span 2
            mesh->meshVerts = 8;
        }
        mesh->Regenerar(); // arma vertex/normals/uv/faces + el grupo de material
        // el nombre nace en el IDIOMA del usuario ("Cubo", "Plano"). De ahi en mas es DATO: se guarda en el .w3d y
        // no vuelve a traducirse -- si no, abrir un archivo en otro idioma te renombraria los objetos.
        mesh->name = (type == MeshType::cube)     ? T("Cube")
                   : (type == MeshType::plane)    ? T("Plane")
                   : (type == MeshType::UVsphere) ? T("UVSphere")
                   : (type == MeshType::cone)     ? T("Cone")
                   : (type == MeshType::cylinder) ? T("Cylinder") : T("Circle");
    } else if (type == MeshType::vertice) {
        // 1 VERT SUELTO en el origen local (la malla ya se ubica en el cursor 3D). looseVerts lo preserva en el
        // rebuild (sino, al no estar en ninguna cara/arista, GenerarRender lo descartaria). Se ve en Edit Mode.
        mesh->vertexSize = 1;
        mesh->vertex      = new GLfloat[3]; mesh->vertex[0]=0.0f; mesh->vertex[1]=0.0f; mesh->vertex[2]=0.0f;
        mesh->normals     = new GLbyte[3];  mesh->normals[0]=0; mesh->normals[1]=127; mesh->normals[2]=0;
        mesh->uv          = new GLfloat[2]; mesh->uv[0]=0.0f; mesh->uv[1]=0.0f;
        mesh->vertexColor = new GLubyte[4]; mesh->vertexColor[0]=255; mesh->vertexColor[1]=255; mesh->vertexColor[2]=255; mesh->vertexColor[3]=255;
        mesh->looseVerts.push_back(0);
        MaterialGroup g; g.startDrawn = 0; g.material = MaterialDefecto;
        mesh->materialsGroup.push_back(g);
        mesh->CalcularBordes();
        mesh->name = T("Vertex");
    } else {
        // otros: malla vacia con su grupo (como antes)
        MaterialGroup g; g.startDrawn = 0; g.material = MaterialDefecto;
        mesh->materialsGroup.push_back(g);
    }
    return mesh;
};
// ===================================================
// Generacion de PRIMITIVAS (cubo/plano/circulo/esfera/cono/cilindro). Antes vivia en el
// Core (Mesh.cpp); es creacion de geometria del editor. Regenerar sigue como Mesh::.
// ===================================================
// --- helpers de generacion (acumuladores dinamicos) ---
// agrega un vertice (pos+normal+uv) y devuelve su indice
static int PushV(std::vector<GLfloat>& vp, std::vector<GLbyte>& vn, std::vector<GLfloat>& vu,
                 float x,float y,float z, float nx,float ny,float nz, float u,float v){
    int i = (int)(vp.size()/3);
    vp.push_back(x); vp.push_back(y); vp.push_back(z);
    vn.push_back((GLbyte)(nx*127.0f)); vn.push_back((GLbyte)(ny*127.0f)); vn.push_back((GLbyte)(nz*127.0f));
    vu.push_back(u); vu.push_back(v);
    return i;
}
// normal (Newell) de un poligono dado por m posiciones (robusto para ngones)
static void NewellPos(const float* pos, int m, float& nx, float& ny, float& nz){
    nx=ny=nz=0.0f;
    for (int k=0;k<m;k++){
        const float* a=&pos[k*3]; const float* b=&pos[((k+1)%m)*3];
        nx += (a[1]-b[1])*(a[2]+b[2]);
        ny += (a[2]-b[2])*(a[0]+b[0]);
        nz += (a[0]-b[0])*(a[1]+b[1]);
    }
    float ln=sqrtf(nx*nx+ny*ny+nz*nz); if (ln>1e-6f){ nx/=ln; ny/=ln; nz/=ln; }
}
// registra una cara logica (poligono) y la triangula en abanico desde ring[0]
static void AddFace(std::vector<GLushort>& tris, std::vector<MeshFace>& f3d, const std::vector<int>& ring){
    MeshFace mf; mf.idx = ring; f3d.push_back(mf);
    for (size_t k=1; k+1 < ring.size(); k++){
        tris.push_back((GLushort)ring[0]);
        tris.push_back((GLushort)ring[k]);
        tris.push_back((GLushort)ring[k+1]);
    }
}

// reconstruye la geometria de la primitiva desde meshTipo/meshSize/meshVerts.
// meshSize = span del cubo/plano o radio del circulo; meshVerts = vertices del
// circulo. La llama el panel "Add" al cambiar un parametro en vivo.
void Mesh::Regenerar(){
    Material* mat = materialsGroup.empty() ? MaterialDefecto : materialsGroup[0].material;
    delete[] vertex;      vertex = NULL;
    delete[] vertexColor; vertexColor = NULL;
    delete[] normals;     normals = NULL;
    delete[] uv;          uv = NULL;
    delete[] chromeExpPos; chromeExpPos = NULL;
    delete[] chromeExpUV;  chromeExpUV = NULL; chromeExpCount = 0; chromeUVValid = false;
    delete[] tangents; tangents = NULL; delete[] nmColors; nmColors = NULL; tangentsValid = false;
    delete[] faces;       faces = NULL;
    materialsGroup.clear();
    faces3d.clear();
    edges.clear();
    posRep.clear();
    bordesBuf.clear();
    normFaceBuf.clear(); normCustomBuf.clear(); normVertBuf.clear();
    overlayLcache = -1.0f;
    vertsAgrupados = 0;
    LiberarCapas();  // las capas (uv/color/groups) se rehacen para la geometria nueva
    InvalidarEdit(); // la malla de edicion se rehace on-demand

    int type = meshTipo;
    if (type == (int)MeshType::plane){
        // plano = UN quad (faces3d), horizontal (XZ), normal +Y
        float h = meshSize * 0.5f;
        std::vector<GLfloat> vp, vu; std::vector<GLbyte> vn; std::vector<GLushort> tris;
        static const float PC[4][3] = { {-1,0,1},{1,0,1},{1,0,-1},{-1,0,-1} }; // CCW -> +Y
        // V invertida (V=0 = ARRIBA de la imagen, como stb top-first + el importador OBJ que
        // hace 1-v): sino la textura sale dada vuelta verticalmente.
        static const float PUV[4][2] = { {0,1},{1,1},{1,0},{0,0} };
        std::vector<int> ring;
        for (int k=0;k<4;k++)
            ring.push_back(PushV(vp,vn,vu, PC[k][0]*h, 0.0f, PC[k][2]*h, 0,1,0, PUV[k][0],PUV[k][1]));
        AddFace(tris, faces3d, ring);
        vertexSize = (int)(vp.size()/3);
        vertex = new GLfloat[vp.size()]; for (size_t i=0;i<vp.size();i++) vertex[i]=vp[i];
        normals = new GLbyte[vn.size()]; for (size_t i=0;i<vn.size();i++) normals[i]=vn[i];
        uv = new GLfloat[vu.size()]; for (size_t i=0;i<vu.size();i++) uv[i]=vu[i];
        vertexColor = new GLubyte[vertexSize*4]; for (int i=0;i<vertexSize*4;i++) vertexColor[i]=255;
        facesSize = (int)tris.size();
        faces = new MeshIndex[facesSize>0?facesSize:1]; for (int i=0;i<facesSize;i++) faces[i]=tris[i];
    }
    else if (type == (int)MeshType::cube){
        // 6 caras QUAD (faces3d): cada una 1 cuadrado -> 1 normal cian. FLAT =
        // cada cara con sus 4 vertices y su normal; SMOOTH = 8 esquinas compartidas
        // con normal diagonal (cubo "redondeado").
        float h = meshSize * 0.5f;
        std::vector<GLfloat> vp, vu; std::vector<GLbyte> vn; std::vector<GLushort> tris;
        static const float CORN[8][3] = {
            {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
            {-1,-1, 1},{1,-1, 1},{1,1, 1},{-1,1, 1}
        };
        static const int CARA[6][4] = { // CCW visto desde afuera
            {4,5,6,7}, // +Z
            {1,0,3,2}, // -Z
            {5,1,2,6}, // +X
            {0,4,7,3}, // -X
            {7,6,2,3}, // +Y
            {0,1,5,4}  // -Y
        };
        // V invertida (V=0 = ARRIBA, como stb top-first + el importador OBJ 1-v): sino la textura del cubo sale dada vuelta verticalmente
        static const float UVQ[4][2] = { {0,1},{1,1},{1,0},{0,0} };
        if (meshSmooth){
            int idx[8];
            for (int c=0;c<8;c++){
                float nx=CORN[c][0], ny=CORN[c][1], nz=CORN[c][2];
                float ln=sqrtf(nx*nx+ny*ny+nz*nz); if(ln<1e-6f)ln=1.0f;
                idx[c]=PushV(vp,vn,vu, CORN[c][0]*h,CORN[c][1]*h,CORN[c][2]*h, nx/ln,ny/ln,nz/ln, 0.0f,0.0f);
            }
            for (int f=0;f<6;f++){
                std::vector<int> ring;
                for (int k=0;k<4;k++) ring.push_back(idx[CARA[f][k]]);
                AddFace(tris, faces3d, ring);
            }
        } else {
            for (int f=0;f<6;f++){
                float pos[12];
                for (int k=0;k<4;k++){
                    pos[k*3]   = CORN[CARA[f][k]][0]*h;
                    pos[k*3+1] = CORN[CARA[f][k]][1]*h;
                    pos[k*3+2] = CORN[CARA[f][k]][2]*h;
                }
                float fnx,fny,fnz; NewellPos(pos,4,fnx,fny,fnz);
                std::vector<int> ring;
                for (int k=0;k<4;k++)
                    ring.push_back(PushV(vp,vn,vu, pos[k*3],pos[k*3+1],pos[k*3+2], fnx,fny,fnz, UVQ[k][0],UVQ[k][1]));
                AddFace(tris, faces3d, ring);
            }
        }
        vertexSize = (int)(vp.size()/3);
        vertex = new GLfloat[vp.size()]; for (size_t i=0;i<vp.size();i++) vertex[i]=vp[i];
        normals = new GLbyte[vn.size()]; for (size_t i=0;i<vn.size();i++) normals[i]=vn[i];
        uv = new GLfloat[vu.size()]; for (size_t i=0;i<vu.size();i++) uv[i]=vu[i];
        vertexColor = new GLubyte[vertexSize*4]; for (int i=0;i<vertexSize*4;i++) vertexColor[i]=255;
        facesSize = (int)tris.size();
        faces = new MeshIndex[facesSize>0?facesSize:1]; for (int i=0;i<facesSize;i++) faces[i]=tris[i];
    }
    else if (type == (int)MeshType::circle){
        // disco = UN ngon (igual que las tapas del cono/cilindro): n vertices en
        // el borde, SIN vertice central, 1 cara -> 1 normal cian (+Y, mira arriba).
        int n = meshVerts; if (n < 3) n = 3;
        float R = meshSize; // radio
        std::vector<GLfloat> vp, vu; std::vector<GLbyte> vn; std::vector<GLushort> tris;
        std::vector<int> ring;
        for (int j = 0; j < n; j++){
            int jr = (n - j) % n; // orden invertido -> normal +Y (mira hacia arriba)
            float a = 2.0f * (float)M_PI * (float)jr / (float)n, ca = cosf(a), sa = sinf(a);
            ring.push_back(PushV(vp,vn,vu, R*ca, 0.0f, R*sa, 0,1,0, ca*0.5f+0.5f, sa*0.5f+0.5f));
        }
        AddFace(tris, faces3d, ring);
        vertexSize = (int)(vp.size()/3);
        vertex = new GLfloat[vp.size()]; for (size_t i=0;i<vp.size();i++) vertex[i]=vp[i];
        normals = new GLbyte[vn.size()]; for (size_t i=0;i<vn.size();i++) normals[i]=vn[i];
        uv = new GLfloat[vu.size()]; for (size_t i=0;i<vu.size();i++) uv[i]=vu[i];
        vertexColor = new GLubyte[vertexSize*4]; for (int i=0;i<vertexSize*4;i++) vertexColor[i]=255;
        facesSize = (int)tris.size();
        faces = new MeshIndex[facesSize>0?facesSize:1]; for (int i=0;i<facesSize;i++) faces[i]=tris[i];
    }
    else if (type == (int)MeshType::UVsphere){
        // esfera UV: meshVerts = segments (longitud), meshVerts2 = rings (latitud),
        // meshSize = radio. Cada celda de la grilla = un QUAD (faces3d) -> 1 normal
        // por cara. SMOOTH = grilla compartida con normal radial; FLAT = cada celda
        // con sus propios vertices y la normal de la cara. Las celdas de los polos
        // quedan como quads degenerados (un triangulo) -> 1 normal igual.
        int seg = meshVerts;  if (seg < 3) seg = 3;
        int rin = meshVerts2; if (rin < 2) rin = 2;
        float R = meshSize;
        std::vector<GLfloat> vp, vu; std::vector<GLbyte> vn; std::vector<GLushort> tris;

        if (meshSmooth){
            int cols = seg + 1;
            std::vector<int> grid((rin+1)*cols);
            for (int i=0;i<=rin;i++){
                float lat=(float)M_PI*i/rin, sLat=sinf(lat), cLat=cosf(lat);
                for (int j=0;j<=seg;j++){
                    float lon=2.0f*(float)M_PI*j/seg;
                    float nx=sLat*cosf(lon), ny=cLat, nz=sLat*sinf(lon);
                    grid[i*cols+j]=PushV(vp,vn,vu, R*nx,R*ny,R*nz, nx,ny,nz, (float)j/seg,(float)i/rin);
                }
            }
            for (int i=0;i<rin;i++){
                for (int j=0;j<seg;j++){
                    std::vector<int> ring; // orden hacia afuera (analogo al cono)
                    ring.push_back(grid[(i+1)*cols+j]);
                    ring.push_back(grid[i*cols+j]);
                    ring.push_back(grid[i*cols+j+1]);
                    ring.push_back(grid[(i+1)*cols+j+1]);
                    AddFace(tris, faces3d, ring);
                }
            }
        } else { // FLAT: cada celda con sus 4 vertices y la normal de la cara
            for (int i=0;i<rin;i++){
                float lat0=(float)M_PI*i/rin, lat1=(float)M_PI*(i+1)/rin;
                for (int j=0;j<seg;j++){
                    float lon0=2.0f*(float)M_PI*j/seg, lon1=2.0f*(float)M_PI*(j+1)/seg;
                    // 4 esquinas (mismo orden que el smooth: (i+1,j),(i,j),(i,j+1),(i+1,j+1))
                    float pos[12];
                    float coords[4][2] = { {lat1,lon0},{lat0,lon0},{lat0,lon1},{lat1,lon1} };
                    for (int k=0;k<4;k++){
                        float la=coords[k][0], lo=coords[k][1], sL=sinf(la);
                        pos[k*3]   = R*sL*cosf(lo);
                        pos[k*3+1] = R*cosf(la);
                        pos[k*3+2] = R*sL*sinf(lo);
                    }
                    float fnx,fny,fnz; NewellPos(pos,4,fnx,fny,fnz);
                    std::vector<int> ring;
                    for (int k=0;k<4;k++)
                        ring.push_back(PushV(vp,vn,vu, pos[k*3],pos[k*3+1],pos[k*3+2], fnx,fny,fnz, 0.0f,0.0f));
                    AddFace(tris, faces3d, ring);
                }
            }
        }

        vertexSize = (int)(vp.size()/3);
        vertex = new GLfloat[vp.size()]; for (size_t i=0;i<vp.size();i++) vertex[i]=vp[i];
        normals = new GLbyte[vn.size()]; for (size_t i=0;i<vn.size();i++) normals[i]=vn[i];
        uv = new GLfloat[vu.size()]; for (size_t i=0;i<vu.size();i++) uv[i]=vu[i];
        vertexColor = new GLubyte[vertexSize*4]; for (int i=0;i<vertexSize*4;i++) vertexColor[i]=255;
        facesSize = (int)tris.size();
        faces = new MeshIndex[facesSize>0?facesSize:1]; for (int i=0;i<facesSize;i++) faces[i]=tris[i];
    }
    else if (type == (int)MeshType::cone || type == (int)MeshType::cylinder){
        // cono = cilindro con radio variable: anillo base (n) + anillo top (n) +
        // n caras laterales (quad) + tapa(s) ngon. r2=0 -> el anillo top colapsa
        // al apex (cada vertice top sigue compartido por 2 caras: flat=2 magenta,
        // smooth=1; +1 si hay tapa de arriba). flat = normal por cara; smooth =
        // anillos compartidos con normal de pendiente (las tapas SIEMPRE propias).
        // El CILINDRO es el mismo cono con r2 = r1 (un solo radio).
        int n = meshVerts; if (n < 3) n = 3;
        float r1 = meshSize;
        float r2 = (type == (int)MeshType::cylinder) ? meshSize : meshSize2;
        float hy = meshDepth * 0.5f;
        bool trunc = (r2 > 0.0001f);
        float nyc = r1 - r2; // componente Y de la normal de pendiente
        std::vector<GLfloat> vp, vu; std::vector<GLbyte> vn; std::vector<GLushort> tris;

        if (meshSmooth){
            std::vector<int> baseR(n), topR(n);
            for (int j=0;j<n;j++){
                float a=2.0f*(float)M_PI*j/n, ca=cosf(a), sa=sinf(a);
                float lx=meshDepth*ca, ly=nyc, lz=meshDepth*sa, ln=sqrtf(lx*lx+ly*ly+lz*lz); if(ln<1e-6f)ln=1.0f;
                baseR[j]=PushV(vp,vn,vu, r1*ca,-hy,r1*sa, lx/ln,ly/ln,lz/ln, (float)j/n,0.0f);
            }
            for (int j=0;j<n;j++){
                float a=2.0f*(float)M_PI*j/n, ca=cosf(a), sa=sinf(a);
                float lx=meshDepth*ca, ly=nyc, lz=meshDepth*sa, ln=sqrtf(lx*lx+ly*ly+lz*lz); if(ln<1e-6f)ln=1.0f;
                topR[j]=PushV(vp,vn,vu, r2*ca,hy,r2*sa, lx/ln,ly/ln,lz/ln, (float)j/n,1.0f);
            }
            for (int j=0;j<n;j++){
                int jn=(j+1)%n; std::vector<int> ring;
                ring.push_back(baseR[j]); ring.push_back(topR[j]);
                ring.push_back(topR[jn]); ring.push_back(baseR[jn]);
                AddFace(tris, faces3d, ring);
            }
        } else { // FLAT: cada cara con sus propios vertices y la normal de la cara
            for (int j=0;j<n;j++){
                int jn=(j+1)%n;
                float aj=2.0f*(float)M_PI*j/n, ajn=2.0f*(float)M_PI*jn/n;
                float cj=cosf(aj), sj=sinf(aj), cn=cosf(ajn), sn=sinf(ajn);
                float pos[12];
                pos[0]=r1*cj; pos[1]=-hy; pos[2]=r1*sj;   // base j
                pos[3]=r2*cj; pos[4]= hy; pos[5]=r2*sj;    // top j
                pos[6]=r2*cn; pos[7]= hy; pos[8]=r2*sn;    // top jn
                pos[9]=r1*cn; pos[10]=-hy; pos[11]=r1*sn;  // base jn
                float fnx,fny,fnz; NewellPos(pos,4,fnx,fny,fnz);
                std::vector<int> ring;
                for (int k=0;k<4;k++)
                    ring.push_back(PushV(vp,vn,vu, pos[k*3],pos[k*3+1],pos[k*3+2], fnx,fny,fnz, 0.0f,0.0f));
                AddFace(tris, faces3d, ring);
            }
        }

        // TAPA de abajo: ngon SIN vertice central, normal -Y. Orden FORWARD ->
        // Newell da -Y (y el abanico mira hacia abajo) -> overlay/culling correctos
        {
            std::vector<int> ring;
            for (int j=0;j<n;j++){
                float a=2.0f*(float)M_PI*j/n, ca=cosf(a), sa=sinf(a);
                ring.push_back(PushV(vp,vn,vu, r1*ca,-hy,r1*sa, 0,-1,0, ca*0.5f+0.5f, sa*0.5f+0.5f));
            }
            AddFace(tris, faces3d, ring);
        }
        // TAPA de arriba (solo si truncado): ngon, normal +Y. Orden INVERTIDO -> +Y
        if (trunc){
            std::vector<int> ring;
            for (int j=0;j<n;j++){
                int jr=(n-j)%n; float a=2.0f*(float)M_PI*jr/n, ca=cosf(a), sa=sinf(a);
                ring.push_back(PushV(vp,vn,vu, r2*ca,hy,r2*sa, 0,1,0, ca*0.5f+0.5f, sa*0.5f+0.5f));
            }
            AddFace(tris, faces3d, ring);
        }

        // volcar acumuladores a los arrays de render
        vertexSize = (int)(vp.size()/3);
        vertex = new GLfloat[vp.size()]; for (size_t i=0;i<vp.size();i++) vertex[i]=vp[i];
        normals = new GLbyte[vn.size()]; for (size_t i=0;i<vn.size();i++) normals[i]=vn[i];
        uv = new GLfloat[vu.size()]; for (size_t i=0;i<vu.size();i++) uv[i]=vu[i];
        vertexColor = new GLubyte[vertexSize*4]; for (int i=0;i<vertexSize*4;i++) vertexColor[i]=255;
        facesSize = (int)tris.size();
        faces = new MeshIndex[facesSize>0?facesSize:1]; for (int i=0;i<facesSize;i++) faces[i]=tris[i];
    }
    else { vertexSize = 0; facesSize = 0; } // no es una primitiva conocida

    MaterialGroup g;
    g.startDrawn = 0;
    g.material = mat;
    g.indicesDrawnCount = facesSize;
    materialsGroup.push_back(g);

    CalcularBordes(); // bordes unicos desde faces3d (contorno de seleccion / wireframe)
}
