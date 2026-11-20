# Licensing and asset policy

## Contributions

The root [MIT license](../LICENSE) covers original uopengolf code, documentation,
and original assets contributed under that license. Copyright attribution is
`2026 uopengolf contributors`. Keep source SPDX identifiers and applicable notices.

### Contributions are MIT-licensed

uopengolf follows the open-source convention known as *inbound = outbound*: you
contribute under the same license the project is released under. Section D.6 of
[GitHub's Terms of Service](https://docs.github.com/en/site-policy/github-terms/github-terms-of-service)
applies the same rule to repositories hosted there.

Any contribution you intentionally submit for inclusion in uopengolf is licensed under the
MIT license, without additional terms or conditions, as soon as you submit it. A
contribution is any work of authorship you submit by pull request, patch, issue, comment
or any other means, including code, scripts, tests, documentation, assets and their
editable sources, maps, configuration, designs, data and translations, and later changes
to any of them. Ideas, suggestions and feedback given in issues, discussions or reviews
may be used by the project freely, with no obligation to the person who gave them.

If you have a separate written agreement with the maintainers that covers a
contribution, that agreement takes precedence.

### What contributing means

- **You keep your copyright.** Contributing doesn't transfer ownership. It grants
  everyone, including the maintainers, the permissions in the MIT license for your
  contribution. No contributor license agreement (CLA) or copyright assignment is required.
- **It is public and permanent.** Your contribution and its record in Git, including the
  name and email address in your commits, are public, are kept indefinitely and may be
  redistributed under the project's licenses. To keep your email address private, use
  GitHub's `noreply` address for your commits.
- **It is voluntary.** Contributing creates no employment, partnership or agency
  relationship, and gives no right to payment, royalties or control over the project.
  Credit comes from the Git history and the `uopengolf contributors` copyright notice. You
  aren't expected to support your contribution after it is merged, unless you choose to.
- **Maintainers decide.** Maintainers may accept, change or decline any contribution,
  and may later modify or remove contributed work.

### What you confirm

By submitting a contribution, you confirm that:

1. You created it, or you otherwise have the right to submit it under the MIT license.
2. If an employer, client or school may hold rights in work you create, you have its
   permission to contribute this work under MIT.
3. Any third-party material in it is under a license that allows redistribution, is
   identified in your pull request and is recorded in
   [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md). Third-party material keeps its own
   license and is not relicensed as MIT. Referencing a project, paying for an asset, or
   modifying an asset does not automatically grant the right to redistribute or relicense it.
4. It contains no confidential information, trade secrets or material whose terms forbid
   releasing it under MIT. Output from code-generation or AI tools counts as your
   submission: you must be able to release it under MIT.

These confirmations are modeled on the
[Developer Certificate of Origin](https://developercertificate.org/), which many open-source
projects use. If you can't confirm them, don't submit the work; open an issue describing
what you would like to add instead.

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
