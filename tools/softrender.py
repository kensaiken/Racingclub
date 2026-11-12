"""Tiny textured software rasterizer for verifying exported OBJ (usage: softrender.py model.obj out.png yaw pitch)."""
import sys, os, math
import numpy as np
from PIL import Image

def load_obj(path):
    V=[];VT=[];VN=[];F=[];mtl={};cur=None;mats={}
    d=os.path.dirname(path)
    for line in open(path):
        p=line.split()
        if not p: continue
        if p[0]=='v': V.append([float(x) for x in p[1:4]])
        elif p[0]=='vt': VT.append([float(x) for x in p[1:3]])
        elif p[0]=='vn': VN.append([float(x) for x in p[1:4]])
        elif p[0]=='usemtl': cur=p[1]
        elif p[0]=='mtllib':
            m=None
            for l in open(os.path.join(d,p[1])):
                q=l.split()
                if not q: continue
                if q[0]=='newmtl': m=q[1]
                elif q[0]=='map_Kd': mats[m]=np.asarray(Image.open(os.path.join(d,q[1])).convert('RGB'),dtype=np.float32)/255
        elif p[0]=='f':
            F.append(([int(x.split('/')[0])-1 for x in p[1:4]],[int(x.split('/')[1])-1 for x in p[1:4]],cur))
    return np.array(V),np.array(VT),F,mats

def render(path,out,yaw=35,pitch=15,W=960,H=600):
    V,VT,F,mats=load_obj(path)
    c=(V.min(0)+V.max(0))/2; r=np.linalg.norm(V.max(0)-V.min(0))/2
    P=V-c
    cy,sy=math.cos(math.radians(yaw)),math.sin(math.radians(yaw)); cp,sp=math.cos(math.radians(pitch)),math.sin(math.radians(pitch))
    Ry=np.array([[cy,0,-sy],[0,1,0],[sy,0,cy]]); Rx=np.array([[1,0,0],[0,cp,-sp],[0,sp,cp]])
    P=P@Ry.T@Rx.T
    dist=r*3.2; z=P[:,2]+dist; f=H*1.25
    sx=W/2+P[:,0]*f/z; sy_=H/2-P[:,1]*f/z
    img=np.ones((H,W,3),np.float32)*np.array([0.55,0.68,0.85]); zb=np.full((H,W),1e9,np.float32)
    light=np.array([0.4,0.8,-0.45]); light/=np.linalg.norm(light)
    for vi,ti,m in F:
        x=sx[vi]; y=sy_[vi]; zz=z[vi]
        a=np.cross(P[vi[1]]-P[vi[0]],P[vi[2]]-P[vi[0]]); n=np.linalg.norm(a)
        if n==0: continue
        shade=0.35+0.65*abs(np.dot(a/n,light))
        x0,x1=int(max(0,math.floor(x.min()))),int(min(W-1,math.ceil(x.max()))); y0,y1=int(max(0,math.floor(y.min()))),int(min(H-1,math.ceil(y.max())))
        if x0>x1 or y0>y1: continue
        X,Y=np.meshgrid(np.arange(x0,x1+1)+0.5,np.arange(y0,y1+1)+0.5)
        den=(y[1]-y[2])*(x[0]-x[2])+(x[2]-x[1])*(y[0]-y[2])
        if abs(den)<1e-9: continue
        w0=((y[1]-y[2])*(X-x[2])+(x[2]-x[1])*(Y-y[2]))/den; w1=((y[2]-y[0])*(X-x[2])+(x[0]-x[2])*(Y-y[2]))/den; w2=1-w0-w1
        inside=(w0>=0)&(w1>=0)&(w2>=0)
        if not inside.any(): continue
        Z=w0*zz[0]+w1*zz[1]+w2*zz[2]
        sub=zb[y0:y1+1,x0:x1+1]; vis=inside&(Z<sub)
        if not vis.any(): continue
        tex=mats.get(m)
        if tex is not None and len(VT):
            # perspective-correct uv
            iz=w0/zz[0]+w1/zz[1]+w2/zz[2]
            u=(w0*VT[ti[0],0]/zz[0]+w1*VT[ti[1],0]/zz[1]+w2*VT[ti[2],0]/zz[2])/iz
            v=(w0*VT[ti[0],1]/zz[0]+w1*VT[ti[1],1]/zz[1]+w2*VT[ti[2],1]/zz[2])/iz
            th,tw=tex.shape[:2]
            tu=((u%1)*tw).astype(int).clip(0,tw-1); tv=(((1-v)%1)*th).astype(int).clip(0,th-1)
            col=tex[tv,tu]
        else:
            col=np.broadcast_to(np.array([0.7,0.7,0.7],np.float32),(y1-y0+1,x1-x0+1,3))
        reg=img[y0:y1+1,x0:x1+1]
        reg[vis]=col[vis]*shade; sub[vis]=Z[vis]
    Image.fromarray((img.clip(0,1)*255).astype(np.uint8)).save(out)

if __name__=='__main__':
    render(sys.argv[1],sys.argv[2],float(sys.argv[3]) if len(sys.argv)>3 else 35,float(sys.argv[4]) if len(sys.argv)>4 else 15)
