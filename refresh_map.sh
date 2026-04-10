#!/bin/bash
# refresh_map.sh - Update the Repomix architectural map for AI agents
# Targeted at token-efficient C++ engine logic mapping.

export PATH=$PATH:/opt/homebrew/bin

# Navigate to project root (in case called from hooks)
cd "$(dirname "$0")"

echo "[AI Workflow] Refreshing architectural map (Repomix)..."
/opt/homebrew/bin/repomix --config repomix.config.json --output repomix-output.xml

if [ $? -eq 0 ]; then
    echo "[AI Workflow] Success: repomix-output.xml generated."
else
    echo "[AI Workflow] Error: Repomix failed to generate map."
    exit 1
fi
