# SPDX-License-Identifier: MIT
"""Navigation and repository-link hooks for the libsixel documentation."""

from __future__ import annotations

from html import unescape
import logging
import os
from pathlib import Path
import re
import subprocess
from urllib.parse import quote, unquote, urlsplit

from mkdocs.exceptions import ConfigurationError


HEADING_RE = re.compile(r"^##\s+(.+?)\s*$")
TITLE_RE = re.compile(r"^#\s+(.+?)\s*$", re.MULTILINE)
LINK_RE = re.compile(
    r"(?P<prefix>!?\[[^\]\n]*\]\()"
    r"(?P<target><[^>\n]+>|[^)\s\n]+)"
)
HTML_MEDIA_TAG_RE = re.compile(r"<(?:img|source)\b[^>]*>", re.IGNORECASE)
HTML_ASSET_ATTRIBUTE_RE = re.compile(
    r"(?P<prefix>\b(?:src|srcset)\s*=\s*)"
    r"(?P<quote>[\"'])"
    r"(?P<value>.*?)"
    r"(?P=quote)",
    re.IGNORECASE,
)
FENCE_RE = re.compile(r"^\s*(`{3,}|~{3,})")
LOGGER = logging.getLogger("mkdocs.plugins.libsixel")

_docs_dir: Path | None = None
_repo_root: Path | None = None
_repository_url = ""
_source_revision = ""

# A Markdown page and a directory index with the same stem otherwise both map
# to one index.html when directory URLs are enabled. Keep both public pages and
# give the higher-level loader document a stable, descriptive URL.
DESTINATION_OVERRIDES = {
    "loader/builtin.md": "loader/builtin-loader/index.html",
}


def _is_relative_to(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
    except ValueError:
        return False
    return True


def _without_angle_brackets(target: str) -> str:
    if target.startswith("<") and target.endswith(">"):
        return target[1:-1]
    return target


def _iter_unfenced_lines(markdown: str):
    fence = None
    for line in markdown.splitlines(keepends=True):
        match = FENCE_RE.match(line)
        if match is not None:
            marker = match.group(1)[0]
            if fence is None:
                fence = marker
            elif fence == marker:
                fence = None
            yield line, False
            continue
        yield line, fence is None


def _local_markdown_targets(index: Path, docs_dir: Path) -> list[Path]:
    targets = []
    seen = set()
    markdown = index.read_text(encoding="utf-8")
    for line, active in _iter_unfenced_lines(markdown):
        if not active:
            continue
        for match in LINK_RE.finditer(line):
            raw_target = _without_angle_brackets(match.group("target"))
            parsed = urlsplit(raw_target)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            if not parsed.path.lower().endswith(".md"):
                continue
            target = (index.parent / unquote(parsed.path)).resolve()
            if not _is_relative_to(target, docs_dir):
                continue
            if not target.is_file() or target in seen:
                continue
            seen.add(target)
            targets.append(target)
    return targets


def _document_title(path: Path) -> str:
    match = TITLE_RE.search(path.read_text(encoding="utf-8"))
    if match is None:
        raise ConfigurationError(f"documentation page has no H1 title: {path}")
    title = unescape(match.group(1))
    return re.sub(r"[`*_]", "", title).strip()


def _nav_path(path: Path, docs_dir: Path) -> str:
    return path.relative_to(docs_dir).as_posix()


def _directory_navigation(
    index: Path, docs_dir: Path, visited: set[Path]
) -> list[object]:
    visited.add(index)
    # A bare first entry is recognized by Material as the section index and
    # retains the document's H1 as its browser and navigation title.
    navigation: list[object] = [_nav_path(index, docs_dir)]
    for target in _local_markdown_targets(index, docs_dir):
        if target in visited or not _is_relative_to(target, index.parent):
            continue
        if target.name == "README.md":
            navigation.append(
                {
                    _document_title(target): _directory_navigation(
                        target, docs_dir, visited
                    )
                }
            )
            continue
        visited.add(target)
        navigation.append({_document_title(target): _nav_path(target, docs_dir)})
    return navigation


def _root_sections(index: Path, docs_dir: Path):
    sections = []
    current = None
    fence = None
    for line in index.read_text(encoding="utf-8").splitlines():
        fence_match = FENCE_RE.match(line)
        if fence_match is not None:
            marker = fence_match.group(1)[0]
            if fence is None:
                fence = marker
            elif fence == marker:
                fence = None
            continue
        if fence is not None:
            continue
        heading = HEADING_RE.match(line)
        if heading is not None:
            current = [heading.group(1).strip(), []]
            sections.append(current)
            continue
        if current is None:
            continue
        for match in LINK_RE.finditer(line):
            raw_target = _without_angle_brackets(match.group("target"))
            parsed = urlsplit(raw_target)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            if not parsed.path.lower().endswith(".md"):
                continue
            target = (index.parent / unquote(parsed.path)).resolve()
            if _is_relative_to(target, docs_dir) and target.is_file():
                if target not in current[1]:
                    current[1].append(target)
    return sections


def _build_navigation(docs_dir: Path) -> list[object]:
    index = docs_dir / "README.md"
    if not index.is_file():
        raise ConfigurationError("docs/README.md is required for navigation")

    visited = {index}
    navigation: list[object] = [{"Home": "README.md"}]
    for section_title, targets in _root_sections(index, docs_dir):
        section_navigation = []
        for target in targets:
            if target in visited:
                continue
            if target.name == "README.md":
                section_navigation.append(
                    {
                        _document_title(target): _directory_navigation(
                            target, docs_dir, visited
                        )
                    }
                )
                continue
            visited.add(target)
            section_navigation.append(
                {_document_title(target): _nav_path(target, docs_dir)}
            )
        if section_navigation:
            navigation.append({section_title: section_navigation})

    all_pages = {path.resolve() for path in docs_dir.rglob("*.md")}
    missing = sorted(all_pages - visited)
    if missing:
        formatted = "\n".join(
            "  " + _nav_path(path, docs_dir) for path in missing
        )
        raise ConfigurationError(
            "documentation pages are missing from README navigation:\n"
            + formatted
        )
    LOGGER.info("Generated navigation for %d Markdown pages", len(visited))
    return navigation


def _git_revision(repo_root: Path, fallback: str) -> str:
    explicit = os.environ.get("LIBSIXEL_DOCS_SOURCE_REVISION")
    if explicit:
        return explicit
    github_revision = os.environ.get("GITHUB_SHA")
    if github_revision:
        return github_revision
    result = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=repo_root,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )
    if result.returncode == 0 and result.stdout.strip():
        return result.stdout.strip()
    return fallback


