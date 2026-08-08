---
name: sync-ayu
description: Curate and reconstruct AyuGram Desktop fork work on the latest ayu/dev while preserving durable Bush intent, Ayu's newer implementations, forked submodules, and merge-only policies. Use when asked to sync, update, reconstruct, or rebase this repository against the AyuGram upstream remote. This workflow rewrites shared history and may publish only after explicit approval immediately before each push.
---

# Sync AyuGram

Read `AGENTS.md` and `FORK.md` before acting. Work in the current checkout. Do not build the project.

## 1. Establish the safety boundary

Require all of the following before changing history:

- `git status --porcelain` is empty.
- `git submodule status` has no line beginning with `+`, `-`, or `U`.
- `git branch --show-current` is `dev`.
- The user or coordinator asserts exclusive ownership of `dev` for this rewrite.

Stop on failure. Do not stash, reset, clean, or alter unrelated work. If no reliable ownership assertion or lock exists in a shared checkout, request confirmation.

Record the starting `dev`, `origin/dev`, `ayu/dev`, `vs2026`, and relevant submodule branch tips. Create a unique local backup ref for the starting `dev`. Keep it until publication and every remote readback succeed.

## 2. Refresh and audit the whole history

Fetch `ayu` and the relevant `origin` refs. If the main fetch cannot recurse because Ayu references fork objects unavailable from a submodule remote, fetch the main repository without recursion, then fetch the registered submodule remotes deliberately.

Record the merge base. Audit the symmetric history, complete local range, first-parent history, patch-equivalent commits, and every merge result against both parents. Use combined or remerge diffs so manual conflict resolutions are not lost. `git cherry` is evidence for non-merge patch equivalence, not a complete merge audit.

Keep a temporary ledger outside the worktree or in a confirmed ignored temporary path. For every source commit and merge, record its topic, final intended behavior, disposition, and evidence. A merge-only resolution is accounted for only when its result-versus-each-parent delta is either present in the reconstructed tree or documented as obsolete.

Classify every local change as one of:

- durable fork intent to port;
- already present in Ayu, possibly under a different commit or newer design;
- temporary, reverted, obsolete, or superseded;
- merge-only behavior or policy that must be recreated explicitly;
- unresolved and requiring more evidence.

Do not ask the user to classify a raw commit list that can be resolved from code and history. Ask only when two plausible product outcomes remain after the audit.

## 3. Scan the current Ayu baseline before every topic

Before creating each reconstructed topic, inspect the latest `ayu/dev` code and history for that topic. This is mandatory even when an old local commit applies cleanly.

For each topic:

1. Identify the old local intent from its final behavior, not only its subject or patch.
2. Search the current Ayu tree for the same symbols, strings, settings, resources, consumers, and adjacent behavior.
3. Inspect how Ayu resolved the same area while merging Telegram upstream. Compare Ayu's implementation with the local one, including merge resolutions.
4. Prefer Ayu's newer or more complete implementation when it already satisfies the intent. Port only the remaining behavioral delta.
5. Search for surfaces added since the local work was written. For features, include settings, storage, UI, localization, build files, platform splits, registrations, and call sites.
6. Record the keep, drop, supersede, or compose rationale in the ledger.

For branding, derive an acceptance matrix from the starting fork tree. Include names, repository slugs, domains, update endpoints, package identifiers, UI copy, resources, diagnostics, setup paths, release workflows, and affected submodules. Search the new baseline for both old and new identifiers. Map every matrix item to Ayu's current implementation before marking it superseded. If the remaining delta requires a path marked hands-off in `FORK.md`, prefer an existing fork-owned extension point or present the exact tradeoff to the user.

Never resolve a conflict by taking an old whole file when that would remove newer Ayu fields, platform sources, lifecycle hooks, or APIs.

## 4. Preserve behavior without redesigning policy

Reconstruction is not authorization to harden, simplify, or redesign product policy. Preserve the current fork's final behavior unless the user explicitly requests a policy change.

In particular, do not silently change release publication timing, workflow-run cleanup and retention, credential reset or logout retention, network-request behavior, defaults, or user-facing semantics. Security and reliability concerns may be reported separately, but they are not reconstruction edits.

