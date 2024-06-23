# WOOFHUD

Woof! supports the WOOFHUD lump to customize the appearance of the extended Boom HUD.

## Description

The Boom HUD shows information about the player's health, armor, weapons, ammo and keys using different widgets, i.e. lines of text and symbols. It is usually made visible by hitting the <kbd>F5</kbd> key, and repeatedly hitting the <kbd>F5</kbd> key toggles through three different modes: the "minimal" mode which only shows the most basic information by default, the "compact" mode which shows all information in the lower left corner of the screen and the "distributed" mode which shows information spread across the corners of the screen.

The WOOFHUD lump can be used to modify the positions of these widgets for each mode.
This lump may either get embedded into a PWAD, or provided by the user on the command line or through the autoload feature.

## Format

The WOOFHUD lump is in text format and consists of paragraphs, which are separated by blank lines. Each paragraph begins with a line starting with the keyword `hud` and a number for the HUD mode which is to be modified: `0` for minimal, `1` for compact and `2` for distributed.
The following lines start with the name of the HUD widget which is to be positioned followed by either a keyword giving the position relative to the screen or two numbers giving the absolute X and Y screen coordinates.

Possible values for the HUD widget names:

 * "title" or "levelname"
 * "message"
 * "secret"
 * "armor"
 * "health"
 * "ammo"
 * "weapon" or "weapons"
 * "keys"
 * "monsec" or "stats"
 * "sttime" or "time"
 * "coord" or "coords"
 * "fps" or "rate"
 * "cmd" or "commands"
 * "compact"

Possible values for the widget position keywords:

 * "topleft" or "upperleft"
 * "topright" or "upperright"
 * "topcenter" or "uppercenter"
 * "bottomleft" or "lowerleft"
 * "bottomright" or "lowerright"
 * "bottomcenter" or "lowercenter"

When using relative screen positioning, the widgets are aligned "first come, first serve". For example, the first widget in a paragraph that is aligned with the "bottomleft" keyword will end up in the very bottom-left area of the screen and each following widget that is aligned with the same keyword will get stacked one line above.

Absolute X and Y screen coordinates are limited to the low-resolution non-widescreen visible area of the screen, i.e. `0 <= X < 320` and `0 <= Y < 200`. Negative values will get interpreted relative to the right or lower edges of the screen, respectively. If the "Widescreen Widget Arrangement" feature is enabled, widgets will get aligned with respect to the widescreen area of the screen (new in Woof! 14.0.0).

## Examples

The following example represents the current default alignments of the Boom HUD widgets:

```
hud 0
compact bottomleft
rate topleft
monsec topleft
sttime topleft
coord topright
fps topright
cmd bottomright

hud 1
rate topleft
armor bottomleft
health bottomleft
ammo bottomleft
weapon bottomleft
keys bottomleft
monsec bottomleft
sttime bottomleft
coord topright
fps topright
cmd bottomright

hud 2
rate topleft
health topright
armor topright
ammo bottomright
weapon bottomright
keys bottomleft
monsec bottomleft
sttime bottomleft
coord topright
fps topright
cmd bottomright
```

An alternative approach to the distributed HUD, using absolute screen coordinates, could look like this:

```
hud 2
health 224 0
armor 224 8
ammo 200 -8
weapon 200 -16
keys 2 -8
monsec 2 8
sttime 2 16
coord 200 8
fps 224 16
```

## Remarks

The "title" widget is only visible if the Automap is enabled. The "monsec", "sttime" and "coord" widgets are only visible if they are explicitly enabled in the Options menu (separately for Automap and HUD). The "fps" widget is only visible if the SHOWFPS cheat is enabled.

The "compact" widget is a minimal widget showing health, armor and ammo information only. It is enabled by default in the minimal Boom HUD mode.

A centered widget does not allow for any other left or right aligned widget on the same line.

HUD modes without a paragraph remain unchanged. Widgets which are not mentioned in a paragraph will *never* be visible in the respective HUD mode. So, it is a good idea to *always* include the five widgets which make up the `hud 0` paragraph in the example above in *any* other paragraph.
The Vanilla Doom widgets (i.e. "title", "message", "secret"), however, will *always* be visible, whether they are explicitly mentioned in a paragraph or not (new in Woof! 12.0.0).

It is currently impossible to use the WOOFHUD lump to modify the appearance of the Status Bar or the Crispy HUD. However, when using the Crispy HUD or the Automap, the visible widgets will align corresponding to the last active HUD mode.

