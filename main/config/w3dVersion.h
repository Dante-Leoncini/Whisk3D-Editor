#ifndef W3D_VERSION_H
#define W3D_VERSION_H

// Version de Whisk3D en formato "YY.MM.DD" = fecha de compilacion (mismo criterio que el APK / .deb / .exe). La
// inyecta el build system (define W3D_VERSION); si falta, se parsea de __DATE__. Se usa en el titulo de ventana y
// en el header del .obj exportado, asi una captura de pantalla dice de que version es.
const char* W3dVersion();

#endif // W3D_VERSION_H
