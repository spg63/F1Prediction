#!/usr/bin/env bash
# Public data only; curl+jq; cached source JSON is reused unless REFRESH=1.
set -euo pipefail
cd "$(dirname "$0")/.."
cutoff=${1:-2026-10-25T01:50:00Z}
base=https://api.jolpi.ca/ergast/f1
mkdir -p data/raw
printf '%s\n' "$cutoff" > data/CUTOFF.txt
: >> data/SOURCES.txt
fetch_pages() {
  local endpoint=$1 offset=0 total=1 file url limit got
  while ((offset < total)); do
    file="data/raw/${endpoint//\//-}-${offset}.json"
    url="$base/$endpoint.json?limit=100&offset=$offset"
    if [[ ! -f "$file" || ${REFRESH:-0} == 1 ]]; then
      sleep 1.1
      curl --fail --silent --show-error --max-time 45 -A 'F1Prediction/0.1.0 curl' "$url" -o "$file.tmp"
      jq -e '.MRData' "$file.tmp" >/dev/null
      mv "$file.tmp" "$file"
      printf '%s\t%s\t%s\n' "${file#data/raw/}" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$url" >> data/SOURCES.txt
    fi
    got=$(jq -r '.MRData.offset' "$file")
    limit=$(jq -r '.MRData.limit' "$file")
    total=$(jq -r '.MRData.total' "$file")
    [[ "$got" == "$offset" && "$limit" -gt 0 ]]
    offset=$((offset + limit))
  done
}
for file in events results qualifying sprints standings; do : > "data/$file.tsv"; done
for season in 2023 2024 2025 2026; do
  fetch_pages "$season/races"
  calendar="data/raw/$season-races-0.json"
  jq -r '.MRData.RaceTable.Races[] | [.season,.round,.Circuit.circuitId,.raceName,(.date+"T"+.time), (if .Qualifying then .Qualifying.date+"T"+.Qualifying.time else "" end),(if .Sprint then .Sprint.date+"T"+.Sprint.time else "" end)] | @tsv' "$calendar" >> data/events.tsv
  for kind in results qualifying sprint; do
    fetch_pages "$season/$kind"
    for file in data/raw/"$season-$kind-"*.json; do
      jq -r --arg kind "$kind" --arg cutoff "$cutoff" --slurpfile cal "$calendar" '
        ($cutoff | fromdateiso8601) as $end |
        .MRData.RaceTable.Races[] as $race |
        ($cal[0].MRData.RaceTable.Races[] | select(.round == $race.round)) as $event |
        (if $kind == "results" then $event elif $kind == "qualifying" then $event.Qualifying else $event.Sprint end) as $session |
        select($session.date != null and $session.time != null) |
        select((($session.date+"T"+$session.time | fromdateiso8601) + (if $kind == "results" then 14400 elif $kind == "qualifying" then 10800 else 7200 end)) <= $end) |
        (if $kind == "results" then $race.Results[] elif $kind == "qualifying" then $race.QualifyingResults[] else $race.SprintResults[] end) |
        if $kind == "qualifying" then [$race.season,$race.round,.Driver.driverId,(.Driver.givenName+" "+.Driver.familyName),.Constructor.constructorId,.position]
        else [$race.season,$race.round,.Driver.driverId,(.Driver.givenName+" "+.Driver.familyName),.Constructor.constructorId,.position,.grid,.laps,.status,.points] end | @tsv' "$file" >> "data/$(if [[ "$kind" == sprint ]]; then echo sprints; else echo "$kind"; fi).tsv"
    done
  done
  latest=$(jq -r --arg cutoff "$cutoff" '[.MRData.RaceTable.Races[] | select((.date+"T"+.time | fromdateiso8601)+14400 <= ($cutoff|fromdateiso8601)) | .round|tonumber] | max // empty' "$calendar")
  if [[ -n "$latest" ]]; then
    fetch_pages "$season/$latest/driverstandings"
    for file in data/raw/"$season-$latest-driverstandings-"*.json; do
      jq -r '.MRData.StandingsTable.StandingsLists[] as $s | $s.DriverStandings[] | [$s.season,$s.round,.Driver.driverId,(.Driver.givenName+" "+.Driver.familyName),.Constructors[-1].constructorId,.position,.points,.wins] | @tsv' "$file" >> data/standings.tsv
    done
  fi
done
if [[ -f data/austin-qualifying.tsv ]]; then
  # Official Austin supplement; retain the same session-time cutoff as API rows.
  jq -Rr --arg cutoff "$cutoff" --slurpfile cal data/raw/2026-races-0.json '
    select(($cutoff|fromdateiso8601) >= ("2026-10-25T01:49:05Z"|fromdateiso8601)) |
    split("\t") as $r |
    ($cal[0].MRData.RaceTable.Races[] | select(.season == $r[0] and .round == $r[1])) as $e |
    select(($e.Qualifying.date+"T"+$e.Qualifying.time | fromdateiso8601)+10800 <= ($cutoff|fromdateiso8601)) | $r | @tsv' data/austin-qualifying.tsv > data/austin-qualifying.filtered.tmp
  awk -F "\t" 'FILENAME==ARGV[1] {replace[$1 FS $2]=1; next} !( ($1 FS $2) in replace)' data/austin-qualifying.filtered.tmp data/qualifying.tsv > data/qualifying.tsv.tmp
  cat data/austin-qualifying.filtered.tmp >> data/qualifying.tsv.tmp
  mv data/qualifying.tsv.tmp data/qualifying.tsv
  rm data/austin-qualifying.filtered.tmp
fi
printf 'Cutoff %s. TSV rows: ' "$cutoff"
wc -l data/events.tsv data/results.tsv data/qualifying.tsv data/sprints.tsv data/standings.tsv
