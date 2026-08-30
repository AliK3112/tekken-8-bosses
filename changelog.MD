## Changelog (August 30, 2026)
 - Fixed `Mishima Jin` performing regular CD+1 (ETU) and CD+2 (EWHF) using the `f, n` input, instead of the Mishima CD+1 (TGF) and CD+2 (EWGF).
 - Disabling Auto-Parries now also disables them for `Kazama Jin`.
 - Restored `Final Kazuya` d/b+1, 2 after the last patch broke it.

## Changelog (August 20, 2026)
 - Added support for game version 3.02.01 and Bob

## Changelog (August 15, 2026)
 - Added support for Devil Jin's story rage for `Chapter 12` and `Chapter 13` variants

## Changelog (August 10, 2026)
 - Added support for Story Rage Arts by implementing a code-cave/injection hook
 - Fixed an issue where Amnesia Heihachi will perform the regular Rage Art against characters he has special interactions with e.g, Kazuya, Jin.
 - Fixed Shadow Heihachi's Rage Art. Now proper camera and dialogue plays

## Changelog (May 28, 2026)
 - Updated addresses for game version 3.01.01

## Changelog (May 12, 2026)
 - Fixed the bug with non-Mishima Jin variants where his `ws+1, [3, 3] ~ df` CD would **incorrectly** cancel into the Mishima-style CD, allowing him to do EWGF, TGF and Spinning Demon
 - Fixed the bug where non-Final Jin variants could **incorrectly** do `EWGF > TGF` from `ZEN ~ df`
 - Edited some UI text labels

## Changelog (March 25, 2026)
 - Added the option to set the `Rage Blast` as Final Battle Kazuya's Rage Art

## Changelog (February 9, 2026)
 - Fixed a bug where Final Heihachi's Heat wouldn't activate on it's own at the start of each round

## Changelog (January 18, 2025)
 - Overhauled the UI
 - Added More Configuration options
  - Disable Auto Parries (for Jin and Heihachi final variants)
  - Tone Down Damage (for Final Kaz ws+2 combo and Angel Jin CD+1)
 - The program now reads configuration from an ini file (if it exists)

## Changelog (January 18, 2025)
 - Fixed Jin's FC df4 ~ ZEN cancel list for "Nerfed Jin"
 - Fixed an issue where if you had a boss selected which required loading the costume and then pick a different character, it would override the costume. E.g, I had Monk Heihachi selected and then loaded Jin. Monk Heihachi model was loaded over Jin, which shouldn't happen.

## Changelog (December 8, 2025)
 - Fixed Final Kazuya's ff2 Heat Dash bug

## Changelog (December 2, 2025)
 - Updated for Miary Zo

## Changelog (November 2, 2025)
 - Added option on the UI to disable auto costume loading for some characters
 - Fixed glowing eyes for Jin during CD
 - Fixed Final Heihachi getting infinite Heat in some cases
 - Fixed Final Heihachi's VFX bug
 - Fixed Final Heihachi's Warrior Instinct getting disabled b/w rounds
 - Fixed Final Jin not showing "loaded" message in the message box
 - Fixed Angel Jin's Rage Art dialogue
 - Fixed a bug with Reina's Name and Icon not loading
