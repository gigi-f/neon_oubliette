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
