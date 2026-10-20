import numpy as np,sys
f0=float(sys.argv[1]); x=np.fromfile('spec.raw',dtype=np.float32).astype(float); x-=x.mean(); N=len(x)
X=np.abs(np.fft.rfft(x*np.blackman(N)))**2; f=np.fft.rfftfreq(N,1/48000)
harm=np.zeros_like(f,bool)
for k in range(0,int(24000/f0)+2): harm|=np.abs(f-f0*k)<8
aud=(f>20)&(f<16000); print('%.1f dB'%(10*np.log10(X[(~harm)&aud].sum()/X[harm].sum())),end=' ')
