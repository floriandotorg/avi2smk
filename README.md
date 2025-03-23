# Smacker Video Encoder/Decoder

A highly-portable, zero-dependency CPP lib/cli for encoding and decoding Smacker video files (.smk), a multimedia file format primarily used in games from the mid-1990s, including the 1997 Lego Island game.

## About Smacker Format

Smacker is a proprietary video file format developed by RAD Game Tools. It was widely used in video games from the mid-1990s to early 2000s due to its high compression ratio and playback efficiency on limited hardware. The format was particularly popular for full-motion video (FMV) sequences in games.

## Features

- Decode Smacker video files to avi
- Encode Smacker video files from avi (WIP)

## Limitations

- **Audio**: Currently, audio decoding/encoding is not supported
- **Version 4**: Smacker version 4 files are not currently supported

## Usage

### Build the Repository

```bash
git clone https://github.com/floriandotorg/avi2smk.git
cd avi2smk
cmake .
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
ffmpeg -i input.mp4 -an -vf "fps=25,format=pal8" -vcodec rawvideo input.avi
```

Then convert it using avi2smk:
```bash
./avi2smk input.avi
```

This will create an `output.smk` file in the current directory.

_Note: AVI to Smacker conversion is still a work in progress as mentioned in the Features section._

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
