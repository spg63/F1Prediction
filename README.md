# F1Prediction

A small C++17 Monte Carlo program for the **final Drivers’ Championship**, with complete race orders feeding season points and countback. Five source files, a Makefile, plain TSV inputs, and a curl/jq fetch script. No Python, third-party runtime dependencies, hashes, database, framework, CI, or test suite.

```sh
cd ~/git/F1Prediction
make
./f1predict forecast
open reports/current/report.html

./f1predict backtest --sims 5000 --out reports/backtest.csv
./f1predict sensitivity --sims 5000 --out reports/sensitivity.csv
```

The default forecast uses **20,000 simulations**, seed **20261025**, and cutoff **2026-10-25T01:50:00Z** (October 24, 21:50 New York). Repeating the command on the same C++ standard library reproduces the output. Flags: `--season`, `--cutoff`, `--sims`, `--seed`, `--data`, `--out`. Large runs take proportionally longer.

## What is current

The supplied snapshot contains 17 completed 2026 Grands Prix and six completed Sprints. Race plus Sprint points exactly reconcile with the standings. Austin FP1, FP2, FP3 and qualifying have concluded; the race is still pending. **Six GPs and no Sprints remain.** The live calendar has 23 rounds; older announcements disagree. Austin’s qualifying and displayed starting grid are official supplements because the API’s qualifying feed stops at round 17. The grid includes penalties and is still provisional. There are 23 season participants, with 22 active entrants.

Inputs were downloaded around cutoff, including some later downloads of already concluded sessions. This is a session-cutoff reconstruction, not proof of every source revision known at that instant. The Australian and Spanish qualifying sources contain fewer entrants than their GP classifications; omitted ranks are not invented. Exact qualifying countback can therefore be incomplete. See [source checks](docs/SOURCE_REVIEW.md) and [data details](docs/DATA.md).

To regenerate from the cached JSON, no network is needed:

```sh
./scripts/fetch.sh 2026-10-25T01:50:00Z
```

For a genuinely newer forecast, use `REFRESH=1 ./scripts/fetch.sh <UTC-cutoff>` and pass the same cutoff to `forecast`. Review any new calendar changes and official grid supplements. The fetcher uses the public Jolpica API with explicit pagination and 1.1-second spacing. It does not silently substitute a previous season when current data are missing. The program rejects missing concluded races/Sprints and points that disagree with standings. Credits and source URLs are in `data/SOURCES.txt`; preserve the source data’s noncommercial attribution/share-alike terms.

## What it samples

Recent finisher ranks, adjusted for starting grid, and qualifying ranks give a team form estimate plus a shrunken driver residual. A six-round half-life emphasizes recent form. A small, strongly shrunken street/fast/mixed circuit adjustment adds weak track information. Only current-season observations estimate pace, so pre-2026 car performance does not carry across the regulation reset.

Every simulated season draws uncertain driver/team strength, then shared team/weekend performance shocks, qualifying, race noise, mechanical failures and incidents. The model produces a coherent full finishing order, classifies late retirements, awards exact full-distance GP/Sprint points, and applies race-place countback followed by qualifying countback under the 2026 rules. Inactive drivers retain their existing points. This is descriptive research code; the uncertainty scales and status-based failure attribution are heuristics. Equations and parameter values are in [MODEL.md](docs/MODEL.md), beside the code in `src/model.cpp`.

## First result

The checked-in run estimates final top-five membership as:

| Driver | Probability |
|---|---:|
| Antonelli | 100.0% |
| Russell | 96.2% |
| Verstappen | 94.8% |
| Hamilton | 90.8% |
| Leclerc | 73.6% |
| Norris | 44.6% |

These are membership probabilities, not a forced finishing order. Full rank distributions, final point intervals and each remaining race are in [the local report](reports/current/report.html), [championship CSV](reports/current/championship.csv), and [race CSV](reports/current/races.csv). Austin win estimates are Antonelli 40.3%, Verstappen 23.9%, Leclerc 9.2%. A simulated zero or one is not a mathematical guarantee: zero hits out of 20,000 has an approximate 95% upper sampling bound of 0.015%. The largest possible Monte Carlo standard error is 0.35 percentage points; model uncertainty is much larger.

## Sanity check, not a calibration claim

Three pre-weekend 2025 folds train through rounds 8, 14 and 18 and simulate the remaining season. Future results, qualifying, grids and standings are removed from fitting. The realized calendar is used retrospectively; future substitutions are unknown, and the final standings are evaluation targets only. No parameter search was run. All future scorers in these folds were already represented, although the as-of roster can miss a future replacement. Fastest-lap bonus points had been removed in 2025, matching this simulator’s scoring.

Mean scores from 5,000 simulations per fold (lower is better):

| Model | Next-race winner log loss | Championship top-five Brier | Championship rank RPS |
|---|---:|---:|---:|
| Recent form | 1.487 | 0.00052 | 0.0580 |
| Points per start | 1.343 | 0.00165 | 0.0633 |
| Equal pace | 2.985 | 0.01517 | 0.0643 |

Recent form loses to the points baseline on winner prediction in this small sample. Championship membership is easier late in the season because existing points already constrain it; the small Brier values are not evidence of great race forecasting. The folds overlap and are not independent. Winner log loss uses half a simulated win of smoothing per entrant to avoid infinite loss from Monte Carlo zeros. The raw scores, including race-level Brier scores, are in [backtest.csv](reports/backtest.csv). This sample does not establish probability calibration or 2026 performance.

Sensitivity matters: shortening the recency half-life to three rounds raises Leclerc’s top-five chance to 84.0%; ten rounds lowers it to 68.8%. Noise, retirement, track and parameter-uncertainty controls are also varied in [sensitivity.csv](reports/sensitivity.csv). These runs use the same seed, not guaranteed identical random paths after branching.

Compiled warning-free with Apple clang 21 on arm64. Temporary, indispensable checks covered points/Sprints, retirement classification, GP and qualifying countback, deterministic seeds, cutoff exclusion, complete probability rows/columns, top-five mass five, no remaining races and mathematical elimination. Every actual forecast checks probability invariants. No test suite is shipped.

## Limits and later ideas

Future events assume full distance and a fixed active roster. No forecast weather, strategy, team orders, safety-car timing, cancellations, shortened events, dead heats or unannounced penalties. Unresolved regulatory ties receive reported random simulation allocation. Broad circuit groups cannot capture every track characteristic, and the 2026 sample remains small.

Monte Carlo remains the sampling engine. Later, if this stays fun: a [partially pooled driver/constructor ranking model](https://doi.org/10.1515/jqas-2022-0021), dynamic form and retirement causes, then a boosted-model benchmark with enough chronological data. A [lap-level tire/pit/safety-car model](https://www.mdpi.com/2076-3417/10/12/4229) is the most work and comes last. None is implemented here.
