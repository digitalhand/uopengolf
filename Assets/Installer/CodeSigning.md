# Signing release files

This guide covers signing UOpenGolf release downloads, so players can check that a file
came from this project and hasn't been changed. Signing adds two files to a release. Every
existing `.sha256` file stays.

| Platform | Status | What is signed |
| --- | --- | --- |
| Linux (`.deb`, `.rpm`, `.tar.gz`) | GPG, by hand at release time | `SHA256SUMS`, which lists every release file, signed as `SHA256SUMS.asc`. In-game `.deb` and `.rpm` updates [require it](#in-game-updates-check-the-signature). The `.rpm` itself is not signed with `rpmsign` |
| Windows (`Setup.exe`, `.zip`) | Authenticode signing [pending](#windows-pending) | Listed in the same `SHA256SUMS` in the meantime |

### How Linux signing works

- **There is no certificate to buy.** You sign Debian packages with an OpenPGP (GPG) key
  that you create yourself. Players trust it because they can compare its fingerprint with
  the one you publish. No certificate authority is involved.
- **apt won't check it for players.** `apt` and `dpkg` check signatures only for packages
  that come from a signed APT repository. Installing a downloaded `.deb` checks nothing.
  Ubuntu's `dpkg.cfg` also sets `no-debsig`, so dpkg ignores signatures embedded in a `.deb`.
  Players verify the download themselves before they install it; the commands are under
  [For players](#for-players-verifying-a-download).
- **One signature covers the whole release.** `SHA256SUMS` lists the SHA-256 of every
  release file, the same pattern Ubuntu uses for its ISO images. Players check the
  signature first, then check their file against the list.
- **Signing is done by hand**, on your own machine. Do it after the workflows have filled
  the draft release and before you publish it. CI never holds the key; see
  [Not covered here](#not-covered-here).
- **The game checks it too.** An in-game update of a `.deb` or `.rpm` install runs as root,
  so it installs only what this key signed; see
  [In-game updates check the signature](#in-game-updates-check-the-signature).

### In-game updates check the signature

The `.deb` and `.rpm` install the public half of the release key as
`/usr/share/uopengolf/release-signing-key.gpg`. The builders convert
`Assets/Installer/release-signing-key.asc` at build time. When the game updates a package install,
it downloads `SHA256SUMS` and `SHA256SUMS.asc` with the package. The root installer
(`Assets/Installer/install-update`) runs `gpgv` against that key and installs the package only
if its SHA-256 and file name are in the signed list. Without that check, any program running as
the player could raise the game's genuine password prompt for a package of its own.

What this means for releases:

- **Sign before you publish.** Players only see published releases. A release without
  `SHA256SUMS.asc` shows `.deb` and `.rpm` players the release page instead of an update. If you
  re-sign after a platform changes, players who started an update in between see "the checksum
  list isn't signed" and can try again.
- **Extending the expiry is safe.** A copy that shipped the old expiry date still accepts
  signatures from the same key made after that date: gpgv reports `EXPKEYSIG` with `VALIDSIG`, and
  the installer accepts that. This was checked with gpg 2.4 on 2026-11-24. Still commit the renewed
  key, so new releases ship it.
- **A new key or a new signing subkey strands older copies.** Their keyring doesn't have it, so
  their in-game update fails with "the checksum list isn't signed by the UOpenGolf release key".
  If you can, sign one release with the old subkey and ship the new key in it first. After a
  compromise, tell `.deb` and `.rpm` players to install the next release by hand.
- **Revocation doesn't reach installed copies.** Their keyring only changes when they update,
  so a stolen signing subkey still verifies on copies that haven't updated yet. That's one more
  reason to keep the primary key offline.

### Release key

Fill this in once the key exists.

| | |
| --- | --- |
| Fingerprint | `4B89 8D0D 1715 3A4E 15F1  4B6F 2548 F3B1 E1C1 6B5D` |
| Public key | [`Assets/Installer/release-signing-key.asc`](release-signing-key.asc) |
| Keyserver | [keys.openpgp.org](https://keys.openpgp.org) |
| Expires | Primary key: 2029-11-19. Signing subkey: 2027-11-20 |

## One-time setup

These steps were dry-run with GnuPG 2.4.8, which Ubuntu installs as `gpg`. Any GnuPG 2.2
or later works.

### 1. Create the key

Choose an email address you're happy to publish. It goes into the key and onto the keyserver.
Replace only `you@example.com` and keep the angle brackets. Without them, keys.openpgp.org
finds no address in the key and has nothing to confirm.

```bash
# Primary key. It only certifies its own subkeys. gpg asks for a passphrase.
gpg --quick-generate-key "UOpenGolf Release Signing <you@example.com>" ed25519 cert 3y

# Its fingerprint, used by the commands below.
FPR=$(gpg --list-keys --with-colons 'UOpenGolf Release Signing' | awk -F: '$1 == "fpr" { print $10; exit }')
echo "$FPR"

# Signing subkey: this is what signs releases.
gpg --quick-add-key "$FPR" ed25519 sign 1y

gpg --list-secret-keys --keyid-format long "$FPR"
```

The listing shows `sec … [C]` for the primary key and `ssb … [S]` for the signing subkey.
Players check the primary key's fingerprint. You can therefore replace or renew the subkey
later without changing what they check.

### 2. Back it up

gpg has already written a revocation certificate to `~/.gnupg/openpgp-revocs.d/<FPR>.rev`.
Export the secret key as well:

```bash
gpg --armor --export-secret-keys "$FPR" > uopengolf-release-secret.asc
```

Copy both files to offline storage, for example an encrypted USB drive kept somewhere
safe. Then delete the exported copy from the machine. Never put either file in this
repository, the Blender art root or a cloud-synced folder. Without the backup you can't
extend the key's expiry, or revoke it if the machine is lost.

### 3. Optional: keep only the signing subkey on your everyday machine

The primary key is needed only to extend expiry, add or revoke subkeys, or revoke the
whole key. If you remove it from the machine you sign on, a stolen laptop exposes only
the subkey, and you can replace that without a new fingerprint.

Do this only after step 2's backup is safe:

```bash
gpg --armor --export-secret-subkeys "$FPR" > subkeys.asc
gpg --delete-secret-keys "$FPR"      # answer yes to each prompt
gpg --import subkeys.asc
rm subkeys.asc
gpg --list-secret-keys "$FPR"        # sec# means the primary secret key is gone
```

Signing still works. [Key maintenance](#key-maintenance) then needs the backup, imported
into a temporary keyring.

### 4. Publish the public key

From the repository root:

```bash
gpg --armor --export "$FPR" > Assets/Installer/release-signing-key.asc
gpg --fingerprint "$FPR"
```

1. Commit `Assets/Installer/release-signing-key.asc`. Players download it from
   `https://raw.githubusercontent.com/digitalhand/uopengolf/main/Assets/Installer/release-signing-key.asc`.
2. Upload the same file at <https://keys.openpgp.org/upload>. Uploading doesn't send an
   email by itself: on the results page, choose **Send Verification Email** next to the
   address, then open the link it emails you. Until you confirm, keys.openpgp.org serves
   the key without its name and address. If the page shows no address, the user ID is
   missing its `<…>` brackets; see [Fixing the user ID](#fixing-the-user-id).
3. Fill in the [Release key](#release-key) table. Use the fingerprint in the spaced form
   that `gpg --fingerprint` prints, and add the expiry dates.
4. Put the fingerprint in every release's notes as well; the
   [player snippet](#for-players-verifying-a-download) includes it. When it appears in more
   than one place, a tampered key file on its own can't fool a careful player.

### 5. Optional: sign release tags

The same key can sign the tag a release is built from. Use `git tag -s` in place of
`git tag -a` in [Making a release](../../Docs/Releases.md#making-a-release):

```bash
git config user.signingkey "$FPR"    # this repository only
git tag -s v0.1.0 -m "v0.1.0"
git tag -v v0.1.0
```

GitHub marks the tag **Verified** only when two things are true. The public key must be
added under GitHub → Settings → SSH and GPG keys. The key's address must also be a verified
email on your account that matches `git config user.email`. Either way, `git tag -v` can
check the signature.

## Each release: sign the checksums

Do this between steps 4 and 5 of [Making a release](../../Docs/Releases.md#making-a-release):
when every platform you're shipping is in the draft, just before **Publish release**.

1. Make an empty folder outside the repository:

   ```bash
   mkdir -p ~/uopengolf-releases/v0.1.0 && cd ~/uopengolf-releases/v0.1.0
   ```

2. Open the draft under **Releases**. Download every `UOpenGolf-…` file and its `.sha256`
   into that folder. Skip GitHub's automatic **Source code** archives, which aren't signed.

3. Check that the downloads match what CI built, and that every file has a checksum:

   ```bash
   sha256sum -c *.sha256
   for f in $(ls UOpenGolf-* | grep -v '\.sha256$'); do [[ -f "$f.sha256" ]] || echo "No CI checksum: $f"; done
   ```

   Every line should end in `OK`, and the loop should print nothing. If a file fails,
   download it again. If a checksum is missing, check that platform's workflow run before
   continuing.

4. List every release file in one checksum file and sign it. In a new shell, first set
   `FPR` to the fingerprint from the [Release key](#release-key) table, without spaces.

   ```bash
   ls UOpenGolf-* | grep -v '\.sha256$' | xargs sha256sum > SHA256SUMS
   cat SHA256SUMS
   gpg --local-user "$FPR" --armor --detach-sign --output SHA256SUMS.asc SHA256SUMS
   ```

   gpg picks the signing subkey and asks for its passphrase.

5. Check what you're about to publish:

   ```bash
   gpg --verify SHA256SUMS.asc SHA256SUMS
   sha256sum -c SHA256SUMS
   ```

   Expect `Good signature from "UOpenGolf Release Signing …"` and `OK` for every file.

6. On the draft, choose **Edit** and attach `SHA256SUMS` and `SHA256SUMS.asc`. Add the
   [player snippet](#for-players-verifying-a-download) to the notes, then choose
   **Publish release**.

Keep every `.sha256` file in the release. The in-game updater reads
`UOpenGolf-<version>-Windows-x64-Setup.exe.sha256` to check its download. `SHA256SUMS`
is an addition to those files, not a replacement.

**If a platform changes after you sign,** `SHA256SUMS` no longer matches the release.
While a release is a draft, running a workflow again replaces that platform's files,
because [`draft_release.sh`](../Scripts/draft_release.sh) uploads with `--clobber`. A run
after publishing can also add a missing platform. In either case, repeat steps 2–6. Delete
the old `SHA256SUMS` and `SHA256SUMS.asc` from the release before you attach the new ones.

## For players: verifying a download

Paste this into each release's notes, with the real fingerprint and version:

````markdown
#### Verify your download (Linux)

The release files are listed in `SHA256SUMS`, signed with the UOpenGolf release key
`XXXX XXXX XXXX XXXX XXXX  XXXX XXXX XXXX XXXX XXXX`. Download `SHA256SUMS`,
`SHA256SUMS.asc` and the `.deb` into one folder, then run:

```bash
curl -fsSL https://raw.githubusercontent.com/digitalhand/uopengolf/main/Assets/Installer/release-signing-key.asc | gpg --import
gpg --verify SHA256SUMS.asc SHA256SUMS
sha256sum --ignore-missing -c SHA256SUMS
sudo apt install ./UOpenGolf-0.1.0-Linux-amd64.deb
```

`gpg --verify` must say **Good signature**, and the `Primary key fingerprint` it prints must
match the one above. It also warns `This key is not certified with a trusted signature`; that
is normal unless you have signed the key yourself. `sha256sum` must print `OK` for your file.
If either check fails, don't install the file. Open an issue instead.
````

Players who prefer the keyserver can import the key with
`gpg --keyserver hkps://keys.openpgp.org --recv-keys <FINGERPRINT>` in place of the `curl` line.

The commands above were tested against a deliberately changed `.deb` and a changed
`SHA256SUMS`: `sha256sum` reports `FAILED` and `gpg` reports `BAD signature`, and both exit
non-zero.

## Key maintenance

Extend the dates before they pass. After a key expires, signatures made earlier still
verify as `Good signature` with a note that the key has expired. You can't sign new
releases with an expired key, though.

To extend the dates you need the primary secret key. If you followed
[step 3](#3-optional-keep-only-the-signing-subkey-on-your-everyday-machine), import the
backup into a temporary keyring:

```bash
export GNUPGHOME="$(mktemp -d)"          # skip this and the import if the primary key is on this machine
gpg --import /path/to/uopengolf-release-secret.asc
gpg --quick-set-expire "$FPR" 3y         # primary key, counted from today
gpg --quick-set-expire "$FPR" 1y '*'     # every subkey
gpg --armor --export "$FPR" > release-signing-key.asc
gpg --armor --export-secret-keys "$FPR" > uopengolf-release-secret.asc   # refresh the offline backup
gpgconf --kill gpg-agent && rm -rf "$GNUPGHOME" && unset GNUPGHOME
gpg --import release-signing-key.asc     # your everyday keyring picks up the new dates
```

Then replace `Assets/Installer/release-signing-key.asc` with the new export and commit it.
Upload it to keys.openpgp.org again, store the refreshed backup offline, and update the
**Expires** row. The fingerprint doesn't change.

### Fixing the user ID

If the user ID lacks the `<…>` around the address, add a corrected one and revoke the old
one. The fingerprint doesn't change. This needs the primary secret key.

```bash
gpg --quick-add-uid "$FPR" "UOpenGolf Release Signing <you@example.com>"
gpg --quick-revoke-uid "$FPR" "UOpenGolf Release Signing you@example.com"
gpg --armor --export "$FPR" > Assets/Installer/release-signing-key.asc
```

Upload the new export to keys.openpgp.org and send the verification email as in
[step 4](#4-publish-the-public-key).

### Replacing or revoking keys

**If only the signing subkey is exposed** (the primary key was offline), revoke that subkey
and add a new one. Players keep checking the same fingerprint. In the keyring that has the
primary key:

```bash
gpg --edit-key "$FPR"        # then: key 1, revkey, answer the prompts, save
gpg --quick-add-key "$FPR" ed25519 sign 1y
```

Publish the updated public key as above. Then re-sign the latest release's `SHA256SUMS`.

**If the primary key is lost or exposed,** revoke the whole key. gpg starts the saved
revocation certificate with a colon so it can't be imported by accident. Remove the colon
as you import it:

```bash
sed 's/^:-----/-----/' /path/to/backup/"$FPR".rev | gpg --import
gpg --armor --export "$FPR" > Assets/Installer/release-signing-key.asc
```

1. Commit the revoked key and upload it to keys.openpgp.org. Players who refresh the key
   then see that it's revoked.
2. Create a new key with [One-time setup](#one-time-setup), then re-sign the latest
   release's `SHA256SUMS`.
3. Announce the new fingerprint in the release notes and update the [Release key](#release-key)
   table. Say why the old key was revoked.

## Not covered here

- **Signing in CI.** The package workflows run on self-hosted runners. Anyone with write
  access can run any branch's workflow file on them; see
  [Public repository safety](../../Docs/Releases.md#public-repository-safety). A key in
  Actions secrets or on a runner would be exposed to every such run. Publishing is also
  where you already check the draft by hand, so signing there adds no extra trip.
- **Signatures inside the `.deb`** (`debsigs`). Ubuntu ships dpkg with `no-debsig`, and
  players would have to install `debsig-verify` policies for those signatures to be
  checked. They add work without protecting anyone.
- **A signed APT repository.** With one, `apt` checks signatures itself and Software
  Updater delivers upgrades. It needs hosting that can serve a `.deb` of several hundred
  megabytes; 0.1.0's was about 670 MB, beyond GitHub Pages' per-file limit. It can
  come later and reuse this key.

## Windows (pending)

Windows signing is planned through SignPath Foundation, and the application is pending.
It will cover Authenticode signatures for `Setup.exe`, its uninstaller and the game
executables. Until then, the Windows files are unsigned, as
[Limitations](../../Docs/Releases.md#limitations-and-troubleshooting) says. This section
will hold the steps once signing is set up.
