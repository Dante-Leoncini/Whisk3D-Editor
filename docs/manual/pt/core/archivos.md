# Arquivos

Uma única API para ler e escrever: `w3dFileSystem::ReadFileBytes`, `ReadTextFile`, `WriteTextFile`, `ReadFileRange` (um pedaço, para carregar malhas por partes), `ListDir`, `FileExists`. Ninguém chama `fopen` por conta própria.

O `ReadFileBytes` procura nesta ordem:

1. **Uma biblioteca vinculada** (os caminhos `lib:<nombre>/...`).
2. **O pak embutido:** um jogo compilado com "Assets: Empacotados" leva todos os seus arquivos dentro do executável.
3. **O contêiner montado:** o .w3d aberto. O contêiner **é montado, não extraído**: texturas, fontes, scripts e interfaces são carregados de dentro do zip sem que ninguém mude o seu código.
4. **Os assets do APK** (Android): um caminho relativo é lido do APK com o `AAssetManager` do NDK.
5. **O disco.**

Os caminhos são normalizados antes da busca (sem `.` nem `..`). Um .mtl que citava `modelos/crash/../../texturas/sombra_blob.png` não estava em nenhum índice, e dentro do APK a textura saía branca.

| Sistema | Os recursos (`res/`) | Onde se escreve |
|---|---|---|
| Windows, Linux | Ao lado do executável (no Linux, também `<prefijo>/share/Whisk3d/res`) | A pasta do usuário; as configurações, na do SDL (`SDL_GetPrefPath`) |
| Android | Dentro do APK | `/storage/emulated/0`; o que é exportado, em Downloads |
| Navegador | Empacotados no `.data`, montados em `/res` | A configuração, no `localStorage`; salvar baixa o arquivo ([como](../editor/proyecto.md)) |
| Symbian | Caminhos próprios do celular | `E:/` (o cartão de memória) ou `C:/` |
| Dreamcast | Um romdisk dentro do executável (`/rd`) | `/ram`, que é apagado ao desligar |
| PSP | Ao lado do `EBOOT.PBP` | `ms0:/` (ou `ef0:/` no PSP Go) |
| PS Vita | Dentro do `.vpk` (`app0:res`) | `ux0:` |
| 3DS | O romfs do `.3dsx` (`romfs:`) | O cartão SD (`sdmc:/`) |

Mais duas peças do módulo:

- **W3dRecursos** controla cada dado pesado (texturas, malhas, animações): quem o usa e quando liberá-lo. O que não é necessário de imediato é carregado em fatias, um pouco a cada quadro.
- **W3dConfig** salva configurações como `clave=valor`. No navegador elas vão para o `localStorage`; no resto, para um arquivo de texto. Não pede memória dinâmica, então funciona igual no Symbian. Em Lua: [Configuração salva](../lua/api-configuracion.md).
