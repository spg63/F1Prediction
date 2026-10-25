# Project working instructions

This is an F1 Drivers' Championship forecasting project. Top five means final championship positions. Preserve source snapshots and distinguish retrieval time, event time, and forecast cutoff. Never present stale, partial, synthetic, or post-cutoff data as current evidence.

Use C++17, public unauthenticated sources, explicit seeds, and offline reproducibility. Keep this a small personal project: no hashes, provenance framework, databases, plugin systems, CI, or elaborate inference machinery. A short shell script with curl/jq is sufficient for fetching plain inputs; no Python. Do not publish a remote repository, purchase data, collect secrets, or execute gambling transactions. Never use `rm -rf`. Use only Sol or Luna agents; never Astra.

Read `docs/MODEL.md` and `docs/DATA.md` before changing their related code. Do not add a test suite or infrastructure. Compile and run the actual program, inspect mathematical invariants and results. Add a unit test only if indispensable. Keep heuristic assumptions visible. Do not claim calibration from model complexity; compare chronological held-out scores to a simple baseline.

No repository-local skills existed at project creation. Before committing, compile, check scoring/countback and simulation invariants, and run one reproducible data-backed forecast if coverage permits. Commit locally only.
