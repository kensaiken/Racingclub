"""Racing Club+ : graphical installer. Runs instalar.py (which does the real work) and shows its progress.

Started by "Instalar Racing Club+.exe" (src/tools/instalador_launcher.c) with the Python bundled in toolchain/python,
or directly: pythonw instalador.pyw
Four sections, each saying which of the user's files go there: the game (.pkg + .rap), update 1.02 (.pkg), the car pack
(optional, .pkg + .rap) and the RPCS3 folder. The installer's language is also the game's language.
"""
import ctypes, json, os, queue, re, struct, subprocess, sys, threading
import tkinter as tk
from tkinter import filedialog, messagebox

ROOT = os.path.dirname(os.path.abspath(__file__))
VERSION = '2.1'
PREFS = os.path.join(ROOT, 'instalador.json')
LANGS = ['English', 'Français', 'Español', 'Deutsch', 'Italiano']   # same order and numbers (1-5) as the game's menu
TITLE_ID, APP_VER = 'NPEB00060', '01.02'

T = {
 'sub':     ['PC installer', 'Installateur PC', 'Instalador para PC', 'PC-Installation', 'Installazione per PC'],
 's_game':  ['Game', 'Jeu', 'Juego', 'Spiel', 'Gioco'],
 'h_game':  ['The game\'s .pkg and its .rap licence', 'Le .pkg du jeu et sa licence .rap', 'El .pkg del juego y su licencia .rap',
             'Die .pkg des Spiels und ihre .rap-Lizenz', 'Il .pkg del gioco e la sua licenza .rap'],
 's_upd':   ['Update 1.02', 'Mise à jour 1.02', 'Actualización 1.02', 'Update 1.02', 'Aggiornamento 1.02'],
 'h_upd':   ['The update\'s .pkg (it has no licence)', 'Le .pkg de la mise à jour (sans licence)', 'El .pkg de la actualización (no lleva licencia)',
             'Die .pkg des Updates (ohne Lizenz)', 'Il .pkg dell\'aggiornamento (senza licenza)'],
 's_pack':  ['Car pack (optional)', 'Pack de voitures (facultatif)', 'Pack de coches (opcional)', 'Autopaket (optional)', 'Pacchetto auto (opzionale)'],
 'h_pack':  ['The DLC\'s .pkg and its .rap licence', 'Le .pkg du DLC et sa licence .rap', 'El .pkg del DLC y su licencia .rap',
             'Die .pkg des DLC und ihre .rap-Lizenz', 'Il .pkg del DLC e la sua licenza .rap'],
 's_rpcs3': ['RPCS3', 'RPCS3', 'RPCS3', 'RPCS3', 'RPCS3'],
 'h_rpcs3': ['The folder where rpcs3.exe is. It is only used to decrypt your game.', 'Le dossier de rpcs3.exe. Il sert uniquement à déchiffrer votre jeu.',
             'La carpeta donde está rpcs3.exe. Solo se usa para descifrar tu juego.', 'Der Ordner mit rpcs3.exe. Nur zum Entschlüsseln.',
             'La cartella di rpcs3.exe. Serve solo a decifrare il gioco.'],
 'pickf':   ['Choose files…', 'Choisir…', 'Elegir ficheros…', 'Dateien wählen…', 'Scegli file…'],
 'pick1':   ['Choose file…', 'Choisir…', 'Elegir fichero…', 'Datei wählen…', 'Scegli file…'],
 'pickd':   ['Choose folder…', 'Choisir…', 'Elegir carpeta…', 'Ordner wählen…', 'Scegli cartella…'],
 'dl':      ['Download RPCS3 (free)', 'Télécharger RPCS3 (gratuit)', 'Descargar RPCS3 (gratis)', 'RPCS3 herunterladen (kostenlos)', 'Scarica RPCS3 (gratis)'],
 'note':    ['European version NPEB00060 with update 1.02. No game file is included in this installer.',
             'Version européenne NPEB00060 avec la mise à jour 1.02. Aucun fichier du jeu n\'est inclus.',
             'Versión europea NPEB00060 con la actualización 1.02. Este instalador no incluye ningún fichero del juego.',
             'Europäische Version NPEB00060 mit Update 1.02. Keine Spieldateien enthalten.',
             'Versione europea NPEB00060 con aggiornamento 1.02. Nessun file del gioco incluso.'],
 'install': ['INSTALL', 'INSTALLER', 'INSTALAR', 'INSTALLIEREN', 'INSTALLA'],
 'play':    ['PLAY', 'JOUER', 'JUGAR', 'SPIELEN', 'GIOCA'],
 'where':   ['Install to', 'Installer dans', 'Instalar en', 'Installieren nach', 'Installa in'],
 'change':  ['change', 'modifier', 'cambiar', 'ändern', 'cambia'],
 'details': ['details', 'détails', 'detalles', 'Details', 'dettagli'],
 'patreon': ['Support on Patreon', 'Soutenir sur Patreon', 'Apoyar en Patreon', 'Auf Patreon unterstützen', 'Sostieni su Patreon'],
 'twitch':  ['Twitch channel', 'Chaîne Twitch', 'Canal de Twitch', 'Twitch-Kanal', 'Canale Twitch'],
 'github':  ['GitHub repository', 'Dépôt GitHub', 'Repositorio GitHub', 'GitHub-Repository', 'Repository GitHub'],
 'licence': ['licence', 'licence', 'licencia', 'Lizenz', 'licenza'],
 'nolic':   ['the .rap licence is missing', 'il manque la licence .rap', 'falta la licencia .rap', 'die .rap-Lizenz fehlt', 'manca la licenza .rap'],
 'nopkg':   ['the .pkg is missing', 'il manque le .pkg', 'falta el .pkg', 'die .pkg fehlt', 'manca il .pkg'],
 'wrong':   ['this is not the right file for this section', 'ce fichier ne va pas ici', 'este fichero no es el de este apartado',
             'diese Datei gehört nicht hierher', 'questo file non va qui'],
 'inrpcs3': ['Not needed: the game is already installed in RPCS3 (version %s)', 'Inutile : le jeu est déjà installé dans RPCS3 (version %s)',
             'No hace falta: el juego ya está instalado en RPCS3 (versión %s)', 'Nicht nötig: das Spiel ist schon in RPCS3 installiert (Version %s)',
             'Non serve: il gioco è già installato in RPCS3 (versione %s)'],
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


def gradient(canvas, x0, y0, x1, y1, a=BLUE_A, b=BLUE_B):
    n = x1 - x0
    for i in range(max(n, 1)):
        canvas.create_line(x0 + i, y0, x0 + i, y1, fill=hexrgb(mix(a, b, i / max(n - 1, 1))))


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


def pkg_id(p):
    """content id of a .pkg ('' if it is not a PS3 package)"""
    try:
        with open(p, 'rb') as f: h = f.read(0x60)
        return h[0x30:0x54].split(b'\0')[0].decode('ascii', 'replace') if h[:4] == b'\x7fPKG' else ''
    except OSError: return ''


# what each section accepts: the content id of its .pkg and the name of its licence must contain this
KIND = {'game': 'NPEB00060_00-GTICLUB1', 'update': 'NPEB00060_00-GTICLUB1', 'pack': 'CARPACK'}


class App:
    def __init__(self, root):
        self.root = root
        p = load_prefs()
        self.li = p.get('lang', windows_lang())
        self.rpcs3 = p.get('rpcs3', '')
        self.sel = {k: list(p.get('sel', {}).get(k, [])) for k in ('game', 'update', 'pack')}
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
        head = tk.Canvas(r, width=W, height=120, highlightthickness=0, bd=0)
        head.pack()
        gradient(head, 0, 0, W, 120)
        logo = os.path.join(ROOT, 'assets', 'racing_club_logo.png')
        if os.path.exists(logo):
            self.logo = tk.PhotoImage(file=logo)
            head.create_image(22, 60, image=self.logo, anchor='w')
        head.create_text(142, 48, text='Racing Club+', anchor='w', fill='white', font=('Arial Black', 26))
        head.create_text(145, 84, text='%s  ·  v%s' % (self.t('sub'), VERSION), anchor='w', fill='#DCEBFF', font=('Segoe UI', 11))
        lang = tk.Label(head, text='🌐 ' + LANGS[self.li] + ' ▾', bg=hexrgb(BLUE_B), fg='white', font=('Segoe UI', 10), cursor='hand2')
        head.create_window(W - 16, 16, window=lang, anchor='ne')
        lang.bind('<Button-1>', self.lang_menu)

        body = tk.Frame(r, bg=WHITE)
        body.pack(fill='x', padx=26, pady=(12, 0))
        self.status = {}
        for n, key, files in (('1', 'game', True), ('2', 'update', True), ('3', 'pack', True), ('4', 'rpcs3', False)):
            self.section(body, n, key, files)
        tk.Label(body, text=self.t('note'), bg=WHITE, fg=GREY, font=('Segoe UI', 8), justify='left', wraplength=W - 56).pack(anchor='w', pady=(10, 0))

        self.btn = tk.Canvas(r, width=W - 52, height=52, highlightthickness=0, bd=0, bg=WHITE, cursor='hand2')
        self.btn.pack(pady=(12, 0))
        self.btn.bind('<Button-1>', lambda e: self.play() if self.ok else self.start())
        self.prog = tk.Canvas(r, width=W - 52, height=6, highlightthickness=0, bd=0, bg=SOFT)
        self.prog.pack(pady=(10, 0))
        self.msg = tk.Label(r, text='', bg=WHITE, fg=GREY, font=('Segoe UI', 9), wraplength=W - 52, justify='center')
        self.msg.pack(pady=(6, 0))
        foot = tk.Frame(r, bg=WHITE)
        foot.pack(fill='x', padx=26, pady=(4, 12))
        self.where_lbl = tk.Label(foot, text='', bg=WHITE, fg=GREY, font=('Segoe UI', 8), anchor='w')
        self.where_lbl.pack(side='left')
        self.link(foot, self.t('change'), self.pick_out, hexrgb(BLUE_B)).pack(side='left', padx=(6, 0))
        self.link(foot, self.t('details'), self.toggle_log, GREY).pack(side='right')
        social = tk.Frame(r, bg=WHITE)
        social.pack(fill='x', padx=26, pady=(0, 12))
        for i, (key, url, bg, act) in enumerate((
                ('patreon', 'https://www.patreon.com/ilesraider/membership', '#FF424D', '#E03A44'),
                ('twitch', 'https://www.twitch.tv/ilesraider', '#9146FF', '#7A35E0'),
                ('github', 'https://github.com/kensaiken/Racingclub', '#24292F', '#3A4048'))):
            social.columnconfigure(i, weight=1, uniform='s')
            tk.Button(social, text=self.t(key), command=lambda u=url: os.startfile(u), font=('Segoe UI Semibold', 9),
                      relief='flat', bd=0, cursor='hand2', pady=6, bg=bg, fg='white',
                      activebackground=act, activeforeground='white').grid(row=0, column=i, sticky='ew', padx=(0 if i == 0 else 4, 0))
        self.logbox = tk.Text(r, height=8, font=('Consolas', 8), bg=SOFT, fg=INK, relief='flat', wrap='none')
        self.refresh()

    def link(self, parent, text, cmd, color, size=8):
        l = tk.Label(parent, text=text, bg=WHITE, fg=color, font=('Segoe UI', size, 'underline'), cursor='hand2')
        l.bind('<Button-1>', lambda e: cmd())
        return l

    def section(self, parent, num, key, files):
        box = tk.Frame(parent, bg=WHITE); box.pack(fill='x', pady=(8, 0))
        top = tk.Frame(box, bg=WHITE); top.pack(fill='x')
        c = tk.Canvas(top, width=22, height=22, highlightthickness=0, bd=0, bg=WHITE)
        c.create_oval(1, 1, 21, 21, fill=hexrgb(mix(BLUE_A, BLUE_B, 0.5)), outline='')
        c.create_text(11, 11, text=num, fill='white', font=('Segoe UI Semibold', 9))
        c.pack(side='left')
        names = {'game': 's_game', 'update': 's_upd', 'pack': 's_pack', 'rpcs3': 's_rpcs3'}
        hints = {'game': 'h_game', 'update': 'h_upd', 'pack': 'h_pack', 'rpcs3': 'h_rpcs3'}
        tk.Label(top, text=self.t(names[key]), bg=WHITE, fg=INK, font=('Segoe UI Semibold', 11)).pack(side='left', padx=(8, 0))
        if key == 'rpcs3': self.link(top, self.t('dl') + ' ↗', lambda: os.startfile('https://rpcs3.net/download'), hexrgb(BLUE_B), 9).pack(side='right')
        tk.Label(box, text=self.t(hints[key]), bg=WHITE, fg=GREY, font=('Segoe UI', 9), anchor='w').pack(fill='x', padx=(30, 0))
        row = tk.Frame(box, bg=WHITE); row.pack(fill='x', padx=(30, 0), pady=(3, 0))
        label = self.t('pickd') if key == 'rpcs3' else (self.t('pick1') if key == 'update' else self.t('pickf'))
        tk.Button(row, text=label, command=lambda: self.pick(key), font=('Segoe UI', 9), relief='flat', bd=0, cursor='hand2', padx=12, pady=4,
                  bg=SOFT, fg=INK, activebackground=LINE, activeforeground=INK).pack(side='left')
        st = tk.Label(row, text='', bg=WHITE, font=('Segoe UI', 9), anchor='w', justify='left')
        st.pack(side='left', padx=(10, 0))
        self.status[key] = st

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
        if self.log_shown: self.logbox.pack(fill='both', padx=26, pady=(0, 12)); self.logbox.see('end')
        else: self.logbox.pack_forget()

    # ------------------------------------------------------------ state
    def parts(self, key):
        """(pkg, rap, problem) of a section"""
        fs = self.sel[key]
        pkgs = [f for f in fs if f.lower().endswith('.pkg')]
        raps = [f for f in fs if f.lower().endswith('.rap')]
        pkg = pkgs[0] if pkgs else ''
        rap = raps[0] if raps else ''
        bad = (pkg and KIND[key] not in pkg_id(pkg)) or (rap and KIND[key] not in os.path.basename(rap).upper())
        if key in ('game', 'update') and pkg and not bad:   # game and update share their id: the game's .pkg is the big one
            other = self.sel['update' if key == 'game' else 'game']
            op = next((f for f in other if f.lower().endswith('.pkg')), '')
            if op and os.path.exists(op) and os.path.exists(pkg) and KIND['game'] in pkg_id(op):
                big = os.path.getsize(pkg) > os.path.getsize(op)
                bad = big if key == 'update' else not big
        return pkg, rap, bad

    def game_in_rpcs3(self):
        sfo = os.path.join(self.rpcs3, 'dev_hdd0', 'game', TITLE_ID, 'PARAM.SFO')
        return sfo_version(sfo) if self.rpcs3 and os.path.exists(sfo) else None

    def section_ok(self, key):
        pkg, rap, bad = self.parts(key)
        if key == 'game': return bool(pkg and rap and not bad)
        if key == 'update': return bool(pkg and not bad)
        return not bad and (not pkg or bool(rap))   # the car pack is optional, but with its licence

    def ready(self):
        if not os.path.exists(os.path.join(self.rpcs3, 'rpcs3.exe')): return False
        if self.game_in_rpcs3() == APP_VER and not any(self.sel.values()): return True
        return all(self.section_ok(k) for k in ('game', 'update', 'pack'))

    def refresh(self):
        inst = self.game_in_rpcs3() if not any(self.sel.values()) else None
        for key in ('game', 'update', 'pack'):
            pkg, rap, bad = self.parts(key)
            if inst and key != 'pack':
                txt, col = self.t('inrpcs3') % inst, OK if inst == APP_VER else WARN
            elif not pkg and not rap:
                txt, col = '', GREY
            elif bad:
                txt, col = '✗ ' + self.t('wrong'), ERR
            else:
                bits = ['✓ ' + os.path.basename(pkg)] if pkg else ['✗ ' + self.t('nopkg')]
                if key != 'update': bits.append(('✓ ' + self.t('licence')) if rap else ('✗ ' + self.t('nolic')))
                txt, col = '   '.join(bits), OK if self.section_ok(key) and pkg else WARN
            self.status[key].config(text=txt, fg=col)
        if not self.rpcs3: self.status['rpcs3'].config(text='')
        elif os.path.exists(os.path.join(self.rpcs3, 'rpcs3.exe')): self.status['rpcs3'].config(text='✓ ' + self.t('rpcs3ok'), fg=OK)
        else: self.status['rpcs3'].config(text='✗ ' + self.t('norpcs3'), fg=ERR)
        o = self.out if len(self.out) <= 52 else self.out[:22] + '…' + self.out[-29:]   # keeps the window at its width
        self.where_lbl.config(text='%s: %s' % (self.t('where'), o))
        if self.ok: self.draw_button(self.t('play'), True)
        elif not self.proc: self.draw_button(self.t('install'), self.ready())
        if not self.proc and not self.ok:
            self.set_progress(0, self.t('pkghelp') if (self.ready() and any(self.sel.values())) else '')

    def pick(self, key):
        if key == 'rpcs3':
            d = filedialog.askdirectory(title='RPCS3', initialdir=self.rpcs3 or None)
            if d: self.rpcs3 = os.path.normpath(d)
        else:
            start = next((os.path.dirname(f) for v in self.sel.values() for f in v), None)
            types = [('PS3 .pkg', '*.pkg')] if key == 'update' else [('PS3 .pkg / .rap', '*.pkg *.rap')]
            fs = filedialog.askopenfilenames(title=self.t({'game': 's_game', 'update': 's_upd', 'pack': 's_pack'}[key]), filetypes=types + [('*', '*.*')], initialdir=start)
            if fs: self.sel[key] = [os.path.normpath(f) for f in fs]
        self.save(); self.refresh()

    def pick_out(self):
        d = filedialog.askdirectory(title=self.t('where'), initialdir=os.path.dirname(self.out) or None)
        if d: self.out = os.path.join(os.path.normpath(d), 'Racing Club+'); self.save(); self.refresh()

    def save(self):
        try: json.dump({'lang': self.li, 'rpcs3': self.rpcs3, 'sel': self.sel, 'out': self.out}, open(PREFS, 'w', encoding='utf-8'), indent=1)
        except Exception: pass

    # ------------------------------------------------------------ run
    def log(self, line):
        try:   # instalacion.log next to the installer, to attach to a problem report
            with open(os.path.join(ROOT, 'instalacion.log'), 'a', encoding='utf-8') as f: f.write(line + '\n')
        except OSError: pass
        self.logbox.insert('end', line + '\n')
        if self.log_shown: self.logbox.see('end')

    def start(self):
        if self.proc or not self.ready(): return
        self.save(); self.ok = False; self.step = 0
        self.logbox.delete('1.0', 'end')
        try:
            import datetime, platform
            with open(os.path.join(ROOT, 'instalacion.log'), 'w', encoding='utf-8') as f:
                f.write('Racing Club+ installer v%s - %s - %s\n' % (VERSION, datetime.datetime.now().strftime('%Y-%m-%d %H:%M'), platform.platform()))
        except OSError: pass
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
        cmd = [py, '-u', os.path.join(ROOT, 'instalar.py'), '--rpcs3', self.rpcs3, '--out', self.out, '--idioma', str(self.li + 1)]
        if any(self.sel.values()):
            (g, gr, _), (u, _, _), (p, pr, _) = self.parts('game'), self.parts('update'), self.parts('pack')
            cmd += ['--pkg'] + [x for x in (g, u, p) if x] + ['--rap'] + [x for x in (gr, pr) if x]
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
