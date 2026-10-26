import Foundation

/// GCJ-02 progress is bounded by elapsed time and physical movement, including loops.
struct MotorRouteTracker {
    struct Match { let progress: Double; let offset: Double; let arrived: Bool }
    private let shape: RouteShape
    private var progress=0.0,lastTime:Double?,lastPoint:MotorPoint?
    private var armed=false,arrivalCount=0,nearSince=0.0
    init(_ points:[MotorPoint]) {shape=RouteShape(points:points)}
    mutating func update(_ point:MotorPoint,accuracy:Double,now:Double)->Match? {
        guard MotorRequest.valid(point.lat,point.lng),accuracy.isFinite,accuracy>=0,accuracy<=50,now.isFinite,shape.points.count>=2 else{return nil}
        if let previous=lastTime,now-previous<0.4{return nil}
        let total=shape.cum.last ?? 0
        var lower=0.0,upper=total
        if let previous=lastTime,let last=lastPoint {
            if now-previous>4 {arrivalCount=0;nearSince=0}
            let travel=RouteShape.metres(last,point),reach=min(2500,max(80,min(30,now-previous)*75))
            guard travel<=reach+accuracy*2 else{return nil}
            lower=max(0,progress-80);upper=min(total,progress+min(reach,max(100,travel+accuracy*2+35)))
        } else if RouteShape.metres(point,shape.points[0])<100 {upper=min(total,300)}
        let scale=cos(point.lat * .pi/180)*111320
        var best=Double.infinity,offset=Double.infinity,along=progress
        for i in 1..<shape.points.count where shape.cum[i]>=lower && shape.cum[i-1]<=upper {
            let a=shape.points[i-1],b=shape.points[i],length=shape.cum[i]-shape.cum[i-1]
            if length<0.01{continue}
            let ax=(a.lng-point.lng)*scale,ay=(a.lat-point.lat)*111320,dx=(b.lng-a.lng)*scale,dy=(b.lat-a.lat)*111320
            let t0=min(1,max(0,-(ax*dx+ay*dy)/max(0.001,dx*dx+dy*dy)))
            let t=max(max(0,(lower-shape.cum[i-1])/length),min(min(1,(upper-shape.cum[i-1])/length),t0))
            let d=hypot(ax+t*dx,ay+t*dy),p=shape.cum[i-1]+t*length
            let score=d+(lastTime == nil ? 0 : abs(p-progress)*0.015)
            if score<best {best=score;offset=d;along=p}
        }
        lastTime=now;lastPoint=point
        if offset<=max(55,accuracy*1.5) {progress=max(progress,along)}
        let remain=max(0,total-progress),end=RouteShape.metres(point,shape.points.last!)
        if remain>min(60,total*0.6),end>min(35,total*0.5),offset<=max(55,accuracy*1.5) {armed=true}
        let near=armed && accuracy<=25 && offset<=30 && remain<=min(30,total*0.15) && end<=min(30,total*0.2)
        if near {if arrivalCount==0{nearSince=now};arrivalCount+=1}else{arrivalCount=0;nearSince=0}
        return Match(progress:progress,offset:offset,arrived:near && arrivalCount>=3 && now-nearSince>=2)
    }
}
