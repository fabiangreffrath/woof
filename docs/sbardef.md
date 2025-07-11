# Woof additions to SBARDEF spec

## JSON lump
Technically, all additions are compatible with the version 1.0.0 parsers. However, due to the "widgets" concept, we updated the version to 1.1.0. More on widgets later.

## Selecting status bars to render
Woof supports unlimited number of `statusbar`  definitions.

## HUD fonts
The Doom HUD font contains a total of 95 glyphs, which represent the ASCII characters from `!` to `_` .

`root`
|Name|Type|Description
|--|--|--|
|hudfonts|array of `hudfont`|An array of HUD fonts. Must not be null; must contain at least one entry.

`hudfont`
|Name|Type|Description
|--|--|--|
|name|string|An identifier used to resolve this HUD font.
|type|integer|One of the values matching the type enumeration described in the number font section.
|stem|string|A string representing the first few characters of each glyph’s name in the WAD dictionary.

## Status bar elements alignment
Alignment flags were undocumented in SBARDEF 0.99.2 spec.

`alignment`
|Flag|Description
|--|--|
|0x00|Horizontal left|
|0x01|Horizontal middle|
|0x02|Horizontal right|
|0x00|Vertical top|
|0x04|Vertical middle|
|0x08|Vertical bottom|

### New alignment flags: 
Align elements according to the widescreen layout offset.

|Flag|Description
|--|--|
|0x10|Move the element to the left in widescreen mode.
|0x20|Move the element to the right in widescreen mode.

## Element translucency
The new `translucency` boolean field enables rendering elements with a global transparency map defined by the Boom standard.

## New element conditions
|Type|Description
|--|--|
|19|Whether the automap mode is equal to the flags defined by **param**.
|20|Whether widget is enabled is equal to type defined by **param**.
|21|Whether widget type is disabled defined by **param**.
|22|Whether the weapon defined by **param** is not owned.
|23|Whether health is greater than or equal to value defined by **param**.
|24|Whether health is less than the value defined by **param**.
|25|Whether health percentage is greater than or equal to the value defined by the **param**.
|26|Whether health percentage is less than the value defined by **param**.
|27|Whether armor is greater than or equal to value defined by **param**.
|28|Whether armor is less than the value defined by **param**.
|29|Whether armor percentage is greater than or equal to value defined by **param**.
|30|Whether armor percentage is less than the value defined by **param**.
|31|Whether ammo is greater than or equal to value defined by **param**.
|32|Whether ammo is less than the value defined by **param**.
|33|Whether ammo percentage is greater than or equal to value defined by **param**.
|34|Whether ammo percentage is less than the value defined by **param**.
|35|Whether widescreen mode is enabled or disabled defined by **param**.
|36|Whether current episode is equal the value defined by **param**.
|37|Whether current level is greater than or equal to value defined by **param**.
|38|Whether current level is less than value defined by **param**.

Automap mode flags:
|Flag|Description
|--|--|
|0x01|Automap is enabled
|0x02|Automap overlay is enabled
|0x04|Automap is disabled

## Widget element
The concept of a separate HUD no longer exists. All HUD elements are now defined in the SBARDEF lump, including the classic Doom "message line" widget.

`widget`
| Name|Type|Description
|--|--|--|
|type|integer|One of the values matching the type enumeration described in the wiget section.
|x|integer|The virtual x position of this element.
|y|integer|The virtual y position of this element.
|alignment|bitfield|The alignment of this element’s rendered graphics.
|vertical|boolean|Optional vertical layout.
|font|string|The name of the `hudfont` font to render this widget with.
|tranmap|string|The name of a transparency map lump to resolve. Can be null.
|translation|string|The name of a translation to resolve. Can be null.
|translucency|boolean|Enable global translucency.
|conditions|array of `condition`|A series of conditions to meet to render both this and all child elements. Can be null; an array length of 0 is considered an error condition.
|children|array of `sbarelem`|An array of child elements. Can be null; an array length of 0 is considered an error condition.

The widget type has following values:
|Type|Description
|--|--|
|0|Kills/Secrets/Items widget from DSDA-Doom
|1|Level time
|2|Player coordinates widget from Boom
|3|FPS widget
|4|"IDRATE" cheat widget from PrBoom+
|5|Command history widget from DSDA-Doom
|6|Speedometer
|7|Message line
|8|Level name announce line
|9|Multiplayer chat
|10|Level title line

## Carousel element
This is a weapon carousel HUD element compatible with Unity and KEX ports.

`carousel`
|Name|Type|Description
|--|--|--|
|x|integer|Ignored. Carousel element is always centered.
|y|integer|The virtual y position of this element.
|alignment|bitfield|The alignment of this element’s rendered graphics.
|tranmap|string|The name of a transparency map lump to resolve. Can be null.
|translation|string|The name of a translation to resolve. Can be null.
|translucency|boolean|Enable global translucency.
|conditions|array of `condition`|A series of conditions to meet to render both this |and all child elements. Can be null; an array length of 0 is considered an error |condition.
|children|array of `sbarelem`|An array of child elements. Can be null; an array length of 0 is considered an error condition.
