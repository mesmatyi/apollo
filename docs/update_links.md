# Document Link Update Guide

This document provides guidance on how to update the internal links of existing MD documents to match the optimized page structure.

## Recommended Link Formats

### 1. Relative Path Links (Recommended)
```markdown
[link text](../relative/path/to/file.md)
[Cyber RT Overview](../../04_CyberRT/cyber_rt_introduction.md)
```

### 2. Using Path Aliases (If Applicable)
```markdown
[Quick Navigation](快速导航.md)
[Installation Guide](安装指南/安装指南.md)
```

### 3. Cross-Directory Link Examples

**Linking from application practice to core modules:**
```markdown
For more information, see [Cyber RT Communication Mechanism](../../04_CyberRT/communication_mechanism.md)
```

**Linking from tool usage to framework design:**
```markdown
For package management concepts, see [Package Management Tool](../../框架设计/软件核心/包管理工具/包管理概念.md)
```

## Common Link Patterns That Need Updating

### Old Link Pattern (Needs Updating)
```markdown
[old link text](../02_Quick%20Start/old_path.md)
```

### New Link Pattern (Recommended)
```markdown
[new link text](../02_Quick%20Start/new_path.md)
```

## Special Link Handling

### 1. Image Links
Keep relative paths unchanged:
```markdown
![image description](../02_Quick%20Start/images/image.png)
```

### 2. External Links
Keep the full URL:
```markdown
[external resource](https://apollo.baidu.com)
```

## Bulk Update Suggestions

We recommend using the following tools for bulk link updates:

### Using the sed Command (Linux/Mac)
```bash
# Update links under a specific directory
find docs/ -name "*.md" -exec sed -i 's|docs/02_Quick%20Start/|../02_Quick%20Start/|g' {} \;
```

### Using a Python Script
```python
import os
import re

def update_links_in_md(file_path):
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Replace link patterns
    content = re.sub(r'\]\(docs/([^)]+)\)', r'](../\1)', content)
    
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(content)

# Traverse all md files
for root, dirs, files in os.walk('docs'):
    for file in files:
        if file.endswith('.md'):
            update_links_in_md(os.path.join(root, file))
```

## Verifying Link Integrity

After updating, verify the links using the following methods:

### 1. Using markdown-link-check
```bash
npx markdown-link-check docs/*.md
```

### 2. Manually Checking Key Documents
Focus on:
- The main page (MAINPAGE_cn.md)
- The overview document of each chapter
- The quick navigation document

## Notes

1. **Keep paths consistent**: make sure all links use a uniform relative path format
2. **Avoid absolute paths**: do not use absolute paths starting with `/`
3. **Encoding**: spaces in URLs use `%20` or can be written as plain spaces
4. **Test and verify**: always verify that all links are valid after updating

## Emergency Recovery

If a problem occurs during the link update, you can:
1. Use git to revert the changes: `git checkout -- docs/`
2. Refer to the backed-up original link patterns
3. Update incrementally and test in stages
