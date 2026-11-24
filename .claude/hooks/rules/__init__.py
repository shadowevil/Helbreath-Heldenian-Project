"""One module per rule, grouped by action: rules/<action>/<id>_<name>.py. Each exposes RULE_ID,
its check, its message and SELF_TEST cases; the entry point's --self-test collects them all."""
import importlib
import os

HERE = os.path.dirname(os.path.abspath(__file__))


def all_rule_modules():
    """Every rule module under rules/<action>/, imported, sorted by dotted name."""
    mods = []
    for action in sorted(os.listdir(HERE)):
        adir = os.path.join(HERE, action)
        if not os.path.isdir(adir) or action.startswith("__"):
            continue
        for fn in sorted(os.listdir(adir)):
            if fn.endswith(".py") and not fn.startswith("__"):
                mods.append(importlib.import_module("rules.%s.%s" % (action, fn[:-3])))
    return mods
