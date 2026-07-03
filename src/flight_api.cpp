#include "flight_api.h"

#include "callsign.h"
#include "geo.h"

#include <nlohmann/json.hpp>

#include <curl/curl.h>
#include <cstdio>
#include <sstream>
#include <vector>

namespace {

std::string g_last_error;

constexpr double kMetersToFeet = 3.28084;
constexpr double kMpsToKmh = 3.6;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* out) {
  out->append(static_cast<char*>(contents), size * nmemb);
  return size * nmemb;
}

std::string HttpGet(const std::string& url) {
  CURL* curl = curl_easy_init();
  if (!curl) {
    g_last_error = "Failed to init curl";
    return {};
  }

  std::string response;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "heathrow-flights/1.0");

  CURLcode res = curl_easy_perform(curl);
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    g_last_error = std::string("HTTP request failed: ") + curl_easy_strerror(res);
    return {};
  }

  if (http_code != 200) {
    g_last_error = "HTTP " + std::to_string(http_code) + ": " + response.substr(0, 200);
    return {};
  }

  return response;
}

std::string TrimCallsign(const std::string& callsign) {
  const auto start = callsign.find_first_not_of(" \t");
  if (start == std::string::npos) {
    return {};
  }
  const auto end = callsign.find_last_not_of(" \t");
  return callsign.substr(start, end - start + 1);
}

std::string ExtractTime(const std::string& datetime) {
  if (datetime.size() >= 16) {
    return datetime.substr(11, 5);
  }
  return datetime;
}

bool JsonToDouble(const nlohmann::json& value, double* out) {
  if (value.is_null() || !out) {
    return false;
  }
  if (value.is_number()) {
    *out = value.get<double>();
    return true;
  }
  if (value.is_string()) {
    try {
      *out = std::stod(value.get<std::string>());
      return true;
    } catch (...) {
      return false;
    }
  }
  return false;
}

bool JsonToInt(const nlohmann::json& value, int* out) {
  if (value.is_null() || !out) {
    return false;
  }
  if (value.is_number()) {
    *out = static_cast<int>(value.get<double>());
    return true;
  }
  if (value.is_string()) {
    try {
      *out = std::stoi(value.get<std::string>());
      return true;
    } catch (...) {
      return false;
    }
  }
  return false;
}

void ComputeBoundingBox(const ApproachSearch& search, double* lamin, double* lamax,
                        double* lomin, double* lomax) {
  const double lat_delta = search.max_distance_km / 111.0;
  const double lon_delta =
      search.max_distance_km /
      (111.0 * std::cos(search.observer_lat * M_PI / 180.0));

  *lamin = search.observer_lat - lat_delta;
  *lamax = search.observer_lat + lat_delta;
  *lomin = search.observer_lon - lon_delta;
  *lomax = search.observer_lon + lon_delta;
}

bool MatchesApproachFilters(Flight& flight, const ApproachSearch& search) {
  if (!flight.has_position) {
    return false;
  }

  flight.distance_km = HaversineDistanceKm(
      search.observer_lat, search.observer_lon,
      flight.latitude, flight.longitude);

  if (flight.distance_km > search.max_distance_km) {
    return false;
  }
  if (flight.altitude_ft > search.max_altitude_ft) {
    return false;
  }
  if (flight.altitude_ft > 0 && flight.altitude_ft < search.min_altitude_ft) {
    return false;
  }
  return true;
}

