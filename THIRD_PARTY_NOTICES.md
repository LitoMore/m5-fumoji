# Third-party notices and asset policy

## Project license boundary

Copyright (C) 2026 LitoMore

The `m5-fumoji` source code is licensed under GNU GPL version 2 only (`GPL-2.0-only`) in [`LICENSE`](LICENSE). That license does **not** grant rights to the original game data, fonts, names, artwork, dialogue, story, or other non-code assets described below.

This is an independent, unofficial compatibility project. It is not affiliated with, endorsed by, or sponsored by the original game's rights holders or by M5Stack. Product and game names remain the property of their respective owners.

## Reference implementation

The firmware incorporates and adapts the C engine from [`erduoniba/baye-fmj-app`](https://github.com/erduoniba/baye-fmj-app). Its repository-level MIT notice is:

> Copyright (c) 2025 harrydeng

The separate `Fmj/fmj_c_engine` directory is licensed under GPL version 2. The engine files in `lib/FmjCEngine` are pinned to upstream commit `60c41ea2d9932b295833ece7004394497610596a`; see [`lib/FmjCEngine/UPSTREAM.md`](lib/FmjCEngine/UPSTREAM.md) and its retained [`LICENSE`](lib/FmjCEngine/LICENSE). Upstream Win32 integration and generated font data are not included. The replacement system layer and documented engine fixes are Copyright (C) 2026 LitoMore and distributed under GPL-2.0-only.

Binary distributors must comply with GPL-2.0, including providing the complete corresponding source and build scripts for the firmware. Permissively licensed dependencies below retain their respective notices.

## Build dependencies

The build currently uses or may pull in the following third-party components. They remain under their own licenses and are not relicensed by this project:

| Component | Version used | License / notice |
| --- | --- | --- |
| M5Cardputer | 1.1.1 | MIT; M5Stack Technology Co., Ltd. |
| M5Unified | 0.2.19 | MIT; Copyright (c) 2021 M5Stack |
| M5GFX | 0.2.26 | MIT; Copyright (c) 2021 M5Stack |
| Arduino-IRremote | 4.7.1 | MIT; copyright holders listed below |
| Adafruit TCA8418 driver | bundled by M5Cardputer | BSD 3-Clause; Copyright (c) 2019 Limor Fried (Adafruit Industries) |
| Arduino-ESP32 | 2.0.17 package | Package declares LGPL-2.1-or-later; individual bundled components may carry additional notices |

Source and the authoritative license text for each dependency are available from its upstream distribution. In particular, Arduino-ESP32 is available at <https://github.com/espressif/arduino-esp32/tree/2.0.17> and its license is at <https://github.com/espressif/arduino-esp32/blob/2.0.17/LICENSE.md>.

Arduino-IRremote carries:

> Copyright 2009 Ken Shirriff  
> Copyright 2016 Rafi Khan  
> Copyright 2020-2022 Armin Joachimsmeyer et al.

### MIT license text for the MIT-licensed components above

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

### BSD 3-Clause license for the bundled Adafruit TCA8418 driver

Copyright (c) 2019 Limor Fried (Adafruit Industries) All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
3. Neither the name of the copyright holders nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## Game data and generated firmware images

`FMJ.LIB`, `HZK16`, and `ASC16` are not covered by this project's GPL source license. They are intentionally excluded from version control. The import tool is intended only for a user's legally obtained copy.

`scripts/build_release.sh` embeds the current contents of `data/` into the generated full-flash firmware image. Therefore, distributing that image also distributes those assets. Do not publish or redistribute a resource-bearing image unless you have permission from the relevant rights holders. For public source distribution, recipients should import their own legally obtained data.
