// Runs on macOS through tests/webmotor/check.py, using production geometry and tracking code.
let north=RouteShape(points:[MotorPoint(lat:30,lng:104),MotorPoint(lat:30.01,lng:104)],roads:["A"])
let camera=north.at(500),back=north.at(475)
precondition(north.match(camera,back:back,road:"A") != nil)
precondition(north.match(camera,back:back,road:"B") == nil)
let reverse=RouteShape(points:Array(north.points.reversed()),roads:["A"])
precondition(reverse.match(camera,back:back,road:"A") == nil)
let parallel=RouteShape(points:north.points.map{MotorPoint(lat:$0.lat,lng:$0.lng+0.0005)},roads:["A"])
precondition(parallel.match(camera,back:back,road:"A") == nil)
let loop=RouteShape(points:[north.points[0],north.points[1],north.points[0],north.points[1]],roads:["A","A","A"])
precondition(loop.match(camera,back:back,road:"A") == nil)
precondition(north.anchors().isEmpty) // no invented coordinates between a two-point route
let bent=RouteShape(points:[MotorPoint(lat:30,lng:104),MotorPoint(lat:30.005,lng:104),MotorPoint(lat:30.005,lng:104.005),MotorPoint(lat:30.01,lng:104.005)])
precondition(bent.anchors().count<=3 && !bent.anchors().isEmpty)
print("PASS camera geometry: same road/approach, reverse, parallel, repeated passage and bounded anchors")

var tracker=MotorRouteTracker(north.points)
precondition(tracker.update(north.at(0),accuracy:5,now:0) != nil)
precondition(tracker.update(north.at(0),accuracy:5,now:0) == nil)
precondition(tracker.update(north.at(1000),accuracy:5,now:1) == nil) // teleport
precondition(tracker.update(north.at(100),accuracy:100,now:5) == nil)
for i in 1...10 {
    let m=tracker.update(north.at(Double(i)*100),accuracy:5,now:Double(i)*5)!
    precondition(!m.arrived)
}
precondition(!tracker.update(north.at(1100),accuracy:5,now:55)!.arrived)
precondition(!tracker.update(north.at(1113.2),accuracy:5,now:56.1)!.arrived)
precondition(tracker.update(north.at(1113.2),accuracy:5,now:57.2)!.arrived)
var closed=MotorRouteTracker(loop.points)
let first=closed.update(loop.points[0],accuracy:5,now:0)!
precondition(!first.arrived && first.progress<300)
print("PASS route progress: duplicates, inaccurate fixes, teleport rejection, loop start and terminal dwell")

var handshake=BleHandshakeClock()
handshake.begin(.connecting,now:0)
precondition(!handshake.expired(now:44))
precondition(handshake.expired(now:45))
precondition(!handshake.expired(now:46))
handshake.begin(.notifications,now:50)
precondition(!handshake.expired(now:69))
precondition(handshake.expired(now:70))
handshake.begin(.ready,now:71)
precondition(!handshake.expired(now:100000))
print("PASS BLE handshake deadlines: bounded setup retries, no timeout on ready transport")
