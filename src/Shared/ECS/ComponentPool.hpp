#pragma once

#include "Shared/ECS/Entity.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace Ecs {

    // Type-erased pool interface used by CRegistry for lifecycle operations.
    //
    // Concrete pools are templated by component type, but destroying an entity
    // needs to remove its components without knowing those types at compile time.
    class IComponentPool {
      public:
        IComponentPool()                                              = default;
        virtual ~IComponentPool()                                     = default;
        IComponentPool(const IComponentPool&)                         = delete;
        IComponentPool& operator=(const IComponentPool&)              = delete;
        IComponentPool(IComponentPool&&)                              = delete;
        IComponentPool&                   operator=(IComponentPool&&) = delete;

        virtual void                      remove(SEntity e)         = 0;
        [[nodiscard]] virtual bool        contains(SEntity e) const = 0;
        [[nodiscard]] virtual std::size_t size() const              = 0;
    };

    // Sparse-set storage for one component type.
    //
    // Entity indices map through `sparse_` into packed `dense_`/`data_` arrays.
    // This gives O(1) lookup while keeping iteration cache-friendly; removal
    // swap-removes the last element to preserve that packing.
    template <typename T>
    class CComponentPool final : public IComponentPool {
      public:
        // Adds a component, or overwrites the existing one for the same entity.
        template <typename... Args>
        T& emplace(SEntity e, Args&&... args) {
            if (e.index >= sparse_.size())
                sparse_.resize(static_cast<std::size_t>(e.index) + 1, NullEntityIndex);

            const std::uint32_t slot = sparse_.at(e.index);
            if (slot != NullEntityIndex) {
                T& existing = data_.at(slot);
                existing    = T(std::forward<Args>(args)...);
                return existing;
            }

            sparse_.at(e.index) = static_cast<std::uint32_t>(dense_.size());
            dense_.push_back(e);
            data_.emplace_back(std::forward<Args>(args)...);
            return data_.back();
        }

        void remove(SEntity e) override {
            if (!contains(e))
                return;

            const std::uint32_t idx  = sparse_.at(e.index);
            const std::uint32_t last = static_cast<std::uint32_t>(dense_.size() - 1);
            if (idx != last) {
                dense_.at(idx)                   = dense_.at(last);
                data_.at(idx)                    = std::move(data_.at(last));
                sparse_.at(dense_.at(idx).index) = idx;
            }
            dense_.pop_back();
            data_.pop_back();
            sparse_.at(e.index) = NullEntityIndex;
        }

        [[nodiscard]] bool contains(SEntity e) const override {
            return e.index < sparse_.size() && sparse_.at(e.index) != NullEntityIndex &&
                dense_.at(sparse_.at(e.index)) == e;
        }

        [[nodiscard]] std::size_t size() const override {
            return dense_.size();
        }

        [[nodiscard]] T* get(SEntity e) {
            return contains(e) ? &data_.at(sparse_.at(e.index)) : nullptr;
        }

        [[nodiscard]] const T* get(SEntity e) const {
            return contains(e) ? &data_.at(sparse_.at(e.index)) : nullptr;
        }

        [[nodiscard]] const std::vector<SEntity>& entities() const {
            return dense_;
        }

        [[nodiscard]] std::vector<T>& components() {
            return data_;
        }

        [[nodiscard]] const std::vector<T>& components() const {
            return data_;
        }

      private:
        std::vector<std::uint32_t> sparse_;
        std::vector<SEntity>       dense_;
        std::vector<T>             data_;
    };

} // namespace Ecs
