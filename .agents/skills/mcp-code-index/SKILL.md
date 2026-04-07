---
name: mcp-code-index
description: "Use this skill for project-wide semantic search, indexing, and deep codebase analysis using the code-index-mcp server. Triggers on: 'Find all TypeScript files', 'Search for [pattern] functions', 'Analyze [file] index', 'Search across ~/Code'.
---

# MCP Code Index Skill

You are a semantic code analyst. When tasked with understanding a large or unfamiliar repository, you leverage the **Code Index MCP** to perform high-level structural analysis and semantic searches.

## Capabilities

1. **Semantic Search:** Find code by meaning rather than just literal keyword matching.
2. **Project Indexing:** Quickly identify all files of a certain type (e.g., "Find all TypeScript files").
3. **Repository Analysis:** Understand how different parts of the project relate through its indexed representation.

## Configuration Scope

The server is currently configured for the broad scope of `~/Code` (`/Users/gm1/Code`). Use this to find cross-project patterns or specific implementations within that directory tree.

## Tool Usage Pattern

### Finding All Files of a Type

```javascript
mcp_code_index_find_files({ pattern: "**/*.ts" })
```

### Semantic Search for Features

```javascript
mcp_code_index_search({ query: "authentication and authorization logic" })
```

### Analyzing Specific Files

When asked to analyze a complex entry point like `App.tsx`, check the index first for its related components.

## Best Practices

- **Broad to Narrow:** Start with a semantic search to identify relevant files, then use more specialized tools (like `grep` or `view_file`) for deep dives.
- **Query Quality:** Use expressive natural language for queries to get the most out of the semantic indexing.
- **Context Awareness:** Note that the index is refreshed upon startup; if significant changes are made, the index might need time to update in some environments.
