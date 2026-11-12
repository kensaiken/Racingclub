# render point cloud (Nx3 numpy) to 3-view PNG
import numpy as np
from PIL import Image, ImageDraw
def render(pts, path, size=400):
    pts = np.asarray(pts, dtype=np.float64)
    img = Image.new('RGB', (size*3, size), 'white'); dr = ImageDraw.Draw(img)
    mn, mx = pts.min(0), pts.max(0); sc = (size-20)/max((mx-mn).max(),1e-9)
    for k,(a,b) in enumerate([(0,1),(2,1),(0,2)]):
        for p in pts:
            x = 10 + (p[a]-mn[a])*sc + k*size; y = size-10-(p[b]-mn[b])*sc
            dr.point((x,y), fill='black')
    img.save(path)
