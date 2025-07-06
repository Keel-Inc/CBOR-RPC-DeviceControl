from click.testing import CliRunner

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


