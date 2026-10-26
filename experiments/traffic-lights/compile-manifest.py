import os
from pathlib import Path
import xml.etree.ElementTree as E,struct,subprocess,json
from zipfile import ZipFile
from resource_table import pool,chunks,U32
r=Path(__file__).resolve().parent
A='{http://schemas.android.com/apk/res/android}'
ns=A[1:-1]
E.register_namespace('android',ns)
tree=E.parse(r/'AndroidManifest.xml');refs={}
def walk(e,path):
    key=(*path,(e.tag,e.get(A+'name','')))
    for attr,value in list(e.attrib.items()):
        if value.startswith('@0x'):
            lookup=(key,attr);assert lookup not in refs
            refs[lookup]=int(value[1:],16)
            e.set(attr,'@android:color/transparent')
    for child in e:walk(child,key)
walk(tree.getroot(),())
tree.write(r/'manifest-placeholder.xml',encoding='utf-8',xml_declaration=True)
sdk=Path(os.environ['ANDROID_SDK_ROOT'])
subprocess.run([str(sdk/'build-tools/35.0.0/aapt2.exe'),'link','--manifest',str(r/'manifest-placeholder.xml'),
 '-I',str(sdk/'platforms/android-35/android.jar'),'-o',str(r/'manifest-placeholder.apk')],check=True)
with ZipFile(r/'manifest-placeholder.apk') as z:b=bytearray(z.read('AndroidManifest.xml'))
strings=None;stack=[];patched=[]
for at,t,h,size in chunks(b,8):
    if t==1:strings=pool(bytes(b[at:at+size]))[0]
    elif t==0x102:
        ext=at+16;tag=strings[U32(b,ext+4)]
        start,step,n=struct.unpack_from('<HHH',b,ext+8)
        attrs={};positions={}
        for i in range(n):
            p=ext+start+i*step;nsidx,nameidx,raw=struct.unpack_from('<III',b,p)
            attr=('{'+strings[nsidx]+'}' if nsidx!=0xffffffff else '')+strings[nameidx]
            typ=b[p+15];data=U32(b,p+16)
            value=strings[raw] if raw!=0xffffffff else strings[data] if typ==3 else ''
            attrs[attr]=value;positions[attr]=p
        stack.append((tag,attrs.get(A+'name','')))
        for attr,p in positions.items():
            key=(tuple(stack),attr)
            if key in refs:
                assert b[p+15]==1,(tag,attr,b[p+15])
                struct.pack_into('<I',b,p+8,0xffffffff)
                struct.pack_into('<I',b,p+16,refs[key]);patched.append(key)
    elif t==0x103:stack.pop()
assert set(patched)==set(refs),(len(patched),len(refs),set(refs)-set(patched))
(r/'AndroidManifest.binary.xml').write_bytes(b)
(r/'manifest-ref-check.json').write_text(json.dumps({'patchedReferences':len(patched),'packageIds':sorted(set(v>>24 for v in refs.values()))},indent=2),'utf8')
print('Manifest references checked:',len(patched))
