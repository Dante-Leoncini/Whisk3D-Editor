/*
 * ==============================================================================
 *  Name        : Whisk3D.cpp
 *  Part of     : OpenGLEx / Whisk3D
 * ==============================================================================
 */

// INCLUDES
#include <e32std.h>
#include <e32math.h>
#include <e32base.h>

//para leer archivos
#include <s32file.h>

//debug
//#include <e32cons.h>
//LOCAL_D CConsoleBase* console;
#include "Whisk3D.h"
#include "Whisk3DContainer.h"

#include "Dialogs.h"
#include "Animation.h"
#include "Mesh.h"
#include "Floor.h" //modelo 3d
#include "Primitivas.h" //sprite 2D
#include "GeometriaUI.h" //elementos de la UI

#include <utf.h>       // Para CnvUtfConverter

#include "whisk3D.rsg"
#include "whisk3D.hrh"
//#include <aknmessagequerydialog.h>		// DialogAlertL
#include <aknnotewrappers.h> 
#include <akncommondialogs.h>

//nuevo apra el nuevo dialogo con opciones
//#include <eikprogi.h>
//#include <aknlists.h>
//#include <aknpopup.h>
//#include <aknapp.h>
//#include <aknutils.h>
//#include <eikclbd.h>
//#include <aknselectionlist.h> // Para CAknListQueryDialog

// MACROS
#define MATERIAL_MAX 1
#define LIGHT_MAX    1

#define cameraDistance 270

#define MATERIALCOLOR(r, g, b, a)     \
       (GLfloat)(r * MATERIAL_MAX),   \
       (GLfloat)(g * MATERIAL_MAX),   \
       (GLfloat)(b * MATERIAL_MAX),   \
       (GLfloat)(a * MATERIAL_MAX)

#define LIGHTCOLOR(r, g, b, a)        \
       (GLfloat)(r * LIGHT_MAX),      \
       (GLfloat)(g * LIGHT_MAX),      \
       (GLfloat)(b * LIGHT_MAX),      \
       (GLfloat)(a * LIGHT_MAX)

// CONSTANTS

/* Materials for the Models object. */
static const GLfloat objDiffuse[4] = { MATERIALCOLOR(0.8, 0.8, 0.8, 1.0) };
static const GLfloat objAmbient[4] = { MATERIALCOLOR(0.4, 0.4, 0.4, 1.0) };

//color vertices
static const GLfloat colorVertice[4]  = { MATERIALCOLOR(0.87, 0.44, 0.06, 1.0) };
//color borde
static const GLfloat colorBorde[4]  = { MATERIALCOLOR(0.68, 0.45, 0.13, 1.0) };
//color borde Select
//GLfloat colorBordeSelect[4] = { MATERIALCOLOR(0.94, 0.59, 0.17, 1.0) };
//array de colores
GLfloat ListaColores[11][4] = {
	{ MATERIALCOLOR(0.22, 0.28, 0.25, 1.0)  },   //fondo
	{ MATERIALCOLOR(1.0, 1.0, 1.0, 1.0)     },   //blanco
	{ MATERIALCOLOR(0.91, 0.50, 0.98, 1.0)  },   //acento (violeta) 
	{ MATERIALCOLOR(0.48, 0.30, 1.0, 1.0)  },   //acento oscuro	
	{ MATERIALCOLOR(0.0, 0.0, 0.0, 1.0)     },   //negro
	{ MATERIALCOLOR(0.12, 0.12, 0.12, 1.0)  },   //gris
	{ MATERIALCOLOR(0.94, 0.59, 0.17, 0.25f)},   //naranja transparente
	{ MATERIALCOLOR(0.22, 0.22, 0.22, 1.0)  },    //cabezera de la barra de herramientas
	{ MATERIALCOLOR(0.0, 0.0, 0.0, 0.25f)     },   //negroTransparente
	{ MATERIALCOLOR(0.757, 0.757, 0.757, 1.0)     },   //azul de los iconos seleccionados
};

enum{
	background,
	blanco,
	accent,
	accentDark,
	negro,
	gris,
	naranjaFace,
	headerColor,
	negroTransparente,
	grisUI
};

static const GLfloat LineaPiso[4]  =      { MATERIALCOLOR(0.29, 0.29, 0.29, 1.0) };
static const GLfloat LineaPisoRoja[4]  =  { MATERIALCOLOR(0.56, 0.23, 0.28, 1.0) };
static const GLfloat LineaPisoVerde[4]  = { MATERIALCOLOR(0.38, 0.53, 0.15, 1.0) };

//ejes transformacion
static const GLfloat ColorTransformX[4]  = { MATERIALCOLOR(0.88, 0.48, 0.54, 1.0) };
static const GLfloat ColorTransformY[4]  = { MATERIALCOLOR(0.65, 0.81, 0.38, 1.0) };
static const GLfloat ColorTransformZ[4]  = { MATERIALCOLOR(0.46, 0.67, 0.89, 1.0) };

/* Global ambient light. */
static const GLfloat globalAmbient[4]   = { LIGHTCOLOR(0.5, 0.5, 0.5, 1.0) };

GLfloat AmbientRender[4] = { LIGHTCOLOR(0.0, 0.0, 0.0, 1.0) };

/* Lamp parameters. */
static const GLfloat lightDiffuseLamp[4]   = { LIGHTCOLOR(0.8, 0.8, 0.8, 1.0) };
static const GLfloat lightAmbient[4]   = { LIGHTCOLOR(0.0, 0.0, 0.0, 1.0) };
static const GLfloat sunLightPosition[4]  = {-100, 1000, 1000, 0 }; // y, z, x, si es direccional o puntual
	
/* Spotlight parameters. */
//static const GLfloat lightDiffuseSpot[4]   = { LIGHTCOLOR(1.0, 1.0, 1.0, 1.0) };
//static const GLfloat lightSpecularSpot[4]  = { LIGHTCOLOR(1.0, 1.0, 1.0, 1.0) };
//static const GLfloat lightPositionSpot[4]  = {  0, -10, -1, 0 };
//static const GLfloat lightDirectionSpot[4] = {  0,  -10,  -1, 1 };

//para las luces puntuales
static const GLfloat positionPuntualLight[4] = {0, 0, 0, 1};

//camara
TBool ortografica = false;
GLfloat aspectRatio;
GLfloat rotX = 113.5;
GLfloat rotY = 20.0; //66.2
GLfloat posX = 0;
GLfloat posY = 0;
GLfloat posZ = 0;
GLfloat PivotX = 0;
GLfloat PivotY = 0;
GLfloat PivotZ = 0;

GLfloat LastRotX = 0;
GLfloat LastRotY = 0;	
GLfloat LastPivotX = 0;
GLfloat LastPivotY = 0;
GLfloat LastPivotZ = 0;

//vista 3d
GLshort mouseX = 0;
GLshort mouseY = 0;
TBool mouseVisible = false;
TBool showOverlays = true;
TBool ShowUi = true;
TBool show3DCursor = true;
TBool showFloor = true;
TBool showYaxis = true;
TBool showXaxis = true;
TBool showOutlineSelect = true;
TBool iWidescreenEnabled = false;
TBool showOrigins = true;
TBool SimularZBuffer = false;
GLfloat Cursor3DposX = 0.0f;
GLfloat Cursor3DposZ = 0.0f;
GLfloat Cursor3DposY = 0.0f;
TInt nextLightId = GL_LIGHT1;

//animacion
TInt StartFrame = 1;
TInt EndFrame = 250;
TInt CurrentFrame = 1;
TBool redibujar = true; //solo redibuja si este valor esta en true
TBool ViewFromCameraActive = false;

//interpolacion
enum {lineal, closest};
enum {Solid, MaterialPreview, Rendered};

enum{
	ObjectMode
};

enum {
	editNavegacion,
    EdgeMove,
	FaceMove,
	timelineMove,
	rotacion,
	EditScale,
	translacion
};

enum{
	Orbit,
	Fly,
	Apuntar
};

enum{
	vertexSelect, edgeSelect, faceSelect
};

enum{
	cubo, esfera, cilindro, plane, vacio, camara,
    cad, luz, vertice, circle
};

//CPrimitiva Primitivas;

typedef enum { X, Y, Z, XYZ } Axis;

TInt view = MaterialPreview;
TInt axisSelect = X;

class SaveState {
	public:
		TInt indice;
		GLfloat posX;
		GLfloat posY;
		GLfloat posZ;
		GLfloat rotX;
		GLfloat rotY;
		GLfloat rotZ;
		GLfixed scaleX;
		GLfixed scaleY;
		GLfixed scaleZ;
};
RArray<SaveState> estadoObjetos;

//GLshort estadoVertex[3]={0, 0, 0};
class SaveStateVertex {
	public:
		TInt indice;
		GLshort x;
		GLfloat y;
		GLfloat z;
};
RArray<SaveStateVertex> estadoVertices;

class VectorTInt {
	public:
		TInt indice;
		TInt x;
		TInt y;
		TInt z;
		GLshort posX;
		GLshort posY;
		GLshort posZ;
};
RArray<VectorTInt> VectoresTInt;

enum {
	TeclaSuelta,
	TeclaPresionada,
	TeclaMantenida,
	TeclaSoltada
};

class FlechaEstado {
	public:
		TInt cual;
		TInt estado;
		TBool activo;
};

//Crea un array de objetos
RArray<TInt> Collection;
RArray<Object> Objects;
RArray<Material> Materials;
RArray<Light> Lights;
RArray<Mesh> Meshes;
RArray<AnimationObject> AnimationObjects;
TInt tipoSelect = vertexSelect;
TInt SelectActivo = 0;
TInt SelectCount = 0;
GLshort TransformPivotPoint[3] = {0,0,0};
GLfloat TransformPivotPointFloat[3] = {0.0f,0.0f,0.0f};
FlechaEstado* flechasEstados;

void CWhisk3D::changeSelect(){
	if (InteractionMode == ObjectMode){
		//si no hay objetos
		//o si esta moviendo, rotando o haciendo algo... no deja que continue
		if (1 > Objects.Count() || estado != editNavegacion){
			return;
		}
		//DeseleccionarTodo();
		//deselecciona el objeto actual si es que estaba seleccionado
		if (Objects[SelectActivo].seleccionado){
			Objects[SelectActivo].seleccionado = false;
			SelectCount--;
		}

		//pasa al siguiente
		SelectActivo++;
		if (SelectActivo > Objects.Count()-1){
			SelectActivo = 0;
		}
		//selecciona el proximo objeto
		if (!Objects[SelectActivo].seleccionado){
			Objects[SelectActivo].seleccionado = true;
			SelectCount++;
		}
	}
    redibujar = true;	
}

void CWhisk3D::SetNavigation(){
	if (navegacionMode == Orbit){
		navegacionMode = Fly;
	}
	else if (navegacionMode == Fly){
		navegacionMode = Apuntar;
		navegacionMode = Orbit;
	}
	else {
		navegacionMode = Orbit;
	}
    redibujar = true;
}

// ============================= LOCAL FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// sin
// Help function to make the duck 'quak'.
// -----------------------------------------------------------------------------
//
GLfloat CWhisk3D::sin(GLfloat aRad){
    TReal trg, src = (TReal)aRad;
    if ( Math::Sin(trg, src) == KErrNone )
        {
        return (GLfloat)trg;
        }
    return 0;
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

GLfloat CWhisk3D::aSin(GLfloat value) {
    TReal result, input = static_cast<TReal>(value);
    if (Math::ASin(result, input) == KErrNone) {
        return static_cast<GLfloat>(result);
    }
    return 0;
}

GLfloat CWhisk3D::atan2(GLfloat y, GLfloat x) {
    TReal result;
    if (x > 0) {
        if (Math::ATan(result, y / x) == KErrNone) {
            return static_cast<GLfloat>(result);
        }
    } else if (x < 0) {
        if (Math::ATan(result, y / x) == KErrNone) {
            if (y >= 0) {
                return static_cast<GLfloat>(result) + M_PI;
            } else {
                return static_cast<GLfloat>(result) - M_PI;
            }
        }
    } else {
        if (y > 0) {
            return M_PI / 2.0;
        } else if (y < 0) {
            return -M_PI / 2.0;
        }
    }
    return 0;
}

// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CWhisk3D::CWhisk3D
// C++ default constructor can NOT contain any code, that
// might leave.
// -----------------------------------------------------------------------------
//
CWhisk3D::CWhisk3D( TUint aWidth, TUint aHeight, CWhisk3DInput* aInputHandler )
: CFiniteStateMachine() // iContainer( NULL ),CFiniteStateMachine()
	{
    iScreenWidth  = aWidth;
    iScreenHeight = aHeight;	
	iScreenHeightSplit = (GLfloat)aHeight/2;
    iInputHandler = aInputHandler;
	}

// -----------------------------------------------------------------------------
// CWhisk3D::ConstructL
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
enum{
	FlechaArriba,
	FlechaDerecha,
	FlechaAbajo,
	FlechaIzquierda
};

void CWhisk3D::ConstructL( void ){
	InteractionMode = ObjectMode;
	estado = editNavegacion;
	navegacionMode = Orbit;
	showOverlays = true;
	show3DCursor = true;
	showFloor = true;
	showYaxis = true;
	showXaxis = true;
	showOutlineSelect = true;
	showOrigins = true;
	PlayAnimation = false;
	ShowTimeline = true;
	ShowRelantionshipsLines = true;
	iShiftPressed = false;
	iAltPressed = false;
	iCtrlPressed = false;
	CameraToView = false;
	SelectActivo = 0;
	SelectActivo = -1;

	CalculateMillisecondsPerFrame(60);

	flechasEstados = new FlechaEstado[4];
	for (TInt i = 0; i < 4; i++) {
	    flechasEstados[i].estado = TeclaSuelta;
	    flechasEstados[i].activo = false;
	}
	flechasEstados[FlechaArriba].cual = EJoystickUp;
	flechasEstados[FlechaDerecha].cual = EJoystickRight;
	flechasEstados[FlechaAbajo].cual = EJoystickDown;
	flechasEstados[FlechaIzquierda].cual = EJoystickLeft;

	//tiene que haber un material por defecto siempre
	NewMaterial(false);

	//debuger
	//console = Console::NewL(_L("Consola"),TSize(KConsFullScreen, KConsFullScreen));
	AddObject(camera);
	Objects[0].posX = -800*6.8;
	Objects[0].posY = -800*7.29;
	Objects[0].posZ = 800*4.91;
	Objects[0].rotZ = -45.0;
	Objects[0].rotY = -26.15;
	Objects[0].scaleX = Objects[0].scaleY = Objects[0].scaleZ = 40000;

	AddObject(light);
	Objects[1].posX = -3000;
	Objects[1].posY = 1500;
	Objects[1].posZ = 4500;
	//GLfloat positionLight[4] = {0, -10, 0, 0};
    //GLfloat lightPositionSpot[4]  = {  0, -10, -1, 0 };
	//glLightfv(  nextLightId-1, GL_POSITION, positionPuntualLight );
	//glLightfv(  nextLightId, GL_POSITION, lightPositionSpot );
	
	AddMesh(cubo);
	//AddMesh(plane);	
	//ImportOBJ();

	/*Animation animNew;	
	AnimationObjects.Append(animNew);
	Animation& anim = AnimationObjects[0];	
	anim.Id = 2;

	AnimProperty propNew;
	anim.Propertys.Append(propNew);
	AnimProperty& prop = anim.Propertys[0];
	prop.Property = AnimPositionX;

	keyFrame key;
	key.frame = 1;
	key.value = 0;
	key.Interpolation = Linear;
	prop.keyframes.Append(key);

	key.frame = 251;
	key.value = 10000;
	prop.keyframes.Append(key);*/
}

void CWhisk3D::RemoveMaterial(){
	//si no hay objetos
	/*if (Objects.Count() < 1){return;}	
	Object& obj = Objects[SelectActivo];
	//si no es un mesh
	if (obj.type != mesh){return;}	
	Mesh& pMesh = Meshes[obj.Id];
	Material& mat = Materials[pMesh.materials[0]];

	Materials.Remove(SelectActivo);*/
};

void CWhisk3D::RemoveTexture(){

};

// -----------------------------------------------------------------------------
// CWhisk3D::NewL
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CWhisk3D* CWhisk3D::NewL( TUint aWidth, TUint aHeight, CWhisk3DInput* aInputHandler ){
    /* Symbian 2-phase constructor. Calls both the default
       C++ constructor and Symbian ConstructL methods */
    CWhisk3D* self = new (ELeave) CWhisk3D( aWidth, aHeight, aInputHandler );
    CleanupStack::PushL( self );
    self->ConstructL();
    CleanupStack::Pop();

    return self;
}


// Destructor.
CWhisk3D::~CWhisk3D(){
    //delete iContainer;
}

// -----------------------------------------------------------------------------
// CWhisk3D::AppInit
// Initialize OpenGL ES, set the vertex and color arrays and pointers,
// and select the shading mode.
// -----------------------------------------------------------------------------
//
RArray<TTexture> Textures;
TInt NumTexturasWhisk3D = 0;
void CWhisk3D::AppInit( void ){
    // Construct a texture manager that uses the application's private
    // directory as the location for all textures.
	iTextureManager = CTextureManager::NewL ( iScreenWidth, iScreenHeight,
											  FRUSTUM_TOP, FRUSTUM_BOTTOM, FRUSTUM_RIGHT, FRUSTUM_LEFT, FRUSTUM_NEAR,
											  this );
  
    // Initialize viewport and projection.
	SetScreenSize( iScreenWidth, iScreenHeight );

    // Set the screen background color.
	glClearColor( ListaColores[background][0], ListaColores[background][1], ListaColores[background][2], 1.f );

    // Enable depth testing, texturing, back face culling, and lighting.
    glEnable( GL_DEPTH_TEST );
    glEnable( GL_CULL_FACE  );
    glEnable( GL_TEXTURE_2D );
    glEnable( GL_LIGHTING   );
    /* Make OpenGL ES automatically normalize all normals after tranformations.
       This is important when making irregular xforms like scaling, or if we
       have specified nonunit-length normals. */
    glEnable( GL_NORMALIZE  );
    
    /* Initialize appropriate texture matrix. First we have to translate the
          input texture coordinate values to be within a range of [0,255]. Hence
          we translate the x- and y-coordinate values by 128. Recall that the
          values in nokTexCoords are between [-128,127], now they are in a range
          of [0,255]. After that we scale the values by 1/255 to make the values
          to be in range [0,1]. */
    glMatrixMode( GL_TEXTURE );
    glLoadIdentity();
    glScalef(     1.0f/255.0f, 1.0f/255.0f, 1.0f );
    glTranslatef( 128.0f,      128.0f,      0.0f );

    /* Remember to change the matrix mode to modelview. */
    glMatrixMode( GL_MODELVIEW );

    // Enable vertex and normal arrays.
    glEnableClientState( GL_VERTEX_ARRAY );
    glEnableClientState( GL_TEXTURE_COORD_ARRAY );
    glEnableClientState( GL_NORMAL_ARRAY );

    // Set up global ambient light.
    glLightModelfv( GL_LIGHT_MODEL_AMBIENT, globalAmbient );

    // Set up lamp.
    glEnable( GL_LIGHT0 );
    glLightfv(  GL_LIGHT0, GL_DIFFUSE,  lightDiffuseLamp  );
    glLightfv(  GL_LIGHT0, GL_AMBIENT,  lightAmbient  );
    glLightfv(  GL_LIGHT0, GL_SPECULAR, lightDiffuseLamp  );
    glLightfv(  GL_LIGHT0, GL_POSITION, sunLightPosition );

    // Set up spot.  Initially spot is disabled.
    //glEnable( GL_LIGHT1 );
    /*glLightfv(  GL_LIGHT1, GL_DIFFUSE,  lightDiffuseSpot  );
    glLightfv(  GL_LIGHT1, GL_AMBIENT,  lightAmbient  );
    glLightfv(  GL_LIGHT1, GL_SPECULAR, lightSpecularSpot );
    glLightfv(  GL_LIGHT1, GL_POSITION, lightPositionSpot );

    glLightf(   GL_LIGHT1, GL_CONSTANT_ATTENUATION,  1.5  );
    glLightf(   GL_LIGHT1, GL_LINEAR_ATTENUATION,    0.5  );
    glLightf(   GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.5  );

    glLightf(   GL_LIGHT1, GL_SPOT_CUTOFF,   170.0                );
    glLightf(   GL_LIGHT1, GL_SPOT_EXPONENT,  20.0                );
    glLightfv(  GL_LIGHT1, GL_SPOT_DIRECTION, lightDirectionSpot );*/

    // Set shading mode
    glShadeModel( GL_SMOOTH );
    //glShadeModel( GL_FLAT );

    // Do not use perspective correction
    glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST );
    
    RFs fs;
    User::LeaveIfError(fs.Connect());
    TFileName privateDir;
	
    TParsePtrC Parse( CEikonEnv::Static()->EikAppUi()->Application()->AppFullName() );
    TFileName RootDirectory = Parse.DriveAndPath().Left( 2 );

    User::LeaveIfError(fs.PrivatePath(privateDir));
    fs.Close();

	// Push the textures into the loading queue.
	_LIT( KOriginTexture, "origen.png" );
	_LIT( KMouseTexture, "cursor.png" );	
	_LIT( KLampTexture, "lamp.png" );		
	_LIT( KCursor3dTextura, "cursor3d.png" );	
	_LIT( KkeyframeTextura, "keyframe.png" );	
	_LIT( KRelationshipLineTextura, "relationshipLine.png" );	
	
	NumTexturasWhisk3D = 6;
	Textures.ReserveL(NumTexturasWhisk3D); // Reservar espacio para las texturas
	for (TInt i = 0; i < NumTexturasWhisk3D; ++i) {
	    TTexture texture;
	    Textures.Append(texture);
	}
	
    // Concatenar la ruta con el nombre del archivo
	TFileName fullFilePath = RootDirectory;
    fullFilePath.Append(privateDir);
	iTextureManager->RequestToLoad( KOriginTexture, fullFilePath, &Textures[0], false );
	iTextureManager->RequestToLoad( KMouseTexture, fullFilePath, &Textures[1], false );
	iTextureManager->RequestToLoad( KLampTexture, fullFilePath, &Textures[2], false );
	iTextureManager->RequestToLoad( KCursor3dTextura, fullFilePath, &Textures[3], false );
	iTextureManager->RequestToLoad( KkeyframeTextura, fullFilePath, &Textures[4], false );
	iTextureManager->RequestToLoad( KRelationshipLineTextura, fullFilePath, &Textures[5], false );
	
	//Start to load the textures.
	iTextureManager->DoLoadL();
}


// -----------------------------------------------------------------------------
// CWhisk3D::AppExit
// Release any allocations made in AppInit.
// -----------------------------------------------------------------------------
//
void CWhisk3D::AppExit( void ){
	delete iTextureManager;
}


// -----------------------------------------------------------------------------
// CWhisk3D::AppCycle
// Draws and animates the objects
// -----------------------------------------------------------------------------
//

// Funcion para calcular las coordenadas de textura esféricas
void CWhisk3D::calculateReflectionUVs(Mesh& pMesh) {
    for (int i = 0; i < pMesh.vertexSize; ++i) {
        // Obtener la normal del vértice
        GLfloat nx = static_cast<GLfloat>(pMesh.normals[i * 3 + 0]) / 127.0f;
        GLfloat ny = static_cast<GLfloat>(pMesh.normals[i * 3 + 1]) / 127.0f;
        GLfloat nz = static_cast<GLfloat>(pMesh.normals[i * 3 + 2]) / 127.0f;

        // Calcular coordenadas UV esféricas
        GLfloat u = 0.5f + (atan2(nx, nz) / (2.0f * M_PI));
        GLfloat v = 0.5f - (asin(ny) / M_PI);

        pMesh.uv[i * 2 + 0] = u;
        pMesh.uv[i * 2 + 1] = v;
    }
}

