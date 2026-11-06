from pathlib import Path
import subprocess,array,math,wave,json,hashlib
import imageio_ffmpeg
R=Path(__file__).resolve().parents[2];O=Path(__file__).parent;DEST=R/'Botanicus/SourceArt/Audio/Checkout';DEST.mkdir(parents=True,exist_ok=True)
ff=imageio_ffmpeg.get_ffmpeg_exe();src=Path(r'C:\Users\quent\Documents\Botanicus\sons');manifest=[]
for filename,name in [('scaner bip.mp3','ScannerBeep'),('cash registrer open.mp3','RegisterOpen'),('money pickup.mp3','CoinPickup'),('close Caisse.mp3','RegisterClose')]:
    p=src/filename;data=subprocess.check_output([ff,'-v','error','-i',str(p),'-ac','1','-ar','48000','-f','f32le','-']);samples=array.array('f');samples.frombytes(data)
    peak=max(abs(x) for x in samples);rms=math.sqrt(sum(x*x for x in samples)/len(samples));threshold=10**(-58/20)
    audible=[i for i,x in enumerate(samples) if abs(x)>=threshold];first=0;end=len(samples)
    if audible:
        if audible[0]/48000>.03:first=max(0,audible[0]-480)
        candidate=min(len(samples),audible[-1]+int(.12*48000))
        if (len(samples)-candidate)/48000>.18:end=candidate
    trimmed=samples[first:end];gain_db=min(6.,-6-20*math.log10(max(peak,1e-12)));gain=10**(gain_db/20)
    pcm=array.array('h');fade=240
    for i,x in enumerate(trimmed):
        envelope=min(1.,i/48,(len(trimmed)-1-i)/fade);pcm.append(round(max(-.9999,min(.9999,x*gain*max(0,envelope)))*32767))
    out=DEST/('SW_Checkout'+name+'.wav')
    with wave.open(str(out),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(48000);w.writeframes(pcm.tobytes())
    output_peak=max(abs(x) for x in pcm)/32768;output_rms=math.sqrt(sum((x/32768)**2 for x in pcm)/len(pcm))
    manifest.append(dict(id=name,source=str(p),source_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),wav=str(out),source_mono_duration=len(samples)/48000,trim_start=first/48000,trim_end=(len(samples)-end)/48000,duration=len(pcm)/48000,gain_db=gain_db,original_mono_peak_dbfs=20*math.log10(max(peak,1e-12)),original_mono_rms_dbfs=20*math.log10(max(rms,1e-12)),output_peak_dbfs=20*math.log10(max(output_peak,1e-12)),output_rms_dbfs=20*math.log10(max(output_rms,1e-12))))
(DEST/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print(json.dumps(manifest,indent=2))
