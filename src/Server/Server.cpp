#include "Server/Server.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
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

    CServer::CServer(asio::io_context& io) : io_(io), buf_(4096), udp_buf_(4096) {}

    CServer::~CServer() = default;

    void CServer::start(uint16_t tcp_port, uint16_t udp_port) {
        acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(io_);
        acceptor_->open(asio::ip::tcp::v4());
        acceptor_->set_option(asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_->bind({asio::ip::tcp::v4(), tcp_port});
        acceptor_->listen();
        std::cout << "[Server] TCP on " << tcp_port << '\n';

        udp_ = std::make_unique<asio::ip::udp::socket>(io_);
        udp_->open(asio::ip::udp::v4());
        udp_->bind({asio::ip::udp::v4(), udp_port});
        std::cout << "[Server] UDP on " << udp_port << '\n';

        doAccept();
        doReadUdp();
    }

    void CServer::poll() {
        io_.poll();
    }

    void CServer::doAccept() {
        auto client = std::make_shared<SClient>(io_);
        acceptor_->async_accept(client->socket, [this, client](const asio::error_code& ec) {
            if (!ec) {
                asio::error_code ep_ec;
                auto             ep = client->socket.remote_endpoint(ep_ec);
                if (!ep_ec)
                    std::cout << "[Server] " << ep << '\n';
                doReadClient(client);
            }
            doAccept();
        });
    }

    void CServer::doReadClient(std::shared_ptr<SClient> ctx) {
        ctx->buf.assign(256, 0);
        asio::async_read(ctx->socket, asio::buffer(ctx->buf),
                         asio::transfer_at_least(sizeof(SHandshakeRequest)),
                         [this, ctx](const asio::error_code& ec, std::size_t bytes) {
                             if (ec || bytes < sizeof(SHandshakeRequest)) {
                                 removeClient(ctx);
                                 return;
                             }

                             SHandshakeRequest req{};
                             std::memcpy(&req, ctx->buf.data(), sizeof(req));
                             if (req.magic != 0x5253) {
                                 removeClient(ctx);
                                 return;
                             }

                             // name may not be null-terminated
                             std::string name(req.name, strnlen(req.name, sizeof(req.name)));
                             std::cout << "[Server] " << name << '\n';

                             SHandshakeResponse resp{};
                             resp.magic    = 0x5253;
                             resp.accepted = 1;
                             resp.playerId = next_id_++;

                             ctx->playerId       = resp.playerId;
                             ctx->state.playerId = resp.playerId;
                             ctx->state.x        = 100.0f;
                             ctx->state.y        = 100.0f;

                             {
                                 std::scoped_lock lock(mtx_);
                                 clients_[resp.playerId] = ctx;
                             }

                             asio::error_code send_ec;
                             asio::write(ctx->socket, asio::buffer(&resp, sizeof(resp)), send_ec);
                             if (send_ec) {
                                 removeClient(ctx);
                                 return;
                             }
                             std::cout << "[Server] -> #" << resp.playerId << '\n';

                             doWaitClient(ctx);
                         });
    }

    // Keeps the TCP connection open and detects when the client goes away.
    void CServer::doWaitClient(std::shared_ptr<SClient> ctx) {
        ctx->buf.assign(256, 0);
        ctx->socket.async_read_some(asio::buffer(ctx->buf),
                                    [this, ctx](const asio::error_code& ec, std::size_t) {
                                        if (ec) {
                                            removeClient(ctx);
                                            return;
                                        }
                                        doWaitClient(ctx);
                                    });
    }

    void CServer::removeClient(const std::shared_ptr<SClient>& ctx) {
        {
            std::scoped_lock lock(mtx_);
            clients_.erase(ctx->playerId);
        }
        std::cout << "[Server] client #" << ctx->playerId << " disconnected\n";
    }

    void CServer::doReadUdp() {
        udp_buf_.assign(4096, 0);
        udp_->async_receive_from(
            asio::buffer(udp_buf_), udp_from_, [this](const asio::error_code& ec, std::size_t bytes) {
                if (ec)
                    return;

                // View of only the bytes actually received (bounds-checked by first()).
                const std::span<const uint8_t> pkt = std::span<const uint8_t>(udp_buf_).first(bytes);

                if (!pkt.empty()) {
                    switch (readPacketType(pkt.data())) {
                        case ePacketType::PKT_STATE_UPDATE: {
                            if (pkt.size() < 1 + sizeof(STateUpdate))
                                break;

                            const auto       upd = readAt<STateUpdate>(pkt, 1);

                            std::scoped_lock lock(mtx_);
                            auto             it = clients_.find(upd.playerId);
                            if (it != clients_.end()) {
                                it->second->state = upd;
                                if (it->second->remote.port() == 0)
                                    it->second->remote = udp_from_;
                            }
                            break;
                        }
                        case ePacketType::PKT_PING: {
                            auto             pong = static_cast<uint8_t>(ePacketType::PKT_PONG);
                            asio::error_code send_ec;
                            udp_->send_to(asio::buffer(&pong, 1), udp_from_, 0, send_ec);
                            break;
                        }
                        default: break;
                    }
                }

                doReadUdp();
            });
    }

    void CServer::broadcast() {
        std::scoped_lock lock(mtx_);
        if (clients_.empty())
            return;

        // Never write past the buffer: header is 1 (type) + 4 (count) bytes.
        constexpr std::size_t header      = 5;
        constexpr std::size_t buf_size    = 4096;
        const std::size_t     max_entries = (buf_size - header) / sizeof(STateUpdate);

        buf_.assign(buf_size, 0);
        const std::span<uint8_t> out(buf_);

        out.front() = static_cast<uint8_t>(ePacketType::PKT_STATE_BROADCAST);

        const auto count = static_cast<int32_t>(std::min(clients_.size(), max_entries));
        writeAt(out, 1, count);

        std::size_t off     = header;
        int32_t     written = 0;
        for (auto& [id, c] : clients_) {
            if (written >= count)
                break;
            writeAt(out, off, c->state);
            off += sizeof(STateUpdate);
            ++written;
        }

        for (auto& [id, c] : clients_) {
            if (c->remote.port() != 0) {
                asio::error_code send_ec;
                udp_->send_to(asio::buffer(buf_.data(), off), c->remote, 0, send_ec);
            }
        }
    }

} // namespace Net
