#include "opendva/domain/ModelSerializer.h"

#include <cstdint>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace opendva {
namespace {

// ===========================================================================
// Minimal XML model: an element has a tag, attributes (ordered), text content
// and child elements. Enough to represent and re-parse the OpenDVAModel schema.
// ===========================================================================
struct XmlNode {
    std::string tag;
    std::vector<std::pair<std::string, std::string>> attrs;
    std::string text;  // leaf text (only meaningful when no children)
    std::vector<std::unique_ptr<XmlNode>> children;

    const std::string* attr(const std::string& key) const {
        for (const auto& a : attrs) {
            if (a.first == key) return &a.second;
        }
        return nullptr;
    }
    const XmlNode* child(const std::string& t) const {
        const XmlNode* found = nullptr;
        for (const auto& c : children) {
            if (c->tag != t) continue;
            if (found) throw std::invalid_argument("XML element has duplicate child tag");
            found = c.get();
        }
        return found;
    }
};

void requireNoText(const XmlNode& n) {
    for (char c : n.text) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            throw std::invalid_argument("XML element has unexpected text");
        }
    }
}

void requireOnlyChildren(const XmlNode& n, const std::vector<std::string>& allowed) {
    requireNoText(n);
    for (const auto& c : n.children) {
        bool found = false;
        for (const auto& tag : allowed) {
            if (c->tag == tag) {
                found = true;
                break;
            }
        }
        if (!found) throw std::invalid_argument("XML element has unexpected child tag");
    }
}

void requireOnlyAttrs(const XmlNode& n, const std::vector<std::string>& allowed) {
    for (const auto& a : n.attrs) {
        bool found = false;
        for (const auto& key : allowed) {
            if (a.first == key) {
                found = true;
                break;
            }
        }
        if (!found) throw std::invalid_argument("XML element has unexpected attribute");
    }
}

void requireNoChildren(const XmlNode& n) {
    requireNoText(n);
    if (!n.children.empty()) throw std::invalid_argument("XML element has unexpected child tag");
}

// ---- Writer ---------------------------------------------------------------

void escapeInto(std::string& out, const std::string& s) {
    for (char c : s) {
        switch (c) {
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '&': out += "&amp;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c; break;
        }
    }
}

void writeNode(std::string& out, const XmlNode& node, int depth) {
    const std::string indent(static_cast<std::size_t>(depth) * 2, ' ');
    out += indent;
    out += '<';
    out += node.tag;
    for (const auto& a : node.attrs) {
        out += ' ';
        out += a.first;
        out += "=\"";
        escapeInto(out, a.second);
        out += '"';
    }
    if (node.children.empty() && node.text.empty()) {
        out += "/>\n";
        return;
    }
    out += '>';
    if (node.children.empty()) {
        escapeInto(out, node.text);
        out += "</";
        out += node.tag;
        out += ">\n";
        return;
    }
    out += '\n';
    for (const auto& c : node.children) {
        writeNode(out, *c, depth + 1);
    }
    out += indent;
    out += "</";
    out += node.tag;
    out += ">\n";
}

// ---- Reader (recursive descent) -------------------------------------------

class XmlParser {
public:
    explicit XmlParser(const std::string& src) : s_(src) {}

    std::unique_ptr<XmlNode> parse() {
        skipInitialUtf8Bom();
        skipProlog();
        skipWs();
        if (pos_ >= s_.size() || s_[pos_] != '<') return nullptr;
        return parseElement();
    }

    bool ok() const { return ok_; }

    bool finished() {
        skipEpilog();
        return ok_ && pos_ == s_.size();
    }

private:
    const std::string& s_;
    std::size_t pos_{0};
    bool ok_{true};

    void fail() { ok_ = false; }

    void skipInitialUtf8Bom() {
        constexpr char bom[] = "\xEF\xBB\xBF";
        if (pos_ == 0 && s_.compare(0, 3, bom) == 0) {
            pos_ = 3;
        }
    }

