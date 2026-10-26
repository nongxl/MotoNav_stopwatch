import Foundation

/// Only connection setup is timed. A ready link is never cancelled by this clock.
struct BleHandshakeClock {
    enum Stage { case idle, connecting, services, notifications, ready }
    private(set) var stage: Stage = .idle
    private var deadline=Double.infinity
    mutating func begin(_ stage: Stage, now: Double) {
        self.stage=stage
        switch stage {
        case .connecting: deadline=now+45
        case .services,.notifications: deadline=now+20
        case .idle,.ready: deadline = .infinity
        }
    }
    mutating func expired(now: Double) -> Bool {
        guard now>=deadline else { return false }
        deadline = .infinity
        return true
    }
}
