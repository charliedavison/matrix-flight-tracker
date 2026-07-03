#include "display.h"
#include "flight_api.h"

#include <led-matrix.h>

#include <cstdlib>
#include <csignal>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

volatile sig_atomic_t g_running = 1;

void HandleSignal(int) { g_running = 0; }

std::string GetEnv(const char* name, const std::string& fallback = "") {
  const char* val = std::getenv(name);
  return val ? std::string(val) : fallback;
}

int GetEnvInt(const char* name, int fallback) {
  const char* val = std::getenv(name);
  if (!val) return fallback;
  try {
    return std::stoi(val);
  } catch (...) {
    return fallback;
  }
}

// Load KEY=VALUE pairs from a .env file (simple parser, no quotes).
void LoadEnvFile(const std::string& path) {
  std::ifstream file(path);
  if (!file) return;

  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') continue;
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;

    std::string key = line.substr(0, eq);
    std::string val = line.substr(eq + 1);

    // Trim whitespace
    while (!val.empty() && (val.back() == '\r' || val.back() == ' ')) {
      val.pop_back();
    }

    if (std::getenv(key.c_str()) == nullptr) {
      setenv(key.c_str(), val.c_str(), 0);
    }
  }
}

std::string FindFontPath() {
  const char* candidates[] = {
      "fonts/4x6.bdf",
      "third_party/rpi-rgb-led-matrix/fonts/4x6.bdf",
      "/usr/share/fonts/4x6.bdf",
  };
  for (const char* path : candidates) {
    std::ifstream f(path);
    if (f) return path;
  }
  return "fonts/4x6.bdf";
}

}  // namespace

int main(int argc, char* argv[]) {
  signal(SIGINT, HandleSignal);
  signal(SIGTERM, HandleSignal);

  LoadEnvFile(".env");

  const std::string api_key = GetEnv("AVIATIONSTACK_API_KEY");
  const int poll_interval = GetEnvInt("POLL_INTERVAL_SEC", 900);
  const int display_seconds = GetEnvInt("DISPLAY_SECONDS", 8);
  const bool use_mock = GetEnv("USE_MOCK", api_key.empty() ? "1" : "0") == "1";

  rgb_matrix::RGBMatrix::Options matrix_options;
  rgb_matrix::RuntimeOptions runtime_options;
  runtime_options.do_gpio_init = true;

  rgb_matrix::RGBMatrix* matrix =
      rgb_matrix::RGBMatrix::CreateFromFlags(&argc, &argv, &matrix_options,
                                             &runtime_options);
  if (!matrix) {
    fprintf(stderr, "Failed to initialize RGB matrix\n");
    return 1;
  }

  FlightDisplay display(matrix, FindFontPath());

  fprintf(stderr, "Heathrow arrivals display starting\n");
  fprintf(stderr, "  Mode: %s\n", use_mock ? "mock data" : "live API");
  fprintf(stderr, "  Poll interval: %ds\n", poll_interval);

  while (g_running) {
    std::vector<Flight> flights;

    if (use_mock) {
      flights = GetMockArrivals();
      display.ShowTitle("LHR ARRIVALS", "DEMO MODE");
      sleep(3);
    } else {
      display.ShowTitle("LHR ARRIVALS", "Fetching...");
      flights = FetchHeathrowArrivals(api_key, 10);

      if (flights.empty()) {
        std::string err = GetLastError();
        fprintf(stderr, "API error: %s\n", err.c_str());
        display.ShowTitle("API ERROR", err.substr(0, 20));
        sleep(10);
        continue;
      }

      fprintf(stderr, "Fetched %zu flights\n", flights.size());
    }

    display.ShowFlights(flights, display_seconds);

    // Wait until next poll, but check for shutdown every second.
    for (int i = 0; i < poll_interval && g_running; ++i) {
      sleep(1);
    }
  }

  matrix->Clear();
  delete matrix;
  return 0;
}
