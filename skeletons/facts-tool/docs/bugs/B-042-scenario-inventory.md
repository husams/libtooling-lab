# B-042 scenario inventory (base b763c55 -> head)

Collected pytest-bdd scenarios (`pytest tests/e2e --collect-only`): base 766, head 888.
Removal rule: a scenario was removed only when its sole subject was intentionally deleted
behaviour (renderer formatting, the JSON document, the Mermaid artifact lifecycle, `--output`
safety, coverage/freshness prose in output). Every graph, scope, budget, recovery, entry and
matched-index check was migrated to `callgraph_run*` reads or the outcome matrix.

## Removed scenarios (sole subject deleted)

| feature | scenario | reason |
|---|---|---|
| native_graph_rendering | Each format produces one coherent final file (text/json/mermaid) | renderer/file formatting of `--format`/`--output` |
| native_graph_rendering | Initial graph is published before recovery | Mermaid artifact initial/final publication lifecycle |
| native_graph_rendering | Output cannot overwrite an input (facts/project/source/alias) | `--output` safety |
| call_graph_analysis | reports simulated complete catalog metadata through a validated project and facts pair | coverage/freshness prose in JSON output |
| call_graph_analysis | reports missing catalog coverage metadata without blindly re-extracting known facts | coverage/freshness prose in JSON output |
| call_graph_analysis | reports simulated stale catalog evidence independently from stored graph traversal | coverage/freshness prose in JSON output |

## Migrated scenarios (renamed or moved to call_graph_runs.feature)

| old scenario | new scenario | migration |
|---|---|---|
| Mermaid output preserves cross-component graph evidence | Recovery persists the cross-component graph and provenance / A read-only re-run reuses the graph without extraction | callgraph_run, callgraph_run_edge, callgraph_run_root, callgraph_run_recovery reads |
| Recovery failure retains a labelled partial diagram | Recovery failure retains the edges reached before the failure | status recovery-failed, failed callgraph_run_recovery row with diagnostic, edges reached |
| Interrupting recovery retains the usable graph (mermaid/json) | SIGINT during recovery retains the last usable generation + outcome matrix cancel_during_recovery | status cancelled, frontier reason cancelled, edges of the last usable generation |
| JSON operational errors retain a single diagnostic channel | outcome matrix database_empty_all / database_missing_facts | one stderr line, empty stdout, exit 1, no run |
| Invalid requests preserve an existing artifact (root/target/configuration/component) | Invalid requests write no run and start no recovery | run_count unchanged, single stderr line, no recovery-start |
| Mermaid keeps explicit node budgets | A one-node budget truncates with a non-empty frontier | truncation_reason max_nodes + callgraph_run_frontier |
| Optional query modes reach the renderer (callers/path) | Optional query modes persist their mode | callgraph_run.mode / callgraph_run_target |
| Unchanged recovery traverses one graph generation | Repeated recovery of an already recovered graph traverses once | -v 1 `graph traversal` count == 1, no attempted rows |
| Configured project and facts defaults support the native invocation | Configured pair defaults produce the same recovered run | --config defaults, run reads |
| Explicit budgets apply to optional query modes (callers/path) | Budgeted callers and path queries truncate | status truncated |
| resolves inherited MessageX/MessageY DispatchCalls exactly in text | ... exactly in the persisted run | callgraph_run_edge + relation_site receiver/certainty |
| keeps an unproven concrete receiver conservative as possible in text | ... in the persisted run | callgraph_run_edge + relation_site |
| selects roots by qualified name and USR with optional depth in text | ... in the persisted run | identical roots/edges for name and USR; max_depth truncation |
| reports invalid context incomplete TUs and invalid explicit depth in text | reports invalid context incomplete TUs and invalid explicit depth | exit codes, single stderr line, no run |
| records complete call graph validation evidence from text probes | records complete call graph validation evidence from the persisted run | callgraph_run row + edges |
| stops default traversal at an external symbol ... in text | stops default traversal at an external symbol and reports external-boundary without truncation | edge to the external target recorded, no frontier |
| applies an explicit positive depth cap ... in text | applies an explicit positive depth cap before an external boundary and reports depth truncation distinctly | status truncated / max_depth frontier |
| path results distinguish complete unknown and truncated evidence | path runs distinguish found unreachable and truncated evidence | target as edge destination / explored edges / truncated |

