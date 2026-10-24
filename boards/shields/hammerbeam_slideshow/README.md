# hammerbeam_slideshow

Custom nice!view screen for both halves: a cycling slideshow of Hammerbeam's
1-bit artwork plus a battery level / connection indicator in the top-right
corner.

Derived from [GPeye/hammerbeam-slideshow](https://github.com/GPeye/hammerbeam-slideshow)
(MIT license), which is itself based on ZMK's `nice_view` shield, modified to:

- run on **both** the central and peripheral halves of a split keyboard,
- choose which of the 30 artworks (1–30) to include at build time via
  `CONFIG_HAMMERBEAM_SLIDESHOW_MASK` in your `.conf` file (bit N−1 selects
  artwork N; `0x3FFFFFFF` = all),
- control the full cycle duration with `CONFIG_CUSTOM_ANIMATION_SPEED`
  (milliseconds, divided evenly across the selected artworks).

## Artwork preview

`artwork_previews.png` shows all 30 artworks with their numbers. The
original artwork is by Hammerbeam (@hammerbeam on the ZMK Discord).