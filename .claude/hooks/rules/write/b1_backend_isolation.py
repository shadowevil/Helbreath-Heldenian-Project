"""B1: client code does not call a graphics backend's own API (user, 2026-11-25, d-038 in
.claude/docs/decisions/graphics-protocol.md): "it should be a graphics protocol addition not
implemented for just SFML - all graphic frameworks that use the graphics protocol should implement
their own way to match the spec".

Meaning: display and window capabilities are specified in the engine-neutral graphics protocol
(sources/dependencies/shared/render/: renderer.h, window.h, ...) and implemented per backend
(sources/sfml_engine/). Code under sources/client/ talks only to the protocol, so it never includes
a backend header (<SFML/...>, "sfml_*.h") and never names a backend type (`sf::`, SFMLWindow,
sfml_renderer).

Checked on the ADDED text of a Write, Edit or MultiEdit to a C++ file under sources/client/: tokens
in comments and string literals are ignored, and a token that is already in the text being replaced
does not count (an edit that leaves an old violation alone is not a new one).

classify() is pure; the live hook and the replay both call it. Nothing here names a project beyond
the anchored prefix in PREFIXES.
"""
import re
from collections import Counter

RULE_ID = "B1"
PREFIXES = ("sources/client/",)
EXTS = (".cpp", ".cc", ".cxx", ".c", ".h", ".hpp", ".hxx", ".inl", ".ipp")

# One scan, leftmost match wins: an include line first (so a commented-out include is eaten by the
# comment that starts earlier), then comments and string literals (skipped), then backend tokens.
_TOKEN = re.compile(r'''
 (?P<inc>^[ \t]*\#[ \t]*include[ \t]*[<"](?P<incpath>[^>"\n]*)[>"])
|(?P<skip>\b(?:u8|u|U|L)?R"(?P<rd>[^()\s\\"]{0,16})\(.*?\)(?P=rd)"|//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*')
|(?P<sf>\bsf(?:\s|/\*.*?\*/|//[^\n]*(?:\n|$))*::)
|(?P<use>\busing\s+namespace\s+sf\b)
|(?P<alias>\bnamespace\s+\w+\s*=\s*(?:::)?\s*sf\s*;)
|(?P<cls>\bSFML[A-Z]\w*)
|(?P<low>\bsfml_\w+)
''', re.M | re.S | re.X)

MSG = ("B1: %s adds backend API use (%s). d-038: display and window capabilities belong to the "
       "engine-neutral graphics protocol (sources/dependencies/shared/render/: renderer.h, "
       "window.h); each backend (sources/sfml_engine/) implements them. Add the capability to the "
       "interface, implement it in the backend, and call only the interface from sources/client/.")


def applies(rel):
    """True for a C++ source or header under sources/client/ (relative path, forward slashes)."""
    if not rel:
        return False
    r = rel.replace("\\", "/").lower()
    return r.endswith(EXTS) and any(r.startswith(p) for p in PREFIXES)


def tokens(text):
    """Backend-API tokens in `text` as [(kind, token)], comments and strings ignored."""
    out = []
    for m in _TOKEN.finditer(text or ""):
        if m.group("inc") is not None:
            if "sfml" in m.group("incpath").lower():
                out.append(("include", m.group("incpath").strip()))
        elif m.group("skip") is not None:
            continue
        elif m.group("sf"):
            out.append(("sf_ns", "sf::"))
        elif m.group("use"):
            out.append(("sf_using", "using namespace sf"))
        elif m.group("alias"):
            out.append(("sf_alias", "namespace alias of sf"))
        elif m.group("cls"):
            out.append(("sfml_type", m.group("cls")))
        elif m.group("low"):
            out.append(("sfml_ident", m.group("low")))
    return out


def classify(rel, pairs):
    """The hit for one write, or None. `pairs` is [(new_text, old_text)]: the text written and the
    text it replaced ('' for a new file or an unknown original). Counts tokens that are net new."""
    if not applies(rel):
        return None
    added = Counter()
    for new, old in pairs:
        added.update(Counter(tokens(new)) - Counter(tokens(old)))
    if not added:
        return None
    kinds = sorted({k for k, _ in added})
    samples = ["%s:%s" % kt for kt in sorted(added)][:3]
    return {"rule": RULE_ID, "path": rel.replace("\\", "/"), "n_added": sum(added.values()),
            "kinds": kinds, "samples": samples}


def message(hit):
    return MSG % (hit["path"], ", ".join(hit["kinds"]))


