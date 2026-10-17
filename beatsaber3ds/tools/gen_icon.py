#!/usr/bin/env python3
from PIL import Image
import base64
raw=base64.b64decode(open('assets/icon-source.base64','rb').read())
open('assets/icon-source.png','wb').write(raw)
im=Image.open('assets/icon-source.png').convert('RGBA')
im.resize((48,48),Image.Resampling.LANCZOS).save('icon.png',optimize=True)
print('icon.png generated directly from the user-uploaded artwork')
