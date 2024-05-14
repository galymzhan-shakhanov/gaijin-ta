module;

#include <cstdint>
#include <string>
#include <vector>

module app:protocol;

namespace app {
  struct message_t {
    struct header_t {
      uint8_t domain;
      uint8_t content_type;
    };

    header_t header;
    std::vector<uint8_t> body;
  };

  struct kv_store_t {
    std::string key;
    std::string value;
  };
} // namespace app
