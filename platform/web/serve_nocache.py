#!/usr/bin/env python3
# Sirve el build web (carpeta web/) SIN cache -> cada reload trae el .wasm/.data fresco.
# El server default de python manda Last-Modified pero no Cache-Control, y el navegador
# cachea los binarios grandes sin revalidar (se sigue viendo el build viejo). Uso:
#   python platform/web/serve_nocache.py            (desde la raiz del editor)
import http.server, socketserver, os, functools

PORT = 8000
DIRECTORY = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "web")

class NoCache(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-cache, no-store, must-revalidate")
        self.send_header("Pragma", "no-cache")
        self.send_header("Expires", "0")
        super().end_headers()

handler = functools.partial(NoCache, directory=DIRECTORY)
socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(("127.0.0.1", PORT), handler) as httpd:
    print("Sirviendo %s en http://127.0.0.1:%d (sin cache)" % (DIRECTORY, PORT))
    httpd.serve_forever()
