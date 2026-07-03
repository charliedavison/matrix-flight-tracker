#pragma once

#include <string>

struct Flight {
  std::string flight_number;   // e.g. "BA123"
  std::string airline;         // e.g. "British Airways"
  std::string origin_iata;     // e.g. "JFK"
  std::string origin_name;       // e.g. "New York"
  std::string scheduled_arrival; // e.g. "14:30"
  std::string estimated_arrival; // e.g. "14:45"
  std::string status;            // scheduled, active, landed, delayed, etc.
  std::string terminal;
  std::string gate;
  int delay_minutes = 0;
};

bool IsDelayed(const Flight& flight);
