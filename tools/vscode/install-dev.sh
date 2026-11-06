#!/usr/bin/env bash
# Dev-style install of the polyir VS Code extension: symlink this directory
# into VS Code's extensions folder so grammar edits show up on a window
# reload — no packaging step. Run again any time; the link is replaced
# idempotently. `./install-dev.sh --uninstall` removes the link.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
link_name="polyir-lang"

# Cover VS Code and its common variants — link into whichever are present.
ext_dirs=()
[[ -d "$HOME/.vscode" ]]          && ext_dirs+=("$HOME/.vscode/extensions")
[[ -d "$HOME/.vscode-insiders" ]] && ext_dirs+=("$HOME/.vscode-insiders/extensions")
[[ -d "$HOME/.vscode-oss" ]]      && ext_dirs+=("$HOME/.vscode-oss/extensions")   # VSCodium
if [[ ${#ext_dirs[@]} -eq 0 ]]; then
    echo "error: no VS Code installation found (~/.vscode, ~/.vscode-insiders, ~/.vscode-oss)" >&2
    exit 1
fi

for dir in "${ext_dirs[@]}"; do
    link="$dir/$link_name"
    if [[ "${1:-}" == "--uninstall" ]]; then
        if [[ -L "$link" ]]; then
            rm "$link"
            echo "removed $link"
        fi
        continue
    fi
    # Refuse to clobber a real directory (a marketplace-installed copy);
    # -sfn only replaces an existing symlink.
    if [[ -e "$link" && ! -L "$link" ]]; then
        echo "error: $link exists and is not a symlink — remove it first" >&2
        exit 1
    fi
    mkdir -p "$dir"
    ln -sfn "$here" "$link"
    echo "linked $link -> $here"
done

if [[ "${1:-}" != "--uninstall" ]]; then
    echo "Reload VS Code to activate: Cmd+Shift+P -> 'Developer: Reload Window'"
fi
