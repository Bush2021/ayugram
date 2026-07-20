---
description: Diagnose the latest failing CI run
allowed-tools: Bash, Read, Edit, Grep, AskUserQuestion
---
1. `gh run list --limit 5` and pick the newest failure
2. `gh run view <id> --log-failed` — read the ACTUAL matrix job log, not the summary
3. Identify root cause; distinguish upstream regression vs. our patch vs. transient (DNS/cache)
4. If transient, say so and stop. Otherwise propose a minimal fix and a local verification command
5. Never push without asking