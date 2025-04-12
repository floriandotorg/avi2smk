# Smacker Video Encoder/Decoder

A **highly-portable**, **zero-dependency** CPP lib/cli for encoding and decoding Smacker video files (.smk), a multimedia file format primarily used in games from the mid-1990s, including the 1997 Lego Island game.

Currently this is the only open-source implementation of a Smacker encoder.

## About Smacker Format

Smacker is a proprietary video file format developed by RAD Game Tools. It was widely used in video games from the mid-1990s to early 2000s due to its high compression ratio and playback efficiency on limited hardware. The format was particularly popular for full-motion video (FMV) sequences in games.

## Features

- Decode Smacker video files to avi
- Encode Smacker video files from avi (WIP)

## Limitations

- **Audio**: Audio decoding/encoding is not supported.
- **Colors**: For encoding the input video is expected to be reduced to 256 colors. This can be achieved by converting it to a GIF first. This, unfortunately, leads to each frame having a separate pallete which lowers compression.  A better approach would be to reduce the colors when encoding and use an intelligent pallete management. This, however, is not trivial.
- **Version 4**: Smacker version 4 files are not supported.
- **Interlacing/Doubling**: Interlacing and doubling are not supported.
- **Padding**: The width and height of the video is expected to be divisible by 4 (encoding & decoding).

## Portability

This project has no dependencies other then the C++ Standard Library. C++23 is the current target standard. Both encoder and decoder are standalone files, optimized for quick copy and paste.

## Usage

### Building the Repository

```bash
git clone https://github.com/floriandotorg/avi2smk.git
cd avi2smk
cmake -DCMAKE_BUILD_TYPE=Release .
make
```

### Using the Tool

#### Convert Smacker Video to AVI

```bash
./smk2avi input.smk
```

This will create an `output.avi` file in the current directory.

#### Convert AVI to Smacker Video

Use ffmpeg to prepare your video file:
```bash
ffmpeg -i input.mp4 temp.gif && ffmpeg -i temp.gif -c:v rawvideo -pix_fmt rgb24 input.avi
```

Then convert it using avi2smk:
```bash
./avi2smk input.avi
```

This will create an `output.smk` file in the current directory.

_Note: AVI to Smacker conversion is still a work in progress as mentioned in the Features section._

## Unit Tests

You can build and run the unit tests like this:

```bash
cmake -DBUILD_TESTING=ON
make
ctest
```

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
