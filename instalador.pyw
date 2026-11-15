"""Racing Club+ : graphical installer. Runs instalar.py (which does the real work) and shows its progress.

Started by "Instalar Racing Club+.exe" (src/tools/instalador_launcher.c) with the Python bundled in toolchain/python,
or directly: pythonw instalador.pyw
"""
import ctypes, json, os, queue, re, subprocess, sys, threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

ROOT = os.path.dirname(os.path.abspath(__file__))
VERSION = '2.0'
PREFS = os.path.join(ROOT, 'instalador.json')
LANGS = ['English', 'Français', 'Español', 'Deutsch', 'Italiano']   # same order and numbers (1-5) as the game's menu

T = {
 'sub':      ['PC installer', 'Installateur PC', 'Instalador para PC', 'PC-Installationsprogramm', 'Installazione per PC'],
 'lang':     ['Language', 'Langue', 'Idioma', 'Sprache', 'Lingua'],
 'req':      ['What you need', 'Ce qu\'il vous faut', 'Qué necesitas', 'Was du brauchst', 'Cosa ti serve'],
 'req1':     ['Your own copy of GTI Club+ Rally Côte d\'Azur, EUROPEAN version NPEB00060 with update 1.02 (car pack optional)',
              'Votre copie de GTI Club+ Rally Côte d\'Azur, version EUROPÉENNE NPEB00060 avec la mise à jour 1.02 (pack de voitures en option)',
              'Tu copia de GTI Club+ Rally Côte d\'Azur, versión EUROPEA NPEB00060 con la actualización 1.02 (pack de coches opcional)',
              'Deine Kopie von GTI Club+ Rally Côte d\'Azur, EUROPÄISCHE Version NPEB00060 mit Update 1.02 (Autopaket optional)',
              'La tua copia di GTI Club+ Rally Côte d\'Azur, versione EUROPEA NPEB00060 con l\'aggiornamento 1.02 (pacchetto auto opzionale)'],
 'req2':     ['Its .pkg files (game, update 1.02 and, if you have it, the car pack) and their .rap licences',
              'Ses fichiers .pkg (jeu, mise à jour 1.02 et, si vous l\'avez, le pack de voitures) et leurs licences .rap',
              'Sus ficheros .pkg (juego, actualización 1.02 y, si lo tienes, el pack de coches) y sus licencias .rap',
              'Die .pkg-Dateien (Spiel, Update 1.02 und, falls vorhanden, das Autopaket) und ihre .rap-Lizenzen',
              'I suoi file .pkg (gioco, aggiornamento 1.02 e, se ce l\'hai, il pacchetto auto) e le licenze .rap'],
 'reqr':     ['RPCS3 (free): it is only used to decrypt your game. No PS3 system software is needed (the music decoder is built in)',
              'RPCS3 (gratuit) : il sert uniquement à déchiffrer votre jeu. Le logiciel système PS3 n\'est pas nécessaire (décodeur de musique intégré)',
              'RPCS3 (gratis): solo se usa para descifrar tu juego. No hace falta el firmware de PS3 (el decodificador de música va integrado)',
              'RPCS3 (kostenlos): wird nur zum Entschlüsseln deines Spiels benutzt. Keine PS3-Systemsoftware nötig (Musikdecoder integriert)',
              'RPCS3 (gratuito): serve solo a decifrare il tuo gioco. Il software di sistema PS3 non serve (decoder musicale integrato)'],
 'dl':       ['Download RPCS3', 'Télécharger RPCS3', 'Descargar RPCS3', 'RPCS3 herunterladen', 'Scarica RPCS3'],
 'src':      ['Your game', 'Votre jeu', 'Tu juego', 'Dein Spiel', 'Il tuo gioco'],
 'src0':     ['From the .pkg files', 'Depuis les fichiers .pkg', 'Desde los ficheros .pkg', 'Aus den .pkg-Dateien', 'Dai file .pkg'],
 'src1':     ['Already installed in RPCS3', 'Déjà installé dans RPCS3', 'Ya instalado en RPCS3', 'Schon in RPCS3 installiert', 'Già installato in RPCS3'],
 'pkg0':     ['Game (.pkg)', 'Jeu (.pkg)', 'Juego (.pkg)', 'Spiel (.pkg)', 'Gioco (.pkg)'],
 'pkg1':     ['Update 1.02 (.pkg)', 'Mise à jour 1.02 (.pkg)', 'Actualización 1.02 (.pkg)', 'Update 1.02 (.pkg)', 'Aggiornamento 1.02 (.pkg)'],
 'pkg2':     ['Car pack (.pkg, optional)', 'Pack voitures (.pkg, option)', 'Pack de coches (.pkg, opcional)', 'Autopaket (.pkg, optional)', 'Pacchetto auto (.pkg, opz.)'],
 'raps':     ['Licences (.rap)', 'Licences (.rap)', 'Licencias (.rap)', 'Lizenzen (.rap)', 'Licenze (.rap)'],
 'rapn':     ['%d selected', '%d sélectionnée(s)', '%d seleccionada(s)', '%d ausgewählt', '%d selezionate'],
 'rpcs3ok':  ['RPCS3 found', 'RPCS3 trouvé', 'RPCS3 encontrado', 'RPCS3 gefunden', 'RPCS3 trovato'],
 'pkghelp':  ['RPCS3 will open once per package and ask you to confirm: press “Install”. It closes by itself when it is done.',
              'RPCS3 s\'ouvrira pour chaque paquet et demandera confirmation : appuyez sur « Install ». Il se ferme tout seul.',
              'RPCS3 se abrirá con cada paquete y te pedirá confirmar: pulsa «Install». Se cierra solo al terminar.',
              'RPCS3 öffnet sich für jedes Paket und fragt nach: „Install“ drücken. Es schließt sich danach von selbst.',
              'RPCS3 si aprirà per ogni pacchetto e chiederà conferma: premi «Install». Si chiude da solo alla fine.'],
 'needpkg':  ['Choose the game and update .pkg files and the .rap licences', 'Choisissez les .pkg du jeu et de la mise à jour et les licences .rap',
              'Elige los .pkg del juego y de la actualización y las licencias .rap', 'Wähle die .pkg von Spiel und Update und die .rap-Lizenzen',
              'Scegli i .pkg del gioco e dell\'aggiornamento e le licenze .rap'],
 'req3':     ['Other regions or versions are not supported. No game file is included in this installer.',
              'Les autres régions ou versions ne sont pas prises en charge. Aucun fichier du jeu n\'est inclus.',
              'Otras regiones o versiones no sirven. Este instalador no incluye ningún fichero del juego.',
              'Andere Regionen oder Versionen werden nicht unterstützt. Es sind keine Spieldateien enthalten.',
              'Altre regioni o versioni non sono supportate. Nessun file del gioco è incluso.'],
 'opts':     ['Installation', 'Installation', 'Instalación', 'Installation', 'Installazione'],
 'rpcs3':    ['RPCS3 folder', 'Dossier RPCS3', 'Carpeta de RPCS3', 'RPCS3-Ordner', 'Cartella di RPCS3'],
 'out':      ['Install to', 'Installer dans', 'Instalar en', 'Installieren nach', 'Installa in'],
 'browse':   ['Browse…', 'Parcourir…', 'Examinar…', 'Durchsuchen…', 'Sfoglia…'],
 'glang':    ['Game language', 'Langue du jeu', 'Idioma del juego', 'Spielsprache', 'Lingua del gioco'],
 'copy':     ['Copy the game data (needed to play; about 350 MB)', 'Copier les données du jeu (nécessaire ; environ 350 Mo)',
              'Copiar los datos del juego (necesario para jugar; unos 350 MB)', 'Spieldaten kopieren (zum Spielen nötig; ca. 350 MB)',
              'Copia i dati del gioco (necessario; circa 350 MB)'],
 'lnk':      ['Create a desktop shortcut', 'Créer un raccourci sur le bureau', 'Crear un acceso directo en el escritorio',
              'Verknüpfung auf dem Desktop erstellen', 'Crea un collegamento sul desktop'],
 'install':  ['Install', 'Installer', 'Instalar', 'Installieren', 'Installa'],
 'quit':     ['Close', 'Fermer', 'Cerrar', 'Schließen', 'Chiudi'],
 'play':     ['Play', 'Jouer', 'Jugar', 'Spielen', 'Gioca'],
 'details':  ['Details', 'Détails', 'Detalles', 'Details', 'Dettagli'],
 'ready':    ['Ready to install. It takes a few minutes.', 'Prêt. L\'installation prend quelques minutes.',
              'Listo para instalar. Tarda unos minutos.', 'Bereit. Die Installation dauert einige Minuten.',
              'Pronto. L\'installazione richiede alcuni minuti.'],
 'found':    ['Game found: NPEB00060, version %s', 'Jeu trouvé : NPEB00060, version %s', 'Juego encontrado: NPEB00060, versión %s',
              'Spiel gefunden: NPEB00060, Version %s', 'Gioco trovato: NPEB00060, versione %s'],
 'nogame':   ['The game NPEB00060 is not installed in this RPCS3 folder', 'Le jeu NPEB00060 n\'est pas installé dans ce dossier RPCS3',
              'En esta carpeta de RPCS3 no está instalado el juego NPEB00060', 'In diesem RPCS3-Ordner ist NPEB00060 nicht installiert',
              'In questa cartella di RPCS3 il gioco NPEB00060 non è installato'],
 'norpcs3':  ['rpcs3.exe was not found in this folder', 'rpcs3.exe introuvable dans ce dossier', 'No encuentro rpcs3.exe en esta carpeta',
              'rpcs3.exe wurde in diesem Ordner nicht gefunden', 'rpcs3.exe non trovato in questa cartella'],
 'nofw':     ['Warning: PS3 system software not installed in RPCS3: the game will have no music',
              'Attention : logiciel système PS3 absent de RPCS3 : le jeu n\'aura pas de musique',
              'Aviso: RPCS3 no tiene el firmware de PS3: el juego no tendrá música',
              'Hinweis: PS3-Systemsoftware fehlt in RPCS3: das Spiel hat keine Musik',
              'Avviso: software di sistema PS3 assente in RPCS3: il gioco non avrà musica'],
 'pip':      ['Preparing the installer (capstone module)…', 'Préparation (module capstone)…', 'Preparando el instalador (módulo capstone)…',
              'Vorbereitung (Modul capstone)…', 'Preparazione (modulo capstone)…'],
 'steps':    [['Checking the game', 'Decrypting the game executable', 'Analysing the game code', 'Translating the game code',
               'Translating the sound and geometry programs', 'Compiling', 'Preparing the game folder'],
              ['Vérification du jeu', 'Déchiffrement de l\'exécutable', 'Analyse du code du jeu', 'Traduction du code du jeu',
               'Traduction des programmes de son et de géométrie', 'Compilation', 'Préparation du dossier du jeu'],
              ['Comprobando el juego', 'Descifrando el ejecutable del juego', 'Analizando el código del juego', 'Traduciendo el código del juego',
               'Traduciendo los programas de sonido y geometría', 'Compilando', 'Preparando la carpeta del juego'],
              ['Spiel wird geprüft', 'Spieldatei wird entschlüsselt', 'Spielcode wird analysiert', 'Spielcode wird übersetzt',
               'Sound- und Geometrieprogramme werden übersetzt', 'Kompilieren', 'Spielordner wird vorbereitet'],
              ['Verifica del gioco', 'Decifratura dell\'eseguibile', 'Analisi del codice del gioco', 'Traduzione del codice del gioco',
               'Traduzione dei programmi audio e geometria', 'Compilazione', 'Preparazione della cartella del gioco']],
 'done':     ['Done! Racing Club+ is installed.', 'Terminé ! Racing Club+ est installé.', '¡Hecho! Racing Club+ está instalado.',
              'Fertig! Racing Club+ ist installiert.', 'Fatto! Racing Club+ è installato.'],
 'failed':   ['The installation failed. Open “Details” to see why.', 'L\'installation a échoué. Ouvrez « Détails » pour voir pourquoi.',
              'La instalación ha fallado. Abre «Detalles» para ver el motivo.', 'Die Installation ist fehlgeschlagen. Siehe „Details“.',
              'Installazione non riuscita. Apri «Dettagli» per vedere il motivo.'],
 'personal': ['The installed folder contains your copy of the game: for your own use only, do not share it.',
              'Le dossier installé contient votre copie du jeu : usage personnel uniquement, ne le partagez pas.',
              'La carpeta instalada contiene tu copia del juego: es para tu uso, no la repartas.',
              'Der Ordner enthält deine Kopie des Spiels: nur für dich, nicht weitergeben.',
              'La cartella contiene la tua copia del gioco: solo per uso personale, non condividerla.'],
 'busy':     ['The installation is still running. Stop it and close?', 'L\'installation est en cours. L\'arrêter et fermer ?',
              'La instalación sigue en marcha. ¿Detenerla y cerrar?', 'Die Installation läuft noch. Abbrechen und schließen?',
              'L\'installazione è in corso. Interromperla e chiudere?'],
}

