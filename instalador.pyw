"""Racing Club+ : graphical installer. Runs instalar.py (which does the real work) and shows its progress.

Started by "Instalar Racing Club+.exe" (src/tools/instalador_launcher.c) with the Python bundled in toolchain/python,
or directly: pythonw instalador.pyw
Two steps only: the user's game files (.pkg and .rap, chosen together and sorted here) and the RPCS3 folder.
The installer's language is also the game's language.
"""
import ctypes, json, os, queue, re, struct, subprocess, sys, threading
import tkinter as tk
from tkinter import filedialog, messagebox

ROOT = os.path.dirname(os.path.abspath(__file__))
VERSION = '2.0'
PREFS = os.path.join(ROOT, 'instalador.json')
LANGS = ['English', 'Français', 'Español', 'Deutsch', 'Italiano']   # same order and numbers (1-5) as the game's menu
TITLE_ID, APP_VER = 'NPEB00060', '01.02'

T = {
 'sub':     ['PC installer', 'Installateur PC', 'Instalador para PC', 'PC-Installation', 'Installazione per PC'],
 'step1':   ['Your game files', 'Les fichiers de votre jeu', 'Los ficheros de tu juego', 'Deine Spieldateien', 'I file del tuo gioco'],
 'pick1':   ['Choose .pkg and .rap files…', 'Choisir les .pkg et .rap…', 'Elegir los .pkg y .rap…', '.pkg- und .rap-Dateien wählen…', 'Scegli i file .pkg e .rap…'],
 'step2':   ['RPCS3 folder', 'Dossier de RPCS3', 'Carpeta de RPCS3', 'RPCS3-Ordner', 'Cartella di RPCS3'],
 'pick2':   ['Choose folder…', 'Choisir le dossier…', 'Elegir carpeta…', 'Ordner wählen…', 'Scegli cartella…'],
 'dl':      ['Download RPCS3 (free)', 'Télécharger RPCS3 (gratuit)', 'Descargar RPCS3 (gratis)', 'RPCS3 herunterladen (kostenlos)', 'Scarica RPCS3 (gratis)'],
 'note':    ['European version NPEB00060 with update 1.02. RPCS3 is only used to decrypt your game. No game file is included.',
             'Version européenne NPEB00060 avec la mise à jour 1.02. RPCS3 sert uniquement à déchiffrer votre jeu. Aucun fichier du jeu inclus.',
             'Versión europea NPEB00060 con la actualización 1.02. RPCS3 solo se usa para descifrar tu juego. No incluye ningún fichero del juego.',
             'Europäische Version NPEB00060 mit Update 1.02. RPCS3 dient nur zum Entschlüsseln. Keine Spieldateien enthalten.',
             'Versione europea NPEB00060 con aggiornamento 1.02. RPCS3 serve solo a decifrare il gioco. Nessun file del gioco incluso.'],
 'install': ['INSTALL', 'INSTALLER', 'INSTALAR', 'INSTALLIEREN', 'INSTALLA'],
 'play':    ['PLAY', 'JOUER', 'JUGAR', 'SPIELEN', 'GIOCA'],
 'where':   ['Install to', 'Installer dans', 'Instalar en', 'Installieren nach', 'Installa in'],
 'change':  ['change', 'modifier', 'cambiar', 'ändern', 'cambia'],
 'details': ['details', 'détails', 'detalles', 'Details', 'dettagli'],
 'game':    ['Game', 'Jeu', 'Juego', 'Spiel', 'Gioco'],
 'update':  ['Update 1.02', 'Mise à jour 1.02', 'Actualización 1.02', 'Update 1.02', 'Aggiornamento 1.02'],
 'pack':    ['Car pack', 'Pack voitures', 'Pack de coches', 'Autopaket', 'Pacchetto auto'],
 'raps':    ['licences', 'licences', 'licencias', 'Lizenzen', 'licenze'],
 'inrpcs3': ['Game already installed in RPCS3 (version %s)', 'Jeu déjà installé dans RPCS3 (version %s)', 'Juego ya instalado en RPCS3 (versión %s)',
             'Spiel bereits in RPCS3 installiert (Version %s)', 'Gioco già installato in RPCS3 (versione %s)'],
 'needpkg': ['Missing: the game and update .pkg files and the .rap licence', 'Manquent : les .pkg du jeu et de la mise à jour et la licence .rap',
             'Faltan: los .pkg del juego y de la actualización y la licencia .rap', 'Fehlen: die .pkg von Spiel und Update und die .rap-Lizenz',
             'Mancano: i .pkg del gioco e dell\'aggiornamento e la licenza .rap'],
 'rpcs3ok': ['RPCS3 found', 'RPCS3 trouvé', 'RPCS3 encontrado', 'RPCS3 gefunden', 'RPCS3 trovato'],
 'norpcs3': ['rpcs3.exe is not in this folder', 'rpcs3.exe n\'est pas dans ce dossier', 'rpcs3.exe no está en esta carpeta',
             'rpcs3.exe ist nicht in diesem Ordner', 'rpcs3.exe non è in questa cartella'],
 'pkghelp': ['RPCS3 will open for each package: press “Install” there. It closes by itself.',
             'RPCS3 s\'ouvrira pour chaque paquet : appuyez sur « Install ». Il se ferme tout seul.',
             'RPCS3 se abrirá con cada paquete: pulsa «Install». Se cierra solo.',
             'RPCS3 öffnet sich für jedes Paket: „Install“ drücken. Es schließt sich selbst.',
             'RPCS3 si aprirà per ogni pacchetto: premi «Install». Si chiude da solo.'],
 'pip':     ['Preparing…', 'Préparation…', 'Preparando…', 'Vorbereitung…', 'Preparazione…'],
 'steps':   [['Checking the game', 'Decrypting', 'Analysing the code', 'Translating the code', 'Translating sound and geometry', 'Compiling', 'Finishing'],
             ['Vérification du jeu', 'Déchiffrement', 'Analyse du code', 'Traduction du code', 'Traduction son et géométrie', 'Compilation', 'Finalisation'],
             ['Comprobando el juego', 'Descifrando', 'Analizando el código', 'Traduciendo el código', 'Traduciendo sonido y geometría', 'Compilando', 'Terminando'],
             ['Spiel wird geprüft', 'Entschlüsseln', 'Code wird analysiert', 'Code wird übersetzt', 'Sound und Geometrie', 'Kompilieren', 'Abschluss'],
             ['Verifica del gioco', 'Decifratura', 'Analisi del codice', 'Traduzione del codice', 'Audio e geometria', 'Compilazione', 'Conclusione']],
 'done':    ['Ready! Racing Club+ is installed (desktop shortcut created).', 'Prêt ! Racing Club+ est installé (raccourci sur le bureau).',
             '¡Listo! Racing Club+ está instalado (acceso directo en el escritorio).', 'Fertig! Racing Club+ ist installiert (Desktop-Verknüpfung).',
             'Fatto! Racing Club+ è installato (collegamento sul desktop).'],
 'failed':  ['The installation failed: see the details.', 'L\'installation a échoué : voir les détails.', 'La instalación ha fallado: mira los detalles.',
             'Die Installation ist fehlgeschlagen: siehe Details.', 'Installazione non riuscita: vedi i dettagli.'],
 'busy':    ['The installation is still running. Stop it and close?', 'L\'installation est en cours. L\'arrêter et fermer ?',
             'La instalación sigue en marcha. ¿Detenerla y cerrar?', 'Die Installation läuft noch. Abbrechen und schließen?',
             'L\'installazione è in corso. Interromperla e chiudere?'],
}

