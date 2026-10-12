# How band conditions are rated

The Band Conditions screen and the dashboard tile rate each band **GOOD**, **FAIR** or **POOR** for
day and for night (6 m: **Es?** or closed). The ratings are an estimate for a typical one-hop path of
about 3000 km at mid latitudes, worked out on the device from the solar data. They are not a
prediction for a particular path: use them to decide where to listen first.

The code is `src/services/band_rating.cpp`; every number below is in its `MODEL` table, and the unit
tests (`test/test_parsers`) check the model.

## Inputs

| Input | Source | Used for |
|---|---|---|
| Solar flux (SFI) | HAMQSL.com (N0NBH) or your own source, every 3 hours | the sunspot number, so the MUF; daytime absorption |
| K index | same | geomagnetic storms (MUF), noise on the low bands |
| A index | same | longer storms (MUF) |
| Station latitude and longitude | web console (or the grid square's centre) | day, night or greyline now; the sporadic-E season |
| Date | the clock (NTP) | the sporadic-E season |

Without solar data the screen shows the bands as unknown.

## Steps

1. **Effective sunspot number** R from the flux, by inverting the usual fit
   SFI = 63.7 + 0.728 R + 0.00089 R². R is 0 at SFI 64 or less.
2. **Maximum usable frequency (MUF)** for a 3000 km hop:
   day MUF = 18 + 0.14 R MHz; night MUF = 10.5 + 0.08 R MHz.
3. **Storm factor:** the MUF is multiplied by the stronger reduction of
   K (4: 0.95, 5: 0.85, 6: 0.75, 7 or more: 0.65) and A (20 or more: 0.95, 30: 0.85, 50: 0.75).
4. **Skywave rating:** a band at or below 85 % of the MUF (the optimum working frequency) is GOOD,
   up to the MUF FAIR, above it POOR.
5. **Daytime absorption (D layer):** the lowest usable frequency is LUF = 4 + 0.02 (SFI − 70) MHz,
   at least 3 MHz. In the day a band below the LUF is POOR, below 1.5 × LUF at best FAIR. At night
   there is no D layer.
6. **Noise on the low bands** (below 7 MHz): K 3-4 makes them at best FAIR, K 5 or more POOR.
7. **6 m:** rated like the other bands when the MUF reaches it (solar maximum). Otherwise it shows
   **Es?** in the daytime in the sporadic-E season (May to August in the northern hemisphere,
   November to February in the southern), and closed the rest of the time.
8. **Groups** (the dashboard tile) show the best band in the group: "something in this group works".

**Day, night and greyline:** the sun more than 6° above the horizon is day, more than 6° below it
night; in between is greyline (about 30-60 minutes around sunrise and sunset), when both columns are
marked because paths along the terminator favour the low bands.

## Examples (quiet conditions unless stated)

| Conditions | Day: GOOD | Day: FAIR | Night: GOOD | Night: FAIR |
|---|---|---|---|---|
| SFI 70, K 1 (solar minimum): MUF 19 MHz day, 11 at night | 40-20 m | 60 m, 17 m | 160-40 m | 30 m |
| SFI 120, K 2: MUF 28 / 16 MHz | 30-15 m | 60-40 m, 12 m | 160-30 m | 20 m |
| SFI 180, K 2 (solar maximum): MUF 37 / 21 MHz | 30-10 m | 40 m | 160-17 m | 15 m |
| SFI 150, K 5, A 30 (storm): MUF 28 / 16 MHz | 30-15 m | 40 m, 12 m | 40-30 m | 20 m |

In the storm the low bands are POOR at night because of the noise, although the MUF alone would
allow them.

## Limits

- One typical path. Long paths, short skip, polar paths (much more affected by storms) and
  north-south paths across the equator behave differently.
- The fit from SFI to sunspots and the MUF formulas are approximations; real MUF varies by
  ±20 % from day to day. The `MUF` value on the SOLAR tab, when the source reports one, is a
  measurement and better than this estimate.
- The seasons, the time of day within "day" and the local ionosonde are not modelled.
- Sporadic E is a season, not a forecast.

## Sources

The SFI-sunspot fit is the commonly used one, and 85 % of the MUF is the classic optimum working
frequency. The MUF, LUF and storm constants are rough estimates for a mid-latitude 3000 km path, not
fitted to measurements. Corrections from operators are welcome (an issue on GitHub).
