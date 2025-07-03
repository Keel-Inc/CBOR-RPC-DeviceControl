import click
import serial
import time


@click.command()
@click.argument('message', required=True)
@click.option('--host', default='localhost', help='Renode host address')
@click.option('--port', default=3456, help='Renode USART6 TCP port')
@click.option('--timeout', default=5.0, help='Communication timeout in seconds')
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
        
        # Send the message using ASCII encoding (add newline for easier parsing on device side)
        message_bytes = (message + '\n').encode('ascii')
        ser.write(message_bytes)
        
        # Wait a bit for the device to process and respond
        time.sleep(0.2)
        
        # Read the echo response with better error handling
        try:
            response_bytes = ser.read_until(b'\n')
            # Try ASCII decoding first, fall back to latin-1 for any bytes
            try:
                response = response_bytes.decode('ascii').strip()
            except UnicodeDecodeError:
                # If ASCII fails, use latin-1 which can decode any byte sequence
                response = response_bytes.decode('latin-1').strip()
                click.echo(f"Warning: Received non-ASCII data: {response_bytes.hex()}")
        except Exception as e:
            click.echo(f"Error reading response: {e}")
            response = ""
        
        if response:
            click.echo(f"Received echo from device: '{response}'")
        else:
            click.echo("No response received from device")
            
    except serial.SerialException as e:
        click.echo(f"Serial communication error: {e}", err=True)
        raise click.ClickException(f"Failed to communicate with device: {e}")
    except Exception as e:
        click.echo(f"Unexpected error: {e}", err=True)
        raise click.ClickException(f"Communication failed: {e}")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            click.echo("Connection closed")
