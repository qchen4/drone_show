// src/ECE_UAV.cpp
#include "ECE_UAV.h"
#include <cmath>
#include <iostream>

ECE_UAV::ECE_UAV(int id_,
                 const Vec3& initialPos,
                 std::atomic<bool>* runningFlag,
                 const std::chrono::steady_clock::time_point& showStart)
    : mass(1.0),
      pos(initialPos),
      vel(0,0,0),
      acc(0,0,0),
      maxForce(20.0),
      maxSpeedAscend(2.0),
      minSphereSpeed(2.0),
      maxSphereSpeed(10.0),
      phase(UAVPhase::GroundWait),
      phaseStartTime(0.0),
      tangentDir(1,0,0),
      running(runningFlag),
      startTime(showStart),
      id(id_),
      center(0,0,50.0),
      sphereRadius(10.0)
{
}

void ECE_UAV::start() {
    worker = std::thread(&ECE_UAV::threadLoop, this);
}

void ECE_UAV::join() {
    if (worker.joinable())
        worker.join();
}

Vec3 ECE_UAV::getPositionSafe() const {
    std::lock_guard<std::mutex> lock(mtx);
    return pos;
}

Vec3 ECE_UAV::getVelocitySafe() const {
    std::lock_guard<std::mutex> lock(mtx);
    return vel;
}

double ECE_UAV::getShowTime() const {
    using clock = std::chrono::steady_clock;
    auto now = clock::now();
    std::chrono::duration<double> dur = now - startTime;
    return dur.count();
}

void ECE_UAV::threadLoop() {
    using namespace std::chrono;
    const double dt = 0.01; // 10ms
    auto nextTick = steady_clock::now();

    // initial random tangent for sphere
    {
        Vec3 radial = (pos - center).normalized();
        tangentDir = randomTangent(radial);
    }

    while (running->load()) {
        nextTick += milliseconds(10);

        double t = getShowTime();

        Vec3 force(0,0,0);

        {
            std::lock_guard<std::mutex> lock(mtx);

            // --- phase transitions ---
            if (phase == UAVPhase::GroundWait && t >= 5.0) {
                phase = UAVPhase::AscendToCenter;
                phaseStartTime = t;
            }

            // choose control based on phase
            switch (phase) {
                case UAVPhase::GroundWait:
                    // lock to ground
                    pos.z = 0.0;
                    vel = Vec3(0,0,0);
                    acc = Vec3(0,0,0);
                    break;
                case UAVPhase::AscendToCenter:
                    force = computeAscendForce(dt);
                    break;
                case UAVPhase::OnSphere:
                    force = computeSphereForce(dt);
                    break;
                case UAVPhase::Done:
                default:
                    vel = Vec3(0,0,0);
                    acc = Vec3(0,0,0);
                    break;
            }

            // integrate if not in GroundWait
            if (phase == UAVPhase::AscendToCenter || phase == UAVPhase::OnSphere) {
                integrate(force, dt);

                // switch to sphere when near shell
                double distToCenter = (pos - center).mag();
                if (phase == UAVPhase::AscendToCenter && std::fabs(distToCenter - sphereRadius) < 1.0) {
                    phase = UAVPhase::OnSphere;
                    phaseStartTime = t;
                    Vec3 radial = (pos - center).normalized();
                    tangentDir = randomTangent(radial);
                }

                // end show after 60s on sphere
                if (phase == UAVPhase::OnSphere && (t - phaseStartTime) >= 60.0) {
                    phase = UAVPhase::Done;
                }
            }
        } // unlock

        std::this_thread::sleep_until(nextTick);
    }
}

