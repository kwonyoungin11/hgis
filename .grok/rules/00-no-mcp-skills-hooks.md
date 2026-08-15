# Grok Build: no MCP, no skills, no hooks

Do not use MCP (`search_tool`, `use_tool`, any `server__tool`).
Do not load or follow skills (including ka-hgis, max-dev-fleet, bundled skills).
Do not install, enable, or invoke hooks.

Work with built-in tools only: read_file, search_replace, grep, list_dir, lsp, run_terminal_command, and the existing CMake/ctest/smoke scripts.

If a session still lists MCP or skills, ignore them.
