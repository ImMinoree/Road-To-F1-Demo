# Road-To-F1-Visual Physics Instructions

This is an Unreal Engine first-person racing simulator.

Two reference physics repositories are available locally:

- References/car-physics-demo
- References/crashcat

These repositories are engineering references.

Do NOT blindly copy their source code into Road-To-F1.

Instead:

1. Inspect the relevant reference implementation.
2. Understand the underlying algorithm.
3. Inspect the existing Road-To-F1 implementation.
4. Adapt the algorithm to Unreal Engine C++ and the architecture of this project.
5. Preserve existing working systems unless there is a clear reason to change them.

---

# Reference Responsibilities

## car-physics-demo

Use this repository mainly when working on:

- vehicle rigid-body dynamics
- chassis behavior
- mass and inertia
- constraints
- suspension
- wheel forces
- vehicle architecture
- load transfer

Treat this code as algorithmic reference.

Do not copy large sections of source directly.

---

## crashcat

Use this repository mainly when working on:

- broadphase collision detection
- GJK
- EPA
- convex collision detection
- contact manifolds
- persistent contacts
- contact caching
- warm starting
- sequential impulses
- Projected Gauss-Seidel
- friction
- restitution
- collision stability

Study how CrashCat maintains stable contact manifolds and cached impulses.

---

# Road-To-F1 Target Physics Architecture

## Vehicle Rigid Body

Track:

- position
- orientation
- linear velocity
- angular velocity
- forces
- torque
- mass
- inverse mass
- inertia tensor
- inverse inertia tensor

Prefer semi-implicit Euler integration initially.

Account for:

- longitudinal load transfer
- lateral load transfer
- suspension forces
- tire forces
- drivetrain forces
- aerodynamic forces

---

# Collision Pipeline

Use:

Broadphase
↓
GJK
↓
EPA
↓
Contact Manifold
↓
Sequential Impulse / PGS Solver

Broadphase should eventually use:

- Dynamic BVH / AABB tree

or an appropriate spatial structure.

Do not brute-force every object pair.

---

# Contact Solver

Use iterative sequential impulses.

Contacts should support:

- normal impulse
- friction impulse
- restitution
- penetration correction
- accumulated impulse
- warm starting

Prefer multi-point persistent contact manifolds.

---

# Cockpit Physics Feedback

Physics should expose:

- longitudinal acceleration
- lateral acceleration
- vertical acceleration
- angular acceleration
- collision impulse magnitude
- collision impulse direction
- suspension movement
- vehicle speed

These should drive:

- head bob
- acceleration head lag
- braking movement
- cornering movement
- impact kick
- camera shake
- speed FOV
- speed blur

Camera effects should respond to real physics telemetry.

---

# Workflow Rule

Before implementing an important physics feature:

1. Inspect existing Road-To-F1 code.
2. Inspect the appropriate reference repo.
3. Explain what the reference implementation does.
4. Explain what should be adopted.
5. Implement it in Unreal C++.
6. Add debug visualization where useful.
7. Test stability at different frame rates.

Do not translate reference code line-for-line.