WHITE, INK, GREY, LINE, SOFT = '#FFFFFF', '#14213D', '#6B7280', '#E2E8F0', '#F1F6FF'
BLUE_A, BLUE_B = (30, 144, 255), (0, 62, 190)          # gradient: bright sky blue -> deep blue (the game's colours)
OK, WARN, ERR = '#138A43', '#B45309', '#C62828'
W = 600


def hexrgb(c): return '#%02x%02x%02x' % c
def mix(a, b, t): return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def gradient(canvas, x0, y0, x1, y1, a=BLUE_A, b=BLUE_B, vertical=False, tag='grad'):
    n = (y1 - y0) if vertical else (x1 - x0)
    for i in range(max(n, 1)):
        c = hexrgb(mix(a, b, i / max(n - 1, 1)))
        if vertical: canvas.create_line(x0, y0 + i, x1, y0 + i, fill=c, tags=tag)
        else: canvas.create_line(x0 + i, y0, x0 + i, y1, fill=c, tags=tag)


def windows_lang():
    try: return {0x09: 0, 0x0c: 1, 0x0a: 2, 0x07: 3, 0x10: 4}.get(ctypes.windll.kernel32.GetUserDefaultUILanguage() & 0x3ff, 0)
    except Exception: return 0


def load_prefs():
    try: return json.load(open(PREFS, encoding='utf-8'))
    except Exception: return {}


