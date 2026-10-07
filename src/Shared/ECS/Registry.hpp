#pragma once

#include "Shared/ECS/ComponentPool.hpp"
#include "Shared/ECS/Entity.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ecs {

    template <typename... Ts>
    class CView;

    // Owns entities and all component pools for one world/state.
    //
    // Entities are generational handles; components live in one sparse set per
    // type. Systems remain plain code: they receive a registry and ask for the
    // component combinations they operate on.
    class CRegistry {
      public:
        // ── Entity lifecycle ────────────────────────────────────────────────

        // Creates a fresh handle, reusing a destroyed slot when possible.
        [[nodiscard]] SEntity create() {
            std::uint32_t index = 0;
            if (!free_.empty()) {
                index = free_.back();
                free_.pop_back();
            } else {
                index = static_cast<std::uint32_t>(generations_.size());
                generations_.push_back(0);
                alive_.push_back(0);
            }
            alive_.at(index) = 1;
            ++aliveCount_;
            return SEntity{.index = index, .generation = generations_.at(index)};
        }

        // Destroys an entity and removes all of its components.
        // Stale or already-destroyed handles are ignored.
        void destroy(SEntity e) {
            if (!alive(e))
                return;

            for (auto& entry : pools_)
                entry.second->remove(e);

            alive_.at(e.index) = 0;
            ++generations_.at(e.index);
            free_.push_back(e.index);
            --aliveCount_;
        }

        // Checks that a handle still refers to the current generation of a live slot.
        [[nodiscard]] bool alive(SEntity e) const {
            return e.index < generations_.size() && alive_.at(e.index) != 0 &&
                generations_.at(e.index) == e.generation;
        }

        [[nodiscard]] std::size_t size() const {
            return aliveCount_;
        }

        void clear() {
            pools_.clear();
            generations_.clear();
            alive_.clear();
            free_.clear();
            aliveCount_ = 0;
        }

        // ── Component access ────────────────────────────────────────────────

        // Constructs or replaces T for an entity and returns the stored component.
        template <typename T, typename... Args>
        T& emplace(SEntity e, Args&&... args) {
            return pool<T>().emplace(e, std::forward<Args>(args)...);
        }

        template <typename T>
        void remove(SEntity e) {
            pool<T>().remove(e);
        }

        // Component lookup never creates a pool; missing types simply return false/null.
        template <typename T>
        [[nodiscard]] bool has(SEntity e) const {
            const auto* p = findPool<T>();
            return p != nullptr && p->contains(e);
        }

        template <typename T>
        [[nodiscard]] T* get(SEntity e) {
            auto* p = findPool<T>();
            return p != nullptr ? p->get(e) : nullptr;
        }

        template <typename T>
        [[nodiscard]] const T* get(SEntity e) const {
            const auto* p = findPool<T>();
            return p != nullptr ? p->get(e) : nullptr;
        }

        // Returns the pool for T, creating it on first use.
        // Pools are keyed by runtime type so the registry can store any component type.
        template <typename T>
        [[nodiscard]] CComponentPool<T>& pool() {
            auto [it, inserted] = pools_.try_emplace(std::type_index(typeid(T)));
            if (inserted)
                it->second = std::make_unique<CComponentPool<T>>();
            return *dynamic_cast<CComponentPool<T>*>(it->second.get());
        }

        // ── Iteration ───────────────────────────────────────────────────────

        // Calls `fn(entity, components...)` for every entity owning all Ts.
        // Iteration starts from the smallest pool, so work scales with the rarest
        // component instead of every live entity.
        //
        // Structural changes (creating/destroying entities or adding/removing
        // components) from inside `fn` invalidate the iteration; defer them.
        template <typename... Ts, typename Fn>
        void each(Fn fn) {
            static_assert(sizeof...(Ts) > 0, "each() needs at least one component type");

            const auto                  pools = std::tuple<CComponentPool<Ts>*...>{&pool<Ts>()...};

            const std::vector<SEntity>* driver   = nullptr;
            std::size_t                 minSize  = std::numeric_limits<std::size_t>::max();
            auto                        consider = [&](const auto& p) {
                if (p.size() < minSize) {
                    minSize = p.size();
                    driver  = &p.entities();
                }
            };
            std::apply([&](auto*... p) { (consider(*p), ...); }, pools);

            if (driver == nullptr)
                return;

            for (const SEntity& e : *driver) {
                std::apply(
                    [&](auto*... p) {
                        if ((p->contains(e) && ...))
                            fn(e, *p->get(e)...);
                    },
                    pools);
            }
        }

        // Calls `fn(entity)` for every alive entity, regardless of components.
        template <typename Fn>
        void eachEntity(Fn fn) {
            for (std::uint32_t i = 0; i < generations_.size(); ++i) {
                if (alive_.at(i) != 0)
                    fn(SEntity{.index = i, .generation = generations_.at(i)});
            }
        }

        template <typename Fn>
        void eachEntity(Fn fn) const {
            for (std::uint32_t i = 0; i < generations_.size(); ++i) {
                if (alive_.at(i) != 0)
                    fn(SEntity{.index = i, .generation = generations_.at(i)});
            }
        }

        // Returns a lazy, non-allocating range over entities owning all Ts.
        template <typename... Ts>
        [[nodiscard]] CView<Ts...> view() {
            return CView<Ts...>(*this);
        }

      private:
        template <typename T>
        [[nodiscard]] CComponentPool<T>* findPool() {
            const auto it = pools_.find(std::type_index(typeid(T)));
            return it == pools_.end() ? nullptr : dynamic_cast<CComponentPool<T>*>(it->second.get());
        }

        template <typename T>
        [[nodiscard]] const CComponentPool<T>* findPool() const {
            const auto it = pools_.find(std::type_index(typeid(T)));
            return it == pools_.end() ? nullptr :
                                        dynamic_cast<const CComponentPool<T>*>(it->second.get());
        }

        std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> pools_;
        std::vector<std::uint32_t>                                           generations_;
        std::vector<std::uint8_t>                                            alive_;
        std::vector<std::uint32_t>                                           free_;
        std::size_t                                                          aliveCount_ = 0;
    };

    // Lazy view over entities that own every component in Ts.
    //
    // The view stores the smallest matching pool as its driver and filters the
    // rest during iteration. As with each(), structural mutations should be
    // deferred until iteration is done.
    template <typename... Ts>
    class CView {
      public:
        explicit CView(CRegistry& registry) : registry_(&registry) {
            std::size_t minSize  = std::numeric_limits<std::size_t>::max();
            auto        consider = [&](const auto& p) {
                if (p.size() < minSize) {
                    minSize = p.size();
                    driver_ = &p.entities();
                }
            };
            (consider(registry.pool<Ts>()), ...);
        }

        class CIterator {
          public:
            CIterator(const CView* view, std::size_t pos) : view_(view), pos_(pos) {
                skip();
            }

            [[nodiscard]] SEntity operator*() const {
                return view_->driver_->at(pos_);
            }

            CIterator& operator++() {
                ++pos_;
                skip();
                return *this;
            }

            [[nodiscard]] bool operator==(const CIterator& other) const {
                return pos_ == other.pos_;
            }

          private:
            void skip() {
                while (view_->driver_ != nullptr && pos_ < view_->driver_->size() &&
                       !view_->matches(view_->driver_->at(pos_)))
                    ++pos_;
            }

            const CView* view_;
            std::size_t  pos_;
        };

        [[nodiscard]] CIterator begin() const {
            return CIterator(this, 0);
        }

        [[nodiscard]] CIterator end() const {
            return CIterator(this, driver_ != nullptr ? driver_->size() : 0);
        }

      private:
        [[nodiscard]] bool matches(SEntity e) const {
            return (registry_->pool<Ts>().contains(e) && ...);
        }

        CRegistry*                  registry_;
        const std::vector<SEntity>* driver_ = nullptr;
    };

} // namespace Ecs
