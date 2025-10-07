bool redibujar = true; //solo redibuja si este valor esta en true
std::vector<int> Collection;

//interpolacion
enum {lineal, closest};

#include "./Textures.h"
#include "./Materials.h"
#include "./Mesh.h"
#include "./Constrains.h"
#include "./Objects.h"
#include "./Modifiers.h"
#include "./UI/UI.h"

enum { Solid, MaterialPreview, Rendered };

bool SimularZBuffer = false;
int view = MaterialPreview;
bool orthographic = false;

GLfloat objAmbient[4]  = { 0.3, 0.3, 0.3, 1.0 };
//GLfloat objAmbient[4]  = { 0.0, 0.0, 0.0, 1.0 };

std::vector<Light> Lights;

void SetPerspectiva(bool orthographicValue, bool redibuja ){
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
		/*glFrustumf( FRUSTUM_LEFT * aspect, FRUSTUM_RIGHT * aspect,
					FRUSTUM_BOTTOM, FRUSTUM_TOP,
					FRUSTUM_NEAR, farClip );  */  	
    }
    glMatrixMode( GL_MODELVIEW );
    redibujar = redibuja;
}

void InitOpenGL(){
    // Configuración básica de OpenGL
    glEnable(GL_DEPTH_TEST); // Habilitar z-buffer
    //glDisable(GL_CULL_FACE); // desactivar culling

    SetPerspectiva(orthographic, redibujar);
}

inline float FIXED_TO_FLOAT(GLfixed x) {
    return static_cast<float>(x) / 65536.0f; // porque Q16.16
}

