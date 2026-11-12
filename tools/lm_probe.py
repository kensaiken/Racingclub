import collections
from ep3 import EP3
import gcmtex
from PIL import Image

S = r'C:/Users/User/AppData/Local/Temp/claude/a7f51cd7dbe32f9c/dacd3166-da07-4620-beef-fec5a8ae9625/scratchpad/'
e = EP3("../../NPEB00060/USRDIR/data/levels/cote d'azur/afterburn.ep3")
lms = [it for it in e.items if it.tag == 'EMTH' and 'AfterBurner_' in e.name(it)]
print(len(lms), 'lightmaps')
t = e.texture(lms[0]); print(e.name(lms[0]), t['width'], t['height'], hex(t['format']))
im = gcmtex.decode(t)
rgb = im.convert('RGB').resize((512, 512)); a = im.getchannel('A').convert('RGB').resize((512, 512))
o = Image.new('RGB', (1024, 512)); o.paste(rgb, (0, 0)); o.paste(a, (512, 0)); o.save(S + 'lightmap.png')
print(collections.Counter((e.texture(x)['width'], hex(e.texture(x)['format'])) for x in lms))
