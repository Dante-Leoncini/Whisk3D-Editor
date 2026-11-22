#ifndef W3D_MCP_H
#define W3D_MCP_H
// ============================================================================
//  El MCP de Whisk3D (Model Context Protocol): "whisk3d --mcp" abre el editor y atiende a una IA por la entrada y
//  la salida estandar. Solo en las PC (W3D_MCP): ver W3dMcp.cpp.
// ============================================================================
#ifdef W3D_MCP
bool W3dMcpStart();    // al arrancar, antes que nada imprima: toma stdin/stdout para el protocolo
bool W3dMcpActive();
bool W3dMcpHandle();    // en cada vuelta del loop: responde lo que llego. false = el cliente se fue (cerrar)
#endif

#endif
