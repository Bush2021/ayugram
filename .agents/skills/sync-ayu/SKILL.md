---
name: sync-ayu
description: Curate and reconstruct AyuGram Desktop fork work on the latest ayu/dev while preserving durable Bush intent, Ayu's newer implementations, forked submodules, and merge-only policies. Use when asked to sync, update, reconstruct, or rebase this repository against the AyuGram upstream remote. This workflow rewrites shared history and may publish only after explicit approval immediately before each push.
---

# Sync AyuGram

## Outcome

Reconstruct `dev` on current `ayu/dev` as a small, coherent history containing only the durable Bush-specific delta. Preserve Ayu's newer implementations, all still-required fork behavior, and meaningful merge-only resolutions. Reconcile forked submodules, update the thin `vs2026` branch, and do not build.

Read `AGENTS.md` and `FORK.md` first. Treat `FORK.md` as the source of truth for ownership, protected surfaces, remotes, and branch roles.

## Safety boundary

- Start only from clean `dev` with aligned submodules and confirmed exclusive ownership of the history rewrite. Do not stash, reset, clean, or disturb unrelated work.
- Record the starting parent and submodule refs. Protect every rewritten Bush-owned ref with a unique local backup until publication and remote readback are complete.
- Never rewrite AyuGram-owned or Telegram-owned refs.
- Do not publish without fresh approval for the exact refs and object IDs. Use exact force-with-lease protection. A changed old or new object ID invalidates approval.

## Reconstruct intent, not chronology

Audit the complete local divergence from `ayu/dev`, including merge results against both parents. Ordinary patch-equivalence checks do not account for merge-only resolutions.

Maintain a temporary decision ledger that accounts for every local topic and merge as one of:

- durable fork intent to preserve;
- already satisfied by Ayu, including a newer or differently structured implementation;
- temporary, reverted, obsolete, or superseded work to drop;
- merge-only behavior to recreate explicitly;
- unresolved intent that needs more evidence or a product decision.

Resolve classifications from code and history when possible. Ask the user only when the evidence still permits materially different product outcomes.

For each durable topic, inspect the current Ayu tree and its relevant history before editing. Compare final behavior, not commit subjects or whether an old patch applies. Prefer Ayu's current solution when it satisfies the intent, then implement only the remaining delta. Search all related surfaces added since the original work, including settings, storage, UI, localization, resources, build registration, platforms, workflows, and consumers.

Preserve the fork's current product policy unless the user explicitly changes it. Reconstruction alone does not authorize different release timing, cleanup or retention, credential lifecycle, network behavior, defaults, or user-facing semantics. Never restore an old whole file when doing so would discard newer Ayu behavior.

## Submodules and history

- Reconcile every forked path registered in `FORK.md` by ownership, ancestry, tree behavior, and the API expected by the new parent baseline.
- Use Ayu's pointer when it already contains the required intent. Reconstruct only unique fork patches on Bush-owned forks when needed.
- Keep `Telegram/lib_icu` on its published Ayu history unless the user explicitly approves transferring it to a user-controlled fork.
- Require the parent gitlink to match the finalized submodule tip. A published fork tip must also be reachable from the exact advertised remote ref before the parent may be published.
- Reconstruct the result on a temporary branch from current `ayu/dev`. Create one coherent commit per surviving topic. Fold repairs and historical workarounds into the topic they complete. Keep unrelated intent separate.
- Move local `dev` only after the reconstructed tree and history pass verification. Rebase `vs2026` onto that result and preserve only its intended Visual Studio compatibility delta.

## Completion contract

Local reconstruction is complete only when:

- every old topic and merge has an evidence-backed disposition;
- the final tree preserves durable fork behavior without regressing current Ayu behavior;
- branding and fork-owned surfaces are complete across all current consumers;
- parent and submodule worktrees are clean and every gitlink is intentional;
- all dropped work is demonstrably redundant, obsolete, temporary, or reverted;
- the curated history contains coherent topics rather than replayed merge chronology;
- `dev` and `vs2026` satisfy the branch roles in `FORK.md`.

For publication, present one exact ordered batch containing each remote URL, full ref, observed old object ID, proposed new object ID, and lease. Publish rewritten Bush-owned submodules before the parent, then `dev`, then `vs2026`. Read each advertised ref back before continuing. Stop on the first failed lease or mismatch. Keep backups and report the exact published and unpublished state until the entire approved batch verifies successfully.
