#include "Shared/Components/ComponentRegistry.hpp"
#include "Shared/Systems/ChatSystem.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

TEST(ChatComponent, RoundTripsThroughComponentRegistry) {
    const SCChatMessage       original{12, 3, "Alice", "hello"};
    std::vector<std::uint8_t> bytes;
    original.serialize(bytes);

    const CComponentRegistry registry;
    const auto               component = registry.deserialize(bytes);
    ASSERT_NE(component, nullptr);
    EXPECT_EQ(component->type(), AComponent::eType::CHAT_MESSAGE);

    const auto* decoded = dynamic_cast<const SCChatMessage*>(component.get());
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->sequence, 12U);
    EXPECT_EQ(decoded->senderId, 3);
    EXPECT_EQ(decoded->sender, "Alice");
    EXPECT_EQ(decoded->text, "hello");
}

TEST(ChatComponent, RejectsTruncatedAndOversizedData) {
    std::vector<std::uint8_t> bytes;
    SCChatMessage{1, 0, "Bob", "hi"}.serialize(bytes);
    const CComponentRegistry registry;

    auto                     truncated = bytes;
    truncated.pop_back();
    EXPECT_EQ(registry.deserialize(truncated), nullptr);

    auto trailing = bytes;
    trailing.push_back(0);
    EXPECT_EQ(registry.deserialize(trailing), nullptr);

    std::vector<std::uint8_t> oversized;
    SCChatMessage{1, 0, "Bob", std::string(Chat::MaxTextLength + 1, 'a')}.serialize(oversized);
    EXPECT_EQ(registry.deserialize(oversized), nullptr);
}

TEST(ChatSystem, StoresMessagesAsEntitiesInOrder) {
    Ecs::CRegistry          registry;
    const Chat::CChatSystem system;
    system.post(registry, SCChatMessage{2, 0, "Bob", "second"});
    system.post(registry, SCChatMessage{1, 1, "Ann", "first"});

    const auto history = system.history(registry);
    ASSERT_EQ(history.size(), 2U);
    EXPECT_EQ(history.at(0)->text, "first");
    EXPECT_EQ(history.at(1)->text, "second");
    EXPECT_EQ(registry.size(), 2U);
}

TEST(ChatSystem, DropsOldestMessagesBeyondHistoryLimit) {
    Ecs::CRegistry          registry;
    const Chat::CChatSystem system;
    for (std::uint32_t sequence = 1; sequence <= Chat::MaxHistory + 5; ++sequence)
        system.post(registry, SCChatMessage{sequence, 0, "Bob", std::to_string(sequence)});

    const auto history = system.history(registry);
    ASSERT_EQ(history.size(), Chat::MaxHistory);
    EXPECT_EQ(history.front()->sequence, 6U);
    EXPECT_EQ(history.back()->sequence, Chat::MaxHistory + 5);
}

TEST(ChatSystem, ClearingMessagesKeepsTypingState) {
    Ecs::CRegistry          registry;
    const Chat::CChatSystem system;
    EXPECT_FALSE(system.typing(registry));

    system.input(registry).focused = true;
    system.input(registry).text    = "draft";
    system.post(registry, SCChatMessage{1, 0, "Bob", "hi"});
    EXPECT_TRUE(system.typing(registry));

    system.clearMessages(registry);
    EXPECT_TRUE(system.history(registry).empty());
    EXPECT_TRUE(system.typing(registry));
    EXPECT_EQ(system.input(registry).text, "draft");
    EXPECT_EQ(registry.size(), 1U);
}
