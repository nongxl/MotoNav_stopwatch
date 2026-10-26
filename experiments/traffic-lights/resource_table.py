import os
"""Merge two compiled resource packages without changing either package's IDs."""
import struct
U32=lambda b,p:struct.unpack_from('<I',b,p)[0]
def chunks(b,start):
    at=start
    while at<len(b):
        t,h,size=struct.unpack_from('<HHI',b,at)
        assert 8<=h<=size and at+size<=len(b),(hex(t),at,h,size,len(b))
        yield at,t,h,size
        at+=size
    assert at==len(b)
def length(b,p,wide):
    if wide:
        v=struct.unpack_from('<H',b,p)[0];p+=2
        if v&0x8000:v=((v&0x7fff)<<16)|struct.unpack_from('<H',b,p)[0];p+=2
    else:
        v=b[p];p+=1
        if v&0x80:v=((v&127)<<8)|b[p];p+=1
    return v,p
def pool(b):
    t,h,size,n,ns,flags,ss,st=struct.unpack_from('<HHI5I',b)
    assert t==1 and h==28 and size==len(b)
    strings=[]
    for i in range(n):
        p=ss+U32(b,28+4*i)
        n16,p=length(b,p,not bool(flags&256))
        if flags&256:
            n8,p=length(b,p,False);text=b[p:p+n8].decode('utf8')
        else:text=b[p:p+n16*2].decode('utf-16le')
        strings.append(text)
    styles=[U32(b,28+4*(n+i)) for i in range(ns)]
    return strings,styles,b[st:] if st else b''
def enc_len(v):
    assert 0<=v<32768
    return bytes([v]) if v<128 else bytes([0x80|(v>>8),v&255])
def make_pool(strings,styles,styledata):
    data=bytearray();offsets=[]
    for text in strings:
        offsets.append(len(data));raw=text.encode('utf8')
        data+=enc_len(len(text.encode('utf-16le'))//2)+enc_len(len(raw))+raw+b'\0'
    data+=b'\0'*(-len(data)%4)
    start=28+4*(len(strings)+len(styles));st=start+len(data) if styles else 0
    size=start+len(data)+(len(styledata) if styles else 0)
    return struct.pack('<HHI5I',1,28,size,len(strings),len(styles),256,start,st)+struct.pack('<'+'I'*(len(offsets)+len(styles)),*(offsets+styles))+data+(styledata if styles else b'')
def spans_shift(data,offsets,base):
    out=bytearray(data)
    for off in set(offsets):
        if off==0xffffffff:continue
        p=off
        while U32(out,p)!=0xffffffff:
            struct.pack_into('<I',out,p,U32(out,p)+base);p+=12
    return bytes(out)
def values(package):
    # Return offsets of every Res_value, including style/bag maps.
    for at,t,h,size in chunks(package,struct.unpack_from('<H',package,2)[0]):
        if t!=0x201:continue
        flags=package[at+9];n=U32(package,at+12);entries=U32(package,at+16)
        if flags&1:
            offsets=[struct.unpack_from('<HH',package,p)[1]*4 for p in range(at+h,at+entries,4)]
        elif flags&2:
            offsets=[v*4 for v in struct.unpack_from('<'+'H'*n,package,at+h) if v!=0xffff]
        else:offsets=[v for v in struct.unpack_from('<'+'I'*n,package,at+h) if v!=0xffffffff]
        for off in offsets:
            p=at+entries+off;sz,ef=struct.unpack_from('<HH',package,p)
            assert not ef&8,'compact entries need explicit support'
            if ef&1:
                count=U32(package,p+12)
                for i in range(count):yield p+sz+12*i+4
            else:yield p+sz
def parse(b):
    t,h,size,n=struct.unpack_from('<HHII',b)
    assert t==2 and h==12 and size==len(b)
    pieces=list(chunks(b,12));pools=[b[a:a+s] for a,t,h,s in pieces if t==1]
    packages=[b[a:a+s] for a,t,h,s in pieces if t==0x200]
    assert len(pools)==1 and len(packages)==n and len(pieces)==n+1
    return pool(pools[0]),packages
def merge(car,moto):
    (cs,cstyles,cdata),cp=parse(car)
    (ms,mstyles,mdata),mp=parse(moto)
    assert len(cp)==len(mp)==1 and U32(cp[0],8)==127 and U32(mp[0],8)==128
    ms=[('res/motonav/'+s[4:]) if s.startswith('res/') else s for s in ms]
    if cstyles or mstyles:
        styles=cstyles+[0xffffffff]*(len(cs)-len(cstyles))
        styles += [(x+len(cdata)) if x!=0xffffffff else x for x in mstyles]+[0xffffffff]*(len(ms)-len(mstyles))
        styled=cdata+spans_shift(mdata,mstyles,len(cs))
    else:styles=[];styled=b''
    pkg=bytearray(mp[0]);changed=0
    for pos in values(pkg):
        assert struct.unpack_from('<H',pkg,pos)[0]==8
        if pkg[pos+3]==3:
            idx=U32(pkg,pos+4);assert idx<len(ms)
            struct.pack_into('<I',pkg,pos+4,idx+len(cs));changed+=1
    shared=make_pool(cs+ms,styles,styled)
    body=shared+cp[0]+pkg
    result=struct.pack('<HHII',2,12,12+len(body),2)+body
    # Parse and validate every rebased string and all resource file paths.
    (allstrings,_,_),packages=parse(result)
    for p in packages:
        for pos in values(p):
            if p[pos+3]==3:assert U32(p,pos+4)<len(allstrings)
    return result,{'carStrings':len(cs),'motoStrings':len(ms),'rebasedStringValues':changed,
                   'packageIds':[U32(p,8) for p in packages]}
