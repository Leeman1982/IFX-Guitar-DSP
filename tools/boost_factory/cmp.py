import numpy as np,sys
a=np.loadtxt('spice.txt'); b=np.loadtxt('model.txt')
ts,ys=a[:,0],a[:,1]; tm,ym=b[:,0],b[:,1]
ysi=np.interp(tm,ts,ys)
sel=tm>0.1  # skip settling
e=ym[sel]-ysi[sel]; r=np.sqrt(np.mean((ysi[sel]-ysi[sel].mean())**2))
print("spice rms(ac) %.4g  model rms(ac) %.4g  dc s/m %.4g/%.4g  err rms %.3g (%.1f dB rel)  maxerr %.3g"%(r,np.std(ym[sel]),ysi[sel].mean(),ym[sel].mean(),np.sqrt(np.mean((e-e.mean())**2)),20*np.log10(np.sqrt(np.mean((e-e.mean())**2))/r+1e-15),np.max(np.abs(e-e.mean()))))
# harmonic comparison (exact integer cycles of f over last 0.1 s)
import sys
f=float(open('freq.txt').read()) if __import__('os').path.exists('freq.txt') else 440.0
fs=1/(tm[1]-tm[0]); N=int(round(fs*0.1/ (fs/f)))*int(round(fs/f)) if False else int(fs*0.1)
seg=slice(len(tm)-N,len(tm))
def harm(y):
    t=tm[seg]; y=y[seg]-np.mean(y[seg]); out=[]
    for k in range(1,11):
        c=np.exp(-2j*np.pi*k*f*t); out.append(2*abs(np.mean(y*c)))
    return np.array(out)
hs,hm=harm(ysi),harm(ym)
print("harmonics dB (spice/model): "+" ".join("%d:%.1f/%.1f"%(k+1,20*np.log10(a+1e-12),20*np.log10(b+1e-12)) for k,(a,b) in enumerate(zip(hs,hm))))
