"""Input-only ROM regression. Reads compiler symbols; never writes game memory."""
import hashlib,json,re,sys
from pathlib import Path
from pyboy import PyBoy

ROOT=Path(__file__).resolve().parents[1]
ROM=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else ROOT/'build/Emberfall.gbc'
OUT=ROOT/'build'/('verification-web' if len(sys.argv)>1 else 'verification');OUT.mkdir(exist_ok=True)
symbols={m[0]:int(m[1],16) for m in re.findall(r'DEF (_ef_\w+) 0x([0-9A-Fa-f]+)',(ROOT/'build/Emberfall.gbc.debug/symbols.noi').read_text())}

def run(name,active):
    p=PyBoy(str(ROM),window='null',sound_emulated=False)
    p.set_emulation_speed(0)
    held=set();frames=0;events=[];captured=set();bosses=set();prior=None;failed=False;peak_scanline=0;overlimit_samples=0
    def step(keys=(),count=1):
        nonlocal held,frames,peak_scanline,overlimit_samples
        keys=set(keys)
        for key in held-keys:p.button_release(key)
        for key in keys-held:p.button_press(key)
        held=keys;p.tick(count,True);frames+=count
        rows=[0]*144
        for a in range(0xFE00,0xFEA0,4):
            y=p.memory[a]-16
            if y<=-16 or y>=144:continue
            for r in range(max(0,y),min(144,y+16)):rows[r]+=1
        peak=max(rows);peak_scanline=max(peak_scanline,peak)
        if peak>10:overlimit_samples+=1
    def byte(n):return p.memory[symbols['_ef_'+n]]
    def word(n):a=symbols['_ef_'+n];return p.memory[a]+256*p.memory[a+1]
    def capture(label):
        path=OUT/f'{name}-{label}.png';p.screen.image.save(path);return str(path)
    def event(kind,**kwargs):
        item={'kind':kind,'frame':frames,'zone':byte('stage'),'hp':word('hp'),'level':byte('level'),**kwargs};events.append(item);print(json.dumps(item),flush=True)
    step(count=120);assert byte('mode')==0;capture('title')
    step(['select'],2);step(count=30);assert byte('mode')==6;capture('guide')
    step(['a'],2);step(count=30);assert byte('mode')==0
    step(['start'],2);step(count=10);assert byte('mode')==1
    # Pause freezes combat under ordinary input and a five-second wait.
    step(['start'],2);assert byte('mode')==5
    frozen=(word('hp'),byte('x'),byte('y'),byte('kills'))
    step(['left','b'],300);assert frozen==(word('hp'),byte('x'),byte('y'),byte('kills'))
    capture('pause');step(['start'],2);step(count=1);assert byte('mode')==1
    event('pause-pass')
    while frames<90000:
        mode=byte('mode');zone=byte('stage')
        if zone!=prior:
            event('zone-entered',attack=byte('atk'),delay=byte('rate'));prior=zone
        if mode==4:
            step(count=30)
            capture('victory');event('victory',kills=word('total'));break
        if mode==3:
            step(count=30)
            capture('defeat');event('defeat',kills=word('total'))
            step(['a'],2);step(count=45);assert byte('mode')==1 and byte('stage')==1 and word('hp')==60
            event('retry-pass');failed=True;break
        if mode==2:
            step(count=30)
            capture(f'zone-{zone:02}-clear');event('zone-cleared',kills=byte('kills'))
            # Exercise all three salvage branches through visible menu navigation.
            choice=(2,1,0)[(zone-1)%3] if active else 0
            for _ in range(choice):step(['down'],20);step(count=20)
            assert byte('choice')==choice
            step(['a'],2);step(count=45);continue
        base=symbols['_ef_foes']
        foes=[(p.memory[base+i*8],p.memory[base+i*8+1],p.memory[base+i*8+2]) for i in range(6) if p.memory[base+i*8+3]]
        if any(k==4 for x,y,k in foes) and zone not in bosses:
            bosses.add(zone);capture(f'zone-{zone:02}-boss');event('boss-encountered')
        if zone not in captured and len(foes)>=2:
            captured.add(zone);capture(f'zone-{zone:02}-combat')
        if not active:step(count=12);continue
        x,y=byte('x'),byte('y');keys=[]
        if foes:
            near=min(abs(x-fx)+abs(y-fy) for fx,fy,k in foes)
            # Choose a legal short direction that keeps distance from live foes
            # while avoiding corners. Same ordinary controls a human can use.
            best=(-1e9,())
            for dx,dy in [(0,-1),(1,-1),(1,0),(1,1),(0,1),(-1,1),(-1,0),(-1,-1),(0,0)]:
                nx,ny=x+dx*12,y+dy*12
                if nx<20 or nx>140 or ny<52 or ny>116:continue
                dists=[abs(nx-fx)+abs(ny-fy) for fx,fy,k in foes]
                score=min(dists)*2+sum(min(d,65) for d in dists)*.06
                score-=abs(nx-80)*.10+abs(ny-84)*.16
                if score>best[0]:best=(score,(dx,dy))
            if best[1]:
                dx,dy=best[1]
                if dx:keys.append('right' if dx>0 else 'left')
                if dy:keys.append('down' if dy>0 else 'up')
            if near<65 and not byte('pulse_cd'):keys.append('b')
            if near<20 and not byte('dash_cd'):keys.append('a')
        step(keys,6)
    else:event('timeout')
    report={'rom_sha256':hashlib.sha256(ROM.read_bytes()).hexdigest(),'run':name,'input_only':True,'memory_writes':False,'frames':frames,'victory':byte('mode')==4,'defeat_observed':failed,'bosses':sorted(bosses),'sampled_peak_sprites_per_scanline':peak_scanline,'overlimit_samples':overlimit_samples,'events':events}
    (OUT/f'{name}.json').write_text(json.dumps(report,indent=2))
    p.stop(save=False)
    return report

if __name__=='__main__':
    reports=[run('idle',False),run('active',True)]
    print(json.dumps({'runs':[{k:v for k,v in r.items() if k!='events'} for r in reports]},indent=2))
