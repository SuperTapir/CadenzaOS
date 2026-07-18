# Vendored dependency policy

Files under this directory are pinned build inputs, not editable forks.
`THIRD_PARTY_NOTICES.md` records upstream commits, licenses, and allowed scope.

U8g2 is intentionally source-curated. Adding a source or font requires:

1. a failing core test that needs the capability;
2. source and symbol review showing the smallest coherent upstream unit;
3. exact framebuffer golden tests on host;
4. independent provenance review for every font or asset;
5. an update to `THIRD_PARTY_NOTICES.md`.

doctest, stb_image_write, and gif-h are desktop/test dependencies and must not
be included from `cadenza_core` public headers.
