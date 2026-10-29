"""Native integration replay. Hidden window, internal scripted input, isolated saves. No ROM is uploaded."""
import argparse, hashlib, json, os, pathlib, re, subprocess

ROOT=pathlib.Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser();p.add_argument('--exe',type=pathlib.Path,default=ROOT/'build/native/walk_window.exe');p.add_argument('--output',type=pathlib.Path,default=ROOT/'build/verification');a=p.parse_args()
    exe=a.exe.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    base={k:v for k,v in os.environ.items() if not k.startswith('SM64DS_')}
    base.update(SM64DS_ASSET_ROOT=str(ROOT),SM64DS_WINDOW_SELFTEST='180',SM64DS_LEVEL='1',SM64DS_SKIP_INTRO='1',
                SM64DS_ONE_SCREEN_MENUS='1',SM64DS_SUB_PANEL='0',SM64DS_NO_PLAYLOG='1',
                SM64DS_NO_FOCUS='1',SM64DS_NO_DIALOG='1',SM64DS_PC_VERIFY_HIDDEN='1',SM64DS_VOLUME='0',
                SM64DS_BIGMAP_TRACE='1')
    rows=[('idle',{'SM64DS_SELFTEST_IDLE':'1'}),('walk',{}),
          ('map-open',{'SM64DS_SELFTEST_IDLE':'1','SM64DS_HOST_KEY':'4d@100'}),
          ('map-close',{'SM64DS_SELFTEST_IDLE':'1','SM64DS_HOST_KEY':'4d@100,4d@150'})]
    results=[]
    for name,extra in rows:
        folder=out/name;folder.mkdir(exist_ok=True)
        env=base|extra;env['SM64DS_SAVE_PATH']=str(folder/'save/sm64ds.sav');(folder/'save').mkdir(exist_ok=True)
        si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
        try:
            with (folder/'run.log').open('w',encoding='utf-8') as log:
                result=subprocess.run([str(exe)],cwd=folder,env=env,stdout=log,stderr=subprocess.STDOUT,
                                      startupinfo=si,creationflags=subprocess.CREATE_NO_WINDOW,timeout=180)
        except OSError as e:
            report=dict(success=False,desktopInput=False,blockedByWindows=getattr(e,'winerror',None)==4551,
                        error=str(e),results=results)
            (out/'verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
            raise SystemExit('Native execution unavailable: '+str(e))
        text=(folder/'run.log').read_text(encoding='utf-8',errors='replace')
        images=list(folder.glob('*.bmp'))
        item=dict(name=name,exitCode=result.returncode,hidden='SW_HIDE' in text,
                  position=re.findall(r'selftest:.*pos=\(([^)]+)\)',text),
                  mapOpens=text.count('open: Select'),mapCloses=text.count('close: Select'),
                  pictures={i.name:hashlib.sha256(i.read_bytes()).hexdigest() for i in images})
        results.append(item);print(json.dumps(item),flush=True)
    checks={
        'all native runs exit cleanly':all(r['exitCode']==0 for r in results),
        'all windows hidden':all(r['hidden'] for r in results),
        'frames captured':all(r['pictures'] for r in results),
        'M opens native map':results[2]['mapOpens']==1,
        'M closes native map':results[3]['mapOpens']==1 and results[3]['mapCloses']==1,
        'walking changes player position':bool(results[0]['position'] and results[1]['position']) and results[0]['position']!=results[1]['position'],
        'map round trip preserves final picture':results[0]['pictures']==results[3]['pictures'],
    }
    report=dict(success=all(checks.values()),desktopInput=False,hiddenWindows=True,checks=checks,results=results)
    (out/'verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    if not report['success']:raise SystemExit(1)

if __name__=='__main__':main()
