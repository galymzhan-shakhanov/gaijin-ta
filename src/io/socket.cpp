module;

#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <tuple>
#include <new>
#include <memory>
#include <atomic>
#include <utility>
#include <print>

#include <msquic.hpp>

const MsQuicApi* MsQuic;

module io.socket;

using namespace io;

namespace {
  constexpr uint64_t idle_timeout_ms = 180'000UL;
  constexpr uint16_t stream_count_max = 8U;
  constexpr uint64_t stream_buffer_size_max = 8192UL;
  constexpr const char* alpn = "quic-socket";
} // namespace

class quic_context_t {
public:
  quic_context_t() noexcept {
    if (counter.fetch_add(1) == 0) {
      MsQuic = new (std::nothrow) MsQuicApi();
    }
    registration = new (std::nothrow) MsQuicRegistration("quic-socket", QUIC_EXECUTION_PROFILE_TYPE_REAL_TIME, true);

    MsQuicSettings settings{};
    settings.SetIdleTimeoutMs(idle_timeout_ms);
    settings.SetPeerBidiStreamCount(stream_count_max);
    settings.SetPeerUnidiStreamCount(stream_count_max);
    settings.SetServerResumptionLevel(QUIC_SERVER_RESUME_AND_ZERORTT);
    configuration = new (std::nothrow) MsQuicConfiguration(*registration, {alpn}, settings);
  }

  ~quic_context_t() noexcept {
    delete configuration;
    delete registration;

    if (counter.fetch_sub(1) == 1) {
      delete MsQuic;
    }
  }

  [[nodiscard]] auto get_registration() const -> MsQuicRegistration& {
    return *registration;
  }

  [[nodiscard]] auto get_configuration() const -> MsQuicConfiguration& {
    return *configuration;
  }

private:
  static std::atomic<int> counter;

  MsQuicRegistration* registration;
  MsQuicConfiguration* configuration;
};

struct stream_context_t {
  socket_t::session_t& session;
  socket_t::listener_t* listener;
  bool is_unidirectional;
  uint8_t* buffer;
  uint64_t buffer_size;
};

std::atomic<int> quic_context_t::counter{0};

socket_t::socket_t() noexcept : context(reinterpret_cast<context_t*>(new(std::nothrow) quic_context_t{})) {};

