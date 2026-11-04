# Script Editor (IDE)

A text editor for Lua scripts and [hand-written shaders](shaders.md) (`.glsl`), with line numbers, syntax colors, indentation guides and support for ñ and accented letters. While it has focus, letters type (they don't trigger editor shortcuts). On Android, tapping the text opens the keyboard. On the N95 you type with T9.

| Function | Description | Shortcut |
|---|---|---|
| Script | Choose which .lua of the project to edit, or Load .lua... (an external file). | Script menu |
| Save | Saves the script. If it lives inside the .w3d, it also saves the project. | <kbd>Ctrl+S</kbd> or File → Save |
| Save As... | Another name, inside or outside the project, in a folder of your choice. | File |
| Save inside the .w3d | Only if the script is external: puts it into the project. | File |
| Save outside the .w3d... | Only if the script is internal: takes it out to a file; the objects switch to using that file. | File |
| Refresh | Loads the file as it is on disk (what was edited with another editor). If there are unsaved changes in the editor, it asks before discarding them (and they can be recovered with <kbd>Ctrl+Z</kbd>). With the game running, it live-reloads the objects that use the script. | Button |
| Shaders (.glsl) | Opened from the material (the small button on the **Shader** row), from the Library or from the selector, and highlighted as GLSL. **Compile** appears next to the name: it compiles what you're writing (if a material uses it, it saves it and compiles that material). Saving a shader compiles it in every material that uses it and reports whether it compiled, or the error with its line. | <kbd>Ctrl+S</kbd> |
| Save protection | If the external file was changed elsewhere since it was opened, Save asks before overwriting it. Save also live-reloads the script if the game is running. | Automatic |
| Indentation (Tab) | <kbd>Tab</kbd> fills up to the next multiple of 4; with several lines selected it indents them one level and <kbd>Shift+Tab</kbd> removes it. Vertical lines mark each level; the one for the cursor's block looks brighter. | <kbd>Tab</kbd> / <kbd>Shift+Tab</kbd> |
| Edit menu | Undo, Redo, Cut, Copy, Paste, Find... and Replace... | Edit button |
| Select menu | Select All. | Select button |
| Undo / Redo | On the script's text (not on the scene). | <kbd>Ctrl+Z</kbd> / <kbd>Ctrl+Y</kbd> or <kbd>Ctrl+Shift+Z</kbd> |
| Find / Replace | Opens a panel at the top right: the text to find, how many matches there are ("3 of 7"), Match case, Whole word, Previous / Next, and in Replace the new text with Replace and Replace all (a single undo step). Matches are highlighted in the code. The panel doesn't block the editor: with the pointer over the script you can keep typing. It closes with Close or <kbd>Esc</kbd>. | <kbd>Ctrl+F</kbd> / <kbd>Ctrl+H</kbd> |
| Copy / Cut / Paste | With the system clipboard. | <kbd>Ctrl+C</kbd> / <kbd>Ctrl+X</kbd> / <kbd>Ctrl+V</kbd> |
| Select All / Undo | Undo affects the text, not the scene. | <kbd>Ctrl+A</kbd> / <kbd>Ctrl+Z</kbd> |
| Selecting with your finger | Double tap selects the word, triple tap the whole line. With a selection, two handles (green circles) appear at its ends: drag them to adjust it. | Taps |
| Magnifier (touch) | Holding your finger still on the code for a moment shows the line enlarged, with the cursor, above your finger; when you drag, the cursor follows your finger and stays where you let go. | Long press |
| Selecting on the N95 | The arrows move the cursor; while holding the pencil key, the arrows select from where the cursor is. While you hold the pencil key, a bar appears at the bottom: the left selection key **copies** and the right one **pastes**. | Pencil + arrows / softkeys |
| Moving through the text | Arrows, Home, End, PageUp, PageDown (with <kbd>Shift</kbd> they select); <kbd>Ctrl+Home</kbd> / <kbd>Ctrl+End</kbd> go to the beginning or the end. | Keyboard |
