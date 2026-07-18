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
#include "WhiskUI/theme/colores.h" // paleta del editor (ListaColores/ColorID) — compartida
#include "w3dnewscene.h"
#include "math/w3dmath.h" // w3dClamp compartido (4 OS)
#include "W3dInitUI.h" // init de UI compartido (texturas+fuente+iconos)

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
    w3dEngine::ClearColor( ListaColores[static_cast<int>(ColorID::background)][0], ListaColores[static_cast<int>(ColorID::background)][1], ListaColores[static_cast<int>(ColorID::background)][2], 1.f );
    W3dInitGraphics();

    RFs fs;
    User::LeaveIfError(fs.Connect());
    w3dLogReset();
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

	// skin.ini COMPARTIDO (misma paleta que PC: verde whisk3d). Antes de las
	// texturas: los colores no dependen de ellas pero conviene tenerlos ya.
	{
		char rutaSkin[260];
		TInt len = fullFilePath.Length() > 240 ? 240 : fullFilePath.Length();
		for (TInt i = 0; i < len; i++) { rutaSkin[i] = (char)fullFilePath[i]; }
		const char* fin = "skin.ini";
		for (TInt k = 0; fin[k] && len < 250; k++) { rutaSkin[len++] = fin[k]; }
		rutaSkin[len] = 0;
		extern bool loadColorsW3d(const char*);
		extern bool loadEditorColorsW3d(const char*);
		extern void SincronizarRenderColores();
		if (loadColorsW3d(rutaSkin)) {
			loadEditorColorsW3d(rutaSkin);
			SincronizarRenderColores();
			w3dLog("skin.ini cargado: paleta de Whisk3D activa");
		} else {
			w3dLog("skin.ini NO se pudo abrir (paleta default)");
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
