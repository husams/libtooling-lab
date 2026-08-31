#ifndef FACTS_COMMANDS_CATALOG_SYMBOLFORMAT_H
#define FACTS_COMMANDS_CATALOG_SYMBOLFORMAT_H

#include "commands/catalog/SymbolData.h"

#include <string>
#include <vector>

namespace facts::storage {
class Row;
}

namespace facts::commands {

std::string displaySymbols(const std::vector<SymbolFact> &values);
std::string displaySymbol(const SymbolFact &value);
std::string symbolDeclaration(const SymbolFact &value);
std::vector<std::string> trueFlags(const storage::Row &row);

} // namespace facts::commands

#endif // FACTS_COMMANDS_CATALOG_SYMBOLFORMAT_H
