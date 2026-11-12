"""Save RGB + alpha previews of textures whose name contains any of the given substrings."""
import sys
from ep3 import EP3
import gcmtex
from PIL import Image

S = r'C:/Users/User/AppData/Local/Temp/claude/a7f51cd7dbe32f9c/dacd3166-da07-4620-beef-fec5a8ae9625/scratchpad/'
e = EP3(sys.argv[1])
keys = sys.argv[2:]
tiles = []
for it in e.items:
    if it.tag != 'EMTH': continue
    n = e.name(it)
    if any(k.lower() in n.lower() for k in keys):
        im = gcmtex.decode(e.texture(it))
        tiles.append(im.convert('RGB').resize((256, 256))); tiles.append(im.getchannel('A').convert('RGB').resize((256, 256)))
        print(len(tiles) // 2 - 1, n, im.size)
out = Image.new('RGB', (256 * len(tiles), 256))
for i, t in enumerate(tiles): out.paste(t, (256 * i, 0))
out.save(S + 'peek.png')