def _github_repository_url(fallback: str) -> str:
    server = os.environ.get("GITHUB_SERVER_URL")
    repository = os.environ.get("GITHUB_REPOSITORY")
    if server and repository:
        return server.rstrip("/") + "/" + repository.strip("/")
    repository_url = fallback.rstrip("/")
    if repository_url.endswith(".git"):
        repository_url = repository_url[:-4]
    return repository_url


def on_config(config):
    global _docs_dir
    global _repo_root
    global _repository_url
    global _source_revision

    _docs_dir = Path(config["docs_dir"]).resolve()
    config_path = Path(config.config_file_path).resolve()
    _repo_root = config_path.parent
    extra = config.get("extra", {})
    _repository_url = _github_repository_url(extra["source_repository"])
    _source_revision = _git_revision(
        _repo_root, extra.get("source_revision", "master")
    )
    config["nav"] = _build_navigation(_docs_dir)
    return config


def on_files(files, config):
    for source, destination in DESTINATION_OVERRIDES.items():
        file = files.get_file_from_path(source)
        if file is None:
            raise ConfigurationError(
                f"documentation destination override is missing: {source}"
            )
        file.dest_uri = destination

    destinations = {}
    for file in files:
        previous = destinations.get(file.dest_uri)
        if previous is not None:
            raise ConfigurationError(
                "documentation files share output path "
                f"{file.dest_uri}: {previous.src_uri}, {file.src_uri}"
            )
        destinations[file.dest_uri] = file
    return files


def _raw_repository_url(repository_url: str) -> str | None:
    parsed = urlsplit(repository_url)
    if parsed.hostname != "github.com":
        return None
    return "https://raw.githubusercontent.com" + parsed.path


