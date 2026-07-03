#include "flight_api.h"

#include "geo.h"

#include <nlohmann/json.hpp>

#include <curl/curl.h>
#include <sstream>

namespace {

std::string g_last_error;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* out) {
  out->append(static_cast<char*>(contents), size * nmemb);
  return size * nmemb;
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

Flight ParseFlight(const nlohmann::json& item) {
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
        JsonToDouble(live.value("longitude", nlohmann::json()), &lon) &&
        lat != 0 && lon != 0) {
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

std::string FetchActiveArrivals(const std::string& api_key) {
  std::ostringstream url;
  url << "https://api.aviationstack.com/v1/flights"
      << "?access_key=" << api_key
      << "&arr_iata=LHR"
      << "&flight_status=active"
      << "&limit=100";

  CURL* curl = curl_easy_init();
  if (!curl) {
    g_last_error = "Failed to init curl";
    return {};
  }

  std::string response;
  curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

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

bool MatchesApproachFilters(const Flight& flight, const ApproachSearch& search) {
  if (!flight.has_position) {
    return false;
  }
  if (flight.altitude_ft > search.max_altitude_ft) {
    return false;
  }
  if (flight.altitude_ft > 0 && flight.altitude_ft < search.min_altitude_ft) {
    return false;
  }
  return flight.distance_km <= search.max_distance_km;
}

}  // namespace

const std::string& GetLastError() { return g_last_error; }

std::optional<Flight> FindNearestApproachFlight(const std::string& api_key,
                                                const ApproachSearch& search) {
  g_last_error.clear();

  if (api_key.empty()) {
    g_last_error = "No API key provided";
    return std::nullopt;
  }

  const std::string response = FetchActiveArrivals(api_key);
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

    for (const auto& item : json["data"]) {
      Flight flight = ParseFlight(item);
      if (!flight.has_position) {
        continue;
      }

      flight.distance_km = HaversineDistanceKm(
          search.observer_lat, search.observer_lon,
          flight.latitude, flight.longitude);

      if (!MatchesApproachFilters(flight, search)) {
        continue;
      }

      if (!nearest || flight.distance_km < nearest_distance) {
        nearest = flight;
        nearest_distance = flight.distance_km;
      }
    }

    if (!nearest) {
      g_last_error = "No active arrivals with position within range";
    }

    return nearest;
  } catch (const std::exception& e) {
    g_last_error = std::string("JSON parse error: ") + e.what();
    return std::nullopt;
  }
}

std::optional<Flight> GetMockNearestFlight(const ApproachSearch& search) {
  Flight f;
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
