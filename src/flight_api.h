#pragma once

#include "flight.h"

#include <optional>
#include <string>

struct ApproachSearch {
  double observer_lat = 0;
  double observer_lon = 0;
  double max_distance_km = 25;
  int max_altitude_ft = 12000;
  int min_altitude_ft = 500;
  bool debug = false;
};

// Find the nearest aircraft overhead using OpenSky (default) or Aviationstack.
std::optional<Flight> FindNearestApproachFlight(const ApproachSearch& search,
                                                const std::string& data_source,
                                                const std::string& api_key = "");

// Sample flight positioned near the observer for demo mode.
std::optional<Flight> GetMockNearestFlight(const ApproachSearch& search);

const std::string& GetLastError();
