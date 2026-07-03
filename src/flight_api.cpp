#include "flight_api.h"

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
  // Aviationstack returns "2026-07-03T14:30:00+00:00" — show HH:MM in UTC.
  if (datetime.size() >= 16) {
    return datetime.substr(11, 5);
  }
  return datetime;
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

  if (f.flight_number.empty()) {
    f.flight_number = "???";
  }
  if (f.origin_iata.empty()) {
    f.origin_iata = "---";
  }

  return f;
}

}  // namespace

const std::string& GetLastError() { return g_last_error; }

std::vector<Flight> FetchHeathrowArrivals(const std::string& api_key, int limit) {
  g_last_error.clear();
  std::vector<Flight> flights;

  if (api_key.empty()) {
    g_last_error = "No API key provided";
    return flights;
  }

  std::ostringstream url;
  url << "https://api.aviationstack.com/v1/flights"
      << "?access_key=" << api_key
      << "&arr_iata=LHR"
      << "&limit=" << limit;

  CURL* curl = curl_easy_init();
  if (!curl) {
    g_last_error = "Failed to init curl";
    return flights;
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
    return flights;
  }

  if (http_code != 200) {
    g_last_error = "HTTP " + std::to_string(http_code) + ": " + response.substr(0, 200);
    return flights;
  }

  try {
    auto json = nlohmann::json::parse(response);

    if (json.contains("error")) {
      g_last_error = json["error"].value("message", "API error");
      return flights;
    }

    if (!json.contains("data") || !json["data"].is_array()) {
      g_last_error = "Unexpected API response format";
      return flights;
    }

    for (const auto& item : json["data"]) {
      flights.push_back(ParseFlight(item));
    }
  } catch (const std::exception& e) {
    g_last_error = std::string("JSON parse error: ") + e.what();
  }

  return flights;
}

std::vector<Flight> GetMockArrivals() {
  return {
      {"BA178", "British Airways", "JFK", "New York", "14:30", "14:45", "active", "5", "A12", 15},
      {"EK001", "Emirates", "DXB", "Dubai", "15:10", "15:10", "scheduled", "3", "B08", 0},
      {"LH921", "Lufthansa", "FRA", "Frankfurt", "15:25", "15:40", "active", "2", "C15", 15},
      {"AF1280", "Air France", "CDG", "Paris", "15:50", "15:50", "landed", "4", "D22", 0},
      {"QR003", "Qatar Airways", "DOH", "Doha", "16:05", "16:20", "active", "4", "A05", 15},
      {"SQ317", "Singapore Airlines", "SIN", "Singapore", "16:30", "16:30", "scheduled", "2", "B18", 0},
  };
}
