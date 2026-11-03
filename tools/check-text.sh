#!/bin/sh
# text rules for every file in the repo, the ones the per-language formatters don't cover:
# plain ascii, no trailing whitespace, a newline at the end, no tabs outside device tree
set -u
cd "$(git rev-parse --show-toplevel)" || exit 1

tab=$(printf '\t')
status=0

report() {
    echo "$file: $1"
    status=1
}

for file in $(git ls-files --cached --others --exclude-standard); do
    [ -f "$file" ] || continue # deleted but not committed yet

    if LC_ALL=C grep -qI '[^ -~[:space:]]' "$file"; then report "not plain ascii"; fi

    case "$file" in
        *.md) ;; # trailing spaces are hard line breaks here
        *) if grep -qIE '[[:space:]]+$' "$file"; then report "trailing whitespace"; fi ;;
    esac

    case "$file" in
        *.dts) ;; # device tree indents with tabs
        *) if grep -qIF "$tab" "$file"; then report "contains tabs"; fi ;;
    esac

    if [ -s "$file" ] && [ -n "$(tail -c1 "$file")" ]; then report "no newline at the end"; fi
done

exit "$status"
