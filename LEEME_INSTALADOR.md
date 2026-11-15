# GTI Club+ para PC — instalador

Este instalador **no contiene ningún fichero del juego**. Fabrica la versión de PC en tu ordenador a partir de **tu propia copia** de GTI Club+ Rally Côte d'Azur de PS3.

## Qué necesitas

1. **El juego, en la versión exacta soportada**
   - Versión **europea**, identificador **NPEB00060**.
   - Con la **actualización 1.02** instalada.
   - Opcional: el pack de coches (DLC).
   - Otras regiones (americana, japonesa) u otras versiones **no funcionan**: el instalador lo comprueba y se detiene.
2. **RPCS3** con el juego instalado y funcionando (con sus licencias `.rap`) y con el **firmware oficial de PS3** instalado. Del firmware solo se usa el decodificador de música; sin él, el juego no tiene música.
3. **Python 3** (con «Add python.exe to PATH»). El módulo `capstone` 5.0.7 lo instala `instalar.bat` si falta.
4. **El compilador**: carpeta `toolchain/` junto a este fichero (llvm-mingw + CMake + Ninja, con SDL3 y zlib en `toolchain/deps`), o indicada con `--toolchain`.
5. Un PC con Windows 10/11 de 64 bits, procesador con AVX2 y gráfica con OpenGL 4.5. Unos 3 GB libres.

## Cómo se usa

Haz doble clic en `instalar.bat`, arrastra la carpeta de RPCS3 a la ventana y pulsa Intro. O desde una consola:

```
python instalar.py --rpcs3 "C:\ruta\a\RPCS3"
```

Tarda varios minutos (traducir y compilar). Al terminar, la carpeta `GTI Club+` contiene `GTI Club+.exe` y una copia de los datos de tu juego. Las partidas guardadas y los ajustes se crean ahí mismo.

## Qué hace, paso a paso

1. Comprueba que el juego instalado es NPEB00060 1.02.
2. Descifra el ejecutable del juego (lo hace RPCS3 con tu licencia).
3. Analiza el código y lo traduce a C++.
4. Traduce los programas de sonido y geometría.
5. Compila todo junto con el motor de este proyecto (gráficos, sonido, mando, menú).
6. Copia el resultado y los datos del juego a la carpeta de salida.

## Importante

La carpeta `GTI Club+` resultante contiene tu copia del juego y código derivado de él: es para tu uso personal. **No la repartas.** Lo que sí se puede compartir es este instalador.

## En el juego

- **F1** (o pulsar el panel táctil / botón PS del mando): menú de opciones.
- «Informe de problemas» crea un fichero con los datos del equipo y el registro, y abre la página de incidencias del proyecto.