void CWhisk3D::RenderMesh( Object& obj, TInt indice ){
	Mesh& pMesh = Meshes[obj.Id];	
    glPushMatrix();
    glScalex(obj.scaleX, obj.scaleZ, obj.scaleY);	
		
	glColor4f(ListaColores[blanco][0],ListaColores[blanco][1],ListaColores[blanco][2],ListaColores[blanco][3]);
	glDisable(GL_COLOR_MATERIAL);
	glMaterialfv(   GL_FRONT_AND_BACK, GL_AMBIENT,  objAmbient  );
	glMaterialx( GL_FRONT_AND_BACK, GL_SHININESS,   12 << 16     );
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
	else if (view == MaterialPreview || view == Rendered){
		// Calcular coordenadas UV reflejadas
		//calculateReflectionUVs(pMesh);

		glTexCoordPointer( 2, GL_FLOAT, 0, pMesh.uv );
		
		for(int f=0; f < pMesh.materialsGroup.Count(); f++){
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
			else {glDisable( GL_TEXTURE_2D );}

			glMaterialfv(   GL_FRONT_AND_BACK, GL_EMISSION, mat.emission );

			glDrawElements( GL_TRIANGLES, pMesh.materialsGroup[f].indicesDrawnCount, GL_UNSIGNED_SHORT, &pMesh.faces[pMesh.materialsGroup[f].startDrawn] );
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
void CWhisk3D::RenderMeshAndChildren(Object& obj, TInt indice){
    // Guardar la matriz actual
    glPushMatrix();
    
    // Aplicar las transformaciones del objeto
    glTranslatef(obj.posX, obj.posZ, obj.posY);
    glRotatef(obj.rotX, 1, 0, 0); // angulo, X Y Z
    glRotatef(obj.rotZ, 0, 1, 0); // angulo, X Y Z
    glRotatef(obj.rotY, 0, 0, 1); // angulo, X Y Z

    // Si es visible y es un mesh, lo dibuja
    if (obj.visible && obj.type == mesh) {
        RenderMesh(obj, indice); // Ajusta el segundo parametro si es necesario
    }
    
    // Procesar cada hijo
    for (int c = 0; c < obj.Childrens.Count(); c++) {
        Object& objChild = Objects[obj.Childrens[c].Id];
        RenderMeshAndChildren(objChild, indice);
    }

    // Restaurar la matriz previa
    glPopMatrix();
}

// Funcion recursiva para renderizar un objeto y sus hijos
void CWhisk3D::RenderObjectAndChildrens(TInt objId){
	Object& obj = Objects[objId];
    // Guardar la matriz actual
    glPushMatrix();
    
    // Aplicar las transformaciones del objeto
    glTranslatef(obj.posX, obj.posZ, obj.posY);
    glRotatef(obj.rotX, 1, 0, 0); // angulo, X Y Z
    glRotatef(obj.rotZ, 0, 1, 0); // angulo, X Y Z
    glRotatef(obj.rotY, 0, 0, 1); // angulo, X Y Z

    // Si es visible y no es un mesh, lo dibuja
    if (obj.visible && obj.type != mesh) {
        RenderObject(objId); // Ajusta el segundo parametro si es necesario
    }
    
    // Procesar cada hijo
    for (int c = 0; c < obj.Childrens.Count(); c++) {
        RenderObjectAndChildrens(obj.Childrens[c].Id);
    }

    // Restaurar la matriz previa
    glPopMatrix();
}

//Relantionshipslines
void CWhisk3D::RenderLinkLines(TInt objId){
	Object& obj = Objects[objId];
    // Guardar la matriz actual
    glPushMatrix();
    
    // Aplicar las transformaciones del objeto
    glTranslatef(obj.posX, obj.posZ, obj.posY);
    glRotatef(obj.rotX, 1, 0, 0); // angulo, X Y Z
    glRotatef(obj.rotZ, 0, 1, 0); // angulo, X Y Z
    glRotatef(obj.rotY, 0, 0, 1); // angulo, X Y Z
    
    // Procesar cada hijo
    for (TInt c = 0; c < obj.Childrens.Count(); c++) {
		Object& objChild = Objects[obj.Childrens[c].Id];
		LineaLinkChild[3] = (GLshort)objChild.posX;
		LineaLinkChild[4] = (GLshort)objChild.posZ;
		LineaLinkChild[5] = (GLshort)objChild.posY;

		// Calcular la distancia en 3D entre obj y objChild
		GLfloat diffX = objChild.posX - obj.posX;
        GLfloat diffY = objChild.posY - obj.posY;
        GLfloat diffZ = objChild.posZ - obj.posZ;
        GLfloat distancia = sqrtu(diffX * diffX + diffY * diffY + diffZ * diffZ);
        
        // Usar la distancia escalada para modificar las coordenadas UV
        lineUV[3] = distancia/8;
		glVertexPointer( 3, GL_SHORT, 0, LineaLinkChild );
		glDrawElements( GL_LINES, LineaEdgeSize, GL_UNSIGNED_SHORT, LineaEdge );
				
		DrawnLines(1, 1, LineaTimeline, LineaEdge);
		if (obj.Childrens.Count() > 0){
			RenderLinkLines(obj.Childrens[c].Id);
		}
    }

    // Restaurar la matriz previa
    glPopMatrix();
}

void CWhisk3D::RenderObject( TInt objId ){
	Object& obj = Objects[objId];
	glPushMatrix();
	glScalex(obj.scaleX, obj.scaleZ, obj.scaleY);	

	glDisable( GL_TEXTURE_2D );

	//color si esta seleccionado
	if (SelectActivo == objId && obj.seleccionado){
		glColor4f(ListaColores[accent][0],ListaColores[accent][1],ListaColores[accent][2],ListaColores[accent][3]);
	}
	else if (obj.seleccionado){
		glColor4f(ListaColores[accentDark][0],ListaColores[accentDark][1],ListaColores[accentDark][2],ListaColores[accentDark][3]);
	}
	else {		
		glColor4f(ListaColores[negro][0],ListaColores[negro][1],ListaColores[negro][2],ListaColores[negro][3]);		
	}
	//si es un empty
	if (obj.type == empty){		
		glDisable( GL_TEXTURE_2D );	 
		glDisable( GL_BLEND );	
		glVertexPointer( 3, GL_SHORT, 0, EmptyVertices );
		glDrawElements( GL_LINES, EmptyEdgesSize, GL_UNSIGNED_SHORT, EmptyEdges );
	}
	else if (obj.type == camera){			
		glDisable( GL_TEXTURE_2D ); 
		glDisable( GL_BLEND );
		glVertexPointer( 3, GL_SHORT, 0, CameraVertices );
		glDrawElements( GL_LINES, CameraEdgesSize, GL_UNSIGNED_SHORT, CameraEdges );
		if (CameraActive == objId){		
    		//glDisable( GL_CULL_FACE  );	
			glDrawElements( GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, CameraFaceActive );	
		}
	}
	else if (obj.type == light){				
		glEnable( GL_TEXTURE_2D ); 
		glEnable( GL_BLEND );
		glDepthMask(GL_FALSE); // Desactiva la escritura en el Z-buffer

		glEnable( GL_POINT_SPRITE_OES ); //activa el uso de sprites en los vertices
		glPointSize( 32 ); //tamaño del punto
		glVertexPointer( 3, GL_SHORT, 0, pointVertex );
		glBindTexture( GL_TEXTURE_2D, Textures[2].iID ); //selecciona la textura

		glTexEnvi( GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_TRUE );
		glDrawArrays( GL_POINTS, 0, 1 );
		glTexEnvi( GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_FALSE);

		glDisable( GL_POINT_SPRITE_OES );			
		glDisable( GL_TEXTURE_2D ); 

		//glScalex( 0, 10, 0 ); //4500
		LineaLightVertex[4] = (GLshort)-obj.posZ;
		glVertexPointer( 3, GL_SHORT, 0, LineaLightVertex );
		glDrawElements( GL_LINES, LineaEdgeSize, GL_UNSIGNED_SHORT, LineaEdge );

		glDisable( GL_BLEND );
		glDepthMask(GL_TRUE); // Reactiva la escritura en el Z-buffer
	}
	glPopMatrix();
}

TInt CWhisk3D::BuscarAnimacionObj(){
	TInt index = -1;
    for(TInt a = 0; a < AnimationObjects.Count(); a++) {
		if (AnimationObjects[a].Id == SelectActivo){
			index = a;
			break;
		}
	}
	return index;
}

TInt CWhisk3D::BuscarAnimProperty(TInt indice, TInt propertySelect){
	TInt index = -1;
	for(TInt pr = 0; pr < AnimationObjects[indice].Propertys.Count(); pr++) {		
		if (AnimationObjects[indice].Propertys[pr].Property == propertySelect){	
			index = pr;
			break;
		}
	}
	return index;
}

TInt CWhisk3D::BuscarAnimacion(TInt propertySelect){
	TInt index = -1;
    for(TInt a = 0; a < AnimationObjects.Count(); a++) {
		if (AnimationObjects[a].Id == SelectActivo){
    		for(TInt pr = 0; pr < AnimationObjects[a].Propertys.Count(); pr++) {		
				if (AnimationObjects[a].Propertys[pr].Property == propertySelect){	
					index = a;
					break;
				}
			}
		}
		if (index > -1){break;}
	}
	return index;
}

void CWhisk3D::InsertKeyframe(TInt propertySelect){
	//ShowOptionsDialogL();
    // Crear una lista de opciones
    //CDesCArrayFlat* options = new (ELeave) CDesCArrayFlat(3);
    /*CleanupStack::PushL(options);
    options->AppendL(_L("Opcion 1"));
    options->AppendL(_L("Opcion 2"));
    options->AppendL(_L("Opcion 3"));

    // Mostrar el dialogo de seleccion de opciones
    TInt selectedIndex = DialogSelectOption(_L("Selecciona una opcion:"), *options);
    CleanupStack::PopAndDestroy(options);

	//si no se selecciono nada
    if (selectedIndex == -1) {return;}*/

	//busca si ya existe esa animacion de objeto con esa propiedad
	
	TInt index = BuscarAnimacion(propertySelect);
	keyFrame key;
	key.frame = CurrentFrame;

	HBufC* noteBuf = HBufC::NewLC(100);
	noteBuf->Des().Copy(_L("Interpolation:\n0 Constant, 1 Linear, 2 EaseInOut, 3 EaseIn, 4 EaseOut"));
	key.Interpolation = DialogNumber(Constant, Constant, EaseOut, noteBuf);
	CleanupStack::PopAndDestroy(noteBuf);

	if (index > -1){
		for(TInt p = 0; p < AnimationObjects[index].Propertys.Count(); p++) {
			AnimProperty& animProp = AnimationObjects[index].Propertys[p];
			if(animProp.Property != propertySelect){continue;}
			switch (animProp.Property) {
				case AnimPosition:
					key.valueX = Objects[SelectActivo].posX;
					key.valueY =  Objects[SelectActivo].posY;
					key.valueZ =  Objects[SelectActivo].posZ;
					break;
				case AnimRotation:
					key.valueX = Objects[SelectActivo].rotX;
					key.valueY =  Objects[SelectActivo].rotY;
					key.valueZ =  Objects[SelectActivo].rotZ;
					break;
				case AnimScale:
					key.valueX = Objects[SelectActivo].scaleX;
					key.valueY =  Objects[SelectActivo].scaleY;
					key.valueZ =  Objects[SelectActivo].scaleZ;
					break;
				default:
					break;
			}

			TBool yaExiste = false;
			for(TInt k = 0; k < animProp.keyframes.Count(); k++) {
				//evita que se duplique. solo reemplaza
				if (animProp.keyframes[k].frame == CurrentFrame){
					animProp.keyframes[k].valueX = key.valueX;
					animProp.keyframes[k].valueY = key.valueY;
					animProp.keyframes[k].valueZ = key.valueZ;
					yaExiste = true;
					break;
				}
			};
			if (!yaExiste){
				animProp.keyframes.Append(key);	
				animProp.SortKeyFrames();
			}
			break;
		}
	}
	else {		
		AnimationObject NewAnim;	
		AnimationObjects.Append(NewAnim);
		AnimationObject& anim = AnimationObjects[AnimationObjects.Count()-1];	
		anim.Id = SelectActivo;
		
		AnimProperty propNew;
		anim.Propertys.Append(propNew);
		AnimProperty& prop = anim.Propertys[anim.Propertys.Count()-1];
		prop.Property = propertySelect;

		switch (propertySelect) {
			case AnimPosition:
				key.valueX = Objects[SelectActivo].posX;
				key.valueY =  Objects[SelectActivo].posY;
				key.valueZ =  Objects[SelectActivo].posZ;
				break;
			case AnimRotation:
				key.valueX = Objects[SelectActivo].rotX;
				key.valueY =  Objects[SelectActivo].rotY;
				key.valueZ =  Objects[SelectActivo].rotZ;
				break;
			case AnimScale:
				key.valueX = Objects[SelectActivo].scaleX;
				key.valueY =  Objects[SelectActivo].scaleY;
				key.valueZ =  Objects[SelectActivo].scaleZ;
				break;
			default:
				break;
		}
		prop.keyframes.Append(key);
	}
    redibujar = true;	
}

void CWhisk3D::RemoveKeyframes(){
    for(TInt a = 0; a < AnimationObjects.Count(); a++) {
		if (AnimationObjects[a].Id == SelectActivo){
        	if (AnimationObjects[a].Propertys.Count() > 0) {
				for(TInt p = 0; p < AnimationObjects[a].Propertys.Count(); p++) {
					AnimProperty& anim = AnimationObjects[a].Propertys[p];
					if (anim.keyframes.Count() > 0) {
						TInt firstFrame = 0;
						TInt firstFrameIndex = 0;
						// Encontrar el primer frame
						for(TInt f = 0; f < anim.keyframes.Count(); f++) {
							if (anim.keyframes[f].frame < CurrentFrame && anim.keyframes[f].frame > firstFrame) {
								firstFrameIndex = f;
							}
						}
						anim.keyframes.Remove(firstFrameIndex);
					}
				}
			}
			break;
		}
	}	
    redibujar = true;
}

void CWhisk3D::ClearKeyframes(){
    for(TInt a = 0; a < AnimationObjects.Count(); a++) {
		if (AnimationObjects[a].Id == SelectActivo){
			/*for(TInt p = 0; p < AnimationObjects[a].Propertys.Count(); p++){
				AnimationObjects[a].Propertys[p].keyframes.Close();
			}*/
			AnimationObjects[a].Propertys.Close();
			AnimationObjects.Remove(a);
			break;
		}
	}	
    redibujar = true;
}

void CWhisk3D::ReloadAnimation(){
    for(TInt a = 0; a < AnimationObjects.Count(); a++) {
		for(TInt p = 0; p < AnimationObjects[a].Propertys.Count(); p++){
			AnimProperty& anim = AnimationObjects[a].Propertys[p];
			if (anim.keyframes.Count() > 0){
				GLfloat valueX = 0;
				GLfloat valueY = 0;
				GLfloat valueZ = 0;
				TInt firstFrameIndex = 0;
				TInt lastFrameIndex = 0;
				// Encontrar el primer frame
				for(TInt f = 0; f < anim.keyframes.Count(); f++) {
					if (anim.keyframes[f].frame <= CurrentFrame) {
						firstFrameIndex = f;
					}
					if (anim.keyframes[f].frame >= CurrentFrame) {
						lastFrameIndex = f;
						break;
					}
				}

				// Si el CurrentFrame esta fuera de los limites de los keyframes, usar el valor del primer o último frame
				if (CurrentFrame <= anim.keyframes[firstFrameIndex].frame) {
					valueX = anim.keyframes[firstFrameIndex].valueX;
					valueY = anim.keyframes[firstFrameIndex].valueY;
					valueZ = anim.keyframes[firstFrameIndex].valueZ;
				} 
				else if (CurrentFrame >= anim.keyframes[lastFrameIndex].frame) {
					valueX = anim.keyframes[anim.keyframes.Count() - 1].valueX;
					valueY = anim.keyframes[anim.keyframes.Count() - 1].valueY;
					valueZ = anim.keyframes[anim.keyframes.Count() - 1].valueZ;
				} 
				// Interpolacion entre keyframes
				else {		
					// Interpolacion entre keyframes
					TInt frame1 = anim.keyframes[firstFrameIndex].frame;
					GLfloat valueX1 = anim.keyframes[firstFrameIndex].valueX;
					GLfloat valueY1 = anim.keyframes[firstFrameIndex].valueY;
					GLfloat valueZ1 = anim.keyframes[firstFrameIndex].valueZ;
					TInt frame2 = anim.keyframes[lastFrameIndex].frame;
					GLfloat valueX2 = anim.keyframes[lastFrameIndex].valueX;
					GLfloat valueY2 = anim.keyframes[lastFrameIndex].valueY;
					GLfloat valueZ2 = anim.keyframes[lastFrameIndex].valueZ;

					GLfloat t;
					GLfloat t2;

					switch (anim.keyframes[firstFrameIndex].Interpolation) {
						case Constant:
							valueX = valueX1;
							valueY = valueY1;
							valueZ = valueZ1;
							break;
						case Linear:
							valueX = valueX1 + ((valueX2 - valueX1) * (CurrentFrame - frame1)) / (frame2 - frame1);
							valueY = valueY1 + ((valueY2 - valueY1) * (CurrentFrame - frame1)) / (frame2 - frame1);
							valueZ = valueZ1 + ((valueZ2 - valueZ1) * (CurrentFrame - frame1)) / (frame2 - frame1);
							break;
						case EaseIn:
							t = (CurrentFrame - frame1) / (GLfloat)(frame2 - frame1); // Normalizar el tiempo entre 0 y 1
							t2 = t * t; // EaseIn (usando una interpolacion cuadratica)
							
							valueX = valueX1 + (valueX2 - valueX1) * t2;
							valueY = valueY1 + (valueY2 - valueY1) * t2;
							valueZ = valueZ1 + (valueZ2 - valueZ1) * t2;
							break;
						case EaseOut:
							t = (CurrentFrame - frame1) / (GLfloat)(frame2 - frame1); // Normalizar el tiempo entre 0 y 1
							
							t2 = 1 - (1 - t) * (1 - t); // EaseOut (inversa de EaseIn)

							valueX = valueX1 + (valueX2 - valueX1) * t2;
							valueY = valueY1 + (valueY2 - valueY1) * t2;
							valueZ = valueZ1 + (valueZ2 - valueZ1) * t2;
							break;						
						case EaseInOut:
							t = (CurrentFrame - frame1) / (GLfloat)(frame2 - frame1); // Normalizar el tiempo entre 0 y 1

							// EaseIn (aceleracion)
							if (t < 0.5) {
								t2 = 2 * t * t;  // t^2
							} 
							// EaseOut (desaceleracion)
							else {
								t2 = 1 - 2 * (1 - t) * (1 - t);  // 1 - (1 - t)^2
							}

							valueX = valueX1 + (valueX2 - valueX1) * t2;
							valueY = valueY1 + (valueY2 - valueY1) * t2;
							valueZ = valueZ1 + (valueZ2 - valueZ1) * t2;
							break;
						default:
							valueX = valueX1;
							valueY = valueY1;
							valueZ = valueZ1;
							break;							
					}
				}

				// Asignar el valor calculado a la propiedad correspondiente del objeto
				switch (anim.Property) {
					case AnimPosition:
						Objects[AnimationObjects[a].Id].posX = valueX;
						Objects[AnimationObjects[a].Id].posY = valueY;
						Objects[AnimationObjects[a].Id].posZ = valueZ;
						break;
					case AnimRotation:
						Objects[AnimationObjects[a].Id].rotX = valueX;
						Objects[AnimationObjects[a].Id].rotY = valueY;
						Objects[AnimationObjects[a].Id].rotZ = valueZ;
						break;
					case AnimScale:
						Objects[AnimationObjects[a].Id].scaleX = valueX;
						Objects[AnimationObjects[a].Id].scaleY = valueY;
						Objects[AnimationObjects[a].Id].scaleZ = valueZ;
						break;
					// Otros casos según las propiedades
					default:
						// Manejar cualquier otro caso aqui si es necesario
						break;
				}
			}
		}
    }
}

void CWhisk3D::SetCurrentFrame(){
	Cancelar();
	estado = timelineMove;
    redibujar = true;
}

void CWhisk3D::SetEndFrame(){
	Cancelar();
	HBufC* noteBuf = HBufC::NewLC(100);
	noteBuf->Des().Copy(_L("Set Final Frame"));
	EndFrame = DialogNumber(EndFrame, 0, 2147483647, noteBuf);	
	CleanupStack::PopAndDestroy(noteBuf);
    redibujar = true;
}

void CWhisk3D::SetStartFrame(){
	Cancelar();
	HBufC* noteBuf = HBufC::NewLC(100);
	noteBuf->Des().Copy(_L("Set Start Frame"));
	StartFrame = DialogNumber(StartFrame, 0, 2147483647, noteBuf);
	CleanupStack::PopAndDestroy(noteBuf);
    redibujar = true;
}

TBool postProcesado = true;
TBool dialogoSymbian = false;

TUint32 millisecondsPerFrame = 67;
TInt FrameRate = 60;

// Obtenemos la frecuencia del FastCounter (ticks por segundo)
TUint32 lastFrameTime = User::NTickCount(); //TickCount

void CWhisk3D::CalculateMillisecondsPerFrame(TInt aFPS){
	FrameRate = aFPS;
	millisecondsPerFrame = 1000/aFPS;
}

void CWhisk3D::SetFrameRate(){
	Cancelar();
	HBufC* noteBuf = HBufC::NewLC(100);
	_LIT(KFormatString, "Set Frame Rate (Now: %d)");
	noteBuf->Des().Format(KFormatString, FrameRate);
	TInt NewFrameRate = DialogNumber(FrameRate, 1, 120, noteBuf);
	CleanupStack::PopAndDestroy(noteBuf);
	CalculateMillisecondsPerFrame(NewFrameRate);
    redibujar = true;
}


void CWhisk3D::AppCycle( TInt iFrame, GLfloat aTimeSecs, GLfloat aDeltaTimeSecs ){
    // If texture loading is still going on, return from this method without doing anything.
	if ( GetState() == ELoadingTextures ){
        return;
    }
	GLfixed fixedDeltaTimeSecs = FLOAT_2_FIXED( aDeltaTimeSecs );
	// Controles
	if (!dialogoSymbian){
		InputUsuario( fixedDeltaTimeSecs );
	}
	
	if ( !redibujar && !PlayAnimation ){	
		/*if (postProcesado){
			GLubyte* pixels = new GLubyte[iScreenWidth * iScreenHeight * 4]; // 4 para RGBA
			
			for (TInt i = 0; i < iScreenWidth * iScreenHeight * 4; ++i){
				pixels[i] = 0;
			}
			
			for (TInt i = iScreenWidth*30*4; i <iScreenWidth*40 * 4; ++i){
				pixels[i] = 255;
			}
			
	    	glBindTexture( GL_TEXTURE_2D, Textures[3].iID); //iCursor3dTextura.iID
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, iScreenWidth, iScreenHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
			//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, iScreenWidth, iScreenHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
			glFlush();	
			delete pixels;
			postProcesado = false;
		}*/
		return;
	}
	//postProcesado = true;

	else if (PlayAnimation){
        TUint32 currentTime = User::NTickCount(); // Obtén el tiempo actual en milisegundos

        // Calcula el tiempo transcurrido en milisegundos
        TUint32 elapsedTimeMillisecs = currentTime - lastFrameTime;
        
        if (elapsedTimeMillisecs >= millisecondsPerFrame) {
			lastFrameTime = currentTime;
	
			if (fixedDeltaTimeSecs)
			CurrentFrame++;
			if (CurrentFrame > EndFrame){
				CurrentFrame = StartFrame;
			}
			ReloadAnimation();
        }
	}

	if (SimularZBuffer){
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Color negro
		glEnable(GL_FOG);
		glFogf(GL_FOG_MODE, GL_LINEAR); // Tipo de niebla lineal
		glFogf(GL_FOG_START, FRUSTUM_NEAR);  // Distancia inicial de la niebla
		glFogf(GL_FOG_END, FRUSTUM_FAR);     // Distancia final de la niebla
		GLfloat fogColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
		glFogfv(GL_FOG_COLOR, fogColor); // Color de la niebla
	}
	else {
		glDisable(GL_FOG);
		glClearColor(ListaColores[background][0],ListaColores[background][1],ListaColores[background][2],ListaColores[background][3]);
	}
	
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glLoadIdentity();
    glEnable( GL_DEPTH_TEST );	

	if (ViewFromCameraActive){
		RecalcViewPos();
		//glTranslatef( posX, posZ, posY);
		glTranslatef( posX, posZ, -cameraDistance+posY);
	}
	else {
		glTranslatef( posX, posZ, -cameraDistance+posY);
	}
	glRotatef(rotY, 1, 0, 0); //angulo, X Y Z
	glRotatef(rotX, 0, 1, 0); //angulo, X Y Z
	// Use some magic numbers to scale the head model to fit the screen.
	glScalex( 1 << 10, 1 << 10, 1 << 10 );
	glTranslatef( PivotX, PivotZ, PivotY);

    // Habilitar la normalizaci�n de las normales
    glEnable(GL_NORMALIZE);	

	//primero hay que colocar las luces en caso de estar en modo render!
	if (!SimularZBuffer && view == MaterialPreview || view == Rendered){
		for(TInt o=0; o < Objects.Count(); o++){
			Object& obj = Objects[o];
			if(!obj.visible || obj.type != light ) {continue;}
			Light& light = Lights[obj.Id];

			glPushMatrix(); //guarda la matrix
			glTranslatef( obj.posX, obj.posZ, obj.posY);
			GLfloat lightPos[] = {0.0f, 0.0f, 0.0f, 1.0f}; // Luz puntual en la posici�n transformada
			glLightfv(light.lightId, GL_POSITION, lightPos);
			//glLightfv(  light.lightId, GL_POSITION, positionPuntualLight );
			glPopMatrix(); //reinicia la matrix a donde se guardo  
		}		
	}

	//por defecto la linea es de 1	
	glLineWidth(1);		
	
	//bucle que dibuja cada objeto en orden
	if(Meshes.Count() > 0){
		// Funcion principal para iterar sobre la coleccion
		for (int o = 0; o < Collection.Count(); o++) {
			Object& obj = Objects[Collection[o]];
			RenderMeshAndChildren(obj, Collection[o]);
		}
	}
	//el valor que tiene que tener para dibujar el resto correctamente
	//glDisable(GL_COLOR_MATERIAL);
	glDisableClientState( GL_COLOR_ARRAY );

	//fin del dibujado de objetos
	//si estaba simulando el zbuffer. el resto no hace falta
	if (SimularZBuffer){		
		redibujar = false;
		return;
	};

	//el resto de objetos no usan materiales ni luces
	glMaterialfv(   GL_FRONT_AND_BACK, GL_DIFFUSE,  ListaColores[negro] );
	glMaterialfv(   GL_FRONT_AND_BACK, GL_AMBIENT,  ListaColores[negro] );
	glMaterialfv(   GL_FRONT_AND_BACK, GL_SPECULAR, ListaColores[negro] );
	glDisable( GL_CULL_FACE ); // Enable back face culling.
	glDisable( GL_LIGHTING );
	glEnable(GL_COLOR_MATERIAL);
	glDisable( GL_TEXTURE_2D );
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	
	//se encarga de dibujar el layout 
	if (showOverlays){
		//dibujar las lineas del piso y el piso
		if (showFloor || showXaxis || showYaxis){
			glEnable(GL_FOG);
			glFogf(GL_FOG_MODE, GL_LINEAR); // Tipo de niebla lineal
			glFogf(GL_FOG_START, FRUSTUM_NEAR);  // Distancia inicial de la niebla
			glFogf(GL_FOG_END, FRUSTUM_FAR);     // Distancia final de la niebla
			//GLfloat fogColor[] = {0.23f, 0.23f, 0.23f, 1.f};
			//glClearColor( ClearColor[0], ClearColor[1], ClearColor[2], 1.f );
			glFogfv(GL_FOG_COLOR, ListaColores[background]); // Color de la niebla
			glLineWidth(1);	 

			glVertexPointer( 3, GL_SHORT, 0, objVertexdataFloor );
			//glNormalPointer( GL_BYTE, 0, objNormaldataFloor );

			//dibuja el piso	
			if (showFloor){
				glColor4f(LineaPiso[0],LineaPiso[1],LineaPiso[2],LineaPiso[3]);
				glDrawElements( GL_LINES, objFacesFloor, GL_UNSIGNED_SHORT, objFacedataFloor );			
			}		
			//linea Verde
			if (showXaxis){
				glLineWidth(2);
				glColor4f(LineaPisoRoja[0],LineaPisoRoja[1],LineaPisoRoja[2],LineaPisoRoja[3]);
				glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeRojo );
				glLineWidth(1);	//la deja como es por defecto
			}
			else if (showFloor){
				glColor4f(LineaPiso[0],LineaPiso[1],LineaPiso[2],LineaPiso[3]);
				glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeRojo );
			}
			//linea Roja	
			if (showYaxis){
				glLineWidth(2);
				glColor4f(LineaPisoVerde[0],LineaPisoVerde[1],LineaPisoVerde[2],LineaPisoVerde[3]);
				glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeVerde );
				glLineWidth(1);	//la deja como es por defecto
			}
			else if (showFloor){
				glColor4f(LineaPiso[0],LineaPiso[1],LineaPiso[2],LineaPiso[3]);
				glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeVerde );
			}	
			glDisable(GL_FOG);
		}
		//esto solo se hace si hay objetos
		if (Objects.Count() > 0){
			//dibujo de objetos nuevo!
			glLineWidth(1);	 
			for (int o = 0; o < Collection.Count(); o++) {
				RenderObjectAndChildrens(Collection[o]);
			}	 
			//dibujar lineas parent		
			if (ShowRelantionshipsLines){
				glEnable( GL_TEXTURE_2D );
				glEnable( GL_BLEND );
				glDepthMask(GL_FALSE); // Desactiva la escritura en el Z-buffer				
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				glTexCoordPointer( 2, GL_FLOAT, 0, lineUV ); //SpriteUvSize
				glColor4f(ListaColores[negro][0],ListaColores[negro][1],ListaColores[negro][2],ListaColores[negro][3]);	
				glBindTexture( GL_TEXTURE_2D, Textures[5].iID ); //selecciona la de linea punteada	
				for (int o = 0; o < Collection.Count(); o++) {
					Object& obj = Objects[Collection[o]];
					if (obj.Childrens.Count() > 0){
						RenderLinkLines(Collection[o]);
					}
				}
				glDepthMask(GL_TRUE); // Reactiva la escritura en el Z-buffer		
			}

			glDisable( GL_DEPTH_TEST );	
			glDisable( GL_BLEND );
			glDisable( GL_TEXTURE_2D );
			//dibuja los ejes de transformacion
			if (estado == translacion || estado == rotacion || estado == EditScale) {
				for (TInt o = 0; o < Collection.Count(); o++) {
					TBool found = false;
					Object& obj = Objects[Collection[o]];
					SearchSelectObj(obj, Collection[o], found);
					if (found) break;  // Si ya encontro el objeto, salir del bucle
				}
			}

			//Dibuja el origen de los objetos seleccionados		
			if (showOrigins){	
				glEnable( GL_TEXTURE_2D );
				glEnable( GL_BLEND );
				// Enable point sprites.
				glEnable( GL_POINT_SPRITE_OES );
				// Make the points bigger.
				glPointSize( 8 );
				for (TInt o = 0; o < Collection.Count(); o++) {
					Object& obj = Objects[o];
					DibujarOrigen(obj, o);
				}
				glTexEnvi( GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_FALSE);
				glDisable( GL_POINT_SPRITE_OES );
			}			
		}
		//dibuja el cursor 3D	
		if (show3DCursor){
			glDisable( GL_DEPTH_TEST );
			glPushMatrix(); //guarda la matrix
			glTranslatef( Cursor3DposX, Cursor3DposZ, Cursor3DposY);
			
			glEnable( GL_TEXTURE_2D );
			glEnable( GL_BLEND );
			glEnable( GL_POINT_SPRITE_OES ); // Enable point sprites.	
			glPointSize( 32 ); // Make the points bigger.
			glColor4f(ListaColores[blanco][0],ListaColores[blanco][1],ListaColores[blanco][2],ListaColores[blanco][3]);
			glVertexPointer( 3, GL_SHORT, 0, pointVertex );
			glBindTexture( GL_TEXTURE_2D, Textures[3].iID);//iCursor3dTextura.iID ); //selecciona la textura

			glTexEnvi( GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_TRUE );
			glDrawArrays( GL_POINTS, 0, 1 );
			glTexEnvi( GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_FALSE);

			//dibuja lineas		
			glDisable( GL_TEXTURE_2D );
			glDisable( GL_POINT_SPRITE_OES );
			glDisable( GL_BLEND );
		
			glColor4f(ListaColores[negro][0],ListaColores[negro][1],ListaColores[negro][2],ListaColores[negro][3]);
			glVertexPointer( 3, GL_SHORT, 0, Cursor3DVertices );
			glDrawElements( GL_LINES, Cursor3DEdgesSize, GL_UNSIGNED_SHORT, Cursor3DEdges );	

			glPopMatrix(); //reinicia la matrix a donde se guardo	
		}
	}

	if (ShowUi){
		dibujarUI();
	}

    //termino de dibujar
    redibujar = false;
}

