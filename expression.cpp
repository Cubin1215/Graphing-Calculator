#include "expression.h"
#include <cctype>
#include <cmath>
#include <cstdlib>

Expression::Expression(const std::string& text) : text_(text) {}

double Expression::evaluate(double x) {
    x_ = x;
    pos_ = 0;
    error_.clear();
    double value = parseExpr();
    skipSpaces();
    if (error_.empty() && pos_ < text_.size())
        error_ = "Unexpected character '" + std::string(1, text_[pos_]) + "'";
    return error_.empty() ? value : 0;
}

void Expression::skipSpaces() {
    while (pos_ < text_.size() && std::isspace((unsigned char)text_[pos_])) pos_++;
}

bool Expression::match(char c) {
    skipSpaces();
    if (pos_ < text_.size() && text_[pos_] == c) {
        pos_++;
        return true;
    }
    return false;
}

double Expression::parseExpr() {
    double value = parseTerm();
    while (error_.empty()) {
        if (match('+')) value += parseTerm();
        else if (match('-')) value -= parseTerm();
        else break;
    }
    return value;
}

double Expression::parseTerm() {
    double value = parseFactor();
    while (error_.empty()) {
        if (match('*')) value *= parseFactor();
        else if (match('/')) value /= parseFactor();  // x/0 gives inf, which the grapher can skip
        else break;
    }
    return value;
}

double Expression::parseFactor() {
    double base = parseUnary();
    if (error_.empty() && match('^')) {
        double exponent = parseFactor();  // recursion makes ^ right-associative
        return std::pow(base, exponent);
    }
    return base;
}

double Expression::parseUnary() {
    if (match('-')) return -parseUnary();
    if (match('+')) return parseUnary();
    return parsePrimary();
}

double Expression::parsePrimary() {
    skipSpaces();
    if (pos_ >= text_.size()) {
        error_ = "Expression ended too soon";
        return 0;
    }

    // Parentheses
    if (match('(')) {
        double value = parseExpr();
        if (error_.empty() && !match(')')) error_ = "Missing ')'";
        return value;
    }

    // Number
    char c = text_[pos_];
    if (std::isdigit((unsigned char)c) || c == '.') {
        const char* start = text_.c_str() + pos_;
        char* end = nullptr;
        double value = std::strtod(start, &end);
        pos_ += (end - start);
        return value;
    }

    // Name: x, pi, e, or a function like sin(...)
    if (std::isalpha((unsigned char)c)) {
        std::string name;
        while (pos_ < text_.size() && std::isalpha((unsigned char)text_[pos_]))
            name += (char)std::tolower((unsigned char)text_[pos_++]);

        if (name == "x") return x_;
        if (name == "pi") return 3.14159265358979323846;
        if (name == "e") return 2.71828182845904523536;

        if (!match('(')) {
            error_ = "Unknown name '" + name + "'";
            return 0;
        }
        double arg = parseExpr();
        if (error_.empty() && !match(')')) error_ = "Missing ')'";
        if (!error_.empty()) return 0;

        if (name == "sin") return std::sin(arg);
        if (name == "cos") return std::cos(arg);
        if (name == "tan") return std::tan(arg);
        if (name == "sqrt") return std::sqrt(arg);
        if (name == "abs") return std::fabs(arg);
        if (name == "ln") return std::log(arg);
        if (name == "log") return std::log10(arg);
        if (name == "exp") return std::exp(arg);
        error_ = "Unknown function '" + name + "'";
        return 0;
    }

    error_ = "Unexpected character '" + std::string(1, c) + "'";
    return 0;
}