def sfo_version(path):
    try:
        d = open(path, 'rb').read()
        keys, data, n = struct.unpack_from('<III', d, 8)
        for i in range(n):
            ko, fmt, ln, mx, do = struct.unpack_from('<HHIII', d, 20 + 16 * i)
            if d[keys + ko: d.index(b'\0', keys + ko)] == b'APP_VER': return d[data + do: data + do + ln].rstrip(b'\0').decode()
    except Exception: pass
    return '?'


def sort_files(paths):
    """.pkg files -> (game, update, car pack) by content id and size; .rap files kept apart."""
    pkgs, raps = [], []
    for p in paths:
        if p.lower().endswith('.rap'): raps.append(p); continue
        try:
            with open(p, 'rb') as f: h = f.read(0x60)
            if h[:4] != b'\x7fPKG': continue
            pkgs.append((h[0x30:0x54].split(b'\0')[0].decode('ascii', 'replace'), os.path.getsize(p), p))
        except OSError: pass
    game = sorted([x for x in pkgs if 'CARPACK' not in x[0]], key=lambda x: -x[1])
    pack = [x[2] for x in pkgs if 'CARPACK' in x[0]]
    return (game[0][2] if game else ''), (game[1][2] if len(game) > 1 else ''), (pack[0] if pack else ''), raps


