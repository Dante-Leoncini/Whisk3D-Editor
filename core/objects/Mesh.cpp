#include "Mesh.h"
#include <iostream>
#include <cmath>

// ===================================================
// Constructor
// ===================================================
Mesh::Mesh(Object* parent, Vector3 pos)
    : Object(parent, "Mesh", pos)
{
    IconType = static_cast<size_t>(IconType::mesh);
}

// ===================================================
// Destructor
// ===================================================
Mesh::~Mesh() {
    //LiberarMemoria();
    delete name;
}

// ===================================================
// Tipo de objeto
// ===================================================
ObjectType Mesh::getType() {
    return ObjectType::mesh;
}

// ===================================================
// Liberar memoria
// ===================================================
void Mesh::LiberarMemoria() {
    delete[] vertex;
    delete[] vertexColor;
    delete[] normals;
    delete[] uv;

    for (size_t i = 0; i < vertexGroups.size(); i++) {
        vertexGroups[i].indices.clear();
    }
    vertexGroups.clear();

    delete[] faces;
    materialsGroup.clear();
}

// ===================================================
// Renderizado
// ===================================================
void Mesh::RenderObject() {
    glPushMatrix();
    //glScalef(scale.x, scale.z, scale.y);

    glColor4f(ListaColores[static_cast<int>(ColorID::blanco)][0],
              ListaColores[static_cast<int>(ColorID::blanco)][1],
              ListaColores[static_cast<int>(ColorID::blanco)][2],
              ListaColores[static_cast<int>(ColorID::blanco)][3]);

    glDisable(GL_COLOR_MATERIAL);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, MaterialPreviewAmbient);

#ifdef ANDROID
    glMaterialxv(GL_FRONT_AND_BACK, GL_EMISSION, ListaColoresX[negro]);
    glMaterialx(GL_FRONT_AND_BACK, GL_SHININESS, 12 << 16);
#else
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, ListaColores[static_cast<int>(ColorID::negro)]);
    glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, 12);
#endif

    glVertexPointer(3, GL_FLOAT, 0, vertex);
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, vertexColor);
    glNormalPointer(GL_BYTE, 0, normals);
    glShadeModel(GL_SMOOTH);

    // Renderizado según el tipo de vista
    if (view == RenderType::MaterialPreview || view == RenderType::Rendered) {
        glTexCoordPointer(2, GL_FLOAT, 0, uv);
        for (size_t f = 0; f < materialsGroup.size(); f++) {
            Material* mat = MaterialDefecto;
            if (materialsGroup[f].material) {
                mat = materialsGroup[f].material;
            }

            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat->diffuse);
            glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat->specular);

            if (mat->vertexColor) {
                glColor4f(ListaColores[static_cast<int>(ColorID::negro)][0],
                          ListaColores[static_cast<int>(ColorID::negro)][1],
                          ListaColores[static_cast<int>(ColorID::negro)][2],
                          ListaColores[static_cast<int>(ColorID::negro)][3]);
                glEnableClientState(GL_COLOR_ARRAY);
                glEnable(GL_COLOR_MATERIAL);
            } else {
                glColor4f(mat->diffuse[0], mat->diffuse[1], mat->diffuse[2], mat->diffuse[3]);
                glDisable(GL_COLOR_MATERIAL);
                glDisableClientState(GL_COLOR_ARRAY);
            }

            if (mat->culling) glEnable(GL_CULL_FACE);
            else glDisable(GL_CULL_FACE);

            if (mat->depth_test) glEnable(GL_DEPTH_TEST);
            else glDisable(GL_DEPTH_TEST);  

            if (mat->lighting) {
                glEnableClientState(GL_NORMAL_ARRAY);
                glEnable(GL_LIGHTING);
            } else {
                glDisableClientState(GL_NORMAL_ARRAY);
                glDisable(GL_LIGHTING);
            }

            if (mat->transparent) glEnable(GL_BLEND);
            else glDisable(GL_BLEND);

            if (mat->texture) {
                glEnable(GL_TEXTURE_2D);
                glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                glBindTexture(GL_TEXTURE_2D, mat->texture->iID);

                if (mat->interpolacion == lineal) {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                } else {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                }

                if (mat->repeat) {
                    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                } else {
                    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                }
            } else {
                glDisable(GL_TEXTURE_2D);
                glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            }

            glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, mat->emission);

            glDrawElements(GL_TRIANGLES, materialsGroup[f].indicesDrawnCount,
                           GL_UNSIGNED_SHORT, &faces[materialsGroup[f].startDrawn]);
        }
    } else if (view == RenderType::Solid) {
        glEnable(GL_LIGHTING);
        glEnable(GL_CULL_FACE);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, MaterialDefecto->diffuse);

        glDrawElements(GL_TRIANGLES, facesSize, GL_UNSIGNED_SHORT, faces);
    }

    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthFunc(GL_LESS);

    glPopMatrix();
}

