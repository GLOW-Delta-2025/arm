// CmdLib.h (modified: removed positional params — only namedParams)
#ifndef CMDLIB_H
#define CMDLIB_H

#if defined(ARDUINO) && !defined(CMDLIB_ARDUINO)
#define CMDLIB_ARDUINO
#endif

#ifdef CMDLIB_ARDUINO
  #include <WString.h>
#else
  #include <string>
  #include <vector>
  #include <unordered_map>
  #include <sstream>
  #include <cctype>
#endif

namespace cmdlib {

#ifdef CMDLIB_ARDUINO
// -------------------- Arduino Version (named-only params) --------------------
#ifndef CMDLIB_MAX_PARAMS
#define CMDLIB_MAX_PARAMS 12
#endif
#ifndef CMDLIB_MAX_HEADER_PARTS
#define CMDLIB_MAX_HEADER_PARTS 8
#endif

struct NamedParam {
  String key;
  String value;
};

struct Command {
  // Leading header parts (source, destination, etc.), e.g. "MASTER", "[ARM#]"
  String headers[CMDLIB_MAX_HEADER_PARTS];
  int headerCount = 0;

  // The message kind and the command name
  String msgKind;
  String command;

  // Named params only
  NamedParam namedParams[CMDLIB_MAX_PARAMS];
  int namedCount = 0;

  void clear() {
    headerCount = 0;
    msgKind = "";
    command = "";
    namedCount = 0;
    for (int i = 0; i < CMDLIB_MAX_PARAMS; ++i) {
      if (i < CMDLIB_MAX_HEADER_PARTS) headers[i] = "";
      namedParams[i].key = ""; namedParams[i].value = "";
    }
  }

  // header helpers
  bool addHeader(const String &h) {
    if (headerCount >= CMDLIB_MAX_HEADER_PARTS) return false;
    headers[headerCount++] = h;
    return true;
  }
  String getHeader(int i) const { if (i < 0 || i >= headerCount) return ""; return headers[i]; }

  // named param helpers
  bool setNamed(const String &k, const String &v) {
    for (int i = 0; i < namedCount; ++i) {
      if (namedParams[i].key == k) { namedParams[i].value = v; return true; }
    }
    if (namedCount >= CMDLIB_MAX_PARAMS) return false;
    namedParams[namedCount].key = k;
    namedParams[namedCount].value = v;
    namedCount++;
    return true;
  }
  String getNamed(const String &k, const String &def = "") const {
    for (int i = 0; i < namedCount; ++i) if (namedParams[i].key == k) return namedParams[i].value;
    return def;
  }

  // Build string using named params (if any)
  String toString() const {
    String out = "!!";
    for (int i = 0; i < headerCount; ++i) {
      if (i) out += ":";
      out += headers[i];
    }
    if (msgKind.length() > 0) {
      if (headerCount) out += ":";
      out += msgKind;
    }
    if (command.length() > 0) {
      out += ":";
      out += command;
    }
    if (namedCount > 0) {
      out += "{";
      for (int i = 0; i < namedCount; ++i) {
        out += namedParams[i].key;
        out += "=";
        out += namedParams[i].value;
        if (i < namedCount - 1) out += ",";
      }
      out += "}";
    }
    out += "##";
    return out;
  }
};

// Utility: trim spaces
static inline String trimStr(const String &s) {
  int start = 0;
  while (start < s.length() && isspace(s.charAt(start))) start++;
  int end = s.length() - 1;
  while (end >= start && isspace(s.charAt(end))) end--;
  if (end < start) return "";
  return s.substring(start, end + 1);
}

// Parse function for Arduino String (named-only)
static inline bool parse(const String &input, Command &out, String &error) {
  out.clear();
  error = "";

  if (!input.startsWith("!!")) { error = "Missing prefix '!!'"; return false; }
  if (!input.endsWith("##")) { error = "Missing suffix '##'"; return false; }

  int braceOpen = input.indexOf('{');
  int braceClose = input.lastIndexOf('}');

  if (braceOpen == -1 && braceClose != -1) {
    error = "Malformed braces";
    return false;
  }

  int headerEnd = (braceOpen != -1) ? braceOpen : input.lastIndexOf("##");
  if (headerEnd == -1) { error = "Malformed header"; return false; }
  String header = input.substring(2, headerEnd);
  if (header.endsWith(":")) header = header.substring(0, header.length() - 1);

  int start = 0;
  while (start < header.length()) {
    int idx = header.indexOf(':', start);
    String token;
    if (idx == -1) { token = trimStr(header.substring(start)); start = header.length(); }
    else { token = trimStr(header.substring(start, idx)); start = idx + 1; }
    if (token.length() > 0) {
      if (!out.addHeader(token)) { error = "Too many header parts"; return false; }
    }
  }

  if (out.headerCount == 0) { error = "Empty header"; return false; }
  if (out.headerCount == 1) { error = "Incomplete header"; return false; }

  out.command = out.headers[out.headerCount - 1];
  out.msgKind = out.headers[out.headerCount - 2];
  out.headerCount -= 2;

  if (braceOpen != -1) {
    if (braceClose == -1 || braceClose < braceOpen) { error = "Malformed braces"; return false; }
    String inside = input.substring(braceOpen + 1, braceClose);
    int i = 0;
    while (i < inside.length()) {
      while (i < inside.length() && isspace(inside.charAt(i))) i++;
      if (i >= inside.length()) break;
      int startKey = i;
      while (i < inside.length() && inside.charAt(i) != ',') i++;
      String token = trimStr(inside.substring(startKey, i));
      if (token.length() > 0) {
        int eq = token.indexOf('=');
        if (eq == -1) {
          // key only, empty value
          out.setNamed(token, "");
        } else {
          String key = trimStr(token.substring(0, eq));
          String val = trimStr(token.substring(eq + 1));
          out.setNamed(key, val);
        }
      }
      if (i < inside.length() && inside.charAt(i) == ',') i++;
    }
  }

  return true;
}

#else
// -------------------- Standard C++ Version (named-only params) --------------------
using std::string;
using std::vector;
using std::unordered_map;
using std::size_t;

struct Command {
  vector<string> headers; // dynamic
  string msgKind;
  string command;
  unordered_map<string, string> namedParams;

