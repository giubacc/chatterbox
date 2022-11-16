#include "globals.h"

namespace utils {

int read_file(const char *fname,
              std::stringstream &out,
              spdlog::logger *log)
{
  FILE *file = fopen(fname, "rb");
  if(file == nullptr) {
    if(log) {
      log->error("fopen:{}", strerror(errno));
    }
    return -1;
  }

  if(fseek(file, 0, SEEK_END)) {
    if(log) {
      log->error("fseek:{}", strerror(errno));
    }
  }
  long size = ftell(file);
  if(size<0) {
    if(log) {
      log->error("ftell:{}", strerror(errno));
    }
  }
  rewind(file);

  std::unique_ptr<char> chars(new char[size + 1]);
  chars.get()[size] = '\0';
  for(size_t i = 0; i < (size_t)size;) {
    i += fread(&chars.get()[i], 1, size - i, file);
    int res = 0;
    if((res = ferror(file))) {
      if(log) {
        log->error("fread:{}", strerror(errno));
      }
      fclose(file);
      return res;
    }
  }
  fclose(file);

  out << chars.get();
  return 0;
}

}
