#!/usr/bin/env python3
"""
Genera prueba_gltf_armature_escalado.gltf (lo usa prueba_gltf_armature_escalado.w3s): un glTF chico, autocontenido
(buffer en data: URI), con UN personaje cuyo ARMATURE esta ESCALADO, como los aliens de Marte de Bongo Cat (Mixamo: el
armature a escala 0.22) y una animacion que GIRA LA RAIZ en Y:

  - "Personaje" (el nodo del armature): escala 0.5. Joints "raiz" -> "punta" (2 m en +X de la raiz en su espacio: 1 m en
    el mundo). La malla "malla" (un quad) pesada a los dos
  - Animacion "Gira": la raiz gira 60 grados en Y en un segundo. La punta pasa de (1, 0, 0) a (0.5, 0, -0.866)

Lo que tiene que hacer el importador (main/importers/import_gltf.cpp): llevar la transformacion del nodo del armature
(el "prefijo": escala incluida) a las claves de la raiz y sacar el euler de la ROTACION sola. Antes lo sacaba de la
matriz con la escala adentro: el seno de Y salia por 0.5 (25.7 grados en vez de 60) y la punta quedaba en (0.901, 0,
-0.433). En Marte, la cadera de los aliens tenia 7.5 grados en vez de 37 y al agacharse el cuerpo quedaba torcido.

Uso: python3 gen_prueba_gltf_armature_escalado.py   (escribe al lado de este script)
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

    esc = 0.5
    # el quad en coordenadas de MUNDO: de la raiz (0, 0, 0) a la punta (1, 0, 0), medio metro de alto
    pos = [(0, 0, 0), (1, 0, 0), (1, 0.5, 0), (0, 0.5, 0)]
    a_pos = agregar(b''.join(struct.pack('<3f', *p) for p in pos), 5126, 4, 'VEC3', ([0, 0, 0], [1, 0.5, 0]))
    a_jnt = agregar(b''.join(struct.pack('<4B', j, 0, 0, 0) for j in (0, 1, 1, 0)), 5121, 4, 'VEC4')
    a_wgt = agregar(b''.join(struct.pack('<4f', 1, 0, 0, 0) for _ in range(4)), 5126, 4, 'VEC4')
    # inversas de bind: inv(S(0.5)) para la raiz; inv(S(0.5) * T(2, 0, 0)) para la punta (en el mundo, T(1, 0, 0) * S)
    ibm = [[1 / esc, 0, 0, 0, 0, 1 / esc, 0, 0, 0, 0, 1 / esc, 0, 0, 0, 0, 1],
           [1 / esc, 0, 0, 0, 0, 1 / esc, 0, 0, 0, 0, 1 / esc, 0, -2, 0, 0, 1]]
    a_ibm = agregar(b''.join(struct.pack('<16f', *m) for m in ibm), 5126, 2, 'MAT4')
    a_idx = agregar(struct.pack('<6H', 0, 1, 2, 0, 2, 3), 5123, 6, 'SCALAR')
    nodes = [{'name': 'malla', 'mesh': 0, 'skin': 0},
             {'name': 'Personaje', 'scale': [esc, esc, esc], 'children': [2]},
             {'name': 'raiz', 'children': [3]},
             {'name': 'punta', 'translation': [2, 0, 0]}]
    meshes = [{'name': 'malla', 'primitives': [{'attributes': {'POSITION': a_pos, 'JOINTS_0': a_jnt, 'WEIGHTS_0': a_wgt},
                                                'indices': a_idx}]}]
    skins = [{'name': 'Personaje', 'joints': [2, 3], 'inverseBindMatrices': a_ibm}]
    a_t = agregar(struct.pack('<2f', 0.0, 1.0), 5126, 2, 'SCALAR', ([0.0], [1.0]))
    s, c = math.sin(math.radians(30)), math.cos(math.radians(30))   # (medio angulo: 60 grados en Y)
    a_v = agregar(struct.pack('<8f', 0, 0, 0, 1, 0, s, 0, c), 5126, 2, 'VEC4')
    anims = [{'name': 'Gira', 'extras': {'frameRate': 24},
              'samplers': [{'input': a_t, 'output': a_v, 'interpolation': 'LINEAR'}],
              'channels': [{'sampler': 0, 'target': {'node': 2, 'path': 'rotation'}}]}]
    g = {
        'asset': {'version': '2.0', 'generator': 'gen_prueba_gltf_armature_escalado.py'},
        'scene': 0, 'scenes': [{'nodes': [0, 1]}],
        'nodes': nodes, 'meshes': meshes, 'skins': skins, 'animations': anims,
        'accessors': accs, 'bufferViews': vistas,
        'buffers': [{'byteLength': len(buf), 'uri': 'data:application/octet-stream;base64,' + base64.b64encode(buf).decode()}],
    }
    out = os.path.join(AQUI, 'prueba_gltf_armature_escalado.gltf')
    with open(out, 'w') as f:
        json.dump(g, f, indent=1)
    print('escrito', out)


if __name__ == '__main__':
    main()
