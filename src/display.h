#pragma once

#include "flight.h"

#include <led-matrix.h>
#include <graphics.h>

#include <string>
#include <vector>

class FlightDisplay {
 public:
  FlightDisplay(rgb_matrix::RGBMatrix* matrix, const std::string& font_path);

  // Show a title screen (e.g. "LHR ARRIVALS" or error message).
  void ShowTitle(const std::string& line1, const std::string& line2 = "");

  // Cycle through flights, showing each for display_seconds.
  void ShowFlights(const std::vector<Flight>& flights, int display_seconds);

  // Scroll a single line of text across the display.
  void ScrollText(const std::string& text, rgb_matrix::Color color, int speed_ms);

 private:
  rgb_matrix::RGBMatrix* matrix_;
  rgb_matrix::Font font_;
  rgb_matrix::FrameCanvas* canvas_;

  void Clear();
  void DrawLine(int y, const std::string& text, rgb_matrix::Color color);
  rgb_matrix::Color StatusColor(const Flight& flight) const;
};
