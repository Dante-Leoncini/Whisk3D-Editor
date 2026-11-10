# Matemática, cena e formatos

**Vetores, matrizes 4x4 e quatérnios** próprios (`math/`), que compilam igual no PC e nos sistemas retrô. Por cima, a **cena** (`objects/`): objetos com hierarquia de pai e filho, malhas, materiais, luzes (direcional, pontual e spot), câmeras e esqueletos. E as **animações** (`animation/`): keyframes, esqueletos, vértices, flipbooks e locomoção.

Os formatos próprios são pensados para que uma máquina pequena os leia rápido:

| Formato | O que é |
|---|---|
| `.w3d` | O projeto: um ZIP padrão (abre com qualquer descompactador). É gravado sem compressão e também é lido compactado. Sem ZIP64 |
| `.w3dm` | A geometria em texto, por linhas: legível, editável à mão e fácil de ler para o N95 |
| `.w3db` | A mesma malha em binário, já pronta para a memória: abri-la é copiar arrays |
| `.w3dt` | Uma textura pronta para a GPU (16 bits e mipmaps já feitos) |
| `.w3dpack` | Um contêiner de assets cifrado com ChaCha20; a chave é definida pela aplicação |
| Pak embutido | Os arquivos de um jogo compilado, ofuscados com XOR. É contra curiosos, não criptografia |
| glTF / GLB | Um leitor e renderizador em um único header (`io/w3dGltf.h`), para usar fora do editor |
