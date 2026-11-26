#!/usr/bin/env python3
"""
Genera prueba_gltf_jerarquia.gltf (lo usa prueba_gltf_jerarquia.w3s): un glTF chico, autocontenido (buffer e imagen
en data: URI), con lo que el importador antes perdia:

  - jerarquia con NOMBRES: "Padre" (nodo vacio) con hijos "Hijo" (malla) y "Camara" (camara, yfov 0.5 rad)
  - "Sol" (KHR_lights_punctual directional, 683 lux) y "Foco" (spot de 683*pi cd, cono 0.5 / 0.25 rad)
  - "Girador" (malla) animado en ROTACION en dos ejes con STEP: en t=1 s vale Rx(30)*Ry(40) (el euler XYZ de los
    objetos de Whisk3D); antes entraba con el orden FBX y quedaba torcido
  - dos animaciones (la de "Girador" y la de "Padre", que se mueve de x=1 a x=3) que no tocan el mismo nodo: tienen
    que caer en UNA sola escena, no en una por animacion
  - material "Mat" con color base LINEAL 0.21404114 (= 0.5 en sRGB), metalico 0.25, rugoso 0.75, emision roja y una
    textura base embebida

Uso: python3 gen_prueba_gltf_jerarquia.py   (escribe al lado de este script)
"""
import base64
import json
import math
import os
import struct
import zlib

AQUI = os.path.dirname(os.path.abspath(__file__))


def png_2x2():
    filas = b''.join(b'\x00' + bytes([255, 0, 0, 0, 255, 0]) for _ in range(2))   # 2x2 RGB
    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, 8, 2, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(filas)) + chunk(b'IEND', b''))


def quat_xyz(x, y, z):
    """cuaternion (x, y, z, w) de R = Rx(x) * Ry(y) * Rz(z) (grados): el euler de los objetos de Whisk3D"""
    def q_eje(ax, ang):
        h = math.radians(ang) / 2
        s = math.sin(h)
        return (ax[0] * s, ax[1] * s, ax[2] * s, math.cos(h))

    def mul(a, b):
        ax, ay, az, aw = a
        bx, by, bz, bw = b
        return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
                aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)
    return mul(mul(q_eje((1, 0, 0), x), q_eje((0, 1, 0), y)), q_eje((0, 0, 1), z))


def main():
    pos = [0, 0, 0, 1, 0, 0, 0, 1, 0]
    nrm = [0, 0, 1] * 3
    uv = [0, 0, 1, 0, 0, 1]
    idx = [0, 1, 2]
    t_gira = [0.0, 1.0]
    r_gira = list(quat_xyz(0, 0, 0)) + list(quat_xyz(30, 40, 0))
    t_padre = [0.0, 1.0]
    v_padre = [1, 0, 0, 3, 0, 0]
    partes = [struct.pack('<9f', *pos), struct.pack('<9f', *nrm), struct.pack('<6f', *uv), struct.pack('<3H', *idx) + b'\0\0',
              struct.pack('<2f', *t_gira), struct.pack('<8f', *r_gira), struct.pack('<2f', *t_padre),
              struct.pack('<6f', *v_padre)]
    vistas, off = [], 0
    for p in partes:
        vistas.append({'buffer': 0, 'byteOffset': off, 'byteLength': len(p)})
        off += len(p)
    buf = b''.join(partes)
    acc = [
        {'bufferView': 0, 'componentType': 5126, 'count': 3, 'type': 'VEC3', 'min': [0, 0, 0], 'max': [1, 1, 0]},
        {'bufferView': 1, 'componentType': 5126, 'count': 3, 'type': 'VEC3'},
        {'bufferView': 2, 'componentType': 5126, 'count': 3, 'type': 'VEC2'},
        {'bufferView': 3, 'componentType': 5123, 'count': 3, 'type': 'SCALAR'},
        {'bufferView': 4, 'componentType': 5126, 'count': 2, 'type': 'SCALAR', 'min': [0], 'max': [1]},
        {'bufferView': 5, 'componentType': 5126, 'count': 2, 'type': 'VEC4'},
        {'bufferView': 6, 'componentType': 5126, 'count': 2, 'type': 'SCALAR', 'min': [0], 'max': [1]},
        {'bufferView': 7, 'componentType': 5126, 'count': 2, 'type': 'VEC3'},
    ]
    g = {
        'asset': {'version': '2.0', 'generator': 'gen_prueba_gltf_jerarquia.py'},
        'extensionsUsed': ['KHR_lights_punctual', 'KHR_materials_emissive_strength'],
        'scene': 0, 'scenes': [{'nodes': [0, 3, 4, 5]}],
        'nodes': [
            {'name': 'Padre', 'translation': [1, 0, 0], 'children': [1, 2]},
            {'name': 'Hijo', 'mesh': 0, 'translation': [0, 1, 0]},
            {'name': 'Camara', 'camera': 0, 'translation': [0, 0, 5]},
            {'name': 'Sol', 'extensions': {'KHR_lights_punctual': {'light': 0}}},
            {'name': 'Foco', 'translation': [0, 3, 0], 'extensions': {'KHR_lights_punctual': {'light': 1}}},
            {'name': 'Girador', 'mesh': 0, 'translation': [0, 0, -2]},
        ],
        'meshes': [{'name': 'Triangulo', 'primitives': [{'attributes': {'POSITION': 0, 'NORMAL': 1, 'TEXCOORD_0': 2},
                                                          'indices': 3, 'material': 0}]}],
        'materials': [{'name': 'Mat', 'pbrMetallicRoughness': {
            'baseColorFactor': [0.21404114, 0.21404114, 0.21404114, 1], 'metallicFactor': 0.25, 'roughnessFactor': 0.75,
            'baseColorTexture': {'index': 0}},
            'emissiveFactor': [1, 0, 0], 'extensions': {'KHR_materials_emissive_strength': {'emissiveStrength': 1}}}],
        'textures': [{'source': 0}],
        'images': [{'name': 'cuadros', 'uri': 'data:image/png;base64,' + base64.b64encode(png_2x2()).decode()}],
        'cameras': [{'type': 'perspective', 'perspective': {'yfov': 0.5, 'znear': 0.1, 'zfar': 100, 'aspectRatio': 1.5}}],
        'extensions': {'KHR_lights_punctual': {'lights': [
            {'type': 'directional', 'intensity': 683.0, 'color': [1, 1, 1]},
            {'type': 'spot', 'intensity': 683.0 * math.pi, 'color': [1, 1, 1],
             'spot': {'innerConeAngle': 0.25, 'outerConeAngle': 0.5}}]}},
        'animations': [
            {'name': 'GiraAction', 'extras': {'frameRate': 24},
             'channels': [{'sampler': 0, 'target': {'node': 5, 'path': 'rotation'}}],
             'samplers': [{'input': 4, 'output': 5, 'interpolation': 'STEP'}]},
            {'name': 'PadreAction', 'extras': {'frameRate': 24},
             'channels': [{'sampler': 0, 'target': {'node': 0, 'path': 'translation'}}],
             'samplers': [{'input': 6, 'output': 7, 'interpolation': 'LINEAR'}]},
        ],
        'accessors': acc, 'bufferViews': vistas,
        'buffers': [{'byteLength': len(buf), 'uri': 'data:application/octet-stream;base64,' + base64.b64encode(buf).decode()}],
    }
    with open(os.path.join(AQUI, 'prueba_gltf_jerarquia.gltf'), 'w') as f:
        json.dump(g, f, indent=1)


if __name__ == '__main__':
    main()