    void skipWs() {
        while (pos_ < s_.size() &&
               (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r')) {
            ++pos_;
        }
    }

    bool skipComment() {
        std::size_t end = s_.find("-->", pos_);
        if (end == std::string::npos) { fail(); return false; }
        if (s_.find("--", pos_ + 4) < end) { fail(); return false; }
        for (std::size_t i = pos_ + 4; i < end;) {
            std::uint32_t cp = 0;
            std::size_t next = i;
            if (!decodeUtf8CodePoint(s_, i, cp, next) || next > end ||
                !isAllowedXmlCodePoint(cp)) {
                fail();
                return false;
            }
            i = next;
        }
        pos_ = end + 3;
        return true;
    }

    static bool isNameStart(char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == ':';
    }

    static bool isNameChar(char c) {
        return isNameStart(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
    }

    static char lowerAscii(char c) {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }

    static bool equalsAsciiNoCase(const std::string& lhs, const char* rhs) {
        std::size_t i = 0;
        for (; rhs[i] != '\0'; ++i) {
            if (i >= lhs.size() || lowerAscii(lhs[i]) != rhs[i]) return false;
        }
        return i == lhs.size();
    }

    static void skipXmlDeclWs(const std::string& data, std::size_t& pos) {
        while (pos < data.size() &&
               (data[pos] == ' ' || data[pos] == '\t' || data[pos] == '\n' || data[pos] == '\r')) {
            ++pos;
        }
    }

    static bool parseXmlDeclPseudoAttribute(const std::string& data, std::size_t& pos,
                                            std::string& name, std::string& value) {
        skipXmlDeclWs(data, pos);
        if (pos >= data.size() || !isNameStart(data[pos])) return false;
        const std::size_t nameStart = pos++;
        while (pos < data.size() && isNameChar(data[pos])) ++pos;
        name = data.substr(nameStart, pos - nameStart);
        skipXmlDeclWs(data, pos);
        if (pos >= data.size() || data[pos] != '=') return false;
        ++pos;
        skipXmlDeclWs(data, pos);
        if (pos >= data.size() || (data[pos] != '"' && data[pos] != '\'')) return false;
        const char quote = data[pos++];
        const std::size_t valueStart = pos;
        while (pos < data.size() && data[pos] != quote) {
            if (data[pos] == '<') return false;
            ++pos;
        }
        if (pos >= data.size() || pos == valueStart) return false;
        value = data.substr(valueStart, pos - valueStart);
        ++pos;
        return true;
    }

    static bool isValidXmlEncodingName(const std::string& value) {
        if (value.empty() ||
            !((value[0] >= 'A' && value[0] <= 'Z') || (value[0] >= 'a' && value[0] <= 'z'))) {
            return false;
        }
        for (std::size_t i = 1; i < value.size(); ++i) {
            const char c = value[i];
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) {
                return false;
            }
        }
        return true;
    }

    static bool isAllowedXmlCodePoint(std::uint32_t cp) {
        return cp == 0x09 || cp == 0x0A || cp == 0x0D ||
               (cp >= 0x20 && cp <= 0xD7FF) ||
               (cp >= 0xE000 && cp <= 0xFFFD) ||
               (cp >= 0x10000 && cp <= 0x10FFFF);
    }

    static bool decodeUtf8CodePoint(const std::string& data, std::size_t pos,
                                    std::uint32_t& cp, std::size_t& next) {
        if (pos >= data.size()) return false;
        const auto byte = [&](std::size_t index) {
            return static_cast<unsigned char>(data[index]);
        };
        const unsigned char b0 = byte(pos);
        if (b0 <= 0x7F) {
            cp = b0;
            next = pos + 1;
            return true;
        }

        std::uint32_t value = 0;
        std::size_t length = 0;
        if (b0 >= 0xC2 && b0 <= 0xDF) {
            value = b0 & 0x1F;
            length = 2;
        } else if (b0 >= 0xE0 && b0 <= 0xEF) {
            value = b0 & 0x0F;
            length = 3;
        } else if (b0 >= 0xF0 && b0 <= 0xF4) {
            value = b0 & 0x07;
            length = 4;
        } else {
            return false;
        }
        if (pos + length > data.size()) return false;

        for (std::size_t offset = 1; offset < length; ++offset) {
            const unsigned char b = byte(pos + offset);
            if ((b & 0xC0) != 0x80) return false;
            value = (value << 6) | (b & 0x3F);
        }
        if ((length == 3 && value < 0x800) ||
            (length == 4 && value < 0x10000) ||
            value > 0x10FFFF ||
            (value >= 0xD800 && value <= 0xDFFF)) {
            return false;
        }

        cp = value;
        next = pos + length;
        return true;
    }

    static int hexDigitValue(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    static bool appendUtf8(std::string& out, std::uint32_t cp) {
        if (!isAllowedXmlCodePoint(cp)) return false;
        if (cp <= 0x7F) {
            out += static_cast<char>(cp);
        } else if (cp <= 0x7FF) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        return true;
    }

    static bool appendNumericCharacterReference(const std::string& in,
                                                std::size_t& pos,
                                                std::string& out) {
        if (in.compare(pos, 2, "&#") != 0) return false;
        std::size_t scan = pos + 2;
        int base = 10;
        if (scan < in.size() && (in[scan] == 'x' || in[scan] == 'X')) {
            base = 16;
            ++scan;
        }
        if (scan >= in.size()) return false;

        std::uint32_t cp = 0;
        bool sawDigit = false;
        while (scan < in.size() && in[scan] != ';') {
            const int digit = base == 16 ? hexDigitValue(in[scan])
                                         : (in[scan] >= '0' && in[scan] <= '9'
                                                ? in[scan] - '0'
                                                : -1);
            if (digit < 0 || digit >= base) return false;
            if (cp > (0x10FFFFu - static_cast<std::uint32_t>(digit)) /
                         static_cast<std::uint32_t>(base)) {
                return false;
            }
            cp = cp * static_cast<std::uint32_t>(base) +
                 static_cast<std::uint32_t>(digit);
            sawDigit = true;
            ++scan;
        }
        if (!sawDigit || scan >= in.size() || in[scan] != ';') return false;
        if (!appendUtf8(out, cp)) return false;
        pos = scan + 1;
        return true;
    }

    static bool xmlDeclarationStartsWithVersion(const std::string& data) {
        std::size_t pos = 0;
        std::string name;
        std::string value;
        if (!parseXmlDeclPseudoAttribute(data, pos, name, value) || name != "version") return false;
        if (value != "1.0") return false;
        bool seenEncoding = false;
        bool seenStandalone = false;
        for (;;) {
            const std::size_t wsStart = pos;
            skipXmlDeclWs(data, pos);
            const bool hadWhitespace = pos > wsStart;
            if (pos >= data.size()) return true;
            if (!hadWhitespace) return false;
            if (!parseXmlDeclPseudoAttribute(data, pos, name, value)) return false;
            if (name == "version") return false;
            if (name == "encoding") {
                if (seenEncoding || seenStandalone) return false;
                if (!isValidXmlEncodingName(value)) return false;
                if (!equalsAsciiNoCase(value, "utf-8")) return false;
                seenEncoding = true;
            } else if (name == "standalone") {
                if (seenStandalone) return false;
                if (value != "yes" && value != "no") return false;
                seenStandalone = true;
            } else {
                return false;
            }
        }
    }

    bool skipProcessingInstruction(std::string* target = nullptr, bool* hasData = nullptr,
                                   std::string* data = nullptr) {
        std::size_t end = s_.find("?>", pos_);
        if (end == std::string::npos) { fail(); return false; }
        for (std::size_t i = pos_ + 2; i < end;) {
            std::uint32_t cp = 0;
            std::size_t next = i;
            if (!decodeUtf8CodePoint(s_, i, cp, next) || next > end ||
                !isAllowedXmlCodePoint(cp)) {
                fail();
                return false;
            }
            i = next;
        }
        if (pos_ + 2 >= end || !isNameStart(s_[pos_ + 2])) { fail(); return false; }
        std::size_t targetEnd = pos_ + 3;
        while (targetEnd < end && isNameChar(s_[targetEnd])) ++targetEnd;
        if (targetEnd < end &&
            s_[targetEnd] != ' ' && s_[targetEnd] != '\t' &&
            s_[targetEnd] != '\n' && s_[targetEnd] != '\r') {
            fail();
            return false;
        }
        if (s_.find('<', pos_ + 2) < end) { fail(); return false; }
        if (target != nullptr) {
            *target = s_.substr(pos_ + 2, targetEnd - (pos_ + 2));
        }
        if (hasData != nullptr) {
            *hasData = targetEnd < end;
        }
        if (data != nullptr) {
            *data = s_.substr(targetEnd, end - targetEnd);
        }
        pos_ = end + 2;
        return true;
    }

    // Skip <?xml ... ?> and comments before the root.
    void skipProlog() {
        bool seenXmlDeclaration = false;
        bool firstPrologToken = true;
        for (;;) {
            skipWs();
            if (s_.compare(pos_, 2, "<?") == 0) {
                std::string target;
                std::string data;
                bool hasData = false;
                if (!skipProcessingInstruction(&target, &hasData, &data)) return;
                if (equalsAsciiNoCase(target, "xml")) {
                    if (target != "xml") { fail(); return; }
                    if (seenXmlDeclaration || !firstPrologToken) { fail(); return; }
                    if (!hasData) { fail(); return; }
                    if (!xmlDeclarationStartsWithVersion(data)) { fail(); return; }
                    seenXmlDeclaration = true;
                }
                firstPrologToken = false;
            } else if (s_.compare(pos_, 4, "<!--") == 0) {
                if (!skipComment()) return;
                firstPrologToken = false;
            } else {
                return;
            }
        }
    }

    // Skip XML Misc after the root element. XML declarations are only valid
    // before the root, but comments and processing instructions may trail it.
    void skipEpilog() {
        for (;;) {
            skipWs();
            if (s_.compare(pos_, 4, "<!--") == 0) {
                if (!skipComment()) return;
            } else if (s_.compare(pos_, 2, "<?") == 0) {
                std::string target;
                if (!skipProcessingInstruction(&target)) return;
                if (equalsAsciiNoCase(target, "xml")) {
                    fail();
                    return;
                }
            } else {
                return;
            }
        }
    }

    static std::string unescape(const std::string& in, bool& ok) {
        std::string out;
        out.reserve(in.size());
        for (std::size_t i = 0; i < in.size();) {
            if (in[i] == '&') {
                if (in.compare(i, 4, "&lt;") == 0) { out += '<'; i += 4; continue; }
                if (in.compare(i, 4, "&gt;") == 0) { out += '>'; i += 4; continue; }
                if (in.compare(i, 5, "&amp;") == 0) { out += '&'; i += 5; continue; }
                if (in.compare(i, 6, "&quot;") == 0) { out += '"'; i += 6; continue; }
                if (in.compare(i, 6, "&apos;") == 0) { out += '\''; i += 6; continue; }
                if (appendNumericCharacterReference(in, i, out)) continue;
                ok = false;
                return {};
            }
            std::uint32_t cp = 0;
            std::size_t next = i;
            if (!decodeUtf8CodePoint(in, i, cp, next) ||
                !isAllowedXmlCodePoint(cp)) {
                ok = false;
                return {};
            }
            out.append(in, i, next - i);
            i = next;
        }
        return out;
    }

    std::string parseName() {
        std::size_t start = pos_;
        while (pos_ < s_.size()) {
            char c = s_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '>' ||
                c == '/' || c == '=') {
                break;
            }
            ++pos_;
        }
        return s_.substr(start, pos_ - start);
    }

    std::unique_ptr<XmlNode> parseElement() {
        if (pos_ >= s_.size() || s_[pos_] != '<') { fail(); return nullptr; }
        ++pos_;  // consume '<'
        auto node = std::make_unique<XmlNode>();
        node->tag = parseName();
        if (node->tag.empty()) { fail(); return nullptr; }

        // Attributes.
        bool parsedAttribute = false;
        for (;;) {
            const std::size_t wsStart = pos_;
            skipWs();
            const bool hadWhitespace = pos_ > wsStart;
            if (pos_ >= s_.size()) { fail(); return nullptr; }
            char c = s_[pos_];
            if (c == '/' || c == '>') break;
            if (parsedAttribute && !hadWhitespace) { fail(); return nullptr; }
            std::string key = parseName();
            if (key.empty()) { fail(); return nullptr; }
            skipWs();
            if (pos_ >= s_.size() || s_[pos_] != '=') { fail(); return nullptr; }
            ++pos_;  // '='
            skipWs();
            if (pos_ >= s_.size() || (s_[pos_] != '"' && s_[pos_] != '\'')) {
                fail();
                return nullptr;
            }
            char quote = s_[pos_++];
            std::size_t vstart = pos_;
            while (pos_ < s_.size() && s_[pos_] != quote) {
                if (s_[pos_] == '<') { fail(); return nullptr; }
                ++pos_;
            }
            if (pos_ >= s_.size()) { fail(); return nullptr; }
            bool unescaped = true;
            std::string value = unescape(s_.substr(vstart, pos_ - vstart), unescaped);
            if (!unescaped) { fail(); return nullptr; }
            ++pos_;  // closing quote
            for (const auto& attr : node->attrs) {
                if (attr.first == key) {
                    fail();
                    return nullptr;
                }
            }
            node->attrs.emplace_back(std::move(key), std::move(value));
            parsedAttribute = true;
        }

        // Self-closing?
        if (s_[pos_] == '/') {
            ++pos_;
            if (pos_ >= s_.size() || s_[pos_] != '>') { fail(); return nullptr; }
            ++pos_;
            return node;
        }
        ++pos_;  // consume '>'

        // Content: text and/or child elements until matching close tag.
        std::string text;
        for (;;) {
            if (pos_ >= s_.size()) { fail(); return nullptr; }
            if (s_[pos_] == '<') {
                if (s_.compare(pos_, 2, "</") == 0) {
                    pos_ += 2;
                    std::string close = parseName();
                    skipWs();
                    if (pos_ >= s_.size() || s_[pos_] != '>' || close != node->tag) {
                        fail();
                        return nullptr;
                    }
                    ++pos_;  // '>'
                    bool unescaped = true;
                    node->text = unescape(text, unescaped);
                    if (!unescaped) { fail(); return nullptr; }
                    return node;
                }
                if (s_.compare(pos_, 4, "<!--") == 0) {
                    if (!skipComment()) return nullptr;
                    continue;
                }
                if (s_.compare(pos_, 2, "<?") == 0) {
                    std::string target;
                    if (!skipProcessingInstruction(&target)) return nullptr;
                    if (equalsAsciiNoCase(target, "xml")) {
                        fail();
                        return nullptr;
                    }
                    continue;
                }
                auto child = parseElement();
                if (!child) return nullptr;
                node->children.push_back(std::move(child));
            } else {
                text += s_[pos_++];
            }
        }
    }
};

// ===========================================================================
// Numeric helpers — write doubles with full precision, parse leniently.
// ===========================================================================
std::string num(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

void requireFullyParsed(const std::string& s, std::size_t parsed) {
    if (parsed != s.size()) throw std::invalid_argument("XML numeric value has trailing characters");
}

void requireNoLeadingNumericWhitespace(const std::string& s) {
    if (s.empty() || s[0] == ' ' || s[0] == '\t' || s[0] == '\n' || s[0] == '\r') {
        throw std::invalid_argument("XML numeric value has leading whitespace");
    }
}

bool isDecimalFloatLiteral(const std::string& s) {
    std::size_t pos = 0;
    if (pos < s.size() && s[pos] == '-') ++pos;

    bool sawDigit = false;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
        sawDigit = true;
        ++pos;
    }

    if (pos < s.size() && s[pos] == '.') {
        ++pos;
        while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
            sawDigit = true;
            ++pos;
        }
    }
    if (!sawDigit) return false;

