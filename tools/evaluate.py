#!/usr/bin/env python3
"""Reproduce image comparisons and timing tables; Pillow is only for figures/preparation."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]

def build_metadata(exe):
    """Read actual CMake metadata when available; never guess compiler/configuration."""
    cache=next((base/'CMakeCache.txt' for base in (exe.parent,exe.parent.parent)
                if (base/'CMakeCache.txt').is_file()),None)
    values={}
    compiler={}
    if cache:
        for line in cache.read_text(encoding='utf-8',errors='replace').splitlines():
            match=re.match(r'([^:#]+):[^=]+=(.*)',line)
            if match:values[match[1]]=match[2]
        for info in sorted((cache.parent/'CMakeFiles').glob('*/CMakeCXXCompiler.cmake')):
            content=info.read_text(encoding='utf-8',errors='replace')
            for key in ('CMAKE_CXX_COMPILER_ID','CMAKE_CXX_COMPILER_VERSION'):
                match=re.search(r'set\('+key+r' "([^"]*)"\)',content)
                if match:compiler[key]=match[1]
    known={'debug','release','relwithdebinfo','minsizerel'}
    configuration=exe.parent.name if exe.parent.name.lower() in known else values.get('CMAKE_BUILD_TYPE','unknown')
    return {'configuration':configuration or 'unknown',
            'compiler_id':compiler.get('CMAKE_CXX_COMPILER_ID','unknown'),
            'compiler_version':compiler.get('CMAKE_CXX_COMPILER_VERSION','unknown'),
            'generator':values.get('CMAKE_GENERATOR','unknown'),
            'configuration_flags':values.get('CMAKE_CXX_FLAGS_'+configuration.upper(),'unknown'),
            'metadata_source':'adjacent CMake build metadata' if cache else 'not available beside executable'}

def invoke(exe, arguments, timeout=180):
    completed = subprocess.run([str(exe), *map(str,arguments)],capture_output=True,text=True,encoding='utf-8',timeout=timeout)
    if completed.returncode:
        raise RuntimeError(completed.stderr)
    return json.loads(completed.stdout)

def contact_sheet(panels, output):
    cell_w,cell_h=256,240
    sheet=Image.new('RGB',(cell_w*len(panels),cell_h),'#f5f6f8')
    draw=ImageDraw.Draw(sheet)
    with Image.open(panels[0][1]) as original:
        display_scale=min(240/original.width,185/original.height)
    for i,(title,path) in enumerate(panels):
        im=Image.open(path).convert('RGB')
        # Equal display scale factor preserves visible aspect ratio differences.
        im=im.resize((max(1,round(im.width*display_scale)),max(1,round(im.height*display_scale))),Image.Resampling.NEAREST)
        x=cell_w*i+(cell_w-im.width)//2;y=35+(185-im.height)//2
        sheet.paste(im,(x,y));draw.text((cell_w*i+12,12),title,fill='#182536')
        draw.text((cell_w*i+12,220),f'{Image.open(path).width} x {Image.open(path).height}',fill='#526174')
    sheet.save(output)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--exe',type=Path,required=True)
    ap.add_argument('--output',type=Path,default=ROOT/'results/evaluation')
    ap.add_argument('--repeats',type=int,default=5)
    ap.add_argument('--max-side',type=int,default=96)
    args=ap.parse_args()
    if not 1<=args.repeats<=100 or args.max_side<16:ap.error('Use 1..100 repeats and max-side >=16.')
    exe=args.exe.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    prepared=out/'inputs';prepared.mkdir(exist_ok=True)
    build=build_metadata(exe)
    environment={
        'platform':platform.platform(),'processor':platform.processor(),
        'logical_cpus':os.cpu_count(),'python':sys.version,'build':build,
        'compiler_note':'Unknown fields mean compiler/build provenance could not be verified; retain the original build records.',
        'algorithm_threads':1,'repetitions':args.repeats,'max_side':args.max_side,
        'energy':'integer grayscale; signed Sobel3x3 L1; replicated borders',
        'timing':'steady_clock; sum energy/search/removal stages; one warmup; excludes disk I/O and visualization',
    }
    (out/'environment.json').write_text(json.dumps(environment,indent=2),encoding='utf-8')
    files=sorted((ROOT/'data/demo').glob('scene_*.png'))+sorted((ROOT/'data/photos').glob('*'))
    files=[p for p in files if p.suffix.lower() in {'.png','.jpg','.jpeg'}]
    if not files:raise RuntimeError('No inputs. Run seamcarve demo --output-dir data/demo first.')
    all_rows=[];index=[];manifest=[]
    for p in files:
        im=Image.open(p).convert('RGB');im.thumbnail((args.max_side,args.max_side),Image.Resampling.LANCZOS)
        sample=prepared/(p.stem+'.png');im.save(sample)
        manifest.append({'input':str(p.relative_to(ROOT)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),
                         'prepared_width':im.width,'prepared_height':im.height})
        for percentage in (10,25):
            width=max(1,round(im.width*(100-percentage)/100));height=max(1,round(im.height*(100-percentage)/100))
            directory=out/f'{p.stem}_{percentage}'
            print(f'Comparing {p.stem} {im.width}x{im.height} -> {width}x{height} ({percentage}%)',flush=True)
            report=invoke(exe,['compare','--input',sample,'--output-dir',directory,'--width',width,'--height',height,'--repeats',args.repeats])
            with (directory/'timings.csv').open(newline='',encoding='utf-8') as f:
                for row in csv.DictReader(f):all_rows.append({'image':p.stem,'reduction':percentage,**row})
            im.resize((width,height),Image.Resampling.BILINEAR).save(directory/'scaling.png')
            panels=[('Original',sample),('DP',directory/'dp.png'),('Greedy',directory/'greedy.png'),('Graph cut',directory/'graph.png'),('Scaling',directory/'scaling.png')]
            contact_sheet(panels,directory/'comparison.png')
            index.append((p.stem,percentage,report,directory.name))
    (out/'inputs.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    if all_rows:
        with (out/'all_timings.csv').open('w',newline='',encoding='utf-8') as f:
            writer=csv.DictWriter(f,fieldnames=list(all_rows[0]));writer.writeheader();writer.writerows(all_rows)
    # Single-removal scaling experiments keep orientation explicit.
    scale_rows=[]
    source=ROOT/'data/photos/astronaut.png'
    if not source.exists():source=files[0]
    for side in (32,64,128,256):
        im=Image.open(source).convert('RGB').resize((side,side),Image.Resampling.LANCZOS)
        sample=prepared/f'scale_{side}.png';im.save(sample)
        for axis in ('vertical','horizontal'):
            directory=out/f'scale_{side}_{axis}'
            print(f'Scaling single {axis} seam {side}x{side}',flush=True)
            report=invoke(exe,['compare','--input',sample,'--output-dir',directory,'--width',side-1 if axis=='vertical' else side,
                                '--height',side-1 if axis=='horizontal' else side,'--repeats',args.repeats])
            for method in report['methods']:
                scale_rows.append({'side':side,'axis':axis,'method':method['method'],'median_ms':method['median_ms'],
                                   'min_ms':method['min_ms'],'max_ms':method['max_ms']})
    with (out/'scaling.csv').open('w',newline='',encoding='utf-8') as f:
        writer=csv.DictWriter(f,fieldnames=list(scale_rows[0]));writer.writeheader();writer.writerows(scale_rows)
    lines=['# Measured evaluation','',
           'Release measurements generated by `tools/evaluate.py`. Each case starts from the same prepared original, uses vertical-first removal, has one warm-up and '+str(args.repeats)+' measured repetitions. Times are medians in milliseconds. Raw repetitions are in `all_timings.csv`; environment and exact inputs are recorded separately.','',
           '| Input | Reduction in both dimensions | DP ms | Greedy ms | Graph ms | Greedy / DP first vertical cost |','|---|---:|---:|---:|---:|---:|']
    agreements=0;worse=0
    for name,pct,report,directory in index:
        m={x['method']:x for x in report['methods']}
        assert m['dp']['vertical_cost']==m['graph']['vertical_cost']
        assert m['dp']['horizontal_cost']==m['graph']['horizontal_cost']
        agreements+=2
        if m['greedy']['vertical_cost']>m['dp']['vertical_cost']:worse+=1
        denom=m['dp']['vertical_cost'];ratio=f"{m['greedy']['vertical_cost']/denom:.3f}" if denom else ('equal zero' if not m['greedy']['vertical_cost'] else 'positive / zero')
        lines.append(f"| [{name}]({directory}/comparison.png) | {pct}% | {m['dp']['median_ms']:.3f} | {m['greedy']['median_ms']:.3f} | {m['graph']['median_ms']:.3f} | {ratio} |")
    lines+=['',f'DP and graph cut agreed on all {agreements} reported shared-map orientation costs. Greedy had a strictly larger first vertical cost in {worse} of {len(index)} resize cases. These repeat the first seam for each reduction amount; they are not independent photographic samples.','',
            'The app validates every current energy map; public greedy search is therefore linear in image pixels despite its smaller local-choice kernel. Graph construction and max-flow create substantial overhead for this objective. These measurements do not establish a universal speed ratio.','',
            '## Visual checks','',
            'Open the linked comparison panels and each method\'s `_vertical.png` / `_horizontal.png` overlays. Compare salient object shape, thin lines and texture with the original and scaling baseline. DP and graph optimize the same backward-energy objective, so equal optimal energy does not imply identical tied paths or identical final images after repeated removals.','',
            'This is a small, deliberately bounded dataset: eight synthetic patterns and four photographs when the bundled photo set is present. Prepared sizes preserve aspect ratio within the stated max-side limit. Add your own representative images before making broad visual-quality claims. Single-removal scaling timings are in `scaling.csv` (32, 64, 128 and 256 square inputs, each orientation).','']
    (out/'SUMMARY.md').write_text('\n'.join(lines),encoding='utf-8')
    print('Wrote',out/'SUMMARY.md',flush=True)

if __name__=='__main__':main()
