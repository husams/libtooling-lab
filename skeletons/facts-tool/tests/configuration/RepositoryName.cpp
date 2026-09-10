#include "Sandbox.h"
#include "config/RepositoryName.h"
#include <fstream>
#include <git2.h>

namespace configuration_test {
namespace {
void initRepo(const fs::path &path, git_repository **out) {
  git_libgit2_init();
  assert(git_repository_init(out, path.c_str(), 0) == 0);
}

void addRemote(git_repository *repo, const char *name, const char *url) {
  git_remote *remote = nullptr;
  assert(git_remote_create(&remote, repo, name, url) == 0);
  git_remote_free(remote);
}
} // namespace

void repositoryName() {
  // A real repository with an origin remote: repositoryName() picks it, and
  // renderDatabasePath proves {project_name} takes the repo name while
  // {filename} keeps meaning the project basename (B-044-style split).
  {
    Sandbox box;
    git_repository *repo = nullptr;
    initRepo(box.root, &repo);
    addRemote(repo, "origin", "https://example.invalid/owner/acme-repo.git");
    git_repository_free(repo);
    const auto name = facts::config::repositoryName(box.root);
    assert(name && *name == "acme-repo");

    facts::config::Resolved v;
    v.projectRoot = box.root;
    v.storageRoot = box.root / "store";
    v.templateText = "{project_name}/{filename}.db";
    const auto rendered = facts::config::renderDatabasePath(v);
    assert(rendered && *rendered ==
          v.storageRoot / "acme-repo" / (box.root.filename().string() + ".db"));
  }

  // Two remotes, no "origin": the alphabetically first name wins.
  {
    Sandbox box;
    git_repository *repo = nullptr;
    initRepo(box.root, &repo);
    addRemote(repo, "zeta", "https://example.invalid/zeta-repo.git");
    addRemote(repo, "alpha", "https://example.invalid/alpha-repo.git");
    git_repository_free(repo);
    const auto name = facts::config::repositoryName(box.root);
    assert(name && *name == "alpha-repo");
  }

  // No remotes at all: nullopt, so the caller falls back to the basename.
  {
    Sandbox box;
    git_repository *repo = nullptr;
    initRepo(box.root, &repo);
    git_repository_free(repo);
    assert(!facts::config::repositoryName(box.root));
  }

  // A ".git" gitlink file (worktrees, submodules) resolves through to the
  // real repository's own remote, the same way a plain ".git" directory
  // would -- libgit2 follows "gitdir: <path>" itself, no special casing
  // needed on our side.
  {
    Sandbox box;
    const auto real = box.root / "real";
    fs::create_directories(real);
    git_repository *repo = nullptr;
    initRepo(real, &repo);
    addRemote(repo, "origin", "https://example.invalid/team/linked-repo.git");
    git_repository_free(repo);
    const auto linked = box.root / "linked";
    fs::create_directories(linked);
    std::ofstream(linked / ".git") << "gitdir: " << (real / ".git").string() << "\n";
    const auto name = facts::config::repositoryName(linked);
    assert(name && *name == "linked-repo");
  }

  // A gitlink that resolves nowhere real, and an empty ".git" directory:
  // both fail to open as a repository and fall back silently, no crash.
  {
    Sandbox box;
    std::ofstream(box.root / ".git") << "gitdir: unused";
    assert(!facts::config::repositoryName(box.root));
  }
  {
    Sandbox box;
    fs::create_directory(box.root / ".git");
    assert(!facts::config::repositoryName(box.root));
  }
}
} // namespace configuration_test