    if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
        ++pos;
        if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) ++pos;
        bool sawExponentDigit = false;
        while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
            sawExponentDigit = true;
            ++pos;
        }
        if (!sawExponentDigit) return false;
    }

    return pos == s.size();
}

bool isSignedIntegerLiteral(const std::string& s) {
    std::size_t pos = 0;
    if (pos < s.size() && s[pos] == '-') ++pos;
    if (pos >= s.size()) return false;
    while (pos < s.size()) {
        if (s[pos] < '0' || s[pos] > '9') return false;
        ++pos;
    }
    return true;
}

double toD(const std::string& s) {
    requireNoLeadingNumericWhitespace(s);
    if (!isDecimalFloatLiteral(s)) {
        throw std::invalid_argument("XML numeric value is not a decimal literal");
    }
    std::size_t parsed = 0;
    const double value = std::stod(s, &parsed);
    requireFullyParsed(s, parsed);
    if (!std::isfinite(value)) {
        throw std::out_of_range("XML numeric value is not finite");
    }
    return value;
}
std::uint64_t toU(const std::string& s) {
    for (char c : s) {
        if (c < '0' || c > '9') {
            throw std::invalid_argument("XML unsigned value must contain only digits");
        }
    }
    std::size_t parsed = 0;
    const std::uint64_t value = std::stoull(s, &parsed);
    requireFullyParsed(s, parsed);
    return value;
}
int toI(const std::string& s) {
    requireNoLeadingNumericWhitespace(s);
    if (!isSignedIntegerLiteral(s)) {
        throw std::invalid_argument("XML integer value is not a signed decimal literal");
    }
    std::size_t parsed = 0;
    const int value = std::stoi(s, &parsed);
    requireFullyParsed(s, parsed);
    return value;
}
bool toB(const std::string& s) {
    if (s == "1" || s == "true") return true;
    if (s == "0" || s == "false") return false;
    throw std::invalid_argument("XML boolean value is invalid");
}

