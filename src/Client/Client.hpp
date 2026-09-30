#pragma once

#include "Shared/Types.hpp"

#include <asio.hpp>
#include <vector>

namespace Net {

    class CClient {
      public:
        explicit CClient(asio::io_context& io);

        void connect(const char* host, uint16_t tcp_port = DefaultTcpPort,
                     uint16_t udp_port = DefaultUdpPort);
        void poll();
        void sendState(float x, float y);
        bool connected() const {
            return connected_;
        }
        int32_t playerId() const {
            return player_id_;
        }

        // Parse last received UDP buffer. Returns player count.
        int broadcast(float* ox, float* oy, int max);

      private:
        void                  doConnect(const char* host, asio::ip::tcp::resolver::results_type results);
        void                  doSendHandshake();
        void                  doReadHandshake();
        void                  doReadUdp();

        asio::io_context&     io_;
        asio::ip::tcp::socket tcp_;
        asio::ip::udp::socket udp_;
        asio::ip::udp::endpoint server_ep_;
        asio::ip::udp::endpoint udp_from_;
        std::vector<uint8_t>    buf_;
        std::vector<uint8_t>    recv_buf_;
        std::vector<uint8_t>    send_buf_;

        bool                    connected_ = false;
        int32_t                 player_id_ = -1;
    };

} // namespace net
