from pathlib import Path
import json,subprocess,math,struct
from PIL import Image,ImageDraw,ImageFont
import imageio_ffmpeg
R=Path(r'C:\Users\quent\Documents\repos\Botanicus');D=R/'Botanicus/Saved/Diagnostics/PlacementJuice4A1';DEST=R/'Docs/PlacementJuice4A1_Captures';DEST.mkdir(parents=True,exist_ok=True)
r=json.loads((D/'capture_placement.json').read_text(encoding='utf-8'));assert r['passed'],r['errors'];frames=r['frames'];font=ImageFont.truetype(r'C:\Windows\Fonts\arial.ttf',24)
labels={'Preview':'Hologramme autorise','BlockPreview':'Placement interdit : resolv eur natif'.replace('resolv eur','resolveur'),'Unblock':'Retour au bleu','Rotate':'Rotation native','ConfirmPreview':'Confirmation serveur puis POP','PrepareStorageShelfFloorLarge':'Etagere volumineuse existante','LargeConfirm':'Pose de l etagere : POP'}
def read_frame(row):
    p=D/'captures/placement_frames'/row['file']
    if p.suffix=='.bgra':
        data=p.read_bytes();size=struct.unpack('<ii',data[:8]);return Image.frombytes('RGBA',size,data[8:],'raw','BGRA').convert('RGB')
    return Image.open(p).convert('RGB')
ffmpeg=imageio_ffmpeg.get_ffmpeg_exe();out=DEST/'PlacementJuice4A1.mp4';size=read_frame(frames[0]).size
proc=subprocess.Popen([ffmpeg,'-y','-f','rawvideo','-vcodec','rawvideo','-s',f'{size[0]}x{size[1]+64}','-pix_fmt','rgb24','-r','30','-i','-','-an','-vf','pad=ceil(iw/2)*2:ceil(ih/2)*2','-vcodec','libx264','-crf','19','-pix_fmt','yuv420p','-movflags','+faststart',str(out)],stdin=subprocess.PIPE,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
i=0;lastfile=None;im=None;lasttime=frames[-1]['worldSeconds']
for n in range(math.ceil(lasttime*30)):
    target=n/30
    while i+1<len(frames) and frames[i+1]['worldSeconds']<=target:i+=1
    row=frames[i]
    if row['file']!=lastfile:im=read_frame(row);lastfile=row['file']
    canvas=Image.new('RGB',(size[0],size[1]+64),(16,29,35));canvas.paste(im,(0,0));draw=ImageDraw.Draw(canvas)
    draw.text((18,size[1]+17),labels[row['phase']]+'  |  PIE, vitesse reelle, capture muette',font=font,fill=(214,245,239));proc.stdin.write(canvas.tobytes())
proc.stdin.close();error=proc.stderr.read().decode(errors='replace');assert proc.wait()==0,error
selected=[]
for name,predicate in [('hologram_blue',lambda f:f['phase']=='Preview' and f['snapshot'].get('valid')),('hologram_red',lambda f:f['phase']=='BlockPreview' and not f['snapshot'].get('valid')),('pop_grow',lambda f:f['phase'] in ['ConfirmPreview','LargeConfirm'] and .12<f['snapshot']['elapsed']<.20),('pop_overshoot',lambda f:f['phase'] in ['ConfirmPreview','LargeConfirm'] and f['snapshot']['scale']>1.04),('placed_final',lambda f:f['phase'] in ['ConfirmPreview','LargeConfirm'] and f['snapshot']['active']==0 and f['snapshot']['started']>0)]:
    match=[f for f in frames if predicate(f)]
    if match:
        row=match[len(match)//2];img=read_frame(row);img.save(DEST/(name+'.png'));selected.append(dict(name=name,frame=row))
(D/'video_manifest.json').write_text(json.dumps({'output':str(out),'frames':len(frames),'duration':lasttime,'world_frame_delta_max':max(b['worldSeconds']-a['worldSeconds'] for a,b in zip(frames,frames[1:])),'selected':selected},indent=2),encoding='utf-8')
print(json.dumps({'output':str(out),'duration':lasttime,'sourceFrames':len(frames),'selected':[s['name'] for s in selected]}))