void CWhisk3D::SearchSelectObj(Object& obj, TInt objIndex, TBool& found) {
    glPushMatrix();    
    glTranslatef(obj.posX, obj.posZ, obj.posY);
    
    if (objIndex == SelectActivo) {
		if (estado == rotacion || estado == EditScale){
			glRotatef(obj.rotX, 1, 0, 0); //angulo, X Y Z
			glRotatef(obj.rotZ, 0, 1, 0); //angulo, X Y Z
			glRotatef(obj.rotY, 0, 0, 1); //angulo, X Y Z
		}		
		//dibuja los ejes de transformacion
		if (estado == translacion || estado == rotacion || estado == EditScale){		
        	DrawTransformAxis(obj);
		}		
        found = true;
    } 
	else if (obj.Childrens.Count() > 0){	
		glRotatef(obj.rotX, 1, 0, 0); //angulo, X Y Z
		glRotatef(obj.rotZ, 0, 1, 0); //angulo, X Y Z
		glRotatef(obj.rotY, 0, 0, 1); //angulo, X Y Z
        for (int c = 0; c < obj.Childrens.Count(); c++) {
            if (found) break;  // Si ya lo encontro, salir del bucle
            Object& objChild = Objects[obj.Childrens[c].Id];
            SearchSelectObj(objChild, obj.Childrens[c].Id, found);
        }
    }
    glPopMatrix();
}

void CWhisk3D::DrawTransformAxis(Object& obj) {
	glPushMatrix();    
	glVertexPointer( 3, GL_SHORT, 0, objVertexdataFloor );
	glLineWidth(2);
		
	if (InteractionMode == ObjectMode){
		glTranslatef(TransformPivotPointFloat[0]/65000, 
					 TransformPivotPointFloat[1]/65000, 
					 TransformPivotPointFloat[2]/65000
		);	
	}
	if (axisSelect == X){
		glColor4f(ColorTransformX[0],ColorTransformX[1],ColorTransformX[2],ColorTransformX[3]);
		glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeRojo );				
	}
	else if (axisSelect == Y){
		glColor4f(ColorTransformY[0],ColorTransformY[1],ColorTransformY[2],ColorTransformY[3]);
		glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeVerde ); 				
	}
	else if (axisSelect == Z){
		glColor4f(ColorTransformZ[0],ColorTransformZ[1],ColorTransformZ[2],ColorTransformZ[3]);
		glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeAzul ); 				
	}	
	else if (axisSelect == XYZ){
		glColor4f(ColorTransformX[0],ColorTransformX[1],ColorTransformX[2],ColorTransformX[3]);
		glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeRojo );	
		glColor4f(ColorTransformY[0],ColorTransformY[1],ColorTransformY[2],ColorTransformY[3]);
		glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeVerde ); 
		glColor4f(ColorTransformZ[0],ColorTransformZ[1],ColorTransformZ[2],ColorTransformZ[3]);
		glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, EjeAzul ); 				
	}
	glLineWidth(1);	//la deja como es por defecto	
	glPopMatrix();
}

void CWhisk3D::DibujarOrigen(Object& obj, TInt objIndex){
    glPushMatrix();    
    glTranslatef(obj.posX, obj.posZ, obj.posY);
    
    if (obj.visible && (obj.seleccionado || objIndex == SelectActivo)){	
		if (objIndex == SelectActivo){
			glColor4f(ListaColores[accent][0],ListaColores[accent][1],ListaColores[accent][2],ListaColores[accent][3]);
		}
		else {
			glColor4f(ListaColores[accentDark][0],ListaColores[accentDark][1],ListaColores[accentDark][2],ListaColores[accentDark][3]);
		}
		glVertexPointer( 3, GL_SHORT, 0, pointVertex );
		glBindTexture( GL_TEXTURE_2D, Textures[0].iID ); //selecciona la textura
		glTexEnvi( GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_TRUE );    	
		glPushMatrix();    		
		glScalex(obj.scaleX, obj.scaleZ, obj.scaleY);	
		glDrawArrays( GL_POINTS, 0, 1 );	
    	glPopMatrix();
    } 
	else if (obj.Childrens.Count() > 0){	
		glRotatef(obj.rotX, 1, 0, 0); //angulo, X Y Z
		glRotatef(obj.rotZ, 0, 1, 0); //angulo, X Y Z
		glRotatef(obj.rotY, 0, 0, 1); //angulo, X Y Z
        for (int c = 0; c < obj.Childrens.Count(); c++) {
            Object& objChild = Objects[obj.Childrens[c].Id];
            DibujarOrigen(objChild, obj.Childrens[c].Id);
        }
    }
    glPopMatrix();
}

//Se encarga de la nueva UI 3d
void CWhisk3D::dibujarUI(){
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();     
	glOrthof(0.0f, (GLfloat)iScreenWidth, (GLfloat)iScreenHeight, 0.0f, -5.0f, 1000.0f);
    glMatrixMode( GL_MODELVIEW );

	glLoadIdentity();
	//glTranslatef( -iScreenHeightSplit* aspectRatioNuevo, iScreenHeightSplit, -100);
	//glScalex( 71400, 71400, 71400 );	

	//glDisable( GL_POINT_SPRITE_OES );
	//glTexEnvi( GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_FALSE);
	//glEnable( GL_TEXTURE_2D ); // Permite usar texturas
	glEnable( GL_BLEND ); //permite transparents
	glDisable( GL_DEPTH_TEST );
	//glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); //hace que sea pixelada
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); //hace que sea pixelada
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	
	
    glVertexPointer( 3, GL_SHORT, 0, SpriteVertices );
	glTexCoordPointer( 2, GL_BYTE, 0, SpriteUV ); //SpriteUvSize
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//header	
	glDisable( GL_TEXTURE_2D ); // Permite usar texturas
	glColor4f(ListaColores[headerColor][0],ListaColores[headerColor][1],ListaColores[headerColor][2],0.8f);
	SetSpriteSize(iScreenWidth,24);
	DrawnRectangle();
	
	glEnable( GL_TEXTURE_2D ); // Permite usar texturas
	glColor4f(ListaColores[blanco][0],ListaColores[blanco][1],ListaColores[blanco][2],ListaColores[blanco][3]);	
	glBindTexture( GL_TEXTURE_2D, Textures[1].iID ); 
    
	//que editor esta abierto
	glPushMatrix();
	UiMoveTo(5,5);
	SetUvSprite(113*2,49*2,28,28);
	SetSpriteSize(14,14);
	DrawnRectangle();

	//icono de modo objeto
	UiMoveTo(25,0);
	if (InteractionMode == ObjectMode){
		SetUvSprite(1*2,97*2,28,28);
	}
	DrawnRectangle();

	//icono de overlay
	UiMoveTo(23,-2);
	IconSelect(showOverlays);
	UiMoveTo(2,2);
	SetUvSprite(33*2,81*2,28,28);
	DrawnRectangle();
	
	//en caso de que overlay este desactivado. ese es el valor que tiene que estar
	glColor4f(ListaColores[blanco][0],ListaColores[blanco][1],ListaColores[blanco][2],ListaColores[blanco][3]);	
	glEnable( GL_TEXTURE_2D );

	//grupo de shaders ancho, alto,
	UiMoveTo(23,-3);
	SetUvBordes(1*2,56*2, 8*2,9*2, 4*2,4*2,5*2,4*2 );	
	DibujarRectanguloBordes(79,21, 4,4,5,4 );

	//solid
	UiMoveTo(18,-2);
	IconSelect(view == Solid);
	UiMoveTo(2,2);
	SetUvSprite(81*2,113*2,28,28);
	DrawnRectangle();

	//material preview
	UiMoveTo(18,-2);
	IconSelect(view == MaterialPreview);
	UiMoveTo(2,2);
	SetUvSprite(97*2,113*2,28,28);
	DrawnRectangle(); 

	//Rendered
	UiMoveTo(18,-2);
	IconSelect(view == Rendered);
	UiMoveTo(2,2);
	SetUvSprite(113*2,113*2,28,28);
	DrawnRectangle();
	glPopMatrix(); //reinicia la matrix a donde se guardo	

	if (ShowTimeline){
		glPushMatrix();
		DrawnTimeline();
		glPopMatrix(); //reinicia la matrix a donde se guardo	
	}

	//dibuja el mouse por arriba de todo
	if (mouseVisible){
		UiMoveTo(mouseX,mouseY);
		SetUvSprite(1,1,20,23);
		SetSpriteSize(10,17);
		DrawnRectangle();
	}

	//resetea la perspectiva	
	SetPerspectiva(false);
}

void CWhisk3D::DrawnTimeline(){
	glDisable( GL_TEXTURE_2D );	
	glDisable( GL_BLEND );

	UiMoveTo(0,iScreenHeight-16);
	/*glColor4f(0.114f,0.114f,0.114f,1.0f);
	SetSpriteSize(iScreenWidth,48);
	DrawnRectangle();

	UiMoveTo(0,24);*/
	glColor4f(0.188f,0.188f,0.188f,1.0f);
	SetSpriteSize(iScreenWidth,16);
	DrawnRectangle();

	glColor4f(0.086f,0.086f,0.086f,1.0f);
    GLshort distanciaEntreLineas = iScreenWidth / 5;
	glPushMatrix();
    for (int l = 0; l < 4; l++) {
		UiMoveTo(distanciaEntreLineas,0);
		DrawnLines(1, 1, LineaTimeline, LineaEdge);
    }
	glPopMatrix(); //reinicia la matrix a donde se guardo	

	glPushMatrix();
	glColor4f(0.149f,0.149f,0.149f,1.0f);
	UiMoveTo(distanciaEntreLineas/2,0);
    for (int l = 0; l < 4; l++) {
		DrawnLines(1, 1, LineaTimeline, LineaEdge);
		UiMoveTo(distanciaEntreLineas,0);
    }
	glPopMatrix(); //reinicia la matrix a donde se guardo	

	//linea azul del tiempo
	glColor4f(0.259f,0.427f,0.682f,1.0f); 
    GLshort posicionTiempo = CurrentFrame;
	UiMoveTo(posicionTiempo,0);
	DrawnLines(2, 1, LineaTimeline, LineaEdge);
}

void CWhisk3D::DrawnLines(TInt LineWidth, TInt cantidad, GLshort* vertexlines, GLushort* lineasIndices){
	glVertexPointer( 3, GL_SHORT, 0, vertexlines );
	glLineWidth(LineWidth);	
	glDrawElements( GL_LINES, cantidad*2, GL_UNSIGNED_SHORT, lineasIndices );
}

void CWhisk3D::DrawnLines(TInt LineWidth, TInt cantidad, const GLshort* vertexlines, const GLushort* lineasIndices) {
    glVertexPointer(3, GL_SHORT, 0, vertexlines);
    glLineWidth(LineWidth);
    glDrawElements(GL_LINES, cantidad * 2, GL_UNSIGNED_SHORT, lineasIndices);
}

void CWhisk3D::SetUvSprite(GLshort x, GLshort y, GLshort ancho, GLshort alto){
	SpriteUV[0] = SpriteUV[6] = (GLbyte)(-128+x);
	SpriteUV[2] = SpriteUV[4] = (GLbyte)(-128+x+ancho);
	SpriteUV[1] = SpriteUV[3] = (GLbyte)(-128+y);
	SpriteUV[5] = SpriteUV[7] = (GLbyte)(-128+y+alto);
}

void CWhisk3D::SetSpriteSize(GLshort ancho, GLshort alto){
	SpriteVertices[3] = SpriteVertices[6] = ancho+1;
	SpriteVertices[7] = SpriteVertices[10] = alto+1;
}

void CWhisk3D::DrawnRectangle(){
	glDrawElements( GL_TRIANGLES, 2*3, GL_UNSIGNED_SHORT, SpriteFaces );
}

void CWhisk3D::IconSelect(TBool activo){
	if (activo){
		glDisable( GL_TEXTURE_2D );
		glColor4f(ListaColores[accent][0],ListaColores[accent][1],ListaColores[accent][2],ListaColores[accent][3]);	
		SetSpriteSize(18,18);
		DrawnRectangle();
		SetSpriteSize(14,14);
		glColor4f(ListaColores[blanco][0],ListaColores[blanco][1],ListaColores[blanco][2],ListaColores[blanco][3]);	
		glEnable( GL_TEXTURE_2D );
	}
	else {
		glColor4f(ListaColores[grisUI][0],ListaColores[grisUI][1],ListaColores[grisUI][2],ListaColores[grisUI][3]);
	}
}

void CWhisk3D::UiMoveTo(GLshort x, GLshort y){
	glTranslatef( x, y, 0);
}

void CWhisk3D::SetUvBordes(GLshort origenX, GLshort origenY, GLshort ancho, GLshort alto, GLshort top, GLshort right, GLshort bottom, GLshort left){
	//Filas en X
	SpriteUV[0]  = SpriteUV[6]  = SpriteUV[16] = SpriteUV[24] = (GLbyte)(-128+origenX);
	SpriteUV[2]  = SpriteUV[4]  = SpriteUV[18] = SpriteUV[26] = (GLbyte)(-128+origenX+left);
	SpriteUV[8]  = SpriteUV[10] = SpriteUV[20] = SpriteUV[28] = (GLbyte)(-128+origenX+ancho-right);	
	SpriteUV[12] = SpriteUV[14] = SpriteUV[22] = SpriteUV[30] = (GLbyte)(-128+origenX+ancho);
	//fila en Y
	SpriteUV[1]  = SpriteUV[3]  = SpriteUV[9]  = SpriteUV[13] =(GLbyte)(-128+origenY);
	SpriteUV[7]  = SpriteUV[5]  = SpriteUV[11] = SpriteUV[15] =(GLbyte)(-128+origenY+top);
	SpriteUV[17] = SpriteUV[19] = SpriteUV[21] = SpriteUV[23] =(GLbyte)(-128+origenY+alto-bottom);
	SpriteUV[25] = SpriteUV[27] = SpriteUV[29] = SpriteUV[31] =(GLbyte)(-128+origenY+alto);
}

void CWhisk3D::DibujarRectanguloBordes(GLshort ancho, GLshort alto, GLshort top, GLshort right, GLshort bottom, GLshort left ){
	//cambia el tamaño
	//Posicion en X
	SpriteVertices[3]  = SpriteVertices[6]  = SpriteVertices[27] = SpriteVertices[39] = left+1;
	SpriteVertices[12] = SpriteVertices[15] = SpriteVertices[30] = SpriteVertices[42] = ancho-right+1;
	SpriteVertices[18] = SpriteVertices[21] = SpriteVertices[33] = SpriteVertices[45] = ancho+1;	
	//Posicion en Y
	SpriteVertices[10] = SpriteVertices[7]  = SpriteVertices[16] = SpriteVertices[22] = -(top+1);
	SpriteVertices[25] = SpriteVertices[28] = SpriteVertices[31] = SpriteVertices[34] = -(alto-bottom+1);
	SpriteVertices[37] = SpriteVertices[40] = SpriteVertices[43] = SpriteVertices[46] = -(alto+1);

	glDrawElements( GL_TRIANGLES, 18*3, GL_UNSIGNED_SHORT, SpriteFaces );
}

void CWhisk3D::SetMouse(){
	mouseVisible = !mouseVisible;
	mouseX = iScreenWidth/2;
	mouseY = (GLshort)iScreenHeightSplit;
    redibujar = true;
}

//invierte cualquier valor que se le manda, de verdadero a falso y viceversa
void CWhisk3D::ToggleValue(TBool& valueToUpdate){
    valueToUpdate = !valueToUpdate;
    redibujar = true;
}

// -------------------------------------------------------------------------------------------------------
// CWhisk3D::OnStartLoadingTextures()
// Called for a MTextureLoadingListener by the texture manager when texture loading operation starts
// -------------------------------------------------------------------------------------------------------

void CWhisk3D::OnStartLoadingTexturesL(){
    SetStateL( ELoadingTextures );
}

// ------------------------------------------------------------------------------------------------------------
// CWhisk3D::OnEndLoadingTextures()
// Called for a MTextureLoadingListener by the texture manager when texture loading operation is completed
// ------------------------------------------------------------------------------------------------------------
void CWhisk3D::OnEndLoadingTexturesL(){
	if ( GetState() == ELoadingTextures ){
		SetStateL( ERunning );
	}
}


void CWhisk3D::SeleccionarAnterior(){
	if (InteractionMode == ObjectMode){
		if (Objects.Count() < 1){return;}
		/*if (Objects[SelectActivo].seleccionado){
			Objects[SelectActivo].seleccionado = false;
			SelectCount--;
		}*/
		SelectActivo--;
		if (0 > SelectActivo){
			SelectActivo = Objects.Count()-1;
		}
		if (!Objects[SelectActivo].seleccionado){
			Objects[SelectActivo].seleccionado = true;
			SelectCount++;
		}
	}
	redibujar = true;
}

void CWhisk3D::SeleccionarProximo(){
	if (InteractionMode == ObjectMode){
		if (Objects.Count() < 1){return;}
		/*if (Objects[SelectActivo].seleccionado){
			Objects[SelectActivo].seleccionado = false;
			SelectCount--;
		}*/
		SelectActivo++;
		if (Objects.Count() <= SelectActivo){
			SelectActivo = 0;
		}
		if (!Objects[SelectActivo].seleccionado){
			Objects[SelectActivo].seleccionado = true;
			SelectCount++;
		}
	}
	redibujar = true;
}

void CWhisk3D::InvertirSeleccion(){
	if (InteractionMode == ObjectMode){
		for(int o=0; o < Objects.Count(); o++){
			Objects[o].seleccionado = !Objects[o].seleccionado;
		}
	}
	redibujar = true;
}

void CWhisk3D::SeleccionarTodo(){
	TBool TodoSeleccionado = true;
	if (InteractionMode == ObjectMode){
		for(int o=0; o < Objects.Count(); o++){
			if (!Objects[o].seleccionado){
				TodoSeleccionado = false;
			}	
		}
		//solo si esta todo seleccionado hace lo contrario
		if (TodoSeleccionado){
			SelectCount = 0;
		}
		else {
			SelectCount = Objects.Count();
		}
		for(int o=0; o < Objects.Count(); o++){
			Objects[o].seleccionado = !TodoSeleccionado;
		}
	}
	redibujar = true;
}

void CWhisk3D::SetTranslacionObjetos(TInt valor){
	for (int o = 0; o < estadoObjetos.Count(); o++) {
		switch (axisSelect) {
			case X:
				Objects[estadoObjetos[o].indice].posX += valor;
				break;
			case Y:
				Objects[estadoObjetos[o].indice].posY -= valor;
				break;
			case Z:
				Objects[estadoObjetos[o].indice].posZ -= valor;
				break;
		}
	}
}

// Convierte grados a radianes
GLfloat CWhisk3D::GradosARadianes(TInt grados) {
    return grados * (PI / 180.0);
}

TInt valorRotacion = 0;
void CWhisk3D::SetRotacion(TInt valor){
	for (int o = 0; o < estadoObjetos.Count(); o++) {
		switch (axisSelect) {
			case X:
				Objects[estadoObjetos[o].indice].rotX -= valor;
				break;
			case Y:
				Objects[estadoObjetos[o].indice].rotY -= valor;
				break;
			case Z:
				Objects[estadoObjetos[o].indice].rotZ -= valor;
				break;
		}
	}
}

void CWhisk3D::SetScale(TInt valor){
	valor = valor*1000;
	for (int o = 0; o < estadoObjetos.Count(); o++) {
		switch (axisSelect) {
			case X:
				Objects[estadoObjetos[o].indice].scaleX += valor;
				break;
			case Y:
				Objects[estadoObjetos[o].indice].scaleY += valor;
				break;
			case Z:
				Objects[estadoObjetos[o].indice].scaleZ += valor;
				break;
			case XYZ:
				Objects[estadoObjetos[o].indice].scaleX += valor;
				Objects[estadoObjetos[o].indice].scaleY += valor;
				Objects[estadoObjetos[o].indice].scaleZ += valor;
				break;
		}
	}	
	redibujar = true;
}

TInt ShiftCount = 0;
void CWhisk3D::InputUsuario(GLfixed aDeltaTimeSecs){
	if (iInputHandler->IsInputPressed( EEscape )){
		Cancelar();
		return;
	}
	//revisa las 4 direcciones
	for(TInt f=0; f < 4; f++){
		if (iInputHandler->IsInputPressed( flechasEstados[f].cual )){
			flechasEstados[f].activo = true;
			if ( flechasEstados[f].estado == TeclaSuelta || flechasEstados[f].estado == TeclaSoltada){
				flechasEstados[f].estado = TeclaPresionada;
			}
			else {
				flechasEstados[f].estado = TeclaMantenida;
			}
		}
		else if ( flechasEstados[f].estado == TeclaMantenida || flechasEstados[f].estado == TeclaPresionada){
			flechasEstados[f].activo = false;
			flechasEstados[f].estado = TeclaSoltada;
		}
		else  {
			flechasEstados[f].activo = false;
			flechasEstados[f].estado = TeclaSuelta;
		}
	}

	if ( iShiftPressed && estado == editNavegacion){
		ShiftCount++;
		if( flechasEstados[FlechaIzquierda].estado == TeclaPresionada ){
			SeleccionarAnterior();
			ShiftCount = 40;
		}		
		else if( flechasEstados[FlechaDerecha].estado == TeclaPresionada ){
			SeleccionarProximo();
			ShiftCount = 40;
		}
		else if( flechasEstados[FlechaArriba].estado == TeclaPresionada ){
			SeleccionarTodo();
			ShiftCount = 40;
		}
		else if( flechasEstados[FlechaAbajo].estado == TeclaPresionada ){
			SeleccionarTodo();
			ShiftCount = 40;
		}
		/*else if (iInputHandler->IsInputPressed( EVolumenUp ) ){
			CurrentFrame++;
			if (CurrentFrame > EndFrame){
				CurrentFrame = StartFrame;
			}
			if (!PlayAnimation){
				ReloadAnimation();
			}
	    	redibujar = true;
			ShiftCount = 40;
		}
		else if( iInputHandler->IsInputPressed( EVolumenDown ) ){
			CurrentFrame--;
			if (CurrentFrame < StartFrame){
				StartFrame = EndFrame;
			}
			if (!PlayAnimation){
				ReloadAnimation();
			}
	    	redibujar = true;
			ShiftCount = 40;
		}*/
		return;
	}
	if ( iShiftPressed && estado != editNavegacion){
		ShiftCount++;
		return;
	}
	else if (ShiftCount > 0){
		if (ShiftCount < 30){
			Tab();
		}
		ShiftCount = 0;
		return;
	}

	//revisa si se apreto una flecha y actualiza los valores dependiendo el estado de la aplicacion
	//ya sea animando, editando una malla 3d o en modo objeto
	if( flechasEstados[FlechaIzquierda].activo ){
		//mueve el mouse
		if (mouseVisible){
			mouseX--;
			if (mouseX < 0){mouseX = 0;};
		}

		//rotX += fixedMul( 0.1, aDeltaTimeSecs );
		if (estado == editNavegacion){ 
			if (navegacionMode == Orbit){
				if (ViewFromCameraActive && CameraToView){
					Object& obj = Objects[CameraActive];
					// Convertir el angulo de rotX a radianes
					GLfloat radRotX = obj.rotX * PI / 180.0;

					obj.posX+= 30 * cos(radRotX);
					obj.posY-= 30 * sin(radRotX);
				}
				else {
					if (ViewFromCameraActive){
						RestaurarViewport();
					}
					rotX-= 0.5;
				}
			}
			else if (navegacionMode == Fly){
				// Convertir el angulo de rotX a radianes
				GLfloat radRotX = rotX * PI / 180.0;

				// Calcular el vector de direccion hacia la izquierda (90 grados a la izquierda del angulo actual)
				GLfloat leftX = cos(radRotX);
				GLfloat leftY = sin(radRotX);

				// Mover hacia la izquierda
				PivotX += 30 * leftX;
				PivotY += 30 * leftY;
			}	
		}
		else if (estado == translacion){	
			SetTranslacionObjetos(30);		
		}
		else if (estado == rotacion){
			SetRotacion(1);
		}
		else if (estado == EditScale){
			SetScale(-1);
		}
		else if (estado == timelineMove){
			CurrentFrame--;
			if (CurrentFrame < StartFrame){
				StartFrame = EndFrame;
			}
			if (!PlayAnimation){
				ReloadAnimation();
			}
		}
	    redibujar = true;
	}
	if( iInputHandler->IsInputPressed( EJoystickRight ) ){
		//mueve el mouse
		if (mouseVisible){
			mouseX++;
			if (mouseX > iScreenWidth-11){mouseX = iScreenWidth-11;};
		}

		//rotX -= fixedMul( 1, aDeltaTimeSecs );
		if (estado == editNavegacion){				
			if (navegacionMode == Orbit){
				if (ViewFromCameraActive && CameraToView){
					Object& obj = Objects[CameraActive];
					// Convertir el angulo de rotX a radianes
					GLfloat radRotX = obj.rotX * PI / 180.0;

					obj.posX-= 30 * cos(radRotX);
					obj.posY+= 30 * sin(radRotX);
				}
				else {
					if (ViewFromCameraActive){
						RestaurarViewport();
					}
					rotX+= 0.5;	
				}		
			}
			else if (navegacionMode == Fly){
				// Convertir el angulo de rotX a radianes
				GLfloat radRotX = rotX * PI / 180.0;

				// Calcular el vector de direccion hacia la izquierda (90 grados a la izquierda del angulo actual)
				GLfloat leftX = cos(radRotX);
				GLfloat leftY = sin(radRotX);

				// Mover hacia la izquierda
				PivotX -= 30 * leftX;
				PivotY -= 30 * leftY;
			}
		}
		else if (estado == translacion){
			SetTranslacionObjetos(-30);		
		}
		else if (estado == rotacion){
			SetRotacion(-1);
		}
		else if (estado == EditScale){
			SetScale(1);	
		}
		else if (estado == timelineMove){
			CurrentFrame++;
			if (!PlayAnimation){
				ReloadAnimation();
			}
		}
	    redibujar = true;
	}
	if( iInputHandler->IsInputPressed( EJoystickUp ) ){
		//mueve el mouse
		if (mouseVisible){
			mouseY--;
			if (mouseY < 0){mouseY = 0;};
		}

		if (estado == editNavegacion){	
			if (navegacionMode == Orbit){
				if (ViewFromCameraActive && CameraToView){
					Object& obj = Objects[CameraActive];
					// Convertir el angulo de rotX a radianes
					GLfloat radRotX = obj.rotX * PI / 180.0;
					GLfloat radRotY = obj.rotY * PI / 180.0;
					//GLfloat radRotZ = obj.rotZ * PI / 180.0;

					obj.posX+= 30 * sin(radRotX);
					//obj.posY-= 30 * cos(radRotX);
					obj.posZ+= 30 * cos(radRotY);
				}
				else {
					if (ViewFromCameraActive){
						RestaurarViewport();
					}
					rotY-= 0.5;	
				}			
			}
			else if (navegacionMode == Fly){
				// Convertir el angulo de rotX a radianes
				GLfloat radRotX = rotX * PI / 180.0;

				PivotY+= 30 * cos(radRotX);
				PivotX-= 30 * sin(radRotX);
			}		
		}
		else if (estado == EditScale){
			SetScale(1);	
		}
		else if (estado == translacion){
			SetTranslacionObjetos(-30);
		}
	    redibujar = true;
	}
	if( iInputHandler->IsInputPressed( EJoystickDown ) ){
		//mueve el mouse
		if (mouseVisible){
			mouseY++;
			if (mouseY > iScreenHeight-17){mouseY = iScreenHeight-17;};
		}

		if (estado == editNavegacion){ 			
			if (navegacionMode == Orbit){
				if (ViewFromCameraActive && CameraToView){
					Object& obj = Objects[CameraActive];
					// Convertir el angulo de rotX a radianes
					GLfloat radRotX = obj.rotX * PI / 180.0;
					GLfloat radRotY = obj.rotY * PI / 180.0;
					//GLfloat radRotZ = obj.rotZ * PI / 180.0;

					obj.posX-= 30 * sin(radRotX);
					//obj.posY-= 30 * cos(radRotX);
					obj.posZ-= 30 * cos(radRotY);
				}
				else {
					if (ViewFromCameraActive){
						RestaurarViewport();
					}
					rotY+= 0.5;	
				}		
			}
			else if (navegacionMode == Fly){
				// Convertir el angulo de rotX a radianes
				GLfloat radRotX = rotX * PI / 180.0;

				PivotY-= 30 * cos(radRotX);
				PivotX+= 30 * sin(radRotX);
			}
		}
		else if (estado == EditScale){
			SetScale(-1);	
		}
		else if (estado == translacion){
			SetTranslacionObjetos(30);		
		}
	    redibujar = true;
	}
}

