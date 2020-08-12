#include "Monitor.h"

#define R_CONSTANT 200

class Temp: public Monitor {
  public:
    static const uint8_t r_fixed = 200;
    static const uint8_t range_length = 34;
    static const int8_t temp_range[range_length] = {-40, -35, -30, -25, -20, -15, -10, -5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115, 120, 125};
    static const float r_range[range_length] = {4397.119, 3088.599, 2197.225, 1581.881, 1151.037, 846.579, 628.988, 471.632, 357.012, 272.5, 209.71, 162.651, 127.08, 100, 79.222, 63.167, 50.677, 40.904, 33.195, 27.091, 22.224, 18.323, 15.184, 12.635, 10.566, 8.873, 7.481, 6.337, 5.384, 4.594, 3.934, 3.38, 2.916, 2.522};

  private:
    uint8_t _temp;
};