void RenderMesh( Object& obj ){
//void RenderMesh( Object& obj, int indice ){
	Mesh& pMesh = Meshes[obj.Id];	
	glPushMatrix();
	//averiguar diferencia entre glScalex y glScalef
	//glScalex(obj.scaleX, obj.scaleZ, obj.scaleY);
	//glScalef(obj.scaleX, obj.scaleZ, obj.scaleY);
	glScalef(
		FIXED_TO_FLOAT(obj.scaleX),
		FIXED_TO_FLOAT(obj.scaleZ),
		FIXED_TO_FLOAT(obj.scaleY)
	);
			
	glColor4f(ListaColores[blanco][0],ListaColores[blanco][1],ListaColores[blanco][2],ListaColores[blanco][3]);
	glDisable(GL_COLOR_MATERIAL);
	glMaterialfv(   GL_FRONT_AND_BACK, GL_AMBIENT,  objAmbient  );
	//averiguar diferencia entre glMaterialx y glMateriali
	//glMaterialx( GL_FRONT_AND_BACK, GL_SHININESS,   12 << 16     );
	glMateriali( GL_FRONT_AND_BACK, GL_SHININESS,   12     );
	glMaterialfv(   GL_FRONT_AND_BACK, GL_EMISSION, ListaColores[negro] );

	// Set array pointers from mesh.
	glVertexPointer( 3, GL_SHORT, 0, pMesh.vertex );
	glColorPointer( 4, GL_UNSIGNED_BYTE, 0, pMesh.vertexColor );	
	glNormalPointer( GL_BYTE, 0, pMesh.normals );
	
	glShadeModel( GL_SMOOTH );
	
	//modelo con textura
	if (SimularZBuffer){
		glDisable( GL_LIGHTING );
		glDisable(GL_BLEND);
		glDisable( GL_TEXTURE_2D );
		glDrawElements(GL_TRIANGLES, pMesh.facesSize, GL_UNSIGNED_SHORT, pMesh.faces);
	}
	
	//material
	if (view == MaterialPreview || view == Rendered){
		// Calcular coordenadas UV reflejadas
		//calculateReflectionUVs(pMesh);

		glTexCoordPointer( 2, GL_FLOAT, 0, pMesh.uv );
		
		//for(int f=0; f < pMesh.materialsGroup.Count(); f++){
		for(size_t f=0; f < pMesh.materialsGroup.size(); f++){
			Material& mat = Materials[pMesh.materialsGroup[f].material];	
			glMaterialfv(   GL_FRONT_AND_BACK, GL_DIFFUSE, mat.diffuse ); 
			glMaterialfv(   GL_FRONT_AND_BACK, GL_SPECULAR, mat.specular );

			//vertex color
			if (mat.vertexColor){
				glColor4f(ListaColores[negro][0],ListaColores[negro][1],ListaColores[negro][2],ListaColores[negro][3]);
				glEnableClientState( GL_COLOR_ARRAY );
				glEnable(GL_COLOR_MATERIAL);
			}
			else {
				glColor4f(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2], mat.diffuse[3]);
				glDisable(GL_COLOR_MATERIAL);
				glDisableClientState( GL_COLOR_ARRAY );
			}		

			//si usa culling
			if (mat.culling){glEnable( GL_CULL_FACE );}
			else {glDisable( GL_CULL_FACE );}		 

			//si tiene iluminacion	
			if (mat.lighting){glEnable( GL_LIGHTING );}
			else {glDisable( GL_LIGHTING );}
			
			//transparent
			if (mat.transparent){glEnable(GL_BLEND);}
			else {glDisable(GL_BLEND);}
			
			//si tiene texturas
			if (mat.textura){
				glEnable( GL_TEXTURE_2D );
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				glBindTexture(  GL_TEXTURE_2D, mat.textureID ); //selecciona la textura							
			
				//textura pixelada o suave
				if (mat.interpolacion == lineal){
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				}
				else if (mat.interpolacion == closest){
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				}	
				//si la textura se repite
				if (mat.repeat){
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				}
				else {
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				}	
			}
			else {
				glDisable( GL_TEXTURE_2D );
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			}

			glMaterialfv(   GL_FRONT_AND_BACK, GL_EMISSION, mat.emission );

			glDrawElements( GL_TRIANGLES, pMesh.materialsGroup[f].indicesDrawnCount, GL_UNSIGNED_SHORT, &pMesh.faces[pMesh.materialsGroup[f].startDrawn] );

			//for (TInt mdf = 0; mdf < pMesh.Modifiers.Count(); ++mdf) {	
			for (size_t mdf = 0; mdf < pMesh.Modifiers.size(); ++mdf) {
				Modifier& modificador = Modifiers[pMesh.Modifiers[mdf]];
				switch (modificador.type) {
					case mirror:
						if (modificador.OpcionesTBool[0]){	
							glPopMatrix();						
							glPushMatrix();
							//glScalex(-obj.scaleX, obj.scaleZ, obj.scaleY);
							glScalef(
								FIXED_TO_FLOAT(-obj.scaleX),
								FIXED_TO_FLOAT(obj.scaleZ),
								FIXED_TO_FLOAT(obj.scaleY)
							);
							glScalef(-1, 0, 0);
							glDrawElements( GL_TRIANGLES, pMesh.materialsGroup[f].indicesDrawnCount, GL_UNSIGNED_SHORT, &pMesh.faces[pMesh.materialsGroup[f].startDrawn] );
						}
						if (modificador.OpcionesTBool[1]){	
							glPopMatrix();				
							glPushMatrix();
							//glScalex(obj.scaleX, obj.scaleZ, -obj.scaleY);
							glScalef(
								FIXED_TO_FLOAT(obj.scaleX),
								FIXED_TO_FLOAT(obj.scaleZ),
								FIXED_TO_FLOAT(-obj.scaleY)
							);
							glDrawElements( GL_TRIANGLES, pMesh.materialsGroup[f].indicesDrawnCount, GL_UNSIGNED_SHORT, &pMesh.faces[pMesh.materialsGroup[f].startDrawn] );
						}
						if (modificador.OpcionesTBool[2]){	
							glPopMatrix();
							glPushMatrix();
							//glScalex(obj.scaleX, -obj.scaleZ, obj.scaleY);
							glScalef(
								FIXED_TO_FLOAT(obj.scaleX),
								FIXED_TO_FLOAT(-obj.scaleZ),
								FIXED_TO_FLOAT(obj.scaleY)
							);
							glDrawElements( GL_TRIANGLES, pMesh.materialsGroup[f].indicesDrawnCount, GL_UNSIGNED_SHORT, &pMesh.faces[pMesh.materialsGroup[f].startDrawn] );
						}
						break;
				}
			}
		}
	}	
	//modelo sin textura
	else if (view == Solid){
		glEnable( GL_LIGHTING );
		glEnable( GL_CULL_FACE );
		glDisableClientState( GL_COLOR_ARRAY );	  	
		glDisable( GL_TEXTURE_2D );
		glDisable(GL_BLEND);
		glMaterialfv(   GL_FRONT_AND_BACK, GL_DIFFUSE,  Materials[0].diffuse   ); 

		//no hace falta usar el bucle ya que es todo del mismo material.
		//esto es mucho mas rapido
		glDrawElements( GL_TRIANGLES, pMesh.facesSize, GL_UNSIGNED_SHORT, pMesh.faces );			

		//glDrawElements( GL_TRIANGLES, pMesh.facesSize*3, GL_UNSIGNED_SHORT, pMesh.faces );	
		//for(int f=0; f < pMesh.materialsGroup.Count(); f++){	
			/*Material& mat = Materials[pMesh.materialsGroup[f].material];
			//si usa culling
			if (mat.culling){glEnable( GL_CULL_FACE );}
			else {glDisable( GL_CULL_FACE );}	*/

			//glDrawElements( GL_TRIANGLES, pMesh.materialsGroupsSize[f]*3, GL_UNSIGNED_SHORT, pMesh.faces[f] );	
			//glDrawElements( GL_TRIANGLES, pMesh.materialsGroup[f].start, GL_UNSIGNED_SHORT, &pMesh.faces[pMesh.materialsGroup[f].start] );	
		//}
	} 

	glDisable(GL_POLYGON_OFFSET_FILL);
	glDepthFunc(GL_LESS);
	
	//dibuja el borde seleccionado
	/*if(SelectActivo == objId && showOverlays && showOutlineSelect){
		glDisableClientState( GL_COLOR_ARRAY );	  
		glDisable( GL_LIGHTING );
		glEnable(GL_COLOR_MATERIAL);
		glDisable( GL_TEXTURE_2D );  
		glEnable(GL_POLYGON_OFFSET_FILL);
		
		//si se esta editando
		else if (view != 2){
			glPolygonOffset(1.0, 200.0);
			glLineWidth(3);	 
			glColor4f(ListaColores[colorBordeSelect][0],ListaColores[colorBordeSelect][1],ListaColores[colorBordeSelect][2],ListaColores[colorBordeSelect][3]);
			//glDrawElements( GL_LINES, obj.edgesSize, GL_UNSIGNED_SHORT, obj.edges );	
			glLineWidth(1); //lo deja en su valor por defecto	 
		}	
	};*/
	glPopMatrix();
}

// Funcion recursiva para renderizar un objeto y sus hijos
void RenderMeshAndChildren(Object& obj){
//void RenderMeshAndChildren(Object& obj, int indice){
    // Guardar la matriz actual
    glPushMatrix();
    
    // Aplicar las transformaciones del objeto
    glTranslatef(obj.posX, obj.posZ, obj.posY);
    glRotatef(obj.rotX, 1, 0, 0); // angulo, X Y Z
    glRotatef(obj.rotZ, 0, 1, 0); // angulo, X Y Z
    glRotatef(obj.rotY, 0, 0, 1); // angulo, X Y Z

    // Si es visible y es un mesh, lo dibuja
    if (obj.visible && obj.type == mesh) {
        RenderMesh(obj);
        //RenderMesh(obj, indice); // Ajusta el segundo parametro si es necesario
    }
    
    // Procesar cada hijo
    //for (int c = 0; c < obj.Childrens.Count(); c++) {
    for (size_t c = 0; c < obj.Childrens.size(); c++) {
        Object& objChild = Objects[obj.Childrens[c].Id];
        //RenderMeshAndChildren(objChild, indice);
        RenderMeshAndChildren(objChild);
    }

    // Restaurar la matriz previa
    glPopMatrix();
}