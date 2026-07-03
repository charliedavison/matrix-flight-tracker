#pragma once

#include <cmath>

constexpr double kEarthRadiusKm = 6371.0;

inline double HaversineDistanceKm(double lat1, double lon1, double lat2,
                                  double lon2) {
  const auto to_rad = [](double deg) { return deg * M_PI / 180.0; };
  const double dlat = to_rad(lat2 - lat1);
  const double dlon = to_rad(lon2 - lon1);
  const double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
                   std::cos(to_rad(lat1)) * std::cos(to_rad(lat2)) *
                       std::sin(dlon / 2) * std::sin(dlon / 2);
  return kEarthRadiusKm * 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
}
