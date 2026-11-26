<p align="center"><img src="assets/banner_racing_club.png" alt="Racing Club+" width="100%"></p>

<p align="center"><a href="https://www.patreon.com/ilesraider/posts/racing-club-175397772"><b>❤️ Patreon: el apoyo se destina al mantenimiento del servidor online. Descarga allí el instalador / Patreon support goes to the upkeep of the online server. Download the installer there</b></a></p>

# GTI Club PC (port nativo) — Hito 1: visor de modelos

Reimplementación nativa para PC (Windows x64, OpenGL) de *GTI Club + Rally Côte d'Azur* (PS3, NPEB00060).
**No incluye ningún archivo del juego**: carga los datos de tu copia (`NPEB00060\USRDIR\data`).

## Probar

```
build\gtiport.exe
```
Detecta automáticamente `..\NPEB00060\USRDIR\data`. También puedes pasar la ruta de `data` como argumento.

| Control | Acción |
|---|---|
| ← / → | coche anterior / siguiente |
| RePág / AvPág | recorrer **todos** los .ep3 del juego (objetos, decorado...) |
| Ratón izq. arrastrar | orbitar · der. arrastrar: desplazar · rueda: zoom |
| L | luces encendidas/apagadas · F alambre · R reset cámara · F11 pantalla completa · Esc salir |

## Compilar
`build.bat` (usa el toolchain llvm-mingw + SDL3 + zlib ya instalado en el equipo; no necesita Visual Studio).

## Estructura
- `src/formats/ep3.*` — lector de paquetes `.ep3` del motor *Emmersion* (Sumo Digital).
- `src/formats/edge_indexes.*` — descompresor de índices de Sony Edge Geometry (ingeniería inversa propia).
- `src/formats/gcm_texture.*` — texturas RSX (DXT1/3/5, ARGB con swizzle).
- `src/render/*` — renderizador OpenGL 3.3.
- `src/tools/ep3tool.cpp` — validador: `build\ep3tool.exe ..\NPEB00060\USRDIR\data`.
- `tools/*.py` — scripts de ingeniería inversa (referencia en Python, exportador OBJ).
- `re/` — EBOOT descifrado y desensamblados (solo para análisis local; no distribuir).

## Formato .ep3 (resumen)
Cabecera de items (`EMTH` texturas, `EMDH` materiales, `EEDH` entidades, `ECEH` cámaras, `EODH` localizadores, `ECDH` colisión)
→ bloques de datos con tabla de reubicación de punteros y un flujo deflate (texeles RSX / vértices RSX).
Las mallas usan segmentos Edge: posiciones float3 + índices comprimidos + flujo RSX (normal X11Y11Z10N, UV half2).