template <typename EnumT>
EnumT toEnum(const std::string& s, int count) {
    const int value = toI(s);
    if (value < 0 || value >= count) {
        throw std::out_of_range("XML enum value out of range");
    }
    return static_cast<EnumT>(value);
}

const std::string& requiredAttr(const XmlNode& n, const std::string& key) {
    const std::string* p = n.attr(key);
    if (!p) throw std::invalid_argument("XML required attribute is missing");
    return *p;
}

// ===========================================================================
// Node builders for each value type. Enums are stored as their integer index
// so the schema is compact and stable.
// ===========================================================================
std::unique_ptr<XmlNode> elem(const std::string& tag) {
    auto n = std::make_unique<XmlNode>();
    n->tag = tag;
    return n;
}

void addAttr(XmlNode& n, const std::string& k, const std::string& v) {
    n.attrs.emplace_back(k, v);
}
void addAttr(XmlNode& n, const std::string& k, double v) { addAttr(n, k, num(v)); }
void addAttrU(XmlNode& n, const std::string& k, std::uint64_t v) {
    addAttr(n, k, std::to_string(v));
}
void addAttrI(XmlNode& n, const std::string& k, int v) {
    addAttr(n, k, std::to_string(v));
}
void addAttrB(XmlNode& n, const std::string& k, bool v) {
    addAttr(n, k, std::string(v ? "1" : "0"));
}

std::unique_ptr<XmlNode> vec3Node(const std::string& tag, const Vec3& v) {
    auto n = elem(tag);
    addAttr(*n, "x", v.x);
    addAttr(*n, "y", v.y);
    addAttr(*n, "z", v.z);
    return n;
}

Vec3 readVec3(const XmlNode& n) {
    requireOnlyAttrs(n, {"x", "y", "z"});
    requireNoChildren(n);
    return Vec3{toD(requiredAttr(n, "x")), toD(requiredAttr(n, "y")),
                toD(requiredAttr(n, "z"))};
}

std::unique_ptr<XmlNode> directionNode(const Direction& d) {
    auto n = elem("Direction");
    addAttrI(*n, "type", static_cast<int>(d.type));
    n->children.push_back(vec3Node("Ijk", d.ijk));
    auto refs = elem("RefPoints");
    for (PointId p : d.refPoints) {
        auto r = elem("Ref");
        addAttrU(*r, "id", p);
        refs->children.push_back(std::move(r));
    }
    if (!refs->children.empty()) n->children.push_back(std::move(refs));
    return n;
}

Direction readDirection(const XmlNode& n) {
    if (n.tag != "Direction") throw std::invalid_argument("XML Direction element has unexpected tag");
    requireOnlyAttrs(n, {"type"});
    requireOnlyChildren(n, {"Ijk", "RefPoints"});
    Direction d;
    d.type = toEnum<DirectionType>(requiredAttr(n, "type"), 6);
    const XmlNode* ijk = n.child("Ijk");
    if (!ijk) {
        throw std::invalid_argument("XML Direction element is missing required vector");
    }
    d.ijk = readVec3(*ijk);
    if (const XmlNode* refs = n.child("RefPoints")) {
        requireOnlyAttrs(*refs, {});
        requireNoText(*refs);
        for (const auto& r : refs->children) {
            if (r->tag != "Ref") throw std::invalid_argument("XML Direction Ref element has unexpected tag");
            requireOnlyAttrs(*r, {"id"});
            requireNoChildren(*r);
            d.refPoints.push_back(toU(requiredAttr(*r, "id")));
        }
    }
    return d;
}

std::unique_ptr<XmlNode> idListNode(const std::string& tag,
                                    const std::vector<std::uint64_t>& ids) {
    auto n = elem(tag);
    for (std::uint64_t id : ids) {
        auto r = elem("Ref");
        addAttrU(*r, "id", id);
        n->children.push_back(std::move(r));
    }
    return n;
}

std::unique_ptr<XmlNode> doubleListNode(const std::string& tag,
                                        const std::vector<double>& values) {
    auto n = elem(tag);
    for (double value : values) {
        auto v = elem("Value");
        addAttr(*v, "v", value);
        n->children.push_back(std::move(v));
    }
    return n;
}

template <typename IdT>
std::vector<IdT> readIdList(const XmlNode* n) {
    std::vector<IdT> out;
    if (!n) return out;
    requireOnlyAttrs(*n, {});
    requireNoText(*n);
    for (const auto& r : n->children) {
        if (r->tag != "Ref") throw std::invalid_argument("XML Ref element has unexpected tag");
        requireOnlyAttrs(*r, {"id"});
        requireNoChildren(*r);
        out.push_back(static_cast<IdT>(toU(requiredAttr(*r, "id"))));
    }
    return out;
}

