#include <fstream>
#include <clocale>

#include "Tokenizer.hpp"

int main(int argc, char *argv[])
{
    std::setlocale(LC_ALL, "en_US.UTF8");
    std::ifstream in(argv[1]);
    Tokenizer t(in);
    t.tokenize();
    t.dumpTokens();
}
