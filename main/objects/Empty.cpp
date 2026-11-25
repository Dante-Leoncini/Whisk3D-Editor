#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "Empty.h"
#include "objects/RenderColors.h" // gRenderColors / RC_selActive (color de seleccion, igual que el resto)
#ifdef W3D_SYMBIAN
#include <GLES/gl.h>
#else
#include <GL/gl.h>
#endif
#include <math.h>
#include <vector>

extern bool g_showEmpty; // toggle del submenu "Objects" (overlays)

bool Empty::DrawsSomething() { return g_showEmpty; }

// ---------------------------------------------------------------------------------------------------------------
//  El DIBUJO (lineas sueltas, de a pares). Se arma en el espacio de la PANTALLA (Z arriba, como Propiedades): el
//  circulo y el cuadrado quedan acostados en el piso. Despues se gira (rotDrawing), se escala (drawingSize) y se pasa
//  al interno (Y arriba). Solo la vista: el objeto no cambia.
// ---------------------------------------------------------------------------------------------------------------
static void Line(std::vector<GLfloat>& v, float ax, float ay, float az, float bx, float by, float bz) {
    v.push_back(ax); v.push_back(ay); v.push_back(az);
    v.push_back(bx); v.push_back(by); v.push_back(bz);
}
// CUANTAS LINEAS tiene un circulo. En una pantalla chica (el N95, la PSP, la 3DS, la Dreamcast) no se ve la diferencia
// y son menos vertices para dibujar: la mitad. La cruz, el cuadrado y el cubo ya son lo minimo (pedido del duenio).
#if defined(W3D_SYMBIAN) || defined(W3D_PSP) || defined(W3D_3DS) || defined(W3D_DREAMCAST)
static const int kSegCircle = 16, kSegSphere = 12;   // (la esfera son tres circulos)
#else
static const int kSegCircle = 32, kSegSphere = 24;
#endif
// un circulo de radio 0.5 en el plano de los ejes a y b (0 = X, 1 = Y, 2 = Z de la pantalla), con N lineas
static void Circle(std::vector<GLfloat>& v, int a, int b, int N) {
    for (int i = 0; i < N; i++) {
        const float t0 = 6.2831853f * (float)i / (float)N, t1 = 6.2831853f * (float)(i + 1) / (float)N;
        float p0[3] = { 0, 0, 0 }, p1[3] = { 0, 0, 0 };
        p0[a] = 0.5f * cosf(t0); p0[b] = 0.5f * sinf(t0);
        p1[a] = 0.5f * cosf(t1); p1[b] = 0.5f * sinf(t1);
        Line(v, p0[0], p0[1], p0[2], p1[0], p1[1], p1[2]);
    }
}
static void BuildShape(std::vector<GLfloat>& v, int shape) {
    v.clear();
    if (shape == EmptyCross) {
        Line(v, -0.5f, 0, 0, 0.5f, 0, 0); Line(v, 0, -0.5f, 0, 0, 0.5f, 0); Line(v, 0, 0, -0.5f, 0, 0, 0.5f);
    } else if (shape == EmptySquare) {   // acostado en el piso
        Line(v, -0.5f, -0.5f, 0, 0.5f, -0.5f, 0); Line(v, 0.5f, -0.5f, 0, 0.5f, 0.5f, 0);
        Line(v, 0.5f, 0.5f, 0, -0.5f, 0.5f, 0);   Line(v, -0.5f, 0.5f, 0, -0.5f, -0.5f, 0);
    } else if (shape == EmptyCube) {       // las 12 aristas
        const float h = 0.5f;
        for (int i = 0; i < 4; i++) {
            const float ax = (i == 0 || i == 3) ? -h : h, ay = (i < 2) ? -h : h;
            const float bx = (i == 0 || i == 1) ? h : -h, by = (i == 0 || i == 3) ? -h : h;
            Line(v, ax, ay, -h, bx, by, -h);   // abajo
            Line(v, ax, ay, h, bx, by, h);     // arriba
            Line(v, ax, ay, -h, ax, ay, h);    // los costados
        }
    } else if (shape == EmptySphere) {     // tres circulos, uno por plano
        Circle(v, 0, 1, kSegSphere); Circle(v, 0, 2, kSegSphere); Circle(v, 1, 2, kSegSphere);
    } else {                               // circulo, acostado en el piso
        Circle(v, 0, 1, kSegCircle);
    }
}

void Empty::RenderObject() {
    if (!g_showEmpty) return; // oculto por el toggle "Empty" del menu de overlays
    static std::vector<GLfloat> v;
    BuildShape(v, shape);
    // girar (X, despues Y, despues Z, en grados, en la pantalla), escalar y pasar al interno (Y arriba)
    const float k = 0.0174532925f;
    const float cx = cosf(rotDrawing.x * k), sx = sinf(rotDrawing.x * k);
    const float cy = cosf(rotDrawing.y * k), sy = sinf(rotDrawing.y * k);
    const float cz = cosf(rotDrawing.z * k), sz = sinf(rotDrawing.z * k);
    const float t = (drawingSize > 0.0f) ? drawingSize : 1.0f;
    for (size_t i = 0; i + 2 < v.size(); i += 3) {
        float x = v[i], y = v[i + 1], z = v[i + 2], q;
        q = y * cx - z * sx; z = y * sx + z * cx; y = q;      // X
        q = x * cy + z * sy; z = -x * sy + z * cy; x = q;     // Y
        q = x * cz - y * sz; y = x * sz + y * cz; x = q;      // Z
        v[i] = x * t; v[i + 1] = z * t; v[i + 2] = y * t;     // Z arriba -> Y arriba
    }
    GLboolean lightWas = w3dEngine::IsEnabled(w3dEngine::Lighting);
    w3dEngine::Disable(w3dEngine::Lighting);
    w3dEngine::DisableArray(w3dEngine::NormalArray);
    // color de seleccion igual que el resto de los objetos: ACTIVO = verde (selActive); seleccionado no-activo =
    // verde secundario (selInactive); sin seleccionar = gris.
    if (select){
        const float* c = gRenderColors[(this == ActiveObj) ? RC_selActive : RC_selInactive];
        w3dEngine::Color4f(c[0], c[1], c[2], 1.0f);
    } else {
        w3dEngine::Color4f(0.85f, 0.85f, 0.85f, 1.0f);
    }
    w3dEngine::VertexPointer3f(0, &v[0]);
    // (las coordenadas de textura, a los MISMOS datos: con el arreglo prendido el driver las lee aunque no haya textura,
    //  y si no se fijan quedan las del que dibujo antes -el Hitbox: su arreglo de 24 vertices- y se leia de mas)
    w3dEngine::TexCoordPointer2f(12, &v[0]);
    w3dEngine::DrawLines((int)(v.size() / 3));
    w3dEngine::EnableArray(w3dEngine::NormalArray);
    if (lightWas) w3dEngine::Enable(w3dEngine::Lighting);
}
