#!/usr/bin/env python3
"""
Genera prueba_gltf_quads.gltf (lo usa prueba_gltf_quads.w3s): mallas chicas, autocontenidas (buffer en data: URI), para
probar que el import glTF REARMA los quads y ngons con extras.w3d_polygons ([lados, cuantos, ...] en el orden de los
triangulos: cada poligono de n lados consume sus n-2 triangulos seguidos). Asi exporta el convertidor de Blender de
Bongo Cat (bongo_cat/convertir_w3d) y ExportGLTF de Whisk3D.

  - "Poligonos", primitiva 0 (material A), w3d_polygons [4, 2, 3, 1, 5, 1]:
      un quad cortado en ABANICO (0-1-2, 0-2-3), un quad cortado por la OTRA diagonal (0-1-3, 1-2-3, como Blender
      cuando esa es la mas corta), un triangulo y un PENTAGONO triangulado sin abanico (1-2-3, 0-1-3, 0-3-4)
    primitiva 1 (material B), w3d_polygons [4, 1]: un quad
    -> 1 triangulo, 3 quads, 1 ngon
  - "SinExtras": un quad en dos triangulos y sin extras (un glTF de cualquier programa) -> 2 triangulos
  - "CuentaMal": w3d_polygons [4, 2] con solo 2 triangulos (no es pareja del archivo) -> 2 triangulos
  - "Degenerado": w3d_polygons [4, 1] pero sus dos triangulos (0-1-2, 0-1-3) no forman un quad (la arista 0-1 va dos
    veces en el mismo sentido) -> 2 triangulos

Uso: python3 gen_prueba_gltf_quads.py   (escribe al lado de este script)
"""
import base64
import json
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

    def primitiva(pos, tris, mat, polys=None):
        p = agregar(b''.join(struct.pack('<3f', *v) for v in pos), 5126, len(pos), 'VEC3',
                    ([min(v[k] for v in pos) for k in range(3)], [max(v[k] for v in pos) for k in range(3)]))
        n = agregar(b''.join(struct.pack('<3f', 0.0, 1.0, 0.0) for _ in pos), 5126, len(pos), 'VEC3')
        i = agregar(b''.join(struct.pack('<3H', *t) for t in tris), 5123, len(tris) * 3, 'SCALAR')
        pr = {'attributes': {'POSITION': p, 'NORMAL': n}, 'indices': i, 'material': mat}
        if polys is not None:
            pr['extras'] = {'w3d_polygons': polys}
        return pr

    # (todo en el plano XZ, normal +Y: el orden de los triangulos es antihorario visto desde arriba)
    q1 = [(0, 0, 0), (0, 0, 1), (1, 0, 1), (1, 0, 0)]                       # abanico
    q2 = [(2, 0, 0), (2, 0, 1), (3, 0, 1), (3, 0, 0)]                       # la otra diagonal
    tr = [(4, 0, 0), (4, 0, 1), (5, 0, 0)]
    pe = [(6, 0, 0), (6, 0, 1), (6.5, 0, 1.5), (7, 0, 1), (7, 0, 0)]        # pentagono
    pos0 = q1 + q2 + tr + pe
    tris0 = [(0, 1, 2), (0, 2, 3),
             (4, 5, 7), (5, 6, 7),
             (8, 9, 10),
             (12, 13, 14), (11, 12, 14), (11, 14, 15)]
    pos1 = [(0, 0, 3), (0, 0, 4), (1, 0, 4), (1, 0, 3)]
    quad = [(0, 1, 2), (0, 2, 3)]
    mallas = [
        {'name': 'Poligonos', 'primitives': [primitiva(pos0, tris0, 0, [4, 2, 3, 1, 5, 1]),
                                             primitiva(pos1, quad, 1, [4, 1])]},
        {'name': 'SinExtras', 'primitives': [primitiva(pos1, quad, 0)]},
        {'name': 'CuentaMal', 'primitives': [primitiva(pos1, quad, 0, [4, 2])]},
        {'name': 'Degenerado', 'primitives': [primitiva(pos1, [(0, 1, 2), (0, 1, 3)], 0, [4, 1])]},
    ]
    nodos = [{'name': m['name'], 'mesh': k, 'translation': [0, 0, -5.0 * k]} for k, m in enumerate(mallas)]
    doc = {
        'asset': {'version': '2.0', 'generator': 'gen_prueba_gltf_quads.py'},
        'scene': 0, 'scenes': [{'nodes': list(range(len(nodos)))}],
        'nodes': nodos, 'meshes': mallas,
        'materials': [{'name': 'A', 'pbrMetallicRoughness': {'baseColorFactor': [0.8, 0.2, 0.2, 1]}},
                      {'name': 'B', 'pbrMetallicRoughness': {'baseColorFactor': [0.2, 0.2, 0.8, 1]}}],
        'accessors': accs, 'bufferViews': vistas,
        'buffers': [{'byteLength': len(buf), 'uri': 'data:application/octet-stream;base64,' + base64.b64encode(buf).decode()}],
    }
    with open(os.path.join(AQUI, 'prueba_gltf_quads.gltf'), 'w') as f:
        json.dump(doc, f, indent=1)


if __name__ == '__main__':
    main()
