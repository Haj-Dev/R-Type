#pragma once

#include "Shared/Types.hpp"

#include <asio.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>

namespace Net {

    class CServer {
      public:
        explicit CServer(asio::io_context& io);
        ~CServer();

        void start(uint16_t tcp_port = DefaultTcpPort, uint16_t udp_port = DefaultUdpPort);
        void poll();
        void broadcast();

      private:
        struct SClient {
            asio::ip::tcp::socket   socket;
            asio::ip::udp::endpoint remote;
            int32_t                 playerId = -1;
            STateUpdate             state{};
            std::vector<uint8_t>    buf;

            explicit SClient(asio::io_context& io) : socket(io) {}
        };

        void                                     doAccept();
        void                                     doReadClient(std::shared_ptr<SClient> ctx);
        void                                     doWaitClient(std::shared_ptr<SClient> ctx);
        void                                     removeClient(const std::shared_ptr<SClient>& ctx);
        void                                     doReadUdp();

        asio::io_context&                        io_;
        std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
        std::unique_ptr<asio::ip::udp::socket>   udp_;
        std::mutex                               mtx_;
        std::unordered_map<int32_t, std::shared_ptr<SClient>> clients_;
        int                                                   next_id_ = 1;
        std::vector<uint8_t>                                  buf_;
        std::vector<uint8_t>                                  udp_buf_;
        asio::ip::udp::endpoint                               udp_from_;
    };

} // namespace net
