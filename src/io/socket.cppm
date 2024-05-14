module;

#include <cstdint>
#include <string_view>
#include <span>
#include <vector>
#include <memory>
#include <functional>

export module io.socket;

namespace io {
  export class [[nodiscard]] socket_t {
    using context_t = void*;

  public:
    class session_t {
      friend socket_t;

      context_t context;
    };

    class listener_t {
      using on_connect_t = std::function<void(const session_t&, bool)>;
      using on_receive_t = std::function<void(const session_t&, std::span<const uint8_t>)>;
      using on_request_t = std::function<std::vector<uint8_t>(const session_t&, std::span<const uint8_t>)>;

    public:
      on_connect_t on_connect;
      on_receive_t on_receive;
      on_request_t on_request;
    };

    socket_t() noexcept;
    socket_t(const socket_t&) = delete;
    socket_t(socket_t&&) noexcept = default;
    virtual ~socket_t() noexcept = default;

    auto operator=(const socket_t&) noexcept -> socket_t& = delete;
    auto operator=(socket_t&& other) noexcept -> socket_t& = default;

    [[nodiscard]] auto bind(uint16_t port, listener_t listener) -> session_t;
    [[nodiscard]] auto connect(std::string_view address, uint16_t port, listener_t listener) -> session_t;
    auto close(session_t& session, uint64_t reason = 0) noexcept -> void;

    auto send(const session_t& session, std::span<const uint8_t> data) const noexcept -> void;
    [[nodiscard]] auto request(const session_t& session, std::span<const uint8_t>) noexcept -> std::vector<uint8_t>;

    auto resume(uint32_t keep_alive_ms) noexcept -> void;
    auto pause() noexcept -> void;

    [[nodiscard]] auto get_address(const session_t& session) const noexcept -> std::string;

  private:
    std::unique_ptr<context_t> context;
  };
} // namespace io
