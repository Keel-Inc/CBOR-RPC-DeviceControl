import tempfile
from pathlib import Path

from click.testing import CliRunner
from PIL import Image

from cbor_host.cli import cli


def test_version():
    runner = CliRunner()
    result = runner.invoke(cli, ["--version"])
    assert result.exit_code == 0
    assert result.output.startswith("cli, version ")


def test_cli_help():
    """Test that main help displays correctly"""
    runner = CliRunner()
    result = runner.invoke(cli, ["--help"])
    assert result.exit_code == 0
    assert "CBOR Host - Communication tools for Device LCD display" in result.output
    assert "test" in result.output
    assert "make-header" in result.output


def test_test_subcommand_help():
    """Test that test subcommand help displays correctly"""
    runner = CliRunner()
    result = runner.invoke(cli, ["test", "--help"])
    assert result.exit_code == 0
    assert "Test RPC communication without sending image data" in result.output
    assert "MESSAGE" in result.output


def test_test_requires_message():
    """Test that test subcommand requires a message argument"""
    runner = CliRunner()
    result = runner.invoke(cli, ["test"])
    assert result.exit_code != 0
    assert "Missing argument" in result.output


def test_make_header_subcommand_help():
    """Test that make-header subcommand help displays correctly"""
    runner = CliRunner()
    result = runner.invoke(cli, ["make-header", "--help"])
    assert result.exit_code == 0
    assert "Convert PNG image to C header file" in result.output
    assert "-i, --input" in result.output
    assert "-o, --output" in result.output
    assert "--columns" in result.output


def test_make_header_requires_input_image():
    """Test that make-header subcommand requires an input image argument"""
    runner = CliRunner()
    result = runner.invoke(cli, ["make-header", "-o", "output.h"])
    assert result.exit_code != 0
    assert "Missing option" in result.output


def test_make_header_requires_output_file():
    """Test that make-header subcommand requires an output file"""
    runner = CliRunner()
    # Create a temporary image file that exists
    test_image = Image.new("RGB", (100, 100), color="red")

    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as img_file:
        test_image.save(img_file.name)
        input_file = img_file.name

    try:
        result = runner.invoke(cli, ["make-header", "-i", input_file])
        assert result.exit_code != 0
        assert "Missing option" in result.output
    finally:
        Path(input_file).unlink(missing_ok=True)


def test_make_header_with_nonexistent_image():
    """Test that make-header subcommand fails with nonexistent image"""
    runner = CliRunner()
    result = runner.invoke(cli, ["make-header", "-i", "nonexistent.png", "-o", "output.h"])
    assert result.exit_code != 0
    assert "does not exist" in result.output


def test_make_header_conversion_with_square_image():
    """Test that make-header subcommand converts a square PNG image correctly"""
    runner = CliRunner()

    # Create a test square PNG image
    test_image = Image.new("RGB", (100, 100), color="red")

    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as img_file:
        test_image.save(img_file.name)
        input_file = img_file.name

    with tempfile.NamedTemporaryFile(mode="w", suffix=".h", delete=False) as f:
        output_file = f.name

    try:
        result = runner.invoke(cli, ["make-header", "-i", input_file, "-o", output_file])
        assert result.exit_code == 0
        assert "Generated" in result.output
        assert "480x272 RGB565" in result.output

        # Check the generated file content
        with open(output_file) as f:
            content = f.read()
            assert "// Generated automatically - do not edit" in content
            # Should have 480*272 = 130,560 values
            assert "130560 uint16_t values" in result.output

    finally:
        Path(input_file).unlink(missing_ok=True)
        Path(output_file).unlink(missing_ok=True)


def test_make_header_conversion_with_rectangular_image():
    """Test that make-header subcommand handles rectangular images correctly"""
    runner = CliRunner()

    # Create a test rectangular PNG image (taller than wide)
    test_image = Image.new("RGB", (200, 300), color="blue")

    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as img_file:
        test_image.save(img_file.name)
        input_file = img_file.name

    with tempfile.NamedTemporaryFile(mode="w", suffix=".h", delete=False) as f:
        output_file = f.name

    try:
        result = runner.invoke(cli, ["make-header", "-i", input_file, "-o", output_file])
        assert result.exit_code == 0
        assert "Generated" in result.output
        assert "480x272 RGB565" in result.output
        assert "130560 uint16_t values" in result.output

    finally:
        Path(input_file).unlink(missing_ok=True)
        Path(output_file).unlink(missing_ok=True)


def test_make_header_conversion_with_custom_columns():
    """Test that make-header subcommand respects custom column count"""
    runner = CliRunner()

    # Create a small test square PNG image
    test_image = Image.new("RGB", (50, 50), color="green")

    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as img_file:
        test_image.save(img_file.name)
        input_file = img_file.name

    with tempfile.NamedTemporaryFile(mode="w", suffix=".h", delete=False) as f:
        output_file = f.name

    try:
        result = runner.invoke(cli, ["make-header", "-i", input_file, "-o", output_file, "--columns", "8"])
        assert result.exit_code == 0
        assert "Generated" in result.output

        # Check that the file uses the custom column count
        with open(output_file) as f:
            lines = f.readlines()
            # Skip the header line and check that we have the expected number of values per line
            data_lines = [line for line in lines if line.strip() and not line.startswith("//")]
            if len(data_lines) > 1:  # If we have multiple data lines
                # Look for a line that has exactly 8 values
                # (not the last line which might have fewer)
                for line in data_lines[:-1]:  # Skip the last line
                    if "," in line:
                        # Count actual values by splitting on comma and filtering non-empty
                        values_in_line = len([v.strip() for v in line.split(",") if v.strip()])
                        assert values_in_line == 8, f"Expected 8 values per line, got {values_in_line}"
                        break

    finally:
        Path(input_file).unlink(missing_ok=True)
        Path(output_file).unlink(missing_ok=True)
