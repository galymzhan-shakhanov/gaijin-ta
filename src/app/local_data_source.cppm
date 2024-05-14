module;

#include <cctype>
#include <string>
#include <map>
#include <functional>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <print>

module app:data_source;

namespace app {
  namespace {
    constexpr uint64_t counter_max = 1000UL;
    constexpr uint64_t counter_interval_ms = 100'000UL;
  } // namespace

  class local_data_source_t {
    using cache_t = std::map<std::string, std::string>;

  public:
    local_data_source_t(std::filesystem::path filepath) : filepath(std::move(filepath)) {
      cache = read_from_file(filepath); // todo: lazy read?
      ticker = std::thread{std::bind_front(&local_data_source_t::tick, this)};
    }

    virtual ~local_data_source_t() noexcept {
      running = false;
      ticker_cv.notify_all();
      ticker.join();
    }

    [[nodiscard]] auto get(const std::string& key) const -> std::string;
    auto set(const std::string& key, const std::string& value) -> void;

  private:
    static auto sanitize(std::string str) -> std::string;
    static auto read_from_file(const std::filesystem::path& path) -> cache_t;
    static auto write_to_file(const cache_t& data, const std::filesystem::path& path) -> void;

    auto tick() -> void;

    std::filesystem::path filepath;

    cache_t cache;
    uint64_t counter;

    std::thread ticker;
    std::atomic<bool> running{true};
    mutable std::shared_mutex mutex;
    std::condition_variable_any ticker_cv;
  };

  auto local_data_source_t::get(const std::string& key_raw) const -> std::string {
    auto key = sanitize(key_raw);

    std::shared_lock<std::shared_mutex> lock{mutex};
    if (cache.contains(key)) [[likely]] {
      return cache.at(key);
    }

    return {};
  }

  auto local_data_source_t::set(const std::string& key_raw, const std::string& value_raw) -> void {
    auto key = sanitize(key_raw);
    auto value = sanitize(value_raw);

    std::unique_lock<std::shared_mutex> lock{mutex};
    cache[key] = value;
    if (++counter > counter_max) {
      ticker_cv.notify_one();
    }
  }

  auto local_data_source_t::tick() -> void {
    while (running) {
      std::unique_lock<std::shared_mutex> lock{mutex};
      ticker_cv.wait_for(lock, std::chrono::milliseconds{counter_interval_ms}, [this] {
        return counter > counter_max || !running;
      });

      if (counter > 0) {
        cache_t copy = cache;
        counter = 0;
        lock.unlock();

        write_to_file(copy, filepath);
      }
    }
  }


  auto local_data_source_t::read_from_file(const std::filesystem::path& path) -> cache_t {
    cache_t result;
    std::ifstream file(path);
    if (file.is_open()) {
      std::string line;
      while (std::getline(file, line)) {
        if (auto pos = line.find('='); pos != std::string::npos) {
          std::string key = line.substr(0, pos);
          std::string value = line.substr(pos + 1);
          result[key] = value;
        }
      }

      file.close();
    }

    return result;
  }

  auto local_data_source_t::write_to_file(const cache_t& data, const std::filesystem::path& path) -> void {
    std::println("writing to file...");
    std::ofstream file(path);
    if (file.is_open()) {
      for (const auto& [key, value] : data) {
        file << key << "=" << value << "\n";
      }

      file.close();
    }
  }

  auto local_data_source_t::sanitize(std::string str) -> std::string {
    str.erase(std::remove_if(str.begin(), str.end(), [](char c) {
      return static_cast<bool>(iscntrl(c)) || !static_cast<bool>(isalnum(c));
    }));
    return str;
  }
} // namespace app
