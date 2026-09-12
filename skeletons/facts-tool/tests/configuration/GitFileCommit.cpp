#include "config/GitFileCommit.h"
#include "Sandbox.h"
#include <git2.h>

namespace configuration_test {
namespace {

void initRepo(const fs::path &path, git_repository **out) {
  git_libgit2_init();
  assert(git_repository_init(out, path.c_str(), 0) == 0);
}

std::string commitTrackedFile(git_repository *repo,
                              const std::string &relativePath) {
  git_index *index = nullptr;
  assert(git_repository_index(&index, repo) == 0);
  assert(git_index_add_bypath(index, relativePath.c_str()) == 0);
  assert(git_index_write(index) == 0);
  git_oid treeId;
  assert(git_index_write_tree(&treeId, index) == 0);
  git_tree *tree = nullptr;
  assert(git_tree_lookup(&tree, repo, &treeId) == 0);
  git_signature *signature = nullptr;
  assert(git_signature_now(&signature, "Test", "test@example.invalid") == 0);
  git_oid commitId;
  assert(git_commit_create_v(&commitId, repo, "HEAD", signature, signature,
                             nullptr, "message", tree, 0) == 0);
  const std::string hex = git_oid_tostr_s(&commitId);
  git_signature_free(signature);
  git_tree_free(tree);
  git_index_free(index);
  return hex;
}

} // namespace

void gitFileCommit() {
  // A tracked file resolves to HEAD's hex commit id.
  {
    Sandbox box;
    git_repository *repo = nullptr;
    initRepo(box.root, &repo);
    const auto tracked = box.write("tracked.cpp", "int value();\n");
    const auto commit = commitTrackedFile(repo, "tracked.cpp");
    git_repository_free(repo);

    facts::config::GitCommitResolver resolver;
    const auto resolved = resolver.commitFor(tracked);
    assert(resolved && *resolved == commit);
  }

  // An untracked file inside the same repository resolves to nullopt: it is
  // not reachable from HEAD, no matter what HEAD says.
  {
    Sandbox box;
    git_repository *repo = nullptr;
    initRepo(box.root, &repo);
    box.write("tracked.cpp", "int value();\n");
    commitTrackedFile(repo, "tracked.cpp");
    const auto untracked = box.write("untracked.cpp", "int other();\n");
    git_repository_free(repo);

    facts::config::GitCommitResolver resolver;
    assert(!resolver.commitFor(untracked));
  }

  // A file outside every repository resolves to nullopt. Self-checking: the
  // sandbox is not git-initialized, but an ambient environment where the
  // system temp directory itself sits inside some other repository (an
  // unusual CI layout, a checkout that redirects TMPDIR) would silently
  // invalidate the premise, so confirm no repository is discoverable here
  // before relying on that for the actual assertion below.
  {
    Sandbox outer;
    git_libgit2_init();
    git_buf discovered{};
    const auto found =
        git_repository_discover(&discovered, outer.root.c_str(), 0, nullptr);
    git_buf_dispose(&discovered);
    assert(found != 0 &&
           "sandbox must not be inside any git repository for this case");

    const auto file = outer.write("outside.cpp", "int value();\n");
    facts::config::GitCommitResolver resolver;
    assert(!resolver.commitFor(file));
  }

  // A repository with an unborn HEAD -- staged but never committed --
  // resolves to nullopt even though the file is tracked in the index.
  {
    Sandbox box;
    git_repository *repo = nullptr;
    initRepo(box.root, &repo);
    const auto staged = box.write("staged.cpp", "int value();\n");
    git_index *index = nullptr;
    assert(git_repository_index(&index, repo) == 0);
    assert(git_index_add_bypath(index, "staged.cpp") == 0);
    assert(git_index_write(index) == 0);
    git_index_free(index);
    git_repository_free(repo);

    facts::config::GitCommitResolver resolver;
    assert(!resolver.commitFor(staged));
  }

  // A file reached through a symlinked intermediate directory still resolves
  // through to its real, tracked index entry: the lexical (symlink-intact)
  // relative path tried first cannot match anything in the index here (the
  // file was `git add`ed under its real path, not this symlinked one), so
  // resolution falls back to the canonical file path, which does match.
  {
    Sandbox box;
    git_repository *repo = nullptr;
    initRepo(box.root, &repo);
    box.write("real/nested/tracked.cpp", "int value();\n");
    const auto commit = commitTrackedFile(repo, "real/nested/tracked.cpp");
    git_repository_free(repo);

    const auto link = box.root / "link";
    fs::create_directory_symlink(box.root / "real", link);
    const auto viaSymlink = link / "nested" / "tracked.cpp";
    assert(fs::is_symlink(link));

    facts::config::GitCommitResolver resolver;
    const auto resolved = resolver.commitFor(viaSymlink);
    assert(resolved && *resolved == commit);
  }

  // A tracked file that is itself a symlink still resolves: the index
  // records the symlink at its own (lexical) path, not at whatever the
  // symlink points to, so canonicalizing first -- which would resolve past
  // the symlink to its target -- must not be tried before the lexical path.
  {
    Sandbox box;
    git_repository *repo = nullptr;
    initRepo(box.root, &repo);
    const auto target = box.write("generated/real.cpp", "int value();\n");
    const auto link = box.root / "linked.cpp";
    fs::create_symlink(target, link);
    assert(fs::is_symlink(link));
    const auto commit = commitTrackedFile(repo, "linked.cpp");
    git_repository_free(repo);

    facts::config::GitCommitResolver resolver;
    const auto resolved = resolver.commitFor(link);
    assert(resolved && *resolved == commit);
  }
}

} // namespace configuration_test
