# Remote UI testing of R Chromium's BeAPI toolbar and bookmarks

No remote mouse on Haiku, so these drive the real handlers by sending the same
BMessages the controls send, addressed with BeAPI scripting specifiers to the
app signature `application/x-vnd.rchromium-native`. Build on the machine with
`g++ -m32 -o <name> <name>.cpp -lbe`.

- `rchmsg WHAT <window> <view-outer> [<view-inner>...]` -- post a 4-char code to
  a view, e.g. `rchmsg rchA 0 "rchromium chrome"` = press the bookmark star,
  `rchL` = open the Bookmarks window, `rchG` = address-bar Go, `rchO` (to
  Window "Bookmarks") = open the selected bookmark.
  `rchmsg settext "<v>" <window> <views...>` sets a text field.
- `rchmsg_r "<text>" <replace-count>` -- set the Bookmarks search field.
- `tget` -- read the search field back.
- `linv <index>` -- select + invoke bookmark list item <index> (== double-click).
- `lsel <index>` -- select only.
- Counts: `hey content_shell count Item of View bookmarks of View scroller of Window Bookmarks`.

Things learned the hard way, so you do not lose an afternoon:
- BTextView accepts the `Text` property ONLY through a B_RANGE_SPECIFIER, and a
  zero-length range on an empty field is a silent no-op (the text never lands;
  read it back with `tget`). Use a large range (e.g. 1000) to replace all.
- Never pass a range far beyond the current length into an EMPTY text view
  (index 0, range 10000): libbe's BTextView faults in memcpy and takes the whole
  browser down. That crash is the harness's fault, not the app's -- typing
  never produces such a range.
- BListView: `Selection` SET wants an index/range specifier with a bool, not a
  bare int32; `Item` EXECUTE with an index specifier both selects and invokes.
- The BTextControl's inner text view is named `_input_`.
- View names: chrome view `rchromium chrome`, address `address`, Bookmarks
  window title `Bookmarks`, search `search`, list `bookmarks` inside `scroller`.
