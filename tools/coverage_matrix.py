#!/usr/bin/env python3
"""Coverage matrix of the PF1550 driver: check it, or regenerate its markdown page.

docs/coverage-matrix.csv is the source: one row per datasheet parameter with the driver API, the model
behaviour, the test suite and files, and a status (OK, INFO, TODO). The markdown page is generated from it.

    python3 tools/coverage_matrix.py check   # totals agree with docs/coverage.md; every code-like name of the api
                                             # column exists in inc/ or src/ and of the model column in tests/;
                                             # test files exist
    python3 tools/coverage_matrix.py write   # regenerate docs/coverage-matrix.md

Uses the standard library only.
"""
import collections
import csv
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
CSV = ROOT / "docs" / "coverage-matrix.csv"
PAGE = ROOT / "docs" / "coverage-matrix.md"
COVERAGE = ROOT / "docs" / "coverage.md"
COLUMNS = ["id", "group", "item", "datasheet_table", "datasheet_page", "api", "model", "test_suite",
           "test_files", "reset_source", "status"]
STATUSES = ("OK", "INFO", "TODO")


def load():
    with CSV.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames != COLUMNS:
            raise SystemExit("coverage-matrix.csv: unexpected columns %s" % reader.fieldnames)
        return list(reader)


def totals(rows):
    count = collections.Counter(r["status"] for r in rows)
    return len(rows), count["OK"], count["INFO"], count["TODO"]


def group_totals(rows):
    groups = collections.OrderedDict()
    for r in rows:
        groups.setdefault(r["group"], collections.Counter())[r["status"]] += 1
    return groups


def source_text():
    parts = []
    for pattern in ("inc/*.hpp", "src/*.ipp"):
        for path in sorted(ROOT.glob(pattern)):
            parts.append(path.read_text(encoding="utf-8"))
    return "\n".join(parts)


def model_text():
    parts = []
    for path in sorted(ROOT.glob("tests/*.hpp")):
        parts.append(path.read_text(encoding="utf-8"))
    return "\n".join(parts)


# A code-like word: a member path (a.b, a.b[i]), a snake_case or kName identifier, or a CamelCase one with two
# humps. Every part of a member path must exist, so an invented field name fails at once.
TOKEN = re.compile(r"\b(?:[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)+|[a-z]+(?:_[a-z0-9]+)+|k[A-Z][A-Za-z0-9]+|"
                   r"[A-Z][a-z0-9]+(?:[A-Z][a-z0-9]*)+)\b")


def code_tokens(cell):
    out = []
    for token in TOKEN.findall(cell):
        for part in token.split("."):
            if part and part not in out:
                out.append(part)
    return out


def test_cases():
    """The test case names of tests/*.cpp, by suite id: `TEST_CASE("T02 name")` belongs to suite T02."""
    suites = collections.OrderedDict()
    for path in sorted(ROOT.glob("tests/*.cpp")):
        for name in re.findall(r'TEST_CASE\("([^"]+)"\)', path.read_text(encoding="utf-8")):
            m = re.match(r"(T\d\d) (.*)", name)
            if m:
                suites.setdefault(m.group(1), []).append((m.group(2), path.name))
    return suites


