#ifndef W3D_MODE_MEMORY_H
#define W3D_MODE_MEMORY_H
// ============================================================================
//  LA MEMORIA DE LOS MODOS: lo que una herramienta reserva solo para trabajar (los buferes del pincel, el mapa de
//  oclusion de la pintura, los pixeles de las texturas que se pintaron) se SUELTA AL SALIR de su modo, en vez de quedar
//  tomado hasta cerrar el editor. En el Galaxy Ace y la Dreamcast eso es RAM para el proyecto.
//
//  Cada herramienta anota, una vez, que soltar y en que modos lo usa:
//      static void MiHerramientaSoltar() { std::vector<float>().swap(gMiBufer); }
//      W3D_MODE_MEMORY(W3D_MODE_BIT(WeightPaint) | W3D_MODE_BIT(VertexPaint), MiHerramientaSoltar);
//  y W3dModeMemoryTick (cada cuadro, por la misma puerta que g_editMesh: UpdateActiveEditMesh) la llama cuando el editor
//  deja TODOS esos modos. Un modo nuevo (la escultura) anota lo suyo igual, sin tocar nada mas.
//  El codigo de las herramientas sigue en el ejecutable (en la PC y en Android el sistema ya carga del disco solo las
//  paginas que se usan); lo que se carga y se descarga es su memoria de trabajo.
//
//  Los modos son los de W3dInteractionState.h (ObjectMode, EditMode, VertexPaint, WeightPaint, TexturePaint, PoseMode);
//  la pintura de textura del EDITOR UV cuenta como TexturePaint.
// ============================================================================
typedef void (*W3dModeReleaseFn)();
#define W3D_MODE_BIT(mode) (1u << (mode))
int  W3dModeMemoryRegister(unsigned modes, W3dModeReleaseFn release);   // (devuelve algo: se llama desde un static)
void W3dModeMemoryTick(unsigned activeModes);                              // los modos en uso en este cuadro
#define W3D_MODE_MEMORY_CAT2(a, b) a##b
#define W3D_MODE_MEMORY_CAT(a, b) W3D_MODE_MEMORY_CAT2(a, b)
#define W3D_MODE_MEMORY(modes, release) \
    static const int W3D_MODE_MEMORY_CAT(gModeMemory, __LINE__) = W3dModeMemoryRegister((modes), (release))
#endif
