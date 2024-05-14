module;

#include <cstdint>
#include <span>
#include <filesystem>
#include <future>
#include <string_view>
#include <vector>
#include <print>

export module app;
import :data_source;
import :protocol;

import io.socket;
import util;

namespace app {
  export class [[nodiscard]] app_t final {
  public:
    app_t(std::string_view config_file) : data_source(std::filesystem::path(config_file)) {};
    virtual ~app_t() = default;

    app_t(const app_t&) = delete;
    app_t(app_t&&) = delete;

    auto operator=(const app_t&) -> app_t& = delete;
    auto operator=(app_t&&) -> app_t& = delete;

    auto run(uint16_t port) -> void {
      io::socket_t::listener_t listener{
        .on_connect = [](const auto& session, bool connected) noexcept {},
        .on_receive = [this](const auto& session, std::span<const uint8_t> data) noexcept {
          if (auto message = util::unpack<message_t>(data); message && message->header.content_type == 2) {
            if (auto kv_store = util::unpack<kv_store_t>(message->body)) {
              data_source.set(kv_store->key, kv_store->value);
            }
          }
        },
        .on_request = [this](const auto& session, std::span<const uint8_t> data) noexcept -> std::vector<uint8_t> {
          if (auto message = util::unpack<message_t>(data); message && message->header.content_type == 1) {
            if (auto kv_store = util::unpack<kv_store_t>(message->body)) {
              auto value = data_source.get(kv_store->key);
              return util::pack(message_t{.header{1, 1}, .body = util::pack(kv_store_t{kv_store->key, value})});
            }
          }

          return {};
        }
      };

      [[maybe_unused]] auto _ = socket.bind(port, listener);

      std::promise<void>().get_future().wait();
    };

  private:
    io::socket_t socket;
    local_data_source_t data_source;
  };
} // namespace app
