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

Linux is an intended game platform; it does not change the engine licensing model.
Check the applicable engine terms when distributing builds or development tools.

## Upstream projects

OpenFairway and the connector addon contain MIT licenses in the inspected local
repositories. They remain future dependencies at this milestone. Before importing,
pin the source revision, inspect the specific files/dependencies, preserve the
original notices, and update [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).

No root license was found in the inspected `ogolf` checkout. Verify permission and
applicable notices before copying its implementation or assets. Architectural
reference does not establish redistribution rights to its files.

## Public and private content

| Content | Location / treatment |
| --- | --- |
| Original MIT assets | `Content/UOpenGolf/`, with editable sources included when available |
| Redistributable third-party work | A documented public location, with its own license/notices; not automatically MIT |
| Purchased or restricted packs and derivatives | Separate private storage; locally installed under ignored `Plugins/PrivateContent/` |
| Generated builds, caches and reports | Ignored `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/` |

Public default maps, code and configuration must work with private packs absent.
Private maps/plugins may reference restricted content; do not introduce those
references into public maps. The intended layout is a private content plugin per
pack under the reserved directory, created when a real pack is introduced.

Do not force-add restricted files or upload them to public Git LFS. `.gitignore`
prevents ordinary accidental adds at the reserved location; it is not a license
scanner and cannot detect restricted files placed elsewhere. Review every content import.

Fab's [license summary](https://www.fab.com/eula) distinguishes incorporated project
distribution and certain collaborator sharing from standalone asset redistribution.
Use the actual license applicable to each asset; packs may have different terms.

## Import record

For every future third-party import, record its source URL, pinned revision/version,
license, required attribution, public/private status, and destination in the third-party
notices or the private pack's own manifest. Preserve full notices where required.
Do not record credentials, purchase receipts, or private account identifiers publicly.
