from PIL import Image, ImageDraw
import math, wave, struct
S=512
im=Image.new('RGB',(S,S),(2,4,8)); d=ImageDraw.Draw(im)
for r,a in [(210,22),(180,28),(145,36)]:
    c=(10+a//4,15+a//3,24+a//2)
    d.ellipse((S//2-r,S//2-r,S//2+r,S//2+r),outline=c,width=max(2,a//6))
red=(245,20,32); blue=(15,111,255)
d.rounded_rectangle((70,85,230,245),22,fill=red)
d.rounded_rectangle((282,85,442,245),22,fill=blue)
poly1=[(130,405),(178,405),(360,112),(317,112)]
poly2=[(344,112),(389,112),(248,405),(199,405)]
d.polygon(poly1,fill=(8,10,14)); d.polygon(poly2,fill=(8,10,14))
poly1=[(139,396),(170,396),(347,121),(322,121)]
poly2=[(350,121),(380,121),(239,396),(208,396)]
d.polygon(poly1,fill=(158,163,169)); d.polygon(poly2,fill=(238,241,244))
for i in range(3):
    d.line((85+i,260,190+i,430),fill=(255,45,68),width=2)
    d.line((425-i,260,320-i,430),fill=(35,180,255),width=2)
im.resize((48,48),Image.Resampling.LANCZOS).save('icon.png')
ban=Image.new('RGB',(256,128),(2,4,8)); bd=ImageDraw.Draw(ban)
small=im.resize((104,104),Image.Resampling.LANCZOS); ban.paste(small,(76,12))
bd.text((8,8),'GPT',fill=(220,225,235)); bd.text((8,22),'SABER',fill=(220,225,235)); bd.text((8,106),'3DS',fill=(95,225,255))
ban.save('banner.png')
sr=22050; dur=1.25
with wave.open('banner.wav','wb') as w:
    w.setparams((2,2,sr,0,'NONE',''))
    for n in range(int(sr*dur)):
        t=n/sr; env=max(0,1-t/dur)
        v=int(6500*env*(math.sin(2*math.pi*440*t)+.45*math.sin(2*math.pi*660*t)))
        w.writeframesraw(struct.pack('<hh',v,v))
