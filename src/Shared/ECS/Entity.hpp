#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>

namespace Ecs {

    // Sentinel index used by default-constructed handles.
    inline constexpr std::uint32_t NullEntityIndex = std::numeric_limits<std::uint32_t>::max();

    // Lightweight handle used to refer to registry-owned entities.
    //
    // The registry can reuse slots after destruction, so `generation` changes
    // whenever an index is recycled. This makes stale handles cheap to detect
    // without storing pointers into registry internals.
    struct SEntity {
        std::uint32_t index      = NullEntityIndex;
        std::uint32_t generation = 0;

        // True only when the handle names a possible entity slot.
        // Use CRegistry::alive() when ownership/lifetime must be verified.
        [[nodiscard]] bool valid() const {
            return index != NullEntityIndex;
        }

        friend bool operator==(const SEntity&, const SEntity&) = default;
    };

} // namespace Ecs

namespace std {

    template <>
    struct hash<Ecs::SEntity> {
        std::size_t operator()(const Ecs::SEntity& e) const noexcept {
            const std::size_t index = static_cast<std::size_t>(e.index);
            const std::size_t gen   = static_cast<std::size_t>(e.generation);
            return index ^ (gen + 0x9e3779b9U + (index << 6U) + (index >> 2U));
        }
    };

} // namespace std