  void clear() {
    headers.clear();
    msgKind.clear();
    command.clear();
    namedParams.clear();
  }

  // header helpers
  void addHeader(const string &h) { headers.push_back(h); }
  string getHeader(size_t i) const { return (i < headers.size()) ? headers[i] : string(); }

  // named param helpers
  void setNamed(const string &k, const string &v) { namedParams[k] = v; }
  string getNamed(const string &k, const string &def = "") const {
    auto it = namedParams.find(k);
    return (it == namedParams.end()) ? def : it->second;
  }

  string toString() const {
    std::ostringstream ss;
    ss << "!!";
    for (size_t i = 0; i < headers.size(); ++i) {
      if (i) ss << ":";
      ss << headers[i];
    }
    if (!msgKind.empty()) {
      if (!headers.empty()) ss << ":";
      ss << msgKind;
    }
    if (!command.empty()) {
      ss << ":" << command;
    }
    if (!namedParams.empty()) {
      ss << "{";
      bool first = true;
      for (auto &kv : namedParams) {
        if (!first) ss << ",";
        first = false;
        ss << kv.first << "=" << kv.second;
      }
      ss << "}";
    }
    ss << "##";
    return ss.str();
  }
};

// trim helper
static inline string trim(const string &s) {
  size_t a = 0;
  while (a < s.size() && isspace((unsigned char)s[a])) ++a;
  if (a == s.size()) return "";
  size_t b = s.size() - 1;
  while (b > a && isspace((unsigned char)s[b])) --b;
  return s.substr(a, b - a + 1);
}

static inline bool parse(const string &input, Command &out, string &error) {
  out.clear();
  error.clear();

  if (input.rfind("!!", 0) != 0) { error = "Missing prefix '!!'"; return false; }
  if (input.size() < 4 || input.substr(input.size() - 2) != "##") { error = "Missing suffix '##'"; return false; }

  auto braceOpen = input.find('{');
  auto braceClose = input.rfind('}');

  if (braceOpen == string::npos && braceClose != string::npos) {
    error = "Malformed braces";
    return false;
  }

  size_t headerEnd = (braceOpen != string::npos) ? braceOpen : input.size() - 2;
  if (headerEnd == string::npos) { error = "Malformed header"; return false; }
  string header = input.substr(2, headerEnd - 2);
  if (!header.empty() && header.back() == ':') header.pop_back();

  size_t i = 0;
  while (i < header.size()) {
    size_t idx = header.find(':', i);
    string token;
    if (idx == string::npos) { token = trim(header.substr(i)); i = header.size(); }
    else { token = trim(header.substr(i, idx - i)); i = idx + 1; }
    if (!token.empty()) out.addHeader(token);
  }

  if (out.headers.empty()) { error = "Empty header"; return false; }
  if (out.headers.size() == 1) { error = "Incomplete header"; return false; }

  out.command = out.headers.back();
  out.msgKind = out.headers[out.headers.size() - 2];
  out.headers.resize(out.headers.size() - 2);

  if (braceOpen != string::npos) {
    if (braceClose == string::npos || braceClose < braceOpen) { error = "Malformed braces"; return false; }
    string inside = input.substr(braceOpen + 1, braceClose - (braceOpen + 1));
    size_t p = 0;
    while (p < inside.size()) {
      while (p < inside.size() && isspace((unsigned char)inside[p])) ++p;
      if (p >= inside.size()) break;
      size_t startKey = p;
      while (p < inside.size() && inside[p] != ',') ++p;
      string token = trim(inside.substr(startKey, p - startKey));
      if (!token.empty()) {
        auto eq = token.find('=');
        if (eq == string::npos) {
          // key only -> empty value
          out.setNamed(token, "");
        } else {
          string key = trim(token.substr(0, eq));
          string val = trim(token.substr(eq + 1));
          out.setNamed(key, val);
        }
      }
      if (p < inside.size() && inside[p] == ',') ++p;
    }
  }

  return true;
}

#endif // CMDLIB_ARDUINO

} // namespace cmdlib

#endif // CMDLIB_H


// ---------- test_cmdlib.cpp (updated tests: named-only params) ----------

// Simple unit tests for CmdLib.h (named-only variant)
// Compile: g++ -std=c++17 test_cmdlib.cpp -o test_cmdlib
// Run: ./test_cmdlib

