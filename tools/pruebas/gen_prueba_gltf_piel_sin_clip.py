#!/usr/bin/env python3
"""
Genera prueba_gltf_piel_sin_clip.gltf (lo usa prueba_gltf_piel_sin_clip.w3s): un glTF chico, autocontenido (buffer
en data: URI), con un esqueleto y una malla con piel y SIN ninguna animacion, como sale un personaje de Blender en
reposo (o con el rig convertido aparte):

  - "armadura" (nodo vacio, el objeto armature de Blender) con dos cadenas de joints:
      "raiz" en el origen -> su hijo "punta" 1 m arriba (+Y)
      "suelto" en x = 2, SIN padre ni hijos (como los huesos de deformacion de un rig de Blender, que no cuelgan de
      nadie y siguen a otro hueso con Copy Transforms), girado 90 grados sobre Z: su +Y apunta a -X
  - "malla": un quad de 1 x 2 m pesado abajo a "raiz" y arriba a "punta"

Lo que tiene que hacer el importador (main/importers/import_gltf.cpp):
  - pegar la malla a su esqueleto aunque no haya clips (antes quedaba suelta: posar o un constraint no la movian)
  - escribir la PUNTA de cada hueso: por su +Y, con el largo hasta su primer hijo ("raiz": 1 m), el del padre en una
    hoja ("punta": 1 m) y una decima del esqueleto en una raiz suelta ("suelto": 0.1 * |(2,1,0)| = 0.2236 m, hacia -X).
    Antes la punta era la cabeza y el motor la dibujaba 5 unidades hacia +Z: rayas de 5 m cruzando la escena.

Uso: python3 gen_prueba_gltf_piel_sin_clip.py   (escribe al lado de este script)
"""
import base64
import json
import math
import os
import struct

AQUI = os.path.dirname(os.path.abspath(__file__))


def main():
    s = math.sqrt(0.5)
    # (x, y, z) de los 4 vertices, sus joints (indices en skin.joints) y sus pesos
    pos = [(-0.5, 0, 0), (0.5, 0, 0), (0.5, 2, 0), (-0.5, 2, 0)]
    joints = [(0, 0, 0, 0), (0, 0, 0, 0), (1, 0, 0, 0), (1, 0, 0, 0)]
    pesos = [(1, 0, 0, 0)] * 4
    idx = [0, 1, 2, 0, 2, 3]
    # inverseBindMatrices (por COLUMNA): la inversa del mundo de reposo de cada joint
    ibm = [
        [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1],               # raiz: identidad
        [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -1, 0, 1],              # punta: T(0,-1,0)
        [0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 2, 0, 1],              # suelto: inv(T(2,0,0) * Rz(90))
    ]
    buf = b''.join(struct.pack('<3f', *p) for p in pos)                          # 0: 48 B
    buf += b''.join(struct.pack('<4B', *j) for j in joints)                      # 48: 16 B
    buf += b''.join(struct.pack('<4f', *w) for w in pesos)                       # 64: 64 B
    buf += b''.join(struct.pack('<16f', *m) for m in ibm)                        # 128: 192 B
    buf += struct.pack('<6H', *idx)                                              # 320: 12 B
    buf += b'\x00' * ((4 - len(buf) % 4) % 4)
    g = {
        'asset': {'version': '2.0', 'generator': 'gen_prueba_gltf_piel_sin_clip.py'},
        'scene': 0,
        'scenes': [{'nodes': [0, 1]}],
        'nodes': [
            {'name': 'malla', 'mesh': 0, 'skin': 0},
            {'name': 'armadura', 'children': [2, 4]},
            {'name': 'raiz', 'children': [3]},
            {'name': 'punta', 'translation': [0, 1, 0]},
            {'name': 'suelto', 'translation': [2, 0, 0], 'rotation': [0, 0, s, s]},
        ],
        'skins': [{'name': 'armadura', 'joints': [2, 3, 4], 'inverseBindMatrices': 3}],
        'meshes': [{'name': 'malla', 'primitives': [{'attributes': {'POSITION': 0, 'JOINTS_0': 1, 'WEIGHTS_0': 2},
                                                     'indices': 4}]}],
        'accessors': [
            {'bufferView': 0, 'componentType': 5126, 'count': 4, 'type': 'VEC3', 'min': [-0.5, 0, 0], 'max': [0.5, 2, 0]},
            {'bufferView': 1, 'componentType': 5121, 'count': 4, 'type': 'VEC4'},
            {'bufferView': 2, 'componentType': 5126, 'count': 4, 'type': 'VEC4'},
            {'bufferView': 3, 'componentType': 5126, 'count': 3, 'type': 'MAT4'},
            {'bufferView': 4, 'componentType': 5123, 'count': 6, 'type': 'SCALAR'},
        ],
        'bufferViews': [
            {'buffer': 0, 'byteOffset': 0, 'byteLength': 48},
            {'buffer': 0, 'byteOffset': 48, 'byteLength': 16},
            {'buffer': 0, 'byteOffset': 64, 'byteLength': 64},
            {'buffer': 0, 'byteOffset': 128, 'byteLength': 192},
            {'buffer': 0, 'byteOffset': 320, 'byteLength': 12},
        ],
        'buffers': [{'byteLength': len(buf), 'uri': 'data:application/octet-stream;base64,' + base64.b64encode(buf).decode()}],
    }
    out = os.path.join(AQUI, 'prueba_gltf_piel_sin_clip.gltf')
    with open(out, 'w') as f:
        json.dump(g, f, indent=1)
    print('escrito', out)


if __name__ == '__main__':
    main()