void CWhisk3D::SetRotacion(){
	//si no hay objetos
	if (Objects.Count() < 1){return;}
	else if (Objects[SelectActivo].seleccionado && estado == editNavegacion){
		guardarEstado();
		estado = rotacion;	
		valorRotacion = 0;
		if (axisSelect > 2){axisSelect = X;}
	}	
	else {
		axisSelect = Y;
	}
	if (estado == rotacion){
		SetRotacion(0);
	}
    redibujar = true;	
};

void CWhisk3D::SetEscala(){
	//XYZ tiene escala
	//si no hay objetos
	if (Objects.Count() < 1){return;}
	else if (Objects[SelectActivo].seleccionado && estado == editNavegacion){
		estado = EditScale;
		guardarEstado();
		axisSelect = XYZ;	
	}	
	else {
		axisSelect = Z;
	}
	if (estado == rotacion){
		SetRotacion(0);
	}
    redibujar = true;	
};

void CWhisk3D::ChangeEje(){
	if (Objects.Count() < 1){return;}
	if (estado != editNavegacion){
		axisSelect++;
		if (axisSelect > 2){
			axisSelect = 0;
		}
	}
	else {SetPosicion();}
	//si esta rotando. redibuja	
	if (estado == rotacion){
		SetRotacion(0);
	}
	redibujar = true;
}

void CWhisk3D::SetPosicion(){
	//si no hay objetos
	if (Objects.Count() < 1){return;}

	if (InteractionMode == ObjectMode && Objects[SelectActivo].seleccionado && estado == editNavegacion){
		guardarEstado();
		estado = translacion;
		if (axisSelect > 2){axisSelect = X;}
	}
	else {
		axisSelect = X;
	}
	if (estado == rotacion){
		SetRotacion(0);
	}
    redibujar = true;	
};

void CWhisk3D::EventKeyDown(TInt scan){
	switch(scan){               
		/*case(4): //ESC no anda...
			Cancelar(); */   
		case(68): //D
			if (iShiftPressed){
				ShiftCount = 40;
				DuplicatedObject();				
			}
			else if (iAltPressed){
				DuplicatedLinked();
			}
			break;
		case(69): //E
			//Extrude();
			break;
		case(73): //I
			if (iCtrlPressed){
				InvertirSeleccion();	
			}
			break;			
		case(83): //S
			SetEscala();
			break;
		case(88): //X
			if (estado != editNavegacion){
				SetEje(0);
			}
			else {
				Borrar();
			}
			break;
		case(77)://M
			VerOpciones();
			break;
	}
};

// Funcion para abrir el popup con las opciones
void CWhisk3D::VerOpciones(){
    // Declarar el indice seleccionado
    /*TInt iSelectedIndex = 0;

    // Crear un array de cadenas con las opciones
    CDesCArrayFlat* items = new (ELeave) CDesCArrayFlat(3);
    CleanupStack::PushL(items);

    // Agregar opciones al array
    items->AppendL(_L("Item 1"));
    items->AppendL(_L("Item 2"));
    items->AppendL(_L("Item 3"));

    // Crear el dialogo de lista y pasar el array de opciones
    CAknListQueryDialog* dlg = new (ELeave) CAknListQueryDialog(&iSelectedIndex);
    //dlg->PrepareLC(R_AVKON_DIALOG_EMPTY_MENUPANE);
    dlg->SetItemTextArray(items);
    dlg->SetOwnershipType(ELbmOwnsItemArray);

    // Mostrar el dialogo y capturar la opcion seleccionada
    if (dlg->RunLD())
    {
        // Aqui puedes manejar la opcion seleccionada en iSelectedIndex
    }

    CleanupStack::Pop(items); // Limpia el array de la pila*/
}

void CWhisk3D::SetEje(TInt eje){
	if (estado != editNavegacion){
		axisSelect = eje;
	}	
    redibujar = true;	
};

void CWhisk3D::Cancelar(){
	if (estado != editNavegacion){
		ReestablecerEstado();
	}
};

void CWhisk3D::Aceptar(){	
	//si no hay objetos
	if (Objects.Count() < 1){return;}

	if ( InteractionMode == ObjectMode ){
		if (estado != editNavegacion){
			estado = editNavegacion;
		}
	}
    redibujar = true;
};

void CWhisk3D::PressTab(){
	if (estado == editNavegacion){
		if (InteractionMode == ObjectMode){
			if (Objects.Count() < 1){return;}
			Object& obj = Objects[SelectActivo];
			if (obj.type != mesh || !obj.seleccionado){return;}
		}
	}
    redibujar = true;
}

void CWhisk3D::Tab(){
	if (estado != editNavegacion){ //} estado == translacion || estado == rotacion || estado == EditScale || estado == VertexMove){
		InsertarValor();
	}
	else if (InteractionMode == ObjectMode){
		changeSelect();
	}
	else if (InteractionMode == timelineMove){	
		HBufC* noteBuf = HBufC::NewLC(100);
		noteBuf->Des().Copy(_L("Set Current Frame"));
		CurrentFrame = DialogNumber(CurrentFrame, StartFrame-1, EndFrame+100, noteBuf);	
		CleanupStack::PopAndDestroy(noteBuf);
		Aceptar();
	};
};

void CWhisk3D::ReestablecerEstado(){
	if (InteractionMode == ObjectMode){
		for(int o=0; o < estadoObjetos.Count(); o++){
			SaveState& estadoObj = estadoObjetos[o];
			Object& obj = Objects[estadoObj.indice];
			obj.posX = estadoObj.posX;
			obj.posY = estadoObj.posY;
			obj.posZ = estadoObj.posZ;
			obj.rotX = estadoObj.rotX;
			obj.rotY = estadoObj.rotY;
			obj.rotZ = estadoObj.rotZ;
			obj.scaleX = estadoObj.scaleX;
			obj.scaleY = estadoObj.scaleY;
			obj.scaleZ = estadoObj.scaleZ;	
		}	
		estadoObjetos.Close();
	}
	estado = editNavegacion;
	redibujar = true;
};


void CWhisk3D::SetTransformPivotPoint(){
	if (Objects.Count() < 1){return;}	
	if (InteractionMode == ObjectMode){	
		TransformPivotPointFloat[0] = 0;
		TransformPivotPointFloat[1] = 0;
		TransformPivotPointFloat[2] = 0;
		for(TInt i=0; i < Objects.Count(); i++){	
			Object& obj = Objects[i];	
			if (obj.seleccionado){
				TransformPivotPointFloat[0] += obj.posX;
				TransformPivotPointFloat[1] += obj.posY;
				TransformPivotPointFloat[2] += obj.posZ;	
			};

			//esto va a dar errores si el padre y el hijo estan seleccionados
			/*TInt ParentID = obj.Parent;
			while (ParentID  > -1) {		
				Object& parentObj = Objects[ParentID];
				TransformPivotPointFloat[0] += parentObj.posX;
				TransformPivotPointFloat[1] += parentObj.posY;
				TransformPivotPointFloat[2] += parentObj.posZ;	
				ParentID = parentObj.Parent;		
			}*/
		}
		TransformPivotPointFloat[0] = TransformPivotPointFloat[0]/SelectCount;
		TransformPivotPointFloat[1] = TransformPivotPointFloat[1]/SelectCount;
		TransformPivotPointFloat[2] = TransformPivotPointFloat[2]/SelectCount;
	}
}

void CWhisk3D::guardarEstado(){
	estadoObjetos.Close();
	estadoObjetos.ReserveL(SelectCount);
	for(int o=0; o < Objects.Count(); o++){
		Object& obj = Objects[o];
		if (obj.seleccionado){
			SaveState NuevoEstado;
			NuevoEstado.indice = o;
			NuevoEstado.posX = obj.posX;
			NuevoEstado.posY = obj.posY;
			NuevoEstado.posZ = obj.posZ;
			NuevoEstado.rotX = obj.rotX;
			NuevoEstado.rotY = obj.rotY;
			NuevoEstado.rotZ = obj.rotZ;
			NuevoEstado.scaleX = obj.scaleX;
			NuevoEstado.scaleY = obj.scaleY;
			NuevoEstado.scaleZ = obj.scaleZ;
			estadoObjetos.Append(NuevoEstado);
		}
	}	
	SetTransformPivotPoint();
};

//cambie el shader
void CWhisk3D::SetShading(TInt valor){	
	for (int i = 0; i < Lights.Count(); i++) {
		//glDisable( Lights[i].lightId );			
		//glDisable( GL_LIGHT3 );			
	}
	switch (valor) {
		case Rendered:
			glDisable( GL_LIGHT0 );			
			for (int i = 0; i < Lights.Count(); i++) {
				//glEnable( Lights[i].lightId );		
				glEnable( GL_LIGHT3 );	
			}
    		glLightModelfv( GL_LIGHT_MODEL_AMBIENT, AmbientRender );
			break;
		case Solid:
			glEnable( GL_LIGHT0 );
    		glLightModelfv( GL_LIGHT_MODEL_AMBIENT, globalAmbient );
			break;
		case MaterialPreview:
			glEnable( GL_LIGHT0 );
    		glLightModelfv( GL_LIGHT_MODEL_AMBIENT, globalAmbient );
			break;
		default:
			glDisable( GL_LIGHT0 );
			break;
	};

	view = valor;
    redibujar = true;	
}

// ---------------------------------------------------------
// CWhisk3D::OnEnterState( TInt aState )
// Called by TFiniteStateMachine when the f.s.m enters a new state
// ---------------------------------------------------------

void CWhisk3D::OnEnterStateL( TInt /*aState*/ ){
	// Nothing to do here...
}

void CWhisk3D::InsertarValor(){
	dialogoSymbian = true;
	HBufC* buf = HBufC::NewLC( 100 );
	if (estado == rotacion ){
		if (axisSelect == X){	
			GLfloat valorActual = Objects[SelectActivo].rotX-estadoObjetos[estadoObjetos.Count()-1].rotX;		
			_LIT(KFormatString, "Rotarion: %d along global X");
			buf->Des().Format(KFormatString, (TInt)valorActual);
			TInt valor = DialogNumber((TInt)valorActual, -100000, 100000,buf);
			for(int o=0; o < estadoObjetos.Count(); o++){
				SaveState& estadoObj = estadoObjetos[o];
				Object& obj = Objects[estadoObj.indice];
				obj.rotX = estadoObj.rotX+valor;
			}
		}
		if (axisSelect == Y){	
			GLfloat valorActual = Objects[SelectActivo].rotY-estadoObjetos[estadoObjetos.Count()-1].rotY;		
			_LIT(KFormatString, "Rotarion: %d along global Y");
			buf->Des().Format(KFormatString, (TInt)valorActual);
			TInt valor = DialogNumber((TInt)valorActual, -100000, 100000,buf);
			for(int o=0; o < estadoObjetos.Count(); o++){
				SaveState& estadoObj = estadoObjetos[o];
				Object& obj = Objects[estadoObj.indice];
				obj.rotY = estadoObj.rotY+valor;
			}
		}
		if (axisSelect == Z){	
			GLfloat valorActual = Objects[SelectActivo].rotZ-estadoObjetos[estadoObjetos.Count()-1].rotZ;		
			_LIT(KFormatString, "Rotarion: %d along global Z");
			buf->Des().Format(KFormatString, (TInt)valorActual);
			TInt valor = DialogNumber((TInt)valorActual, -100000, 100000,buf);
			for(int o=0; o < estadoObjetos.Count(); o++){
				SaveState& estadoObj = estadoObjetos[o];
				Object& obj = Objects[estadoObj.indice];
				obj.rotZ = estadoObj.rotZ+valor;
			}
		}
	}
	else if (estado == translacion ){
		if (axisSelect == X){
			buf->Des().Copy(_L("Mover en X"));
			TInt valorX = DialogNumber((TInt)(Objects[SelectActivo].posX-estadoObjetos[estadoObjetos.Count()-1].posX), -100000, 100000,buf);	
			for(int o=0; o < estadoObjetos.Count(); o++){
				SaveState& estadoObj = estadoObjetos[o];
				Object& obj = Objects[estadoObj.indice];
				obj.posX = estadoObj.posX+valorX;
			}
		}
		else if (axisSelect == Y){
			buf->Des().Copy(_L("Mover en Y"));
			TInt valorY = DialogNumber((TInt)(Objects[SelectActivo].posY-estadoObjetos[estadoObjetos.Count()-1].posY), -100000, 100000,buf);	
			for(int o=0; o < estadoObjetos.Count(); o++){
				SaveState& estadoObj = estadoObjetos[o];
				Object& obj = Objects[estadoObj.indice];
				obj.posY = estadoObj.posY+valorY;
			}	
		}
		else if (axisSelect == Z){
			buf->Des().Copy(_L("Mover en Z"));
			TInt valorZ = DialogNumber((TInt)(Objects[SelectActivo].posZ-estadoObjetos[estadoObjetos.Count()-1].posZ), -100000, 100000,buf);
			for(int o=0; o < estadoObjetos.Count(); o++){
				SaveState& estadoObj = estadoObjetos[o];
				Object& obj = Objects[estadoObj.indice];
				obj.posZ = estadoObj.posZ+valorZ;
			}		
		}
	}
	CleanupStack::PopAndDestroy(buf);
	dialogoSymbian = false;
	redibujar = true;	
}

void CWhisk3D::TecladoNumerico(TInt numero){
	if (InteractionMode == ObjectMode){
		switch (numero) {
			case 1:
				SetPosicion();
				break;
			case 2:
				SetRotacion();
				break;
			case 3:
				SetEscala();
				break;
			case 4:
				SetShading(0);	
				break;
			case 5:
				SetShading(1);	
				break;
			case 6:
				SetShading(2);	
				break;
			case 0:
				EnfocarObject();
			default:
				break;
		}	
	}
};


void CWhisk3D::SetParent(){
	if (Objects.Count() < 1){return;}
	
	TInt ParentID = 1;
	HBufC* noteBuf = HBufC::NewLC(100);
	_LIT(KFormatString, "Set Parent 1 to %d");
	noteBuf->Des().Format(KFormatString, Objects.Count());
	ParentID = DialogNumber(1, 1, Objects.Count(), noteBuf);	
	ParentID --;

	//si se emparento a si mismo. o hay un loopParent falla	
	TBool error = false;
	TInt ParentIDtest = ParentID;
	while (ParentIDtest > -1) {
		//_LIT(KFormatString2, "activo: %d\nParentID: %d");
		//noteBuf->Des().Format(KFormatString2, SelectActivo, ParentIDtest);
		//CDialogs::Alert(noteBuf);
		if (ParentIDtest == SelectActivo){
			ParentIDtest = -1;
			error = true;
		}
		else {
			ParentIDtest = Objects[ParentIDtest].Parent;
		}
	}

	if (error){
		noteBuf->Des().Copy(_L("loop in parent"));	
		MensajeError(noteBuf);
		CleanupStack::PopAndDestroy(noteBuf);
		return;
	};

	//si esta emparentado a algo. primero quita el parent
	if (Objects[SelectActivo].Parent > -1){
		ClearParent();
	}
	Objects[SelectActivo].Parent = ParentID;
	
	Children NewChildren;
	NewChildren.Id = SelectActivo;
	NewChildren.OriginalScaleX = Objects[SelectActivo].scaleX;
	NewChildren.OriginalScaleY = Objects[SelectActivo].scaleY; 
	NewChildren.OriginalScaleZ = Objects[SelectActivo].scaleZ;		
	Objects[ParentID].Childrens.Append(NewChildren);

	ParentIDtest = ParentID;
	while (ParentIDtest > -1) {
		Objects[SelectActivo].posX -= Objects[ParentIDtest].posX;
		Objects[SelectActivo].posY -= Objects[ParentIDtest].posY;
		Objects[SelectActivo].posZ -= Objects[ParentIDtest].posZ;

		Objects[SelectActivo].rotX -= Objects[ParentIDtest].rotX;
		Objects[SelectActivo].rotY -= Objects[ParentIDtest].rotY;
		Objects[SelectActivo].rotZ -= Objects[ParentIDtest].rotZ;
		ParentIDtest = Objects[ParentIDtest].Parent;
	}

	//si esta en la coleccion. lo borra
	for(int c=0; c < Collection.Count(); c++){
		if (Collection[c] == SelectActivo){
			Collection.Remove(c);
			break;			
		}
	}
	CleanupStack::PopAndDestroy(noteBuf);
	redibujar = true;
}

void CWhisk3D::ClearParent(){
	if (Objects.Count() < 1){return;}
	for(int c=0; c < Collection.Count(); c++){
		if (Collection[c] == SelectActivo){
			Collection.Remove(c);	
			break;
		}
	}
	Collection.Append(SelectActivo);

	//lo borra si quedo emparentado en algun objeto
	for(int o=0; o < Objects.Count(); o++){
		TBool salirBucle = false;
		for(int c=0; c < Objects[o].Childrens.Count(); c++){
			if (Objects[o].Childrens[c].Id == SelectActivo){
				Objects[o].Childrens.Remove(c);	
				salirBucle = true;
				break;
			}
		}
		if (salirBucle){break;}
	}
	redibujar = true;
};

void CWhisk3D::Borrar(){
	if (estado != editNavegacion ){
		Cancelar();
	}
	else if (InteractionMode == ObjectMode){
		if (Objects.Count() < 1){return;}

		//si no hay nada seleccionado. no borra
		TBool algoSeleccionado = false;
		for (TInt o = Objects.Count() - 1; o >= 0; o--) {
			if (Objects[o].seleccionado){
				algoSeleccionado = true;
				break;	
			}		
		}
		if (!algoSeleccionado){return;}
		//pregunta de confirmacion
		HBufC* noteBuf = HBufC::NewLC(100);
		_LIT(KStaticErrorMessage, "Delete?");
		noteBuf->Des().Format(KStaticErrorMessage);
		if (!CDialogs::Alert(noteBuf)){
			CleanupStack::PopAndDestroy(noteBuf);	
			return;
		}
		CleanupStack::PopAndDestroy(noteBuf);	
		Cancelar();

		//libera la memoria de los punteros primero	
		// Obtener el objeto seleccionado			
		for (TInt o = Objects.Count() - 1; o >= 0; o--) {
			if (Objects[o].seleccionado){
				BorrarObjeto(o);
			}			
		}
	}
    redibujar = true;	
}

void CWhisk3D::BorrarMesh(TInt indice){
	TInt links = 0;
	
	for(int o=0; o < Objects.Count(); o++){
		if (Objects[o].type == mesh && Objects[o].Id == indice){links++;};				
	}

	if (links < 2){	
		for(int o=0; o < Objects.Count(); o++){
			if (Objects[o].type == mesh && Objects[o].Id > indice){
				Objects[o].Id--;
			};				
		}
		Mesh& pMesh = Meshes[indice];
		//primero se borran los objetos a los que apunta el mesh
		delete[] pMesh.vertex;
		delete[] pMesh.vertexColor;
		delete[] pMesh.normals;
		delete[] pMesh.uv;
		for(TInt i=0; i < pMesh.vertexGroups.Count(); i++){
			pMesh.vertexGroups[i].indices.Close();
		}
		pMesh.vertexGroups.Close();

		delete[] pMesh.faces;
		pMesh.materialsGroup.Close();

		//ahora se borra el mesh
		Meshes.Remove(indice);
	}
}

void CWhisk3D::BorrarAnimaciones(TInt indice){
	for(TInt a = 0; a < AnimationObjects.Count(); a++) {
		if (AnimationObjects[a].Id == indice) {	
			for(TInt p = 0; p < AnimationObjects[a].Propertys.Count(); p++) {
				AnimationObjects[a].Propertys[p].keyframes.Close();
			}				
			AnimationObjects[a].Propertys.Close();
			AnimationObjects.Remove(a);
		}
		// Hace falta cambiar los indices
		else if (AnimationObjects[a].Id > indice) {
			AnimationObjects[a].Id--;
		}			
	}
}

void CWhisk3D::BorrarObjeto(TInt indice){
	Object& obj = Objects[indice];
	// Liberar memoria de los punteros del objeto seleccionado
	if (obj.type == mesh){
		BorrarMesh(obj.Id);
	}

	//si existe animaciones para ese objeto. las borra		
	BorrarAnimaciones(indice);

	// Borrar de la coleccion
	for (int c = Collection.Count() - 1; c >= 0; c--) {
		if (Collection[c] == indice) {
			Collection.Remove(c);
		}
		// Hace falta cambiar los indices
		else if (Collection[c] > indice) {
			Collection[c]--;
		}
	}

	//si es la camara activa. borra el indice
	if (CameraActive == indice){
		CameraActive = -1;	
		ViewFromCameraActive = false;	
	}
	//si era mas grande. resta uno para que el indice apunte a la camara correcta
	else if (CameraActive > indice){
		CameraActive--;
	}

	Objects.Remove(indice);
	SelectCount--;
	SelectActivo = 0;
	/*if (Objects.Count() > 0){
		SelectCount = 1;
		SelectActivo = Objects.Count()-1;
		Objects[SelectActivo].seleccionado = true;
	}
	else {
		SelectCount = 0;
		SelectActivo = 0;
	}*/
	
	// Actualizar indices en los objetos
	for (TInt o = 0; o < Objects.Count(); o++) {
		for (TInt c = Objects[o].Childrens.Count() - 1; c >= 0; c--) {
			if (Objects[o].Childrens[c].Id == indice) {
				Objects[o].Childrens.Remove(c);
			} 
			else if (Objects[o].Childrens[c].Id > indice) {
				Objects[o].Childrens[c].Id--;
			}
		}
		//borra y actualiza los padres
		if (Objects[o].Parent == indice){				
			Objects[o].Parent = -1;
			Collection.Append(o);
		} 
		else if (Objects[o].Parent > indice) {
			Objects[o].Parent--;
		}
	}
}

void CWhisk3D::CursorToSelect(){		
	SetTransformPivotPoint();
	if (InteractionMode == ObjectMode){
		Cursor3DposX = TransformPivotPointFloat[0];
		Cursor3DposY = TransformPivotPointFloat[1];
		Cursor3DposZ = TransformPivotPointFloat[2];
	}
	redibujar = true;
}

void CWhisk3D::SelectToCursor(){
	if (Objects.Count() < 1){return;}	
	if (InteractionMode == ObjectMode){	
		for(TInt i=0; i < Objects.Count(); i++){	
			Object& obj = Objects[i];	
			if (obj.seleccionado){
				obj.posX = Cursor3DposX;
				obj.posY = Cursor3DposY;
				obj.posZ = Cursor3DposZ;
			};
		}
	}
	redibujar = true;
}

void CWhisk3D::CursorToWorldOrigin(){
	Cursor3DposX = 0;
	Cursor3DposZ = 0;
	Cursor3DposY = 0;
	redibujar = true;
}

void CWhisk3D::AddObject( TInt tipo ){
	//Cancelar();
	Object obj;
	obj.type = tipo;
	obj.visible = true;
	obj.seleccionado = false;
	obj.posX = Cursor3DposX;
	obj.posY = Cursor3DposY;
	obj.posZ = Cursor3DposZ;
	obj.rotX = obj.rotY = obj.rotZ = 0;
	obj.scaleX = obj.scaleY = obj.scaleZ = 45000;
	obj.Parent = -1;	
	obj.Id = -0;
	Objects.Append(obj);
	Collection.Append(Objects.Count()-1);
	SelectActivo = Objects.Count()-1;
	if (tipo == light){
		Light tempLight;
		tempLight.type = pointLight;
		tempLight.lightId = nextLightId;
		tempLight.color[0] = 1.0;
		tempLight.color[1] = 1.0;
		tempLight.color[2] = 1.0;
		tempLight.color[3] = 1.0;
		//tempLight.color  = { MATERIALCOLOR(1.0, 1.0, 1.0, 1.0) };

		glEnable( nextLightId );		
		GLfloat lightDiffuseSpot[4]   = { LIGHTCOLOR(1.0, 1.0, 1.0, 1.0) };
		GLfloat lightSpecularSpot[4]  = { LIGHTCOLOR(1.0, 1.0, 1.0, 1.0) };
		glLightfv(  nextLightId, GL_DIFFUSE,  lightDiffuseSpot  );
		glLightfv(  nextLightId, GL_AMBIENT,  lightAmbient  );
		glLightfv(  nextLightId, GL_SPECULAR, lightSpecularSpot );
		//glLightfv(  nextLightId, GL_POSITION, lightPositionSpot );
		glLightfv(  nextLightId, GL_POSITION, positionPuntualLight );
		

		glLightf(   nextLightId, GL_CONSTANT_ATTENUATION,  1.5  );
		glLightf(   nextLightId, GL_LINEAR_ATTENUATION,    0.5  );
		glLightf(   nextLightId, GL_QUADRATIC_ATTENUATION, 0.5  );

		/*glLightf(   nextLightId, GL_SPOT_CUTOFF,   170.0                );
		glLightf(   nextLightId, GL_SPOT_EXPONENT,  20.0                );
		glLightfv(  nextLightId, GL_SPOT_DIRECTION, lightDirectionSpot );*/
		nextLightId++;

		Lights.Append(tempLight);
		obj.Id = Lights.Count()-1;
	}
	//tipo camara
	else if (tipo == camera){
		if (CameraActive < 0){
			CameraActive = SelectActivo;		
		}		
	}
	DeseleccionarTodo();
	Objects[SelectActivo].seleccionado = true;
	SelectCount = 1;
    redibujar = true;
}


void CWhisk3D::DeseleccionarTodo(){
	if (InteractionMode == ObjectMode){
		for(int o=0; o < Objects.Count(); o++){
			Objects[o].seleccionado = false;				
		}
		SelectCount = 0;
	}
}