def check():
    rows = load()
    problems = []
    seen = set()
    for r in rows:
        if r["id"] in seen:
            problems.append("duplicate id " + r["id"])
        seen.add(r["id"])
        if r["status"] not in STATUSES:
            problems.append("%s: status %r" % (r["id"], r["status"]))
    n, ok, info, todo = totals(rows)
    text = COVERAGE.read_text(encoding="utf-8")
    m = re.search(r"\*\*Totals: (\d+) rows; OK (\d+), INFO (\d+), TODO (\d+);", text)
    if not m:
        problems.append("docs/coverage.md has no Totals line")
    elif tuple(int(x) for x in m.groups()) != (n, ok, info, todo):
        problems.append("docs/coverage.md totals %s differ from the matrix %s" % (m.groups(), (n, ok, info, todo)))
    for name, count in group_totals(rows).items():
        label = re.sub(r"\s*\(.*\)\s*$", "", name)
        line = [l for l in text.splitlines() if l.startswith("| ") and label.split(" (")[0][:20] in l]
        if not line:
            problems.append("docs/coverage.md has no row for group " + name)
            continue
        cells = [c.strip() for c in line[0].strip("|").split("|")]
        want = [str(sum(count.values())), str(count["OK"]), str(count["INFO"]), str(count["TODO"])]
        if cells[-4:] != want:
            problems.append("group %s: coverage.md %s, matrix %s" % (name, cells[-4:], want))
    source = source_text()
    model = model_text()
    cases = test_cases()
    for r in rows:
        for suite in re.findall(r"\bT\d\d\b", r["test_suite"]):
            if suite not in cases:
                problems.append("%s: suite %s has no TEST_CASE named \"%s ...\"" % (r["id"], suite, suite))
    for r in rows:
        for column, haystack, where in (("api", source, "inc/ or src/"), ("model", model, "tests/")):
            for token in code_tokens(r[column]):
                if token not in haystack:
                    problems.append("%s: %s name %s is not in %s" % (r["id"], column, token, where))
        for token in re.findall(r"\b([A-Z][A-Za-z0-9]+)\(", r["api"]):
            if token not in source:
                problems.append("%s: API name %s is not in inc/ or src/" % (r["id"], token))
        for f in filter(None, (x.strip() for x in r["test_files"].split(";"))):
            if not (ROOT / f.split(" (")[0]).exists():
                problems.append("%s: test file %s does not exist" % (r["id"], f))
    for p in problems:
        print("coverage-matrix: " + p)
    print("coverage-matrix: %d rows; OK %d, INFO %d, TODO %d; %d problem(s)" % (n, ok, info, todo, len(problems)))
    return 1 if problems else 0


def esc(cell):
    return cell.replace("|", "\\|").replace("\n", " ")


def write():
    rows = load()
    n, ok, info, todo = totals(rows)
    out = [
        "# Coverage matrix",
        "",
        "One row per datasheet (Rev. 7) parameter: register, bit field, interrupt source, state, electrical or",
        "timing parameter, pin, OTP value. Each row names the driver API, the model behaviour and the test suite",
        "with its files; the status says whether the behaviour is covered. The source is",
        "[`coverage-matrix.csv`](coverage-matrix.csv); this page is generated from it by",
        "`python3 tools/coverage_matrix.py write`, and `python3 tools/coverage_matrix.py check` verifies the totals",
        "against [Coverage](coverage.md).",
        "",
        "**Totals: %d rows; OK %d, INFO %d, TODO %d.**" % (n, ok, info, todo),
        "",
        "The test suite ids (`T01` to `T28`) name groups of tests; the files column lists where they live. A suite",
        "is not one test case: it is the set of cases in those files that exercise the behaviour.",
        "",
        "| Suite | Test cases | Files |",
        "| --- | --- | --- |",
    ]
    cases = test_cases()
    suites = collections.OrderedDict()
    for r in rows:
        for s in filter(None, re.split(r"[,; ]+", r["test_suite"])):
            if re.fullmatch(r"T\d\d", s):
                suites.setdefault(s, r["test_files"] if r["test_suite"].strip() == s else suites.get(s, ""))
    for s in sorted(suites):
        files = next((r["test_files"] for r in rows if r["test_suite"].strip() == s), "")
        names = "; ".join(n for n, _ in cases.get(s, []))
        out.append("| %s | %s | %s |" % (s, esc(names), esc(files)))
    out.append("")
    current = None
    for r in rows:
        if r["group"] != current:
            current = r["group"]
            out += ["", "## " + current, "",
                    "| Id | Item | Datasheet | API | Model | Test | Status |",
                    "| --- | --- | --- | --- | --- | --- | --- |"]
        ref = r["datasheet_table"] + ((" p" + r["datasheet_page"]) if r["datasheet_page"] else "")
        test = r["test_suite"] + ((": " + r["test_files"]) if r["test_files"] else "")
        out.append("| %s | %s | %s | %s | %s | %s | %s |" % (
            esc(r["id"]), esc(r["item"]), esc(ref), esc(r["api"]), esc(r["model"]), esc(test), r["status"]))
    PAGE.write_text("\n".join(out) + "\n", encoding="utf-8")
    print("wrote %s (%d rows)" % (PAGE.relative_to(ROOT), n))
    return 0


def main(argv):
    if len(argv) != 2 or argv[1] not in ("check", "write"):
        print(__doc__)
        return 2
    return check() if argv[1] == "check" else write()


if __name__ == "__main__":
    sys.exit(main(sys.argv))