Flight ParseAviationstackFlight(const nlohmann::json& item) {
  Flight f;

  if (item.contains("flight") && item["flight"].is_object()) {
    const auto& flight = item["flight"];
    if (flight.contains("iata") && !flight["iata"].is_null()) {
      f.flight_number = flight["iata"].get<std::string>();
    } else if (flight.contains("number") && !flight["number"].is_null()) {
      f.flight_number = flight["number"].get<std::string>();
    }
  }

  if (item.contains("airline") && item["airline"].is_object()) {
    const auto& airline = item["airline"];
    if (airline.contains("name") && !airline["name"].is_null()) {
      f.airline = airline["name"].get<std::string>();
    }
  }

  if (item.contains("departure") && item["departure"].is_object()) {
    const auto& dep = item["departure"];
    if (dep.contains("iata") && !dep["iata"].is_null()) {
      f.origin_iata = dep["iata"].get<std::string>();
    }
    if (dep.contains("airport") && !dep["airport"].is_null()) {
      f.origin_name = dep["airport"].get<std::string>();
    }
  }

  if (item.contains("arrival") && item["arrival"].is_object()) {
    const auto& arr = item["arrival"];
    if (arr.contains("scheduled") && !arr["scheduled"].is_null()) {
      f.scheduled_arrival = ExtractTime(arr["scheduled"].get<std::string>());
    }
    if (arr.contains("estimated") && !arr["estimated"].is_null()) {
      f.estimated_arrival = ExtractTime(arr["estimated"].get<std::string>());
    }
    if (arr.contains("delay") && !arr["delay"].is_null()) {
      f.delay_minutes = arr["delay"].get<int>();
    }
    if (arr.contains("terminal") && !arr["terminal"].is_null()) {
      f.terminal = arr["terminal"].get<std::string>();
    }
    if (arr.contains("gate") && !arr["gate"].is_null()) {
      f.gate = arr["gate"].get<std::string>();
    }
  }

  if (item.contains("flight_status") && !item["flight_status"].is_null()) {
    f.status = item["flight_status"].get<std::string>();
  }

  if (item.contains("live") && item["live"].is_object()) {
    const auto& live = item["live"];
    double lat = 0;
    double lon = 0;
    int altitude = 0;
    int speed = 0;

    if (JsonToDouble(live.value("latitude", nlohmann::json()), &lat) &&
        JsonToDouble(live.value("longitude", nlohmann::json()), &lon)) {
      f.has_position = true;
      f.latitude = lat;
      f.longitude = lon;
      if (JsonToInt(live.value("altitude", nlohmann::json()), &altitude)) {
        f.altitude_ft = altitude;
      }
      if (JsonToInt(live.value("speed_horizontal", nlohmann::json()), &speed)) {
        f.speed_kmh = speed;
      }
    }
  }

  if (f.flight_number.empty()) {
    f.flight_number = "???";
  }
  if (f.origin_iata.empty()) {
    f.origin_iata = "---";
  }

  return f;
}

void MergeFlightMetadata(Flight& flight, const Flight& metadata) {
  if (!metadata.flight_number.empty() && metadata.flight_number != "???") {
    flight.flight_number = metadata.flight_number;
  }
  if (!metadata.airline.empty()) {
    flight.airline = metadata.airline;
  }
  if (!metadata.origin_iata.empty() && metadata.origin_iata != "---") {
    flight.origin_iata = metadata.origin_iata;
  }
  if (!metadata.origin_name.empty()) {
    flight.origin_name = metadata.origin_name;
  }
  if (!metadata.scheduled_arrival.empty()) {
    flight.scheduled_arrival = metadata.scheduled_arrival;
  }
  if (!metadata.estimated_arrival.empty()) {
    flight.estimated_arrival = metadata.estimated_arrival;
  }
  if (!metadata.terminal.empty()) {
    flight.terminal = metadata.terminal;
  }
  if (!metadata.gate.empty()) {
    flight.gate = metadata.gate;
  }
  if (!metadata.status.empty()) {
    flight.status = metadata.status;
  }
  if (metadata.delay_minutes > 0) {
    flight.delay_minutes = metadata.delay_minutes;
  }
}

bool CallsignMatchesAviationstackEntry(const std::string& callsign,
                                       const nlohmann::json& item) {
  if (!item.contains("flight") || !item["flight"].is_object()) {
    return false;
  }

  const auto& flight = item["flight"];
  const std::string callsign_upper = ToUpper(callsign);
  const std::string flight_iata = CallsignToFlightIata(callsign);

  if (flight.contains("icao") && !flight["icao"].is_null()) {
    if (ToUpper(flight["icao"].get<std::string>()) == callsign_upper) {
      return true;
    }
  }

  if (flight.contains("iata") && !flight["iata"].is_null()) {
    if (ToUpper(flight["iata"].get<std::string>()) == ToUpper(flight_iata)) {
      return true;
    }
  }

  if (flight.contains("codeshared") && flight["codeshared"].is_object()) {
    const auto& shared = flight["codeshared"];
    if (shared.contains("flight_icao") && !shared["flight_icao"].is_null()) {
      if (ToUpper(shared["flight_icao"].get<std::string>()) == callsign_upper) {
        return true;
      }
    }
    if (shared.contains("flight_iata") && !shared["flight_iata"].is_null()) {
      if (ToUpper(shared["flight_iata"].get<std::string>()) == ToUpper(flight_iata)) {
        return true;
      }
    }
  }

  return false;
}

std::optional<nlohmann::json> FetchAviationstackData(const std::string& url) {
  const std::string response = HttpGet(url);
  if (response.empty()) {
    return std::nullopt;
  }

  try {
    auto json = nlohmann::json::parse(response);
    if (json.contains("error")) {
      if (g_last_error.empty()) {
        g_last_error = json["error"].value("message", "API error");
      }
      return std::nullopt;
    }
    return json;
  } catch (const std::exception& e) {
    g_last_error = std::string("JSON parse error: ") + e.what();
    return std::nullopt;
  }
}