std::vector<double> readDoubleList(const XmlNode* n) {
    std::vector<double> out;
    if (!n) return out;
    requireOnlyAttrs(*n, {});
    requireNoText(*n);
    for (const auto& v : n->children) {
        if (v->tag != "Value") throw std::invalid_argument("XML Value element has unexpected tag");
        requireOnlyAttrs(*v, {"v"});
        requireNoChildren(*v);
        out.push_back(toD(requiredAttr(*v, "v")));
    }
    return out;
}

// ===========================================================================
// Model -> XML.
// ===========================================================================
std::unique_ptr<XmlNode> pointNode(const Point& p) {
    auto n = elem("Point");
    addAttrU(*n, "id", p.id);
    addAttrI(*n, "kind", static_cast<int>(p.kind));
    addAttrI(*n, "holeType", static_cast<int>(p.holeType));
    addAttr(*n, "diameter", p.diameter);
    addAttrB(*n, "active", p.active);
    n->children.push_back(vec3Node("Position", p.position));
    n->children.push_back(vec3Node("Ijk", p.ijk));
    return n;
}

std::unique_ptr<XmlNode> featureNode(const Feature& f) {
    auto n = elem("Feature");
    addAttrU(*n, "id", f.id);
    addAttrI(*n, "kind", static_cast<int>(f.kind));
    n->children.push_back(idListNode("DefiningPoints", f.definingPoints));
    auto mesh = elem("Mesh");
    addAttrU(*mesh, "feature", f.mesh.feature);
    addAttrU(*mesh, "meshNodeNum", f.mesh.meshNodeNum);
    addAttrU(*mesh, "cadPtNum", f.mesh.cadPtNum);
    addAttrU(*mesh, "version", f.mesh.version);
    n->children.push_back(std::move(mesh));
    return n;
}

std::unique_ptr<XmlNode> toleranceNode(const ToleranceDef& t) {
    auto n = elem("Tolerance");
    addAttrU(*n, "id", t.id);
    addAttr(*n, "name", t.name);
    addAttrB(*n, "active", t.active);

    auto ir = elem("IR");
    addAttr(*ir, "rangeScale", t.ir.rangeScale);
    addAttrI(*ir, "geomRule", static_cast<int>(t.ir.geomRule));
    auto rands = elem("Rands");
    for (const auto& r : t.ir.rands) {
        auto rn = elem("Rand");
        addAttrI(*rn, "distribution", static_cast<int>(r.distribution));
        addAttr(*rn, "range", r.range);
        addAttr(*rn, "offset", r.offset);
        addAttr(*rn, "sigmaNum", r.sigmaNum);
        if (!r.userDefinedSamplePath.empty()) {
            addAttr(*rn, "userDefinedSamplePath", r.userDefinedSamplePath);
        }
        rands->children.push_back(std::move(rn));
    }
    ir->children.push_back(std::move(rands));
    auto tr = elem("Truncation");
    addAttr(*tr, "minTrunc", t.ir.truncation.minTrunc);
    addAttr(*tr, "maxTrunc", t.ir.truncation.maxTrunc);
    addAttrB(*tr, "active", t.ir.truncation.active);
    ir->children.push_back(std::move(tr));
    ir->children.push_back(directionNode(t.ir.direction));
    n->children.push_back(std::move(ir));

    n->children.push_back(idListNode("Features", t.features));
    return n;
}

std::unique_ptr<XmlNode> gdtNode(const GdtDef& g) {
    auto n = elem("Gdt");
    addAttrU(*n, "id", g.id);
    addAttr(*n, "name", g.name);
    addAttrB(*n, "active", g.active);
    addAttrI(*n, "type", static_cast<int>(g.type));
    addAttr(*n, "range", g.range);
    addAttrB(*n, "diametrical", g.diametrical);
    auto drf = elem("Drf");
    addAttrU(*drf, "primary", g.drf.primary);
    addAttrU(*drf, "secondary", g.drf.secondary);
    addAttrU(*drf, "tertiary", g.drf.tertiary);
    n->children.push_back(std::move(drf));
    n->children.push_back(idListNode("Features", g.features));
    return n;
}

std::unique_ptr<XmlNode> partNode(const Part& p) {
    auto n = elem("Part");
    addAttrU(*n, "id", p.id);
    addAttr(*n, "cadName", p.cadName);
    addAttr(*n, "dcsName", p.dcsName);
    auto pts = elem("Points");
    for (const auto& pt : p.points) pts->children.push_back(pointNode(pt));
    n->children.push_back(std::move(pts));
    auto feats = elem("Features");
    for (const auto& f : p.features) feats->children.push_back(featureNode(f));
    n->children.push_back(std::move(feats));
    auto tols = elem("Tolerances");
    for (const auto& t : p.tolerances) tols->children.push_back(toleranceNode(t));
    n->children.push_back(std::move(tols));
    auto gdts = elem("Gdts");
    for (const auto& g : p.gdts) gdts->children.push_back(gdtNode(g));
    n->children.push_back(std::move(gdts));
    return n;
}

std::unique_ptr<XmlNode> movePairNode(const MovePair& mp) {
    auto n = elem("Pair");
    n->children.push_back(vec3Node("ObjectPoint", mp.objectPoint));
    n->children.push_back(vec3Node("TargetPoint", mp.targetPoint));
    n->children.push_back(directionNode(mp.direction));
    return n;
}

std::unique_ptr<XmlNode> moveNode(const MoveDef& m) {
    auto n = elem("Move");
    addAttrU(*n, "id", m.id);
    addAttr(*n, "name", m.name);
    addAttrB(*n, "active", m.active);

    auto in = elem("Inputs");
    addAttrI(*in, "type", static_cast<int>(m.inputs.type));
    addAttr(*in, "searchAccuracy", m.inputs.searchAccuracy);
    addAttrI(*in, "maxIterations", m.inputs.maxIterations);
    addAttrB(*in, "isNominalBuild", m.inputs.isNominalBuild);
    addAttr(*in, "userDllRoutine", m.inputs.userDllRoutine);
    auto pairs = elem("Pairs");
    for (const auto& mp : m.inputs.pairs) pairs->children.push_back(movePairNode(mp));
    in->children.push_back(std::move(pairs));
    auto fl = elem("Float");
    addAttrB(*fl, "active", m.inputs.hole_pin_float.active);
    addAttrI(*fl, "sigmaNumber", m.inputs.hole_pin_float.sigmaNumber);
    addAttr(*fl, "rangeScale", m.inputs.hole_pin_float.rangeScale);
    addAttr(*fl, "angleRangeDeg", m.inputs.hole_pin_float.angleRangeDeg);
    addAttr(*fl, "angleOffsetDeg", m.inputs.hole_pin_float.angleOffsetDeg);
    in->children.push_back(std::move(fl));
    n->children.push_back(std::move(in));

    n->children.push_back(idListNode("MoveParts", m.moveParts));
    return n;
}

