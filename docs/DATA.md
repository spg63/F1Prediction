# F1 inputs

The forecasting implementation is C++. `scripts/fetch.sh` uses Bash, curl and jq to fetch public Jolpica JSON and generate plain TSV inputs. Run:

```sh
./scripts/fetch.sh 2026-10-25T01:50:00Z
```

Cached files in `data/raw/` have names such as `2026-results-0.json`. Existing files are reused; `REFRESH=1 ./scripts/fetch.sh ...` replaces cached downloads. Requests use an identifying user agent and wait 1.1 seconds between downloads, below Jolpica's stated 4/second and 500/hour limits. Pagination uses 100 rows with explicit offsets; result totals count individual drivers across grouped race objects. Failed runs preserve successful downloads. No Python is needed.

`data/SOURCES.txt` lists each raw filename, UTC retrieval time, and source URL. `data/CUTOFF.txt` records **2026-10-25T01:50:00Z** (**2026-10-24 21:50 America/New_York**). Some data were retrieved after cutoff. The API has no reliable historical publication timestamps: a later download of a pre-cutoff classification cannot establish whether subsequent corrections were already known at cutoff. Source classifications and lap times remain in raw JSON.

TSV files have no headers, except the separately supplied official grid which may have a header. Fields, in order:

- `events.tsv`: season, round, circuit_id, name, race_start, qualifying_start, sprint_start; absent sprint time is empty. Future events define the remaining schedule.
- `results.tsv` and `sprints.tsv`: season, round, driver_id, driver_name, team_id, position, grid, laps, status, points.
- `qualifying.tsv`: season, round, driver_id, driver_name, team_id, position.
- `standings.tsv`: season, round, driver_id, name, team_id, position, points, wins.
- `grid.tsv`: season, round, driver_id, position, known_at. The official Austin grid includes penalties and differs from qualifying; `known_at` is its verified UTC retrieval time. See `docs/SOURCE_REVIEW.md` for URLs and the provisional-grid limitation.

Race, qualifying, and sprint classifications enter TSV evidence only when scheduled start plus a conservative four-, three-, or two-hour buffer respectively is at or before cutoff and source results exist. This is a schedule safeguard, not proof of actual finish time. The C++ runtime must reject known missing completed races or points that fail to reconcile. Status strings, retirement laps, grid zeroes, and actual awarded points are preserved. No synthetic current observations or silent corrections are added.

At cutoff the live 2026 calendar contains 23 rounds. All 17 completed GPs have 22 classifications each, and all six sprints have results. Race plus sprint points reconcile exactly with the round-17 standings. Standings include 23 season participants, while the current Austin field has 22. Six GPs and zero sprints remain, beginning with Austin on October 25 at 20:00 UTC. Round 16 is unusually labeled “Bahrain Grand Prix in Malaysia” at Sepang; independent review corroborated the official calendar label, which is preserved.

Jolpica's qualifying endpoint currently stops at round 17 despite completed Austin qualifying. Its 2026 Australian qualifying has 19 rows and Spanish qualifying has 20; those source omissions are retained. The independently verified official `data/austin-qualifying.tsv` supplies all 22 Austin qualifying positions. The fetch script merges this supplement after applying the qualifying cutoff and replaces any API rows for that same session. The current qualifying file therefore contains 391 rows, including Austin; it does not infer a penalty-adjusted grid from qualifying. Historical result rows are 440 in 2023, 479 in 2024, and 479 in 2025; calendars have 22, 24, and 24 rounds. These are returned source classifications, with absent non-starters not invented.

Explicit-round standings are requested through the last expected completed GP. Source mistakes, cancellations, revised calendars, penalties, and session delays still require review; the shell fetcher adds no generic validation framework.

Data credit: **Jolpica F1 and its contributors, via the Ergast-compatible API**. [Public API](https://api.jolpi.ca/ergast/f1/), [documentation](https://github.com/jolpica/jolpica-f1/blob/main/docs/README.md), and [Terms of Use](https://github.com/jolpica/jolpica-f1/blob/main/TERMS.md). API data are offered for non-commercial use under [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/); preserve attribution with derived datasets. The volunteer service does not guarantee correctness or availability. Official Formula 1 cross-checks are described in `docs/SOURCE_REVIEW.md`.
