# Official source review

Forecast cutoff: **2026-10-25T01:50:00Z** (24 October, 21:50 EDT). Review completed 2026-10-25T01:50:41Z. Retrieved time, session time, and cutoff are distinct. Live pages have no immutable publication history; later corrections cannot be ruled out.

## Calendar and cutoff

[Current official calendar](https://www.formula1.com/en/racing/2026), checked at 2026-10-25T01:48:26Z: 23 rounds, 17 completed. Remaining races: United States Oct25; Mexico Nov1; Brazil Nov8; Las Vegas Nov21; Qatar Nov29; Abu Dhabi Dec6. Round16 is explicitly titled Bahrain Grand Prix in Malaysia; the unusual feed name is corroborated. Do not reuse the original 24-round announcement.

[Austin schedule](https://www.formula1.com/en/racing/2026/united-states), checked at 01:48:26Z: practice1/2 Oct23, practice3 Oct24, qualifying Oct24 21:00–22:00 UTC, race Oct25 20:00 UTC. Qualifying is complete; the race remains pending at cutoff.

[Official Sprint announcement](https://www.formula1.com/en/latest/article/formula-1-and-fia-announce-2026-sprint-calendar.3PyLPAazrBNe8kQIS3wOfY), checked during this review: China, Miami, Canada, Great Britain, Netherlands, Singapore (Oct9–11). All six Sprint weekends precede cutoff; **zero Sprints remain**. Its original full-season race calendar is superseded by the current calendar above.

## Austin grid

[Official displayed starting grid](https://www.formula1.com/en/results/2026/races/1297/united-states/starting-grid) was directly retrieved at **2026-10-25T01:49:04.297472Z**, before revised cutoff. Its 22 positions are retained in `data/grid.tsv`; driver IDs are local mappings from official names/codes. [Qualifying](https://www.formula1.com/en/results/2026/races/1297/united-states/qualifying) was independently retrieved at 01:49:04.389233Z.

Qualifying order: ANT VER HAD LEC HAM RUS PIA NOR LIN LAW GAS HUL BOR COL ALO BEA OCO STR SAI ALB PER BOT. Grid differs: LIN14, COL21, BOR22. [Official penalty report](https://www.formula1.com/en/latest/article/lindblad-colapinto-and-bortoleto-to-take-grid-penalties-for-united-states-grand-prix.4qjn84W4zOmwNXeELFWn0k) confirms respective 5-place, 10-place and 80-place penalties. This is the current website grid, not a claimed final FIA grid; final publication is due one hour before race formation.

`data/austin-qualifying.tsv` retains those 22 qualifying positions separately because the public feed lacked round18 qualifying. It uses the verified official order and names/team IDs mapped from round17 feed entries, including `arvid_lindblad`. Its columns are season, round, driver_id, name, team_id, position (no header). The grid's fifth `known_at` column conservatively uses retrieval time `2026-10-25T01:49:04Z`, not an invented announcement timestamp.

## Standings cross-check

[Official drivers standings](https://www.formula1.com/en/results/2026/drivers), checked at 01:48:26Z: ANT340 RUS238 HAM229 VER221 LEC215 NOR201 PIA138 HAD97 LAW75 GAS41 LIN38 COL27 BEA23 HUL13 BOR10 OCO7 ALO7 SAI7 ALB5 TSU1 STR0 BOT0 PER0. These are pre-Austin-race totals. Twenty-three season scorers are compatible with a 22-car Austin field.

## Rules

[FIA Section A, Issue03, 25 June 2026](https://www.fia.com/system/files/documents/fia_2026_f1_regulations_-_section_a_general_provisions_-_iss_03_-_2026-06-25.pdf), reviewed 01:48–01:50Z, A2.1.4/A2.2:

- Championship ties: compare numbers of GP wins, seconds, thirds, and subsequent places; if still tied, repeat with qualifying results. Sprint placements do not enter the GP result countback. No final arbitrary driver-ID rule is specified here.
- GP scoring vectors by leader distance: under25% `[6,4,3,2,1]`; 25–under50% `[13,10,8,6,5,4,3,2,1]`; 50–under75% `[19,14,12,10,8,6,4,3,2,1]`; at least75% `[25,18,15,12,10,8,6,4,2,1]`. At least two consecutive leader laps without SC/VSC are required. No fastest-lap bonus.
- Sprint: at least50% distance and the same two-lap condition; `[8,7,6,5,4,3,2,1]`. Dead-heat points are pooled and shared equally.

[FIA Section B, Issue09, 1 October 2026](https://www.fia.com/system/files/documents/fia_2026_f1_regulations_-_section_b_sporting_-_iss_09_-_2026-10-01.pdf), reviewed before completion, B2.3.5/B2.5.5: classify by completed laps then line-crossing order; minimum distance is `floor(0.9 × winner_laps)`. A retirement can still classify and score. At56 winner laps the threshold is50, not51. B2.5.4 distinguishes provisional and final grid publication.

Direct FIA PDF caching returned HTTP403. The public web reader exposed the documents and relevant clauses; no local PDF is claimed. This is a source review, with no fabricated forecast or claim of full revision-history verification.
