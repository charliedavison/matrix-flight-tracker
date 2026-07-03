#include "flight.h"

bool IsDelayed(const Flight& flight) {
  return flight.delay_minutes > 0 ||
         flight.status == "delayed" ||
         flight.status == "diverted";
}
