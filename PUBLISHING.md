# Publishing checklist

Nothing has been published. Do these only when Mitchell says go, roughly in this order.

## Before

- [ ] Fresh clone builds: `git clone . /tmp/w && cd /tmp/w && ./build.sh` (library + both examples, no warnings).
- [ ] Run `examples/hello` and `examples/netx` on a console with the TU2-ET100 (DHCP, `curl`, `nc ... 7`), and
      `examples/netx` once without the adapter (Wi-Fi fallback).
- [ ] If possible, try an official Nintendo RVL-015 and any other AX88772 adapter; update the README's adapter table
      (tested vs expected) and the CHANGELOG.
- [ ] Re-read README for anything personal (names of other machines, addresses, paths): `git grep -niE "192\.168|/Users|cab[0-9]"`.
- [ ] Decide the author line for the README/meta.xml (name, handle) and an e-mail for issues, if any.
- [ ] Set the CHANGELOG's 0.1.0 date.

## GitHub

- [ ] Create the repository (e.g. `libwiieth`), public, no template; description: "Wired Ethernet for Wii homebrew:
      ASIX AX88772 USB LAN driver + lwIP for libogc". Topics: wii, homebrew, libogc, devkitpro, lwip, usb, ethernet.
- [ ] `git remote add origin ...`, `git push -u origin main`.
- [ ] Tag: `git tag -a v0.1.0 -m "libwiieth 0.1.0"`, `git push origin v0.1.0`.
- [ ] Release v0.1.0 from the tag: attach `libwiieth-0.1.0.zip` (lib/libwiieth.a, include/, LICENSE, lwip/COPYING,
      README.md) and the two example apps as HBC zips (`apps/wiieth-hello/{boot.dol,meta.xml}`,
      `apps/wiieth-netx/...`). Paste the CHANGELOG entry as the notes.
- [ ] Optional: a GitHub Actions workflow building in `devkitpro/devkitppc` on each push.

## Announcements

- [ ] r/WiiHacks (and/or r/homebrew): short post - what it is, the adapter that IOS can't run, tested vs expected
      adapters, link to the repo and the release. Ask for reports from other AX88772 adapters and vendor ids.
- [ ] GBAtemp Wii homebrew forum: same post in "Wii - Hacking" / homebrew development.
- [ ] WiiBrew: a page for libwiieth (Development / Libraries; infobox: author, licence MIT + lwIP BSD, version,
      link), and a line on the USB LAN adapter / "Wii LAN Adapter" pages pointing to it.
- [ ] Optional: tell the devkitPro / libogc people (issue or Discord) in case it's useful as a portlib.

## After

- [ ] Watch issues for adapter reports; add vendor ids that work (one line in `source/asix.c`).
- [ ] Later: the rest of the project (launcher), separately, when Mitchell decides.
