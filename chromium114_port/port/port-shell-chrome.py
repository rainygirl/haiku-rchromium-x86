#!/usr/bin/env python3
"""The native toolbar, and with it the install button.

`files/content/shell/browser/shell_platform_delegate_aura.cc` replaces the
stock one. All the toolbar's BeAPI machinery is already in
`haiku_port/ozone/haiku_beapi_views.cc` -- back, forward, reload, the address
field, bookmarks, the install button, and the icon scaling that gives an
installed app the site's own icon. Nothing called it: the caller lived in the
87 overlay's shell delegate, and the 114 port started from the stock one.

So this is the other half. It gives the shell a HaikuBrowserChromeClient,
attaches the toolbar unless RCH_NO_TOOLBAR is set, asks for the page's web app
manifest when a load finishes, and on the install button writes a launcher
into ~/config/non-packaged/apps with a Deskbar link and the site's icon.

That is where R Twitter comes from. It is why the browser package does not
require a Twitter launcher: installing a web app is something the browser
does.

Two things this needs from gn: the ozone target (whose visibility list already
names //content/shell/*, from when the 87 port did this) and blink's manifest
types.
"""
import sys

root = sys.argv[1]
path = "%s/content/shell/BUILD.gn" % root
s = open(path).read()

if '"//haiku_port/ozone"' in s:
    print("  BUILD.gn: 이미 적용")
else:
    old = ('    } else {\n'
           '      sources += [\n'
           '        "browser/shell_platform_delegate_aura.cc",\n'
           '        "browser/shell_web_contents_view_delegate_aura.cc",\n'
           '      ]\n'
           '    }')
    assert s.count(old) == 1, s.count(old)
    new = (old + '\n\n'
           '    if (is_haiku) {\n'
           '      # The toolbar lives in the ozone target, whose visibility\n'
           '      # list already names this one. blink\'s manifest types come\n'
           '      # with it because the delegate reads a web app manifest to\n'
           '      # decide whether a page can be installed.\n'
           '      deps += [\n'
           '        "//haiku_port/ozone",\n'
           '        "//third_party/blink/public/common",\n'
           '        "//third_party/blink/public/mojom:mojom_platform",\n'
           '      ]\n'
           '    }')
    open(path, "w").write(s.replace(old, new, 1))
    print("  BUILD.gn: ozone 의존 추가")

# content/shell's DEPS allows +content/public only. The delegate reaches into
# content/browser for ManifestManagerHost, because WebContents::GetManifest()
# -- which 87 used -- is gone in 114 and nothing public replaced it.
path = "%s/content/shell/DEPS" % root
s = open(path).read()
if "content/browser/manifest" in s:
    print("  DEPS: 이미 적용")
else:
    anchor = '  "+content/public",'
    assert s.count(anchor) == 1, s.count(anchor)
    new = (anchor + "\n\n"
           "  # WebContents::GetManifest() was public in 87 and is not in 114.\n"
           "  # ManifestManagerHost is the only way to the parsed manifest now,\n"
           "  # and the install button needs it. An embedder outside the content\n"
           "  # tree could not do this; content_shell is inside it.\n"
           '  "+content/browser/manifest",')
    open(path, "w").write(s.replace(anchor, new, 1))
    print("  DEPS: content/browser/manifest 허용")

print("shell chrome: done")
