# O projeto (.w3d) e os arquivos

| Função | Descrição | Onde |
|---|---|---|
| Abrir projeto | Abre um .w3d (também com clique duplo pelo sistema). | Propriedades → Arquivo/Render → Arquivo |
| Salvar / Salvar como | Salva na pasta e com o nome do cartão; Salvar como pede outro. Se faltar o nome, o campo fica vermelho. | Cartão Arquivo, <kbd>Ctrl+S</kbd> |
| Salvar versão | Salva e, além disso, deixa uma cópia numerada (`proyecto_v03.w3d`). | Cartão Arquivo |
| Extrair assets | Copia tudo o que está dentro para uma pasta (as cópias não mudam o projeto). | Cartão Arquivo |
| Malhas em texto | Salva as malhas como texto legível em vez de binário. | Cartão Arquivo |
| Importar | OBJ, FBX, glTF, GLB e interfaces .w3dui. O que é importado é copiado para dentro do projeto (sem duplicados). | Viewport 3D → Adicionar → Importar |
| Exportar | OBJ, glTF ou GLB do objeto selecionado (com ou sem modificadores e transformações). | Propriedades → Exportar |
| Mudanças não salvas | O que foi editado e não salvo (scripts, texturas pintadas) é marcado com `*`. Ao fechar ou abrir outro projeto, um aviso lista o que seria perdido. | Automático |
| Recurso dentro ou fora | Um recurso externo não é copiado: o seu caminho é salvo e há um aviso se ele faltar. | Cartão Recurso; "Salvar no / fora do w3d" no IDE e no Editor UV |

O salvamento nunca deixa o arquivo pela metade: primeiro escreve uma cópia temporária e depois a renomeia.

## No navegador (versão web)

O navegador não deixa que uma página leia nem grave os arquivos do PC por conta própria. Por isso, na versão web:

- **Abrir projeto**, **Importar** e escolher uma textura abrem o explorador de arquivos do navegador: aparece um aviso com o botão **Escolher arquivo**. Para um OBJ, escolha juntos o .obj, o seu .mtl e as suas texturas; para um glTF, o .gltf e o seu .bin.
- **Salvar** (<kbd>Ctrl+S</kbd>), **Salvar como**, **Salvar versão** e **Extrair assets** baixam o arquivo para a pasta de downloads do navegador. O mesmo acontece com **Exportar** (o OBJ é baixado com o seu .mtl e as suas texturas ao lado), o render, o mapa UV, as texturas pintadas salvas fora e os scripts externos.
- Quando o editor pede uma pasta para salvar, nenhum explorador é aberto: o arquivo é baixado diretamente.
- Se o navegador perguntar se a página pode baixar vários arquivos, é preciso aceitar. Se não, só o primeiro é baixado.
- O que foi aberto fica na memória da página: ao recarregá-la, perde-se tudo o que não foi baixado.
