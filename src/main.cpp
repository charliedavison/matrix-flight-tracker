#include "display.h"
#include "flight_api.h"

#include <led-matrix.h>

#include <cstdlib>
#include <csignal>
#include <fstream>
#include <iostream>
#include <optional>
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

double GetEnvDouble(const char* name, double fallback) {
  const char* val = std::getenv(name);
  if (!val) return fallback;
  try {
    return std::stod(val);
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

ApproachSearch LoadApproachSearch() {
  ApproachSearch search;
  search.observer_lat = GetEnvDouble("OBSERVER_LAT", 51.4465501);
  search.observer_lon = GetEnvDouble("OBSERVER_LON", -0.2407212);
  search.max_distance_km = GetEnvDouble("MAX_DISTANCE_KM", 25);
  search.max_altitude_ft = GetEnvInt("MAX_ALTITUDE_FT", 12000);
  search.min_altitude_ft = GetEnvInt("MIN_ALTITUDE_FT", 500);
  return search;
}

}  // namespace

int main(int argc, char* argv[]) {
  signal(SIGINT, HandleSignal);
  signal(SIGTERM, HandleSignal);

  LoadEnvFile(".env");

  const std::string api_key = GetEnv("AVIATIONSTACK_API_KEY");
  const std::string data_source = GetEnv("DATA_SOURCE", "opensky");
  const int poll_interval = GetEnvInt("POLL_INTERVAL_SEC", 60);
  const bool use_mock = GetEnv("USE_MOCK", "0") == "1";
  ApproachSearch search = LoadApproachSearch();
  search.debug = GetEnv("DEBUG_FLIGHTS", "0") == "1";

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

  fprintf(stderr, "Heathrow approach tracker starting\n");
  fprintf(stderr, "  Mode: %s\n", use_mock ? "mock data" : data_source.c_str());
  if (!use_mock && !api_key.empty()) {
    fprintf(stderr, "  Enrichment: aviationstack (flight details)\n");
  }
  fprintf(stderr, "  Observer: %.6f, %.6f\n", search.observer_lat,
          search.observer_lon);
  fprintf(stderr, "  Search radius: %.1f km, altitude %d-%d ft\n",
          search.max_distance_km, search.min_altitude_ft,
          search.max_altitude_ft);
  fprintf(stderr, "  Poll interval: %ds\n", poll_interval);

  while (g_running) {
    display.ShowTitle("NEAREST", "Searching...");

    std::optional<Flight> nearest;
    if (use_mock) {
      nearest = GetMockNearestFlight(search);
    } else {
      nearest = FindNearestApproachFlight(search, data_source, api_key);
    }

    if (!nearest) {
      std::string err = GetLastError();
      fprintf(stderr, "No approach flight: %s\n", err.c_str());
      display.ShowTitle("NO FLIGHT", err.substr(0, 20));
      for (int i = 0; i < poll_interval && g_running; ++i) {
        sleep(1);
      }
      continue;
    }

    fprintf(stderr, "Tracking %s %.1fkm away at %dft\n",
            nearest->flight_number.c_str(), nearest->distance_km,
            nearest->altitude_ft);

    for (int i = 0; i < poll_interval && g_running; ++i) {
      display.ShowApproachFlight(*nearest);
      sleep(1);
    }
  }

  matrix->Clear();
  delete matrix;
  return 0;
}
