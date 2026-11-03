#pragma once

#include <string>

// Client-side typing state of the chat. It lives on a single entity of the chat
// registry and is never sent over the network, so it does not derive from
// AComponent.
struct SCChatInput {
    std::string text;
    bool        focused = false;
};
