import pytest

pytest.importorskip("httpx", reason="REST SDK requires the optional rest extra")
