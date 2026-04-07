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
