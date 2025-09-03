//tipos de objetos
enum { mesh, camera, light, empty, armature, curve };
enum { Solid, MaterialPreview, Rendered };
enum {
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

enum{
	top,
	front,
	right,
    cameraView
};

enum{
	ObjectMode
};

enum { trackto, copyrotation, copylocation };

//interpolacion
enum {lineal, closest};

enum{array, mirror,	screw, boolean};

enum { pointLight, sunLight };

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

typedef enum { X, Y, Z, XYZ } Axis;

int axisSelect = X;

//luz
//GLfloat light_pos[] = { -5000.0f, 5000.0f, 5000.0f, 1.0f };
//GLfloat light_diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
//GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
GLfloat objAmbient[4] = { 0.4, 0.4, 0.4, 1.0 };

GLfloat lightDiffuseLamp[4]   = { 0.8, 0.8, 0.8, 1.0 };
GLfloat sunLightPosition[4]  = {-100, 1000, 1000, 0 }; // y, z, x, si es direccional o puntual

//para las luces puntuales
GLfloat positionPuntualLight[4] = {0, 0, 0, 1};
GLfloat lightAmbient[4]   = { 0.0, 0.0, 0.0, 1.0 };

GLshort TransformPivotPoint[3] = {0,0,0};
GLfloat TransformPivotPointFloat[3] = {0.0f,0.0f,0.0f};

int nextLightId = GL_LIGHT1;

float angle = 55.0f;
bool SimularZBuffer = false;
bool ViewFromCameraActive = false;
float FRUSTUM_NEAR  = 1.f;     //near depth clipping plane
float FRUSTUM_FAR = 1000.f;     //far depth clipping plane
        
int estado;
int InteractionMode;
int navegacionMode;

// Variables para el Mouse
bool middleMouseDown = false;
int lastMouseX, lastMouseY;

//camara
int cameraDistance = 270; //3000.0f; //270;
GLfloat posX = 0;
GLfloat posY = 0;
GLfloat posZ = 0.0f; //0;	//3500.0f
GLfloat rotX = 113.5;
GLfloat rotY = 20.0;
GLfloat PivotX = 0;
GLfloat PivotY = 0;
GLfloat PivotZ = 0;
int CameraActive;
int view = MaterialPreview;
bool CameraToView = false;

GLfloat LastRotX = 0;
GLfloat LastRotY = 0;	
GLfloat LastPivotX = 0;
GLfloat LastPivotY = 0;
GLfloat LastPivotZ = 0;

int SelectActivo = 0;
int SelectCount = 0;

bool showOverlays = true;
bool ShowUi = true;
bool showFloor = true;
bool showYaxis = true;
bool showXaxis = true;
bool ShowRelantionshipsLines = true;

//animacion
int StartFrame = 1;
int EndFrame = 250;
int CurrentFrame = 1;
bool redibujar = true; //solo redibuja si este valor esta en true
bool showOrigins = true;
bool show3DCursor = true;
GLfloat Cursor3DposX = 0.0f;
GLfloat Cursor3DposZ = 0.0f;
GLfloat Cursor3DposY = 0.0f;

//listados
//RArray<Object> Objects;
std::vector<Object> Objects;
//RArray<Light> Lights;
std::vector<Light> Lights;
//RArray<Mesh> Meshes;
std::vector<Mesh> Meshes;
//RArray<TInt> Collection;
std::vector<int> Collection;
//RArray<Material> Materials;
std::vector<Material> Materials;
//RArray<Modifier> Modifiers;
std::vector<Modifier> Modifiers;
//RArray<SaveState> estadoObjetos;
std::vector<SaveState> estadoObjetos;
//RArray<Constraint> Constraints;
std::vector<Constraint> Constraints;

//RArray<TTexture> Textures;
std::vector<Texture> Textures;
int NumTexturasWhisk3D = 0;

//array de colores
GLfloat ListaColores[11][4] = {
	{ 0.22, 0.28, 0.25, 1.0  },   //fondo
	{ 1.0, 1.0, 1.0, 1.0     },   //blanco
	{ 0.91, 0.50, 0.98, 1.0  },   //acento (violeta) 
	{ 0.48, 0.30, 1.0, 1.0  },   //acento oscuro	
	{ 0.0, 0.0, 0.0, 1.0     },   //negro
	{ 0.12, 0.12, 0.12, 1.0  },   //gris
	{ 0.94, 0.59, 0.17, 0.25f},   //naranja transparente
	{ 0.22, 0.22, 0.22, 1.0  },    //cabezera de la barra de herramientas
	{ 0.0, 0.0, 0.0, 0.25f     },   //negroTransparente
	{ 0.757, 0.757, 0.757, 1.0     },   //azul de los iconos seleccionados
};

GLfloat LineaPiso[4]  =      { 0.22, 0.22, 0.22, 1.0 };
GLfloat LineaPisoRoja[4]  =  { 0.56, 0.23, 0.28, 1.0 };
GLfloat LineaPisoVerde[4]  = { 0.38, 0.53, 0.15, 1.0 };

//ejes transformacion
GLfloat ColorTransformX[4]  = { 0.88, 0.48, 0.54, 1.0 };
GLfloat ColorTransformY[4]  = { 0.65, 0.81, 0.38, 1.0 };
GLfloat ColorTransformZ[4]  = { 0.46, 0.67, 0.89, 1.0 };