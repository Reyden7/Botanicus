from pathlib import Path
import json,subprocess,imageio_ffmpeg,argparse
from PIL import Image
parser=argparse.ArgumentParser();parser.add_argument('--data-dir',required=True,type=Path);parser.add_argument('--output-dir',type=Path,default=Path(__file__).parent);args=parser.parse_args()
OUT=args.output_dir;OUT.mkdir(parents=True,exist_ok=True);CAP=args.data_dir/'captures'
j=json.loads((args.data_dir/'capture_noctiflores.json').read_text(encoding='utf-8'))
lines=["file '"+str(CAP/'comparison_before.png').replace('\\','/')+"'",'duration 2']
frames=j['frames']
for i,row in enumerate(frames):
    p=CAP/'comparison_frames'/('frame_%03d.png'%row['index'])
    duration=max(.04,min(.7,frames[i+1]['worldSeconds']-row['worldSeconds'])) if i+1<len(frames) else .2
    lines += ["file '"+str(p).replace('\\','/')+"'",'duration '+str(duration)]
lines.append(lines[-2])
manifest=OUT/'comparison_concat.txt';manifest.write_text('\n'.join(lines),encoding='utf-8')
video=OUT/'noctiflores_before_after.mp4'
subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(),'-y','-loglevel','error','-safe','0','-f','concat','-i',str(manifest),'-vf',r"pad=ceil(iw/2)*2:ceil(ih/2)*2,setsar=1,drawtext=fontfile='C\:/Windows/Fonts/segoeui.ttf':text='Avant - 3C OFF':enable='lt(t,2)':x=20:y=20:fontsize=24:fontcolor=white:box=1:boxcolor=black@0.5,drawtext=fontfile='C\:/Windows/Fonts/segoeui.ttf':text='Apres - 3C ON':enable='gte(t,2)':x=20:y=20:fontsize=24:fontcolor=white:box=1:boxcolor=black@0.5",'-r','30','-c:v','libx264','-crf','18','-pix_fmt','yuv420p','-movflags','+faststart',str(video)],check=True)
print('Video:',video,video.stat().st_size,'bytes; capture dimensions',Image.open(CAP/'comparison_before.png').size)
