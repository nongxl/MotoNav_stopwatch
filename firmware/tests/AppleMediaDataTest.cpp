#include <stdint.h>
#include "../boards/175/MotoNav/AppleMediaData.h"
#include <cassert>
#include <iostream>
int main() {
  int checks = 0;
  auto check = [&](bool p) { assert(p); ++checks; };
  using namespace AppleMediaData;
  auto p = playback("1,1.000,20.5"); check(p.known && p.state == 1 && p.rate == 1 && p.elapsed == 20.5);
  p = playback("0,0.000,0"); check(p.known && p.state == 0 && p.elapsed == 0);
  p = playback("2,-2.0,90"); check(p.known && p.state == 2 && p.rate == -2);
  for (auto text : {"", "bad", "4,1,1", "-1,1,1", "1,nan,1", "1,1,nan", "1,1,-1", "1,1,1junk", "1,,1", "1,1,", "1,1,1,2"}) check(!playback(text).known);
  check(duration("240.125") == 240.125); check(duration("nan") == 0); check(duration("-1") == 0); check(duration("20x") == 0);
  const uint8_t title[] = {0xe5,0xa4,0x9c,0xe6,0x9b,0xb2}; // 夜曲
  check(utf8Prefix(title, 6) == 6); check(utf8Prefix(title, 5) == 3); check(utf8Prefix(title, 2) == 0);
  const uint8_t emoji[] = {65,0xf0,0x9f,0x8e,0xb5};
  check(utf8Prefix(emoji, 5) == 5); check(utf8Prefix(emoji, 4) == 1); check(utf8Prefix(nullptr, 0) == 0);
  for (int f = 0; f < 6; ++f) check(field(entity(f), attribute(f)) == f);
  check(field(1,0) == -1); check(field(2,4) == -1); check(field(0,2) == -1);
  std::cout << "AMS data: " << checks << " checks passed\n";
}
