import Foundation

/// Static metadata matching. It never changes the selected route or navigation progress.
struct RouteShape {
    struct Projection { let along: Double; let offset: Double; let segment: Int }
    let points: [MotorPoint]
    let roads: [String]
    let cum: [Double]
    init(points: [MotorPoint], roads: [String] = []) {
        self.points = points; self.roads = roads
        var c = [0.0]
        if points.count > 1 { for i in 1..<points.count { c.append(c.last! + Self.metres(points[i-1],points[i])) } }
        cum = c
    }
    static func metres(_ a: MotorPoint, _ b: MotorPoint) -> Double {
        hypot((a.lat-b.lat)*111320,(a.lng-b.lng)*111320*cos((a.lat+b.lat) * .pi/360))
    }
    static func bearing(_ a: MotorPoint, _ b: MotorPoint) -> Double {
        atan2((b.lng-a.lng)*cos(a.lat * .pi/180),b.lat-a.lat)*180 / .pi
    }
    static func angle(_ a: Double, _ b: Double) -> Double { abs((a-b+540).truncatingRemainder(dividingBy:360)-180) }
    func at(_ progress: Double) -> MotorPoint {
        guard points.count > 1 else { return points.first ?? MotorPoint(lat:0,lng:0) }
        var lo=1, hi=points.count-1
        while lo<hi { let mid=(lo+hi)/2; if cum[mid]<progress { lo=mid+1 } else { hi=mid } }
        let f=min(1,max(0,(progress-cum[lo-1])/max(0.001,cum[lo]-cum[lo-1])))
        let a=points[lo-1],b=points[lo]
        return MotorPoint(lat:a.lat+(b.lat-a.lat)*f,lng:a.lng+(b.lng-a.lng)*f)
    }
    func projections(_ p: MotorPoint, road: String? = nil, maximum: Double = 18) -> [Projection] {
        guard points.count>1 else { return [] }
        let scale=cos(p.lat * .pi/180)*111320
        var result: [Projection]=[]
        for i in 1..<points.count {
            if let road=road, (i-1>=roads.count || roads[i-1] != road) { continue }
            let a=points[i-1],b=points[i]
            let ax=(a.lng-p.lng)*scale,ay=(a.lat-p.lat)*111320
            let dx=(b.lng-a.lng)*scale,dy=(b.lat-a.lat)*111320
            let t=min(1,max(0,-(ax*dx+ay*dy)/max(0.001,dx*dx+dy*dy)))
            let off=hypot(ax+t*dx,ay+t*dy)
            if off<=maximum { result.append(Projection(along:cum[i-1]+t*(cum[i]-cum[i-1]),offset:off,segment:i-1)) }
        }
        return result
    }
    static func unique(_ candidates: [Projection]) -> Projection? {
        guard let best=candidates.min(by:{$0.offset<$1.offset}), !candidates.contains(where:{abs($0.along-best.along)>80}) else { return nil }
        return best
    }
    /// Named road, same direction and an agreeing 25m approach; parallel/repeated roads are rejected.
    func match(_ point: MotorPoint, back: MotorPoint, road: String) -> Projection? {
        guard !road.isEmpty, Self.metres(point,back)>=8 else { return nil }
        let heading=Self.bearing(back,point)
        return Self.unique(projections(point,road:road).filter { p in
            let before=at(max(0,p.along-25))
            return Self.metres(before,back)<=18 && Self.angle(Self.bearing(before,at(p.along)),heading)<=30
        })
    }
    /// Up to three ordered on-route anchors, favoring deviations rather than cutting across them.
    func anchors() -> [MotorPoint] {
        guard points.count>2, let total=cum.last, total>=400 else { return [] }
        let spacing=min(150,total/8)
        var selected: [Int]=[]
        for _ in 0..<3 {
            let ends=[0]+selected.sorted()+[points.count-1]
            var candidate: Int?; var score = -Double.infinity
            for k in 1..<ends.count {
                let a=ends[k-1],b=ends[k],length=cum[b]-cum[a]
                if b<=a+1 || length<2*spacing { continue }
                let chord=RouteShape(points:[points[a],points[b]])
                for i in (a+1)..<b where cum[i]>=spacing && total-cum[i]>=spacing && !selected.contains(where:{abs(cum[$0]-cum[i])<spacing}) {
                    let off=chord.projections(points[i],maximum:Double.greatestFiniteMagnitude).first?.offset ?? 0
                    let value=off+length*0.025*(1-abs(2*(cum[i]-cum[a])/length-1))
                    if value>score { score=value;candidate=i }
                }
            }
            guard let i=candidate else { break }; selected.append(i)
        }
        return selected.sorted().map{points[$0]}
    }
}
