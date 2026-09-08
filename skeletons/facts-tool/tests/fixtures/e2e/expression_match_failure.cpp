namespace expression_match_failure {
struct Record {
  int field = 0;
  void write() { field = 1; }
};
// Deliberately invalid after a complete definition has been parsed.
int broken = ;
} // namespace expression_match_failure
