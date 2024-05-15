module;

#include <vector>
#include <utility>
#include <memory>
#include <thread>
#include <mutex>

#include <pthread.h>

module app:counter;

namespace app {
  export template <typename T>
  class counter_t final {
    template <typename R, typename F, typename V, typename... A>
    static constexpr auto first_lambda_arg(R (F::*)(V, A...) const) -> V;
  
  public:
    counter_t() {
      pthread_key_create(&key, nullptr);
    }

    virtual ~counter_t() noexcept {
      pthread_key_delete(key);
    }

    counter_t(const counter_t&) = delete; // todo: lazy to implement
    counter_t(counter_t&&) = delete;      // todo: lazy to implement

    auto operator=(const counter_t&) -> counter_t& = delete;
    auto operator=(counter_t&&) -> counter_t& = delete;

    template <typename F, typename... A>
      requires std::is_same_v<decltype(first_lambda_arg(std::declval<decltype(&F::operator())>())), T&>
    auto add(F&& fn, A&&... args) -> void {
      auto* data = reinterpret_cast<T*>(pthread_getspecific(key));
      if (data == nullptr) {
        std::scoped_lock<std::mutex> lock{mutex};
        auto& [_, value] = counter_t::data.emplace_back(std::this_thread::get_id(), std::make_unique<T>());
        data = value.get();
        pthread_setspecific(key, data);
      }

      fn(*data, std::forward<A>(args)...);
    }

    auto reset() -> void;

    [[nodiscard]] auto count() const -> T {
      T result{};
      std::scoped_lock<std::mutex> lock{mutex};
      for (const auto& [_, value] : data) {
        result += *value;
      }

      return result;
    }

  private:
    std::vector<std::pair<std::thread::id, std::unique_ptr<T>>> data;
    mutable std::mutex mutex;
    pthread_key_t key;
  };
} // namespace app
