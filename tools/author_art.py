"""Rebuild original, hand-pixelled artwork and the native combat font."""
from pathlib import Path
from PIL import Image, ImageDraw
import json, hashlib

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets'
SRC = [(224,248,207),(134,192,108),(48,104,80),(7,24,33)]
SPR = {'.':(101,255,0),'1':SRC[3],'2':SRC[1],'3':SRC[0]}
patterns = {
'ranger':[
'................','.....111111.....','....12222221....','...1233333321...',
'...1231111321...','...1223333221...','....12222221....','.....111111.....',
'...1122222211...','..122232222221..','..122233222221..','...12222222111..',
'....12222221....','....12111221....','...1221..1221...','...1111..1111...'],
'crawler':[
'................','................','....11111111....','...1222222221...',
'...1231221321...','...1222222221...','....12133121....','.....122221.....',
'....11222211....','...1222122221...','..122221222221..','.1112221222111..',
'....12222221....','....12111221....','...1221..1221...','...1111..1111...'],
'runner':[
'................','.......11.......','.....112211.....','....12222221....',
'....12311321....','....12222221....','.....111111.....','......1221......',
'...1112222111...','..122222222221..','..111222221111..','.....122221.....',
'.....121121.....','....122..221....','...121....121...','...11......11...'],
'brute':[
'.....111111.....','...1122222211...','..122222222221..','.12233113322221.',
'.12222222222221.','.12222111122221.','..122233332221..','...1112222111...',
'.11122222222111.','1222222222222221','1222332222332221','1222332222332221',
'.11122222222111.','...1222112221...','...1221..1221...','...1111..1111...'],
'spitter':[
'................','.....111111.....','....12222221....','...1223333221...',
'...1231331321...','...1223333221...','....12222221....','.....123321.....',
'...1122332211...','..122223322221..','..122223322221..','...1222332221...',
'....12222221....','....12211221....','...1221..1221...','...1111..1111...'],
'boss':[
'...11......11...','..122111111221..','.12222222222221.','1223332222333221',
'1223112222113221','1222222222222221','.12223333332221.','..122311113221..',
'.11223333332211.','1222222222222221','1233222332223321','1233222332223321',
'1222222222222221','.1122222222211..','..12211..11221..','..11111..11111..'],
'bolt':[
'................','................','................','................',
'................','.......11.......','......1331......','.....133331.....',
'.....133331.....','......1331......','.......11.......','................',
'................','................','................','................'],
'acid':[
'................','................','................','................',
'......1111......','.....122221.....','....12333321....','....12333321....',
'....12333321....','....12333321....','.....122221.....','......1111......',
'................','................','................','................'],
}
for name, rows in patterns.items():
    image = Image.new('RGB',(16,16))
    image.putdata([SPR[v] for row in rows for v in row])
    if name == 'ranger':
        sheet=Image.new('RGB',(96,16),SPR['.'])
        for i in range(6):
            frame=image.copy()
            if i%2:
                d=ImageDraw.Draw(frame)
                d.rectangle((3,14,6,15), fill=SPR['.'])
                d.rectangle((4,13,6,14), fill=SPR['1'])
                d.rectangle((9,14,12,15), fill=SPR['1'])
            if i in (2,3):
                d=ImageDraw.Draw(frame);d.rectangle((5,4,10,5),fill=SPR['2'])
            sheet.paste(frame,(i*16,0))
        image=sheet
    if name in ('bolt','acid'):
        # A projectile fits in one 8x16 hardware object; center that object
        # in the 16px logical canvas rather than allocating two objects.
        image=Image.new('RGB',(16,16),SPR['.'])
        rows=(['........']*5+['...11...','..1331..','.133331.','..1331..','...11...']+['........']*6) if name=='bolt' else (['........']*4+['..1111..','.122221.','12333321','12333321','.122221.','..1111..']+['........']*6)
        for yy,row in enumerate(rows):
            for xx,v in enumerate(row):image.putpixel((xx,yy),SPR[v])
    dest=OUT/'sprites'/f'{name}.png';image.save(dest)
    meta=dest.with_suffix('.png.gbsres')
    if name in ('bolt','acid') and meta.exists():
        data=json.loads(meta.read_text());data['numTiles']=1;data['checksum']=hashlib.sha1(dest.read_bytes()).hexdigest()
        for state in data['states']:
            for anim in state['animations']:
                for frame in anim['frames']:
                    if frame['tiles']:
                        frame['tiles']=frame['tiles'][:1];frame['tiles'][0]['x']=4
        meta.write_text(json.dumps(data,indent=2)+'\n')

