import Foundation
import ObjectiveC

/// Public iOS SDK bicycle planning. No driving conversion or electric-bike fallback.
final class BicycleRouteQuery: NSObject {
    private static var pending: [UUID:BicycleRouteQuery]=[:]
    private let id=UUID(), request: MotorRequest
    private var api: AMapSearchAPI?
    private var continuation: CheckedContinuation<[MotorRoute],Error>?
    private var timeout: DispatchWorkItem?
    private init(_ request: MotorRequest, _ continuation: CheckedContinuation<[MotorRoute],Error>) {
        self.request=request;self.continuation=continuation;super.init()
        class_addProtocol(BicycleRouteQuery.self,AMapSearchDelegate.self)
    }
    static func load(_ request: MotorRequest) async throws -> [MotorRoute] {
        try await withCheckedThrowingContinuation { continuation in
            DispatchQueue.main.async {
                let query=BicycleRouteQuery(request,continuation)
                Self.pending[query.id]=query; query.start()
            }
        }
    }
    private func start() {
        guard request.vias.isEmpty else { finish(.failure(AmapShare.Fail("自行车规划暂不支持途经点")));return }
        api=AMapSearchAPI(); api?.delegate=(self as AnyObject) as? AMapSearchDelegate
        let r=AMapRidingRouteSearchRequest()
        r.origin=AMapGeoPoint.location(withLatitude:CGFloat(request.start.lat),longitude:CGFloat(request.start.lng))
        r.destination=AMapGeoPoint.location(withLatitude:CGFloat(request.end.lat),longitude:CGFloat(request.end.lng))
        let work=DispatchWorkItem { [weak self] in self?.finish(.failure(AmapShare.Fail("自行车规划超时，请检查网络、定位和高德 iOS Key"))) }
        timeout=work;DispatchQueue.main.asyncAfter(deadline:.now()+25,execute:work)
        api?.aMapRidingRouteSearch(r)
    }
    private func finish(_ result: Result<[MotorRoute],Error>) {
        if !Thread.isMainThread {DispatchQueue.main.async{[weak self] in self?.finish(result)};return}
        guard let c=continuation else { return };continuation=nil
        timeout?.cancel();timeout=nil;api?.delegate=nil;api=nil
        Self.pending.removeValue(forKey:id);c.resume(with:result)
    }
    @objc(onRouteSearchDone:response:)
    func completed(_ r: AMapRouteSearchBaseRequest?, response: AMapRouteSearchResponse?) {
        do {
            let paths=response?.route?.paths ?? []
            guard !paths.isEmpty else { throw AmapShare.Fail("高德没有返回自行车路线") }
            let routes=try paths.prefix(10).map { path -> MotorRoute in
                var points: [MotorPoint]=[], steps: [MotorStep]=[]
                for step in path.steps ?? [] {
                    let start=max(0,points.count-1)
                    for pair in (step.polyline ?? "").split(separator:";") {
                        let xy=pair.split(separator:",")
                        guard xy.count==2,let lo=Double(xy[0]),let la=Double(xy[1]),MotorRequest.valid(la,lo) else { throw AmapShare.Fail("自行车路线坐标无效") }
                        let p=MotorPoint(lat:la,lng:lo)
                        if let previous=points.last {
                            if RouteShape.metres(previous,p)<0.01 { continue }
                            guard RouteShape.metres(previous,p)<5000 else { throw AmapShare.Fail("自行车路线不连续") }
                        }
                        guard points.count<150000 else { throw AmapShare.Fail("自行车路线过长") }; points.append(p)
                    }
                    guard !points.isEmpty else { continue }
                    steps.append(MotorStep(instruction:step.instruction ?? "",road:step.road ?? "",action:step.action ?? "",end:points.count-1,start:start,distance:step.distance,duration:step.duration))
                }
                guard points.count>=2,!steps.isEmpty else { throw AmapShare.Fail("自行车路线缺少分段") }
                return MotorRoute(request:request,points:points,steps:steps,seconds:path.duration)
            }
            finish(.success(routes))
        } catch { finish(.failure(error)) }
    }
    @objc(AMapSearchRequest:didFailWithError:)
    func failed(_ request: Any?, error: NSError?) {
        finish(.failure(AmapShare.Fail("自行车规划失败（\(error?.code ?? 0)）：\(error?.localizedDescription ?? "请检查高德 iOS Key 权限")")))
    }
}
