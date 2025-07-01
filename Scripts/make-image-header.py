import click
import struct
import sys

@click.command()
@click.option('-o','--output', 'output_file', type=click.Path(), required=True, help='Output C header file')
@click.option('--endian', type=click.Choice(['little', 'big']), default='little', help='Byte order of input file')
@click.option('--columns', default=14, help='Number of values per line')
def rgb565_to_c_array(output_file, endian, columns):
    """Convert RGB565 raw file to C header file."""
    
    # Read from stdin
    data = sys.stdin.buffer.read()
    
    # Ensure we have complete 16-bit values
    if len(data) % 2 != 0:
        raise click.ClickException("Input data size must be even (complete 16-bit values)")
    
    # Read as 16-bit values
    fmt = '<H' if endian == 'little' else '>H'
    values = [struct.unpack(fmt, data[i:i+2])[0] for i in range(0, len(data), 2)]
    
    with open(output_file, 'w') as f:
        # Write header
        f.write("// Generated automatically - do not edit\n")
        
        # Generate include-style format (just the values)
        for i, val in enumerate(values):
            f.write(f'0x{val:04X}')
            if i < len(values) - 1:
                f.write(', ')
                if (i + 1) % columns == 0:
                    f.write('\n')
                    continue
            else:
                f.write('\n')
    
    click.echo(f"Generated {output_file} with {len(values)} uint16_t values")

if __name__ == '__main__':
    rgb565_to_c_array()