# Making a LolOS pre-release

The first planned public milestone is `v0.0.1-pre.1`. It is an experimental
x86_64 boot checkpoint, not a usable or secure operating system. The 32-bit
image is included only as a reference build.

## Before tagging

1. Merge and push the reviewed packaging changes to `main`.
2. Confirm the `OS Build Check` workflow passes on that commit.
3. Build both images locally with `make release-assets` and verify
   `cd dist && sha256sum -c SHA256SUMS.txt`.
4. Boot `dist/lolos-x86_64-checkpoint.iso` in QEMU and confirm the serial
   message `LolOS x86_64: boot map valid.`

## Publish

Create and push the annotated prerelease tag:

```sh
git switch main
git pull --ff-only origin main
git tag -a v0.0.1-pre.1 -m "LolOS 0.0.1 pre-release 1"
git push origin v0.0.1-pre.1
```

The tag starts the build workflow. It rebuilds both ISOs, boot-checks the
x86_64 image, creates `SHA256SUMS.txt`, and publishes a GitHub prerelease with
all three files attached. The release job needs `contents: write` permission
for `GITHUB_TOKEN`; if repository policy disallows it, update the repository's
Actions workflow permissions before pushing the tag.

After the run finishes, verify the release is marked **Pre-release**, download
all three assets from GitHub, and check the hashes with:

```sh
sha256sum -c SHA256SUMS.txt
```

Do not describe this checkpoint as secure, isolated, or general-purpose. The
x86_64 bootstrap uses a temporary first-GiB identity map that is not built from
the Multiboot memory map. It has no user mode, interrupts, drivers, or
interactive shell. See `SECURITY.md` and the release notes for details.