## Added scenarios

- call_graph_runs.feature: the outcome matrix (17 outcomes x verbosity off/on, including the post-traversal operational failure and help at both verbosities), the deterministic run examples (two successive runs, failed recovery, operational failure after traversal, SIGINT during recovery, SIGINT before traversal, read-only facts store, forced child-row rollback), plus the migrated S-025 checks above.
- call_graph_entries.feature: `Entry command help and output match the recorded pre-change snapshot` (byte-identical `analyse call-graph-entry` regression against tests/fixtures/e2e/entries/call_graph_entry_snapshot.json recorded from the base binary).

## Dropped assertions inside migrated scenarios (fields no longer persisted)

- S-021 recovery: per-entry driver/working_directory/arguments/related_usrs (verified through the project store instead), `errors`, `coverage.missing_definitions`, `definition_availability` (replaced by `definition`/`symbol.is_external` reads).
- S-022 / S-027: `edge_view`, `extraction_coverage`, `definition_availability`, `semantic_kind` (derived in the test from symbol rows when needed); a virtual destructor call now surfaces as its two raw relation kinds (Calls + DispatchCalls) at one site.
- B-040: coverage/freshness/action labels; only traversal completion, recorded edges and truncation are asserted.

## Collected test ids removed (base) / added (head)

Removed:

- test_applies_an_explicit_positive_depth_cap_before_an_external_boundary_and_reports_depth_truncation_distinctly_in_text
- test_configured_project_and_facts_defaults_support_the_native_invocation
- test_each_format_produces_one_coherent_final_file[json]
- test_each_format_produces_one_coherent_final_file[mermaid]
- test_each_format_produces_one_coherent_final_file[text]
- test_explicit_budgets_apply_to_optional_query_modes[callers]
- test_explicit_budgets_apply_to_optional_query_modes[path]
- test_initial_graph_is_published_before_recovery
- test_interrupting_recovery_retains_the_usable_graph[json]
- test_interrupting_recovery_retains_the_usable_graph[mermaid]
- test_invalid_requests_preserve_an_existing_artifact[component]
- test_invalid_requests_preserve_an_existing_artifact[configuration]
- test_invalid_requests_preserve_an_existing_artifact[root]
- test_invalid_requests_preserve_an_existing_artifact[target]
- test_json_operational_errors_retain_a_single_diagnostic_channel
- test_keeps_an_unproven_concrete_receiver_conservative_as_possible_in_text
- test_mermaid_keeps_explicit_node_budgets
- test_mermaid_output_preserves_crosscomponent_graph_evidence
- test_optional_query_modes_reach_the_renderer[callers]
- test_optional_query_modes_reach_the_renderer[path]
- test_output_cannot_overwrite_an_input[alias]
- test_output_cannot_overwrite_an_input[facts]
- test_output_cannot_overwrite_an_input[project]
- test_output_cannot_overwrite_an_input[source]
- test_path_results_distinguish_complete_unknown_and_truncated_evidence
- test_records_complete_call_graph_validation_evidence_from_text_probes
- test_recovery_failure_retains_a_labelled_partial_diagram
- test_reports_invalid_context_incomplete_tus_and_invalid_explicit_depth_in_text
- test_reports_missing_catalog_coverage_metadata_without_blindly_reextracting_known_facts
- test_reports_simulated_complete_catalog_metadata_through_a_validated_project_and_facts_pair
- test_reports_simulated_stale_catalog_evidence_independently_from_stored_graph_traversal
- test_resolves_inherited_messagex_dispatchcalls_exactly_in_text
- test_resolves_inherited_messagey_dispatchcalls_exactly_in_text
- test_selects_roots_by_qualified_name_and_usr_with_optional_depth_in_text
- test_stops_default_traversal_at_an_external_symbol_and_reports_externalboundary_without_truncation_in_text
- test_unchanged_recovery_traverses_one_graph_generation

Added:

