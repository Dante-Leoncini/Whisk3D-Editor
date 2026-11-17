# ============================================================================
#  proguard-rules.pro - reglas de R8 para el AAB de Google Play (bundleRelease)
# ----------------------------------------------------------------------------
#  R8 achica, optimiza y renombra el codigo Java (Play mide la "optimizacion de codigo DEX" con eso). Lo unico que
#  R8 NO VE son las llamadas desde C/C++ por JNI (GetMethodID / GetStaticMethodID con el nombre en un string): sin
#  estas reglas borra o renombra esos metodos y la app se cae al llamarlos. Por eso aca se guarda EXACTAMENTE eso y
#  nada mas (guardar paquetes enteros con -keep class org.libsdl.app.** baja el puntaje de Play).
#
#  Si se agrega una llamada nueva de C a Java (SDL nuevo, o un metodo de Whisk3DActivity que se llame desde C++),
#  hay que agregarla aca. tools/android_jni_check.py busca esas llamadas en el codigo nativo y se fija que esten en
#  el .dex del AAB con el mismo nombre y firma.
#  El APK de la pagina (assembleDebug) no pasa por R8: estas reglas solo cuentan para el release.
# ============================================================================

# Los stack traces de los crashes en Play con numero de linea (Play los desofusca con el mapping.txt del AAB)
-keepattributes SourceFile,LineNumberTable
-renamesourcefileattribute SourceFile

# --- Los metodos native de SDL: el C los registra por clase y nombre (RegisterNatives / FindClass de
#     "org/libsdl/app/..."), asi que las clases que los tienen conservan el nombre y no se borra ninguno ---
-keepclasseswithmembers,includedescriptorclasses class org.libsdl.app.** {
    native <methods>;
}

# --- SDL: lo que llama src/core/android/SDL_android.c ---
-keep,includedescriptorclasses class org.libsdl.app.SDLActivity {
    java.lang.String clipboardGetText();
    boolean clipboardHasText();
    void clipboardSetText(java.lang.String);
    int createCustomCursor(int[], int, int, int, int);
    void destroyCustomCursor(int);
    android.content.Context getContext();
    android.util.DisplayMetrics getDisplayDPI();
    boolean getManifestEnvironmentVariables();
    android.view.Surface getNativeSurface();
    void initTouch();
    boolean isAndroidTV();
    boolean isChromebook();
    boolean isDeXMode();
    boolean isScreenKeyboardShown();
    boolean isTablet();
    void manualBackButton();
    int messageboxShowMessageBox(int, java.lang.String, java.lang.String, int[], int[], java.lang.String[], int[]);
    void minimizeWindow();
    int openURL(java.lang.String);
    void requestPermission(java.lang.String, int);
    boolean sendMessage(int, int);
    boolean setActivityTitle(java.lang.String);
    boolean setCustomCursor(int);
    void setOrientation(int, int, boolean, java.lang.String);
    boolean setRelativeMouseEnabled(boolean);
    boolean setSystemCursor(int);
    void setWindowStyle(boolean);
    boolean shouldMinimizeOnFocusLoss();
    boolean showTextInput(int, int, int, int);
    int showToast(java.lang.String, int, int, int, int);
    boolean supportsRelativeMouse();
}
-keep,includedescriptorclasses class org.libsdl.app.SDLAudioManager {
    void audioClose();
    int[] audioOpen(int, int, int, int, int);
    void audioSetThreadPriority(boolean, int);
    void audioWriteByteBuffer(byte[]);
    void audioWriteFloatBuffer(float[]);
    void audioWriteShortBuffer(short[]);
    void captureClose();
    int[] captureOpen(int, int, int, int, int);
    int captureReadByteBuffer(byte[], boolean);
    int captureReadFloatBuffer(float[], boolean);
    int captureReadShortBuffer(short[], boolean);
    int[] getAudioInputDevices();
    int[] getAudioOutputDevices();
}
-keep,includedescriptorclasses class org.libsdl.app.SDLControllerManager {
    void hapticRun(int, float, int);
    void hapticStop(int);
    void pollHapticDevices();
    void pollInputDevices();
}
# --- SDL: lo que llama src/hidapi/android/hid.cpp (joysticks por USB / Bluetooth) ---
-keep,includedescriptorclasses class org.libsdl.app.HIDDeviceManager {
    boolean initialize(boolean, boolean);
    boolean openDevice(int);
    int sendOutputReport(int, byte[]);
    int sendFeatureReport(int, byte[]);
    boolean getFeatureReport(int, byte[]);
    void closeDevice(int);
}

# --- Whisk3D: lo que el C++ llama sobre la actividad (GetMethodID sobre la clase de la actividad) ---
#     videoAbrir/videoCuadro/videoCerrar/audioAac: main/ui/ViewPorts/Properties.cpp (exportar video)
#     tieneNotch: main/app/W3dTeclado.cpp   -   vibrar: libs/Whisk3DCore/input/W3dVibrar.cpp
-keep,includedescriptorclasses class com.whisk3d.Whisk3DActivity {
    boolean videoAbrir(java.lang.String, int, int, int, int);
    boolean videoCuadro(byte[], int, int);
    boolean videoCerrar(java.lang.String, int);
    boolean audioAac(java.lang.String, java.lang.String, int);
    boolean tieneNotch();
    void vibrar(int);
}
# (y el aviso del teclado que el C++ exporta como Java_com_whisk3d_Whisk3DActivity_nativeTecladoTapado: W3dTeclado.cpp)
-keepclasseswithmembernames,includedescriptorclasses class com.whisk3d.Whisk3DActivity {
    native <methods>;
}
