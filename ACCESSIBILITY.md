# Accessibility

QRPickle is a small touch screen (2.8", 320×240, resistive touch) and a web console. This page
says what helps today, what doesn't work well yet, and how to tell us.

## On the device

- **Readable type:** prose, buttons and titles use [Atkinson Hyperlegible](https://www.brailleinstitute.org/freefont/),
  a font designed for low-vision readers; tables of numbers use JetBrains Mono so columns line up.
- **Themes** (Settings → Theme): a high-contrast light theme, two monochrome "E-Ink" themes (light
  and dark, no colour needed), a red night theme that keeps dark adaptation, and dark themes.
- **Brightness:** manual, or automatic from the light sensor; the screen can sleep after a timeout
  and wakes on a touch.
- **No sound is needed:** everything shown on the screen is visible without audio, and alerts also
  light the status LED ([docs/LEDColours.md](docs/LEDColours.md)).
- **Screen cycling** can show the screens one after another without touching the display.

## In the web console

- Every setting can also be changed from a browser, where your own zoom, screen reader and
  keyboard work; form fields have labels.

## Known gaps

- **Small text and targets:** the smallest text is 10 px and some buttons are small on a 2.8"
  screen. Resistive touch needs a firm press; a stylus helps.
- **Colour carries meaning** in places (band conditions, spot types, the status LED). The E-Ink
  themes and the text values help, but not every colour has a text equivalent yet.
- **The LED** signals by colour and blink pattern; there is no non-visual alert yet.
- **The web console** hasn't been tested with a screen reader end to end, and has no phone layout yet.

## Tell us

If something is hard to use, open an [issue](https://github.com/bmp/QRPickle/issues/new/choose)
(say "accessibility" in the title) and describe what you tried and what got in the way.
Suggestions that work within the board's limits (no PSRAM, a small screen) are especially welcome.
