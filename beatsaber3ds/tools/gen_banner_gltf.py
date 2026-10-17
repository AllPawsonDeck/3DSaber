#!/usr/bin/env python3
import json, struct, base64, math

verts=[]; idx=[]; nodes=[]; meshes=[]; mats=[]

def mat(name, rgba):
    mats.append({"name":name,"pbrMetallicRoughness":{"baseColorFactor":rgba,"metallicFactor":0.0,"roughnessFactor":0.55},"doubleSided":True})
    return len(mats)-1

RED=mat("red",[0.72,0.05,0.06,1]); RED2=mat("red2",[0.9,0.12,0.13,1]); BLUE=mat("blue",[0.02,0.33,0.78,1]); NAVY=mat("navy",[0.01,0.10,0.18,1]); WHITE=mat("white",[0.98,0.98,0.98,1])
buffer_views=[]; accessors=[]; meshes2=[]; nodes2=[]; blob=b''

def emit_cube(name,cx,cy,cz,sx,sy,sz,material,rz=0.0,ry=0.0):
    global blob
    p=[(-.5,-.5,-.5),(.5,-.5,-.5),(.5,.5,-.5),(-.5,.5,-.5),(-.5,-.5,.5),(.5,-.5,.5),(.5,.5,.5),(-.5,.5,.5)]
    czr,szr=math.cos(rz),math.sin(rz); cyr,syr=math.cos(ry),math.sin(ry)
    vv=[]
    for x,y,z in p:
        x*=sx;y*=sy;z*=sz; x,y=x*czr-y*szr,x*szr+y*czr; x,z=x*cyr+z*syr,-x*syr+z*cyr; vv.append((x+cx,y+cy,z+cz))
    ii=[0,1,2,0,2,3,4,6,5,4,7,6,0,4,5,0,5,1,1,5,6,1,6,2,2,6,7,2,7,3,3,7,4,3,4,0]
    while len(blob)%4: blob+=b'\0'
    po=len(blob); pb=b''.join(struct.pack('<fff',*v) for v in vv); blob+=pb
    pview=len(buffer_views); buffer_views.append({"buffer":0,"byteOffset":po,"byteLength":len(pb),"target":34962})
    pacc=len(accessors); mins=[min(v[i] for v in vv) for i in range(3)]; maxs=[max(v[i] for v in vv) for i in range(3)]
    accessors.append({"bufferView":pview,"componentType":5126,"count":8,"type":"VEC3","min":mins,"max":maxs})
    while len(blob)%4: blob+=b'\0'
    io=len(blob); ib=b''.join(struct.pack('<H',i) for i in ii); blob+=ib
    iview=len(buffer_views); buffer_views.append({"buffer":0,"byteOffset":io,"byteLength":len(ib),"target":34963})
    iacc=len(accessors); accessors.append({"bufferView":iview,"componentType":5123,"count":len(ii),"type":"SCALAR","min":[0],"max":[7]})
    meshes2.append({"name":name,"primitives":[{"attributes":{"POSITION":pacc},"indices":iacc,"material":material}]})
    nodes2.append({"name":name,"mesh":len(meshes2)-1})

emit_cube("BlueBlock",-.52,-.04,-.10,1.15,1.35,.34,BLUE,-.12,-.18)
emit_cube("BlueInner",-.18,.02,.11,.58,1.05,.10,NAVY,-.12,-.18)
emit_cube("RedBlock",.43,.18,.22,1.18,1.38,.42,RED,-.12,-.22)
emit_cube("RedFace",.54,.22,.45,.92,1.05,.06,RED2,-.12,-.22)
for n in range(12):
    a=2*math.pi*n/12; emit_cube(f"Ring{n}",.67+math.cos(a)*.18,.22+math.sin(a)*.22,.50,.11,.11,.035,WHITE,a,-.22)
emit_cube("WhiteCenter",.67,.22,.50,.26,.34,.04,WHITE,0,-.22)
emit_cube("BlueSlash1",-.75,.42,.12,.055,.33,.045,WHITE,.08,-.18)
emit_cube("BlueSlash2",-.70,-.18,.12,.045,.24,.045,WHITE,.08,-.18)
emit_cube("SaberSlash",.08,-.70,.38,.055,.88,.055,WHITE,-1.02,-.10)

gltf={"asset":{"version":"2.0","generator":"3DSaber banner generator by 3DCreatorS"},"scene":0,"scenes":[{"name":"COMMON","nodes":list(range(len(nodes2)))}],"nodes":nodes2,"meshes":meshes2,"materials":mats,"buffers":[{"byteLength":len(blob),"uri":"data:application/octet-stream;base64,"+base64.b64encode(blob).decode()}],"bufferViews":buffer_views,"accessors":accessors}
with open('banner.gltf','w') as f: json.dump(gltf,f,separators=(',',':'))
print('banner.gltf generated',len(blob),'bytes')
