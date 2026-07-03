#pragma once

#include <string>
#include <unordered_map>

inline std::string ToUpper(std::string value) {
  for (char& c : value) {
    if (c >= 'a' && c <= 'z') {
      c = static_cast<char>(c - 'a' + 'A');
    }
  }
  return value;
}

inline const std::unordered_map<std::string, std::string>& AirlineIcaoToIata() {
  static const std::unordered_map<std::string, std::string> map = {
      {"AAL", "AA"},  {"AFR", "AF"},  {"ANA", "NH"},  {"AWE", "US"},
      {"BAW", "BA"},  {"BER", "AB"},  {"BWA", "BW"},  {"CCA", "CA"},
      {"CPA", "CX"},  {"DAL", "DL"},  {"DLH", "LH"},  {"EIN", "EI"},
      {"ELY", "LY"},  {"ETD", "EY"},  {"EZY", "U2"},  {"FDX", "FX"},
      {"FIN", "AY"},  {"IBE", "IB"},  {"ICE", "FI"},  {"JAL", "JL"},
      {"KLM", "KL"},  {"LOT", "LO"},  {"MSR", "MS"},  {"QTR", "QR"},
      {"RAM", "AT"},  {"RYR", "FR"},  {"SAS", "SK"},  {"SIA", "SQ"},
      {"SWA", "WN"},  {"TAP", "TP"},  {"THY", "TK"},  {"UAL", "UA"},
      {"UAE", "EK"},  {"VIR", "VS"},  {"VOZ", "VA"},  {"WZZ", "W6"},
      {"CFG", "DE"},  {"OCN", "4Y"},  {"EXS", "LS"},  {"TOM", "BY"},
      {"EWG", "EW"},  {"PGT", "PC"},  {"TVF", "TO"},  {"SWR", "LX"},
      {"AUA", "OS"},  {"CSA", "OK"},  {"ROT", "RO"},  {"SBI", "S7"},
      {"CES", "MU"},  {"CSN", "CZ"},  {"EVA", "BR"},  {"HDA", "KA"},
      {"KAL", "KE"},  {"PAL", "PR"},  {"THA", "TG"},  {"VIV", "VB"},
      {"AZW", "UM"},  {"ETH", "ET"},  {"MEA", "ME"},  {"RJA", "RJ"},
      {"SVA", "SV"},  {"PIA", "PK"},  {"AIC", "AI"},  {"VTI", "UK"},
  };
  return map;
}

// Convert ADS-B callsign (e.g. BAW573) to IATA flight number (e.g. BA573).
inline std::string CallsignToFlightIata(const std::string& callsign) {
  const std::string trimmed = ToUpper(callsign);
  if (trimmed.size() < 4) {
    return trimmed;
  }

  const std::string prefix = trimmed.substr(0, 3);
  const std::string number = trimmed.substr(3);
  const auto& map = AirlineIcaoToIata();
  const auto it = map.find(prefix);
  if (it != map.end()) {
    return it->second + number;
  }

  return trimmed;
}
