#pragma once
#include <string>

class Expression {
public:
    // Stores the text. Call evaluate() as many times as you like.
    explicit Expression(const std::string& text);

    // Computes the value at x. Sets ok=false (and error message) if the text is invalid.
    double evaluate(double x);

    bool ok() const { return error_.empty(); }
    const std::string& error() const { return error_; }

private:
    // Grammar (lowest to highest priority):
    //   expr   = term   { ('+' | '-') term }
    //   term   = factor { ('*' | '/') factor }
    //   factor = unary  [ '^' factor ]        (right-associative)
    //   unary  = '-' unary | primary
    //   primary= number | 'x' | 'pi' | 'e' | name '(' expr ')' | '(' expr ')'
    double parseExpr();
    double parseTerm();
    double parseFactor();
    double parseUnary();
    double parsePrimary();

    void skipSpaces();
    bool match(char c);

    std::string text_;
    size_t pos_ = 0;
    double x_ = 0;
    std::string error_;
};
