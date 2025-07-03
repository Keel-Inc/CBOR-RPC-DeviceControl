import pytest
from click.testing import CliRunner
from cbor_host.cli import cli


def test_version():
    runner = CliRunner()
    result = runner.invoke(cli, ["--version"])
    assert result.exit_code == 0
    assert result.output.startswith("cli, version ")


def test_cli_requires_message():
    """Test that CLI requires a message argument"""
    runner = CliRunner()
    result = runner.invoke(cli, [])
    assert result.exit_code != 0
    assert "Missing argument" in result.output


def test_cli_help():
    """Test that help displays correctly"""
    runner = CliRunner()
    result = runner.invoke(cli, ["--help"])
    assert result.exit_code == 0
    assert "Send message to Device via USART6" in result.output
    assert "MESSAGE" in result.output
