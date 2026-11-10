# Provenance and source preservation

This repository presents coursework by [sam710026](https://github.com/sam710026)
(蕭以勝) for National Tsing Hua University, IIS5008 Hardware Security,
Final Project, 2026.

## Original materials

The supplied `110070016_final.tar.gz` was extracted into a separate workspace.
All three C++ source files, both headers and the Makefile are preserved
byte-for-byte. The enclosed `110070016_report.pdf` is preserved as
`docs/report/report.pdf`.

The three adjacent PDFs are also preserved:

| Original file | Repository location |
| --- | --- |
| `2026_IIS5005_Hardware_Security_Final (1).pdf` | `docs/spec/assignment.pdf` |
| `IIS5008_Final_Project_Slide.pdf` | `docs/spec/project_slides.pdf` |
| `Makefile_tutorial.pdf` | `docs/notes/Makefile_tutorial.pdf` |

The assignment's title and report identify the course as IIS5008; the original
specification filename is retained in the mapping. [ORIGINAL_SOURCES.json](ORIGINAL_SOURCES.json)
records these **10 preserved public assets**, the archive hash, and the hash of
the omitted prebuilt `bin/solution`. That executable remains in the original
archive locally and is rebuilt from source for this portfolio.

The original directory and archive were not modified. Existing attribution is
retained; no repository-wide license is assigned to course materials.

## Portfolio additions

README, project rules, design/performance/provenance documents, the example,
test-scope notes and Python verification helpers were added. They do not modify
the codec, its default parameters or the original Makefile.
[SOURCES.sha256](SOURCES.sha256) covers packaged sources and execution inputs;
`python3 scripts/check_project.py` also verifies original-asset preservation.
Generated binaries, build directories and local working files are ignored by Git.

## Historical result limits

The original report records 100% correctness on public1–public4, their case-total
runtimes and average redundancy. The official evaluator, `grade.sh`, public
`input.txt`/`meta.txt` files and raw benchmark logs were not supplied. The report
also lacks compiler/machine details and a tested-source manifest. Sharing an
archive establishes common submission provenance, but does not independently
bind that performance table to the source or prebuilt binary.

The historical results are therefore attributed to the report. No official
public or hidden grading run is claimed by this repository.

## Fresh evidence

The [release evidence](evidence/release/) records a clean source build and
deterministic synthetic tests on 2026-11-11 (Asia/Taipei). Metadata identifies
compiler/Python versions, source hashes, the newly compiled binary hash and the
full trial-vector hash. Local project prefixes in published log copies are
replaced with `<project>/`; measured outcomes and values are retained.

All stimuli are synthetic. The test runner independently introduces declared
bit/byte corruptions and compares decoded bytes with the known input. All eight
failed noisy recoveries remain in the recorded results. The new test channels
do not reconstruct the unavailable official Flash evaluator.