std::optional<Flight> LookupFlightMetadata(const std::string& api_key,
                                           const std::string& callsign) {
  const std::string flight_iata = CallsignToFlightIata(callsign);

  const std::vector<std::string> queries = {
      "flight_icao=" + callsign + "&flight_status=active&limit=5",
      "flight_iata=" + flight_iata + "&flight_status=active&limit=5",
      "flight_icao=" + callsign + "&arr_iata=LHR&flight_status=active&limit=5",
      "flight_iata=" + flight_iata + "&arr_iata=LHR&flight_status=active&limit=5",
  };

  for (const auto& query : queries) {
    std::ostringstream url;
    url << "https://api.aviationstack.com/v1/flights"
        << "?access_key=" << api_key << "&" << query;

    auto json = FetchAviationstackData(url.str());
    if (!json || !json->contains("data") || !(*json)["data"].is_array()) {
      continue;
    }

    for (const auto& item : (*json)["data"]) {
      if (!CallsignMatchesAviationstackEntry(callsign, item)) {
        continue;
      }
      return ParseAviationstackFlight(item);
    }
  }

  return std::nullopt;
}

bool EnrichFlightFromAviationstack(Flight& flight, const std::string& api_key,
                                   bool debug) {
  if (api_key.empty() || flight.callsign.empty()) {
    return false;
  }

  const std::string saved_error = g_last_error;

  std::ostringstream url;
  url << "https://api.aviationstack.com/v1/flights"
      << "?access_key=" << api_key
      << "&arr_iata=LHR"
      << "&flight_status=active"
      << "&limit=100";

  auto json = FetchAviationstackData(url.str());
  if (json && json->contains("data") && (*json)["data"].is_array()) {
    for (const auto& item : (*json)["data"]) {
      if (!CallsignMatchesAviationstackEntry(flight.callsign, item)) {
        continue;
      }

      const Flight metadata = ParseAviationstackFlight(item);
      MergeFlightMetadata(flight, metadata);
      if (debug) {
        fprintf(stderr, "Enriched %s from LHR arrivals list\n",
                flight.callsign.c_str());
      }
      g_last_error = saved_error;
      return true;
    }
  }

  auto metadata = LookupFlightMetadata(api_key, flight.callsign);
  if (!metadata) {
    if (debug) {
      fprintf(stderr, "No Aviationstack metadata for callsign %s\n",
              flight.callsign.c_str());
    }
    g_last_error = saved_error;
    return false;
  }

  MergeFlightMetadata(flight, *metadata);
  if (debug) {
    fprintf(stderr, "Enriched %s via direct lookup\n", flight.callsign.c_str());
  }
  g_last_error = saved_error;
  return true;
}

std::optional<Flight> ParseOpenSkyState(const nlohmann::json& state,
                                        const ApproachSearch& search) {
  if (!state.is_array() || state.size() < 9) {
    return std::nullopt;
  }

  if (!state[8].is_boolean() || state[8].get<bool>()) {
    return std::nullopt;
  }

  double latitude = 0;
  double longitude = 0;
  if (!JsonToDouble(state[6], &latitude) || !JsonToDouble(state[5], &longitude)) {
    return std::nullopt;
  }

  Flight flight;
  flight.has_position = true;
  flight.latitude = latitude;
  flight.longitude = longitude;
  flight.status = "active";

  if (state[1].is_string()) {
    flight.callsign = TrimCallsign(state[1].get<std::string>());
    flight.flight_number = CallsignToFlightIata(flight.callsign);
  }
  if (flight.callsign.empty()) {
    if (state[0].is_string()) {
      flight.callsign = state[0].get<std::string>();
      flight.flight_number = flight.callsign;
    } else {
      flight.callsign = "???";
      flight.flight_number = "???";
    }
  }

  if (state[2].is_string()) {
    flight.origin_name = state[2].get<std::string>();
    flight.origin_iata = "---";
  }

  double altitude_m = 0;
  if (JsonToDouble(state[7], &altitude_m)) {
    flight.altitude_ft = static_cast<int>(altitude_m * kMetersToFeet);
  }

  double velocity_mps = 0;
  if (JsonToDouble(state[9], &velocity_mps)) {
    flight.speed_kmh = static_cast<int>(velocity_mps * kMpsToKmh);
  }

  if (!MatchesApproachFilters(flight, search)) {
    return std::nullopt;
  }

  return flight;
}

