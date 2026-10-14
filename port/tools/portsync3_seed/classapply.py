import sys,pathlib,re,json,collections,os,glob
sys.path.insert(0,'C:/tmp/sm64ds-portsync3/port/tools')
import merge_rename as M
rn=M.Renamer(pathlib.Path('C:/tmp/ps3_cfgrel/config'),None)
cm=rn.cmap
rows=[l.rstrip('\n') for l in open('C:/tmp/ps3_work/unres_walk_window.exe.txt')]
need=collections.defaultdict(set)
for r in rows:
    s,_,o=r.partition('  <- ')
    m=re.search(r'\((\?[^)]*)\)$',s)
    dec=m.group(1) if m else s
    cls=None
    mm=re.match(r'\?\??[\w$]*@(\w+)@@',dec)
    if dec.startswith('??'):
        mm2=re.match(r'\?\?[0-9](\w+)@@',dec)
        if mm2: cls=mm2.group(1)
    elif mm: cls=mm.group(1)
    elif dec.startswith('@actorport_d16t_'):
        cls=re.match(r'@actorport_d16t_(\w+)@8',dec).group(1)
    if cls and cls in cm:
        for ob in o.split(','): need[ob.strip()].add(cls)
apply='--apply' in sys.argv
for ob,cl in sorted(need.items()):
    stem=ob[:-4]  # drop .obj
    cands=[p for p in glob.glob('C:/tmp/sm64ds-portsync3/port/**/'+stem,recursive=True) if not 'build' in p]
    print(ob,sorted(cl),'->',[os.path.relpath(c,'C:/tmp/sm64ds-portsync3') for c in cands][:2])
    if apply:
        for c in cands[:1]:
            raw=open(c,encoding='utf-8',errors='replace',newline='').read()
            new=rn.rewrite(raw,'c',set(cl))
            if new!=raw: open(c,'w',encoding='utf-8',newline='').write(new); print('   changed')
