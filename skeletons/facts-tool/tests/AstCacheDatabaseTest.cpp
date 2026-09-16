#include "astcache/Fixture.h"

int main() {
  ast_cache_database_test::lifecycle();
  ast_cache_database_test::migration();
  ast_cache_database_test::failures();
}
