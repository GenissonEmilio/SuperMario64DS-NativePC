"""Prepare a privately supplied ASMP revision 0 dump for the native build. Never downloads game data."""
import argparse, hashlib, json, pathlib, subprocess, sys, zipfile

ROOT=pathlib.Path(__file__).resolve().parents[1]
EXPECTED='ce5829aaa79f06c67140ed1f45cbca738b0a9d0180538c8c1498472d859db032'

def read_rom(path):
    if path.suffix.lower()=='.zip':
        with zipfile.ZipFile(path) as z:
            entries=[e for e in z.infolist() if e.filename.lower().endswith('.nds')]
            if len(entries)!=1 or entries[0].file_size!=16777216:raise ValueError('Use um ZIP com uma ROM ASMP de 16 MiB.')
            data=z.read(entries[0])
    else:data=path.read_bytes()
    if len(data)!=16777216 or data[12:16]!=b'ASMP' or data[30]!=0 or hashlib.sha256(data).hexdigest()!=EXPECTED:
        raise ValueError('Esta base nativa exige a ROM europeia ASMP revisão 0. A ROM USA não é compatível.')
    return data

def main():
    p=argparse.ArgumentParser();p.add_argument('rom',type=pathlib.Path);p.add_argument('--dsd',type=pathlib.Path,default=ROOT/'tools/bin/dsd.exe');a=p.parse_args()
    data=read_rom(a.rom)
    if not a.dsd.is_file():raise FileNotFoundError('Instale dsd 0.11.0 em tools/bin/dsd.exe (veja README-PC.md).')
    target=ROOT/'build/input/sm64ds-eu.nds';target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    for command in [[sys.executable,'tools/unpack.py',str(target)],
                    [str(a.dsd.resolve()),'rom','extract','--rom',str(target),'--output-path',str(ROOT/'extracted/dsd')],
                    [sys.executable,'tools/asset_catalog.py','generate',str(target)]]:
        subprocess.run(command,cwd=ROOT,check=True)
    (target.parent/'import.json').write_text(json.dumps(dict(gameCode='ASMP',revision=0,sha256=EXPECTED),indent=2))
    print('ROM validada e preparada localmente. O ZIP original foi preservado.')

if __name__=='__main__':main()
