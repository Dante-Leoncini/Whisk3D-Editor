// INCLUDES
#include <e32std.h>
#include <e32math.h>
#include <e32base.h>
#include "fscompat.h" // FsCloseCompat: cerrar RFs sin importar efsrv@390 (Symbian^3)
#include "w3dlog.h" // log de diagnostico E:\whisk3d.log (modo dev)
#include "w3dlayout.h" // layout compartido (Fase 1 unificacion)
#include "w3dmouse.h" // cursor virtual de Symbian (el telefono no tiene mouse)
#include "w3dTexture.h" // motor: cargador de texturas universal
#include "w3dGraphics.h" // motor: abstraccion del estado de graficos
#include "WhiskUI/theme/colors.h" // paleta del editor (ColorList/ColorID) — compartida
#include "w3dnewscene.h"
#include "math/w3dmath.h" // w3dClamp compartido (4 OS)
#include "W3dInitUI.h" // init de UI compartido (texturas+fuente+iconos)
#include "w3dFilesystem.h" // FileExists: detectar el proyecto bundleado (modo juego)
#include <string>

#include "Whisk3D.h"


// -----------------------------------------------------------------------------
// CWhisk3D::CWhisk3D
// C++ default constructor can NOT contain any code, that
// might leave.
// -----------------------------------------------------------------------------
//
CWhisk3D::CWhisk3D( TUint aWidth, TUint aHeight, CWhisk3DInput* aInputHandler ){
    // tamano de pantalla en los globales COMPARTIDOS (variables.cpp; los usan
    // tambien el layout y el cursor). El N95 ya no guarda su copia.
    winW = (int)aWidth;
    winH = (int)aHeight;
    iInputHandler = aInputHandler;
    // teclas modificadoras del telefono (las prende/apaga AppUi, las lee
    // w3dinput). CWhisk3D no es CBase: hay que inicializarlas a mano.
    iShiftPressed = EFalse;
    iAltPressed = EFalse;
    iCtrlPressed = EFalse;
}

// -----------------------------------------------------------------------------
// CWhisk3D::NewL
// El constructor no deja (solo setea miembros), asi que no hace falta el patron
// 2-fase con ConstructL/CleanupStack.
// -----------------------------------------------------------------------------
CWhisk3D* CWhisk3D::NewL( TUint aWidth, TUint aHeight, CWhisk3DInput* aInputHandler ){
    return new (ELeave) CWhisk3D( aWidth, aHeight, aInputHandler );
}

// Destructor.
CWhisk3D::~CWhisk3D(){}

// -----------------------------------------------------------------------------
// CWhisk3D::AppInit
// Initialize OpenGL ES, set the vertex and color arrays and pointers,
// and select the shading mode.
// -----------------------------------------------------------------------------
//