void CWhisk3D::AddMesh( int modelo ){
	Cancelar();
	DeseleccionarTodo();
	Collection.Append(Objects.Count());
	SelectActivo = Objects.Count();
	SelectCount = 1;
	
	Object TempObj;	
	Objects.Append(TempObj);
	Object& obj = Objects[Objects.Count()-1];
	Objects[SelectActivo].seleccionado = true;
	
	obj.Id = Meshes.Count();
	obj.type = mesh;
	obj.visible = true;
	obj.posX = Cursor3DposX;
	obj.posY = Cursor3DposY;
	obj.posZ = Cursor3DposZ;
	obj.rotX = obj.rotY = obj.rotZ = 0;
	obj.scaleX = obj.scaleY = obj.scaleZ = 45000;
	obj.Parent = -1;
	
	Mesh tempMesh;
	Meshes.Append(tempMesh);
	Mesh& pMesh = Meshes[obj.Id];	
	
	MaterialGroup tempFaceGroup;
	tempFaceGroup.startDrawn = 0;
	tempFaceGroup.material = 0;
	
	VertexGroup TempVertexGroups;	
	
	if (modelo == plane){ pMesh.vertexSize = 4;
		pMesh.vertex = new GLshort[pMesh.vertexSize*3];
		pMesh.vertexColor = new GLubyte[pMesh.vertexSize*4];
		pMesh.normals = new GLbyte[pMesh.vertexSize*3];
		pMesh.uv = new GLfloat[pMesh.vertexSize*2];
		HBufC* noteBuf = HBufC::NewLC(100);
		noteBuf->Des().Copy(_L("Add Plane Size"));
		TInt InputSize = DialogNumber(2, 0, 20, noteBuf);		
		CleanupStack::PopAndDestroy(noteBuf);
		InputSize = InputSize*1000;	

		for (int i = 0; i < pMesh.vertexSize*3; i++) {
			pMesh.vertex[i] = PlaneVertices[i]*InputSize;
		}
		for (int i = 0; i < pMesh.vertexSize; i++) {
			pMesh.normals[i*3] = 0;
			pMesh.normals[i*3+1] = 127;
			pMesh.normals[i*3+2] = 0;
		}
		for (int i = 0; i < pMesh.vertexSize*4; i++) {
			pMesh.vertexColor[i] = 255;
		}
		for (int i = 0; i < pMesh.vertexSize*2; i++) {
			pMesh.uv[i] = (GLfloat)PlaneUV[i];
		}

		//pMesh.facesCount = tempFaceGroup.count = 2;
		pMesh.facesSize = tempFaceGroup.indicesDrawnCount = 6;

		pMesh.faces = new GLushort[tempFaceGroup.indicesDrawnCount];
		for (int i = 0; i < tempFaceGroup.indicesDrawnCount; i++) {
			pMesh.faces[i] = PlaneTriangles[i];
		}
	}	
	else if (modelo == circle){ 
		HBufC* noteBuf = HBufC::NewLC(100);
		noteBuf->Des().Copy(_L("Add Circle vertices"));
		TInt originalVertexSize  = DialogNumber(8, 3, 512, noteBuf);	
		pMesh.vertexSize = originalVertexSize + 1;  // Agregar un vértice adicional
		noteBuf->Des().Copy(_L("Add Circle radius cm"));
		TInt Radius = DialogNumber(100, 1, 10000, noteBuf);	
		CleanupStack::PopAndDestroy(noteBuf);
		Radius = Radius*20;	

		pMesh.vertex = new GLshort[pMesh.vertexSize*3];
		pMesh.vertexColor = new GLubyte[pMesh.vertexSize*4];
		pMesh.normals = new GLbyte[pMesh.vertexSize*3];
		pMesh.uv = new GLfloat[pMesh.vertexSize*2];

		for (int i = 0; i < pMesh.vertexSize*3; i++) {
			pMesh.vertex[i] = Radius;
		}

		for (int i = 0; i < pMesh.vertexSize; i++) {
			GLfloat angle = 2.0f * M_PI * (i % originalVertexSize) / originalVertexSize;  // angulo para los vértices

			pMesh.vertex[i*3] = -Radius * cos(angle);
			pMesh.vertex[i*3+1] = 0;
			pMesh.vertex[i*3+2] = -Radius * sin(angle);
			pMesh.normals[i*3] = 0;
			pMesh.normals[i*3+1] = 127;
			pMesh.normals[i*3+2] = 0;
			pMesh.uv[i * 2] = static_cast<GLfloat>(cos(angle) * 127);
			pMesh.uv[i * 2 + 1] = static_cast<GLfloat>(sin(angle) * 127);
		}

		for (int i = 0; i < pMesh.vertexSize*4; i++) {
			pMesh.vertexColor[i] = 255;
		}

		pMesh.facesSize = tempFaceGroup.indicesDrawnCount = (pMesh.vertexSize - 2) * 3;

		pMesh.faces = new GLushort[tempFaceGroup.indicesDrawnCount];
		for (TInt i = 0; i < originalVertexSize - 1; i++) {
			pMesh.faces[i * 3] = 0;          // Primer vértice
			pMesh.faces[i * 3 + 1] = i + 2;  // Segundo vértice
			pMesh.faces[i * 3 + 2] = i + 1;  // Tercer vértice
		}
	}
	else if (modelo == cubo){ 
    	pMesh.vertexSize = 24;
		pMesh.vertex = new GLshort[pMesh.vertexSize*3];
		pMesh.vertexColor = new GLubyte[pMesh.vertexSize*4];
		pMesh.normals = new GLbyte[pMesh.vertexSize*3];
		pMesh.uv = new GLfloat[pMesh.vertexSize*2];

		for (int i = 0; i < pMesh.vertexSize*3; i++) {
			pMesh.vertex[i] = CuboVertices[i];
			pMesh.normals[i] = CuboNormals[i];
		}
		for (int i = 0; i < pMesh.vertexSize*4; i++) {
			pMesh.vertexColor[i] = 255;
		}
		for (int i = 0; i < pMesh.vertexSize*2; i++) {
			//pMesh.uv[i] = (GLfloat)((CuboUV[i]+128)/255)*1280;
			pMesh.uv[i] = (GLfloat)CuboUV[i];
		}

		pMesh.facesSize = tempFaceGroup.indicesDrawnCount = 36;

		pMesh.faces = new GLushort[tempFaceGroup.indicesDrawnCount];
		for (int i = 0; i < tempFaceGroup.indicesDrawnCount; i++) {
			pMesh.faces[i] = CuboTriangles[i];
		}
	}	

	//creamos el objeto y le asignamos la mesh	
	Meshes[obj.Id].materialsGroup.Append(tempFaceGroup);
    redibujar = true;
}

void CWhisk3D::ActivarTextura(){
	//si no hay objetos
	if (Objects.Count() < 1){return;}	
	Object& obj = Objects[SelectActivo];
	//si no es un mesh
	if (obj.type != mesh){return;}	
	Mesh& pMesh = Meshes[obj.Id];

	Cancelar();
	TInt MaterialID = 1;
	HBufC* noteBuf = HBufC::NewLC(100);
	if (pMesh.materialsGroup.Count() > 1){
		_LIT(KFormatString, "Material 1 to %d");
		noteBuf->Des().Format(KFormatString, pMesh.materialsGroup.Count());
		MaterialID = DialogNumber(1, 1, pMesh.materialsGroup.Count(), noteBuf);		
	}
	Material& mat = Materials[pMesh.materialsGroup[MaterialID-1].material];

	Cancelar();
	//activa o desactiva las texturas
	noteBuf->Des().Copy(_L("Activar Textura?"));
	if (CDialogs::Alert(noteBuf)){	
		mat.textura = true;
		
	}
	else {
		mat.textura = false;	
	}
	CleanupStack::PopAndDestroy(noteBuf);	
    redibujar = true;
}

void CWhisk3D::SetTransparencia(){
	//si no hay objetos
	if (Objects.Count() < 1){return;}	
	Object& obj = Objects[SelectActivo];
	//si no es un mesh
	if (obj.type != mesh){return;}	
	Mesh& pMesh = Meshes[obj.Id];

	Cancelar();
	TInt MaterialID = 1;
	HBufC* noteBuf = HBufC::NewLC(100);
	if (pMesh.materialsGroup.Count() > 1){
		_LIT(KFormatString, "Material 1 to %d");
		noteBuf->Des().Format(KFormatString, pMesh.materialsGroup.Count());
		MaterialID = DialogNumber(1, 1, pMesh.materialsGroup.Count(), noteBuf);		
	}

	Material& mat = Materials[pMesh.materialsGroup[MaterialID-1].material];

	Cancelar();
	//activa o desactiva las Transparencias
	noteBuf->Des().Copy(_L("Activar Transparencia?"));
	if (CDialogs::Alert(noteBuf)){	
		mat.transparent = true;
	}
	else {
		mat.transparent = false;	
	}
	CleanupStack::PopAndDestroy(noteBuf);	
    redibujar = true;
}

void CWhisk3D::SetTextureRepeat(){
	//si no hay objetos
	if (Objects.Count() < 1){return;}	
	Object& obj = Objects[SelectActivo];
	//si no es un mesh
	if (obj.type != mesh){return;}	
	Mesh& pMesh = Meshes[obj.Id];

	Cancelar();
	TInt MaterialID = 1;
	HBufC* noteBuf = HBufC::NewLC(100);
	if (pMesh.materialsGroup.Count() > 1){
		_LIT(KFormatString, "Material 1 to %d");
		noteBuf->Des().Format(KFormatString, pMesh.materialsGroup.Count());
		MaterialID = DialogNumber(1, 1, pMesh.materialsGroup.Count(), noteBuf);		
	}

	Material& mat = Materials[pMesh.materialsGroup[MaterialID-1].material];

	//activa o desactiva las Transparencias
	noteBuf->Des().Copy(_L("Activar Repeticion de Textura?"));
	if (CDialogs::Alert(noteBuf)){	
		mat.repeat = true;
	}
	else {
		mat.repeat = false;	
	}
	CleanupStack::PopAndDestroy(noteBuf);	
    redibujar = true;
}

void CWhisk3D::SetCulling(){
	//si no hay objetos
	if (Objects.Count() < 1){return;}	
	Object& obj = Objects[SelectActivo];
	//si no es un mesh
	if (obj.type != mesh){return;}	
	Mesh& pMesh = Meshes[obj.Id];

	Cancelar();
	TInt MaterialID = 1;
	HBufC* noteBuf = HBufC::NewLC(100);
	if (pMesh.materialsGroup.Count() > 1){
		_LIT(KFormatString, "Material 1 to %d");
		noteBuf->Des().Format(KFormatString, pMesh.materialsGroup.Count());
		MaterialID = DialogNumber(1, 1, pMesh.materialsGroup.Count(), noteBuf);		
	}
	Material& mat = Materials[pMesh.materialsGroup[MaterialID-1].material];

	//activa o desactiva culling de caras
	noteBuf->Des().Copy(_L("Activar Culling?"));
	if (CDialogs::Alert(noteBuf)){	
		mat.culling = true;
	}
	else {
		mat.culling = false;	
	}
	CleanupStack::PopAndDestroy(noteBuf);	
    redibujar = true;
}

void CWhisk3D::SetLighting(){
	//si no hay objetos
	if (Objects.Count() < 1){return;}	
	Object& obj = Objects[SelectActivo];
	//si no es un mesh
	if (obj.type != mesh){return;}	
	Mesh& pMesh = Meshes[obj.Id];

	TInt MaterialID = 1;
	HBufC* noteBuf = HBufC::NewLC(100);
	if (pMesh.materialsGroup.Count() > 1){
		_LIT(KFormatString, "Material 1 to %d");
		noteBuf->Des().Format(KFormatString, pMesh.materialsGroup.Count());
		MaterialID = DialogNumber(1, 1, pMesh.materialsGroup.Count(), noteBuf);	
	}
	Material& mat = Materials[pMesh.materialsGroup[MaterialID-1].material];

	Cancelar();
	//activa o desactiva las Transparencias
	noteBuf->Des().Copy(_L("Activar Iluminacion?"));
	if (CDialogs::Alert(noteBuf)){	
		mat.lighting = true;
	}
	else {
		mat.lighting = false;	
	}
	CleanupStack::PopAndDestroy(noteBuf);	
    redibujar = true;
}

void CWhisk3D::SetVertexColor(){
	//si no hay objetos
	if (Objects.Count() < 1){return;}	
	Object& obj = Objects[SelectActivo];
	//si no es un mesh
	if (obj.type != mesh){return;}	
	Mesh& pMesh = Meshes[obj.Id];

	TInt MaterialID = 1;
	HBufC* noteBuf = HBufC::NewLC(100);
	if (pMesh.materialsGroup.Count() > 1){
		_LIT(KFormatString, "Material 1 to %d");
		noteBuf->Des().Format(KFormatString, pMesh.materialsGroup.Count());
		MaterialID = DialogNumber(1, 1, pMesh.materialsGroup.Count(), noteBuf);	
	}
	Material& mat = Materials[pMesh.materialsGroup[MaterialID-1].material];

	Cancelar();
	//activa o desactiva las Transparencias
	noteBuf->Des().Copy(_L("Activar Vertex Color?"));
	if (CDialogs::Alert(noteBuf)){	
		mat.vertexColor = true;
	}
	else {
		mat.vertexColor = false;	
	}
	CleanupStack::PopAndDestroy(noteBuf);	
    redibujar = true;
}

void CWhisk3D::SetInterpolation(){
	//si no hay objetos
	if (Objects.Count() < 1){return;}	
	Object& obj = Objects[SelectActivo];
	//si no es un mesh
	if (obj.type != mesh){return;}	
	Mesh& pMesh = Meshes[obj.Id];

	TInt MaterialID = 1;
	HBufC* noteBuf = HBufC::NewLC(100);
	if (pMesh.materialsGroup.Count() > 1){
		_LIT(KFormatString, "Material 1 to %d");
		noteBuf->Des().Format(KFormatString, pMesh.materialsGroup.Count());
		MaterialID = DialogNumber(1, 1, pMesh.materialsGroup.Count(), noteBuf);
	}
	Material& mat = Materials[pMesh.materialsGroup[MaterialID-1].material];

	Cancelar();
	//activa o desactiva las Transparencias
	noteBuf->Des().Copy(_L("Interpolacion Lineal?"));
	if (CDialogs::Alert(noteBuf)){	
		mat.interpolacion = lineal;
	}
	else {
		mat.interpolacion = closest;	
	}
	CleanupStack::PopAndDestroy(noteBuf);	
    redibujar = true;
}

void CWhisk3D::SetTexture(){
	//si solo estan las texturas de blender
	if (Textures.Count() < NumTexturasWhisk3D+1){		
		HBufC* noteBuf = HBufC::NewLC(100);	
		noteBuf->Des().Copy(_L("No hay texturas cargadas"));	
		MensajeError(noteBuf);
		CleanupStack::PopAndDestroy(noteBuf);
		return;
	}	
	//si no hay objetos
	if (Objects.Count() < 1){return;}	
	Object& obj = Objects[SelectActivo];
	//si no es un mesh
	if (obj.type != mesh){return;}	
	Mesh& pMesh = Meshes[obj.Id];

	Cancelar();
	TInt MaterialID = 1;
	HBufC* noteBuf = HBufC::NewLC(100);
	if (pMesh.materialsGroup.Count() > 1){
		_LIT(KFormatString, "Material 1 to %d");
		noteBuf->Des().Format(KFormatString, pMesh.materialsGroup.Count());
		MaterialID = DialogNumber(1, 1, pMesh.materialsGroup.Count(), noteBuf);		
	}

	Material& mat = Materials[pMesh.materialsGroup[MaterialID-1].material];

	_LIT(KFormatString2, "Select Texture 1 to %d");
	noteBuf->Des().Format(KFormatString2, Textures.Count()-NumTexturasWhisk3D);
	TInt textureID = DialogNumber(1, 1, Textures.Count()-NumTexturasWhisk3D, noteBuf);
	CleanupStack::PopAndDestroy(noteBuf);
	
	mat.textura = true;
	mat.textureID = textureID+NumTexturasWhisk3D;
	//mat.textureID = Textures[textureID-5].iID;
    redibujar = true;
}

void CWhisk3D::NewMaterial(TBool reemplazar){
	/*HBufC* inicialBuf = HBufC::NewLC(100);
	_LIT(Kinicial, "Material.%03d");
	inicialBuf->Des().Format(Kinicial, Materials.Count()+1);
	
	HBufC* tituloBuf = HBufC::NewLC(100);
	_LIT(Ktitulo, "Ingrese el nombre del Material");
	tituloBuf->Des().Copy(Ktitulo);
	TPtr nombre = DialogText(inicialBuf, tituloBuf);
	
	HBufC* noteBuf = HBufC::NewLC(100);//textoingresado.AllocLC();
	noteBuf->Des().Copy(textoingresado);
	CDialogs::Alert(noteBuf);
	CleanupStack::PopAndDestroy(noteBuf);	
	CleanupStack::PopAndDestroy(tituloBuf);	
	CleanupStack::PopAndDestroy(inicialBuf);	*/
	
	Material mat;	
	mat.specular[0] = mat.specular[1] = mat.specular[2] = mat.specular[3] = 0.3;
	mat.diffuse[0] = mat.diffuse[1] = mat.diffuse[2] = mat.diffuse[3] = 1.0;
	mat.emission[0] = mat.emission[1] = mat.emission[2] = mat.emission[3] = 0.0;
	mat.textura = false;
	mat.vertexColor = false;
	mat.repeat = true;
	mat.lighting = true;
	mat.culling = true;
	mat.transparent = false;
	mat.interpolacion = lineal;
	mat.textureID = 0;
	if (Materials.Count() < 1){
		mat.name = HBufC::NewL(15);
		_LIT(KMatNameDefault, "DefaultMaterial");
		mat.name->Des().Format(KMatNameDefault, Materials.Count()+1);
	}
	else {
		mat.name = HBufC::NewL(12);
		_LIT(KMatName, "Material.%03d");
		mat.name->Des().Format(KMatName, Materials.Count()+1);
	}
	Materials.Append(mat);

	
	//si no hay objetos
	if (Objects.Count() < 1){return;}	
	Object& obj = Objects[SelectActivo];
	//si no es un mesh
	if (obj.type != mesh || !obj.seleccionado){return;}		
	Mesh& pMesh = Meshes[obj.Id];

	if (reemplazar){
		HBufC* noteBuf = HBufC::NewLC(100);
		_LIT(KFormatString, "Old Material 1 to %d");
		noteBuf->Des().Format(KFormatString, pMesh.materialsGroup.Count());
		TInt OldMaterialID = 1;
		if (pMesh.materialsGroup.Count() > 1){
			OldMaterialID = DialogNumber(1, 1, pMesh.materialsGroup.Count(), noteBuf);
		}
		pMesh.materialsGroup[OldMaterialID-1].material = Materials.Count()-1;
	}
	redibujar = true;
}

void CWhisk3D::SetMaterial(){
	//si no hay objetos
	if (Objects.Count() < 1){return;}	
	Object& obj = Objects[SelectActivo];
	//si no es un mesh
	if (obj.type != mesh){return;}	
	Mesh& pMesh = Meshes[obj.Id];

	Cancelar();
	HBufC* noteBuf = HBufC::NewLC(100);
	_LIT(KFormatString, "Old Material 1 to %d");
	noteBuf->Des().Format(KFormatString, pMesh.materialsGroup.Count());
	TInt OldMaterialID = 1;
	if (pMesh.materialsGroup.Count() > 1){
		OldMaterialID = DialogNumber(1, 1, pMesh.materialsGroup.Count(), noteBuf);
	}
	
	if (OldMaterialID > pMesh.materialsGroup.Count() || 1 > OldMaterialID){		
		noteBuf->Des().Copy(_L("Indice invalido"));	
		MensajeError(noteBuf);
	}
	else {
		_LIT(KFormatString, "New Material 1 to %d");
		noteBuf->Des().Format(KFormatString, Materials.Count());
		TInt MaterialID = DialogNumber(1, 1, Materials.Count(), noteBuf);
		
		if (MaterialID > Materials.Count() || 1 > MaterialID){		
			noteBuf->Des().Copy(_L("Indice invalido"));	
			MensajeError(noteBuf);
		}
		else {
			pMesh.materialsGroup[OldMaterialID-1].material = MaterialID-1;
		}
	}
	CleanupStack::PopAndDestroy(noteBuf);
    redibujar = true;
}

void CWhisk3D::EnfocarObject(){
	//si no hay objetos
	if (Objects.Count() < 1){return;}
	SetTransformPivotPoint();	
	PivotX = 0.0f; 
	PivotY = 0.0f;
	PivotZ = 0.0f;
	PivotX = PivotX-TransformPivotPointFloat[0]; 
	PivotY = PivotY-TransformPivotPointFloat[1];
	PivotZ = PivotZ-TransformPivotPointFloat[2];
	//EjecutarScriptPython();
    redibujar = true;
}

// Función para obtener el valor de la variable global
/*PyObject* CWhisk3D::GetShowOverlays(PyObject* self, PyObject* args){
    return PyBool_FromLong(showOverlays ? 1 : 0);
}*/

void CWhisk3D::EjecutarScriptPython(){
    // Inicializa el intérprete de Python
    Py_Initialize();
    
	// Script Python a ejecutar
	const char* script = "print('Python esta funcionando')\n"
                         "from time import time,ctime\n"
                         "print('Today is', ctime(time()))\n";

	const char* script2 =  "try:\n"
	        "    import appuifw\n"
	        "    appuifw.note(u'Importaci�n exitosa', 'info')\n"
	        "except ImportError:\n"
	        "    print('appuifw no est� disponible')\n";
    
	const char* script3 = "import appuifw\n"
		"print('Python está funcionando')\n"
		"L = [u'Fer', u'Dante', u'Leandro']\n"
		"test = appuifw.popup_menu(L, u'Quien es el mas lindo?')\n"
		"if test == 0:\n"
		"    appuifw.note(u'lo siento, segui participando :p', 'error')\n"
		"elif test == 1:\n"
		"    appuifw.note(u'obio! xD', 'conf')\n"
		"elif test == 2:\n"
		"    appuifw.note(u'no en mi juego xD', 'error')\n";

	// Ejecuta el script Python usando PyRun_SimpleString
	int result = PyRun_SimpleString(script);
	if (result != 0) {
		// Si hay un error en la ejecuci�n del script, imprime el error
		PyErr_Print();
		_LIT(KFormatString, "Error al ejecutar el script");
		HBufC* noteBuf = HBufC::NewLC(50);
		noteBuf->Des().Format(KFormatString);
		CDialogs::Alert(noteBuf);  
		CleanupStack::PopAndDestroy(noteBuf);
	}
	result = PyRun_SimpleString(script2);
	if (result != 0) {
		// Si hay un error en la ejecuci�n del script, imprime el error
		PyErr_Print();
		_LIT(KFormatString, "Error al ejecutar el script");
		HBufC* noteBuf = HBufC::NewLC(50);
		noteBuf->Des().Format(KFormatString);
		CDialogs::Alert(noteBuf);  
		CleanupStack::PopAndDestroy(noteBuf);
	}
	
	// Ruta del archivo de script
    _LIT8(KScriptPath, "E:/hola.py");

    // Convertir TDesC8 a const char*
    TPtrC8 ptrC8 = KScriptPath();
    const char* scriptPath = reinterpret_cast<const char*>(ptrC8.Ptr());

    // Abre el archivo de script
    FILE* fp = fopen(scriptPath, "r");
    if (fp != NULL){
        // Ejecuta el archivo de script
    	PyRun_SimpleFile(fp, scriptPath);
        fclose(fp);
    }
    else{
        // Si no se puede abrir el archivo, muestra un mensaje de error
        CAknInformationNote* note = new (ELeave) CAknInformationNote();
        note->ExecuteLD(_L("No se pudo abrir el script hola.py"));
    }

    // Finaliza el intérprete de Python
    //Py_Finalize();
}


void CWhisk3D::DuplicatedObject(){	
	if (estado != editNavegacion || InteractionMode != ObjectMode){return;};	
	TInt cantObjetosOriginal = Objects.Count();
	for(TInt a=0; a < cantObjetosOriginal; a++){
		Object& obj = Objects[a];
		if (!obj.seleccionado){continue;};
		Objects.Append(obj);	
		obj.seleccionado = false;	
		TInt nuevoindice = Objects.Count()-1;
		if (SelectActivo == a){
			SelectActivo = nuevoindice;
		}
		Collection.Append(nuevoindice);
		//si es un mesh
		if (obj.type == mesh){			
			Mesh tempMesh;	
			
			Meshes.Append(tempMesh);
			Mesh& originaMesh = Meshes[obj.Id];
			obj.Id = Meshes.Count()-1;
			Mesh& pMesh = Meshes[obj.Id];
			
			pMesh.vertexSize = originaMesh.vertexSize;
			pMesh.vertex = new GLshort[pMesh.vertexSize*3];
			pMesh.vertexColor = new GLubyte[pMesh.vertexSize*4];
			pMesh.normals = new GLbyte[pMesh.vertexSize*3];
			pMesh.uv = new GLfloat[pMesh.vertexSize*2];

			for (int i = 0; i < pMesh.vertexSize*3; i++) {
				pMesh.vertex[i] = originaMesh.vertex[i];
				pMesh.normals[i] = originaMesh.normals[i];
			}
			for (int i = 0; i < pMesh.vertexSize*4; i++) {
				pMesh.vertexColor[i] = originaMesh.vertexColor[i];
			}
			for (int i = 0; i < pMesh.vertexSize*2; i++) {
				pMesh.uv[i] = originaMesh.uv[i];
			}

			pMesh.facesSize = originaMesh.facesSize;

			pMesh.faces = new GLushort[pMesh.facesSize];
			for (int i = 0; i < pMesh.facesSize; i++) {
				pMesh.faces[i] = originaMesh.faces[i];
			}		

			//creamos el objeto y le asignamos la mesh	
			MaterialGroup tempFaceGroup;
			pMesh.materialsGroup.Close();
			pMesh.materialsGroup.ReserveL(originaMesh.materialsGroup.Count());
			for (TInt i = 0; i < originaMesh.materialsGroup.Count(); i++) {
				tempFaceGroup.startDrawn = originaMesh.materialsGroup[i].startDrawn;
				tempFaceGroup.material = originaMesh.materialsGroup[i].material;
				tempFaceGroup.count = originaMesh.materialsGroup[i].count;
				tempFaceGroup.indicesDrawnCount = originaMesh.materialsGroup[i].indicesDrawnCount;
				pMesh.materialsGroup.Append(tempFaceGroup);
			}
		}
	}
	SetPosicion();
    redibujar = true;
}

void CWhisk3D::DuplicatedLinked(){
	if (estado != editNavegacion || InteractionMode != ObjectMode){return;};
	TInt cantObjetosOriginal = Objects.Count();
	for(TInt a=0; a < cantObjetosOriginal; a++){
		Object& obj = Objects[a];
		if (!obj.seleccionado){continue;};
		Objects.Append(obj);	
		obj.seleccionado = false;	
		TInt nuevoindice = Objects.Count()-1;
		if (SelectActivo == a){
			SelectActivo = nuevoindice;
		}
		Collection.Append(nuevoindice);
	}
	SetPosicion();
    redibujar = true;
}

void CWhisk3D::SetSpecular(){	
	//si no hay objetos
	if (Objects.Count() < 1){return;}	
	Object& obj = Objects[SelectActivo];
	//si no es un mesh
	if (obj.type != mesh){return;}	
	Mesh& pMesh = Meshes[obj.Id];

	Cancelar();
	TInt MaterialID = 1;
	HBufC* noteBuf = HBufC::NewLC(100);
	if (pMesh.materialsGroup.Count() > 1){
		_LIT(KFormatString, "Material 1 to %d");
		noteBuf->Des().Format(KFormatString, pMesh.materialsGroup.Count());
		MaterialID = DialogNumber(1, 1, pMesh.materialsGroup.Count(), noteBuf);		
	}
	Material& mat = Materials[pMesh.materialsGroup[MaterialID-1].material];

	noteBuf->Des().Copy(_L("Valor Specular (0 - 100)"));
	TInt valor = DialogNumber((TInt)(mat.specular[0]*100.f), 0, 100, noteBuf);
	GLfloat resultado = valor/100.0f;

	mat.specular[0] = resultado;
	mat.specular[1] = resultado;
	mat.specular[2] = resultado;
	mat.specular[3] = resultado;
	CleanupStack::PopAndDestroy(noteBuf);
    redibujar = true;
}