std::unique_ptr<XmlNode> measureNode(const MeasureRecord& r) {
    auto n = elem("Measure");
    addAttrU(*n, "id", r.id);
    addAttr(*n, "name", r.name);
    auto d = elem("Def");
    addAttrI(*d, "type", static_cast<int>(r.def.type));
    addAttrI(*d, "dirMode", static_cast<int>(r.def.dirMode));
    addAttr(*d, "scale", r.def.scale);
    addAttrB(*d, "active", r.def.active);
    addAttrB(*d, "asOutput", r.def.asOutput);
    addAttr(*d, "equation", r.def.equation);
    d->children.push_back(idListNode("InputPoints", r.def.inputPoints));
    d->children.push_back(idListNode("InputFeatures", r.def.inputFeatures));
    d->children.push_back(doubleListNode("Values", r.def.values));
    d->children.push_back(directionNode(r.def.direction));
    auto spec = elem("Spec");
    addAttr(*spec, "usl", r.def.spec.usl);
    addAttr(*spec, "lsl", r.def.spec.lsl);
    addAttrB(*spec, "uslActive", r.def.spec.uslActive);
    addAttrB(*spec, "lslActive", r.def.spec.lslActive);
    addAttrI(*spec, "mode", static_cast<int>(r.def.spec.mode));
    d->children.push_back(std::move(spec));
    n->children.push_back(std::move(d));
    return n;
}

std::unique_ptr<XmlNode> variantNode(const ModelVariant& v) {
    auto n = elem("Variant");
    addAttr(*n, "name", v.name);
    addAttrB(*n, "active", v.active);
    n->children.push_back(idListNode("Moves", v.moves));
    n->children.push_back(idListNode("Tolerances", v.tolerances));
    n->children.push_back(idListNode("Measures", v.measures));
    return n;
}

std::unique_ptr<XmlNode> buildModelTree(const Model& model) {
    auto root = elem("OpenDVAModel");
    addAttr(*root, "version", std::string("1"));
    addAttr(*root, "assemblyName", model.assemblyName);

    auto parts = elem("Parts");
    for (const auto& p : model.parts) parts->children.push_back(partNode(p));
    root->children.push_back(std::move(parts));

    auto moves = elem("Moves");
    for (const auto& m : model.moves) moves->children.push_back(moveNode(m));
    root->children.push_back(std::move(moves));

    auto measures = elem("Measures");
    for (const auto& r : model.measures) measures->children.push_back(measureNode(r));
    root->children.push_back(std::move(measures));

    auto variants = elem("Variants");
    for (const auto& v : model.variants) variants->children.push_back(variantNode(v));
    root->children.push_back(std::move(variants));

    return root;
}

// ===========================================================================
// XML -> Model.
// ===========================================================================
Point readPoint(const XmlNode& n) {
    if (n.tag != "Point") throw std::invalid_argument("XML Point element has unexpected tag");
    requireOnlyAttrs(n, {"id", "kind", "holeType", "diameter", "active"});
    requireOnlyChildren(n, {"Position", "Ijk"});
    Point p;
    p.id = toU(requiredAttr(n, "id"));
    p.kind = toEnum<PointKind>(requiredAttr(n, "kind"), 3);
    p.holeType = toEnum<HoleType>(requiredAttr(n, "holeType"), 3);
    p.diameter = toD(requiredAttr(n, "diameter"));
    p.active = toB(requiredAttr(n, "active"));
    const XmlNode* pos = n.child("Position");
    const XmlNode* ijk = n.child("Ijk");
    if (!pos || !ijk) {
        throw std::invalid_argument("XML Point element is missing required vector");
    }
    p.position = readVec3(*pos);
    p.ijk = readVec3(*ijk);
    return p;
}

Feature readFeature(const XmlNode& n) {
    if (n.tag != "Feature") throw std::invalid_argument("XML Feature element has unexpected tag");
    requireOnlyAttrs(n, {"id", "kind"});
    requireOnlyChildren(n, {"DefiningPoints", "Mesh"});
    Feature f;
    f.id = toU(requiredAttr(n, "id"));
    f.kind = toEnum<FeatureKind>(requiredAttr(n, "kind"), 8);
    const XmlNode* definingPoints = n.child("DefiningPoints");
    const XmlNode* mesh = n.child("Mesh");
    if (!definingPoints || !mesh) {
        throw std::invalid_argument("XML Feature element is missing required child");
    }
    f.definingPoints = readIdList<PointId>(definingPoints);
    requireOnlyAttrs(*mesh, {"feature", "meshNodeNum", "cadPtNum", "version"});
    requireNoChildren(*mesh);
    f.mesh.feature = toU(requiredAttr(*mesh, "feature"));
    f.mesh.meshNodeNum = static_cast<std::size_t>(toU(requiredAttr(*mesh, "meshNodeNum")));
    f.mesh.cadPtNum = static_cast<std::size_t>(toU(requiredAttr(*mesh, "cadPtNum")));
    f.mesh.version = toU(requiredAttr(*mesh, "version"));
    return f;
}

ToleranceDef readTolerance(const XmlNode& n) {
    if (n.tag != "Tolerance") throw std::invalid_argument("XML Tolerance element has unexpected tag");
    requireOnlyAttrs(n, {"id", "name", "active"});
    requireOnlyChildren(n, {"IR", "Features"});
    ToleranceDef t;
    t.id = toU(requiredAttr(n, "id"));
    t.name = requiredAttr(n, "name");
    t.active = toB(requiredAttr(n, "active"));
    const XmlNode* ir = n.child("IR");
    const XmlNode* features = n.child("Features");
    if (!ir || !features) {
        throw std::invalid_argument("XML Tolerance element is missing required child");
    }
    requireOnlyAttrs(*ir, {"rangeScale", "geomRule"});
    requireOnlyChildren(*ir, {"Rands", "Truncation", "Direction"});
    t.ir.rangeScale = toD(requiredAttr(*ir, "rangeScale"));
    t.ir.geomRule = toEnum<GeomRule>(requiredAttr(*ir, "geomRule"), 5);
    const XmlNode* rands = ir->child("Rands");
    const XmlNode* tr = ir->child("Truncation");
    const XmlNode* d = ir->child("Direction");
    if (!rands || !tr || !d) {
        throw std::invalid_argument("XML Tolerance IR element is missing required child");
    }
    requireOnlyAttrs(*rands, {});
    requireNoText(*rands);
    for (const auto& rn : rands->children) {
        if (rn->tag != "Rand") throw std::invalid_argument("XML Rand element has unexpected tag");
        requireOnlyAttrs(*rn, {"distribution", "range", "offset", "sigmaNum",
                               "userDefinedSamplePath"});
        requireNoChildren(*rn);
        RandSpec r;
        r.distribution =
            toEnum<DistributionType>(requiredAttr(*rn, "distribution"), 20);
        r.range = toD(requiredAttr(*rn, "range"));
        r.offset = toD(requiredAttr(*rn, "offset"));
        r.sigmaNum = toD(requiredAttr(*rn, "sigmaNum"));
        if (const std::string* path = rn->attr("userDefinedSamplePath")) {
            r.userDefinedSamplePath = *path;
        }
        t.ir.rands.push_back(r);
    }
    requireOnlyAttrs(*tr, {"minTrunc", "maxTrunc", "active"});
    requireNoChildren(*tr);
    t.ir.truncation.minTrunc = toD(requiredAttr(*tr, "minTrunc"));
    t.ir.truncation.maxTrunc = toD(requiredAttr(*tr, "maxTrunc"));
    t.ir.truncation.active = toB(requiredAttr(*tr, "active"));
    t.ir.direction = readDirection(*d);
    t.features = readIdList<FeatureId>(features);
    return t;
}