- test_a_forced_childrow_failure_rolls_the_whole_run_back[0]
- test_a_forced_childrow_failure_rolls_the_whole_run_back[1]
- test_a_onenode_budget_truncates_with_a_nonempty_frontier
- test_a_readonly_facts_store_rejects_the_final_commit
- test_a_readonly_rerun_reuses_the_graph_without_extraction
- test_an_operational_failure_after_traversal_persists_a_failed_run[0]
- test_an_operational_failure_after_traversal_persists_a_failed_run[1]
- test_applies_an_explicit_positive_depth_cap_before_an_external_boundary_and_reports_depth_truncation_distinctly
- test_budgeted_callers_and_path_queries_truncate[callers]
- test_budgeted_callers_and_path_queries_truncate[path]
- test_configured_pair_defaults_produce_the_same_recovered_run
- test_entry_command_help_and_output_match_the_recorded_prechange_snapshot
- test_invalid_requests_write_no_run_and_start_no_recovery[component-2]
- test_invalid_requests_write_no_run_and_start_no_recovery[configuration-1]
- test_invalid_requests_write_no_run_and_start_no_recovery[root-2]
- test_invalid_requests_write_no_run_and_start_no_recovery[target-2]
- test_keeps_an_unproven_concrete_receiver_conservative_as_possible_in_the_persisted_run
- test_optional_query_modes_persist_their_mode[callers]
- test_optional_query_modes_persist_their_mode[path]
- test_path_runs_distinguish_found_unreachable_and_truncated_evidence
- test_records_complete_call_graph_validation_evidence_from_the_persisted_run
- test_recovery_failure_retains_the_edges_reached_before_the_failure
- test_recovery_persists_the_crosscomponent_graph_and_provenance
- test_repeated_recovery_of_an_already_recovered_graph_traverses_once
- test_reports_invalid_context_incomplete_tus_and_invalid_explicit_depth
- test_resolves_inherited_messagex_dispatchcalls_exactly_in_the_persisted_run
- test_resolves_inherited_messagey_dispatchcalls_exactly_in_the_persisted_run
- test_selects_roots_by_qualified_name_and_usr_with_optional_depth_in_the_persisted_run
- test_sigint_before_traversal_cancels_before_the_graph_is_touched
- test_sigint_during_recovery_retains_the_last_usable_generation
- test_stops_default_traversal_at_an_external_symbol_and_reports_externalboundary_without_truncation
- test_the_outcome_matrix_holds_without_and_with_verbosity[cancel_before_traversal-0-130-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[cancel_before_traversal-1-130-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[cancel_during_recovery-0-130-present]
- test_the_outcome_matrix_holds_without_and_with_verbosity[cancel_during_recovery-1-130-present]
- test_the_outcome_matrix_holds_without_and_with_verbosity[commit_failure-0-1-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[commit_failure-1-1-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[complete-0-0-present]
- test_the_outcome_matrix_holds_without_and_with_verbosity[complete-1-0-present]
- test_the_outcome_matrix_holds_without_and_with_verbosity[configuration_no_project-0-3-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[configuration_no_project-1-3-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[database_empty_all-0-1-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[database_empty_all-1-1-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[database_missing_facts-0-1-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[database_missing_facts-1-1-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[failed_after_traversal-0-1-present]
- test_the_outcome_matrix_holds_without_and_with_verbosity[failed_after_traversal-1-1-present]
- test_the_outcome_matrix_holds_without_and_with_verbosity[help-0-0-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[help-1-0-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[recovery_failure-0-1-present]
- test_the_outcome_matrix_holds_without_and_with_verbosity[recovery_failure-1-1-present]
- test_the_outcome_matrix_holds_without_and_with_verbosity[truncated-0-0-present]
- test_the_outcome_matrix_holds_without_and_with_verbosity[truncated-1-0-present]
- test_the_outcome_matrix_holds_without_and_with_verbosity[usage_format-0-2-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[usage_format-1-2-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[usage_invalid_budget-0-2-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[usage_invalid_budget-1-2-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[usage_missing_root-0-2-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[usage_missing_root-1-2-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[usage_output-0-2-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[usage_output-1-2-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[usage_to_with_all-0-2-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[usage_to_with_all-1-2-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[usage_unknown_option-0-2-absent]
- test_the_outcome_matrix_holds_without_and_with_verbosity[usage_unknown_option-1-2-absent]
- test_two_successive_runs_against_the_same_graph_are_ordered_and_independently_accurate