BLUE, BLUE_DARK, INK, GREY, LINE, CARD, BG = '#0064F5', '#004FC4', '#1B1F27', '#6B7280', '#E3E7EE', '#F5F7FB', '#FFFFFF'
OK, WARN, ERR = '#138A43', '#B45309', '#C62828'


def windows_lang():
    try:
        primary = ctypes.windll.kernel32.GetUserDefaultUILanguage() & 0x3ff
        return {0x09: 0, 0x0c: 1, 0x0a: 2, 0x07: 3, 0x10: 4}.get(primary, 0)
    except Exception:
        return 0


def load_prefs():
    try:
        return json.load(open(PREFS, encoding='utf-8'))
    except Exception:
        return {}


def save_prefs(d):
    try:
        json.dump(d, open(PREFS, 'w', encoding='utf-8'), indent=1)
    except Exception:
        pass


def sfo_version(path):
    import struct
    try:
        d = open(path, 'rb').read()
        keys, data, n = struct.unpack_from('<III', d, 8)
        for i in range(n):
            ko, fmt, ln, mx, do = struct.unpack_from('<HHIII', d, 20 + 16 * i)
            if d[keys + ko: d.index(b'\0', keys + ko)] == b'APP_VER':
                return d[data + do: data + do + ln].rstrip(b'\0').decode()
    except Exception:
        pass
    return '?'


