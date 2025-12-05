// src/ECE_UAV.h
#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <random>
#include <chrono>
#include <cmath>

// Simple 3D vector
struct Vec3 {
    double x, y, z;
    Vec3(double x_=0, double y_=0, double z_=0) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return Vec3(x+o.x, y+o.y, z+o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x-o.x, y-o.y, z-o.z); }
    Vec3 operator*(double s) const { return Vec3(x*s, y*s, z*s); }
    Vec3 operator/(double s) const { return Vec3(x/s, y/s, z/s); }

    Vec3& operator+=(const Vec3& o) { x+=o.x; y+=o.y; z+=o.z; return *this; }

    double mag() const { return std::sqrt(x*x + y*y + z*z); }
    Vec3 normalized() const {
        double m = mag();
        return (m > 1e-9) ? (*this)/m : Vec3(0,0,0);
    }
};

enum class UAVPhase {
    GroundWait,
    AscendToCenter,
    OnSphere,
    Done
};

class ECE_UAV {
public:
    // public for collision and debug
    double mass;
    Vec3   pos;
    Vec3   vel;
    Vec3   acc;

    // configuration
    double maxForce;        // 20 N
    double maxSpeedAscend;  // 2 m/s during approach
    double minSphereSpeed;  // 2 m/s
    double maxSphereSpeed;  // 10 m/s;

    std::thread worker;
    mutable std::mutex mtx;


    // phase state
    UAVPhase phase;
    double   phaseStartTime;

    // tangent direction for sphere motion
    Vec3 tangentDir;

    // shared flags
    std::atomic<bool>* running;     // global flag
    std::chrono::steady_clock::time_point startTime; // global show start
    int id;

    // Sphere / target config
    Vec3 center;  // (0,0,50)
    double sphereRadius;

    // constructor
    ECE_UAV(int id_, const Vec3& initialPos,
            std::atomic<bool>* runningFlag,
            const std::chrono::steady_clock::time_point& showStart);

    // start worker thread
    void start();

    // join worker thread
    void join();

    // safe getters for render thread
    Vec3 getPositionSafe() const;
    Vec3 getVelocitySafe() const;

private:
    // internal physics step
    void threadLoop();

    double getShowTime() const;

    // control helpers
    Vec3 computeAscendForce(double dt);
    Vec3 computeSphereForce(double dt);

    // physics integration
    void integrate(const Vec3& force, double dt);

    // random tangent direction
    Vec3 randomTangent(const Vec3& radialDir);
};
