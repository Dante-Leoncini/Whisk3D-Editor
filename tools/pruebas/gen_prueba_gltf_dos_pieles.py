#!/usr/bin/env python3
"""
Genera prueba_gltf_dos_pieles.gltf (lo usa prueba_gltf_dos_pieles.w3s): un glTF chico, autocontenido (buffer en data:
URI), con DOS personajes, cada uno con SU esqueleto, SU malla y SU animacion, como sale una escena de Blender con varios
armatures (la de Marte de Bongo Cat: el gato y tres aliens bailando):

  - "PersonajeA" (el nodo del armature, en el origen): joints "A_raiz" -> "A_punta" (1 m arriba, +Y); la malla
    "mallaA" (un quad de 1 x 2 m) pesada abajo a A_raiz y arriba a A_punta. Animacion "BailaA": A_raiz se mueve de
    (0, 0, 0) a (0, 0, 1) en un segundo
  - "PersonajeB" (el nodo del armature, en x = 3): joints "B_raiz" -> "B_punta" y la malla "mallaB", igual. Animacion
    "BailaB": B_raiz gira 90 grados en Z en un segundo (la punta, de (3, 1, 0) a (2, 0, 0))

Lo que tiene que hacer el importador (main/importers/import_gltf.cpp): UN ARMATURE POR PIEL, cada malla pegada al suyo y
cada animacion como clip del armature cuyos huesos mueve. Antes solo leia la primera piel: el segundo personaje quedaba
sin esqueleto (en la pose de reposo) y su malla se pegaba al primero.

Uso: python3 gen_prueba_gltf_dos_pieles.py   (escribe al lado de este script)
"""
import base64
import json
import math
import os
import struct

AQUI = os.path.dirname(os.path.abspath(__file__))


def main():
    buf = b''
    vistas, accs = [], []

    def agregar(datos, tipo_comp, cuenta, tipo, minmax=None):
        nonlocal buf
        while len(buf) % 4:
            buf += b'\x00'
        vistas.append({'buffer': 0, 'byteOffset': len(buf), 'byteLength': len(datos)})
        buf += datos
        a = {'bufferView': len(vistas) - 1, 'componentType': tipo_comp, 'count': cuenta, 'type': tipo}
        if minmax:
            a['min'], a['max'] = minmax
        accs.append(a)
        return len(accs) - 1

    nodes, meshes, skins, anims = [], [], [], []
    for k, (letra, x0) in enumerate((('A', 0.0), ('B', 3.0))):
        # el quad en coordenadas de MUNDO (la piel va en el origen; la ubica el esqueleto)
        pos = [(x0 - 0.5, 0, 0), (x0 + 0.5, 0, 0), (x0 + 0.5, 2, 0), (x0 - 0.5, 2, 0)]
        a_pos = agregar(b''.join(struct.pack('<3f', *p) for p in pos), 5126, 4, 'VEC3',
                        ([x0 - 0.5, 0, 0], [x0 + 0.5, 2, 0]))
        a_jnt = agregar(b''.join(struct.pack('<4B', j, 0, 0, 0) for j in (0, 0, 1, 1)), 5121, 4, 'VEC4')
        a_wgt = agregar(b''.join(struct.pack('<4f', 1, 0, 0, 0) for _ in range(4)), 5126, 4, 'VEC4')
        ibm = [[1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -x0, 0, 0, 1],      # raiz: inv(T(x0, 0, 0))
               [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -x0, -1, 0, 1]]     # punta: inv(T(x0, 1, 0))
        a_ibm = agregar(b''.join(struct.pack('<16f', *m) for m in ibm), 5126, 2, 'MAT4')
        a_idx = agregar(struct.pack('<6H', 0, 1, 2, 0, 2, 3), 5123, 6, 'SCALAR')
        base = len(nodes)
        # nodos: malla, armature, raiz, punta
        nodes.append({'name': 'malla' + letra, 'mesh': k, 'skin': k})
        nodes.append({'name': 'Personaje' + letra, 'translation': [x0, 0, 0], 'children': [base + 2]})
        nodes.append({'name': letra + '_raiz', 'children': [base + 3]})
        nodes.append({'name': letra + '_punta', 'translation': [0, 1, 0]})
        meshes.append({'name': 'malla' + letra, 'primitives': [{'attributes': {'POSITION': a_pos, 'JOINTS_0': a_jnt,
                                                                              'WEIGHTS_0': a_wgt}, 'indices': a_idx}]})
        skins.append({'name': 'Personaje' + letra, 'joints': [base + 2, base + 3], 'inverseBindMatrices': a_ibm})
        # la animacion: dos keys (0 s y 1 s) sobre la raiz
        a_t = agregar(struct.pack('<2f', 0.0, 1.0), 5126, 2, 'SCALAR', ([0.0], [1.0]))
        if letra == 'A':
            a_v = agregar(struct.pack('<6f', 0, 0, 0, 0, 0, 1), 5126, 2, 'VEC3')
            path = 'translation'
        else:
            s = math.sqrt(0.5)
            a_v = agregar(struct.pack('<8f', 0, 0, 0, 1, 0, 0, s, s), 5126, 2, 'VEC4')
            path = 'rotation'
        anims.append({'name': 'Baila' + letra, 'extras': {'frameRate': 24},
                      'samplers': [{'input': a_t, 'output': a_v, 'interpolation': 'LINEAR'}],
                      'channels': [{'sampler': 0, 'target': {'node': base + 2, 'path': path}}]})
    g = {
        'asset': {'version': '2.0', 'generator': 'gen_prueba_gltf_dos_pieles.py'},
        'scene': 0, 'scenes': [{'nodes': [0, 1, 4, 5]}],
        'nodes': nodes, 'meshes': meshes, 'skins': skins, 'animations': anims,
        'accessors': accs, 'bufferViews': vistas,
        'buffers': [{'byteLength': len(buf), 'uri': 'data:application/octet-stream;base64,' + base64.b64encode(buf).decode()}],
    }
    out = os.path.join(AQUI, 'prueba_gltf_dos_pieles.gltf')
    with open(out, 'w') as f:
        json.dump(g, f, indent=1)
    print('escrito', out)


if __name__ == '__main__':
    main()
