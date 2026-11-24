#!/usr/bin/env python3
"""Create a reproducible source package from the explicit project allow-list."""
from pathlib import Path
import argparse
import zipfile

ROOT=Path(__file__).resolve().parents[1]
DIRECTORIES={'src','tests','web','data','third_party','docs','tools','.github','results'}
FILES={'CMakeLists.txt','README.md','TESTING.md','build.ps1','run_viewer.ps1','.gitignore'}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output',type=Path,default=ROOT.parent/'submission/SeamCarving_Source.zip')
    args=ap.parse_args();output=args.output.resolve();output.parent.mkdir(parents=True,exist_ok=True)
    count=0
    with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(ROOT.rglob('*')):
            if not path.is_file():continue
            if path.resolve()==output:continue
            relative=path.relative_to(ROOT)
            if relative.parts[0] not in DIRECTORIES and str(relative) not in FILES:continue
            if '__pycache__' in relative.parts or path.suffix=='.pyc' or relative.parts[:2]==('web','runs'):continue
            if relative.parts[0]=='results' and len(relative.parts)>1 and relative.parts[1] not in {'evaluation','verification'}:continue
            if relative.parts[:2]==('results','verification') and path.suffix=='.txt':continue
            archive.write(path,Path('SeamCarving')/relative);count+=1
    print(f'Wrote {output} ({count} files, {output.stat().st_size:,} bytes)')

if __name__=='__main__':main()