class App:
    def __init__(self, root):
        self.root = root
        p = load_prefs()
        self.li = p.get('lang', windows_lang())
        self.rpcs3 = p.get('rpcs3', '')
        self.files = p.get('files', [])
        self.out = p.get('out', os.path.join(ROOT, 'Racing Club+'))
        self.proc, self.q, self.step, self.ok, self.log_shown = None, queue.Queue(), 0, False, False
        root.title('Racing Club+')
        root.configure(bg=WHITE)
        root.resizable(False, False)
        ico = os.path.join(ROOT, 'assets', 'racing_club.ico')
        if os.path.exists(ico):
            try: root.iconbitmap(ico)
            except tk.TclError: pass
        self.build()
        root.protocol('WM_DELETE_WINDOW', self.close)
        root.after(100, self.poll)

    def t(self, k): return T[k][self.li]

    # ------------------------------------------------------------ layout
    def build(self):
        for w in self.root.winfo_children(): w.destroy()
        r = self.root
        head = tk.Canvas(r, width=W, height=132, highlightthickness=0, bd=0)
        head.pack()
        gradient(head, 0, 0, W, 132)
        logo = os.path.join(ROOT, 'assets', 'racing_club_logo.png')
        if os.path.exists(logo):
            self.logo = tk.PhotoImage(file=logo)
            head.create_image(26, 66, image=self.logo, anchor='w')
        head.create_text(146, 52, text='Racing Club+', anchor='w', fill='white', font=('Arial Black', 26))
        head.create_text(149, 90, text='%s  ·  v%s' % (self.t('sub'), VERSION), anchor='w', fill='#DCEBFF', font=('Segoe UI', 11))
        lang = tk.Label(head, text='🌐 ' + LANGS[self.li] + ' ▾', bg=hexrgb(BLUE_B), fg='white', font=('Segoe UI', 10), cursor='hand2')
        head.create_window(W - 18, 18, window=lang, anchor='ne')
        lang.bind('<Button-1>', self.lang_menu)

        body = tk.Frame(r, bg=WHITE)
        body.pack(fill='x', padx=28, pady=(18, 0))
        self.section(body, '1', self.t('step1'))
        row = tk.Frame(body, bg=WHITE); row.pack(fill='x', pady=(6, 0))
        self.flat_button(row, self.t('pick1'), self.pick_files).pack(side='left')
        self.files_lbl = tk.Label(body, text='', bg=WHITE, font=('Segoe UI', 9), anchor='w', justify='left', wraplength=W - 60)
        self.files_lbl.pack(fill='x', pady=(6, 0))

        self.section(body, '2', self.t('step2'), top=16)
        row = tk.Frame(body, bg=WHITE); row.pack(fill='x', pady=(6, 0))
        self.flat_button(row, self.t('pick2'), self.pick_rpcs3).pack(side='left')
        dl = tk.Label(row, text=self.t('dl') + ' ↗', bg=WHITE, fg=hexrgb(BLUE_B), font=('Segoe UI', 9, 'underline'), cursor='hand2')
        dl.pack(side='right')
        dl.bind('<Button-1>', lambda e: os.startfile('https://rpcs3.net/download'))
        self.rpcs3_lbl = tk.Label(body, text='', bg=WHITE, font=('Segoe UI', 9), anchor='w', justify='left', wraplength=W - 60)
        self.rpcs3_lbl.pack(fill='x', pady=(6, 0))

        tk.Label(body, text=self.t('note'), bg=WHITE, fg=GREY, font=('Segoe UI', 8), justify='left', wraplength=W - 60).pack(anchor='w', pady=(16, 0))

        self.btn = tk.Canvas(r, width=W - 56, height=54, highlightthickness=0, bd=0, bg=WHITE, cursor='hand2')
        self.btn.pack(pady=(14, 0))
        self.btn.bind('<Button-1>', lambda e: self.play() if self.ok else self.start())
        self.prog = tk.Canvas(r, width=W - 56, height=6, highlightthickness=0, bd=0, bg=SOFT)
        self.prog.pack(pady=(10, 0))
        self.msg = tk.Label(r, text='', bg=WHITE, fg=GREY, font=('Segoe UI', 9), wraplength=W - 56, justify='center')
        self.msg.pack(pady=(6, 0))

        foot = tk.Frame(r, bg=WHITE)
        foot.pack(fill='x', padx=28, pady=(6, 14))
        self.where_lbl = tk.Label(foot, text='', bg=WHITE, fg=GREY, font=('Segoe UI', 8), anchor='w')
        self.where_lbl.pack(side='left')
        ch = tk.Label(foot, text=self.t('change'), bg=WHITE, fg=hexrgb(BLUE_B), font=('Segoe UI', 8, 'underline'), cursor='hand2')
        ch.pack(side='left', padx=(6, 0))
        ch.bind('<Button-1>', lambda e: self.pick_out())
        det = tk.Label(foot, text=self.t('details'), bg=WHITE, fg=GREY, font=('Segoe UI', 8, 'underline'), cursor='hand2')
        det.pack(side='right')
        det.bind('<Button-1>', lambda e: self.toggle_log())
        self.logbox = tk.Text(r, height=8, font=('Consolas', 8), bg=SOFT, fg=INK, relief='flat', wrap='none')
        self.refresh()

    def section(self, parent, num, text, top=0):
        f = tk.Frame(parent, bg=WHITE); f.pack(fill='x', pady=(top, 0))
        c = tk.Canvas(f, width=24, height=24, highlightthickness=0, bd=0, bg=WHITE)
        c.create_oval(1, 1, 23, 23, fill=hexrgb(mix(BLUE_A, BLUE_B, 0.5)), outline='')
        c.create_text(12, 12, text=num, fill='white', font=('Segoe UI Semibold', 10))
        c.pack(side='left')
        tk.Label(f, text=text, bg=WHITE, fg=INK, font=('Segoe UI Semibold', 12)).pack(side='left', padx=(8, 0))

    def flat_button(self, parent, text, cmd):
        return tk.Button(parent, text=text, command=cmd, font=('Segoe UI', 10), relief='flat', bd=0, cursor='hand2', padx=16, pady=6,
                         bg=SOFT, fg=INK, activebackground=LINE, activeforeground=INK)

    def draw_button(self, text, enabled):
        c = self.btn; c.delete('all')
        w, h = int(c['width']), int(c['height'])
        if enabled: gradient(c, 0, 0, w, h)
        else: c.create_rectangle(0, 0, w, h, fill=LINE, outline='')
        c.create_text(w // 2, h // 2, text=text, fill='white' if enabled else GREY, font=('Arial Black', 15))
        c.configure(cursor='hand2' if enabled else 'arrow')

    def set_progress(self, frac, text, color=GREY):
        c = self.prog; c.delete('all')
        w, h = int(c['width']), int(c['height'])
        if frac > 0: gradient(c, 0, 0, max(2, int(w * min(frac, 1))), h)
        self.msg.config(text=text, fg=color)

    def lang_menu(self, e):
        m = tk.Menu(self.root, tearoff=0)
        for i, n in enumerate(LANGS): m.add_command(label=n, command=lambda i=i: self.set_lang(i))
        m.tk_popup(e.x_root, e.y_root)

    def set_lang(self, i):
        self.li = i; self.save(); self.build()

    def toggle_log(self):
        self.log_shown = not self.log_shown
        if self.log_shown: self.logbox.pack(fill='both', padx=28, pady=(0, 14)); self.logbox.see('end')
        else: self.logbox.pack_forget()

    # ------------------------------------------------------------ state
    def sorted(self): return sort_files(self.files)

    def game_in_rpcs3(self):
        sfo = os.path.join(self.rpcs3, 'dev_hdd0', 'game', TITLE_ID, 'PARAM.SFO')
        return sfo_version(sfo) if self.rpcs3 and os.path.exists(sfo) else None

    def ready(self):
        if not os.path.exists(os.path.join(self.rpcs3, 'rpcs3.exe')): return False
        game, upd, pack, raps = self.sorted()
        return bool(game and upd and raps) or self.game_in_rpcs3() == APP_VER

    def refresh(self):
        game, upd, pack, raps = self.sorted()
        if self.files:
            parts = [('✓ ' if game else '✗ ') + self.t('game'), ('✓ ' if upd else '✗ ') + self.t('update'),
                     ('✓ ' if pack else '– ') + self.t('pack'), '%d %s' % (len(raps), self.t('raps'))]
            good = game and upd and raps
            self.files_lbl.config(text='   '.join(parts) + ('' if good else '\n' + self.t('needpkg')), fg=OK if good else WARN)
        else:
            v = self.game_in_rpcs3()
            self.files_lbl.config(text=('✓ ' + self.t('inrpcs3') % v) if v else '', fg=OK if v == APP_VER else WARN)
        if not self.rpcs3: self.rpcs3_lbl.config(text='')
        elif os.path.exists(os.path.join(self.rpcs3, 'rpcs3.exe')): self.rpcs3_lbl.config(text='✓ %s  —  %s' % (self.t('rpcs3ok'), self.rpcs3), fg=OK)
        else: self.rpcs3_lbl.config(text='✗ ' + self.t('norpcs3'), fg=ERR)
        o = self.out if len(self.out) <= 52 else self.out[:22] + '…' + self.out[-29:]   # keeps the window at its width
        self.where_lbl.config(text='%s: %s' % (self.t('where'), o))
        if self.ok: self.draw_button(self.t('play'), True)
        elif not self.proc: self.draw_button(self.t('install'), self.ready())
        if not self.proc and not self.ok:
            self.set_progress(0, self.t('pkghelp') if (self.ready() and game) else '')

    def pick_files(self):
        fs = filedialog.askopenfilenames(title=self.t('pick1'), filetypes=[('PS3', '*.pkg *.rap'), ('*', '*.*')],
                                         initialdir=os.path.dirname(self.files[0]) if self.files else None)
        if fs: self.files = [os.path.normpath(f) for f in fs]; self.save(); self.refresh()

    def pick_rpcs3(self):
        d = filedialog.askdirectory(title=self.t('step2'), initialdir=self.rpcs3 or None)
        if d: self.rpcs3 = os.path.normpath(d); self.save(); self.refresh()

    def pick_out(self):
        d = filedialog.askdirectory(title=self.t('where'), initialdir=os.path.dirname(self.out) or None)
        if d: self.out = os.path.join(os.path.normpath(d), 'Racing Club+'); self.save(); self.refresh()

    def save(self):
        try: json.dump({'lang': self.li, 'rpcs3': self.rpcs3, 'files': self.files, 'out': self.out}, open(PREFS, 'w', encoding='utf-8'), indent=1)
        except Exception: pass

    # ------------------------------------------------------------ run
    def log(self, line):
        self.logbox.insert('end', line + '\n')
        if self.log_shown: self.logbox.see('end')

    def start(self):
        if self.proc or not self.ready(): return
        self.save(); self.ok = False; self.step = 0
        self.logbox.delete('1.0', 'end')
        self.draw_button(self.t('install'), False)
        threading.Thread(target=self.worker, daemon=True).start()

    def worker(self):
        py = sys.executable
        if os.path.basename(py).lower() == 'pythonw.exe':
            alt = os.path.join(os.path.dirname(py), 'python.exe')
            if os.path.exists(alt): py = alt
        flags = 0x08000000   # CREATE_NO_WINDOW
        if subprocess.run([py, '-c', 'import capstone'], creationflags=flags, capture_output=True).returncode != 0:
            self.q.put(('msg', self.t('pip')))
            r = subprocess.run([py, '-m', 'pip', 'install', 'capstone==5.0.7'], creationflags=flags, capture_output=True, text=True)
            self.q.put(('log', r.stdout + r.stderr))
        game, upd, pack, raps = self.sorted()
        cmd = [py, '-u', os.path.join(ROOT, 'instalar.py'), '--rpcs3', self.rpcs3, '--out', self.out, '--idioma', str(self.li + 1)]
        if game and upd: cmd += ['--pkg'] + [p for p in (game, upd, pack) if p] + ['--rap'] + raps
        self.proc = subprocess.Popen(cmd, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, creationflags=flags, text=True, encoding='utf-8', errors='replace')
        for line in self.proc.stdout: self.q.put(('line', line.rstrip()))
        rc = self.proc.wait()
        self.proc = None
        self.q.put(('end', rc))

    def poll(self):
        try:
            while True:
                kind, v = self.q.get_nowait()
                if kind == 'msg': self.set_progress(0.01, v)
                elif kind == 'log': self.log(v)
                elif kind == 'line': self.on_line(v)
                elif kind == 'end': self.finish(v)
        except queue.Empty: pass
        self.root.after(100, self.poll)

    def on_line(self, line):
        self.log(line)
        steps = T['steps'][self.li]
        m = re.match(r'^\[(\d+)/7\] ', line)
        if m and not re.match(r'^\[\d+/7\] (Building|Linking)', line):
            self.step = int(m.group(1))
            self.set_progress((self.step - 1) / 7, '%d/7 · %s…' % (self.step, steps[self.step - 1]))
            return
        m = re.match(r'^\[(\d+)/(\d+)\] (Building|Linking)', line)
        if m and self.step == 6:
            k, n = int(m.group(1)), max(int(m.group(2)), 1)
            self.set_progress((5 + k / n) / 7, '6/7 · %s… %d %%' % (steps[5], 100 * k // n))
        elif 'confirma la instalacion' in line:
            self.set_progress((self.step - 1) / 7 + 0.02, self.t('pkghelp'), INK)

    def finish(self, rc):
        if rc == 0:
            self.ok = True
            self.shortcut()
            self.set_progress(1, self.t('done'), OK)
            self.draw_button(self.t('play'), True)
        else:
            self.set_progress(max(self.step - 1, 0) / 7, self.t('failed'), ERR)
            self.draw_button(self.t('install'), self.ready())
            if not self.log_shown: self.toggle_log()

    def exe(self): return os.path.join(self.out, 'Racing Club+.exe')

    def shortcut(self):
        q = lambda s: s.replace("'", "''")
        ps = ("$d=[Environment]::GetFolderPath('Desktop');$s=(New-Object -ComObject WScript.Shell).CreateShortcut((Join-Path $d 'Racing Club+.lnk'));"
              "$s.TargetPath='%s';$s.WorkingDirectory='%s';$s.IconLocation='%s,0';$s.Save()") % (q(self.exe()), q(self.out), q(self.exe()))
        subprocess.run(['powershell', '-NoProfile', '-Command', ps], creationflags=0x08000000, capture_output=True)

    def play(self):
        if os.path.exists(self.exe()):
            subprocess.Popen([self.exe()], cwd=self.out)
            self.root.destroy()

    def close(self):
        if self.proc:
            if not messagebox.askyesno('Racing Club+', self.t('busy')): return
            try: self.proc.kill()
            except Exception: pass
        self.save()
        self.root.destroy()


if __name__ == '__main__':
    try: ctypes.windll.shcore.SetProcessDpiAwareness(1)
    except Exception: pass
    root = tk.Tk()
    if os.environ.get('INSTALADOR_TOP'): root.attributes('-topmost', True)   # tests started from a background process
    App(root)
    root.mainloop()
