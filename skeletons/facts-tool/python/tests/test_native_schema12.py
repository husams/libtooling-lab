from facts_tool import open_codebase


def test_native_current_reader_and_existing_navigation(native_current_pair):
    with open_codebase(
        facts_db=native_current_pair[0], project_db=native_current_pair[1]
    ) as cb:
        assert cb.provenance.facts.schema.user_version == 13
        run = cb.callgraphs.latest()
        assert run.status == "complete" and run.path_outcome == "not-applicable"
        assert [item.qualified_name for item in cb.find("app::run").callees()] == [
            "app::save"
        ]
        assert [item.qualified_name for item in cb.find("app::save").callers()] == [
            "app::run"
        ]
        assert [item.qualified_name for item in cb.find("app::Box").bases()] == [
            "app::Base"
        ]
