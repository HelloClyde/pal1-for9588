# Third-party components

## SDLPAL

- Repository: `https://github.com/sdlpal/sdlpal.git`
- Pinned commit: `224cb0a0cc839e0e8ecaa299cd480b02b372b9f7`
- License: GPL-3.0-or-later (`third_party/sdlpal/gpl.txt`)

This is the commit immediately before the historical Dingux/GPH/PSP/Wii
ports were removed. The 9588 port uses it as a JZ4740-oriented compatibility
baseline; no source files inside the submodule are patched.

The enabled RIX music path also uses the AdPlug-derived OPL components already
vendored by SDLPAL; their file-level GPL/LGPL notices remain in that submodule.
AVI playback uses SDLPAL's `aviplay.c`, including its attributed FFmpeg-derived
Microsoft Video 1 decoder. Its existing GPL/LGPL notices remain in the pinned
submodule; no proprietary player binary from the original device is copied.

## BBK 9588 BDA SDK

- Repository: `https://github.com/HelloClyde/bbk9588-bda-sdk.git`
- Pinned commit: `73ef26afacea117b0b46ee9213d2efa80724a3a4`
- License: see `sdk/LICENSE` and `sdk/NOTICE`

## PAL game data

PAL game data is proprietary and is not covered by this repository's GPL
license. The normal source history and `main` branch contain no original or
converted game data.

For the owner's private development workflow only, the private `release`
branch stores one Git-LFS-managed `release-assets/PAL-ORIGINAL.PAK` assembled
from a legally purchased Steam copy. That branch must not be merged into
`main`, shared with unauthorized users, or retained if the repository is made
public. Other users must supply their own legally obtained files.

The resource preparation scripts invoke a user-installed FFmpeg executable as
an offline tool. FFmpeg is not bundled. `PAL9588.PAK` and converted movies
remain derived from the user's legally obtained PAL data and are excluded from
the source branches.
