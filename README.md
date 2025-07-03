# CBOR-RPC for Device Control

## Build and Setup

Check out the repository
```powershell
git clone https://github.com/Keel-Inc/CBOR-RPC-DeviceControl.git
cd CBOR-RPC-DeviceControl
```

Build the Device application
```
# Build the Device application
cd .\Device
cmake --preset RelWithDebInfo
cd ..
cmake --build Device\build\RelWithDebInfo
```

Setup the Host application
```powershell
# Create a new virtual environment:
uv venv && .venv\Scripts\activate.ps1

# Install dependencies (including test):
uv pip install -e 'Host\.[test]'
```

Run the tests:
```powershell
python -m pytest
```

## Generate an Image

Install ffmpeg
```
winget install Gyan.FFmpeg
```

Turn any square image into a 480x272 pixel image suitable for the STM32F746G Discovery board's LCD screen
```
$squareSourceImage = "Images\Keel-Inc.png"
ffmpeg -i $squareSourceImage -vf "crop='min(iw,ih*480/272)':'min(ih,iw*272/480)',scale=480:272" -f rawvideo -pix_fmt rgb565le - | `
	python Scripts\make-image-header.py -o "Device\Core\Inc\image_rgb565.h"
```

## Usage

```
renode .\Scripts\device.resc
```

```
