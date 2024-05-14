module;

#include <cstdint>
#include <vector>
#include <expected>

#include <struct_pack.hpp>

export module util;

namespace util {
  enum class packer_error : uint8_t {
    out_of_range = 1,
  };

  export template <typename... Args>
  [[nodiscard]] auto inline __attribute__((always_inline)) pack(const Args&... args) -> std::vector<uint8_t> {
    static_assert(sizeof...(args) > 0);
    std::vector<uint8_t> buffer;
    struct_pack::serialize_to(buffer, args...);
    return buffer;
  }

  export template <typename T, struct_pack::detail::deserialize_view view_t>
  [[nodiscard]] STRUCT_PACK_INLINE auto unpack(const view_t& view) -> std::expected<T, packer_error> {
    std::expected<T, packer_error> result;
    if (auto errc = struct_pack::deserialize_to(result.value(), view)) [[unlikely]] {
      result = std::unexpected{packer_error::out_of_range};
    }
    return result;
  }
} // namespace util
