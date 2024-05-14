#include <cstdint>
#include <cstdlib>
#include <string>
#include <array>
#include <vector>
#include <span>
#include <chrono>
#include <random>
#include <thread>
#include <future>
#include <print>

#include <msquic.hpp>
#include <struct_pack.hpp>

namespace util {
  enum class packer_error {
    out_of_range = 1,
  };

  auto inline __attribute__((always_inline)) current_time_ms() -> uint64_t {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
  }

  template <typename... Args>
  [[nodiscard]] auto inline __attribute__((always_inline)) pack(const Args&... args) -> std::vector<uint8_t> {
    static_assert(sizeof...(args) > 0);
    std::vector<uint8_t> buffer;
    struct_pack::serialize_to(buffer, args...);
    return buffer;
  }

  template <typename T, struct_pack::detail::deserialize_view view_t>
  [[nodiscard]] STRUCT_PACK_INLINE auto unpack(const view_t& view) -> std::expected<T, packer_error> {
    std::expected<T, packer_error> result;
    if (auto errc = struct_pack::deserialize_to(result.value(), view)) [[unlikely]] {
      result = std::unexpected{packer_error::out_of_range};
    }
    return result;
  }
} // namespace util

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

const MsQuicApi* MsQuic;

auto run(const char* address, int port, int count) -> void;
auto send(MsQuicConnection* connection, std::span<const uint8_t> data) -> void;
auto request(MsQuicConnection* connection, std::span<const uint8_t> data) -> std::vector<uint8_t>;

auto main(int argc, char** argv) -> int {
  if (argc != 5) {
    std::println("address port conn_count iter_count");
    return -1;
  }

  const char* address = argv[1];
  int port = std::stoi(argv[2]);
  int connection_count = std::stoi(argv[3]);
  int iteration_count = std::stoi(argv[4]);

  MsQuic = new MsQuicApi();

  std::vector<std::thread> threads{};
  threads.reserve(connection_count);
  for (int i = 0; i < connection_count; i++) {
    threads.emplace_back(run, address, port, iteration_count);
  }

  for (auto&& th : threads) {
    th.join();
  }

  delete MsQuic;
}

enum class key_value_t : uint8_t { key, value };

template <bool key>
auto generate_random_key_value() {}

auto run(const char* address, int port, int count) -> void {
  auto* registration = new (std::nothrow) MsQuicRegistration("test", QUIC_EXECUTION_PROFILE_TYPE_REAL_TIME, true);

  MsQuicSettings settings{};
  settings.SetPeerBidiStreamCount(8);
  settings.SetPeerUnidiStreamCount(8);
  settings.SetSendBufferingEnabled(false);
  auto* configuration = new (std::nothrow) MsQuicConfiguration(*registration, {"quic-socket"}, settings);

  QUIC_CREDENTIAL_CONFIG credentials{};
  credentials.Flags = QUIC_CREDENTIAL_FLAG_CLIENT | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
  configuration->LoadCredential(&credentials);

  auto* connection = new MsQuicConnection(*registration, CleanUpAutoDelete);
  connection->Start(*configuration, address, port);

  std::random_device rdev;
  std::default_random_engine engine(rdev());
  std::uniform_int_distribution<int> uniform_dist(1, 100);

  auto get_random_key_value = [&](key_value_t kv) -> const char* {
    constexpr int N = 10;
    static const std::array<const char*, N> keys{
        "key1",
        "key2",
        "key3",
        "key4",
        "key5",
        "key6",
        "key7",
        "key8\n",
        "key\09",
        "key\t1\n0\0a",
    };

    static const std::array<const char*, N> values{
        "value1",
        "value2",
        "value3",
        "value4",
        "value5",
        "value6",
        "value7",
        "value8\n",
        "value\09",
        "value\t1\n0\0a",
    };

    static std::uniform_int_distribution<int> dist(1, N);
    int idx = dist(engine) - 1;

    if (kv == key_value_t::key) {
      return keys[idx];
    }

    return values[idx];
  };

  for (int i = 0; i < count; i++) {
    const uint64_t start = util::current_time_ms();
    const char* key = get_random_key_value(key_value_t::key);

    if (int mean = uniform_dist(engine); mean == 1) {
      const char* value = get_random_key_value(key_value_t::value);
      send(connection, util::pack(message_t{.header{1, 2}, .body = util::pack(kv_store_t{key, value})}));

      static int writer_i = 1;
      if (++writer_i % 100 == 0) {
        std::println("[{}] set {}={}", util::current_time_ms() - start, key, value);
      }
      continue;
    }

    auto response = request(connection, util::pack(message_t{.header{1, 1}, .body = util::pack(kv_store_t{key})}));
    if (i % 1000 == 0) {
      auto message = util::unpack<message_t>(response);
      auto store = util::unpack<kv_store_t>(message->body);
      std::println("[{}] get {}={}", util::current_time_ms() - start, key, store->value);
    }
  }

  delete configuration;
  delete registration;
}

