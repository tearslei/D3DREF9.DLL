import sys,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent)); import runtime_hotkey_batch as b
for c in ['F7+6','F7+6','F7+8','F7+8']:
 print(c); b.send_combo(c); time.sleep(1.0)