void CWhisk3D::SetEmission(){
	//si no hay objetos
	if (Objects.Count() < 1){return;}	
	Object& obj = Objects[SelectActivo];
	//si no es un mesh
	if (obj.type != mesh){return;}	
	Mesh& pMesh = Meshes[obj.Id];

	Cancelar();
	TInt MaterialID = 1;
	HBufC* noteBuf = HBufC::NewLC(100);
	if (pMesh.materialsGroup.Count() > 1){
		_LIT(KFormatString, "Material 1 to %d");
		noteBuf->Des().Format(KFormatString, pMesh.materialsGroup.Count());
		MaterialID = DialogNumber(1, 1, pMesh.materialsGroup.Count(), noteBuf);		
	}
	Material& mat = Materials[pMesh.materialsGroup[MaterialID-1].material];
	
	noteBuf->Des().Copy(_L("Emission Roja (0 - 100)"));
	TInt valor = DialogNumber((TInt)(mat.emission[0]*100.f), 0, 100, noteBuf);
	mat.emission[0] = valor/100.0f;
    redibujar = true;

	noteBuf->Des().Copy(_L("Emission Verde (0 - 100)"));
    valor = DialogNumber((TInt)(mat.emission[1]*100.f), 0, 100, noteBuf);
    mat.emission[1] = valor/100.0f;
	redibujar = true;	

	noteBuf->Des().Copy(_L("Emission Verde (0 - 100)"));
    valor = DialogNumber((TInt)(mat.emission[2]*100.f), 0, 100, noteBuf);
    mat.emission[2] = valor/100.0f;
	CleanupStack::PopAndDestroy(noteBuf);
	redibujar = true;
}

void CWhisk3D::SetAmbientLight(){
	HBufC* buf = HBufC::NewLC( 25 );
	buf->Des().Copy(_L("Rojo (0 - 100)"));
	TInt valorR = DialogNumber((TInt)(AmbientRender[0]*100.f), 0, 100, buf);
	AmbientRender[0] = (GLfloat)valorR/100.0f;
    redibujar = true;	
	buf->Des().Copy(_L("Verde (0 - 100)"));
	TInt valorG = DialogNumber((TInt)(AmbientRender[1]*100.f), 0, 100, buf);
	AmbientRender[1] = (GLfloat)valorG/100.0f;
    redibujar = true;	
	buf->Des().Copy(_L("Azul (0 - 100)"));
	TInt valorB = DialogNumber((TInt)(AmbientRender[2]*100.f), 0, 100, buf);
	AmbientRender[2] = (GLfloat)valorB/100.0f;
	CleanupStack::PopAndDestroy(buf);
	if (view == Rendered){
		glLightModelfv( GL_LIGHT_MODEL_AMBIENT, AmbientRender );
	}
    redibujar = true;
}

void CWhisk3D::SetViewportBackgroudColor(){
	HBufC* noteBuf = HBufC::NewLC(100);
	noteBuf->Des().Copy(_L("Red (0 - 100)"));
	TInt valor = DialogNumber((TInt)(ListaColores[background][0]*100.f), 0, 100, noteBuf);
	ListaColores[background][0] = (GLfloat)valor/100.0f;
    redibujar = true;	
	noteBuf->Des().Copy(_L("Green (0 - 100)"));
	valor = DialogNumber((TInt)(ListaColores[background][1]*100.f), 0, 100, noteBuf);
	ListaColores[background][1] = (GLfloat)valor/100.0f;
    redibujar = true;	
	noteBuf->Des().Copy(_L("Blue (0 - 100)"));
	valor = DialogNumber((TInt)(ListaColores[background][2]*100.f), 0, 100, noteBuf);
	ListaColores[background][2] = (GLfloat)valor/100.0f;
	CleanupStack::PopAndDestroy(noteBuf);
    redibujar = true;		
}

void CWhisk3D::SetDiffuse(){
	//si no hay objetos
	if (Objects.Count() < 1){return;}	
	Object& obj = Objects[SelectActivo];
	//si no es un mesh
	if (obj.type != mesh){return;}	
	Mesh& pMesh = Meshes[obj.Id];

	Cancelar();
	TInt MaterialID = 1;
	HBufC* noteBuf = HBufC::NewLC(100);
	if (pMesh.materialsGroup.Count() > 1){
		_LIT(KFormatString, "Material 1 to %d");
		noteBuf->Des().Format(KFormatString, pMesh.materialsGroup.Count());
		MaterialID = DialogNumber(1, 1, pMesh.materialsGroup.Count(), noteBuf);		
	}
	Material& mat = Materials[pMesh.materialsGroup[MaterialID-1].material];

	noteBuf->Des().Copy(_L("Rojo (0 - 100)"));
	TInt valor = DialogNumber((TInt)(mat.diffuse[0]*100.f), 0, 100, noteBuf);
	mat.diffuse[0] = (GLfloat)valor/100.0f;
    redibujar = true;	
	noteBuf->Des().Copy(_L("Verde (0 - 100)"));
	valor = DialogNumber((TInt)(mat.diffuse[1]*100.f), 0, 100, noteBuf);
	mat.diffuse[1] = (GLfloat)valor/100.0f;
    redibujar = true;	
	noteBuf->Des().Copy(_L("Azul (0 - 100)"));
	valor = DialogNumber((TInt)(mat.diffuse[2]*100.f), 0, 100, noteBuf);
	mat.diffuse[2] = (GLfloat)valor/100.0f;
    redibujar = true;	
	noteBuf->Des().Copy(_L("Alpha (0 - 100)"));	
	valor = DialogNumber((TInt)(mat.diffuse[3]*100.f), 0, 100, noteBuf);
	//setea la transparencia deacuerdo al alpha
	if (valor < 100){mat.transparent = true;}
	else {mat.transparent = false;}
	mat.diffuse[3] = (GLfloat)valor/100.0f;
	
	CleanupStack::PopAndDestroy(noteBuf);
    redibujar = true;
}

void CWhisk3D::SetPerspectiva(TBool redibuja ){
	// Reinitialize viewport and projection.
	//glViewport( 0, 0, iScreenWidth, iScreenHeight );
	// Recalculate the view frustrum
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
    if ( ortografica ){
    	glOrthof(-90.0f * aspectRatio, 90.0f * aspectRatio, -90.0f, 90.0f, -0.0f, 1000.0f);
    }
    else {
		glFrustumf( FRUSTUM_LEFT * aspectRatio, FRUSTUM_RIGHT * aspectRatio,
					FRUSTUM_BOTTOM, FRUSTUM_TOP,
					FRUSTUM_NEAR, FRUSTUM_FAR );    	
    }
    glMatrixMode( GL_MODELVIEW );
    redibujar = redibuja;
}

/*void CWhisk3D::CDialogs::Alert(){
	_LIT( KAlerta, "origen.png" );
	_LIT( KAlerta2, "origen.png2" );
	CAknMessageQueryDialog* dialog = CAknMessageQueryDialog::NewL( KAlerta );

    dialog->PrepareLC( R_AVKON_MESSAGE_QUERY_DIALOG );
    dialog->ButtonGroupContainer().MakeCommandVisible( 
													EAknSoftkeyCancel, EFalse );

    dialog->QueryHeading()->SetTextL( KAlerta2 );

    dialog->RunLD();
	
    HBufC* buf = HBufC::NewLC( 4 );
    buf->Des().Copy(_L("Hola"));
    CAknMessageQueryDialog* dialog = 
                                CAknMessageQueryDialog::NewL( *buf );
    dialog->PrepareLC( R_AVKON_MESSAGE_QUERY_DIALOG );
    //dialog->ButtonGroupContainer().MakeCommandVisible(EAknSoftkeyCancel, EFalse );
	dialog->QueryHeading()->SetTextL(_L("cabeza"));
    dialog->SetHeaderTextL(_L("Example"));
    dialog->SetMessageTextL(_L("Do you want to continue?"));
    dialog->RunLD();
    delete buf;
}*/

enum{
	top,
	front,
	right,
    cameraView
};

enum{
	array,
	mirror,
	screw
};

void CWhisk3D::AddModificador(TInt opcion){
	/*Mesh& obj = Objects[SelectActivo];
	if (opcion == mirror){
		//primero crea los array temporales y les suma el espacio del nuevo vertice
		TInt copias = 2;
		GLshort* TempVertex = new GLshort[obj.vertexSize*copias];
		GLbyte* TempNormals = new GLbyte[obj.normalsSize*copias];
		GLushort* TempEdges = new GLushort[obj.edgesSize*copias];
		GLbyte* TempUv = new GLbyte[obj.uvSize*copias];
	    GLushort* TempVertexGroup = new GLushort[obj.vertexGroupSize*copias];
		GLushort** TempVertexGroupIndices = new GLushort*[obj.vertexGroupSize*copias];
		TInt* TempVertexGroupIndicesSize =  new TInt[obj.vertexGroupSize*copias];
		//copia los valores originales al array temporal
		for(TInt a=0; a < obj.vertexSize; a++){
			TempVertex[a] = TempVertex[a+obj.vertexSize] = obj.vertex[a];
			TempNormals[a] = TempNormals[a+obj.vertexSize] = obj.normals[a];
		}
		for(TInt a=0; a < obj.vertexSize/3; a++){
			TempVertex[a*3+obj.vertexSize] = -obj.vertex[a*3];
			//TempNormals[a*3+obj.vertexSize] = obj.normals[a];
		}
		for(TInt a=0; a < obj.edgesSize; a++){
			TempEdges[a] = obj.edges[a];	
			TempEdges[a+obj.edgesSize] = obj.vertexSize/3+obj.edges[a];			
		}
		for(TInt a=0; a < obj.uvSize; a++){
			TempUv[a] = TempUv[a+obj.uvSize] = obj.uv[a];
		}
		for(TInt a=0; a < obj.vertexGroupSize; a++){
			TempVertexGroupIndicesSize[a] = TempVertexGroupIndicesSize[a+obj.vertexGroupSize] = obj.vertexGroupIndiceSize[a];
			TempVertexGroupIndices[a] = TempVertexGroupIndices[a+obj.vertexGroupSize] = new GLushort[obj.vertexGroupIndiceSize[a]];
			TempVertexGroup[a] = obj.vertexGroup[a];
			TempVertexGroup[a+obj.vertexGroupSize] = obj.vertexSize/3+obj.vertexGroup[a];*/
			/*for(TInt s=0; s < obj.vertexGroupIndiceSize[a]; s++){
				TempVertexGroupIndices[a][s] = obj.vertexGroupIndice[a][s];
				TempVertexGroupIndices[a+obj.vertexGroupSize][s] = obj.vertexSize/3+obj.vertexGroupIndice[a][s];			
			}*/
		/*}
		TempVertexGroupIndices[0][0] = 11;
		TempVertexGroupIndices[1][0] = 22;
		TempVertexGroupIndices[2][0] = 33;
		TempVertexGroupIndices[3][0] = 44;
		//suma el nuevo tama�o
		obj.vertexSize = obj.vertexSize*copias;
		obj.normalsSize = obj.normalsSize*copias;
		obj.edgesSize = obj.edgesSize*copias;
		obj.uvSize = obj.uvSize*copias;
		obj.vertexGroupSize = obj.vertexGroupSize*copias;
		//asigna el nuevo tama�o
		obj.vertex =  new GLshort[obj.vertexSize];
		obj.normals = new GLbyte[obj.normalsSize];
		obj.edges =   new GLushort[obj.edgesSize];
		obj.uv =      new GLbyte[obj.uvSize];
		obj.vertexGroup = new GLushort[obj.vertexGroupSize];
		obj.vertexGroupIndiceSize = new TInt[obj.vertexGroupSize];
		obj.vertexGroupIndice = new GLushort*[obj.vertexGroupSize];
		//agrega los valores temporales al objeto
		for(TInt a=0; a < obj.vertexSize; a++){
			obj.vertex[a] = TempVertex[a];
			obj.normals[a] = TempNormals[a];			
		}
		for(TInt a=0; a < obj.edgesSize; a++){
			obj.edges[a] = TempEdges[a];			
		}
		for(TInt a=0; a < obj.uvSize; a++){
			obj.uv[a]= TempUv[a];			
		}
		for(TInt a=0; a < obj.vertexGroupSize; a++){
			obj.vertexGroup[a] = TempVertexGroup[a];	
			obj.vertexGroupIndiceSize[a] = TempVertexGroupIndicesSize[a];
			obj.vertexGroupIndice[a] = new GLushort[obj.vertexGroupIndiceSize[a]];
			for(TInt s=0; s < obj.vertexGroupIndiceSize[a]; s++){
				obj.vertexGroupIndice[a][s] = TempVertexGroupIndices[a][s];	
			}
		}

		TempVertexGroupIndices[0][0] = 11;
		TempVertexGroupIndices[1][0] = 22;
		TempVertexGroupIndices[2][0] = 33;
		TempVertexGroupIndices[3][0] = 44;*/
		/*HBufC* noteBuf = HBufC::NewLC(30); //TInt::Length(obj.vertexGroupSize)
		_LIT(KFormatString, "indices %d %d %d %d");
		noteBuf->Des().Format(KFormatString,  
				TempVertexGroupIndices[0][0],
				TempVertexGroupIndices[1][0],
				TempVertexGroupIndices[2][0],
				TempVertexGroupIndices[3][0]);
		Mensaje(noteBuf);*/
		//libera memoria
		/*delete[] TempVertex;
		delete[] TempNormals;
		delete[] TempEdges;
		delete[] TempUv;
		delete[] TempVertexGroup;
		delete[] TempVertexGroupIndicesSize;
		delete[] TempVertexGroupIndices;*/
		//CleanupStack::PopAndDestroy(noteBuf);
	//}
	/*redibujar = true;*/
}

//mira si no hay camara activa
//si no hay una camara activa. busca una camara para asignarla
//si no hay camaras... quedara en -1
void CWhisk3D::CheckCameraState(){
	if (CameraActive < 0){
		for(TInt i=0; i < Objects.Count(); i++){
			if (Objects[i].type == camera){
				CameraActive = i;
				return;
			}		
		}
	}	
}

void CWhisk3D::SetActiveObjectAsCamera(){
	if (Objects.Count() < 1){return;}	
	Object& obj = Objects[SelectActivo];
	if (!obj.seleccionado){return;}	
	//si es una camara. ahora va a ser la camara activa
	if (obj.type == camera){
		CameraActive = SelectActivo;		
	}	
	redibujar = true;
}

void CWhisk3D::SetViewpoint(TInt opcion){
	switch (opcion) {
		case top:
			rotX = -180.0;
			rotY = 90.0;
			ViewFromCameraActive = false;	
			CameraToView = false;
			break;
		case front:
			rotX = -180.0;
			rotY = 0.0;	
			ViewFromCameraActive = false;	
			CameraToView = false;
			break;
		case right:
			rotX = 90.0;
			rotY = 0.0;		
			ViewFromCameraActive = false;	
			CameraToView = false;
			break;
		case cameraView:
			CheckCameraState();
			if (CameraActive < 0){
				_LIT(KFormatString, "There are no cameras!");
				HBufC* noteBuf = HBufC::NewLC(50);
				noteBuf->Des().Format(KFormatString);
				MensajeError(noteBuf);  
				CleanupStack::PopAndDestroy(noteBuf);
			}
			else if (Objects.Count() > CameraActive && !ViewFromCameraActive){	
				LastRotX = rotX;
				LastRotY = rotY;	
				LastPivotX = PivotX;
				LastPivotY = PivotY;
				LastPivotZ = PivotZ;
				RecalcViewPos();
				ViewFromCameraActive = true;
			}
			break;
	}
	redibujar = true;
}


void CWhisk3D::RestaurarViewport(){
	ViewFromCameraActive = false;
	rotX = LastRotX;
	rotY = LastRotY;	
	PivotX = LastPivotX;
	PivotY = LastPivotY;
	PivotZ = LastPivotZ;
}

void CWhisk3D::SetCameraToView(){	
	CameraToView = !CameraToView;
}

void CWhisk3D::RecalcViewPos(){		
	Object& obj = Objects[CameraActive];
	rotX = -obj.rotZ+90;
	rotY = -obj.rotY;	
	PivotX = -obj.posX;
	PivotY = -obj.posY;
	PivotZ = -obj.posZ;

	//en caso de que este emparentado
	/*TInt ParentID = obj.Parent;
	while (ParentID  > -1) {		
		Object& parentObj = Objects[ParentID];		
		rotX = -parentObj.rotZ;
		rotY = -parentObj.rotY;	
		//PivotX = -parentObj.posX;
		//PivotY = -parentObj.posY;
		//PivotZ = -parentObj.posZ;	
		ParentID = parentObj.Parent;		
	}*/
}

void CWhisk3D::InfoObject(TInt opciones){
	HBufC* noteBuf = HBufC::NewLC(100);	
	_LIT(KFormatString, "Posicion: x: %d, y: %d, z: %d");
	noteBuf->Des().Format(KFormatString, Objects[SelectActivo].posX, Objects[SelectActivo].posY, Objects[SelectActivo].posZ);
	CDialogs::Alert(noteBuf);	
	
	_LIT(KFormatString2, "Rotacion: x: %d, y: %d, z: %d");
	noteBuf->Des().Format(KFormatString2, Objects[SelectActivo].rotX, Objects[SelectActivo].rotY, Objects[SelectActivo].rotZ);
	CDialogs::Alert(noteBuf);	

	_LIT(KFormatString3, "Scale: x: %d, y: %d, z: %d");
	noteBuf->Des().Format(KFormatString3, Objects[SelectActivo].scaleX, Objects[SelectActivo].scaleY, Objects[SelectActivo].scaleZ);
	CDialogs::Alert(noteBuf);

	CleanupStack::PopAndDestroy(noteBuf);	
	/*if (opciones == 1){
		HBufC* noteBuf = HBufC::NewLC(30); //TInt::Length(obj.vertexGroupSize)
		_LIT(KFormatString, "size %d Indices %d");
		noteBuf->Des().Format(KFormatString,  Objects[SelectActivo].vertexGroupIndiceSize[SelectActivo],
				              Objects[SelectActivo].vertexGroupIndice[SelectActivo][0]);
				                                                                 //,
				              //Objects[SelectActivo].vertexGroupIndice[SelectActivo][1],
				              //Objects[SelectActivo].vertexGroupIndice[SelectActivo][2]);
		Mensaje(noteBuf);	
		CleanupStack::PopAndDestroy(noteBuf);
	}
	else if (opciones == 1){ //cantidad de vertices
		HBufC* noteBuf = HBufC::NewLC(30); //TInt::Length(obj.vertexGroupSize)
		_LIT(KFormatString, "obj: %d Vertices: %d");
		noteBuf->Des().Format(KFormatString, SelectActivo+1, Objects[SelectActivo].vertexSize/3);
		Mensaje(noteBuf);	
		CleanupStack::PopAndDestroy(noteBuf);	
	}	
	else if (opciones == 2){ //cantidad de vertices
		HBufC* noteBuf = HBufC::NewLC(35); //TInt::Length(obj.vertexGroupSize)
		_LIT(KFormatString, "VertexGroup: %d group: %d");
		noteBuf->Des().Format(KFormatString, Objects[SelectActivo].vertexGroupSize, Objects[SelectActivo].vertexGroupIndiceSize[SelectActivo]);
		Mensaje(noteBuf);	
		CleanupStack::PopAndDestroy(noteBuf);	
	}	*/
}; 


void CWhisk3D::Mensaje(HBufC* noteBuf){	        	
	CAknInformationNote* note = new (ELeave) CAknInformationNote();
	note->ExecuteLD(*noteBuf);
	//CleanupStack::PopAndDestroy(noteBuf);
};

void CWhisk3D::MensajeError(HBufC* noteBuf){
    CAknErrorNote* note = new (ELeave) CAknErrorNote();
    note->ExecuteLD(*noteBuf);
    //CleanupStack::PopAndDestroy(noteBuf);
}

/*TBool CWhisk3D::CDialogs::Alert(HBufC* noteBuf){	  
    TBool retVal( EFalse );
    CAknQueryDialog* query = CAknQueryDialog::NewL();
    if( query->ExecuteLD( R_ACCEPT_INVITATION_DLG, *noteBuf )) {
        retVal = ETrue;
    }
	CleanupStack::PopAndDestroy( noteBuf );

    return retVal;
}*/

/*TBool CWhisk3D::CDialogs::Alert(HBufC* noteBuf) {
    TBool retVal(EFalse);
	CAknQueryDialog* query = CAknQueryDialog::NewL();
	if (query->ExecuteLD(R_ACCEPT_INVITATION_DLG, *noteBuf)) {
		retVal = ETrue;
	}
    return retVal;
}


TBool CWhisk3D::CDialogs::Alert(const TDesC& noteBuf) {
    TBool retVal(EFalse);
	CAknQueryDialog* query = CAknQueryDialog::NewL();
	if (query->ExecuteLD(R_ACCEPT_INVITATION_DLG, noteBuf)) {
		retVal = ETrue;
	}

    return retVal;
}*/

TInt CWhisk3D::DialogNumber(TInt valor, TInt min, TInt max, HBufC* noteBuf ){ //TPtrC etiqueta
	TInt number = valor;
	CAknNumberQueryDialog* dlg = CAknNumberQueryDialog::NewL( number );
	dlg->PrepareLC( R_AVKON_DIALOG_QUERY_VALUE_NUMBER );
	dlg->SetPromptL(*noteBuf);
	dlg->SetMinimumAndMaximum( min, max );
	if (dlg->RunLD()){
		number = number;
	}    
    return number;
}

TPtr CWhisk3D::DialogText(HBufC* textBuf, HBufC* noteBuf) {
    TPtr textPtr = textBuf->Des();
    
    CAknTextQueryDialog* dlg = CAknTextQueryDialog::NewL(textPtr);
    dlg->PrepareLC(R_AVKON_DIALOG_QUERY_VALUE_TEXT);
    dlg->SetPromptL(*noteBuf);    
    dlg->RunLD();
	return textPtr;
}

void CWhisk3D::ShowWaitDialogL(){
	iWaitDialog = new (ELeave) CAknWaitDialog((REINTERPRET_CAST(CEikDialog**, &iWaitDialog)), ETrue);
	//iWaitDialog->SetCallback(this); // Opcional: establece un callback para eventos del dialogo
	iWaitDialog->ExecuteLD(R_WHISK3D_WAIT_NOTE_SOFTKEY_CANCEL);
};

void CWhisk3D::CloseWaitDialog(){
    if (iWaitDialog){
        iWaitDialog->ProcessFinishedL(); // Esto cierra el cuadro de espera.
        iWaitDialog = NULL; // Asegúrate de que el puntero sea nulo después de cerrar el cuadro de espera.
    }
};

void CWhisk3D::DialogWait(HBufC* noteBuf){
    ShowWaitDialogL(); // Mostrar el cuadro de espera

    // Aqui puedes agregar el codigo que necesita tiempo para ejecutarse.
    /*User::After(2000000); // Simular una espera de 2 segundos.

    CloseWaitDialog(); // Cerrar el cuadro de espera*/
}

TInt CWhisk3D::ShowOptionsDialogL() {	
	/*HBufC* noteBuf = HBufC::NewLC(100);	
	noteBuf->Des().Copy(_L("Ver wait?"));
	DialogWait(noteBuf);*/
	ShowWaitDialogL();
	//CleanupStack::PopAndDestroy(noteBuf);	
	
	///iContainer->ShowProgressNoteUnderSingleProcessL(R_WHISK3D_PROGRESS_NOTE, EAknExNoteCtrlIdProgressNote);
    //iContainer->ShowWaitNoteL(R_WHISK3D_PROGRESS_NOTE, EAknExNoteCtrlIdWaitNote);
    //R_WHISK3D_WAIT_NOTE_SOFTKEY_CANCEL r_whisk3D_wait_note_softkey_cancel
	
	TInt blee = 1;
	return blee;
    /*TInt index = 0; // Variable para almacenar el �ndice seleccionado
    CAknListQueryDialog* dialog = new (ELeave) CAknListQueryDialog(&index); // Crear una instancia de CAknListQueryDialog

    const TInt KNumberOfItems = 3;
    CDesC16ArrayFlat* itemTextArray = new (ELeave) CDesC16ArrayFlat(KNumberOfItems);
    CleanupStack::PushL(itemTextArray);

    // Agregar los textos de las opciones al array de texto
    itemTextArray->AppendL(_L("Option 1"));
    itemTextArray->AppendL(_L("Option 2"));
    itemTextArray->AppendL(_L("Option 3"));

    dialog->PrepareLC(R_AVKON_DIALOG_POPUP_LIST); // Preparar el di�logo
    dialog->SetItemTextArray(itemTextArray); // Establecer el array de texto como opciones
    dialog->SetOwnershipType(ELbmOwnsItemArray);

    dialog->RunLD(); // Ejecutar el di�logo

    CleanupStack::PopAndDestroy(itemTextArray); // Limpiar el array de texto

    return index;*/
}

//recalcula la geometria de la camaras
void CWhisk3D::SetCameraGeometria( TUint aWidth, TUint aHeight ){
	//ancho
	CameraVertices[5]  = CameraVertices[8]  =  aWidth*5;
	CameraVertices[11] = CameraVertices[14] = -aWidth*5;
	//alto
	CameraVertices[4] = CameraVertices[13] =  aHeight*5;
	CameraVertices[7] = CameraVertices[10] = -aHeight*5;
	//triangulo alto
	CameraVertices[16] = CameraVertices[19] = aHeight*5+260;
	CameraVertices[22] = CameraVertices[16] + 1400;	
}

/*static GLshort CameraVertices[CameraVertexSize] = {   

    0, 0, 0, //origen
    3500,    1440, 1920,
    3500,    -1440, 1920,
    3500,    -1440, -1920,
    3500,    1440, -1920,
    //tiangulo
    15,    1700, 1280,
    18,    1700, -1280,
    21,    3100, 0,
};*/

// -----------------------------------------------------------------------------
// CWhisk3D::SetScreenSize
// Reacts to the dynamic screen size change during execution of this program.
// -----------------------------------------------------------------------------
//
void CWhisk3D::SetScreenSize( TUint aWidth, TUint aHeight, TBool widescreen = false ){
    iScreenWidth  = aWidth;
    iScreenHeight = aHeight;
	SetCameraGeometria(aWidth, aHeight);
	iScreenHeightSplit = (GLfloat)aHeight/2;
    
    // Notify the texture manager of screen size change
    iTextureManager->SetScreenSize( aWidth, aHeight );

    // Reinitialize viewport and projection.
    glViewport( 0, 0, iScreenWidth, iScreenHeight );

    // Recalculate the view frustrum
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    aspectRatio = (GLfloat)(iScreenWidth) / (GLfloat)(iScreenHeight);
    if (widescreen && iScreenWidth > iScreenHeight){
        aspectRatio = (GLfloat)(16) / (GLfloat)(9);    	
    };
    if ( !ortografica ){
		glFrustumf( FRUSTUM_LEFT * aspectRatio, FRUSTUM_RIGHT * aspectRatio,
					FRUSTUM_BOTTOM, FRUSTUM_TOP,
					FRUSTUM_NEAR, FRUSTUM_FAR );
    }
    else {
    	glOrthof(-90.0f * aspectRatio, 90.0f * aspectRatio, -90.0f, 90.0f, -5.0f, 1000.0f);    	
    }
    glMatrixMode( GL_MODELVIEW );
	redibujar = true;
}

void CWhisk3D::SetScreenSize( TUint aWidth, TUint aHeight ){
    iScreenWidth  = aWidth;
    iScreenHeight = aHeight;
	SetCameraGeometria(aWidth, aHeight);
	iScreenHeightSplit = (GLfloat)aHeight/2;
    
    // Notify the texture manager of screen size change
    iTextureManager->SetScreenSize( aWidth, aHeight );

    // Reinitialize viewport and projection.
    glViewport( 0, 0, iScreenWidth, iScreenHeight );

    // Recalculate the view frustrum
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    aspectRatio = (GLfloat)(iScreenWidth) / (GLfloat)(iScreenHeight);
    if ( !ortografica ){
		glFrustumf( FRUSTUM_LEFT * aspectRatio, FRUSTUM_RIGHT * aspectRatio,
					FRUSTUM_BOTTOM, FRUSTUM_TOP,
					FRUSTUM_NEAR, FRUSTUM_FAR );
    }
    else {
    	glOrthof(-90.0f * aspectRatio, 90.0f * aspectRatio, -90.0f, 90.0f, -5.0f, 1000.0f);    	
    }
    glMatrixMode( GL_MODELVIEW );
	redibujar = true;
}

