#pragma once

#include "flight.h"

#include <string>
#include <vector>

// Fetch upcoming Heathrow (LHR) arrivals from Aviationstack.
// Returns empty vector on error (check GetLastError()).
std::vector<Flight> FetchHeathrowArrivals(const std::string& api_key, int limit = 10);

// Sample flights for testing without an API key.
std::vector<Flight> GetMockArrivals();

const std::string& GetLastError();
