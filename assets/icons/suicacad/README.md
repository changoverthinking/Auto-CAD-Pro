# SuicaCad icon system

This directory preserves the visual language of the user's original SuicaCad icon set and extends it for the current Auto CAD Pro 2D application.

## Visual contract

- 24x24 source grid
- sharp outline geometry
- square line caps
- miter joins
- source stroke approximately 1.6 px
- source color `#26292c`
- runtime recoloring for dark-theme inactive/active states
- no bitmap scaling dependency in the Win32 toolbar

The runtime toolbar and left rail render these icons as GDI vector geometry so they stay crisp at different window sizes and can change color without maintaining separate light/dark PNG sets.

## Original icons

01-30 are the original SuicaCad set supplied by the project owner.

## Auto CAD Pro extensions

31-50 extend the same design language to functions that exist in the current application but were not present in the original pack.

The canonical mapping is kept in `manifest.tsv`.

### Runtime tool mapping

- Select -> 01
- Line -> 02
- Polyline -> 03
- Rectangle -> 04
- Circle -> 05
- Arc -> 06
- Move -> 08
- Rotate -> 09
- Mirror -> 10
- Trim -> 11
- Offset -> 12
- Extend -> 14
- Dimension -> 15
- Text -> 16
- Copy -> 31
- Scale -> 32
- Hatch -> 33
- Block Insert -> 34

Other extended symbols cover file actions, properties, layout/recovery and layer state for future native menu/ribbon surfaces.