class App:
    def __init__(self, root):
        self.root = root
        prefs = load_prefs()
        self.li = prefs.get('lang', windows_lang())
        self.proc = None
        self.q = queue.Queue()
        self.step = 0
        self.ok = False
        root.title('Racing Club+')
        root.configure(bg=BG)
        root.resizable(False, False)
        ico = os.path.join(ROOT, 'assets', 'racing_club.ico')
        if os.path.exists(ico):
            try: root.iconbitmap(ico)
            except tk.TclError: pass
        st = ttk.Style(root)
        st.theme_use('clam')
        st.configure('TCombobox', fieldbackground=BG, background=BG, bordercolor=LINE, lightcolor=LINE, darkcolor=LINE, arrowcolor=INK, padding=4)
        st.map('TCombobox', fieldbackground=[('readonly', BG)], selectbackground=[('readonly', BG)], selectforeground=[('readonly', INK)])
        st.configure('Blue.Horizontal.TProgressbar', troughcolor=CARD, background=BLUE, bordercolor=LINE, lightcolor=BLUE, darkcolor=BLUE, thickness=10)
        st.configure('TCheckbutton', background=BG, foreground=INK, font=('Segoe UI', 10))
        st.map('TCheckbutton', background=[('active', BG)])

        self.rpcs3 = tk.StringVar(value=prefs.get('rpcs3', ''))
        self.mode = tk.IntVar(value=prefs.get('mode', 0))   # 0 from the .pkg files, 1 game already installed in RPCS3
        self.pkgs = [tk.StringVar(value=v) for v in (prefs.get('pkgs') or ['', '', ''])]
        self.raps = list(prefs.get('raps', []))
        self.out = tk.StringVar(value=prefs.get('out', os.path.join(ROOT, 'Racing Club+')))
        self.glang = tk.IntVar(value=prefs.get('game_lang', self.li))
        self.copy = tk.BooleanVar(value=True)
        self.lnk = tk.BooleanVar(value=prefs.get('shortcut', True))
        self.build()
        self.rpcs3.trace_add('write', lambda *a: self.check())
        self.check()
        root.protocol('WM_DELETE_WINDOW', self.close)
        root.after(100, self.poll)

    def t(self, k):
        return T[k][self.li]

    # ---------------------------------------------------------------- layout
    def build(self):
        for w in self.root.winfo_children(): w.destroy()
        r = self.root
        pad = 28
        head = tk.Frame(r, bg=BG)
        head.pack(fill='x', padx=pad, pady=(16, 8))
        logo = os.path.join(ROOT, 'assets', 'racing_club_logo.png')
        if os.path.exists(logo):
            self.logo = tk.PhotoImage(file=logo)
            tk.Label(head, image=self.logo, bg=BG).pack(side='left')
        title = tk.Frame(head, bg=BG)
        title.pack(side='left', padx=(18, 0), fill='y')
        tk.Label(title, text='Racing Club+', font=('Arial Black', 26), fg=BLUE, bg=BG).pack(anchor='w', pady=(14, 0))
        tk.Label(title, text=self.t('sub') + '   ·   v' + VERSION, font=('Segoe UI', 12), fg=GREY, bg=BG).pack(anchor='w')
        lf = tk.Frame(head, bg=BG)
        lf.pack(side='right', anchor='n', pady=(6, 0))
        tk.Label(lf, text=self.t('lang'), font=('Segoe UI', 9), fg=GREY, bg=BG).pack(anchor='e')
        cb = ttk.Combobox(lf, values=LANGS, state='readonly', width=11, font=('Segoe UI', 10))
        cb.current(self.li)
        cb.bind('<<ComboboxSelected>>', lambda e: self.set_lang(cb.current()))
        cb.pack(anchor='e')

        # requirements card
        card = tk.Frame(r, bg=CARD, highlightbackground=LINE, highlightthickness=1)
        card.pack(fill='x', padx=pad, pady=(2, 10))
        tk.Label(card, text=self.t('req'), font=('Segoe UI Semibold', 11), fg=INK, bg=CARD).pack(anchor='w', padx=16, pady=(8, 2))
        for k in ('req1', 'req2', 'reqr'):
            row = tk.Frame(card, bg=CARD)
            row.pack(fill='x', padx=16, pady=1)
            tk.Label(row, text='•', font=('Segoe UI', 10), fg=BLUE, bg=CARD).pack(side='left', anchor='n')
            tk.Label(row, text=self.t(k), font=('Segoe UI', 10), fg=INK, bg=CARD, justify='left', wraplength=600).pack(side='left', padx=(6, 0))
        foot = tk.Frame(card, bg=CARD)
        foot.pack(fill='x', padx=16, pady=(4, 8))
        tk.Label(foot, text=self.t('req3'), font=('Segoe UI', 9), fg=GREY, bg=CARD, justify='left', wraplength=470).pack(side='left')
        link = tk.Label(foot, text=self.t('dl') + '  ↗', font=('Segoe UI Semibold', 10, 'underline'), fg=BLUE, bg=CARD, cursor='hand2')
        link.pack(side='right')
        link.bind('<Button-1>', lambda e: os.startfile('https://rpcs3.net/download'))

        # options
        tk.Label(r, text=self.t('opts'), font=('Segoe UI Semibold', 11), fg=INK, bg=BG).pack(anchor='w', padx=pad)
        grid = tk.Frame(r, bg=BG)
        grid.pack(fill='x', padx=pad, pady=(6, 0))
        grid.columnconfigure(1, weight=1)
        tk.Label(grid, text=self.t('src'), font=('Segoe UI', 10), fg=INK, bg=BG).grid(row=0, column=0, sticky='w')
        rb = tk.Frame(grid, bg=BG)
        rb.grid(row=0, column=1, columnspan=2, sticky='w', padx=8)
        for i, k in enumerate(('src0', 'src1')):
            tk.Radiobutton(rb, text=self.t(k), variable=self.mode, value=i, command=self.rebuild, font=('Segoe UI', 10), bg=BG, fg=INK,
                           activebackground=BG, selectcolor=BG, highlightthickness=0).pack(side='left', padx=(0, 16))
        row = 1
        self.rap_label = None
        if self.mode.get() == 0:
            for i in range(3):
                self.path_row(grid, row, 'pkg%d' % i, self.pkgs[i], lambda i=i: self.pick_pkg(i), pady=3)
                row += 1
            tk.Label(grid, text=self.t('raps'), font=('Segoe UI', 10), fg=INK, bg=BG).grid(row=row, column=0, sticky='w')
            self.rap_label = tk.Label(grid, text='', font=('Segoe UI', 10), fg=INK, bg=BG, anchor='w', width=40)
            self.rap_label.grid(row=row, column=1, sticky='ew', padx=12)
            self.button(grid, self.t('browse'), self.pick_raps, primary=False).grid(row=row, column=2, sticky='e', pady=3)
            tk.Label(grid, text=self.t('pkghelp'), font=('Segoe UI', 9), fg=GREY, bg=BG, justify='left', wraplength=640).grid(row=row + 1, column=0, columnspan=3, sticky='w', pady=(2, 8))
            row += 2
        self.path_row(grid, row, 'rpcs3', self.rpcs3, self.pick_rpcs3, pady=3)
        self.status_rpcs3 = tk.Label(grid, text='', font=('Segoe UI', 9), bg=BG, anchor='w')
        self.status_rpcs3.grid(row=row + 1, column=1, columnspan=2, sticky='w', pady=(0, 4))
        self.path_row(grid, row + 2, 'out', self.out, self.pick_out, pady=3)
        tk.Label(grid, text=self.t('glang'), font=('Segoe UI', 10), fg=INK, bg=BG).grid(row=row + 3, column=0, sticky='w', pady=(8, 0))
        gl = ttk.Combobox(grid, values=LANGS, state='readonly', width=14, font=('Segoe UI', 10))
        gl.current(self.glang.get())
        gl.bind('<<ComboboxSelected>>', lambda e: self.glang.set(gl.current()))
        gl.grid(row=row + 3, column=1, sticky='w', padx=(12, 0), pady=(8, 0))
        ttk.Checkbutton(grid, text=self.t('copy'), variable=self.copy).grid(row=row + 4, column=0, columnspan=3, sticky='w', pady=(10, 0))
        ttk.Checkbutton(grid, text=self.t('lnk'), variable=self.lnk).grid(row=row + 5, column=0, columnspan=3, sticky='w', pady=(2, 0))

        # progress
        prog = tk.Frame(r, bg=BG)
        prog.pack(fill='x', padx=pad, pady=(12, 0))
        self.bar = ttk.Progressbar(prog, style='Blue.Horizontal.TProgressbar', maximum=1000)
        self.bar.pack(fill='x')
        self.msg = tk.Label(prog, text=self.t('ready'), font=('Segoe UI', 10), fg=GREY, bg=BG, anchor='w', justify='left', wraplength=640)
        self.msg.pack(fill='x', pady=(6, 0))
        self.logbox = tk.Text(r, height=9, font=('Consolas', 9), bg=CARD, fg=INK, relief='flat', highlightbackground=LINE, highlightthickness=1, wrap='none')
        self.log_shown = False

        # buttons
        bar = tk.Frame(r, bg=BG)
        bar.pack(side='bottom', fill='x', padx=pad, pady=(10, 16))
        self.btn_details = self.button(bar, self.t('details'), self.toggle_log, primary=False)
        self.btn_details.pack(side='left')
        self.btn_quit = self.button(bar, self.t('quit'), self.close, primary=False)
        self.btn_quit.pack(side='right')
        self.btn_go = self.button(bar, self.t('play') if self.ok else self.t('install'), self.play if self.ok else self.start, primary=True)
        self.btn_go.pack(side='right', padx=(0, 10))

    def button(self, parent, text, cmd, primary):
        b = tk.Button(parent, text=text, command=cmd, font=('Segoe UI Semibold', 10), relief='flat', bd=0, cursor='hand2', padx=20, pady=6,
                      bg=BLUE if primary else CARD, fg='white' if primary else INK,
                      activebackground=BLUE_DARK if primary else LINE, activeforeground='white' if primary else INK)
        return b

    def path_row(self, grid, row, key, var, cmd, pady=0):
        tk.Label(grid, text=self.t(key), font=('Segoe UI', 10), fg=INK, bg=BG).grid(row=row, column=0, sticky='w')
        e = tk.Entry(grid, textvariable=var, font=('Segoe UI', 10), relief='flat', bg=BG, fg=INK, highlightbackground=LINE, highlightcolor=BLUE, highlightthickness=1)
        e.grid(row=row, column=1, sticky='ew', padx=12, ipady=3)
        self.button(grid, self.t('browse'), cmd, primary=False).grid(row=row, column=2, sticky='e', pady=pady)

    def set_lang(self, i):
        self.li = i
        self.rebuild()

    def rebuild(self):
        self.save()
        self.build()
        self.check()

    def pick_pkg(self, i):
        f = filedialog.askopenfilename(title=self.t('pkg%d' % i), filetypes=[('PS3 .pkg', '*.pkg'), ('*', '*.*')],
                                       initialdir=os.path.dirname(self.pkgs[i].get()) or None)
        if f: self.pkgs[i].set(os.path.normpath(f)); self.check()

    def pick_raps(self):
        fs = filedialog.askopenfilenames(title=self.t('raps'), filetypes=[('PS3 .rap', '*.rap'), ('*', '*.*')],
                                         initialdir=os.path.dirname(self.raps[0]) if self.raps else (os.path.dirname(self.pkgs[0].get()) or None))
        if fs: self.raps = [os.path.normpath(f) for f in fs]; self.check()

    def toggle_log(self):
        self.log_shown = not self.log_shown
        if self.log_shown:
            self.logbox.pack(fill='both', padx=28, pady=(10, 0), before=self.btn_details.master)
            self.logbox.see('end')
        else:
            self.logbox.pack_forget()

    # ---------------------------------------------------------------- checks
    def check(self):
        if self.rap_label is not None:
            self.rap_label.config(text=self.t('rapn') % len(self.raps) + ('   ' + ', '.join(os.path.basename(x)[-16:] for x in self.raps[:2]) if self.raps else ''))
        p = self.rpcs3.get().strip().strip('"')
        if not p:
            self.status_rpcs3.config(text='')
            return False
        if not os.path.exists(os.path.join(p, 'rpcs3.exe')):
            self.status_rpcs3.config(text='✗  ' + self.t('norpcs3'), fg=ERR)
            return False
        if self.mode.get() == 0:   # the game comes from the .pkg files: RPCS3 only needs to be there
            self.status_rpcs3.config(text='✓  ' + self.t('rpcs3ok'), fg=OK, justify='left')
            if not (self.pkgs[0].get() and self.pkgs[1].get() and self.raps):
                self.msg.config(text=self.t('needpkg'), fg=GREY)
                return False
            self.msg.config(text=self.t('ready'), fg=GREY)
            return True
        sfo = os.path.join(p, 'dev_hdd0', 'game', 'NPEB00060', 'PARAM.SFO')
        if not os.path.exists(sfo):
            self.status_rpcs3.config(text='✗  ' + self.t('nogame'), fg=ERR)
            return False
        ver = sfo_version(sfo)
        txt, col = '✓  ' + self.t('found') % ver, OK if ver == '01.02' else ERR
        self.status_rpcs3.config(text=txt, fg=col, justify='left')
        return True

    def pick_rpcs3(self):
        d = filedialog.askdirectory(title=self.t('rpcs3'), initialdir=self.rpcs3.get() or None)
        if d: self.rpcs3.set(os.path.normpath(d))

    def pick_out(self):
        d = filedialog.askdirectory(title=self.t('out'), initialdir=os.path.dirname(self.out.get()) or None)
        if d: self.out.set(os.path.join(os.path.normpath(d), 'Racing Club+') if os.path.basename(d) != 'Racing Club+' else os.path.normpath(d))

    def save(self):
        save_prefs({'lang': self.li, 'rpcs3': self.rpcs3.get(), 'out': self.out.get(), 'game_lang': self.glang.get(), 'shortcut': self.lnk.get(),
                    'mode': self.mode.get(), 'pkgs': [v.get() for v in self.pkgs], 'raps': self.raps})

    # ---------------------------------------------------------------- run
    def log(self, line):
        self.logbox.insert('end', line + '\n')
        if self.log_shown: self.logbox.see('end')

    def set_progress(self, frac, text, color=GREY):
        self.bar['value'] = int(frac * 1000)
        self.msg.config(text=text, fg=color)

    def start(self):
        if self.proc or not self.check(): return
        self.save()
        self.ok = False
        self.logbox.delete('1.0', 'end')
        self.btn_go.config(state='disabled', bg=LINE)
        self.step = 0
        threading.Thread(target=self.worker, daemon=True).start()

    def worker(self):
        py = sys.executable
        if os.path.basename(py).lower() == 'pythonw.exe':   # a console-less child still pipes its output
            alt = os.path.join(os.path.dirname(py), 'python.exe')
            if os.path.exists(alt): py = alt
        flags = 0x08000000   # CREATE_NO_WINDOW
        if subprocess.run([py, '-c', 'import capstone'], creationflags=flags, capture_output=True).returncode != 0:
            self.q.put(('msg', self.t('pip')))
            r = subprocess.run([py, '-m', 'pip', 'install', 'capstone==5.0.7'], creationflags=flags, capture_output=True, text=True)
            self.q.put(('log', r.stdout + r.stderr))
        cmd = [py, '-u', os.path.join(ROOT, 'instalar.py'), '--rpcs3', self.rpcs3.get().strip().strip('"'), '--out', self.out.get(), '--idioma', str(self.glang.get() + 1)]
        if not self.copy.get(): cmd.append('--no-copy')
        if self.mode.get() == 0:
            cmd += ['--pkg'] + [v.get().strip().strip('"') for v in self.pkgs if v.get().strip()] + ['--rap'] + self.raps
        self.proc = subprocess.Popen(cmd, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, creationflags=flags, text=True, encoding='utf-8', errors='replace')
        for line in self.proc.stdout: self.q.put(('line', line.rstrip()))
        rc = self.proc.wait()
        self.proc = None
        self.q.put(('end', rc))

    def poll(self):
        try:
            while True:
                kind, v = self.q.get_nowait()
                if kind == 'msg': self.set_progress(0, v)
                elif kind == 'log': self.log(v)
                elif kind == 'line': self.on_line(v)
                elif kind == 'end': self.finish(v)
        except queue.Empty:
            pass
        self.root.after(100, self.poll)

    def on_line(self, line):
        self.log(line)
        m = re.match(r'^\[(\d+)/7\] ', line)
        if m and not re.match(r'^\[\d+/7\] (Building|Linking)', line):
            self.step = int(m.group(1))
            self.set_progress((self.step - 1) / 7, '%d/7 · %s…' % (self.step, T['steps'][self.li][self.step - 1]))
            return
        m = re.match(r'^\[(\d+)/(\d+)\] (Building|Linking)', line)
        if m and self.step == 6:   # compiler progress inside step 6
            k, n = int(m.group(1)), int(m.group(2))
            self.set_progress((5 + k / max(n, 1)) / 7, '6/7 · %s… %d %%' % (T['steps'][self.li][5], 100 * k // max(n, 1)))

    def finish(self, rc):
        if rc == 0:
            self.ok = True
            if self.lnk.get(): self.shortcut()
            self.set_progress(1, self.t('done') + '\n' + self.t('personal'), OK)
            self.btn_go.config(text=self.t('play'), command=self.play, state='normal', bg=BLUE)
        else:
            self.set_progress(self.bar['value'] / 1000, self.t('failed'), ERR)
            self.btn_go.config(state='normal', bg=BLUE)
            if not self.log_shown: self.toggle_log()

    def exe(self):
        return os.path.join(self.out.get(), 'Racing Club+.exe')

    def shortcut(self):
        ps = ("$d=[Environment]::GetFolderPath('Desktop');$s=(New-Object -ComObject WScript.Shell).CreateShortcut((Join-Path $d 'Racing Club+.lnk'));"
              "$s.TargetPath='%s';$s.WorkingDirectory='%s';$s.IconLocation='%s,0';$s.Save()") % (self.exe().replace("'", "''"), self.out.get().replace("'", "''"), self.exe().replace("'", "''"))
        subprocess.run(['powershell', '-NoProfile', '-Command', ps], creationflags=0x08000000, capture_output=True)

    def play(self):
        if os.path.exists(self.exe()):
            subprocess.Popen([self.exe()], cwd=self.out.get())
            self.root.destroy()

    def close(self):
        if self.proc:
            if not messagebox.askyesno('Racing Club+', self.t('busy')): return
            try: self.proc.kill()
            except Exception: pass
        self.save()
        self.root.destroy()


if __name__ == '__main__':
    try: ctypes.windll.shcore.SetProcessDpiAwareness(1)   # sharp text on scaled displays
    except Exception: pass
    root = tk.Tk()
    if os.environ.get('INSTALADOR_TOP'): root.attributes('-topmost', True)   # tests started from a background process
    App(root)
    root.mainloop()
