// A minimal test harness: TEST(name) registers a case, CHECK fails it, main runs one case by
// name (CTest gives each its own test) or all of them.
#pragma once

#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace harness {

struct Case {
  const char* name;
  std::function<void()> fn;
};

inline std::vector<Case>& cases() {
  static std::vector<Case> all;
  return all;
}

struct Register {
  Register(const char* name, std::function<void()> fn) { cases().push_back({name, std::move(fn)}); }
};

struct Failure : std::exception {
  std::string message;
  explicit Failure(std::string m) : message(std::move(m)) {}
  const char* what() const noexcept override { return message.c_str(); }
};

struct Skip : std::exception {};

template <class A, class B>
void check_eq(const A& a, const B& b, const char* ea, const char* eb, const char* file, int line) {
  if (!(a == b)) {
    std::ostringstream s;
    s << file << ":" << line << ": CHECK_EQ(" << ea << ", " << eb << ") failed: " << a
      << " != " << b;
    throw Failure(s.str());
  }
}

inline int run(int argc, char** argv) {
  if (argc > 2 && std::strcmp(argv[1], "--check-count") == 0) {
    std::size_t want = std::strtoul(argv[2], nullptr, 10);
    if (cases().size() != want) {
      std::cerr << cases().size() << " cases compiled, " << want
                << " registered with CTest: update tests/CMakeLists.txt\n";
      return 1;
    }
    return 0;
  }
  int failed = 0, ran = 0, skipped = 0;
  for (const auto& c : cases()) {
    if (argc > 1 && std::strcmp(argv[1], c.name) != 0) continue;
    ++ran;
    try {
      c.fn();
      std::cout << "PASS " << c.name << "\n";
    } catch (const Skip&) {
      ++skipped;
      std::cout << "SKIP " << c.name << "\n";
    } catch (const std::exception& e) {
      ++failed;
      std::cout << "FAIL " << c.name << ": " << e.what() << "\n";
    }
  }
  if (ran == 0) {
    std::cerr << "no test named " << (argc > 1 ? argv[1] : "") << "\n";
    return 1;
  }
  if (failed) return 1;
  if (skipped == ran) return 77;
  return 0;
}

}  // namespace harness

#define HARNESS_CAT2(a, b) a##b
#define HARNESS_CAT(a, b) HARNESS_CAT2(a, b)
#define TEST(name)                                                               \
  static void name();                                                            \
  static const harness::Register HARNESS_CAT(register_, name)(#name, name);      \
  static void name()

#define CHECK(cond)                                                                       \
  do {                                                                                    \
    if (!(cond)) {                                                                        \
      std::ostringstream s_;                                                              \
      s_ << __FILE__ << ":" << __LINE__ << ": CHECK(" #cond ") failed";                   \
      throw harness::Failure(s_.str());                                                   \
    }                                                                                     \
  } while (0)

#define CHECK_EQ(a, b) harness::check_eq((a), (b), #a, #b, __FILE__, __LINE__)

#define CHECK_THROWS_AS(expr, kind_)                                                       \
  [&]() -> probnode::Error {                                                               \
    try {                                                                                  \
      (void)(expr);                                                                        \
    } catch (const probnode::Error& e_) {                                                  \
      if (e_.kind() != (kind_)) {                                                          \
        throw harness::Failure(std::string(__FILE__) + ": " #expr " threw " + e_.what() + \
                               ", of another kind");                                       \
      }                                                                                    \
      return e_;                                                                           \
    }                                                                                      \
    throw harness::Failure(std::string(__FILE__) + ": " #expr " did not throw");          \
  }()
