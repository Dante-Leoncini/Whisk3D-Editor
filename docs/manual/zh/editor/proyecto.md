# 项目（.w3d）与文件

| 功能 | 说明 | 位置 |
|---|---|---|
| Open project | 打开一个 .w3d（也可以在系统中双击打开）。 | Properties → File/Render → File |
| Save / Save As | 按卡片中设置的文件夹和名称保存；Save As 会要求你输入另一个名称。如果没有填名称，该字段会变成红色。 | File 卡片，<kbd>Ctrl+S</kbd> |
| Save version | 保存，并额外留下一个带编号的副本（`proyecto_v03.w3d`）。 | File 卡片 |
| Extract assets | 把内部的所有内容复制到一个文件夹中（这些副本不会改变项目）。 | File 卡片 |
| Meshes as text | 把网格保存为可读的文本，而不是二进制。 | File 卡片 |
| Imports | OBJ、FBX、glTF、GLB 和 .w3dui 界面。导入的内容会复制到项目内部（不会重复）。 | 3D Viewport → Add → Imports |
| Export | 导出所选对象的 OBJ、glTF 或 GLB（可选择是否包含修改器和变换）。 | Properties → Export |
| 未保存的修改 | 已编辑但未保存的内容（脚本、绘制过的纹理）会标上 `*`。关闭项目或打开另一个项目时，会弹出提示，列出将会丢失的内容。 | 自动 |
| 资源在内部或外部 | 外部资源不会被复制：只保存它的路径，缺失时会发出提示。 | Resource 卡片；IDE 和 UV Editor 中的“Save inside / outside the .w3d” |

保存时绝不会留下写了一半的文件：它会先写入一个临时副本，然后再重命名。

## 在浏览器中（网页版）

浏览器不允许网页自行读取或写入电脑上的文件。因此，在网页版中：

- **Open project**、**Imports** 以及选择纹理时，会打开浏览器的文件选择器：会出现一个带有 **Choose file** 按钮的提示框。对于 OBJ，要一起选择 .obj、它的 .mtl 和纹理；对于 glTF，则选择 .gltf 和它的 .bin。
- **Save**（<kbd>Ctrl+S</kbd>）、**Save As**、**Save version** 和 **Extract assets** 会把文件下载到浏览器的下载文件夹。**Export**（OBJ 会连同它的 .mtl 和纹理一起下载）、渲染结果、UV 贴图、保存在外部的绘制纹理以及外部脚本也是如此。
- 编辑器要求选择保存文件夹时，不会打开任何文件浏览器：文件会直接下载。
- 如果浏览器询问是否允许该网页下载多个文件，请选择允许。否则只会下载第一个文件。
- 打开的内容保存在网页的内存中：重新加载网页后，所有没有下载的内容都会丢失。
