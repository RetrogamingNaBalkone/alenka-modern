# Application icon

The icon uses the first outlined A from the original intro, with its grey
extrusion on a graphite background (`#282a2e`). The letter keeps the original
pixel art and the 48/35 EGA pixel aspect correction used by the application.
Area sampling preserves its thin outline at small sizes.

Run `python tools/generate_icon.py` to regenerate all three checked-in assets:

- `alenka.png`: 256 px Linux launcher icon and preview.
- `alenka.ico`: 16, 20, 24, 32, 48, 64, 128 and 256 px Windows icon frames.
- `app_icon.hpp`: embedded 64 px RGB icon for SDL windows on both platforms.

Generation uses only Python's standard library and the original DOS resources.
Windows embeds the ICO in the executable; Linux packages install the PNG in
the hicolor theme. No additional runtime data file is needed for the window icon.
