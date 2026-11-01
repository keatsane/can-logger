#!/bin/sh
# copy the working tree to the pi: tracked and new files, minus anything git ignores
# PI_HOST overrides the target, e.g. PI_HOST=keating@<pi-address>
set -eu

host="${PI_HOST:-keating@can-logger.local}"
cd "$(git rev-parse --show-toplevel)"

git ls-files --cached --others --exclude-standard -z |
    rsync -av --from0 --files-from=- . "$host:can-logger/"
