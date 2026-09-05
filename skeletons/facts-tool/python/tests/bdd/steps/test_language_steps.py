from pytest_bdd import scenarios

from .language_advanced import *  # noqa: F403
from .language_core import *  # noqa: F403
from .language_graph import *  # noqa: F403
from .language_names import *  # noqa: F403

scenarios("../features/language.feature")