Vec3 ECE_UAV::computeAscendForce(double dt) {
    // PD-like controller to move to (0,0,50) with |v| <= 2 m/s
    Vec3 target = center; // center of sphere

    Vec3 posError = target - pos;
    Vec3 velError = Vec3(0,0,0) - vel;

    double kp = 5.0;
    double kd = 3.0;

    Vec3 force;
    force.x = kp*posError.x + kd*velError.x;
    force.y = kp*posError.y + kd*velError.y;
    // z needs gravity compensation (~10 N)
    force.z = kp*posError.z + kd*velError.z + 10.0;

    // clamp force magnitude
    double fmag = force.mag();
    if (fmag > maxForce) {
        force = force * (maxForce / fmag);
    }

    // after integration, we'll clamp speed in integrate()
    (void)dt;
    return force;
}

Vec3 ECE_UAV::computeSphereForce(double dt) {
    // Keep UAV on sphere, move along tangent direction on surface,
    // and keep speed in [minSphereSpeed, maxSphereSpeed].
    (void)dt;

    Vec3 radial = (pos - center);
    double r = radial.mag();
    if (r < 1e-6) radial = Vec3(1,0,0);
    Vec3 radialDir = radial / r;

    // Enforce radius = sphereRadius via radial correction
    double radialError = sphereRadius - r;

    // Update tangentDir slowly for some randomness
    // small random noise
    static thread_local std::mt19937 rng(std::random_device{}());
    std::normal_distribution<double> noise(0.0, 0.05);
    Vec3 noisyTangent = tangentDir + Vec3(noise(rng), noise(rng), noise(rng));
    // re-project onto tangent plane
    noisyTangent = noisyTangent - radialDir * (noisyTangent.x*radialDir.x +
                                               noisyTangent.y*radialDir.y +
                                               noisyTangent.z*radialDir.z);
    tangentDir = noisyTangent.normalized();

    // choose desired speed
    double currentSpeed = vel.mag();
    double desiredSpeed = currentSpeed;
    if (currentSpeed < minSphereSpeed) desiredSpeed = minSphereSpeed;
    if (currentSpeed > maxSphereSpeed) desiredSpeed = maxSphereSpeed;

    // desired velocity = tangentDir * desiredSpeed (pure tangent motion)
    Vec3 desiredVel = tangentDir * desiredSpeed;

    // PD on velocity + radial correction (to stay on shell)
    double kv = 4.0;    // velocity error gain
    double kr = 8.0;    // radial correction gain

    Vec3 velError = desiredVel - vel;
    Vec3 force;
    force.x = kv*velError.x + kr*radialError*radialDir.x;
    force.y = kv*velError.y + kr*radialError*radialDir.y;
    force.z = kv*velError.z + kr*radialError*radialDir.z + 10.0; // gravity comp

    double fmag = force.mag();
    if (fmag > maxForce) {
        force = force * (maxForce / fmag);
    }

    return force;
}

void ECE_UAV::integrate(const Vec3& force, double dt) {
    // F_total = force + gravity (we already added gravity in control)
    Vec3 totalForce = force;
    acc = totalForce / mass;

    // clamp accel if desired (not strictly required)
    // integrate (constant acceleration)
    vel = vel + acc * dt;
    pos = pos + vel * dt + acc * (0.5 * dt * dt);

    // ground constraint
    if (pos.z < 0.0) {
        pos.z = 0.0;
        if (vel.z < 0) vel.z = 0;
    }

    // clamp speed during ascend
    if (phase == UAVPhase::AscendToCenter) {
        double speed = vel.mag();
        if (speed > maxSpeedAscend) {
            vel = vel * (maxSpeedAscend / speed);
        }
    }
}

Vec3 ECE_UAV::randomTangent(const Vec3& radialDir) {
    // Pick random vector not parallel to radialDir, then orthogonalize
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> uni(-1.0, 1.0);

    Vec3 v(uni(rng), uni(rng), uni(rng));
    // remove radial component
    double dot = v.x*radialDir.x + v.y*radialDir.y + v.z*radialDir.z;
    Vec3 t = v - radialDir * dot;
    return t.normalized();
}