Object* NewMesh(MeshType type, Object* parent, bool query){
    Mesh* mesh = new Mesh(parent, cursor3D.pos);

    //SetActive(NewMesh);
	
	MaterialGroup tempFaceGroup;
	tempFaceGroup.startDrawn = 0;
	tempFaceGroup.material = MaterialDefecto;
	
	VertexGroup TempVertexGroups;	
	
	if (type == MeshType::plane){ 
		mesh->vertexSize = 4;
		mesh->vertex = new GLfloat[mesh->vertexSize*3];
		mesh->vertexColor = new GLubyte[mesh->vertexSize*4];
		mesh->normals = new GLbyte[mesh->vertexSize*3];
		mesh->uv = new GLfloat[mesh->vertexSize*2];
		int InputSize = 2;
		//CleanupStack::PopAndDestroy(noteBuf);
		InputSize = InputSize*1000;	

		for (int i = 0; i < mesh->vertexSize*3; i++) {
			mesh->vertex[i] = PlaneVertices[i]*InputSize;
		}
		for (int i = 0; i < mesh->vertexSize; i++) {
			mesh->normals[i*3] = 0;
			mesh->normals[i*3+1] = 127;
			mesh->normals[i*3+2] = 0;
		}
		for (int i = 0; i < mesh->vertexSize*4; i++) {
			mesh->vertexColor[i] = 255;
		}
		for (int i = 0; i < mesh->vertexSize*2; i++) {
			mesh->uv[i] = (GLfloat)PlaneUV[i];
		}

		//mesh->facesCount = tempFaceGroup.count = 2;
		mesh->facesSize = tempFaceGroup.indicesDrawnCount = 6;

		mesh->faces = new GLushort[tempFaceGroup.indicesDrawnCount];
		for (int i = 0; i < tempFaceGroup.indicesDrawnCount; i++) {
			mesh->faces[i] = PlaneTriangles[i];
		}
	}	
	else if (type == MeshType::circle){ 
		//HBufC* noteBuf = HBufC::NewLC(100);
		//noteBuf->Des().Copy(_L("Add Circle vertices"));
		//TInt originalVertexSize  = DialogNumber(8, 3, 512, noteBuf);	
		int originalVertexSize = 8;
		mesh->vertexSize = originalVertexSize + 1;  // Agregar un vértice adicional
		//noteBuf->Des().Copy(_L("Add Circle radius cm"));
		//TInt Radius = DialogNumber(100, 1, 10000, noteBuf);	
		int Radius = 100;	
		//CleanupStack::PopAndDestroy(noteBuf);
		Radius = Radius*20;	

		mesh->vertex = new GLfloat[mesh->vertexSize*3];
		mesh->vertexColor = new GLubyte[mesh->vertexSize*4];
		mesh->normals = new GLbyte[mesh->vertexSize*3];
		mesh->uv = new GLfloat[mesh->vertexSize*2];

		for (int i = 0; i < mesh->vertexSize*3; i++) {
			mesh->vertex[i] = Radius;
		}

		for (int i = 0; i < mesh->vertexSize; i++) {
			GLfloat angle = 2.0f * M_PI * (i % originalVertexSize) / originalVertexSize;  // angulo para los vértices

			mesh->vertex[i*3] = -Radius * cos(angle);
			mesh->vertex[i*3+1] = 0;
			mesh->vertex[i*3+2] = -Radius * sin(angle);
			mesh->normals[i*3] = 0;
			mesh->normals[i*3+1] = 127;
			mesh->normals[i*3+2] = 0;
			mesh->uv[i * 2] = static_cast<GLfloat>(cos(angle) * 127);
			mesh->uv[i * 2 + 1] = static_cast<GLfloat>(sin(angle) * 127);
		}

		for (int i = 0; i < mesh->vertexSize*4; i++) {
			mesh->vertexColor[i] = 255;
		}

		mesh->facesSize = tempFaceGroup.indicesDrawnCount = (mesh->vertexSize - 2) * 3;

		mesh->faces = new GLushort[tempFaceGroup.indicesDrawnCount];
		for (int i = 0; i < originalVertexSize - 1; i++) {
			mesh->faces[i * 3] = 0;          // Primer vértice
			mesh->faces[i * 3 + 1] = i + 2;  // Segundo vértice
			mesh->faces[i * 3 + 2] = i + 1;  // Tercer vértice
		}
	}
	else if (type == MeshType::cube){ 
    	mesh->vertexSize = 24;
		mesh->vertex = new GLfloat[mesh->vertexSize*3];
		mesh->vertexColor = new GLubyte[mesh->vertexSize*4];
		mesh->normals = new GLbyte[mesh->vertexSize*3];
		mesh->uv = new GLfloat[mesh->vertexSize*2];

		for (int i = 0; i < mesh->vertexSize*3; i++) {
			mesh->vertex[i] = CuboVertices[i];
			mesh->normals[i] = CuboNormals[i];
		}
		for (int i = 0; i < mesh->vertexSize*4; i++) {
			mesh->vertexColor[i] = 255;
		}
		for (int i = 0; i < mesh->vertexSize*2; i++) {
			//mesh->uv[i] = (GLfloat)((CuboUV[i]+128)/255)*1280;
			mesh->uv[i] = (GLfloat)CuboUV[i];
		}

		mesh->facesSize = tempFaceGroup.indicesDrawnCount = 36;

		mesh->faces = new GLushort[tempFaceGroup.indicesDrawnCount];
		for (int i = 0; i < tempFaceGroup.indicesDrawnCount; i++) {
			mesh->faces[i] = CuboTriangles[i];
		}
		mesh->name->SetValue(mesh->SetName("Cube"));
	}	

	//le asignamos la mesh	
	mesh->materialsGroup.push_back(tempFaceGroup);
	return mesh;
};