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