auto send(MsQuicConnection* connection, std::span<const uint8_t> data) -> void {
  std::promise<void> promise{};
  auto* stream = new (std::nothrow) MsQuicStream(
      *connection, QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL, CleanUpAutoDelete,
      [](MsQuicStream*, void* context, QUIC_STREAM_EVENT* event) -> QUIC_STATUS {
        if (event->Type == QUIC_STREAM_EVENT_SEND_COMPLETE) {
          free(event->SEND_COMPLETE.ClientContext);
          static_cast<std::promise<void>*>(context)->set_value();
        }

        return QUIC_STATUS_SUCCESS;
      },
      &promise);

  auto* buffer = static_cast<QUIC_BUFFER*>(malloc(sizeof(QUIC_BUFFER) + data.size()));
  buffer->Buffer = reinterpret_cast<uint8_t*>(buffer) + sizeof(QUIC_BUFFER);
  buffer->Length = data.size();
  std::memcpy(buffer->Buffer, data.data(), data.size());
  stream->Send(buffer, 1, QUIC_SEND_FLAG_START | QUIC_SEND_FLAG_FIN, buffer);

  promise.get_future().wait();
}

auto request(MsQuicConnection* connection, std::span<const uint8_t> data) -> std::vector<uint8_t> {
  std::promise<void> promise{};

  struct stream_context_t {
    std::promise<void>& promise;
    uint8_t* buffer;
    uint64_t buffer_size;
  };

  auto* stream_context = new stream_context_t{
      .promise = promise,
      .buffer = static_cast<uint8_t*>(malloc(0)),
      .buffer_size = 0UL,
  };
  auto* stream = new (std::nothrow) MsQuicStream(
      *connection, QUIC_STREAM_OPEN_FLAG_NONE, CleanUpAutoDelete,
      [](MsQuicStream*, void* context, QUIC_STREAM_EVENT* event) -> QUIC_STATUS {
        if (event->Type == QUIC_STREAM_EVENT_RECEIVE) {
          auto* stream_context = static_cast<stream_context_t*>(context);
          stream_context->buffer = static_cast<uint8_t*>(
              realloc(stream_context->buffer, stream_context->buffer_size + event->RECEIVE.TotalBufferLength));
          memcpy(&(stream_context->buffer[stream_context->buffer_size]), event->RECEIVE.Buffers->Buffer,
              event->RECEIVE.TotalBufferLength);
          stream_context->buffer_size += event->RECEIVE.TotalBufferLength;
        } else if (event->Type == QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN) {
          auto* stream_context = static_cast<stream_context_t*>(context);
          stream_context->promise.set_value();
        } else if (event->Type == QUIC_STREAM_EVENT_SEND_COMPLETE) {
          free(event->SEND_COMPLETE.ClientContext);
        }

        return QUIC_STATUS_SUCCESS;
      },
      stream_context);

  auto* buffer = static_cast<QUIC_BUFFER*>(malloc(sizeof(QUIC_BUFFER) + data.size()));
  buffer->Buffer = reinterpret_cast<uint8_t*>(buffer) + sizeof(QUIC_BUFFER);
  buffer->Length = data.size();
  std::memcpy(buffer->Buffer, data.data(), data.size());
  stream->Send(buffer, 1, QUIC_SEND_FLAG_START | QUIC_SEND_FLAG_FIN, buffer);

  promise.get_future().wait();
  std::vector<uint8_t> result{stream_context->buffer, stream_context->buffer + stream_context->buffer_size};

  free(stream_context->buffer);
  delete stream_context;

  return result;
}
