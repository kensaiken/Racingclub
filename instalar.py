"""Racing Club+ : builds the PC version of the game from the user's own copy. No game file is part of this project.

usage: python instalar.py --rpcs3 <RPCS3 folder> [--out <folder>] [--toolchain <folder>] [--jobs N] [--no-copy]

Needs, inside the RPCS3 folder:
  - the game installed: GTI Club+ Rally Cote d'Azur, EUROPEAN version NPEB00060 with update 1.02 (and, optionally,
    the car pack) with its licences (.rap) -> dev_hdd0/game/NPEB00060
  - the PS3 system software installed -> dev_flash/sys/external/flashATRAC.pic (music decoder; without it the game
    has no music)
Needs on this PC: Python 3 with capstone 5.0.x, and the llvm-mingw + CMake + Ninja toolchain (see LEEME_INSTALADOR.md).

Steps: check the version, decrypt the executable (RPCS3 does it), translate the game code and the SPU programs to
C++, compile, and put the result in the output folder (executable + a copy of the game data).
"""
import argparse, hashlib, os, re, shutil, struct, subprocess, sys, time

ROOT = os.path.dirname(os.path.abspath(__file__))
TITLE_ID, APP_VER = 'NPEB00060', '01.02'
ELF_SHA1 = 'e9faf331b52770164be0c19f427c10aef747776d'     # decrypted EBOOT of NPEB00060 1.02
EBOOT_SHA1 = 'c68e065fb373c69c18195182fc0cd79635cead39'   # the same, encrypted (USRDIR/EBOOT.BIN)
SPU_SPECS = ['ba3680:0xa200', 'baea80:0x14c00', '0xbd0b00:0xd560:0x3000', '0xbce880:0x2000:0x2f080']
ATRAC_SPEC = 'file:re/spu_atrac_32980.bin:0:0x7d40:32980:a7ac00'


def fail(msg):
    print('\nERROR: ' + msg)
    sys.exit(1)


def step(n, text):
    print('\n[%d/7] %s' % (n, text), flush=True)


def run(cmd, env=None, cwd=ROOT, quiet=False):
    r = subprocess.run(cmd, cwd=cwd, env=env, stdout=subprocess.PIPE if quiet else None, stderr=subprocess.STDOUT if quiet else None)
    if r.returncode != 0:
        if quiet and r.stdout: print(r.stdout.decode('utf-8', 'replace')[-3000:])
        fail('ha fallado: ' + ' '.join(str(c) for c in cmd))


def sfo(path):
    d = open(path, 'rb').read()
    if d[:4] != b'\0PSF': fail('PARAM.SFO no valido: ' + path)
    keys, data, n = struct.unpack_from('<III', d, 8)
    out = {}
    for i in range(n):
        ko, fmt, ln, mx, do = struct.unpack_from('<HHIII', d, 20 + 16 * i)
        k = d[keys + ko: d.index(b'\0', keys + ko)].decode()
        v = d[data + do: data + do + ln]
        out[k] = struct.unpack('<I', v[:4])[0] if fmt == 0x0404 else v.rstrip(b'\0').decode('utf-8', 'replace')
    return out


def read_shared(path):
    """Reads a file another process keeps open for writing and deleting (RPCS3's log): plain open() is refused."""
    import ctypes, msvcrt
    k = ctypes.windll.kernel32
    k.CreateFileW.restype = ctypes.c_void_p
    h = k.CreateFileW(path, 0x80000000, 7, None, 3, 0x80, None)   # GENERIC_READ, share read|write|delete, OPEN_EXISTING
    if h in (None, ctypes.c_void_p(-1).value): raise OSError('cannot open ' + path)
    with open(msvcrt.open_osfhandle(h, os.O_RDONLY), 'r', encoding='utf-8', errors='replace') as f:
        return f.read()