GdtDef readGdt(const XmlNode& n) {
    if (n.tag != "Gdt") throw std::invalid_argument("XML Gdt element has unexpected tag");
    requireOnlyAttrs(n, {"id", "name", "active", "type", "range", "diametrical"});
    requireOnlyChildren(n, {"Drf", "Features"});
    GdtDef g;
    g.id = toU(requiredAttr(n, "id"));
    g.name = requiredAttr(n, "name");
    g.active = toB(requiredAttr(n, "active"));
    g.type = toEnum<GdtType>(requiredAttr(n, "type"), 19);
    g.range = toD(requiredAttr(n, "range"));
    g.diametrical = toB(requiredAttr(n, "diametrical"));
    const XmlNode* drf = n.child("Drf");
    const XmlNode* features = n.child("Features");
    if (!drf || !features) {
        throw std::invalid_argument("XML Gdt element is missing required child");
    }
    requireOnlyAttrs(*drf, {"primary", "secondary", "tertiary"});
    requireNoChildren(*drf);
    g.drf.primary = toU(requiredAttr(*drf, "primary"));
    g.drf.secondary = toU(requiredAttr(*drf, "secondary"));
    g.drf.tertiary = toU(requiredAttr(*drf, "tertiary"));
    g.features = readIdList<FeatureId>(features);
    return g;
}

Part readPart(const XmlNode& n) {
    if (n.tag != "Part") throw std::invalid_argument("XML Part element has unexpected tag");
    requireOnlyAttrs(n, {"id", "cadName", "dcsName"});
    requireOnlyChildren(n, {"Points", "Features", "Tolerances", "Gdts"});
    Part p;
    p.id = toU(requiredAttr(n, "id"));
    p.cadName = requiredAttr(n, "cadName");
    p.dcsName = requiredAttr(n, "dcsName");
    const XmlNode* pts = n.child("Points");
    const XmlNode* feats = n.child("Features");
    const XmlNode* tols = n.child("Tolerances");
    const XmlNode* gdts = n.child("Gdts");
    if (!pts || !feats || !tols || !gdts) {
        throw std::invalid_argument("XML Part element is missing required child");
    }
    requireOnlyAttrs(*pts, {});
    requireNoText(*pts);
    for (const auto& c : pts->children) p.points.push_back(readPoint(*c));
    requireOnlyAttrs(*feats, {});
    requireNoText(*feats);
    for (const auto& c : feats->children) p.features.push_back(readFeature(*c));
    requireOnlyAttrs(*tols, {});
    requireNoText(*tols);
    for (const auto& c : tols->children) p.tolerances.push_back(readTolerance(*c));
    requireOnlyAttrs(*gdts, {});
    requireNoText(*gdts);
    for (const auto& c : gdts->children) p.gdts.push_back(readGdt(*c));
    return p;
}

MovePair readMovePair(const XmlNode& n) {
    if (n.tag != "Pair") throw std::invalid_argument("XML Pair element has unexpected tag");
    requireOnlyAttrs(n, {});
    requireOnlyChildren(n, {"ObjectPoint", "TargetPoint", "Direction"});
    MovePair mp;
    const XmlNode* objectPoint = n.child("ObjectPoint");
    const XmlNode* targetPoint = n.child("TargetPoint");
    const XmlNode* direction = n.child("Direction");
    if (!objectPoint || !targetPoint || !direction) {
        throw std::invalid_argument("XML Pair element is missing required child");
    }
    mp.objectPoint = readVec3(*objectPoint);
    mp.targetPoint = readVec3(*targetPoint);
    mp.direction = readDirection(*direction);
    return mp;
}

MoveDef readMove(const XmlNode& n) {
    if (n.tag != "Move") throw std::invalid_argument("XML Move element has unexpected tag");
    requireOnlyAttrs(n, {"id", "name", "active"});
    requireOnlyChildren(n, {"Inputs", "MoveParts"});
    MoveDef m;
    m.id = toU(requiredAttr(n, "id"));
    m.name = requiredAttr(n, "name");
    m.active = toB(requiredAttr(n, "active"));
    const XmlNode* in = n.child("Inputs");
    const XmlNode* moveParts = n.child("MoveParts");
    if (!in || !moveParts) {
        throw std::invalid_argument("XML Move element is missing required child");
    }
    requireOnlyAttrs(*in, {"type", "searchAccuracy", "maxIterations", "isNominalBuild",
                           "userDllRoutine"});
    requireOnlyChildren(*in, {"Pairs", "Float"});
    m.inputs.type = toEnum<MoveType>(requiredAttr(*in, "type"), 20);
    m.inputs.searchAccuracy = toD(requiredAttr(*in, "searchAccuracy"));
    m.inputs.maxIterations = toI(requiredAttr(*in, "maxIterations"));
    m.inputs.isNominalBuild = toB(requiredAttr(*in, "isNominalBuild"));
    if (const std::string* routine = in->attr("userDllRoutine")) {
        m.inputs.userDllRoutine = *routine;
    }
    const XmlNode* pairs = in->child("Pairs");
    const XmlNode* fl = in->child("Float");
    if (!pairs || !fl) {
        throw std::invalid_argument("XML Move Inputs element is missing required child");
    }
    requireOnlyAttrs(*pairs, {});
    requireNoText(*pairs);
    for (const auto& c : pairs->children) {
        m.inputs.pairs.push_back(readMovePair(*c));
    }
    requireOnlyAttrs(*fl, {"active", "sigmaNumber", "rangeScale", "angleRangeDeg", "angleOffsetDeg"});
    requireNoChildren(*fl);
    m.inputs.hole_pin_float.active = toB(requiredAttr(*fl, "active"));
    m.inputs.hole_pin_float.sigmaNumber = toI(requiredAttr(*fl, "sigmaNumber"));
    m.inputs.hole_pin_float.rangeScale = toD(requiredAttr(*fl, "rangeScale"));
    m.inputs.hole_pin_float.angleRangeDeg = toD(requiredAttr(*fl, "angleRangeDeg"));
    m.inputs.hole_pin_float.angleOffsetDeg = toD(requiredAttr(*fl, "angleOffsetDeg"));
    m.moveParts = readIdList<PartId>(moveParts);
    return m;
}

