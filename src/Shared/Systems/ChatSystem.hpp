#pragma once

#include "Shared/Components/ChatInput.hpp"
#include "Shared/Components/ChatMessage.hpp"
#include "Shared/ECS/ECS.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace Chat {

    // Operates on a chat registry: every chat line is an entity owning an
    // SCChatMessage, and a single entity owns the local SCChatInput.
    class CChatSystem {
      public:
        // Stores a message as a new entity and drops the oldest ones beyond MaxHistory.
        Ecs::SEntity post(Ecs::CRegistry& registry, SCChatMessage message) const {
            const auto entity = registry.create();
            registry.emplace<SCChatMessage>(entity, std::move(message));
            trim(registry);
            return entity;
        }

        // Messages ordered from the oldest to the newest.
        [[nodiscard]] std::vector<const SCChatMessage*> history(Ecs::CRegistry& registry) const {
            std::vector<const SCChatMessage*> messages;
            registry.each<SCChatMessage>(
                [&messages](Ecs::SEntity, const SCChatMessage& message) { messages.push_back(&message); });
            std::ranges::stable_sort(messages, [](const SCChatMessage* left, const SCChatMessage* right) {
                return left->sequence < right->sequence;
            });
            return messages;
        }

        // Removes every message but keeps the typing state.
        void clearMessages(Ecs::CRegistry& registry) const {
            std::vector<Ecs::SEntity> expired;
            registry.each<SCChatMessage>(
                [&expired](Ecs::SEntity entity, const SCChatMessage&) { expired.push_back(entity); });
            for (const auto entity : expired)
                registry.destroy(entity);
        }

        // The typing state, created on first use.
        SCChatInput& input(Ecs::CRegistry& registry) const {
            auto& inputs = registry.pool<SCChatInput>().components();
            if (inputs.empty())
                registry.emplace<SCChatInput>(registry.create());
            return inputs.front();
        }

        // True while the chat input captures the keyboard.
        [[nodiscard]] bool typing(Ecs::CRegistry& registry) const {
            const auto& inputs = registry.pool<SCChatInput>().components();
            return !inputs.empty() && inputs.front().focused;
        }

      private:
        void trim(Ecs::CRegistry& registry) const {
            struct SEntry {
                std::uint32_t sequence;
                Ecs::SEntity  entity;
            };
            std::vector<SEntry> entries;
            registry.each<SCChatMessage>([&entries](Ecs::SEntity entity, const SCChatMessage& message) {
                entries.push_back(SEntry{.sequence = message.sequence, .entity = entity});
            });
            if (entries.size() <= MaxHistory)
                return;
            std::ranges::stable_sort(
                entries, [](const SEntry& left, const SEntry& right) { return left.sequence < right.sequence; });
            const auto excess = entries.size() - MaxHistory;
            for (std::size_t index = 0; index < excess; ++index)
                registry.destroy(entries.at(index).entity);
        }
    };

} // namespace Chat