// Funcion para obtener el directorio raiz de un filePath
TFileName CWhisk3D::GetRootDirectory(const TDesC& aFilePath)
{
    TParse parse;
    parse.Set(aFilePath, NULL, NULL);
    return parse.DriveAndPath();
}

//import
void CWhisk3D::NewTexture(){
	//si no hay objetos
	if (Objects.Count() < 1){return;}	
	Object& obj = Objects[SelectActivo];
	//si no es un mesh
	if (obj.type != mesh){return;}	
	Mesh& pMesh = Meshes[obj.Id];

	Cancelar();
	TInt MaterialID = 1;
	if (pMesh.materialsGroup.Count() > 1){
		HBufC* noteBuf = HBufC::NewLC(100);
		_LIT(KFormatString, "Material 1 to %d");
		noteBuf->Des().Format(KFormatString, pMesh.materialsGroup.Count());
		MaterialID = DialogNumber(1, 1, pMesh.materialsGroup.Count(), noteBuf);	
		CleanupStack::PopAndDestroy(noteBuf);	
	}
	Material& mat = Materials[pMesh.materialsGroup[MaterialID-1].material];

    _LIT(KTitle, "Selecciona la Textura");
    TFileName filePath;
    if (AknCommonDialogs::RunSelectDlgLD(filePath, R_WHISK3D_SELECT_DIALOG, KTitle)) {
		
		iTextureManager = CTextureManager::NewL ( iScreenWidth, iScreenHeight,
												FRUSTUM_TOP, FRUSTUM_BOTTOM, FRUSTUM_RIGHT, FRUSTUM_LEFT, FRUSTUM_NEAR,
												this );
		TTexture newTexture;

		// Crear un objeto TParse para analizar la ruta del archivo
        TParse parse;
        parse.Set(filePath, NULL, NULL);

        // Obtener el nombre del archivo seleccionado
        //const TFileName& selectedFileName = parse.Name();
        //const TFileName& selectedFileName = parse.FullName();
        const TFileName& selectedFileName = parse.NameAndExt();
        
		newTexture.iTextureName = selectedFileName;
		
		Textures.Append(newTexture);
		mat.textura = true;
		//mat.textureID = Textures[Textures.Count()-1].iID;
		mat.textureID = Textures.Count();
    	redibujar = true;

		// Agregar la nueva textura a la cola de carga del TextureManager
		TFileName directorioRaiz = GetRootDirectory(filePath);
		iTextureManager->RequestToLoad(newTexture.iTextureName, directorioRaiz, &Textures[Textures.Count()-1], false);
		//iTextureManager->RequestToLoad(KTextureName, &Textures[Textures.Count()-1], false);

		// Iniciar la carga de texturas
		iTextureManager->DoLoadL();
    }
}

void CWhisk3D::LeerMTL(const TFileName& aFile, TInt objetosCargados) {
	RFs fsSession2;	
	User::LeaveIfError(fsSession2.Connect());

	RFile rFile;
	TInt err;
	
	TRAP(err,rFile.Open(fsSession2, aFile, EFileRead));
	if (err != KErrNone){
		_LIT(KFormatString, "Error al abrir: %S");
        HBufC* noteBuf = HBufC::NewLC(aFile.Length() + 16);
        noteBuf->Des().Format(KFormatString, &aFile);
        MensajeError(noteBuf);
        return;
    }

	TBuf8<2048> buffer;
	TInt pos = 0;
	TInt64 startPos = 0; // Variable para mantener la posicion de lectura en el archivo
	TInt fileSize;
	rFile.Size(fileSize);

	//necesario para modificar el material correcto	
	Object& obj = Objects[SelectActivo];
	/*RArray<Object&> objs;	
	objs.Append(Objects[SelectActivo-3])
	Object& obj = Objects[SelectActivo];*/

	Mesh& pMesh = Meshes[obj.Id];
	Material* mat = NULL; 
	HBufC* materialName16 = HBufC::NewLC(180);
	HBufC* noteBuf3 = HBufC::NewLC(180);
	TBool encontrado = false;

	// Cargar la textura desde la ruta absoluta
	TBool HaytexturasQueCargar = false;
	iTextureManager = CTextureManager::NewL(iScreenWidth, iScreenHeight,
											FRUSTUM_TOP, FRUSTUM_BOTTOM, FRUSTUM_RIGHT, FRUSTUM_LEFT, FRUSTUM_NEAR,
											this); 

	while (startPos < fileSize) {
		// Leer una linea del archivo desde la posicion actual
		err = rFile.Read(startPos, buffer, buffer.MaxLength());
		if (err != KErrNone) {
			_LIT(KFormatString, "Error al leer linea");
			HBufC* noteBuf = HBufC::NewLC(100);
			noteBuf->Des().Format(KFormatString);
			MensajeError(noteBuf);
    		rFile.Close();	
    		fsSession2.Close();	
            //CleanupStack::PopAndDestroy(&fsSession);
			break;
		}      

        while ((pos = buffer.Locate('\n')) != KErrNotFound || (pos = buffer.Locate('\r')) != KErrNotFound) {
			//TInt indice = 0;
            TPtrC8 line = buffer.Left(pos);

            if (line.Length() > 0) {
                if (line.Left(7) == _L8("newmtl ")) {
                    TLex8 lex(line.Mid(7));
                    TPtrC8 materialName = lex.NextToken();

					// Convertir materialName de TPtrC8 (8 bits) a HBufC (16 bits)
					//delete materialName16;
					materialName16 = CnvUtfConverter::ConvertToUnicodeFromUtf8L(materialName);

					/*_LIT(KFormatString3, "newmtl %S");
					noteBuf3->Des().Format(KFormatString3, materialName16);
					CDialogs::Alert(noteBuf3);*/

					//buscar el material con el mismo nombre
					encontrado = false;
					for(int f=0; f < pMesh.materialsGroup.Count(); f++){
						//no se puede usar el material 0. ese el que es por defecto y no se toca por mas que se llame igual
						if (Materials[pMesh.materialsGroup[f].material].name->Compare(*materialName16) == 0 && pMesh.materialsGroup[f].material != 0){
							mat = &Materials[pMesh.materialsGroup[f].material];
							/*_LIT(KFormatString3, "newmtl %S encontrado\nMaterial: %d");
							noteBuf3->Des().Format(KFormatString3, materialName16, pMesh.materials[f]+1);
							CDialogs::Alert(noteBuf3);*/
							encontrado = true;
							break;
						}
					}	
                } 
				else if (encontrado){
					//specular
					if (line.Left(3) == _L8("Ns ")) {
						TLex8 lex(line.Mid(3));
						TReal nsValue;
						lex.Val(nsValue, '.');
						GLfloat resultado = nsValue/1000.0f;
						mat->specular[0] = resultado;
						mat->specular[1] = resultado;
						mat->specular[2] = resultado;
						mat->specular[3] = resultado;
						
						/*_LIT(KFormatString3, "newmtl %S encontrado\nSpecular: %f");
						noteBuf3->Des().Format(KFormatString3, materialName16, resultado);
						CDialogs::Alert(noteBuf3);*/
					} 
					//difusso, Aqui manejas el color ambiental Ka (kaR, kaG, kaB)			
					else if (line.Left(3) == _L8("Kd ")) {
						TLex8 lex(line.Mid(3));
						TReal kdR, kdG, kdB;
						lex.Val(kdR, '.');
						lex.SkipSpace();
						lex.Val(kdG, '.');
						lex.SkipSpace();
						lex.Val(kdB, '.');

						mat->diffuse[0] = (GLfloat)kdR;
						mat->diffuse[1] = (GLfloat)kdG;
						mat->diffuse[2] = (GLfloat)kdB;
					} 
					/*else if (line.Left(3) == _L8("Ks ")) {
						TLex8 lex(line.Mid(3));
						TReal ksR, ksG, ksB;
						lex.Val(ksR, '.');
						lex.SkipSpace();
						lex.Val(ksG, '.');
						lex.SkipSpace();
						lex.Val(ksB, '.');
						// Aqui manejas el color especular Ks (ksR, ksG, ksB)
					}*/
					// Aqui manejas el color de emision Ke (keR, keG, keB)
					else if (line.Left(3) == _L8("Ke ")) {
						TLex8 lex(line.Mid(3));
						TReal keR, keG, keB;
						lex.Val(keR, '.');
						lex.SkipSpace();
						lex.Val(keG, '.');
						lex.SkipSpace();
						lex.Val(keB, '.');	
						mat->emission[0] = (GLfloat)keR;
						mat->emission[1] = (GLfloat)keG;
						mat->emission[2] = (GLfloat)keB;
					} 
					/*else if (line.Left(3) == _L8("Ni ")) {
						TLex8 lex(line.Mid(3));
						TReal niValue;
						lex.Val(niValue, '.');
						// Aqui manejas el indice de refraccion Ni
					}*/
					//opacidad 
					else if (line.Left(2) == _L8("d ")) {
						TLex8 lex(line.Mid(2));
						TReal dValue;
						lex.Val(dValue, '.');					
						mat->diffuse[3] = (GLfloat)dValue;
						//setea la transparencia deacuerdo al alpha
						if (dValue < 1.f){mat->transparent = true;}
						else {mat->transparent = false;}
					} 
					/*else if (line.Left(6) == _L8("illum ")) {
						TLex8 lex(line.Mid(6));
						TInt illumValue;
						lex.Val(illumValue);
						// Aqui manejas el modelo de iluminacion illum
					}*/
					// Aqui manejas la textura difusa map_Kd
					else if (line.Left(18) == _L8("BackfaceCullingOff")){
						mat->culling = false;
					}
					else if (line.Left(7) == _L8("NoLight")){
						mat->lighting = false;
					}
					else if (line.Left(6) == _L8("map_d ")){
						mat->transparent = true;
					}
					else if (line.Left(7) == _L8("map_Kd ")) {
						HaytexturasQueCargar = true;
						TLex8 lex(line.Mid(7));
						TPtrC8 texturePath = lex.NextToken();

						// Convertir la ruta relativa a una ruta absoluta
						HBufC* texturePath16 = HBufC::NewLC(180);
						texturePath16 = CnvUtfConverter::ConvertToUnicodeFromUtf8L(texturePath);
						TParse fileParser;
						fileParser.Set(aFile, NULL, NULL);
						TFileName absolutePath = fileParser.DriveAndPath();
						absolutePath.Append(*texturePath16);

						// Comprobar si la textura existe
						RFs fs;
						fs.Connect();
						//User::LeaveIfError(fs.Connect());
						TEntry entry;
						TInt err = fs.Entry(absolutePath, entry);
						if (err == KErrNone) {						
							TTexture newTexture;
							newTexture.iTextureName = *texturePath16;
							Textures.Append(newTexture);
							mat->textura = true;
							mat->textureID = Textures.Count();

							iTextureManager->RequestToLoad(newTexture.iTextureName, fileParser.DriveAndPath(), &Textures[Textures.Count() - 1], false);
						} else {
							// El archivo no existe, manejar el error
							_LIT(KFileNotFound, "No existe la textura '%S'");
							noteBuf3->Des().Format(KFileNotFound, texturePath16);
							MensajeError(noteBuf3);
						}
						fs.Close();
						CleanupStack::PopAndDestroy(texturePath16);
					}				
				}
            }

            startPos += pos + 1;
            buffer.Delete(0, pos + 1);
            buffer.TrimLeft();
        }
    }
	CleanupStack::PopAndDestroy(materialName16);
	CleanupStack::PopAndDestroy(noteBuf3);
    rFile.Close();	
    fsSession2.Close();	

	//si hay texturas. las lee
	if (HaytexturasQueCargar){		
		iTextureManager->DoLoadL();
	}
    //CleanupStack::PopAndDestroy(&fsSession);
}

void CWhisk3D::ImportAnimation(){
	//si no hay objetos
	if (Objects.Count() < 1){return;}	
	Object& obj = Objects[SelectActivo];
	if (!obj.seleccionado){return;}

    _LIT(KTitle, "Import Animation (.txt)");
    TFileName file(KNullDesC);
    if (AknCommonDialogs::RunSelectDlgLD(file, R_WHISK3D_SELECT_DIALOG, KTitle)){		
    	RFs fsSession;	
    	User::LeaveIfError(fsSession.Connect());
    	CleanupClosePushL(fsSession);

		// Revisar la extension del archivo
		TPtrC extension = file.Right(4);  // Obtiene las ultimas 4 letras del nombre del archivo
		if (extension.CompareF(_L(".txt")) != 0) {
			_LIT(KExtensionError, "Error: El archivo seleccionado no tiene la extension .txt");
			HBufC* noteBuf = HBufC::NewLC(180);
			noteBuf->Des().Format(KExtensionError);
			MensajeError(noteBuf);
			CleanupStack::PopAndDestroy(noteBuf);
			fsSession.Close();
			return;
		}

    	RFile rFile;
    	TInt err;	

    	TRAP(err,rFile.Open(fsSession, file, EFileRead));
		if (err != KErrNone){
			_LIT(KFormatString, "Error al abrir: %S");
			HBufC* noteBuf = HBufC::NewLC(file.Length()+16);
			noteBuf->Des().Format(KFormatString, &file);
			MensajeError(noteBuf);
			CleanupStack::PopAndDestroy(noteBuf);
			rFile.Close();
			fsSession.Close();
			return;
		}	

		Cancelar();

		TInt64 startPos = 0;
		TBool continuarLeyendo = true; // Variable para controlar la lectura del archivo
		TBuf8<2048> buffer;
		TInt pos = 0;
		TInt fileSize;
		rFile.Size(fileSize);

		/*if (AnimationObjects.Count() < 1){				
			AnimationObject NewAnim;	
			AnimationObjects.Append(NewAnim);
			AnimationObject& anim = AnimationObjects[AnimationObjects.Count()-1];	
			anim.Id = SelectActivo;
			
			AnimProperty propNew;
			anim.Propertys.Append(propNew);
			AnimProperty& prop = anim.Propertys[anim.Propertys.Count()-1];
			prop.Property = AnimPosition;			
		}*/
		TInt animIndex = 0;
		TInt propIndex = 0;
		
		keyFrame key;
		key.Interpolation = 0;					
		key.valueX = 0;
		key.valueY =  0;
		key.valueZ =  0;				
		key.frame = 0;

		while (continuarLeyendo && startPos < fileSize ) {
			// Leer una linea del archivo desde la posicion actual
			err = rFile.Read(startPos, buffer, buffer.MaxLength());
			if (err != KErrNone) {
				//tarde o temprano va a fallar la lectura y va a parar
				// Manejar error al leer
				_LIT(KFormatString, "Error al leer linea");
				HBufC* noteBuf = HBufC::NewLC(100);
				noteBuf->Des().Format(KFormatString);
				MensajeError(noteBuf);
				continuarLeyendo = false; // Salir del bucle
				break;
			}	

			// Procesar la linea hasta que no haya mas caracteres en buffer
			while (continuarLeyendo && (pos = buffer.Locate('\n')) != KErrNotFound || (pos = buffer.Locate('\r')) != KErrNotFound) {
				TPtrC8 line = buffer.Left(pos);
			
				// Contador para almacenar la cantidad de "strings" separados por espacios
				TInt contador = 0;
				if (line.Length() > 0) {					
					if (line.Left(9) == _L8("rotacion ")) {						
						animIndex = BuscarAnimacionObj();
						//si no existe la animacion. la crea
						if (animIndex < 0){							
							AnimationObject NewAnim;	
							AnimationObjects.Append(NewAnim);
							animIndex = AnimationObjects.Count()-1;	
						}
						AnimationObject& anim = AnimationObjects[animIndex];
						anim.Id = SelectActivo;
							
						propIndex = BuscarAnimProperty(animIndex, AnimRotation);
						//si no existe el animProp lo crea
						if (propIndex < 0){		
							AnimProperty propNew;
							anim.Propertys.Append(propNew);
							propIndex = anim.Propertys.Count()-1;
						}
						AnimProperty& prop = anim.Propertys[propIndex];
						prop.Property = AnimRotation;
						
						TLex8 lex(line.Mid(9));  // Inicializa TLex con la subcadena a partir del tercer caracter
						// Iterar mientras no se llegue al final del descriptor y se haya alcanzado el limite de 8 strings
						contador = 0;
						while (!lex.Eos() && contador < 2) {							
							TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador
							TLex8 testLex(currentString); // Crear un nuevo objeto TLex para la prueba
							
							TInt number = 0;
							TInt err = testLex.Val(number);
							if (err == KErrNone && number > 0) {	
								prop.keyframes.ReserveL(number); // Reservar espacio para los keyframes
									
								/*HBufC* noteBuf3 = HBufC::NewLC(180);
								_LIT(KFormatString33, "reservado %d");
								noteBuf3->Des().Format(KFormatString33, number);
								CDialogs::Alert(noteBuf3);
								CleanupStack::PopAndDestroy(noteBuf3);*/
							}
							contador++;
							lex.SkipSpace();
						}
					}
					else if (line.Left(2) == _L8("r ")){	
						AnimationObject& anim = AnimationObjects[animIndex];
						AnimProperty& prop = anim.Propertys[propIndex];					
						TInt indiceKey = prop.keyframes.Count();
						prop.keyframes.Append(key);

						TLex8 lex(line.Mid(2));
						// Iterar mientras no se llegue al final del descriptor y se haya alcanzado el limite de 8 strings
						contador = 0;
						while (!lex.Eos() && contador < 4) {							
							TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador
							TLex8 testLex(currentString); // Crear un nuevo objeto TLex para la prueba
							
							TInt number = 0;
							TInt err = testLex.Val(number);
							if (err == KErrNone){
								switch (contador) {
									case 0:
										prop.keyframes[indiceKey].frame = number;
										break;
									case 1:
										prop.keyframes[indiceKey].valueX = (GLfloat)number;
										break;
									case 2:
										prop.keyframes[indiceKey].valueZ = (GLfloat)number;	
										break;
									case 3:	
										prop.keyframes[indiceKey].valueY = (GLfloat)number;	
										break;
								}
							}
							
							contador++;
							lex.SkipSpace();				
									
							/*HBufC* noteBuf3 = HBufC::NewLC(180);
							_LIT(KFormatString33, "frame %d\nRotacion\nX: %f, Y: %f, Z: %f");
							noteBuf3->Des().Format(KFormatString33, prop.keyframes[indiceKey].frame, 
												prop.keyframes[indiceKey].valueX, prop.keyframes[indiceKey].valueY, prop.keyframes[indiceKey].valueZ);
							CDialogs::Alert(noteBuf3);
							CleanupStack::PopAndDestroy(noteBuf3);*/
						}
					}
					else if (line.Left(9) == _L8("locacion ")) {
						animIndex = BuscarAnimacionObj();
						//si no existe la animacion. la crea
						if (animIndex < 0){							
							AnimationObject NewAnim;	
							AnimationObjects.Append(NewAnim);
							animIndex = AnimationObjects.Count()-1;	
						}
						AnimationObject& anim = AnimationObjects[animIndex];
						anim.Id = SelectActivo;
							
						propIndex = BuscarAnimProperty(animIndex, AnimPosition);
						//si no existe el animProp lo crea
						if (propIndex < 0){		
							AnimProperty propNew;
							anim.Propertys.Append(propNew);
							propIndex = anim.Propertys.Count()-1;
							
							/*HBufC* noteBuf3 = HBufC::NewLC(180);
							_LIT(KFormatString33, "se creo position\ncontador: %d");
							noteBuf3->Des().Format(KFormatString33, anim.Propertys.Count());
							CDialogs::Alert(noteBuf3);
							CleanupStack::PopAndDestroy(noteBuf3);*/
						}
						AnimProperty& prop = anim.Propertys[propIndex];
						prop.Property = AnimPosition;	
						
						TLex8 lex(line.Mid(9));  // Inicializa TLex con la subcadena a partir del tercer caracter
						// Iterar mientras no se llegue al final del descriptor y se haya alcanzado el limite de 8 strings
						contador = 0;
						while (!lex.Eos() && contador < 2) {							
							TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador
							TLex8 testLex(currentString); // Crear un nuevo objeto TLex para la prueba
							
							TInt number = 0;
							TInt err = testLex.Val(number);
							if (err == KErrNone && number > 0) {	
								prop.keyframes.ReserveL(number); // Reservar espacio para los keyframes
							}
							contador++;
							lex.SkipSpace();
						}
					}
					else if (line.Left(2) == _L8("l ")){	
						AnimationObject& anim = AnimationObjects[animIndex];
						AnimProperty& prop = anim.Propertys[propIndex];					
						TInt indiceKey = prop.keyframes.Count();
						prop.keyframes.Append(key);

						TLex8 lex(line.Mid(2));
						// Iterar mientras no se llegue al final del descriptor y se haya alcanzado el limite de 8 strings
						contador = 0;
						while (!lex.Eos() && contador < 4) {							
							TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador
							TLex8 testLex(currentString); // Crear un nuevo objeto TLex para la prueba
							
							TInt number = 0;
							TInt err = testLex.Val(number);
							if (err == KErrNone){
								switch (contador) {
									case 0:
										prop.keyframes[indiceKey].frame = number;
										break;
									case 1:
										prop.keyframes[indiceKey].valueX = (GLfloat)number;
										break;
									case 2:
										prop.keyframes[indiceKey].valueY = (GLfloat)number;	
										break;
									case 3:
										prop.keyframes[indiceKey].valueZ = (GLfloat)number;		
										break;
								}
							}
							
							contador++;
							lex.SkipSpace();
						}
					}
					else if (line.Left(7) == _L8("escala ")) {
						animIndex = BuscarAnimacionObj();
						//si no existe la animacion. la crea
						if (animIndex < 0){							
							AnimationObject NewAnim;	
							AnimationObjects.Append(NewAnim);
							animIndex = AnimationObjects.Count()-1;	
						}
						AnimationObject& anim = AnimationObjects[animIndex];
						anim.Id = SelectActivo;
							
						propIndex = BuscarAnimProperty(animIndex, AnimScale);
						//si no existe el animProp lo crea
						if (propIndex < 0){		
							AnimProperty propNew;
							anim.Propertys.Append(propNew);
							propIndex = anim.Propertys.Count()-1;
						}
						AnimProperty& prop = anim.Propertys[propIndex];
						prop.Property = AnimScale;	
						
						TLex8 lex(line.Mid(7));  // Inicializa TLex con la subcadena a partir del tercer caracter
						// Iterar mientras no se llegue al final del descriptor y se haya alcanzado el limite de 8 strings
						contador = 0;
						while (!lex.Eos() && contador < 2) {							
							TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador
							TLex8 testLex(currentString); // Crear un nuevo objeto TLex para la prueba
							
							TInt number = 0;
							TInt err = testLex.Val(number);
							if (err == KErrNone && number > 0) {	
								prop.keyframes.ReserveL(number); // Reservar espacio para los keyframes
							}
							contador++;
							lex.SkipSpace();
						}
					}
					else if (line.Left(2) == _L8("s ")){	
						AnimationObject& anim = AnimationObjects[animIndex];
						AnimProperty& prop = anim.Propertys[propIndex];					
						TInt indiceKey = prop.keyframes.Count();
						prop.keyframes.Append(key);

						TLex8 lex(line.Mid(2));
						// Iterar mientras no se llegue al final del descriptor y se haya alcanzado el limite de 8 strings
						contador = 0;
						while (!lex.Eos() && contador < 4) {							
							TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador
							TLex8 testLex(currentString); // Crear un nuevo objeto TLex para la prueba
							
							TInt number = 0;
							TInt err = testLex.Val(number);
							if (err == KErrNone){
								switch (contador) {
									case 0:
										prop.keyframes[indiceKey].frame = number;
										break;
									case 1:
										prop.keyframes[indiceKey].valueX = (GLfloat)number;
										break;
									case 2:
										prop.keyframes[indiceKey].valueY = (GLfloat)number;	
										break;
									case 3:
										prop.keyframes[indiceKey].valueZ = (GLfloat)number;		
										break;
								}
							}
							
							contador++;
							lex.SkipSpace();
						}
					}
				}
				if (continuarLeyendo){
					// Actualizar la posicion de inicio para la proxima lectura
					startPos += pos + 1;

					// Eliminar la parte de la linea ya procesada y el caracter de salto de linea
					buffer.Delete(0, pos + 1);
					buffer.TrimLeft(); // Eliminar espacios en blanco iniciales
				}
			}
		}

		// Cerrar el archivo
		rFile.Close();
		fsSession.Close();
		/*AnimationObject& anim = AnimationObjects[animIndex];
		AnimProperty& prop = anim.Propertys[propIndex];
		prop.SortKeyFrames();*/

		redibujar = true;
	}
    else {
    	_LIT(KFormatString, "Error al leer el Archivo");
		HBufC* noteBuf = HBufC::NewLC(24);
		noteBuf->Des().Format(KFormatString);
		MensajeError(noteBuf);  
		CleanupStack::PopAndDestroy(noteBuf);
    }
}

void CWhisk3D::ImportOBJ(){    
    _LIT(KTitle, "Import Wavefront (.obj)");
    TFileName file(KNullDesC);
    if (AknCommonDialogs::RunSelectDlgLD(file, R_WHISK3D_SELECT_DIALOG, KTitle)){		
    	RFs fsSession;	
    	User::LeaveIfError(fsSession.Connect());
    	CleanupClosePushL(fsSession);

		// Revisar la extension del archivo
		TPtrC extension = file.Right(4);  // Obtiene las últimas 4 letras del nombre del archivo
		if (extension.CompareF(_L(".obj")) != 0) {
			_LIT(KExtensionError, "Error: El archivo seleccionado no tiene la extension .obj");
			HBufC* noteBuf = HBufC::NewLC(180);
			noteBuf->Des().Format(KExtensionError);
			MensajeError(noteBuf);
			CleanupStack::PopAndDestroy(noteBuf);
			fsSession.Close();
			return;
		}

    	RFile rFile;
    	TInt err;	

    	TRAP(err,rFile.Open(fsSession, file, EFileRead));
		if (err != KErrNone){
			_LIT(KFormatString, "Error al abrir: %S");
			HBufC* noteBuf = HBufC::NewLC(file.Length()+16);
			noteBuf->Des().Format(KFormatString, &file);
			MensajeError(noteBuf);
			CleanupStack::PopAndDestroy(noteBuf); 
			rFile.Close();
			fsSession.Close();
			return;
		}	
		/*if (file){
			_LIT(KFormatString, "Formato no valido");
			HBufC* noteBuf = HBufC::NewLC(100);
			noteBuf->Des().Format(KFormatString, &file);
			MensajeError(noteBuf); 
			return;
		}			*/
		//crea el objeto
		Cancelar();

		TInt64 startPos = 0; // Variable para mantener la posicion de lectura en el archivo
		//esto se hace para no cerrar el archivo y por cada nuevo obj encontrado simplemente volvemos a usar leerOBJ con el archivo donde quedo
		//TBool hayMasObjetos;
		TInt objetosCargados = 0;		
		TInt acumuladoVertices = 0;
		TInt acumuladoNormales = 0;
		TInt acumuladoUVs = 0;
		while (LeerOBJ(&fsSession, &rFile, &file, &startPos, &acumuladoVertices, &acumuladoNormales, &acumuladoUVs)) { // && objetosCargados < 1
			objetosCargados++;
		}

		// Cerrar el archivo
		rFile.Close();
		fsSession.Close();

		TFileName mtlFile = file;
		mtlFile.Replace(file.Length() - 4, 4, _L(".mtl"));

		RFs fs;
	    User::LeaveIfError(fs.Connect()); // Asegurarse de que fs se conecta correctamente
	    CleanupClosePushL(fs);
	    
		TEntry entry;
		err = fs.Entry(mtlFile, entry);

		//si el archivo existe. no tendria que marcar error
		if (err == KErrNone) {
			TRAP(err, LeerMTL(mtlFile, objetosCargados));
			//si ocurrio algun error al leerlo
			if (err != KErrNone) {
				_LIT(KFormatString, "Error al leer el archivo .mtl");
				HBufC* noteBuf = HBufC::NewLC(100);
				noteBuf->Des().Format(KFormatString);
				MensajeError(noteBuf);
				CleanupStack::PopAndDestroy(noteBuf);
			}
		} else {
            // El archivo no existe, manejar el error
            _LIT(KFileNotFound, "El archivo .mtl no existe");
            HBufC* noteBuf = HBufC::NewLC(100);
            noteBuf->Des().Format(KFileNotFound);
            MensajeError(noteBuf);
            CleanupStack::PopAndDestroy(noteBuf);
        }
		fs.Close();

		redibujar = true;
	}	
    else {
    	_LIT(KFormatString, "Error al leer el Archivo");
		HBufC* noteBuf = HBufC::NewLC(24);
		noteBuf->Des().Format(KFormatString);
		MensajeError(noteBuf);  
		CleanupStack::PopAndDestroy(noteBuf);
    }
}