def install_pkg(exe, rp, pkg):
    """Installs one .pkg with RPCS3. RPCS3 only installs packages with its window open and asks the user to confirm
    ("Install"); its log tells when it is done, then this closes it."""
    if not os.path.exists(pkg): fail('no encuentro ' + pkg)
    if subprocess.run(['tasklist', '/FI', 'IMAGENAME eq rpcs3.exe', '/NH'], capture_output=True, text=True).stdout.lower().count('rpcs3.exe'):
        fail('RPCS3 esta abierto: cierralo y vuelve a empezar')
    print('  instalando %s con RPCS3: confirma la instalacion en su ventana ("Install")' % os.path.basename(pkg), flush=True)
    log = os.path.join(rp, 'log', 'RPCS3.log')
    os.makedirs(os.path.dirname(log), exist_ok=True)
    p = subprocess.Popen([exe, '--installpkg', pkg], cwd=rp, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ok = None
    t_end = time.time() + 30 * 60
    time.sleep(3)
    while ok is None and time.time() < t_end and p.poll() is None:
        time.sleep(1)
        try:
            text = read_shared(log)
        except OSError:
            continue
        if 'Successfully installed ' + pkg in text or 'GUI: Successfully installed' in text: ok = True
        elif re.search(r'(Failed to install|installation (failed|cancel)|PKG: .*(not supported|failed to))', text, re.I): ok = False
    time.sleep(1)
    if p.poll() is None: p.kill()
    if ok is None: fail('RPCS3 se ha cerrado o no ha terminado de instalar %s (cancelado o falta la licencia .rap)' % os.path.basename(pkg))
    if not ok: fail('RPCS3 no ha podido instalar %s. Mira %s' % (os.path.basename(pkg), log))
    print('  %s instalado' % os.path.basename(pkg), flush=True)


def main():
    ap = argparse.ArgumentParser(description='Racing Club+ para PC a partir de tu copia del juego (NPEB00060, 1.02)')
    ap.add_argument('--rpcs3', required=True, help='carpeta de RPCS3 (con el firmware de PS3 instalado; y con el juego, si no se usa --juego)')
    ap.add_argument('--pkg', nargs='*', default=[], help='paquetes .pkg a instalar antes en RPCS3, en orden: juego, actualizacion 1.02, pack de coches')
    ap.add_argument('--juego', help='carpeta del juego extraido de los .pkg (p. ej. con PyKG): NPEB00060 con USRDIR dentro')
    ap.add_argument('--rap', nargs='*', default=[], help='licencias .rap del juego (y del pack de coches); se copian a RPCS3 para descifrar')
    ap.add_argument('--out', default=os.path.join(ROOT, 'Racing Club+'), help='carpeta de salida')
    ap.add_argument('--toolchain', default=os.environ.get('GTI_TOOLCHAIN', ''), help='carpeta del compilador (bin/, deps/)')
    ap.add_argument('--jobs', type=int, default=os.cpu_count() or 4)
    ap.add_argument('--no-copy', action='store_true', help='no copiar los datos del juego a la salida')
    ap.add_argument('--keep', action='store_true', help='conservar los ficheros intermedios (re/, codigo traducido, build-inst/) para recompilar mas rapido')
    ap.add_argument('--idioma', type=int, choices=range(1, 6), help='idioma del juego: 1 English, 2 Francais, 3 Espanol, 4 Deutsch, 5 Italiano')
    a = ap.parse_args()
    t0 = time.time()

    step(1, 'Comprobando el juego (hace falta la version europea %s con la actualizacion %s)' % (TITLE_ID, APP_VER))
    rp = os.path.abspath(a.rpcs3)
    exe = os.path.join(rp, 'rpcs3.exe')
    if not os.path.exists(exe): fail('no encuentro rpcs3.exe en ' + rp)
    if a.rap:   # the licences: RPCS3 looks for them in its user folder when it installs and decrypts
        exdata = os.path.join(rp, 'dev_hdd0', 'home', '00000001', 'exdata')
        os.makedirs(exdata, exist_ok=True)
        for r in a.rap:
            dst = os.path.join(exdata, os.path.basename(r))
            if not os.path.exists(dst): shutil.copyfile(r, dst)
    for p in a.pkg:   # base game, update 1.02, car pack: in this order
        install_pkg(exe, rp, os.path.abspath(p))
    if a.juego:   # the game extracted from its .pkg files (e.g. with PyKG): base game + update 1.02 + optional car pack
        game = os.path.abspath(a.juego)
        if os.path.basename(game).upper() == 'USRDIR': game = os.path.dirname(game)
        if not os.path.exists(os.path.join(game, 'USRDIR', 'EBOOT.BIN')):
            fail('en %s no esta el juego extraido (hace falta la carpeta %s con USRDIR\\EBOOT.BIN dentro)' % (game, TITLE_ID))
    else:         # the game installed in RPCS3
        game = os.path.join(rp, 'dev_hdd0', 'game', TITLE_ID)
        if not os.path.exists(os.path.join(game, 'PARAM.SFO')):
            fail('RPCS3 no tiene instalado el juego %s (version europea de GTI Club+). Otras regiones no sirven.' % TITLE_ID)
    if os.path.exists(os.path.join(game, 'PARAM.SFO')):
        info = sfo(os.path.join(game, 'PARAM.SFO'))
        ver = info.get('APP_VER')
        print('  %s  %s  version %s' % (info.get('TITLE_ID', '?'), info.get('TITLE', '?').replace('\n', ' '), ver or '?'))
        if info.get('TITLE_ID') != TITLE_ID: fail('el juego no es %s (version europea). Otras regiones no sirven.' % TITLE_ID)
        if ver and ver != APP_VER: fail('hace falta la actualizacion %s del juego (encontrada: %s).' % (APP_VER, ver))
    usrdir = os.path.join(game, 'USRDIR')
    # the executable itself tells the exact version (an extracted base game keeps the PARAM.SFO without APP_VER)
    if hashlib.sha1(open(os.path.join(usrdir, 'EBOOT.BIN'), 'rb').read()).hexdigest() != EBOOT_SHA1:
        fail('el EBOOT.BIN no es el de %s con la actualizacion %s. Si has extraido los .pkg, extrae tambien la actualizacion encima del juego.' % (TITLE_ID, APP_VER))
    print('  ejecutable del juego: version %s correcta' % APP_VER)
    if not os.path.isdir(os.path.join(usrdir, 'data', 'cars', 'pack_modern_1')): print('  aviso: no veo el pack de coches (DLC); el juego funciona sin el')
    pic = os.path.join(rp, 'dev_flash', 'sys', 'external', 'flashATRAC.pic')
    if not os.path.exists(pic):
        pic = None
        print('  aviso: RPCS3 no tiene el firmware de PS3 instalado (flashATRAC.pic): el juego no tendra musica')
    tc = a.toolchain or os.path.join(ROOT, 'toolchain')
    if not os.path.isdir(os.path.join(tc, 'bin')): tc = os.path.join(os.path.expanduser('~'), '.local', 'share', 'retcomm', 'toolchains', 'cmake-clang-v1', 'latest')
    if not os.path.exists(os.path.join(tc, 'bin', 'clang++.exe')): fail('no encuentro el compilador (carpeta toolchain/ o --toolchain). Mira LEEME_INSTALADOR.md')
    try:
        sys.path[:] = [p for p in sys.path if os.path.abspath(p or '.') != os.path.join(ROOT, 'tools')]
        import capstone
    except ImportError:
        fail('falta el modulo capstone de Python:  python -m pip install capstone==5.0.7')

    step(2, 'Descifrando el ejecutable del juego con RPCS3')
    re_dir = os.path.join(ROOT, 're')
    os.makedirs(re_dir, exist_ok=True)
    elf = os.path.join(re_dir, 'EBOOT.elf')
    if not (os.path.exists(elf) and hashlib.sha1(open(elf, 'rb').read()).hexdigest() == ELF_SHA1):
        shutil.copyfile(os.path.join(usrdir, 'EBOOT.BIN'), os.path.join(re_dir, 'EBOOT.BIN'))
        if os.path.exists(elf): os.remove(elf)
        run([exe, '--decrypt', os.path.join(re_dir, 'EBOOT.BIN')], cwd=rp, quiet=True)
        if not os.path.exists(elf): fail('RPCS3 no ha podido descifrar el juego: falta la licencia .rap del juego (indica el fichero .rap) o no es la correcta.')
    if hashlib.sha1(open(elf, 'rb').read()).hexdigest() != ELF_SHA1:
        fail('el ejecutable descifrado no es el de %s %s; esta version no esta soportada' % (TITLE_ID, APP_VER))
    print('  ejecutable correcto')

    py = sys.executable
    step(3, 'Analizando el codigo del juego')
    run([py, os.path.join('tools', 'prepare_re.py'), elf, re_dir])
    run([py, os.path.join('tools', 'jumptables.py')], quiet=True)

    step(4, 'Traduciendo el codigo del juego a C++ (unos minutos)')
    full = os.path.join(ROOT, 'src', 'recomp', 'full')
    os.makedirs(full, exist_ok=True)
    run([py, os.path.join('tools', 'ppc2cpp.py'), os.path.join('src', 'recomp', 'full', 'game.cpp'), '10000-bb3000'], env=dict(os.environ, SPLIT='48'), quiet=True)

    step(5, 'Traduciendo los programas de sonido y geometria')
    specs = list(SPU_SPECS)
    if pic:
        run([py, os.path.join('tools', 'atrac_image.py'), pic, os.path.join(re_dir, 'spu_atrac_32980.bin')])
        specs.append(ATRAC_SPEC)
    run([py, os.path.join('tools', 'spu2cpp.py'), os.path.join('re', 'EBOOT.elf'), os.path.join('src', 'recomp', 'spu_gen')] + specs, quiet=True)

    step(6, 'Compilando (varios minutos; usa %d hilos)' % a.jobs)
    env = dict(os.environ)
    env['PATH'] = os.path.join(tc, 'bin') + os.pathsep + env.get('PATH', '')
    build = os.path.join(ROOT, 'build-inst')
    run(['cmake', '-S', ROOT, '-B', build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_COMPILER=clang', '-DCMAKE_CXX_COMPILER=clang++',
         '-DCMAKE_PREFIX_PATH=' + os.path.join(tc, 'deps').replace('\\', '/')], env=env, quiet=True)
    run(['cmake', '--build', build, '--target', 'recomp_boot', '-j', str(a.jobs)], env=env)

    step(7, 'Preparando la carpeta del juego')
    out = os.path.abspath(a.out)
    os.makedirs(os.path.join(out, 'juego'), exist_ok=True)
    shutil.copyfile(os.path.join(build, 'recomp_boot.exe'), os.path.join(out, 'Racing Club+.exe'))
    shutil.copyfile(elf, os.path.join(out, 'juego', 'EBOOT.elf'))
    if pic:
        os.makedirs(os.path.join(out, 'dev_flash', 'sys', 'external'), exist_ok=True)
        shutil.copyfile(pic, os.path.join(out, 'dev_flash', 'sys', 'external', 'flashATRAC.pic'))
    if not a.no_copy:
        print('  copiando los datos del juego...', flush=True)
        shutil.copytree(usrdir, os.path.join(out, 'juego', 'USRDIR'), dirs_exist_ok=True, ignore=shutil.ignore_patterns('EBOOT.BIN'))
    # the decrypted executable and the translated code stay out of the installer's folder, so it can be shared;
    # never in a development checkout (.git), where re/ also holds analysis files that cannot be regenerated
    if not a.keep and not os.path.exists(os.path.join(ROOT, '.git')):
        for d in ('re', os.path.join('src', 'recomp', 'full'), os.path.join('src', 'recomp', 'spu_gen'), 'build-inst'):
            shutil.rmtree(os.path.join(ROOT, d), ignore_errors=True)
    if a.idioma:   # the game's language, in the settings file the game reads next to its executable
        ini = os.path.join(out, 'gtiport.ini')
        lines = [l for l in (open(ini, encoding='utf-8').read().splitlines() if os.path.exists(ini) else []) if not l.startswith('idioma=')]
        open(ini, 'w', encoding='utf-8').write('\n'.join(lines + ['idioma=%d' % a.idioma]) + '\n')
    print('\nHecho en %d min. Para jugar: "%s"' % ((time.time() - t0) / 60, os.path.join(out, 'Racing Club+.exe')))
    print('Esa carpeta contiene tu copia del juego: es para tu uso, no la repartas.')


if __name__ == '__main__':
    main()
