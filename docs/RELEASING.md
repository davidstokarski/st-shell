# Releasing a plugin

Every ST plugin repo ships its binaries as a GitHub Release with one zip containing only the plugin bundles:

```
ST <Name> <version>/
  INSTALL.txt
  ST <Name>.vst3
  ST <Name>.component
```

The README links to the stable asset name so the link never changes:
`https://github.com/davidstokarski/<repo>/releases/latest/download/<Slug>-macOS.zip`

## How (copy ST Sat's script)

`st-sat/scripts/release.sh` is the template — copy it and change `PRODUCT`, `SLUG`, `TARGET`. It:

1. configures `build-release` with `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DST_BUILD_STANDALONE=OFF -DST_COPY_AFTER_BUILD=OFF`
2. builds the `<Target>_VST3` and `<Target>_AU` targets and checks both slices with `lipo -info`
3. stages the bundles, ad-hoc signs them (`codesign --force --deep --sign -`), writes `INSTALL.txt`
4. zips with `ditto -c -k --norsrc --noextattr --noqtn --keepParent` (no `__MACOSX` clutter) as both
   `<Slug>-<version>-macOS.zip` and `<Slug>-macOS.zip`
5. creates the GitHub release `v<version>` or replaces its assets (`gh release upload --clobber`)

Bump `project(<Name> VERSION x.y.z)` in the plugin's `CMakeLists.txt` before releasing a new version.

## Notes

- Builds are ad-hoc signed, not notarised: users may need
  `xattr -dr com.apple.quarantine <bundle>` (documented in INSTALL.txt and the README). Notarisation would need an
  Apple Developer ID.
- Run the plugin's tests and `auval` before releasing.
- `dist/` and `build-release/` are git-ignored.
