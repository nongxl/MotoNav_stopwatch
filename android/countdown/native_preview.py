"""Pinned HMI-only change: route choice must wait for MotoNav's Start button.

9.5.13 libautonavi.so, AArch64 InitStartCountDown() at VA/file 0xd612c0.
The function is void; its prologue and unique __FUNCTION__ text were checked
against the HMI disassembly. Returning before its prologue prevents only the
route-result auto-start timer. Explicit protocol start and core guidance remain.
Never apply this offset to a different binary.
"""
import hashlib

ORIGINAL_SHA256="4e4283da68f12e546508357dc6adefe6f275a9e62f795cff5e71acc8558143c4"
OFFSET=0xd612c0
PROLOGUE=bytes.fromhex("ff8306d1fd7b16a9fc5f17a9f65718a9")
RET=bytes.fromhex("c0035fd6")

def manual_preview(binary):
    assert hashlib.sha256(binary).hexdigest()==ORIGINAL_SHA256,"Unsupported HMI library; refusing preview patch"
    assert binary[:5]==b"\x7fELF\x02" and binary[18:20]==b"\xb7\x00","Expected AArch64 ELF64"
    assert binary[OFFSET:OFFSET+len(PROLOGUE)]==PROLOGUE,"HMI countdown prologue mismatch"
    assert binary.count(b"void hmi::CGFragmentRouteCarResult::InitStartCountDown()\0")==1
    patched=binary[:OFFSET]+RET+binary[OFFSET+4:]
    assert len(patched)==len(binary) and patched[:OFFSET]==binary[:OFFSET] and patched[OFFSET+4:]==binary[OFFSET+4:]
    return patched
