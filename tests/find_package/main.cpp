#include <probnode/client.hpp>

#include <iostream>

int main() {
  // No network: the installed headers and library link, and the bundled sha256 answers.
  std::cout << "probnode " << probnode::version() << " "
            << probnode::sha256_hex("abc").substr(0, 12) << "\n";
  probnode::ClientOptions o;
  o.from_env = false;
  probnode::Client pn(o);
  return pn.base_url() == probnode::kDefaultBaseUrl ? 0 : 1;
}
