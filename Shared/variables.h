enum { Constant, Linear, EaseInOut, EaseIn, EaseOut };

enum { AnimPosition, AnimRotation, AnimScale };

enum{
	top,
	front,
	right,
    cameraView
};

enum{
	ObjectMode
};

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

typedef enum { X, Y, Z, XYZ, ViewAxis } Axis;

int axisSelect = X;

SDL_Window* window = nullptr;  // definición real

//NUEVO! es para poder usar OpenGl o Vulkan
SDL_GLContext glContext = nullptr;

// Para Vulkan
VkInstance vkInstance = VK_NULL_HANDLE;
VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
bool usingVulkan = false;

//luz
//GLfloat light_pos[] = { -5000.0f, 5000.0f, 5000.0f, 1.0f };
//GLfloat light_diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
//GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };

GLfloat lightDiffuseLamp[4]   = { 0.9, 0.9, 0.9, 1.0 };
GLfloat lightSpecularLamp[4]   = { 1.0, 1.0, 1.0, 1.0 };

GLfloat sunLightPosition[4]  = {-50, 500, 500, 0 }; // y, z, x, si es direccional o puntual

//para las luces puntuales
GLfloat positionPuntualLight[4] = {0, 0, 0, 1};

GLshort TransformPivotPoint[3] = {0,0,0};
GLfloat TransformPivotPointFloat[3] = {0.0f,0.0f,0.0f};

int winW = 800; 
int winH = 600;
float fovDeg = 45.0f;
float aspect = (float)winW / (float)winH;
float nearClip = 10.0f;
float farClip = 2000.0f;

int nextLightId = GL_LIGHT1;

float angle = 55.0f;
bool ViewFromCameraActive = false;
        
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
bool PlayAnimation = false;
int StartFrame = 1;
int EndFrame = 250;
int CurrentFrame = 1;
bool showOrigins = true;
bool show3DCursor = true;
GLfloat Cursor3DposX = 0.0f;
GLfloat Cursor3DposZ = 0.0f;
GLfloat Cursor3DposY = 0.0f;

//mouse
GLshort mouseX = 0;
GLshort mouseY = 0;
bool mouseVisible = false;

int ShiftCount = 0;
int valorRotacion = 0;

//listados
//RArray<SaveState> estadoObjetos;
std::vector<SaveState> estadoObjetos;
//RArray<AnimationObject> AnimationObjects;
std::vector<AnimationObject> AnimationObjects;
//RArray<ShapeKeyAnimation> ShapeKeyAnimations;
std::vector<ShapeKeyAnimation> ShapeKeyAnimations;

int NumTexturasWhisk3D = 0;