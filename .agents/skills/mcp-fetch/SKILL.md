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
