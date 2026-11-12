# Formato de interfaz de Sumo ("sumotool"): HUD y menús

Ruta: `data/sumotool/ps3/<pantalla>/`. Pantallas: hud, main_menu, garage, pause_menu, options, machine_select, popups, credits…

Cada pantalla tiene tres archivos y, además, variantes por idioma (`_en`, `_sp`, `_fr`, `_ge`, `_it`) para lo que lleva texto:

| Archivo | Contenido |
|---|---|
| `.dat` | Bloques `tag(4) size(4) payload(4) 0x44332211` en big-endian |
| `.rel` | Bloque `RELO`: tabla de recolocación de punteros del `.dat` |
| `.gpu` | Texturas en bruto para la GPU. El HUD es **DXT5 lineal de 512×2048** (1 MB) |

## Bloques del `.dat` (hud.dat)
- **`INFO`** (0x40): cabecera.
- **`PTEX`**: 57 sprites. Cada uno lleva 4 esquinas en píxeles del atlas (x,y)×4 y 2 floats más (40 bytes).
- **`KFRM`** (27 KB): keyframes de animación (posición, escala, alfa…), al estilo Flash.
- **`OBJS`** (13 KB): objetos o instancias de la escena.
- **`SCNE`**: escena o raíz.

## Pendiente
- Decodificar `KFRM`, `OBJS` y `SCNE`.
- Decidir si se dibuja con el código original recompilado (enganchando sus llamadas de dibujado) o con un intérprete propio de estas escenas.