TBool CWhisk3D::LeerOBJ(RFs* fsSession, RFile* rFile, TFileName* file, TInt64* startPos,
		TInt* acumuladoVertices,
		TInt* acumuladoNormales,
		TInt* acumuladoUVs
	){
	TInt err;
	Object obj;
	TBool NombreEncontrado = false;
	TBool hayMasObjetos = false;
	obj.visible = true;
	obj.posX = obj.posY = obj.posZ = 0;
	obj.rotX = obj.rotY = obj.rotZ = 0;
	obj.scaleX = obj.scaleY = obj.scaleZ = 65000;
	obj.Id = 0;		
	obj.type = mesh;

	Wavefront Wobj;
	Wobj.Reset();
	Face NewFace;
	FaceCorners NewFaceCorners;
	TBool TieneVertexColor = false;
	TInt acumuladoVerticesProximo = 0;
	TInt acumuladoNormalesProximo = 0;
	TInt acumuladoUVsProximo = 0;

	TBool continuarLeyendo = true; // Variable para controlar la lectura del archivo
	TBuf8<2048> buffer;
	TInt pos = 0;
	TInt fileSize;
	rFile->Size(fileSize);

	while (continuarLeyendo && *startPos < fileSize ) {
		// Leer una linea del archivo desde la posicion actual
		err = rFile->Read(*startPos, buffer, buffer.MaxLength());
		if (err != KErrNone) {
			//tarde o temprano va a fallar la lectura y va a parar
			// Manejar error al leer
			_LIT(KFormatString, "Error al leer linea");
			HBufC* noteBuf = HBufC::NewLC(100);
			noteBuf->Des().Format(KFormatString);
			MensajeError(noteBuf);
			continuarLeyendo = false; // Salir del bucle
			break;
		}
		// Procesar la linea hasta que no haya mas caracteres en buffer
		while (continuarLeyendo && (pos = buffer.Locate('\n')) != KErrNotFound || (pos = buffer.Locate('\r')) != KErrNotFound) {
			TPtrC8 line = buffer.Left(pos);
		
			// Contador para almacenar la cantidad de "strings" separados por espacios
			TInt contador = 0;
			if (line.Length() > 0) {
				if (!NombreEncontrado && line.Left(2) == _L8("o ")) {
					//evita el crasheo en caso de que no tenga materiales
					//se le asigna el material por defecto
					//MaterialesNuevos.Append(0);
					//MeshsGroups.Append(0);

					TLex8 lex(line.Mid(2));
					if (!lex.Eos()){
						TPtrC8 currentString = lex.NextToken();							
						obj.name = HBufC::NewL(currentString.Length());
						obj.name->Des().Copy(currentString);
					}
					
					/*HBufC* noteBuf3 = HBufC::NewLC(180);
					_LIT(KFormatString3, "Nombre de objeto: %S");
					noteBuf3->Des().Format(KFormatString3, obj.name);
					CDialogs::Alert(noteBuf3);
					CleanupStack::PopAndDestroy(noteBuf3);*/

					NombreEncontrado = true;
				}
				//si encuentra otro objeto para
				else if (NombreEncontrado && line.Left(2) == _L8("o ")) {
					continuarLeyendo = false; // Salir del bucle
					hayMasObjetos = true;
				}
				else {
					if (line.Left(2) == _L8("v ")) {
						contador = 0;
						Wobj.vertex.ReserveL(Wobj.vertex.Count()+3);
						Wobj.vertexColor.ReserveL(Wobj.vertexColor.Count()+3);
						acumuladoVerticesProximo++;

						TLex8 lex(line.Mid(2));  // Inicializa TLex con la subcadena a partir del tercer caracter
						// Iterar mientras no se llegue al final del descriptor y se haya alcanzado el limite de 8 strings
						while (!lex.Eos() && contador < 6) {		
							TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador					
							TLex8 testLex(currentString);// Crear un nuevo objeto TLex para la prueba
							
							// Convertir el string en un número TInt
							TReal number = 0.0;
							TInt err = testLex.Val(number, '.');
							if (err == KErrNone){
								if (contador < 3) {
									number = number*2000;								
									GLshort glNumber = static_cast<GLshort>(number);
									Wobj.vertex.Append(glNumber);							
								}
								//que el vertice tenga el color es una mierda. se hara un nuevo formato y se guardara en el face corner...							
								else if (contador < 6){
									number = number*255.0;	
									if (number > 255.0){number = 255.0;}	
									GLshort glNumber = static_cast<GLubyte>(number); // Conversion a GLbyte
									Wobj.vertexColor.Append(glNumber);	
									TieneVertexColor = true;
									
									/*HBufC* noteBuf3 = HBufC::NewLC(180);
									_LIT(KFormatString3, "se agrego color: %f, %d");
									noteBuf3->Des().Format(KFormatString3, number, glNumber);
									CDialogs::Alert(noteBuf3);
									CleanupStack::PopAndDestroy(noteBuf3);*/
								}
							}

							// Avanzar al siguiente "string" que no sea espacio en blanco
							lex.SkipSpace();

							// Incrementar el contador para llevar la cuenta de los strings procesados
							contador++;
						}
						//en caso de no tener color
						while (contador < 6) {		
							if (contador > 2){
								Wobj.vertexColor.Append(255);	
							}
							contador++;
						}
					}
					else if (line.Left(3) == _L8("vn ")) {
						acumuladoNormalesProximo++;
						contador = 0;
						Wobj.normals.ReserveL(Wobj.normals.Count()+3);
						TLex8 lex(line.Mid(3));  // Inicializa TLex con la subcadena a partir del tercer caracter
						// Iterar mientras no se llegue al final del descriptor y se haya alcanzado el limite de 8 strings
						while (!lex.Eos() && contador < 3) {							
							TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador
							TLex8 testLex(currentString); // Crear un nuevo objeto TLex para la prueba
							
							TReal number = 0.0;
							TInt err = testLex.Val(number, '.');
							if (err == KErrNone && contador < 3) {
								number = ((number +1)/2)* 255.0 - 128.0;
								if (number > 127.0){number = 127.0;}
								else if (number < -128.0){number = -128.0;}
								GLbyte glNumber = static_cast<GLbyte>(number); // Conversion a GLbyte
								Wobj.normals.Append(glNumber);
							}
							lex.SkipSpace();
							contador++;
						}			
					}
					else if (line.Left(3) == _L8("vt ")) {	
						acumuladoUVsProximo++;				
						contador = 0;
						Wobj.uv.ReserveL(Wobj.uv.Count()+2);

						TLex8 lex(line.Mid(3));  // Inicializa TLex con la subcadena a partir del tercer caracter
						// Iterar mientras no se llegue al final del descriptor y se haya alcanzado el limite de 8 strings
						while (!lex.Eos() && contador < 2) {							
							TPtrC8 currentString = lex.NextToken(); // Mostrar el mensaje con el valor actual del "string" y el contador
							TLex8 testLex(currentString); // Crear un nuevo objeto TLex para la prueba
							
							TReal number = 0.0;
							TInt err = testLex.Val(number, '.');
							if (err == KErrNone) {	
								GLfloat glNumber;
								switch (contador) {
									case 0:
										number = number * 255.0 - 128.0;
										glNumber = static_cast<GLfloat>(number); // Conversion a GLbyte
										//newVertexUV.u = glNumber;
										Wobj.uv.Append(glNumber);
										break;
									case 1:
										number = -number+1;
										number = number * 255.0 - 128.0;
										glNumber = static_cast<GLfloat>(number); // Conversion a GLbyte
										//number = number * 255.0 - 128.0;
										//newVertexUV.v = glNumber;
										Wobj.uv.Append(glNumber);
										break;
									default:
										break;
								}		
							}
							contador++;
							lex.SkipSpace();
						}	
					}
					else if (line.Left(2) == _L8("f ")) {
						contador = 0;
						TInt conTBarras = 0;

						TLex8 lex(line.Mid(2));  // Inicializa TLex con la subcadena a partir del tercer caracter

						Wobj.faces.Append(NewFace);
						Face& TempFace = Wobj.faces[Wobj.faces.Count()-1];
						MaterialGroup& UltimoMG = Wobj.materialsGroup[Wobj.materialsGroup.Count()-1];
						
						//HBufC* noteBuf4 = HBufC::NewLC(180);								
						while (!lex.Eos()) {		
							TPtrC8 currentString = lex.NextToken();

							TInt startPos2 = 0; // Posicion inicial
							TInt tokenLength = 0; // Longitud del token
							conTBarras = 0;

							// Recorre la cadena hasta encontrar el final
							while (startPos2 < currentString.Length()) {
								// Busca la posicion de la siguiente barra diagonal
								TInt nextSlashPos = currentString.Mid(startPos2).Locate('/');

								// Si no se encuentra una barra diagonal, asume que es el último token
								if (nextSlashPos == KErrNotFound) {
									tokenLength = currentString.Length() - startPos2;
								} else {
									tokenLength = nextSlashPos; // Longitud del token hasta la barra diagonal
								}

								// Extrae el token utilizando la posicion y longitud calculadas
								TPtrC8 token = currentString.Mid(startPos2, tokenLength);
								TLex8 testLex(token); // Crear un nuevo objeto TLex para la prueba

								// Convertir el string en un número TInt
								TInt number = 0;
								TInt err = testLex.Val(number);
								//v1/vt1/vn1
								if (err == KErrNone) {
									/*_LIT(KFormatString5, "contador: %d\nconTBarras: %d\nnumber: %d");
									noteBuf4->Des().Format(KFormatString5, contador, conTBarras, number);
									CDialogs::Alert(noteBuf4);*/
									//resetea el contador de barras si se pasa
									if (conTBarras > 2){
										//NewFaceGroup.FaceCorners.Append(TempFaceCorner);
										//NewFaceCorner = NewFaceGroup.FaceCorners[NewFaceGroup.FaceCorners.Count()-1];
										conTBarras = 0;
									};
									switch (conTBarras) {
										case 0:
											NewFaceCorners.vertex = number-1;
											break;
										case 1:
											NewFaceCorners.uv = number-1;
											break;
										case 2:
											//se da por hecho que aca se termino un triangulo
											NewFaceCorners.normal = number-1;
											TempFace.corner.Append(NewFaceCorners);
											break;
										default:
											break;
									}	
									//cada face corner extra es un triangulo
									if (contador > 1 && conTBarras == 2){
										Wobj.facesCount++;
										Wobj.facesSize += 3;	
										UltimoMG.count++;
										UltimoMG.indicesDrawnCount += 3;
									}
									/*_LIT(KFormatString6, "facesCount: %d\nfacesSize: %d");
									noteBuf4->Des().Format(KFormatString6, Wobj.facesCount, Wobj.facesSize);
									CDialogs::Alert(noteBuf4);*/
								}

								// Actualiza la posicion inicial para el proximo token
								startPos2 += tokenLength + 1; // Suma 1 para omitir la barra diagonal
								conTBarras++;
							}	

							lex.SkipSpace();
							contador++;				
						}

						// Crear triangulos de la lista temporal de indices
						//TInt numIndices = tempIndices.Count();

						/*_LIT(KFormatString4, "Num. indices: %d\nCaras: %d");
						noteBuf3->Des().Format(KFormatString4, numIndices, contador);
						CDialogs::Alert(noteBuf3);*/

						//si se resta 2 al contador. el contador es el numero de caras
						//contador -= 2;
						// Esto resuelve tanto triangulos, quads como ngones
						//ListCaras.ReserveL(ListCaras.Count() +3*contador); // Reservar espacio para los elementos
						//el contador es cuantos vertices hay. un triangulo tiene minimo 3. si es un quad o ngone tiene mas
						/*for (TInt c = 0; c < contador; ++c) {
							for (TInt i = 0; i < 3; ++i) {
								//el primer vertice de los triangulos es el primero
								if (i == 0){
									ListCaras.Append(tempIndices[0]);
									ListCaras.Append(tempIndices[1]);
									ListCaras.Append(tempIndices[2]);
								}
								else {									
									ListCaras.Append(tempIndices[3*c+ i*3  ]);
									ListCaras.Append(tempIndices[3*c+ i*3+1]);
									ListCaras.Append(tempIndices[3*c+ i*3+2]);
								}
							}
						}	*/
						/*HBufC* noteBuf3 = HBufC::NewLC(180);
						_LIT(KFormatString5, "tempIndices\ncontador: %d\nCaras: %d\nListCaras: %d");
						noteBuf3->Des().Format(KFormatString5, contador, tempIndices.Count()/3,ListCaras.Count());
						CDialogs::Alert(noteBuf3);
						CleanupStack::PopAndDestroy(noteBuf3);*/

						// Limpiar la lista temporal de indices
						//tempIndices.Close();

						// Actualizar el contador de caras en el último grupo de mallas
						//MeshsGroups[MeshsGroups.Count()-1] += (contador - 2);
						/*if (TempMaterialGroup.Count() < 1){
							MaterialGroup tempFaceGroup;
							tempFaceGroup.count = tempFaceGroup.indicesDrawnCount = tempFaceGroup.material = 0;
							tempFaceGroup.start = facesSize;
							TempMaterialGroup.Append(tempFaceGroup);
						}*/
						//TempMaterialGroup[TempMaterialGroup.Count()-1].count += contador;
						/*NewMesh.facesCount++;
						NewMesh.facesSize += 3;
						TempMaterialGroup[TempMaterialGroup.Count()-1].count ++;
						TempMaterialGroup[TempMaterialGroup.Count()-1].indicesDrawnCount += 3;*/

						//MeshsGroups[MeshsGroups.Count()-1]++;
					}
					else if (line.Left(7) == _L8("usemtl ")) {	
						MaterialGroup tempFaceGroup;
						tempFaceGroup.count = 0;
						tempFaceGroup.indicesDrawnCount = 0;
						tempFaceGroup.start = Wobj.facesSize/3;
						tempFaceGroup.startDrawn = Wobj.facesSize;
						tempFaceGroup.material = 0;

						Wobj.materialsGroup.Append(tempFaceGroup);
						tempFaceGroup.count = 0;
						Material mat;	
						mat.specular[0] = mat.specular[1] = mat.specular[2] = mat.specular[3] = 0.3;
						mat.diffuse[0] = mat.diffuse[1] = mat.diffuse[2] = mat.diffuse[3] = 1.0;
						mat.emission[0] = mat.emission[1] = mat.emission[2] = mat.emission[3] = 0.0;
						mat.textura = false;
						mat.vertexColor = TieneVertexColor;
						mat.repeat = true;
						mat.lighting = true;
						mat.culling = true;
						mat.transparent = false;
						mat.interpolacion = lineal;
						mat.textureID = 0;
						HBufC* materialName16 = HBufC::NewLC(180);

						TInt encontrado = -1;	
						TLex8 lex(line.Mid(7));
						if (!lex.Eos()){
							TPtrC8 currentString = lex.NextToken();							
							mat.name = HBufC::NewL(currentString.Length());
							mat.name->Des().Copy(currentString);

							//busca si existe el material
							materialName16 = CnvUtfConverter::ConvertToUnicodeFromUtf8L(currentString);
							for(int f=0; f < Materials.Count(); f++){
								if (Materials[f].name->Compare(*materialName16) == 0 && f != 0){
									encontrado = f;
									break;
								}
							}
						}
						else {	
							mat.name = HBufC::NewL(100);						
							_LIT(KMatName, "Material.%03d");
							mat.name->Des().Format(KMatName, Materials.Count()+1);
						}

						/*HBufC* noteBuf3 = HBufC::NewLC(180);
						_LIT(KFormatString3, "Material: %d\nNombre: %S");
						noteBuf3->Des().Format(KFormatString3, Materials.Count(), mat.name);
						CDialogs::Alert(noteBuf3);
						CleanupStack::PopAndDestroy(noteBuf3);*/

						//si se encontro no crea el nuevo material
						if (encontrado > -1){
							/*HBufC* noteBuf3 = HBufC::NewLC(180);
							_LIT(KFormatString3, "newmtl %S\nencontrado %d\nmateriales num: %d");
							noteBuf3->Des().Format(KFormatString3, materialName16, encontrado, Materials.Count());
							CDialogs::Alert(noteBuf3);*/
							Wobj.materialsGroup[Wobj.materialsGroup.Count()-1].material = encontrado;
						}
						else {
							/*HBufC* noteBuf3 = HBufC::NewLC(180);
							_LIT(KFormatString3, "no encontrado %S");
							noteBuf3->Des().Format(KFormatString3, materialName16);
							CDialogs::Alert(noteBuf3);*/
							Materials.Append(mat);
							Wobj.materialsGroup[Wobj.materialsGroup.Count()-1].material = Materials.Count()-1;
						}
						CleanupStack::PopAndDestroy(materialName16);
					}
				}
			}
			if (continuarLeyendo){
				// Actualizar la posicion de inicio para la proxima lectura
				*startPos += pos + 1;

				// Eliminar la parte de la linea ya procesada y el caracter de salto de linea
				buffer.Delete(0, pos + 1);
				buffer.TrimLeft(); // Eliminar espacios en blanco iniciales
			}
		}
	}
	
    Mesh NewMesh;
	Meshes.Append(NewMesh);
	obj.Id = Meshes.Count()-1;
	Mesh& TempMesh = Meshes[obj.Id];
	Wobj.ConvertToES1(TempMesh, acumuladoVertices, acumuladoNormales, acumuladoUVs);
	*acumuladoVertices += acumuladoVerticesProximo;
	*acumuladoNormales += acumuladoNormalesProximo;
	*acumuladoUVs += acumuladoUVsProximo;
	Objects.Append(obj);
	SelectActivo = Objects.Count()-1;
	Collection.Append(SelectActivo);
	
	DeseleccionarTodo();
	Objects[SelectActivo].seleccionado = true;
	SelectCount = 1;

	/*HBufC* noteBuf3 = HBufC::NewLC(180);
	_LIT(KFormatString4, "vertices: %d\nCaras: %d\nMateriales: %d");
	noteBuf3->Des().Format(KFormatString4, Meshes[obj.Id].vertexSize/3, Meshes[obj.Id].facesSize/3, Meshes[obj.Id].materialsGroup.Count());
	CDialogs::Alert(noteBuf3);*/

	HBufC* noteBuf = HBufC::NewLC(180);
	_LIT(KFormatString, "se creo la malla 3D");
	noteBuf->Des().Format(KFormatString);
	CDialogs::Alert(noteBuf);
	CleanupStack::PopAndDestroy(noteBuf);

	return hayMasObjetos;
};

struct __attribute__((packed)) TBMPFileHeader {
    TUint16 bfType;
    TUint32 bfSize;
    TUint16 bfReserved1;
    TUint16 bfReserved2;
    TUint32 bfOffBits;
};

struct TBMPInfoHeader {
    TUint32 biSize;
    TInt32 biWidth;
    TInt32 biHeight;
    TUint16 biPlanes;
    TUint16 biBitCount;
    TUint32 biCompression;
    TUint32 biSizeImage;
    TInt32 biXPelsPerMeter;
    TInt32 biYPelsPerMeter;
    TUint32 biClrUsed;
    TUint32 biClrImportant;
};

void CWhisk3D::SaveAsBMP(int width, int height, const GLubyte* pixels, const TDesC& fileName) {
    RFs fsSession;
    RFile file;
    TInt err = fsSession.Connect();
    if (err != KErrNone) {
        return;
    }

    // Crear la carpeta si no existe
    _LIT(KDirName, "E:\\whisk3D\\");
    err = fsSession.MkDirAll(KDirName);
    if (err != KErrNone && err != KErrAlreadyExists) {
		fsSession.Close();
		return;
    }

	err = file.Replace(fsSession, fileName, EFileWrite);
    if (err != KErrNone) {
        fsSession.Close();
        return;
    }

    TBMPFileHeader fileHeader;
    TBMPInfoHeader infoHeader;

    TInt32 rowSize = ((24 * width + 31) / 32) * 4;  // Row size with padding
    TUint32 imageSize = rowSize * height;  // Image size in bytes
    fileHeader.bfType = 0x4D42;  // 'BM'
    fileHeader.bfSize = sizeof(TBMPFileHeader) + sizeof(TBMPInfoHeader) + imageSize;
    fileHeader.bfReserved1 = 0;
    fileHeader.bfReserved2 = 0;
    fileHeader.bfOffBits = sizeof(TBMPFileHeader) + sizeof(TBMPInfoHeader);

	/*HBufC* noteBuf = HBufC::NewLC(200);
	_LIT(KFormatString, "TBMPFileHeader: %d\nTBMPInfoHeader:%d");
	noteBuf->Des().Format(KFormatString, sizeof(TBMPFileHeader), sizeof(TBMPInfoHeader));

	_LIT(KFormatString2, "rowSize: %d\nimage size:%d\nbfSize: %d\nbfReserved1: %d\nbfReserved2: %d");
	noteBuf->Des().Format(KFormatString2, rowSize, imageSize, fileHeader.bfSize, fileHeader.bfReserved1, fileHeader.bfReserved2);
	CDialogs::Alert(noteBuf);	
	CleanupStack::PopAndDestroy(noteBuf);*/

    infoHeader.biSize = sizeof(TBMPInfoHeader);
    infoHeader.biWidth = width;
    infoHeader.biHeight = height;
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 24;  // 24 bits per pixel
    infoHeader.biCompression = 0;
    infoHeader.biSizeImage = imageSize;
    infoHeader.biXPelsPerMeter = 0;
    infoHeader.biYPelsPerMeter = 0;
    infoHeader.biClrUsed = 0;
    infoHeader.biClrImportant = 0;

    TPtrC8 fileHeaderPtr(reinterpret_cast<const TUint8*>(&fileHeader), sizeof(fileHeader));
    file.Write(fileHeaderPtr);
    TPtrC8 infoHeaderPtr(reinterpret_cast<const TUint8*>(&infoHeader), sizeof(infoHeader));
    file.Write(infoHeaderPtr);

    // Convert from RGBA to RGB
    GLubyte* rgbPixels = new GLubyte[imageSize];
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int srcIndex = (y * width + x) * 4;  // Source RGBA index
            int destIndex = y * rowSize + x * 3;  // Destination RGB index with padding
            rgbPixels[destIndex] = pixels[srcIndex+2];        // R
            rgbPixels[destIndex + 1] = pixels[srcIndex + 1]; // G
            rgbPixels[destIndex + 2] = pixels[srcIndex ]; // B
        }
    }

    TPtrC8 pixelsPtr(reinterpret_cast<const TUint8*>(rgbPixels), imageSize);
    file.Write(pixelsPtr);

    delete[] rgbPixels;
    file.Close();
    fsSession.Close();
}

void CWhisk3D::SaveCanvasToImage(TBool secuencia, TBool showUi)  {
    // Redibuja el canvas sin los overlays
    TBool originalShowOverlays = showOverlays;
    TBool originalShowUi = ShowUi;
    ShowUi = showUi;    
    showOverlays = false;

    // Tamaño temporal del canvas
    /*TUint temScreenWidth  = iScreenWidth;
    TUint temScreenHeight = iScreenHeight;

    // Nuevo tamaño del canvas
    TUint ScreenWidth  = 320; // Puedes cambiar esto al tamaño que desees
    TUint ScreenHeight = 320; // Puedes cambiar esto al tamaño que desees

    SetScreenSize(ScreenWidth, ScreenHeight, false);*/

    // Captura los pixeles de la pantalla
    GLubyte* pixels = new GLubyte[iScreenWidth * iScreenHeight * 4]; // 4 para RGBA

    _LIT(KFileName, "E:\\whisk3D\\render_%04d.bmp");
    _LIT(KFileNameZbuffer, "E:\\whisk3D\\renderZbuffer_%04d.bmp");
    TFileName fileName;	
	TBuf<256> buffer;

	if (secuencia){
		CurrentFrame = StartFrame;
		ReloadAnimation();
		for (int r = 0; r < EndFrame; ++r) {	
			redibujar = true;
			AppCycle(0, 0, 0);		
    		glReadPixels(0, 0, iScreenWidth, iScreenHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
			buffer.Format(KFileName, CurrentFrame);
			fileName.Copy(buffer);
			SaveAsBMP(iScreenWidth, iScreenHeight, pixels, fileName);
			CurrentFrame++;
			ReloadAnimation();
		}
	}
	else {   
		redibujar = true;
		AppCycle(0, 0, 0);
		glReadPixels(0, 0, iScreenWidth, iScreenHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

		//postprocesado
		//applyBlur(pixels, iScreenWidth, iScreenHeight, 3);

		buffer.Format(KFileName, CurrentFrame);
		fileName.Copy(buffer);
		SaveAsBMP(iScreenWidth, iScreenHeight, pixels, fileName);

		SimularZBuffer = true;
		redibujar = true;
		AppCycle(0, 0, 0);	
		glReadPixels(0, 0, iScreenWidth, iScreenHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

		buffer.Format(KFileNameZbuffer, CurrentFrame);
		fileName.Copy(buffer);
		SaveAsBMP(iScreenWidth, iScreenHeight, pixels, fileName);
		SimularZBuffer = false;
	}

    // Restaura el estado original
    showOverlays = originalShowOverlays;
    ShowUi = originalShowUi;
    delete[] pixels;

    // Restaura el tamaño original del canvas
    //SetScreenSize(temScreenWidth, temScreenHeight, false);
    redibujar = true;
}

void CWhisk3D::applyBlur(GLubyte* pixels, int width, int height, int radius) {
    // Calcula el tamaño del area del kernel
    int kernelSize = 2 * radius + 1;
    
    // Itera sobre cada pixel de la imagen
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float r = 0.0f, g = 0.0f, b = 0.0f;
            
            // Itera sobre cada pixel en el area del kernel
            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    int pixelX = clamp(x + kx, 0, width - 1);
                    int pixelY = clamp(y + ky, 0, height - 1);
                    
                    // Obtiene el valor del pixel original
                    int index = (pixelY * width + pixelX) * 4;
                    r += pixels[index];
                    g += pixels[index + 1];
                    b += pixels[index + 2];
                }
            }
            
            // Calcula el promedio de los valores RGB en el area del kernel
            int index = (y * width + x) * 4;
            pixels[index] = static_cast<GLubyte>(r / (kernelSize * kernelSize));
            pixels[index + 1] = static_cast<GLubyte>(g / (kernelSize * kernelSize));
            pixels[index + 2] = static_cast<GLubyte>(b / (kernelSize * kernelSize));
        }
    }
}

int CWhisk3D::clamp(int value, int min, int max) {
    if (value < min) {
        return min;
    } else if (value > max) {
        return max;
    } else {
        return value;
    }
}

void CWhisk3D::SetWidescreen(){
	iWidescreenEnabled = !iWidescreenEnabled;
	SetScreenSize( iScreenWidth, iScreenHeight, iWidescreenEnabled );
}

//  End of File
