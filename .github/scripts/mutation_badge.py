#!/usr/bin/env python3
"""Writes an SVG badge with the mutation score from the JSON report of Mull (reporter "Elements").

    mutation_badge.py reports/mutation.json mutation.svg

Like the coverage badge: the CI commits the SVG to the branch "badges", the README shows it from there.
The score counts a timeout as found (like Stryker): the mutant made a test hang, the tests noticed it. Mull's own
"mutationScore" counts it as not found.
"""
import json
import sys

from coverage_badge import badge, color


def mutation_score(report):
    """Found mutants (killed or timed out) in percent of all mutants of the Elements report."""
    status = [m["status"] for f in report["files"].values() for m in f["mutants"]]
    found = sum(1 for s in status if s in ("Killed", "Timeout"))
    return 100.0 * found / len(status) if status else 0.0


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: mutation_badge.py <mull mutation.json> <badge.svg>")
    with open(sys.argv[1], encoding="utf-8") as report:
        score = mutation_score(json.load(report))
    with open(sys.argv[2], "w", encoding="utf-8") as svg:
        svg.write(badge("mutation score", f"{score:.0f}%", color(score)))
    print(f"mutation score {score:.0f}%")


if __name__ == "__main__":
    main()
