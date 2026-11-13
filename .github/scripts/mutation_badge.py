#!/usr/bin/env python3
"""Writes an SVG badge with the mutation score from the JSON report of Mull (reporter "Elements").

    mutation_badge.py reports/mutation.json mutation.svg

Like the coverage badge: the CI commits the SVG to the branch "badges", the README shows it from there.
"""
import json
import sys

from coverage_badge import badge, color


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: mutation_badge.py <mull mutation.json> <badge.svg>")
    with open(sys.argv[1], encoding="utf-8") as report:
        score = float(json.load(report)["mutationScore"])
    with open(sys.argv[2], "w", encoding="utf-8") as svg:
        svg.write(badge("mutation score", f"{score:.0f}%", color(score)))
    print(f"mutation score {score:.0f}%")


if __name__ == "__main__":
    main()
