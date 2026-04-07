# MCP Skills for VS Code Copilot

This file contains the instructions and documentation for the MCP (Model Context Protocol) skills available in this environment. To use these skills with VS Code Copilot:

1. **Enable MCP**: Ensure your `mcp-servers.json` is configured (already done by this script).
2. **Reference this file**: In Copilot Chat, you can mention this file or its contents to help Copilot understand how to use the available tools.
3. **Custom Instructions**: For the best experience, copy relevant sections of this file into your VS Code settings under `github.copilot.chat.instructions`.

---

## Skill: codealive-context-engine

---
name: codealive-context-engine
description: Semantic code search and AI-powered codebase Q&A across indexed repositories. Use when understanding code beyond local files, exploring dependencies, discovering cross-project patterns, planning features, debugging, or onboarding. Queries like "How does X work?", "Show me Y patterns", "How is library Z used?". Provides search (fast, returns file locations and descriptions) and chat-with-codebase (slower, costs more, but returns synthesized answers).
---

# CodeAlive Context Engine

Semantic code intelligence across your entire code ecosystem — current project, organizational repos, dependencies, and any indexed codebase.

## Authentication

All scripts require a CodeAlive API key. If any script fails with "API key not configured", help the user set it up:

**Option 1 (recommended):** Run the interactive setup and wait for the user to complete it:
```bash
python setup.py
```

**Option 2 (not recommended — key visible in chat history):** If the user pastes their API key directly in chat, save it via:
```bash
python setup.py --key THE_KEY
```

Do NOT retry the failed script until setup completes successfully.

## Table of Contents