def _rewrite_target(prefix: str, raw_target: str, source: Path) -> str:
    if _docs_dir is None or _repo_root is None:
        return raw_target
    target_text = _without_angle_brackets(raw_target)
    parsed = urlsplit(target_text)
    if parsed.scheme or parsed.netloc or not parsed.path:
        return raw_target
    target = (source.parent / unquote(parsed.path)).resolve()
    if _is_relative_to(target, _docs_dir):
        # MkDocs can route a documentation directory only when it owns an
        # index page. Asset and measurement directories need a repository-tree
        # link because the generated site has no directory-listing endpoint.
        if not target.is_dir():
            return raw_target
        if (target / "README.md").is_file() or (target / "index.md").is_file():
            return raw_target
    if not _is_relative_to(target, _repo_root):
        return raw_target

    repository_path = quote(
        target.relative_to(_repo_root).as_posix(), safe="/"
    )
    if prefix.startswith("!["):
        raw_repository = _raw_repository_url(_repository_url)
        if raw_repository is not None:
            rewritten = (
                f"{raw_repository}/{quote(_source_revision, safe='')}/"
                f"{repository_path}"
            )
        else:
            rewritten = (
                f"{_repository_url}/raw/{quote(_source_revision, safe='')}/"
                f"{repository_path}"
            )
    else:
        object_type = "tree" if target.is_dir() else "blob"
        rewritten = (
            f"{_repository_url}/{object_type}/"
            f"{quote(_source_revision, safe='')}/{repository_path}"
        )
    if parsed.query:
        rewritten += "?" + parsed.query
    if parsed.fragment:
        rewritten += "#" + parsed.fragment
    return rewritten


def _rewrite_html_asset_target(target_text, source, page, files):
    parsed = urlsplit(target_text)
    if parsed.scheme or parsed.netloc or not parsed.path:
        return target_text
    target = (source.parent / unquote(parsed.path)).resolve()
    if _docs_dir is None or not _is_relative_to(target, _docs_dir):
        return target_text
    source_uri = target.relative_to(_docs_dir).as_posix()
    target_file = files.get_file_from_path(source_uri)
    if target_file is None or not target.is_file():
        return target_text
    rewritten = target_file.url_relative_to(page.file)
    if parsed.query:
        rewritten += "?" + parsed.query
    if parsed.fragment:
        rewritten += "#" + parsed.fragment
    return rewritten


def _rewrite_srcset(value, source, page, files):
    rewritten = []
    for candidate in value.split(","):
        leading = candidate[: len(candidate) - len(candidate.lstrip())]
        trailing = candidate[len(candidate.rstrip()) :]
        fields = candidate.strip().split(None, 1)
        if not fields:
            rewritten.append(candidate)
            continue
        target = _rewrite_html_asset_target(fields[0], source, page, files)
        descriptor = " " + fields[1] if len(fields) == 2 else ""
        rewritten.append(leading + target + descriptor + trailing)
    return ",".join(rewritten)


def _rewrite_html_media_tag(match, source, page, files):
    def replace_attribute(attribute_match):
        prefix = attribute_match.group("prefix")
        quote_character = attribute_match.group("quote")
        value = attribute_match.group("value")
        attribute = prefix.split("=", 1)[0].strip().lower()
        if attribute == "srcset":
            value = _rewrite_srcset(value, source, page, files)
        else:
            value = _rewrite_html_asset_target(value, source, page, files)
        return prefix + quote_character + value + quote_character

    return HTML_ASSET_ATTRIBUTE_RE.sub(replace_attribute, match.group(0))


def on_page_markdown(markdown, page, config, files):
    if _docs_dir is None:
        return markdown
    source = (_docs_dir / page.file.src_path).resolve()
    output = []
    for line, active in _iter_unfenced_lines(markdown):
        if active:
            line = LINK_RE.sub(
                lambda match: (
                    match.group("prefix")
                    + _rewrite_target(
                        match.group("prefix"),
                        match.group("target"),
                        source,
                    )
                ),
                line,
            )
            line = HTML_MEDIA_TAG_RE.sub(
                lambda match: _rewrite_html_media_tag(
                    match, source, page, files
                ),
                line,
            )
        output.append(line)
    return "".join(output)
