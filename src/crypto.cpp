#include "crypto.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

//crypto++ headers
#include "cryptlib.h"
#include "hmac.h"
#include "hex.h"
#include "sha.h"
#include "base64.h"
#include "filters.h"

namespace crypto {

std::string hmac_sha1(const std::string &key,
                      const std::string &data)
{
  if(key.empty()) {
    return "";
  }
  std::vector<CryptoPP::byte> byte_key;
  try {

    std::transform(key.begin(), key.end(), std::back_inserter(byte_key), [](auto it) -> CryptoPP::byte { return (CryptoPP::byte)it; });

    // Calculate HMAC
    CryptoPP::HMAC<CryptoPP::SHA1> hmac(&byte_key.at(0), byte_key.size());

    //apply HMAC
    std::string result;
    CryptoPP::StringSource(data, true, new CryptoPP::HashFilter(hmac, new CryptoPP::StringSink(result)));
    return result;
  } catch(const CryptoPP::Exception &e) {
    return "";
  }
}

std::string hmac_sha256(const std::string &key,
                        const std::string &data)
{
  if(key.empty()) {
    return "";
  }
  std::vector<CryptoPP::byte> byte_key;
  try {

    std::transform(key.begin(), key.end(), std::back_inserter(byte_key), [](auto it) -> CryptoPP::byte { return (CryptoPP::byte)it; });

    // Calculate HMAC
    CryptoPP::HMAC<CryptoPP::SHA256> hmac(&byte_key.at(0), byte_key.size());

    //apply HMAC
    std::string result;
    CryptoPP::StringSource(data, true, new CryptoPP::HashFilter(hmac, new CryptoPP::StringSink(result)));
    return result;
  } catch(const CryptoPP::Exception &e) {
    return "";
  }
}

std::string sha256(const std::string &data)
{
  CryptoPP::byte const *pbData = (CryptoPP::byte *)data.data();
  size_t nDataLen = data.length();
  CryptoPP::byte abDigest[CryptoPP::SHA256::DIGESTSIZE];
  CryptoPP::SHA256().CalculateDigest(abDigest, pbData, nDataLen);
  return std::string((char *)abDigest, CryptoPP::SHA256::DIGESTSIZE);
}

std::string hex(const std::string &data)
{
  std::string result;
  CryptoPP::StringSource(data, true, new CryptoPP::HexEncoder(new CryptoPP::StringSink(result), false));
  return result;
}

std::string base64(const std::string &data)
{
  std::string result;
  CryptoPP::StringSource(data, true, new CryptoPP::Base64Encoder(new CryptoPP::StringSink(result), false));
  return result;
}

}
