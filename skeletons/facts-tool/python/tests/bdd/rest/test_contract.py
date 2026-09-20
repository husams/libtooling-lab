import asyncio

from pytest_bdd import scenarios, then, when

from .project_steps import *  # noqa: F403

scenarios("contract.feature")


@when("I download the YAML and JSON contracts through generated operations")
def download_contract(sdk, world):
    world["yaml"] = sdk["call"]("openapi_yaml")
    world["json"] = sdk["call"]("openapi")


@then("both contract formats advertise the async job and command operations")
def documented_contract(world):
    document = world["json"]
    assert document["openapi"] == "3.1.0"
    for path in ("/v1/jobs", "/v1/jobs/{id}", "/v1/commands/{commandPath}"):
        assert path in world["yaml"] and path in document["paths"]
    assert document["paths"]["/v1/jobs"]["post"]["operationId"] == "submit"
    assert "202" in document["paths"]["/v1/jobs"]["post"]["responses"]
    assert any(value in world["yaml"] for value in (
        '"operationId": "submit"', 'operationId: "submit"', "operationId: submit",
    ))


@when("I download the contract while polling the blocked import")
def download_during_import(sdk, world):
    async def concurrent():
        api = sdk["client"]
        waiting = asyncio.create_task(api.wait(world["blocked"].id, timeout=5))
        try:
            yaml, document, health = await asyncio.wait_for(
                asyncio.gather(api.openapi_yaml(), api.openapi(), api.health()),
                timeout=2,
            )
            world["responsive"] = bool(yaml) and bool(document) and not waiting.done()
            world["health"] = health
            world["lock"].rollback()
            world["completed"] = await waiting
        finally:
            waiting.cancel()
            await asyncio.gather(waiting, return_exceptions=True)

    sdk["runner"].run(concurrent())


@then("the contract and health respond before the import is released")
def responsive_contract(world):
    assert world["responsive"] and world["health"] == {"status": "ok"}
    assert world["completed"].succeeded
