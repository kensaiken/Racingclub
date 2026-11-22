"""Racing Club+ : graphical installer. Runs instalar.py (which does the real work) and shows its progress.

Started by "Instalar Racing Club+.exe" (src/tools/instalador_launcher.c) with the Python bundled in toolchain/python,
or directly: pythonw instalador.pyw
Three sections, one per file the user bought: the game (.pkg + its .rap licence), update 1.02 (.pkg) and the car pack
(optional, .pkg). No emulator: the packages are extracted and the game decrypted by instalar.py (tools/ps3crypt.py).
The installer's language is also the game's language.
"""
import ctypes, json, os, queue, re, struct, subprocess, sys, threading
import tkinter as tk
from tkinter import filedialog, messagebox

ROOT = os.path.dirname(os.path.abspath(__file__))
VERSION = '2.7'
PREFS = os.path.join(ROOT, 'instalador.json')
LANGS = ['English', 'Français', 'Español', 'Deutsch', 'Italiano']   # same order and numbers (1-5) as the game's menu
TITLE_ID, APP_VER = 'NPEB00060', '01.02'

T = {
 'sub':     ['PC installer', 'Installateur PC', 'Instalador para PC', 'PC-Installation', 'Installazione per PC'],
 's_game':  ['Game', 'Jeu', 'Juego', 'Spiel', 'Gioco'],
 'h_game':  ['The game\'s .pkg (its .rap licence is found next to it)', 'Le .pkg du jeu (sa licence .rap est trouvée à côté)',
             'El .pkg del juego (su licencia .rap se busca a su lado)', 'Die .pkg des Spiels (die .rap-Lizenz daneben wird gefunden)',
             'Il .pkg del gioco (la licenza .rap accanto viene trovata)'],
 's_upd':   ['Update 1.02', 'Mise à jour 1.02', 'Actualización 1.02', 'Update 1.02', 'Aggiornamento 1.02'],
 'h_upd':   ['The update\'s .pkg (it has no licence)', 'Le .pkg de la mise à jour (sans licence)', 'El .pkg de la actualización (no lleva licencia)',
             'Die .pkg des Updates (ohne Lizenz)', 'Il .pkg dell\'aggiornamento (senza licenza)'],
 's_pack':  ['Car pack (optional)', 'Pack de voitures (facultatif)', 'Pack de coches (opcional)', 'Autopaket (optional)', 'Pacchetto auto (opzionale)'],
 'h_pack':  ['The DLC\'s .pkg (you can leave it empty)', 'Le .pkg du DLC (vous pouvez le laisser vide)', 'El .pkg del DLC (puedes dejarlo vacío)',
             'Die .pkg des DLC (darf leer bleiben)', 'Il .pkg del DLC (puoi lasciarlo vuoto)'],
 'pickf':   ['Choose files…', 'Choisir…', 'Elegir ficheros…', 'Dateien wählen…', 'Scegli file…'],
 'pick1':   ['Choose file…', 'Choisir…', 'Elegir fichero…', 'Datei wählen…', 'Scegli file…'],
 'pickd':   ['Choose folder…', 'Choisir…', 'Elegir carpeta…', 'Ordner wählen…', 'Scegli cartella…'],
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
 'nolic':   ['no .rap licence next to it: put it in the same folder', 'pas de licence .rap à côté : mettez-la dans le même dossier',
             'no hay licencia .rap a su lado: ponla en la misma carpeta', 'keine .rap-Lizenz daneben: in denselben Ordner legen',
             'nessuna licenza .rap accanto: mettila nella stessa cartella'],
 'nopkg':   ['the .pkg is missing', 'il manque le .pkg', 'falta el .pkg', 'die .pkg fehlt', 'manca il .pkg'],
 'wrong':   ['this is not the right file for this section', 'ce fichier ne va pas ici', 'este fichero no es el de este apartado',
             'diese Datei gehört nicht hierher', 'questo file non va qui'],
 'tab_inst': ['Install', 'Installer', 'Instalar', 'Installieren', 'Installa'],
 'tab_online': ['Play online', 'Jouer en ligne', 'Jugar online', 'Online spielen', 'Gioca online'],
 'online':  ['# Create a game (host)\n- In the game press F1 > Online: «Juego online» Sí, and your name (restart the game if you change it).\n- Same house / same network: nothing else to do, your friends will find the game by themselves.\n- Over the internet: the game tries to open port 3658 on your router by itself (F1 > Online shows «router abierto (UPnP)»). If it shows «abre el puerto a mano», open port 3658 TCP and UDP in your router towards the address shown in F1 > Online > «Tu IP en la red local».\n- Give your friends your public IP (search "what is my IP" on the internet).\n- Multiplayer > Create game: choose race type, settings and car. Wait in the Lobby until everyone is in and start the race.\n# Join a game\n- Same network: nothing to set. Over the internet: F1 > Online > «Unirse a (IP del anfitrión)» and type your friend\'s public IP.\n- Multiplayer > Join a game (or Quick game): your friend\'s game appears in the list. Choose it.\n- Choose your car and wait in the Lobby until the host starts.\n# Good to know\nThe first time, Windows asks whether Racing Club+ may use the network: press Allow. Everybody needs the same version of Racing Club+. No PSN account or server is used: the games connect to each other.', "# Créer une partie (hôte)\n- Dans le jeu, appuyez sur F1 > Online : «Juego online» Sí, et votre nom (redémarrez le jeu si vous le changez).\n- Même maison / même réseau : rien d'autre à faire, vos amis trouveront la partie tout seuls.\n- Par internet : le jeu essaie d'ouvrir lui-même le port 3658 de votre box (F1 > Online affiche «router abierto (UPnP)»). Sinon, ouvrez le port 3658 TCP et UDP dans votre box vers l'adresse affichée dans F1 > Online > «Tu IP en la red local».\n- Donnez votre IP publique à vos amis (cherchez « quelle est mon IP » sur internet).\n- Multijoueur > Créer une partie : choisissez le type, les réglages et la voiture. Attendez dans le Salon que tout le monde soit là et lancez la course.\n# Rejoindre une partie\n- Même réseau : rien à régler. Par internet : F1 > Online > «Unirse a (IP del anfitrión)» et tapez l'IP publique de votre ami.\n- Multijoueur > Rejoindre une partie (ou Partie rapide) : la partie de votre ami apparaît dans la liste. Choisissez-la.\n- Choisissez votre voiture et attendez dans le Salon que l'hôte lance la course.\n# Bon à savoir\nLa première fois, Windows demande si Racing Club+ peut utiliser le réseau : cliquez sur Autoriser. Tout le monde doit avoir la même version de Racing Club+. Aucun compte PSN ni serveur : les jeux se connectent directement.", '# Crear una partida (anfitrión)\n- En el juego pulsa F1 > Online: «Juego online» Sí, y tu nombre (reinicia el juego si lo cambias).\n- Misma casa / misma red: no hace falta nada más, tus amigos encontrarán la partida solos.\n- Por internet: el juego intenta abrir solo el puerto 3658 de tu router (F1 > Online pone «router abierto (UPnP)»). Si pone «abre el puerto a mano», abre en tu router el puerto 3658 TCP y UDP hacia la dirección que aparece en F1 > Online > «Tu IP en la red local».\n- Dales a tus amigos tu IP pública (busca «cuál es mi IP» en internet).\n- Multijugador > Crear partida: elige tipo de carrera, configuración y coche. Espera en la Sala a que entren todos y empieza la carrera.\n# Unirse a una partida\n- En la misma red no hay que configurar nada. Por internet: F1 > Online > «Unirse a (IP del anfitrión)» y escribe la IP pública de tu amigo.\n- Multijugador > Unirse a la partida (o Partida rápida): aparece la partida de tu amigo en la lista. Elígela.\n- Elige tu coche y espera en la Sala a que el anfitrión empiece.\n# Conviene saber\nLa primera vez Windows pregunta si Racing Club+ puede usar la red: pulsa Permitir. Todos necesitáis la misma versión de Racing Club+. No se usa cuenta de PSN ni ningún servidor: los juegos se conectan entre sí.', '# Spiel erstellen (Host)\n- Im Spiel F1 > Online: «Juego online» Sí und deinen Namen (nach einer Änderung das Spiel neu starten).\n- Gleiches Haus / gleiches Netzwerk: sonst nichts nötig, deine Freunde finden das Spiel von selbst.\n- Über das Internet: das Spiel versucht, Port 3658 im Router selbst zu öffnen (F1 > Online zeigt «router abierto (UPnP)»). Sonst Port 3658 TCP und UDP im Router auf die Adresse unter F1 > Online > «Tu IP en la red local» weiterleiten.\n- Gib deinen Freunden deine öffentliche IP (im Internet nach „meine IP“ suchen).\n- Mehrspieler > Spiel erstellen: Rennart, Einstellungen und Auto wählen. In der Lobby warten, bis alle da sind, und das Rennen starten.\n# Einem Spiel beitreten\n- Gleiches Netzwerk: nichts einzustellen. Über das Internet: F1 > Online > «Unirse a (IP del anfitrión)» und die öffentliche IP deines Freundes eingeben.\n- Mehrspieler > Spiel beitreten (oder Schnelles Spiel): das Spiel deines Freundes erscheint in der Liste. Auswählen.\n- Auto wählen und in der Lobby warten, bis der Host startet.\n# Gut zu wissen\nBeim ersten Mal fragt Windows, ob Racing Club+ das Netzwerk benutzen darf: Zulassen klicken. Alle brauchen dieselbe Version von Racing Club+. Kein PSN-Konto und kein Server: die Spiele verbinden sich direkt.', "# Creare una partita (host)\n- Nel gioco premi F1 > Online: «Juego online» Sí e il tuo nome (riavvia il gioco se lo cambi).\n- Stessa casa / stessa rete: nient'altro da fare, i tuoi amici troveranno la partita da soli.\n- Via internet: il gioco prova ad aprire da solo la porta 3658 del router (F1 > Online mostra «router abierto (UPnP)»). Altrimenti apri la porta 3658 TCP e UDP nel router verso l'indirizzo mostrato in F1 > Online > «Tu IP en la red local».\n- Dai ai tuoi amici il tuo IP pubblico (cerca «qual è il mio IP» su internet).\n- Multigiocatore > Crea partita: scegli tipo di gara, impostazioni e auto. Aspetta nella Sala che entrino tutti e avvia la gara.\n# Unirsi a una partita\n- Stessa rete: niente da impostare. Via internet: F1 > Online > «Unirse a (IP del anfitrión)» e scrivi l'IP pubblico del tuo amico.\n- Multigiocatore > Unisciti alla partita (o Partita rapida): la partita del tuo amico compare nella lista. Sceglila.\n- Scegli l'auto e aspetta nella Sala che l'host inizi.\n# Da sapere\nLa prima volta Windows chiede se Racing Club+ può usare la rete: premi Consenti. Serve la stessa versione di Racing Club+ per tutti. Nessun account PSN né server: i giochi si collegano direttamente."],
 'pip':     ['Preparing…', 'Préparation…', 'Preparando…', 'Vorbereitung…', 'Preparazione…'],
 'steps':   [['Extracting the game', 'Decrypting', 'Analysing the code', 'Translating the code', 'Translating sound and geometry', 'Compiling', 'Finishing'],
             ['Extraction du jeu', 'Déchiffrement', 'Analyse du code', 'Traduction du code', 'Traduction son et géométrie', 'Compilation', 'Finalisation'],
             ['Extrayendo el juego', 'Descifrando', 'Analizando el código', 'Traduciendo el código', 'Traduciendo sonido y geometría', 'Compilando', 'Terminando'],
             ['Spiel wird entpackt', 'Entschlüsseln', 'Code wird analysiert', 'Code wird übersetzt', 'Sound und Geometrie', 'Kompilieren', 'Abschluss'],
             ['Estrazione del gioco', 'Decifratura', 'Analisi del codice', 'Traduzione del codice', 'Audio e geometria', 'Compilazione', 'Conclusione']],
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


def find_rap(pkg):
    """The game's .rap licence next to its .pkg: same folder, its subfolders, the folder above and its subfolders"""
    d = os.path.dirname(os.path.abspath(pkg))
    for base, depth in ((d, 2), (os.path.dirname(d), 2)):
        for root, dirs, files in os.walk(base):
            if root[len(base):].count(os.sep) >= depth: dirs[:] = []
            for f in files:
                if f.lower().endswith('.rap') and 'NPEB00060_00-GTICLUB1' in f.upper(): return os.path.join(root, f)
    return ''


# what each section accepts: the content id of its .pkg and the name of its licence must contain this
KIND = {'game': 'NPEB00060_00-GTICLUB1', 'update': 'NPEB00060_00-GTICLUB1', 'pack': 'CARPACK'}


class App:
    def __init__(self, root):
        self.root = root
        p = load_prefs()
        self.li = p.get('lang', windows_lang())
        self.sel = {k: list(p.get('sel', {}).get(k, [])) for k in ('game', 'update', 'pack')}
        self.out = p.get('out', os.path.join(ROOT, 'Racing Club+'))
        self.proc, self.q, self.step, self.ok, self.log_shown = None, queue.Queue(), 0, False, False
        self.page = 'install'
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

        tabs = tk.Frame(r, bg=WHITE)
        tabs.pack(fill='x', padx=26, pady=(10, 0))
        for key, page in (('tab_inst', 'install'), ('tab_online', 'online')):
            on = self.page == page
            l = tk.Label(tabs, text=self.t(key), bg=WHITE, fg=INK if on else GREY, font=('Segoe UI Semibold' if on else 'Segoe UI', 11), cursor='hand2')
            l.pack(side='left', padx=(0, 18))
            l.bind('<Button-1>', lambda e, pg=page: self.show_page(pg))
            if on: tk.Frame(tabs, bg=hexrgb(mix(BLUE_A, BLUE_B, 0.5)), height=3).place(in_=l, relx=0, rely=1.0, relwidth=1.0, y=1)
        tk.Frame(r, bg=LINE, height=1).pack(fill='x', padx=26, pady=(6, 0))
        if self.page == 'online':
            self.build_online()
            return
        body = tk.Frame(r, bg=WHITE)
        body.pack(fill='x', padx=26, pady=(12, 0))
        self.status = {}
        for n, key, files in (('1', 'game', True), ('2', 'update', True), ('3', 'pack', True)):
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

    def show_page(self, page):
        if self.proc and page != 'install': pass   # the installation keeps running; its page comes back with its state
        self.page = page; self.build()

    def build_online(self):
        box = tk.Text(self.root, height=24, width=70, bg=WHITE, fg=INK, relief='flat', wrap='word', font=('Segoe UI', 9),
                      padx=4, pady=4, cursor='arrow', highlightthickness=0, spacing1=2, spacing3=2)
        box.tag_configure('h', font=('Segoe UI Semibold', 11), foreground=hexrgb(BLUE_B), spacing1=10, spacing3=4)
        box.tag_configure('step', lmargin1=4, lmargin2=22)
        box.tag_configure('note', foreground=GREY)
        n = 0
        for line in self.t('online').split('\n'):
            if line.startswith('# '): box.insert('end', line[2:] + '\n', 'h'); n = 0
            elif line.startswith('- '): n += 1; box.insert('end', '%d.  %s\n' % (n, line[2:]), 'step')
            elif line.strip(): box.insert('end', line + '\n', 'note')
        box.configure(state='disabled')
        box.pack(fill='both', padx=26, pady=(8, 14))

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
        names = {'game': 's_game', 'update': 's_upd', 'pack': 's_pack'}
        hints = {'game': 'h_game', 'update': 'h_upd', 'pack': 'h_pack'}
        tk.Label(top, text=self.t(names[key]), bg=WHITE, fg=INK, font=('Segoe UI Semibold', 11)).pack(side='left', padx=(8, 0))
        tk.Label(box, text=self.t(hints[key]), bg=WHITE, fg=GREY, font=('Segoe UI', 9), anchor='w').pack(fill='x', padx=(30, 0))
        row = tk.Frame(box, bg=WHITE); row.pack(fill='x', padx=(30, 0), pady=(3, 0))
        label = self.t('pick1')
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
        if key == 'game' and pkg and not rap: rap = find_rap(pkg)   # not asked for: the licence bought with the game
        bad = (pkg and KIND[key] not in pkg_id(pkg)) or (rap and KIND[key] not in os.path.basename(rap).upper())
        if key in ('game', 'update') and pkg and not bad:   # game and update share their id: the game's .pkg is the big one
            other = self.sel['update' if key == 'game' else 'game']
            op = next((f for f in other if f.lower().endswith('.pkg')), '')
            if op and os.path.exists(op) and os.path.exists(pkg) and KIND['game'] in pkg_id(op):
                big = os.path.getsize(pkg) > os.path.getsize(op)
                bad = big if key == 'update' else not big
        return pkg, rap, bad

    def section_ok(self, key):
        pkg, rap, bad = self.parts(key)
        if key == 'game': return bool(pkg and rap and not bad)
        if key == 'update': return bool(pkg and not bad)
        return not bad   # the car pack is optional (its licence is not needed: its files are not encrypted)

    def ready(self):
        return all(self.section_ok(k) for k in ('game', 'update', 'pack'))

    def refresh(self):
        if self.page != 'install': return
        for key in ('game', 'update', 'pack'):
            pkg, rap, bad = self.parts(key)
            if not pkg and not rap:
                txt, col = '', GREY
            elif bad:
                txt, col = '✗ ' + self.t('wrong'), ERR
            else:
                bits = ['✓ ' + os.path.basename(pkg)] if pkg else ['✗ ' + self.t('nopkg')]
                if key == 'game': bits.append(('✓ ' + self.t('licence')) if rap else ('✗ ' + self.t('nolic')))
                txt, col = '   '.join(bits), OK if self.section_ok(key) and pkg else WARN
            self.status[key].config(text=txt, fg=col)
        o = self.out if len(self.out) <= 52 else self.out[:22] + '…' + self.out[-29:]   # keeps the window at its width
        self.where_lbl.config(text='%s: %s' % (self.t('where'), o))
        if self.ok: self.draw_button(self.t('play'), True)
        elif not self.proc: self.draw_button(self.t('install'), self.ready())
        if not self.proc and not self.ok:
            self.set_progress(0, '')

    def pick(self, key):
        start = next((os.path.dirname(f) for v in self.sel.values() for f in v), None)
        title = self.t({'game': 's_game', 'update': 's_upd', 'pack': 's_pack'}[key])
        if key == 'game':   # the .pkg and its licence, together or one after the other
            fs = filedialog.askopenfilenames(title=title, filetypes=[('PS3 .pkg / .rap', '*.pkg *.rap'), ('*', '*.*')], initialdir=start)
            if fs:
                new = [os.path.normpath(f) for f in fs]
                keep = [f for f in self.sel[key] if not any(f.lower()[-4:] == n.lower()[-4:] for n in new)]
                self.sel[key] = keep + new
        else:
            f = filedialog.askopenfilename(title=title, filetypes=[('PS3 .pkg', '*.pkg'), ('*', '*.*')], initialdir=start)
            if f: self.sel[key] = [os.path.normpath(f)]
        self.save(); self.refresh()

    def pick_out(self):
        d = filedialog.askdirectory(title=self.t('where'), initialdir=os.path.dirname(self.out) or None)
        if d: self.out = os.path.join(os.path.normpath(d), 'Racing Club+'); self.save(); self.refresh()

    def save(self):
        try: json.dump({'lang': self.li, 'sel': self.sel, 'out': self.out}, open(PREFS, 'w', encoding='utf-8'), indent=1)
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
        (g, gr, _), (u, _, _), (p, _, _) = self.parts('game'), self.parts('update'), self.parts('pack')
        cmd = [py, '-u', os.path.join(ROOT, 'instalar.py'), '--out', self.out, '--idioma', str(self.li + 1),
               '--pkg'] + [x for x in (g, u, p) if x] + ['--rap', gr]
        self.proc = subprocess.Popen(cmd, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, creationflags=flags, text=True, encoding='utf-8', errors='replace')
        for line in self.proc.stdout: self.q.put(('line', line.rstrip()))
        rc = self.proc.wait()
        self.proc = None
        self.q.put(('end', rc))

    def poll(self):
        if self.page != 'install': self.root.after(100, self.poll); return   # messages wait in the queue for the install page
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
