import struct
import time

import cbor2
import click
import serial


@click.command()
@click.argument("message", required=True)
@click.option("--host", default="localhost", help="Renode host address")
@click.option("--port", default=3456, help="Renode USART6 TCP port")
@click.option("--timeout", default=5.0, help="Communication timeout in seconds")
@click.version_option()
def cli(message: str, host: str, port: int, timeout: float):
    """CBOR Host - Send message to Device via USART6

    MESSAGE: The message to send to the device
    """
    click.echo(f"CBOR Host - Connecting to {host}:{port}")

    try:
        # Connect to Renode's virtual serial port via TCP
        # Note: pyserial can handle TCP connections using socket:// URLs
        ser = serial.serial_for_url(f"socket://{host}:{port}", timeout=timeout)

        click.echo(f"Connected to device. Sending message: '{message}'")

        # Flush any existing data in the buffer to avoid reading garbage
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        # Encode the message as CBOR
        cbor_data = cbor2.dumps(message)

        # Send length prefix (4 bytes, big-endian) followed by CBOR data
        length_prefix = struct.pack(">I", len(cbor_data))
        ser.write(length_prefix + cbor_data)

        # Wait a bit for the device to process and respond
        time.sleep(0.2)

        # Read the CBOR response with proper framing
        try:
            # Read 4-byte length prefix
            length_bytes = ser.read(4)
            if len(length_bytes) != 4:
                raise Exception(f"Expected 4 bytes for length, got {len(length_bytes)}")

            # Unpack length (big-endian unsigned int)
            response_length = struct.unpack(">I", length_bytes)[0]

            if response_length > 1024:  # Sanity check
                raise Exception(f"Response length too large: {response_length}")

            # Read the CBOR data
            response_bytes = ser.read(response_length)
            if len(response_bytes) != response_length:
                raise Exception(f"Expected {response_length} bytes, got {len(response_bytes)}")

            # Decode CBOR to get the original message
            response = cbor2.loads(response_bytes)

        except Exception as e:
            click.echo(f"Error reading response: {e}")
            response = ""

        if response:
            click.echo(f"Received echo from device: '{response}'")
        else:
            click.echo("No response received from device")
    finally:
        if "ser" in locals() and ser.is_open:
            ser.close()
            click.echo("Connection closed")
