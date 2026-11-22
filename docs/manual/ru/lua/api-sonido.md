# Звук

| Функция | Что делает |
|---|---|
| `beep(freq [, ms, vol])` | Звуковой сигнал (без файла). |
| `sound("sounds/x.wav" [, vol, pitch, loop, pan]) → h` | Воспроизводит звук; возвращает идентификатор. |
| `sound("sounds/x.wav", {vol, pitch, loop, pan, attack, decay, sustain, fall, release, duration, reverb}) → h` | То же самое с именами, плюс огибающая (атака, спад, уровень удержания от 0 до 1, `fall`: удержание падает на 60 дБ за столько секунд, затухание и `duration`: нота отпускается сама через столько секунд) и доля звука, идущая в реверберацию (от 0 до 1). |
| `stopSound(h [, fundido])`, `stopSounds()`, `adjustSound(h, vol [, pitch])` | Остановить или настроить звуки. |
| `releaseSound(h)` | Отпускает ноту: огибающая переходит к затуханию. |
| `reverb(room, damping, level)` | Общая реверберация: размер зала, демпфирование и уровень (каждый от 0 до 1; уровень 0 выключает). |
| `mute(bool)`, `isMuted()` | Общее отключение звука. |
| `playSound(obj [, seg])`, `muteSound(obj)` | Запустить **объект Звук** заново (с `seg` секунд его отрезка) или заглушить. |
| `soundObject(obj) → t`, `setSoundObject(obj, t)` | Прочитать / изменить объект Звук: `file` (источник), `active`, `volume`, `pitch`, `reverb`, `pan`, `start`, `clipStart`, `clipEnd`, `loop`, `positional`, `radius`, `range` (при чтении ещё `duration`, `playing`). |
| `createSound("sounds/x.wav" [, parent]) → obj` | Новый объект Звук (`destroy(obj)` удаляет его). В префабе создаётся как любой объект. |
