// #include <crcutil/crc32c_sse4.h>
#include <snappy.h>
// #include <zstd.h>

#include <iostream>
int main() {
  std::cout <<snappy::kBlockLog << std::endl;
  // ZSTD_isError(0);
  return 0;
}