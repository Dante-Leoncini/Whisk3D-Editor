#!/usr/bin/env python3
# ============================================================================================================
#  simbolos_heap.py -- traduce el volcado de heap.c (W3D_HEAP_OUT) a funciones: por cada etiqueta, las pilas con mas
#  memoria viva, mostrando las funciones del programa (sin las de libc / libstdc++) de la mas cercana al malloc a la
#  que la llamo.
#
#    python3 tools/perfil/simbolos_heap.py /tmp/heap.txt [cuantas=25] [profundidad=5] [diff]
#
#  Con dos etiquetas se ve que quedo tomado entre una y otra: 'heap antes' / 'heap despues' en el guion.
# ============================================================================================================
import subprocess, sys, collections, os

path = sys.argv[1]
top = int(sys.argv[2]) if len(sys.argv) > 2 else 25
depth = int(sys.argv[3]) if len(sys.argv) > 3 else 5

blocks = []          # (etiqueta, total, [(bytes, count, [(archivo, offset)])])
for line in open(path):
    line = line.rstrip('\n')
    if line.startswith('== '):
        blocks.append([line[3:], '', []]); continue
    if line.startswith('total '):
        blocks[-1][1] = line; continue
    parts = line.split(' ')
    if len(parts) < 2 or not blocks: continue
    frames = []
    for fr in parts[2:]:
        if fr == '?' or '+0x' not in fr: frames.append(('?', 0)); continue
        f, off = fr.rsplit('+0x', 1)
        frames.append((f, int(off, 16)))
    blocks[-1][2].append((int(parts[0]), int(parts[1]), frames))

# todas las direcciones de cada archivo, traducidas de una (addr2line por archivo)
want = collections.defaultdict(set)
for _, _, st in blocks:
    for _, _, frames in st:
        for f, off in frames:
            if f != '?': want[f].add(off)
names = {}
for f, offs in want.items():
    offs = sorted(offs)
    try:
        out = subprocess.run(['addr2line', '-f', '-C', '-e', f] + ['0x%x' % o for o in offs],
                             capture_output=True, text=True).stdout.split('\n')
    except Exception:
        out = []
    for i, o in enumerate(offs):
        fn = out[2 * i] if 2 * i < len(out) else '??'
        names[(f, o)] = fn

NOISE = ('libc.so', 'libstdc++', 'ld-linux', 'heap.so', 'libgcc')
def short(fn):
    fn = fn.split('(')[0]
    return fn if len(fn) < 70 else fn[:67] + '...'

# el programa: el ejecutable que mas aparece (whisk3d, un juego compilado)
exe = collections.Counter(fr[0] for _, _, st in blocks for _, _, frames in st for fr in frames if fr[0] != '?')
exe = [f for f, _ in exe.most_common() if not any(n in f for n in NOISE) and not f.endswith('.so') and '.so.' not in f]
exe = exe[0] if exe else ''

# (con 'diff': solo lo que cambio de una etiqueta a la siguiente, por funcion del programa y por pila de 3)
only_diff = len(sys.argv) > 4 and sys.argv[4] == 'diff'
def own_key(frames, n):
    own = [fr for fr in frames if fr[0] == exe]
    return ' < '.join(short(names.get(fr, '??')) for fr in own[:n]) if own else '(fuera del programa)'
prev = None
for label, total, st in blocks:
    by = collections.Counter()
    for b, c, frames in st: by[own_key(frames, depth)] += b
    if prev is not None:
        print('== %s -> %s' % (prev[0], label))
        keys = set(by) | set(prev[1])
        d = sorted(((by[k] - prev[1][k], k) for k in keys), key=lambda x: -abs(x[0]))
        for delta, k in d[:top]:
            if abs(delta) < 1024: break
            print('  %+9.1f KB  %s' % (delta / 1024.0, k))
        print()
    prev = (label, by)
if only_diff: sys.exit(0)

for label, total, st in blocks:
    print('==', label, '|', total)
    for b, c, frames in st[:top]:
        prog = [short(names.get(fr, '??')) for fr in frames if fr[0] != '?' and not any(n in fr[0] for n in NOISE)]
        print('  %8.1f KB %7d  %s' % (b / 1024.0, c, ' < '.join(prog[:depth])))
    # POR LA PRIMERA FUNCION DEL PROGRAMA en la pila (lo que piden los drivers se le cuenta a quien los llamo; lo que
    # no pasa por el programa -hilos del driver- va aparte)
    by = collections.Counter(); outside = 0
    for b, c, frames in st:
        own = [fr for fr in frames if fr[0] == exe]
        if own: by[short(names.get(own[0], '??'))] += b
        else: outside += b
    print('  -- por la primera funcion de %s (fuera del programa: %.1f KB)' % (os.path.basename(exe), outside / 1024.0))
    for fn, b in by.most_common(top):
        print('  %8.1f KB  %s' % (b / 1024.0, fn))
    print()
