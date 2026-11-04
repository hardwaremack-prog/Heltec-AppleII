# Builds Heltec-AppleII/player.h, webui.h and font.h from web/*.html and tools/font.py
import os, base64, sys
here = os.path.dirname(os.path.abspath(__file__)); root = os.path.join(here, '..')
sys.path.insert(0, here)
import font
data = font.build()
os.system('python3 "%s" > /dev/null' % os.path.join(here, 'font.py'))
b64 = base64.b64encode(bytes(data)).decode()
def emit(src, dst, var, comment):
    html = open(os.path.join(root, 'web', src)).read().replace('%FONT%', b64)
    assert ')HTML"' not in html
    with open(os.path.join(root, 'Heltec-AppleII', dst), 'w') as f:
        f.write('// %s\n// Generated from web/%s by tools/make_web.py - edit that file, not this one.\n#pragma once\n' % (comment, src))
        f.write('static const char %s[] PROGMEM = R"HTML(%s)HTML";\n' % (var, html))
emit('player.html', 'player.h', 'PLAYER_HTML', 'The Apple II itself, in the browser: http://<board-ip>/')
emit('settings.html', 'webui.h', 'SETTINGS_HTML', 'Settings page: http://<board-ip>/settings')
print('ok')
