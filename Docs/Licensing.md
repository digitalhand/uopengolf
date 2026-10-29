# Licensing and asset policy

## Original contributions

The root [MIT license](../LICENSE) covers original uopengolf code, documentation,
and original assets contributed under that license. Copyright attribution is
`2026 uopengolf contributors`. Keep source SPDX identifiers and applicable notices.

Contributors must have permission to release their work under MIT. Referencing a
project, paying for an asset, or modifying an asset does not automatically grant
the right to redistribute or relicense it.

## Unreal Engine

Unreal Engine is obtained separately and retains the
[Unreal Engine EULA](https://www.unrealengine.com/eula/unreal). It is not relicensed
by this project. Built-in engine resources used by the game retain their applicable
terms. Do not copy engine source or engine content into the public repository as MIT.

Linux is a supported game platform; it does not change the engine licensing model.
Check the applicable engine terms when distributing builds or development tools.

## Upstream projects

open-lm-connector, ogolf-physics, OpenFairway and ogolf are the maintainer's own
`digitalhand` projects. Code ported from them is rewritten in C++ and covered by this
repository's MIT license, with no separate notice. Pin the reference revision in the
relevant design note so behavior can be traced back.

Third-party dependencies or assets inside those projects are not the maintainer's;
check their licenses before importing and record them in
[THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).

## Content

| Content | Location / treatment |
| --- | --- |
| Original MIT assets | `Content/UOpenGolf/`, with editable sources included when available |
| Redistributable third-party work | A documented public location, with its own license/notices; not automatically MIT |
| Purchased or restricted packs and derivatives | Not used by the project and never committed |
| Generated builds, caches and reports | Ignored `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/` |

Maps, code and configuration use only project, engine and open-licence content. The
[asset licence roadmap](AssetLicenseRoadmap.md) lists the third-party art on the public map and
the obligations each item carries.

Do not force-add restricted files or upload them to public Git LFS. `.gitignore` is not a
license scanner and cannot detect restricted files. Review every content import, and use the
actual license applicable to each asset; packs may have different terms.

## Import record

For every future third-party import, record its source URL, pinned revision/version,
license, required attribution and destination in the third-party notices. Preserve full
notices where required.
Do not record credentials, purchase receipts, or private account identifiers publicly.
