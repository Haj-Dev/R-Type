#pragma once

#include "Abstract/AComponent.hpp"
#include "Serialization.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace Chat {

    inline constexpr std::size_t MaxTextLength   = 120;
    inline constexpr std::size_t MaxSenderLength = 8;
    inline constexpr std::size_t MaxHistory      = 50;

} // namespace Chat

// One chat line. The same component is used in both directions: a client sends
// it with only `text` filled in, the server stamps the sender and sequence and
// broadcasts it to the room.
struct SCChatMessage : AComponent {
  public:
    SCChatMessage() = default;
    SCChatMessage(std::uint32_t messageSequence, std::uint8_t messageSenderId, std::string messageSender,
                  std::string messageText) :
        sequence(messageSequence), senderId(messageSenderId), sender(std::move(messageSender)),
        text(std::move(messageText)) {}
    explicit SCChatMessage(const std::vector<std::uint8_t>& data) : AComponent(data) {
        Components::CReader reader(data);
        if (!reader.read(sequence) || !reader.read(senderId) || !reader.readString(sender) ||
            !reader.readString(text) || !reader.finished() || sender.size() > Chat::MaxSenderLength ||
            text.size() > Chat::MaxTextLength)
            setInvalid();
    }

    [[nodiscard]] eType type() const override {
        return eType::CHAT_MESSAGE;
    }
    void serialize(std::vector<std::uint8_t>& data) const override {
        Components::append(data, type());
        Components::append(data, sequence);
        Components::append(data, senderId);
        Components::appendString(data, sender);
        Components::appendString(data, text);
    }

    std::uint32_t sequence = 0;
    std::uint8_t  senderId = 0;
    std::string   sender;
    std::string   text;
};
