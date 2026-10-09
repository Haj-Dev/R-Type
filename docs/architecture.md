# Architecture

## File architecture

- `src/`
  - `Server/` — authoritative simulation and networking
  - `Client/` — input, networking, and rendering
  - `Shared/`
    - `Components/` — passive ECS component structs
    - `ECS/` — entity handles, component pools, and the registry
    - `Systems/` — shared gameplay systems
    - `GameSimulation.hpp` — ECS world and system scheduler
    - `GameWorld.hpp` — shared world constants and snapshot DTO

## Simulation

The server owns the authoritative gameplay simulation. The client does not
simulate authoritative outcomes; it sends input and renders server snapshots.

The server advances the world at a fixed 60 Hz step. `Game::CSimulation` owns
the ECS registry and schedules the systems; it does not contain gameplay logic
itself:

1. `CPlayerSystem` applies input, moves players, spawns players, and creates
   projectiles.
2. `CMovementSystem` integrates every entity with `SCPosition` and
   `SCVelocity`.
3. `CEnemySpawnSystem` periodically creates enemies.
4. `CCollisionSystem` applies projectile damage and queues destruction.
5. `CCleanupSystem` removes entities outside the world.
6. `CSnapshotSystem` converts component state to the network snapshot DTO.

The client projects each received snapshot into its own render registry. The
`CRenderSystem` queries `SCPosition` and `SCDrawable` and performs the Raylib
drawing.

## Entity Component System

The shared ECS (`src/Shared/ECS/`, namespace `Ecs`) is the building block both
the server simulation and the client render world use. It is header-only. The
network snapshot remains a transport DTO; incoming snapshots are immediately
projected into client components before rendering.

- `Ecs::SEntity` — a generational handle (`index` + `generation`). Recycling a
  slot bumps its generation, so a handle to a destroyed entity never aliases
  the entity that later reuses the same slot.
- `Ecs::CComponentPool<T>` — a sparse set storing one component type. Lookups
  are O(1) and iteration is cache-friendly; removal swap-removes the last
  element to keep the dense arrays packed.
- `Ecs::CRegistry` — owns entities and every component pool. It exposes
  `create`/`destroy`, `emplace`/`get`/`has`/`remove`, and iteration through
  `each<Ts...>()`, `view<Ts...>()` and `eachEntity()`.
- `Ecs::CView<Ts...>` — a lazy, non-allocating range over the entities owning
  every requested component.

All active game components derive from `AComponent` and live in
`src/Shared/Components/`. Components use the `SC` prefix:
`SCPosition`, `SCVelocity`, `SCHealth`, `SCCollider`, `SCEntityType`,
`SCPlayer`, `SCProjectile`, and `SCDrawable`.

`AComponent` is a common value/marker base, not a place for per-entity
behavior. Components contain state only. Movement, collision, simulation,
networking, and rendering remain systems that query component combinations.
`SCDrawable` describes the visual representation; `CRenderSystem` performs the
actual Raylib drawing.

Systems are explicit classes that take a `CRegistry&` and iterate:

```cpp
class CMovementSystem {
  public:
    void update(Ecs::CRegistry& registry, float dt) const {
      registry.each<SCPosition, SCVelocity>(
        [dt](Ecs::SEntity, SCPosition& p, const SCVelocity& v) {
        p.x += v.x * dt;
        p.y += v.y * dt;
      });
    }
};
```

Iteration is driven by the smallest matching pool, so its cost scales with the
rarest component. Structural changes (creating/destroying entities or
adding/removing components) must not happen while iterating; defer them.
