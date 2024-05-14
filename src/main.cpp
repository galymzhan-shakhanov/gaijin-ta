#include <print>
#include <string>

import app;

auto main(int argc, char** argv) -> int {
  if (argc != 2) {
    std::println("port args required\n");
    return -1;
  }

  app::app_t app{"data/config.cfg"};
  app.run(std::stoi(argv[1]));
}
