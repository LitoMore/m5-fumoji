Game data is intentionally not committed here.

FMJ.LIB, HZK16 and ASC16 are not covered by this project's GPL license. Keep
them out of public source and binary releases unless you have redistribution
permission from the relevant rights holders. A full-flash image produced by
scripts/build_release.sh embeds these files and therefore redistributes them.

Run scripts/import_reference_assets.mjs with a baye-fmj-app checkout, then use
`pio run -e cardputer_adv -t uploadfs` to install FMJ.LIB and the two fonts.