MeasureRecord readMeasure(const XmlNode& n) {
    if (n.tag != "Measure") throw std::invalid_argument("XML Measure element has unexpected tag");
    requireOnlyAttrs(n, {"id", "name"});
    requireOnlyChildren(n, {"Def"});
    MeasureRecord r;
    r.id = toU(requiredAttr(n, "id"));
    r.name = requiredAttr(n, "name");
    const XmlNode* d = n.child("Def");
    if (!d) {
        throw std::invalid_argument("XML Measure element is missing required child");
    }
    requireOnlyAttrs(*d, {"type", "dirMode", "scale", "active", "asOutput", "equation"});
    requireOnlyChildren(*d, {"InputPoints", "InputFeatures", "Values", "Direction", "Spec"});
    r.def.type = toEnum<MeasureType>(requiredAttr(*d, "type"), 26);
    r.def.dirMode = toEnum<DirectionMode>(requiredAttr(*d, "dirMode"), 3);
    r.def.scale = toD(requiredAttr(*d, "scale"));
    r.def.active = toB(requiredAttr(*d, "active"));
    r.def.asOutput = toB(requiredAttr(*d, "asOutput"));
    r.def.equation = requiredAttr(*d, "equation");
    const XmlNode* inputPoints = d->child("InputPoints");
    const XmlNode* inputFeatures = d->child("InputFeatures");
    const XmlNode* values = d->child("Values");
    const XmlNode* dir = d->child("Direction");
    const XmlNode* spec = d->child("Spec");
    if (!inputPoints || !inputFeatures || !values || !dir || !spec) {
        throw std::invalid_argument("XML Measure Def element is missing required child");
    }
    r.def.inputPoints = readIdList<PointId>(inputPoints);
    r.def.inputFeatures = readIdList<FeatureId>(inputFeatures);
    r.def.values = readDoubleList(values);
    r.def.direction = readDirection(*dir);
    requireOnlyAttrs(*spec, {"usl", "lsl", "uslActive", "lslActive", "mode"});
    requireNoChildren(*spec);
    r.def.spec.usl = toD(requiredAttr(*spec, "usl"));
    r.def.spec.lsl = toD(requiredAttr(*spec, "lsl"));
    r.def.spec.uslActive = toB(requiredAttr(*spec, "uslActive"));
    r.def.spec.lslActive = toB(requiredAttr(*spec, "lslActive"));
    r.def.spec.mode = toEnum<SpecMode>(requiredAttr(*spec, "mode"), 2);
    return r;
}

ModelVariant readVariant(const XmlNode& n) {
    if (n.tag != "Variant") throw std::invalid_argument("XML Variant element has unexpected tag");
    requireOnlyAttrs(n, {"name", "active"});
    requireOnlyChildren(n, {"Moves", "Tolerances", "Measures"});
    ModelVariant v;
    v.name = requiredAttr(n, "name");
    v.active = toB(requiredAttr(n, "active"));
    const XmlNode* moves = n.child("Moves");
    const XmlNode* tolerances = n.child("Tolerances");
    const XmlNode* measures = n.child("Measures");
    if (!moves || !tolerances || !measures) {
        throw std::invalid_argument("XML Variant element is missing required child");
    }
    v.moves = readIdList<MoveId>(moves);
    v.tolerances = readIdList<ToleranceId>(tolerances);
    v.measures = readIdList<MeasureId>(measures);
    return v;
}

bool readModelTree(const XmlNode& root, Model& model) {
    if (root.tag != "OpenDVAModel") return false;
    requireOnlyAttrs(root, {"version", "assemblyName"});
    requireOnlyChildren(root, {"Parts", "Moves", "Measures", "Variants"});
    model = Model{};
    if (requiredAttr(root, "version") != "1") {
        throw std::invalid_argument("XML OpenDVAModel version is unsupported");
    }
    model.assemblyName = requiredAttr(root, "assemblyName");
    const XmlNode* parts = root.child("Parts");
    const XmlNode* moves = root.child("Moves");
    const XmlNode* measures = root.child("Measures");
    const XmlNode* variants = root.child("Variants");
    if (!parts || !moves || !measures || !variants) {
        throw std::invalid_argument("XML OpenDVAModel element is missing required child");
    }
    requireOnlyAttrs(*parts, {});
    requireNoText(*parts);
    for (const auto& c : parts->children) model.parts.push_back(readPart(*c));
    requireOnlyAttrs(*moves, {});
    requireNoText(*moves);
    for (const auto& c : moves->children) model.moves.push_back(readMove(*c));
    requireOnlyAttrs(*measures, {});
    requireNoText(*measures);
    for (const auto& c : measures->children) model.measures.push_back(readMeasure(*c));
    requireOnlyAttrs(*variants, {});
    requireNoText(*variants);
    for (const auto& c : variants->children) model.variants.push_back(readVariant(*c));
    return true;
}

}  // namespace

// ===========================================================================
// Public API.
// ===========================================================================
std::string saveModelToString(const Model& model) {
    auto root = buildModelTree(model);
    std::string out = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    writeNode(out, *root, 0);
    return out;
}

bool loadModelFromString(Model& model, const std::string& xml) {
    try {
        XmlParser parser(xml);
        auto root = parser.parse();
        if (!parser.ok() || !root || !parser.finished()) return false;
        Model loaded;
        if (!readModelTree(*root, loaded)) return false;
        model = loaded;
        return true;
    } catch (...) {
        return false;
    }
}

bool saveModel(const Model& model, const std::string& path) {
    std::ofstream os(path, std::ios::binary);
    if (!os) return false;
    os << saveModelToString(model);
    return static_cast<bool>(os);
}

bool loadModel(Model& model, const std::string& path) {
    std::ifstream is(path, std::ios::binary);
    if (!is) return false;
    std::ostringstream ss;
    ss << is.rdbuf();
    return loadModelFromString(model, ss.str());
}

}  // namespace opendva
