#pragma once

// Umbrella header for the shared Entity Component System.
//
// The ECS keeps game state as small component structs attached to opaque entity
// handles. Systems stay decoupled by querying the component types they need
// instead of depending on inheritance-heavy game object classes.
//
//   Ecs::CRegistry   owns entities and component pools
//   Ecs::SEntity     a generational entity handle
//   Ecs::CView       a lazy range over entities owning a set of components
//
// Typical use:
//
//   Ecs::CRegistry registry;
//   Ecs::SEntity   player = registry.create();
//   registry.emplace<SCPosition>(player, SCPosition{0.0F, 0.0F});
//   registry.emplace<SCVelocity>(player, SCVelocity{1.0F, 0.0F});
//
//   registry.each<SCPosition, SCVelocity>([](Ecs::SEntity, SCPosition& p, SCVelocity& v) {
//       p.x += v.dx;
//       p.y += v.dy;
//   });

#include "Shared/ECS/ComponentPool.hpp" // IWYU pragma: export
#include "Shared/ECS/Entity.hpp"        // IWYU pragma: export
#include "Shared/ECS/Registry.hpp"      // IWYU pragma: export
