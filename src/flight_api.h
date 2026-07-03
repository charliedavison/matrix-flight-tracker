#pragma once

#include "flight.h"

#include <optional>
#include <string>
#include <vector>

struct ApproachSearch {
  double observer_lat = 0;
  double observer_lon = 0;
  double max_distance_km = 25;
  int max_altitude_ft = 12000;
  int min_altitude_ft = 500;
};

// Find the active LHR arrival nearest the observer with live position data.
std::optional<Flight> FindNearestApproachFlight(const std::string& api_key,
                                                const ApproachSearch& search);

// Sample flight positioned near the observer for demo mode.
std::optional<Flight> GetMockNearestFlight(const ApproachSearch& search);

const std::string& GetLastError();
