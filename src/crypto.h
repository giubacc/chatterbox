#pragma once
#include "config.h"
#include <vector>

namespace crypto {

std::string hmac_sha1(const std::string &key,
                      const std::string &data);

std::string hmac_sha256(const std::string &key,
                        const std::string &data);

std::string sha256(const std::string &data);

std::string hex(const std::string &data);

std::string base64(const std::string &data);

}
