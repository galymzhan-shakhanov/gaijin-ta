module;

#include <cstdint>
#include <string_view>
#include <vector>
#include <span>
#include <chrono>
#include <filesystem>
#include <future>
#include <print>

export module app;
import :data_source;
import :protocol;
import :counter;

import io.socket;
import util;

namespace {
  constexpr uint64_t stats_output_interval_ms = 5'000UL;
}

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
      struct stats_t {
        uint64_t total_gets;
        uint64_t total_sets;

        constexpr auto operator+=(const stats_t& rhs) -> stats_t& {
          total_gets += rhs.total_gets;
          total_sets += rhs.total_sets;
          return *this;
        }
      };

      counter_t<stats_t> counter{};

      io::socket_t::listener_t listener{
        .on_connect = [](const auto& session, bool connected) noexcept {},
        .on_receive = [this, &counter](const auto& session, std::span<const uint8_t> data) noexcept {
          if (auto message = util::unpack<message_t>(data); message && message->header.content_type == 2) {
            if (auto kv_store = util::unpack<kv_store_t>(message->body)) {
              counter.add([](stats_t& stats) { ++stats.total_sets; });

              data_source.set(kv_store->key, kv_store->value);
            }
          }
        },
        .on_request = [this, &counter](const auto& session, std::span<const uint8_t> data) noexcept -> std::vector<uint8_t> {
          if (auto message = util::unpack<message_t>(data); message && message->header.content_type == 1) {
            if (auto kv_store = util::unpack<kv_store_t>(message->body)) {
              counter.add([](stats_t& stats) { ++stats.total_gets; });

              auto value = data_source.get(kv_store->key);
              return util::pack(message_t{.header{1, 1}, .body = util::pack(kv_store_t{kv_store->key, value})});
            }
          }

          return {};
        }
      };

      [[maybe_unused]] auto _ = socket.bind(port, listener);

      stats_t prev{};
      while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds{stats_output_interval_ms});
        auto stats = counter.count();
        std::println("gets: {} (total: {}) sets: {} (total: {})\n", stats.total_gets - prev.total_gets, stats.total_gets,
            stats.total_sets - prev.total_sets, stats.total_sets);
        prev = stats;
      }
      std::promise<void>().get_future().wait();
    };

  private:
    io::socket_t socket;
    local_data_source_t data_source;
  };
} // namespace app
