#include "Client/Client.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

namespace Net {

    namespace {

        // Bounds-checked helpers: subspan(off, sizeof(T)) verifies that the whole
        // object fits inside the buffer before memcpy touches it.
        // memcpy is used because buf + off is not guaranteed to be aligned for T.
        template <typename T>
        void writeAt(std::span<uint8_t> buf, std::size_t off, const T& value) {
            static_assert(std::is_trivially_copyable_v<T>);
            std::memcpy(buf.subspan(off, sizeof(T)).data(), &value, sizeof(T));
        }

        template <typename T>
        T readAt(std::span<const uint8_t> buf, std::size_t off) {
            static_assert(std::is_trivially_copyable_v<T>);
            T value{};
            std::memcpy(&value, buf.subspan(off, sizeof(T)).data(), sizeof(T));
            return value;
        }

    } // namespace

    CClient::CClient(asio::io_context& io) :
        io_(io), tcp_(io), udp_(io), buf_(4096), recv_buf_(4096), send_buf_(1 + sizeof(STateUpdate)) {}

    void CClient::connect(const char* host, uint16_t tcp_port, uint16_t) {
        asio::ip::tcp::resolver resolver(io_);
        asio::error_code        ec;
        auto                    results = resolver.resolve(host, std::to_string(tcp_port), ec);
        if (ec) {
            std::cout << "[Client] resolve: " << ec.message() << '\n';
            return;
        }
        doConnect(host, results);
    }

    void CClient::doConnect(const char* host, asio::ip::tcp::resolver::results_type results) {
        // Copy the host: don't rely on the caller's pointer outliving the async operation.
        asio::async_connect(
            tcp_, results,
            [this, host_str = std::string(host)](const asio::error_code& ec, const auto&) {
                if (ec) {
                    std::cout << "[Client] connect: " << ec.message() << '\n';
                    return;
                }
                std::cout << "[Client] TCP connected\n";

                asio::error_code addr_ec;
                auto             addr = asio::ip::make_address(host_str, addr_ec);
                if (addr_ec) {
                    std::cout << "[Client] bad address: " << addr_ec.message() << '\n';
                    return;
                }
                server_ep_ = asio::ip::udp::endpoint(addr, DefaultUdpPort);
                udp_.open(asio::ip::udp::v4());
                doSendHandshake();
            });
    }

    void CClient::doSendHandshake() {
        SHandshakeRequest req{};
        req.magic   = 0x5253;
        req.version = 1;

        constexpr std::string_view name = "Player";
        std::copy_n(name.begin(), std::min(name.size(), sizeof(req.name) - 1), req.name);

        asio::error_code ec;
        asio::write(tcp_, asio::buffer(&req, sizeof(req)), ec);
        if (ec) {
            std::cout << "[Client] handshake send: " << ec.message() << '\n';
            return;
        }
        doReadHandshake();
    }

    void CClient::doReadHandshake() {
        buf_.assign(256, 0);
        asio::async_read(tcp_, asio::buffer(buf_), asio::transfer_exactly(sizeof(SHandshakeResponse)),
                         [this](const asio::error_code& ec, std::size_t) {
                             if (ec) {
                                 std::cout << "[Client] handshake: " << ec.message() << '\n';
                                 return;
                             }
                             SHandshakeResponse res{};
                             std::memcpy(&res, buf_.data(), sizeof(res));
                             if (res.magic == 0x5253 && res.accepted) {
                                 player_id_ = res.playerId;
                                 connected_ = true;
                                 std::cout << "[Client] accepted #" << player_id_ << '\n';
                                 doReadUdp();
                             }
                         });
    }

    void CClient::poll() {
        io_.poll();
    }

    void CClient::sendState(float x, float y) {
        if (!connected_)
            return;
        STateUpdate s{.playerId = player_id_, .x = x, .y = y};

        send_buf_.front() = static_cast<uint8_t>(ePacketType::PKT_STATE_UPDATE);
        writeAt(std::span<uint8_t>(send_buf_), 1, s);

        asio::error_code ec;
        udp_.send_to(asio::buffer(send_buf_, 1 + sizeof(s)), server_ep_, 0, ec);
    }

    int CClient::broadcast(float* ox, float* oy, int max) {
        constexpr std::size_t          header = 5; // 1 byte type + 4 bytes count

        const std::span<const uint8_t> pkt(buf_);
        if (pkt.size() < header || pkt.front() != static_cast<uint8_t>(ePacketType::PKT_STATE_BROADCAST))
            return 0;

        const auto count = readAt<int32_t>(pkt, 1);
        if (count <= 0 || max <= 0)
            return 0;

        // Clamp to what the caller can hold AND to what the packet actually contains.
        const std::size_t available = (pkt.size() - header) / sizeof(STateUpdate);
        const int         n         = static_cast<int>(
            std::min({static_cast<std::size_t>(count), static_cast<std::size_t>(max), available}));

        const auto body = pkt.subspan(header);
        for (int i = 0; i < n; i++) {
            const auto s = readAt<STateUpdate>(body, static_cast<std::size_t>(i) * sizeof(STateUpdate));
            if (ox)
                ox[i] = s.x;
            if (oy)
                oy[i] = s.y;
        }
        return n;
    }

    void CClient::doReadUdp() {
        udp_.async_receive_from(
            asio::buffer(recv_buf_), udp_from_, [this](const asio::error_code& ec, std::size_t bytes) {
                if (ec)
                    return;
                buf_.assign(recv_buf_.begin(), recv_buf_.begin() + static_cast<std::ptrdiff_t>(bytes));
                doReadUdp();
            });
    }

} // namespace Net