# Eight-pixel authored tiles; repeated patterns keep cartridge use predictable.
tiles=[]
def tile(rows):
    im=Image.new('RGB',(8,8));im.putdata([SRC[int(c)] for row in rows for c in row]);tiles.append(im)
tile(['33333333']*8)
tile(['22222222','22322222','22222222','22222222','22222222','22222222','22222232','22222222'])
tile(['22222222','22222222','22232222','22223222','22223222','22222322','22222222','22222222'])
tile(['33333333','31111113','31222213','31233213','31233213','31222213','31111113','33333333'])
tile(['33333333','31111113','31222213','31222213','31111113','31222213','31222213','33333333'])
tile(['33333333','33003300','30033003','00330033','03300330','33003300','30033003','33333333'])
tile(['33333333','33333333','11111111','03300330','11111111','33333333','33333333','33333333'])
tile(['33311333','33301333','33311333','33301333','33311333','33301333','33311333','33301333'])
tile(['33333333','33000333','33030333','33000333','33333333','33333333','33333333','33333333'])
tile(['11111111','13333331','13222231','13222231','13222231','13222231','13333331','11111111'])
tile(['22222222','22222222','22222222','21111112','21133112','21111112','22222222','22222222'])
grid=[]
for y in range(18):
    row=[]
    for x in range(20):
        if y<3 or y>15: t=0
        elif y in (3,15):t=5 if 7<x<12 else 4
        elif x in (0,19): t=7
        else:t=2 if (x*7+y*3)%19==0 else 1
        if (x,y) in [(2,5),(17,5),(2,13),(17,13)]:t=3
        if (x,y) in [(5,7),(14,11)]:t=10
        if (x,y) in [(4,4),(15,4),(4,14),(15,14)]:t=8
        row.append(t)
    grid.append(row)
image=Image.new('RGB',(160,144))
for y,row in enumerate(grid):
    for x,t in enumerate(row):image.paste(tiles[t],(8*x,8*y))
image.save(OUT/'backgrounds'/'deadzone.png')

# Native font matches the editable project font, with light ink on a dark field.
font=Image.open(OUT/'fonts'/'bench-mono.png').convert('RGB')
data=[]
for ch in range(32,96):
    glyph=font.crop((((ch-32)%16)*8,((ch-32)//16)*8,((ch-32)%16)*8+8,((ch-32)//16)*8+8))
    for y in range(8):
        lo=hi=0
        for x in range(8):
            v=0 if glyph.getpixel((x,y))==SRC[3] else 3
            lo|=(v&1)<<(7-x);hi|=((v>>1)&1)<<(7-x)
        data.extend([lo,hi])
header='#ifndef EF_ART_H\n#define EF_ART_H\nstatic const unsigned char ef_font[] = {\n'
header+=','.join(str(v) for v in data)+'\n};\n#endif\n'
p=ROOT/'plugins'/'emberfall'/'engine'/'include'/'states';p.mkdir(parents=True,exist_ok=True)
(p/'ef_art.h').write_text(header)
(ROOT/'art-source').mkdir(exist_ok=True)
(ROOT/'art-source'/'sprites.json').write_text(json.dumps(patterns,indent=2))
print('Authored 8 sprite sheets, arena tiles, editable pixel grids and native font.')
