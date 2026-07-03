#include "display.h"

#include <unistd.h>

#include <cmath>
#include <sstream>
#include <iomanip>

FlightDisplay::FlightDisplay(rgb_matrix::RGBMatrix* matrix,
                             const std::string& font_path)
    : matrix_(matrix), canvas_(matrix->CreateFrameCanvas()) {
  if (!font_.LoadFont(font_path.c_str())) {
    fprintf(stderr, "Could not load font %s\n", font_path.c_str());
    exit(1);
  }
}

void FlightDisplay::Clear() {
  canvas_->Clear();
}

void FlightDisplay::DrawLine(int y, const std::string& text,
                             rgb_matrix::Color color) {
  rgb_matrix::DrawText(canvas_, font_, 0, y, color, nullptr, text.c_str());
}

rgb_matrix::Color FlightDisplay::StatusColor(const Flight& flight) const {
  if (flight.status == "landed") {
    return rgb_matrix::Color(0, 200, 80);
  }
  if (IsDelayed(flight)) {
    return rgb_matrix::Color(255, 160, 0);
  }
  if (flight.status == "cancelled") {
    return rgb_matrix::Color(255, 40, 40);
  }
  if (flight.status == "active") {
    return rgb_matrix::Color(80, 160, 255);
  }
  return rgb_matrix::Color(200, 200, 200);
}

void FlightDisplay::ShowTitle(const std::string& line1,
                              const std::string& line2) {
  Clear();
  DrawLine(8, line1, rgb_matrix::Color(255, 200, 0));
  if (!line2.empty()) {
    DrawLine(20, line2, rgb_matrix::Color(180, 180, 180));
  }
  canvas_ = matrix_->SwapOnVSync(canvas_);
}

namespace {

std::string FormatDistance(double km) {
  if (km < 1.0) {
    return std::to_string(static_cast<int>(std::round(km * 1000))) + "m";
  }
  std::ostringstream out;
  out << std::fixed << std::setprecision(1) << km << "km";
  return out.str();
}

}  // namespace

void FlightDisplay::ShowApproachFlight(const Flight& flight) {
  const rgb_matrix::Color white(220, 220, 220);
  const rgb_matrix::Color dim(120, 120, 120);

  Clear();

  DrawLine(7, flight.flight_number, rgb_matrix::Color(255, 220, 0));

  std::string origin = flight.origin_iata;
  if (!flight.origin_name.empty()) {
    std::string name = flight.origin_name;
    if (name.size() > 10) {
      name = name.substr(0, 10);
    }
    origin += " " + name;
  }
  DrawLine(17, origin, white);

  std::ostringstream position;
  position << FormatDistance(flight.distance_km);
  if (flight.altitude_ft > 0) {
    position << " " << flight.altitude_ft << "ft";
  }
  DrawLine(27, position.str(), rgb_matrix::Color(80, 200, 255));

  std::ostringstream motion;
  if (!flight.terminal.empty() || !flight.gate.empty()) {
    if (!flight.terminal.empty()) {
      motion << "T" << flight.terminal;
    }
    if (!flight.gate.empty()) {
      if (motion.tellp() > 0) {
        motion << " G" << flight.gate;
      } else {
        motion << "G" << flight.gate;
      }
    }
  } else {
    if (flight.speed_kmh > 0) {
      motion << flight.speed_kmh << "km/h";
    }
    std::string eta = flight.estimated_arrival.empty()
                          ? flight.scheduled_arrival
                          : flight.estimated_arrival;
    if (!eta.empty()) {
      if (motion.tellp() > 0) {
        motion << " ETA" << eta;
      } else {
        motion << "ETA " << eta;
      }
    }
  }
  if (motion.tellp() == 0) {
    motion << flight.status;
  }
  DrawLine(37, motion.str(), StatusColor(flight));

  std::string airline = flight.airline;
  if (airline.size() > 14) {
    airline = airline.substr(0, 14);
  }
  if (!airline.empty()) {
    DrawLine(47, airline, dim);
  }

  canvas_ = matrix_->SwapOnVSync(canvas_);
}

void FlightDisplay::ShowFlights(const std::vector<Flight>& flights,
                                int display_seconds) {
  const rgb_matrix::Color white(220, 220, 220);
  const rgb_matrix::Color dim(120, 120, 120);

  for (const auto& flight : flights) {
    Clear();

    // Line 1: flight number (large-ish, top)
    DrawLine(7, flight.flight_number, rgb_matrix::Color(255, 220, 0));

    // Line 2: origin
    std::string origin = flight.origin_iata;
    if (!flight.origin_name.empty()) {
      // Truncate long airport names for 64px width
      std::string name = flight.origin_name;
      if (name.size() > 10) {
        name = name.substr(0, 10);
      }
      origin += " " + name;
    }
    DrawLine(17, origin, white);

    // Line 3: arrival time
    std::string eta = flight.estimated_arrival.empty()
                          ? flight.scheduled_arrival
                          : flight.estimated_arrival;
    std::string time_line = "ETA " + (eta.empty() ? "--:--" : eta);
    if (IsDelayed(flight) && flight.delay_minutes > 0) {
      time_line += " +" + std::to_string(flight.delay_minutes) + "m";
    }
    DrawLine(27, time_line, StatusColor(flight));

    // Line 4: terminal / gate / status
    std::ostringstream details;
    if (!flight.terminal.empty()) {
      details << "T" << flight.terminal;
    }
    if (!flight.gate.empty()) {
      if (details.tellp() > 0) details << " ";
      details << "G" << flight.gate;
    }
    if (details.tellp() == 0) {
      details << flight.status;
    } else {
      details << " " << flight.status;
    }
    DrawLine(37, details.str(), dim);

    // Line 5: airline (truncated)
    std::string airline = flight.airline;
    if (airline.size() > 14) {
      airline = airline.substr(0, 14);
    }
    if (!airline.empty()) {
      DrawLine(47, airline, dim);
    }

    canvas_ = matrix_->SwapOnVSync(canvas_);
    sleep(display_seconds);
  }
}

void FlightDisplay::ScrollText(const std::string& text,
                               rgb_matrix::Color color, int speed_ms) {
  const int text_width = rgb_matrix::MeasureText(font_, text.c_str());

  for (int x = matrix_->width(); x > -text_width; --x) {
    Clear();
    rgb_matrix::DrawText(canvas_, font_, x, 12, color, nullptr, text.c_str());
    canvas_ = matrix_->SwapOnVSync(canvas_);
    usleep(speed_ms * 1000);
  }
}