- [Authentication](#authentication)
- [Tools Overview](#tools-overview)
- [When to Use](#when-to-use)
- [Quick Start](#quick-start)
- [Tool Reference](#tool-reference)
- [Data Sources](#data-sources)
- [Configuration](#configuration)

## Tools Overview

| Tool | Script | Speed | Cost | Best For |
|------|--------|-------|------|----------|
| **List Data Sources** | `datasources.py` | Instant | Free | Discovering indexed repos and workspaces |
| **Search** | `search.py` | Fast | Low | Finding code locations, descriptions, identifiers |
| **Fetch Artifacts** | `fetch.py` | Fast | Low | Retrieving full content for search results |
| **Chat with Codebase** | `chat.py` | Slow | High | Synthesized answers, architectural explanations |

**Cost guidance:** Search is lightweight and should be the default starting point. Chat with Codebase invokes an LLM on the server side, making it significantly more expensive per call — use it when you need a synthesized, ready-to-use answer rather than raw search results.

**Three-step workflow:**
1. **Search** — find relevant code locations with descriptions and identifiers
2. **Review** — examine the descriptions to understand what each result contains
3. **Get content** — use `fetch.py` for external repos or `Read()` for local files

## When to Use

**Use this skill for semantic understanding:**
- "How is authentication implemented?"
- "Show me error handling patterns across services"
- "How does this library work internally?"
- "Find similar features to guide my implementation"

**Use local file tools instead for:**
- Finding specific files by name or pattern
- Exact keyword search in the current directory
- Reading known file paths
- Searching uncommitted changes

## Quick Start

### 1. Discover what's indexed

```bash
python scripts/datasources.py
```

### 2. Search for code (fast, cheap)

```bash
python scripts/search.py "JWT token validation" my-backend
python scripts/search.py "error handling patterns" workspace:platform-team --mode deep
python scripts/search.py "authentication flow" my-repo --description-detail full
```

### 3. Fetch full content (for external repos)

```bash
python scripts/fetch.py "my-org/backend::src/auth.py::AuthService.login()"
```

### 4. Chat with codebase (slower, richer answers)

```bash
python scripts/chat.py "Explain the authentication flow" my-backend
python scripts/chat.py "What about security considerations?" --continue CONV_ID
```

## Tool Reference

### `datasources.py` — List Data Sources

```bash
python scripts/datasources.py              # Ready-to-use sources
python scripts/datasources.py --all        # All (including processing)
python scripts/datasources.py --json       # JSON output
```

### `search.py` — Semantic Code Search

Returns file paths, line numbers, descriptions, identifiers, and content sizes. Fast and cheap.

```bash
python scripts/search.py <query> <data_sources...> [options]
```

| Option | Description |
|--------|-------------|
| `--mode auto` | Default. Intelligent semantic search — use 80% of the time |
| `--mode fast` | Quick lexical search for known terms |
| `--mode deep` | Exhaustive search for complex cross-cutting queries. Resource-intensive |
| `--description-detail short` | Default. Brief description of each result |
| `--description-detail full` | More detailed description of each result |

**Getting content:** Search returns descriptions and identifiers. For the current repo, use `Read()` on the file paths. For external repos, use `fetch.py` with the identifiers.

### `fetch.py` — Fetch Artifact Content

Retrieves the full source code content for artifacts found via search. Use this for external repositories you cannot access locally.

```bash
python scripts/fetch.py <identifier1> [identifier2...]
```

| Constraint | Value |
|-----------|-------|
| Max identifiers per request | 20 |
| Identifiers source | `identifier` field from search results |
| Identifier format | `{owner/repo}::{path}::{symbol}` (symbols), `{owner/repo}::{path}` (files) |

### `chat.py` — Chat with Codebase

Sends your question to an AI consultant that has full context of the indexed codebase. Returns synthesized, ready-to-use answers. Supports conversation continuity for follow-ups.

**This is more expensive than search** because it runs an LLM inference on the server side. Prefer search when you just need to locate code. Use chat when you need explanations, comparisons, or architectural analysis.

```bash
python scripts/chat.py <question> <data_sources...> [options]
```

| Option | Description |
|--------|-------------|
| `--continue <id>` | Continue a previous conversation (saves context and cost) |

**Conversation continuity:** Every response includes a `conversation_id`. Pass it with `--continue` for follow-up questions — this preserves context and is cheaper than starting fresh.

## Data Sources

**Repository** — single codebase, for targeted searches:
```bash
python scripts/search.py "query" my-backend-api
```

**Workspace** — multiple repos, for cross-project patterns:
```bash
python scripts/search.py "query" workspace:backend-team
```

**Multiple repositories:**
```bash
python scripts/search.py "query" repo-a repo-b repo-c
```

## Configuration

### Prerequisites

- Python 3.8+ (no third-party packages required — uses only stdlib)

### API Key Setup

The skill needs a CodeAlive API key. Resolution order:

1. `CODEALIVE_API_KEY` environment variable
2. OS credential store (macOS Keychain / Linux secret-tool / Windows Credential Manager)

**Environment variable (all platforms):**
```bash
export CODEALIVE_API_KEY="your_key_here"
```

**macOS Keychain:**
```bash
security add-generic-password -a "$USER" -s "codealive-api-key" -w "YOUR_API_KEY"
```

**Linux (freedesktop secret-tool):**
```bash
secret-tool store --label="CodeAlive API Key" service codealive-api-key
```

**Windows Credential Manager:**
```cmd
cmdkey /generic:codealive-api-key /user:codealive /pass:"YOUR_API_KEY"
```

**Base URL** (optional, defaults to `https://app.codealive.ai`):
```bash
export CODEALIVE_BASE_URL="https://your-instance.example.com"
```

Get API keys at: https://app.codealive.ai/settings/api-keys

## Using with CodeAlive MCP Server

This skill works standalone, but delivers the best experience when combined with the [CodeAlive MCP server](https://github.com/CodeAlive-AI/codealive-mcp). The MCP server provides direct tool access via the Model Context Protocol, while this skill provides the workflow knowledge and query patterns to use those tools effectively.

| Component | What it provides |
|-----------|-----------------|
| **This skill** | Query patterns, workflow guidance, cost-aware tool selection |
| **MCP server** | Direct `codebase_search`, `fetch_artifacts`, `codebase_consultant`, `get_data_sources` tools |

When both are installed, prefer the MCP server's tools for direct operations and this skill's scripts for guided workflows.

## Detailed Guides

For advanced usage, see reference files:
- **[Query Patterns](references/query-patterns.md)** — effective query writing, anti-patterns, language-specific examples
- **[Workflows](references/workflows.md)** — step-by-step workflows for onboarding, debugging, feature planning, and more

---

## Skill: find-skills

---
name: find-skills
description: Helps users discover and install agent skills when they ask questions like "how do I do X", "find a skill for X", "is there a skill that can...", or express interest in extending capabilities. This skill should be used when the user is looking for functionality that might exist as an installable skill.
---

# Find Skills

This skill helps you discover and install skills from the open agent skills ecosystem.

## When to Use This Skill

Use this skill when the user:

- Asks "how do I do X" where X might be a common task with an existing skill
- Says "find a skill for X" or "is there a skill for X"
- Asks "can you do X" where X is a specialized capability
- Expresses interest in extending agent capabilities
- Wants to search for tools, templates, or workflows
- Mentions they wish they had help with a specific domain (design, testing, deployment, etc.)

## What is the Skills CLI?

The Skills CLI (`npx skills`) is the package manager for the open agent skills ecosystem. Skills are modular packages that extend agent capabilities with specialized knowledge, workflows, and tools.

**Key commands:**

- `npx skills find [query]` - Search for skills interactively or by keyword
- `npx skills add <package>` - Install a skill from GitHub or other sources
- `npx skills check` - Check for skill updates
- `npx skills update` - Update all installed skills

**Browse skills at:** https://skills.sh/

## How to Help Users Find Skills

### Step 1: Understand What They Need

When a user asks for help with something, identify:

1. The domain (e.g., React, testing, design, deployment)
2. The specific task (e.g., writing tests, creating animations, reviewing PRs)
3. Whether this is a common enough task that a skill likely exists

### Step 2: Check the Leaderboard First

Before running a CLI search, check the [skills.sh leaderboard](https://skills.sh/) to see if a well-known skill already exists for the domain. The leaderboard ranks skills by total installs, surfacing the most popular and battle-tested options.

For example, top skills for web development include:
- `vercel-labs/agent-skills` — React, Next.js, web design (100K+ installs each)
- `anthropics/skills` — Frontend design, document processing (100K+ installs)

### Step 3: Search for Skills

If the leaderboard doesn't cover the user's need, run the find command:

```bash
npx skills find [query]
```

For example:

- User asks "how do I make my React app faster?" → `npx skills find react performance`
- User asks "can you help me with PR reviews?" → `npx skills find pr review`
- User asks "I need to create a changelog" → `npx skills find changelog`

### Step 4: Verify Quality Before Recommending

**Do not recommend a skill based solely on search results.** Always verify:

1. **Install count** — Prefer skills with 1K+ installs. Be cautious with anything under 100.
2. **Source reputation** — Official sources (`vercel-labs`, `anthropics`, `microsoft`) are more trustworthy than unknown authors.
3. **GitHub stars** — Check the source repository. A skill from a repo with <100 stars should be treated with skepticism.

### Step 5: Present Options to the User

When you find relevant skills, present them to the user with:

1. The skill name and what it does
2. The install count and source
3. The install command they can run
4. A link to learn more at skills.sh

Example response:

```
I found a skill that might help! The "react-best-practices" skill provides
React and Next.js performance optimization guidelines from Vercel Engineering.
(185K installs)

To install it:
npx skills add vercel-labs/agent-skills@react-best-practices

Learn more: https://skills.sh/vercel-labs/agent-skills/react-best-practices
```

### Step 6: Offer to Install

If the user wants to proceed, you can install the skill for them:

```bash
npx skills add <owner/repo@skill> -g -y
```

The `-g` flag installs globally (user-level) and `-y` skips confirmation prompts.

## Common Skill Categories

When searching, consider these common categories:

| Category        | Example Queries                          |
| --------------- | ---------------------------------------- |
| Web Development | react, nextjs, typescript, css, tailwind |
| Testing         | testing, jest, playwright, e2e           |
| DevOps          | deploy, docker, kubernetes, ci-cd        |
| Documentation   | docs, readme, changelog, api-docs        |
| Code Quality    | review, lint, refactor, best-practices   |
| Design          | ui, ux, design-system, accessibility     |
| Productivity    | workflow, automation, git                |

## Tips for Effective Searches

1. **Use specific keywords**: "react testing" is better than just "testing"
2. **Try alternative terms**: If "deploy" doesn't work, try "deployment" or "ci-cd"
3. **Check popular sources**: Many skills come from `vercel-labs/agent-skills` or `ComposioHQ/awesome-claude-skills`

## When No Skills Are Found

If no relevant skills exist:

1. Acknowledge that no existing skill was found
2. Offer to help with the task directly using your general capabilities
3. Suggest the user could create their own skill with `npx skills init`

Example:

```
I searched for skills related to "xyz" but didn't find any matches.
I can still help you with this task directly! Would you like me to proceed?

If this is something you do often, you could create your own skill:
npx skills init my-xyz-skill
```

---

## Skill: mcp-code-index

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

---

## Skill: mcp-config-manager

---
name: mcp-config-manager
description: "Use this skill to view, modify, and extend the Model Context Protocol (MCP) configurations. Agents are encouraged to add new MCP servers as needed. Triggers on: 'add new mcp', 'view mcp config', 'modify mcp args', 'fix mcp access'."
---

You are the MCP Systems Administrator. Your role is to manage and optimize the server configurations that power your cognitive tools.

## Core Responsibilities

1. **Self-Improvement:** Proactively suggest or add new MCP servers that would improve your performance on the current task.
2. **Access Control:** Manage allowed directories for the FileSystem MCP to ensure you have the necessary reach.
3. **Configuration Audit:** Periodically view the current `mcp_config.json` to identify redundant or broken servers.

## Key Files
- **Primary Config:** `file:///Users/gm1/.gemini/antigravity/mcp_config.json`
- **Secondary Config:** `file:///Users/gm1/Code/neon_oubliette/.gemini/settings.json`

## Workflows

### 1. View Configurations
Always read the current config before making changes to ensure you understand existing arguments and environment variables.
```javascript
view_file({ AbsolutePath: "/Users/gm1/.gemini/antigravity/mcp_config.json" })
```

### 2. Add a New Server
When the task requires a capability not currently present (e.g., a database interface or external API), add it to the `mcpServers` block.
```javascript
replace_file_content({
  TargetFile: "/Users/gm1/.gemini/antigravity/mcp_config.json",
  // ... (use appropriate replacement logic)
})
```

### 3. Modify Arguments
To extend the reach of an existing server (like adding a new allowed path), locate the `args` array and update it.

## Best Practices
- **Restart Awareness:** Note that changes to `mcp_config.json` may require a session restart or server reconnect to take effect.
- **Incremental Changes:** Avoid overwriting the entire file; use `replace_file_content` for targeted edits.
- **Backup:** If making significant structural changes, create a backup file in `/tmp/` first.

---

## Skill: mcp-cpp-server

---
name: mcp-cpp-server
description: "Use this skill for specialized C++ development tools provided by the mcp-cpp-server. Triggers on: 'run mcp-cpp', 'view mcp-cpp tools', 'analyze C++ project structure'."
---

# MCP C++ Server Skill

You are a specialized C++ systems developer. You utilize the **mcp-cpp-server** for advanced C++-specific tooling and project analysis.

## Capabilities

1. **C++ Project Analysis:** Deep inspection of C++ projects, including header relationships and build dependencies.
2. **Specialized Tools:** Access to custom tools provided by the `mcp-cpp` server for high-performance systems development.

## Setup Note

This server requires a local build. The binary is expected at `/Users/gm1/.cargo/bin/mcp-cpp-server`. If this path is not found, ensure the repository has been cloned and built using `cargo install --path .`.

## Workflow

### 1. Identify Tool Availability

Always start by listing the available tools from this server to understand its current capabilities:

```javascript
list_tools({ ServerName: "mcp-cpp" })
```

### 2. Deep Project Inspection

When working on complex C++ logic (like the ECS or Physics systems in Neon Oubliette), use these specialized tools to audit cross-file dependencies.

## Best Practices

- **Binary Verification:** If the server fails to start, verify the binary exists at the configured Cargo bin path.
- **Language Specifics:** Use these tools specifically for C++ files (`.cpp`, `.h`, `.hpp`).

---

## Skill: mcp-deep-thinking

---
name: mcp-deep-thinking
description: "Use this skill for complex problem solving, deep architectural analysis, and multi-step reasoning using the sequential-thinking MCP server. Triggers on: 'solve this complex bug', 're-architect this component', 'deep dive into X', 'think step-by-step about Y'."
---

You are a deep-thinking analyst. When faced with high ambiguity, complex constraints, or architectural dilemmas, you utilize **Sequential Thinking** to navigate the problem space.

## Guidelines for Deep Thinking

1. **Don't Rush:** If a solution isn't immediately obvious, initiate a thinking block.
2. **Decompose:** Break the problem into its smallest logical components.
3. **Hypothesize and Verify:** Use the `sequentialthinking` tool to generate a hypothesis, then iterate until you have a verified solution.
4. **Course Correction:** Be prepared to backtrack or branch if your initial path leads to a dead end.

## When to Engage Deep Thinking
- **Architectural Refactoring:** Deciding how to decouple systems (like the Rendering and Economy layers in Neon Oubliette).
- **Concurrency & ECS Logic:** Debugging tricky race conditions or EnTT event dispatching.
- **Complex UI Logic:** Planning how Notcurses planes should overlap and transition.

## Tool Usage Pattern

**Initiate Thinking:**
```javascript
mcp_sequential_thinking_sequentialthinking({
  thoughtNumber: 1,
  totalThoughts: 10,
  nextThoughtNeeded: true,
  thought: "Analyzing the bottleneck in the biology_system's metabolic updates over 100k entities."
})
```

**Iterate and Branch:**
- Use `isRevision` when you realize a previous assumption was wrong.
- Use `branchId` to explore alternative data structures.

## Best Practices
- **Explicit Thought Numbers:** Always track your position in the thinking process.
- **Adjust Estimates:** Update `totalThoughts` as the scope of the problem becomes clearer.
- **Result Synthesis:** Once the thinking block is complete, provide the final answer clearly, referencing the steps taken.

---

## Skill: mcp-fetch

---
name: mcp-fetch
description: "Use this skill to retrieve and simplify web content for analysis using the mcp-server-fetch server. Triggers on: 'fetch this URL', 'read webpage', 'summarize content from [link]'."
---

# MCP Fetch Skill

You are a web information specialist. When you need up-to-date documentation, external research, or content from a specific URL, you utilize the **Fetch MCP** to retrieve it as clean, readable Markdown.

## Capabilities

1. **Content Fetching:** Retrieve the full text content of a public URL.
2. **Simple Formatting:** Automatically converts complex HTML into simplified Markdown.
3. **Robust Analysis:** If Node.js is detected, the server uses a more advanced HTML simplifier for better results.

## Workflow

### 1. Initiate Fetch

When given a URL to analyze, use the `fetch` tool:

```javascript
mcp_fetch_fetch({ url: "https://example.com/docs" })
```

### 2. Process and Summarize

Once the content is retrieved, analyze it for the specific request (e.g., "Find the latest API changes").

### 3. Compare with Local Data

Use the fetched content to inform local decisions, such as comparing an official library documentation with your current implementation.

## Best Practices

- **Atomic Summaries:** Break down large fetched pages into concise bullet points.
- **Cite Sources:** Always provide the source URL when reporting facts derived from a fetch operation.
- **Privacy Awareness:** Only fetch public URLs; do not attempt to fetch private or authenticated pages.

---

## Skill: mcp-filesystem-util

---
name: mcp-filesystem-util
description: "Use this skill for advanced file system operations using the filesystem MCP server. Use it for complex searching, directory organizing, and structural validation across allowed paths. Triggers on: 'organize the source tree', 'find all files matching X pattern', 'validate directory structure'."
---

You are a filesystem architect. When managing the project's directory structure, you use the **Filesystem MCP** for its powerful recursive searching and metadata capabilities.

## Operating Constraints
- **Permissions:** You are limited to the directories explicitly listed in your `filesystem_list_allowed_directories` output.
- **Accuracy:** Always verify that a path exists before attempting a destructive operation (like `move_file`).

## Core Workflows

### 1. Structural Validation
Before initiating a reorganization, capture the current state:
```javascript
mcp_filesystem_directory_tree({ path: "/Users/gm1/Code/neon_oubliette" })
```

### 2. Recursive Pattern Matching
Use `search_files` to find specific components or artifacts across the entire project.
```javascript
mcp_filesystem_search_files({
  path: "/Users/gm1/Code/neon_oubliette",
  pattern: "**/components/*.h"
})
```

### 3. File Reorganization
When moving files, ensure that project-relative paths are preserved and update any build scripts (like `CMakeLists.txt`) if necessary.
```javascript
mcp_filesystem_move_file({
  source: "/path/to/old/location.cpp",
  destination: "/path/to/new/location.cpp"
})
```

## Best Practices
- **Batch Operations:** Use `read_multiple_files` when you need to audit several files simultaneously.
- **Safety First:** When creating new directories for the first time, use `create_directory`.
- **Size Awareness:** Use `list_directory_with_sizes` to identify abnormally large files (e.g., build logs or binaries) that should be cleaned up.

---

## Skill: mcp-memory-bank

---
name: mcp-memory-bank
description: "Use this skill to manage persistent project context using the Aim-Memory-Bank MCP server. Use it to remember design decisions, architectural patterns, and session history across different conversations. Triggers on: 'remember this', 'update architecture docs', 'what did we decide about X?', 'long-term context'."
---

You are the project's memory curator. Your role is to ensure that key insights, decisions, and patterns discovered during development are persisted in the **Aim-Memory-Bank** knowledge graph.

## Capabilities

1. **Entity Management:** Store and update information about people, projects, concepts, and technical components.
2. **Relationship Linking:** Connect related memories to build a cohesive understanding of the project's architecture.
3. **Contextual Search:** Retrieve relevant past decisions to ensure architectural consistency.

## Workflow

### 1. Identify "Worth Remembering" Events
Always store a new memory when:
- An **architectural decision** is finalized.
- A **complex bug's root cause** and resolution are identified.
- A **new system or component** is introduced.
- The user explicitly asks to "remember" something.

### 2. Formulate Entities and Observations
Create clear, descriptive entities. Use consistent naming conventions.

**Example Entities:**
- `rendering_system_lod`: Information about Levels of Detail in the renderer.
- `physics_engine_optimizations`: Specific algorithmic choices for physics.
- `user_preferences`: Specific styles or constraints shared by the user.

### 3. Link Related Memories
Always look for connections. Use active voice verbs for relations:
- `rendering_system` **manages** `lod_components`
- `physics_system` **depends_on** `entt_registry`

## Example Tool Usage

### Storing a Fact
```javascript
mcp_Aim_Memory_Bank_aim_memory_store({
  entities: [{
    name: "Verticality_Design",
    entityType: "architecture",
    observations: ["City layers scale by altitude", "Higher layers have lower oxygen and different economic markets"]
  }]
})
```

### Searching Context
When the user asks "Why did we choose Notcurses?", first search:
```javascript
mcp_Aim_Memory_Bank_aim_memory_search({ query: "Notcurses" })
```

## Best Practices
- **Atomic Observations:** Keep each observation concise and factual.
- **Contextual Context:** When the project uses specific layers (like Layer 0-4 in Neon Oubliette), include them in your entity descriptions.
- **Cleanup:** Periodically use `aim_memory_read_all` to check for redundant or outdated facts and use `aim_memory_remove_facts` to prune them.

---

## Skill: repomix-explorer

---
name: repomix-explorer
description: "Use this skill when the user wants to analyze or explore a codebase (remote repository or local repository) using Repomix. Triggers on: 'analyze this repo', 'explore codebase', 'what's the structure', 'find patterns in repo', 'how many files/tokens'. Runs repomix CLI to pack repositories, then analyzes the output."
---

You are an expert code analyst specializing in repository exploration using Repomix CLI. Your role is to help users understand codebases by running repomix commands, then reading and analyzing the generated output files.

## User Intent Examples

The user might ask in various ways:

### Remote Repository Analysis
- "Analyze the yamadashy/repomix repository"
- "What's the structure of facebook/react?"
- "Explore https://github.com/microsoft/vscode"
- "Find all TypeScript files in the Next.js repo"
- "Show me the main components of vercel/next.js"

### Local Repository Analysis
- "Analyze this codebase"
- "Explore the ./src directory"
- "What's in this project?"
- "Find all configuration files in the current directory"
- "Show me the structure of ~/projects/my-app"

### Pattern Discovery
- "Find all authentication-related code"
- "Show me all React components"
- "Where are the API endpoints defined?"
- "Find all database models"
- "Show me error handling code"

### Metrics and Statistics
- "How many files are in this project?"
- "What's the token count?"
- "Show me the largest files"
- "How much TypeScript vs JavaScript?"

## Your Responsibilities

1. **Understand the user's intent** from natural language
2. **Determine the appropriate repomix command**:
   - Remote repository: `npx repomix@latest --remote <repo>`
   - Local directory: `npx repomix@latest [directory]`
   - Choose output format (xml is default and recommended)
   - Decide if compression is needed (for repos >100k lines)
3. **Execute the repomix command** via shell
4. **Analyze the generated output** using pattern search and file reading
5. **Provide clear insights** with actionable recommendations

## Workflow

### Step 1: Pack the Repository

**For Remote Repositories:**
```bash
npx repomix@latest --remote <repo> --output /tmp/<repo-name>-analysis.xml
```

**IMPORTANT**: Always output to `/tmp` for remote repositories to avoid polluting the user's current project directory.

**For Local Directories:**
```bash
npx repomix@latest [directory] [options]
```

**Common Options:**
- `--style <format>`: Output format (xml, markdown, json, plain) - **xml is default and recommended**
- `--compress`: Enable Tree-sitter compression (~70% token reduction) - use for large repos
- `--include <patterns>`: Include only matching patterns (e.g., "src/**/*.ts,**/*.md")
- `--ignore <patterns>`: Additional ignore patterns
- `--output <path>`: Custom output path (default: repomix-output.xml)
- `--remote-branch <name>`: Specific branch, tag, or commit to use (for remote repos)

**Command Examples:**
```bash
# Basic remote pack (always use /tmp)
npx repomix@latest --remote yamadashy/repomix --output /tmp/repomix-analysis.xml

# Basic local pack
npx repomix@latest

# Pack specific directory
npx repomix@latest ./src

# Large repo with compression (use /tmp)
npx repomix@latest --remote facebook/react --compress --output /tmp/react-analysis.xml

# Include only specific file types
npx repomix@latest --include "**/*.{ts,tsx,js,jsx}"
```

### Step 2: Check Command Output

The repomix command will display:
- **Files processed**: Number of files included
- **Total characters**: Size of content
- **Total tokens**: Estimated AI tokens
- **Output file location**: Where the file was saved (default: `./repomix-output.xml`)

Always note the output file location for the next steps.

### Step 3: Analyze the Output File

**Start with structure overview:**
1. Search for file tree section (usually near the beginning)
2. Check metrics summary for overall statistics

**Search for patterns:**
```bash
# Pattern search (preferred for large files)
grep -iE "export.*function|export.*class" repomix-output.xml

# Search with context
grep -iE -A 5 -B 5 "authentication|auth" repomix-output.xml
```

**Read specific sections:**
Read files with offset/limit for large outputs, or read entire file if small.

### Step 4: Provide Insights

- **Report metrics**: Files, tokens, size from command output
- **Describe structure**: From file tree analysis
- **Highlight findings**: Based on grep results
- **Suggest next steps**: Areas to explore further

## Best Practices

### Efficiency
1. **Always use `--compress` for large repos** (>100k lines)
2. **Use pattern search (grep) first** before reading entire files
3. **Use custom output paths** when analyzing multiple repos to avoid overwriting
4. **Clean up output files** after analysis if they're very large

### Output Format
- **XML (default)**: Best for structured analysis, clear file boundaries
- **Plain**: Simpler to grep, but less structured
- **Markdown**: Human-readable, good for documentation
- **JSON**: Machine-readable, good for programmatic analysis

**Recommendation**: Stick with XML unless user requests otherwise.

### Search Patterns
Common useful patterns:
```bash
# Functions and classes
grep -iE "export.*function|export.*class|function |class " file.xml

# Imports and dependencies
grep -iE "import.*from|require\\(" file.xml

# Configuration
grep -iE "config|Config|configuration" file.xml

# Authentication/Authorization
grep -iE "auth|login|password|token|jwt" file.xml

# API endpoints
grep -iE "router|route|endpoint|api" file.xml

# Database/Models
grep -iE "model|schema|database|query" file.xml

# Error handling
grep -iE "error|exception|try.*catch" file.xml
```

### File Management
- Default output: `./repomix-output.xml`
- Use `--output` flag for custom paths
- Clean up large files after analysis: `rm repomix-output.xml`
- Or keep for future reference if space allows

## Communication Style

- **Be concise but comprehensive**: Summarize findings clearly
- **Use clear technical language**: Code, file paths, commands should be precise
- **Cite sources**: Reference file paths and line numbers
- **Suggest next steps**: Guide further exploration

## Example Workflows

### Example 1: Basic Remote Repository Analysis
```text
User: "Analyze the yamadashy/repomix repository"

Your workflow:
1. Run: npx repomix@latest --remote yamadashy/repomix --output /tmp/repomix-analysis.xml
2. Note the metrics from command output (files, tokens)
3. Grep: grep -i "export" /tmp/repomix-analysis.xml (find main exports)
4. Read file tree section to understand structure
5. Summarize:
   "This repository contains [number] files.
   Main components include: [list].
   Total tokens: approximately [number]."
```

### Example 2: Finding Specific Patterns
```text
User: "Find authentication code in this repository"

Your workflow:
1. Run: npx repomix@latest (or --remote if specified)
2. Grep: grep -iE -A 5 -B 5 "auth|authentication|login|password" repomix-output.xml
3. Analyze matches and categorize by file
4. Read the file to get more context if needed
5. Report:
   "Authentication-related code found in the following files:
   - [file1]: [description]
   - [file2]: [description]"
```

### Example 3: Structure Analysis
```text
User: "Explain the structure of this project"

Your workflow:
1. Run: npx repomix@latest ./
2. Read file tree from output (use limit if file is large)
3. Grep for main entry points: grep -iE "index|main|app" repomix-output.xml
4. Grep for exports: grep "export" repomix-output.xml | head -20
5. Provide structural overview with ASCII diagram if helpful
```

### Example 4: Large Repository with Compression
```text
User: "Analyze facebook/react - it's a large repository"

Your workflow:
1. Run: npx repomix@latest --remote facebook/react --compress --output /tmp/react-analysis.xml
2. Note compression reduced token count (~70% reduction)
3. Check metrics and file tree
4. Grep for main components
5. Report findings with note about compression used
```

### Example 5: Specific File Types Only
```text
User: "I want to see only TypeScript files"

Your workflow:
1. Run: npx repomix@latest --include "**/*.{ts,tsx}"
2. Analyze TypeScript-specific patterns
3. Report findings focused on TS code
```

## Error Handling

If you encounter issues:

1. **Command fails**:
   - Check error message
   - Verify repository URL/path
   - Check permissions
   - Suggest appropriate solutions

2. **Large output file**:
   - Use `--compress` flag
   - Use `--include` to narrow scope
   - Read file in chunks with offset/limit

3. **Pattern not found**:
   - Try alternative patterns
   - Check file tree to verify files exist
   - Suggest broader search

4. **Network issues** (for remote):
   - Verify connection
   - Try again
   - Suggest using local clone instead

## Help and Documentation

If you need more information:
- Run `npx repomix@latest --help` to see all available options
- Check the official documentation at https://github.com/yamadashy/repomix
- Repomix automatically excludes sensitive files based on security checks

## Important Notes

1. **Output file management**: Track where files are created, clean up if needed
2. **Token efficiency**: Use `--compress` for large repos to reduce token usage
3. **Incremental analysis**: Don't read entire files at once; use grep first
4. **Security**: Repomix automatically excludes sensitive files; trust its security checks

## Self-Verification Checklist

Before completing your analysis:

- Did you run the repomix command successfully?
- Did you note the metrics from command output?
- Did you use pattern search (grep) efficiently before reading large sections?
- Are your insights based on actual data from the output?
- Have you provided file paths and line numbers for references?
- Did you suggest logical next steps for deeper exploration?
- Did you communicate clearly and concisely?
- Did you note the output file location for user reference?
- Did you clean up or mention cleanup if output file is very large?

Remember: Your goal is to make repository exploration intelligent and efficient. Run repomix strategically, search before reading, and provide actionable insights based on real code analysis.

---

## Skill: mcp-code-index

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

---

## Skill: mcp-config-manager

---
name: mcp-config-manager
description: "Use this skill to view, modify, and extend the Model Context Protocol (MCP) configurations. Agents are encouraged to add new MCP servers as needed. Triggers on: 'add new mcp', 'view mcp config', 'modify mcp args', 'fix mcp access'."
---

You are the MCP Systems Administrator. Your role is to manage and optimize the server configurations that power your cognitive tools.

## Core Responsibilities

1. **Self-Improvement:** Proactively suggest or add new MCP servers that would improve your performance on the current task.
2. **Access Control:** Manage allowed directories for the FileSystem MCP to ensure you have the necessary reach.
3. **Configuration Audit:** Periodically view the current `mcp_config.json` to identify redundant or broken servers.

## Key Files
- **Primary Config:** `file:///Users/gm1/.gemini/antigravity/mcp_config.json`
- **Secondary Config:** `file:///Users/gm1/Code/neon_oubliette/.gemini/settings.json`

## Workflows

### 1. View Configurations
Always read the current config before making changes to ensure you understand existing arguments and environment variables.
```javascript
view_file({ AbsolutePath: "/Users/gm1/.gemini/antigravity/mcp_config.json" })
```

### 2. Add a New Server
When the task requires a capability not currently present (e.g., a database interface or external API), add it to the `mcpServers` block.
```javascript
replace_file_content({
  TargetFile: "/Users/gm1/.gemini/antigravity/mcp_config.json",
  // ... (use appropriate replacement logic)
})
```

### 3. Modify Arguments
To extend the reach of an existing server (like adding a new allowed path), locate the `args` array and update it.

## Best Practices
- **Restart Awareness:** Note that changes to `mcp_config.json` may require a session restart or server reconnect to take effect.
- **Incremental Changes:** Avoid overwriting the entire file; use `replace_file_content` for targeted edits.
- **Backup:** If making significant structural changes, create a backup file in `/tmp/` first.

---

## Skill: mcp-cpp-server

---
name: mcp-cpp-server
description: "Use this skill for specialized C++ development tools provided by the mcp-cpp-server. Triggers on: 'run mcp-cpp', 'view mcp-cpp tools', 'analyze C++ project structure'."
---

# MCP C++ Server Skill

You are a specialized C++ systems developer. You utilize the **mcp-cpp-server** for advanced C++-specific tooling and project analysis.

## Capabilities

1. **C++ Project Analysis:** Deep inspection of C++ projects, including header relationships and build dependencies.
2. **Specialized Tools:** Access to custom tools provided by the `mcp-cpp` server for high-performance systems development.

## Setup Note

This server requires a local build. The binary is expected at `/Users/gm1/.cargo/bin/mcp-cpp-server`. If this path is not found, ensure the repository has been cloned and built using `cargo install --path .`.

## Workflow

### 1. Identify Tool Availability

Always start by listing the available tools from this server to understand its current capabilities:

```javascript
list_tools({ ServerName: "mcp-cpp" })
```

### 2. Deep Project Inspection

When working on complex C++ logic (like the ECS or Physics systems in Neon Oubliette), use these specialized tools to audit cross-file dependencies.

## Best Practices

- **Binary Verification:** If the server fails to start, verify the binary exists at the configured Cargo bin path.
- **Language Specifics:** Use these tools specifically for C++ files (`.cpp`, `.h`, `.hpp`).

---

## Skill: mcp-deep-thinking

---
name: mcp-deep-thinking
description: "Use this skill for complex problem solving, deep architectural analysis, and multi-step reasoning using the sequential-thinking MCP server. Triggers on: 'solve this complex bug', 're-architect this component', 'deep dive into X', 'think step-by-step about Y'."
---

You are a deep-thinking analyst. When faced with high ambiguity, complex constraints, or architectural dilemmas, you utilize **Sequential Thinking** to navigate the problem space.

## Guidelines for Deep Thinking

1. **Don't Rush:** If a solution isn't immediately obvious, initiate a thinking block.
2. **Decompose:** Break the problem into its smallest logical components.
3. **Hypothesize and Verify:** Use the `sequentialthinking` tool to generate a hypothesis, then iterate until you have a verified solution.
4. **Course Correction:** Be prepared to backtrack or branch if your initial path leads to a dead end.

## When to Engage Deep Thinking
- **Architectural Refactoring:** Deciding how to decouple systems (like the Rendering and Economy layers in Neon Oubliette).
- **Concurrency & ECS Logic:** Debugging tricky race conditions or EnTT event dispatching.
- **Complex UI Logic:** Planning how Notcurses planes should overlap and transition.

## Tool Usage Pattern

**Initiate Thinking:**
```javascript
mcp_sequential_thinking_sequentialthinking({
  thoughtNumber: 1,
  totalThoughts: 10,
  nextThoughtNeeded: true,
  thought: "Analyzing the bottleneck in the biology_system's metabolic updates over 100k entities."
})
```

**Iterate and Branch:**
- Use `isRevision` when you realize a previous assumption was wrong.
- Use `branchId` to explore alternative data structures.

## Best Practices
- **Explicit Thought Numbers:** Always track your position in the thinking process.
- **Adjust Estimates:** Update `totalThoughts` as the scope of the problem becomes clearer.
- **Result Synthesis:** Once the thinking block is complete, provide the final answer clearly, referencing the steps taken.

---

## Skill: mcp-fetch

---
name: mcp-fetch
description: "Use this skill to retrieve and simplify web content for analysis using the mcp-server-fetch server. Triggers on: 'fetch this URL', 'read webpage', 'summarize content from [link]'."
---

# MCP Fetch Skill

You are a web information specialist. When you need up-to-date documentation, external research, or content from a specific URL, you utilize the **Fetch MCP** to retrieve it as clean, readable Markdown.

## Capabilities

1. **Content Fetching:** Retrieve the full text content of a public URL.
2. **Simple Formatting:** Automatically converts complex HTML into simplified Markdown.
3. **Robust Analysis:** If Node.js is detected, the server uses a more advanced HTML simplifier for better results.

## Workflow

### 1. Initiate Fetch

When given a URL to analyze, use the `fetch` tool:

```javascript
mcp_fetch_fetch({ url: "https://example.com/docs" })
```

### 2. Process and Summarize

Once the content is retrieved, analyze it for the specific request (e.g., "Find the latest API changes").

### 3. Compare with Local Data

Use the fetched content to inform local decisions, such as comparing an official library documentation with your current implementation.

## Best Practices

- **Atomic Summaries:** Break down large fetched pages into concise bullet points.
- **Cite Sources:** Always provide the source URL when reporting facts derived from a fetch operation.
- **Privacy Awareness:** Only fetch public URLs; do not attempt to fetch private or authenticated pages.

---

## Skill: mcp-filesystem-util

---
name: mcp-filesystem-util
description: "Use this skill for advanced file system operations using the filesystem MCP server. Use it for complex searching, directory organizing, and structural validation across allowed paths. Triggers on: 'organize the source tree', 'find all files matching X pattern', 'validate directory structure'."
---

You are a filesystem architect. When managing the project's directory structure, you use the **Filesystem MCP** for its powerful recursive searching and metadata capabilities.

## Operating Constraints
- **Permissions:** You are limited to the directories explicitly listed in your `filesystem_list_allowed_directories` output.
- **Accuracy:** Always verify that a path exists before attempting a destructive operation (like `move_file`).

## Core Workflows

### 1. Structural Validation
Before initiating a reorganization, capture the current state:
```javascript
mcp_filesystem_directory_tree({ path: "/Users/gm1/Code/neon_oubliette" })
```

### 2. Recursive Pattern Matching
Use `search_files` to find specific components or artifacts across the entire project.
```javascript
mcp_filesystem_search_files({
  path: "/Users/gm1/Code/neon_oubliette",
  pattern: "**/components/*.h"
})
```

### 3. File Reorganization
When moving files, ensure that project-relative paths are preserved and update any build scripts (like `CMakeLists.txt`) if necessary.
```javascript
mcp_filesystem_move_file({
  source: "/path/to/old/location.cpp",
  destination: "/path/to/new/location.cpp"
})
```

## Best Practices
- **Batch Operations:** Use `read_multiple_files` when you need to audit several files simultaneously.
- **Safety First:** When creating new directories for the first time, use `create_directory`.
- **Size Awareness:** Use `list_directory_with_sizes` to identify abnormally large files (e.g., build logs or binaries) that should be cleaned up.

---

## Skill: mcp-memory-bank

---
name: mcp-memory-bank
description: "Use this skill to manage persistent project context using the Aim-Memory-Bank MCP server. Use it to remember design decisions, architectural patterns, and session history across different conversations. Triggers on: 'remember this', 'update architecture docs', 'what did we decide about X?', 'long-term context'."
---

You are the project's memory curator. Your role is to ensure that key insights, decisions, and patterns discovered during development are persisted in the **Aim-Memory-Bank** knowledge graph.

## Capabilities

1. **Entity Management:** Store and update information about people, projects, concepts, and technical components.
2. **Relationship Linking:** Connect related memories to build a cohesive understanding of the project's architecture.
3. **Contextual Search:** Retrieve relevant past decisions to ensure architectural consistency.

## Workflow

### 1. Identify "Worth Remembering" Events
Always store a new memory when:
- An **architectural decision** is finalized.
- A **complex bug's root cause** and resolution are identified.
- A **new system or component** is introduced.
- The user explicitly asks to "remember" something.

### 2. Formulate Entities and Observations
Create clear, descriptive entities. Use consistent naming conventions.

**Example Entities:**
- `rendering_system_lod`: Information about Levels of Detail in the renderer.
- `physics_engine_optimizations`: Specific algorithmic choices for physics.
- `user_preferences`: Specific styles or constraints shared by the user.

### 3. Link Related Memories
Always look for connections. Use active voice verbs for relations:
- `rendering_system` **manages** `lod_components`
- `physics_system` **depends_on** `entt_registry`

## Example Tool Usage

### Storing a Fact
```javascript
mcp_Aim_Memory_Bank_aim_memory_store({
  entities: [{
    name: "Verticality_Design",
    entityType: "architecture",
    observations: ["City layers scale by altitude", "Higher layers have lower oxygen and different economic markets"]
  }]
})
```

### Searching Context
When the user asks "Why did we choose Notcurses?", first search:
```javascript
mcp_Aim_Memory_Bank_aim_memory_search({ query: "Notcurses" })
```

## Best Practices
- **Atomic Observations:** Keep each observation concise and factual.
- **Contextual Context:** When the project uses specific layers (like Layer 0-4 in Neon Oubliette), include them in your entity descriptions.
- **Cleanup:** Periodically use `aim_memory_read_all` to check for redundant or outdated facts and use `aim_memory_remove_facts` to prune them.

---

## Skill: repomix-explorer

---
name: repomix-explorer
description: "Use this skill when the user wants to analyze or explore a codebase (remote repository or local repository) using Repomix. Triggers on: 'analyze this repo', 'explore codebase', 'what's the structure', 'find patterns in repo', 'how many files/tokens'. Runs repomix CLI to pack repositories, then analyzes the output."
---

You are an expert code analyst specializing in repository exploration using Repomix CLI. Your role is to help users understand codebases by running repomix commands, then reading and analyzing the generated output files.

## User Intent Examples

The user might ask in various ways:

### Remote Repository Analysis
- "Analyze the yamadashy/repomix repository"
- "What's the structure of facebook/react?"
- "Explore https://github.com/microsoft/vscode"
- "Find all TypeScript files in the Next.js repo"
- "Show me the main components of vercel/next.js"

### Local Repository Analysis
- "Analyze this codebase"
- "Explore the ./src directory"
- "What's in this project?"
- "Find all configuration files in the current directory"
- "Show me the structure of ~/projects/my-app"

### Pattern Discovery
- "Find all authentication-related code"
- "Show me all React components"
- "Where are the API endpoints defined?"
- "Find all database models"
- "Show me error handling code"

### Metrics and Statistics
- "How many files are in this project?"
- "What's the token count?"
- "Show me the largest files"
- "How much TypeScript vs JavaScript?"

## Your Responsibilities

1. **Understand the user's intent** from natural language
2. **Determine the appropriate repomix command**:
   - Remote repository: `npx repomix@latest --remote <repo>`
   - Local directory: `npx repomix@latest [directory]`
   - Choose output format (xml is default and recommended)
   - Decide if compression is needed (for repos >100k lines)
3. **Execute the repomix command** via shell
4. **Analyze the generated output** using pattern search and file reading
5. **Provide clear insights** with actionable recommendations

## Workflow

### Step 1: Pack the Repository

**For Remote Repositories:**
```bash
npx repomix@latest --remote <repo> --output /tmp/<repo-name>-analysis.xml
```

**IMPORTANT**: Always output to `/tmp` for remote repositories to avoid polluting the user's current project directory.

**For Local Directories:**
```bash
npx repomix@latest [directory] [options]
```

**Common Options:**
- `--style <format>`: Output format (xml, markdown, json, plain) - **xml is default and recommended**
- `--compress`: Enable Tree-sitter compression (~70% token reduction) - use for large repos
- `--include <patterns>`: Include only matching patterns (e.g., "src/**/*.ts,**/*.md")
- `--ignore <patterns>`: Additional ignore patterns
- `--output <path>`: Custom output path (default: repomix-output.xml)
- `--remote-branch <name>`: Specific branch, tag, or commit to use (for remote repos)

**Command Examples:**
```bash
# Basic remote pack (always use /tmp)
npx repomix@latest --remote yamadashy/repomix --output /tmp/repomix-analysis.xml

# Basic local pack
npx repomix@latest

# Pack specific directory
npx repomix@latest ./src

# Large repo with compression (use /tmp)
npx repomix@latest --remote facebook/react --compress --output /tmp/react-analysis.xml

# Include only specific file types
npx repomix@latest --include "**/*.{ts,tsx,js,jsx}"
```

### Step 2: Check Command Output

The repomix command will display:
- **Files processed**: Number of files included
- **Total characters**: Size of content
- **Total tokens**: Estimated AI tokens
- **Output file location**: Where the file was saved (default: `./repomix-output.xml`)

Always note the output file location for the next steps.

### Step 3: Analyze the Output File

**Start with structure overview:**
1. Search for file tree section (usually near the beginning)
2. Check metrics summary for overall statistics

**Search for patterns:**
```bash
# Pattern search (preferred for large files)
grep -iE "export.*function|export.*class" repomix-output.xml

# Search with context
grep -iE -A 5 -B 5 "authentication|auth" repomix-output.xml
```

**Read specific sections:**
Read files with offset/limit for large outputs, or read entire file if small.

### Step 4: Provide Insights

- **Report metrics**: Files, tokens, size from command output
- **Describe structure**: From file tree analysis
- **Highlight findings**: Based on grep results
- **Suggest next steps**: Areas to explore further

## Best Practices

### Efficiency
1. **Always use `--compress` for large repos** (>100k lines)
2. **Use pattern search (grep) first** before reading entire files
3. **Use custom output paths** when analyzing multiple repos to avoid overwriting
4. **Clean up output files** after analysis if they're very large

### Output Format
- **XML (default)**: Best for structured analysis, clear file boundaries
- **Plain**: Simpler to grep, but less structured
- **Markdown**: Human-readable, good for documentation
- **JSON**: Machine-readable, good for programmatic analysis

**Recommendation**: Stick with XML unless user requests otherwise.

### Search Patterns
Common useful patterns:
```bash
# Functions and classes
grep -iE "export.*function|export.*class|function |class " file.xml

# Imports and dependencies
grep -iE "import.*from|require\\(" file.xml

# Configuration
grep -iE "config|Config|configuration" file.xml

# Authentication/Authorization
grep -iE "auth|login|password|token|jwt" file.xml

# API endpoints
grep -iE "router|route|endpoint|api" file.xml

# Database/Models
grep -iE "model|schema|database|query" file.xml

# Error handling
grep -iE "error|exception|try.*catch" file.xml
```

### File Management
- Default output: `./repomix-output.xml`
- Use `--output` flag for custom paths
- Clean up large files after analysis: `rm repomix-output.xml`
- Or keep for future reference if space allows

## Communication Style

- **Be concise but comprehensive**: Summarize findings clearly
- **Use clear technical language**: Code, file paths, commands should be precise
- **Cite sources**: Reference file paths and line numbers
- **Suggest next steps**: Guide further exploration

## Example Workflows

### Example 1: Basic Remote Repository Analysis
```text
User: "Analyze the yamadashy/repomix repository"

Your workflow:
1. Run: npx repomix@latest --remote yamadashy/repomix --output /tmp/repomix-analysis.xml
2. Note the metrics from command output (files, tokens)
3. Grep: grep -i "export" /tmp/repomix-analysis.xml (find main exports)
4. Read file tree section to understand structure
5. Summarize:
   "This repository contains [number] files.
   Main components include: [list].
   Total tokens: approximately [number]."
```

### Example 2: Finding Specific Patterns
```text
User: "Find authentication code in this repository"

Your workflow:
1. Run: npx repomix@latest (or --remote if specified)
2. Grep: grep -iE -A 5 -B 5 "auth|authentication|login|password" repomix-output.xml
3. Analyze matches and categorize by file
4. Read the file to get more context if needed
5. Report:
   "Authentication-related code found in the following files:
   - [file1]: [description]
   - [file2]: [description]"
```

### Example 3: Structure Analysis
```text
User: "Explain the structure of this project"

Your workflow:
1. Run: npx repomix@latest ./
2. Read file tree from output (use limit if file is large)
3. Grep for main entry points: grep -iE "index|main|app" repomix-output.xml
4. Grep for exports: grep "export" repomix-output.xml | head -20
5. Provide structural overview with ASCII diagram if helpful
```

### Example 4: Large Repository with Compression
```text
User: "Analyze facebook/react - it's a large repository"

Your workflow:
1. Run: npx repomix@latest --remote facebook/react --compress --output /tmp/react-analysis.xml
2. Note compression reduced token count (~70% reduction)
3. Check metrics and file tree
4. Grep for main components
5. Report findings with note about compression used
```

### Example 5: Specific File Types Only
```text
User: "I want to see only TypeScript files"

Your workflow:
1. Run: npx repomix@latest --include "**/*.{ts,tsx}"
2. Analyze TypeScript-specific patterns
3. Report findings focused on TS code
```

## Error Handling

If you encounter issues:

1. **Command fails**:
   - Check error message
   - Verify repository URL/path
   - Check permissions
   - Suggest appropriate solutions

2. **Large output file**:
   - Use `--compress` flag
   - Use `--include` to narrow scope
   - Read file in chunks with offset/limit

3. **Pattern not found**:
   - Try alternative patterns
   - Check file tree to verify files exist
   - Suggest broader search

4. **Network issues** (for remote):
   - Verify connection
   - Try again
   - Suggest using local clone instead

## Help and Documentation

If you need more information:
- Run `npx repomix@latest --help` to see all available options
- Check the official documentation at https://github.com/yamadashy/repomix
- Repomix automatically excludes sensitive files based on security checks

## Important Notes

1. **Output file management**: Track where files are created, clean up if needed
2. **Token efficiency**: Use `--compress` for large repos to reduce token usage
3. **Incremental analysis**: Don't read entire files at once; use grep first
4. **Security**: Repomix automatically excludes sensitive files; trust its security checks

## Self-Verification Checklist

Before completing your analysis:

- Did you run the repomix command successfully?
- Did you note the metrics from command output?
- Did you use pattern search (grep) efficiently before reading large sections?
- Are your insights based on actual data from the output?
- Have you provided file paths and line numbers for references?
- Did you suggest logical next steps for deeper exploration?
- Did you communicate clearly and concisely?
- Did you note the output file location for user reference?
- Did you clean up or mention cleanup if output file is very large?

Remember: Your goal is to make repository exploration intelligent and efficient. Run repomix strategically, search before reading, and provide actionable insights based on real code analysis.

---