std::optional<Flight> FindNearestFromOpenSky(const ApproachSearch& search) {
  double lamin = 0;
  double lamax = 0;
  double lomin = 0;
  double lomax = 0;
  ComputeBoundingBox(search, &lamin, &lamax, &lomin, &lomax);

  std::ostringstream url;
  url << "https://opensky-network.org/api/states/all"
      << "?lamin=" << lamin
      << "&lamax=" << lamax
      << "&lomin=" << lomin
      << "&lomax=" << lomax;

  const std::string response = HttpGet(url.str());
  if (response.empty()) {
    return std::nullopt;
  }

  try {
    auto json = nlohmann::json::parse(response);
    if (!json.contains("states") || json["states"].is_null()) {
      g_last_error = "No aircraft reported in search area";
      return std::nullopt;
    }

    const auto& states = json["states"];
    if (!states.is_array() || states.empty()) {
      g_last_error = "No aircraft reported in search area";
      return std::nullopt;
    }

    std::optional<Flight> nearest;
    double nearest_distance = 0;
    int total = 0;
    int matched = 0;

    for (const auto& state : states) {
      ++total;
      auto flight = ParseOpenSkyState(state, search);
      if (!flight) {
        continue;
      }
      ++matched;

      if (!nearest || flight->distance_km < nearest_distance) {
        nearest = flight;
        nearest_distance = flight->distance_km;
      }
    }

    if (search.debug) {
      fprintf(stderr, "OpenSky: %d aircraft in box, %d matched filters\n",
              total, matched);
    }

    if (!nearest) {
      g_last_error = "No aircraft within range/altitude (" +
                     std::to_string(total) + " in area)";
    }

    return nearest;
  } catch (const std::exception& e) {
    g_last_error = std::string("JSON parse error: ") + e.what();
    return std::nullopt;
  }
}

std::optional<Flight> FindNearestFromAviationstack(const std::string& api_key,
                                                   const ApproachSearch& search) {
  if (api_key.empty()) {
    g_last_error = "No API key provided";
    return std::nullopt;
  }

  std::ostringstream url;
  url << "https://api.aviationstack.com/v1/flights"
      << "?access_key=" << api_key
      << "&arr_iata=LHR"
      << "&flight_status=active"
      << "&limit=100";

  const std::string response = HttpGet(url.str());
  if (response.empty()) {
    return std::nullopt;
  }

  try {
    auto json = nlohmann::json::parse(response);

    if (json.contains("error")) {
      g_last_error = json["error"].value("message", "API error");
      return std::nullopt;
    }

    if (!json.contains("data") || !json["data"].is_array()) {
      g_last_error = "Unexpected API response format";
      return std::nullopt;
    }

    std::optional<Flight> nearest;
    double nearest_distance = 0;
    int total = 0;
    int with_position = 0;
    int matched = 0;

    for (const auto& item : json["data"]) {
      ++total;
      Flight flight = ParseAviationstackFlight(item);
      if (!flight.has_position) {
        continue;
      }
      ++with_position;

      if (!MatchesApproachFilters(flight, search)) {
        continue;
      }
      ++matched;

      if (!nearest || flight.distance_km < nearest_distance) {
        nearest = flight;
        nearest_distance = flight.distance_km;
      }
    }

    if (search.debug) {
      fprintf(stderr,
              "Aviationstack: %d LHR active flights, %d with live position, "
              "%d matched filters\n",
              total, with_position, matched);
    }

    if (!nearest) {
      if (with_position == 0) {
        g_last_error = "No live positions from API";
      } else {
        g_last_error = "No active arrivals within range";
      }
    }

    return nearest;
  } catch (const std::exception& e) {
    g_last_error = std::string("JSON parse error: ") + e.what();
    return std::nullopt;
  }
}

}  // namespace

const std::string& GetLastError() { return g_last_error; }

std::optional<Flight> FindNearestApproachFlight(const ApproachSearch& search,
                                                const std::string& data_source,
                                                const std::string& api_key) {
  g_last_error.clear();

  std::optional<Flight> nearest;
  if (data_source == "aviationstack") {
    nearest = FindNearestFromAviationstack(api_key, search);
  } else {
    nearest = FindNearestFromOpenSky(search);
    if (nearest && !api_key.empty()) {
      EnrichFlightFromAviationstack(*nearest, api_key, search.debug);
    }
  }

  return nearest;
}

std::optional<Flight> GetMockNearestFlight(const ApproachSearch& search) {
  Flight f;
  f.callsign = "BAW258";
  f.flight_number = "BA258";
  f.airline = "British Airways";
  f.origin_iata = "BOS";
  f.origin_name = "Boston";
  f.scheduled_arrival = "15:10";
  f.estimated_arrival = "15:08";
  f.status = "active";
  f.terminal = "5";
  f.gate = "A22";
  f.has_position = true;
  f.latitude = search.observer_lat + 0.003;
  f.longitude = search.observer_lon + 0.002;
  f.altitude_ft = 2800;
  f.speed_kmh = 320;
  f.distance_km = HaversineDistanceKm(
      search.observer_lat, search.observer_lon, f.latitude, f.longitude);
  return f;
}
