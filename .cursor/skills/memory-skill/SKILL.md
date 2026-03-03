---
name: "memory-skill"
description: "Record key information to graph-memory to avoid repetitive prompts. Invoke when user needs to record bug fixes, architecture decisions, important configurations, or project knowledge."
---

# Memory Skill - Graph Memory Recording

## Overview

This skill persists project key information to graph-memory, supporting entities, relations, and observations.

## Core Functions

- **Create Entities**: Record bugs, features, files, concepts
- **Establish Relations**: Link dependencies, fixes, references
- **Add Observations**: Attach detailed descriptions and context

## Use Cases

### Case 1: Record Bug Fix

Record after fixing an important bug:
- Bug entity (name, type, status)
- Fix solution observation
- Related file entities
- Fix relation

### Case 2: Record Architecture Decision

Record when making architectural changes:
- Architecture component entity
- Decision reason observation
- Component relations

### Case 3: Record Key Configuration

Record when discovering important configs:
- Configuration item entity
- Config value observation
- Usage scenarios

## Entity Types

| Type | Purpose | Example |
|------|---------|---------|
| Bug | Record bugs | P0.1-SeekingAudioSilent |
| Feature | Record features | SeekingFeature |
| File | Record files | OpenALAudio.cpp |
| Component | Record components | AudioThread |
| Config | Record configs | FFmpegDecodeConfig |
| Decision | Record decisions | UseBasePtsSync |

## Relation Types

| Relation | Meaning | Example |
|----------|---------|---------|
| fixes | Fixes | Bug fixes File |
| depends_on | Depends on | Component depends_on Component |
| related_to | Related to | File related_to Feature |
| implements | Implements | File implements Feature |
| causes | Causes | Bug causes Bug |

## Best Practices

1. **Record promptly**: Log bug fixes immediately to preserve context
2. **Detailed descriptions**: Include root cause, solution, verification
3. **Establish links**: Connect related files, components, decisions
4. **Regular review**: Periodically review and update knowledge graph

## Templates

### Bug Record Template

```python
bug_record = {
    "name": "P{x}.{y}-{short_description}",
    "entityType": "Bug",
    "observations": [
        "Problem: ...",
        "Root cause: ...",
        "Solution: ...",
        "Verification: ...",
        "Related files: ...",
        "Fix date: ..."
    ]
}
```

### Decision Record Template

decision_record = {
    "name": "Decision-{topic}",
    "entityType": "Decision",
    "observations": [
        "Decision: ...",
        "Reason: ...",
        "Alternatives: ...",
        "Impact: ...",
        "Decision date: ..."
    ]
}
```
