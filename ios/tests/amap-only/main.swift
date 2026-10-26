import Foundation

var checks=0
func check(_ value: Bool,_ label: String) { precondition(value,label);checks+=1 }
let suite="motonav-amap-only-\(UUID().uuidString)"
let ud=UserDefaults(suiteName:suite)!
defer {ud.removePersistentDomain(forName:suite)}
ud.set("retain",forKey:"key_amap");ud.set(11,forKey:"map_theme")
for old in [-1,0,1,2,3,99] {
    ud.set(old,forKey:"map_src");ud.set("retired",forKey:"key_tdt")
    check(MapDefaults.source(ud)==3 && ud.integer(forKey:"map_src")==3,"Migrate every previous map provider")
    check(ud.object(forKey:"key_tdt")==nil,"Remove obsolete credential")
    check(ud.string(forKey:"key_amap")=="retain" && ud.integer(forKey:"map_theme")==11,"Retain current credential and style")
}
let frame=MapGeo.Frame(refLat:30,refLng:104,id:77)
var raw=[UInt32](repeating:0,count:160000)
let rgb:[UInt32]=[0xffff0000,0xff00ff00,0xff0000ff,0xffffff00]
for y in 0..<400 { for x in 0..<400 { raw[y*400+x]=rgb[(y>=200 ? 2:0)+(x>=200 ? 1:0)] } }
let first=try SatelliteRaster.encode(frame,cx:0,cy:0,half:700,raw:raw,shouldCancel:{false}).bytes
let bytes=[UInt8](first)
check(RasterPacket.u16(bytes,0)==77 && RasterPacket.i32(bytes,2)==(-700) && RasterPacket.i32(bytes,6)==700,"Coordinate origin")
check(RasterPacket.u16(bytes,10)==400 && RasterPacket.u16(bytes,12)==400 && RasterPacket.u16(bytes,14)==(3500|0x8000),"Existing firmware geometry and palette flag")
var decoded=[Int]();var p=48
while p<bytes.count {
    let v=Int(bytes[p]);p+=1
    var run=(v>>4)+1
    if run==16 {run+=Int(bytes[p]);p+=1}
    let color=RasterPacket.u16(bytes,16+2*(v&15))
    decoded.append(contentsOf:repeatElement(color,count:run))
}
check(decoded.count==160000,"Complete RLE frame")
check(decoded[0]==0xf800 && decoded[399]==0x07e0 && decoded[399*400]==0x001f && decoded[159999]==0xffe0,"Color and coordinate orientation")
let second=try SatelliteRaster.encode(frame,cx:0,cy:0,half:700,raw:raw,shouldCancel:{false}).bytes
check(first==second,"Deterministic palette")
do {_=try SatelliteRaster.encode(frame,cx:0,cy:0,half:700,raw:raw,shouldCancel:{true});preconditionFailure("Cancellation ignored")}
catch MapProcessingError.cancelled {checks+=1}
do {_=try SatelliteRaster.encode(frame,cx:0,cy:0,half:700,raw:[1],shouldCancel:{false});preconditionFailure("Invalid input accepted")}
catch {checks+=1}
print("AMap-only: \(checks) checks passed")
