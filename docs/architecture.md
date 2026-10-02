# Architecture

## File architecture

- `src/`
  - `server/`
  - `client/`
  - `shared/`
    - `Ecs/` — the shared Entity Component System

## Simulation

Both server and client simulates the game.  
The server gets priority over the client to avoid cheating.  

## Entity Component System

The shared ECS (`src/Shared/ECS/`, namespace `Ecs`) is the building block both
the client and the server use to represent the game world. It is header-only.

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

Systems are plain functions that take a `CRegistry&` and iterate:

```cpp
void movementSystem(Ecs::CRegistry& registry, float dt) {
    registry.each<SPosition, SVelocity>([dt](Ecs::SEntity, SPosition& p, SVelocity& v) {
        p.x += v.dx * dt;
        p.y += v.dy * dt;
    });
}
```

Iteration is driven by the smallest matching pool, so its cost scales with the
rarest component. Structural changes (creating/destroying entities or
adding/removing components) must not happen while iterating; defer them.
