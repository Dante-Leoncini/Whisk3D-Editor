//interpolacion
enum {lineal, closest};

enum class RenderType { Solid, MaterialPreview, Rendered, ZBuffer };

RenderType view = RenderType::MaterialPreview;

RenderType StringToRenderType(const std::string& s){
    if(s == "Solid")            return RenderType::Solid;
    if(s == "MaterialPreview")  return RenderType::MaterialPreview;
    if(s == "Rendered")         return RenderType::Rendered;
    if(s == "ZBuffer")          return RenderType::ZBuffer;

    std::cerr << "[StringToRenderType] WARNING: valor desconocido '" << s
              << "' → usando RenderType::Solid" << std::endl;
    return RenderType::Solid; // fallback seguro
}

//luz material preview
GLfloat MaterialPreviewAmbient[4]  = { 0.3f, 0.3f, 0.3f, 1.0f };
GLfloat MaterialPreviewDiffuse[4]   = { 1.0f, 1.0f, 1.0f, 1.0f };
GLfloat MaterialPreviewSpecular[4]   = { 0.2f, 0.2f, 0.2f, 1.0f };
GLfloat MaterialPreviewPosition[4]  = { -0.45f, 0.55f, 1.0f, 0.0f };

#include "./Objects/Textures.h"
#include "./UI/UI.h"
#include "./Objects/Materials.h"
#include "./Objects/Objects.h"
#include "./Constrains.h"

void ReloadViewport(){
	//Recalcula los constrains
    for(size_t c = 0; c < Constraints.size(); c++) {
		Object& objTarget = *Constraints[c].Target;
		Object& obj = *Constraints[c].Id;
		switch (Constraints[c].type) {
			case trackto: {
				// Calcular vector dirección
				GLfloat dirX = objTarget.posX - obj.posX;
				GLfloat dirY = objTarget.posY - obj.posY;
				GLfloat dirZ = objTarget.posZ - obj.posZ;						
				
				obj.rotZ = atan2(dirX, dirY) * (180.0 / M_PI);  // Azimut

				// Calcular longitud del vector (magnitud)
				GLfloat length = sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);

				// Cálculo de la elevación (rotY)
				if (Constraints[c].opcion){	
					obj.rotZ += 180; // Para invertir el eje si necesario.
					obj.rotX = asin(dirZ/length) * (180.0 / M_PI);					
				}
				else {
					obj.rotZ -= 90; // Para invertir el eje si necesario.
					obj.rotY = asin(dirZ/length) * (180.0 / M_PI);	
				}

				break;
			}
			case copyrotation:
				obj.rotX = objTarget.rotX;
				obj.rotY = objTarget.rotY;
				obj.rotZ = objTarget.rotZ;
				break;
			case copylocation:
				obj.posX = objTarget.posX;
				obj.posY = objTarget.posY;
				obj.posZ = objTarget.posZ;
				break;
		}
	}
}

/*void SetPerspectiva(bool orthographicValue ){
	orthographic = orthographicValue;
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
    if ( orthographicValue ){
        // Proyección ortográfica
        float size = 90.0f;
        glOrtho(-size * aspect, size * aspect,
                -size, size,
                -1.0f, 1000.0f);
    }
    else {
        gluPerspective(fovDeg, aspect, nearClip, farClip);
    }
    glMatrixMode( GL_MODELVIEW );
}

void InitOpenGL(){
    // Configuración básica de OpenGL
    glEnable(GL_DEPTH_TEST); // Habilitar z-buffer
    //glDisable(GL_CULL_FACE); // desactivar culling

	//glViewport(0, 0, winW / 2, winH); // x, y, ancho, alto
    SetPerspectiva(orthographic);
}*/