void CWhisk3D::AppInit( void ){
    // Initialize viewport and projection.
	SetScreenSize( winW, winH );

    // estado de graficos inicial: el MISMO inicializador universal que usa PC
    w3dEngine::ClearColor( ColorList[static_cast<int>(ColorID::background)][0], ColorList[static_cast<int>(ColorID::background)][1], ColorList[static_cast<int>(ColorID::background)][2], 1.f );
    W3dInitGraphics();

    RFs fs;
    User::LeaveIfError(fs.Connect());
    // (el log arranca en CWhisk3DContainer::ConstructL, antes de EGL: asi queda el init de GL y de los shaders)
    w3dLog("AppInit: inicio");

    // carpetas de SALIDA del editor: render PNG en E:\whisk3d\render, export OBJ en E:\whisk3d\models.
    // Se crean al arrancar si no estan (MkDirAll ignora KErrAlreadyExists). Asi las salidas quedan
    // prolijas y el usuario sabe donde buscarlas.
    fs.MkDirAll(_L("E:\\whisk3d\\render\\"));
    fs.MkDirAll(_L("E:\\whisk3d\\models\\"));

    TFileName privateDir;
	
    // unidad donde corre la app (c: / e:): de la ruta del propio exe. No
    // dependemos de la jerarquia de app-UI de Symbian solo para sacar la unidad.
    TFileName exePath = RProcess().FileName();
    TParsePtrC Parse( exePath );
    TFileName RootDirectory = Parse.DriveAndPath().Left( 2 );

    User::LeaveIfError(fs.PrivatePath(privateDir));
    FsCloseCompat(fs);

	
    // ruta de la carpeta privada (donde estan los assets de UI)
	TFileName fullFilePath = RootDirectory;
    fullFilePath.Append(privateDir);

	// skin.start COMPARTIDO (misma paleta que PC: verde whisk3d). Antes de las
	// texturas: los colores no dependen de ellas pero conviene tenerlos ya.
	{
		char skinPath[260];
		TInt len = fullFilePath.Length() > 240 ? 240 : fullFilePath.Length();
		for (TInt i = 0; i < len; i++) { skinPath[i] = (char)fullFilePath[i]; }
		const char* end = "skin.ini";
		for (TInt k = 0; end[k] && len < 250; k++) { skinPath[len++] = end[k]; }
		skinPath[len] = 0;
		extern bool loadColorsW3d(const char*);
		extern bool loadEditorColorsW3d(const char*);
		extern void SyncRenderColors();
		if (loadColorsW3d(skinPath)) {
			loadEditorColorsW3d(skinPath);
			SyncRenderColors();
			w3dLog("skin.ini cargado: paleta de Whisk3D activa");
		} else {
			w3dLog("skin.ini NO se pudo abrir (paleta default)");
		}
	}

	// ---- MODO JUEGO ----------------------------------------------------------
	// Si el .sisx trae un proyecto bundleado en <privado>\game\game.w3d, se AUTO-ABRE
	// en el arranque (encolando en g_projOpenPending, que drena el DrawCallBack en el
	// primer frame -> OpenProjectNow). Un .sisx de EDITOR no trae ese archivo, asi que
	// arranca como editor normal. Mismo binario, dos comportamientos segun el bundle.
	{
		char gamePath[260];
		TInt lj = fullFilePath.Length() > 230 ? 230 : fullFilePath.Length();
		for (TInt i = 0; i < lj; i++) { gamePath[i] = (char)fullFilePath[i]; }
		const char* endJ = "game\\game.w3d";
		for (TInt k = 0; endJ[k] && lj < 250; k++) { gamePath[lj++] = endJ[k]; }
		gamePath[lj] = 0;
		if (w3dFileSystem::FileExists(gamePath)) {
			extern std::string g_projOpenPending;
			extern bool g_gameMode;
			g_projOpenPending = gamePath;
			g_gameMode = true;   // -> full-screen del 3D al abrir (el juego se ve solo)
			w3dLog("modo juego: proyecto bundleado detectado, auto-abriendo full-screen");
		}
	}

	// --- UI COMPARTIDA: texturas + fuente + iconos (mismo init que PC) ---
	// El shell ya no carga texturas a mano: W3dInitUI las sube al vector
	// 'Textures' compartido y arma fuente/iconos/9-patch desde font.png.
	std::string skinDir;
	for (TInt i = 0; i < fullFilePath.Length(); i++) {
	    skinDir.push_back((char)fullFilePath[i]); // carpeta privada (con la barra final)
	}
	W3dInitUI(skinDir);

	// cursor del mouse: UNICO extra de Symbian (point-sprite 32x32 de
	// mouse.png; PC usa el cursor del sistema)
	{
	    std::string mp = skinDir + "mouse.png";
	    unsigned char* rgba = NULL;
	    int mw = 0, mh = 0;
	    if (w3dEngine::DecodeImage(mp.c_str(), &rgba, &mw, &mh)) {
	        W3dMouseBuildTex(rgba, mw, mh);
	        w3dEngine::FreeImage(rgba);
	    }
	}

	w3dLog("AppInit: UI cargada (init compartido) + cursor");
}

// -----------------------------------------------------------------------------
// CWhisk3D::SetScreenSize
// Reacts to the dynamic screen size change during execution of this program.
// -----------------------------------------------------------------------------
//
void CWhisk3D::SetScreenSize( TUint aWidth, TUint aHeight ){
    winW = (int)aWidth;
    winH = (int)aHeight;

    // Reinitialize viewport (la proyeccion la setea cada Viewport3D por frame)
    w3dEngine::Viewport( 0, 0, winW, winH );
}