Keep dependent changes together. Examples include a feature with its storage migration and security companion, generated code with its build registration and consumers, or a workflow with the script options it invokes.

## 5. Reconcile forked submodules

Use the five-path registry in `FORK.md`: `Telegram/lib_ui`, `Telegram/lib_tl`, `Telegram/codegen`, `Telegram/lib_icu`, and `cmake`.

For every path, record its owner, fetch URL, push URL, source ref, target ref, and observed target object ID.

1. Fetch the relevant fork, Ayu, and Telegram refs.
2. Compare ancestry, trees, patches, and the main-repository API expected by the new Ayu baseline.
3. Use the Ayu pointer when it already contains the fork intent.
4. If a Bush-owned fork diverged, reconstruct its branch on the new Ayu or Telegram base and replay only unique fork patches. Do not preserve historical pointer churn.
5. Never rewrite AyuGram-owned or desktop-app-owned refs. For `Telegram/lib_icu`, use its published Ayu tip unless the user explicitly approves moving the gitlink and URL to a user-controlled fork.
6. Create a unique local backup ref in every rewritten Bush-owned submodule. Retain all parent and submodule backup refs until all publication and readback steps succeed.
7. Before publication, require the parent gitlink to match the finalized local submodule tip. After the approved submodule push and readback, require that tip to be reachable from the exact advertised fork ref.
8. Update and publish the parent only after every rewritten submodule passes that post-publication reachability check.

## 6. Reconstruct curated history

Create a temporary reconstruction branch from the refreshed `ayu/dev`. Recreate one coherent topic at a time using the ledger and baseline scan. Cherry-pick only when the old commit still represents the correct complete change. Otherwise implement the final behavioral delta directly.

Use concise repository-style commits. Squash chronology, workarounds, and follow-up repairs into the topic they complete. Recreate durable merge-only resolutions as explicit final-tree changes. Keep unrelated topics separate.

After every topic, inspect its diff against the current parent and repeat the related-surface search. Do not continue while unexpected paths or obsolete code remain.

When the curated branch is complete, compare it with both the starting fork tip and `ayu/dev`. Move local `dev` to it only after the local verification gates pass.

## 7. Verify without building

Do not build. Before publication, verify:

- the worktree and all submodules are clean;
- no submodule status line begins with `+`, `-`, or `U`;
- every registered gitlink matches its finalized local or published submodule tip;
- fork URLs, branding acceptance matrix, resources, Ayu code surfaces, settings, storage, build registrations, and workflow dependencies are complete;
- every dropped local change is demonstrably upstream-equivalent, reverted, temporary, obsolete, or superseded;
- every durable merge-only behavior is present in the reconstructed tree;
- no old whole-file resolution removed newer Ayu behavior;
- the curated history contains coherent topics rather than merge chronology.

Because reconstructed `dev` changes, rebase `vs2026` onto it as required by `FORK.md`. Preserve only the intended Visual Studio compatibility delta. Verify the rebased branch before publication.

After submodule publication, repeat remote reachability for every fork gitlink. After parent publication, repeat cleanliness, advertised-ref equality, and ancestry checks. Remove temporary audit notes after handoff.

## 8. Publish only with fresh exact leases

Immediately before an external history rewrite, obtain explicit user approval for the exact ordered push batch. The approval request must list every remote URL, full ref, freshly observed old object ID, proposed new object ID, and lease expression. A changed old or new ID invalidates the approval.

For the approved batch:

1. Push rewritten Bush-owned submodule branches first.
2. Read each advertised remote ref back and verify equality and reachability.
3. Push `dev`.
4. Read `origin/dev` back and verify equality.
5. Push `vs2026`.
6. Read `origin/vs2026` back and verify equality.
7. Return the checkout to `dev`.

Use `--force-with-lease=<full-ref>:<observed-object-id>`. Never use plain `--force`. Stop after the first failed lease or readback. Report pushed and unpushed refs with old and new IDs. Do not roll forward or roll back without fresh approval. Retain every backup ref until the complete ordered batch and all readbacks succeed.
