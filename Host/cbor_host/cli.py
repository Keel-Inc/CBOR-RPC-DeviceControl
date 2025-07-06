import struct
from pathlib import Path

import cbor2
import click
import serial
from PIL import Image


def resize_image(image_path: Path, target_width: int = 480, target_height: int = 272) -> Image.Image:
    """Resize image to target dimensions using ffmpeg-style crop and scale logic."""
    with Image.open(image_path) as img:
        # Convert to RGB if not already
        if img.mode != "RGB":
            img = img.convert("RGB")

        # Get original dimensions
        iw, ih = img.size

        # Apply the ffmpeg crop logic: crop='min(iw,ih*480/272)':'min(ih,iw*272/480)'
        target_aspect = target_width / target_height  # 480/272 = ~1.765

        # Calculate crop dimensions
        crop_width = min(iw, ih * target_aspect)
        crop_height = min(ih, iw / target_aspect)

        # Calculate crop box (center crop)
        left = (iw - crop_width) / 2
        top = (ih - crop_height) / 2
        right = left + crop_width
        bottom = top + crop_height

        # Crop the image
        cropped = img.crop((left, top, right, bottom))

        # Scale to target dimensions
        scaled = cropped.resize((target_width, target_height), Image.Resampling.LANCZOS)

        return scaled


def convert_to_rgb565(image: Image.Image) -> bytes:
    """Convert PIL Image to RGB565 format."""
    # Get pixel data
    pixels = image.load()
    width, height = image.size

    # Convert to RGB565
    rgb565_data = []
    for y in range(height):
        for x in range(width):
            r, g, b = pixels[x, y]

            # Convert 8-bit RGB to 5-6-5 format
            r5 = (r >> 3) & 0x1F
            g6 = (g >> 2) & 0x3F
            b5 = (b >> 3) & 0x1F

            # Pack into 16-bit value (big-endian)
            rgb565 = (r5 << 11) | (g6 << 5) | b5
            rgb565_data.append(rgb565)

    # Convert to bytes (little-endian for STM32)
    return b"".join(struct.pack("<H", pixel) for pixel in rgb565_data)


def send_rpc_message(rpc_message: dict, host: str, port: int) -> dict:
    """Send an RPC message to the device and return the response."""
    try:
        # Connect to Renode's virtual serial port via TCP
        ser = serial.serial_for_url(f"socket://{host}:{port}", timeout=None)

        # Flush any existing data in the buffer
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        # Encode the message as CBOR
        cbor_data = cbor2.dumps(rpc_message)

        click.echo(f"Sending RPC message ({len(cbor_data)} bytes)")

        # Send length prefix (4 bytes, big-endian) followed by CBOR data
        length_prefix = struct.pack(">I", len(cbor_data))
        ser.write(length_prefix + cbor_data)

        # Read response (serial timeout will handle waiting)
        click.echo("Waiting for device response...")
        try:
            # Read 4-byte length prefix
            length_bytes = ser.read(4)
            if len(length_bytes) != 4:
                raise Exception(f"Expected 4 bytes for length, got {len(length_bytes)}")

            # Unpack length (big-endian unsigned int)
            response_length = struct.unpack(">I", length_bytes)[0]

            # Read the CBOR data
            response_bytes = ser.read(response_length)
            if len(response_bytes) != response_length:
                raise Exception(f"Expected {response_length} bytes, got {len(response_bytes)}")

            # Decode CBOR to get the response
            response = cbor2.loads(response_bytes)

        except Exception as e:
            click.echo(f"Error reading response: {e}")
            response = None

        if response:
            click.echo(f"✓ Device response: {response}")
            if response.get("status") == "success":
                click.echo(f"✓ {response.get('message', 'Operation completed successfully')}")
            else:
                click.echo(f"⚠ Device returned: {response.get('message', 'unknown error')}")
        else:
            click.echo("✗ No response received from device")

        return response

    finally:
        if "ser" in locals() and ser.is_open:
            ser.close()
            click.echo("Connection closed")


@click.group()
@click.version_option()
def cli():
    """CBOR Host - Communication tools for Device LCD display"""
    pass


@cli.command()
@click.argument("message", type=str, required=True)
@click.option("--host", default="localhost", help="Renode host address")
@click.option("--port", default=3456, help="Renode USART6 TCP port")
def test(message: str, host: str, port: int):
    """Test RPC communication without sending image data

    MESSAGE: Test message to send to the device
    """
    click.echo("CBOR Host - Testing RPC communication")

    rpc_message = {"method": "test", "params": {"test_message": message}}

    click.echo(f"Connecting to {host}:{port}")
    response = send_rpc_message(rpc_message, host, port)

    if response and response.get("status") == "success":
        received_message = response.get("received_message", "")
        if received_message:
            click.echo(f"✓ RPC communication working correctly! Device received: '{received_message}'")
        else:
            click.echo("✓ RPC communication working correctly!")
    else:
        click.echo("✗ RPC communication failed")


@cli.command()
@click.option("-i", "--input", type=click.Path(exists=True, path_type=Path), required=True)
@click.option("-o", "--output", type=click.Path(), required=True, help="Output C header file")
@click.option("--columns", default=14, help="Number of values per line")
def make_header(input: Path, output: Path, columns: int):
    """Convert PNG image to C header file with RGB565 data."""
    # Resize and crop image to 480x272 using the shared logic
    processed_image = resize_image(input, 480, 272)

    # Convert to RGB565
    rgb565_data = convert_to_rgb565(processed_image)

    # Convert RGB565 bytes to 16-bit values
    values = [struct.unpack("<H", rgb565_data[i : i + 2])[0] for i in range(0, len(rgb565_data), 2)]

    with open(output, "w") as f:
        # Write header
        f.write("// Generated automatically - do not edit\n")

        # Generate values suitable for a C array
        for i, val in enumerate(values):
            f.write(f"0x{val:04X}")
            if (i + 1) % columns == 0:
                f.write(",\n")
            else:
                f.write(", ")

        # Ensure we end with a newline if the last line wasn't complete
        if len(values) % columns != 0:
            f.write("\n")

    click.echo(f"Generated {output} with {len(values)} uint16_t values (480x272 RGB565)")