auto socket_t::bind(uint16_t port, listener_t listener) -> session_t {
  auto* quic_context = reinterpret_cast<quic_context_t*>(context.get());

  QUIC_CERTIFICATE_FILE cert_file{"server.key", "server.cert"};
  QUIC_CREDENTIAL_CONFIG credentials{};
  credentials.Flags = QUIC_CREDENTIAL_FLAG_NONE;
  credentials.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;
  credentials.CertificateFile = &cert_file;
  if (QUIC_FAILED(quic_context->get_configuration().LoadCredential(&credentials))) {
    throw std::runtime_error("configuration credentials load failed");
  }

  auto listener_callback = [](MsQuicListener* listener, void* context, QUIC_LISTENER_EVENT* event) -> QUIC_STATUS {
    if (event->Type == QUIC_LISTENER_EVENT_NEW_CONNECTION) {
      auto connection_callback = [](MsQuicConnection* connection, void* context, QUIC_CONNECTION_EVENT* event) {
        if (event->Type == QUIC_CONNECTION_EVENT_CONNECTED) {
          auto [listener, session] = *(static_cast<std::tuple<listener_t*, session_t*>*>(context));
          listener->on_connect(*session, true);
        } else if (event->Type == QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED) {
          auto stream_callback = [](MsQuicStream* stream, void* context, QUIC_STREAM_EVENT* event) -> QUIC_STATUS {
            if (event->Type == QUIC_STREAM_EVENT_RECEIVE) {
              auto* stream_context = static_cast<stream_context_t*>(context);
              stream_context->buffer = static_cast<uint8_t*>(
                  realloc(stream_context->buffer, stream_context->buffer_size + event->RECEIVE.TotalBufferLength));
              memcpy(&(stream_context->buffer[stream_context->buffer_size]), event->RECEIVE.Buffers->Buffer,
                  event->RECEIVE.TotalBufferLength);
              stream_context->buffer_size += event->RECEIVE.TotalBufferLength;

              if (stream_context->buffer_size > stream_buffer_size_max) {
                stream->Shutdown(0UL, QUIC_STREAM_SHUTDOWN_FLAG_ABORT);
              }
            } else if (event->Type == QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN) {
              auto* stream_context = static_cast<stream_context_t*>(context);
              if (!stream_context->is_unidirectional) {
                auto response = stream_context->listener->on_request(
                    stream_context->session, {stream_context->buffer, stream_context->buffer_size});

                auto* buffer = static_cast<QUIC_BUFFER*>(malloc(sizeof(QUIC_BUFFER) + response.size()));
                buffer->Buffer = reinterpret_cast<uint8_t*>(buffer) + sizeof(QUIC_BUFFER);
                buffer->Length = response.size();
                std::memcpy(buffer->Buffer, response.data(), response.size());
                stream->Send(buffer, 1, QUIC_SEND_FLAG_FIN, buffer);
              } else {
                stream_context->listener->on_receive(
                    stream_context->session, {stream_context->buffer, stream_context->buffer_size});
                stream->Shutdown(0UL, QUIC_STREAM_SHUTDOWN_FLAG_GRACEFUL);
              }
            } else if (event->Type == QUIC_STREAM_EVENT_PEER_SEND_ABORTED) {
              stream->Shutdown(0UL, QUIC_STREAM_SHUTDOWN_FLAG_ABORT);
            } else if (event->Type == QUIC_STREAM_EVENT_SEND_COMPLETE) {
              free(event->SEND_COMPLETE.ClientContext);
            } else if (event->Type == QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE) {
              auto* stream_context = static_cast<stream_context_t*>(context);
              free(stream_context->buffer);
              delete stream_context;
            }

            return QUIC_STATUS_SUCCESS;
          };

          auto* session_context = static_cast<std::tuple<listener_t*, session_t*>*>(context);
          new (std::nothrow) MsQuicStream(event->PEER_STREAM_STARTED.Stream, CleanUpAutoDelete, stream_callback,
              new stream_context_t{
                  .session = *std::get<1>(*session_context),
                  .listener = std::get<0>(*session_context),
                  .is_unidirectional = (event->PEER_STREAM_STARTED.Flags & QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL) == 1,
                  .buffer = static_cast<uint8_t*>(malloc(0)),
                  .buffer_size = 0UL,
              });
        } else if (event->Type == QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE) {
          auto* session_context = static_cast<std::tuple<listener_t*, session_t*>*>(context);
          auto* listener = std::get<0>(*session_context);
          auto* session = std::get<1>(*session_context);
          listener->on_connect(*session, false);
          delete session_context;
        }
        return QUIC_STATUS_SUCCESS;
      };

      auto* tuple = static_cast<std::tuple<quic_context_t*, listener_t>*>(context);
      auto* connection = new (std::nothrow) MsQuicConnection(event->NEW_CONNECTION.Connection, CleanUpAutoDelete,
          connection_callback, new std::tuple(&std::get<1>(*tuple), new session_t{}));
      connection->SetConfiguration(std::get<0>(*tuple)->get_configuration());
    } else if (event->Type == QUIC_LISTENER_EVENT_STOP_COMPLETE) {
      delete static_cast<std::tuple<quic_context_t*, listener_t>*>(context);
    }
    return QUIC_STATUS_SUCCESS;
  };

  auto* quic_listener = new (std::nothrow) MsQuicListener(quic_context->get_registration(), CleanUpAutoDelete,
      listener_callback, new std::tuple{quic_context, std::move(listener)});
  if (quic_listener == nullptr || QUIC_FAILED(quic_listener->GetInitStatus())) {
    delete quic_listener;
    throw std::runtime_error("listener not started");
  }

  QUIC_ADDR address{};
  QuicAddrSetFamily(&address, QUIC_ADDRESS_FAMILY_INET);
  QuicAddrSetPort(&address, port);
  if (QUIC_FAILED(quic_listener->Start({alpn}, &address))) {
    delete quic_listener;
    throw std::runtime_error("quic port opening failed");
  }

  // todo: mark session.context as a listener;
  session_t session{};
  return session;
}
