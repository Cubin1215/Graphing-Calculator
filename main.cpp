#include <iostream>
#include <string>
#include "expression.h"

int main() {
    std::cout << "Graphing calculator - step 1: expression tester\n";
    std::cout << "Functions: sin cos tan sqrt abs ln log exp. Constants: pi e. Type 'quit' to exit.\n";

    while (true) {
        std::cout << "\nf(x) = ";
        std::string text;
        if (!std::getline(std::cin, text) || text == "quit") break;

        Expression f(text);
        std::cout << "x = ";
        double x;
        if (!(std::cin >> x)) break;
        std::cin.ignore(1000, '\n');

        double y = f.evaluate(x);
        if (f.ok()) std::cout << "f(" << x << ") = " << y << "\n";
        else std::cout << "Error: " << f.error() << "\n";
    }
    return 0;
}