C = "sources/client/game.cpp"
# (name, rel, [(new, old)], expected subset of the hit or None)
SELF_TEST = [
    ("sfml include fires", C, [('#include <SFML/Graphics.hpp>\n', "")], {"kinds": ["include"]}),
    ("sfml include quoted fires", C, [('#include "SFML/Window.hpp"\n', "")], {"kinds": ["include"]}),
    ("backend header fires", C, [('#include "sfml_window.h"\n', "")], {"kinds": ["include"]}),
    ("engine folder include fires", C, [('#include "sfml_engine/sfml_renderer.h"\n', "")],
     {"kinds": ["include"]}),
    ("sf namespace fires", C, [("sf::Vector2u size = window.getSize();\n", "")], {"kinds": ["sf_ns"]}),
    ("video mode list fires", C, [("auto m = sf::VideoMode::getFullscreenModes();\n", "")],
     {"kinds": ["sf_ns"]}),
    ("sfml type fires", C, [("SFMLWindow* w = nullptr;\n", "")], {"kinds": ["sfml_type"]}),
    ("sfml ident fires", C, [("sfml_renderer r;\n", "")], {"kinds": ["sfml_ident"]}),
    ("header file fires", "sources/client/game.h", [("sf::Font m_font;\n", "")], {"kinds": ["sf_ns"]}),
    ("two kinds counted", C, [('#include <SFML/Window.hpp>\nsf::Vector2i p;\nsf::Vector2i q;\n', "")],
     {"n_added": 3, "kinds": ["include", "sf_ns"]}),
    ("backslash path fires", "sources\\client\\game.cpp", [("sf::Clock c;\n", "")],
     {"path": "sources/client/game.cpp"}),
    ("new violation beside an old one fires", C,
     [("sf::Clock a;\nsf::Clock b;\n", "sf::Clock a;\n")], {"n_added": 1}),
    ("code after a comment on the same line fires", C, [("/* x */ sf::Time t;\n", "")],
     {"kinds": ["sf_ns"]}),
    ("space before scope fires", C, [("sf ::Clock c;\n", "")], {"kinds": ["sf_ns"]}),
    ("comment before scope fires", C, [("sf/*c*/::Clock c;\n", "")], {"kinds": ["sf_ns"]}),
    ("newline before scope fires", C, [("sf\n::Clock c;\n", "")], {"kinds": ["sf_ns"]}),
    ("using namespace sf fires", C, [("using namespace sf;\n", "")], {"kinds": ["sf_using"]}),
    ("namespace alias fires", C, [("namespace gfx = sf;\n", "")], {"kinds": ["sf_alias"]}),
    ("namespace alias spaced fires", C, [("namespace gfx=::sf ;\n", "")], {"kinds": ["sf_alias"]}),
    ("raw string then real use fires", C, [('auto s = R"x(sf::A)x"; sf::B b;\n', "")], {"n_added": 1}),
    ("raw string that is not closed fires after it", C,
     [('auto s = R"(a)" "b"; sf::Clock c;\n', "")], {"kinds": ["sf_ns"]}),
    # --- must not fire
    ("raw string silent", C, [('auto s = R"(sf::Clock #include <SFML/Window.hpp>)";\n', "")], None),
    ("raw string with delimiter silent", C,
     [('auto s = R"d(a )" sf::X\n using namespace sf;)d";\n', "")], None),
    ("prefixed raw string silent", C, [('auto s = u8R"(sf::Clock)";\n', "")], None),
    ("self scope silent", C, [("self::x y; surf::z q;\n", "")], None),
    ("sf_value silent", C, [("int sf_value = 1; sf_value++;\n", "")], None),
    ("variable named sf silent", C, [("int sf = 1; int a = sf;\n", "")], None),
    ("using namespace sf_x silent", C, [("using namespace sf_x;\nusing namespace std;\n", "")], None),
    ("alias to other namespace silent", C, [("namespace fs = std::filesystem;\n", "")], None),
    ("commented using silent", C, [("// using namespace sf;\n", "")], None),
    ("line comment silent", C, [("// SFMLWindow delivers the event\n", "")], None),
    ("trailing comment silent", C, [("int x = 1; // as sf::Window does\n", "")], None),
    ("block comment silent", C, [("/* sf::Font caches glyphs\n   #include <SFML/Window.hpp> */\n", "")], None),
    ("commented include silent", C, [("// #include <SFML/Graphics.hpp>\n", "")], None),
    ("string literal silent", C, [('log("sf::Texture failed");\n', "")], None),
    ("char/escaped string silent", C, [('auto s = "a \\" sf::x"; char c = \'x\';\n', "")], None),
    ("build macro silent", C, [("#ifdef SFML_ENGINE\n#endif\n", "")], None),
    ("protocol include silent", C, [('#include "renderer.h"\n#include "window.h"\n', "")], None),
    ("protocol use silent", C, [("hb::shared::render::window* w = nullptr;\n", "")], None),
    ("suffix is not sf namespace", C, [("ssf::x y; msf::z q;\n", "")], None),
    ("old violation untouched silent", C,
     [("sf::Clock a;\nint y = 2;\n", "sf::Clock a;\nint y = 1;\n")], None),
    ("removing a violation silent", C, [("int y = 2;\n", "sf::Clock a;\n")], None),
    ("backend folder not covered", "sources/sfml_engine/sfml_window.cpp",
     [("sf::RenderWindow w;\n#include <SFML/Window.hpp>\n", "")], None),
    ("shared render not covered", "sources/dependencies/shared/render/renderer.h",
     [("sf::Font f;\n", "")], None),
    ("client build file not covered", "sources/client/CMakeLists.txt",
     [("GIT_REPOSITORY https://github.com/SFML/SFML.git\n", "")], None),
    ("vcxproj not covered", "sources/client/Client.vcxproj",
     [("sfml-graphics-s.lib;SFMLEngine.lib\n", "")], None),
    ("scratch copy not covered", ".claude/scratch/x/sources/client/a.cpp", [("sf::Clock c;\n", "")], None),
    ("prefix must be a whole segment", "sources/client_tools/a.cpp", [("sf::Clock c;\n", "")], None),
    ("empty text silent", C, [("", "")], None),
    ("no pairs silent", C, [], None),
]


def run_self_test():
    """[(name, ok, detail)] for every case."""
    out = []
    for name, rel, pairs, want in SELF_TEST:
        hit = classify(rel, pairs)
        if want is None:
            ok = hit is None
        else:
            ok = hit is not None and all(hit.get(k) == v for k, v in want.items())
        if ok and hit is not None:
            message(hit)  # must format without raising
        out.append((name, ok, "" if ok else "got %r" % (hit,)))
    return out
