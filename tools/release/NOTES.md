TidBank (tape echo and BBD), Reels (looper) and Filtaer (filter), built by GitHub Actions from this tag.

| File | Formats |
|---|---|
| `TidBank-macOS-arm64.zip` | VST3, AU, Standalone. Apple silicon, macOS 11 or later. Ad-hoc signed, **not notarized** |
| `TidBank-Linux-x86_64.zip` | VST3, Standalone. glibc 2.35 or later |

**Checked in CI:** unit tests, plugin tests, pluginval at strictness 5 (VST3) and, on macOS, `auval` (AU). **Not checked:** nobody has listened to the Linux build in a DAW.

Verify the download before you install it: `gh attestation verify <zip> --repo StigLau/tidbank-oss`. That shows which commit and workflow built it, not that the code is safe. Read [the security audit guide](https://github.com/StigLau/tidbank-oss/blob/main/docs/security-audit.md) first. Install steps are in `INSTALL.txt` inside the zip.

Licence: the plugins are AGPLv3 (JUCE 9), the core is MIT. `SOURCE.txt` names the exact sources.
