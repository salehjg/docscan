# docscan

Turn phone photos of documents into clean, straight PDF pages from the command line, the way
Microsoft Lens did on Android. docscan finds the page in each photo, cuts away the background,
undoes the perspective, evens out the lighting and contrast, optionally makes the page gray or
black & white, and builds one PDF with pdfTeX.

```sh
docscan photos/ -o notes.pdf
```

## Install

From the AUR:

```sh
yay -S docscan        # or any AUR helper, or makepkg -si in packaging/arch
```

From source (needs `opencv`, `cli11`, `cmake`, and `texlive-basic` at runtime):

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

## Usage

Every page goes through a **chain of filters**, in order. A **profile** is a ready-made chain:

| profile | chain |
|---|---|
| `color` (default) | crop → resize:max=2400 → light → contrast:amount=1.3 → sharpen |
| `gray` | crop → resize:max=2400 → gray → light → contrast:amount=1.3 → sharpen |
| `bw` | crop → resize:max=3300 → gray → light → bw |
| `photo` | crop → resize:max=2400 |

Start from a profile and adjust it, or write your own chain:

```sh
docscan IMG_*.jpg -p bw                                 # black & white
docscan IMG_*.jpg -p gray -s resize.max=1600            # change a setting (-s FILTER.KEY=VALUE)
docscan IMG_*.jpg -p bw -x light -a rotate:90           # skip a filter, add one at the end
docscan IMG_*.jpg -f crop -f gray -f resize:1200 -f bw  # your own chain, run in this order
docscan IMG_*.jpg -p bw -n                              # show the final chain without running it
```

`-f` builds a chain from scratch, so it can't be combined with `-p`; to adjust a profile, use `-s`, `-a` and `-x`.
A filter is written as `NAME[:KEY=VALUE,...]`. A bare value sets its main setting, so
`resize:1600` means `resize:max=1600` and `rotate:90` means `rotate:angle=90`.
The order matters: `-f bw -f resize:1000` scales an already black & white page (giving gray edges),
while `-f resize:1000 -f bw` scales first and keeps the page pure black & white.

| filter | does | settings (default) |
|---|---|---|
| `crop` | find the page, cut away the background, straighten it | `aspect` (auto, a4, letter, legal or a ratio), `trim` (0.5 %) |
| `light` | even out lighting and shadows, make the paper white | `strength` (1) |
| `contrast` | stretch levels: ink black, paper white | `amount` (1), `clip` (1 %) |
| `gray` | grayscale | |
| `bw` | black & white | `method` (sauvola, otsu, adaptive), `window` (41), `clean` (0.2) |
| `resize` | change the size | `max` (2000), `width`, `height`, `scale` |
| `rotate` | turn clockwise | `angle` (90) |
| `sharpen` | crisper text edges | `amount` (0.6), `radius` (1) |
| `denoise` | remove grain from dark photos | `strength` (5) |

Other options: `--page a4|a5|letter|legal|fit`, `--quality N` (JPEG), `--images DIR` to also save
the pages as images, `--debug DIR` to save the result of every filter, `-j N` for parallel pages.
See `docscan -h`, `docscan --filters` and `docscan --profiles`.

## How it is built

```
src/app        command line (CLI11) and the program flow
src/core       errors, filter specs ("resize:max=1600"), typed and validated settings, logging
src/filters    one class per filter, and the FilterRegistry that creates them by name
src/vision     page detection, edge snapping, perspective geometry, binarization
src/pipeline   Pipeline, PipelineBuilder (profile + -f/-x/-a/-s), profiles, the parallel Scanner
src/export     PDF via pdfTeX, image export, page layout
```

### Adding a filter

1. Write a class deriving from `Filter` with a static `info()` describing its name and settings,
   a constructor taking `const Settings&`, and `apply()`:

   ```cpp
   class InvertFilter final : public Filter {
   public:
       static FilterInfo info() { return {"invert", "swap black and white", {}}; }
       explicit InvertFilter(const Settings&) {}
       cv::Mat apply(const cv::Mat& image) const override { return ~image; }
   };
   ```

2. Register it in `FilterRegistry::builtin()` with `r.add<InvertFilter>();` and add the .cpp to `CMakeLists.txt`.

The new filter can then be used with `-f`, `-a`, `-s` and `-x`, and it shows up in `--filters` and `-h`.
New profiles are one line each in `ProfileRegistry::builtin()`.

## Releasing to the AUR

1. Tag the release: `git tag v0.1.0 && git push --tags`.
2. In `packaging/arch`, fill in the checksum with `updpkgsums`, then test with `makepkg -si`.
3. Generate `.SRCINFO` with `makepkg --printsrcinfo > .SRCINFO` and push `PKGBUILD` and `.SRCINFO`
   to `ssh://aur@aur.archlinux.org/docscan.git`.

## License

GPL-3.0-or-later; see [LICENSE](LICENSE).
