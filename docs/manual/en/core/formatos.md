# Math, scene and formats

Its own **vectors, 4x4 matrices and quaternions** (`math/`), which compile the same on PC and on the retro systems. On top of that, the **scene** (`objects/`): objects with a parent-child hierarchy, meshes, materials, lights (directional, point and spot), cameras and armatures. And the **animations** (`animation/`): keyframes, armatures, vertices, flipbooks and locomotion.

Its own formats are designed so that a small machine can read them fast:

| Format | What it is |
|---|---|
| `.w3d` | The project: a standard ZIP (any unzip tool opens it). It's written uncompressed and can also be read compressed. No ZIP64 |
| `.w3dm` | The geometry as line-based text: readable, editable by hand and easy for the N95 to read |
| `.w3db` | The same mesh in binary, already laid out for memory: opening it means copying arrays |
| `.w3dt` | A texture ready for the GPU (16-bit, with mipmaps already made) |
| `.w3dpack` | An asset container encrypted with ChaCha20; the application provides the key |
| Embedded pak | A built game's files, obfuscated with XOR. It's against the curious, not cryptography |
| glTF / GLB | A reader and renderer in a single header (`io/w3dGltf.h`), for use outside the editor |
